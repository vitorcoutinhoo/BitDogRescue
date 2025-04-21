// BitDogRescue - Jogo de resgate de vítimas com drone
// Autor: Vítor Coutinho Lima

// Bibliotecas necessárias
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "ws2812.pio.h"
#include "ssd1306.h"

// Definindo os pinos dos leds
#define BLUE 12
#define GREEN 11
#define RED 13
#define LED_MATRIX 7

// Eixos do joystick
#define EIXO_X 26
#define EIXO_Y 27

// Botões
#define BBUTTON 6
#define ABUTTON 5

// Buzzers
#define ABUZZER 10
#define BBUZZER 21

// Display OLED
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C

// Display
ssd1306_t ssd;
char timer[4];

// Constantes
#define MAX_VITIMAS 5
#define DRONE_SIZE 8
#define VITIMA_SIZE 4

// Variáveis globais
int posx[MAX_VITIMAS];
int posy[MAX_VITIMAS];
bool vitima_ativa[MAX_VITIMAS];
int total_resgatadas = 0;
int dronex, droney;
bool jogo_ativo = false;
volatile bool botao_pressionado_flag = false;
volatile bool tocar_som_inicio_flag = false;
absolute_time_t ultimo_press = 0;

// Prototipação das funções
bool update_timer(int, int);
void draw_object(int, int, int);
void posicionar_vitimas();
void desenhar_vitimas();
void posicionar_drone();
void mover_drone();
void verificar_resgate();
void irq_buttons(uint, uint32_t);
bool checar_vitoria();
void atualizar_led_azul();
void atualizar_matriz_led();
void put_pixel(uint32_t);
uint32_t color(uint8_t, uint8_t, uint8_t);
void show_numbers(uint number);
void beep(int, int);
void som_tela_inicial();
void som_inicio_jogo();
void som_mover_drone();
void som_resgate();
void som_vitoria();
void som_derrota();

int main() {
    stdio_init_all();

    // ADC
    adc_init();
    adc_gpio_init(EIXO_X); 
    adc_gpio_init(EIXO_Y); 

    // LEDs
    gpio_init(RED); gpio_set_dir(RED, GPIO_OUT);
    gpio_init(BLUE); gpio_set_dir(BLUE, GPIO_OUT);
    gpio_init(GREEN); gpio_set_dir(GREEN, GPIO_OUT);

    // Botões
    gpio_init(BBUTTON); gpio_set_dir(BBUTTON, GPIO_IN); gpio_pull_up(BBUTTON);
    gpio_init(ABUTTON); gpio_set_dir(ABUTTON, GPIO_IN); gpio_pull_up(ABUTTON);
    gpio_set_irq_enabled_with_callback(BBUTTON, GPIO_IRQ_EDGE_FALL, true, &irq_buttons);
    gpio_set_irq_enabled_with_callback(ABUTTON, GPIO_IRQ_EDGE_FALL, true, &irq_buttons);

    // Buzzers
    gpio_init(ABUZZER); gpio_set_dir(ABUZZER, GPIO_OUT);
    gpio_init(BBUZZER); gpio_set_dir(BBUZZER, GPIO_OUT);

    // Display OLED
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA); gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // WS2812
    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_MATRIX, 800000, false);

    int count = 0; // Contador de tempo
    int x = 0;
    absolute_time_t ultimo_tempo = get_absolute_time();
    bool som_tela_inicial_tocado = false;

    while (true) {
        if (!jogo_ativo) {
            if (!som_tela_inicial_tocado) {
                som_tela_inicial();
                som_tela_inicial_tocado = true;
            }
            
            // Tela inicial
            ssd1306_fill(&ssd, false);
            ssd1306_rect(&ssd, 0, 0, 128, 64, true, false); 
            ssd1306_draw_string(&ssd, "BitDogRescue", 16, 12);
            ssd1306_draw_string(&ssd, "pressione A", 20, 36);
            ssd1306_draw_string(&ssd, "para iniciar", 18, 46);
            ssd1306_send_data(&ssd);
            show_numbers(0);
            count = 0;
        } else {
            ssd1306_fill(&ssd, false);
            snprintf(timer, sizeof(timer), "%d", count);
            ssd1306_rect(&ssd, 0, 0, 128, 64, true, false);

            // Move o drone e verifica se está sobre uma vítima e a possibilidade de resgate
            mover_drone();
            atualizar_led_azul();
            verificar_resgate();
            
            // Desenha o drone e as vítimas e atualiza a matriz de LEDs
            desenhar_vitimas();
            draw_object(dronex, droney, DRONE_SIZE);
            atualizar_matriz_led();
            jogo_ativo = update_timer(count, x); // Verifica se o tempo esgotou

            if (tocar_som_inicio_flag) {
                som_inicio_jogo();
                tocar_som_inicio_flag = false;
            }

            if (checar_vitoria()) {
                // Tela de vitória
                ssd1306_fill(&ssd, false);
                ssd1306_rect(&ssd, 1, 1, 127, 63, true, false);
                ssd1306_draw_string(&ssd, "Voce Venceu", 20, 26);
                som_vitoria();
                gpio_put(BLUE, false);
                gpio_put(GREEN, true);
                ssd1306_send_data(&ssd);
                
                // mensagem uart
                printf("[FIM] Todas as vítimas foram salvas!\n");
                printf("[TEMPO] Missao concluida em %dmin %ds.\n", count / 60, count % 60);
                sleep_ms(5000);
                gpio_put(GREEN, false);
                
                // Reseta as variáveis para reiniciar o jogo
                jogo_ativo = false;
                som_tela_inicial_tocado = false;
                continue;
            }

            ssd1306_send_data(&ssd);

            if (absolute_time_diff_us(ultimo_tempo, get_absolute_time()) >= 1000000) {
                count++;
                ultimo_tempo = get_absolute_time();
            }

            sleep_ms(200);
        }
    }
}

// Atualiza o timer na tela e verifica se o tempo esgotou
bool update_timer(int tempo, int x) {
    // Move o contador para a esquerda baseado na quantidade de dígitos
    if (tempo <= 60) {
        if ((int)(floor(log10(tempo)) + 1) == 1 || tempo == 0)
            x = 119;
        else if ((int)(floor(log10(tempo) + 1)) == 2)
            x = 111;

        ssd1306_draw_string(&ssd, timer, x, 2);
    } else {
        // Tela de derrota
        ssd1306_fill(&ssd, false);
        ssd1306_rect(&ssd, 1, 1, 127, 63, true, false);
        ssd1306_draw_string(&ssd, "Voce Perdeu", 20, 26);
        
        som_derrota();
        gpio_put(RED, true);
        ssd1306_send_data(&ssd);
        
        printf("[FIM] Tempo esgotado. Missao falhou.\n");
        sleep_ms(5000);       
        gpio_put(RED, false);
        return false;
    }
    return true;
}

// Desenha um objeto na tela (vítima ou drone)
void draw_object(int x, int y, int size) {
    ssd1306_rect(&ssd, y, x, size, size, true, true);
}

// Posiciona as vítimas em posições aleatórias na tela, evitando sobreposição
void posicionar_vitimas() {
    srand(time_us_32());
    
    for (int i = 0; i < MAX_VITIMAS; i++) {
        bool pos_valida = false;
        
        while (!pos_valida) {
            pos_valida = true;

            // Gera uma posição aleatória para a vítima
            posx[i] = (rand() % (118 - 4 + 1)) + 4;
            posy[i] = (rand() % (59 - 12 + 1)) + 8;
            
            // Verifica se a posição da vítima não colide com outras vítimas
            for (int j = 0; j < i; j++) {
                int dx = abs(posx[i] - posx[j]);
                int dy = abs(posy[i] - posy[j]);
                
                if (dx < VITIMA_SIZE * 2 && dy < VITIMA_SIZE * 2) {
                    pos_valida = false;
                    break;
                }
            }
        }

        // Inicializa a posição da vítima
        vitima_ativa[i] = true;
    }
}

// Desenha as vítimas na tela
void desenhar_vitimas() {
    for (int i = 0; i < MAX_VITIMAS; i++)
        if (vitima_ativa[i])
            draw_object(posx[i], posy[i], VITIMA_SIZE);
}

// Posiciona o drone em uma posição válida, longe das vítimas
void posicionar_drone() {
    bool pos_valida = false;
    
    while (!pos_valida) {
        pos_valida = true;

        // Gera uma posição aleatória para o drone
        dronex = (rand() % (118 - 8 + 1)) + 4;
        droney = (rand() % (54 - 8 + 1)) + 8;
        
        // Verifica se a posição do drone não colide com as vítimas
        for (int i = 0; i < MAX_VITIMAS; i++) {
            int dx = abs(dronex - posx[i]);
            int dy = abs(droney - posy[i]);
            
            if (dx < DRONE_SIZE + 10 && dy < DRONE_SIZE + 10) {
                pos_valida = false;
                break;
            }
        }
    }
}

// Move o drone com base nos valores do joystick
void mover_drone() {
    const int passo = 4;
    const int limiar_baixo = 1000;
    const int limiar_alto  = 3000;

    adc_select_input(1);
    uint16_t val_x = adc_read();
    adc_select_input(0);
    uint16_t val_y = adc_read();

    // Mapeia os valores do joystick para o movimento do drone
    if (val_x < limiar_baixo && dronex > 4) { dronex -= passo; som_mover_drone();}
    else if (val_x > limiar_alto && dronex < 120) { dronex += passo; som_mover_drone();}
    if (val_y < limiar_baixo && droney < 56) { droney += passo; som_mover_drone();} 
    else if (val_y > limiar_alto && droney > 8) { droney -= passo; som_mover_drone();}
}

// Verifica se o drone está sobre uma vítima e atualiza o estado
void verificar_resgate() {
    if(!botao_pressionado_flag) return;
    
    for (int i = 0; i < MAX_VITIMAS; i++) {
        // Verifica se o drone está sobre a vítima
        if (vitima_ativa[i] && abs(dronex - posx[i]) < DRONE_SIZE && abs(droney - posy[i]) < DRONE_SIZE) {
            vitima_ativa[i] = false;
            total_resgatadas++;
            
            printf("[RESGATE] Vítima salva em %d, %d\n", posx[i], posy[i]);
            som_resgate();
        }
    }

    botao_pressionado_flag = false;
}

// Verifica se todas as vítimas foram resgatadas
bool checar_vitoria() {
    return total_resgatadas == MAX_VITIMAS;
}

// Interrupção para os botões
void irq_buttons(uint gpio, uint32_t event){
    absolute_time_t agr = get_absolute_time();

    // Interrupção para o botão B
    if (gpio == BBUTTON && absolute_time_diff_us(ultimo_press, agr) > 250000){
        botao_pressionado_flag = true;
        ultimo_press = agr;
    }

    // Interrupção para o botão A
    if (gpio == ABUTTON && absolute_time_diff_us(ultimo_press, agr) > 250000){
        jogo_ativo = true;
        ultimo_press = agr;
        total_resgatadas = 0;
        
        // Inicializa o jogo
        posicionar_vitimas();
        posicionar_drone();
        
        gpio_put(RED, false);
        gpio_put(GREEN, false);
        
        printf("[INICIO] Jogo iniciado.\n");
        tocar_som_inicio_flag = true; 
    }
}

// Atualiza o LED azul se o drone estiver sobre uma vítima
void atualizar_led_azul() {
    bool sobre_vitima = false;
    for (int i = 0; i < MAX_VITIMAS; i++) {
        if (vitima_ativa[i] && abs(dronex - posx[i]) < DRONE_SIZE && abs(droney - posy[i]) < DRONE_SIZE) {
            sobre_vitima = true;
            break;
        }
    }
    gpio_put(BLUE, sobre_vitima);
}

// Atualiza a matriz de LEDs com o número de vítimas restantes
void atualizar_matriz_led() {
    int vitimas_restantes = 0;
    for (int i = 0; i < MAX_VITIMAS; i++)
        if (vitima_ativa[i]) vitimas_restantes++;

    show_numbers(vitimas_restantes);
}

// Função para colocar um pixel na matriz de LEDs
void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Função para criar uma cor no formato RGB
uint32_t color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | b;
}

// Função para mostrar números de 1 a 5 na matriz de LEDs e o simbolo -
void show_numbers(uint number) {
    uint32_t numbers[6][25] = {
        {0,0,0,0,0, 0,0,0,0,0, 0,1,1,1,0, 0,0,0,0,0, 0,0,0,0,0}, // - 
        {0,1,1,1,0, 0,0,1,0,0, 0,0,1,0,0, 0,1,1,0,0, 0,0,1,0,0}, // 1
        {0,1,1,1,0, 0,1,0,0,0, 0,1,1,0,0, 0,0,0,1,0, 0,1,1,1,0}, // 2
        {0,1,1,1,0, 0,0,0,1,0, 0,1,1,1,0, 0,0,0,1,0, 0,1,1,1,0}, // 3
        {0,1,0,0,0, 0,0,0,1,0, 0,1,1,1,0, 0,1,0,1,0, 0,1,0,1,0}, // 4
        {0,1,1,1,0, 0,0,0,1,0, 0,0,1,1,0, 0,1,0,0,0, 0,1,1,1,0}  // 5
    };

    if (number > 5) number = 5;
    for (uint i = 0; i < 25; i++) {
        if (numbers[number][i])
            put_pixel(color(2, 2, 2));  // cor branca
        else
            put_pixel(0);                 
    }
}


// Efeitos sonoros
void beep(int freq, int duration_ms) {
    int delay_us = 1000000 / (freq * 2);
    int cycles = (freq * duration_ms) / 1000;
    for (int i = 0; i < cycles; i++) {
        gpio_put(ABUZZER, 1);
        gpio_put(BBUZZER, 1);
        sleep_us(delay_us);
        gpio_put(ABUZZER, 0);
        gpio_put(BBUZZER, 0);
        sleep_us(delay_us);
    }
}


void som_tela_inicial() {
    beep(1000, 100);
    sleep_ms(100);
    beep(1200, 100);
}

void som_inicio_jogo() {
    beep(1500, 200);
}

void som_mover_drone() {
    beep(900, 20);
}

void som_resgate() {
    beep(800, 100);
    sleep_ms(50);
    beep(600, 100);
}

void som_vitoria() {
    beep(1000, 100);
    sleep_ms(100);
    beep(1200, 100);
    sleep_ms(100);
    beep(1400, 200);
}

void som_derrota() {
    beep(600, 300);
    sleep_ms(100);
    beep(400, 300);
}