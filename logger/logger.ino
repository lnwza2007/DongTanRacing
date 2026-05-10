#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "soc/soc.h"             // สำหรับปิด Brownout
#include "soc/rtc_cntl_reg.h"    // สำหรับปิด Brownout

#define SD_CS 5
#define I2C_SDA 21
#define I2C_SCL 22

const char* STATION_SSID = "Frankza";//Frank
const char* STATION_PASS = "frank0864793559";//112233445566
const char* google_script_url = "https://script.google.com/macros/s/AKfycbyGUq43Cb1i1KU1XGxfvYZ61A1LiJAzAhcMjoXJUOeseo6tX7aquFjHX4g0iPcBlJLs/exec";

Adafruit_ADS1115 ads;
unsigned long lastLog = 0;
unsigned long lastGoogleUpdate = 0;

void setup() {
  // 0. ปิดระบบตรวจจับไฟตกทันทีที่เริ่ม (แก้ปัญหารีเซ็ตจาก WiFi)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  
  delay(3000); 
  Serial.println("\n--- Malasta I Booting (Brownout Disabled) ---");

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!ads.begin()) {
    Serial.println("❌ ADS1115 Not Found");
  } else {
    Serial.println("✅ ADS1115 Ready");
    ads.setGain(GAIN_TWOTHIRDS); // รับแรงดันได้ถึง +/- 6.144V (ครอบคลุม 4.9V)
  }

  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD Card FAIL");
  } else {
    Serial.println("✅ SD Card Ready");
  }

  WiFi.begin(STATION_SSID, STATION_PASS);
  Serial.print("Connecting WiFi");
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
  } else {
    Serial.println("\n❌ WiFi Offline Mode");
  }
}

void loop() {
  if (millis() - lastLog >= 100) {
    lastLog = millis();

    int16_t adc0 = ads.readADC_SingleEnded(0);
    float volts = ads.computeVolts(adc0);
    
    // --- ปรับสูตรใหม่: 4.9V = 75mm ---
    float mm = volts * (75.0 / 4.9); 
    
    // จำกัดค่าไม่ให้เกินสเปก
    if (mm > 75.0) mm = 75.0; 
    if (mm < 0.0) mm = 0.0;

    File f = SD.open("/log.csv", FILE_APPEND);
    if (f) {
      f.printf("%.2f,%.2f\n", millis() / 1000.0, mm);
      f.close();
    }

    Serial2.println(mm);
    Serial.print("."); 
  }

  if (WiFi.status() == WL_CONNECTED && millis() - lastGoogleUpdate >= 10000) {
    lastGoogleUpdate = millis();
    
    float current_mm = ads.computeVolts(ads.readADC_SingleEnded(0)) * (75.0 / 4.9);
    if (current_mm > 75.0) current_mm = 75.0;

    HTTPClient http;
    http.begin(google_script_url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");
    
    String json = "{\"mm\":" + String(current_mm, 2) + "}";
    int httpCode = http.POST(json);
    
    if (httpCode > 0) Serial.printf("\n[GoogleSheet] Success: %d\n", httpCode);
    else Serial.printf("\n[GoogleSheet] Error: %s\n", http.errorToString(httpCode).c_str());
    
    http.end();
  }
}