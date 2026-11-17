#include "controlador_reservatorio.h"

void ControladorReservatorio::inicializar()
{
    estadoAtual = EstadoReservatorio::INICIALIZANDO;

    setpoint = 70.0f;
    tolerancia = 5.0f;

    bombaLigada = false;
    contadorLeiturasInvalidas = 0;

    contadorLeiturasSemFluxo = 0;
    tempoBombaLigada = 0.0f;

    // Inicializacao concluida: aguarda o comando do usuario
    estadoAtual = EstadoReservatorio::AGUARDANDO;
}

float ControladorReservatorio::calcularNivelPercentual(float distanciaCm) const
{
    // Verifica se a leitura esta dentro da faixa calibrada
    if (distanciaCm < distanciaNivelCheio ||
        distanciaCm > distanciaNivelVazio)
    {
        return -1.0f;
    }

    // Converte distancia em nivel percentual
    float nivel = (distanciaNivelVazio - distanciaCm) /
                  (distanciaNivelVazio - distanciaNivelCheio) * 100.0f;

    return nivel;
}

ComandosSaida ControladorReservatorio::atualizar(const DadosEntrada &entrada)
{
    ComandosSaida saida{};
    bool bombaEstavaLigada = bombaLigada;

    saida.potenciaBomba = bombaLigada ? 100 : 0;

    // Mantem o sistema em falha ate receber um comando de recuperacao
    // Verifica se o sensor apresenta uma leitura segura
    bool leituraSegura = false;

    if (entrada.leituraUltrassomValida)
    {
        leituraSegura =
            calcularNivelPercentual(entrada.distanciaCm) >= 0.0f;
    }

    // Mantem a falha ate receber RESET com sensor funcionando
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

    // Inicia a operacao quando o sistema esta aguardando
    if (estadoAtual == EstadoReservatorio::AGUARDANDO &&
        entrada.botaoLiga)
    {
        estadoAtual = EstadoReservatorio::OPERACAO_NORMAL;
    }

    // Retorna ao estado de espera quando o usuario desliga
    if (estadoAtual == EstadoReservatorio::OPERACAO_NORMAL &&
        entrada.botaoDesliga)
    {
        estadoAtual = EstadoReservatorio::AGUARDANDO;
        bombaLigada = false;
        contadorLeiturasSemFluxo = 0;
        tempoBombaLigada = 0.0f;
    }

    saida.estado = estadoAtual;

    // Define os indicadores conforme o estado do sistema
    switch (estadoAtual)
    {
    case EstadoReservatorio::INICIALIZANDO:
        saida.ledAzul = true;
        break;

    case EstadoReservatorio::AGUARDANDO:
        saida.ledAzul = true;
        break;

    case EstadoReservatorio::OPERACAO_NORMAL:
        saida.ledVerde = true;
        break;

    case EstadoReservatorio::FALHA:
        saida.ledVermelho = true;
        saida.buzzer = true;
        break;

    case EstadoReservatorio::RECUPERACAO:
        saida.ledAzul = true;
        break;
    }

    // Calcula o nivel somente quando houver uma nova medicao
    float nivel = -1.0f;

    if (entrada.leituraUltrassomValida)
    {
        nivel = calcularNivelPercentual(entrada.distanciaCm);
    }

    // Controle da bomba por histerese
    if (estadoAtual == EstadoReservatorio::OPERACAO_NORMAL &&
        entrada.novaLeituraUltrassom &&
        nivel >= 0.0f)
    {
        if (nivel < setpoint - tolerancia)
        {
            bombaLigada = true;
        }
        else if (nivel >= setpoint)
        {
            bombaLigada = false;
        }
    }

    if (!bombaEstavaLigada && bombaLigada)
    {
        tempoBombaLigada = 0.0f;
        contadorLeiturasSemFluxo = 0;
    }

    saida.potenciaBomba = bombaLigada ? 100 : 0;

    // Sem nova medicao, nao atualiza o contador de falhas
    if (!entrada.novaLeituraUltrassom)
    {
        saida.estado = estadoAtual;
        saida.nivelPercentual = nivel;
        return saida;
    }

    if (bombaLigada)
    {
        tempoBombaLigada += entrada.dt;
    }
    else
    {
        tempoBombaLigada = 0.0f;
        contadorLeiturasSemFluxo = 0;
    }

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
            saida.ledVermelho = true;
            saida.buzzer = true;

            return saida;
        }
    }
    // Verifica se a leitura e a distancia estao dentro da faixa valida
    if (nivel < 0.0f)
    {
        // Desliga a bomba imediatamente por seguranca
        bombaLigada = false;
        saida.potenciaBomba = 0;

        if (contadorLeiturasInvalidas < 3)
        {
            contadorLeiturasInvalidas++;
        }

        if (contadorLeiturasInvalidas >= 3)
        {
            estadoAtual = EstadoReservatorio::FALHA;
            bombaLigada = false;
            saida.potenciaBomba = 0;
        }

        saida.nivelPercentual = -1.0f;
        saida.estado = estadoAtual;

        saida.ledVerde = false;
        saida.ledAzul = false;
        saida.ledVermelho = (estadoAtual == EstadoReservatorio::FALHA);
        saida.buzzer = (estadoAtual == EstadoReservatorio::FALHA);

        return saida;
    }

    // Leitura valida: reinicia o contador
    contadorLeiturasInvalidas = 0;

    saida.nivelPercentual = nivel;

    return saida;
}

void ControladorReservatorio::configurarSetpoint(float valor)
{
    if (valor < 0.0f || valor > 100.0f)
        return;

    setpoint = valor;
}

void ControladorReservatorio::configurarTolerancia(float valor)
{
    if (valor < 0.0f || valor > 100.0f)
        return;

    tolerancia = valor;
}