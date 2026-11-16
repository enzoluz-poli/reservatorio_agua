#pragma once

#include <stdint.h>

class DriverAtuacao {
public:
    void inicializar();
    void definirPotencia(uint8_t p);   // 0 a 100 (%)
    void desligar();
    uint8_t obterPotencia() const;

private:
    uint32_t frequenciaPWM = 1000;     // Hz
    uint8_t potenciaAtual = 0;         // %
};
