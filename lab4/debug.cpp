#include "debug.h"

struct debug_event_t {
    const char *fmt;
    time_t timestamp;
    uint32_t data[3];
};

Debug::Debug()
{ 
    q = xQueueCreate(10, sizeof(debug_event_t));
    xTaskCreate(Debug::task, "DEBUG", 512, static_cast<void *>(this), tskIDLE_PRIORITY + 1, NULL);
}
void Debug::print(const char *format, uint32_t d1, uint32_t d2, uint32_t d3)
{
    debug_event_t e = { 
        .fmt = format, 
        .timestamp = time(NULL), 
        .data = { d1, d2, d3 } 
    };
    xQueueSendToBack(q, &e, pdMS_TO_TICKS(10));
}

void Debug::task(void *param)
{
    Debug *d = static_cast<Debug *>(param);
    
    char time_buff[64];
    char debug_msg_buff[128];
    debug_event_t e;

    while (true) {
        if (xQueueReceive(d->q, &e, pdMS_TO_TICKS(50))) {
            struct tm *local = localtime(&e.timestamp);

            snprintf(time_buff, sizeof(time_buff), "(%02d:%02d:%02d) ", local->tm_hour, local->tm_min, local->tm_sec);
            snprintf(debug_msg_buff, sizeof(debug_msg_buff), e.fmt, e.data[0], e.data[1], e.data[2]);

            std::cout << time_buff << debug_msg_buff << std::endl;
        }
    }
}