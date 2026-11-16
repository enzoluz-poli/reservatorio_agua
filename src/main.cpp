#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "task_sensores.h"
#include "task_controle.h"
#include "dados_sensores.h"

extern "C" void app_main(void)
{
    ESP_LOGI("RESERVATORIO", "Inicializando sistema");

    QueueHandle_t filaSensores = xQueueCreate(
        5,
        sizeof(DadosSensores));

    if (filaSensores == nullptr)
    {
        ESP_LOGE("RESERVATORIO", "Erro ao criar fila de sensores");
        return;
    }

    ESP_LOGI("RESERVATORIO", "Fila de sensores criada");

    // Inicia tarefa de sensores
    xTaskCreate(
        taskSensores,
        "TaskSensores",
        4096,
        (void *)filaSensores,
        5,
        nullptr);

    // Inicia tarefa de controle
    xTaskCreate(
        taskControle,
        "TaskControle",
        4096,
        (void *)filaSensores,
        5,
        nullptr);
}