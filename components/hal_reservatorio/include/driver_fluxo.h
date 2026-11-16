#pragma once

#include <stdint.h>

class DriverFluxo
{
public:
    void inicializar();
    uint32_t obterPulsos();
    float calcularVazao(uint32_t pulsos, float intervaloSegundos);
};