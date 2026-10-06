#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include <SD_MMC.h>

#define SD_CMD  38
#define SD_CLK  39
#define SD_D0   40

const char* ssid     = "...";
const char* password = "...";

const int RX_PIN = 16;
const int TX_PIN = 17;

HardwareSerial GPS_Serial(1);
TinyGPSPlus gps;

const char* server = "...";
const int port = ...;
const long mmsi_device = ...;

WiFiClientSecure wifiClient;
HttpClient client = HttpClient(wifiClient, server, port);

const char* LOG_FILE = "/gps_log.jsonl";
const char* OFFSET_FILE = "/gps_offset.txt";

bool sdReady = false;

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 5000;

unsigned long lastWifiCheck = 0;
unsigned long lastQueueRetry = 0;

const unsigned long WIFI_CHECK_INTERVAL = 5000;
const unsigned long QUEUE_RETRY_INTERVAL = 3000;

uint64_t sendOffset = 0;

void saveToSD(
  long mmsi,
  float lat,
  float lon,
  float speed,
  float course
);

bool sendToAPI(String json);

bool processPendingData();

String createGPSJson(
  long mmsi,
  float lat,
  float lon,
  float speed,
  float course
);

void loadOffset();

void saveOffset(uint64_t offset);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println(" Seven Oceans PMS - GPS Tracker ESP32 ");
  Serial.println(" GPS + WiFi + API + MicroSD Logger  ");
  Serial.println("========================================");

  Serial.println("\n[SD] Inisialisasi MicroSD...");

  if (!SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0)) {
    Serial.println("[SD] Gagal mengatur pin SDMMC!");
    sdReady = false;
  } else {

    if (!SD_MMC.begin("/sdcard", true)) {
      Serial.println("[SD] MicroSD gagal diinisialisasi!");
      sdReady = false;
    } else {

      uint8_t cardType = SD_MMC.cardType();

      if (cardType == CARD_NONE) {
        Serial.println("[SD] Tidak ada MicroSD!");
        sdReady = false;
      } else {

        sdReady = true;

        Serial.println("[SD] MicroSD berhasil terdeteksi!");

        if (cardType == CARD_MMC) {
          Serial.println("[SD] Tipe: MMC");
        } else if (cardType == CARD_SD) {
          Serial.println("[SD] Tipe: SDSC");
        } else if (cardType == CARD_SDHC) {
          Serial.println("[SD] Tipe: SDHC");
        } else {
          Serial.println("[SD] Tipe: UNKNOWN");
        }

        uint64_t cardSize =
          SD_MMC.cardSize() / (1024 * 1024);

        Serial.print("[SD] Kapasitas: ");
        Serial.print(cardSize);
        Serial.println(" MB");

        if (!SD_MMC.exists(LOG_FILE)) {

          File file = SD_MMC.open(LOG_FILE, FILE_WRITE);

          if (file) {
            file.close();
            Serial.println(
              "[SD] File log dibuat: " +
              String(LOG_FILE)
            );
          } else {
            Serial.println(
              "[SD] Gagal membuat file log!"
            );
          }

        } else {

          Serial.println(
            "[SD] File log sudah tersedia."
          );
        }

        loadOffset();
      }
    }
  }

  Serial.print("\n[WiFi] Menghubungkan ke: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempt = 0;

  while (
    WiFi.status() != WL_CONNECTED &&
    attempt < 40
  ) {
    delay(500);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println(
      "\n[WiFi] Berhasil Terhubung!"
    );

    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println(
      "\n[WiFi] Gagal terhubung di awal."
    );

    Serial.println(
      "[WiFi] Loop akan mencoba reconnect."
    );
  }

  wifiClient.setInsecure();
  client.setHttpTimeout(6000);

  GPS_Serial.begin(
    9600,
    SERIAL_8N1,
    RX_PIN,
    TX_PIN
  );

  Serial.println(
    "[GPS] Modul NEO-M8N siap membaca satelit..."
  );
}


void loop() {

  /*
   * ========================================
   * 1. CEK WIFI
   * ========================================
   */

  if (WiFi.status() != WL_CONNECTED) {

    if (
      millis() - lastWifiCheck >=
      WIFI_CHECK_INTERVAL
    ) {

      lastWifiCheck = millis();

      Serial.println(
        "[WiFi] Terputus! Mencoba reconnect..."
      );

      WiFi.reconnect();
    }
  }


  /*
   * ========================================
   * 2. BACA GPS
   * ========================================
   */

  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }


  /*
   * ========================================
   * 3. SIMPAN GPS SETIAP 5 DETIK
   * ========================================
   */

  if (
    millis() - lastSend >=
    SEND_INTERVAL
  ) {

    if (
      gps.location.isValid() &&
      gps.location.age() < 3000
    ) {

      float lat = gps.location.lat();
      float lon = gps.location.lng();

      if (
        lat != 0.0 &&
        lon != 0.0
      ) {

        lastSend = millis();

        float speed =
          gps.speed.isValid()
          ? gps.speed.knots()
          : 0.0;

        float course =
          gps.course.isValid()
          ? gps.course.deg()
          : 0.0;

        saveToSD(
          mmsi_device,
          lat,
          lon,
          speed,
          course
        );
      }

    } else {

      static unsigned long lastWaitPrint = 0;

      if (
        millis() - lastWaitPrint >=
        5000
      ) {

        lastWaitPrint = millis();

        int sats =
          gps.satellites.isValid()
          ? gps.satellites.value()
          : 0;

        Serial.printf(
          "[GPS] Menunggu Lock Satelit... (Satelit: %d)\n",
          sats
        );
      }
    }
  }


  /*
   * ========================================
   * 4. KIRIM QUEUE DARI SD KE PMS
   * ========================================
   *
   * Data dikirim satu per satu.
   *
   * Kalau gagal:
   * - offset tidak berubah
   * - data akan dicoba lagi
   *
   * Kalau berhasil:
   * - offset maju
   * - lanjut data berikutnya
   */

  if (
    WiFi.status() == WL_CONNECTED &&
    sdReady
  ) {

    if (
      millis() - lastQueueRetry >=
      QUEUE_RETRY_INTERVAL
    ) {

      lastQueueRetry = millis();

      processPendingData();
    }
  }


  /*
   * ========================================
   * 5. CEK GPS HARDWARE
   * ========================================
   */

  static unsigned long lastCheckHardware = 0;

  if (
    millis() - lastCheckHardware >=
    10000
  ) {

    lastCheckHardware = millis();

    if (gps.charsProcessed() < 10) {

      Serial.println(
        "[Peringatan] Tidak ada data masuk dari GPS. "
        "Periksa kabel TX/RX!"
      );
    }
  }
}


/*
 * ========================================
 * BUAT JSON GPS
 * ========================================
 */

String createGPSJson(
  long mmsi,
  float lat,
  float lon,
  float speed,
  float course
) {

  String json = "{";

  json += "\"mmsi\":\"";
  json += String(mmsi);
  json += "\",";

  json += "\"latitude\":";
  json += String(lat, 6);
  json += ",";

  json += "\"longitude\":";
  json += String(lon, 6);
  json += ",";

  json += "\"speed\":";
  json += String(speed, 2);
  json += ",";

  json += "\"course\":";
  json += String(course, 2);


  if (
    gps.date.isValid() &&
    gps.time.isValid() &&
    gps.date.year() >= 2024
  ) {

    char dtBuf[25];

    sprintf(
      dtBuf,
      "%04d-%02d-%02d %02d:%02d:%02d",
      gps.date.year(),
      gps.date.month(),
      gps.date.day(),
      gps.time.hour(),
      gps.time.minute(),
      gps.time.second()
    );

    json += ",\"datetime\":\"";
    json += String(dtBuf);
    json += "\"";
  }

  json += "}";

  return json;
}


/*
 * ========================================
 * SIMPAN GPS KE MICROSD
 * ========================================
 */

void saveToSD(
  long mmsi,
  float lat,
  float lon,
  float speed,
  float course
) {

  if (!sdReady) {

    Serial.println(
      "[SD] MicroSD tidak tersedia. "
      "Data tidak disimpan."
    );

    return;
  }

  String json = createGPSJson(
    mmsi,
    lat,
    lon,
    speed,
    course
  );

  File file =
    SD_MMC.open(
      LOG_FILE,
      FILE_APPEND
    );

  if (!file) {

    Serial.println(
      "[SD] Gagal membuka file log!"
    );

    return;
  }

  file.println(json);
  file.close();

  Serial.println(
    "\n[SD] Data GPS tersimpan:"
  );

  Serial.println(json);
}


/*
 * ========================================
 * PROSES DATA PENDING
 * ========================================
 *
 * Membaca satu data yang belum terkirim.
 *
 * sendOffset menunjukkan posisi byte
 * terakhir yang sudah berhasil dikirim.
 */

bool processPendingData() {

  if (!sdReady) {
    return false;
  }

  File file =
    SD_MMC.open(
      LOG_FILE,
      FILE_READ
    );

  if (!file) {

    Serial.println(
      "[QUEUE] Gagal membuka file log."
    );

    return false;
  }


  uint64_t fileSize = file.size();

  /*
   * Tidak ada data baru yang belum dikirim.
   */

  if (sendOffset >= fileSize) {

    file.close();

    return false;
  }


  /*
   * Pindahkan posisi file ke data
   * yang belum terkirim.
   */

  if (!file.seek(sendOffset)) {

    Serial.println(
      "[QUEUE] Gagal seek ke offset."
    );

    file.close();

    return false;
  }


  String json =
    file.readStringUntil('\n');

  uint64_t nextOffset =
    file.position();

  file.close();


  json.trim();

  if (json.length() == 0) {

    sendOffset = nextOffset;
    saveOffset(sendOffset);

    return true;
  }


  Serial.println(
    "\n[QUEUE] Mengirim data lama:"
  );

  Serial.println(json);


  /*
   * Kirim ke PMS.
   */

  bool success =
    sendToAPI(json);


  if (success) {

    /*
     * Hanya setelah PMS memberikan
     * response sukses, offset dimajukan.
     */

    sendOffset = nextOffset;

    saveOffset(sendOffset);

    Serial.println(
      "[QUEUE] Data berhasil dikirim."
    );

    Serial.print(
      "[QUEUE] Offset berikutnya: "
    );

    Serial.println(
      (unsigned long)sendOffset
    );

    return true;

  } else {

    Serial.println(
      "[QUEUE] Gagal mengirim data."
    );

    Serial.println(
      "[QUEUE] Data akan dicoba lagi."
    );

    return false;
  }
}


/*
 * ========================================
 * KIRIM JSON KE PMS
 * ========================================
 */

bool sendToAPI(String json) {

  String fullApiPath =
    String("/api/update-user-point/") +
    String(mmsi_device);


  Serial.println(
    "\n[API] Mengirim JSON:"
  );

  Serial.println(json);


  client.beginRequest();

  client.post(fullApiPath);

  client.sendHeader(
    "Content-Type",
    "application/json"
  );

  client.sendHeader(
    "X-Secret-Key",
    "..."
  );

  client.sendHeader(
    "Content-Length",
    json.length()
  );

  client.beginBody();

  client.print(json);

  client.endRequest();


  int statusCode =
    client.responseStatusCode();

  String response =
    client.responseBody();


  Serial.printf(
    "[API] Response Status: %d\n",
    statusCode
  );

  Serial.println(
    "[API] Response Body: " +
    response
  );

  Serial.println(
    "----------------------------------------"
  );


  /*
   * HTTP 200-299 dianggap berhasil.
   */

  if (
    statusCode >= 200 &&
    statusCode < 300
  ) {

    return true;
  }


  return false;
}


/*
 * ========================================
 * LOAD OFFSET DARI MICROSD
 * ========================================
 */

void loadOffset() {

  if (!sdReady) {
    return;
  }


  if (
    !SD_MMC.exists(OFFSET_FILE)
  ) {

    sendOffset = 0;

    saveOffset(sendOffset);

    Serial.println(
      "[QUEUE] Offset baru dibuat: 0"
    );

    return;
  }


  File file =
    SD_MMC.open(
      OFFSET_FILE,
      FILE_READ
    );

  if (!file) {

    sendOffset = 0;

    Serial.println(
      "[QUEUE] Gagal membaca offset. "
      "Menggunakan offset 0."
    );

    return;
  }


  String value =
    file.readStringUntil('\n');

  file.close();

  value.trim();

  if (value.length() == 0) {

    sendOffset = 0;

  } else {

    sendOffset =
      strtoull(
        value.c_str(),
        NULL,
        10
      );
  }


  Serial.print(
    "[QUEUE] Offset terakhir: "
  );

  Serial.println(
    (unsigned long)sendOffset
  );
}


/*
 * ========================================
 * SIMPAN OFFSET KE MICROSD
 * ========================================
 */

void saveOffset(
  uint64_t offset
) {

  if (!sdReady) {
    return;
  }


  File file =
    SD_MMC.open(
      OFFSET_FILE,
      FILE_WRITE
    );

  if (!file) {

    Serial.println(
      "[QUEUE] Gagal menyimpan offset!"
    );

    return;
  }


  file.print(
    (unsigned long long)offset
  );

  file.close();
}
