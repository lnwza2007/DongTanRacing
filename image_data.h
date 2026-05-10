#ifndef IMAGE_DATA_H
#define IMAGE_DATA_H

#include <stdint.h>
typedef struct {
    const uint16_t *data;  // ชี้ไปที่ข้อมูลรูปภาพ
    uint16_t width;        // ความกว้างของรูป
    uint16_t height;       // ความสูงของรูป
} tImage;

extern const uint16_t image_data_Screenshot20250103021558[];

#endif // IMAGE_DATA_H