#pragma once

#include <stdint.h>

#define NUM_BOTOES 6

struct EstadoBotoes
{
    bool ligar = false;
    bool desligar = false;
    bool reset = false;
    bool menu = false;
    bool aumentar = false;
    bool diminuir = false;
};

class DriverBotoes
{
public:
    void inicializar();
    EstadoBotoes lerBotoes();

private:
    bool estadoEstavel[NUM_BOTOES] = {};
    bool ultimaLeitura[NUM_BOTOES] = {};
    int64_t instanteMudanca[NUM_BOTOES] = {};
};
