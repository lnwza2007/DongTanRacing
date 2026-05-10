//                            USER DEFINED SETTINGS
//   Set driver type, fonts to be loaded, pins used and SPI control method etc.

#define USER_SETUP_INFO "User_Setup"

// ##################################################################################
// Section 1. Call up the right driver file and any options for it
// ##################################################################################

// 💡 เปิดไดรเวอร์สำหรับจอ 4.0 นิ้ว v1.1 ( ST7796S คือไดรเวอร์หลักของจอรุ่นนี้ )
#define ST7796_DRIVER     

// 💡 หากเจมส์คอมไพล์แล้วจอขาว ให้ลองใส่ // หน้า ST7796 แล้วมาเอา // หน้า ILI9488 ด้านล่างนี้ออกแทนครับ
//#define ILI9488_DRIVER     

// 💡 กำหนดขนาดพิกเซลพิกัดแนวตั้งมาตรฐานสำหรับจอ 4 นิ้ว (กว้าง 320 สูง 480)
// 💡 เมื่อรันคำสั่ง tft.setRotation(1) ในโค้ดหลัก มันจะพลิกกลับมาเป็นแนวนอน 480x320 ให้โดยอัตโนมัติครับ
#define TFT_WIDTH  320 
#define TFT_HEIGHT 480 

// ##################################################################################
// Section 2. Define the pins that are used to interface with the display here
// ##################################################################################

// 💡 ยึดตามชุดพินฮาร์ดแวร์เชื่อมต่อจริงที่เจมส์บัดกรีจัมพ์สายไว้บนบอร์ดเป๊ะๆ 
#define TFT_MISO 19   // (ปล่อยลอยไว้ได้)
#define TFT_MOSI 23   // ต่อเข้าขา SDI ของจอจริงบน PCB (ดังปี๊บ)
#define TFT_SCLK 18   // ต่อเข้าขา SCK ของจอจริงบน PCB (ดังปี๊บ)

#define TFT_CS    5   // 💡 ขา CS ต่อตรงเข้า GPIO 5
#define TFT_DC   17   // 💡 ขา D/C หรือ RS ต่อตรงเข้า GPIO 17 (TX2)
#define TFT_RST  -1   // 💡 ขา RST ของจอต่อเข้าปุ่มกด EN ของบอร์ดโดยตรง (-1 คือให้รีเซ็ตด้วยฮาร์ดแวร์ปุ่มกด)

// ##################################################################################
// Section 3. Define the fonts that are to be used here
// ##################################################################################

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font
#define LOAD_FONT2  // Font 2. Small 16 pixel high font
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font
#define LOAD_FONT6  // Font 6. Large 48 pixel font
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font
#define LOAD_FONT8  // Font 8. Large 75 pixel font
#define LOAD_GFXFF  // FreeFonts.

#define SMOOTH_FONT

// ##################################################################################
// Section 4. Other options
// ##################################################################################

// 💡 สำหรับจอ 4 นิ้ว SPI ความเร็ว 27MHz (27000000) ถือเป็นค่ามาตรฐานที่นิ่ง เสถียร และไม่เกิด Noise บนสายจัมพ์ง่ายครับ
#define SPI_FREQUENCY  27000000

// Optional reduced SPI frequency for reading TFT
#define SPI_READ_FREQUENCY  20000000

// The XPT2046 requires a lower SPI clock rate of 2.5MHz so we define that here:
#define SPI_TOUCH_FREQUENCY  2500000