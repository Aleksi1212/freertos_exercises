#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "debug.h"

#include "hardware/timer.h"
extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
}
}

void sender(void *p) {
    Debug *d = static_cast<Debug *>(p);

    while (true) {
        d->print("Vittu kys %d, %d, %d", 1, 2, 3);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void) {
    stdio_init_all();

    Debug d;
    xTaskCreate(sender, "SENDER", 512, static_cast<void *>(&d), tskIDLE_PRIORITY + 2, NULL);
    vTaskStartScheduler();

    while (1){};
    return 0;
}
