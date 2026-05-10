#include <SPI.h>
#include <SD.h>

#define SD_CS 5

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); } // รอให้ Serial พร้อม

  Serial.println("\n--- Malasta I: SD Card Diagnostics ---");

  // กำหนดพิน SPI ให้ชัดเจน (เผื่อสาย Jumper มีปัญหา)
  SPI.begin(18, 19, 23, SD_CS); 

if (!SD.begin(SD_CS, SPI, 4000000)) {
    Serial.println("❌ SD Card Mount Failed!");
    Serial.println("สาเหตุที่เป็นไปได้:");
    Serial.println("1. ลืมเสียบ Card");
    Serial.println("2. สาย CS (พิน 5) หลวม หรือต่อผิด");
    Serial.println("3. Card ไม่ได้ Format เป็น FAT32 (ห้ามใช้ ExFAT)");
    Serial.println("4. แรงดันไฟตกขณะเริ่มทำงาน");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("❌ No SD card attached");
    return;
  }

  Serial.print("✅ SD Card Type: ");
  if (cardType == CARD_MMC) Serial.println("MMC");
  else if (cardType == CARD_SD) Serial.println("SDSC");
  else if (cardType == CARD_SDHC) Serial.println("SDHC");
  else Serial.println("UNKNOWN");

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("💾 SD Card Size: %llu MB\n", cardSize);
  
  Serial.println("--- ทดสอบการเขียนไฟล์ ---");
  File file = SD.open("/test.txt", FILE_WRITE);
  if (file) {
    file.println("Malasta I Test Write OK!");
    file.close();
    Serial.println("📝 Write Test: SUCCESS!");
    SD.remove("/test.txt"); // ลบไฟล์ทดสอบทิ้ง
  } else {
    Serial.println("📝 Write Test: FAILED!");
  }
}

void loop() {
  // ไม่ต้องทำอะไร
}