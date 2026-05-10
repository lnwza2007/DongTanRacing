#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "driver/twai.h"

// --- 1. ตั้งค่า WiFi คณะ Dongtaan ---
#define WIFI_SSID "Frank"
#define WIFI_PASSWORD "112233445566"

// --- 2. ข้อมูล InfluxDB จริงของเจมส์ (อัปเดต IP และ Token ล่าสุด) ---
#define INFLUXDB_URL "http://172.20.10.4:8086" 
#define INFLUXDB_TOKEN "0MvbrNQw5yj-KDbA-QZ73I4oXVQUV4_NE5mhluuZtVB9FzJIJ_6fscWunUVG0TVQ5RxxraCo3-yBzfHGdlG_bw=="
#define INFLUXDB_ORG "a4f7cc5900f2228e"       // Org ID ของเจมส์
#define INFLUXDB_BUCKET "EV"

// ตั้งค่า Time Zone ประเทศไทย
#define TZ_INFO "WIB-7"

// --- 3. ขาเชื่อมต่อโมดูล CAN ---
#define CAN_TX_PIN GPIO_NUM_4  // CTX -> GPIO 4
#define CAN_RX_PIN GPIO_NUM_5  // CRX -> GPIO 5
#define LF_BASE_ID 0x4B0       // Base ID สำหรับล้อหน้าซ้าย Izze IRTS-V3

// --- 4. ตัวแปรเก็บค่าอุณหภูมิยาง ---
float tire_temps[16] = {0.0};
float internal_sensor_temp = 0.0;
bool data_updated = false;

WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// สร้าง Data Point สำหรับยิงเข้า InfluxDB
Point tireSensor("tire_temperature");

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- Dongtaan Racing: Telemetry starting on DongtaanWiFi_5G ---");

  // เชื่อมต่อ WiFi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to DongtaanWiFi_5G");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ WiFi Connected!");

  // ซิงค์เวลาจากอินเทอร์เน็ต
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // ทดสอบเชื่อมต่อ InfluxDB
  if (client.validateConnection()) {
    Serial.print("✅ Connected to InfluxDB Server: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("❌ InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // คอนฟิกพอร์ต CAN Bus (TWAI) ความเร็ว 1 Mbit/s
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS(); 
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
    Serial.println("✅ CAN Bus (TWAI) Driver Online!");
  } else {
    Serial.println("❌ Failed to start CAN Bus");
    while (1);
  }

  // ใส่ Tag เพื่อจัดกลุ่มข้อมูลใน Grafana
  tireSensor.addTag("wheel_position", "front_left");
}

void loop() {
  // รักษาการเชื่อมต่อ WiFi
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi Disconnected! Trying to reconnect...");
    delay(1000);
    return;
  }

  twai_message_t message;
  
  // รอรับข้อมูลจาก CAN Bus
  if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    
    // ดักกรองเอาเฉพาะข้อมูลในช่วง ID ของเซ็นเซอร์ล้อหน้าซ้าย (0x4B0 - 0x4B4)
    if (!(message.rtr) && (message.identifier >= LF_BASE_ID) && (message.identifier <= LF_BASE_ID + 4)) {
      
      int msg_offset = message.identifier - LF_BASE_ID;

      if (msg_offset >= 0 && msg_offset <= 3) {
        // แกะค่าอุณหภูมิยาง 4 จุดต่อ 1 Message (16-bit Big-Endian)
        for (int i = 0; i < 4; i++) {
          int byte_idx = i * 2;
          int channel_idx = (msg_offset * 4) + i;

          uint16_t raw_val = (message.data[byte_idx] << 8) | message.data[byte_idx + 1];
          tire_temps[channel_idx] = (raw_val * 0.1) - 100.0;
        }
      } 
      else if (msg_offset == 4) {
        // ID 0x4B4 คืออุณหภูมิภายในตัวเซ็นเซอร์เอง
        uint16_t raw_internal = (message.data[0] << 8) | message.data[1];
        internal_sensor_temp = (raw_internal * 0.1) - 100.0;
        
        // รับข้อมูลครบ 16 จุดและอุณหภูมิภายในตัวเซ็นเซอร์แล้ว สั่งให้ส่งขึ้น Database
        data_updated = true;
      }
    }
  }

  // --- จัดการส่งค่าเข้า InfluxDB ---
  if (data_updated) {
    tireSensor.clearFields(); // เคลียร์บัฟเฟอร์ข้อมูลเก่าออก

    // แพ็กค่าอุณหภูมิทั้ง 16 จุดใส่ Data Point
    for (int i = 0; i < 16; i++) {
      String field_name = "ch_" + String(i + 1);
      tireSensor.addField(field_name, tire_temps[i]);
    }
    tireSensor.addField("internal_temp", internal_sensor_temp);

    Serial.print("Uploading Tire Temps to InfluxDB... ");
    
    // ยิงขึ้นเซิร์ฟเวอร์
    if (!client.writePoint(tireSensor)) {
      Serial.print("❌ Error: ");
      Serial.println(client.getLastErrorMessage());
    } else {
      Serial.println("✅ Success!");
    }

    data_updated = false; // เคลียร์สถานะ
  }
}