#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h" // Thư viện FreeRTOS Timer dùng thay thế cho delay
#include "esp_log.h"

#include "ecg.h"
#include "pcg.h"

static const char *TAG = "MAIN_APP";
static QueueHandle_t ecg_queue;
static QueueHandle_t pcg_queue;

// Hàm callback của Monitor - Gọi ngầm mỗi 1 giây (KHÔNG CÒN WHILE(1) VÀ DELAY)
static void monitor_timer_callback(TimerHandle_t xTimer)
{

}

// Task tiêu thụ dữ liệu - ĐÃ ĐỒNG BỘ THỜI GIAN THEO TẦN SỐ KHÁC NHAU (KHÔNG DELAY)
// Task tiêu thụ dữ liệu - ĐỒNG BỘ 400HZ GỌN GÀNG CHO TỐC ĐỘ 115200 BAUD
static void task_consumer(void *arg)
{
    ecg_sample_t ecg_data;
    pcg_sample_t pcg_data;
    
    int32_t last_ecg_value = 0;
    uint32_t pcg_counter = 0;

    while(1)
    {
        // 1. Cập nhật giá trị ECG mới nhất nếu có (Tần số 400Hz)
        if (xQueueReceive(ecg_queue, &ecg_data, 0) == pdTRUE) 
        {
            last_ecg_value = ecg_data.ecg;
        }

        // 2. Nhận dữ liệu PCG tốc độ cao (16kHz)
        if (xQueueReceive(pcg_queue, &pcg_data, portMAX_DELAY) == pdTRUE) 
        {
            pcg_counter++;

            // Cứ đúng 40 mẫu PCG (~400Hz), ta mới in ra Serial một lần để tránh nghẽn luồng 115200
            if (pcg_counter >= 40) 
            {
                pcg_counter = 0;

                // Xử lý toán học ép kiểu Signed 24-bit để loại bỏ số khổng lồ 83xxxxx
                int32_t corrected_pcg = (pcg_data.pcg << 8) >>8 ; // Dịch trái rồi dịch phải để giữ nguyên bit dấu của số 24-bit gốc
                

                /*
                 * In đồng thời cả 2 kênh lên đồ thị máy tính.
                 * Tần số xuất dữ liệu lúc này là 400Hz, cổng 115200 tải cực kỳ mượt mà.
                 */
                printf("PCG:%ld,  ECG:%ld\n", (long)corrected_pcg, (long)last_ecg_value);
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Medical Signal Acquisition System...");

    // 1. Khởi tạo các hàng đợi kết nối dữ liệu (Queue)
    ecg_queue = xQueueCreate(512, sizeof(ecg_sample_t));
    pcg_queue = xQueueCreate(2048, sizeof(pcg_sample_t));

    if (ecg_queue == NULL || pcg_queue == NULL) {
        ESP_LOGE(TAG, "Queue creation failed!");
        return;
    }

    // 2. Kích hoạt driver phần cứng độc lập (ECG ADC và PCG I2S)
    ecg_init(ecg_queue);
    pcg_init(pcg_queue);

    // 3. Tạo Task tiêu thụ và định dạng dữ liệu (Chạy trên Core 0, độ ưu tiên trung bình)
    xTaskCreatePinnedToCore(task_consumer, "consumer", 4096, NULL, 3, NULL, 0);

    // 4. Khởi tạo bộ định thời phần mềm chạy ngầm thay thế hoàn toàn task_monitor cũ
    TimerHandle_t monitor_timer = xTimerCreate(
        "monitor_timer",            // Tên của Timer
        pdMS_TO_TICKS(1000),        // Định thời chu kỳ 1 giây (1000ms)
        pdTRUE,                     // Tự động lặp lại liên tục (Auto-reload)
        (void *)0,                  // ID (không dùng đến)
        monitor_timer_callback      // Hàm callback thực hiện in trạng thái
    );

    if (monitor_timer != NULL) {
        xTimerStart(monitor_timer, 0); // Kích hoạt chạy ngầm hệ thống
        ESP_LOGI(TAG, "Monitor software timer started successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to create monitor software timer!");
    }
}
