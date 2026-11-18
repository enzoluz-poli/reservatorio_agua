#pragma once

#include <stdint.h>

class DriverDisplay
{
public:
    void inicializar();
    void limpar();
    void escrever(uint8_t linha, const char *texto);
};