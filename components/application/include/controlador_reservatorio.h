
#pragma once

#include <stdint.h>

enum class EstadoReservatorio
{
    INICIALIZANDO,
    AGUARDANDO,
    OPERACAO_NORMAL,
    FALHA,
    RECUPERACAO
};

enum class SubestadoOperacao
{
    NENHUM,
    ABAIXO_SETPOINT,
    PROXIMO_SETPOINT,
    ESTABILIZADO
};

enum class CausaFalha
{
    NENHUMA,
    SENSOR_NIVEL,
    SEM_FLUXO
};

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
};

struct ComandosSaida
{
    uint8_t potenciaBomba = 0;
    float nivelPercentual = -1.0f;

    EstadoReservatorio estado = EstadoReservatorio::INICIALIZANDO;
    SubestadoOperacao subestado = SubestadoOperacao::NENHUM;
    CausaFalha causaFalha = CausaFalha::NENHUMA;

    bool ledVerde = false;
    bool ledVermelho = false;
    bool ledAzul = false;
    bool buzzer = false;
};

class ControladorReservatorio
{
public:
    void inicializar();

    ComandosSaida atualizar(const DadosEntrada &entrada);

    void configurarSetpoint(float valor);
    float obterSetpoint() const { return setpoint; }

private:
    float distanciaNivelVazio = 40.0f;
    float distanciaNivelCheio = 5.0f;

    float calcularNivelPercentual(float distanciaCm) const;

    float setpoint = 70.0f;
    float tolerancia = 2.0f;

    uint8_t contadorLeiturasInvalidas = 0;
    bool bombaLigada = false;
    uint8_t potenciaBombaAtual = 0;

    uint8_t contadorLeiturasSemFluxo = 0;
    float tempoBombaLigada = 0.0f;
    float tempoPartidaBomba = 2.0f;

    bool fluxoPendenteVerificacao = false;
    uint8_t contadorLeiturasComFluxo = 0;

    CausaFalha causaFalha = CausaFalha::NENHUMA;

    EstadoReservatorio estadoAtual =
        EstadoReservatorio::INICIALIZANDO;
};
