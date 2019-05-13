/**
  @file defs.h
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

#ifndef INCLUDES_USB_UVC_H_
#define INCLUDES_USB_UVC_H_

/* CONFIGURATION *******************************************************************/

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
#define UVC_DATA_USBD_EP_SIZE_HS 		USBD_EP_SIZE_1023
#else // !USB_ENDPOINT_USE_ISOC
#define UVC_DATA_EP_SIZE_HS				0x200
#define UVC_DATA_USBD_EP_SIZE_HS 		USBD_EP_SIZE_512
#endif // USB_ENDPOINT_USE_ISOC
/// Endpoint for Full Speed mode
#define UVC_DATA_EP_SIZE_FS				0x200
#define UVC_DATA_USBD_EP_SIZE_FS 		USBD_EP_SIZE_512
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

/** @brief Compressed payload pixel size
 * @details Uncompressed payload images can be 16 or 24 bits per pixel.
 */
#define PAYLOAD_BBP_MJPEG 0x10 /* format.bBitsPerPixel */

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
 @brief Format Bits Per Pixel definition for UVC device.
 @details Derived from the image type.
 */
#define FORMAT_MJPEG_BBP (PAYLOAD_BBP_MJPEG >> 3)

/**
 @brief Format Index Type definitions for UVC device.
 @details This can be done more neatly in an enum but preprocessor
  macros are used. If an enum were used then these
 */
//@{
enum {
	FORMAT_INDEX_TYPE_NONE = 0,
	FORMAT_INDEX_TYPE_UNCOMPRESSED,
	FORMAT_INDEX_TYPE_MJPEG,
	FORMAT_INDEX_MAX,
};
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
  @brief Maximum Length USB transfer
  @details This is the longest packet to write to the USB interface.
  */
 #define UVC_DATA_MAX_TRANSFER_SIZE 960

 /**
  @brief Maximum Length for Line Buffer.
  @details This is the longest buffer length required to hold one line
  	 of uncompressed data or a section of compressed data.
  */
 #define FORMAT_MAX_LINE_BYTES_ALIGNED ((UVC_DATA_MAX_TRANSFER_SIZE + 3) & (~3L))

/**
 @brief Clock Frequency definitions for UVC device.
 */
//@{
#define CLK_FREQ_48MHz 0x02dc6c00
//@}

/**
 @brief      Get current alternate setting.
 @details    Return the alternate setting of the video streaming interface.
 	 	 	 A SET_INTERFACE request from the host will be handled within
 	 	 	 this module to set the alternate interface.
 **/
uint8_t usb_uvc_get_alt();

/**
 @brief      Test whether UVC module has a valid format set with COMMIT.
 **/
int8_t usb_uvc_has_commit();
int8_t usb_uvc_is_uncompressed();
int8_t usb_uvc_is_mjpeg();

/**
 @brief      Test whether a frame size and frame rate can be transferred
 	 	 	 over USB.
 **/
int8_t usb_uvc_bandwidth_ok(uint16_t width, uint16_t height, uint8_t frame_rate);

/**
 @brief      Set up the USB device configuration.
 @details    Setup the configuration descriptors, probe settings and strings.
 **/
void usb_uvc_build_configuration(uint16_t module);

/**
 @brief      Initialise the USB UVC module.
 @returns 	 Packet length for data endpoint.
 **/
uint16_t usb_uvc_init();

/**
 @brief      Set up the USB device.
 @details    Abstract the USB interface for the application. Handle
 	 	 	 all USB traffic except the USB data packets.
 **/
void usb_uvc_setup();

#endif /* INCLUDES_USB_UVC_H_ */
