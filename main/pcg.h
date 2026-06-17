#ifndef PCG_H
#define PCG_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct
{
    uint64_t timestamp_us;
    int32_t pcg;
} pcg_sample_t;

void pcg_init(QueueHandle_t queue);

#endif