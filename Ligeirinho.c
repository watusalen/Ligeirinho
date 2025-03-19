/**
 * @file buzzer_pwm1.c
 * @brief Firmware para o jogo de reflexo "Ligeirinho" utilizando a placa BitDogLab.
 *
 * Este firmware mede o tempo de reação do jogador. O jogo inicia quando o botão A
 * (conectado ao GPIO5) é pressionado e, em seguida, aguarda um tempo aleatório para
 * acionar o LED (controlado via PWM com 50% de duty cycle) e emitir um som com o buzzer
 * (via PWM no GPIO21). O jogador deve pressionar o botão B (GPIO6) para capturar seu tempo de
 * reação. O display OLED exibe mensagens de status.
 *
 * Melhorias implementadas:
 *  - O acionamento dos LEDs (LED_GREEN e LED_RED) agora é feito via PWM com duty cycle fixo de 50%,
 *    reduzindo seu brilho para metade do valor máximo.
 *  - O buzzer continua operando via PWM conforme implementado originalmente.
 *
 * @author
 * @date 06/12/2024 (Atualizado para controle PWM dos LEDs)
 */

#include <stdio.h>           // Biblioteca padrão de entrada e saída
#include <stdlib.h>          // Biblioteca para manipulação de memória e funções rand()
#include <string.h>          // Biblioteca para manipulação de strings
#include "pico/stdlib.h"     // Biblioteca padrão do Raspberry Pi Pico
#include "hardware/timer.h"  // Biblioteca para manipulação de timers no RP2040
#include "hardware/pwm.h"    // Biblioteca para controle de PWM
#include "hardware/clocks.h" // Biblioteca para manipulação de clocks
#include "hardware/gpio.h"   // Biblioteca para manipulação de GPIOs
#include "hardware/i2c.h"    // Biblioteca para comunicação I2C
#include "inc/ssd1306.h"     // Biblioteca para comunicação com o display OLED

// Definição dos pinos utilizados no projeto
#define BUTTON_START 5 // Botão A - Inicia o jogo
#define BUTTON_STOP 6  // Botão B - Captura o tempo de reação
#define LED_GREEN 11   // LED Verde - Indica preparação (via PWM)
#define LED_RED 13     // LED Vermelho - Indica reação (via PWM)
#define BUZZER 21      // Buzzer para emitir som ao acionar o LED vermelho
#define I2C_SDA 14     // Pino SDA para o display OLED
#define I2C_SCL 15     // Pino SCL para o display OLED

// Definição dos parâmetros para o PWM dos LEDs
#define LED_PWM_WRAP 1000         /**< Valor de wrap do PWM para os LEDs (define o período) */
#define LED_ON (LED_PWM_WRAP / 8) /**< Nível de PWM para 50% do duty cycle (LED aceso com brilho reduzido) */

// Variáveis globais para controle do jogo
bool game_running = false;                  /**< Indica se o jogo está em execução */
bool reaction_phase = false;                /**< Indica se o jogador deve reagir */
absolute_time_t start_time, reaction_time;  /**< Armazena os tempos de início e de reação */
volatile bool buzzer_active = false;        /**< Indica se o buzzer está ativo */
volatile bool false_start_detected = false; /**< Indica se houve uma queima de largada */
volatile bool button_b_pressed = false;     /**< Indica se o botão B foi pressionado */

/**
 * @brief Exibe um texto no display OLED, quebrando linhas automaticamente.
 *
 * Cada linha comporta até 15 caracteres. Essa função utiliza as funções da
 * biblioteca ssd1306 para desenhar strings e renderizar o conteúdo no display.
 *
 * @param text Mensagem a ser exibida no display.
 */
void display_text(const char *text)
{
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1};

    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    int y = 0;
    int line_len = 15;
    char line_buffer[16];
    int text_len = strlen(text);

    for (int i = 0; i < text_len; i += line_len)
    {
        strncpy(line_buffer, text + i, line_len);
        line_buffer[line_len] = '\0';
        ssd1306_draw_string(ssd, 2, y, line_buffer);
        y += 8;
        if (y >= ssd1306_height)
            break;
    }

    render_on_display(ssd, &frame_area);
}

/**
 * @brief Inicializa o PWM no pino do buzzer.
 *
 * Configura o pino especificado para funcionar como saída PWM, define
 * o divisor de clock e inicializa o PWM com duty cycle 0.
 *
 * @param pin Pino do buzzer.
 */
void pwm_init_buzzer(uint pin)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); // Ajusta divisor de clock
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente
}

/**
 * @brief Inicializa o PWM para controle de um LED.
 *
 * Configura o pino especificado para funcionar como saída PWM, define
 * um período fixo (wrap) e deixa o LED inicialmente desligado.
 *
 * @param pin Pino do LED.
 */
void pwm_init_led(uint pin)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_init(slice_num, &config, true);
    pwm_set_wrap(slice_num, LED_PWM_WRAP);
    pwm_set_gpio_level(pin, 0); // LED desligado inicialmente
}

/**
 * @brief Callback chamado para desligar o buzzer após um tempo determinado.
 *
 * Esta função é chamada pelo sistema de alarmes para interromper o som no buzzer.
 *
 * @param id Identificador do alarme (não utilizado)
 * @param user_data Ponteiro para dados do usuário (não utilizado)
 * @return int64_t 0, para indicar que não há necessidade de reiniciar o alarme.
 */
int64_t stop_buzzer(alarm_id_t id, void *user_data)
{
    pwm_set_gpio_level(BUZZER, 0);
    buzzer_active = false;
    return 0;
}

/**
 * @brief Emite um som curto no buzzer para alertar o jogador.
 *
 * Configura o PWM do buzzer para gerar uma nota com 50% de duty cycle por um tempo determinado.
 *
 * @param frequency Frequência da nota (Hz)
 * @param duration_ms Duração da nota (ms)
 */
void buzzer_beep(uint frequency, uint duration_ms)
{
    if (buzzer_active)
        return;

    uint slice_num = pwm_gpio_to_slice_num(BUZZER);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1;

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(BUZZER, top / 2); // 50% de duty cycle
    buzzer_active = true;

    add_alarm_in_ms(duration_ms, stop_buzzer, NULL, false);
}

/**
 * @brief Inicia o temporizador do jogo, marcando o tempo inicial.
 */
void start_timer()
{
    start_time = get_absolute_time();
}

/**
 * @brief Calcula o tempo de reação do jogador.
 *
 * Compara o tempo de início com o tempo em que o jogador pressionou o botão de parada.
 *
 * @return uint32_t Tempo decorrido em milissegundos.
 */
uint32_t get_elapsed_time()
{
    return absolute_time_diff_us(start_time, reaction_time) / 1000;
}

/**
 * @brief Função de debouncing para os botões.
 *
 * Impede que leituras múltiplas rápidas sejam interpretadas como várias pressões.
 *
 * @param gpio Pino do botão a ser verificado.
 * @return true Se o botão foi pressionado.
 * @return false Caso contrário.
 */
bool debounce_button(uint gpio)
{
    static uint32_t last_time = 0;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (current_time - last_time < 50)
    {
        return false;
    }

    last_time = current_time;
    return gpio_get(gpio) == 0;
}

/**
 * @brief Inicia uma nova rodada do jogo.
 *
 * Configura o estado inicial do jogo, atualiza o display e controla os LEDs e buzzer
 * para sinalizar a preparação e o início da fase de reação.
 */
void start_game()
{
    if (!game_running)
    {
        game_running = true;
        reaction_phase = false;
        false_start_detected = false;
        button_b_pressed = false;
        display_text("PREPARAR...!");

        // Liga o LED verde com PWM (50% brightness)
        pwm_set_gpio_level(LED_GREEN, LED_ON);

        uint delay_ms = 1000 + (rand() % 4000);
        for (uint i = 0; i < delay_ms / 10; i++)
        {
            sleep_ms(10);
            if (gpio_get(BUTTON_STOP) == 0)
            {
                false_start_detected = true;
                break;
            }
        }

        if (false_start_detected)
        {
            display_text("MUITO CEDO!");
            // Desliga o LED verde
            pwm_set_gpio_level(LED_GREEN, 0);
            // Pisca o LED vermelho três vezes (50% brightness)
            for (int j = 0; j < 3; j++)
            {
                pwm_set_gpio_level(LED_RED, LED_ON);
                sleep_ms(200);
                pwm_set_gpio_level(LED_RED, 0);
                sleep_ms(200);
            }
            game_running = false;
            reaction_phase = false;
            sleep_ms(2000);
            display_text("PRESSIONE A    PARA COMECAR!");
            return;
        }

        // Desliga o LED verde e liga o LED vermelho com PWM
        pwm_set_gpio_level(LED_GREEN, 0);
        pwm_set_gpio_level(LED_RED, LED_ON);

        // Emite um beep curto com o buzzer
        buzzer_beep(3000, 300);
        start_timer();
        reaction_phase = true;
        display_text("PRESSIONE B    PARA MARCAR!");
    }
}

/**
 * @brief Callback de interrupção para o botão de parada (B).
 *
 * Quando o botão B é pressionado, e se o jogo estiver em andamento e na fase de reação,
 * marca o tempo de reação.
 *
 * @param gpio Pino que gerou a interrupção.
 * @param events Máscara dos eventos que ocorreram.
 */
void gpio_callback(uint gpio, uint32_t events)
{
    if (gpio == BUTTON_STOP && game_running && reaction_phase)
    {
        reaction_time = get_absolute_time();
        button_b_pressed = true;
    }
}

/**
 * @brief Função principal.
 *
 * Inicializa o hardware (I2C, display OLED, botões, LEDs e PWM para buzzer e LEDs),
 * e entra em loop infinito monitorando os botões para iniciar e controlar o jogo.
 *
 * Os LEDs agora são acionados via PWM, garantindo que mesmo quando "ligados" o brilho seja
 * limitado a 50% do máximo.
 *
 * @return int 0 ao término (embora o loop seja infinito).
 */
int main()
{
    // Inicializa as interfaces padrão (UART, USB, etc.)
    stdio_init_all();

    // Inicializa a interface I2C para o display OLED
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa o display OLED e exibe mensagem inicial
    ssd1306_init();
    display_text("PRESSIONE A    PARA COMECAR!");

    // Configura os botões como entradas com pull-up interno
    gpio_init(BUTTON_START);
    gpio_init(BUTTON_STOP);
    gpio_set_dir(BUTTON_START, GPIO_IN);
    gpio_set_dir(BUTTON_STOP, GPIO_IN);
    gpio_pull_up(BUTTON_START);
    gpio_pull_up(BUTTON_STOP);

    // Inicializa os LEDs para PWM
    pwm_init_led(LED_GREEN);
    pwm_init_led(LED_RED);
    // Inicialmente, ambos os LEDs estão desligados
    pwm_set_gpio_level(LED_GREEN, 0);
    pwm_set_gpio_level(LED_RED, 0);

    // Inicializa o buzzer com PWM
    pwm_init_buzzer(BUZZER);

    // Configura a interrupção para o botão B (BUTTON_STOP)
    gpio_set_irq_enabled_with_callback(BUTTON_STOP, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    // Loop principal do jogo
    while (true)
    {
        // Verifica se o botão A foi pressionado com debounce
        if (debounce_button(BUTTON_START))
        {
            if (!game_running)
            {
                start_game();
            }
            sleep_ms(300);
        }

        // Se o jogo estiver em execução e o botão B foi pressionado
        if (game_running && reaction_phase && button_b_pressed)
        {
            uint32_t elapsed_time = get_elapsed_time();
            // Desliga o LED vermelho via PWM
            pwm_set_gpio_level(LED_RED, 0);

            stop_buzzer(0, NULL);

            char buffer[20];
            sprintf(buffer, "Tempo: %.1f ms", (float)elapsed_time);
            display_text(buffer);

            sleep_ms(5000);

            // Reseta o estado do jogo
            game_running = false;
            reaction_phase = false;
            false_start_detected = false;
            button_b_pressed = false;

            display_text("PRESSIONE A    PARA COMECAR!");
        }
    }

    return 0;
}