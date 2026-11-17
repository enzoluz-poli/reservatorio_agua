
#include "controlador_reservatorio.h"

#include <algorithm>

static constexpr float GANHO_PROPORCIONAL = 5.0f;

void ControladorReservatorio::inicializar()
{
    estadoAtual = EstadoReservatorio::INICIALIZANDO;

    setpoint = 70.0f;
    tolerancia = 2.0f;

    bombaLigada = false;
    contadorLeiturasInvalidas = 0;

    contadorLeiturasSemFluxo = 0;
    tempoBombaLigada = 0.0f;

    estadoAtual = EstadoReservatorio::AGUARDANDO;
}

static constexpr float MARGEM_CM = 2.0f;

float ControladorReservatorio::calcularNivelPercentual(float distanciaCm) const
{
    if (distanciaCm < distanciaNivelCheio - MARGEM_CM ||
        distanciaCm > distanciaNivelVazio + MARGEM_CM)
    {
        return -1.0f;
    }

    float nivel = (distanciaNivelVazio - distanciaCm) /
                  (distanciaNivelVazio - distanciaNivelCheio) * 100.0f;

    return std::clamp(nivel, 0.0f, 100.0f);
}

ComandosSaida ControladorReservatorio::atualizar(const DadosEntrada &entrada)
{
    ComandosSaida saida{};

    bool bombaEstavaLigada = bombaLigada;

    float nivel = -1.0f;

    if (entrada.leituraUltrassomValida)
    {
        nivel = calcularNivelPercentual(entrada.distanciaCm);
    }

    bool leituraSegura = nivel >= 0.0f;

    // Mantem o sistema em falha ate receber um comando de recuperacao.
    if (estadoAtual == EstadoReservatorio::FALHA)
    {
        bombaLigada = false;
        saida.potenciaBomba = 0;

        if (entrada.botaoReset &&
            entrada.novaLeituraUltrassom &&
            leituraSegura)
        {
            estadoAtual = EstadoReservatorio::RECUPERACAO;
            contadorLeiturasInvalidas = 0;
            contadorLeiturasSemFluxo = 0;
            tempoBombaLigada = 0.0f;

            saida.estado = EstadoReservatorio::RECUPERACAO;
            saida.ledAzul = true;

            return saida;
        }

        saida.estado = EstadoReservatorio::FALHA;
        saida.nivelPercentual = -1.0f;
        saida.ledVermelho = true;
        saida.buzzer = true;

        return saida;
    }

    // Etapa de recuperacao.
    if (estadoAtual == EstadoReservatorio::RECUPERACAO &&
        entrada.novaLeituraUltrassom)
    {
        bombaLigada = false;

        if (leituraSegura)
        {
            estadoAtual = EstadoReservatorio::AGUARDANDO;
        }
        else
        {
            estadoAtual = EstadoReservatorio::FALHA;
        }
    }

    // Liga o sistema quando solicitado.
    if (estadoAtual == EstadoReservatorio::AGUARDANDO &&
        entrada.botaoLiga)
    {
        estadoAtual = EstadoReservatorio::OPERACAO_NORMAL;
    }

    // Desliga o sistema quando solicitado.
    if (estadoAtual == EstadoReservatorio::OPERACAO_NORMAL &&
        entrada.botaoDesliga)
    {
        estadoAtual = EstadoReservatorio::AGUARDANDO;

        bombaLigada = false;
        contadorLeiturasSemFluxo = 0;
        tempoBombaLigada = 0.0f;
    }

    if (estadoAtual == EstadoReservatorio::OPERACAO_NORMAL)
    {
        if (entrada.novaLeituraUltrassom)
        {
            if (leituraSegura)
            {
                float erro = setpoint - nivel;
                float potenciaCalculada = 0.0f;

                // Zona morta: bomba desligada quando nivel >= setpoint - tolerancia
                if (nivel < setpoint - tolerancia)
                {
                    potenciaCalculada = GANHO_PROPORCIONAL * erro;
                }

                potenciaCalculada = std::clamp(potenciaCalculada, 0.0f, 100.0f);
                potenciaBombaAtual = static_cast<uint8_t>(potenciaCalculada + 0.5f);
            }
            else
            {
                potenciaBombaAtual = 0;
            }
        }

        // Entre medicoes, mantem a ultima potencia calculada
        saida.potenciaBomba = potenciaBombaAtual;
    }
    else
    {
        potenciaBombaAtual = 0;
        saida.potenciaBomba = 0;
    }

    bombaLigada = saida.potenciaBomba > 0;

    // Detecta o inicio do acionamento da bomba.
    if (!bombaEstavaLigada && bombaLigada)
    {
        tempoBombaLigada = 0.0f;
        contadorLeiturasSemFluxo = 0;
    }

    saida.estado = estadoAtual;

    // Indicadores conforme o estado.
    switch (estadoAtual)
    {
    case EstadoReservatorio::INICIALIZANDO:
    case EstadoReservatorio::AGUARDANDO:
    case EstadoReservatorio::RECUPERACAO:
        saida.ledAzul = true;
        break;

    case EstadoReservatorio::OPERACAO_NORMAL:
        saida.ledVerde = true;
        break;

    case EstadoReservatorio::FALHA:
        saida.ledVermelho = true;
        saida.buzzer = true;
        break;
    }

    // Sem nova medicao, nao atualiza os contadores de falha.
    if (!entrada.novaLeituraUltrassom)
    {
        saida.nivelPercentual = nivel;
        return saida;
    }

    // Atualiza o tempo de funcionamento da bomba.
    if (bombaLigada)
    {
        tempoBombaLigada += entrada.dt;
    }
    else
    {
        tempoBombaLigada = 0.0f;
        contadorLeiturasSemFluxo = 0;
    }

    // Detecta ausencia de fluxo depois do tempo de partida.
    if (bombaLigada &&
        tempoBombaLigada >= tempoPartidaBomba)
    {
        if (entrada.leituraFluxoValida)
        {
            if (entrada.fluxoLmin < 0.3f)
            {
                if (contadorLeiturasSemFluxo < 3)
                {
                    contadorLeiturasSemFluxo++;
                }
            }
            else
            {
                contadorLeiturasSemFluxo = 0;
            }
        }

        if (contadorLeiturasSemFluxo >= 3)
        {
            estadoAtual = EstadoReservatorio::FALHA;
            bombaLigada = false;

            saida.potenciaBomba = 0;
            saida.estado = EstadoReservatorio::FALHA;
            saida.nivelPercentual = nivel;
            saida.ledVerde = false;
            saida.ledAzul = false;
            saida.ledVermelho = true;
            saida.buzzer = true;

            return saida;
        }
    }

    // Trata leituras invalidas do sensor ultrassonico.
    if (!leituraSegura)
    {
        bombaLigada = false;
        saida.potenciaBomba = 0;

        if (contadorLeiturasInvalidas < 3)
        {
            contadorLeiturasInvalidas++;
        }

        if (contadorLeiturasInvalidas >= 3)
        {
            estadoAtual = EstadoReservatorio::FALHA;
        }

        saida.nivelPercentual = -1.0f;
        saida.estado = estadoAtual;

        saida.ledVerde = false;
        saida.ledAzul = false;
        saida.ledVermelho =
            (estadoAtual == EstadoReservatorio::FALHA);
        saida.buzzer =
            (estadoAtual == EstadoReservatorio::FALHA);

        return saida;
    }

    contadorLeiturasInvalidas = 0;

    saida.nivelPercentual = nivel;

    return saida;
}

void ControladorReservatorio::configurarSetpoint(float valor)
{
    if (valor < 10.0f || valor > 95.0f)
        return;
    setpoint = valor;
}

void ControladorReservatorio::configurarTolerancia(float valor)
{
    if (valor < 0.0f || valor > 100.0f)
        return;

    tolerancia = valor;
}
