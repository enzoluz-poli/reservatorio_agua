#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

extern "C" void app_main(void)
{
    ESP_LOGI("RESERVATORIO", "Sistema iniciado!");

    while (1)
    {
        ESP_LOGI("RESERVATORIO", "Reservatorio inteligente em execucao");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}