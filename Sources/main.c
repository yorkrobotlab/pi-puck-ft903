/**
  @file main.c
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

#include <ft900.h>
#include <ft900_uart_simple.h>

#include <ft900_usb.h>
#include <ft900_usbd.h>
#include <ft900_usb_uvc.h>
#include <ft900_startup_dfu.h>

/* UART support for printf output. */
#include "tinyprintf.h"

#include "camera.h"


/**
 @brief UVC device to use isochronous endpoints.
 @details When defined the UVC interface will present an isochronous
  data endpoint to transmit image data to the host. When undefined the
  endpoint will be a bulk type. The UVC specification does not insist
  on either bulk or isochronous endpoints.
  On the FT90x, an isochronous endpoint may transmit up-to 1kB in a
  single packet to the host in each microframe at high speed. This
  limits the amount of data which can be send to 8192 kB/sec. For VGA
  resolutions (uncompressed) a safe frame rate would be 7.5 fps when
  overheads are taken into account.
  A bulk endpoint on the FT90x will allow multiple data packets per
  microframe and hence a higher bandwidth.
 */
#undef USB_ENDPOINT_USE_ISOC

/**
 @brief Include a DFU Interface in the configuration.
 @details This adds an interface to the USB configuration descriptor
  to present a DFU option to the host. It will add in WCID descriptors
  so that a WinUSB-based device driver can be used with a DFU program
  such as dfu-util.
 */
#undef USB_INTERFACE_USE_DFU

/**
 @brief Start DFU Interface briefly before the UVC camera.
 @details This enables the DFU interface to appear for a short period
  of time to present a DFU option to the host. After this, the
  configuration will be removed and the main program's UVC configuration
  will start. A DFU program can start when the DFU configuration is
  active and update firmware.
 */
#undef USB_INTERFACE_USE_STARTUPDFU

/**
 * @brief Show diagonal lines on the image sent to the USB host.
 * @details For uncompressed formats this will make a pair of diagonal
 *  lines from the top left and top right corner of the frame. Issues
 *  with the image can be observed and more easily compared to USB or
 *  data analyser captures.
 *  This can not be used for MJPEG format.
 */
#undef SHOW_DEBUG_DIAGONAL_LINES

/**
 * @brief Show output on UART to debug frame size in MJPEG.
 * @details For compressed formats an indication of the actual number
 * of lines used by the decoder will be printed out on the UART.
 * It should have at least 1 dash between the start 's' and the end
 * character 'x' or 'X'. If there are not enough dashes then there are
 * not enough lines.
 *  This can not be used for MJPEG format.
 */
#undef SHOW_DEBUG_LINE_USAGE

/**
 * @brief USB Video Class specification version numbers.
 * @details The version of the UVC specification that this firmware
 * supports. Currently this is version 1.1.
 */
//@{
#define USB_VIDEO_CLASS_VERSION_MAJOR 1
#define USB_VIDEO_CLASS_VERSION_MINOR 1
//@}

#define BRIDGE_DEBUG
#ifdef BRIDGE_DEBUG
#define BRIDGE_DEBUG_PRINTF(...) do {tfp_printf(__VA_ARGS__);} while (0)
#else
#define BRIDGE_DEBUG_PRINTF(...)
#endif

/* For MikroC const qualifier will place variables in Flash
 * not just make them constant.
 */
#if defined(__GNUC__)
#define DESCRIPTOR_QUALIFIER const
#elif defined(__MIKROC_PRO_FOR_FT90x__)
#define DESCRIPTOR_QUALIFIER data
#endif

/* CONSTANTS ***********************************************************************/

/**
 @name USB and Hub Configuration
 @brief Indication of how the USB device is powered and the size of
 	 the control endpoint Max Packets.
 */
//@{
// USB Bus Powered - set to 1 for self-powered or 0 for bus-powered
#ifndef USB_SELF_POWERED
#define USB_SELF_POWERED 1
#endif // USB_SELF_POWERED
#if USB_SELF_POWERED == 1
#define USB_CONFIG_BMATTRIBUTES_VALUE (USB_CONFIG_BMATTRIBUTES_SELF_POWERED | USB_CONFIG_BMATTRIBUTES_RESERVED_SET_TO_1)
#else // USB_SELF_POWERED
#define USB_CONFIG_BMATTRIBUTES_VALUE USB_CONFIG_BMATTRIBUTES_RESERVED_SET_TO_1
#endif // USB_SELF_POWERED
// USB Endpoint Zero packet size (both must match)
#define USB_CONTROL_EP_MAX_PACKET_SIZE 64
#define USB_CONTROL_EP_SIZE USBD_EP_SIZE_64
//@}


/**
 @name DFU Configuration
 @brief Determines the parts of the DFU specification which are supported
        by the DFU library code. Features can be disabled if required.
 */
//@{
#define DFU_ATTRIBUTES USBD_DFU_ATTRIBUTES
//@}

/**
 @name Device Configuration Areas
 @brief Size and location reserved for string descriptors.
 Leaving the allocation size blank will make an array exactly the size
 of the string allocation.
 Note: Specifying the location is not supported by the GCC compiler.
 */
//@{
// String descriptors - allow a maximum of 256 bytes for this
#define STRING_DESCRIPTOR_LOCATION 0x80
#define STRING_DESCRIPTOR_ALLOCATION 0x100
//@}

/**
 @name DFU_TRANSFER_SIZE definition.
 @brief Number of bytes in block, sent in each DFU_DNLOAD request
 from the DFU update program on the host. This is simplified
 in that the meaning of a block is an arbitrary number of
 bytes. This is intentionally a multiple of the maximum
 packet size for the control endpoints.
 It is used in the DFU functional descriptor as wTransferSize.
 The maximum size supported by the DFU library is 256 bytes
 which is the size of a page of Flash.
 */
//@{
#define DFU_TRANSFER_SIZE USBD_DFU_MAX_BLOCK_SIZE
#define DFU_TIMEOUT USBD_DFU_TIMEOUT
//@}

/**
 @name USB_PID_UVC configuration.
 @brief Run Time Product ID for UVC function.
 */
//@{
#define USB_PID_UVC 0x0fd8
//@}

/**
 @name DFU_USB_PID_DFUMODE configuration.
 @brief FTDI predefined DFU Mode Product ID.
 */
//@{
#define DFU_USB_PID_DFUMODE 0x0fde
//@}

/**
 @name DFU_USB_INTERFACE configuration..
 @brief Run Time and DFU Mode Interface Numbers.
 */
//@{
#define DFU_USB_INTERFACE_RUNTIME 2
#define DFU_USB_INTERFACE_DFUMODE 0
//@}

/**
 @name WCID_VENDOR_REQUEST_CODE for WCID.
 @brief Unique vendor request code for WCID OS Vendor Extension validation.
 */
//@{
#define WCID_VENDOR_REQUEST_CODE	 0xF1
//@}

/**
 @brief Endpoint definitions for UVC device.
 */
//@{
#define UVC_EP_INTERRUPT	 			USBD_EP_1
#define UVC_EP_DATA_IN 					USBD_EP_2
#define UVC_INTERRUPT_EP_SIZE 			0x40
#define UVC_INTERRUPT_USBD_EP_SIZE 		USBD_EP_SIZE_64
/// Endpoint for High Speed mode
#ifdef USB_ENDPOINT_USE_ISOC
#define UVC_DATA_EP_SIZE_HS				0x400
#define UVC_DATA_USBD_EP_SIZE_HS 		USBD_EP_SIZE_1024
#else // !USB_ENDPOINT_USE_ISOC
#define UVC_DATA_EP_SIZE_HS				0x200
#define UVC_DATA_USBD_EP_SIZE_HS 		USBD_EP_SIZE_512
#endif // USB_ENDPOINT_USE_ISOC
/// Endpoint for Full Speed mode
#define UVC_DATA_EP_SIZE_FS				0x200
#define UVC_DATA_USBD_EP_SIZE_FS 		USBD_EP_SIZE_512
//@}

/** @brief Payload Type Definition Count
 * @details Count supported payload types for the selected camera module.
 */
//@{
#define FORMAT_INDEX_HS_COUNT 1
//@}

/** @brief Supported Uncompressed Formats for BULK endpoints
 * @details Supported formats for uncompressed video from the
 * camera module.
 * The uncompressed output is limited to QVGA and VGA due to the large
 * data size for each frame. A single frame of VGA is 600kB. SVGA and
 * require too much bandwidth to transmit uncompressed. It is left here
 * and can be selected.
 */
//@{
#ifndef USB_ENDPOINT_USE_ISOC
#define UC_QVGA // Works for OV965x and OV5640
#define UC_VGA // Works for OV965x and OV5640
/// Count of the total number of formats enabled.
#define UC_FORMAT_COUNT 2
#endif // USB_ENDPOINT_USE_ISOC
//@}

/** @brief Correct Supported Uncompressed Formats for ISO Endpoints
 * @details Maximum of 1024 bytes can be sent per microframe in isochronous
 * mode. This limits the application to QVGA resolution and 642 bytes per
 * transfer.
 */
//@{
#ifdef USB_ENDPOINT_USE_ISOC
#define UC_QVGA // Works for OV965x and OV5640
#undef UC_VGA // Note: untested for OV965x and OV5640
/// Count of the total number of formats enabled.
#define UC_FORMAT_COUNT 1
#endif // USB_ENDPOINT_USE_ISOC
//@}

/** @brief Uncompressed payload format definition
 * @details The uncompressed payload formats for UVC devices allow
 * NV12 or YUY2. These are selected using the following GUIDs:
 * USB_UVC_GUID_NV12 or USB_UVC_GUID_YUY2.
 */
#define PAYLOAD_FORMAT_UNCOMPRESSED USB_UVC_GUID_YUY2 /* format.guidFormat[16] */

/** @brief Uncompressed payload pixel size
 * @details Uncompressed payload images can be 16 or 24 bits per pixel.
 */
#define PAYLOAD_BBP_UNCOMPRESSED 0x10 /* format.bBitsPerPixel */

/** @brief UVC Payload Header Length
 * @details The UVC payload can have a variable length header. This must
 * include 2 bytes described in the structure USB_UVC_Payload_Header but
 * can also include the presentation time and/or the source clock.
 * The payload header is defined as USB_UVC_Payload_Header but as a lot of
 * calculations on buffer and transfer sizes are made in the preprocessor
 * the define must be an integer constant rather than a sizeof which is
 * filled in by the compiler - not the preprocessor.
 */
#define PAYLOAD_HEADER_LENGTH 2

/**
 @brief Entity ID definitions for UVC device.
 */
//@{
#define ENTITY_ID_NONE 0
#define ENTITY_ID_CAMERA 1
#define ENTITY_ID_OUTPUT 3
#define ENTITY_ID_PROCESSING 5
//@}

/**
 @brief Format Bits Per Pixel definition for UVC device.
 @details Derived from the image type for uncompressed format.
 */
#define FORMAT_UC_BBP (PAYLOAD_BBP_UNCOMPRESSED >> 3)

/**
 @brief Frame Index definitions for UVC device.
 @details For uncompressed formats.
 */
//@{
enum {
	FRAME_INDEX_HS_UC_DISABLED = 0, /// No uncompressed format
#ifdef UC_QVGA
	FRAME_INDEX_HS_UC_QVGA, /// QVGA - 320 x 240
#endif
#ifdef UC_VGA
	FRAME_INDEX_HS_UC_VGA, /// VGA - 640 x 480
#endif
	FRAME_INDEX_HS_UC_END,

	/** Add dummy definitions for unused resolutions.
	 * This allows us to simplify code later.
	 * Definitions are made but are invalid.
	 */
	//@{
#ifndef UC_QVGA
	FRAME_INDEX_HS_UC_QVGA, /// Dummy definition
#endif
#ifndef UC_VGA
	FRAME_INDEX_HS_UC_VGA, /// Dummy definition
#endif
	//@}
};
///
#define FRAME_INDEX_HS_UC_COUNT UC_FORMAT_COUNT
#define FRAME_INDEX_HS_UC_DEFAULT  1
//@}

/**
 @brief Supported Frame Index definitions for UVC device.
 @details Declare the range of resolutions to support for uncompressed formats.
 */
//@{
#ifdef UC_VGA
#define FRAME_INDEX_HS_UC_MAX FRAME_INDEX_HS_UC_VGA
#elif defined(UC_QVGA)
#define FRAME_INDEX_HS_UC_MAX FRAME_INDEX_HS_UC_QVGA
#else
#define FRAME_INDEX_HS_UC_MAX 0
#endif

#ifdef UC_QVGA
#define FRAME_INDEX_HS_UC_MIN FRAME_INDEX_HS_UC_QVGA
#elif defined(UC_VGA)
#define FRAME_INDEX_HS_UC_MIN FRAME_INDEX_HS_UC_VGA
#else
#define FRAME_INDEX_HS_UC_MIN 0
#endif
//@}

/**
 @brief Format Index Type definitions for UVC device.
 @details This can be done more neatly in an enum but preprocessor
  macros are used. If an enum were used then these
 */
//@{
enum {
	FORMAT_INDEX_TYPE_NONE = 0,
	FORMAT_INDEX_TYPE_UNCOMPRESSED,
	FORMAT_INDEX_END,

	/** Add dummy definitions for unused types.
	 * This allows us to simplify code later.
	 * Definitions are made but are invalid.
	 */
	//@{
	FORMAT_INDEX_TYPE_MJPEG,
	//@}
};
#define FORMAT_INDEX_HS_COUNT 1
//@}

/**
 @brief Frame Size definitions for UVC device.
 @details Frames sizes for uncompressed formats.
 */
//@{
#define FRAME_INDEX_HS_UC_QVGA_SIZE (CAMERA_FRAME_WIDTH_QVGA * CAMERA_FRAME_HEIGHT_QVGA * FORMAT_UC_BBP)
#define FRAME_INDEX_HS_UC_VGA_SIZE (CAMERA_FRAME_WIDTH_VGA * CAMERA_FRAME_HEIGHT_VGA * FORMAT_UC_BBP)

#if defined(UC_VGA)
#define FRAME_INDEX_HS_UC_MAX_SIZE FRAME_INDEX_HS_UC_VGA_SIZE
#elif defined(UC_QVGA)
#define FRAME_INDEX_HS_UC_MAX_SIZE FRAME_INDEX_HS_UC_QVGA_SIZE
#else
#define FRAME_INDEX_HS_UC_MAX_SIZE 0
#endif

#if defined(UC_QVGA)
#define FRAME_INDEX_HS_UC_MIN_SIZE FRAME_INDEX_HS_UC_QVGA_SIZE
#elif defined(UC_VGA)
#define FRAME_INDEX_HS_UC_MIN_SIZE FRAME_INDEX_HS_UC_VGA_SIZE
#else
#define FRAME_INDEX_HS_UC_MIN_SIZE 0
#endif

#define FRAME_INDEX_FS_QVGA_SIZE (160 * 120 * FORMAT_UC_BBP)
#define FRAME_INDEX_FS_MAX_SIZE FRAME_INDEX_FS_QVGA_SIZE
//@}

/**
 @brief Frame Aspect Ratio definitions for UVC device.
 */
//@{
#define FRAME_RATIO_X 4
#define FRAME_RATIO_Y 3
//@}

/**
 @brief Frame Interval definitions for UVC device.
 */
//@{
#define FRAME_INTERVAL_7_5 (20000000/15)
#define FRAME_INTERVAL_10 (10000000/10)
#define FRAME_INTERVAL_15 (10000000/15)
#define FRAME_INTERVAL_30 (10000000/30)
//@}

/**
 @brief Frame Rate definitions for UVC device.
 @details Frame rates for uncompressed formats.
 */
//@{
// High-speed
#define FRAME_INDEX_HS_UC_QVGA_FRAME_RATE CAMERA_FRAME_RATE_15
#define FRAME_INDEX_HS_UC_QVGA_FRAME_INTERVAL FRAME_INTERVAL_15

#define FRAME_INDEX_HS_UC_VGA_FRAME_RATE CAMERA_FRAME_RATE_15
#define FRAME_INDEX_HS_UC_VGA_FRAME_INTERVAL FRAME_INTERVAL_15

#define FRAME_INDEX_HS_UC_MIN_FRAME_INTERVAL FRAME_INTERVAL_15
#define FRAME_INDEX_HS_UC_MAX_FRAME_INTERVAL FRAME_INTERVAL_15

// Full-speed
#define FRAME_INDEX_FS_QVGA_FRAME_RATE CAMERA_FRAME_RATE_15
#define FRAME_INDEX_FS_QVGA_FRAME_INTERVAL FRAME_INTERVAL_15
//@}

/**
 @brief Frame Rate definitions for UVC device.
 @details Frame rates for MJPEG formats.
 */
//@{
// High-speed
#define FRAME_INDEX_HS_MJPEG_QVGA_FRAME_RATE CAMERA_FRAME_RATE_15
#define FRAME_INDEX_HS_MJPEG_QVGA_FRAME_INTERVAL FRAME_INTERVAL_15

#define FRAME_INDEX_HS_MJPEG_VGA_FRAME_RATE CAMERA_FRAME_RATE_15
#define FRAME_INDEX_HS_MJPEG_VGA_FRAME_INTERVAL FRAME_INTERVAL_15

//@}

/**
 @brief Transfer size definitions for UVC device.
 @details Frames sizes for uncompressed formats.
 */
//@{
#define FRAME_INDEX_HS_UC_QVGA_TRANSFER_SIZE (CAMERA_FRAME_WIDTH_QVGA * FORMAT_UC_BBP)
#define FRAME_INDEX_HS_UC_VGA_TRANSFER_SIZE (CAMERA_FRAME_WIDTH_VGA * FORMAT_UC_BBP)

#if defined(UC_VGA)
#define FRAME_INDEX_HS_UC_MAX_TRANSFER_SIZE FRAME_INDEX_HS_UC_VGA_TRANSFER_SIZE
#elif defined(UC_QVGA)
#define FRAME_INDEX_HS_UC_MAX_TRANSFER_SIZE FRAME_INDEX_HS_UC_QVGA_TRANSFER_SIZE
#else
#define FRAME_INDEX_HS_UC_MAX_TRANSFER_SIZE 0
#endif

#if defined(UC_QVGA)
#define FRAME_INDEX_HS_UC_MIN_TRANSFER_SIZE FRAME_INDEX_HS_UC_QVGA_TRANSFER_SIZE
#elif defined(UC_VGA)
#define FRAME_INDEX_HS_UC_MIN_TRANSFER_SIZE FRAME_INDEX_HS_UC_VGA_TRANSFER_SIZE
#else
#define FRAME_INDEX_HS_UC_MIN_TRANSFER_SIZE 0
#endif
//@}

/**
 @brief Maximum/Minimum Frame Lengths in bytes for UVC device.
 */
//@{
#define FRAME_INDEX_MAX_SIZE (FRAME_INDEX_HS_UC_MAX_SIZE)
#define FRAME_INDEX_MIN_SIZE (FRAME_INDEX_HS_UC_MIN_SIZE)
//@}

/**
 @brief Maximum/Minimum Line Lengths in bytes for UVC device.
 */
//@{
#define FRAME_INDEX_MAX_TRANSFER_SIZE (FRAME_INDEX_HS_UC_MAX_TRANSFER_SIZE)
#define FRAME_INDEX_MIN_TRANSFER_SIZE (FRAME_INDEX_HS_UC_MIN_TRANSFER_SIZE)
//@}

/**
 @brief Maximum Length for Line Buffer.
 @details This is the longest buffer length required to hold one line
 	 of uncompressed data or a section of compressed data.
 */
#define FORMAT_MAX_LINE_BYTES_ALIGNED ((FRAME_INDEX_MAX_TRANSFER_SIZE + 3) & (~3L))

/**
 @brief Clock Frequency definitions for UVC device.
 */
//@{
#define CLK_FREQ_48MHz 0x02dc6c00
//@}

/**
 @brief Definition of camera streaming state
 */
#define CAMERA_STREAMING_OFF 0
#define CAMERA_STREAMING_START 1
#define CAMERA_STREAMING_STOP 2

/**
 @brief Number of lines of image data to buffer.
 */
#define CAMERA_BUFFER_MAX_LINES 24

/* GLOBAL VARIABLES ****************************************************************/

/* LOCAL VARIABLES *****************************************************************/

// Make this a multiple of the largest line of data received.
static uint8_t camera_buffer[CAMERA_BUFFER_MAX_LINES][FORMAT_MAX_LINE_BYTES_ALIGNED]  __attribute__((aligned(4)));

/** @brief Camera and VSYNC ISR.
 * @details Keep state of camera interface and communicate with
 * the bottom half.
 */
//@{
/// Count of number of lines available from camera.
static volatile uint16_t camera_rx_data_avail = 0;
/// Buffer line within the camera_buffer array.
static volatile uint16_t camera_rx_buffer_line = 0;
//@}

/** @brief Used to wait for vsync (GPIO)
 */
static volatile uint8_t camera_vsync = 0;

/** @brief Current set image information.
 *  @details Updated by a commit and used to time frames.
 */
//@{
/// @brief Total pixels in a line or sample received from the camera module.
/// @details This is for reporting information only.
static uint16_t sample_width = 0;
/// @brief Total lines in a frame received from the camera module.
/// @details This is for reporting information only.
static uint16_t frame_height = 0;
/// @brief Total size of the frame received from the camera module.
static uint32_t frame_size = 0;
/// @brief Total bytes in a line or sample received from the camera module.
static uint16_t sample_length = 0;
/// @brief Current threshold for data received from the camera module.
static uint16_t sample_threshold;
//@}
/** @brief Camera state change flag.
 * @details Signals bottom half that a commit has changed something about the camera image.
 */
static uint8_t camera_state_change = CAMERA_STREAMING_OFF;

/**
 @name string_descriptor
 @brief Table of USB String descriptors

 This is placed at a fixed location in the const section allowing
 up-to 256 bytes of string descriptors to be defined. These can be
 modified or replaced by a utility or binary editor without
 requiring a recompilation of the firmware.
 They are placed at offset 0x100 and reserve 0x100 bytes.
 The data is not stored in section and is therefore
 regarded as const.
 */
#define UNICODE_LEN(A) (((A * 2) + 2) | (USB_DESCRIPTOR_TYPE_STRING << 8))

DESCRIPTOR_QUALIFIER /*__at(STRING_DESCRIPTOR_LOCATION)*/ uint16_t string_descriptor[STRING_DESCRIPTOR_ALLOCATION/sizeof(uint16_t)] =
{
		UNICODE_LEN(1), 0x0409, // 0409 = English (US)
		// String 1 (Manufacturer): "FTDI"
		UNICODE_LEN(4), L'F', L'T', L'D', L'I',
		// String 2 (Product): Depends on settings

#ifdef USB_ENDPOINT_USE_ISOC
		// String 2 (Product): "AN_414 UVC ISOC"
		UNICODE_LEN(15), L'A', L'N', L'_', L'4', L'1', L'4', L' ', L'U', L'V', L'C', L' ', L'I', L'S', L'O', L'C',
#else // USB_ENDPOINT_USE_ISOC
		// String 2 (Product): "AN_414 UVC BULK"
		UNICODE_LEN(15), L'A', L'N', L'_', L'4', L'1', L'4', L' ', L'U', L'V', L'C', L' ', L'B', L'U', L'L', L'K',
#endif // USB_ENDPOINT_USE_ISOC

		// String 3 (Serial Number): "UCxxxxx" where UC signifies
		// UnCompressed video supported.
		// Each x is 1 or 0 depending on support for QVGA, VGA, SVGA, XGA or SXGA.
		UNICODE_LEN(7),
		L'U', L'C',
#ifdef UC_QVGA
		L'1',
#else
		L'0',
#endif
#ifdef UC_VGA
		L'1',
#else
		L'0',
#endif
		L'0',
		L'0',
		L'0',

		// String 4 (DFU Product Name): "FT900 DFU Mode"
		UNICODE_LEN(14), L'F', L'T', L'9', L'0', L'0', L' ', L'D', L'F', L'U', L' ', L'M', L'o', L'd', L'e',
		// String 5 (Interface Name): "DFU Interface"
		UNICODE_LEN(13), L'D', L'F', L'U', L' ', L'I', L'n', L't', L'e', L'r', L'f', L'a', L'c', L'e',
		// END OF STRINGS
		0x0000,
};

/**
 @name wcid_string_descriptor
 @brief USB String descriptor for WCID identification.
 */
DESCRIPTOR_QUALIFIER uint8_t wcid_string_descriptor[USB_MICROSOFT_WCID_STRING_LENGTH] = {
		USB_MICROSOFT_WCID_STRING(WCID_VENDOR_REQUEST_CODE)
};

/**
 @name device_descriptor_uvc
 @brief Device descriptor for Run Time mode.
 */
DESCRIPTOR_QUALIFIER USB_device_descriptor device_descriptor_uvc =
{
		sizeof(USB_device_descriptor), /* bLength */
		USB_DESCRIPTOR_TYPE_DEVICE, /* bDescriptorType */
		USB_BCD_VERSION_2_0, /* bcdUSB */          // V2.0
		USB_CLASS_MISCELLANEOUS, /* bDeviceClass */       // Defined in interface
		USB_SUBCLASS_COMMON_CLASS, /* bDeviceSubClass */ // Defined in interface
		USB_PROTOCOL_INTERFACE_ASSOCIATION, /* bDeviceProtocol */ // Defined in interface
		USB_CONTROL_EP_MAX_PACKET_SIZE, /* bMaxPacketSize0 */
		USB_VID_FTDI, /* idVendor */   // idVendor: 0x0403 (FTDI)
		USB_PID_UVC, /* idProduct */ // idProduct: 0x0fd9
		0x0101, /* bcdDevice */        // 1.1
		0x01, /* iManufacturer */      // Manufacturer
		0x02, /* iProduct */           // Product
		0x03, /* iSerialNumber */      // Serial Number
		0x01, /* bNumConfigurations */
};

/**
 @name device_qualifier_descriptor_uvc
 @brief Device qualifier descriptor for Run Time mode.
 */
DESCRIPTOR_QUALIFIER USB_device_qualifier_descriptor device_qualifier_descriptor_uvc =
{
		sizeof(USB_device_qualifier_descriptor), /* bLength */
		USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER, /* bDescriptorType */
		USB_BCD_VERSION_2_0, /* bcdUSB */          // V2.0
		USB_CLASS_MISCELLANEOUS, /* bDeviceClass */       // Defined in interface
		USB_SUBCLASS_COMMON_CLASS, /* bDeviceSubClass */ // Defined in interface
		USB_PROTOCOL_INTERFACE_ASSOCIATION, /* bDeviceProtocol */ // Defined in interface
		USB_CONTROL_EP_MAX_PACKET_SIZE, /* bMaxPacketSize0 */
		1, /* bNumConfigurations */
		0
};

/**
 @brief Configuration descriptor for Run Time mode.
 */
//@{

/**
 @brief VideoControl Configuration descriptor for Run Time mode.
 */
struct UVC_VC_config_descriptor {
	USB_UVC_VC_CSInterfaceHeaderDescriptor(1) vc_header;
	USB_UVC_VC_CameraTerminalDescriptor(2) camera_input;
	USB_UVC_VC_OutputTerminalDescriptor camera_output;
	USB_UVC_VC_ProcessingUnitDescriptor(2) camera_proc_unit;
	//USB_UVC_VC_ExtensionUnitDescriptor(1, 3) camera_ext_unit;
};

/**
 @brief VideoStreaming Configuration descriptor for High Speed.
 */
struct UVC_VS_config_descriptor_hs {
	USB_UVC_VS_CSInterfaceInputHeaderDescriptor(FORMAT_INDEX_HS_COUNT) vs_header;

	// Uncompressed video format and frame descriptors
	USB_UVC_VS_UncompressedVideoFormatDescriptor format_uc;
#ifdef UC_QVGA
	USB_UVC_VS_UncompressedVideoFrameDescriptorDiscrete(1) hs_frame_qvga;
#endif
#ifdef UC_VGA
	USB_UVC_VS_UncompressedVideoFrameDescriptorDiscrete(1) hs_frame_vga;
#endif
	USB_UVC_ColorMatchingDescriptor hs_colour_match_uc;
};

/**
 @brief Configuration descriptor - High Speed.
 */
struct PACK config_descriptor_uvc_hs
{
	USB_configuration_descriptor configuration;

	USB_UVC_interface_association_descriptor interface_association;

	USB_UVC_VC_StandardInterfaceDescriptor interface_video_control;
	struct UVC_VC_config_descriptor vc_config;
	USB_UVC_VC_StandardInterruptEndpointDescriptor endpoint_int_in;
	USB_UVC_VC_CSEndpointDescriptor endpoint_int_descriptor;

	USB_UVC_VS_StandardInterfaceDescriptor interface_video_stream_alt_0;
	struct UVC_VS_config_descriptor_hs vs_config_alt1;
#ifndef USB_ENDPOINT_USE_ISOC
	USB_UVC_VS_BulkVideoDataEndpointDescriptor endpoint_data_in_hs;
#endif // !USB_ENDPOINT_USE_ISOC

#ifdef USB_ENDPOINT_USE_ISOC
	USB_UVC_VS_StandardInterfaceDescriptor interface_video_stream_alt_1;
	USB_UVC_VS_IsochronousVideoDataEndpointDescriptor endpoint_data_in_hs;
#endif // USB_ENDPOINT_USE_ISOC

#ifdef USB_INTERFACE_USE_DFU
	USB_interface_descriptor dfu_interface;
	USB_dfu_functional_descriptor dfu_functional;
#endif // USB_INTERFACE_USE_DFU
};

/**
 @brief Configuration descriptor - Full Speed.
 @details This is the configuration descriptor when in Full Speed mode.
 	 It does not have a video interface with an Isochronous IN endpoint as
 	 the bandwidth for Full Speed is inadequate.
 */
struct PACK config_descriptor_uvc_fs
{
	USB_configuration_descriptor configuration;

	USB_UVC_interface_association_descriptor interface_association;

	USB_UVC_VC_StandardInterfaceDescriptor interface_video_control;
	struct UVC_VC_config_descriptor vc_config;
	USB_UVC_VC_StandardInterruptEndpointDescriptor endpoint_int_in;
	USB_UVC_VC_CSEndpointDescriptor endpoint_int_descriptor;

#ifdef USB_INTERFACE_USE_DFU
	USB_interface_descriptor dfu_interface;
	USB_dfu_functional_descriptor dfu_functional;
#endif // USB_INTERFACE_USE_DFU
};

/**
 @brief Configuration descriptor declaration and initialisation.
 */
DESCRIPTOR_QUALIFIER struct config_descriptor_uvc_hs config_descriptor_uvc_hs =
{
	{
		sizeof(USB_configuration_descriptor), /* configuration.bLength */
		USB_DESCRIPTOR_TYPE_CONFIGURATION, /* configuration.bDescriptorType */
		sizeof(struct config_descriptor_uvc_hs), /* configuration.wTotalLength */
#ifdef USB_INTERFACE_USE_DFU
		0x03, /* configuration.bNumInterfaces */
#else // !USB_INTERFACE_USE_DFU
		0x02, /* configuration.bNumInterfaces */
#endif // USB_INTERFACE_USE_DFU
		0x01, /* configuration.bConfigurationValue */
		0x00, /* configuration.iConfiguration */
		USB_CONFIG_BMATTRIBUTES_VALUE, /* configuration.bmAttributes */
		//0xFA, /* configuration.bMaxPower */           // 500mA
		0, /* configuration.bMaxPower */           // 0mA
	},

	{
		sizeof(USB_UVC_interface_association_descriptor), /* interface_association.bLength */
		USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION, /* interface_association.bDescriptorType */
		0, /* interface_association.bFirstInterface */
		2, /* interface_association.bInterfaceCount */
		USB_CLASS_VIDEO, /* interface_association.bFunctionClass */
		USB_SUBCLASS_VIDEO_INTERFACE_COLLECTION, /* interface_association.bFunctionSubClass */
		USB_PROTOCOL_VIDEO_UNDEFINED, /* interface_association.bFunctionProtocol */
		2, /* interface_association.iFunction */  // "FT900 UVC"
	},

	// ---- INTERFACE DESCRIPTOR ----
	{
		sizeof(USB_UVC_VC_StandardInterfaceDescriptor), /* interface_video_control.bLength */
		USB_DESCRIPTOR_TYPE_INTERFACE, /* interface_video_control.bDescriptorType */
		0, /* interface_video_control.bInterfaceNumber */
		0x00, /* interface_video_control.bAlternateSetting */
		0x01, /* interface_video_control.bNumEndpoints */
		USB_CLASS_VIDEO, /* interface_video_control.bInterfaceClass */ // UVC Class
		USB_SUBCLASS_VIDEO_VIDEOCONTROL, /* interface_video_control.bInterfaceSubClass */ // Abstract Control Model
		USB_PROTOCOL_VIDEO_UNDEFINED, /* interface_video_control.bInterfaceProtocol */ // No built-in protocol
		2, /* interface_video_control.iInterface */ // Same as IAD iFunction.
	},

	{
		// ---- Class specific VC Interface Header Descriptor ----
		{
			sizeof(USB_UVC_VC_CSInterfaceHeaderDescriptor(1)), /* vc_header.bLength */
			USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* vc_header.bDescriptorType */
			USB_UVC_DESCRIPTOR_SUBTYPE_VC_HEADER, /* vc_header.bDescriptorSubtype */
			(USB_VIDEO_CLASS_VERSION_MAJOR << 8) | (USB_VIDEO_CLASS_VERSION_MINOR << 4), /* vc_header.bcdUVC */
			sizeof(struct UVC_VC_config_descriptor), /* vc_header.wTotalLength */
			CLK_FREQ_48MHz, /* vc_header.dwClockFrequency */
			0x01, /* vc_header.bInCollection */
			{0x01,} /* vc_header.baInterfaceNr */
		},

		// ---- Input Terminal Descriptor - Camera ----
		{
			sizeof(USB_UVC_VC_CameraTerminalDescriptor(2)), /* camera_input.bLength */
			USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* camera_input.bDescriptorType */
			USB_UVC_DESCRIPTOR_SUBTYPE_VC_INPUT_TERMINAL, /* camera_input.bDescriptorSubtype */
			ENTITY_ID_CAMERA, /* camera_input.bTerminalID */
			USB_UVC_ITT_CAMERA, /* camera_input.wTerminalType */
			0x00, /* camera_input.bAssocTerminal */
			0x00, /* camera_input.iTerminal */
			0x0000, /* camera_input.wObjectiveFocalLengthMin */
			0x0000, /* camera_input.wObjectiveFocalLengthMax */
			0x0000, /* camera_input.wOcularFocalLength */
			0x02, /* camera_input.bControlSize */
			{0x00, 0x00,}, /* camera_input.bmControls[2] */
		},

		// ---- Output Terminal Descriptor ----
		{
			sizeof(USB_UVC_VC_OutputTerminalDescriptor), /* camera_output.bLength */
			USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* camera_output.bDescriptorType */
			USB_UVC_DESCRIPTOR_SUBTYPE_VC_OUTPUT_TERMINAL, /* camera_output.bDescriptorSubtype */
			ENTITY_ID_OUTPUT, /* camera_output.bTerminalID */
			USB_UVC_TT_STREAMING, /* camera_output.wTerminalType */
			0x00, /* camera_output.bAssocTerminal */
			ENTITY_ID_PROCESSING, /* camera_output.bSourceID */
			0x00, /* camera_output.iTerminal */
		},

		// ---- Processing Unit Descriptor ----
		{
			sizeof(USB_UVC_VC_ProcessingUnitDescriptor(2)), /* camera_proc_unit.bLength */
			USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* camera_proc_unit.bDescriptorType */
			USB_UVC_DESCRIPTOR_SUBTYPE_VC_PROCESSING_UNIT, /* camera_proc_unit.bDescriptorSubtype */
			ENTITY_ID_PROCESSING, /* camera_proc_unit.bUnitID */
			ENTITY_ID_CAMERA, /* camera_proc_unit.bSourceID */
			0x0000, /* camera_proc_unit.wMaxMultiplier */
			0x02, /* camera_proc_unit.bControlSize */
			{0x00, 0x00,}, /* camera_proc_unit.bmControls[3] */
			0x00, /* camera_proc_unit.iProcessing */
			//0x06, /* camera_proc_unit.bmVideoStandards */
		},
	},

	// ---- ENDPOINT DESCRIPTOR ----
	{
		sizeof(USB_UVC_VC_StandardInterruptEndpointDescriptor), /* endpoint_int_in.bLength */
		USB_DESCRIPTOR_TYPE_ENDPOINT, /* endpoint_int_in.bDescriptorType */
		USB_ENDPOINT_DESCRIPTOR_EPADDR_IN | UVC_EP_INTERRUPT, /* endpoint_int_in.bEndpointAddress */
		USB_ENDPOINT_DESCRIPTOR_ATTR_INTERRUPT, /* endpoint_int_in.bmAttributes */
		UVC_INTERRUPT_EP_SIZE, /* endpoint_int_in.wMaxPacketSize */
		0x08, /* endpoint_int_in.bInterval */
	},

	// ---- Class Specific ENDPOINT DESCRIPTOR ----
	{
		sizeof(USB_UVC_VC_CSEndpointDescriptor), /* endpoint_int_descriptor.bLength */
		USB_UVC_DESCRIPTOR_TYPE_CS_ENDPOINT, /* endpoint_int_descriptor.bDescriptorType */
		USB_UVC_DESCRIPTOR_SUBTYPE_EP_INTERRUPT, /* endpoint_int_descriptor.bDescriptorSubType */
		UVC_INTERRUPT_EP_SIZE, /* endpoint_int_descriptor.wMaxTransferSize */
	},

	// ---- Standard Video Streaming Interface Descriptor ----
	{
		sizeof(USB_UVC_VS_StandardInterfaceDescriptor), /* interface_video_stream.bLength */
		USB_DESCRIPTOR_TYPE_INTERFACE, /* interface_video_stream.bDescriptorType */
		1, /* interface_video_stream.bInterfaceNumber */
		0x00, /* interface_video_stream.bAlternateSetting */
#ifdef USB_ENDPOINT_USE_ISOC
		0x00, /* interface_video_stream.bNumEndpoints */
#else // !USB_ENDPOINT_USE_ISOC
		0x01, /* interface_video_stream.bNumEndpoints */
#endif // USB_ENDPOINT_USE_ISOC
		USB_CLASS_VIDEO, /* interface_video_stream.bInterfaceClass */
		USB_SUBCLASS_VIDEO_VIDEOSTREAMING, /* interface_video_stream.bInterfaceSubClass */
		USB_PROTOCOL_VIDEO_UNDEFINED, /* interface_video_stream.bInterfaceProtocol */
		0x00 /* interface_video_stream.iInterface */
	},

	{
		// ---- Class-specific Video Streaming Input Header Descriptor ----
		{
			sizeof(USB_UVC_VS_CSInterfaceInputHeaderDescriptor(FORMAT_INDEX_HS_COUNT)), /* vs_header.bLength */
			USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* vs_header.bDescriptorType */
			USB_UVC_DESCRIPTOR_SUBTYPE_VS_INPUT_HEADER, /* vs_header.bDescriptorSubType */
			FORMAT_INDEX_HS_COUNT, /* vs_header.bNumFormats */
			sizeof(struct UVC_VS_config_descriptor_hs), /* vs_header.wTotalLength */
			USB_ENDPOINT_DESCRIPTOR_EPADDR_IN | UVC_EP_DATA_IN, /* vs_header.bEndpointAddress */
			0x00, /* vs_header.bmInfo */
			0x03, /* vs_header.bTerminalLink */
			0x00, /* vs_header.bStillCaptureMethod */
			0x00, /* vs_header.bTriggerSupport */
			0x00, /* vs_header.bTriggerUsage */
			0x01, /* vs_header.bControlSize */
			{
				0x00, /* vs_header.bmaControls format 0 */
			},
		},

		// ---- Class specific Uncompressed VS Format Descriptor ----
		{
			sizeof(USB_UVC_VS_UncompressedVideoFormatDescriptor), /* format.bLength */
			USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* format.bDescriptorType */
			USB_UVC_DESCRIPTOR_SUBTYPE_VS_FORMAT_UNCOMPRESSED, /* format.bDescriptorSubType */
			FORMAT_INDEX_TYPE_UNCOMPRESSED, /* format.bFormatIndex */
			FRAME_INDEX_HS_UC_COUNT, /* format.bNumFrameDescriptors */
			PAYLOAD_FORMAT_UNCOMPRESSED, /* format.guidFormat[16] */
			PAYLOAD_BBP_UNCOMPRESSED, /* format.bBitsPerPixel */
			FRAME_INDEX_HS_UC_DEFAULT, /* format.bDefaultFrameIndex */
			FRAME_RATIO_X, /* format.bAspectRatioX */
			FRAME_RATIO_Y, /* format.bAspectRatioY */
			0x00, /* format.bmInterlaceFlags */
			0x00, /* format.bCopyProtect */
		},

#ifdef UC_QVGA
		// ---- Class specific Uncompressed VS Frame Descriptor ----
		// 320 x 240 pixels, 15 fps. QVGA.
		{
			sizeof(USB_UVC_VS_UncompressedVideoFrameDescriptorDiscrete(1)), /* frame.bLength */
			USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* frame.bDescriptorType */
			USB_UVC_DESCRIPTOR_SUBTYPE_VS_FRAME_UNCOMPRESSED, /* frame.bDescriptorSubType */
			FRAME_INDEX_HS_UC_QVGA, /* frame.bFrameIndex */
			0x00, /* frame.bmCapabilities */
			CAMERA_FRAME_WIDTH_QVGA, /* frame.wWidth */
			CAMERA_FRAME_HEIGHT_QVGA, /* frame.wHeight */
			FRAME_INDEX_HS_UC_QVGA_SIZE * FRAME_INDEX_HS_UC_QVGA_FRAME_RATE * 8, /* frame.dwMinBitRate */
			FRAME_INDEX_HS_UC_QVGA_SIZE * FRAME_INDEX_HS_UC_QVGA_FRAME_RATE * 8, /* frame.dwMaxBitRate */
			FRAME_INDEX_HS_UC_QVGA_SIZE, /* frame.dwMaxVideoFrameBufferSize */
			FRAME_INDEX_HS_UC_QVGA_FRAME_INTERVAL, /* frame.dwDefaultFrameInterval */
			0x01, /* frame.bFrameIntervalType */
			{FRAME_INDEX_HS_UC_QVGA_FRAME_INTERVAL, }, /* frame.dwFrameInterval */
		},
#endif

#ifdef UC_VGA
		// ---- Class specific Uncompressed VS Frame Descriptor ----
		// 640 x 480 pixels, 15 fps. VGA.
		{
			sizeof(USB_UVC_VS_UncompressedVideoFrameDescriptorDiscrete(1)), /* frame.bLength */
			USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* frame.bDescriptorType */
			USB_UVC_DESCRIPTOR_SUBTYPE_VS_FRAME_UNCOMPRESSED, /* frame.bDescriptorSubType */
			FRAME_INDEX_HS_UC_VGA, /* frame.bFrameIndex */
			0x00, /* frame.bmCapabilities */
			CAMERA_FRAME_WIDTH_VGA, /* frame.wWidth */
			CAMERA_FRAME_HEIGHT_VGA, /* frame.wHeight */
			FRAME_INDEX_HS_UC_VGA_SIZE * FRAME_INDEX_HS_UC_VGA_FRAME_RATE * 8, /* frame.dwMinBitRate */
			FRAME_INDEX_HS_UC_VGA_SIZE * FRAME_INDEX_HS_UC_VGA_FRAME_RATE * 8, /* frame.dwMaxBitRate */
			FRAME_INDEX_HS_UC_VGA_SIZE, /* frame.dwMaxVideoFrameBufferSize */
			FRAME_INDEX_HS_UC_VGA_FRAME_INTERVAL, /* frame.dwDefaultFrameInterval */
			0x01, /* frame.bFrameIntervalType */
			{FRAME_INDEX_HS_UC_VGA_FRAME_INTERVAL, }, /* frame.dwFrameInterval */
		},
#endif

		// ---- Class specific Color Matching Descriptor ----
		{
				sizeof(USB_UVC_ColorMatchingDescriptor),  /* desc.bLength */
				USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* desc.bDescriptorType */
				USB_UVC_DESCRIPTOR_SUBTYPE_VS_COLORFORMAT, /* desc.bDescriptorSubType */
				1, /* desc.bColorPrimaries */
				1, /* desc.bTransferCharacteristics */
				4, /* desc.bMatrixCoefficients */
		},
	},

#ifndef USB_ENDPOINT_USE_ISOC
	// ---- ENDPOINT DESCRIPTOR ----
	{
		sizeof(USB_UVC_VS_BulkVideoDataEndpointDescriptor), /* endpoint_bulk_in.bLength */
		USB_DESCRIPTOR_TYPE_ENDPOINT, /* endpoint_bulk_in.bDescriptorType */
		USB_ENDPOINT_DESCRIPTOR_EPADDR_IN | UVC_EP_DATA_IN, /* endpoint_bulk_in.bEndpointAddress */
		USB_ENDPOINT_DESCRIPTOR_ATTR_BULK //|
			/*USB_ENDPOINT_DESCRIPTOR_ISOCHRONOUS_ASYNCHRONOUS*/, /* endpoint_bulk_in.bmAttributes */
		/*USB_ENDPOINT_DESCRIPTOR_MAXPACKET_ADDN_TRANSACTION_1 |*/ UVC_DATA_EP_SIZE_HS, /* endpoint_bulk_in.wMaxPacketSize */
		0x00, /* endpoint_bulk_in.bInterval */
	},
#endif // !USB_ENDPOINT_USE_ISOC

#ifdef USB_ENDPOINT_USE_ISOC
	// ---- Standard Video Streaming Interface Descriptor ----
	{
		sizeof(USB_UVC_VS_StandardInterfaceDescriptor), /* interface_video_stream.bLength */
		USB_DESCRIPTOR_TYPE_INTERFACE, /* interface_video_stream.bDescriptorType */
		1, /* interface_video_stream.bInterfaceNumber */
		0x01, /* interface_video_stream.bAlternateSetting */
		0x01, /* interface_video_stream.bNumEndpoints */
		USB_CLASS_VIDEO, /* interface_video_stream.bInterfaceClass */
		USB_SUBCLASS_VIDEO_VIDEOSTREAMING, /* interface_video_stream.bInterfaceSubClass */
		USB_PROTOCOL_VIDEO_UNDEFINED, /* interface_video_stream.bInterfaceProtocol */
		0x00 /* interface_video_stream.iInterface */
	},

	// ---- ENDPOINT DESCRIPTOR ----
	{
		sizeof(USB_UVC_VS_IsochronousVideoDataEndpointDescriptor), /* endpoint_bulk_in.bLength */
		USB_DESCRIPTOR_TYPE_ENDPOINT, /* endpoint_bulk_in.bDescriptorType */
		USB_ENDPOINT_DESCRIPTOR_EPADDR_IN | UVC_EP_DATA_IN, /* endpoint_bulk_in.bEndpointAddress */
		USB_ENDPOINT_DESCRIPTOR_ATTR_ISOCHRONOUS /*|
			USB_ENDPOINT_DESCRIPTOR_ISOCHRONOUS_ASYNCHRONOUS*/, /* endpoint_bulk_in.bmAttributes */
		/*USB_ENDPOINT_DESCRIPTOR_MAXPACKET_ADDN_TRANSACTION_1 |*/ UVC_DATA_EP_SIZE_HS, /* endpoint_bulk_in.wMaxPacketSize */
		0x01, /* endpoint_bulk_in.bInterval */
	},
#endif // USB_ENDPOINT_USE_ISOC

#ifdef USB_INTERFACE_USE_DFU
	// ---- INTERFACE DESCRIPTOR for DFU Interface ----
	{
		sizeof(USB_interface_descriptor), /* dfu_interface.bLength */
		USB_DESCRIPTOR_TYPE_INTERFACE, /* dfu_interface.bDescriptorType */
		DFU_USB_INTERFACE_RUNTIME, /* dfu_interface.bInterfaceNumber */
		0x00, /* dfu_interface.bAlternateSetting */
		0x00, /* dfu_interface.bNumEndpoints */
		USB_CLASS_APPLICATION, /* dfu_interface.bInterfaceClass */ // bInterfaceClass: Application Specific Class
		USB_SUBCLASS_DFU, /* dfu_interface.bInterfaceSubClass */ // bInterfaceSubClass: Device Firmware Update
		USB_PROTOCOL_DFU_RUNTIME, /* dfu_interface.bInterfaceProtocol */ // bInterfaceProtocol: Runtime Protocol
		0x05 /* dfu_interface.iInterface */       // * iInterface: "DFU Interface"
	},

	// ---- FUNCTIONAL DESCRIPTOR for DFU Interface ----
	{
		sizeof(USB_dfu_functional_descriptor), /* dfu_functional.bLength */
		USB_DESCRIPTOR_TYPE_DFU_FUNCTIONAL, /* dfu_functional.bDescriptorType */
		DFU_ATTRIBUTES, /* dfu_functional.bmAttributes */  	// bmAttributes
		DFU_TIMEOUT, /* dfu_functional.wDetatchTimeOut */ // wDetatchTimeOut
		DFU_TRANSFER_SIZE, /* dfu_functional.wTransferSize */     // wTransferSize
		USB_BCD_VERSION_DFU_1_1 /* dfu_functional.bcdDfuVersion */ // bcdDfuVersion: DFU Version 1.1
	}
#endif // USB_INTERFACE_USE_DFU
};

DESCRIPTOR_QUALIFIER struct config_descriptor_uvc_fs config_descriptor_uvc_fs =
{
	{
		sizeof(USB_configuration_descriptor), /* configuration.bLength */
		USB_DESCRIPTOR_TYPE_CONFIGURATION, /* configuration.bDescriptorType */
		sizeof(struct config_descriptor_uvc_fs), /* configuration.wTotalLength */
		0x02, /* configuration.bNumInterfaces */
		0x01, /* configuration.bConfigurationValue */
		0x00, /* configuration.iConfiguration */
		USB_CONFIG_BMATTRIBUTES_VALUE, /* configuration.bmAttributes */
		//0xFA, /* configuration.bMaxPower */           // 500mA
		0, /* configuration.bMaxPower */           // 0mA
	},

	{
		sizeof(USB_UVC_interface_association_descriptor), /* interface_association.bLength */
		USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION, /* interface_association.bDescriptorType */
		0, /* interface_association.bFirstInterface */
		2, /* interface_association.bInterfaceCount */
		USB_CLASS_VIDEO, /* interface_association.bFunctionClass */
		USB_SUBCLASS_VIDEO_INTERFACE_COLLECTION, /* interface_association.bFunctionSubClass */
		USB_PROTOCOL_VIDEO_UNDEFINED, /* interface_association.bFunctionProtocol */
		2, /* interface_association.iFunction */  // "FT900 UVC"
	},

	// ---- INTERFACE DESCRIPTOR ----
	{
		sizeof(USB_UVC_VC_StandardInterfaceDescriptor), /* interface_video_control.bLength */
		USB_DESCRIPTOR_TYPE_INTERFACE, /* interface_video_control.bDescriptorType */
		0, /* interface_video_control.bInterfaceNumber */
		0x00, /* interface_video_control.bAlternateSetting */
		0x01, /* interface_video_control.bNumEndpoints */
		USB_CLASS_VIDEO, /* interface_video_control.bInterfaceClass */ // UVC Class
		USB_SUBCLASS_VIDEO_VIDEOCONTROL, /* interface_video_control.bInterfaceSubClass */ // Abstract Control Model
		USB_PROTOCOL_VIDEO_UNDEFINED, /* interface_video_control.bInterfaceProtocol */ // No built-in protocol
		2, /* interface_video_control.iInterface */ // Same as IAD iFunction.
	},

	{
			// ---- Class specific VC Interface Header Descriptor ----
			{
				sizeof(USB_UVC_VC_CSInterfaceHeaderDescriptor(1)), /* vc_header.bLength */
				USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* vc_header.bDescriptorType */
				USB_UVC_DESCRIPTOR_SUBTYPE_VC_HEADER, /* vc_header.bDescriptorSubtype */
				(USB_VIDEO_CLASS_VERSION_MAJOR << 8) | (USB_VIDEO_CLASS_VERSION_MINOR << 4), /* vc_header.bcdUVC */
				sizeof(struct UVC_VC_config_descriptor), /* vc_header.wTotalLength */
				CLK_FREQ_48MHz, /* vc_header.dwClockFrequency */
				0x01, /* vc_header.bInCollection */
				{0x01, }, /* vc_header.baInterfaceNr */
			},

			// ---- Input Terminal Descriptor - Camera ----
			{
				sizeof(USB_UVC_VC_CameraTerminalDescriptor(2)), /* camera_input.bLength */
				USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* camera_input.bDescriptorType */
				USB_UVC_DESCRIPTOR_SUBTYPE_VC_INPUT_TERMINAL, /* camera_input.bDescriptorSubtype */
				ENTITY_ID_CAMERA, /* camera_input.bTerminalID */
				USB_UVC_ITT_CAMERA, /* camera_input.wTerminalType */
				0x00, /* camera_input.bAssocTerminal */
				0x00, /* camera_input.iTerminal */
				0x0000, /* camera_input.wObjectiveFocalLengthMin */
				0x0000, /* camera_input.wObjectiveFocalLengthMax */
				0x0000, /* camera_input.wOcularFocalLength */
				0x02, /* camera_input.bControlSize */
				{0x00, 0x00, }, /* camera_input.bmControls[2] */
			},

			// ---- Output Terminal Descriptor ----
			{
				sizeof(USB_UVC_VC_OutputTerminalDescriptor), /* camera_output.bLength */
				USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* camera_output.bDescriptorType */
				USB_UVC_DESCRIPTOR_SUBTYPE_VC_OUTPUT_TERMINAL, /* camera_output.bDescriptorSubtype */
				ENTITY_ID_OUTPUT, /* camera_output.bTerminalID */
				USB_UVC_TT_STREAMING, /* camera_output.wTerminalType */
				0x00, /* camera_output.bAssocTerminal */
						ENTITY_ID_PROCESSING, /* camera_output.bSourceID */
				0x00, /* camera_output.iTerminal */
			},

			// ---- Processing Unit Descriptor ----
			{
				sizeof(USB_UVC_VC_ProcessingUnitDescriptor(2)), /* camera_proc_unit.bLength */
				USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* camera_proc_unit.bDescriptorType */
				USB_UVC_DESCRIPTOR_SUBTYPE_VC_PROCESSING_UNIT, /* camera_proc_unit.bDescriptorSubtype */
				ENTITY_ID_PROCESSING, /* camera_proc_unit.bUnitID */
				ENTITY_ID_CAMERA, /* camera_proc_unit.bSourceID */
				0x0000, /* camera_proc_unit.wMaxMultiplier */
				0x02, /* camera_proc_unit.bControlSize */
				{0x00, 0x00, }, /* camera_proc_unit.bmControls[3] */
				0x00, /* camera_proc_unit.iProcessing */
				//0x06, /* camera_proc_unit.bmVideoStandards */
			},
	},

	// ---- ENDPOINT DESCRIPTOR ----
	{
		sizeof(USB_UVC_VC_StandardInterruptEndpointDescriptor), /* endpoint_int_in.bLength */
		USB_DESCRIPTOR_TYPE_ENDPOINT, /* endpoint_int_in.bDescriptorType */
		USB_ENDPOINT_DESCRIPTOR_EPADDR_IN | UVC_EP_INTERRUPT, /* endpoint_int_in.bEndpointAddress */
		USB_ENDPOINT_DESCRIPTOR_ATTR_INTERRUPT, /* endpoint_int_in.bmAttributes */
		UVC_INTERRUPT_EP_SIZE, /* endpoint_int_in.wMaxPacketSize */
		0x08, /* endpoint_int_in.bInterval */
	},

	// ---- Class Specific ENDPOINT DESCRIPTOR ----
	{
		sizeof(USB_UVC_VC_CSEndpointDescriptor), /* endpoint_int_descriptor.bLength */
		USB_UVC_DESCRIPTOR_TYPE_CS_ENDPOINT, /* endpoint_int_descriptor.bDescriptorType */
		USB_UVC_DESCRIPTOR_SUBTYPE_EP_INTERRUPT, /* endpoint_int_descriptor.bDescriptorSubType */
		UVC_INTERRUPT_EP_SIZE, /* endpoint_int_descriptor.wMaxTransferSize */
	},

#ifdef USB_INTERFACE_USE_DFU
	// ---- INTERFACE DESCRIPTOR for DFU Interface ----
	{
		sizeof(USB_interface_descriptor), /* dfu_interface.bLength */
		USB_DESCRIPTOR_TYPE_INTERFACE, /* dfu_interface.bDescriptorType */
		DFU_USB_INTERFACE_RUNTIME, /* dfu_interface.bInterfaceNumber */
		0x00, /* dfu_interface.bAlternateSetting */
		0x00, /* dfu_interface.bNumEndpoints */
		USB_CLASS_APPLICATION, /* dfu_interface.bInterfaceClass */ // bInterfaceClass: Application Specific Class
		USB_SUBCLASS_DFU, /* dfu_interface.bInterfaceSubClass */ // bInterfaceSubClass: Device Firmware Update
		USB_PROTOCOL_DFU_RUNTIME, /* dfu_interface.bInterfaceProtocol */ // bInterfaceProtocol: Runtime Protocol
		0x05 /* dfu_interface.iInterface */       // * iInterface: "DFU Interface"
	},

	// ---- FUNCTIONAL DESCRIPTOR for DFU Interface ----
	{
		sizeof(USB_dfu_functional_descriptor), /* dfu_functional.bLength */
		USB_DESCRIPTOR_TYPE_DFU_FUNCTIONAL, /* dfu_functional.bDescriptorType */
		DFU_ATTRIBUTES, /* dfu_functional.bmAttributes */  	// bmAttributes
		DFU_TIMEOUT, /* dfu_functional.wDetatchTimeOut */ // wDetatchTimeOut
		DFU_TRANSFER_SIZE, /* dfu_functional.wTransferSize */     // wTransferSize
		USB_BCD_VERSION_DFU_1_1 /* dfu_functional.bcdDfuVersion */ // bcdDfuVersion: DFU Version 1.1
	}
#endif // USB_INTERFACE_USE_DFU
};
//@}

#ifdef USB_INTERFACE_USE_DFU
/**
 @name wcid_feature_runtime
 @brief WCID Compatible ID for DFU interface in runtime.
 */
//@{
DESCRIPTOR_QUALIFIER USB_WCID_feature_descriptor wcid_feature_runtime =
{
	sizeof(struct USB_WCID_feature_descriptor), /* dwLength */
	USB_MICROSOFT_WCID_VERSION, /* bcdVersion */
	USB_MICROSOFT_WCID_FEATURE_WINDEX_COMPAT_ID, /* wIndex */
	1, /* bCount */
	{0, 0, 0, 0, 0, 0, 0,}, /* rsv1 */
	DFU_USB_INTERFACE_RUNTIME, /* bFirstInterfaceNumber */
	1, /* rsv2 - set to 1 */
	{'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,}, /* compatibleID[8] */
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,}, /* subCompatibleID[8] */
	{0, 0, 0, 0, 0, 0,}, /* rsv3[6] */
};
#endif // USB_INTERFACE_USE_DFU

/**
 @name device_descriptor_dfumode
 @brief Device descriptor for DFU Mode.
 */
DESCRIPTOR_QUALIFIER USB_device_descriptor device_descriptor_dfumode =
{
		0x12, /* bLength */
		USB_DESCRIPTOR_TYPE_DEVICE, /* bDescriptorType */
		USB_BCD_VERSION_2_0, /* bcdUSB */          // V2.00
		USB_CLASS_DEVICE, /* bDeviceClass */       // Defined in interface
		USB_SUBCLASS_DEVICE, /* bDeviceSubClass */ // Defined in interface
		USB_PROTOCOL_DEVICE, /* bDeviceProtocol */ // Defined in interface
		USB_CONTROL_EP_MAX_PACKET_SIZE, /* bMaxPacketSize0 */    // 8
		USB_VID_FTDI, /* idVendor */   // idVendor: 0x0403 (FTDI)
		DFU_USB_PID_DFUMODE, /* idProduct */ // idProduct: 0x0fee
		0x0101, /* bcdDevice */        // 1.1
		0x01, /* iManufacturer */      // Manufacturer
		0x04, /* iProduct */           // Product
		0x03, /* iSerialNumber */      // Serial Number
		0x01, /* bNumConfigurations */
};

/**
 @name config_descriptor_dfumode
 @brief Config descriptor for DFU Mode.
 */
//@{
struct PACK config_descriptor_dfumode
{
	USB_configuration_descriptor configuration;
	USB_interface_descriptor dfu_interface;
	USB_dfu_functional_descriptor dfu_functional;
};

DESCRIPTOR_QUALIFIER struct config_descriptor_dfumode config_descriptor_dfumode =
{
	{
		0x09, /* configuration.bLength */
		USB_DESCRIPTOR_TYPE_CONFIGURATION, /* configuration.bDescriptorType */
		sizeof(struct config_descriptor_dfumode), /* configuration.wTotalLength */
		0x01, /* configuration.bNumInterfaces */
		0x01, /* configuration.bConfigurationValue */
		0x00, /* configuration.iConfiguration */
		USB_CONFIG_BMATTRIBUTES_VALUE, /* configuration.bmAttributes */
		//0xFA /* configuration.bMaxPower */ // 500mA
		0 /* configuration.bMaxPower */ // 0mA
	},

	// ---- INTERFACE DESCRIPTOR for DFU Interface ----
	{
		0x09, /* dfu_interface.bLength */
		USB_DESCRIPTOR_TYPE_INTERFACE, /* dfu_interface.bDescriptorType */
		DFU_USB_INTERFACE_DFUMODE, /* dfu_interface.bInterfaceNumber */
		0x00, /* dfu_interface.bAlternateSetting */
		0x00, /* dfu_interface.bNumEndpoints */
		USB_CLASS_APPLICATION, /* dfu_interface.bInterfaceClass */ // bInterfaceClass: Application Specific Class
		USB_SUBCLASS_DFU, /* dfu_interface.bInterfaceSubClass */ // bInterfaceSubClass: Device Firmware Update
		USB_PROTOCOL_DFU_DFUMODE, /* dfu_interface.bInterfaceProtocol */ // bInterfaceProtocol: Runtime Protocol
		0x05 /* dfu_interface.iInterface */
	},

	// ---- FUNCTIONAL DESCRIPTOR for DFU Interface ----
	{
		0x09, /* dfu_functional.bLength */
		USB_DESCRIPTOR_TYPE_DFU_FUNCTIONAL, /* dfu_functional.bDescriptorType */
		DFU_ATTRIBUTES, /* dfu_functional.bmAttributes */  	// bmAttributes
		DFU_TIMEOUT, /* dfu_functional.wDetatchTimeOut */ // wDetatchTimeOut
		DFU_TRANSFER_SIZE, /* dfu_functional.wTransferSize */     // wTransferSize
		USB_BCD_VERSION_DFU_1_1 /* dfu_functional.bcdDfuVersion */ // bcdDfuVersion: DFU Version 1.1
	}
};
//@}

/**
 @name wcid_feature_dfumode
 @brief WCID Compatible ID for DFU interface in DFU mode.
 */
//@{
DESCRIPTOR_QUALIFIER USB_WCID_feature_descriptor wcid_feature_dfumode =
{
	sizeof(struct USB_WCID_feature_descriptor), /* dwLength */
	USB_MICROSOFT_WCID_VERSION, /* bcdVersion */
	USB_MICROSOFT_WCID_FEATURE_WINDEX_COMPAT_ID, /* wIndex */
	1, /* bCount */
	{0, 0, 0, 0, 0, 0, 0,}, /* rsv1 */
	DFU_USB_INTERFACE_DFUMODE, /* bFirstInterfaceNumber */
	1, /* rsv2 - set to 1 */
	{'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,}, /* compatibleID[8] */
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,}, /* subCompatibleID[8] */
	{0, 0, 0, 0, 0, 0,}, /* rsv3[6] */
};

/* GLOBAL VARIABLES ****************************************************************/

/* LOCAL VARIABLES *****************************************************************/

/**
 @brief Millisecond counter
 @details Count-up timer to provide the elapsed time for network operations.
 */
static uint32_t milliseconds = 0;

/**
 @brief Active Alternate Setting
 @details Current active alternate setting for the USB interface.
 */
static uint8_t usb_alt = 0;

/**
 @brief Storage for Configuration Descriptors.
 @details Configuration descriptors may need to be modified to turn from type
 USB_DESCRIPTOR_TYPE_CONFIGURATION to USB_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION.
 */
union config_descriptor_buffer {
	struct config_descriptor_uvc_hs hs;
	struct config_descriptor_uvc_fs fs;
} config_descriptor_buffer;

/** @brief Error control generated from UVC requests.
 */
uint8_t uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_NO_ERROR;

/** @brief Negotiated probe and commit states.
 *  @details The Probe settings are used to negotiate an acceptable frame, format
 *  and data rate for the device. A negotiated setting obtained through probes is
 *  then committed. The committed setting is used for streaming.
 */
//@{
/** @brief Current probe state used during negotiation.
 */
USB_UVC_VideoProbeAndCommitControls uvc_probe;

/** @brief Current committed state used after negotiation.
 */
USB_UVC_VideoProbeAndCommitControls uvc_commit;

/** @brief Default state used at start of negotiation. High-speed.
 */
const USB_UVC_VideoProbeAndCommitControls uvc_probe_def_hs = {
		USB_UVC_VS_PROBE_COMMIT_CONTROL_BMHINT_FRAMINGINFO, /* bmHint */
		FORMAT_INDEX_TYPE_UNCOMPRESSED, /*  bFormatIndex */
		FRAME_INDEX_HS_UC_MIN, /*  bFrameIndex */
		FRAME_INDEX_HS_UC_MIN_FRAME_INTERVAL, /*  dwFrameInterval */
		0, /*  wKeyFrameRate */
		0, /*  wPFrameRate */
		0, /*  wCompQuality */
		0, /*  wCompWindowSize */
		0, /*  wDelay */
		FRAME_INDEX_HS_UC_MIN_SIZE, /*  dwMaxVideoFrameSize */
		FRAME_INDEX_HS_UC_MIN_TRANSFER_SIZE + sizeof(USB_UVC_Payload_Header), /*  dwMaxPayloadTransferSize */
		CLK_FREQ_48MHz,    /*  dwClockFrequency */
		USB_UVC_VS_PROBE_COMMIT_CONTROL_BMFRAMINGINFO_FRAMEIDFIELD |
			USB_UVC_VS_PROBE_COMMIT_CONTROL_BMFRAMINGINFO_EOFFIELD, /*  bmFramingInfo */
		USB_VIDEO_CLASS_VERSION_MINOR, /*  bPreferedVersion */
		USB_VIDEO_CLASS_VERSION_MINOR, 	/*  bMinVersion */
		USB_VIDEO_CLASS_VERSION_MINOR,	/*  bMaxVersion */
};
//@}

/** @brief Current device speed.
 */
USBD_DEVICE_SPEED usb_speed;

/* MACROS **************************************************************************/

/* LOCAL FUNCTIONS / INLINES *******************************************************/

/** @name tfp_putc
 *  @details Machine dependent putc function for tfp_printf (tinyprintf) library.
 *  @param p Parameters (machine dependent)
 *  @param c The character to write
 */
void tfp_putc(void* p, char c)
{
	uart_write((ft900_uart_regs_t*)p, (uint8_t)c);
}

/** @brief Returns the current millisecond country
 *  @returns A count of milliseconds
 */
uint32_t millis(void)
{
	return milliseconds;
}



/**
 * I2C Slave
 */

/* i2cs_dev variables */
volatile uint8_t *i2cs_dev_buffer;
volatile size_t i2cs_dev_buffer_size;
volatile uint8_t i2cs_dev_buffer_ptr;
volatile uint8_t i2cs_dev_registers[3] =
{
	0,
	0,
	0
};

volatile uint8_t led1 = 0xFF;
volatile uint8_t led2 = 0xFF;
volatile uint8_t led3 = 0xFF;

void update_leds(void) {
	if (led1 != i2cs_dev_registers[0]) {
		led1 = i2cs_dev_registers[0];
		uint8_t led1r = !(led1 & 0x1);
		uint8_t led1g = !(led1 & 0x2);
		uint8_t led1b = !(led1 & 0x4);
		gpio_write(55, led1r);
		gpio_write(29, led1g);
		gpio_write(45, led1b);
//		BRIDGE_DEBUG_PRINTF("Setting LED1 to R%x G%x B%x\r\n", led1r, led1g, led1b);
	}
	if (led2 != i2cs_dev_registers[1]) {
		led2 = i2cs_dev_registers[1];
		uint8_t led2r = !(led2 & 0x1);
		uint8_t led2g = !(led2 & 0x2);
		uint8_t led2b = !(led2 & 0x4);
		gpio_write(56, led2r);
		gpio_write(57, led2g);
		gpio_write(58, led2b);
//		BRIDGE_DEBUG_PRINTF("Setting LED2 to R%x G%x B%x\r\n", led2r, led2g, led2b);
	}
	if (led3 != i2cs_dev_registers[2]) {
		led3 = i2cs_dev_registers[2];
		uint8_t led3r = !(led3 & 0x1);
		uint8_t led3g = !(led3 & 0x2);
		uint8_t led3b = !(led3 & 0x4);
		gpio_write(52, led3r);
		gpio_write(53, led3g);
		gpio_write(54, led3b);
//		BRIDGE_DEBUG_PRINTF("Setting LED3 to R%x G%x B%x\r\n", led3r, led3g, led3b);
	}
}

void i2cs_dev_ISR(void)
{
	static uint8_t rx_addr = 1;
	uint8_t status;

	if (i2cs_is_interrupted(MASK_I2CS_FIFO_INT_PEND_I2C_INT))
	{
		status = i2cs_get_status();

		/* For a write transaction... */
		if(status & MASK_I2CS_STATUS_RX_REQ)
		{
			/* If we are wanting for the Write address to appear... */
			if (rx_addr)
			{
				/* Read in the initial offset to read/write... */
				rx_addr = 0;
				i2cs_read((uint8_t *)(&i2cs_dev_buffer_ptr), 1);
			}
			else
			{
				/* Write the byte to the register buffer... */
				i2cs_read((uint8_t *)(&(i2cs_dev_buffer[i2cs_dev_buffer_ptr])), 1);
				i2cs_dev_buffer_ptr++;
			}

		}
		/* For a read transaction... */
		else if(status & MASK_I2CS_STATUS_TX_REQ)
		{
			/* Write the byte to the I2C bus... */
			i2cs_write((uint8_t *)(&(i2cs_dev_buffer[i2cs_dev_buffer_ptr])), 1);
			i2cs_dev_buffer_ptr++;
		}

		/* For the completion of a transaction... */
		else if (status & (MASK_I2CS_STATUS_REC_FIN | MASK_I2CS_STATUS_SEND_FIN))
		{
			/* Finished transaction, reset... */
			rx_addr = 1;
		}

		/* Wrap around */
		if (i2cs_dev_buffer_ptr > i2cs_dev_buffer_size)
			i2cs_dev_buffer_ptr = 0;
	}
}








void timer_ISR(void)
{
	if (timer_is_interrupted(timer_select_a))
	{
		milliseconds++;
	}
}

/* Power management ISR */
void powermanagement_ISR(void)
{
	if (SYS->PMCFG_H & MASK_SYS_PMCFG_HOST_RST_DEV)
	{
		// Clear Host Reset interrupt
		SYS->PMCFG_H = MASK_SYS_PMCFG_HOST_RST_DEV;
		USBD_wakeup();
	}

	if (SYS->PMCFG_H & MASK_SYS_PMCFG_HOST_RESUME_DEV)
	{
		// Clear Host Resume interrupt
		SYS->PMCFG_H = MASK_SYS_PMCFG_HOST_RESUME_DEV;
		USBD_resume();
	}
}

static int check =0;

void cam_ISR(void)
{
	static uint8_t *pbuffer;
	static uint16_t len;

	// Synchronise on the start of a frame.
	// If we are waiting for the VSYNC signal then flush all data.
	if (camera_vsync != 0)
	{
		// Read in a line of data from the camera.
		len = cam_available();
		if (len >= sample_threshold)
		{
			// Point to the current line in the camera_buffer.
			pbuffer = camera_buffer[camera_rx_buffer_line];

			// Stream data from the camera to camera_buffer.
			// This must be aligned to and be a multiple of 4 bytes.
			asm("streamin.l %0,%1,%2" \
			: \
			:"r"(pbuffer), "r"(&(CAM->CAM_REG3)), "r"(sample_threshold));

#ifdef SHOW_DEBUG_DIAGONAL_LINES
#ifndef PAYLOAD_COMPRESSED
			pbuffer[check++] = 0;
			pbuffer[check++] = 0;
			if (check >= sample_threshold) check = sample_threshold - 2;
#endif // !PAYLOAD_COMPRESSED
#endif // SHOW_DEBUG_DIAGONAL_LINES

			// Increment the number of lines available to read.
			// This will signal data is ready to transmit.
			camera_rx_data_avail++;
			camera_rx_buffer_line++;
			if (camera_rx_buffer_line == CAMERA_BUFFER_MAX_LINES)
			{
				// Wrap around in camera_buffer.
				camera_rx_buffer_line = 0;
			}
		}
	}
	else
	{
		cam_flush();
	}
}

void vsync_ISR(void)
{
	if (gpio_is_interrupted(8))
	{
		// Signal start of frame received. Will now wait for line data.
		camera_vsync = 1;
		check = 0;
	}
}

void wait_for_vsync()
{
	camera_vsync = 0;
	while (!camera_vsync){
		// Flush all waiting data in the camera buffer.
		cam_flush();
	}

	camera_rx_buffer_line = 0;
	camera_rx_data_avail = 0;
}

/**
 @brief      USB Set/Get Interface request handler
 @details    Handle standard requests from the host application
 for GetInterface and SetInterface requests.
 @param[in]	req - USB_device_request structure containing the
 SETUP portion of the request from the host.
 @return		status - USBD_OK if successful or USBD_ERR_*
 if there is an error or the bRequest is not
 supported.
 **/
//@{
int8_t setif_req_cb(USB_device_request *req)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;

	if (req->wIndex == 0)
	{
		// Interface 0 can only have an Alt Setting of zero.
		if (req->wValue == 0x00)
		{
			status = USBD_OK;
		}
	}
	else if (req->wIndex == 1)
	{
#ifdef USB_ENDPOINT_USE_ISOC
		// Interface 1 can change Alt Settings to zero or 1 in ISOC mode.
		if ((req->wValue == 0x00) || (req->wValue == 0x01))
		{
			usb_alt = LSB(req->wValue);
			status = USBD_OK;

			// Start or stop the camera depending on the alternate interface.
			camera_state_change = CAMERA_STREAMING_START;
		}
#else // !USB_ENDPOINT_USE_ISOC
		// Interface 1 can only have an Alt Setting of zero in BULK mode.
		if (req->wValue == 0x00)
		{
			status = USBD_OK;
		}
#endif // USB_ENDPOINT_USE_ISOC
	}

	return status;
}

int8_t getif_req_cb(USB_device_request *req, uint8_t *val)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;

	// Only interface 1 has Alt Settings in this application.
	if (req->wIndex == 0)
	{
		// Alt setting for interface 0 is always zero.
		*val = 0;
		status = USBD_OK;
	}
	else if (req->wIndex == 1)
	{
		// Alt setting for interface 1 can be changed.
		*val = usb_alt;
		status = USBD_OK;
	}

	return status;
}
//@}

/**
 @brief      USB DFU class request handler (runtime)
 @details    Handle class requests from the host application.
 @param[in]	req - USB_device_request structure containing the
 SETUP portion of the request from the host.
 @return		status - USBD_OK if successful or USBD_ERR_*
 if there is an error or the bRequest is not
 supported.
 **/
int8_t class_req_dfu_interface_runtime(USB_device_request *req)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;

	// Handle only DFU_DETATCH, DFU_GETSTATE and DFU_GETSTATUS
	// when in Runtime mode. Table 3.2 DFU_DETACH is mandatory
	// in Runtime mode, DFU_GETSTATE and DFU_GETSTATUS are
	// optional.
	switch (req->bRequest)
	{
	case USB_CLASS_REQUEST_DETACH:
		USBD_DFU_class_req_detach(req->wValue);
		status = USBD_OK;
		break;
	case USB_CLASS_REQUEST_GETSTATUS:
		USBD_DFU_class_req_getstatus(req->wLength);
		status = USBD_OK;
		break;
	case USB_CLASS_REQUEST_GETSTATE:
		USBD_DFU_class_req_getstate(req->wLength);
		status = USBD_OK;
		break;
	}
	return status;
}

/**
 @brief      USB DFU class request handler (DFU mode)
 @details    Handle class requests from the host application.
 @param[in]	req - USB_device_request structure containing the
 SETUP portion of the request from the host.
 @return		status - USBD_OK if successful or USBD_ERR_*
 if there is an error or the bRequest is not
 supported.
 **/
int8_t class_req_dfu_interface_dfumode(USB_device_request *req)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;

	// Handle remaining DFU class requests when in DFU Mode.
	// Table 3.2 DFU_DETACH is not supported in DFU Mode.
	switch (req->bRequest)
	{
	case USB_CLASS_REQUEST_DNLOAD:
		/* Block number passed in wValue gives the start address of
		 * to program based on the size of the transfer size.
		 */
		USBD_DFU_class_req_download(req->wValue * DFU_TRANSFER_SIZE,
				req->wLength);
		status = USBD_OK;
		break;

	case USB_CLASS_REQUEST_UPLOAD:
		/* Block number passed in wValue gives the start address of
		 * to program based on the size of the transfer size.
		 */
		USBD_DFU_class_req_upload(req->wValue * DFU_TRANSFER_SIZE,
				req->wLength);
		status = USBD_OK;
		break;

	case USB_CLASS_REQUEST_GETSTATUS:
		USBD_DFU_class_req_getstatus(req->wLength);
		status = USBD_OK;
		break;

	case USB_CLASS_REQUEST_GETSTATE:
		USBD_DFU_class_req_getstate(req->wLength);
		status = USBD_OK;
		break;
	case USB_CLASS_REQUEST_CLRSTATUS:
		USBD_DFU_class_req_clrstatus();
		status = USBD_OK;
		break;
	case USB_CLASS_REQUEST_ABORT:
		USBD_DFU_class_req_abort();
		status = USBD_OK;
		break;

	default:
		// Unknown or unsupported request.
		break;
	}
	return status;
}

int8_t class_req_interface_video_control(USB_device_request *req)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;
	uint8_t request = req->bRequest;
	uint8_t requestType = req->bmRequestType;
	uint8_t controlSelector = MSB(req->wValue);
	uint8_t entityID = MSB(req->wIndex);

	// Interface requests to the VideoControl interface
	// of the video function. Section 4.2.
	// Interface number is in LSB of wIndex
	if ((requestType & USB_BMREQUESTTYPE_DIR_MASK) ==
			USB_BMREQUESTTYPE_DIR_HOST_TO_DEV)
	{
		switch (request)
		{
		case USB_UVC_REQUEST_SET_CUR:
			// No SET_CUR supported for video control.
			uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;
			break;
		}
	}
	else
	{
		// Interface requests to the VideoControl interface
		// of the video function. Section 4.2.
		// Interface number is in LSB of wIndex
		switch (request)
		{
		case USB_UVC_REQUEST_GET_INFO:
			// All GET_INFO responses will say GET_CUR supported but SET_CUR not.
			{
				uint8_t inforesponse = USB_UVC_GET_INFO_RESPONSE_SUPPORTS_GET |
						USB_UVC_GET_INFO_RESPONSE_SUPPORTS_SET;
				USBD_transfer_ep0(USBD_DIR_IN, &inforesponse, sizeof(inforesponse), req->wLength);
				status = USBD_OK;
			}
			break;

		case USB_UVC_REQUEST_GET_CUR:
		case USB_UVC_REQUEST_GET_MIN:
		case USB_UVC_REQUEST_GET_MAX:
		case USB_UVC_REQUEST_GET_DEF:
			// Entity ID is in the MSB of wIndex
			switch (entityID)
			{
			// Virtual Entity ID of zero points to the "interface".
			case ENTITY_ID_NONE:
				// Control selector is in the MSB of wValue
				switch (controlSelector)
				{
				case USB_UVC_VC_VIDEO_POWER_MODE_CONTROL:
					{
						uint8_t powerresponse = 0;
						USBD_transfer_ep0(USBD_DIR_IN, &powerresponse, sizeof(powerresponse), req->wLength);
						status = USBD_OK;
					}
					break;
				case USB_UVC_VC_REQUEST_ERROR_CODE_CONTROL:
					{
						uint8_t errorresponse = uvc_error_control;
						uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_NO_ERROR;
						USBD_transfer_ep0(USBD_DIR_IN, &errorresponse, sizeof(errorresponse), req->wLength);
						status = USBD_OK;
					}
					break;
				default:
					uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_CONTROL;
					break;
				}
				break;

			case ENTITY_ID_CAMERA:
			case ENTITY_ID_PROCESSING:
			case ENTITY_ID_OUTPUT:
				uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_CONTROL;
				// STALL
				break;
			default:
				uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_UNIT;
				break;
			}
			break;

		case USB_UVC_REQUEST_GET_RES:
			{
				uint8_t resresponse = 0;
				USBD_transfer_ep0(USBD_DIR_IN, &resresponse, sizeof(resresponse), req->wLength);
				status = USBD_OK;
			}
			break;
		case USB_UVC_REQUEST_GET_LEN:
			{
				uint8_t lenresponse = 0;
				USBD_transfer_ep0(USBD_DIR_IN, &lenresponse, sizeof(lenresponse), req->wLength);
				status = USBD_OK;
			}
			break;

		case USB_UVC_REQUEST_SET_CUR_ALL:
		case USB_UVC_REQUEST_GET_CUR_ALL:
		case USB_UVC_REQUEST_GET_MIN_ALL:
		case USB_UVC_REQUEST_GET_MAX_ALL:
		case USB_UVC_REQUEST_GET_RES_ALL:
		case USB_UVC_REQUEST_GET_DEF_ALL:
		case USB_UVC_REQUEST_RC_UNDEFINED:
		default:
			uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;
			// STALL
			break;
		}

		if (status == USBD_OK)
		{
			USBD_transfer_ep0(USBD_DIR_OUT, NULL, 0, 0);
	}
	}

	return status;
}

int8_t class_vs_check_probecommit(USB_UVC_VideoProbeAndCommitControls *probecommit)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;

	uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;

	// Fix any read-only bits of bmFramingInfo for the host.

	// Framing Info does include a frame ID and an EOF flag.
	probecommit->bmFramingInfo |= (USB_UVC_VS_PROBE_COMMIT_CONTROL_BMFRAMINGINFO_FRAMEIDFIELD |
										USB_UVC_VS_PROBE_COMMIT_CONTROL_BMFRAMINGINFO_EOFFIELD);
	// The is (minor or point) version of the class specifications.
	probecommit->bPreferedVersion = USB_VIDEO_CLASS_VERSION_MINOR;
	probecommit->bMinVersion = USB_VIDEO_CLASS_VERSION_MINOR;
	probecommit->bMaxVersion = USB_VIDEO_CLASS_VERSION_MINOR;

	if (probecommit->wDelay == 0)
	{
		probecommit->wDelay = uvc_probe_def_hs.wDelay;
	}
	if (probecommit->dwClockFrequency == 0)
	{
		probecommit->dwClockFrequency = uvc_probe_def_hs.dwClockFrequency;
	}

	if (usb_speed == USBD_SPEED_HIGH)
	{
		// Check for valid format index set.
		if (probecommit->bFormatIndex == FORMAT_INDEX_TYPE_UNCOMPRESSED)
		{
			// Check for valid frame index set.
			if (probecommit->bmFramingInfo & USB_UVC_VS_PROBE_COMMIT_CONTROL_BMHINT_FRAMINGINFO)
			{
				// If framing info hint is set then check frame interval is supported.
				switch (probecommit->bFrameIndex)
				{
				case FRAME_INDEX_HS_UC_QVGA:
					if (probecommit->dwFrameInterval == FRAME_INDEX_HS_UC_QVGA_FRAME_INTERVAL)
					{
						status = USBD_OK;
					}
					break;
				case FRAME_INDEX_HS_UC_VGA:
					if (probecommit->dwFrameInterval == FRAME_INDEX_HS_UC_VGA_FRAME_INTERVAL)
					{
						status = USBD_OK;
					}
					break;
				}
			}
			else
			{
				status = USBD_OK;
			}

			// Set required parts of structure depending on the frame index.
			switch (probecommit->bFrameIndex)
			{
			case FRAME_INDEX_HS_UC_QVGA:
				probecommit->dwFrameInterval = FRAME_INDEX_HS_UC_QVGA_FRAME_INTERVAL;
				probecommit->dwMaxVideoFrameSize = FRAME_INDEX_HS_UC_QVGA_SIZE;
				probecommit->dwMaxPayloadTransferSize = FRAME_INDEX_HS_UC_QVGA_TRANSFER_SIZE + sizeof(USB_UVC_Payload_Header);
				break;
			case FRAME_INDEX_HS_UC_VGA:
				probecommit->dwFrameInterval = FRAME_INDEX_HS_UC_VGA_FRAME_INTERVAL;
				probecommit->dwMaxVideoFrameSize = FRAME_INDEX_HS_UC_VGA_SIZE;
				probecommit->dwMaxPayloadTransferSize = FRAME_INDEX_HS_UC_VGA_TRANSFER_SIZE + sizeof(USB_UVC_Payload_Header);
				break;
			}
		}
		else if (probecommit->bFormatIndex == FORMAT_INDEX_TYPE_MJPEG)
		{
		}
	}

	if (status == USBD_OK)
	{
		uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_NO_ERROR;
	}

	return status;
}

int8_t class_vs_probe_min_max(USB_UVC_VideoProbeAndCommitControls *uvc_probe_ret)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;
	const USB_UVC_VideoProbeAndCommitControls *uvc_probe_cur = &uvc_probe;

	uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;

	// bFormatIndex and bFrameIndex must have been specified by a SET_CUR or
	// the default setting.
	uvc_probe_ret->bFormatIndex = uvc_probe_cur->bFormatIndex;
	uvc_probe_ret->bFrameIndex = uvc_probe_cur->bFrameIndex;

	if (usb_speed == USBD_SPEED_HIGH)
	{
		// Check for valid format index set.
		if (uvc_probe_cur->bFormatIndex == FORMAT_INDEX_TYPE_UNCOMPRESSED)
		{

			// Set required parts of structure depending on the frame index.
			// There are only one minimum and maximum value per frame and format.
			// This greatly simplifies calculating the minumum and maximum values.
			switch (uvc_probe_cur->bFrameIndex)
			{
			case FRAME_INDEX_HS_UC_QVGA:
				uvc_probe_ret->dwFrameInterval = FRAME_INDEX_HS_UC_QVGA_FRAME_INTERVAL;
				uvc_probe_ret->dwMaxVideoFrameSize = FRAME_INDEX_HS_UC_QVGA_SIZE;
				uvc_probe_ret->dwMaxPayloadTransferSize = FRAME_INDEX_HS_UC_QVGA_TRANSFER_SIZE + sizeof(USB_UVC_Payload_Header);
				status = USBD_OK;
				break;
			case FRAME_INDEX_HS_UC_VGA:
				uvc_probe_ret->dwFrameInterval = FRAME_INDEX_HS_UC_VGA_FRAME_INTERVAL;
				uvc_probe_ret->dwMaxVideoFrameSize = FRAME_INDEX_HS_UC_VGA_SIZE;
				uvc_probe_ret->dwMaxPayloadTransferSize = FRAME_INDEX_HS_UC_VGA_TRANSFER_SIZE + sizeof(USB_UVC_Payload_Header);
				status = USBD_OK;
				break;
			}
		}
		else if (uvc_probe_cur->bFormatIndex == FORMAT_INDEX_TYPE_MJPEG)
		{
		}
	}

	if (status == USBD_OK)
	{
		uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_NO_ERROR;
	}

	return status;
}

int8_t class_vs_set_commit(USB_UVC_VideoProbeAndCommitControls *commit)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;
	// Format bits per pixel.
	uint16_t bbp = 0;

	uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;

	// Check for valid format index set.
	if ((commit->bmFramingInfo & USB_UVC_VS_PROBE_COMMIT_CONTROL_BMFRAMINGINFO_FRAMEIDFIELD) &&
		(commit->bmFramingInfo & USB_UVC_VS_PROBE_COMMIT_CONTROL_BMFRAMINGINFO_EOFFIELD) &&
		(commit->bPreferedVersion == USB_VIDEO_CLASS_VERSION_MINOR))
	{
		if (usb_speed == USBD_SPEED_HIGH)
		{
			if (commit->bFormatIndex == FORMAT_INDEX_TYPE_UNCOMPRESSED)
			{
				bbp = FORMAT_UC_BBP;

				// Check for valid frame index set.
				if ((commit->bFrameIndex >= FRAME_INDEX_HS_UC_MIN) && (commit->bFrameIndex <= FRAME_INDEX_HS_UC_MAX))
				{
					if (commit->bmFramingInfo & USB_UVC_VS_PROBE_COMMIT_CONTROL_BMHINT_FRAMINGINFO)
					{
						// If framing info hint is set then check frame interval is supported.
						switch (commit->bFrameIndex)
						{
						case FRAME_INDEX_HS_UC_QVGA:
							if (commit->dwFrameInterval == FRAME_INDEX_HS_UC_QVGA_FRAME_INTERVAL)
							{
								status = USBD_OK;
							}
							break;
						case FRAME_INDEX_HS_UC_VGA:
							if (commit->dwFrameInterval == FRAME_INDEX_HS_UC_VGA_FRAME_INTERVAL)
							{
								status = USBD_OK;
							}
							break;
						}
					}
					else
					{
						status = USBD_OK;
					}

//					camera_stop();

					// Set required parts of structure depending on the frame index.
					switch (commit->bFrameIndex)
					{
					case FRAME_INDEX_HS_UC_QVGA:
//						camera_set(CAMERA_MODE_QVGA, FRAME_INDEX_HS_UC_QVGA_FRAME_RATE, CAMERA_FORMAT_UNCOMPRESSED);
						sample_width = CAMERA_FRAME_WIDTH_QVGA;
						frame_height = CAMERA_FRAME_HEIGHT_QVGA;
						frame_size = FRAME_INDEX_HS_UC_QVGA_SIZE;
						break;
					case FRAME_INDEX_HS_UC_VGA:
//						camera_set(CAMERA_MODE_VGA, FRAME_INDEX_HS_UC_VGA_FRAME_RATE, CAMERA_FORMAT_UNCOMPRESSED);
						sample_width = CAMERA_FRAME_WIDTH_VGA;
						frame_height = CAMERA_FRAME_HEIGHT_VGA;
						frame_size = FRAME_INDEX_HS_UC_VGA_SIZE;
						break;
					}

					// Set globals required for sample acquisition.
					sample_length = sample_width * bbp;
				}
			}
			else if (commit->bFormatIndex == FORMAT_INDEX_TYPE_MJPEG)
			{
			}
		}
	}

	// Check the sample length is suitable for an isochronous endpoint where it
	// must transmit the whole sample with a header in a single packet.
#ifdef USB_ENDPOINT_USE_ISOC
	if (sample_length + sizeof(USB_UVC_Payload_Header) > UVC_DATA_EP_SIZE_HS)
	{
		// Cause a STALL if the configuration is illegal.
		status = USBD_ERR_INVALID_PARAMETER;
		uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_OUT_OF_RANGE;
	}
#endif // USB_ENDPOINT_USE_ISOC

	if (status == USBD_OK)
	{
#ifndef USB_ENDPOINT_USE_ISOC
		camera_state_change = CAMERA_STREAMING_START;
#endif // !USB_ENDPOINT_USE_ISOC

		uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_NO_ERROR;
	}

	return status;
}

int8_t class_req_interface_video_streaming(USB_device_request *req)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;
	uint8_t request = req->bRequest;
	uint8_t requestType = req->bmRequestType;
	uint8_t controlSelector = MSB(req->wValue);
	uint16_t dataLen = req->wLength;
	USB_UVC_VideoProbeAndCommitControls probecommit;
	const USB_UVC_VideoProbeAndCommitControls *proberesp = NULL;

	if (dataLen > sizeof(USB_UVC_VideoProbeAndCommitControls))
	{
		dataLen = sizeof(USB_UVC_VideoProbeAndCommitControls);
	}

	// Interface requests to the VideoStreaming interface
	// of the video function. Section 4.2.
	// Interface number is in LSB of wIndex
	if ((requestType & USB_BMREQUESTTYPE_DIR_MASK) ==
			USB_BMREQUESTTYPE_DIR_HOST_TO_DEV)
	{
		switch (request)
		{
		case USB_UVC_REQUEST_SET_CUR:
			switch (controlSelector)
			{
			case USB_UVC_VS_PROBE_CONTROL:
				if (dataLen >= 8)
				{
					memcpy(&probecommit, &uvc_probe, sizeof(USB_UVC_VideoProbeAndCommitControls));
					USBD_transfer_ep0(USBD_DIR_OUT, (uint8_t *)&probecommit,
							sizeof(USB_UVC_VideoProbeAndCommitControls), dataLen);
					status = class_vs_check_probecommit(&probecommit);
					if (status == USBD_OK)
					{
						memcpy(&uvc_probe, &probecommit, dataLen);
						// ACK
						USBD_transfer_ep0(USBD_DIR_IN, NULL, 0, 0);
					}
				}
				break;

			case USB_UVC_VS_COMMIT_CONTROL:
				if (dataLen >= 8)
				{
					USBD_transfer_ep0(USBD_DIR_OUT, (uint8_t *)&probecommit,
							sizeof(USB_UVC_VideoProbeAndCommitControls), dataLen);
					// Check the commit parameters match the last probe.
					if (memcmp(&probecommit, &uvc_probe, dataLen) == 0)
					{
						memcpy(&uvc_commit, &uvc_probe, sizeof(USB_UVC_VideoProbeAndCommitControls));
						memcpy(&uvc_commit, &probecommit, dataLen);
						// ACK
						USBD_transfer_ep0(USBD_DIR_IN, NULL, 0, 0);

						status = class_vs_set_commit(&uvc_commit);
					}
				}
				break;

			case USB_UVC_VS_STILL_PROBE_CONTROL:
			case USB_UVC_VS_STILL_COMMIT_CONTROL:
			case USB_UVC_VS_STILL_IMAGE_TRIGGER_CONTROL:
			case USB_UVC_VS_STREAM_ERROR_CODE_CONTROL:
			case USB_UVC_VS_GENERATE_KEY_FRAME_CONTROL:
			case USB_UVC_VS_UPDATE_FRAME_SEGMENT_CONTROL:
			case USB_UVC_VS_SYNCH_DELAY_CONTROL:
			default:
				uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_CONTROL;
				break;
			}
			break;
		default:
			uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;
			break;
		}
	}
	else
	{
		// Interface requests to the VideoStreaming interface
		// of the video function. Section 4.2.
		// Interface number is in LSB of wIndex

		switch (request)
		{
		case USB_UVC_REQUEST_GET_INFO:
			{
				uint8_t inforesponse;

				inforesponse = USB_UVC_GET_INFO_RESPONSE_SUPPORTS_GET
						| USB_UVC_GET_INFO_RESPONSE_SUPPORTS_SET;

				USBD_transfer_ep0(USBD_DIR_IN, &inforesponse, sizeof(inforesponse), req->wLength);

			USBD_transfer_ep0(USBD_DIR_OUT, NULL, 0, 0);
				status = USBD_OK;
			}
			break;

		case USB_UVC_REQUEST_GET_CUR:
			switch (controlSelector)
			{
			case USB_UVC_VS_PROBE_CONTROL:
				proberesp = &uvc_probe;
				break;

			case USB_UVC_VS_COMMIT_CONTROL:
				proberesp = &uvc_commit;
				break;

			case USB_UVC_VS_STILL_PROBE_CONTROL:
			case USB_UVC_VS_STILL_COMMIT_CONTROL:
			case USB_UVC_VS_STILL_IMAGE_TRIGGER_CONTROL:
			case USB_UVC_VS_STREAM_ERROR_CODE_CONTROL:
			case USB_UVC_VS_GENERATE_KEY_FRAME_CONTROL:
			case USB_UVC_VS_UPDATE_FRAME_SEGMENT_CONTROL:
			case USB_UVC_VS_SYNCH_DELAY_CONTROL:
			default:
				uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_CONTROL;
				break;
			}
			break;

		case USB_UVC_REQUEST_GET_DEF:
			if (controlSelector == USB_UVC_VS_PROBE_CONTROL)
			{
				if (usb_speed == USBD_SPEED_HIGH)
				{
					proberesp = &uvc_probe_def_hs;
				}
				else
				{
					uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;
				}
			}
			else
			{
				uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_CONTROL;
			}
			break;

		case USB_UVC_REQUEST_GET_MIN:
			if (controlSelector == USB_UVC_VS_PROBE_CONTROL)
			{
				if (usb_speed == USBD_SPEED_HIGH)
				{
					// Note that the min and max values are the same as we have exactly
					// one configuration per frame and format.
					proberesp = &probecommit;
					status = class_vs_probe_min_max(&probecommit);
				}
				else
				{
					uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;
				}
			}
			else
			{
				uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_CONTROL;
			}
			break;

		case USB_UVC_REQUEST_GET_MAX:
			if (controlSelector == USB_UVC_VS_PROBE_CONTROL)
			{
				if (usb_speed == USBD_SPEED_HIGH)
				{
					// Note that the min and max values are the same as we have exactly
					// one configuration per frame and format.
					proberesp = &probecommit;
					status = class_vs_probe_min_max(&probecommit);
				}
				else
				{
					uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;
				}
			}
			else
			{
				uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_CONTROL;
			}
			break;

		case USB_UVC_REQUEST_GET_RES:
			uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_CONTROL;
			break;

		case USB_UVC_REQUEST_GET_LEN:
			switch (controlSelector)
			{
			case USB_UVC_VS_PROBE_CONTROL:
			case USB_UVC_VS_COMMIT_CONTROL:
				{
					uint8_t proberesponse = sizeof(USB_UVC_VideoProbeAndCommitControls);
					USBD_transfer_ep0(USBD_DIR_IN, &proberesponse, sizeof(proberesponse), req->wLength);
					USBD_transfer_ep0(USBD_DIR_OUT, NULL, 0, 0);
					status = USBD_OK;
				}
				break;
			case USB_UVC_VS_STILL_PROBE_CONTROL:
			case USB_UVC_VS_STILL_COMMIT_CONTROL:
			case USB_UVC_VS_STILL_IMAGE_TRIGGER_CONTROL:
			case USB_UVC_VS_STREAM_ERROR_CODE_CONTROL:
			case USB_UVC_VS_GENERATE_KEY_FRAME_CONTROL:
			case USB_UVC_VS_UPDATE_FRAME_SEGMENT_CONTROL:
			case USB_UVC_VS_SYNCH_DELAY_CONTROL:
			default:
				uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_CONTROL;
				break;
			}
			break;

		case USB_UVC_REQUEST_SET_CUR_ALL:
		case USB_UVC_REQUEST_GET_CUR_ALL:
		case USB_UVC_REQUEST_GET_MIN_ALL:
		case USB_UVC_REQUEST_GET_MAX_ALL:
		case USB_UVC_REQUEST_GET_RES_ALL:
		case USB_UVC_REQUEST_GET_DEF_ALL:
		case USB_UVC_REQUEST_RC_UNDEFINED:
		default:
			uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;
			// STALL
			break;
		}

		if (proberesp)
		{
			uint16_t len = sizeof(USB_UVC_VideoProbeAndCommitControls);

			if (len > req->wLength)
			{
				len = req->wLength;
			}
			USBD_transfer_ep0(USBD_DIR_IN, (uint8_t *)proberesp,
				len, req->wLength);
			USBD_transfer_ep0(USBD_DIR_OUT, NULL, 0, 0);
			status = USBD_OK;
		}
	}

	return status;
}

int8_t class_req_endpoint_video(USB_device_request *req)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;
	uint8_t command[USB_CONTROL_EP_MAX_PACKET_SIZE];
	uint8_t request = req->bRequest;

	// Endpoint requests to the VideoStreaming endpoint
	// of the video function. Section 4.1.
	// Endpoint number is in LSB of wIndex
	// Entity ID is MSB of wIndex
	switch (request)
	{
	case USB_UVC_REQUEST_SET_CUR:
	case USB_UVC_REQUEST_GET_CUR:
	case USB_UVC_REQUEST_GET_MIN:
	case USB_UVC_REQUEST_GET_MAX:
	case USB_UVC_REQUEST_GET_RES:
	case USB_UVC_REQUEST_GET_LEN:
	case USB_UVC_REQUEST_GET_INFO:
	case USB_UVC_REQUEST_GET_DEF:
		uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;
		USBD_transfer_ep0(USBD_DIR_IN, command, req->wLength, req->wLength);
		USBD_transfer_ep0(USBD_DIR_OUT, NULL, 0, 0);
		break;
	case USB_UVC_REQUEST_SET_CUR_ALL:
	case USB_UVC_REQUEST_GET_CUR_ALL:
	case USB_UVC_REQUEST_GET_MIN_ALL:
	case USB_UVC_REQUEST_GET_MAX_ALL:
	case USB_UVC_REQUEST_GET_RES_ALL:
	case USB_UVC_REQUEST_GET_DEF_ALL:
	case USB_UVC_REQUEST_RC_UNDEFINED:
	default:
		// ACK
		uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;
		USBD_transfer_ep0(USBD_DIR_IN, NULL, 0, 0);
		USBD_transfer_ep0(USBD_DIR_OUT, NULL, 0, 0);
		break;
	}

	return status;
}

/**
 @brief      USB class request handler
 @details    Handle class requests from the host application.
 The bRequest value is parsed and the appropriate
 action or function is performed. Additional values
 from the USB_device_request structure are decoded
 and the DFU state machine and status updated as
 required.
 @param[in]	req - USB_device_request structure containing the
 SETUP portion of the request from the host.
 @return		status - USBD_OK if successful or USBD_ERR_*
 if there is an error or the bRequest is not
 supported.
 **/
int8_t class_req_cb(USB_device_request *req)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;
	uint8_t requestType = req->bmRequestType;
	uint8_t interface = LSB(req->wIndex) & 0x0F;

	// For DFU requests the SETUP packet must be addressed to the
	// the DFU interface on the device.

	// For DFU requests ensure the recipient is an interface...
	if ((requestType & USB_BMREQUESTTYPE_RECIPIENT_MASK) ==
			USB_BMREQUESTTYPE_RECIPIENT_INTERFACE)
	{
		// ...and that the interface is the correct Runtime interface
		if (USBD_DFU_is_runtime())
		{
			if ((interface == DFU_USB_INTERFACE_RUNTIME))
			{
				status = class_req_dfu_interface_runtime(req);
			}
			else if (interface == 0)
			{
				status = class_req_interface_video_control(req);
			}
			else if (interface == 1)
			{
				status = class_req_interface_video_streaming(req);
			}
		}
		// ...or the correct DFU Mode interface
		else
		{
			if (interface == DFU_USB_INTERFACE_DFUMODE)
			{
				status = class_req_dfu_interface_dfumode(req);
			}
		}
	}
	else if ((requestType & USB_BMREQUESTTYPE_RECIPIENT_MASK) ==
			USB_BMREQUESTTYPE_RECIPIENT_ENDPOINT)
	{
		status = class_req_endpoint_video(req);
	}

	return status;
}

/**
 @brief      USB standard request handler for GET_DESCRIPTOR
 @details    Handle standard GET_DESCRIPTOR requests from the host
 application.
 The hValue is parsed and the appropriate device,
 configuration or string descriptor is selected.
 For device and configuration descriptors the DFU
 state machine determines which descriptor to use.
 For string descriptors the lValue selects which
 string from the string_descriptors table to use.
 The string table is automatically traversed to find
 the correct string and the length is taken from the
 string descriptor.
 @param[in]	req - USB_device_request structure containing the
 SETUP portion of the request from the host.
 @return		status - USBD_OK if successful or USBD_ERR_*
 if there is an error or the bmRequestType is not
 supported.
 **/
int8_t standard_req_get_descriptor(USB_device_request *req, uint8_t **buffer, uint16_t *len)
{
	uint8_t *src = NULL;
	uint16_t length = req->wLength;
	uint8_t hValue = MSB(req->wValue);
	uint8_t lValue = LSB(req->wValue);
	uint8_t i;
	uint8_t slen;

	switch (hValue)
	{
	case USB_DESCRIPTOR_TYPE_DEVICE:
		if (USBD_DFU_is_runtime())
		{
			src = (uint8_t *) &device_descriptor_uvc;
		}
		else
		{
			src = (uint8_t *) &device_descriptor_dfumode;
		}
		if (length > sizeof(USB_device_descriptor)) // too many bytes requested
				length = sizeof(USB_device_descriptor); // Entire structure.
		break;

	case USB_DESCRIPTOR_TYPE_CONFIGURATION:
		if (USBD_DFU_is_runtime())
		{
			if (usb_speed == USBD_SPEED_HIGH)
			{
				memcpy((void *)&config_descriptor_buffer.hs, (void *)&config_descriptor_uvc_hs, sizeof(config_descriptor_uvc_hs));
				if (length > sizeof(config_descriptor_uvc_hs)) // too many bytes requested
					length = sizeof(config_descriptor_uvc_hs); // Entire structure.
			}
			else
			{
				memcpy((void *)&config_descriptor_buffer.fs, (void *)&config_descriptor_uvc_fs, sizeof(config_descriptor_uvc_fs));
				if (length > sizeof(config_descriptor_uvc_fs)) // too many bytes requested
					length = sizeof(config_descriptor_uvc_fs); // Entire structure.
			}
			src = (uint8_t *)&config_descriptor_buffer.hs;
			config_descriptor_buffer.hs.configuration.bDescriptorType = USB_DESCRIPTOR_TYPE_CONFIGURATION;
		}
		else
		{
			src = (uint8_t *) &config_descriptor_dfumode;
			if (length > sizeof(config_descriptor_dfumode)) // too many bytes requested
				length = sizeof(config_descriptor_dfumode); // Entire structure.
		}
		break;

	case USB_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION:
		if (USBD_DFU_is_runtime())
		{
			if (usb_speed == USBD_SPEED_HIGH)
			{
				memcpy((void *)&config_descriptor_buffer.fs, (void *)&config_descriptor_uvc_fs, sizeof(config_descriptor_uvc_fs));
				if (length > sizeof(config_descriptor_uvc_fs)) // too many bytes requested
					length = sizeof(config_descriptor_uvc_fs); // Entire structure.
			}
			else
			{
				memcpy((void *)&config_descriptor_buffer.hs, (void *)&config_descriptor_uvc_hs, sizeof(config_descriptor_uvc_hs));
				if (length > sizeof(config_descriptor_uvc_hs)) // too many bytes requested
					length = sizeof(config_descriptor_uvc_hs); // Entire structure.
			}
			src = (uint8_t *)&config_descriptor_buffer.hs;
			config_descriptor_buffer.hs.configuration.bDescriptorType = USB_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION;
		}
		break;

	case USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER:
		src = (uint8_t *) &device_qualifier_descriptor_uvc;
		if (length > sizeof(USB_device_qualifier_descriptor)) // too many bytes requested
			length = sizeof(USB_device_qualifier_descriptor); // Entire structure.
		break;

	case USB_DESCRIPTOR_TYPE_STRING:
		// Special case: WCID descriptor
#ifdef USB_INTERFACE_USE_DFU
		if (lValue == USB_MICROSOFT_WCID_STRING_DESCRIPTOR)
		{
			src = (uint8_t *)wcid_string_descriptor;
			length = sizeof(wcid_string_descriptor);
			break;
		}
#endif // USB_INTERFACE_USE_DFU

		// Find the nth string in the string descriptor table
		i = 0;
		while ((slen = ((uint8_t *)string_descriptor)[i]) > 0)
		{
			// Point to start of string descriptor in __code segment
			// Typecast prevents warning for losing const when USBD_transfer
			// is called
			src = (uint8_t *) &((uint8_t *)string_descriptor)[i];
			if (lValue == 0)
			{
				break;
			}
			i += slen;
			lValue--;
		}
		if (lValue > 0)
		{
			// String not found
			return USBD_ERR_NOT_SUPPORTED;
		}
		// Update the length returned only if it is less than the requested
		// size
		if (length > slen)
		{
			length = slen;
		}

		break;

	default:
		return USBD_ERR_NOT_SUPPORTED;
	}

	*buffer = src;
	*len = length;

	return USBD_OK;
}

/**
 @brief      USB vendor request handler
 @details    Handle vendor requests from the host application.
 The bRequest value is parsed and the appropriate
 action or function is performed. Additional values
 from the USB_device_request structure are decoded
 and provided to other handlers.
 There are no vendor requests requiring handling in
 this firmware.
 @param[in]	req - USB_device_request structure containing the
 SETUP portion of the request from the host.
 @return		status - USBD_OK if successful or USBD_ERR_*
 if there is an error or the bRequest is not
 supported.
 **/
int8_t vendor_req_cb(USB_device_request *req)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;
#ifdef USB_INTERFACE_USE_DFU
	uint16_t length = req->wLength;
#endif // USB_INTERFACE_USE_DFU

	// For Microsoft WCID only.
	// Request for Compatible ID Feature Descriptors.
	// Check the request code and wIndex.
#ifdef USB_INTERFACE_USE_DFU
	if (req->bRequest == WCID_VENDOR_REQUEST_CODE)
	{
		if (req->wIndex == USB_MICROSOFT_WCID_FEATURE_WINDEX_COMPAT_ID)
		{
			if ((req->bmRequestType & USB_BMREQUESTTYPE_DIR_MASK) ==
				USB_BMREQUESTTYPE_DIR_DEV_TO_HOST)
			{
				if (length > sizeof(wcid_feature_runtime)) // too many bytes requested
					length = sizeof(wcid_feature_runtime); // Entire structure.
				// Return a compatible ID feature descriptor.
				if (USBD_DFU_is_runtime())
				{
					USBD_transfer_ep0(USBD_DIR_IN, (uint8_t *) &wcid_feature_runtime,
							length, length);
				}
				else
				{
					USBD_transfer_ep0(USBD_DIR_IN, (uint8_t *) &wcid_feature_dfumode,
							length, length);
				}
				// ACK packet
				USBD_transfer_ep0(USBD_DIR_OUT, NULL, 0, 0);
				status = USBD_OK;
			}
		}
	}
#endif // USB_INTERFACE_USE_DFU

	return status;
}

/**
 @brief      USB endpoint feature request handler
 @details    Handle endpoint feature requests from the
 host application.
 The bRequest value is parsed and the appropriate
 action or function is performed. Additional values
 from the USB_device_request structure are decoded
 and provided to other handlers.
 @param[in]	req - USB_device_request structure containing the
 SETUP portion of the request from the host.
 @return		status - USBD_OK if successful or USBD_ERR_*
 if there is an error or the bRequest is not supported.
 **/
int8_t endpoint_req_cb(USB_device_request *req)
{
	int8_t status = USBD_ERR_NOT_SUPPORTED;

	if (req->bRequest == USB_REQUEST_CODE_CLEAR_FEATURE)
	{
		if (req->wIndex == (USB_ENDPOINT_DESCRIPTOR_EPADDR_IN | UVC_EP_DATA_IN))
		{
			if ((req->bmRequestType & USB_BMREQUESTTYPE_DIR_MASK) ==
				USB_BMREQUESTTYPE_DIR_HOST_TO_DEV)
			{
				// Stop the stream.
				camera_state_change = CAMERA_STREAMING_STOP;

				// ACK packet
				USBD_transfer_ep0(USBD_DIR_IN, NULL, 0, 0);
				status = USBD_OK;
			}
		}
	}

	return status;
}

/**
 @brief      USB suspend callback
 @details    Called when the USB bus enters the suspend state.
 @param[in]	status - unused.
 @return		N/A
 **/
void suspend_cb(uint8_t status)
{
	(void) status;

	SYS->PMCFG_L |= MASK_SYS_PMCFG_HOST_RESUME_DEV;

	BRIDGE_DEBUG_PRINTF("Suspend\r\n");
	// Stop the stream.
	camera_state_change = CAMERA_STREAMING_STOP;
	return;
}

/**
 @brief      USB resume callback
 @details    Called when the USB bus enters the resume state
 prior to restating after a suspend.
 @param[in]  status - unused.
 @return     N/S
 **/
void resume_cb(uint8_t status)
{
	(void) status;

	SYS->PMCFG_L &= (~MASK_SYS_PMCFG_HOST_RESUME_DEV);

	BRIDGE_DEBUG_PRINTF("Resume\r\n");
	return;
}

/**
 @brief      USB reset callback
 @details    Called when the USB bus enters the reset state.
 @param[in]	status - unused.
 @return		N/A
 **/
void reset_cb(uint8_t status)
{
	(void) status;

	USBD_set_state(USBD_STATE_DEFAULT);

	// If we are at DFUSTATE_MANIFEST_WAIT_RESET stage and do
	// not support detach then the DFU will reset the chip and
	// run the new firmware.
	// From the DFUSTATE_APPIDLE state advance to DFUSTATE_DFUIDLE.
	// Other states will not advance the state machine or may
	// move this to DFUSTATE_DFUERROR if it is inappropriate to
	// find a reset at that stage.
	USBD_DFU_reset();
	BRIDGE_DEBUG_PRINTF("Reset\r\n");

//	camera_stop();

	return;
}


uint8_t usbd_testing(void)
{
	USBD_ctx usb_ctx;

	uint16_t camera_tx_buffer_line = 0;
	uint16_t camera_tx_data_avail;
	uint32_t camera_tx_frame_size = 0;
	uint8_t *pstart = NULL;
	uint8_t *pmark;
	uint8_t frame_active = 0;
	uint8_t frame_eof_pending = 0;

	// Length of data packet.
	uint16_t len;
	// Length of line data left to send.
	uint16_t remain_len = 0;
	// Packet length.
	uint16_t packet_len;
	// Part transfer required.
	uint8_t part;
	// Frame ID toggle
	uint8_t frame_toggle = 0;

	// Header for UVC sample transfer.
	static USB_UVC_Payload_Header hdr;

	// Header length always stays the same.
	hdr.bHeaderLength = sizeof(USB_UVC_Payload_Header);

	memset(&usb_ctx, 0, sizeof(usb_ctx));

	usb_ctx.standard_req_cb = NULL;
	usb_ctx.get_descriptor_cb = standard_req_get_descriptor;
	usb_ctx.set_interface_cb = setif_req_cb;
	usb_ctx.get_interface_cb = getif_req_cb;
	usb_ctx.class_req_cb = class_req_cb;
	usb_ctx.vendor_req_cb = vendor_req_cb;
	usb_ctx.ep_feature_req_cb = endpoint_req_cb;
	usb_ctx.suspend_cb = suspend_cb;
	usb_ctx.resume_cb = resume_cb;
	usb_ctx.reset_cb = reset_cb;
	usb_ctx.lpm_cb = NULL;
	usb_ctx.speed = USBD_SPEED_HIGH;

	// Initialise the USB device with a control endpoint size
	// of 8 to 64 bytes. This must match the size for bMaxPacketSize0
	// defined in the device descriptor.
	usb_ctx.ep0_size = USB_CONTROL_EP_SIZE;
	//usb_ctx.ep0_cb = ep_cb;

	BRIDGE_DEBUG_PRINTF("Initialising USB...\r\n");

	USBD_initialise(&usb_ctx);

	BRIDGE_DEBUG_PRINTF("Initialised USB.\r\n");

	// Wait for connect to host.
	while (USBD_connect() != USBD_OK) {
		update_leds();
	}

	BRIDGE_DEBUG_PRINTF("Connected to USB host.\r\n");

	USBD_create_endpoint(UVC_EP_INTERRUPT, USBD_EP_INT, USBD_DIR_IN,
			UVC_INTERRUPT_USBD_EP_SIZE, USBD_DB_OFF, NULL /*ep_cb*/);
	if (USBD_get_bus_speed() == USBD_SPEED_HIGH)
	{
#ifdef USB_ENDPOINT_USE_ISOC
		USBD_create_endpoint(UVC_EP_DATA_IN, USBD_EP_ISOC, USBD_DIR_IN,
				UVC_DATA_USBD_EP_SIZE_HS, USBD_DB_OFF, NULL /*ep_cb*/);
#else // !USB_ENDPOINT_USE_ISOC
		USBD_create_endpoint(UVC_EP_DATA_IN, USBD_EP_BULK, USBD_DIR_IN,
			UVC_DATA_USBD_EP_SIZE_HS, USBD_DB_ON, NULL /*ep_cb*/);
#endif // USB_ENDPOINT_USE_ISOC
		packet_len = UVC_DATA_EP_SIZE_HS;
		usb_speed = USBD_SPEED_HIGH;
	}
	else
	{
		USBD_create_endpoint(UVC_EP_DATA_IN, USBD_EP_ISOC, USBD_DIR_IN,
			UVC_DATA_USBD_EP_SIZE_FS, USBD_DB_ON, NULL /*ep_cb*/);
		packet_len = UVC_DATA_EP_SIZE_FS;
		usb_speed = USBD_SPEED_FULL;
	}

	// Setup probe and commit settings.
	if (usb_speed == USBD_SPEED_HIGH)
	{
		memcpy(&uvc_probe, &uvc_probe_def_hs, sizeof(USB_UVC_VideoProbeAndCommitControls));
	}
	else
	{
		memset(&uvc_probe, 0, sizeof(USB_UVC_VideoProbeAndCommitControls));
	}
	memset(&uvc_commit, 0, sizeof(USB_UVC_VideoProbeAndCommitControls));

	BRIDGE_DEBUG_PRINTF("Starting\r\n");

	camera_state_change = CAMERA_STREAMING_OFF;

	for (;;)
	{
		update_leds();

		if (USBD_connect() == USBD_OK)
		{
			if (USBD_DFU_is_runtime())
			{
				// Start the UVC emulation code.
				while (USBD_process() == USBD_OK)
				{
					update_leds();

					// Check a commit has occurred successfully first.
					if (uvc_commit.bFormatIndex != FORMAT_INDEX_TYPE_NONE)
					{
						// Change in interface made for ISO mode.
						if (camera_state_change != CAMERA_STREAMING_OFF)
						{
							// Start or stop the camera.
#ifdef USB_ENDPOINT_USE_ISOC
							// Interface for non-zero-bandwith interface selected.
							if (usb_alt == 1)
#else // !USB_ENDPOINT_USE_ISOC
							// Streaming commit received.
							if (camera_state_change == CAMERA_STREAMING_START)
#endif // USB_ENDPOINT_USE_ISOC
							{

								BRIDGE_DEBUG_PRINTF("Camera starting (sample length %d frame %ld)\r\n", sample_length, frame_size);

								cam_set_threshold(sample_length);
								sample_threshold = sample_length;
								camera_tx_frame_size = 0;
								frame_active = 0;
								camera_tx_buffer_line = 0;

								cam_start(sample_length);
//								camera_start();
								wait_for_vsync();
								cam_enable_interrupt();
#ifndef USB_ENDPOINT_USE_ISOC
								usb_alt = 1;
#endif // USB_ENDPOINT_USE_ISOC
							}
							// Stream stopping.
							else // CAMERA_STREAMING_STOP or zero-bandwith interface selected.
							{
//								camera_stop();

								cam_disable_interrupt();
								cam_stop();

								BRIDGE_DEBUG_PRINTF("Camera stopping\r\n");
#ifndef USB_ENDPOINT_USE_ISOC
								usb_alt = 0;
#endif // USB_ENDPOINT_USE_ISOC
							}

							camera_state_change = CAMERA_STREAMING_OFF;
						}

						if (usb_alt == 1)
						{
							do {

								if (!USBD_ep_buffer_full(UVC_EP_DATA_IN))
								{
									// Start a new line. If this is a continuation of a line then
									// the remainder of data will be sent.
									if (remain_len == 0)
									{
										cam_disable_interrupt();
										// Number of lines buffered.
										camera_tx_data_avail = camera_rx_data_avail;
										if (camera_rx_data_avail)
										{
											camera_rx_data_avail--;
										}
										cam_enable_interrupt();

										// Set the header info frame toggle bit.
										hdr.bmHeaderInfo = frame_toggle | 0x80;

										// Send a full line of data if there is data available.
										if (camera_tx_data_avail)
										{
											len = sample_length;

											pstart = (uint8_t *)&camera_buffer[camera_tx_buffer_line];

											// Loop around camera_buffer.
											camera_tx_buffer_line++;
											if (camera_tx_buffer_line == CAMERA_BUFFER_MAX_LINES)
											{
												camera_tx_buffer_line = 0;
											}

											if (uvc_commit.bFormatIndex == FORMAT_INDEX_TYPE_UNCOMPRESSED)
											{
												camera_tx_frame_size += sample_length;
												if (camera_tx_frame_size >= frame_size)
												{
													// END of frame
													hdr.bmHeaderInfo |= 2;
													frame_toggle++; frame_toggle &= 1;

													camera_tx_frame_size = 0;
												}

												remain_len = len;
											}
											else if (uvc_commit.bFormatIndex == FORMAT_INDEX_TYPE_MJPEG)
											{
												// MJPEG handler
												// Wait for a frame start which will have 0xff 0xd8 in the first 2 bytes.
												if (!frame_active)
												{
													if ((pstart[0] == 0xff) && (pstart[1] == 0xd8))
													{
#ifdef SHOW_DEBUG_LINE_USAGE
														tfp_putc(UART0, 's');
#endif // SHOW_DEBUG_LINE_USAGE
														frame_active = 1;
													}
												}

												// If we have just started a new frame or are continuing then look for the
												// end of frame marker.
												if (frame_active)
												{
													// If an end of frame marker was pending (last byte of previous sample
													// was 0xff) then check first byte might be EOF marker.
													if (frame_eof_pending)
													{
#ifdef SHOW_DEBUG_LINE_USAGE
														tfp_putc(UART0, 'p');
#endif // SHOW_DEBUG_LINE_USAGE
														if (*pstart == 0xd9)
														{
															// END of frame
															hdr.bmHeaderInfo |= 2;
															frame_toggle++; frame_toggle &= 1;

															// Only first byte of this sample is valid.
															len = 1;
															frame_active = 0;
#ifdef SHOW_DEBUG_LINE_USAGE
															tfp_putc(UART0, 'x');
#endif // SHOW_DEBUG_LINE_USAGE
														}
														frame_eof_pending = 0;
													}

													// Look for end of frame markers in this sample.
													if (frame_active)
													{
														pmark = pstart;
														while (pmark)
														{
															pmark = memchr(pmark, 0xff, sample_length - (pmark - pstart));
															if (pmark)
															{
																pmark++;
																if ((pmark - pstart) == sample_length)
																{
																	// The last byte of the sample is 0xff so there might be
																	// and end of frame marker continued on the next sample.
																	frame_eof_pending = 1;
#ifdef SHOW_DEBUG_LINE_USAGE
																	tfp_putc(UART0, 'P');
#endif // SHOW_DEBUG_LINE_USAGE
																}
																else if (*pmark == 0xd9)
																{
																	// END of frame.
																	hdr.bmHeaderInfo |= 2;
																	frame_toggle++; frame_toggle &= 1;

																	// Length of remaining data is up to and including the
																	// 0xd9 of the end of frame marker.
																	len = pmark - pstart + 1;
																	frame_active = 0;

#ifdef SHOW_DEBUG_LINE_USAGE
																	tfp_putc(UART0, 'X');
#endif // SHOW_DEBUG_LINE_USAGE
																}
															}
														}
													}

													remain_len = len;
												}
												else
												{
													remain_len = 0;
#ifdef SHOW_DEBUG_LINE_USAGE
													tfp_putc(UART0, '-');
#endif // SHOW_DEBUG_LINE_USAGE
												}
											}

											if (remain_len)
											{
												part = USBD_TRANSFER_EX_PART_NORMAL;

												// Add header to USB endpoint buffer.
												// Set flag to allow follow-on data.
												USBD_transfer_ex(UVC_EP_DATA_IN, (uint8_t *)&hdr,
														sizeof(USB_UVC_Payload_Header),
														USBD_TRANSFER_EX_PART_NO_SEND, 0);

												// Calculate the size of the remaining data for this
												// packet. It may all be able to be sent in one packet.
												if (len > (packet_len - sizeof(USB_UVC_Payload_Header)))
												{
													len = (packet_len - sizeof(USB_UVC_Payload_Header));
													part = USBD_TRANSFER_EX_PART_NO_SEND;
												}

												// Send follow-on data up-to the length of one packet.
												// Flag is set if there is more data following.
												// Offset is set the size of the header.
												USBD_transfer_ex(UVC_EP_DATA_IN,
													pstart,
													len,
#ifdef USB_ENDPOINT_USE_ISOC
													// Can only send ONE packet with header and data in
													// isochronous mode.
													USBD_TRANSFER_EX_PART_NORMAL,
#else // USB_ENDPOINT_USE_ISOC
													// In bulk mode we can send multiple packets to make
													// a single transfer. But this is either a whole packet
													// of length wMaxPacketSize (since total length is
													// greater than wMaxPacketSize) or smaller and will
													// therefore be the last in the transfer.
													part,
#endif // USB_ENDPOINT_USE_ISOC
													sizeof(USB_UVC_Payload_Header));

												remain_len -= len;
											}
										}
									}
#ifndef USB_ENDPOINT_USE_ISOC
									// This is only relevant for bulk mode.
									// We can send multiple USB packets with a single header
									// to form a transfer of UVC data. This cannot be done with
									// an isochronous endpoint.
									else
									{
										part = USBD_TRANSFER_EX_PART_NORMAL;
										len = remain_len;

										// Do not send the header on all subsequent packets.
										// Send only one packet at a time.
										if (len >= (packet_len))
										{
											len = (packet_len);
											part = USBD_TRANSFER_EX_PART_NO_SEND;
										}

										// Send the next packet of data from the camera buffer.
										USBD_transfer_ex(UVC_EP_DATA_IN,
												&pstart[sample_length - remain_len],
												len,
												part, 0);

										remain_len -= len;
									}
#endif // USB_ENDPOINT_USE_ISOC
								}
							} while (0);
						}
					}
				}
			}
			else
			{
				// In DFU mode. Process USB requests.
				while (USBD_process() == USBD_OK);
			}
			BRIDGE_DEBUG_PRINTF("Restarting\r\n");
		}
	}

	return 0;
}

/* FUNCTIONS ***********************************************************************/

int main(void)
{
#ifdef USB_INTERFACE_USE_STARTUPDFU
	STARTUP_DFU();
#endif // USB_INTERFACE_USE_STARTUPDFU

	sys_reset_all();
	sys_disable(sys_device_camera);
	sys_disable(sys_device_i2c_master);
	sys_disable(sys_device_i2c_slave);
	sys_disable(sys_device_usb_device);

	/* Enable the UART Device... */
	sys_enable(sys_device_uart0);
	/* Make GPIO48 function as UART0_TXD and GPIO49 function as UART0_RXD... */
	gpio_function(48, pad_uart0_txd); /* UART0 TXD */
	gpio_function(49, pad_uart0_rxd); /* UART0 RXD */

	/* Enable tfp_printf() functionality... */
	init_printf(UART0, tfp_putc);

#ifdef BRIDGE_DEBUG
	// Open the UART using the coding required.
	uart_open(UART0, 1, UART_DIVIDER_115200_BAUD, uart_data_bits_8, uart_parity_none, uart_stop_bits_1);

	/* Print out a welcome message... */
	BRIDGE_DEBUG_PRINTF("\x1B[2J" /* ANSI/VT100 - Clear the Screen */
	                    "\x1B[H"  /* ANSI/VT100 - Move Cursor to Home */
	);

	BRIDGE_DEBUG_PRINTF("(C) Copyright 2016, Bridgetek Pte Ltd. \r\n");
	BRIDGE_DEBUG_PRINTF("--------------------------------------------------------------------- \r\n");
	BRIDGE_DEBUG_PRINTF("Welcome to the UVC Webcam ... \r\n");
	BRIDGE_DEBUG_PRINTF("\r\n");
	BRIDGE_DEBUG_PRINTF("Emulate a UVC device connected to the USB.\r\n");
	BRIDGE_DEBUG_PRINTF("--------------------------------------------------------------------- \r\n");

	BRIDGE_DEBUG_PRINTF("Pi-puck camera test\r\n");
	BRIDGE_DEBUG_PRINTF("%s %s\r\n", __DATE__, __TIME__);
#endif // BRIDGE_DEBUG

	/* Timer A = 1ms */
	timer_prescaler(1000);
	timer_init(timer_select_a, 100, timer_direction_down, timer_prescaler_select_on, timer_mode_continuous);
	timer_enable_interrupt(timer_select_a);
	timer_start(timer_select_a);

	interrupt_attach(interrupt_timers, (int8_t)interrupt_timers, timer_ISR);
	/* Enable power management interrupts. Primarily to detect resume signalling
	 * from the USB host. */
	interrupt_attach(interrupt_0, (int8_t)interrupt_0, powermanagement_ISR);

	/* Set up the Camera Interface */
	cam_disable_interrupt();
	cam_stop();
	cam_flush();

	gpio_function(6, pad_cam_xclk);  /* XCLK */
	gpio_function(7, pad_cam_pclk);  /* PCLK */
	//gpio_function(8, pad_cam_vd);  /* VD */
	gpio_function(9, pad_cam_hd);  /* HD */
	gpio_function(10, pad_cam_d7); /* D7 */
	gpio_function(11, pad_cam_d6); /* D6 */
	gpio_function(12, pad_cam_d5); /* D5 */
	gpio_function(13, pad_cam_d4); /* D4 */
	gpio_function(14, pad_cam_d3); /* D3 */
	gpio_function(15, pad_cam_d2); /* D2 */
	gpio_function(16, pad_cam_d1); /* D1 */
	gpio_function(17, pad_cam_d0); /* D0 */
	sys_enable(sys_device_camera);

	/* Set VD as a GPIO input */
	gpio_function(8, pad_gpio8);
	gpio_dir(8, pad_dir_input);
	gpio_interrupt_enable(8, gpio_int_edge_falling); /* VD */
	interrupt_attach(interrupt_gpio, (uint8_t)interrupt_gpio, vsync_ISR);

	/* Set up I2C */
	sys_enable(sys_device_i2c_master);
	sys_enable(sys_device_i2c_slave);

	gpio_function(44, pad_i2c0_scl); /* I2C0_SCL */
	gpio_function(45, pad_i2c0_sda); /* I2C0_SDA */
	gpio_function(46, pad_i2c1_scl); /* I2C1_SCL */
	gpio_function(47, pad_i2c1_sda); /* I2C1_SDA */
	gpio_pull(46, pad_pull_none);
	gpio_pull(47, pad_pull_none);

	i2cm_init(I2CM_NORMAL_SPEED, 100000);

	/* Initialise the I2C Slave hardware... */
	i2cs_init(0x38);
	/* Set up the handler for i2cs_dev... */
	i2cs_dev_buffer = i2cs_dev_registers;
	i2cs_dev_buffer_size = 3;
	i2cs_enable_interrupt(MASK_I2CS_FIFO_INT_ENABLE_I2C_INT);
	interrupt_attach(interrupt_i2cs, (uint8_t)interrupt_i2cs, i2cs_dev_ISR);

	// Set up GPIOs for RGB LEDs
	gpio_function(44, pad_i2c0_scl); /* I2C0_SCL */
	gpio_function(45, pad_i2c0_sda); /* I2C0_SDA */
	gpio_function(46, pad_i2c1_scl); /* I2C1_SCL */
	gpio_function(47, pad_i2c1_sda); /* I2C1_SDA */
	gpio_pull(46, pad_pull_none);
	gpio_pull(47, pad_pull_none);

	// LED1: R=GPIO55, G=GPIO29, B=GPIO45
	// LED2: R=GPIO56, G=GPIO57, B=GPIO58
	// LED3: R=GPIO52, G=GPIO53, B=GPIO54
	gpio_function(55, pad_gpio55);
	gpio_function(29, pad_gpio29);
	gpio_function(45, pad_gpio45);
	gpio_function(56, pad_gpio56);
	gpio_function(57, pad_gpio57);
	gpio_function(58, pad_gpio58);
	gpio_function(52, pad_gpio52);
	gpio_function(53, pad_gpio53);
	gpio_function(54, pad_gpio54);
	gpio_dir(55, pad_dir_output);
	gpio_dir(29, pad_dir_output);
	gpio_dir(45, pad_dir_output);
	gpio_dir(56, pad_dir_output);
	gpio_dir(57, pad_dir_output);
	gpio_dir(58, pad_dir_output);
	gpio_dir(52, pad_dir_output);
	gpio_dir(53, pad_dir_output);
	gpio_dir(54, pad_dir_output);
	gpio_write(55, 1);
	gpio_write(29, 1);
	gpio_write(45, 1);
	gpio_write(56, 1);
	gpio_write(57, 1);
	gpio_write(58, 1);
	gpio_write(52, 1);
	gpio_write(53, 1);
	gpio_write(54, 1);

	update_leds();

	BRIDGE_DEBUG_PRINTF("Camera initialisation must be done by external device (e.g. Raspberry Pi)\r\n");

	/* Clock data in when VREF is low and HREF is high */
	cam_init(cam_trigger_mode_1, cam_clock_pol_raising);

	interrupt_attach(interrupt_camera, (uint8_t)interrupt_camera, cam_ISR);

	interrupt_enable_globally();

	usbd_testing();

	interrupt_disable_globally();

	BRIDGE_DEBUG_PRINTF("Done..\r\n");

	// Wait forever...
	for (;;);

	return 0;
}
