
#pragma once

#include <stdint.h>

struct EstadoBotoes
{
    bool ligar = false;
    bool desligar = false;
    bool reset = false;
    bool menu = false;
};

class DriverBotoes
{
public:
    void inicializar();
    EstadoBotoes lerBotoes();

private:
    bool estadoEstavel[4] = {};
    bool ultimaLeitura[4] = {};
    int64_t instanteMudanca[4] = {};
};
