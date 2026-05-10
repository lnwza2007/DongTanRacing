#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <SD.h>

// การตั้งค่า Pin
#define SD_CS 5
#define I2C_SDA 21
#define I2C_SCL 22

Adafruit_ADS1115 ads;

void setup() {
  Serial.begin(115200);   
  // Serial2 สำหรับส่งไปบอร์ด Web: ขา 17 เป็น TX
  Serial2.begin(115200, SERIAL_8N1, 16, 17); 
  
  // เริ่มต้น I2C สำหรับ ADS1115
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!ads.begin()) { 
    Serial.println("❌ ADS1115 Fail - Check Wiring!"); 
  }
  ads.setGain(GAIN_TWOTHIRDS); // รองรับแรงดันสูงสุด 6.144V (เหมาะสำหรับ Sensor 5V)
  
  // เริ่มต้น SD Card
  if(!SD.begin(SD_CS)) {
    Serial.println("❌ SD Card FAIL");
  } else {
    Serial.println("✅ SD Card READY");
    // สร้าง Header สำหรับไฟล์ CSV ถ้ายังไม่มี
    if(!SD.exists("/log.csv")) {
      File f = SD.open("/log.csv", FILE_WRITE);
      if(f) {
        f.println("Time_sec,Linear_mm");
        f.close();
      }
    }
  }
}

void loop() {
  static unsigned long lastLog = 0;
  
  // อัปเดตข้อมูลทุก 100ms (10Hz)
  if (millis() - lastLog >= 100) { 
    lastLog = millis();
    
    // 1. อ่านค่าแรงดันจริงจาก ADS1115
    int16_t adc0 = ads.readADC_SingleEnded(0);
    float volts = ads.computeVolts(adc0);
    
    // 2. คำนวณระยะ mm (จูนค่า V_max = 3.8276 ตามที่เช็กมา)
    float mm = volts * (75.0 / );
    
    // 3. ปรับจูนค่าให้อยู่ในขอบเขต 0 - 75 mm
    if (mm > 75.0) mm = 75.0;
    if (mm < 0.05) mm = 0.0; // ตัด Noise ที่จุดเริ่ม

    // 4. บันทึกลง SD Card
    File f = SD.open("/log.csv", FILE_APPEND);
    if(f) {
      f.printf("%.2f,%.2f\n", millis() / 1000.0, mm);
      f.close(); // ปิดไฟล์ทุกครั้งเพื่อความปลอดภัยของข้อมูล
    } else {
      Serial.println("⚠️ SD Write Error!");
    }

    // 5. ส่งออก Serial2 ไปบอร์ด Web Gateway
    Serial2.println(String(mm, 2)); 
    
    // 6. Debug พิมพ์ค่าออก Serial Monitor (Mac)
    Serial.printf("[%.2fs] Volts: %.4fV -> %.2f mm\n", millis()/1000.0, volts, mm);
  }
}