#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver_indicador.h"
#include "driver_display.h"
#include "driver_botoes.h"
#include "driver_atuacao.h"

#include "esp_log.h"

#include "dados_sensores.h"
#include "controlador_reservatorio.h"

#include <stdio.h>

static const char *TAG = "TASK_CONTROLE";

static constexpr float PASSO_SETPOINT = 5.0f;
static constexpr uint8_t NUM_TELAS = 3;

static const char *nomeEstado(EstadoReservatorio estado)
{
    switch (estado)
    {
    case EstadoReservatorio::INICIALIZANDO:
        return "INICIALIZANDO";

    case EstadoReservatorio::AGUARDANDO:
        return "AGUARDANDO";

    case EstadoReservatorio::OPERACAO_NORMAL:
        return "OPERANDO";

    case EstadoReservatorio::FALHA:
        return "FALHA";

    case EstadoReservatorio::RECUPERACAO:
        return "RECUPERANDO";

    default:
        return "DESCONHECIDO";
    }
}

static const char *nomeSubestado(SubestadoOperacao subestado)
{
    switch (subestado)
    {
    case SubestadoOperacao::ABAIXO_SETPOINT:
        return "Abaixo setpoint";

    case SubestadoOperacao::PROXIMO_SETPOINT:
        return "Proximo setpoint";

    case SubestadoOperacao::ESTABILIZADO:
        return "Estabilizado";

    default:
        return "Operando";
    }
}

static const char *nomeCausa(CausaFalha causa)
{
    switch (causa)
    {
    case CausaFalha::NENHUMA:
        return "NENHUMA";

    case CausaFalha::SENSOR_NIVEL:
        return "SENSOR NIVEL";

    case CausaFalha::SEM_FLUXO:
        return "SEM FLUXO";

    case CausaFalha::FLUXO_INSUFICIENTE:
        return "FLUXO INSUF.";

    case CausaFalha::FLUXO_EXCESSIVO:
        return "FLUXO EXCESS.";

    case CausaFalha::FLUXO_SEM_ENCHIMENTO:
        return "SEM ENCHIMENTO";
    case CausaFalha::FALHA_SENSOR_FLUXO:
        return "SENSOR FLUXO";

    default:
        return "DESCONHECIDA";
    }
}

void taskControle(void *pvParameters)
{
    QueueHandle_t filaSensores = (QueueHandle_t)pvParameters;

    DriverIndicador indicador;
    indicador.inicializar();

    DriverAtuacao atuacao;
    atuacao.inicializar();
    atuacao.desligar();

    DriverBotoes botoes;
    botoes.inicializar();

    DriverDisplay display;
    display.inicializar();

    indicador.acionarAzul();

    ControladorReservatorio controlador;
    controlador.inicializar();

    display.escrever(0, "Reservatorio");
    display.escrever(1, "Inicializando");

    ESP_LOGI(TAG, "Tarefa de controle iniciada");

    DadosSensores dados{};

    bool resetPendente = false;
    bool menuAnterior = false;
    bool aumentarAnterior = false;
    bool diminuirAnterior = false;

    EstadoReservatorio estadoAnterior = EstadoReservatorio::AGUARDANDO;
    SubestadoOperacao subestadoAnterior = SubestadoOperacao::NENHUM;

    uint8_t telaAtual = 0;

    while (1)
    {
        bool novaLeitura = false;

        if (xQueueReceive(
                filaSensores,
                &dados,
                pdMS_TO_TICKS(50)) == pdPASS)
        {
            novaLeitura = true;
        }

        EstadoBotoes estadoBotoes = botoes.lerBotoes();

        bool mudouTela = false;

        // Navegacao entre as tres telas.
        if (estadoBotoes.menu && !menuAnterior)
        {
            telaAtual = (telaAtual + 1) % NUM_TELAS;
            mudouTela = true;
        }

        menuAnterior = estadoBotoes.menu;

        // Detecta o pressionamento do botao de aumentar.
        if (estadoBotoes.aumentar && !aumentarAnterior)
        {
            float novoSetpoint =
                controlador.obterSetpoint() + PASSO_SETPOINT;

            controlador.configurarSetpoint(novoSetpoint);

            telaAtual = 2;
            mudouTela = true;
        }

        aumentarAnterior = estadoBotoes.aumentar;

        // Detecta o pressionamento do botao de diminuir.
        if (estadoBotoes.diminuir && !diminuirAnterior)
        {
            float novoSetpoint =
                controlador.obterSetpoint() - PASSO_SETPOINT;

            controlador.configurarSetpoint(novoSetpoint);

            telaAtual = 2;
            mudouTela = true;
        }

        diminuirAnterior = estadoBotoes.diminuir;

        if (estadoBotoes.reset && !resetPendente)
        {
            resetPendente = true;
        }

        DadosEntrada entrada{};

        entrada.distanciaCm = dados.distanciaCm;
        entrada.leituraUltrassomValida = dados.valida;
        entrada.novaLeituraUltrassom = novaLeitura;
        entrada.fluxoLmin = dados.fluxoLmin;
        entrada.leituraFluxoValida = dados.fluxoValido;
        entrada.dt = novaLeitura ? dados.dt : 0.0f;

        entrada.botaoLiga = estadoBotoes.ligar;
        entrada.botaoDesliga = estadoBotoes.desligar;
        entrada.botaoReset = resetPendente;

        ComandosSaida saida = controlador.atualizar(entrada);

        atuacao.definirPotencia(saida.potenciaBomba);

        // Ao entrar em falha, volta para a tela principal com a causa.
        if (saida.estado == EstadoReservatorio::FALHA &&
            estadoAnterior != EstadoReservatorio::FALHA)
        {
            telaAtual = 0;
            mudouTela = true;
        }

        bool mudouEstado = (saida.estado != estadoAnterior) ||
                           (saida.subestado != subestadoAnterior);

        estadoAnterior = saida.estado;
        subestadoAnterior = saida.subestado;

        if (novaLeitura && resetPendente)
        {
            resetPendente = false;
        }

        indicador.desligarIndicadores();

        if (saida.ledVerde)
            indicador.acionarVerde();

        if (saida.ledVermelho)
            indicador.acionarVermelho();

        if (saida.ledAzul)
            indicador.acionarAzul();

        if (saida.buzzer)
            indicador.acionarBuzzer();

        if (novaLeitura)
        {
            ESP_LOGI(
                TAG,
                "Nivel: %.2f%% | Estado: %d | Potencia: %u%% | Setpoint: %.1f%%",
                saida.nivelPercentual,
                static_cast<int>(saida.estado),
                saida.potenciaBomba,
                controlador.obterSetpoint());
        }

        // Atualiza o display em novas leituras, mudancas de tela
        // ou mudancas de estado.
        if (novaLeitura || mudouTela || mudouEstado)
        {
            char linha1[17];
            char linha2[17];

            if (telaAtual == 0)
            {
                if (saida.nivelPercentual >= 0.0f)
                {
                    snprintf(
                        linha1,
                        sizeof(linha1),
                        "Nivel: %5.1f%%",
                        saida.nivelPercentual);
                }
                else
                {
                    snprintf(
                        linha1,
                        sizeof(linha1),
                        "Nivel: INVALIDO");
                }

                if (saida.estado == EstadoReservatorio::FALHA)
                {
                    snprintf(
                        linha2,
                        sizeof(linha2),
                        "FALHA: %s",
                        nomeCausa(saida.causaFalha));
                }
                else if (saida.estado == EstadoReservatorio::OPERACAO_NORMAL)
                {
                    snprintf(
                        linha2,
                        sizeof(linha2),
                        "%s",
                        nomeSubestado(saida.subestado));
                }
                else
                {
                    snprintf(
                        linha2,
                        sizeof(linha2),
                        "Estado: %s",
                        nomeEstado(saida.estado));
                }
            }
            else if (telaAtual == 1)
            {
                snprintf(
                    linha1,
                    sizeof(linha1),
                    "Bomba: %s",
                    saida.potenciaBomba > 0 ? "LIGADA" : "DESLIGADA");

                snprintf(
                    linha2,
                    sizeof(linha2),
                    "Potencia: %u%%",
                    saida.potenciaBomba);
            }
            else
            {
                snprintf(
                    linha1,
                    sizeof(linha1),
                    "Ajuste Setpoint");

                snprintf(
                    linha2,
                    sizeof(linha2),
                    "Valor: %5.1f%%",
                    controlador.obterSetpoint());
            }

            display.escrever(0, linha1);
            display.escrever(1, linha2);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
