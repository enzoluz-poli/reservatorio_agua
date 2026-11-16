
#pragma once

#include <stdint.h>

// Estados operacionais do reservatorio
enum class EstadoReservatorio
{
    INICIALIZANDO,
    AGUARDANDO,
    OPERACAO_NORMAL,
    FALHA,
    RECUPERACAO
};

// Informacoes recebidas pela Application
struct DadosEntrada
{
    float distanciaCm = 0.0f;
    bool leituraUltrassomValida = false;
    bool novaLeituraUltrassom = false;

    float fluxoLmin = 0.0f;
    bool leituraFluxoValida = false;

    float dt = 0.0f;

    bool botaoLiga = false;
    bool botaoDesliga = false;
    bool botaoReset = false;

    bool botaoMenu = false;
    bool botaoAumentar = false;
    bool botaoDiminuir = false;
};

// Comandos produzidos pela Application
struct ComandosSaida
{
    uint8_t potenciaBomba = 0;
    float nivelPercentual = -1.0f;

    EstadoReservatorio estado = EstadoReservatorio::INICIALIZANDO;

    bool ledVerde = false;
    bool ledVermelho = false;
    bool ledAzul = false;
    bool buzzer = false;
};

// Controlador principal do reservatorio
class ControladorReservatorio
{
public:
    void inicializar();

    ComandosSaida atualizar(const DadosEntrada &entrada);

    void configurarSetpoint(float valor);
    void configurarTolerancia(float valor);

private:
    // Parametros de calibracao do sensor ultrassonico (cm)
    float distanciaNivelVazio = 40.0f;
    float distanciaNivelCheio = 5.0f;

    // Converte distancia medida em nivel percentual
    float calcularNivelPercentual(float distanciaCm) const;

    float setpoint = 0.0f;
    float tolerancia = 0.0f;

    uint8_t contadorLeiturasInvalidas = 0;

    EstadoReservatorio estadoAtual =
        EstadoReservatorio::INICIALIZANDO;
};
