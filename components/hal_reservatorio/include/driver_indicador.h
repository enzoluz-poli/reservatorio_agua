
#pragma once

class DriverIndicador
{
public:
    void inicializar();

    void acionarVerde();
    void acionarVermelho();
    void acionarAzul();

    void desligarAzul();

    void acionarBuzzer();
    void desligarBuzzer();
    void desligarIndicadores();

private:
    void aplicar();

    bool estadoVerde = false;
    bool estadoVermelho = false;
    bool estadoAzul = false;
    bool estadoBuzzer = false;
};
