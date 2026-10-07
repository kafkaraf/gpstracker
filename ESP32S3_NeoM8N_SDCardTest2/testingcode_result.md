# Testing Code Result - GPS Tracker NEO-M8N

## Informasi Singkat
- Penyimpanan GPS: Data GPS disimpan ke MicroSD setiap 5 detik.
- Backlog: Jika masih ada data di MicroSD yang belum terkirim, sistem mengirim 1 data setiap ±3 detik.
- Urutan Backlog: Data dikirim berdasarkan urutan data paling lama → data berikutnya → sampai backlog habis.
- Validasi Pengiriman: Data baru dianggap terkirim jika API memberikan response HTTP 2xx.
- Jika WiFi mati: Data GPS tetap disimpan ke MicroSD dan tidak hilang.
- Jika WiFi mati 5 hari: Data selama 5 hari akan menjadi backlog di MicroSD. Setelah WiFi kembali, sistem akan mengirim backlog tersebut bertahap, bukan langsung sekaligus.
- Setelah Backlog Habis: Sistem kembali mengirim data GPS terbaru secara realtime.
- Jika API gagal: Data yang gagal dikirim tidak dilewati dan akan dicoba kembali.
- Datetime: Waktu pada data menggunakan waktu GPS dari NEO-M8N, sehingga data backlog tetap membawa waktu saat data GPS direkam.

## Hasil Pengujian

Pengujian dilakukan pada GPS Tracker menggunakan ESP32-S3, NEO-M8N, MicroSD, WiFi, dan API PMS.

Alur sistem:
```text
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
```
Data GPS tetap disimpan ke MicroSD ketika koneksi internet tidak tersedia.
Ketika koneksi tersedia, data dapat dikirim ke API.

## Masalah Ditemukan

Data `datetime` pada MicroSD menggunakan waktu dari GPS NEO-M8N.

Contoh:
```text
MicroSD:
2026-10-07 07:15:32
```
Namun pada server ditemukan konfigurasi:
```text
'datetime' => Carbon::now(),

`Carbon::now()` menyebabkan server menggunakan waktu saat data diterima/diproses, bukan waktu saat data GPS direkam.
```
Akibatnya:
```text
MicroSD datetime != Website PMS datetime

Masalah ini terutama terjadi ketika data lama yang tersimpan di MicroSD baru dikirim ke server setelah koneksi internet tersedia.
```
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

- Masalah `datetime` bukan berasal dari ESP32. ESP32 sudah mengirimkan waktu GPS pada payload API.
- Masalah berada pada backend server yang mengganti `datetime` dengan `Carbon::now()`.
- Perbaikan perlu dilakukan pada API agar menggunakan `datetime` dari data GPS sehingga timestamp pada MicroSD dan website PMS tetap sinkron.
