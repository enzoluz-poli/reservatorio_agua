#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver_indicador.h"

#include "esp_log.h"

#include "dados_sensores.h"
#include "controlador_reservatorio.h"

#include "driver_botoes.h"

static const char *TAG = "TASK_CONTROLE";

void taskControle(void *pvParameters)
{
    QueueHandle_t filaSensores = (QueueHandle_t)pvParameters;

    DriverIndicador indicador;
    indicador.inicializar();

    DriverBotoes botoes;
    botoes.inicializar();

    indicador.acionarAzul();

    ControladorReservatorio controlador;
    controlador.inicializar();

    ESP_LOGI(TAG, "Tarefa de controle iniciada");

    DadosSensores dados{};

    bool resetPendente = false;

    while (1)
    {
        // Verifica se chegou uma nova medicao.
        bool novaLeitura = false;

        if (xQueueReceive(
                filaSensores,
                &dados,
                pdMS_TO_TICKS(50)) == pdPASS)
        {
            novaLeitura = true;
        }

        // Le os botoes a cada iteracao (50 ms).
        EstadoBotoes estadoBotoes = botoes.lerBotoes();

        if (estadoBotoes.reset && !resetPendente)
        {
            resetPendente = true;
        }

        DadosEntrada entrada{};

        entrada.distanciaCm = dados.distanciaCm;
        entrada.leituraUltrassomValida = dados.valida;
        entrada.novaLeituraUltrassom = novaLeitura;

        entrada.botaoLiga = estadoBotoes.ligar;
        entrada.botaoDesliga = estadoBotoes.desligar;
        entrada.botaoReset = resetPendente;
        entrada.botaoMenu = estadoBotoes.menu;

        // Atualiza a Application.
        ComandosSaida saida = controlador.atualizar(entrada);

        if (novaLeitura && resetPendente)
        {
            resetPendente = false;
        }

        // Aplica os comandos dos indicadores.
        if (saida.ledVerde)
            indicador.acionarVerde();

        if (saida.ledVermelho)
            indicador.acionarVermelho();

        if (saida.ledAzul)
            indicador.acionarAzul();
        else
            indicador.desligarAzul();

        if (saida.buzzer)
            indicador.acionarBuzzer();
        else
            indicador.desligarBuzzer();

        // Registra os dados somente quando chega uma nova medicao.
        if (novaLeitura)
        {
            ESP_LOGI(
                TAG,
                "Nivel: %.2f%% | Estado: %d",
                saida.nivelPercentual,
                static_cast<int>(saida.estado));
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
