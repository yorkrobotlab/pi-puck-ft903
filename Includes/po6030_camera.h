#ifndef PO6030_CAMERA_H
#define PO6030_CAMERA_H

uint16_t po6030_init(void);
void po6030_start(void);
void po6030_stop(void);
void po6030_set(int resolution, int frame_rate, int format);

#endif // PO6030_CAMERA_H
