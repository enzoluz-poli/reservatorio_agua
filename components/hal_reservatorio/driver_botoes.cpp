
#include "driver_botoes.h"

#include "driver/gpio.h"
#include "esp_timer.h"

#include "esp_log.h"

#define PINO_BOTAO_LIGAR GPIO_NUM_32
#define PINO_BOTAO_DESLIGAR GPIO_NUM_33
#define PINO_BOTAO_RESET GPIO_NUM_21
#define PINO_BOTAO_MENU GPIO_NUM_22

#define TEMPO_DEBOUNCE_MS 30

void DriverBotoes::inicializar()
{
    const gpio_num_t pinos[4] = {
        PINO_BOTAO_LIGAR,
        PINO_BOTAO_DESLIGAR,
        PINO_BOTAO_RESET,
        PINO_BOTAO_MENU};

    for (int i = 0; i < 4; i++)
    {
        gpio_config_t config = {};

        config.pin_bit_mask = (1ULL << pinos[i]);
        config.mode = GPIO_MODE_INPUT;
        config.pull_up_en = GPIO_PULLUP_ENABLE;
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.intr_type = GPIO_INTR_DISABLE;

        gpio_config(&config);

        bool leituraInicial = (gpio_get_level(pinos[i]) == 0);

        if (i == 0)
        {
            ESP_LOGI("TESTE_BOTAO", "GPIO 32 nivel: %d", gpio_get_level(pinos[i]));
        }

        ultimaLeitura[i] = leituraInicial;
        estadoEstavel[i] = leituraInicial;
        instanteMudanca[i] = esp_timer_get_time();
    }
}

EstadoBotoes DriverBotoes::lerBotoes()
{
    EstadoBotoes estado{};

    const gpio_num_t pinos[4] = {
        PINO_BOTAO_LIGAR,
        PINO_BOTAO_DESLIGAR,
        PINO_BOTAO_RESET,
        PINO_BOTAO_MENU};

    int64_t agora = esp_timer_get_time();

    for (int i = 0; i < 4; i++)
    {
        bool leituraAtual = (gpio_get_level(pinos[i]) == 0);

        if (leituraAtual != ultimaLeitura[i])
        {
            ultimaLeitura[i] = leituraAtual;
            instanteMudanca[i] = agora;
        }

        int64_t tempoEstavel =
            agora - instanteMudanca[i];

        if (tempoEstavel >= TEMPO_DEBOUNCE_MS * 1000)
        {
            estadoEstavel[i] = leituraAtual;
        }
    }

    estado.ligar = estadoEstavel[0];
    estado.desligar = estadoEstavel[1];
    estado.reset = estadoEstavel[2];
    estado.menu = estadoEstavel[3];

    return estado;
}
