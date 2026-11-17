
#include "driver_display.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "DRIVER_DISPLAY";

#define PORTA_I2C I2C_NUM_0
#define PINO_SDA GPIO_NUM_21
#define PINO_SCL GPIO_NUM_22

#define ENDERECO_LCD 0x27
#define FREQUENCIA_I2C 100000

#define LCD_RS 0x01
#define LCD_EN 0x04
#define LCD_BACKLIGHT 0x08

static i2c_master_bus_handle_t barramentoI2C = nullptr;
static i2c_master_dev_handle_t dispositivoLCD = nullptr;

static esp_err_t enviarByte(uint8_t dado)
{
    return i2c_master_transmit(
        dispositivoLCD,
        &dado,
        1,
        1000);
}

static esp_err_t enviarNibble(uint8_t nibble, bool caractere)
{
    uint8_t dado = (nibble & 0xF0) | LCD_BACKLIGHT;

    if (caractere)
        dado |= LCD_RS;

    esp_err_t resultado = enviarByte(dado | LCD_EN);

    if (resultado != ESP_OK)
        return resultado;

    resultado = enviarByte(dado);

    vTaskDelay(pdMS_TO_TICKS(1));

    return resultado;
}

static esp_err_t enviarByteLCD(uint8_t dado, bool caractere)
{
    esp_err_t resultado = enviarNibble(dado & 0xF0, caractere);

    if (resultado != ESP_OK)
        return resultado;

    return enviarNibble((dado << 4) & 0xF0, caractere);
}

void DriverDisplay::inicializar()
{
    i2c_master_bus_config_t configuracao = {};

    configuracao.i2c_port = PORTA_I2C;
    configuracao.sda_io_num = PINO_SDA;
    configuracao.scl_io_num = PINO_SCL;
    configuracao.clk_source = I2C_CLK_SRC_DEFAULT;
    configuracao.glitch_ignore_cnt = 7;
    configuracao.flags.enable_internal_pullup = true;

    esp_err_t resultado =
        i2c_new_master_bus(&configuracao, &barramentoI2C);

    if (resultado != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao inicializar barramento I2C");
        return;
    }

    i2c_device_config_t dispositivo = {};

    dispositivo.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dispositivo.device_address = ENDERECO_LCD;
    dispositivo.scl_speed_hz = FREQUENCIA_I2C;

    resultado = i2c_master_bus_add_device(
        barramentoI2C,
        &dispositivo,
        &dispositivoLCD);

    if (resultado != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao adicionar LCD ao barramento I2C");
        return;
    }

    ESP_LOGI(TAG, "Barramento I2C inicializado");

    vTaskDelay(pdMS_TO_TICKS(50));

    enviarNibble(0x30, false);
    vTaskDelay(pdMS_TO_TICKS(5));

    enviarNibble(0x30, false);
    vTaskDelay(pdMS_TO_TICKS(1));

    enviarNibble(0x30, false);
    vTaskDelay(pdMS_TO_TICKS(1));

    enviarNibble(0x20, false);

    enviarByteLCD(0x28, false);
    enviarByteLCD(0x0C, false);
    enviarByteLCD(0x06, false);
    enviarByteLCD(0x01, false);

    vTaskDelay(pdMS_TO_TICKS(5));
}

void DriverDisplay::limpar()
{
    enviarByteLCD(0x01, false);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void DriverDisplay::escrever(uint8_t linha, const char *texto)
{
    if (linha > 1 || texto == nullptr || dispositivoLCD == nullptr)
        return;

    uint8_t endereco = (linha == 0) ? 0x80 : 0xC0;

    enviarByteLCD(endereco, false);

    for (uint8_t i = 0; texto[i] != '\0' && i < 16; i++)
    {
        enviarByteLCD(texto[i], true);
    }
}
