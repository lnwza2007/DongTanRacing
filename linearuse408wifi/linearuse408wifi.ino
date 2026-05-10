#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <InfluxDbClient.h> 
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- ตั้งค่า InfluxDB ---
#define INFLUXDB_URL "http://192.168.1.3:8086" 
#define INFLUXDB_TOKEN "0MvbrNQw5yj-KDbA-QZ73I4oXVQUV4_NE5mhluuZtVB9FzJIJ_6fscWunUVG0TVQ5RxxraCo3-yBzfHGdlG_bw=="
#define INFLUXDB_ORG "a4f7cc5900f2228e"       
#define INFLUXDB_BUCKET "EV"
#define TZ_INFO "WIB-7"

// --- ขาต่อใช้งานเดิมของเจมส์ ---
#define SD_CS 5
#define I2C_SDA 21
#define I2C_SCL 22

const char* STATION_SSID = "Frankza";
const char* STATION_PASS = "frank0864793559";
// ลิงก์ Apps Script ตัวจริงที่อัปเดตล่าสุดของเจมส์
const char* google_script_url = "https://script.google.com/macros/s/AKfycbw9tdxfIRcttzx2VZ1yTJvoOHpo_i4wnld5hbrZnOo9fBx4voJcWM9yC7rFpajif2Is/exec";

Adafruit_ADS1115 ads;
unsigned long lastLog = 0;
unsigned long lastGoogleUpdate = 0;

// ตัวแปรส่วนกลางสำหรับกรองข้อมูลผ่านบัฟเฟอร์แบบ Non-blocking
String rx_buffer = "";
String latest_tire_data = "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0"; 

InfluxDBClient influxClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point sensorData("suspension_data"); 

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  Serial.begin(115200);
  // เริ่มต้นรับข้อมูลจากบอร์ด Temp (RX=16, TX=17)
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  
  delay(3000); 
  Serial.println("\n--- Malasta I: Dashboard & Google Sheets Integrated System ---");

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!ads.begin()) Serial.println("❌ ADS1115 FAIL");
  else {
    ads.setGain(GAIN_TWOTHIRDS);
    Serial.println("✅ ADS1115 Ready");
  }

  if (!SD.begin(SD_CS)) Serial.println("❌ SD Card FAIL");
  else Serial.println("✅ SD Card Ready");

  WiFi.begin(STATION_SSID, STATION_PASS);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");

  if (influxClient.validateConnection()) {
    Serial.println("✅ InfluxDB Connected!");
  } else {
    Serial.print("❌ InfluxDB Fail: ");
    Serial.println(influxClient.getLastErrorMessage());
  }
}

void loop() {
  // 1. งานอ่านระยะยุบโช้คและจดบันทึกลง SD Card / InfluxDB (ทุก 100ms)
  if (millis() - lastLog >= 100) {
    lastLog = millis();
    int16_t adc0 = ads.readADC_SingleEnded(0);
    float volts = ads.computeVolts(adc0);
    
    // คำนวณระยะยุบช่วงล่าง (0.0246V -> 0mm / 4.2776V -> 75mm)
    float mm = ((volts - 0.0246) / (4.2776 - 0.0246)) * 75.0;
    
    if (mm > 75.0) mm = 75.0;
    if (mm < 0.0) mm = 0.0;

    // บันทึกระยะยุบโช้คลง SD Card (ไฟล์ log.csv)
    File f = SD.open("/log.csv", FILE_APPEND);
    if (f) {
      // จดค่ามิลลิเซก หน่วยมิลลิเมตร และโวลต์ดิบลง SD Card เพื่อใช้วิเคราะห์ช่วงล่าง
      f.printf("%.2f,%.2f,%.4f\n", millis() / 1000.0, mm, volts);
      f.close();
    }
    
    // อัปเดตข้อมูลขึ้น InfluxDB
    if (WiFi.status() == WL_CONNECTED) {
      sensorData.clearFields();
      sensorData.addField("mm", mm);
      sensorData.addField("volts", volts);
      
      if (!influxClient.writePoint(sensorData)) {
        Serial.print("Influx Write Fail: ");
        Serial.println(influxClient.getLastErrorMessage());
      }
    }
    Serial.print("."); 
  }

  // 2. 💡 ดักกวาดข้อมูล Serial2 แบบเสถียร (คัดกรองขยะด้วยหัวสัญลักษณ์ $)
  while (Serial2.available() > 0) {
    char c = Serial2.read();
    if (c == '\n') { // เมื่อตรวจพบสัญลักษณ์ปิดท้ายบรรทัด
      rx_buffer.trim();
      
      // ตรวจสอบว่าสายอักษรเป็นแพ็กเกจข้อมูลอุณหภูมิยางที่สมบูรณ์จริงหรือไม่
      if (rx_buffer.startsWith("$")) {
        // ตัดสัญลักษณ์ $ นำหน้าออก เพื่อเอาเฉพาะอุณหภูมิยาง 16 ตัวเลขล้วนๆ
        String clean_data = rx_buffer.substring(1); 
        latest_tire_data = clean_data; // พักค่าไว้สำหรับรอบส่งขึ้น Google Sheet

        // ทำการบันทึกแยกเป็นไฟล์ tire_log.csv ลง SD Card ของบอร์ดนี้เลย
        File f = SD.open("/tire_log.csv", FILE_APPEND); 
        if (f) {
          f.printf("%.2f,%s\n", millis() / 1000.0, clean_data.c_str());
          f.close();
          Serial.println("\n💾 Tire temperatures successfully saved to SD Card!");
        }
      }
      rx_buffer = ""; // รีเซ็ตบัฟเฟอร์พร้อมรับข้อความถัดไป
    } else {
      rx_buffer += c; // เก็บสะสมตัวอักษรเข้าสตริง
    }
  }

  // 3. ส่งข้อมูลรวม (โช้คอัพ + อุณหภูมิหน้ายาง) ขึ้น Google Sheet (ทุก 10 วินาที)
  if (WiFi.status() == WL_CONNECTED && millis() - lastGoogleUpdate >= 10000) {
    lastGoogleUpdate = millis();
    
    float current_volts = ads.computeVolts(ads.readADC_SingleEnded(0));
    float current_mm = ((current_volts - 0.0246) / (4.2776 - 0.0246)) * 75.0;
    if (current_mm > 75.0) current_mm = 75.0;
    if (current_mm < 0.0) current_mm = 0.0;

    HTTPClient http;
    http.begin(google_script_url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");
    
    // แพ็กข้อมูลรูปแบบ JSON รวมยอดส่งขึ้นสคริปต์
    String json = "{"
                  "\"mm\":" + String(current_mm, 2) + ","
                  "\"volts\":" + String(current_volts, 4) + ","
                  "\"tire_data\":\"" + latest_tire_data + "\""
                  "}";
                  
    int httpCode = http.POST(json);
    
    if (httpCode > 0) {
      Serial.printf("\n📊 [Sheets] Sent All Data! Response Code: %d\n", httpCode);
    } else {
      Serial.printf("\n❌ [Sheets] Post Failed, Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}