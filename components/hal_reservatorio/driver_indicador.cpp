#include "driver_indicador.h"
#include "driver/gpio.h"

#define PINO_LED_VERDE GPIO_NUM_26
#define PINO_LED_VERMELHO GPIO_NUM_27
#define PINO_BUZZER GPIO_NUM_25

void DriverIndicador::inicializar()
{
    const gpio_num_t pinos[3] = {PINO_LED_VERDE, PINO_LED_VERMELHO, PINO_BUZZER};

    for (int i = 0; i < 3; i++)
    {
        gpio_reset_pin(pinos[i]);
        gpio_set_direction(pinos[i], GPIO_MODE_OUTPUT);
    }
    desligarIndicadores();
}

void DriverIndicador::acionarVerde()
{
    estadoVerde = true;
    estadoVermelho = false;
    aplicar();
}

void DriverIndicador::acionarVermelho()
{
    estadoVermelho = true;
    estadoVerde = false;
    aplicar();
}

void DriverIndicador::acionarBuzzer()
{
    estadoBuzzer = true;
    aplicar();
}

void DriverIndicador::desligarBuzzer()
{
    estadoBuzzer = false;
    aplicar();
}

void DriverIndicador::desligarIndicadores()
{
    estadoVerde = false;
    estadoVermelho = false;
    estadoBuzzer = false;
    aplicar();
}

void DriverIndicador::aplicar()
{
    gpio_set_level(PINO_LED_VERDE, estadoVerde ? 1 : 0);
    gpio_set_level(PINO_LED_VERMELHO, estadoVermelho ? 1 : 0);
    gpio_set_level(PINO_BUZZER, estadoBuzzer ? 1 : 0);
}