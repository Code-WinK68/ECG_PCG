# Hệ thống Thu thập Đồng thời Tín hiệu Điện tim (ECG) và Âm tim (PCG) trên ESP32

Dự án này triển khai hệ thống thu thập dữ liệu sinh học thời gian thực, xử lý đồng thời hai luồng tín hiệu có tần số lệch nhau lớn: **Tín hiệu Điện tim (ECG - 400Hz)** và **Tín hiệu Âm tim (PCG - 16kHz)**. Mã nguồn được tối ưu hóa hoàn toàn dựa trên kiến trúc hướng sự kiện (Event-driven) của FreeRTOS, tương thích hoàn hảo với **ESP-IDF v5.4.4**.

---

## 🚀 Tính năng nổi bật

* **Tương thích ESP-IDF v5.4.x**: Sử dụng hoàn toàn Driver I2S chuẩn mới (`driver/i2s_std.h`) và Driver ADC Oneshot mới (`esp_adc/adc_oneshot.h`).
* **Không dùng hàm Delay (`vTaskDelay`)**: Toàn bộ hệ thống thu thập và tiêu thụ dữ liệu hoạt động dựa trên ngắt cứng, ngắt mềm (High-Resolution Timer) và đồng bộ bằng Queue/Semaphore để tối ưu tài nguyên CPU, loại bỏ hoàn toàn lỗi sập Watchdog (WDT).
* **Đồng bộ hóa tần số hiển thị**: Hệ thống tự động xử lý giữ nguyên giá trị ECG cũ (Lập cấu trúc lặp mẫu) song song với luồng PCG và hạ dải mẫu hiển thị cổng Serial (Downsampling) về mức **400Hz** giúp cổng truyền **115200 Baud** hoạt động mượt mà, không bị nghẽn mạch.
* **Bộ lọc toán học bit dấu**: Tích hợp thuật toán dịch chuyển bit số học xử lý triệt để lỗi tràn bit dấu (Signed 24-bit sang 32-bit) của Micro I2S ICS43434, đưa biên độ âm thanh về dải số chuẩn đối xứng qua trục 0.

---

## 🎛️ Sơ đồ kết nối phần cứng (GPIO Pinout)

Hệ thống được cấu hình mặc định trên các chân GPIO của chip ESP32 (Ví dụ: ESP32-WROOM-32) như sau:

| Module Phần cứng | Chân Cảm biến | Chân GPIO ESP32 | Ghi chú |
| :--- | :--- | :--- | :--- |
| **Micro I2S ICS43434 (PCG)** | BCLK (Serial Clock) | **GPIO 26** | Chân tạo xung nhịp bit |
| | WS (Word Select) | **GPIO 25** | Chân chọn kênh (L/R) |
| | SD (Serial Data) | **GPIO 34** | Chân nhận dữ liệu (Input-Only) |
| **Mạch AD8232 (ECG)** | OUTPUT (Tín hiệu Analog) | **GPIO 36** | Cấu hình qua ADC1 Channel 0 |
| | LO+ (Lead-Off Plus) | **GPIO 16** | Kiểm tra đứt dây điện cực |
| | LO- (Lead-Off Minus) | **GPIO 4** | Kiểm tra đứt dây điện cực |

---

## 📂 Cấu trúc thư mục mã nguồn

```text
tên_dự_án/
├── CMakeLists.txt         # Cấu hình CMake tổng của dự án
├── sdkconfig              # File lưu cấu hình menuconfig của ESP-IDF
└── main/
    ├── CMakeLists.txt     # Liên kết thư viện (driver, esp_adc, esp_timer)
    ├── app_config.h       # Định nghĩa chân GPIO và tần số lấy mẫu (400Hz / 16kHz)
    ├── ecg.h / ecg.c      # Khởi tạo ADC Oneshot & Kích hoạt định thời esp_timer 400Hz
    ├── pcg.h / pcg.c      # Khởi tạo kênh I2S Standard Master RX nhận dữ liệu 16kHz
    └── main.c             # Task consumer FreeRTOS đồng bộ dữ liệu & xuất Serial
```

---

## 🛠️ Hướng dẫn cài đặt và biên dịch

### 1. Chuẩn bị môi trường
* Cài đặt **ESP-IDF v5.4.4** (hoặc các phiên bản thuộc nhánh v5.4.x).
* Đảm bảo Terminal đã được nạp biến môi trường của ESP-IDF (`export.sh` trên Linux/Mac hoặc chạy `ESP-IDF 5.4 CMD` trên Windows).

### 2. Biên dịch sạch bộ đệm và nạp chip
Để tránh lỗi kẹt bộ nhớ đệm liên kết driver cũ, bạn bắt buộc phải dọn dẹp dự án trước khi biên dịch:

```bash
# Xóa hoàn toàn bộ đệm build cũ
idf.py fullclean

# Biên dịch dự án
idf.py build

# Nạp chương trình xuống chip và mở màn hình giám sát (Thay COM4 bằng cổng của bạn)
idf.py -p COM4 flash monitor
```

---

## 📊 Hướng dẫn cấu hình hiển thị đồ thị 2 đường (Real-time Plotting)

Mã nguồn được cấu hình in dữ liệu ra cổng Serial theo cú pháp định dạng đồ thị chuẩn (`>Tên_Biến:Giá_Trị`). 

### Sử dụng Tiện ích mở rộng Teleplot (Khuyên dùng trên VS Code)
1. Cài đặt extension **Teleplot** từ chợ ứng dụng của VS Code.
2. Nhấn tổ hợp phím `Ctrl + Shift + P` (hoặc `Cmd + Shift + P`), gõ `Teleplot: Open Teleplot` để mở màn hình đồ thị.
3. Chọn cổng `COM` tương ứng của bạn và đặt Baudrate cố định ở mức **115200**, sau đó nhấn **Open**.
4. Nhấn vào biểu tượng dấu **`+` (Add Plot)** trên giao diện Teleplot để chia màn hình thành 2 ô đồ thị riêng biệt (trên và dưới):
   * **Ô đồ thị 1**: Tích chọn luồng dữ liệu `ECG` (Biên độ dao động dải 0 - 4095).
   * **Ô đồ thị 2**: Tích chọn luồng dữ liệu `PCG` (Biên độ dao động âm thanh đối xứng qua trục 0).

---

## ⚠️ Lưu ý quan trọng khi vận hành phần cứng
* **Tín hiệu ECG bằng 0 hoặc kịch trần 4095**: Mạch AD8232 rất nhạy cảm với nhiễu chuyển động và hở mạch. Hãy đảm bảo các miếng dán điện cực gel được tiếp xúc chắc chắn trên cơ thể người đo, ngồi giữ nguyên tư thế tĩnh để sóng bộ lọc ECG không bị bão hòa.
* **Micro PCG**: Chân GPIO 34 là chân chỉ nhận đầu vào (Input-only) và không có điện trở kéo nội bộ. Hãy chắc chắn bo mạch micro ICS43434 của bạn đã có sẵn trở kéo trên phần cứng để tín hiệu I2S không bị trôi nổi.
