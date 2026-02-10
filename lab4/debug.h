#ifndef DEBUG_H
#define DEBUG_H

#include <cstdint>
#include "FreeRTOS.h"
#include "queue.h"
#include <iostream>
#include <time.h>
#include "hardware/rtc.h"
#include "pico/stdlib.h"

struct debug_event_t {
    const char *fmt;
    time_t timestamp;
    uint32_t data[3];
};

class Debug {
private:
    QueueHandle_t q;

    static void task(void *param);
public:
    Debug();
    void print(const char *format, uint32_t d1, uint32_t d2, uint32_t d3);
};

#endif