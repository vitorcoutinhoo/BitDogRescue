#include <stdio.h>
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
char time[4];

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

    // Cronômetro
    int count = 90;

    while (true) {
        sniprintf(time, sizeof(time), "%d", count);
        ssd1306_rect(&ssd, 1, 1, 127, 63, true, false);

        // tempo de 2 min pra completar o desafio
        if(count <= 120)
            ssd1306_draw_string(&ssd, time, 56, 28);
        else {
            ssd1306_fill(&ssd, false);
            ssd1306_rect(&ssd, 1, 1, 127, 63, true, false);
            ssd1306_draw_string(&ssd, "Voce Perdeu", 20, 26);
            gpio_put(RED, true);

            // volta pra tela inicial
        }
        ssd1306_send_data(&ssd);

        sleep_ms(1000);
        count += 1;
    }
}
