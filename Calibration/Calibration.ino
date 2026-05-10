#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  
  if (!ads.begin()) {
    Serial.println("ADS1115 Fail!");
    while(1);
  }
  
  // ใช้ Gain นี้เพื่อให้วัดได้ถึง 5V (4.096V หรือ 6.144V แล้วแต่รุ่น)
  ads.setGain(GAIN_TWOTHIRDS); 
  
  Serial.println("--- Malasta I Calibration Mode ---");
  Serial.println("1. ปล่อยก้านให้สุด (0mm) แล้วจดค่า Volts");
  Serial.println("2. ดันก้านให้สุด (75mm) แล้วจดค่า Volts");
  Serial.println("---------------------------------");
}

void loop() {
  int16_t adc0 = ads.readADC_SingleEnded(0);
  float volts = ads.computeVolts(adc0);
  
  // แสดงค่าดิบและแรงดัน
  Serial.print("Raw ADC: "); Serial.print(adc0);
  Serial.print(" | Volts: "); Serial.println(volts, 4); // ทศนิยม 4 ตำแหน่งไปเลยเจมส์
  
  delay(200);
}