#ifndef PO6030_CAMERA_H
#define PO6030_CAMERA_H

#include <stdint.h>

#define PO6030_PID 0x6030

uint16_t po6030_init(void);
void po6030_start(void);
void po6030_stop(void);
void po6030_set(int resolution, int frame_rate, int format);

#endif // PO6030_CAMERA_H
