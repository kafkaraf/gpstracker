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

bool sdReady = false;

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 5000;

void sendToAPI(long mmsi, float lat, float lon, float speed, float course);
void saveToSD(long mmsi, float lat, float lon, float speed, float course);

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

        uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);

        Serial.print("[SD] Kapasitas: ");
        Serial.print(cardSize);
        Serial.println(" MB");

        if (!SD_MMC.exists(LOG_FILE)) {
          File file = SD_MMC.open(LOG_FILE, FILE_WRITE);

          if (file) {
            file.close();
            Serial.println("[SD] File log dibuat: " + String(LOG_FILE));
          } else {
            Serial.println("[SD] Gagal membuat file log!");
          }
        } else {
          Serial.println("[SD] File log sudah tersedia.");
        }
      }
    }
  }

  Serial.print("\n[WiFi] Menghubungkan ke: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempt = 0;

  while (WiFi.status() != WL_CONNECTED && attempt < 40) {
    delay(500);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Berhasil Terhubung!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Gagal terhubung di awal, loop akan mencoba reconnect...");
  }

  wifiClient.setInsecure();
  client.setHttpTimeout(6000);

  GPS_Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  Serial.println("[GPS] Modul NEO-M8N siap membaca satelit...");
}

void loop() {
  static unsigned long lastWifiCheck = 0;

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiCheck > 5000) {
      lastWifiCheck = millis();
      Serial.println("[WiFi] Terputus! Mencoba reconnect...");
      WiFi.reconnect();
    }
  }

  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }

  if (millis() - lastSend >= SEND_INTERVAL) {
    if (gps.location.isValid() && gps.location.age() < 3000) {
      float lat = gps.location.lat();
      float lon = gps.location.lng();

      if (lat != 0.0 && lon != 0.0) {
        lastSend = millis();

        float speed = gps.speed.isValid()
                      ? gps.speed.knots()
                      : 0.0;

        float course = gps.course.isValid()
                       ? gps.course.deg()
                       : 0.0;

        saveToSD(
          mmsi_device,
          lat,
          lon,
          speed,
          course
        );

        if (WiFi.status() == WL_CONNECTED) {
          sendToAPI(
            mmsi_device,
            lat,
            lon,
            speed,
            course
          );
        } else {
          Serial.println("[API] WiFi tidak tersedia. Data tetap tersimpan di SD.");
        }
      }
    } else {
      static unsigned long lastWaitPrint = 0;

      if (millis() - lastWaitPrint > 5000) {
        lastWaitPrint = millis();

        int sats = gps.satellites.isValid()
                   ? gps.satellites.value()
                   : 0;

        Serial.printf(
          "[GPS] Menunggu Lock Satelit... (Satelit: %d)\n",
          sats
        );
      }
    }
  }

  static unsigned long lastCheckHardware = 0;

  if (millis() - lastCheckHardware > 10000) {
    lastCheckHardware = millis();

    if (gps.charsProcessed() < 10) {
      Serial.println(
        "[Peringatan] Tidak ada data masuk dari GPS. Periksa kabel TX/RX!"
      );
    }
  }
}

void saveToSD(
  long mmsi,
  float lat,
  float lon,
  float speed,
  float course
) {
  if (!sdReady) {
    Serial.println("[SD] MicroSD tidak tersedia. Data tidak disimpan.");
    return;
  }

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

  File file = SD_MMC.open(LOG_FILE, FILE_APPEND);

  if (!file) {
    Serial.println("[SD] Gagal membuka file log!");
    return;
  }

  file.println(json);
  file.close();

  Serial.println("[SD] Data berhasil disimpan:");
  Serial.println(json);
}

void sendToAPI(
  long mmsi,
  float lat,
  float lon,
  float speed,
  float course
) {
  String fullApiPath =
    String("/api/update-user-point/") +
    String(mmsi);

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

  Serial.println("\n[API] Mengirim Data Koordinat:");
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

  int statusCode = client.responseStatusCode();
  String response = client.responseBody();

  Serial.printf(
    "[API] Response Status: %d\n",
    statusCode
  );

  Serial.println(
    "[API] Response Body: " + response
  );

  Serial.println(
    "----------------------------------------"
  );
}
