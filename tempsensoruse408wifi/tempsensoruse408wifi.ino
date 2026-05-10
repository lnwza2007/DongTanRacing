#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "driver/twai.h"

// --- 1. ตั้งค่า WiFi (วง Hotspot ของเจมส์) ---
#define WIFI_SSID "Frankza"
#define WIFI_PASSWORD "frank0864793559"

// --- 2. ข้อมูล InfluxDB จริงของเจมส์ ---
#define INFLUXDB_URL "http://192.168.1.3:8086"  
#define INFLUXDB_TOKEN "0MvbrNQw5yj-KDbA-QZ73I4oXVQUV4_NE5mhluuZtVB9FzJIJ_6fscWunUVG0TVQ5RxxraCo3-yBzfHGdlG_bw=="
#define INFLUXDB_ORG "a4f7cc5900f2228e"       
#define INFLUXDB_BUCKET "EV"
#define TZ_INFO "WIB-7"

// --- 3. ขาเชื่อมต่อโมดูล CAN ---
#define CAN_TX_PIN GPIO_NUM_4  // CTX -> GPIO 4
#define CAN_RX_PIN GPIO_NUM_5  // CRX -> GPIO 5
#define LF_BASE_ID 0x4B0       // Base ID สำหรับล้อหน้าซ้าย Izze IRTS-V3

// --- 4. ตัวแปรเก็บค่าอุณหภูมิยาง ---
float tire_temps[16] = {0.0};
bool data_updated = false;

WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point tireSensor("tire_temperature");

void setup() {
  Serial.begin(115200);
  // เปิดพอร์ต Serial2 ดักรับและส่งสัญญาณ (TX=17, RX=16) เพื่อคุยกับบอร์ด Linear
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  
  delay(2000);
  Serial.println("\n--- Dongtaan Racing: Telemetry (Temp Board) Online ---");

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");

  // ซิงค์เวลาจากอินเทอร์เน็ต
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  if (client.validateConnection()) {
    Serial.println("✅ InfluxDB Connected!");
  } else {
    Serial.print("❌ InfluxDB Connection Fail: ");
    Serial.println(client.getLastErrorMessage());
  }

  // เริ่มใช้งานระบบ CAN Bus (TWAI) ที่ความเร็ว 1 Mbit/s
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS(); 
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
    Serial.println("✅ CAN Bus Online!");
  } else {
    Serial.println("❌ Failed to start CAN Bus");
    while (1);
  }

  tireSensor.addTag("wheel_position", "front_left");
}

void loop() {
  if (wifiMulti.run() != WL_CONNECTED) {
    delay(1000);
    return;
  }

  twai_message_t message;
  
  // 1. อ่านข้อมูล CAN Bus
  if (twai_receive(&message, pdMS_TO_TICKS(5)) == ESP_OK) {
    if (!(message.rtr) && (message.identifier >= LF_BASE_ID) && (message.identifier <= LF_BASE_ID + 3)) {
      int msg_offset = message.identifier - LF_BASE_ID;

      // แกะข้อมูลอุณหภูมิยางทีละ 4 แชนเนล
      for (int i = 0; i < 4; i++) {
        int byte_idx = i * 2;
        int channel_idx = (msg_offset * 4) + i;

        uint16_t raw_val = (message.data[byte_idx] << 8) | message.data[byte_idx + 1];
        float temp_val = (raw_val * 0.1) - 100.0;
        
        // ตรวจสอบข้อมูลขยะ (กรองให้สัญญาณเสถียรที่สุด)
        if (temp_val >= -50.0 && temp_val <= 200.0) {
          tire_temps[channel_idx] = temp_val;
        }
      }

      // เมื่อแกะค่าถึงแชนเนลกลุ่มสุดท้าย (0x4B3) เรียบร้อย สั่งอัปเดตข้อมูล
      if (msg_offset == 3) {
        data_updated = true; 
      }
    }
  }

  // 2. ส่งข้อมูลเมื่ออัปเดตครบถ้วน
  if (data_updated) {
    tireSensor.clearFields(); 
    
    // ตั้งโครงสร้างข้อความเปิดหัวด้วยสัญลักษณ์ $ เพื่อใช้กรองจังหวะข้อมูลชนกัน
    String data_string = "$"; 

    // แพ็กข้อมูลสำหรับยิงเข้า InfluxDB และสร้าง CSV String ส่งไปบอร์ด Linear
    for (int i = 0; i < 16; i++) {
      tireSensor.addField("ch_" + String(i + 1), tire_temps[i]);
      
      data_string += String(tire_temps[i], 1);
      if (i < 15) {
        data_string += ",";
      }
    }

    // 💡 ยิงข้อมูลอุณหภูมิยาง 16 แชนเนล พ่นผ่านขา D17 (TX2) ข้ามไปบอร์ด Linear
    Serial2.println(data_string); 

    if (!client.writePoint(tireSensor)) {
      Serial.print("Influx Write Fail: ");
      Serial.println(client.getLastErrorMessage());
    } else {
      Serial.println("✅ Tire Temps Sent (Influx & Serial2)");
    }

    data_updated = false; 
  }
}