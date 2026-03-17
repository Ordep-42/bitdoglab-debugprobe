#include "oled_display.h"
#include "ssd1306.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>

#define OLED_I2C_PORT i2c1
#define OLED_SDA_PIN 14
#define OLED_SCL_PIN 15
#define OLED_I2C_FREQ 400000
#define OLED_ADDR 0x3C

#define LINE_BUF_SIZE 64
#define MAX_LINES 7
#define OLED_QUEUE_SIZE 256

static ssd1306_t display;
static QueueHandle_t oled_byte_queue;

static char lines[MAX_LINES][LINE_BUF_SIZE];

static void oled_scroll_println(const char *line) {
    for (int i = 0; i < MAX_LINES - 1; i++)
        strncpy(lines[i], lines[i + 1], LINE_BUF_SIZE - 1);
    strncpy(lines[MAX_LINES - 1], line, LINE_BUF_SIZE - 1);

    ssd1306_clear(&display);
    for (int i = 0; i < MAX_LINES; i++) {
        if (lines[i][0] != '\0')
            ssd1306_draw_string(&display, 0, i * 9, 1, lines[i]);
    }
    ssd1306_show(&display);
}

// Task dedicada — única função que toca o I2C/display
static void oled_task(void *ptr) {
    static char line_buf[LINE_BUF_SIZE];
    static uint16_t line_len = 0;
    uint8_t byte;

    while (1) {
        // Bloqueia aqui até chegar um byte — não desperdiça CPU
        if (xQueueReceive(oled_byte_queue, &byte, portMAX_DELAY) == pdTRUE) {
            if (byte == '\n' || byte == '\r') {
                if (line_len > 0) {
                    line_buf[line_len] = '\0';
                    oled_scroll_println(line_buf);
                    line_len = 0;
                }
            } else if (line_len < LINE_BUF_SIZE - 1) {
                line_buf[line_len++] = (char)byte;
                // Força exibição se linha cheia
                if (line_len == LINE_BUF_SIZE - 1) {
                    line_buf[line_len] = '\0';
                    oled_scroll_println(line_buf);
                    line_len = 0;
                }
            }
        }
    }
}

void oled_display_init(void) {
    i2c_init(OLED_I2C_PORT, OLED_I2C_FREQ);
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);

    ssd1306_init(&display, 128, 64, OLED_ADDR, OLED_I2C_PORT);
    ssd1306_clear(&display);
    ssd1306_draw_string(&display, 0, 0, 1, "Aguardando STM32");
    ssd1306_show(&display);

    oled_byte_queue = xQueueCreate(OLED_QUEUE_SIZE, sizeof(uint8_t));
}

void oled_task_create(void) {
    xTaskCreate(oled_task, "OLED", 512, NULL, tskIDLE_PRIORITY + 1, NULL);
}

// Chamado do cdc_task — apenas enfileira, nunca bloqueia
void oled_push_byte(uint8_t byte) {
    xQueueSendToBack(oled_byte_queue, &byte, 0);
}
