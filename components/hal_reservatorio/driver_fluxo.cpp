#include "driver_fluxo.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static const char *TAG = "DRIVER_FLUXO";

static portMUX_TYPE muxPulsos = portMUX_INITIALIZER_UNLOCKED;

#define PINO_SENSOR_FLUXO GPIO_NUM_34

static uint32_t contadorPulsos = 0;

static void IRAM_ATTR contarPulso(void *arg)
{
    portENTER_CRITICAL_ISR(&muxPulsos);
    contadorPulsos++;
    portEXIT_CRITICAL_ISR(&muxPulsos);
}

void DriverFluxo::inicializar()
{
    gpio_config_t config = {};

    config.pin_bit_mask = (1ULL << PINO_SENSOR_FLUXO);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_POSEDGE;

    esp_err_t resultado = gpio_config(&config);

    if (resultado != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao configurar GPIO 34: %s",
                 esp_err_to_name(resultado));
        return;
    }

    resultado = gpio_install_isr_service(0);

    if (resultado != ESP_OK && resultado != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Erro ao instalar servico de interrupcao: %s",
                 esp_err_to_name(resultado));
        return;
    }

    resultado = gpio_isr_handler_add(
        PINO_SENSOR_FLUXO,
        contarPulso,
        nullptr);

    if (resultado != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao registrar interrupcao: %s",
                 esp_err_to_name(resultado));
        return;
    }

    ESP_LOGI(TAG, "Sensor de fluxo inicializado no GPIO 34");
}

uint32_t DriverFluxo::obterPulsos()
{
    portENTER_CRITICAL(&muxPulsos);
    uint32_t pulsos = contadorPulsos;
    contadorPulsos = 0;
    portEXIT_CRITICAL(&muxPulsos);

    return pulsos;
}

float DriverFluxo::calcularVazao(
    uint32_t pulsos,
    float intervaloSegundos)
{
    if (intervaloSegundos <= 0.0f)
    {
        return 0.0f;
    }

    float frequencia = pulsos / intervaloSegundos;

    return frequencia / 7.5f;
};