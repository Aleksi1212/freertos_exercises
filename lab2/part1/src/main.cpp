#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "hardware/timer.h"
extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
}
}

struct led_t {
    uint pin;
    uint32_t delay_ms;
    SemaphoreHandle_t s;
};

void read_char_task(void *param)
{
    SemaphoreHandle_t smph = (SemaphoreHandle_t)param;

    while (true) {
        if (int c = getchar_timeout_us(0); c != PICO_ERROR_TIMEOUT) {
            putchar(c);
            xSemaphoreGive(smph);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void blink_task(void *param)
{
    led_t *led = (led_t*)param;

    gpio_init(led->pin);
    gpio_set_dir(led->pin, GPIO_OUT);

    while (true) {
        if (xSemaphoreTake(led->s, portMAX_DELAY) == pdPASS) {
            gpio_put(led->pin, true);
            vTaskDelay(pdMS_TO_TICKS(led->delay_ms));
            gpio_put(led->pin, false);
            vTaskDelay(pdMS_TO_TICKS(led->delay_ms));
        }
    }
}

int main(void) {
    stdio_init_all();

    static SemaphoreHandle_t smph = xSemaphoreCreateBinary();
    xSemaphoreGive(smph);

    static led_t blinker_led = { .pin = 21, .delay_ms = 100, .s = smph };

    xTaskCreate(read_char_task, "READ", 512, smph, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(blink_task, "BLINK", 512, &blinker_led, tskIDLE_PRIORITY + 1, NULL);
    vTaskStartScheduler();


    while(1){};

    return 0;
}
