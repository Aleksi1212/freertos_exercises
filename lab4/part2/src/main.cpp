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

#define BUTTON_1 7
#define BUTTON_2 8
#define BUTTON_3 9

#define EG_BIT_0 (1 << 0)
#define EG_BIT_4 (1 << 4)
#define EG_BIT_8 (1 << 8)

static EventGroupHandle_t eg;

struct button_t {
    uint pin;
    uint16_t event_bit;
    EventGroupHandle_t e;
    std::shared_ptr<Debug> debug;
};
struct watchdog_t {
    EventGroupHandle_t e;
    std::shared_ptr<Debug> debug;
};

void button_task(void *param)
{
    button_t *b = (button_t*)param;
    auto dbg = b->debug;

    gpio_init(b->pin);
    gpio_set_dir(b->pin, GPIO_IN);
    gpio_pull_up(b->pin);
    vTaskDelay(pdMS_TO_TICKS(10));

    bool prev_state = true;

    while (true) {
        bool curr_state = gpio_get(b->pin);
        if (!prev_state && curr_state) {
            xEventGroupSetBits(b->e, b->event_bit);
            dbg->print("Button %d pressed", b->pin, 0, 0);
        }
        prev_state = curr_state;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void watchdog_task(void *param)
{
    watchdog_t *w = (watchdog_t*)param;
    auto dbg = w->debug;

    while (true) {
        EventBits_t bits = xEventGroupWaitBits(w->e, 
            EG_BIT_0 | EG_BIT_4 | EG_BIT_8,
            pdTRUE,
            pdTRUE,
            pdMS_TO_TICKS(5000)
        );

        std::cout << (bits & EG_BIT_0) << std::endl;
        std::cout << (bits & EG_BIT_4) << std::endl;
        std::cout << (bits & EG_BIT_8) << std::endl;

    }
}

int main(void) {
    stdio_init_all();

    auto debug = std::make_shared<Debug>();
    eg = xEventGroupCreate();

    static button_t button_1 = { .pin = BUTTON_1, .event_bit = EG_BIT_0,  .e = eg, .debug = debug };
    static button_t button_2 = { .pin = BUTTON_2, .event_bit = EG_BIT_4, .e = eg, .debug = debug };
    static button_t button_3 = { .pin = BUTTON_3, .event_bit = EG_BIT_8, .e = eg, .debug = debug };

    static watchdog_t watchdog = { .e = eg, .debug = debug };

    xTaskCreate(button_task, "BUTTON_1", 512, &button_1, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(button_task, "BUTTON_2", 512, &button_2, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(button_task, "BUTTON_3", 512, &button_3, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(watchdog_task, "WATCHDOG", 512, &watchdog, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();

    while (1){};

    return 0;
}
