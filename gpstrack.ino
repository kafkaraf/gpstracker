#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>

// ================== KONFIGURASI WIFI ==================
const char* ssid     = "...";
const char* password = "...";

// ================== KONFIGURASI GPS (NEO-M8N) ==================
const int RX_PIN = 16; // ESP32 RX terhubung ke TX GPS
const int TX_PIN = 17; // ESP32 TX terhubung ke RX GPS
HardwareSerial GPS_Serial(1);
TinyGPSPlus gps;

// ================== API CONFIG (PMS) ==================
const char* server = "...";
const int port     = ...;
const long mmsi_device = ...; // Nomor MMSI kapal

WiFiClientSecure wifiClient;
HttpClient client = HttpClient(wifiClient, server, port);

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 5000; // Kirim data setiap 5 detik

// ================== PROTOTYPE FUNGSI ==================
void sendToAPI(long mmsi, float lat, float lon, float speed, float course);

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println("  Seven Oceans PMS - GPS Tracker ESP32  ");
  Serial.println("========================================");

  // 1. Hubungkan ke WiFi
  Serial.print("Menghubungkan ke WiFi: ");
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

  // 2. Set SSL Insecure (mengabaikan validasi cert HTTPS agar ringan di ESP32)
  wifiClient.setInsecure();
  client.setHttpTimeout(6000); // Timeout 6 detik

  // 3. Inisialisasi Serial GPS
  GPS_Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("[GPS] Modul NEO-M8N siap membaca satelit...");
}

// ================== LOOP UTAMA ==================
void loop() {
  // 1. Auto-Reconnect WiFi jika sinyal sempat terputus (Non-blocking)
  static unsigned long lastWifiCheck = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiCheck > 5000) {
      lastWifiCheck = millis();
      Serial.println("[WiFi] Terputus! Mencoba reconnect...");
      WiFi.reconnect();
    }
    return; // Tunggu WiFi pulih sebelum proses pengiriman
  }

  // 2. Baca aliran data serial dari GPS NEO-M8N
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }

  // 3. Kirim data setiap 5 detik HANYA jika sinyal GPS VALID (Sudah Lock Satelit)
  if (millis() - lastSend >= SEND_INTERVAL) {
    // Pastikan koordinat valid dan data masih segar (usia data < 3 detik)
    if (gps.location.isValid() && gps.location.age() < 3000) {
      float lat = gps.location.lat();
      float lon = gps.location.lng();

      // Hindari koordinat 0.0 jika GPS baru lepas lock
      if (lat != 0.0 && lon != 0.0) {
        lastSend = millis();

        float speed = gps.speed.isValid() ? gps.speed.knots() : 0.0;
        float course = gps.course.isValid() ? gps.course.deg() : 0.0;

        sendToAPI(mmsi_device, lat, lon, speed, course);
      }
    } else {
      // Debug info jika belum lock satelit
      static unsigned long lastWaitPrint = 0;
      if (millis() - lastWaitPrint > 5000) {
        lastWaitPrint = millis();
        int sats = gps.satellites.isValid() ? gps.satellites.value() : 0;
        Serial.printf("[GPS] Menunggu Lock Satelit... (Satelit terdeteksi: %d)\n", sats);
      }
    }
  }

  // 4. Deteksi jika kabel TX/RX GPS putus / tidak ada karakter masuk
  static unsigned long lastCheckHardware = 0;
  if (millis() - lastCheckHardware > 10000) {
    lastCheckHardware = millis();
    if (gps.charsProcessed() < 10) {
      Serial.println("[Peringatan] Tidak ada data masuk dari GPS. Periksa kabel TX/RX!");
    }
  }
}

// ================== PENGIRIMAN DATA KE SERVER ==================
void sendToAPI(long mmsi, float lat, float lon, float speed, float course) {
  String fullApiPath = String("/api/update-user-point/") + String(mmsi);

  // Buat JSON Payload dengan presisi desimal 6 digit
  String json = "{";
  json += "\"mmsi\":\"" + String(mmsi) + "\",";
  json += "\"latitude\":" + String(lat, 6) + ",";
  json += "\"longitude\":" + String(lon, 6) + ",";
  json += "\"speed\":" + String(speed, 2) + ",";
  json += "\"course\":" + String(course, 2);

  // Kirim waktu UTC asli dari satelit GPS jika valid
  if (gps.date.isValid() && gps.time.isValid() && gps.date.year() >= 2024) {
    char dtBuf[25];
    sprintf(dtBuf, "%04d-%02d-%02d %02d:%02d:%02d",
            gps.date.year(), gps.date.month(), gps.date.day(),
            gps.time.hour(), gps.time.minute(), gps.time.second());
    json += ",\"datetime\":\"" + String(dtBuf) + "\"";
  }

  json += "}";

  Serial.println("\n[API] Mengirim Data Koordinat:");
  Serial.println(json);

  // Kirim HTTP POST Request ke Laravel PMS
  client.beginRequest();
  client.post(fullApiPath);
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("X-Secret-Key", "...");
  client.sendHeader("Content-Length", json.length());
  client.beginBody();
  client.print(json);
  client.endRequest();

  // Baca Status Response Server
  int statusCode = client.responseStatusCode();
  String response = client.responseBody(); // Wajib dibaca agar socket tertutup bersih

  Serial.printf("[API] Response Status: %d\n", statusCode);
  Serial.println("[API] Response Body: " + response);
  Serial.println("----------------------------------------");
}
