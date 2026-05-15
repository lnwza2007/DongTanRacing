#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "driver/twai.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> 
#include <WiFiClientSecure.h> 
#include <PubSubClient.h>

// --- 1. ตั้งค่า WiFi ---
#define WIFI_SSID "Frankza"
#define WIFI_PASSWORD "frank0864793559"

// --- 2. ข้อมูล InfluxDB ---
#define INFLUXDB_URL "http://192.168.1.8:8086"  
#define INFLUXDB_TOKEN "0MvbrNQw5yj-KDbA-QZ73I4oXVQUV4_NE5mhluuZtVB9FzJIJ_6fscWunUVG0TVQ5RxxraCo3-yBzfHGdlG_bw=="
#define INFLUXDB_ORG "a4f7cc5900f2228e"       
#define INFLUXDB_BUCKET "EV"
#define TZ_INFO "WIB-7"

#define MQTT_SERVER     "2898b29c070f4985b025bbc1d2e1d216.s1.eu.hivemq.cloud" 
#define MQTT_PORT       8883                                   
#define MQTT_USER       "dongtaan_vcu"
#define MQTT_PASSWORD   "Frank2007"
#define MQTT_TOPIC      "balone2/telemetry/tire_fl"       

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// --- 3. ขาเชื่อมต่อโมดูล CAN ---
#define CAN_TX_PIN GPIO_NUM_4  
#define CAN_RX_PIN GPIO_NUM_5  
#define LF_BASE_ID 0x4B0       

// --- 3.1 ตั้งค่า ESP-NOW ---
uint8_t receiverAddress[] = {0xF8, 0xB3, 0xB7, 0x3C, 0x1A, 0xFC}; 

typedef struct struct_message {
    int id;           
    float temp_FR;    
} struct_message;

struct_message myData;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nESP-NOW Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// --- 4. ตัวแปรเก็บค่า ---
float tire_temps[16] = {0.0};
bool data_updated = false;

WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point tireSensor("tire_temperature");

// 💡 จองแรมไว้ขนาดคงที่ (256 bytes) เพื่อแก้ปัญหา Memory Leak 100%
char mqtt_payload[256]; 

// ฟังก์ชันรักษาท่อเชื่อมต่อคลาวด์ MQTT (ลอจิกเดิม)
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-TireFL-" + String(random(0, 0xffff), HEX);
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("✅ Connected to HiveMQ Cloud Broker!");
    } else {
      Serial.print("❌ Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying in 2 seconds...");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
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

  int32_t channel = WiFi.channel();
  Serial.print("Current Wi-Fi Channel: ");
  Serial.println(channel);

  espClient.setInsecure();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
  } else {
    esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo)); 
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = channel;  
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA; 
    
    esp_err_t addStatus = esp_now_add_peer(&peerInfo);
    if (addStatus != ESP_OK){
      Serial.print("Failed to add peer: ");
      Serial.println(addStatus);
    } else {
      Serial.println("✅ ESP-NOW Peer Added Successfully!");
    }
  }

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  if (client.validateConnection()) {
    Serial.println("✅ InfluxDB Local Connected!");
  } else {
    Serial.print("❌ InfluxDB Connection Fail: ");
    Serial.println(client.getLastErrorMessage());
  }

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
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  if (wifiMulti.run() != WL_CONNECTED) {
    delay(1000);
    return;
  }

  twai_message_t message;
  
  if (twai_receive(&message, pdMS_TO_TICKS(5)) == ESP_OK) {
    if (!(message.rtr) && (message.identifier >= LF_BASE_ID) && (message.identifier <= LF_BASE_ID + 3)) {
      int msg_offset = message.identifier - LF_BASE_ID;

      for (int i = 0; i < 4; i++) {
        int byte_idx = i * 2;
        int channel_idx = (msg_offset * 4) + i;

        uint16_t raw_val = (message.data[byte_idx] << 8) | message.data[byte_idx + 1];
        float temp_val = (raw_val * 0.1) - 100.0;
        
        if (temp_val >= -50.0 && temp_val <= 200.0) {
          tire_temps[channel_idx] = temp_val;
        }
      }

      if (msg_offset == 3) {
        data_updated = true; 
      }
    }
  }

  if (data_updated) {
    tireSensor.clearFields(); 
    int pos = 0;
    mqtt_payload[0] = '\0'; // ล้างข้อมูลเก่าทิ้ง

    // 💡 เปลี่ยนการบวก String รัวๆ มาใช้ snprintf เขียนลง Memory ก้อนเดียว
    for (int i = 0; i < 16; i++) {
      char field_name[10];
      snprintf(field_name, sizeof(field_name), "ch_%d", i + 1);
      tireSensor.addField(String(field_name), tire_temps[i]);
      
      // แพ็กข้อมูลตัวเลขและลูกน้ำลงใน mqtt_payload
      pos += snprintf(mqtt_payload + pos, sizeof(mqtt_payload) - pos, "%.1f%s", 
                      tire_temps[i], (i < 15 ? "," : ""));
    }

    // 🚀 ส่งข้อมูลขึ้น Cloud 
    if (mqttClient.connected()) {
      mqttClient.publish(MQTT_TOPIC, mqtt_payload);
    }

    // 🚀 พ่นออกพอร์ต Serial2 ไปบอร์ดหลัก
    Serial2.print("$");
    Serial2.println(mqtt_payload); 

    // ส่งค่าเฉลี่ยผ่าน ESP-NOW
    float sum_temp = 0;
    for (int i = 0; i < 16; i++) {
      sum_temp += tire_temps[i];
    }
    float avg_temp = sum_temp / 16.0;

    myData.id = 2; 
    myData.temp_FR = avg_temp; 

    esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));

    // บันทึก InfluxDB
    if (!client.writePoint(tireSensor)) {
      Serial.print("Influx Write Fail: ");
      Serial.println(client.getLastErrorMessage());
    } else {
      Serial.println("✅ Tire Temps Dispatched to Cloud & Local DB!");
    }

    data_updated = false; 
  }
}