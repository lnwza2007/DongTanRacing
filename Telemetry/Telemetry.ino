#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <SD.h>

#define SD_CS 5
#define I2C_SDA 21
#define I2C_SCL 22

Adafruit_ADS1115 ads;
bool isLogging = true; // สถานะเริ่มต้นคือให้บันทึกข้อมูลปกติ

void setup() {
  Serial.begin(115200);   
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // ส่งค่าไปบอร์ด Dashboard
  
  Wire.begin(I2C_SDA, I2C_SCL);
  ads.begin();
  ads.setGain(GAIN_TWOTHIRDS);
  
  if(!SD.begin(SD_CS)) {
    Serial.println("❌ SD Card FAIL");
  } else {
    Serial.println("✅ SD Card READY");
  }
  Serial.println("--- System Ready ---");
  Serial.println("Commands: 'd' = Dump & Pause | 'r' = Resume Logging");
}

void loop() {
  // --- ส่วนรับคำสั่งจาก Serial Monitor ---
  if (Serial.available()) {
    char cmd = Serial.read();
    
    if (cmd == 'd') { 
      // 1. สั่งหยุดบันทึกข้อมูลทันที
      isLogging = false; 
      Serial.println("\n[PAUSED] Dumping data...");
      Serial.println("--- START_LOG_DATA ---");
      
      // 2. เปิดไฟล์ออกมาอ่านเพื่อส่งเข้าคอม
      File f = SD.open("/log.csv", FILE_READ);
      if (f) {
        while (f.available()) {
          Serial.write(f.read());
        }
        f.close();
      }
      Serial.println("\n--- END_LOG_DATA ---");
      Serial.println("[STATUS] Data dumped. System is PAUSED. Press 'r' to resume.");
    } 
    
    else if (cmd == 'r') {
      // 3. สั่งให้กลับมาบันทึกข้อมูลใหม่
      isLogging = true;
      Serial.println("\n[RESUMED] Recording started...");
    }
  }

  // --- ส่วนบันทึก Log และส่งข้อมูล (จะทำงานเมื่อ isLogging เป็น true เท่านั้น) ---
  static unsigned long lastLog = 0;
  if (isLogging && (millis() - lastLog >= 100)) { 
    lastLog = millis();
    
    int16_t adc0 = ads.readADC_SingleEnded(0);
    float volts = ads.computeVolts(adc0);
    float mm = volts * (75.0 / 3.808); // สูตรคำนวณของเจมส์

    // บันทึกลง SD Card
    File f = SD.open("/log.csv", FILE_APPEND);
    if(f) {
      f.printf("%.2f,%.2f\n", millis() / 1000.0, mm);
      f.close();
    }

    // ส่งไปบอร์ดที่ 2 (Dashboard) เพื่อให้คนขับเห็นค่าตลอดเวลา
    Serial2.println(mm); 
    
    // Debug บนหน้าจอคอม
    Serial.print("."); // แสดงจุดเล็กๆ เพื่อให้รู้ว่าเครื่องยังทำงานอยู่
  }
}