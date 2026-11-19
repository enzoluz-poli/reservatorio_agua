# Reservatório Inteligente de Água

Projeto de PMR0120 (Introdução a Sistemas Embarcados), Escola Politécnica da Universidade de São Paulo (USP).

Sistema embarcado baseado em ESP32 para monitoramento e controle automático do nível de um reservatório de água. O projeto utiliza controle PI, monitoramento de vazão, detecção de falhas e uma máquina de estados para gerenciar a operação e a recuperação do sistema.

Desenvolvido em C++ com ESP-IDF e FreeRTOS, com simulação no Wokwi.

## 1. Funcionamento

O sistema monitora o nível de água por meio de um sensor ultrassônico HC-SR04 e controla uma bomba utilizando PWM.

A distância medida pelo sensor é convertida em porcentagem de nível:
- 40 cm corresponde a 0%.
- 5 cm corresponde a 100%.
- Uma margem de 2 cm é utilizada na validação das leituras.

O controle utiliza um controlador PI (Proporcional-Integral), com os seguintes parâmetros:
- Setpoint inicial: 70%.
- Tolerância: 2 pontos percentuais.
- Ganho proporcional (Kp): 5,0.
- Ganho integral (Ki): 0,15.
- Potência da bomba: 0 a 100%.

Quando o nível está abaixo da faixa de tolerância, a potência da bomba é calculada em função do erro entre o nível medido e o setpoint. Ao alcançar a faixa de tolerância, a bomba é desligada.

O controlador também utiliza saturação da potência e proteção anti-windup para evitar o acúmulo excessivo do erro integral.

Um sensor de vazão YF-S201 monitora o fluxo enquanto a bomba está ligada. A vazão medida é comparada com a vazão esperada para a potência aplicada.

## 2. Interface com o usuário

O sistema possui um LCD 16x2 com interface I2C e seis botões físicos.

### Telas do LCD

1. **Nível e estado:** apresenta o nível percentual e o estado atual. Durante a operação, também apresenta o subestado; em caso de falha, mostra sua causa.
2. **Bomba:** informa se a bomba está ligada ou desligada e apresenta a potência aplicada.
3. **Ajuste do setpoint:** apresenta o valor atual do setpoint.

O botão MENU alterna entre as três telas.

### Botões

| Botão | Função |
|---|---|
| LIGAR | Inicia a operação automática |
| DESLIGAR | Interrompe a operação |
| RESET | Inicia a recuperação após uma falha |
| MENU | Alterna as telas do LCD |
| + | Aumenta o setpoint em 5 pontos percentuais |
| - | Diminui o setpoint em 5 pontos percentuais |

O setpoint pode variar de 10% a 95%, com valor inicial de 70%. Valores fora desses limites são rejeitados pelo controlador.

## 3. Máquina de estados

O funcionamento do sistema é organizado em cinco estados:

| Estado | Descrição | Indicação |
|---|---|---|
| INICIALIZANDO | Configuração inicial dos periféricos | LED azul |
| AGUARDANDO | Sistema pronto, aguardando comando de operação | LED azul |
| OPERACAO_NORMAL | Controle automático do nível do reservatório | LED verde |
| FALHA | Falha detectada; bomba desligada | LED vermelho e buzzer |
| RECUPERACAO | Verificação das condições para retorno à operação | LED azul |

### Subestados da operação normal

- **ABAIXO_SETPOINT:** nível abaixo de 10 pontos percentuais do setpoint.
- **PROXIMO_SETPOINT:** nível entre o limite anterior e a faixa de tolerância.
- **ESTABILIZADO:** nível dentro da tolerância de 2 pontos percentuais, com a bomba desligada.

## 4. Diagnóstico de falhas

O controlador monitora o funcionamento da bomba e compara a vazão medida com a vazão esperada.

A verificação da vazão começa após 2 segundos de funcionamento da bomba.

Para o modelo utilizado na simulação, considera-se que 100% de PWM corresponde a uma vazão de 5,0 L/min. A faixa aceitável utiliza uma tolerância de ±50% em relação à vazão esperada.

O sistema identifica as seguintes condições:

| Causa | Descrição |
|---|---|
| SENSOR_NIVEL | Três leituras consecutivas inválidas do sensor ultrassônico |
| SEM_FLUXO | Vazão inferior a 0,3 L/min |
| FLUXO_INSUFICIENTE | Vazão abaixo da faixa esperada para a potência aplicada |
| FLUXO_EXCESSIVO | Vazão acima da faixa esperada para a potência aplicada |
| FLUXO_SEM_ENCHIMENTO | Vazão normal, mas sem aumento significativo do nível |
| FALHA_SENSOR_FLUXO | Ausência de pulsos de vazão enquanto o nível continua aumentando |

As falhas relacionadas à vazão são confirmadas após três leituras consecutivas anormais. A condição de vazão normal sem aumento significativo do nível também é monitorada e pode provocar falha após três leituras consecutivas sem enchimento.

Quando uma falha é confirmada:
- A bomba é desligada.
- O LED vermelho é acionado.
- O buzzer é ativado.
- A operação normal fica bloqueada até a recuperação.

## 5. Recuperação

O estado RECUPERACAO é iniciado pelo botão RESET quando o sistema está em FALHA.

O procedimento depende da causa identificada.

### Falha do sensor de nível

O sistema verifica novamente as leituras do sensor ultrassônico. Quando uma leitura válida é recebida, retorna a AGUARDANDO. Caso três leituras consecutivas continuem inválidas, retorna a FALHA.

### Falha relacionada à vazão

Quando existe uma verificação de vazão pendente, a bomba é acionada com potência fixa de 50% para testar o funcionamento.

O sistema retorna a AGUARDANDO após duas leituras consecutivas de vazão dentro da faixa esperada. Caso a condição anormal persista, retorna a FALHA e desliga a bomba.

## 6. Arquitetura de software

O projeto utiliza uma arquitetura dividida em três componentes do ESP-IDF, localizados em `components/`.

### HAL — Hardware Abstraction Layer

Localizada em `components/hal_reservatorio/`, contém os drivers responsáveis pela comunicação com os periféricos:

- `driver_ultrassom.cpp`
- `driver_fluxo.cpp`
- `driver_atuacao.cpp`
- `driver_indicador.cpp`
- `driver_botoes.cpp`
- `driver_display.cpp`

### Application

Localizada em `components/application/`, contém o `ControladorReservatorio`, responsável pela lógica principal do sistema:

- Máquina de estados.
- Controle PI da bomba.
- Cálculo do nível percentual.
- Monitoramento da vazão.
- Diagnóstico de falhas.
- Gerenciamento da recuperação.

A camada de aplicação não acessa diretamente os periféricos. Ela recebe os dados por meio da estrutura `DadosEntrada` e retorna os comandos por meio de `ComandosSaida`.

### Tasks

Localizada em `components/tasks/`, contém as tarefas responsáveis pela execução concorrente do sistema.

**TaskSensores**
- Realiza a aquisição dos dados dos sensores a cada 1 segundo.
- Calcula e registra o intervalo real entre leituras.
- Envia os dados à TaskControle por meio de uma fila FreeRTOS de cinco posições.

**TaskControle**
- Recebe os dados dos sensores.
- Processa os comandos dos botões.
- Executa o controlador.
- Atualiza a bomba, os LEDs, o buzzer e o display.

A comunicação entre as tarefas é realizada por uma fila, evitando o compartilhamento direto dos dados de aquisição.

## 7. Pinagem do ESP32

| Função | GPIO |
|---|---|
| HC-SR04 TRIG | 5 |
| HC-SR04 ECHO | 18 |
| Sensor de vazão (pulsos) | 34 |
| Saída PWM da bomba | 23 |
| Sinal de bomba ligada para os chips simulados | 4 |
| LED verde | 26 |
| LED vermelho | 27 |
| LED azul | 19 |
| Buzzer | 25 |
| LCD SDA | 21 |
| LCD SCL | 22 |
| Botão LIGAR | 32 |
| Botão DESLIGAR | 33 |
| Botão RESET | 16 |
| Botão MENU | 17 |
| Botão + | 13 |
| Botão - | 14 |

## 8. Como executar

### Requisitos

- Visual Studio Code.
- PlatformIO.
- Extensão Wokwi.
- ESP-IDF.

### Compilação

Na raiz do projeto, execute:

```bash
pio run
```

### Simulação

No VS Code, abra a paleta de comandos:

`Ctrl + Shift + P` → `Wokwi: Start Simulator`

### Chips personalizados

A simulação utiliza dois chips personalizados localizados em `chips/`.

**ultrassom.chip.c**
- Simula o sensor ultrassônico.
- Representa o aumento do nível do reservatório quando a bomba está ligada.
- A distância medida diminui durante o funcionamento da bomba.

**pulse.chip.c**
- Simula o sensor de vazão.
- Gera pulsos quando recebe o sinal de bomba ligada.
- Permite alterar a frequência para representar diferentes condições de vazão.

Para simular ausência de vazão, configure a frequência do chip `pulse` para 0 Hz.

Na simulação, um LED amarelo conectado ao GPIO 23 representa visualmente a potência PWM aplicada à bomba.

## 9. Testes funcionais

Os seguintes cenários foram executados e validados no Wokwi:

| Teste | Resultado observado |
|---|---|
| Inicialização do sistema | Aprovado |
| Aquisição e leitura dos sensores | Aprovado |
| Acionamento da bomba | Aprovado |
| Aumento simulado do nível durante o bombeamento | Aprovado |
| Desligamento automático ao alcançar a faixa do setpoint | Aprovado |
| Desligamento manual | Aprovado |
| Detecção de ausência de vazão | Aprovado |
| Desligamento automático após detecção de falha | Aprovado |
| Recuperação após restabelecimento da vazão | Aprovado |

Os testes de recuperação foram realizados após o acionamento do RESET físico do ESP32 no simulador. Isso não significa que todos os tipos de falha e todas as condições de recuperação tenham sido testados individualmente.

## 10. Estrutura do repositório

```text
reservatorio_agua/
│
├── src/
│   └── main.cpp
│
├── components/
│   ├── application/
│   │   ├── controlador_reservatorio.cpp
│   │   └── include/
│   │       └── controlador_reservatorio.h
│   │
│   ├── hal_reservatorio/
│   │   ├── driver_atuacao.cpp
│   │   ├── driver_botoes.cpp
│   │   ├── driver_display.cpp
│   │   ├── driver_fluxo.cpp
│   │   ├── driver_indicador.cpp
│   │   ├── driver_ultrassom.cpp
│   │   └── include/
│   │
│   └── tasks/
│       ├── task_controle.cpp
│       ├── task_sensores.cpp
│       └── include/
│
├── chips/
│   ├── pulse.chip.c
│   ├── pulse.chip.json
│   ├── pulse.chip.wasm
│   ├── ultrassom.chip.c
│   ├── ultrassom.chip.json
│   ├── ultrassom.chip.wasm
│   └── wokwi-api.h
│
├── diagram.json
└── wokwi.toml
```

## 11. Limitações

- O controlador utiliza ação proporcional e integral, sem ação derivativa.
- O modelo da bomba e do sensor de vazão utilizado na simulação é simplificado.
- A simulação não substitui testes com sensores, bomba e reservatório físicos.
- O modelo de vazão deve ser calibrado com os componentes reais antes da utilização em hardware.
- O pino ECHO do HC-SR04 opera em 5 V e exige divisor de tensão para compatibilidade com o ESP32.

---

**Projeto desenvolvido para a disciplina PMR0120 — Introdução a Sistemas Embarcados, Escola Politécnica da Universidade de São Paulo.**