#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "driver/twai.h"
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> 
#include <WiFiClientSecure.h> 
#include <PubSubClient.h>
#include <time.h>

// --- Config เดิมของเจมส์ ---
#define WIFI_SSID "Frankza"
#define WIFI_PASSWORD "frank0864793559"
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

#define CAN_TX_PIN GPIO_NUM_4  
#define CAN_RX_PIN GPIO_NUM_5  
#define LF_BASE_ID 0x4B0       

// --- Global Objects ---
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point tireSensor("tire_temperature");

uint8_t receiverAddress[] = {0xF8, 0xB3, 0xB7, 0x3C, 0x1A, 0xFC}; 
typedef struct struct_message {
    int id;           
    float temp_FR;    
} struct_message;
struct_message myData;

float tire_temps[16] = {0.0};
bool data_updated = false;
char mqtt_payload[256]; 
unsigned long lastMQTTRetry = 0;
bool time_synced = false;

// 💡 ฟังก์ชัน Sync เวลา (สำคัญมากสำหรับ SSL rc=-2)
void syncTime() {
  Serial.print("Syncing Time... ");
  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
  time_t now = time(nullptr);
  int retry = 0;
  while (now < 8 * 3600 * 2 && retry < 10) {
    delay(500);
    now = time(nullptr);
    retry++;
  }
  if (now > 8 * 3600 * 2) {
    Serial.println("✅ Time Synced!");
    time_synced = true;
  } else {
    Serial.println("❌ Time Sync Failed");
    time_synced = false;
  }
}

// 💡 ปรับเป็น Non-blocking (แก้ปัญหาบอร์ดค้างตอนเน็ตเน่า)
void maintainMQTT() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!time_synced) syncTime();
    
    if (!mqttClient.connected() && millis() - lastMQTTRetry > 5000) {
      lastMQTTRetry = millis();
      Serial.print("Attempting MQTT connection... ");
      String clientId = "DTR-TireFL-" + String(random(0, 0xffff), HEX);
      if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("✅ Connected!");
      } else {
        Serial.print("❌ rc=");
        Serial.println(mqttClient.state());
      }
    }
    mqttClient.loop();
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("ESP-NOW Send: ");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  delay(2000);

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting WiFi");
  while (wifiMulti.run() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n✅ WiFi Connected!");

  syncTime();
  espClient.setInsecure();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(512);

  // ESP-NOW
  if (esp_now_init() == ESP_OK) {
    esp_now_register_send_cb(OnDataSent);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = WiFi.channel(); // 💡 ใช้ channel เดียวกับ WiFi
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  // CAN Setup
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS(); 
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  twai_driver_install(&g_config, &t_config, &f_config);
  twai_start();

  tireSensor.addTag("wheel_position", "front_left");
}

void loop() {
  maintainMQTT();

  // รับค่า CAN
  twai_message_t message;
  if (twai_receive(&message, pdMS_TO_TICKS(2)) == ESP_OK) {
    if (!(message.rtr) && (message.identifier >= LF_BASE_ID) && (message.identifier <= LF_BASE_ID + 3)) {
      int msg_offset = message.identifier - LF_BASE_ID;
      for (int i = 0; i < 4; i++) {
        uint16_t raw_val = (message.data[i*2] << 8) | message.data[i*2 + 1];
        tire_temps[(msg_offset * 4) + i] = (raw_val * 0.1) - 100.0;
      }
      if (msg_offset == 3) data_updated = true;
    }
  }

  if (data_updated) {
    tireSensor.clearFields(); 
    int pos = 0;
    mqtt_payload[0] = '\0';

    for (int i = 0; i < 16; i++) {
      char field_name[10];
      snprintf(field_name, sizeof(field_name), "ch_%d", i + 1);
      tireSensor.addField(String(field_name), tire_temps[i]);
      pos += snprintf(mqtt_payload + pos, sizeof(mqtt_payload) - pos, "%.1f%s", 
                      tire_temps[i], (i < 15 ? "," : ""));
    }

    // 🚀 พ่นออก Serial2 (ลำดับความสำคัญสูงสุด)
    Serial2.print("$");
    Serial2.println(mqtt_payload); 

    // 🚀 MQTT Cloud
    if (mqttClient.connected()) {
      mqttClient.publish(MQTT_TOPIC, mqtt_payload);
    }

    // 🚀 ESP-NOW
    float sum_temp = 0;
    for (float t : tire_temps) sum_temp += t;
    myData.id = 2; 
    myData.temp_FR = sum_temp / 16.0; 
    esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));

    // 🚀 InfluxDB
    if (WiFi.status() == WL_CONNECTED) {
      client.writePoint(tireSensor);
    }

    data_updated = false; 
  }
}