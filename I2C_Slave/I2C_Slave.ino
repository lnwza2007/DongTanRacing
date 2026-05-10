#include <Wire.h>
#include <LoRa.h>
#include <SPI.h>

#define I2C_SLAVE_ADDRESS 0x08
#define LoRa_CS 2
#define LoRa_rst 16
#define LoRa_dio 4 

int AccX, AccY, AccZ, TEMP, Speed;

void setup() {
  Wire.begin(I2C_SLAVE_ADDRESS); // เริ่มต้น I2C ในโหมด slave
  Wire.onReceive(receiveEvent); // ตั้งค่าฟังก์ชัน callback เมื่อมีข้อมูลเข้ามา
  Wire.onRequest(requestEvent); // ตั้งค่าฟังก์ชัน callback เมื่อ Master ขอข้อมูล
  Serial.begin(115200);
  Serial.println("I2C Slave Initialized");
  
  // Setup LoRa
  pinMode(LoRa_CS, OUTPUT);
  digitalWrite(LoRa_CS, HIGH);  // Ensure LoRa CS is not active
  SPI.begin();
  LoRa.setPins(LoRa_CS, LoRa_rst, LoRa_dio);
  
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  
  LoRa.setSpreadingFactor(12);
  LoRa.setTxPower(20);
  LoRa.setCodingRate4(5);
  LoRa.setSignalBandwidth(250E3);
}

void loop() {
  static int counter = 0;

  // ตัวอย่างการอัปเดตค่า AccX, AccY, AccZ, TEMP, Speed
  // คุณอาจจะได้มาโดยการอ่านเซ็นเซอร์ต่างๆ หรือคำนวณจากข้อมูลอื่น
  AccX = analogRead(34);  // ตัวอย่างการอ่านค่าจากเซ็นเซอร์ (เปลี่ยนตามที่คุณต้องการ)
  AccY = analogRead(35);
  AccZ = analogRead(32);
  TEMP = analogRead(33);
  Speed = counter;  // ตัวอย่างการใช้ counter เป็น Speed

  // ส่งข้อมูลผ่าน LoRa
  Serial.print("Sending packet: ");
  Serial.println(counter);

  digitalWrite(LoRa_CS, LOW);  // Activate LoRa CS
  LoRa.beginPacket();
  LoRa.print("hello world I'm Peter Parker ");
  LoRa.print(counter);
  LoRa.endPacket();
  
  counter++;
  delay(1000); // รอ 1 วินาทีก่อนส่งข้อมูลอีกครั้ง
}

// ฟังก์ชัน callback เมื่อมีข้อมูลเข้ามา
void receiveEvent(int howMany) {
  Serial.print("Received data: ");
  if (howMany == 4) { // ตรวจสอบว่ามีข้อมูลเข้ามา 5 ไบต์
    AccX = Wire.read(); // อ่านข้อมูลจาก master
    AccY = Wire.read(); 
    AccZ = Wire.read(); 
    TEMP = Wire.read(); 
    //Speed = Wire.read(); 
    
    Serial.print("Received: AccX=");
    Serial.print(AccX);
    Serial.print(", AccY=");
    Serial.print(AccY);
    Serial.print(", AccZ=");
    Serial.print(AccZ);
    Serial.print(", TEMP=");
    Serial.print(TEMP);
    //Serial.print(", Speed=");
    //Serial.print(Speed);
    //Serial.println();
  }
}

// ฟังก์ชัน callback เมื่อ Master ขอข้อมูล
void requestEvent() {
  // ส่งค่าที่ต้องการกลับไปยัง Master
  Wire.write(AccX);  // ส่ง AccX
  Wire.write(AccY);  // ส่ง AccY
  Wire.write(AccZ);  // ส่ง AccZ
  Wire.write(TEMP);  // ส่ง TEMP
  //Wire.write(Speed);  // ส่ง Speed
}
