
# GPS Tracker ESP32-S3 + OLED

GPS Tracker berbasis ESP32-S3 N16R8 dengan GPS NEO-M8N, MicroSD, Wi-Fi, API PMS, dan OLED SSD1306 128x64.

## Fitur

- Membaca koordinat GPS menggunakan NEO-M8N.
- Menyimpan data GPS ke MicroSD dalam format JSONL.
- Mengirim data GPS ke API PMS melalui HTTPS ketika Wi-Fi tersedia.
- Menampilkan status Wi-Fi dan kekuatan sinyal pada OLED.
- Menampilkan status GPS, MicroSD, latitude, longitude, speed, dan course.

## Wiring

### 1. OLED SSD1306 128x64 (I2C)

| Pin OLED | ESP32-S3 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 1 |
| SCL | GPIO 2 |

### 2. GPS NEO-M8N (UART)

| Pin GPS | ESP32-S3 |
|---|---|
| TX | GPIO 16 (RX) |
| RX | GPIO 17 (TX) |
| GND | GND |
| VCC | Sesuai spesifikasi modul |

### 3. MicroSD Onboard (SDMMC 1-bit)

| Sinyal SDMMC | ESP32-S3 |
|---|---|
| CMD | GPIO 38 |
| CLK | GPIO 39 |
| D0 | GPIO 40 |

MicroSD menggunakan slot onboard ESP32-S3 dan library `SD_MMC.h`.

## Tampilan OLED

OLED diperbarui setiap satu detik dan menampilkan:

- Status koneksi Wi-Fi: ONLINE atau OFFLINE.
- Indikator kekuatan sinyal Wi-Fi hingga 4 batang.
- Nilai RSSI dalam dBm.
- Status GPS: FIX atau WAIT.
- Status MicroSD: READY atau ERROR.
- Latitude dan longitude.
- Speed dalam knot dan course dalam derajat.

## Library

- TinyGPSPlus
- ArduinoHttpClient
- Adafruit GFX Library
- Adafruit SSD1306

Library `WiFi`, `WiFiClientSecure`, `Wire`, dan `SD_MMC` tersedia melalui dukungan board ESP32 pada Arduino IDE.

## Catatan

- Pastikan GPIO 1 dan GPIO 2 tersedia pada board yang digunakan.
- Alamat I2C OLED default adalah `0x3C`.
- Data GPS disimpan ke MicroSD setiap kali data valid diproses.
- Pengiriman data ke PMS memerlukan koneksi internet.
- Versi ini belum menerapkan mekanisme pengiriman backlog MicroSD secara berurutan.

## Dokumentasi Project

![Circuit GPS Tracker](docs/images/Circuit_GPS_Tracker.PNG)

![Dokumentasi GPS Tracker OLED](docs/images/Dokuemntasi_GPS_TrackerOLED.jpeg)

![Dokumentasi GPS Tracker OLED 2](docs/images/Dokuemntasi_GPS_TrackerOLED2.jpeg)
