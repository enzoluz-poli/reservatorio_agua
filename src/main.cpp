#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver_indicador.h"

extern "C" void app_main(void)
{
    DriverIndicador indicador;
    indicador.inicializar();
    ESP_LOGI("RESERVATORIO", "Sistema iniciado!");

    while (1)
    {
        indicador.acionarVerde();
        ESP_LOGI("RESERVATORIO", "Verde");
        vTaskDelay(pdMS_TO_TICKS(1000));

        indicador.acionarVermelho();
        indicador.acionarBuzzer();
        ESP_LOGI("RESERVATORIO", "Vermelho + buzzer");
        vTaskDelay(pdMS_TO_TICKS(1000));

        indicador.desligarIndicadores();
        ESP_LOGI("RESERVATORIO", "Tudo desligado");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}