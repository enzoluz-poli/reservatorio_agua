#include "wokwi-api.h"
#include <stdlib.h>

#define DISTANCIA_INICIAL_CM 20.0
#define DISTANCIA_MINIMA_CM 5.0
#define VELOCIDADE_SUBIDA_CM_S 1.0
#define ATRASO_ECHO_US 200

typedef struct
{
    pin_t pin_trig;
    pin_t pin_echo;
    pin_t pin_pump;

    timer_t timer_atraso;
    timer_t timer_echo;

    double distanciaCm;
    double instanteAnteriorNs;

    uint32_t duracaoEchoUs;
    bool bombaLigada;
} chip_state_t;

static void atualizar_distancia(chip_state_t *chip)
{
    double agoraNs = get_sim_nanos_d();

    if (chip->bombaLigada)
    {
        double intervaloS =
            (agoraNs - chip->instanteAnteriorNs) / 1000000000.0;

        chip->distanciaCm -=
            VELOCIDADE_SUBIDA_CM_S * intervaloS;

        if (chip->distanciaCm < DISTANCIA_MINIMA_CM)
            chip->distanciaCm = DISTANCIA_MINIMA_CM;
    }

    chip->instanteAnteriorNs = agoraNs;
}

static void ao_mudar_bomba(void *user_data, pin_t pin, uint32_t value)
{
    chip_state_t *chip = (chip_state_t *)user_data;

    atualizar_distancia(chip);
    chip->bombaLigada = (value == HIGH);
}

static void iniciar_echo(void *user_data)
{
    chip_state_t *chip = (chip_state_t *)user_data;

    pin_write(chip->pin_echo, HIGH);
    timer_start(chip->timer_echo, chip->duracaoEchoUs, false);
}

static void finalizar_echo(void *user_data)
{
    chip_state_t *chip = (chip_state_t *)user_data;

    pin_write(chip->pin_echo, LOW);
}

static void ao_mudar_trig(void *user_data, pin_t pin, uint32_t value)
{
    chip_state_t *chip = (chip_state_t *)user_data;

    if (value == HIGH)
    {
        atualizar_distancia(chip);

        chip->duracaoEchoUs =
            (uint32_t)(chip->distanciaCm * 58.0 + 0.5);
    }
    else
    {
        timer_start(chip->timer_atraso, ATRASO_ECHO_US, false);
    }
}

void chip_init(void)
{
    chip_state_t *chip = malloc(sizeof(chip_state_t));

    if (chip == NULL)
        return;

    chip->pin_trig = pin_init("TRIG", INPUT);
    chip->pin_echo = pin_init("ECHO", OUTPUT_LOW);
    chip->pin_pump = pin_init("PUMP", INPUT);

    chip->distanciaCm = DISTANCIA_INICIAL_CM;
    chip->instanteAnteriorNs = get_sim_nanos_d();
    chip->duracaoEchoUs = (uint32_t)(DISTANCIA_INICIAL_CM * 58.0);
    chip->bombaLigada = (pin_read(chip->pin_pump) == HIGH);

    const timer_config_t config_atraso = {
        .callback = iniciar_echo,
        .user_data = chip,
    };

    const timer_config_t config_echo = {
        .callback = finalizar_echo,
        .user_data = chip,
    };

    chip->timer_atraso = timer_init(&config_atraso);
    chip->timer_echo = timer_init(&config_echo);

    const pin_watch_config_t config_trig = {
        .user_data = chip,
        .edge = BOTH,
        .pin_change = ao_mudar_trig,
    };

    const pin_watch_config_t config_bomba = {
        .user_data = chip,
        .edge = BOTH,
        .pin_change = ao_mudar_bomba,
    };

    pin_watch(chip->pin_trig, &config_trig);
    pin_watch(chip->pin_pump, &config_bomba);
}