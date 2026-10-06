## Flow Pengiriman Data GPS (Untested)
- Ini merupakan kode program lanjutan dari ESP32S3_NeoM8N_SDCardTest.ino tetapi versi yang diperbaiki dengan menambahkan sistem antrian pada pengiriman ke servernya
- Sistem memiliki 2 mode pengiriman data, yaitu MODE BACKLOG dan MODE REALTIME.
- Mode yang digunakan ditentukan berdasarkan apakah masih terdapat data GPS yang belum berhasil dikirim ke Server.

### MODE 1 — BACKLOG
- Mode BACKLOG digunakan ketika MicroSD Card masih memiliki data GPS yang belum terkirim ke Server.
- Data dikirim berdasarkan urutan waktu, yaitu data paling lama dikirim terlebih dahulu (FIFO).

    SD
     ↓
    Data paling lama
     ↓
    Server
     ↓
    Response sukses
     ↓
    Data berikutnya
     ↓
    Server
     ↓
    ...
     ↓
    SD kosong

- Proses backlog terus berjalan sampai seluruh data yang tersimpan di SD berhasil dikirim dan SD menjadi kosong.
- Jika pengiriman suatu data gagal, data tersebut tetap berada di SD dan tidak dilanjutkan ke data berikutnya.


### MODE 2 — REALTIME
- Mode REALTIME digunakan ketika MicroSD Card sudah tidak memiliki data backlog.
- GPS terbaru disimpan ke SD terlebih dahulu, kemudian dikirim ke Server.

    GPS terbaru
         ↓
        SD
         ↓
        Server

- Penyimpanan ke SD dilakukan terlebih dahulu agar data tetap memiliki salinan lokal apabila koneksi WiFi atau komunikasi ke Server mengalami gangguan.


## Contoh Flow Lengkap

### Kondisi Normal

- Ketika WiFi dan koneksi ke Server tersedia, data GPS dapat langsung dikirim:

    10:00:00 → GPS A → Server ✓
    10:00:05 → GPS B → Server ✓


### WiFi Terputus

- Ketika WiFi atau koneksi ke Server terputus, data GPS tidak dapat dikirim ke Server.
- Data kemudian disimpan ke MicroSD sebagai backlog:

    10:00:10 → GPS C → SD
    10:00:15 → GPS D → SD
    10:00:20 → GPS E → SD
    10:00:25 → GPS F → SD

Data C, D, E, dan F tetap tersimpan di SD sampai koneksi kembali tersedia.


### WiFi Kembali

- Ketika koneksi kembali tersedia, sistem terlebih dahulu mengecek apakah masih terdapat data backlog di SD.

    10:00:30
         ↓
       Cek SD
         ↓
    Ada C, D, E, F
         ↓
       C → Server ✓
         ↓
       D → Server ✓
         ↓
       E → Server ✓
         ↓
       F → Server ✓
         ↓
      SD KOSONG
         ↓
    MODE REALTIME

- Data backlog dikirim berdasarkan urutan data yang paling lama terlebih dahulu.


### Kembali ke Mode Realtime

- Setelah seluruh backlog berhasil dikirim dan SD kosong, sistem kembali menggunakan mode realtime:

    10:00:30 → GPS G → SD → Server ✓
    10:00:35 → GPS H → SD → Server ✓
    10:00:40 → GPS I → SD → Server ✓


## Ringkasan Logika Sistem

```text
                  GPS TERBARU
                       ↓
                      SD
                       ↓
                Cek data backlog
                       ↓
              ┌────────┴────────┐
              ↓                 ↓
         Ada backlog         SD kosong
              ↓                 ↓
        MODE BACKLOG       MODE REALTIME
              ↓                 ↓
       Data paling lama      GPS terbaru
              ↓                 ↓
             Server               Server
              ↓
       Response sukses?
          ↓          ↓
         Ya        Tidak
          ↓          ↓
    Data berikutnya  Tetap di SD
          ↓
       SD kosong?
        ↓      ↓
       Tidak   Ya
        ↓      ↓
       Server   REALTIME
```

## Prinsip Utama

1. Data GPS disimpan ke SD terlebih dahulu sebelum dikirim ke Server.
2. Jika masih terdapat backlog, backlog harus diselesaikan terlebih dahulu.
3. Pengiriman backlog menggunakan prinsip FIFO (First In, First Out).
4. Data hanya dianggap berhasil terkirim apabila Server memberikan response sukses.
5. Jika pengiriman gagal, data tetap disimpan di SD untuk dikirim kembali.
6. Setelah seluruh backlog berhasil dikirim dan SD kosong, sistem kembali ke MODE REALTIME.
