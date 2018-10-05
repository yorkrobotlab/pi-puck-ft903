#ifndef PO8030_CAMERA_H
#define PO8030_CAMERA_H

#include <stdint.h>

#define PO8030_PID 0x8030

uint16_t po8030_init(void);
void po8030_start(void);
void po8030_stop(void);
void po8030_set(int resolution, int frame_rate, int format);

#endif // PO8030_CAMERA_H
