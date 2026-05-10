#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <InfluxDbClient.h> // เพิ่ม Library InfluxDB
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- ตั้งค่า InfluxDB (Docker) ---
#define INFLUXDB_URL "http://172.20.10.4:8086" 
#define INFLUXDB_TOKEN "0MvbrNQw5yj-KDbA-QZ73I4oXVQUV4_NE5mhluuZtVB9FzJIJ_6fscWunUVG0TVQ5RxxraCo3-yBzfHGdlG_bw=="
#define INFLUXDB_ORG "a4f7cc5900f2228e"       // Org ID ของเจมส์
#define INFLUXDB_BUCKET "EV"
#define TZ_INFO "WIB-7"

// --- ตั้งค่าเดิมของเจมส์ ---
#define SD_CS 5
#define I2C_SDA 21
#define I2C_SCL 22

const char* STATION_SSID = "Frank";
const char* STATION_PASS = "112233445566";
const char* google_script_url = "https://script.google.com/macros/s/AKfycbyGUq43Cb1i1KU1XGxfvYZ61A1LiJAzAhcMjoXJUOeseo6tX7aquFjHX4g0iPcBlJLs/exec";

Adafruit_ADS1115 ads;
unsigned long lastLog = 0;
unsigned long lastGoogleUpdate = 0;
unsigned long lastInfluxUpdate = 0; 

// สร้าง Client สำหรับ InfluxDB
InfluxDBClient influxClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point sensorData("suspension_data"); // ชื่อ Measurement ใน Grafana

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  
  delay(3000); 
  Serial.println("\n--- Malasta I Telemetry System ---");

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

  // ตรวจสอบการเชื่อมต่อ InfluxDB
  if (influxClient.validateConnection()) {
    Serial.println("✅ InfluxDB Connected!");
  } else {
    Serial.print("❌ InfluxDB Fail: ");
    Serial.println(influxClient.getLastErrorMessage());
  }
}

void loop() {
  // 1. อ่านค่าและบันทึกลง SD Card / Serial2 (ทุก 100ms)
  if (millis() - lastLog >= 100) {
    lastLog = millis();
    int16_t adc0 = ads.readADC_SingleEnded(0);
    float volts = ads.computeVolts(adc0);
    
    // --- 🎛️ ปรับสูตร Calibrate ด้วยแรงดันจริงของเจมส์ ---
    // ยืดสุด 0.0246 V -> 0 mm / ยุบสุด 4.2776 V -> 75 mm
    float mm = ((volts - 0.0246) / (4.2776 - 0.0246)) * 75.0;
    
    // ป้องกันค่าหลุดขอบเขตช่วงล่างทางฟิสิกส์
    if (mm > 75.0) mm = 75.0;
    if (mm < 0.0) mm = 0.0;

    // บันทึกลง SD Card
    File f = SD.open("/log.csv", FILE_APPEND);
    if (f) {
      f.printf("%.2f,%.2f\n", millis() / 1000.0, mm);
      f.close();
    }
    
    Serial2.println(mm); // ส่งไปจอหรือบอร์ดอื่น
    
    // --- ส่งข้อมูลเข้า InfluxDB (แบบเรียลไทม์) ---
    if (WiFi.status() == WL_CONNECTED) {
      sensorData.clearFields();
      sensorData.addField("mm", mm);
      sensorData.addField("volts", volts);
      
      if (!influxClient.writePoint(sensorData)) {
        Serial.print("Influx Write Fail: ");
        Serial.println(influxClient.getLastErrorMessage());
      }
    }
    Serial.print("."); // จุดบอกสถานะการทำงาน
  }

  // 2. ส่งเข้า Google Sheet (ทุก 10 วินาทีตามเดิม)
  if (WiFi.status() == WL_CONNECTED && millis() - lastGoogleUpdate >= 10000) {
    lastGoogleUpdate = millis();
    
    // อ่านค่าสดนำมาสเกลจริงเพื่อยิงขึ้น Sheets
    float current_volts = ads.computeVolts(ads.readADC_SingleEnded(0));
    float current_mm = ((current_volts - 0.0246) / (4.2776 - 0.0246)) * 75.0;
    if (current_mm > 75.0) current_mm = 75.0;
    if (current_mm < 0.0) current_mm = 0.0;

    HTTPClient http;
    http.begin(google_script_url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");
    
    String json = "{\"mm\":" + String(current_mm, 2) + "}";
    int httpCode = http.POST(json);
    
    if (httpCode > 0) Serial.printf("\n[Sheets] OK: %d\n", httpCode);
    http.end();
  }
}