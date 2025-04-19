#include <stdio.h>
#include <math.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"

// Definindo os pinos dos leds
#define BLUE 12
#define GREEN 11
#define RED 13

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

// Função pra atualizar o cronômetro
bool update_timer(int, int);

// Função pra desenhar o drone ou a vitima
void draw_object(int x, int y, int size);

// Gera as coordenadas aleatórias das vitimas
void gerar_vitimas();

// Desenha as vitimas no display
void desenhar_vitimas();


int main()
{
    stdio_init_all();
    
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

    gerar_vitimas();

    // Cronômetro
    int count = 0;
    // posição do cronometro
    int x = 0;

    while (true) {
        snprintf(timer, sizeof(timer), "%d", count);
        ssd1306_rect(&ssd, 0, 0, 128, 64, true, false);

        update_timer(count, x);
        desenhar_vitimas();
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
    ssd1306_rect(&ssd, x, y, size, size, true, true);
}

void gerar_vitimas(){
    srand(to_us_since_boot(get_absolute_time()));

    for(int i = 0; i < MAX_VITIMAS; i++){
        posx[i] = (rand() % (WIDTH / VITIMA_SIZE)) * VITIMA_SIZE;
        posy[i] = (rand() % (HEIGHT / VITIMA_SIZE)) * VITIMA_SIZE;
    }
}

void desenhar_vitimas() {
    for (int i = 0; i < MAX_VITIMAS; i++) {
        draw_object(posx[i], posy[i], VITIMA_SIZE);
    }
}