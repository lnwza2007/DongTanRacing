#include <esp_now.h>
#include <WiFi.h>

typedef struct struct_message {
    int id;
    float temp_FR;
} struct_message;

struct_message incomingData;

// ตัวแปรเก็บค่าอุณหภูมิที่พร้อมนำไปแสดงผลบนหน้าจอ
float display_temp_FR = 0.0;

// ฟังก์ชันที่จะทำงานทันทีเมื่อได้รับข้อมูลจากบอร์ดเซ็นเซอร์ FR
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingDataPtr, int len) {
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  
  if (incomingData.id == 1) {
    display_temp_FR = incomingData.temp_FR;
    
    Serial.print("Received - FR Tire Temp: ");
    Serial.print(display_temp_FR);
    Serial.println(" C");
  }
}

void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

void loop() {
  // ค่าอุณหภูมิในตัวแปร display_temp_FR จะอัปเดตอัตโนมัติแบบ Real-time
  // คุณสามารถเขียนคำสั่งส่งค่านี้ออกจอที่ต่ออยู่กับบอร์ดพวงมาลัยได้เลยตรงนี้ครับ
  
  delay(10); 
}