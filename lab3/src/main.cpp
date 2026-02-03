#include <iostream>
#include "pico/stdlib.h"
#include "PicoOsUart.h"
#include "task.h"
#include "timers.h"

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
#define HELP \
    "help - display usage instructions\n" \
    "interval <number> - set the led toggle interval (default is 5 seconds)\n" \
    "time - prints the number of seconds with 0.1s accuracy since the last led toggle"


struct Command_t {
    uint8_t *buffer;
    size_t buffer_size;
    // TaskHandle_t timer;
};

class Led {
private:
    uint pin;
    bool on;
    uint16_t interval_ms;
    TimerHandle_t timer;

    static void blink_timer_cb(TimerHandle_t xTimer) {
        Led *led = static_cast<Led *>(pvTimerGetTimerID(xTimer));

        led->toggle();
        vTimerSetTimerID(xTimer, static_cast<void *>(led));
    }

public:
    Led(uint _pin) : pin(_pin), on(false), interval_ms(5000) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);

        timer = xTimerCreate("LED_TIMER", pdMS_TO_TICKS(interval_ms), pdTRUE,
            static_cast<void *>(this),
            Led::blink_timer_cb
        );
        xTimerStart(timer, 0);
    }
    void toggle() { on = !on; gpio_put(pin, on); }
    void set_interval(uint16_t interval) {
        interval_ms = interval;
        xTimerChangePeriod(timer, pdMS_TO_TICKS(interval_ms), 0);
    }
};

void uart_task(void *param) {
    Command_t *command = (Command_t*)param;

    PicoOsUart u(UART_NR, UART_TX, UART_RX, UART_SPEED);
    while (true) {
        int count = u.read(command->buffer, command->buffer_size, 30);
        if (count > 0) {
            u.write(command->buffer, command->buffer_size);
        }
    }

}


int main(void) {
    stdio_init_all();

    Led led(21);

    uint8_t buf[64];
    static Command_t command = { .buffer = buf, .buffer_size = sizeof(buf) };
    xTaskCreate(uart_task, "UART", 512, static_cast<void *>(&command), tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();

    while (1) {};

    // PicoOsUart uart(0, 0, 1, 115200);
    // uint8_t buffer[64];
    // while(true) {
    //     if (int count = uart.read(buffer, 64, 30); count > 0) {
    //         printf("test\n");
    //         // uart.write(buffer, count);
    //     }
    //     // std::cout << buffer << std::endl;
    // }

    return 0;
}
