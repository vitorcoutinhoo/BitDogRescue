#include <stdio.h>
#include <math.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "ssd1306.h"

// Definindo os pinos dos leds
#define BLUE 12
#define GREEN 11
#define RED 13

// Definindo os eixos do joystick
#define EIXO_X 26
#define EIXO_Y 27

// Definindo os botões
#define BBUTTON 6
#define ABUTTON 5

// Definição dos pinos do display
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C

// Estrutura do display
ssd1306_t ssd;

// Variável do Cronômetro
char timer[4];

// Constantes de quantidade e tamanho
#define MAX_VITIMAS 5
#define DRONE_SIZE 8
#define VITIMA_SIZE 4

// Posições das vítimas
int posx[MAX_VITIMAS];
int posy[MAX_VITIMAS];

// Contagem de vitimas
bool vitima_ativa[MAX_VITIMAS];
int total_resgatadas = 0;

// Coordenadas do drone
int dronex;
int droney;

// Estado do jogo
bool jogo_ativo = false;

// Controle do botão
volatile bool botao_pressionado_flag = false;
absolute_time_t ultimo_press = 0;

// Funções
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

int main() {
    stdio_init_all();

    // Iniciando ADC
    adc_init();
    adc_gpio_init(EIXO_X); 
    adc_gpio_init(EIXO_Y); 

    // Iniciando LEDs
    gpio_init(RED);   gpio_set_dir(RED, GPIO_OUT);
    gpio_init(BLUE);  gpio_set_dir(BLUE, GPIO_OUT);
    gpio_init(GREEN); gpio_set_dir(GREEN, GPIO_OUT);

    // Iniciando Button e interrupção
    gpio_init(BBUTTON); gpio_set_dir(BBUTTON, GPIO_IN); gpio_pull_up(BBUTTON);
    gpio_init(ABUTTON); gpio_set_dir(ABUTTON, GPIO_IN); gpio_pull_up(ABUTTON);
    gpio_set_irq_enabled_with_callback(BBUTTON, GPIO_IRQ_EDGE_FALL, true, &irq_buttons);
    gpio_set_irq_enabled_with_callback(ABUTTON, GPIO_IRQ_EDGE_FALL, true, &irq_buttons);

    // Configurações do display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    int count = 0;
    int x = 0;
    absolute_time_t ultimo_tempo = get_absolute_time();

    while (true) {
        if (!jogo_ativo){
            ssd1306_fill(&ssd, false);
            ssd1306_rect(&ssd, 0, 0, 128, 64, true, false); 
            ssd1306_draw_string(&ssd, "BitDogRescue", 16, 12);
            ssd1306_draw_string(&ssd, "pressione A", 20, 36);
            ssd1306_draw_string(&ssd, "para iniciar", 18, 46);
            ssd1306_send_data(&ssd);
            count = 0;
        }
        else {
            ssd1306_fill(&ssd, false);
            snprintf(timer, sizeof(timer), "%d", count);
            ssd1306_rect(&ssd, 0, 0, 128, 64, true, false);

            mover_drone();
            atualizar_led_azul();
            verificar_resgate();
            desenhar_vitimas();
            draw_object(dronex, droney, DRONE_SIZE);
            jogo_ativo = update_timer(count, x);

            if (checar_vitoria()) {
                ssd1306_fill(&ssd, false);
                ssd1306_rect(&ssd, 1, 1, 127, 63, true, false);
                ssd1306_draw_string(&ssd, "Voce Venceu", 20, 26);
                gpio_put(BLUE, false);
                gpio_put(GREEN, true);

                int tempo_final = count;
                printf("[FIM] Todas as vítimas foram salvas!\n");
                printf("[TEMPO] Missao concluida em %dmin %ds.\n", tempo_final / 60, tempo_final % 60);
                jogo_ativo = false;
            }

            ssd1306_send_data(&ssd);

            // Atualiza o cronômetro a cada 1 segundo real
            if (absolute_time_diff_us(ultimo_tempo, get_absolute_time()) >= 1000000) {
                count += 1;
                ultimo_tempo = get_absolute_time();
            }

            sleep_ms(300); // atualização mais suave do movimento do drone
        }  
    }
}


bool update_timer(int tempo, int x) {
    if (tempo <= 120) {
        if ((int)(floor(log10(tempo)) + 1) == 1 || tempo == 0)
            x = 119;
        if ((int)(floor(log10(tempo) + 1)) == 2)
            x = 111;
        if ((int)(floor(log10(tempo) + 1)) == 3)
            x = 103;

        ssd1306_draw_string(&ssd, timer, x, 2);
    } else {
        ssd1306_fill(&ssd, false);
        ssd1306_rect(&ssd, 1, 1, 127, 63, true, false);
        ssd1306_draw_string(&ssd, "Voce Perdeu", 20, 26);
        gpio_put(RED, true);
        return false;
    }
    return true;
}

void draw_object(int x, int y, int size) {
    ssd1306_rect(&ssd, y, x, size, size, true, true);
}

void posicionar_vitimas() {
    srand(time_us_32());

    for (int i = 0; i < MAX_VITIMAS; i++) {
        bool pos_valida = false;

        while (!pos_valida) {
            pos_valida = true;
            posx[i] = (rand() % (118 - 4 + 1)) + 4;
            posy[i] = (rand() % (59 - 12 + 1)) + 8;

            for (int j = 0; j < i; j++) {
                int dx = abs(posx[i] - posx[j]);
                int dy = abs(posy[i] - posy[j]);
                if (dx < VITIMA_SIZE * 2 && dy < VITIMA_SIZE * 2) {
                    pos_valida = false;
                    break;
                }
            }
        }
        vitima_ativa[i] = true;
    }
}

void desenhar_vitimas() {
    for (int i = 0; i < MAX_VITIMAS; i++) {
        if (vitima_ativa[i]) {
            draw_object(posx[i], posy[i], VITIMA_SIZE);
        }
    }
}

void posicionar_drone() {
    bool pos_valida = false;

    while (!pos_valida) {
        pos_valida = true;
        dronex = (rand() % (118 - 8 + 1)) + 4;
        droney = (rand() % (54 - 8 + 1)) + 8;

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

void mover_drone() {
    const int passo = 8;
    const int limiar_baixo = 1000;
    const int limiar_alto  = 3000;

    adc_select_input(1);
    uint16_t val_x = adc_read();

    adc_select_input(0);
    uint16_t val_y = adc_read();

    if (val_x < limiar_baixo && dronex > 4)
        dronex -= passo;
    else if (val_x > limiar_alto && dronex < 120)
        dronex += passo;

    if (val_y < limiar_baixo && droney < 56)
        droney += passo;
    else if (val_y > limiar_alto && droney > 8)
        droney -= passo;
}

void verificar_resgate() {
    if(!botao_pressionado_flag) return;


    for (int i = 0; i < MAX_VITIMAS; i++) {
        if (vitima_ativa[i] &&
            abs(dronex - posx[i]) < DRONE_SIZE &&
            abs(droney - posy[i]) < DRONE_SIZE) {
            
            vitima_ativa[i] = false;
            total_resgatadas++;
            printf("[RESGATE] Vítima salva em %d, %d\n", posx[i], posy[i]);
        }
    }

    botao_pressionado_flag = false;
}

bool checar_vitoria() {
    return total_resgatadas == MAX_VITIMAS;
}

void irq_buttons(uint gpio, uint32_t event){
    if (gpio == BBUTTON){
        absolute_time_t agr = get_absolute_time();

        if (absolute_time_diff_us(ultimo_press, agr) > 250000){
            botao_pressionado_flag = true;
            ultimo_press = agr;
        }
    }

    if (gpio == ABUTTON) {
        absolute_time_t agr = get_absolute_time();

        if (absolute_time_diff_us(ultimo_press, agr) > 250000){
            jogo_ativo = true;
            ultimo_press = agr;

            total_resgatadas = 0;
            posicionar_vitimas();
            posicionar_drone();
            gpio_put(RED, false);
            gpio_put(GREEN, false);
            printf("[INICIO] Jogo iniciado.\n");
        }
    }
}

void atualizar_led_azul() {
    bool sobre_vitima = false;

    for (int i = 0; i < MAX_VITIMAS; i++) {
        if (vitima_ativa[i] &&
            abs(dronex - posx[i]) < DRONE_SIZE &&
            abs(droney - posy[i]) < DRONE_SIZE) {
            sobre_vitima = true;
            break;
        }
    }

    gpio_put(BLUE, sobre_vitima);
}