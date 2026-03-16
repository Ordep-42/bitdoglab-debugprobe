#include "oled_display.h"
#include "ssd1306.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <string.h>

#define OLED_I2C_PORT i2c1
#define OLED_SDA_PIN 14
#define OLED_SCL_PIN 15
#define OLED_I2C_FREQ 400000
#define OLED_ADDR 0x3C

#define LINE_BUF_SIZE 256

static ssd1306_t display;

static char line_buf[LINE_BUF_SIZE];
static uint16_t line_len = 0;

void oled_display_init(void) {
	i2c_init(OLED_I2C_PORT, OLED_I2C_FREQ);
	gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(OLED_SDA_PIN);
	gpio_pull_up(OLED_SCL_PIN);

	ssd1306_init(&display, 128, 64, OLED_ADDR, OLED_I2C_PORT);
	ssd1306_clear(&display);
	ssd1306_draw_string(&display, 0, 0, 1, "Conecte o STM32");
	ssd1306_show(&display);
}

// Rola as linhas do display pra cima e escreve a nova na última linha
static void oled_println(const char *line) {
    // O display tem 8 linhas de 8px cada com escala 1
    // Vamos usar 4 linhas com escala 1 (8px cada) = 32px usados de 64
    // Simples: redesenha tudo com scroll manual num array de 4 linhas

    #define MAX_LINES 7
    static char lines[MAX_LINES][LINE_BUF_SIZE];

    // Sobe tudo uma linha
    for (int i = 0; i < MAX_LINES - 1; i++) {
        strncpy(lines[i], lines[i + 1], LINE_BUF_SIZE - 1);
    }
    // Insere nova linha no final
    strncpy(lines[MAX_LINES - 1], line, LINE_BUF_SIZE - 1);

    ssd1306_clear(&display);
    for (int i = 0; i < MAX_LINES; i++) {
        if (lines[i][0] != '\0') {
            ssd1306_draw_string(&display, 0, i * 9, 1, lines[i]);
        }
    }
    ssd1306_show(&display);
}

void oled_push_byte(uint8_t byte) {
    if (byte == '\n' || byte == '\r') {
        if (line_len > 0) {
            line_buf[line_len] = '\0';
            oled_println(line_buf);
            line_len = 0;
        }
        return;
    }

    if (line_len < LINE_BUF_SIZE - 1) {
        line_buf[line_len++] = (char)byte;
    }
    // Se chegou no limite sem \n, força exibição
    if (line_len == LINE_BUF_SIZE - 1) {
        line_buf[line_len] = '\0';
        oled_println(line_buf);
        line_len = 0;
    }
}
