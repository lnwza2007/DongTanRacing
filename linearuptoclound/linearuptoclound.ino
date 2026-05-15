#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <InfluxDbClient.h> 
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h> 
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- 1. Network & Cloud Config ---
#define WIFI_SSID       "Frankza"
#define WIFI_PASSWORD   "frank0864793559"

// HiveMQ Cloud Credentials
#define MQTT_SERVER     "2898b29c070f4985b025bbc1d2e1d216.s1.eu.hivemq.cloud" 
#define MQTT_PORT       8883                                   
#define MQTT_USER       "dongtaan_vcu"
#define MQTT_PASSWORD   "Frank2007"
#define MQTT_TOPIC      "balone2/telemetry/tire_fl"         

// InfluxDB Config
#define INFLUXDB_URL    "http://192.168.1.8:8086" 
#define INFLUXDB_TOKEN  "0MvbrNQw5yj-KDbA-QZ73I4oXVQUV4_NE5mhluuZtVB9FzJIJ_6fscWunUVG0TVQ5RxxraCo3-yBzfHGdlG_bw=="
#define INFLUXDB_ORG    "a4f7cc5900f2228e"       
#define INFLUXDB_BUCKET "EV"

// Google Sheets
const char* google_script_url = "https://script.google.com/macros/s/AKfycbw9tdxfIRcttzx2VZ1yTJvoOHpo_i4wnld5hbrZnOo9fBx4voJcWM9yC7rFpajif2Is/exec";

// --- 2. Hardware Pinout ---
#define SD_CS    5
#define I2C_SDA  21
#define I2C_SCL  22

// --- 3. Objects & Variables ---
Adafruit_ADS1115 ads;
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
InfluxDBClient influxClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point sensorData("suspension_data");

unsigned long lastLog = 0;
unsigned long lastGoogleUpdate = 0;
String rx_buffer = "";
String latest_tire_data = "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0"; 

// --- 4. Core Functions ---

// 💡 หัวใจหลัก: ซิงค์เวลาโลกเพื่อให้คุยกับ SSL ของ HiveMQ ได้ (แก้ rc=-2)
void syncTime() {
  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\n✅ Time Synced!");
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to HiveMQ Cloud... ");
    String clientId = "MalastaI-Master-" + String(random(0, 0xffff), HEX);
    
    // พยายามเชื่อมต่อ
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("✅ Connected!");
    } else {
      Serial.print("❌ Fail, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying in 2s...");
      delay(2000);
    }
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // รับข้อมูลจากบอร์ด Temp
  
  delay(2000);
  Serial.println("\n--- DONGTAAN RACING | MALASTA I MASTER ---");

  // 1. WiFi & Time Sync
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n✅ WiFi Connected!");
  syncTime();

  // 2. MQTT SSL Config
  espClient.setInsecure(); // ข้ามใบเซอร์เพื่อความไว (แต่ต้องมีเวลาที่ตรง)
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(512); // ขยาย Buffer สำหรับ String ยาวๆ
  mqttClient.setKeepAlive(60);

  // 3. Hardware Init
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!ads.begin()) Serial.println("❌ ADS1115 FAIL");
  else ads.setGain(GAIN_TWOTHIRDS);

  if (!SD.begin(SD_CS)) Serial.println("❌ SD Card FAIL");
  else Serial.println("✅ SD Card Ready");

  if (influxClient.validateConnection()) Serial.println("✅ InfluxDB Ready!");
}

void loop() {
  // --- A. MQTT Maintenance ---
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // --- B. Suspension Monitoring (10Hz) ---
  if (millis() - lastLog >= 100) {
    lastLog = millis();
    int16_t adc0 = ads.readADC_SingleEnded(0);
    float volts = ads.computeVolts(adc0);
    
    // ลอจิกการคำนวณระยะยุบช่วงล่างของเจมส์
    float mm = ((volts - 0.0246) / (4.2776 - 0.0246)) * 75.0;
    mm = constrain(mm, 0, 75.0);

    // Write to SD
    File f = SD.open("/log.csv", FILE_APPEND);
    if (f) {
      f.printf("%.2f,%.2f,%.4f\n", millis() / 1000.0, mm, volts);
      f.close();
    }
    
    // Write to InfluxDB
    if (WiFi.status() == WL_CONNECTED) {
      sensorData.clearFields();
      sensorData.addField("mm", mm);
      sensorData.addField("volts", volts);
      influxClient.writePoint(sensorData);
    }
  }

  // --- C. Tire Temp Stream (Serial2 -> Cloud) ---
  while (Serial2.available() > 0) {
    char c = Serial2.read();
    if (c == '\n') {
      rx_buffer.trim();
      if (rx_buffer.startsWith("$")) {
        String clean_data = rx_buffer.substring(1); 
        latest_tire_data = clean_data;

        // Save Tire Data to SD
        File f = SD.open("/tire_log.csv", FILE_APPEND); 
        if (f) {
          f.printf("%.2f,%s\n", millis() / 1000.0, clean_data.c_str());
          f.close();
        }

        // 🚀 สตรีมข้อมูลขึ้น Dashboard OS (Vercel)
        // ส่ง Format: [Suspension_mm],[Tire_1],...,[Tire_16]
        int16_t current_adc = ads.readADC_SingleEnded(0);
        float current_mm = ((ads.computeVolts(current_adc) - 0.0246) / 4.253) * 75.0;
        String payload = String(constrain(current_mm, 0, 75.0), 1) + "," + latest_tire_data;
        
        mqttClient.publish(MQTT_TOPIC, payload.c_str());
      }
      rx_buffer = "";
    } else {
      rx_buffer += c;
    }
  }

  // --- D. Google Sheets Update (ทุก 10 วินาที) ---
  if (WiFi.status() == WL_CONNECTED && millis() - lastGoogleUpdate >= 10000) {
    lastGoogleUpdate = millis();
    
    float mm_sheet = ((ads.computeVolts(ads.readADC_SingleEnded(0)) - 0.0246) / 4.253) * 75.0;
    
    HTTPClient http;
    http.begin(google_script_url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");
    
    String json = "{\"mm\":" + String(constrain(mm_sheet, 0, 75.0), 2) + 
                  ",\"tire_data\":\"" + latest_tire_data + "\"}";
                  
    int httpCode = http.POST(json);
    http.end();
    Serial.printf("📊 [Sheets] Update Status: %d\n", httpCode);
  }
}