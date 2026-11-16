#pragma once

class DriverIndicador {
public:
    void inicializar();
    void acionarVerde();        // acende o verde e apaga o vermelho
    void acionarVermelho();     // acende o vermelho e apaga o verde
    void acionarBuzzer();
    void desligarBuzzer();
    void desligarIndicadores(); // apaga LEDs e buzzer

private:
    void aplicar();             // escreve os estados nos pinos

    bool estadoVerde = false;
    bool estadoVermelho = false;
    bool estadoBuzzer = false;
};
