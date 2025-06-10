#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "ssd1306/ssd1306.h"

// Configurações de pinos
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL)  // GPIO 28
#define BUZZER1_PIN 21
#define BUZZER2_PIN 10
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6
#define LED_GREEN_PIN 11
#define LED_RED_PIN 13

// Configuração de áudio
#define AUDIO_DURATION_SECONDS 5
#define SAMPLE_RATE_HZ 22000
#define MAX_RECORDING_SAMPLES 120000
#define CALCULATED_SAMPLES (SAMPLE_RATE_HZ * AUDIO_DURATION_SECONDS)
#define RECORDING_SAMPLES ((CALCULATED_SAMPLES > MAX_RECORDING_SAMPLES) ? MAX_RECORDING_SAMPLES : CALCULATED_SAMPLES)
#define SAMPLE_PERIOD_US (1000000 / SAMPLE_RATE_HZ)

// Configuração PWM
#define PWM_FREQ_HZ 88000
#define PWM_TOP 1023

// OLED display
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_SDA_PIN 14
#define OLED_SCL_PIN 15
#define OLED_ADDRESS 0x3C

// Estados do programa
typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PLAYBACK
} ProgramState;

// Variáveis globais
ssd1306_t disp;
uint16_t adc_buffer[1];
uint16_t recording_buffer[RECORDING_SAMPLES];
uint recording_index = 0;
uint buzzer1_slice, buzzer2_slice;
uint buzzer1_channel, buzzer2_channel;
ProgramState current_state = STATE_IDLE;

// Protótipos
void setup_pwm_audio();
void setup_buttons_leds();
void sample_mic();
void record_audio_sample();
bool is_recording_full();
void reset_recording_buffer();
void play_audio_sample(uint16_t sample_value, uint16_t center_value, float gain);
void play_recorded_audio();
void stop_audio();
bool check_button_press(uint button_pin);
void set_led(uint led_pin, bool state);
void start_recording();
void start_playback();
void setup_oled();
void display_waveform(uint16_t* buffer, uint size, uint16_t center_value);
void display_recording_stats();

// Inicializa display OLED
void setup_oled() {
    i2c_init(i2c1, 400000);
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);
    
    ssd1306_init(&disp, OLED_WIDTH, OLED_HEIGHT, OLED_ADDRESS, i2c1);
    ssd1306_clear(&disp);
    
    ssd1306_draw_string(&disp, 0, 0, 1, "Inicializando...");
    ssd1306_show(&disp);
}

// Desenha forma de onda no OLED
void display_waveform(uint16_t* buffer, uint size, uint16_t center_value) {
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "Forma de Onda");
    
    // Calcula tamanho do passo para caber no display
    float step = (float)size / (OLED_WIDTH - 1);
    
    // Desenha a forma de onda
    for (int x = 0; x < OLED_WIDTH; x++) {
        int buffer_index = (int)(x * step);
        if (buffer_index >= size) buffer_index = size - 1;
        
        // Converte valor ADC para coordenada y
        int16_t value = buffer[buffer_index];
        int16_t normalized = value - center_value;
        
        // Escala para caber na altura do display
        int y = (OLED_HEIGHT/2) - (normalized * (OLED_HEIGHT/2 - 10) / 2048);
        
        // Garante que y está dentro dos limites
        if (y < 10) y = 10;
        if (y >= OLED_HEIGHT) y = OLED_HEIGHT - 1;
        
        ssd1306_draw_pixel(&disp, x, y);
    }
    
    // Desenha linha central
    for (int x = 0; x < OLED_WIDTH; x++) {
        ssd1306_draw_pixel(&disp, x, OLED_HEIGHT/2);
    }
    
    ssd1306_show(&disp);
}

// Exibe estatísticas da gravação
void display_recording_stats() {
    char buffer[32];
    
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "Gravacao Completa");
    
    snprintf(buffer, sizeof(buffer), "Amostras: %d", recording_index);
    ssd1306_draw_string(&disp, 0, 10, 1, buffer);
    
    snprintf(buffer, sizeof(buffer), "Tempo: %.1fs", (float)recording_index / SAMPLE_RATE_HZ);
    ssd1306_draw_string(&disp, 0, 20, 1, buffer);
    
    // Calcula estatísticas
    uint32_t sum = 0;
    uint16_t min_val = 4095, max_val = 0;
    for (uint i = 0; i < recording_index; i++) {
        sum += recording_buffer[i];
        if (recording_buffer[i] < min_val) min_val = recording_buffer[i];
        if (recording_buffer[i] > max_val) max_val = recording_buffer[i];
    }
    uint16_t avg_val = sum / recording_index;
    uint16_t amplitude = max_val - min_val;
    
    snprintf(buffer, sizeof(buffer), "Media: %d", avg_val);
    ssd1306_draw_string(&disp, 0, 30, 1, buffer);
    
    snprintf(buffer, sizeof(buffer), "Ampl: %d", amplitude);
    ssd1306_draw_string(&disp, 0, 40, 1, buffer);
    
    ssd1306_show(&disp);
}

// Configura botões e LEDs
void setup_buttons_leds() {
    // Botões como entrada com pull-up
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);
    
    // LEDs como saída
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, 0);
    
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, 0);
}

// Controla os LEDs
void set_led(uint led_pin, bool state) {
    gpio_put(led_pin, state);
}

// Verifica se um botão foi pressionado com debounce
bool check_button_press(uint button_pin) {
    if (gpio_get(button_pin) == 0) {  // Botão pressionado (pull-up)
        sleep_ms(20);  // Debounce
        if (gpio_get(button_pin) == 0) {
            while (gpio_get(button_pin) == 0) {  // Espera soltar
                sleep_ms(10);
            }
            return true;
        }
    }
    return false;
}

// Configura PWM para áudio
void setup_pwm_audio() {
    gpio_set_function(BUZZER1_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BUZZER2_PIN, GPIO_FUNC_PWM);
    
    buzzer1_slice = pwm_gpio_to_slice_num(BUZZER1_PIN);
    buzzer1_channel = pwm_gpio_to_channel(BUZZER1_PIN);
    buzzer2_slice = pwm_gpio_to_slice_num(BUZZER2_PIN);
    buzzer2_channel = pwm_gpio_to_channel(BUZZER2_PIN);
    
    float clock_div = 125000000.0f / (PWM_FREQ_HZ * (PWM_TOP + 1));
    uint16_t center_level = PWM_TOP / 2;
    
    pwm_set_clkdiv(buzzer1_slice, clock_div);
    pwm_set_wrap(buzzer1_slice, PWM_TOP);
    pwm_set_chan_level(buzzer1_slice, buzzer1_channel, center_level);
    pwm_set_enabled(buzzer1_slice, true);
    
    pwm_set_clkdiv(buzzer2_slice, clock_div);
    pwm_set_wrap(buzzer2_slice, PWM_TOP);
    pwm_set_chan_level(buzzer2_slice, buzzer2_channel, center_level);
    pwm_set_enabled(buzzer2_slice, true);
}

// Amostra o microfone
void sample_mic() {
    adc_select_input(MIC_CHANNEL);
    adc_buffer[0] = adc_read();
}

// Grava uma amostra de áudio
void record_audio_sample() {
    if (recording_index >= RECORDING_SAMPLES) {
        return;
    }
    
    sample_mic();
    recording_buffer[recording_index] = adc_buffer[0];
    recording_index++;
}

// Verifica se o buffer está cheio
bool is_recording_full() {
    return (recording_index >= RECORDING_SAMPLES);
}

// Limpa o buffer de gravação
void reset_recording_buffer() {
    recording_index = 0;
}

// Envia uma amostra para o PWM
void play_audio_sample(uint16_t sample_value, uint16_t center_value, float gain) {
    int32_t centered = (int32_t)sample_value - center_value;
    centered = (int32_t)(centered * gain);
    
    int32_t pwm_value = (centered * PWM_TOP / 4096) + (PWM_TOP / 2);
    
    if (pwm_value > PWM_TOP) pwm_value = PWM_TOP;
    if (pwm_value < 0) pwm_value = 0;
    
    pwm_set_chan_level(buzzer1_slice, buzzer1_channel, (uint16_t)pwm_value);
    pwm_set_chan_level(buzzer2_slice, buzzer2_channel, (uint16_t)pwm_value);
}

// Para a reprodução de áudio
void stop_audio() {
    pwm_set_chan_level(buzzer1_slice, buzzer1_channel, PWM_TOP / 2);
    pwm_set_chan_level(buzzer2_slice, buzzer2_channel, PWM_TOP / 2);
    set_led(LED_GREEN_PIN, false);
}

// Inicia gravação de áudio
void start_recording() {
    // Mostra no display
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 2, "GRAVANDO");
    ssd1306_draw_string(&disp, 0, 20, 1, "Fale no microfone...");
    ssd1306_show(&disp);
    
    // Acende LED vermelho
    set_led(LED_RED_PIN, true);
    set_led(LED_GREEN_PIN, false);
    
    reset_recording_buffer();
    current_state = STATE_RECORDING;
    
    uint32_t start_time = time_us_32();
    uint display_update_counter = 0;
    
    while (!is_recording_full()) {
        record_audio_sample();
        
        // Timing preciso
        uint32_t target_time = start_time + (recording_index) * SAMPLE_PERIOD_US;
        while (time_us_32() < target_time) {
            tight_loop_contents();
        }
        
        // Atualiza display ocasionalmente
        display_update_counter++;
        if (display_update_counter >= 2000) {
            display_update_counter = 0;
            
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "Amostras: %d", recording_index);
            ssd1306_clear(&disp);
            ssd1306_draw_string(&disp, 0, 0, 2, "GRAVANDO");
            ssd1306_draw_string(&disp, 0, 20, 1, buffer);
            snprintf(buffer, sizeof(buffer), "%.1f segundos", (float)recording_index / SAMPLE_RATE_HZ);
            ssd1306_draw_string(&disp, 0, 30, 1, buffer);
            ssd1306_show(&disp);
        }
        
        // Interrompe se botão B pressionado
        if (gpio_get(BUTTON_B_PIN) == 0) {
            ssd1306_clear(&disp);
            ssd1306_draw_string(&disp, 0, 0, 1, "Gravacao interrompida");
            ssd1306_draw_string(&disp, 0, 20, 1, "pelo usuario");
            ssd1306_show(&disp);
            sleep_ms(1000);
            
            break;
        }
    }
    
    // Apaga LED vermelho
    set_led(LED_RED_PIN, false);
    
    // Mostra estatísticas no display
    display_recording_stats();
    
    // Mostra forma de onda final
    if (recording_index > 0) {
        uint32_t sum = 0;
        for (uint i = 0; i < recording_index; i++) {
            sum += recording_buffer[i];
        }
        uint16_t avg_val = sum / recording_index;
        display_waveform(recording_buffer, recording_index, avg_val);
    }
    
    current_state = STATE_IDLE;
}

// Reproduz o áudio gravado
void start_playback() {
    if (recording_index == 0) {
        ssd1306_clear(&disp);
        ssd1306_draw_string(&disp, 0, 0, 1, "ERRO");
        ssd1306_draw_string(&disp, 0, 20, 1, "Nenhum audio gravado!");
        ssd1306_show(&disp);
        sleep_ms(2000);
        
        return;
    }
    
    // Mostra no display
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 2, "REPRODUZINDO");
    ssd1306_draw_string(&disp, 0, 20, 1, "Audio...");
    ssd1306_show(&disp);
    
    // Acende LED verde
    set_led(LED_GREEN_PIN, true);
    set_led(LED_RED_PIN, false);
    
    current_state = STATE_PLAYBACK;
    
    // Calcula estatísticas do sinal
    uint32_t sum = 0;
    uint16_t min_val = 4095, max_val = 0;
    for (uint i = 0; i < recording_index; i++) {
        sum += recording_buffer[i];
        if (recording_buffer[i] < min_val) min_val = recording_buffer[i];
        if (recording_buffer[i] > max_val) max_val = recording_buffer[i];
    }
    uint16_t avg_val = sum / recording_index;
    uint16_t amplitude = max_val - min_val;
    
    // Ganho adaptativo
    float gain = 6.0f;
    if (amplitude < 500) {
        gain = 12.0f;
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "Ganho: %.1fx", gain);
        ssd1306_draw_string(&disp, 0, 30, 1, buffer);
        ssd1306_show(&disp);
    }
    
    uint32_t start_time = time_us_32();
    uint display_update_counter = 0;
    
    for (uint i = 0; i < recording_index; i++) {
        play_audio_sample(recording_buffer[i], avg_val, gain);
        
        // Timing preciso
        uint32_t target_time = start_time + (i + 1) * SAMPLE_PERIOD_US;
        while (time_us_32() < target_time) {
            tight_loop_contents();
        }
        
        // Atualiza display ocasionalmente
        display_update_counter++;
        if (display_update_counter >= 5000) {
            display_update_counter = 0;
            
            // Mostra progresso
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "Progresso: %.1f%%", (float)i * 100 / recording_index);
            ssd1306_clear(&disp);
            ssd1306_draw_string(&disp, 0, 0, 1, "REPRODUZINDO");
            ssd1306_draw_string(&disp, 0, 20, 1, buffer);
            ssd1306_show(&disp);
        }
        
        // Interrompe se botão A pressionado
        if (gpio_get(BUTTON_A_PIN) == 0) {
            ssd1306_clear(&disp);
            ssd1306_draw_string(&disp, 0, 0, 1, "Reproducao interrompida");
            ssd1306_draw_string(&disp, 0, 20, 1, "pelo usuario");
            ssd1306_show(&disp);
            sleep_ms(1000);
            
            break;
        }
    }
    
    stop_audio();
    
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "Reproducao concluida");
    ssd1306_show(&disp);
    sleep_ms(1000);
    
    // Mostra waveform completa
    display_waveform(recording_buffer, recording_index, avg_val);
    
    current_state = STATE_IDLE;
}

int main() {
    stdio_init_all();
    sleep_ms(3000);

    // Inicializações
    adc_init();
    adc_gpio_init(MIC_PIN);
    setup_buttons_leds();
    setup_oled();
    setup_pwm_audio();
    
    // Mensagem de boas-vindas
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 2, "SINTETIZADOR");
    ssd1306_draw_string(&disp, 0, 20, 1, "DE AUDIO");
    ssd1306_draw_string(&disp, 0, 35, 1, "22kHz - 5s");
    ssd1306_show(&disp);
    sleep_ms(2000);
    
    // Teste de som inicial
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "Teste de som...");
    ssd1306_show(&disp);
    
    set_led(LED_GREEN_PIN, true);
    for (int i = 0; i < 11000; i++) {
        uint16_t test_sample = (uint16_t)(2048 + 1800 * sin(2 * M_PI * 1000 * i / 22000.0));
        play_audio_sample(test_sample, 2048, 4.0);
        sleep_us(SAMPLE_PERIOD_US);
    }
    stop_audio();
    
    // Exibe instruções
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 2, "PRONTO");
    ssd1306_draw_string(&disp, 0, 20, 1, "Botao A: Gravar");
    ssd1306_draw_string(&disp, 0, 30, 1, "Botao B: Reproduzir");
    ssd1306_show(&disp);

    // Loop principal
    while (true) {
        // Verifica botões quando ocioso
        if (current_state == STATE_IDLE) {
            // Botão A (gravação)
            if (check_button_press(BUTTON_A_PIN)) {
                start_recording();
                
                // Exibe instruções novamente
                ssd1306_clear(&disp);
                ssd1306_draw_string(&disp, 0, 0, 2, "PRONTO");
                ssd1306_draw_string(&disp, 0, 20, 1, "Botao A: Gravar");
                ssd1306_draw_string(&disp, 0, 30, 1, "Botao B: Reproduzir");
                ssd1306_show(&disp);
            }
            
            // Botão B (reprodução)
            if (check_button_press(BUTTON_B_PIN)) {
                start_playback();
                
                // Exibe instruções novamente
                ssd1306_clear(&disp);
                ssd1306_draw_string(&disp, 0, 0, 2, "PRONTO");
                ssd1306_draw_string(&disp, 0, 20, 1, "Botao A: Gravar");
                ssd1306_draw_string(&disp, 0, 30, 1, "Botao B: Reproduzir");
                ssd1306_show(&disp);
            }
        }
        
        // Pausa para economizar energia
        sleep_ms(10);
    }
    
    return 0;
}