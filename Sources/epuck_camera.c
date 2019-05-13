#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <ft900.h>

/* UART support for printf output. */
#include "tinyprintf.h"

#include "camera.h"
#include "epuck_camera.h"

#define CAMERA_DEBUG
#ifdef CAMERA_DEBUG
#define CAMERA_DEBUG_PRINTF(...) do {tfp_printf(__VA_ARGS__);} while (0)
#else
#define CAMERA_DEBUG_PRINTF(...)
#endif


uint16_t epuck_init(void)
{
	/* Wait a short period of time to allow camera to initialise. */
	delayms(10);
	return 0;
}

void epuck_start(void)
{
	CAMERA_DEBUG_PRINTF("Start streaming...\r\n");
}

void epuck_stop(void)
{
	CAMERA_DEBUG_PRINTF("...streaming stopped.\r\n");
}

int8_t epuck_supports(uint16_t width, uint16_t height, int8_t frame_rate, int8_t format)
{
	int8_t ret = -1;

	// Check the camera module supports the requested
	// resolution, frame rate and format. If it is not
	// supported then return an indicator.
	if (format == CAMERA_FORMAT_UNCOMPRESSED)
	{
		// VGA supports uncompressed and MJPEG at 15 fps.
		if ((width == CAMERA_FRAME_WIDTH_VGA)
				&& (height == CAMERA_FRAME_HEIGHT_VGA))
		{
			if ((frame_rate == 15) || (frame_rate == CAMERA_FRAME_RATE_ANY))
			{
				ret = 0;
			}
		}
	}

	return ret;
}

int8_t epuck_set(uint16_t width, uint16_t height, int8_t format,
		int8_t *frame_rate, uint16_t *sample_size, uint32_t *frame_size)
{
	int8_t ret = 0;

	CAMERA_DEBUG_PRINTF("epuck");

	if (format == CAMERA_FORMAT_UNCOMPRESSED)
	{
		CAMERA_DEBUG_PRINTF(" uncompressed");

		// VGA supports uncompressed at 15 fps.
		if ((width == CAMERA_FRAME_WIDTH_VGA)
				&& (height == CAMERA_FRAME_HEIGHT_VGA))
		{
			CAMERA_DEBUG_PRINTF(" VGA");
			if ((*frame_rate == 15) || (*frame_rate == CAMERA_FRAME_RATE_ANY))
			{
				CAMERA_DEBUG_PRINTF(" 15fps");
				// Sample size is 1 complete line - 1280 bytes.
				*sample_size = ((CAMERA_FRAME_WIDTH_VGA * EPUCK_BBP));
				*frame_size = CAMERA_FRAME_WIDTH_VGA * CAMERA_FRAME_HEIGHT_VGA * EPUCK_BBP;
				*frame_rate = 15;
				ret = 0;
			}
		}
	}

	CAMERA_DEBUG_PRINTF("\r\n");

	return ret;
}

