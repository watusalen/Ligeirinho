#include <stdio.h>        // Biblioteca padrão de entrada e saída
#include <stdlib.h>       // Biblioteca para manipulação de memória e funções rand()
#include <string.h>           // Biblioteca para manipulação de strings
#include "pico/stdlib.h"  // Biblioteca padrão do Raspberry Pi Pico
#include "hardware/timer.h" // Biblioteca para manipulação de timers no RP2040
#include "hardware/pwm.h"   // Biblioteca para controle de PWM
#include "hardware/clocks.h" // Biblioteca para manipulação de clocks
#include "hardware/gpio.h"   // Biblioteca para manipulação de GPIOs
#include "hardware/i2c.h"    // Biblioteca para comunicação I2C
#include "inc/ssd1306.h"     // Biblioteca para comunicação com o display OLED

// Definição dos pinos utilizados no projeto
#define BUTTON_START 5   // Botão A - Inicia o jogo
#define BUTTON_STOP 6    // Botão B - Captura o tempo de reação
#define LED_GREEN 11     // LED Verde - Indica preparação
#define LED_RED 13       // LED Vermelho - Indica reação
#define BUZZER 21        // Buzzer para emitir som ao acionar o LED vermelho
#define I2C_SDA 14       // Pino SDA para o display OLED
#define I2C_SCL 15       // Pino SCL para o display OLED    

// Variáveis globais para controle do jogo
bool game_running = false;      // Indica se o jogo está em execução
bool reaction_phase = false;    // Indica se o jogador deve reagir
absolute_time_t start_time, reaction_time;  // Armazena os tempos de início e reação
volatile bool buzzer_active = false;  // Indica se o buzzer está ativo
volatile bool false_start_detected = false; // Indica se houve uma queima de largada
volatile bool button_b_pressed = false; // Indica se o botão B foi pressionado

/**
 * Exibe um texto no display OLED, quebrando linhas automaticamente.
 * Cada linha comporta até 15 caracteres.
 * 
 * @param text Mensagem a ser exibida no display
 */
void display_text(const char *text) {
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    int y = 0;  
    int line_len = 15;  
    char line_buffer[16]; 
    int text_len = strlen(text);

    for (int i = 0; i < text_len; i += line_len) {
        strncpy(line_buffer, text + i, line_len);
        line_buffer[line_len] = '\0'; 
        ssd1306_draw_string(ssd, 2, y, line_buffer); 
        y += 8;  
        if (y >= ssd1306_height) break; 
    }

    render_on_display(ssd, &frame_area);
}

/**
 * Inicializa o PWM no pino do buzzer.
 * 
 * @param pin Pino do buzzer
 */
void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0);
}

/**
 * Callback chamado para desligar o buzzer após um tempo determinado.
 * 
 * @return Retorna 0 (sem efeito sobre o sistema de alarmes)
 */
int64_t stop_buzzer(alarm_id_t id, void *user_data) {
    pwm_set_gpio_level(BUZZER, 0); 
    buzzer_active = false;         
    return 0;
}

/**
 * Emite um som curto no buzzer para alertar o jogador.
 * 
 * @param frequency Frequência do som (Hz)
 * @param duration_ms Duração do som (ms)
 */
void buzzer_beep(uint frequency, uint duration_ms) {
    if (buzzer_active) return; 

    uint slice_num = pwm_gpio_to_slice_num(BUZZER);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1; 

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(BUZZER, top / 2); 
    buzzer_active = true; 

    add_alarm_in_ms(duration_ms, stop_buzzer, NULL, false);
}


/**
 * Inicia o temporizador do jogo, marcando o tempo inicial.
 */
void start_timer() {
    start_time = get_absolute_time();
}

/**
 * Calcula o tempo de reação do jogador.
 * 
 * @return Tempo decorrido em milissegundos
 */
uint32_t get_elapsed_time() {
    return absolute_time_diff_us(start_time, reaction_time) / 1000;
}

/**
 * Função de debouncing para os botões, evitando leituras múltiplas rápidas.
 * 
 * @param gpio Pino do botão a ser verificado
 * @return Retorna true se o botão foi pressionado, false caso contrário
 */
bool debounce_button(uint gpio) {
    static uint32_t last_time = 0; 
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (current_time - last_time < 50) {
        return false;
    }

    last_time = current_time;
    return gpio_get(gpio) == 0; 
}

/**
 * Inicia uma nova rodada do jogo.
 */
void start_game() {
    if (!game_running) {
        game_running = true;
        reaction_phase = false;
        false_start_detected = false;
        button_b_pressed = false;  
        display_text("PREPARAR...!");

        gpio_put(LED_GREEN, 1); 

        uint delay_ms = 1000 + (rand() % 4000);
        for (uint i = 0; i < delay_ms / 10; i++) {
            sleep_ms(10);
            if (gpio_get(BUTTON_STOP) == 0) {
                false_start_detected = true; 
                break;
            }
        }

        if (false_start_detected) {
            display_text("MUITO CEDO!");
            gpio_put(LED_GREEN, 0);
            for (int j = 0; j < 3; j++) {
                gpio_put(LED_RED, 1);
                sleep_ms(200);
                gpio_put(LED_RED, 0);
                sleep_ms(200);
            }
            game_running = false;
            reaction_phase = false;
            sleep_ms(2000); 
            display_text("PRESSIONE A    PARA COMECAR!");
            return;
        }

        gpio_put(LED_GREEN, 0); 
        gpio_put(LED_RED, 1);   
        buzzer_beep(3000, 300); 
        start_timer();          
        reaction_phase = true;  
        display_text("PRESSIONE B    PARA MARCAR!");
    }
}

/**
 * Callback de interrupção para o botão de parada (B).
 */
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == BUTTON_STOP && game_running && reaction_phase) {
        reaction_time = get_absolute_time();  
        button_b_pressed = true;
    }
}


int main() {
    stdio_init_all(); 

    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();
    display_text("PRESSIONE A    PARA COMECAR!"); 

    gpio_init(BUTTON_START);
    gpio_init(BUTTON_STOP);
    gpio_set_dir(BUTTON_START, GPIO_IN);
    gpio_set_dir(BUTTON_STOP, GPIO_IN);
    gpio_pull_up(BUTTON_START);
    gpio_pull_up(BUTTON_STOP);

    gpio_init(LED_GREEN);
    gpio_init(LED_RED);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_RED, 0);

    pwm_init_buzzer(BUZZER);

    gpio_set_irq_enabled_with_callback(BUTTON_STOP, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    while (true) {
        if (debounce_button(BUTTON_START)) {
            if (!game_running) {
                start_game(); 
            }
            sleep_ms(300);
        }

        if (game_running && reaction_phase && button_b_pressed) {
            uint32_t elapsed_time = get_elapsed_time(); 
            gpio_put(LED_RED, 0); 

            stop_buzzer(0, NULL);

            char buffer[20];
            sprintf(buffer, "Tempo: %.1f ms", (float)elapsed_time);
            display_text(buffer);

            sleep_ms(5000); 

            game_running = false;
            reaction_phase = false;
            false_start_detected = false;
            button_b_pressed = false;

            display_text("PRESSIONE A    PARA COMECAR!");
        }
    }
}