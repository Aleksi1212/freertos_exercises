#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <vector>
#include <memory>
#include "FreeRTOS.h"
#include <iostream>
#include "task.h"
#include "semphr.h"
#include <iomanip>


#include "hardware/timer.h"
extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
}
}

#define LED_1 20
#define LED_2 21
#define LED_3 22

#define ROT_A 10
#define ROT_B 11
#define ROT_SW 12

#define MAX_LED_LEVEL 1000
#define MAX_BLINK_FREQ 200
#define FREQ_INCR 10

static QueueHandle_t q;

enum RotEvent {
    PRESS = 0x1u,
    INCREMENT = 0x2u,
    DECREMENT = 0x4u
};

class Led
{
    private:
        uint pin;
        uint pwm_slice;
        uint16_t blink_freq;
        bool on;
        
    public:

        Led(uint _pin) : pin(_pin), on(false), blink_freq(MAX_BLINK_FREQ / 100)
        {
            gpio_set_function(pin, GPIO_FUNC_PWM);
            pwm_slice = pwm_gpio_to_slice_num(pin);

            pwm_set_clkdiv(pwm_slice, 125.0f);
            pwm_set_wrap(pwm_slice, MAX_LED_LEVEL-1);
            pwm_set_enabled(pwm_slice, true);
        }
        void operator()(bool togg)
        {
            pwm_set_chan_level(pwm_slice,
                pwm_gpio_to_channel(pin), togg ? MAX_LED_LEVEL / 5 : 0);
        }
        bool operator()() { return on; }

        void toggle()
        {
            on = !on;
            operator()(on);
        }
        Led& operator++(int) {
            if (on && blink_freq <= MAX_BLINK_FREQ - FREQ_INCR) {
                blink_freq += FREQ_INCR;
                std::cout << "\rBlinking frequency: " << std::setw(3) << blink_freq << std::flush;
            }
            return *this;
        }
        Led& operator--(int) {
            if (on && blink_freq >= (MAX_BLINK_FREQ / 100) + FREQ_INCR) {
                blink_freq -= FREQ_INCR;
                std::cout << "\rBlinking frequency: " << std::setw(3) << blink_freq << std::flush;

            }
            return *this;
        }
        float get_delay_ms() { return ((1.0f / blink_freq) * 1000); }
};

void gpio_callback(uint gpio, uint32_t event_mask)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t event;
    switch (gpio)
    {
    case ROT_SW:
        if (event_mask & GPIO_IRQ_EDGE_RISE) {
            event = (uint8_t)PRESS;
        }
        break;
    case ROT_A:
        if (event_mask & GPIO_IRQ_EDGE_RISE) {
            event = gpio_get(ROT_B) ?
                (uint8_t)INCREMENT : (uint8_t)DECREMENT;
        }
        break;
    default:
        event = 0;
        break;
    }
    if (event) {
        xQueueSendToBackFromISR(q, &event, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void control_leds_task(void *param)
{
    std::vector<std::shared_ptr<Led>> *led_vec =
        (std::vector<std::shared_ptr<Led>>*)param;

    gpio_init(ROT_SW);
    gpio_set_dir(ROT_SW, GPIO_IN);
    gpio_pull_up(ROT_SW);
    gpio_set_irq_enabled_with_callback(ROT_SW,
        GPIO_IRQ_EDGE_RISE, true, &gpio_callback);


    gpio_init(ROT_A);
    gpio_set_dir(ROT_A, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ROT_A,
        GPIO_IRQ_EDGE_RISE, true, &gpio_callback);

    gpio_init(ROT_B);
    gpio_set_dir(ROT_B, GPIO_IN);

    uint8_t event;

    while (true) {
        if (xQueueReceive(q, &event, pdMS_TO_TICKS(50)) == pdPASS) {
            for (auto it = led_vec->begin(); it != led_vec->end(); it++) {
                if (auto led = it->get()) {
                    if (event & PRESS) led->toggle();
                    else if ((*led)() && event & INCREMENT) (*led)++;
                    else if ((*led)() && event & DECREMENT) (*led)--;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void blink_leds_task(void *param)
{
    std::vector<std::shared_ptr<Led>> *led_vec =
        (std::vector<std::shared_ptr<Led>>*)param;

    float delay_ms = 0;
    bool on = false;
    
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        for (auto it = led_vec->begin(); it != led_vec->end(); it++) {
            if (auto led = it->get(); (*led)()) {
                delay_ms = led->get_delay_ms();
                (*led)(on);
            }
        }
        on = !on;
    }
}

int main(void) {
    stdio_init_all();

    static std::vector<std::shared_ptr<Led>> led_vec = {
        std::make_shared<Led>(LED_1),
        std::make_shared<Led>(LED_2),
        std::make_shared<Led>(LED_3)
    };

    q = xQueueCreate(10, sizeof(uint8_t));
    vQueueAddToRegistry(q, "EventQueue");

    xTaskCreate(control_leds_task, "CONTROL_LEDS", 512, &led_vec, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(blink_leds_task, "BLINK_LEDS", 512, &led_vec, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();

    while(1){};

    return 0;
}
