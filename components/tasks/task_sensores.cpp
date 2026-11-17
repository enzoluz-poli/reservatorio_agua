#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "driver_ultrassom.h"
#include "dados_sensores.h"

#include "driver_fluxo.h"

#include "esp_timer.h"

static const char *TAG = "TASK_SENSORES";

void taskSensores(void *pvParameters)
{
    QueueHandle_t filaSensores = (QueueHandle_t)pvParameters;

    DriverUltrassom ultrassom;

    ultrassom.inicializar();

    DriverFluxo sensorFluxo;
    sensorFluxo.inicializar();
    int64_t instanteAnterior = esp_timer_get_time();
    bool primeiraLeituraFluxo = true;
    ESP_LOGI(TAG, "Tarefa de sensores iniciada");

    while (1)
    {
        float distancia = ultrassom.medirDistanciaCm();

        uint32_t pulsos = sensorFluxo.obterPulsos();

        int64_t instanteAtual = esp_timer_get_time();

        float intervaloSegundos =
            (instanteAtual - instanteAnterior) / 1000000.0f;

        instanteAnterior = instanteAtual;

        float vazao = 0.0f;
        bool fluxoValido = false;

        if (primeiraLeituraFluxo)
        {
            primeiraLeituraFluxo = false;
        }
        else
        {
            vazao = sensorFluxo.calcularVazao(
                pulsos,
                intervaloSegundos);

            fluxoValido = true;
        }

        DadosSensores dados{};

        dados.fluxoLmin = vazao;
        dados.fluxoValido = fluxoValido;

        if (distancia < 0.0f)
        {
            dados.distanciaCm = -1.0f;
            dados.valida = false;

            ESP_LOGW(TAG, "Falha na leitura do ultrassom");
        }
        else
        {
            dados.distanciaCm = distancia;
            dados.valida = true;

            ESP_LOGI(TAG, "Distancia medida: %.2f cm", distancia);
        }
        ESP_LOGI(
            TAG,
            "Fluxo: %.2f L/min | Pulsos: %lu",
            vazao,
            static_cast<unsigned long>(pulsos));

        ESP_LOGI(TAG, "Intervalo: %.3f s | Fluxo valido: %d",
                 intervaloSegundos, fluxoValido);

        // Envia os dados para a fila
        if (xQueueSend(filaSensores, &dados, 0) != pdPASS)
        {
            ESP_LOGW(TAG, "Fila de sensores cheia");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
