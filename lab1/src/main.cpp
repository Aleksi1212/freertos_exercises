#include <iostream>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "hardware/timer.h"

#define SW_0 9
#define SW_1 8
#define SW_2 7

extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
}
}

struct button_t {
    uint pin;
    QueueHandle_t q;
};
struct lock_t {
    uint led_pin;
    QueueHandle_t q;
    uint32_t delay;
};

void button_read_task(void *param)
{
    button_t *button = (button_t*)param;

    gpio_init(button->pin);
    gpio_set_dir(button->pin, GPIO_IN);
    gpio_pull_up(button->pin);

    bool prev_state = true;

    while (true) {
        bool curr_state = gpio_get(button->pin);
        if (!prev_state && curr_state) {
            auto resp = xQueueSendToBack(button->q, static_cast<void*>(&button->pin), pdMS_TO_TICKS(500));
            assert(resp == pdPASS);
        }
        prev_state = curr_state;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void button_process_task(void *param)
{
    lock_t *lock = (lock_t*)param;
    uint16_t val;
    int idx = 0;
    bool open = false;

    gpio_init(lock->led_pin);
    gpio_set_dir(lock->led_pin, GPIO_OUT);

    while (true) {
        gpio_put(lock->led_pin, open);
        if (open) {
            vTaskDelay(pdMS_TO_TICKS(lock->delay));
            open = false;
            continue;
        }
        if (idx <= 4 && xQueueReceive(lock->q, &val, pdMS_TO_TICKS(lock->delay)) == pdPASS) {
            switch (idx)
            {
            case 0:
                if (val != SW_0)
                { idx = 0; continue; }
                break;
            case 1:
                if (val != SW_0)
                { idx = 0; continue; }
                break;
            case 2:
                if (val != SW_2)
                { idx = 0; continue; }
                break;
            case 3:
                if (val != SW_1)
                { idx = 0; continue; }
                break;
            case 4:
                idx = 0;
                if (val != SW_2) continue;
                else open = true;
                break;
            default:
                break;
            }
            idx++;
        }
    }
}

int main(void)
{
    stdio_init_all();
    
    static QueueHandle_t button_q = xQueueCreate(10, sizeof(uint16_t));
    static button_t button_1 = { .pin = SW_0, .q = button_q };
    static button_t button_2 = { .pin = SW_1, .q = button_q };
    static button_t button_3 = { .pin = SW_2, .q = button_q };

    static lock_t lock = { .led_pin = 21, .q = button_q, .delay = 5000 };

    xTaskCreate(button_read_task, "BUTTON_1", 512, &button_1, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(button_read_task, "BUTTON_2", 512, &button_2, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(button_read_task, "BUTTON_3", 512, &button_3, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(button_process_task, "BUTTON_P", 512, &lock, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();

    
    while (1){};
    return 0;
}
