#pragma once

struct DadosSensores
{
    // Sensor ultrassonico
    float distanciaCm = -1.0f;
    bool valida = false;

    // Sensor de fluxo
    float fluxoLmin = 0.0f;
    bool fluxoValido = false;
};