#include "driver/twai.h"

// --- ตั้งค่าขาเชื่อมต่อโมดูล CAN ---
#define CAN_TX_PIN GPIO_NUM_4  // CTX -> GPIO 4
#define CAN_RX_PIN GPIO_NUM_5  // CRX -> GPIO 5
#define LF_BASE_ID 0x4B0       // Base ID ล้อหน้าซ้าย (0x4B0 - 0x4B4)

// ตัวแปรนับจำนวนแพ็กเกจข้อมูลที่ได้รับจริงใน 1 วินาที
unsigned long packet_counts[5] = {0, 0, 0, 0, 0}; 
unsigned long last_print_time = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=============================================");
  Serial.println("🏎️ DONGTAAN RACING: CAN BUS DEBBUGGER ONLINE");
  Serial.println("=============================================");

  // ตั้งค่าและเริ่มทำงาน TWAI Driver (CAN Bus ของ ESP32) ความเร็ว 1 Mbit/s
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS(); 
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
    Serial.println("✅ CAN Bus (TWAI) Driver Started Successfully!");
  } else {
    Serial.println("❌ Failed to start CAN Bus Driver. Check your wiring!");
    while (1);
  }
}

void loop() {
  twai_message_t message;
  
  // คอยดักฟังบัฟเฟอร์สัญญาณ CAN Bus แบบไม่มีการหน่วงเวลา (0ms)
  if (twai_receive(&message, pdMS_TO_TICKS(0)) == ESP_OK) {
    
    // กรองเอาเฉพาะช่วง ID 0x4B0 ถึง 0x4B4
    if (!(message.rtr) && (message.identifier >= LF_BASE_ID) && (message.identifier <= LF_BASE_ID + 4)) {
      int msg_offset = message.identifier - LF_BASE_ID;
      
      if (msg_offset >= 0 && msg_offset <= 4) {
        // นับจำนวนครั้งที่ได้รับข้อมูลของแต่ละ ID
        packet_counts[msg_offset]++;
      }
    }
  }

  // ปริ้นท์รายงานสถานะสัญญาณทุกๆ 1 วินาที (1000ms)
  if (millis() - last_print_time >= 1000) {
    last_print_time = millis();
    
    Serial.println("\n📊 --- CAN Bus Receive Status (Last 1 Second) ---");
    
    // รายงานข้อมูลอุณหภูมิยางแต่ละกลุ่ม
    for (int i = 0; i < 4; i++) {
      Serial.printf("ID 0x%03X (Tire Ch %d-%d)   : Received %d packets\n", 
                    LF_BASE_ID + i, (i * 4) + 1, (i * 4) + 4, packet_counts[i]);
    }
    
    // รายงานข้อมูลอุณหภูมิบอร์ดภายในเซ็นเซอร์
    Serial.printf("ID 0x%03X (Internal Temp)  : Received %d packets\n", 
                  LF_BASE_ID + 4, packet_counts[4]);
    
    Serial.println("-------------------------------------------------");
    
    // รีเซ็ตตัวนับรอบใหม่
    for (int i = 0; i < 5; i++) {
      packet_counts[i] = 0;
    }
  }
}