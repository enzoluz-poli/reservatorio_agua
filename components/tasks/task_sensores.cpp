#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "driver_ultrassom.h"
#include "dados_sensores.h"

static const char *TAG = "TASK_SENSORES";

void taskSensores(void *pvParameters)
{
    QueueHandle_t filaSensores = (QueueHandle_t)pvParameters;

    DriverUltrassom ultrassom;

    ultrassom.inicializar();

    ESP_LOGI(TAG, "Tarefa de sensores iniciada");

    while (1)
    {
        float distancia = ultrassom.medirDistanciaCm();

        DadosSensores dados{};

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

        // Envia os dados para a fila
        if (xQueueSend(filaSensores, &dados, 0) != pdPASS)
        {
            ESP_LOGW(TAG, "Fila de sensores cheia");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}