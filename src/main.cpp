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
    // Inicia tarefa de sensores
    TaskHandle_t tarefaSensores = nullptr;

    BaseType_t resultadoSensores = xTaskCreate(
        taskSensores,
        "TaskSensores",
        4096,
        (void *)filaSensores,
        5,
        &tarefaSensores);

    if (resultadoSensores != pdPASS)
    {
        ESP_LOGE("RESERVATORIO", "Erro ao criar tarefa de sensores");
        return;
    }

    TaskHandle_t tarefaControle = nullptr;

    BaseType_t resultadoControle = xTaskCreate(
        taskControle,
        "TaskControle",
        4096,
        (void *)filaSensores,
        5,
        &tarefaControle);

    if (resultadoControle != pdPASS)
    {
        ESP_LOGE("RESERVATORIO", "Erro ao criar tarefa de controle");

        vTaskDelete(tarefaSensores);
        return;
    }

    ESP_LOGI("RESERVATORIO", "Tarefas iniciadas com sucesso");
}
