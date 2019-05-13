/**
 @file camera.h
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

#ifndef SOURCES_CAMERA_H_
#define SOURCES_CAMERA_H_

/**
 @brief Output format definitions for camera interface.
 @details Uncompressed video is an uncompressed bitmap format which is
 	 encoded in YUV422 with a byte order of YUYV. This is supported only
 	 by DirectShow as an input format from a WebCam. It is not supported
 	 by Live555 (and derivatives such as VLC) as an input format.
 	 MJPEG format is Motion-JPEG and consists of a series of JPEG
 	 compressed frames. There is no inter-frame compression. */
//@{
#define CAMERA_FORMAT_ANY 0
#define CAMERA_FORMAT_UNCOMPRESSED 1
//@}

/**
 @brief Frame Width and Height definitions for camera interface.
 */
//@{
#define CAMERA_FRAME_WIDTH_QQVGA 160
#define CAMERA_FRAME_WIDTH_QVGA 320
#define CAMERA_FRAME_WIDTH_VGA 640

#define CAMERA_FRAME_HEIGHT_QQVGA 120
#define CAMERA_FRAME_HEIGHT_QVGA 240
#define CAMERA_FRAME_HEIGHT_VGA 480
//@}

/**
 @brief Frame Rate definitions for camera interface.
 @details Camera module to choose default frame rate for mode.
 */
//@{
#define CAMERA_FRAME_RATE_ANY 0
//@}

/**
 @brief Definition of camera streaming state
 */
#define CAMERA_STREAMING_OFF 0
#define CAMERA_STREAMING_START 1
#define CAMERA_STREAMING_STOP 2
#define CAMERA_STREAMING_STARTED 3
#define CAMERA_STREAMING_STOPPED 4

/**
 @brief Number of lines of image data to buffer.
 @details This has to create a buffer large enough to buffer JPEG
 	 encoded and uncompressed data before it is transmitted.
 	 16kB is sufficient for reliability up to SVGA on a typical busy
 	 network.
 */
#define CAMERA_BUFFER_LENGTH (32 * 1024)

typedef void (*CAMERA_start_stop)(void);
typedef int8_t (*CAMERA_supports)(uint16_t width, uint16_t height, int8_t frame_rate, int8_t format);
typedef int8_t (*CAMERA_set)(uint16_t width, uint16_t height, int8_t format,
		int8_t *frame_rate, uint16_t *sample, uint32_t *frame);

/**
 @brief Camera Initialisation
 @details Initialises the camera module registers.
 @returns Return the hardware ID of the camera module.
 */
uint16_t camera_init(void);

/**
 @brief Camera Add
 @details Adds camera mode support for a resolution, frame
 	 rate and output format.
 */
void camera_mode_add(uint16_t width, uint16_t height, int8_t frame_rate, int8_t format);

/**
 @brief Camera Mode Count
 @details Counts the number of camera modes for an output format.
 */
uint8_t camera_mode_get_frame_count(int8_t format);

/**
 @brief Camera Mode Get Frame
 @details Returns parameters for a particular camera mode and output format.
 */
uint8_t camera_mode_get_frame(int8_t format, int8_t count, uint16_t *width, uint16_t *height);
uint8_t camera_mode_get_frame_rate_count(int8_t format, int8_t count);
uint8_t camera_mode_get_frame_rate(int8_t format, int8_t count, int8_t frame_rate);

/**
 @brief Camera Mode Get Sample Size
 @details Returns maximum sample size for that is a factor of the frame size.
 */
uint16_t camera_mode_get_sample_size(int8_t format, int8_t count, uint16_t max_sample);

/**
 @brief Camera Start
 @details Starts streaming data from the Camera.
 */
void camera_start();

/**
 @brief Camera Stop
 @details Stops streaming data from the Camera.
 */
void camera_stop(void);

/**
 @brief Camera Read
 @details Read a sample line of data from the camera buffer.
 @returns Pointer to the start of a camera buffer data line.
 */
uint8_t *camera_read(void);

/**
 @brief Camera Supports
 @details Check that the camera module supports a resolution, frame
 	 rate and output format.
 */
int8_t camera_supports(uint16_t width, uint16_t height, int8_t frame_rate, int8_t format);

/**
 @brief Camera Setup
 @details Configures the camera module to a resolution, frame rate and
   output format.
 */
int8_t camera_set(uint16_t width, uint16_t height, int8_t frame_rate, int8_t format, uint16_t max_sample);

/**
 @brief      CAMERA state change
 @details    Will flag that there is a state change to the camera.
 **/
void camera_set_state(int8_t state);

/**
 @brief      CAMERA get format
 @details    Will return the current format of the camera.
 **/
uint8_t camera_get_format();

/**
 @brief      CAMERA get resolution
 @details    Will return the current resolution of the camera.
 **/
uint8_t camera_get_resolution(uint16_t *width, uint16_t *height);

/**
 @brief      CAMERA get frame rate
 @details    Will return the current frame rate of the camera.
 **/
uint8_t camera_get_frame_rate();

/**
 @brief      CAMERA state change
 @details    Will return the current state of the camera.
 **/
int8_t camera_get_state();

/**
 @brief      CAMERA sample length setup
 @details    Sets the sampling parameters of the current frame.
 **/
void camera_set_sample(uint16_t length);

/**
 @brief      CAMERA frame size
 @details    Gets the frame size.
 **/
uint32_t camera_get_frame_size();

/**
 @brief      CAMERA sample length
 @details    Gets the sampling parameters of the current frame.
 **/
uint16_t camera_get_sample();
/**
 @brief      CAMERA VSYNC detected
 @details    Tells the camera interface code that VSYNC event has been
 	 	 	 detected.
 **/
void camera_vsync(volatile uint8_t *signal);

#include "epuck_camera.h"

#endif /* SOURCES_CAMERA_H_ */
