
#include "driver_ultrassom.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void DriverUltrassom::inicializar()
{
    gpio_config_t configTrig = {};
    configTrig.pin_bit_mask = (1ULL << PINO_TRIG);
    configTrig.mode = GPIO_MODE_OUTPUT;
    configTrig.pull_up_en = GPIO_PULLUP_DISABLE;
    configTrig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    configTrig.intr_type = GPIO_INTR_DISABLE;

    gpio_config(&configTrig);

    gpio_config_t configEcho = {};
    configEcho.pin_bit_mask = (1ULL << PINO_ECHO);
    configEcho.mode = GPIO_MODE_INPUT;
    configEcho.pull_up_en = GPIO_PULLUP_DISABLE;
    configEcho.pull_down_en = GPIO_PULLDOWN_DISABLE;
    configEcho.intr_type = GPIO_INTR_DISABLE;

    gpio_config(&configEcho);

    gpio_set_level((gpio_num_t)PINO_TRIG, 0);
}

float DriverUltrassom::medirDistanciaCm()
{
    // Envia pulso de disparo de 10 microssegundos
    gpio_set_level((gpio_num_t)PINO_TRIG, 0);
    esp_rom_delay_us(2);

    gpio_set_level((gpio_num_t)PINO_TRIG, 1);
    esp_rom_delay_us(10);

    gpio_set_level((gpio_num_t)PINO_TRIG, 0);

    // Aguarda o inicio do sinal ECHO
    int64_t inicioEspera = esp_timer_get_time();

    while (gpio_get_level((gpio_num_t)PINO_ECHO) == 0)
    {
        if (esp_timer_get_time() - inicioEspera > 30000)
            return -1.0f;
    }

    // Mede a duracao do pulso ECHO
    int64_t inicioPulso = esp_timer_get_time();

    while (gpio_get_level((gpio_num_t)PINO_ECHO) == 1)
    {
        if (esp_timer_get_time() - inicioPulso > 30000)
            return -1.0f;
    }

    int64_t fimPulso = esp_timer_get_time();

    float duracaoUs = (float)(fimPulso - inicioPulso);

    // Converte tempo em distancia (velocidade do som)
    float distanciaCm = duracaoUs / 58.0f;

    return distanciaCm;
}
