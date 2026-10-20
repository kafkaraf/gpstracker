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
// KONFIGURASI BUZZER
// ========================================================
// Gunakan active buzzer yang kompatibel dengan logika 3.3V.
// Positif buzzer -> GPIO41, negatif buzzer -> GND.
#define BUZZER_PIN 41

void buzzerBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}

void buzzerTwoBeep() {
  buzzerBeep();
  delay(150);
  buzzerBeep();
}

// ========================================================
// KONFIGURASI WIFI
// ========================================================

const char* ssid = "...";
const char* password = "...";

// Pengecekan internet setiap 10 detik
bool previousInternetConnected = false;
unsigned long lastInternetCheck = 0;
const unsigned long INTERNET_CHECK_INTERVAL = 10000;

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

const char* server = "...";
const int port = ...;
const long mmsi_device = ...;

WiFiClientSecure wifiClient;
HttpClient client = HttpClient(wifiClient, server, port);

// ========================================================
// KONFIGURASI MICROSD
// ========================================================

const char* LOG_FILE = "/gps_log.jsonl";
const char* OFFSET_FILE = "/gps_offset.txt";
bool sdReady = false;
uint64_t sendOffset = 0;
unsigned long lastQueueRetry = 0;
const unsigned long QUEUE_RETRY_INTERVAL = 3000;

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

bool sendToAPI(String json);
bool processPendingData();
String createGPSJson(long mmsi, float lat, float lon, float speed, float course);
void saveToSD(long mmsi, float lat, float lon, float speed, float course);
void loadOffset();
void saveOffset(uint64_t offset);
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

  // Inisialisasi buzzer; tidak berbunyi saat boot.
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

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
    display.println("by SOTS dirutjoss!");
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

  // Muat posisi antrean setelah MicroSD berhasil diinisialisasi.
  if (sdReady) {
    loadOffset();
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
  // 3. DETEKSI INTERNET + AUTO RECONNECT WIFI
  // ======================================================

  // Cek internet setiap 10 detik. Wi-Fi tersambung belum
  // tentu berarti internet tersedia.
  if (millis() - lastInternetCheck >= INTERNET_CHECK_INTERVAL) {
    lastInternetCheck = millis();

    bool internetConnected = false;

    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient internetTestClient;
      internetTestClient.setTimeout(2000);
      internetConnected = internetTestClient.connect("www.google.com", 80);
      internetTestClient.stop();
    }

    // Internet baru tersambung: 1 beep
    if (internetConnected && !previousInternetConnected) {
      Serial.println("[Internet] Terhubung. Buzzer 1 beep.");
      buzzerBeep();
    }

    // Internet baru terputus: 2 beep
    if (!internetConnected && previousInternetConnected) {
      Serial.println("[Internet] Terputus. Buzzer 2 beep.");
      buzzerTwoBeep();
    }

    previousInternetConnected = internetConnected;
  }

  // Reconnect Wi-Fi jika koneksi ke router terputus
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

        // Simpan titik baru terlebih dahulu ke JSONL.
        // Pengiriman dilakukan antrean agar rekaman tertua dikirim lebih dulu.
        saveToSD(mmsi_device, lat, lon, speed, course);
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
  // 5. KIRIM ANTREAN JSONL KE PMS
  // ======================================================
  // Proses satu rekaman tiap interval. Offset maju hanya setelah HTTP 2xx.
  if (WiFi.status() == WL_CONNECTED && sdReady) {
    if (millis() - lastQueueRetry >= QUEUE_RETRY_INTERVAL) {
      lastQueueRetry = millis();
      processPendingData();
    }
  }

  // ======================================================
  // 6. DETEKSI GPS TIDAK MENGIRIM DATA
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
// MEMBUAT JSON GPS
// ========================================================

String createGPSJson(
  long mmsi,
  float lat,
  float lon,
  float speed,
  float course
) {
  String json = "{";
  json += "\"mmsi\":\"" + String(mmsi) + "\",";
  json += "\"latitude\":" + String(lat, 6) + ",";
  json += "\"longitude\":" + String(lon, 6) + ",";
  json += "\"speed\":" + String(speed, 2) + ",";
  json += "\"course\":" + String(course, 2);

  // Simpan waktu UTC GPS saat rekaman dibuat.
  if (gps.date.isValid() && gps.time.isValid() && gps.date.year() >= 2024) {
    char dtBuf[25];
    snprintf(dtBuf, sizeof(dtBuf), "%04d-%02d-%02d %02d:%02d:%02d",
      gps.date.year(), gps.date.month(), gps.date.day(),
      gps.time.hour(), gps.time.minute(), gps.time.second());
    json += ",\"datetime\":\"";
    json += String(dtBuf);
    json += "\"";
  }
  json += "}";
  return json;
}

// ========================================================
// SIMPAN GPS KE MICROSD SEBAGAI JSONL
// ========================================================

void saveToSD(long mmsi, float lat, float lon, float speed, float course) {
  if (!sdReady) {
    Serial.println("[SD] MicroSD tidak tersedia. Data tidak disimpan.");
    return;
  }

  String json = createGPSJson(mmsi, lat, lon, speed, course);
  File file = SD_MMC.open(LOG_FILE, FILE_APPEND);
  if (!file) {
    Serial.println("[SD] Gagal membuka file log!");
    return;
  }

  size_t written = file.println(json);
  file.flush();
  file.close();
  if (written == 0) {
    Serial.println("[SD] Penulisan JSONL gagal!");
    return;
  }
  Serial.println("\n[SD] Data GPS tersimpan di JSONL:");
  Serial.println(json);
}

// ========================================================
// KIRIM SATU REKAMAN PENDING DARI JSONL
// ========================================================

bool processPendingData() {
  if (!sdReady) return false;

  File file = SD_MMC.open(LOG_FILE, FILE_READ);
  if (!file) {
    Serial.println("[QUEUE] Gagal membuka file log.");
    return false;
  }

  uint64_t fileSize = file.size();
  if (sendOffset >= fileSize) {
    file.close();
    return false; // tidak ada rekaman tertunda
  }

  if (!file.seek(sendOffset)) {
    Serial.println("[QUEUE] Gagal seek ke offset.");
    file.close();
    return false;
  }

  String json = file.readStringUntil('\n');
  uint64_t nextOffset = file.position();
  file.close();
  json.trim();

  if (json.length() == 0) {
    sendOffset = nextOffset;
    saveOffset(sendOffset);
    return true;
  }

  Serial.println("\n[QUEUE] Mengirim rekaman JSONL tertua yang belum terkirim:");
  Serial.println(json);

  if (sendToAPI(json)) {
    sendOffset = nextOffset;
    saveOffset(sendOffset);
    Serial.println("[QUEUE] Data berhasil dikirim; offset dimajukan.");
    Serial.print("[QUEUE] Offset berikutnya: ");
    Serial.println((unsigned long)sendOffset);
    return true;
  }

  // Bila API gagal, offset tetap sehingga baris ini dicoba lagi.
  Serial.println("[QUEUE] Gagal mengirim data. Akan dicoba kembali.");
  return false;
}

// ========================================================
// KIRIM JSON KE API PMS
// ========================================================

bool sendToAPI(String json) {
  String fullApiPath = String("/api/update-user-point/") + String(mmsi_device);

  Serial.println("\n[API] Mengirim JSON:");
  Serial.println(json);

  client.beginRequest();
  client.post(fullApiPath);
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("X-Secret-Key", "...");
  client.sendHeader("Content-Length", json.length());
  client.beginBody();
  client.print(json);
  client.endRequest();

  int statusCode = client.responseStatusCode();
  String response = client.responseBody();
  Serial.printf("[API] Response Status: %d\n", statusCode);
  Serial.println("[API] Response Body: " + response);
  Serial.println("----------------------------------------");

  if (statusCode >= 200 && statusCode < 300) {
    Serial.println("[BUZZER] API sukses (HTTP 2xx). Beep 1 kali.");
    buzzerBeep();
    return true;
  }

  Serial.println("[API] Gagal. Rekaman tetap dalam antrean.");
  return false;
}

// ========================================================
// LOAD OFFSET DARI MICROSD
// ========================================================

void loadOffset() {
  if (!sdReady) return;

  if (!SD_MMC.exists(OFFSET_FILE)) {
    sendOffset = 0;
    saveOffset(sendOffset);
    Serial.println("[QUEUE] File offset dibuat dengan nilai 0.");
    return;
  }

  File file = SD_MMC.open(OFFSET_FILE, FILE_READ);
  if (!file) {
    sendOffset = 0;
    Serial.println("[QUEUE] Gagal membaca offset; mulai dari 0.");
    return;
  }

  String value = file.readStringUntil('\n');
  file.close();
  value.trim();
  sendOffset = value.length() == 0 ? 0 : strtoull(value.c_str(), nullptr, 10);

  File logFile = SD_MMC.open(LOG_FILE, FILE_READ);
  if (logFile) {
    if (sendOffset > logFile.size()) {
      Serial.println("[QUEUE] Offset melebihi ukuran log; reset ke 0.");
      sendOffset = 0;
      saveOffset(sendOffset);
    }
    logFile.close();
  }

  Serial.print("[QUEUE] Offset terakhir: ");
  Serial.println((unsigned long)sendOffset);
}

// ========================================================
// SIMPAN OFFSET KE MICROSD
// ========================================================

void saveOffset(uint64_t offset) {
  if (!sdReady) return;

  File file = SD_MMC.open(OFFSET_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("[QUEUE] Gagal menyimpan offset!");
    return;
  }

  file.print((unsigned long long)offset);
  file.print('\n');
  file.flush();
  file.close();
}
