#include "pcg.h"
#include "app_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/i2s_std.h" // Sử dụng driver std mới của v5.x
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "PCG_MODULE";
static QueueHandle_t pcg_queue;
static i2s_chan_handle_t rx_handle = NULL;

#define DMA_BUF_LEN 256
static int32_t buffer[DMA_BUF_LEN];

static void task_pcg(void *arg)
{
    // Bắt buộc kích hoạt kênh RX trước khi đọc dữ liệu
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    while(1)
    {
        size_t bytes_read = 0;
        // Thay thế i2s_read cũ bằng i2s_channel_read
        esp_err_t ret = i2s_channel_read(rx_handle, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(ret));
            continue;
        }

        int samples = bytes_read / sizeof(int32_t);
        uint64_t current_time = esp_timer_get_time();

        for(int i = 0; i < samples; i++)
        {
            pcg_sample_t sample;
            // Tính toán timestamp mịn lùi dần cho từng mẫu dựa trên tần số 16kHz (62.5us)
            sample.timestamp_us = current_time - ((samples - 1 - i) * 625 / 10);
            
            /* 
             * GIẢI PHÁP SỬA LỖI:
             * Ép kiểu sang (uint32_t) để thực hiện phép dịch phải logic (Logical Right Shift).
             * Sau đó kết quả dịch bit sẽ tự động ép kiểu ngược lại về kiểu số có dấu int32_t.
             * Cách này giữ nguyên vẹn cấu trúc bit dấu gốc của dữ liệu 24-bit từ micro ICS43434.
             */
            sample.pcg = (int32_t)((uint32_t)buffer[i] >> 8);

            // Đẩy vào Queue
            xQueueSend(pcg_queue, &sample, 0);
        }
    }
}

void pcg_init(QueueHandle_t queue)
{
    pcg_queue = queue;

    // Cấu hình tài nguyên phần cứng I2S0 dạng Master RX
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = DMA_BUF_LEN;
    chan_cfg.auto_clear = true;

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    // Cấu hình Slot và Clock chuẩn Philips I2S cho Micro ICS43434
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(PCG_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .bclk = I2S_BCLK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DATA_PIN,
        },
    };
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT; // ICS43434 mặc định gửi kênh trái

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));

    xTaskCreatePinnedToCore(task_pcg, "task_pcg", 4096, NULL, 5, NULL, 0);
    ESP_LOGI(TAG, "PCG Module initialized successfully.");
}
