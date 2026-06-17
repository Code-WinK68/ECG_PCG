#ifndef ECG_H
#define ECG_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct
{
    uint64_t timestamp_us;
    uint16_t ecg;
} ecg_sample_t;

void ecg_init(QueueHandle_t queue);

#endif