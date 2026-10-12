  #include <Arduino.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ========================================================
// KONFIGURASI ESP32-S3 N16R8 CAM
// ========================================================

// Onboard MicroSD - SDMMC 1-bit
#define SD_CMD 38
#define SD_CLK 39
#define SD_D0  40

// OLED SSD1306 128x64
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C

// Sesuaikan dengan wiring OLED
#define OLED_SDA 1
#define OLED_SCL 2

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

bool oledReady = false;

// ========================================================
// KONFIGURASI WIFI
// ========================================================

const char* ssid = "...";
const char* password = "...";

// ========================================================
// KONFIGURASI GPS NEO-M8N
// ========================================================

const int RX_PIN = 16;
const int TX_PIN = 17;

HardwareSerial GPS_Serial(1);
TinyGPSPlus gps;

// ========================================================
// API PMS
// ========================================================

const char* server = "....";
const int port = ....;
const long mmsi_device = ....;

WiFiClientSecure wifiClient;
HttpClient client = HttpClient(wifiClient, server, port);

// ========================================================
// KONFIGURASI MICROSD
// ========================================================

const char* LOG_FILE = "/gps_log.jsonl";

bool sdReady = false;

// ========================================================
// TIMER
// ========================================================

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 5000;

unsigned long lastOledUpdate = 0;
const unsigned long OLED_UPDATE_INTERVAL = 1000;

unsigned long lastWifiCheck = 0;
unsigned long lastCheckHardware = 0;
unsigned long lastWaitPrint = 0;

// ========================================================
// PROTOTYPE FUNGSI
// ========================================================

void sendToAPI(
  long mmsi,
  float lat,
  float lon,
  float speed,
  float course
);

void saveToSD(
  long mmsi,
  float lat,
  float lon,
  float speed,
  float course
);

void drawWifiSignal(int x, int bottomY, int level);
void drawOledScreen();

// ========================================================
// MENGHITUNG KEKUATAN SINYAL WIFI
// ========================================================

int getWifiLevel() {
  if (WiFi.status() != WL_CONNECTED) {
    return 0;
  }

  int rssi = WiFi.RSSI();

  if (rssi >= -55) return 4;
  if (rssi >= -67) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;

  return 0;
}

// ========================================================
// MENGGAMBAR INDIKATOR WIFI
// Hanya batang aktif yang ditampilkan
// ========================================================

void drawWifiSignal(int x, int bottomY, int level) {
  level = constrain(level, 0, 4);

  const int barWidth = 4;
  const int gap = 2;
  const int heights[4] = {4, 6, 8, 10};

  for (int i = 0; i < level; i++) {
    int barX = x + i * (barWidth + gap);
    int barY = bottomY - heights[i];

    display.fillRoundRect(
      barX,
      barY,
      barWidth,
      heights[i],
      1,
      SSD1306_WHITE
    );
  }
}

// ========================================================
// TAMPILAN OLED
// ========================================================

void drawOledScreen() {
  if (!oledReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // ------------------------------------------------------
  // HEADER: WIFI DAN RSSI
  // ------------------------------------------------------

  drawWifiSignal(2, 14, getWifiLevel());

  display.setCursor(34, 5);

  if (WiFi.status() == WL_CONNECTED) {
    display.print("ONLINE");
  } else {
    display.print("OFFLINE");
  }

  display.setCursor(82, 5);

  if (WiFi.status() == WL_CONNECTED) {
    display.print(WiFi.RSSI());
    display.print("dBm");
  } else {
    display.print("--dBm");
  }

  display.drawLine(0, 17, 127, 17, SSD1306_WHITE);

  // ------------------------------------------------------
  // STATUS GPS DAN MICROSD
  // ------------------------------------------------------

  display.setCursor(2, 21);
  display.print("GPS:");

  if (gps.location.isValid()) {
    display.print("FIX");
  } else {
    display.print("WAIT");
  }

  display.setCursor(70, 21);
  display.print("SD:");

  if (sdReady) {
    display.print("READY");
  } else {
    display.print("ERROR");
  }

  // ------------------------------------------------------
  // LATITUDE
  // ------------------------------------------------------

  display.setCursor(2, 33);
  display.print("LAT:");

  if (gps.location.isValid()) {
    display.print(gps.location.lat(), 5);
  } else {
    display.print("--");
  }

  // ------------------------------------------------------
  // LONGITUDE
  // ------------------------------------------------------

  display.setCursor(2, 44);
  display.print("LON:");

  if (gps.location.isValid()) {
    display.print(gps.location.lng(), 5);
  } else {
    display.print("--");
  }

  // ------------------------------------------------------
  // SPEED DAN COURSE
  // ------------------------------------------------------

  display.setCursor(2, 55);
  display.print("SPD:");

  if (gps.speed.isValid()) {
    display.print(gps.speed.knots(), 1);
  } else {
    display.print("0.0");
  }

  display.print("kn");

  display.setCursor(70, 55);
  display.print("COG:");

  if (gps.course.isValid()) {
    display.print(gps.course.deg(), 0);
  } else {
    display.print("0");
  }

  display.display();
}

// ========================================================
// SETUP
// ========================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" Seven Oceans PMS - GPS Tracker ESP32 ");
  Serial.println(" GPS + WiFi + API + MicroSD + OLED");
  Serial.println("========================================");

  // ======================================================
  // 1. INISIALISASI OLED
  // ======================================================

  Serial.println("\n[OLED] Inisialisasi OLED...");

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      )) {
    Serial.println("[OLED] Gagal diinisialisasi!");
    oledReady = false;
  } else {
    oledReady = true;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.println("GPS TRACKER");
    display.setCursor(10, 35);
    display.println("Starting...");
    display.display();

    Serial.println("[OLED] Berhasil diinisialisasi!");
  }

  // ======================================================
  // 2. INISIALISASI MICROSD
  // ======================================================

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
          File file = SD_MMC.open(
            LOG_FILE,
            FILE_WRITE
          );

          if (file) {
            file.close();

            Serial.print("[SD] File log dibuat: ");
            Serial.println(LOG_FILE);
          } else {
            Serial.println("[SD] Gagal membuat file log!");
          }
        } else {
          Serial.println("[SD] File log sudah tersedia.");
        }
      }
    }
  }

  // ======================================================
  // 3. HUBUNGKAN WIFI
  // ======================================================

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
    Serial.println("\n[WiFi] Berhasil terhubung!");

    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(
      "\n[WiFi] Gagal terhubung di awal, "
      "loop akan mencoba reconnect..."
    );
  }

  // ======================================================
  // 4. KONFIGURASI SSL DAN HTTP
  // ======================================================

  wifiClient.setInsecure();
  client.setTimeout(6000);

  // ======================================================
  // 5. INISIALISASI GPS
  // ======================================================

  GPS_Serial.begin(
    9600,
    SERIAL_8N1,
    RX_PIN,
    TX_PIN
  );

  Serial.println(
    "[GPS] Modul NEO-M8N siap membaca satelit..."
  );

  drawOledScreen();
}

// ========================================================
// LOOP
// ========================================================

void loop() {
  // ======================================================
  // 1. BACA DATA GPS TERUS-MENERUS
  // ======================================================

  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }

  // ======================================================
  // 2. UPDATE OLED SETIAP 1 DETIK
  // ======================================================

  if (
    millis() - lastOledUpdate >=
    OLED_UPDATE_INTERVAL
  ) {
    lastOledUpdate = millis();
    drawOledScreen();
  }

  // ======================================================
  // 3. AUTO RECONNECT WIFI
  // ======================================================

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiCheck > 5000) {
      lastWifiCheck = millis();

      Serial.println(
        "[WiFi] Terputus! Mencoba reconnect..."
      );

      WiFi.reconnect();
    }
  }

  // ======================================================
  // 4. PROSES DATA GPS SETIAP 5 DETIK
  // ======================================================

  if (millis() - lastSend >= SEND_INTERVAL) {
    if (
      gps.location.isValid() &&
      gps.location.age() < 3000
    ) {
      float lat = gps.location.lat();
      float lon = gps.location.lng();

      if (lat != 0.0 && lon != 0.0) {
        lastSend = millis();

        float speed =
          gps.speed.isValid()
          ? gps.speed.knots()
          : 0.0;

        float course =
          gps.course.isValid()
          ? gps.course.deg()
          : 0.0;

        // Simpan data GPS ke MicroSD
        saveToSD(
          mmsi_device,
          lat,
          lon,
          speed,
          course
        );

        // Kirim data GPS ke PMS jika WiFi tersedia
        if (WiFi.status() == WL_CONNECTED) {
          sendToAPI(
            mmsi_device,
            lat,
            lon,
            speed,
            course
          );
        } else {
          Serial.println(
            "[API] WiFi tidak tersedia. "
            "Data tetap tersimpan di SD."
          );
        }
      }
    } else {
      if (millis() - lastWaitPrint > 5000) {
        lastWaitPrint = millis();

        int sats =
          gps.satellites.isValid()
          ? gps.satellites.value()
          : 0;

        Serial.printf(
          "[GPS] Menunggu Lock Satelit... "
          "(Satelit: %d)\n",
          sats
        );
      }
    }
  }

  // ======================================================
  // 5. DETEKSI GPS TIDAK MENGIRIM DATA
  // ======================================================

  if (millis() - lastCheckHardware > 10000) {
    lastCheckHardware = millis();

    if (gps.charsProcessed() < 10) {
      Serial.println(
        "[Peringatan] Tidak ada data masuk dari GPS. "
        "Periksa kabel TX/RX!"
      );
    }
  }
}

// ========================================================
// SIMPAN DATA GPS KE MICROSD
// ========================================================

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

  // Waktu UTC dari GPS
  if (
    gps.date.isValid() &&
    gps.time.isValid() &&
    gps.date.year() >= 2024
  ) {
    char dtBuf[25];

    snprintf(
      dtBuf,
      sizeof(dtBuf),
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

  File file = SD_MMC.open(
    LOG_FILE,
    FILE_APPEND
  );

  if (!file) {
    Serial.println("[SD] Gagal membuka file log!");
    return;
  }

  file.println(json);
  file.close();

  Serial.println("[SD] Data berhasil disimpan:");
  Serial.println(json);
}

// ========================================================
// KIRIM DATA GPS KE API PMS
// ========================================================

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

  // Waktu UTC dari GPS
  if (
    gps.date.isValid() &&
    gps.time.isValid() &&
    gps.date.year() >= 2024
  ) {
    char dtBuf[25];

    snprintf(
      dtBuf,
      sizeof(dtBuf),
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

  Serial.println("\n[API] Mengirim data koordinat:");
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
