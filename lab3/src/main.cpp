#include <iostream>
#include "pico/stdlib.h"
#include "PicoOsUart.h"
#include "task.h"
#include "timers.h"
#include <string.h>
#include <memory>

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

#define COMMAND_BUF_SIZE 64
class Uart : private PicoOsUart {
private:
    // uint8_t command_buf[COMMAND_BUF_SIZE];
    QueueHandle_t command_queue;
    TimerHandle_t timer;
    std::shared_ptr<Led> led;

    static void timer_cb(TimerHandle_t xTimer) {
        Uart *u = static_cast<Uart *>(pvTimerGetTimerID(xTimer));

        const char inactive[] = "\n\r[Inactive]\n\r";
        u->write((const uint8_t *)inactive, strlen(inactive));
        u->flush();

        vTimerSetTimerID(xTimer, static_cast<void *>(u));
    }
    static void read_command_task(void *param) {
        Uart *u = static_cast<Uart *>(param);

        uint8_t local_buf[COMMAND_BUF_SIZE];
        int index = 0;
        while (true) {
            int count = u->read(local_buf, COMMAND_BUF_SIZE, 30);
            if (count > 0) {
                xTimerStart(u->timer, 0);
                // u->command_buf[index++] = local_buf[count-1];

                u->write(local_buf, count);
                if (local_buf[count-1] == '\n' || local_buf[count-1] == '\r') {

                    auto resp = xQueueSendToBack(u->command_queue, static_cast<void *>(local_buf),
                        pdMS_TO_TICKS(100)
                    );
                    if (resp != pdPASS) std::cout << "ERROR" << std::endl;
                    xTimerStop(u->timer, 0);
                }
            }
        }
    }
    static void handle_command_task(void *param) {
        Uart *u = static_cast<Uart *>(param);
        auto led = u->led;
        
        uint8_t result_buff[COMMAND_BUF_SIZE];
        while (true) {
            if (xQueueReceive(u->command_queue, static_cast<void *>(result_buff),
                pdMS_TO_TICKS(100))
            ) {
                u->write(result_buff, strlen((const char *)(result_buff)));
            }
        }
    }
public:
    Uart(std::shared_ptr<Led> led_p) : 
        PicoOsUart(UART_NR, UART_TX, UART_RX, UART_SPEED),
        led(led_p)
    {
        command_queue = xQueueCreate(COMMAND_BUF_SIZE * 2, COMMAND_BUF_SIZE);

        timer = xTimerCreate("UART_TIMER", pdMS_TO_TICKS(2000),
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

// void uart_timer_cb(TimerHandle_t xTimer) {
//     PicoOsUart *u = static_cast<PicoOsUart *>(pvTimerGetTimerID(xTimer));

//     const char inactive[] = "[Inactive]";
//     u->write((const uint8_t *)inactive, strlen(inactive));
//     u->flush();

//     vTimerSetTimerID(xTimer, static_cast<void *>(u));
// }

// void uart_task(void *param) {
//     Command_t *command = (Command_t*)param;

//     PicoOsUart u(UART_NR, UART_TX, UART_RX, UART_SPEED);
//     while (true) {
//         int count = u.read(command->buffer, command->buffer_size, 30);
//         if (count > 0) {
//             u.write(command->buffer, count);
//             if (command->buffer[count-1] == '\n' || command->buffer[count-1] == '\r') {
//                 char test[] = "\ndoing command\n\r";
//                 u.write((const uint8_t*)test, strlen(test));
//             }
//         }
//     }
// }


int main(void) {
    stdio_init_all();

    auto led = std::make_shared<Led>(21);
    Uart u(led);

    // static TimerHandle_t uart_timer = xTimerCreate("UART_TIMER", pdMS_TO_TICKS(30000), pdFALSE,

    // )

    // static uint8_t buf[64];
    // static Command_t command = { .buffer = buf, .buffer_size = sizeof(buf) };
    // xTaskCreate(uart_task, "UART", 512, static_cast<void *>(&command), tskIDLE_PRIORITY + 1, NULL);


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
