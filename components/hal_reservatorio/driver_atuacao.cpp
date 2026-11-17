#include "driver_atuacao.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

#define PINO_BOMBA 23
#define PINO_STATUS_BOMBA GPIO_NUM_4
#define PWM_MODO LEDC_HIGH_SPEED_MODE
#define PWM_TIMER LEDC_TIMER_0
#define PWM_CANAL LEDC_CHANNEL_0
#define PWM_RESOLUCAO LEDC_TIMER_8_BIT
#define DUTY_MAXIMO 255

void DriverAtuacao::inicializar()
{
    gpio_reset_pin(PINO_STATUS_BOMBA);
    gpio_set_direction(PINO_STATUS_BOMBA, GPIO_MODE_OUTPUT);
    gpio_set_level(PINO_STATUS_BOMBA, 0);
    ledc_timer_config_t timer = {};
    timer.speed_mode = PWM_MODO;
    timer.duty_resolution = PWM_RESOLUCAO;
    timer.timer_num = PWM_TIMER;
    timer.freq_hz = frequenciaPWM;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer);

    ledc_channel_config_t canal = {};
    canal.gpio_num = PINO_BOMBA;
    canal.speed_mode = PWM_MODO;
    canal.channel = PWM_CANAL;
    canal.timer_sel = PWM_TIMER;
    canal.duty = 0;
    canal.hpoint = 0;
    ledc_channel_config(&canal);

    potenciaAtual = 0;
}

void DriverAtuacao::definirPotencia(uint8_t p)
{
    if (p > 100)
    {
        p = 100;
    }
    potenciaAtual = p;
    gpio_set_level(PINO_STATUS_BOMBA, p > 0 ? 1 : 0);

    uint32_t duty = (uint32_t)p * DUTY_MAXIMO / 100;
    ledc_set_duty(PWM_MODO, PWM_CANAL, duty);
    ledc_update_duty(PWM_MODO, PWM_CANAL);
}

void DriverAtuacao::desligar()
{
    definirPotencia(0);
}

uint8_t DriverAtuacao::obterPotencia() const
{
    return potenciaAtual;
}
