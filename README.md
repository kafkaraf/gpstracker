# GPS Tracker JP19

Sistem **GPS Tracking berbasis IoT** yang dikembangkan untuk kapal **Jaya Perkasa 19 (JP19)**.

Sistem ini menggunakan **ESP32-S3** dan **modul GPS NEO-M8N** untuk mendapatkan posisi kapal dan mengirimkan data lokasi ke **PMS (Planned Maintenance System)** melalui API HTTPS.

## Deskripsi

Proyek ini merupakan perangkat GPS Tracker yang digunakan untuk memantau posisi kapal JP19.

ESP32-S3 membaca data navigasi dari modul GPS NEO-M8N melalui komunikasi UART. Data GPS kemudian diproses dan dikirimkan ke server PMS menggunakan koneksi Wi-Fi dan HTTPS.

Data yang dikirimkan meliputi:

* MMSI
* Latitude
* Longitude
* Kecepatan kapal (Speed)
* Arah kapal (Course)

## Alur Sistem

```text
NEO-M8N GPS
     │
     │ UART 9600 bps
     ▼
ESP32-S3
     │
     │ Wi-Fi
     ▼
Internet
     │
     │ HTTPS POST
     ▼
PMS Server
     │
     ▼
GPS Tracking / Monitoring Kapal
```

## Hardware

### 1. ESP32-S3

ESP32-S3 digunakan sebagai mikrokontroler utama yang bertugas untuk:

* Membaca data GPS
* Memproses data GPS
* Menghubungkan perangkat ke jaringan Wi-Fi
* Mengirim data GPS ke server PMS
* Menampilkan status komunikasi melalui Serial Monitor

### 2. NEO-M8N GPS

NEO-M8N digunakan untuk mendapatkan data posisi dan navigasi kapal.

Data GPS diterima oleh ESP32-S3 menggunakan komunikasi UART dengan baud rate **9600 bps**.

## Koneksi GPS

| NEO-M8N | ESP32-S3                     |
| ------- | ---------------------------- |
| TX      | GPIO 16 (RX)                 |
| RX      | GPIO 17 (TX)                 |
| GND     | GND                          |
| VCC     | Sesuai spesifikasi modul GPS |

### Konfigurasi UART

```text
Baud Rate : 9600
Format    : 8N1
Interface : UART
```

## Software

Firmware dikembangkan menggunakan **Arduino Framework untuk ESP32**.

### Library yang Digunakan

```cpp
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
```

### Fungsi Library

**TinyGPSPlus**
Digunakan untuk membaca dan memproses data NMEA dari GPS serta mengambil informasi latitude, longitude, speed, dan course.

**HardwareSerial**
Digunakan untuk komunikasi UART antara ESP32-S3 dengan modul NEO-M8N.

**WiFi**
Digunakan untuk menghubungkan ESP32-S3 ke jaringan Wi-Fi.

**WiFiClientSecure**
Digunakan untuk komunikasi HTTPS antara ESP32-S3 dengan server PMS.

**ArduinoHttpClient**
Digunakan untuk membuat dan mengirim HTTP/HTTPS request ke server.

## Data GPS

Sistem mengambil beberapa parameter utama dari GPS:

| Parameter | Keterangan                          |
| --------- | ----------------------------------- |
| MMSI      | Identitas kapal                     |
| Latitude  | Koordinat lintang kapal             |
| Longitude | Koordinat bujur kapal               |
| Speed     | Kecepatan kapal dalam knot          |
| Course    | Arah pergerakan kapal dalam derajat |

Nilai kecepatan dari GPS menggunakan satuan **knot**.

## Komunikasi API

Data GPS dikirimkan ke server PMS menggunakan metode HTTP:

```text
POST
```

melalui koneksi:

```text
HTTPS
```

### Endpoint API

```text
POST /api/update-user-point/{MMSI}
```

MMSI digunakan sebagai identitas perangkat/kapal pada endpoint API.

### Format Data

Data dikirimkan dalam format JSON.

Contoh:

```json
{
  "mmsi": "YOUR_MMSI",
  "latitude": -6.000000,
  "longitude": 106.000000,
  "speed": 10.5,
  "course": 180.0
}
```

## Autentikasi API

API menggunakan secret key yang dikirim melalui HTTP header:

```text
X-Secret-Key: <SECRET_KEY>
```

**Secret key tidak boleh ditulis di README atau repository yang dapat diakses publik.**

Jika repository digunakan untuk kebutuhan internal perusahaan, tetap disarankan untuk memisahkan credential dari source code.

## Konfigurasi Wi-Fi

ESP32-S3 akan terhubung ke jaringan Wi-Fi sebelum melakukan proses tracking.

Contoh konfigurasi:

```cpp
const char* ssid = "NAMA_WIFI";
const char* password = "PASSWORD_WIFI";
```

**Password Wi-Fi asli jangan di-upload ke GitHub.**

Gunakan placeholder pada source code yang dibagikan:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

## Pembatasan Pengiriman Data

Sistem memiliki **rate limit 2 detik** untuk pengiriman data ke API.

Artinya, perangkat tidak akan mengirimkan request API lebih dari satu kali dalam interval 2 detik.

```text
1 request / 2 detik
```

Hal ini digunakan untuk mencegah pengiriman request API secara berlebihan.

## Validasi Data GPS

Sistem melakukan pengecekan terhadap data GPS sebelum dikirimkan.

Data yang digunakan:

* Latitude
* Longitude
* Speed
* Course

Jika data speed atau course tidak valid, sistem menggunakan nilai:

```text
Speed  = 0.0
Course = 0.0
```

Sistem juga melakukan pengecekan apakah modul GPS mengirimkan data.

Jika tidak ada data GPS yang diterima, Serial Monitor akan memberikan peringatan:

```text
[Peringatan] Tidak ada data dari GPS. Periksa kabel TX/RX!
```

## Serial Monitor

Serial Monitor ESP32-S3 menggunakan baud rate:

```text
115200
```

Contoh output:

```text
Menghubungkan ke WiFi: YOUR_WIFI_SSID
WiFi Berhasil Terhubung!
IP Address: xxx.xxx.xxx.xxx
----------------------------------------
GPS Module Initializing...

Sending API:
{"mmsi":"YOUR_MMSI","latitude":-6.000000,"longitude":106.000000,"speed":10.5,"course":180.0}

Status: 200
Response: ...
----------------------------------------
```

## HTTPS

Firmware menggunakan:

```cpp
wifiClient.setInsecure();
```

Konfigurasi tersebut membuat ESP32 tidak melakukan verifikasi sertifikat SSL server.

Konfigurasi ini dapat mempermudah proses pengembangan dan pengujian koneksi HTTPS.

Namun, untuk penggunaan **production**, sebaiknya menggunakan validasi sertifikat SSL yang benar agar komunikasi lebih aman.

## Status Proyek

**Status: Development / Testing**

GPS Tracker telah dipasang untuk proses pengujian pada kapal JP19.

Selama tahap pengujian, perangkat akan dipantau untuk mengetahui:

* Stabilitas koneksi Wi-Fi
* Kestabilan pembacaan GPS
* Akurasi koordinat
* Keberhasilan pengiriman data API
* Response dari server PMS
* Stabilitas perangkat selama beroperasi

Hasil pengujian akan digunakan sebagai dasar evaluasi sebelum perangkat digunakan secara operasional.

## Pengembangan Selanjutnya

Beberapa pengembangan yang dapat dilakukan:

* Implementasi validasi sertifikat SSL
* Automatic Wi-Fi reconnect
* Mekanisme retry ketika API gagal
* Monitoring kualitas sinyal GPS
* GPS data buffering ketika internet terputus
* Watchdog system
* Monitoring status perangkat
* Proteksi dan manajemen power supply
* Logging data GPS
* Monitoring error API
* Integrasi dengan sensor kapal lainnya

## Informasi Proyek

| Informasi         | Detail                |
| ----------------- | --------------------- |
| Nama Proyek       | GPS Tracker JP19      |
| Kapal             | Jaya Perkasa 19       |
| Mikrokontroler    | ESP32-S3              |
| Modul GPS         | NEO-M8N               |
| Komunikasi GPS    | UART 9600 bps         |
| Komunikasi Server | Wi-Fi                 |
| Protokol          | HTTPS                 |
| Format Data       | JSON                  |
| Server            | PMS                   |
| Fungsi            | GPS Tracking Kapal    |
| Status            | Development / Testing |

## Catatan Keamanan

Jangan memasukkan informasi sensitif ke repository, seperti:

* Password Wi-Fi
* API Secret Key
* Password server
* Database credential
* Token autentikasi
* Private key

Gunakan placeholder untuk credential pada source code yang di-upload ke repository.

Contoh:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

dan:

```cpp
client.sendHeader("X-Secret-Key", "YOUR_SECRET_KEY");
```

## Dokumentasi Wiring

![Wiring Jadi](docs/images/Fritzing Wiring.jpg)
![Pengetesan JP19](docs/images/Pengetesan JP19.jpg)
![Fritzing Wiring](docs/images/Wiring Jadi.jpg)
![Penempatan GPS](docs/images/Penempatan GPS.jpg)
