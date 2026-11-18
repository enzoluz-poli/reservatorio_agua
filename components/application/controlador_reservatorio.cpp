
#include "controlador_reservatorio.h"

#include <algorithm>

#include "esp_log.h"

static const char *TAG = "CONTROLADOR";

static constexpr float MARGEM_CM = 2.0f;

// Modelo linear da bomba simulada:
// PWM de 100% corresponde a 5,0 L/min.
static constexpr float VAZAO_POR_PWM = 0.05f;

// Tolerancia de 50% em torno da vazao esperada.
static constexpr float TOLERANCIA_VAZAO = 0.50f;

// Leituras consecutivas para confirmar diagnostico.
static constexpr uint8_t LEITURAS_FALHA_FLUXO = 3;
static constexpr uint8_t LEITURAS_TESTE_OK = 2;

static constexpr uint8_t POTENCIA_TESTE_BOMBA = 50;

// Classifica a vazao medida em relacao a potencia da bomba.
static CausaFalha classificarFluxo(float fluxoLmin, uint8_t potencia)
{
    if (fluxoLmin < 0.3f)
        return CausaFalha::SEM_FLUXO;

    float vazaoEsperada = potencia * VAZAO_POR_PWM;

    float vazaoMinima =
        vazaoEsperada * (1.0f - TOLERANCIA_VAZAO);

    float vazaoMaxima =
        vazaoEsperada * (1.0f + TOLERANCIA_VAZAO);

    if (fluxoLmin < vazaoMinima)
        return CausaFalha::FLUXO_INSUFICIENTE;

    if (fluxoLmin > vazaoMaxima)
        return CausaFalha::FLUXO_EXCESSIVO;

    return CausaFalha::NENHUMA;
}

void ControladorReservatorio::inicializar()
{
    estadoAtual = EstadoReservatorio::INICIALIZANDO;

    setpoint = 70.0f;
    tolerancia = 2.0f;

    erroIntegral = 0.0f;

    bombaLigada = false;
    potenciaBombaAtual = 0;

    contadorLeiturasInvalidas = 0;

    contadorLeiturasSemFluxo = 0;
    contadorLeiturasNivelSubindoSemFluxo = 0;
    contadorLeiturasFluxoForaFaixa = 0;
    contadorLeiturasComFluxo = 0;
    nivelAnteriorDiagnostico = 0.0f;
    nivelAnteriorValido = false;
    contadorLeiturasSemEnchimento = 0;

    fluxoPendenteVerificacao = false;
    causaFluxoDetectada = CausaFalha::NENHUMA;

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

    // Em falha, so sai com RESET e uma nova leitura do sensor.
    if (estadoAtual == EstadoReservatorio::FALHA)
    {
        bombaLigada = false;
        potenciaBombaAtual = 0;
        erroIntegral = 0.0f;

        saida.potenciaBomba = 0;

        if (entrada.botaoReset &&
            entrada.novaLeituraUltrassom)
        {
            estadoAtual = EstadoReservatorio::RECUPERACAO;

            contadorLeiturasInvalidas = 0;
            contadorLeiturasSemFluxo = 0;
            contadorLeiturasNivelSubindoSemFluxo = 0;
            nivelAnteriorDiagnostico = 0.0f;
            nivelAnteriorValido = false;
            contadorLeiturasSemEnchimento = 0;
            contadorLeiturasFluxoForaFaixa = 0;
            contadorLeiturasComFluxo = 0;

            causaFluxoDetectada = CausaFalha::NENHUMA;

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

    // Recuperacao: confere o sensor de nivel.
    // Se a ultima falha foi de fluxo, aguarda o teste da bomba.
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
        erroIntegral = 0.0f;
    }

    // Desliga o sistema quando solicitado.
    if (estadoAtual == EstadoReservatorio::OPERACAO_NORMAL &&
        entrada.botaoDesliga)
    {
        estadoAtual = EstadoReservatorio::AGUARDANDO;

        bombaLigada = false;
        erroIntegral = 0.0f;

        contadorLeiturasSemFluxo = 0;
        contadorLeiturasNivelSubindoSemFluxo = 0;
        nivelAnteriorDiagnostico = 0.0f;
        nivelAnteriorValido = false;
        contadorLeiturasSemEnchimento = 0;
        contadorLeiturasFluxoForaFaixa = 0;
        contadorLeiturasComFluxo = 0;

        tempoBombaLigada = 0.0f;
    }

    // Controle PI da bomba.
    if (estadoAtual == EstadoReservatorio::OPERACAO_NORMAL)
    {
        if (entrada.novaLeituraUltrassom)
        {
            if (leituraSegura)
            {
                float erro = setpoint - nivel;

                if (erro > tolerancia)
                {
                    float dt = entrada.dt;

                    // Protege contra intervalos invalidos ou excessivos.
                    dt = std::clamp(dt, 0.0f, 1.0f);

                    // Atualiza a parcela integral.
                    float integralAnterior = erroIntegral;
                    erroIntegral += erro * dt;

                    // Limita o acumulador integral.
                    erroIntegral = std::clamp(erroIntegral, 0.0f, 300.0f);

                    float potenciaCalculada =
                        ganhoProporcional * erro +
                        ganhoIntegral * erroIntegral;

                    // Anti-windup simples.
                    if (potenciaCalculada > 100.0f)
                    {
                        erroIntegral = integralAnterior;

                        potenciaCalculada =
                            ganhoProporcional * erro +
                            ganhoIntegral * erroIntegral;
                    }

                    potenciaCalculada =
                        std::clamp(potenciaCalculada, 0.0f, 100.0f);

                    potenciaBombaAtual =
                        static_cast<uint8_t>(potenciaCalculada + 0.5f);
                }
                else
                {
                    // Dentro da tolerancia, desliga a bomba.
                    potenciaBombaAtual = 0;
                    erroIntegral = 0.0f;
                }
            }
            else
            {
                potenciaBombaAtual = 0;
                erroIntegral = 0.0f;
            }
        }

        // Entre medicoes, mantem a ultima potencia calculada.
        saida.potenciaBomba = potenciaBombaAtual;
    }
    else if (estadoAtual == EstadoReservatorio::RECUPERACAO &&
             fluxoPendenteVerificacao)
    {
        // Teste da bomba com potencia fixa.
        potenciaBombaAtual = POTENCIA_TESTE_BOMBA;
        saida.potenciaBomba = potenciaBombaAtual;
    }
    else
    {
        potenciaBombaAtual = 0;
        saida.potenciaBomba = 0;

        if (estadoAtual != EstadoReservatorio::RECUPERACAO)
        {
            erroIntegral = 0.0f;
        }
    }

    bombaLigada = saida.potenciaBomba > 0;

    // Detecta o inicio do acionamento da bomba.
    if (!bombaEstavaLigada && bombaLigada)
    {
        tempoBombaLigada = 0.0f;

        contadorLeiturasSemFluxo = 0;
        contadorLeiturasNivelSubindoSemFluxo = 0;
        nivelAnteriorDiagnostico = 0.0f;
        nivelAnteriorValido = false;
        contadorLeiturasSemEnchimento = 0;
        contadorLeiturasFluxoForaFaixa = 0;
        contadorLeiturasComFluxo = 0;
    }

    saida.estado = estadoAtual;

    // Subestado visivel da operacao normal.
    if (estadoAtual == EstadoReservatorio::OPERACAO_NORMAL && leituraSegura)
    {
        float limiteAbaixo = setpoint - 10.0f;

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

    // Atualiza o tempo real de funcionamento da bomba.
    if (bombaEstavaLigada && bombaLigada)
    {
        tempoBombaLigada += entrada.dt;
    }
    else if (!bombaLigada)
    {
        tempoBombaLigada = 0.0f;
        nivelAnteriorDiagnostico = 0.0f;
        nivelAnteriorValido = false;
        contadorLeiturasSemEnchimento = 0;
        contadorLeiturasNivelSubindoSemFluxo = 0;

        contadorLeiturasSemFluxo = 0;
        contadorLeiturasFluxoForaFaixa = 0;
        contadorLeiturasComFluxo = 0;
        causaFluxoDetectada = CausaFalha::NENHUMA;
    }

    // Verifica a vazao depois do tempo de partida da bomba.
    if (bombaLigada &&
        tempoBombaLigada >= tempoPartidaBomba &&
        entrada.leituraFluxoValida)
    {
        CausaFalha diagnostico =
            classificarFluxo(entrada.fluxoLmin, potenciaBombaAtual);

        ESP_LOGI(TAG,
                 "Diagnostico: bomba=%d | potencia=%u%% | tempo=%.2fs | fluxo=%.2f L/min | valid=%d | contador=%u",
                 bombaLigada,
                 potenciaBombaAtual,
                 tempoBombaLigada,
                 entrada.fluxoLmin,
                 entrada.leituraFluxoValida,
                 contadorLeiturasFluxoForaFaixa);

        if (diagnostico == CausaFalha::NENHUMA)
        {
            // Vazao compativel com a potencia solicitada.
            contadorLeiturasSemFluxo = 0;
            contadorLeiturasFluxoForaFaixa = 0;
            contadorLeiturasNivelSubindoSemFluxo = 0;
            // Verifica se a vazao normal esta produzindo aumento do nivel.
            if (!nivelAnteriorValido)
            {
                nivelAnteriorDiagnostico = nivel;
                nivelAnteriorValido = true;
                contadorLeiturasSemEnchimento = 0;
            }
            else
            {
                float aumentoNivel = nivel - nivelAnteriorDiagnostico;

                if (aumentoNivel < 0.5f)
                {
                    if (contadorLeiturasSemEnchimento < 255)
                    {
                        contadorLeiturasSemEnchimento++;
                    }
                }
                else
                {
                    contadorLeiturasSemEnchimento = 0;
                }

                nivelAnteriorDiagnostico = nivel;
            }

            if (contadorLeiturasComFluxo < 255)
            {
                contadorLeiturasComFluxo++;
            }

            causaFluxoDetectada = CausaFalha::NENHUMA;
        }
        else
        {
            // Vazao anormal: reinicia a contagem de leituras boas.
            contadorLeiturasComFluxo = 0;
            contadorLeiturasSemEnchimento = 0;

            if (contadorLeiturasFluxoForaFaixa < 255)
            {
                contadorLeiturasFluxoForaFaixa++;
            }

            if (diagnostico == CausaFalha::SEM_FLUXO)
            {
                if (contadorLeiturasSemFluxo < 255)
                {
                    contadorLeiturasSemFluxo++;
                }

                // Compara o nivel mesmo sem pulsos no sensor de vazao.
                if (!nivelAnteriorValido)
                {
                    nivelAnteriorDiagnostico = nivel;
                    nivelAnteriorValido = true;
                    contadorLeiturasNivelSubindoSemFluxo = 0;
                }
                else
                {
                    float aumentoNivel = nivel - nivelAnteriorDiagnostico;

                    if (aumentoNivel >= 0.5f)
                    {
                        if (contadorLeiturasNivelSubindoSemFluxo < 255)
                        {
                            contadorLeiturasNivelSubindoSemFluxo++;
                        }
                    }
                    else
                    {
                        contadorLeiturasNivelSubindoSemFluxo = 0;
                    }

                    nivelAnteriorDiagnostico = nivel;
                }

                causaFluxoDetectada =
                    contadorLeiturasNivelSubindoSemFluxo >= 2
                        ? CausaFalha::FALHA_SENSOR_FLUXO
                        : CausaFalha::SEM_FLUXO;
            }
            else
            {
                contadorLeiturasSemFluxo = 0;
                contadorLeiturasNivelSubindoSemFluxo = 0;
                causaFluxoDetectada = diagnostico;
                nivelAnteriorValido = false;
            }
        }

        // Tres leituras consecutivas anormais confirmam a falha.
        if (contadorLeiturasFluxoForaFaixa >= LEITURAS_FALHA_FLUXO)
        {
            estadoAtual = EstadoReservatorio::FALHA;

            fluxoPendenteVerificacao = true;
            causaFalha = causaFluxoDetectada;

            bombaLigada = false;
            potenciaBombaAtual = 0;
            erroIntegral = 0.0f;

            saida.potenciaBomba = 0;
            saida.estado = EstadoReservatorio::FALHA;
            saida.causaFalha = causaFalha;
            saida.nivelPercentual = nivel;

            saida.ledVerde = false;
            saida.ledAzul = false;
            saida.ledVermelho = true;
            saida.buzzer = true;

            return saida;
        }

        // Vazao normal sem aumento do nivel: possivel falha de enchimento.
        if (bombaLigada &&
            contadorLeiturasSemEnchimento >= 3)
        {
            estadoAtual = EstadoReservatorio::FALHA;

            // Esta falha exige reset e nova operacao para reavaliacao.
            fluxoPendenteVerificacao = false;
            causaFalha = CausaFalha::FLUXO_SEM_ENCHIMENTO;

            bombaLigada = false;
            potenciaBombaAtual = 0;
            erroIntegral = 0.0f;

            saida.potenciaBomba = 0;
            saida.estado = EstadoReservatorio::FALHA;
            saida.causaFalha = causaFalha;
            saida.nivelPercentual = nivel;

            saida.ledVerde = false;
            saida.ledAzul = false;
            saida.ledVermelho = true;
            saida.buzzer = true;

            return saida;
        }

        // Recuperacao aprovada: duas leituras normais consecutivas.
        if (estadoAtual == EstadoReservatorio::RECUPERACAO &&
            fluxoPendenteVerificacao &&
            contadorLeiturasComFluxo >= LEITURAS_TESTE_OK)
        {
            fluxoPendenteVerificacao = false;

            estadoAtual = EstadoReservatorio::AGUARDANDO;

            bombaLigada = false;
            potenciaBombaAtual = 0;
            erroIntegral = 0.0f;

            contadorLeiturasComFluxo = 0;
            contadorLeiturasFluxoForaFaixa = 0;
            causaFluxoDetectada = CausaFalha::NENHUMA;

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
        potenciaBombaAtual = 0;
        erroIntegral = 0.0f;

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
        saida.ledVermelho = (estadoAtual == EstadoReservatorio::FALHA);
        saida.buzzer = (estadoAtual == EstadoReservatorio::FALHA);

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
    erroIntegral = 0.0f;
}
