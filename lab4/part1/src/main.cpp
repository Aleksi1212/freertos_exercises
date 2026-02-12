#include <stdio.h>
#include "pico/stdlib.h"
#include "debug.h"
#include "event_groups.h"
#include <memory>


#include "hardware/timer.h"
extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
}
}

#define MIN_MS 1000
#define MAX_MS 1000 * 2

#define BUTTON 8
#define EG_BIT_0 (1 << 0)

static EventGroupHandle_t eg;

struct button_t {
    uint pin;
    EventGroupHandle_t e;
};
struct debug_t {
    std::shared_ptr<Debug> d;
    EventGroupHandle_t e;
    uint8_t number;
};

void button_task(void *param)
{
    button_t *b = (button_t*)param;

    gpio_init(b->pin);
    gpio_set_dir(b->pin, GPIO_IN);
    gpio_pull_up(b->pin);

    bool prev_state = true;

    while (true) {
        bool curr_state = gpio_get(b->pin);
        if (!prev_state && curr_state) {
            xEventGroupSetBits(b->e, EG_BIT_0);
        }
        prev_state = curr_state;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void print_task(void *param)
{
    debug_t *debug = (debug_t*)param;
    auto d = debug->d;
    srand(debug->number);

    while (true) {
        EventBits_t bits = xEventGroupWaitBits(debug->e, EG_BIT_0,
            pdFALSE, pdFALSE, pdMS_TO_TICKS(500));
        if (bits & EG_BIT_0) {
            int random_ms = (rand() % 1001) + 1000;
            vTaskDelay(pdMS_TO_TICKS(random_ms));
            d->print("Task %d ticks since last print: %d", debug->number, random_ms, 0);
        }
    }
}

int main(void) {
    stdio_init_all();

    auto debug = std::make_shared<Debug>();
    eg = xEventGroupCreate();

    static button_t button_p = { .pin = BUTTON, .e = eg };
    static debug_t debug_p1 = { .d = debug, .e = eg, .number = 1 };
    static debug_t debug_p2 = { .d = debug, .e = eg, .number = 2 };

    xTaskCreate(button_task, "BUTTON", 512, &button_p, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(print_task, "PRINT_1", 512, &debug_p1, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(print_task, "PRINT_2", 512, &debug_p2, tskIDLE_PRIORITY + 2, NULL);
    vTaskStartScheduler();

    while (1){};
    return 0;
}
