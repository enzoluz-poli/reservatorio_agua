#include "driver_botoes.h"

#include "driver/gpio.h"
#include "esp_timer.h"

#define PINO_BOTAO_LIGAR     GPIO_NUM_32
#define PINO_BOTAO_DESLIGAR  GPIO_NUM_33
#define PINO_BOTAO_RESET     GPIO_NUM_16
#define PINO_BOTAO_MENU      GPIO_NUM_17
#define PINO_BOTAO_AUMENTAR  GPIO_NUM_13
#define PINO_BOTAO_DIMINUIR  GPIO_NUM_14

#define TEMPO_DEBOUNCE_MS 30

static const gpio_num_t PINOS[NUM_BOTOES] = {
    PINO_BOTAO_LIGAR,
    PINO_BOTAO_DESLIGAR,
    PINO_BOTAO_RESET,
    PINO_BOTAO_MENU,
    PINO_BOTAO_AUMENTAR,
    PINO_BOTAO_DIMINUIR};

void DriverBotoes::inicializar()
{
    for (int i = 0; i < NUM_BOTOES; i++)
    {
        gpio_config_t config = {};

        config.pin_bit_mask = (1ULL << PINOS[i]);
        config.mode = GPIO_MODE_INPUT;
        config.pull_up_en = GPIO_PULLUP_ENABLE;
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.intr_type = GPIO_INTR_DISABLE;

        gpio_config(&config);

        bool leituraInicial = (gpio_get_level(PINOS[i]) == 0);

        ultimaLeitura[i] = leituraInicial;
        estadoEstavel[i] = leituraInicial;
        instanteMudanca[i] = esp_timer_get_time();
    }
}

EstadoBotoes DriverBotoes::lerBotoes()
{
    EstadoBotoes estado{};

    int64_t agora = esp_timer_get_time();

    for (int i = 0; i < NUM_BOTOES; i++)
    {
        bool leituraAtual = (gpio_get_level(PINOS[i]) == 0);

        if (leituraAtual != ultimaLeitura[i])
        {
            ultimaLeitura[i] = leituraAtual;
            instanteMudanca[i] = agora;
        }

        if (agora - instanteMudanca[i] >= TEMPO_DEBOUNCE_MS * 1000)
        {
            estadoEstavel[i] = leituraAtual;
        }
    }

    estado.ligar = estadoEstavel[0];
    estado.desligar = estadoEstavel[1];
    estado.reset = estadoEstavel[2];
    estado.menu = estadoEstavel[3];
    estado.aumentar = estadoEstavel[4];
    estado.diminuir = estadoEstavel[5];

    return estado;
}
