# Projetos de Sistemas Embarcados - EmbarcaTech 2025

Autor: **Andre de Oliveira Melo**

Curso: Residência Tecnológica em Sistemas Embarcados

Instituição: EmbarcaTech - HBr

Brasilia, junho de 2025

---

# Sintetizador de Áudio para Raspberry Pi Pico

Este projeto implementa um sintetizador de áudio para o Raspberry Pi Pico, capaz de gravar e reproduzir áudio utilizando um microfone e dois buzzers para saída estéreo. O sistema também inclui um display OLED para feedback visual e LEDs para indicação de status.

## Características

- Gravação de áudio via microfone ADC (até 5 segundos a 22kHz)
- Reprodução de áudio através de dois buzzers controlados por PWM
- Interface visual usando display OLED SSD1306
- Visualização de forma de onda em tempo real
- Análise de áudio (valor médio, amplitude)
- Ganho adaptativo para melhorar a qualidade do áudio
- Interface com dois botões para controle
- Indicadores LED para status de operação

## Hardware necessário

- Raspberry Pi Pico
- Microfone com saída analógica (conectado ao ADC2/GPIO28)
- 2x Buzzers/alto-falantes (conectados aos GPIOs 21 e 10)
- Display OLED SSD1306 (I2C, endereço 0x3C)
- 2x Botões com pull-up (GPIOs 5 e 6)
- 2x LEDs com resistores (GPIOs 11 e 13)

## Conexões

- **Microfone**: ADC2 (GPIO28)
- **Buzzers**: GPIO21 e GPIO10
- **Botões**:
  - Botão A (Gravar): GPIO5
  - Botão B (Reproduzir): GPIO6
- **LEDs**:
  - LED Verde: GPIO11
  - LED Vermelho: GPIO13
- **Display OLED**:
  - SDA: GPIO14
  - SCL: GPIO15

## Compilação e Upload

Este projeto utiliza o SDK do Raspberry Pi Pico com CMake para compilação:

1. Configure o ambiente de desenvolvimento para o Pico SDK
2. Clone este repositório
3. Crie uma pasta build e navegue até ela:
   ```
   mkdir build && cd build
   ```
4. Execute o CMake:
   ```
   cmake ..
   ```
5. Compile o projeto:
   ```
   make
   ```
6. Conecte o Pico em modo bootloader e copie o arquivo `.uf2` gerado

## Uso

1. Ao iniciar, o sistema mostra uma tela de boas-vindas e realiza um teste de som
2. Use o **Botão A** para iniciar a gravação (LED vermelho acenderá)
3. Use o **Botão B** durante a gravação para interrompê-la antes do tempo máximo
4. Após a gravação, o sistema mostra estatísticas e a forma de onda do áudio capturado
5. Use o **Botão B** para reproduzir o áudio gravado (LED verde acenderá)
6. Use o **Botão A** durante a reprodução para interrompê-la

## Arquivos do projeto

- [`sintetizador.c`](sintetizador.c): Código principal do projeto
- [`ssd1306/ssd1306.c`](ssd1306/ssd1306.c): Biblioteca para controle do display OLED
- [`CMakeLists.txt`](CMakeLists.txt): Configuração de compilação do projeto

## Características técnicas

- Taxa de amostragem: 22kHz
- Resolução ADC: 12 bits
- Frequência PWM: 88kHz
- Resolução PWM: 10 bits (0-1023)
- Tempo máximo de gravação: 5 segundos

## Dependências

- Pico SDK
- Biblioteca SSD1306 para display OLED

---

## 📜 Licença
GNU GPL-3.0.
