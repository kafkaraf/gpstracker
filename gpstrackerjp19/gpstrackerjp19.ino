#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>      // Menggunakan WiFiClientSecure untuk HTTPS di ESP32
#include <ArduinoHttpClient.h>     // Library untuk HTTP/HTTPS Request

// ================== KONFIGURASI WIFI ==================
const char* ssid = "...";
const char* password = "";

// ================== KONFIGURASI GPS ==================
const int RX_PIN = 16; // ESP32-S3 RX terhubung ke TX GPS
const int TX_PIN = 17; // ESP32-S3 TX terhubung ke RX GPS
HardwareSerial GPS_Serial(1);
TinyGPSPlus gps;

// ================== API CONFIG ==================
const char* server = "...";
int port = ...; // Port HTTPS
long mmsi_device = ....; // Set nomor MMSI di sini

WiFiClientSecure wifiClient;
HttpClient client = HttpClient(wifiClient, server, port);
unsigned long lastSend = 0; // Variabel global untuk rate limit API

// ================== DEKLARASI FUNGSI ==================
void sendToAPI(long mmsi, float lat, float lon, float speed, float course);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inisialisasi koneksi WiFi
  Serial.println();
  Serial.print("Menghubungkan ke WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Berhasil Terhubung!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("----------------------------------------");

  // By-pass verifikasi sertifikat SSL untuk kemudahan koneksi HTTPS
  wifiClient.setInsecure();

  // Inisialisasi GPS
  GPS_Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("GPS Module Initializing...");
}

void loop() {
  // Membaca data dari GPS
  while (GPS_Serial.available() > 0) {
    char c = GPS_Serial.read();
    gps.encode(c);
  }

  // Jika lokasi ter-update, ambil datanya dan kirim ke API
  if (gps.location.isUpdated()) {
    float lat = gps.location.lat();
    float lon = gps.location.lng();
    
    // Ambil nilai Speed (dalam Knots) dan Course (dalam Derajat)
    float speed = gps.speed.isValid() ? gps.speed.knots() : 0.0;
    float course = gps.course.isValid() ? gps.course.deg() : 0.0;

    // Panggil fungsi kirim ke API
    sendToAPI(mmsi_device, lat, lon, speed, course);
  }

  // Peringatan jika GPS tidak mendeteksi sinyal / tidak ada data kabel
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (gps.charsProcessed() < 10) {
      Serial.println("[Peringatan] Tidak ada data dari GPS. Periksa kabel TX/RX!");
    }
  }
}

// ================== API FUNCTION ==================
void sendToAPI(long mmsi, float lat, float lon, float speed, float course) {
  // rate limit 2 detik
  if (millis() - lastSend < 2000) return;
  lastSend = millis();

  String fullApiPath = String("/api/update-user-point/") + String(mmsi);

  // JSON body sesuai validator Laravel
  String json = "{";
  json += "\"mmsi\":\"" + String(mmsi) + "\",";
  json += "\"latitude\":" + String(lat, 6) + ",";
  json += "\"longitude\":" + String(lon, 6) + ",";
  json += "\"speed\":" + String(speed) + ",";
  json += "\"course\":" + String(course);
  json += "}";

  Serial.println("\nSending API:");
  Serial.println(json);

  // Memulai proses request ke server
  client.beginRequest();
  client.post(fullApiPath);
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("X-Secret-Key", "....");
  client.sendHeader("Content-Length", json.length());
  client.beginBody();
  client.print(json);
  client.endRequest();

  // Membaca response dari server
  int statusCode = client.responseStatusCode();
  String response = client.responseBody();

  Serial.print("Status: ");
  Serial.println(statusCode);
  Serial.println("Response: " + response);
  Serial.println("----------------------------------------");
}
