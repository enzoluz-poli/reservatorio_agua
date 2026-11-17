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

static const float PASSO_SETPOINT = 5.0f;
static const uint8_t NUM_TELAS = 3;

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

        // Menu: alterna a tela somente no momento do pressionamento.
        if (estadoBotoes.menu && !menuAnterior)
        {
            telaAtual = (telaAtual + 1) % NUM_TELAS;
            mudouTela = true;
        }

        // + e -: ajustam o setpoint e mostram a tela de setpoint.
        if (estadoBotoes.aumentar && !aumentarAnterior)
        {
            controlador.configurarSetpoint(
                controlador.obterSetpoint() + PASSO_SETPOINT);
            telaAtual = 2;
            mudouTela = true;
        }

        if (estadoBotoes.diminuir && !diminuirAnterior)
        {
            controlador.configurarSetpoint(
                controlador.obterSetpoint() - PASSO_SETPOINT);
            telaAtual = 2;
            mudouTela = true;
        }

        menuAnterior = estadoBotoes.menu;
        aumentarAnterior = estadoBotoes.aumentar;
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
        entrada.dt = novaLeitura ? 1.0f : 0.0f;

        entrada.botaoLiga = estadoBotoes.ligar;
        entrada.botaoDesliga = estadoBotoes.desligar;
        entrada.botaoReset = resetPendente;
        entrada.botaoMenu = estadoBotoes.menu;
        entrada.botaoAumentar = estadoBotoes.aumentar;
        entrada.botaoDiminuir = estadoBotoes.diminuir;

        ComandosSaida saida = controlador.atualizar(entrada);

        atuacao.definirPotencia(saida.potenciaBomba);

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

        // Atualiza o display quando chega uma leitura
        // ou quando o usuario troca de tela.
        if (novaLeitura || mudouTela)
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

                snprintf(
                    linha2,
                    sizeof(linha2),
                    "Estado: %s",
                    nomeEstado(saida.estado));
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
                    "Setpoint: %4.1f%%",
                    controlador.obterSetpoint());

                snprintf(linha2, sizeof(linha2), "+/- para mudar");
            }

            display.escrever(0, linha1);
            display.escrever(1, linha2);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
