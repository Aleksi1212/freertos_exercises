#include <iostream>
#include "pico/stdlib.h"
#include "PicoOsUart.h"
#include "task.h"
#include "timers.h"
#include <string.h>
#include <memory>
#include <utils.h>
#include <cmath>

#include "hardware/timer.h"
extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
}
}

#define UART_NR 0
#define UART_TX 0
#define UART_RX 1
#define UART_SPEED 115200
#define COMMAND_BUF_SIZE 64
#define HELP \
    "\n\rhelp - display usage instructions\n\r" \
    "interval <number> - set the led toggle interval in milliseconds (default is 5 seconds)\n\r" \
    "time - prints the number of seconds with 0.1s accuracy since the last led toggle\n\r"

class Led {
private:
    uint pin;
    bool on;
    uint32_t interval_ms;
    TimerHandle_t timer;
    uint32_t time_since_togg;

    static void blink_timer_cb(TimerHandle_t xTimer) {
        Led *led = static_cast<Led *>(pvTimerGetTimerID(xTimer));

        led->toggle();
        vTimerSetTimerID(xTimer, static_cast<void *>(led));
    }
    static void clock(void *param) {
        Led *led = static_cast<Led *>(param);

        const int delay_ms = 100;
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            led->time_since_togg += delay_ms;
        }
    }

public:
    Led(uint _pin) : pin(_pin), on(false), interval_ms(5000) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);

        timer = xTimerCreate("LED_TIMER", pdMS_TO_TICKS(interval_ms), pdTRUE,
            static_cast<void *>(this),
            Led::blink_timer_cb
        );
        xTaskCreate(Led::clock, "CLOCK", 512, static_cast<void *>(this), tskIDLE_PRIORITY+1, NULL);

        xTimerStart(timer, 0);
    }
    void toggle() {
        on = !on;
        time_since_togg = 0;
        gpio_put(pin, on);
    }
    void set_interval(uint32_t interval) {
        interval_ms = interval;
        xTimerChangePeriod(timer, pdMS_TO_TICKS(interval_ms), 0);
    }
    double get_time_since_togg() {
        double time_s = time_since_togg / 1000.0;
        return std::round(time_s * 10.0) / 10.0;
    }
};

class Uart : private PicoOsUart {
private:
    QueueHandle_t command_queue;
    TimerHandle_t timer;
    std::shared_ptr<Led> led;

    int index = 0;
    char command_buff[COMMAND_BUF_SIZE];

    static void timer_cb(TimerHandle_t xTimer) {
        Uart *u = static_cast<Uart *>(pvTimerGetTimerID(xTimer));

        uint8_t inactive[] = "\n\r[Inactive]\n\r";
        u->write(inactive, strlen((const char *)inactive));
        u->flush();

        u->index = 0;
        memset(u->command_buff, 0, COMMAND_BUF_SIZE);

        vTimerSetTimerID(xTimer, static_cast<void *>(u));
    }
    static void read_command_task(void *param) {
        Uart *u = static_cast<Uart *>(param);

        uint8_t read_buff[COMMAND_BUF_SIZE];
        while (true) {
            int count = u->read(read_buff, COMMAND_BUF_SIZE, 30);
            if (count > 0) {
                xTimerStart(u->timer, 10);

                for (int i = 0; i < count; i++) {
                    if (u->index < COMMAND_BUF_SIZE - 1)
                    { u->command_buff[u->index++] = (char)read_buff[i]; }
                }

                u->write(read_buff, count);
                if (read_buff[count - 1] == '\n' || read_buff[count - 1] == '\r') {
                    u->command_buff[u->index-1] = '\0';
                    xTimerStop(u->timer, 10);
                    xQueueSendToBack(u->command_queue, u->command_buff, pdMS_TO_TICKS(50));
                    u->index = 0;
                    memset(u->command_buff, 0, COMMAND_BUF_SIZE);
                }
            }
        }
    }
    static void handle_command_task(void *param) {
        Uart *u = static_cast<Uart *>(param);
        auto led = u->led;
        
        char result_buff[COMMAND_BUF_SIZE];
        while (true) {
            if (xQueueReceive(u->command_queue, result_buff, pdMS_TO_TICKS(50))) {
                std::string cmd_str(result_buff);
                std::transform(cmd_str.begin(), cmd_str.end(), cmd_str.begin(), ::toupper);
                auto cmd_parts = split(cmd_str, ' ');

                uint32_t interval;
                if (cmd_parts[0] == "HELP") {
                    u->write((const uint8_t *)HELP, strlen(HELP));
                }
                else if (cmd_parts[0] == "INTERVAL") {
                    if (cmd_parts.size() == 2 && to_uint32(cmd_parts[1], interval)) {
                        led->set_interval(interval);
                    } else {
                        led->set_interval(5000);
                    }
                    uint8_t msg[] = "\n\rSuccess\n\r";
                    u->write(msg, strlen((const char *)msg));
                }
                else if (cmd_parts[0] == "TIME") {
                    double time = led->get_time_since_togg();
                    char msg_buf[50];
                    snprintf(msg_buf, sizeof(msg_buf), "\n\rTime since led toggle: %.1f s\n\r", time);
                    u->write((const uint8_t*)msg_buf, strlen(msg_buf));
                }
                else {
                    uint8_t error[] = "\n\runknown command\n\r";
                    u->write(error, strlen((const char *)error));
                }
            }
        }
    }
public:
    Uart(std::shared_ptr<Led> led_p) : 
        PicoOsUart(UART_NR, UART_TX, UART_RX, UART_SPEED),
        led(led_p)
    {
        command_queue = xQueueCreate(10, COMMAND_BUF_SIZE);

        timer = xTimerCreate("UART_TIMER", pdMS_TO_TICKS(30000),
            pdFALSE,
            static_cast<void *>(this),
            Uart::timer_cb
        );
        xTaskCreate(Uart::read_command_task, "UART_READ_TASK", 512,
            static_cast<void *>(this),
            tskIDLE_PRIORITY + 1,
            NULL
        );
        xTaskCreate(Uart::handle_command_task, "UART_HANDLE_TASK", 512,
            static_cast<void *>(this),
            tskIDLE_PRIORITY + 1,
            NULL
        );
    }
};

int main(void) {
    stdio_init_all();

    auto led = std::make_shared<Led>(21);
    Uart _(led);
    vTaskStartScheduler();

    while (1) {};
    return 0;
}
