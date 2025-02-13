# Ligeirinho: Jogo de Reflexo

## Descrição
O **Ligeirinho** é um jogo de reflexo desenvolvido como parte do projeto **Embarcatech**, um programa de capacitação profissional técnica destinado a alunos de nível superior em Tecnologias da Informação e Comunicação (TIC) e áreas correlatas, com foco em tecnologias de Sistemas Embarcados. 

Este projeto utiliza a placa **BitDogLab**, que é baseada no microcontrolador **RP2040** da Raspberry Pi Pico W. O objetivo do jogo é medir o tempo de reação do jogador, que deve pressionar um botão assim que um LED acender. O projeto foi desenvolvido em linguagem **C** e utiliza componentes como LEDs, botões, um buzzer e um display OLED para fornecer feedback visual e sonoro ao jogador.

## Materiais Necessários
- Placa BitDogLab (com microcontrolador RP2040)
- Fonte de alimentação (USB ou bateria)
- Botões
- LEDs (verde e vermelho)
- Buzzer
- Display OLED
- Jumpers e protoboard

## Esquemático de Conexões com a BitDogLab

| Pino do Componente | Pino do Raspberry Pi Pico W |
|---------------------|-----------------------------|
| VCC (LEDs, Buzzer) | 3.3V                        |
| GND (LEDs, Buzzer)  | GND                         |
| LED Verde           | GP11                        |
| LED Vermelho        | GP13                        |
| Buzzer              | GP21                        |
| Botão A (Start)     | GP5                         |
| Botão B (Stop)      | GP6                         |
| I2C SDA (OLED)      | GP14                        |
| I2C SCL (OLED)      | GP15                        |

# Explicação do Código
1. O código começa configurando os pinos GPIO, inicializando a comunicação I2C para o display OLED e configurando o PWM para o buzzer.
2. Os botões são lidos para detectar quando o jogador inicia o jogo ou marca o tempo de reação.
3. Um temporizador é iniciado quando o LED vermelho acende, e o tempo de reação é calculado quando o jogador pressiona o botão B.
4. O display OLED exibe mensagens de preparação e o tempo de reação, enquanto o buzzer emite um som quando o LED vermelho acende.
5. O jogo continua em um loop infinito, permitindo que o jogador jogue várias vezes.

# Testando o Circuito

1. Conecte a BitDogLab ao computador via USB.
2. Compile e carregue o código usando o SDK do Raspberry Pi Pico.
3. Conecte os componentes conforme o esquemático fornecido.
4. Pressione o botão A para iniciar o jogo.
5. Aguarde o LED vermelho acender e pressione o botão B o mais rápido possível.
6. Observe o tempo de reação exibido no display OLED.

# Conclusão

Este projeto demonstra como utilizar o microcontrolador RP2040 da Raspberry Pi Pico W para criar um jogo de reflexo interativo. Através da integração de componentes como LEDs, botões, buzzer e display OLED, o projeto oferece uma experiência prática de desenvolvimento de sistemas embarcados. 

O conhecimento adquirido pode ser aplicado em diversos projetos, como robótica, automação e dispositivos interativos.
