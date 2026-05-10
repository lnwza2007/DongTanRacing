#include "driver/twai.h"

// --- กำหนดขาเชื่อมต่อกับ SN65HVD230 ---
#define CAN_TX_PIN GPIO_NUM_4  // CTX ต่อเข้าขา 4 ของ ESP32
#define CAN_RX_PIN GPIO_NUM_5  // CRX ต่อเข้าขา 5 ของ ESP32

// --- กำหนดค่า Base ID สำหรับล้อหน้าซ้าย (LF: 0x4B0) ---
#define LF_BASE_ID 0x4B0

// ตัวแปรเก็บค่าอุณหภูมิยางทั้ง 16 จุด (Channels) และอุณหภูมิในตัวเซ็นเซอร์ (Sensor Temp)
float tire_temps[16] = {0.0};
float internal_sensor_temp = 0.0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- Dongtaan Racing: Tire Temp Telemetry ---");

  // 1. ตั้งค่าคอนฟิกสำหรับ TWAI Driver (CAN Bus)
  // ความเร็วบัสของเซ็นเซอร์ Izze-Racing คือ 1 Mbit/s (TWAI_TIMING_CONFIG_1MBITS)
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); // รับทุก ID ก่อนเพื่อทดสอบ

  // 2. ติดตั้ง TWAI Driver
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("✅ TWAI Driver Installed");
  } else {
    Serial.println("❌ Failed to install TWAI Driver");
    while (1);
  }

  // 3. เริ่มต้นทำงานระบบ CAN Bus
  if (twai_start() == ESP_OK) {
    Serial.println("✅ CAN Bus Started! Listening for Izze IRTS-V3...");
  } else {
    Serial.println("❌ Failed to start CAN Bus");
    while (1);
  }
}

void loop() {
  twai_message_t message;
  
  // รอรับข้อมูลจาก CAN Bus (Timeout 10ms เพื่อไม่ให้บอร์ดค้าง)
  if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    
    // ตรวจสอบว่าข้อมูลเป็น Standard Frame และช่วง ID ตรงกับล้อหน้าซ้าย (0x4B0 - 0x4B4)
    if (!(message.rtr) && (message.identifier >= LF_BASE_ID) && (message.identifier <= LF_BASE_ID + 4)) {
      
      int msg_offset = message.identifier - LF_BASE_ID; // หาว่ามาจาก ID ย่อยตัวไหน (0 ถึง 4)

      if (msg_offset >= 0 && msg_offset <= 3) {
        // ถอดรหัสอุณหภูมิผิวหน้ายาง 4 จุดต่อ 1 Message (16-bit Big-Endian / Motorola)
        for (int i = 0; i < 4; i++) {
          int byte_idx = i * 2;
          int channel_idx = (msg_offset * 4) + i; // จุดที่ 0 ถึง 15

          // รวม Byte สูง (MSB) และ Byte ต่ำ (LSB)
          uint16_t raw_val = (message.data[byte_idx] << 8) | message.data[byte_idx + 1];
          
          // แปลงค่าตามสูตรของดาต้าชีท: Temp = (Raw * 0.1) - 100
          tire_temps[channel_idx] = (raw_val * 0.1) - 100.0;
        }
      } 
      else if (msg_offset == 4) {
        // ID 0x4B4 เก็บค่าอุณหภูมิภายในตัวเซ็นเซอร์เอง (Internal Sensor Temperature)
        uint16_t raw_internal = (message.data[0] << 8) | message.data[1];
        internal_sensor_temp = (raw_internal * 0.1) - 100.0;
        
        // เมื่อได้รับ Message ครบชุด (0x4B4 เป็นตัวปิดท้าย) สั่ง Print ข้อมูลออกหน้าจอทันที
        printTireTemps();
      }
    }
  }
}

// ฟังก์ชันสำหรับแสดงผลหน้าจอ Serial Monitor
void printTireTemps() {
  Serial.print("\n>>> LF Tire Surface Temps [16 Chs]: ");
  for (int i = 0; i < 16; i++) {
    Serial.print(tire_temps[i], 1);
    Serial.print("°C ");
  }
  Serial.printf("\n>>> Internal Sensor Temp: %.1f°C\n", internal_sensor_temp);
  Serial.println("--------------------------------------------------------------------------------");
}