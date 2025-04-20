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

// Definição dos pinos do display
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C

// Estrutura do display
ssd1306_t ssd;

// Variavel do Cronômetro
char timer[4];

// Constantes de quantidade e tamanho
#define MAX_VITIMAS 5
#define DRONE_SIZE 8
#define VITIMA_SIZE 4

// Posição x e y das vitimas
int posx[MAX_VITIMAS];
int posy[MAX_VITIMAS];

// Coordenadas do drone
int dronex;
int droney;

// Função pra atualizar o cronômetro
bool update_timer(int, int);

// Função pra desenhar o drone ou a vitima
void draw_object(int x, int y, int size);

// Gera as coordenadas aleatórias das vitimas
void posicionar_vitimas();

// Desenha as vitimas no display
void desenhar_vitimas();

// Posiciona o drone
void posicionar_drone();

// Move o drone
void mover_drone();

int main()
{
    stdio_init_all();

    // iniciando o adc
    adc_init();
    adc_gpio_init(EIXO_X); 
    adc_gpio_init(EIXO_Y); 

    // Iniciando os leds
    gpio_init(RED);
    gpio_set_dir(RED, GPIO_OUT);
    
    gpio_init(BLUE);
    gpio_set_dir(RED, GPIO_OUT);
    
    gpio_init(GREEN);
    gpio_set_dir(RED, GPIO_OUT);

    // Configurações do display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    posicionar_vitimas();
    posicionar_drone();

    // Cronômetro
    int count = 0;
    // posição do cronometro
    int x = 0;

    while (true) {
        ssd1306_fill(&ssd, false);
        snprintf(timer, sizeof(timer), "%d", count);
        ssd1306_rect(&ssd, 0, 0, 128, 64, true, false);
        
        mover_drone();
        desenhar_vitimas();
        draw_object(dronex, droney, DRONE_SIZE);
        
        update_timer(count, x);
        ssd1306_send_data(&ssd);

        sleep_ms(1000);
        count += 1;
    }
}

bool update_timer(int tempo, int x) {
    // Tempo de 2 min pra completar o desafio
    if(tempo <= 120) {
        // Ajeita a pos do cronômetro no display
        if((int)(floor(log10(tempo)) + 1) == 1 || tempo == 0)
            x = 119;
        if ((int)(floor(log10(tempo) + 1)) == 2)
            x = 111;
        if((int)(floor(log10(tempo) + 1)) == 3)
            x = 103;

        ssd1306_draw_string(&ssd, timer, x, 2);
    }
    else {
        ssd1306_fill(&ssd, false);
        ssd1306_rect(&ssd, 1, 1, 127, 63, true, false);

        // Informa que o jogador perdeu e liga o led vermelho
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
    srand(time_us_32()); // Semente aleatória baseada no tempo

    for (int i = 0; i < MAX_VITIMAS; i++) {
        bool pos_valida = false;

        while (!pos_valida) {
            pos_valida = true;

            // Gera coordenadas dentro dos limites seguros
            posx[i] = (rand() % (118 - 4 + 1)) + 4; // entre 4 e 118
            posy[i] = (rand() % (59 - 12 + 1)) + 8;  // entre 12 e 59

            // Verifica distância das vítimas anteriores
            for (int j = 0; j < i; j++) {
                int dx = abs(posx[i] - posx[j]);
                int dy = abs(posy[i] - posy[j]);

                if (dx < VITIMA_SIZE * 2 && dy < VITIMA_SIZE * 2) {
                    pos_valida = false; // muito próxima, tenta de novo
                    break;
                }
            }
        }
    }
}

void desenhar_vitimas() {
    for (int i = 0; i < MAX_VITIMAS; i++) {
        draw_object(posx[i], posy[i], VITIMA_SIZE);
    }
}

void posicionar_drone() {
    bool pos_valida = false;

    while (!pos_valida) {
        pos_valida = true;

        dronex = (rand() % (118 - 8 + 1)) + 4;  // entre 4 e 118
        droney = (rand() % (55 - 8 + 1)) + 8;   // entre 8 e 59

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

    // Leitura eixo X
    adc_select_input(1);
    uint16_t val_x = adc_read();

    // Leitura eixo Y
    adc_select_input(0);
    uint16_t val_y = adc_read();

    printf("%d | %d\n", val_x, val_y);

    // Movimento horizontal
    if (val_x < limiar_baixo && dronex > 4)
        dronex -= passo;
    else if (val_x > limiar_alto && dronex < 120)
        dronex += passo;

    // Movimento vertical
    if (val_y < limiar_baixo && droney > 8)
        droney += passo;
    else if (val_y > limiar_alto && droney < 56)
        droney -= passo;
}