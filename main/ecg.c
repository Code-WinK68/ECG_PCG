#include "ecg.h"
#include "app_config.h"
/* Bắt buộc phải đưa FreeRTOS.h lên đầu tiên trước các sub-header của FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h" // Thêm include để sử dụng Queue

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"         // Thêm log để dễ dàng debug
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "ECG_MODULE";

static QueueHandle_t ecg_queue;
static SemaphoreHandle_t ecg_sem;
static adc_oneshot_unit_handle_t adc_handle;

// Hàm callback của timer (chạy trong ngữ cảnh ngắt mềm)
static void ecg_timer_callback(void *arg)
{
    // Giải phóng semaphore để đánh thức task đọc ADC
    xSemaphoreGive(ecg_sem);
}

// Task xử lý lấy mẫu dữ liệu ECG
static void task_ecg(void *arg)
{
    while(1)
    {
        // Chờ tín hiệu từ timer định kỳ (400Hz -> 2500 us)
        if (xSemaphoreTake(ecg_sem, portMAX_DELAY) == pdTRUE)
        {
            // Kiểm tra chân báo ngắt kết nối điện cực Lead-Off (LO+ và LO-)
            if(gpio_get_level(ECG_LO_PLUS_PIN) || gpio_get_level(ECG_LO_MINUS_PIN))
            {
                // Nếu điện cực bị hở, bỏ qua lượt lấy mẫu này
                continue;
            }

            int adc_raw;
            // Đọc dữ liệu thô từ bộ ADC Oneshot
            esp_err_t ret = adc_oneshot_read(adc_handle, ECG_ADC_CHANNEL, &adc_raw);
            
            if (ret == ESP_OK)
            {
                ecg_sample_t sample;
                sample.timestamp_us = esp_timer_get_time();
                sample.ecg = adc_raw;

                // Gửi dữ liệu vào Queue, nếu Queue đầy thì log cảnh báo thay vì im lặng bỏ qua
                if (xQueueSend(ecg_queue, &sample, 0) != pdTRUE)
                {
                    ESP_LOGW(TAG, "ECG Queue full! Sample dropped.");
                }
            }
            else
            {
                ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(ret));
            }
        }
    }
}

void ecg_init(QueueHandle_t queue)
{
    ecg_queue = queue;

    // Cấu hình các chân Lead-Off input kéo trở xuống hoặc kéo lên tùy thiết kế phần cứng của bạn
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ECG_LO_PLUS_PIN) | (1ULL << ECG_LO_MINUS_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // Thường AD8232 cần kéo chân LO xuống GND
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 1. Khởi tạo ADC Unit 1
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    // 2. Cấu hình Channel cho ADC (Dùng ADC_ATTEN_DB_11 để đo dải điện áp lớn nhất ~0-3.1V)
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12, 
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ECG_ADC_CHANNEL, &chan_cfg));

    // 3. Khởi tạo Semaphore để đồng bộ hóa task
    ecg_sem = xSemaphoreCreateBinary();
    if (ecg_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }

    // 4. Tạo và khởi động High-Resolution Timer định kỳ
    const esp_timer_create_args_t timer_args = {
        .callback = ecg_timer_callback,
        .name = "ecg_timer",
    };

    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    
    // Tần số 400Hz tương đương chu kỳ T = 1/400 = 0.0025 giây = 2500 micro giây
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 2500));

    // 5. Khởi tạo Task thu thập dữ liệu trên Core 1
    xTaskCreatePinnedToCore(
        task_ecg,
        "task_ecg",
        4096,
        NULL,
        4,
        NULL,
        1);
        
    ESP_LOGI(TAG, "ECG module initialized successfully at 400Hz.");
}
