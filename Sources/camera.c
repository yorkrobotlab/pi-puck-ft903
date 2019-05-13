/**
  @file camera.c
 */
/*
 * ============================================================================
 * History
 * =======
 * 2017-03-15 : Created
 *
 * (C) Copyright Bridgetek Pte Ltd
 * ============================================================================
 *
 * This source code ("the Software") is provided by Bridgetek Pte Ltd
 * ("Bridgetek") subject to the licence terms set out
 * http://www.ftdichip.com/FTSourceCodeLicenceTerms.htm ("the Licence Terms").
 * You must read the Licence Terms before downloading or using the Software.
 * By installing or using the Software you agree to the Licence Terms. If you
 * do not agree to the Licence Terms then do not download or use the Software.
 *
 * Without prejudice to the Licence Terms, here is a summary of some of the key
 * terms of the Licence Terms (and in the event of any conflict between this
 * summary and the Licence Terms then the text of the Licence Terms will
 * prevail).
 *
 * The Software is provided "as is".
 * There are no warranties (or similar) in relation to the quality of the
 * Software. You use it at your own risk.
 * The Software should not be used in, or for, any medical device, system or
 * appliance. There are exclusions of Bridgetek liability for certain types of loss
 * such as: special loss or damage; incidental loss or damage; indirect or
 * consequential loss or damage; loss of income; loss of business; loss of
 * profits; loss of revenue; loss of contracts; business interruption; loss of
 * the use of money or anticipated savings; loss of information; loss of
 * opportunity; loss of goodwill or reputation; and/or loss of, damage to or
 * corruption of data.
 * There is a monetary cap on Bridgetek's liability.
 * The Software may have subsequently been amended by another user and then
 * distributed by that other user ("Adapted Software").  If so that user may
 * have additional licence terms that apply to those amendments. However, Bridgetek
 * has no liability in relation to those amendments.
 * ============================================================================
 */

/* INCLUDES ************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <ft900.h>

#include "camera.h"
#include "tinyprintf.h"

#define CAMERA_DEBUG
#ifdef CAMERA_DEBUG
#define CAMERA_DEBUG_PRINTF(...) do {tfp_printf(__VA_ARGS__);} while (0)
#else
#define CAMERA_DEBUG_PRINTF(...)
#endif

static CAMERA_start_stop CAMERA_start_fn;
static CAMERA_start_stop CAMERA_stop_fn;
static CAMERA_set CAMERA_set_fn;
static CAMERA_supports CAMERA_supports_fn;

/** @brief Camera state change flag.
 * @details Signals bottom half that a commit has changed something about the camera image.
 */
static uint8_t camera_state = CAMERA_STREAMING_OFF;

/** @brief Current set image information.
 *  @details Updated by a commit and used to time frames.
 */
//@{
/// @brief Total pixels in a line or sample received from the camera module.
/// @details This is for reporting information only.
static uint16_t frame_width = 0;
/// @brief Total lines in a frame received from the camera module.
/// @details This is for reporting information only.
static uint16_t frame_height = 0;
/// @brief Total size of the frame received from the camera module.
static uint32_t frame_size = 0;
/// @brief Total bytes in a line or sample received from the camera module.
static uint16_t camera_sample_length = 0;
/// @brief Total bytes in a line or sample returned by camera_read function.
static uint16_t read_sample_length = 0;
/// @brief Number of bytes in a line from the camera module.
//static uint16_t camera_threshold = 0;

/** @brief Camera frame rate.
 * @details One of CAMERA_FRAME_RATE10/15/30
 */
static int8_t camera_frame_rate = 0;

/** @brief Camera format.
 * @details Must be CAMERA_FORMAT_UNCOMPRESSED.
 */
static int8_t camera_format = 0;

/** @brief Used to wait for vsync (VIDEO)
 */
static volatile uint8_t vsync = 0;

/** @brief Camera and VSYNC ISR.
 * @details Keep state of camera interface and communicate with
 * the bottom half.
 */
//@{
/// Count of number of lines available from camera.
static uint32_t camera_rx_data_avail = 0;
/// Buffer line write location within the camera_buffer array.
static uint16_t camera_wr_buffer = 0;
/// Buffer line read location within the camera_buffer array.
static uint16_t camera_rd_buffer = 0;
//@}

/* @brief Camera Buffer
 * @details Circular buffer to receive data from the camera inteface.
 * "Lines" of data from the camera are written here and data is taken
 * from here by the camera interface code. This buffer is passed to
 * the camera interface code.
 */
static uint8_t camera_buffer[CAMERA_BUFFER_LENGTH]  __attribute__((aligned(4)));

/** @brief Current set image information.
 *  @details Updated by a commit and used to time frames.
 */
//@{
/// @brief Total size of the frame received from the camera module.
static uint8_t *camera_buffer_ptr = camera_buffer;
/// @brief Total size of camera buffer.
static uint16_t camera_buffer_size = CAMERA_BUFFER_LENGTH;
//@}

void cam_ISR(void)
{
	static uint8_t *pbuffer;
	static uint16_t len;

	// Synchronise on the start of a frame.
	// If we are waiting for the VSYNC signal then flush all data.
	if (vsync != 0)
	{
		// Read in a line of data from the camera.
		len = cam_available();
		if (len >= camera_sample_length)
		{
			// Point to the current line in the camera_buffer.
			pbuffer = &camera_buffer_ptr[camera_wr_buffer];

			// Stream data from the camera to camera_buffer.
			// This must be aligned to and be a multiple of 4 bytes.
			asm("streamin.l %0,%1,%2" \
					: \
					  :"r"(pbuffer), "r"(&(CAM->CAM_REG3)), "r"(camera_sample_length));

			// Increment the number of bytes available to read.
			// This will signal data is ready to transmit.
			camera_rx_data_avail += camera_sample_length;
			camera_wr_buffer += camera_sample_length;
			if (camera_wr_buffer >= camera_buffer_size)
			{
				// Wrap around in camera_buffer.
				camera_wr_buffer = 0;
			}
		}
	}
	else
	{
		cam_flush();
	}
}

uint16_t camera_init(void)
{
	CAMERA_DEBUG_PRINTF("Camera Test ");

	epuck_init();

	CAMERA_DEBUG_PRINTF("e-puck\r\n");

	CAMERA_start_fn = epuck_start;
	CAMERA_stop_fn = epuck_stop;
	CAMERA_set_fn = epuck_set;
	CAMERA_supports_fn = epuck_supports;

	/* Clock data in when VREF is low and HREF is high */
	cam_init(cam_trigger_mode_1, cam_clock_pol_raising);
	interrupt_attach(interrupt_camera, (uint8_t)interrupt_camera, cam_ISR);

	return 1;
}

struct modes {
	uint16_t width;
	uint16_t height;
	uint8_t frame_rate_count;
	uint8_t frame_rates[16];
	uint8_t format;
	uint8_t index;
	struct modes *next;
};

static struct modes *uvc_cam_modes;
static uint8_t frame_idx_uncompressed = 0;

static void cam_modes_append(uint16_t width, uint16_t height, uint8_t frame_rate, uint8_t format)
{
	struct modes *new, *end;

	/* Look for existing width/height/format matches.*/
	end = uvc_cam_modes;
	while (end)
	{
		if ((width == end->width) && (height == end->height) && (format == end->format))
		{
			/* Add new frame rate to this entry. */
			end->frame_rates[end->frame_rate_count] = frame_rate;
			end->frame_rate_count++;
			return;
		}
		end = end->next;
	}

	uint8_t frame_index = 0;
	if (format == CAMERA_FORMAT_UNCOMPRESSED)
	{
		frame_index = ++frame_idx_uncompressed;
	}
	if (frame_index)
	{
		/* Make a new width/height/format. */
		new = malloc(sizeof(struct modes));
		if (new)
		{
			memset(new, 0, sizeof(struct modes));
			new->next = NULL;
			new->width = width;
			new->height = height;
			new->frame_rate_count = 1;
			new->frame_rates[0] = frame_rate;
			new->format = format;
			new->index = frame_index;

			/* Append to the end of the list. */
			if (uvc_cam_modes == NULL)
			{
				uvc_cam_modes = new;
			}
			else
			{
				end = uvc_cam_modes;
				while (end->next)
				{
					end = end->next;
				}
				end->next = new;
			}
		}
	}
}

/*static void cam_modes_free()
{
	struct modes *old, *end;

	old = uvc_cam_modes;
	while (old)
	{
		end = old->next;
		free(old);
		old = end;
	}

	uvc_cam_modes = NULL;
}*/

void camera_mode_add(uint16_t width, uint16_t height, int8_t frame_rate, int8_t format)
{
	cam_modes_append(width, height, frame_rate, format);
}

uint8_t camera_mode_get_frame_count(int8_t format)
{
	struct modes *end;
	uint8_t count = 0;

	end = uvc_cam_modes;
	while (end)
	{
		if (end->format == format)
		{
			count++;
		}
		end = end->next;
	}
	return count;
}

uint8_t camera_mode_get_frame(int8_t format, int8_t count, uint16_t *width, uint16_t *height)
{
	struct modes *end;

	end = uvc_cam_modes;
	while (end)
	{
		if (end->format == format)
		{
			if (count == 0)
			{
				if (width) *width = end->width;
				if (height) *height = end->height;

				return end->index;
			}
			count--;
		}
		end = end->next;
	}
	return 0;
}

uint8_t camera_mode_get_frame_rate_count(int8_t format, int8_t count)
{
	struct modes *end;

	end = uvc_cam_modes;
	while (end)
	{
		if (end->format == format)
		{
			if (count == 0)
			{
				return end->frame_rate_count;
			}
			count--;
		}
		end = end->next;
	}
	return 0;
}

uint8_t camera_mode_get_frame_rate(int8_t format, int8_t count, int8_t frame_rate)
{
	struct modes *end;

	end = uvc_cam_modes;
	while (end)
	{
		if (end->format == format)
		{
			if (count == 0)
			{
				return end->frame_rates[frame_rate];
			}
			count--;
		}
		end = end->next;
	}
	return 0;
}

uint16_t camera_mode_get_sample_size(int8_t format, int8_t count, uint16_t max_sample)
{
	struct modes *end;
	uint16_t sample;
	uint16_t factor;

	end = uvc_cam_modes;
	while (end)
	{
		if (end->format == format)
		{
			if (count == 0)
			{
				// Enforce a longword boundary.
				max_sample = ((max_sample) & ~3);
				factor = 1;

				if (format == CAMERA_FORMAT_UNCOMPRESSED)
				{
					if (max_sample > 0)
					{
						// For uncompressed images the sample size read from the
						// camera buffer MUST be a factor of the total line size.
						do
						{
							sample = (end->width * 2) / factor;
							if (factor * sample < max_sample)
							{
								return sample;
							}
							factor++;
						} while (sample > 1);

						return 1;
					}
					else
					{
						return end->width * 2;
					}
				}
			}
			count--;
		}
		end = end->next;
	}
	return 0;
}

/**
 * @brief CAMERA start.
 */
void camera_start(void)
{
	// Make the buffer large enough to handle:
	// 1) As many samples of data from the camera module as possible.
	// 2) An amount to align an additional read sample at the end of the buffer.
	camera_buffer_size = ((CAMERA_BUFFER_LENGTH - read_sample_length) / camera_sample_length) * camera_sample_length;
	tfp_printf("camera buffer size: %d\r\n", camera_buffer_size);
	vsync = 0;

	/* Clock data in when VREF is low and HREF is high */
	cam_init(cam_trigger_mode_1, cam_clock_pol_raising);
	interrupt_attach(interrupt_camera, (uint8_t)interrupt_camera, cam_ISR);

	//camera_buffer_ptr = (uint8_t *)buffer;
	//camera_buffer_size = size;
	cam_set_threshold(camera_sample_length);
	cam_start(camera_sample_length);
	cam_enable_interrupt();

	camera_rd_buffer = 0;
	camera_wr_buffer = 0;
	camera_rx_data_avail = 0;

	camera_state = CAMERA_STREAMING_STARTED;

	if (CAMERA_start_fn)
		return CAMERA_start_fn();
}

/**
 * @brief CAMERA stop.
 */
void camera_stop(void)
{
	cam_stop();
	cam_disable_interrupt();

	camera_state = CAMERA_STREAMING_STOPPED;

	if (CAMERA_stop_fn)
		return CAMERA_stop_fn();
}

uint8_t *camera_read(void)
{
	int16_t camera_tx_data_avail;
	uint8_t *pstart;

	if (camera_state != CAMERA_STREAMING_STARTED)
		return NULL;

	if (vsync != 0)
	{
		cam_disable_interrupt();
		/* Number of lines buffered.
		 * This is usually called frequently enough to keep up with camera
		 * data being written by cam_ISR. The image processing which follows
		 * will work behind this data. */
		camera_tx_data_avail = camera_rx_data_avail;
		if (camera_rx_data_avail >= read_sample_length)
		{
			camera_rx_data_avail -= read_sample_length;
		}
		cam_enable_interrupt();

		if (camera_tx_data_avail >= read_sample_length)
		{
			pstart = &camera_buffer_ptr[camera_rd_buffer];
			camera_rd_buffer += read_sample_length;
			/* If the end of the read buffer is past the end of the camera
			 * buffer then enact a special case. Copy the start of the next
			 * sample to the end of the current one. That way the calling
			 * program will get contiguous data. The calculations for
			 * camera_buffer_size ensure this is safe within camera_buffer[].
			 */
			if (camera_rd_buffer == camera_buffer_size)
			{
				camera_rd_buffer = 0;
			}
			else if (camera_rd_buffer > camera_buffer_size)
			{
				memcpy(camera_buffer + camera_buffer_size, camera_buffer, read_sample_length);
				camera_rd_buffer = read_sample_length - (camera_rd_buffer - camera_buffer_size);
			}
			return pstart;
		}
	}
	return NULL;
}

/**
 * @brief CAMERA supports.
 */
int8_t camera_supports(uint16_t width, uint16_t height, int8_t frame_rate, int8_t format)
{
	if (CAMERA_supports_fn)
		return CAMERA_supports_fn(width, height, frame_rate, format);
	return -1;
}

/**
 * @brief CAMERA setup.
 */
int8_t camera_set(uint16_t width, uint16_t height, int8_t frame_rate, int8_t format, uint16_t max_sample)
{
	int8_t ret;

	if (CAMERA_set_fn)
	{
		uint16_t module_sample;
		uint32_t frame;

		ret = CAMERA_set_fn(width, height, format, &frame_rate,
				&module_sample, &frame);

		if (ret == 0)
		{
			camera_sample_length = module_sample;
			read_sample_length = max_sample;
			frame_size = frame;

			CAMERA_DEBUG_PRINTF("Camera frame size %ld\r\n", frame_size);
			CAMERA_DEBUG_PRINTF("Camera module sample %d read samples %d\r\n", camera_sample_length, read_sample_length);

			frame_width = width;
			frame_height = height;
			camera_frame_rate = frame_rate;
			camera_format = format;

			return 0;
		}
	}

	camera_sample_length = 0;
	return -1;
}

/**
 @brief      CAMERA get format
 @details    Will return the current format of the camera.
 **/
uint8_t camera_get_format()
{
	if (camera_state == CAMERA_STREAMING_STARTED)
	{
		return camera_format;
	}
	return 0;
}

/**
 @brief      CAMERA get frame rate
 @details    Will return the current frame rate of the camera.
 **/
uint8_t camera_get_frame_rate()
{
	if (camera_state == CAMERA_STREAMING_STARTED)
	{
		return camera_frame_rate;
	}
	return 0;
}

/**
 @brief      CAMERA get resolution
 @details    Will return the current resolution of the camera.
 **/
uint8_t camera_get_resolution(uint16_t *width, uint16_t *height)
{
	*width = frame_width;
	*height = frame_height;
	if (camera_state == CAMERA_STREAMING_STARTED)
	{
		return 1;
	}
	return 0;
}

/**
 @brief      CAMERA state change
 @details    Will flag that there is a state change to the camera.
 **/
void camera_set_state(int8_t state)
{
	camera_state = state;
}

/**
 @brief      CAMERA state change
 @details    Will return the current state of the camera.
 **/
int8_t camera_get_state()
{
	return camera_state;
}

/**
 @brief      CAMERA frame size
 @details    Gets the frame size.
 **/
uint32_t camera_get_frame_size()
{
	return frame_size;
}

/**
 @brief      CAMERA sample length setup
 @details    Sets the sampling parameters of the current frame.
 **/
void camera_set_sample(uint16_t length)
{
	read_sample_length = length;
	read_sample_length = ((read_sample_length + 3) & ~3);
}

/**
 @brief      CAMERA sample length
 @details    Gets the sampling parameters of the current frame.
 **/
uint16_t camera_get_sample()
{
	return read_sample_length;
}

/**
 @brief      CAMERA VSYNC detected
 @details    Tells the camera interface code that VSYNC event has been
 	 	 	 detected.
 **/
void camera_vsync(volatile uint8_t *signal)
{
	cam_flush();
	*signal = 0;
	camera_wr_buffer = 0;
	camera_rx_data_avail = 0;
	camera_rd_buffer = 0;
	while (!*signal){
		cam_flush();
	};

	vsync = 1;
}
