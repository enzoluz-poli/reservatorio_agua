#include "controlador_reservatorio.h"

#include <algorithm>

static constexpr float GANHO_PROPORCIONAL = 5.0f;
static constexpr float MARGEM_CM = 2.0f;
static constexpr float LIMIAR_FLUXO_LMIN = 0.3f;
static constexpr uint8_t POTENCIA_TESTE_BOMBA = 50;
static constexpr uint8_t LEITURAS_TESTE_OK = 2;

void ControladorReservatorio::inicializar()
{
    estadoAtual = EstadoReservatorio::INICIALIZANDO;

    setpoint = 70.0f;
    tolerancia = 2.0f;

    bombaLigada = false;
    potenciaBombaAtual = 0;
    contadorLeiturasInvalidas = 0;

    contadorLeiturasSemFluxo = 0;
    contadorLeiturasComFluxo = 0;
    fluxoPendenteVerificacao = false;
    tempoBombaLigada = 0.0f;

    estadoAtual = EstadoReservatorio::AGUARDANDO;
}

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

    // Em falha, so sai com RESET e sensor de nivel funcionando.
    if (estadoAtual == EstadoReservatorio::FALHA)
    {
        bombaLigada = false;
        potenciaBombaAtual = 0;
        saida.potenciaBomba = 0;

        if (entrada.botaoReset &&
            entrada.novaLeituraUltrassom)
        {
            estadoAtual = EstadoReservatorio::RECUPERACAO;
            contadorLeiturasInvalidas = 0;
            contadorLeiturasSemFluxo = 0;
            contadorLeiturasComFluxo = 0;
            tempoBombaLigada = 0.0f;

            saida.estado = EstadoReservatorio::RECUPERACAO;
            saida.nivelPercentual = nivel;
            saida.ledAzul = true;

            return saida;
        }

        saida.estado = EstadoReservatorio::FALHA;
        saida.causaFalha = causaFalha;
        saida.nivelPercentual = nivel;
        saida.ledVermelho = true;
        saida.buzzer = true;

        return saida;
    }

    // Recuperacao: confere o sensor de nivel. Se a ultima falha foi de
    // fluxo, permanece aqui ate o teste da bomba confirmar o fluxo.
    if (estadoAtual == EstadoReservatorio::RECUPERACAO &&
        entrada.novaLeituraUltrassom)
    {
        if (leituraSegura && !fluxoPendenteVerificacao)
        {
            estadoAtual = EstadoReservatorio::AGUARDANDO;
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
        contadorLeiturasComFluxo = 0;
        tempoBombaLigada = 0.0f;
    }

    // Potencia da bomba conforme o estado.
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
    else if (estadoAtual == EstadoReservatorio::RECUPERACAO &&
             fluxoPendenteVerificacao)
    {
        // Teste da bomba: liga com potencia fixa para verificar o fluxo
        potenciaBombaAtual = POTENCIA_TESTE_BOMBA;
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
        contadorLeiturasComFluxo = 0;
    }

    saida.estado = estadoAtual;

    // Subestado visivel da operacao normal (nao altera o controle da bomba)
    if (estadoAtual == EstadoReservatorio::OPERACAO_NORMAL && leituraSegura)
    {
        float limiteAbaixo =
            std::min(0.9f * setpoint, setpoint - tolerancia);

        if (nivel < limiteAbaixo)
        {
            saida.subestado = SubestadoOperacao::ABAIXO_SETPOINT;
        }
        else if (nivel < setpoint - tolerancia)
        {
            saida.subestado = SubestadoOperacao::PROXIMO_SETPOINT;
        }
        else
        {
            saida.subestado = SubestadoOperacao::ESTABILIZADO;
        }
    }

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
        contadorLeiturasComFluxo = 0;
    }

    // Verifica o fluxo depois do tempo de partida da bomba.
    if (bombaLigada &&
        tempoBombaLigada >= tempoPartidaBomba)
    {
        if (entrada.leituraFluxoValida)
        {
            if (entrada.fluxoLmin < LIMIAR_FLUXO_LMIN)
            {
                contadorLeiturasComFluxo = 0;

                if (contadorLeiturasSemFluxo < 3)
                {
                    contadorLeiturasSemFluxo++;
                }
            }
            else
            {
                contadorLeiturasSemFluxo = 0;

                if (contadorLeiturasComFluxo < 255)
                {
                    contadorLeiturasComFluxo++;
                }
            }
        }

        // Ausencia de fluxo: falha (so sai com RESET e novo teste)
        if (contadorLeiturasSemFluxo >= 3)
        {
            estadoAtual = EstadoReservatorio::FALHA;
            fluxoPendenteVerificacao = true;
            causaFalha = CausaFalha::SEM_FLUXO;
            bombaLigada = false;
            potenciaBombaAtual = 0;

            saida.potenciaBomba = 0;
            saida.estado = EstadoReservatorio::FALHA;
            saida.causaFalha = CausaFalha::SEM_FLUXO;
            saida.nivelPercentual = nivel;
            saida.ledVerde = false;
            saida.ledAzul = false;
            saida.ledVermelho = true;
            saida.buzzer = true;

            return saida;
        }

        // Teste de recuperacao aprovado: fluxo confirmado
        if (estadoAtual == EstadoReservatorio::RECUPERACAO &&
            fluxoPendenteVerificacao &&
            contadorLeiturasComFluxo >= LEITURAS_TESTE_OK)
        {
            fluxoPendenteVerificacao = false;
            estadoAtual = EstadoReservatorio::AGUARDANDO;
            bombaLigada = false;
            potenciaBombaAtual = 0;

            saida.potenciaBomba = 0;
            saida.estado = EstadoReservatorio::AGUARDANDO;
            saida.nivelPercentual = nivel;
            saida.ledAzul = true;

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
            causaFalha = CausaFalha::SENSOR_NIVEL;
        }

        saida.nivelPercentual = -1.0f;
        saida.estado = estadoAtual;
        saida.causaFalha =
            (estadoAtual == EstadoReservatorio::FALHA)
                ? causaFalha
                : CausaFalha::NENHUMA;

        saida.ledVerde = false;
        saida.ledAzul = (estadoAtual == EstadoReservatorio::RECUPERACAO);
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
