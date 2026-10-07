# Testing Code Result - GPS Tracker NEO-M8N

## Hasil Pengujian

Pengujian dilakukan pada GPS Tracker menggunakan ESP32-S3, NEO-M8N, MicroSD, WiFi, dan API PMS.

Alur sistem:

GPS NEO-M8N
     |
     v
ESP32-S3
     |
     +----> Simpan ke MicroSD (JSONL)
     |
     +----> Jika WiFi tersedia
                 |
                 v
                API
                 |
                 v
            Website PMS

Data GPS tetap disimpan ke MicroSD ketika koneksi internet tidak tersedia.
Ketika koneksi tersedia, data dapat dikirim ke API.

## Masalah Ditemukan

Data `datetime` pada MicroSD menggunakan waktu dari GPS NEO-M8N.

Contoh:

MicroSD:
2026-10-07 07:15:32

Namun pada server ditemukan konfigurasi:

'datetime' => Carbon::now(),

`Carbon::now()` menyebabkan server menggunakan waktu saat data diterima/diproses, bukan waktu saat data GPS direkam.

Akibatnya:

MicroSD datetime != Website PMS datetime

Masalah ini terutama terjadi ketika data lama yang tersimpan di MicroSD baru dikirim ke server setelah koneksi internet tersedia.

## Perbaikan

ESP32 sudah mengirimkan `datetime` GPS dalam JSON payload.

Server seharusnya menggunakan nilai `datetime` dari request:

'datetime' => $request->datetime,

Atau menggunakan fallback:

'datetime' => $request->datetime ?? Carbon::now(),

Dengan demikian:

GPS Datetime
     |
     +----> MicroSD
     |
     +----> API
              |
              v
        Website PMS

Datetime tetap sinkron.

## Status Pengujian

GPS Reading      : PASS
MicroSD Logging  : PASS
JSONL Logging    : PASS
WiFi             : PASS
API Transmission : PASS
Website Display  : PASS
Datetime Sync    : ISSUE FOUND
Server Fix       : REQUIRED

## Kesimpulan

Masalah `datetime` bukan berasal dari ESP32. ESP32 sudah mengirimkan waktu GPS pada payload API.

Masalah berada pada backend server yang mengganti `datetime` dengan `Carbon::now()`.

Perbaikan perlu dilakukan pada API agar menggunakan `datetime` dari data GPS sehingga timestamp pada MicroSD dan website PMS tetap sinkron.
