#include "esp_task_wdt.h"  // ── Disable watchdog during DMP calibration
#define WHITE_GREY 0xFFFE
#define BLACK      0x0000
#define RED        0xF800  
#define MAROON     0x7800
#define CYAN       0x07FF
#define YELLOW     0xFFE0
#define TFT_GREY   0x2104 
#define ILI9341_WHITE 0xFFFF
#define ILI9341_GREEN 0x07E0 // 💡 แก้ไขรหัสสีเขียว RGB565 จริง เพื่อไม่ให้ระบบวาดสีกราฟิกค้างขาว
#define GREEN2RED  4
#define OUTPUT_READABLE_YAWPITCHROLL

// 💡 ย้ายขา INTERRUPT_PIN หนีจากขา 2 (ไปขา 13) เพื่อปลดปล่อย GPIO 2 คืนให้ไฟสีฟ้าและหน้าจอ
#define INTERRUPT_PIN 13  
#define Slave_Address 0x08
///  ======================== library =============================  ///

#include "Alert.h"       // Out of range alert icon
#include "image_data.h"  // Out of range alert icon
#include "Dashboard.h"   // 💡 New beautiful 480×320 dark dashboard
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>
#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif
#define dacPin  25  // กำหนดขา DAC ที่ต้องการใช้ (GPIO 25)
#include <EEPROM.h>
MPU6050 mpu;
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library with default width and height
TFT_eSprite sprite = TFT_eSprite(&tft);  // สร้าง Sprite ที่เชื่อมต่อกับจอ

//------------------ Data -------------------------//
int BUFF ;
int RAMP_UP[3]       ;
int RAMP_DOWN[3]     ;
int SPEED_LIM[3]     ;

//-----------------------------------------------------//

int PRE_RAMP_UP[3]   ;
int PRE_RAMP_DOWN[3] ;
int PRE_SPEED_LIM[3] ;

// ================ Address EEPROM =====================//

const int address_RAMP_UP0      =   1 ;  // ตำแหน่งที่ต้องการเก็บข้อมูล in EEPROM
const int address_RAMP_UP1      =   2 ;
const int address_RAMP_UP2      =   3 ;
const int address_RAMP_DOWN0    =   4 ;
const int address_RAMP_DOWN1    =   5 ;
const int address_RAMP_DOWN2    =   6 ;
const int address_SPEED_LIM0    =   7 ;
const int address_SPEED_LIM1    =   8 ;
const int address_SPEED_LIM2    =   9 ;

// ==================  load bar ================================= //
// 💡 ปรับสเกลจุดโหลดบาร์กึ่งกลางจอ 4.0 นิ้วใหม่
const int position_X_bar      =   180  ;
const int position_Y_bar      =   240  ;
const int barLong         =   120  ;
const int barWidth        =   20  ;

// ==================  position drawBitImage ===================== //
// 💡 ปรับสเกลจุดรูปกึ่งกลางจอ 4.0 นิ้วใหม่
const int position_X      =   217  ;
const int position_Y      =   120  ;

// ================== define pin ================================= //

const int S2 = 35;          // CLK
const int S1 = 34;          // DT
const int key = 23;         // DT
const int buttonPin = 33;   // switch

// ================== parameter (int) ============================ //

int hundreds ;
int tens ;
int units ;
int Pre_Rotary ;
int lastValue_Rotary ;
int mappedRotary ;
int Speed  ;
int Pre_Speed = -1 ;
int func ;
int buttonState;            // สถานะของปุ่มปัจจุบัน
int lastButtonState = HIGH; // เก็บสถานะล่าสุดของปุ่ม
int mode = 0;               // ตัวแปรเก็บโหมดปัจจุบัน

// Theme color getter functions
uint16_t get_db_bg()    { return (mode == 2) ? 0x064D : 0x0841; }
uint16_t get_db_panel() { return (mode == 2) ? 0x054A : 0x1082; }
uint16_t get_db_white() { return (mode == 2) ? 0x0000 : 0xFFFF; }
uint16_t get_db_grey()  { return (mode == 2) ? 0x2104 : 0x528A; }

int mode1 = 0;
int reading ;
int read1 ;
int read2 ;
volatile int Rotary = 0; // Value to be incremented or decremented
unsigned long lastDebounceTime = 0 ;
unsigned long lastUpdateTime = 0;  // เก็บเวลาส่งข้อมูลครั้งล่าสุด
const unsigned long UpdateSpeed = 500;  // ช่วงเวลาการส่งข้อมูล (500ms)
long int potValue = 0;           // ค่าจากโพเทนมิเตอร์

// ================== parameter (float) ========================= //

float angle   ;
float angle1 = -1 ;

// ================== parameter (bool) ========================= //

bool Enter = false ;
bool blinkState = false;
bool show_data = true ;
bool lock = true;
bool PETCH = true ;
bool selectingMode = true; // สถานะว่ากำลังเลือกโหมดหรือไม่
bool selectingMode1 = true;
bool cancle = false ;
bool Petch = true ;
bool send_para = false ;

// ================= MPU control/status vars ================== //

bool dmpReady = false;    // set true if DMP init was successful
uint8_t mpuIntStatus;     // holds actual interrupt status byte from MPU
uint8_t devStatus;        // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;      // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;       // count of all bytes currently in FIFO
uint8_t fifoBuffer[64];   // FIFO storage buffer

// ================== value for i2c ========================== //

int receivedValue [10] ;

//---------------------- Function -----------------------------//

int lastPotValue = 0;
int modeMax = 3;            // จำนวนโหมดที่มี (1-3)
int previousMode = -1; // ตัวแปรเก็บค่า mode ก่อนหน้า
int previousMode1 = -1 ;
int xpos = 0, ypos = 5, gap = 4, radius = 52;
unsigned long lastInterruptTime = 0;  // To keep track of the last interrupt time


// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
// packet structure for InvenSense teapot demo
uint8_t teapotPacket[14] = { '$', 0x02, 0,0, 0,0, 0,0, 0,0, 0x00, 0x00, '\r', '\n' };
// ================================================================ //
//               INTERRUPT DETECTION ROUTINE                        //
// ================================================================ //
volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
    mpuInterrupt = true;
}
int d = 0; // Variable used for the sinewave test waveform
bool range_error = 0;
int8_t ramp = 1;
const int stepSize = 1; // Increment/Decrement step size

volatile byte lastState = 0; // Keep track of last state

// =================== Function Rotary ============================ //

void IRAM_ATTR readEncoder() {
    byte currentState = (digitalRead(S1) << 1) | digitalRead(S2); // Combine S1 and S2 as 2-bit value

    if ((lastState == 0b00 && currentState == 0b01) || 
        (lastState == 0b01 && currentState == 0b11) || 
        (lastState == 0b11 && currentState == 0b10) || 
        (lastState == 0b10 && currentState == 0b00)) {
        Rotary += stepSize; // Clockwise
    } else if ((lastState == 0b00 && currentState == 0b10) || 
               (lastState == 0b10 && currentState == 0b11) || 
               (lastState == 0b11 && currentState == 0b01) || 
               (lastState == 0b01 && currentState == 0b00)) {
        Rotary -= stepSize; // Counter-clockwise
    }
    lastState = currentState;
}

 
//****************************************************************************************************//

void setup(void) {
  // ── 💡 สเต็ปแก้บั๊กกดปุ่มรีเซ็ต: ถ่วงเวลารอระบบจ่ายไฟบอร์ดให้ตื่นเต็มตัวก่อนเริ่มบูต ──
  delay(1000);                       // รอให้ไฟเลี้ยงบนบอร์ดนิ่งสนิทก่อนเริ่มทำงาน
  Serial.begin(115200);
  delay(500);                        // รอให้ท่อรับส่งข้อมูล Serial บูตตัวเองสำเร็จ
  yield();                           // สลัดภาระงาน RTOS อื่น ๆ ออกไป
  Serial.println("\n=== BBPN EV CONTROLLER BOOT ===");

  // ── 💡 แก้ไข API Watchdog (ESP-IDF v5) ไม่ให้ตัวตรวจจับคิดว่าบอร์ดเอ๋อระหว่างรอ DMP คาลิเบรต
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 60000,           // ถ่างเวลาเฝ้าระวังเป็น 60 วินาที (จุใจ)
      .idle_core_mask = 0,
      .trigger_panic = false
  };
  esp_err_t wdt_err = esp_task_wdt_reconfigure(&wdt_config);
  if (wdt_err != ESP_OK) {
    Serial.print("WDT reconfigure warning: ");
    Serial.println(wdt_err);
  }

  tft.begin();
  db_init(tft);                      // 💡 Init dark dashboard (sets rotation, fills BG, draws chrome)
  
  // 💡 ปรับขยายขนาด Sprite และตั้งจุดหมุนกึ่งกลางจอ 4 นิ้วให้รับส่งสัมพันธ์กันเป๊ะๆ
  tft.setPivot(375, 160);            
  sprite.createSprite(170, 160);     // หดสเกลกล่องเพื่อวาดเฉพาะตัวเลขความเร็วให้กึ่งกลางคอลัมน์กลางพอดี
  sprite.setPivot(85, 80);           // ปักแกนหมุนตรงกลางกล่อง Sprite ของตัวสปีด (170/2, 160/2)
  sprite.setFreeFont(&FreeMono18pt7b);
  tft.setFreeFont(&FreeMono18pt7b);
      
    // join I2C bus (I2Cdev library doesn't do this automatically)
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
        Wire.setClock(100000); // 💡 บังคับล็อกท่อส่ง I2C ไว้ที่สปีดเสถียร 100kHz เพื่อให้เขียน DMP ผ่านง่ายๆ
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif

    // ── 💡 ระบบวิเคราะห์ I2C (Diagnostic Scanner) ค้นหาจุดหลวมบนตัวรถแข่ง ──
    Serial.println(F("Scanning I2C bus..."));
    int nDevices = 0;
    for (byte addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      byte error = Wire.endTransmission();
      if (error == 0) {
        Serial.print(F("  Found device at 0x"));
        if (addr < 16) Serial.print("0");
        Serial.println(addr, HEX);
        nDevices++;
      }
    }
    if (nDevices == 0) {
      Serial.println(F("  *** NO I2C devices found! Check SDA/SCL wiring ***"));
    } else {
      Serial.print(F("  Total: "));
      Serial.print(nDevices);
      Serial.println(F(" device(s)"));
    }
    Serial.println(F("──────────────────────────────────"));

    // initialize device
    Serial.println(F("Initializing I2C devices..."));
    mpu.initialize();
    pinMode(INTERRUPT_PIN, INPUT);
    delay(1500); // 💡 ปล่อยแรงดันไฟเลี้ยง I2C คลอตัวนิ่งสนิทก่อนเริ่มโหลดข้อมูลเฟิร์มแวร์
    // verify connection
    Serial.println(F("Testing device connections..."));
    Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));
    delay(1500);
    // load and configure the DMP
    Serial.println(F("Initializing DMP..."));
    devStatus = mpu.dmpInitialize();
    mpu.setXGyroOffset(220);
    mpu.setYGyroOffset(76);
    mpu.setZGyroOffset(-85);
    mpu.setZAccelOffset(1788); 
    if (devStatus == 0) {
        Serial.println(">>> devStatus = 0 : DMP init SUCCESS <<<");
        mpu.CalibrateAccel(6);
        mpu.CalibrateGyro(6);
        mpu.PrintActiveOffsets();
        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);
        Serial.print(F("Enabling interrupt detection (Arduino external interrupt "));
        Serial.print(digitalPinToInterrupt(INTERRUPT_PIN));
        Serial.println(F(")..."));
        attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();
        Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;
        Serial.print(">>> packetSize = "); Serial.println(mpu.dmpGetFIFOPacketSize());
        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }
// ======================= set pin input ===================================//

    pinMode(buttonPin , INPUT_PULLUP);  
    pinMode(S2, INPUT);
    pinMode(S1, INPUT);
    
// ================= Attach interrupt to S2 (CLK) ======================== //

    attachInterrupt(digitalPinToInterrupt(S2), readEncoder, CHANGE);
    Serial.println("Rotary Encoder Initialized");
    
// ======================================================================= //
    
    EEPROM_SET() ;
    HELLO();
    tft.setTextDatum(TL_DATUM);
    
}

void loop() {
  Main_task () ;
  delay(10); // ปรับลดดีเลย์หน่วงลูปใหญ่เพื่อเพิ่มความไวตอบสนองของตัว Rotary
  if(Pre_Rotary != Rotary) {
    dacWrite(dacPin, 0); 
    if (Pre_Rotary != Rotary) {
        dacWrite(dacPin, 0); 

        if (Rotary < -1) {
            handleRotaryBelowThreshold();
        } else if (Rotary > 1) {
            handleRotaryAboveThreshold();
        }

        Rotary = 0;
        Pre_Rotary = Rotary;
    }
}


//----------------------------------------------------------------------------------------//

  if(Petch == true){
  
  if(digitalRead(buttonPin) == 1){
          Pre_Speed = -1;    
          dacWrite(dacPin, 0); 
          Rotary = 0 ;
          while(digitalRead(buttonPin) == 1) ;
                  mode1 = 0;    
                  cancle = false ;
                  unsigned long previousMillis = millis() ;
                  int previousValue = -1 ;
                  int currentValue ;
                  db_drawChrome(tft);   // ✅ dark chrome
                  Pre_Speed = -1;
                  previousMode1 = -1 ;
                  sendCommandToSlave('S');
                  while (cancle == false)
                 {
                    if (Rotary>22) {
                      Rotary = 0 ;
                      }
                    if (Rotary<-15) {
                      Rotary = 0 ;
                      }    
                Serial.print("Value Rotary: ");
                Serial.print(Rotary);  
                if (Rotary >= 0 && Rotary <= 5) {
                    mode1 = 0;
                }
                else if (Rotary >= 7 && Rotary <= 13) {
                    mode1 = 1;
                }
                else if (Rotary >= 16 && Rotary <= 22) {
                    mode1 = 2;
                }
                else if(Rotary <= -3 && Rotary >= -8){
                    mode1 = 2 ;
                }
                else if(Rotary <= -9 && Rotary >= -14){
                    mode1 = 1 ;
                }
                Serial.print("Current mode: ");
                Serial.println(mode1);
                     if (mode1 != previousMode1)
                     {  
                        Set_Parameter();         
                        previousMode1 = mode1;   
                     }
        //---------------***********************************-------------------------//            
                     if (digitalRead(buttonPin) == 1)  //-----------
                          {
                            previousValue = -1 ;
                            delay(1);
                          if(digitalRead(buttonPin)==1 )
                              { Rotary = 0 ;
                                while(digitalRead(buttonPin) == 1) ;
                                selectingMode = true ;
                                selectingMode1 = true ;
                                while (selectingMode == true)
                                { 
                                  Serial.println("petch23");
                                  if (mode1 == 0)
                                  { 
                                    int value_X ;
                                    unsigned long previousMillis = millis();  
                                    int previousValue = -1 ;
                                    tft.fillScreen(TFT_BLACK);
                                    while (selectingMode1 == true)
                                    {  int mappedValue = map(Rotary, 0, 200, 0, 99);  
                                       value_X = Rotary ;
                                       Rotary = constrain(Rotary, -100, 100);
                                        Serial.println("Setting SpeedUp");
                                       if(value_X > 99 ) {
                                          value_X = 99 ;
                                        }  
                                                    
                                                                                      
                                        // เช็คว่าค่ามีการเปลี่ยนแปลงหรือไม่
                                        if (value_X != previousValue)
                                          { 
                                            if (mode == 0){
                                              PRE_RAMP_UP[0] = RAMP_UP[0] ;
                                              BUFF = PRE_RAMP_UP[0] + value_X ;
                                            if (BUFF>99){
                                              BUFF = 99 ;
                                            }
                                            if (BUFF<0){
                                              BUFF = 0 ;
                                             
                                            }
                                            SET_RAMP_UP(BUFF) ;
                                            }
                                            if (mode == 1){
                                              PRE_RAMP_UP[1] = RAMP_UP[1] ;
                                              BUFF = PRE_RAMP_UP[1] + value_X ;
                                            if (BUFF>99){
                                              BUFF = 99 ;
                                            }
                                            if (BUFF<0){
                                              BUFF = 0 ;
                                             
                                            }
                                            SET_RAMP_UP(BUFF) ;
                                            } 
                                            if (mode == 2){
                                            PRE_RAMP_UP[2] = RAMP_UP[2] ;
                                            BUFF = PRE_RAMP_UP[2] + value_X ;
                                            if (BUFF>99){
                                              BUFF = 99 ; 
                                            } 
                                            if (BUFF<0){
                                              BUFF = 0 ;
                                             
                                            }
                                            SET_RAMP_UP(BUFF) ;
                                            } 
                                            

                                            previousValue = value_X;  
                                            previousMillis = millis();     
                                          }
                                        PETCH = true ;
                                        unsigned long buttonPressStart = millis() ;   
                                        while(digitalRead(buttonPin) == 1 && PETCH == true ) {
                                          Serial.println("A") ;
                                          if (millis() - buttonPressStart >= 2000) {
                                              Serial.println("ทำงาน 1: ปุ่มถูกกดค้าง 3 วินาที");
                                              Enter = true ;
                                              PETCH = false ;
                                              if (mode == 0){
                                            
                                            RAMP_UP[0] = BUFF ;
                                            EEPROM.write(address_RAMP_UP0, RAMP_UP[0]);  
                                            EEPROM.commit();               
                                            
                                            }
                                            if (mode == 1){
                                            
                                            RAMP_UP[1] = BUFF ;
                                            EEPROM.write(address_RAMP_UP1, RAMP_UP[1]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            if (mode == 2){
                                              
                                            RAMP_UP[2] = BUFF ;
                                            EEPROM.write(address_RAMP_UP2, RAMP_UP[2]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            selectingMode1 = false;
                                            selectingMode = false;
                                            cancle = true ;
                                            db_drawSaving(tft);  // ✅ dark saving screen
                                            drawBitImage(position_X, position_Y, 46, 40, image_data_Screenshot20250103021558,2);
                                            delay(10);
                                            for (int i = 0; i <= 100; i++) {
                                            int fillWidth = map(i, 0, 100, 0, barLong);
                                            drawLoadingBar(position_X_bar, position_Y_bar, barLong, barWidth, fillWidth);
                                            delay(10); 
                                            }
                                            Data_to_ESPslave('D',func);
                                            delay(10);
                                            
                                          }else if(digitalRead(buttonPin) == 0){
                                              PETCH = false ;
                                              Serial.println("ทำงาน 2: ปุ่มถูกกดค้าง 3 วินาที");
                                              if (mode == 0){
                                            RAMP_UP[0] = BUFF ;
                                            EEPROM.write(address_RAMP_UP0, RAMP_UP[0]);  
                                            EEPROM.commit();               
                                            
                                            }
                                            if (mode == 1){
                                            
                                            RAMP_UP[1] = BUFF ;
                                            EEPROM.write(address_RAMP_UP1, RAMP_UP[1]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            if (mode == 2){
                                              
                                            RAMP_UP[2] = BUFF ;
                                            EEPROM.write(address_RAMP_UP2, RAMP_UP[2]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            selectingMode1 = false;
                                            selectingMode = false;
                                            }
                                        }  
                                        Serial.println("KKKK");
                                      
                                      
                                        if (millis() - previousMillis > 5000) 
                                          {
                                              selectingMode1 = false;  
                                              selectingMode = false;
                                              
                                          }
                                    }
                                                                                                        
                                  }   
                                  ////----------------------------------------------------------------------------------//                                                
                                  if (mode1 == 1)
                                  { 
                                    unsigned long previousMillis = millis();  
                                    int previousValue = -1 ;
                                    int value_X ;
                                    tft.fillScreen(TFT_BLACK);
                                    while (selectingMode1 == true)
                                    {  int mappedValue = map(Rotary, 0, 200, 0, 99);  
                                       value_X = Rotary ;
                                       Rotary = constrain(Rotary, -100, 100);
                                        Serial.println("Setting SpeedDown");
                                        if(value_X > 99 ) {
                                          value_X = 99 ;
                                        } 
                                        if (value_X != previousValue)
                                           { 
                                            if (mode == 0){
                                            PRE_RAMP_DOWN[0] = RAMP_DOWN[0] ;
                                            BUFF = PRE_RAMP_DOWN[0] + value_X ;
                                            if (BUFF>99){
                                              BUFF = 99 ;
                                            }
                                             if (BUFF<0){
                                              BUFF = 0 ;
                                             
                                            }
                                            SET_RAMP_DOWN(BUFF) ;
                                            }
                                            if (mode == 1){
                                            PRE_RAMP_DOWN[1] = RAMP_DOWN[1] ;
                                            BUFF = PRE_RAMP_DOWN[1] + value_X ;
                                            if (BUFF>99){
                                              BUFF = 99 ;
                                            }
                                            if (BUFF<0){
                                              BUFF = 0 ;
                                             
                                            }
                                            SET_RAMP_DOWN(BUFF) ;
                                            } 
                                            if (mode == 2){
                                            PRE_RAMP_DOWN[2] = RAMP_DOWN[2] ;
                                            BUFF = PRE_RAMP_DOWN[2] + value_X ;
                                            if (BUFF>99){
                                              BUFF = 99 ;
                                            } 
                                            if (BUFF<0){
                                              BUFF = 0 ;
                                             
                                            } 
                                            SET_RAMP_DOWN(BUFF) ;
                                            } 
                                            

                                            previousValue = value_X;  
                                            previousMillis = millis();     
                                          }
                                          PETCH = true ;
                                        unsigned long buttonPressStart = millis() ; 
                                        while(digitalRead(buttonPin) == 1 && PETCH == true ) {
                                          if (millis() - buttonPressStart >= 2000) {
                                              Serial.println("ทำงาน 1: ปุ่มถูกกดค้าง 3 วินาที");
                                              Enter = true ;
                                              PETCH = false ;
                                              if (mode == 0){
                                            
                                            RAMP_DOWN[0] = BUFF ;
                                            EEPROM.write(address_RAMP_DOWN0, RAMP_DOWN[0]);  
                                            EEPROM.commit();               
                                            
                                            }
                                            if (mode == 1){
                                            
                                            RAMP_DOWN[1] = BUFF ;
                                            EEPROM.write(address_RAMP_DOWN1, RAMP_DOWN[1]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            if (mode == 2){
                                              
                                            RAMP_DOWN[2] = BUFF ;
                                            EEPROM.write(address_RAMP_DOWN2, RAMP_DOWN[2]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            selectingMode1 = false;
                                            selectingMode = false;
                                            cancle = true ;
                                            db_drawSaving(tft);  
                                            drawBitImage(position_X, position_Y, 46, 40, image_data_Screenshot20250103021558,2);
                                            delay(10);
                                            for (int i = 0; i <= 100; i++) {
                                            int fillWidth = map(i, 0, 100, 0, barLong);
                                            drawLoadingBar(position_X_bar, position_Y_bar, barLong, barWidth, fillWidth);
                                            delay(10); 
                                            }
                                            Data_to_ESPslave('D',func);
                                            delay(10);
                                            
                                          }else if(digitalRead(buttonPin) == 0){
                                              PETCH = false ;
                                              if (mode == 0){
                                            RAMP_DOWN[0] = BUFF ;
                                            EEPROM.write(address_RAMP_DOWN0, RAMP_DOWN[0]);  
                                            EEPROM.commit();               
                                            
                                            }
                                            if (mode == 1){
                                            
                                            RAMP_DOWN[1] = BUFF ;
                                            EEPROM.write(address_RAMP_DOWN1, RAMP_DOWN[1]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            if (mode == 2){
                                              
                                            RAMP_DOWN[2] = BUFF ;
                                            EEPROM.write(address_RAMP_DOWN2, RAMP_DOWN[2]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            selectingMode1 = false;
                                            selectingMode = false;
                                            }
                                        }  
                                                                                    

                                        if (millis() - previousMillis > 5000) 
                                          {
                                              selectingMode1 = false;  
                                              selectingMode = false;
                                          }
                                    }
                                    
                                  }
                                  ////----------------------------------------------------------------------------------//  
                                  if (mode1 == 2)
                                  { 
                                    unsigned long previousMillis = millis();  
                                    int previousValue = -1 ;
                                    int value_X ;
                                    tft.fillScreen(TFT_BLACK);
                                    while (selectingMode1 == true)
                                    {  int mappedValue = map(Rotary, 0, 200, 0, 99);  
                                       value_X = Rotary ;
                                       Rotary = constrain(Rotary, -100, 100);
                                        Serial.println("Setting Limite_Speed");
                                        if(value_X > 99 ) {
                                           value_X = 99 ;
                                        }  
                                        if (value_X != previousValue)
                                          { 
                                            if (mode == 0){
                                            PRE_SPEED_LIM[0] = SPEED_LIM[0] ;
                                            BUFF = PRE_SPEED_LIM[0] + value_X ;
                                            if (BUFF>99){
                                              BUFF = 99 ;
                                            }
                                             if (BUFF<0){
                                              BUFF = 0 ;
                                             
                                            }
                                            LIM_SPEED(BUFF) ;
                                            }
                                            if (mode == 1){
                                            PRE_SPEED_LIM[1] = SPEED_LIM[1] ;
                                            BUFF = PRE_SPEED_LIM[1] + value_X ;
                                            if (BUFF>99){
                                              BUFF = 99 ;
                                            }
                                             if (BUFF<0){
                                              BUFF = 0 ;
                                             
                                            }
                                            LIM_SPEED(BUFF) ;
                                            } 
                                            if (mode == 2){
                                            PRE_SPEED_LIM[2] = SPEED_LIM[2] ;  
                                            BUFF = PRE_SPEED_LIM[2] + value_X ;
                                            if (BUFF>99){
                                              BUFF = 99 ;
                                            }
                                             if (BUFF<0){
                                              BUFF = 0 ;
                                             
                                            }
                                            LIM_SPEED(BUFF) ;
                                            } 
                                            

                                            previousValue = value_X;  
                                            previousMillis = millis();     
                                             
                                          }
                                           PETCH = true ;
                                        unsigned long buttonPressStart = millis() ; 
                                        while(digitalRead(buttonPin) == 1 && PETCH == true ) {
                                          Serial.println("A") ;
                                          if (millis() - buttonPressStart >= 2000) {
                                              Serial.println("ทำงาน 1: ปุ่มถูกกดค้าง 3 วินาที");
                                              Enter = true ;
                                              PETCH = false ;
                                              if (mode == 0){
                                            
                                            SPEED_LIM[0] = BUFF ;
                                            EEPROM.write(address_SPEED_LIM0, SPEED_LIM[0]);  
                                            EEPROM.commit();               
                                            
                                            }
                                            if (mode == 1){
                                            
                                            SPEED_LIM[1] = BUFF ;
                                            EEPROM.write(address_SPEED_LIM1, SPEED_LIM[1]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            if (mode == 2){
                                              
                                            SPEED_LIM[2] = BUFF ;
                                            EEPROM.write(address_SPEED_LIM2, SPEED_LIM[2]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            selectingMode1 = false;
                                            selectingMode = false;
                                            cancle = true ;
                                            db_drawSaving(tft);  
                                            drawBitImage(position_X, position_Y, 46, 40, image_data_Screenshot20250103021558,2);
                                            delay(10);
                                            for (int i = 0; i <= 100; i++) {
                                            int fillWidth = map(i, 0, 100, 0, barLong);
                                            drawLoadingBar(position_X_bar, position_Y_bar, barLong, barWidth, fillWidth);
                                            delay(10); 
                                            }
                                            Data_to_ESPslave('D',func);
                                            delay(10);
                                          }else if(digitalRead(buttonPin) == 0){
                                              PETCH = false ;
                                              Serial.println("ทำงาน 2: ปุ่มถูกกดค้าง 3 วินาที");
                                              if (mode == 0){
                                            
                                            SPEED_LIM[0] = BUFF ;
                                            EEPROM.write(address_SPEED_LIM0, SPEED_LIM[0]);  
                                            EEPROM.commit();               
                                            
                                            }
                                            if (mode == 1){
                                            
                                            SPEED_LIM[1] = BUFF ;
                                            EEPROM.write(address_SPEED_LIM1, SPEED_LIM[1]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            if (mode == 2){
                                              
                                            SPEED_LIM[2] = BUFF ;
                                            EEPROM.write(address_SPEED_LIM2, SPEED_LIM[2]);  
                                            EEPROM.commit();               
                                            
                                            } 
                                            selectingMode1 = false;
                                            selectingMode = false;
                                            }
                                        }  
                                                                                    

                                        if (millis() - previousMillis > 5000) 
                                          {
                                              selectingMode1 = false;  
                                              selectingMode = false;
                                          }
                                    }
                                    
                                  }
                                  ////----------------------------------------------------------------------------------// 
                                }
                                tft.fillScreen(DB_BG);
                              }
                            previousMode1 = -1 ;
                            mode1 = 0 ;
                            Rotary = 0 ;
                            
                          } //------------
                    
                       currentValue = Rotary ;
                    if (currentValue != previousValue)
                      { 
                        previousValue = currentValue;  
                        previousMillis = millis();     
                                          }
                    if (millis() - previousMillis > 5000)
                      {
                        sendCommandToSlave('C')  ; 
                        cancle = true ;
                      }
                 }  //------------mode 1 
          db_drawChrome(tft);   // ✅ dark chrome after param edit
          tft.setTextDatum(TL_DATUM);
          Pre_Rotary = Rotary ;
          Petch = false ;
          
      } ///- ------------ mode
 
  } else {
           if (digitalRead(buttonPin) == 1 ){
            Petch = false ;
          } else{
            Petch = true ;
          }
    }
}

void erasePreviousSprite(int x, int y) {
    sprite.fillSprite(TFT_RED);  
    sprite.pushSprite(x - sprite.width() / 2, y - sprite.height() / 2);  
}

uint16_t speedColor(int spd, int lim) {
  if (lim <= 0) return DB_CYAN;
  int pct = spd * 100 / lim;
  if (pct >= 85) return DB_RED;
  if (pct >= 60) return DB_ORANGE;
  return DB_CYAN;
}

// 💡 ปรับสเกลพิกัดวาดตัวหนังสือลงกึ่งกลาง Sprite 170x160 ตัวใหม่ เพื่อป้องกันขอบตัวอักษรแหว่ง
void drawRotatedText(String hundreds, String tens, String units, int x, int y, float angle, uint16_t col) {
  sprite.fillSprite(DB_BG);
  sprite.setTextSize(4); // ลดจาก 9 เหลือ 4 จะพอดีกับกรอบ 170x160 และตัวฟอนต์ FreeMono18pt
  sprite.setTextColor(col, DB_BG);
  sprite.setTextDatum(MC_DATUM); // บังคับอ้างอิงกึ่งกลางอักษรเป๊ะ ๆ
  
  // จัดตัวเลขหลักสิบและหลักหน่วยขนาบข้างกึ่งกลาง Sprite (X=85, Y=80) 
  sprite.drawString(tens, 45, 80);
  sprite.drawString(units, 125, 80);
  sprite.pushRotated(angle, DB_BG);  // DB_BG ทำหน้าที่คอยรีเฟรชถมทับสีรอบนอกกันขุยขยะค้าง
}

void drawRotatedText_TEN(String hundreds, String tens, String units, int x, int y, float angle, uint16_t col) {
  sprite.fillSprite(DB_BG);
  sprite.setTextSize(4);
  sprite.setTextColor(col, DB_BG);
  sprite.setTextDatum(MC_DATUM);
  
  sprite.drawString(tens, 50, 80);
  sprite.drawString(units, 120, 80);
  sprite.pushRotated(angle, DB_BG);
}

void drawRotatedText_UNIT(String units, int x, int y, float angle, uint16_t col) {
  sprite.fillSprite(DB_BG);
  sprite.setTextSize(4);
  sprite.setTextColor(col, DB_BG);
  sprite.setTextDatum(MC_DATUM);
  
  sprite.drawString(units, 85, 80); // ตัวเลขตัวเดียวให้อยู่กึ่งกลาง Sprite พอดีเป๊ะ
  sprite.pushRotated(angle, DB_BG);
}

void Data_to_ESPslave (char command ,int x) {
  Wire.beginTransmission(Slave_Address); 
  switch (x){ 
  case 0:
    Wire.write(command);
    Wire.write(x);
    Wire.write(RAMP_UP[x]); 
    Wire.write(RAMP_DOWN[x]); 
    Wire.write(SPEED_LIM[x]); 
    break ;
  case 1:
    Wire.write(command);
    Wire.write(x);
    Wire.write(RAMP_UP[x]); 
    Wire.write(RAMP_DOWN[x]); 
    Wire.write(SPEED_LIM[x]); 
    break ;
  case 2:
    Wire.write(command);
    Wire.write(x);
    Wire.write(RAMP_UP[x]); 
    Wire.write(RAMP_DOWN[x]); 
    Wire.write(SPEED_LIM[x]); 
    break ;
  }
  if (Wire.endTransmission() == 0) {
        Serial.println("Data sent successfully");
    } else {
        Serial.println("Failed to send data");
    }
  Serial.println("Parameters sent to Slave:");
  Serial.print("Command: "); Serial.println(command);
  Serial.print("Mode: "); Serial.println(x);
  Serial.print("Up: "); Serial.println(RAMP_UP[x]);
  Serial.print("Down: "); Serial.println(RAMP_UP[x]);
  Serial.print("Speed: "); Serial.println(RAMP_UP[x]);
  delay(50); 
}

void Read_from_ESPslave () {
  Wire.requestFrom(0x08, 2); 
  if (Wire.available() >= 2) {
     receivedValue[0] = Wire.read(); 
     receivedValue[1] = Wire.read(); 

    Serial.print("Received Value 1: ");
    Serial.println(receivedValue[0]);
    Serial.print("Received Value 2: ");
    Serial.println(receivedValue[1]);
  }else {
        Serial.println("No data available");
    }
}
void Set_Parameter() {
    db_drawModeSelect(tft, mode);  
}
void displayMode() {
    db_drawModeSelect(tft, mode);  
}

void SET_RAMP_UP(int value_speedup) {
  static int prevalue_speedup = -1;
  if (value_speedup != prevalue_speedup) {
    db_drawSetParam(tft, 0, value_speedup, mode);  
    prevalue_speedup = value_speedup;
  }
}

void SET_RAMP_DOWN(int value_speeddown) {
  static int prevalue_speeddown = -1;
  if (value_speeddown != prevalue_speeddown) {
    db_drawSetParam(tft, 1, value_speeddown, mode);  
    prevalue_speeddown = value_speeddown;
  }
}

void LIM_SPEED(int value_limite) {
  static int prevalue_value_limite = -1;
  if (value_limite != prevalue_value_limite) {
    db_drawSetParam(tft, 2, value_limite, mode);   
    prevalue_value_limite = value_limite;
  }
}

void EEPROM_SET(){
    EEPROM.begin(20);  
    RAMP_UP[0] = EEPROM.read(address_RAMP_UP0);
    RAMP_UP[1] = EEPROM.read(address_RAMP_UP1);
    RAMP_UP[2] = EEPROM.read(address_RAMP_UP2);
    RAMP_DOWN[0] = EEPROM.read(address_RAMP_DOWN0);
    RAMP_DOWN[1] = EEPROM.read(address_RAMP_DOWN1);
    RAMP_DOWN[2] = EEPROM.read(address_RAMP_DOWN2);
    SPEED_LIM[0] = EEPROM.read(address_SPEED_LIM0);
    SPEED_LIM[1] = EEPROM.read(address_SPEED_LIM1);
    SPEED_LIM[2] = EEPROM.read(address_SPEED_LIM2);
}
void HELLO() {
    db_drawHello(tft);  
    for(int i = 255 ; i > 0 ; i-- ){
      dacWrite(dacPin, i);
      delay(7);
    }
    for(int i = 0 ; i < 255 ; i++ ){
      dacWrite(dacPin, i);
      delay(3);
    }
    for(int i = 255 ; i > 0 ; i-- ){
      dacWrite(dacPin, i);
      delay(1);
    }
    for(int i = 0 ; i < 255 ; i++ ){
      dacWrite(dacPin, i);
      delay(1);
    }
    db_drawChrome(tft);  
}
void DOT(){
   tft.fillCircle(250, 270, 6, TFT_GREY);
   tft.fillCircle(230, 270, 6, TFT_GREY);
   tft.fillCircle(270, 270, 6, TFT_GREY);
}
int ringMeter(int value, int vmin, int vmax, int x, int y, int r, const char *units, byte scheme)
{
  x += r; y += r;   

  int w = r / 3;    
  
  int angle = 150;  

  int v = map(value, vmin, vmax, -angle, angle); 

  byte seg = 5; 
  byte inc = 10; 

  int colour = TFT_BLUE;
 
  for (int i = -angle+inc/2; i < angle-inc/2; i += inc) {
    float sx = cos((i - 90) * 0.0174532925);
    float sy = sin((i - 90) * 0.0174532925);
    uint16_t x0 = sx * (r - w) + x;
    uint16_t y0 = sy * (r - w) + y;
    uint16_t x1 = sx * r + x;
    uint16_t y1 = sy * r + y;

    float sx2 = cos((i + seg - 90) * 0.0174532925);
    float sy2 = sin((i + seg - 90) * 0.0174532925);
    int x2 = sx2 * (r - w) + x;
    int y2 = sy2 * (r - w) + y;
    int x3 = sx2 * r + x;
    int y3 = sy2 * r + y;

    if (i < v) { 
      switch (scheme) {
        case 0: colour = TFT_RED; break; 
        case 1: colour = TFT_GREEN; break; 
        case 2: colour = TFT_BLUE; break; 
        case 3: colour = rainbow(map(i, -angle, angle, 0, 127)); break; 
        case 4: colour = rainbow(map(i, -angle, angle, 70, 127)); break; 
        case 5: colour = rainbow(map(i, -angle, angle, 127, 63)); break; 
        default: colour = TFT_BLUE; break; 
      }
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, colour);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, colour);
    }
    else 
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_GREY);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_GREY);
    }
  }
  char buf[10];
  byte len = 3; if (value > 999) len = 5;
  dtostrf(value, len, 0, buf);
  buf[len] = ' '; buf[len+1] = 0; 
  tft.setTextSize(1);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextColor(colour, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  if (r > 84) {
    tft.setTextPadding(55*3); 
    tft.drawString(buf, x, y, 8); 
  }
  else {
    tft.setTextPadding(3 * 14); 
    tft.drawString(buf, x, y, 4); 
  }
  tft.setTextSize(1);
  tft.setTextPadding(0);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (r > 84) tft.drawString(units, x, y + 100, 4); 
  else tft.drawString(units, x, y + 15, 2); 

  return x + r;
}
unsigned int rainbow(byte value)
{
  byte red = 0; 
  byte green = 0;
  byte blue = 0; 

  byte quadrant = value / 32;

  if (quadrant == 0) {
    blue = 31;
    green = 2 * (value % 32);
    red = 0;
  }
  if (quadrant == 1) {
    blue = 31 - (value % 32);
    green = 63;
    red = 0;
  }
  if (quadrant == 2) {
    blue = 0;
    green = 63;
    red = value % 32;
  }
  if (quadrant == 3) {
    blue = 0;
    green = 63 - 2 * (value % 32);
    red = 31;
  }
  return (red << 11) + (green << 5) + blue;
}

float sineWave(int phase) {
  return sin(phase * 0.0174532925);
}
void drawAlert(int x, int y , int side, bool draw)
{
  if (draw && !range_error) {
    drawIcon(alert, x - alertWidth/2, y - alertHeight/2, alertWidth, alertHeight);
    range_error = 1;
  }
  else if (!draw) {
    tft.fillRect(x - alertWidth/2, y - alertHeight/2, alertWidth, alertHeight, TFT_BLACK);
    range_error = 0;
  }
}
#define BUFF_SIZE 64

void drawIcon(const unsigned short* icon, int16_t x, int16_t y, int8_t width, int8_t height) {

  uint16_t  pix_buffer[BUFF_SIZE];   

  tft.startWrite();
  tft.setAddrWindow(x, y, width, height);

  uint16_t nb = ((uint16_t)height * width) / BUFF_SIZE;

  for (int i = 0; i < nb; i++) {
    for (int j = 0; j < BUFF_SIZE; j++) {
      pix_buffer[j] = pgm_read_word(&icon[i * BUFF_SIZE + j]);
    }
    tft.pushColors(pix_buffer, BUFF_SIZE);
  }

  uint16_t np = ((uint16_t)height * width) % BUFF_SIZE;

  if (np) {
    for (int i = 0; i < np; i++) pix_buffer[i] = pgm_read_word(&icon[nb * BUFF_SIZE + i]);
    tft.pushColors(pix_buffer, np);
  }

  tft.endWrite();
}

void Main_task() {
  int DAC;
  unsigned long currentMillis = millis();

  if (currentMillis - lastUpdateTime >= UpdateSpeed) {
    get_speed();
    Serial.print("Speed : ");
    Serial.println(Speed);
    lastUpdateTime = currentMillis;
  }

  if (Speed > 99) Speed = 99;
  DAC = map(Speed, 0, 116, 0, 255);
  dacWrite(dacPin, DAC);

  bool gotPacket = false;
  if (dmpReady && mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {
    gotPacket = true;
    #ifdef OUTPUT_READABLE_YAWPITCHROLL
      mpu.dmpGetQuaternion(&q, fifoBuffer);
      mpu.dmpGetGravity(&gravity, &q);
      mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    #endif
  }

  float new_angle = ypr[1] * 180.0f / M_PI;
  // ── 💡 ระบบเอียงแบบสมูท (Smooth Rotation) สลับซ้าย-ขวา ──
  float target_angle = ypr[1] * -180.0f / M_PI; // สลับซ้ายขวาด้วยการใส่ลบ (-)
  
  // สร้าง Deadzone ตอนตั้งตรง (ถ้าเอียงนิดเดียวไม่ถึง 5 องศา ให้ถือว่าตรงเป๊ะ)
  if (target_angle > -5 && target_angle < 5) {
    target_angle = 0;
  }
  // จำกัดการเอียงสูงสุดที่ 45 องศา
  target_angle = constrain(target_angle, -45.0f, 45.0f);
  
  // สมูทตัวเลข: ค่อยๆ วิ่งเข้าหาเป้าหมาย (0.15 = ความหนืด/ความสมูท)
  angle1 += (target_angle - angle1) * 0.15f;

  Serial.print(gotPacket ? "[PKT] " : "[---] ");   
  Serial.print("dmpReady:");
  Serial.print(dmpReady ? "Y" : "N");
  Serial.print("  YAW:");
  Serial.print(ypr[0] * 57.2957795f, 1);
  Serial.print("  PITCH:");
  Serial.print(ypr[1] * 57.2957795f, 1);
  Serial.print("  ROLL:");
  Serial.print(ypr[2] * 57.2957795f, 1);
  Serial.print("  rawPitch_rad:");
  Serial.print(ypr[1], 4);
  Serial.print("  angle1:");
  Serial.print(angle1, 0);
  Serial.print("  SPD:");
  Serial.println(Speed);


  bool fullRedraw = db_update(tft,
            Speed,
            RAMP_UP[mode], RAMP_DOWN[mode], SPEED_LIM[mode],
            mode,
            ypr);

  static int   prevSpriteSpeed = -1;
  static float prevAngle1      = -999.0f;
  if (fullRedraw) { prevSpriteSpeed = -1; prevAngle1 = -999.0f; }  

  if (Speed != prevSpriteSpeed || abs(angle1 - prevAngle1) >= 1.0f) {
    // 💡 ขยับแกนวาด Sprite ขึ้นไปที่พิกัด 90 เพื่อดันตัวเลข 0 ขึ้นไปอีก
    tft.setPivot(375, 90);
    // ปรับความสูงของสี่เหลี่ยมดำที่ลบพื้นหลังให้สั้นลง (แค่ 170) จะได้ไม่ไปทับชื่อ Mode ด้านล่าง
    tft.fillRect(SPD_X + 1, BODY_Y + 1, SPD_W - 2, 170, DB_BG);

    uint16_t col = speedColor(Speed, SPEED_LIM[mode]);

    hundreds = Speed / 100;
    tens     = (Speed / 10) % 10;
    units    = Speed % 10;

    if (Speed <= 9) {
      drawRotatedText_UNIT(String(units), 0, 0, angle1, col);
    } else if (Speed < 20) {
      drawRotatedText_TEN(String(hundreds), String(tens), String(units), 0, 0, angle1, col);
    } else {
      drawRotatedText(String(hundreds), String(tens), String(units), 0, 0, angle1, col);
    }

    prevSpriteSpeed = Speed;
    prevAngle1      = angle1;
  }

  Pre_Speed = Speed;
}


// Handles when Rotary < -1
void handleRotaryBelowThreshold() {
    bool Enter = true;
    unsigned long Time = millis();
    previousMode = -1;
    Rotary = 0;
    db_drawModeSelect(tft, mode);  
    sendCommandToSlave('S');
    while (Enter) {
        if (Rotary < -20) {
            Rotary = 0;
        }

        if (Rotary <= -2 && Rotary >= -8) {
            mode = 0;
        } else if (Rotary <= -9 && Rotary >= -14) {
            mode = 1;
        } else if (Rotary <= -14 && Rotary >= -20) {
            mode = 2;
        }

        if (mode != previousMode) {
            displayMode();
            previousMode = mode;
            Time = millis();
        }

        if (digitalRead(buttonPin) == HIGH) {
            func = mode;
            Enter = false;
            while (digitalRead(buttonPin) == HIGH);
            db_drawSaving(tft);  
            drawBitImage(position_X, position_Y, 46, 40, image_data_Screenshot20250103021558,2);
            for (int i = 0; i <= 100; i++) {
            int fillWidth = map(i, 0, 100, 0, barLong);
            drawLoadingBar(position_X_bar, position_Y_bar, barLong, barWidth, fillWidth);
              delay(10); 
            }
            
            Data_to_ESPslave('D',func);
            db_drawChrome(tft);
            db_invalidate();  
            Time = millis();
            Pre_Speed = -1 ;
            
        }

        if (millis() - Time > 3000) {
            Enter = false;
            sendCommandToSlave('C')  ; 
            db_drawChrome(tft);
            db_invalidate();  
            Pre_Speed = -1 ;
        }
    }
}

// Handles when Rotary > 1
void handleRotaryAboveThreshold() {
    db_drawChrome(tft);  
    unsigned long Time = millis();
    bool Enter = true, Show = true;

    while (Enter) {
        if (Show) {
            updateDisplay(func);
            Show = false;
        }

        if (millis() - Time > 2000) {
            Enter = false;
             Pre_Speed = -1 ;
            db_drawChrome(tft);
            db_invalidate();  
        }
    }
}

void updateDisplay(int func) {
    if (func >= 0 && func < 3) {
        db_drawChrome(tft);
        db_drawParams(tft, RAMP_UP[func], RAMP_DOWN[func], SPEED_LIM[func], func);
    }
}
void sendCommandToSlave(char command) {
  Wire.beginTransmission(Slave_Address);
  Wire.write(command);
  Wire.endTransmission();
  Serial.print("Command sent to Slave: ");
  Serial.println(command);
}
void requestSlaveReady() {
  unsigned long startTime = millis(); 
  unsigned long timeout = 3000; 
  bool success = false;

  while (millis() - startTime < timeout && !success) { 
  
    Wire.requestFrom(Slave_Address, 1); 
      delay(1000) ;
    if (Wire.available()) { 
    
      char response = Wire.read(); 
      Serial.println(response);
      if (response == 'R') { 
        Serial.println("Slave is ready");
        success = true; 
      } else {
        Serial.println("Slave not ready");
      }
    } else {
      Serial.println("No response from Slave");
    }

    if (!success) {
      delay(50); 
    }
  }

  if (!success) {
    Serial.println("Failed to communicate with Slave after 5 seconds.");
    
  } else {
   Serial.println("Proceeding to the next task...");
   send_para = true ;
  }
}
void drawBitImage(int x, int y, int width, int height, const uint16_t *imageData, int scale) {
  int byteIndex = 0;
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      uint16_t color = imageData[byteIndex];

      for (int dy = 0; dy < scale; dy++) {
        for (int dx = 0; dx < scale; dx++) {
          tft.drawPixel(x + col * scale + dx, y + row * scale + dy, color);
        }
      }

      byteIndex++;
    }
  }
}
void drawLoadingBar(int x, int y, int width, int height, int fillWidth) {
  tft.fillRoundRect(x+2, y+2, fillWidth-4, height-4, 3, ILI9341_GREEN);  
  tft.drawRoundRect(x, y, width, height, 5, BLACK);  
}

void get_speed (){
  Wire.requestFrom(Slave_Address, 1);
  if (Wire.available()) { 
    Speed = Wire.read() ;
  }
}


/*
 * Copyright (c) 2004, 2005 by
 * Ralf Corsepius, Ulm/Germany. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software
 * is freely granted, provided that this notice is preserved.
 */

#ifndef _SYS__STDINT_H
#define _SYS__STDINT_H

#include <machine/_default_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ___int8_t_defined
#ifndef _INT8_T_DECLARED
typedef __int8_t int8_t ;
#define _INT8_T_DECLARED
#endif
#ifndef _UINT8_T_DECLARED
typedef __uint8_t uint8_t ;
#define _UINT8_T_DECLARED
#endif
#define __int8_t_defined 1
#endif /* ___int8_t_defined */

#ifdef ___int16_t_defined
#ifndef _INT16_T_DECLARED
typedef __int16_t int16_t ;
#define _INT16_T_DECLARED
#endif
#ifndef _UINT16_T_DECLARED
typedef __uint16_t uint16_t ;
#define _UINT16_T_DECLARED
#endif
#define __int16_t_defined 1
#endif /* ___int16_t_defined */

#ifdef ___int32_t_defined
#ifndef _INT32_T_DECLARED
typedef __int32_t int32_t ;
#define _INT32_T_DECLARED
#endif
#ifndef _UINT32_T_DECLARED
typedef __uint32_t uint32_t ;
#define _UINT32_T_DECLARED
#endif
#define __int32_t_defined 1
#endif /* ___int32_t_defined */

#ifdef ___int64_t_defined
#ifndef _INT64_T_DECLARED
typedef __int64_t int64_t ;
#define _INT64_T_DECLARED
#endif
#ifndef _UINT64_T_DECLARED
typedef __uint64_t uint64_t ;
#define _UINT64_T_DECLARED
#endif
#define __int64_t_defined 1
#endif /* ___int64_t_defined */

#ifndef _INTMAX_T_DECLARED
typedef __intmax_t intmax_t;
#define _INTMAX_T_DECLARED
#endif

#ifndef _UINTMAX_T_DECLARED
typedef __uintmax_t uintmax_t;
#define _UINTMAX_T_DECLARED
#endif

#ifndef _INTPTR_T_DECLARED
typedef __intptr_t intptr_t;
#define _INTPTR_T_DECLARED
#endif

#ifndef _UINTPTR_T_DECLARED
typedef __uintptr_t uintptr_t;
#define _UINTPTR_T_DECLARED
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS__STDINT_H */