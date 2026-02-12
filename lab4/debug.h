#ifndef DEBUG_H
#define DEBUG_H

#include <cstdint>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <iostream>
#include <time.h>


class Debug {
private:
    QueueHandle_t q;

    static void task(void *param);
public:
    Debug();
    void print(const char *format, uint32_t d1, uint32_t d2, uint32_t d3);
};

#endif