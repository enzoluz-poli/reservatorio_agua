# Reservatório Inteligente de Água

Projeto de PMR0120 (Introdução a Sistemas Embarcados), Escola Politécnica da USP.
Sistema embarcado em ESP32 que mantém o nível de uma caixa d'água em um
setpoint ajustável, com detecção de falhas. Executa em FreeRTOS (ESP-IDF,
C++) e é simulado no Wokwi.

## Funcionamento

- O nível é medido por um sensor ultrassônico HC-SR04 e convertido em
  porcentagem (distância de 40 cm = 0%, 5 cm = 100%, com margem de 2 cm).
- A bomba é comandada por PWM (0 a 100%) com controle proporcional (ganho 5)
  e zona morta: liga quando o nível fica abaixo de (setpoint - 2%) e desliga
  ao alcançá-lo.
- Um sensor de vazão YF-S201 confirma que há fluxo quando a bomba está ligada.
- O LCD 16x2 (I2C) tem 3 telas (botão MENU): nível e estado, bomba e
  ajuste do setpoint.
- Os botões LIGAR, DESLIGAR, RESET, MENU, + e - controlam o sistema.

## Estados

| Estado          | Descrição                                          | Indicação             |
|-----------------|----------------------------------------------------|-----------------------|
| INICIALIZANDO   | Configuração dos periféricos                       | LED azul              |
| AGUARDANDO      | Pronto, bomba desligada, espera o LIGAR            | LED azul              |
| OPERACAO_NORMAL | Controla o nível em torno do setpoint              | LED verde             |
| FALHA           | Leitura inválida ou ausência de fluxo              | LED vermelho + buzzer |
| RECUPERACAO     | Após RESET; ver abaixo                             | LED azul              |

Subestados da operação normal (mostrados no LCD):
- Abaixo do setpoint: nível abaixo de 90% do setpoint (ou de setpoint - 2%,
  o que for menor); a bomba trabalha com potência alta.
- Próximo do setpoint: entre esse limite e (setpoint - 2%); a potência
  diminui conforme o erro diminui.
- Estabilizado: a partir de (setpoint - 2%); bomba desligada.

Condições de falha (o LCD mostra a causa):
- FALHA: SENSOR: 3 leituras consecutivas inválidas do ultrassom (fora da faixa).
- FALHA: SEM FLUXO: bomba ligada há mais de 2 s e 3 leituras consecutivas de
  vazão abaixo de 0,3 L/min.

Recuperação (botão RESET em FALHA):
- O sistema entra em RECUPERACAO e a bomba permanece desligada, exceto no
  teste de vazão descrito abaixo.
- Falha de sensor: se o nível voltar a ser lido corretamente, vai a AGUARDANDO.
  Se 3 leituras consecutivas continuarem inválidas, retorna a FALHA.
- Falha de vazão: a bomba é ligada a 50% como teste e o sistema só volta a
  AGUARDANDO se o fluxo for confirmado em 2 leituras. Se o problema
  persistir, retorna a FALHA e o LIGAR continua bloqueado.

## Arquitetura

O código está dividido em 3 componentes do ESP-IDF, em `components/`:

- `hal_reservatorio/`: drivers de hardware (DriverUltrassom, DriverFluxo,
  DriverAtuacao, DriverIndicador, DriverBotoes, DriverDisplay).
- `application/`: ControladorReservatorio, a máquina de estados, o controle
  da bomba e a detecção de falhas. Não acessa o hardware diretamente: recebe
  `DadosEntrada` e devolve `ComandosSaida`.
- `tasks/`: tarefas FreeRTOS.

Tarefas:
- TaskSensores: lê o ultrassom e o sensor de vazão a cada 1 s e envia os
  dados (inclusive o intervalo real entre leituras) à TaskControle por uma
  fila de 5 posições.
- TaskControle: recebe a fila, lê os botões, executa o controlador e comanda
  a bomba, os LEDs, o buzzer e o display.

## Pinos (ESP32)

| Função                                  | GPIO              |
|-----------------------------------------|-------------------|
| HC-SR04 TRIG / ECHO                     | 5 / 18            |
| Sensor de vazão (pulsos)                | 34                |
| Bomba (PWM)                             | 23                |
| Sinal "bomba ligada" (gerador de pulsos)| 4                 |
| LED verde / vermelho / azul             | 26 / 27 / 19      |
| Buzzer                                  | 25                |
| LCD I2C SDA / SCL (endereço 0x27)       | 21 / 22           |
| Botões LIGAR / DESLIGAR / RESET / MENU  | 32 / 33 / 16 / 17 |
| Botões + / -                            | 13 / 14           |

## Como executar

Requisitos: VSCode com as extensões PlatformIO e Wokwi (framework ESP-IDF).

    pio run                                   # compila
    Ctrl+Shift+P > Wokwi: Start Simulator     # simula

No simulador, o nível é alterado pelo controle deslizante do HC-SR04. A vazão
é gerada por um chip customizado do Wokwi (`chips/`), que só emite pulsos
quando a bomba está ligada; para simular falha, ajuste sua frequência a 0 e
reinicie a simulação. O LED amarelo no GPIO 23 representa a potência da bomba.

## Uso

1. LIGAR: inicia a operação (LED verde).
2. MENU: alterna as telas do LCD.
3. + e -: alteram o setpoint de 5 em 5 (limites de 10% a 95%).
4. DESLIGAR: volta para AGUARDANDO.
5. RESET: inicia a recuperação a partir de FALHA.

## Estrutura do repositório

    src/main.cpp      cria a fila e as tarefas
    components/       camadas hal_reservatorio, application e tasks
    chips/            chip de pulsos do Wokwi (sensor de vazão)
    diagram.json      circuito do Wokwi
    wokwi.toml        configuração da simulação

Branches: `main` (versão final), `develop` (integração) e `feature/*`.

## Limitações

- O controle é proporcional com zona morta, sem ação integral ou derivativa.
- No simulador o nível é definido manualmente, então a bomba não altera o
  nível medido.
- Em hardware real, o ECHO do HC-SR04 (5 V) exige divisor de tensão para o
  ESP32

