#include <Arduino.h>
#include <SD_MMC.h>

// =====================================
// ESP32-S3 N16R8 CAM
// Onboard MicroSD
// SDMMC 1-BIT MODE
// =====================================

#define SD_CMD  38
#define SD_CLK  39
#define SD_D0   40

void setup() {

  Serial.begin(115200);

  delay(3000);

  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP32-S3 N16R8 CAM");
  Serial.println(" MICRO SD CARD TEST");
  Serial.println("================================");

  // =================================
  // SET PIN SDMMC
  // =================================

  Serial.println("Setting SDMMC pins...");

  if (!SD_MMC.setPins(
        SD_CLK,
        SD_CMD,
        SD_D0
      )) {

    Serial.println("ERROR: Gagal set pin SDMMC!");
    return;
  }

  Serial.println("SDMMC pins OK");

  // =================================
  // INITIALIZE SD CARD
  // =================================

  Serial.println("Initializing SD Card...");

  // true = 1-bit mode
  if (!SD_MMC.begin("/sdcard", true)) {

    Serial.println("ERROR: SD Card gagal diinisialisasi!");
    Serial.println();
    Serial.println("Cek:");
    Serial.println("- MicroSD sudah terpasang");
    Serial.println("- Format FAT32");
    Serial.println("- SD Card tidak rusak");
    Serial.println("- Pin SDMMC sesuai board");

    return;
  }

  Serial.println("SD Card berhasil!");

  // =================================
  // CARD TYPE
  // =================================

  uint8_t cardType = SD_MMC.cardType();

  Serial.print("Card Type: ");

  if (cardType == CARD_MMC) {

    Serial.println("MMC");

  }
  else if (cardType == CARD_SD) {

    Serial.println("SDSC");

  }
  else if (cardType == CARD_SDHC) {

    Serial.println("SDHC");

  }
  else if (cardType == CARD_NONE) {

    Serial.println("NO CARD");

    return;

  }
  else {

    Serial.println("UNKNOWN");

  }

  // =================================
  // CARD SIZE
  // =================================

  uint64_t cardSize = SD_MMC.cardSize();

  Serial.print("Card Size: ");

  Serial.print(cardSize / (1024 * 1024));

  Serial.println(" MB");

  // =================================
  // TOTAL SPACE
  // =================================

  uint64_t totalBytes = SD_MMC.totalBytes();

  Serial.print("Total Space: ");

  Serial.print(totalBytes / (1024 * 1024));

  Serial.println(" MB");

  // =================================
  // USED SPACE
  // =================================

  uint64_t usedBytes = SD_MMC.usedBytes();

  Serial.print("Used Space: ");

  Serial.print(usedBytes / (1024 * 1024));

  Serial.println(" MB");

  // =================================
  // WRITE FILE
  // =================================

  Serial.println();
  Serial.println("Writing /test.txt...");

  File file = SD_MMC.open("/test.txt", FILE_APPEND);

  if (!file) {

    Serial.println("ERROR: Gagal membuka file!");

  }
  else {

    file.println("Hello ESP32-S3 N16R8 CAM!");
    file.println("SD Card test berhasil.");
    file.println("------------------------");

    file.close();

    Serial.println("Write berhasil!");

  }

  // =================================
  // READ FILE
  // =================================

  Serial.println();
  Serial.println("Reading /test.txt...");

  file = SD_MMC.open("/test.txt", FILE_READ);

  if (!file) {

    Serial.println("ERROR: Gagal membuka file untuk read!");

  }
  else {

    Serial.println("------------------------");

    while (file.available()) {

      Serial.write(file.read());

    }

    file.close();

    Serial.println();
    Serial.println("------------------------");

    Serial.println("Read berhasil!");

  }

  // =================================
  // TEST SELESAI
  // =================================

  Serial.println();
  Serial.println("================================");
  Serial.println(" SD CARD TEST SELESAI");
  Serial.println("================================");
}

void loop() {

}
