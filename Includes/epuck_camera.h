#ifndef SOURCES_EPUCK_CAMERA_H_
#define SOURCES_EPUCK_CAMERA_H_

/**
 @brief Format Bytes Per Pixel definition for camera uncompressed output.
 @details Derived from the image type.
 */
#define EPUCK_BBP (16 >> 3)

uint16_t epuck_init(void);
void epuck_start(void);
void epuck_stop(void);
int8_t epuck_supports(uint16_t width, uint16_t height, int8_t frame_rate, int8_t format);
int8_t epuck_set(uint16_t width, uint16_t height, int8_t format,
		int8_t *frame_rate, uint16_t *sample, uint32_t *frame);

#endif /* SOURCES_EPUCK_CAMERA_H_ */
