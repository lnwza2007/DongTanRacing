#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "driver/twai.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // ดึงคำสั่งตั้งค่า Channel ระดับลึกของ Wi-Fi

// --- 1. ตั้งค่า WiFi (วง Hotspot ของเจมส์) ---
#define WIFI_SSID "Frank"
#define WIFI_PASSWORD "112233445566"

// --- 2. ข้อมูล InfluxDB จริงของเจมส์ ---
#define INFLUXDB_URL "http://192.168.1.2:8086"  
#define INFLUXDB_TOKEN "0MvbrNQw5yj-KDbA-QZ73I4oXVQUV4_NE5mhluuZtVB9FzJIJ_6fscWunUVG0TVQ5RxxraCo3-yBzfHGdlG_bw=="
#define INFLUXDB_ORG "a4f7cc5900f2228e"       
#define INFLUXDB_BUCKET "EV"
#define TZ_INFO "WIB-7"

// --- 3. ขาเชื่อมต่อโมดูล CAN ---
#define CAN_TX_PIN GPIO_NUM_4  // CTX -> GPIO 4
#define CAN_RX_PIN GPIO_NUM_5  // CRX -> GPIO 5
#define LF_BASE_ID 0x4B0       // Base ID สำหรับล้อหน้าซ้าย Izze IRTS-V3

// --- 3.1 ตั้งค่า ESP-NOW ---
uint8_t receiverAddress[] = {0xF8, 0xB3, 0xB7, 0x3C, 0x1A, 0xFC}; 

typedef struct struct_message {
    int id;           // 2 = ล้อ FL
    float temp_FR;    // อุณหภูมิยางเฉลี่ย
} struct_message;

struct_message myData;

// Callback ตรวจสอบสถานะการส่งข้อมูล
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nESP-NOW Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

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

  // 1. เริ่มต้นเปิด Wi-Fi (เซ็ตโหมดรอก่อนเชื่อมต่อ)
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");

  // 2. ดึงค่า Channel ของ Wi-Fi ที่เชื่อมต่ออยู่จริงขึ้นมาใช้
  int32_t channel = WiFi.channel();
  Serial.print("Current Wi-Fi Channel: ");
  Serial.println(channel);

  // 3. เริ่มต้นใช้งาน ESP-NOW (รันหลังจากต่อ Wi-Fi สำเร็จ เพื่อป้องกันช่องสัญญาณขัดแย้งกัน)
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
  } else {
    // บังคับแปลง Type ให้ตรงระบบตรวจสอบไทป์เวอร์ชันใหม่ของ ESP32 Core
    esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
    
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo)); // ล้างค่าขยะที่อาจค้างอยู่ในเมมโมรี่ออกก่อน
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    
    peerInfo.channel = channel;  // 🔥 บังคับจับคู่โดยใช้ Channel เดียวกันกับ Router/Hotspot
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA; // บังคับใช้งานผ่าน Wi-Fi Interface STA
    
    esp_err_t addStatus = esp_now_add_peer(&peerInfo);
    if (addStatus != ESP_OK){
      Serial.print("Failed to add peer: ");
      Serial.println(addStatus);
    } else {
      Serial.println("✅ ESP-NOW Peer Added Successfully on the same channel!");
    }
  }

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

    // 💡 ส่งข้อมูลผ่าน ESP-NOW (คำนวณค่าเฉลี่ย 16 แชนเนลเพื่อส่งไปแสดงผลบนพวงมาลัย)
    float sum_temp = 0;
    for (int i = 0; i < 16; i++) {
      sum_temp += tire_temps[i];
    }
    float avg_temp = sum_temp / 16.0;

    myData.id = 2; // ID = 2 สำหรับบ่งบอกว่ามาจากล้อหน้าซ้าย (Front Left)
    myData.temp_FR = avg_temp; 

    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));
    if (result == ESP_OK) {
      Serial.print("ESP-NOW Sent Avg Temp: ");
      Serial.println(avg_temp);
    } else {
      Serial.print("ESP-NOW Error sending: ");
      Serial.println(result);
    }

    if (!client.writePoint(tireSensor)) {
      Serial.print("Influx Write Fail: ");
      Serial.println(client.getLastErrorMessage());
    } else {
      Serial.println("✅ Tire Temps Sent (Influx & Serial2)");
    }

    data_updated = false; 
  }
}