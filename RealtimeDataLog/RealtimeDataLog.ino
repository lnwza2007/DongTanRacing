#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <SD.h>

Adafruit_ADS1115 ads;

// --- ขาเชื่อมต่อ SD Card ---
#define SD_CS 5

// --- ตัวแปรสำหรับ Filter (EMA) ---
float filtered_L = 0, filtered_T = 0;
float alpha = 0.2; 

// --- ชื่อไฟล์บันทึกข้อมูล ---
String fileName = "/log_001.csv";

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("\n--- Malasta I: SD Card Logger Mode ---");

  // 1. เริ่มการทำงาน I2C (ADS1115)
  Wire.begin(21, 22);
  if (!ads.begin()) {
    Serial.println("หา ADS1115 ไม่เจอ!");
    while (1);
  }
  ads.setGain(GAIN_TWOTHIRDS);

  // 2. เริ่มการทำงาน SD Card
  SPI.begin(18, 19, 23, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card Mount Failed! (เช็กสาย/Format FAT32)");
    while (1);
  }
  Serial.println("SD Card Ready!");

  // 3. สร้าง Header ของไฟล์ (ถ้ายังไม่มีไฟล์)
  File dataFile = SD.open(fileName, FILE_WRITE);
  if (dataFile) {
    dataFile.println("Millis,Linear_mm,TPS_percent");
    dataFile.close();
    Serial.println("สร้างไฟล์ " + fileName + " สำเร็จ");
  } else {
    Serial.println("เขียนไฟล์ลง SD Card ไม่ได้!");
  }
}

void loop() {
  // 1. อ่านค่าจาก ADS1115
  float vL = ads.computeVolts(ads.readADC_SingleEnded(0)); // A0 = Linear
  float vT = ads.computeVolts(ads.readADC_SingleEnded(1)); // A1 = TPS

  // 2. กรองสัญญาณ (Filter)
  filtered_L = (vL * alpha) + (filtered_L * (1.0 - alpha));
  filtered_T = (vT * alpha) + (filtered_T * (1.0 - alpha));

  // 3. แปลงค่า (Scaling)
  // Linear 75mm (0.5V-4.5V)
  float mm = constrain((filtered_L - 0.5) * (75.0 / 4.0), 0, 75.0);
  // TPS 0-100% (0.5V-4.5V)
  float tps = constrain((filtered_T - 0.5) * (100.0 / 4.0), 0, 100.0);

  // 4. บันทึกลง SD Card ทุกๆ 100ms (10 ครั้งต่อวินาที)
  static unsigned long lastLog = 0;
  if (millis() - lastLog >= 100) {
    File dataFile = SD.open(fileName, FILE_APPEND);
    if (dataFile) {
      dataFile.print(millis());
      dataFile.print(",");
      dataFile.print(mm, 1);
      dataFile.print(",");
      dataFile.println(tps, 1);
      dataFile.close(); // ปิดไฟล์ทุกครั้งเพื่อกันข้อมูลหายถ้าไฟดับ
      
      // แสดงผลทางจอเพื่อเช็ก
      Serial.print("Saved -> mm: "); Serial.print(mm, 1);
      Serial.print(" | TPS: "); Serial.println(tps, 1);
    } else {
      Serial.println("Error writing to SD Card!");
    }
    lastLog = millis();
  }
}