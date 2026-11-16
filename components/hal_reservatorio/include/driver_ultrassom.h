#pragma once

class DriverUltrassom
{
public:
    void inicializar();

    float medirDistanciaCm();

private:
    static constexpr int PINO_TRIG = 5;
    static constexpr int PINO_ECHO = 18;
};