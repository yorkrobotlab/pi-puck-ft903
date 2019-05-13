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
#include <stdlib.h>

#include <ft900.h>
#include <ft900_uart_simple.h>

#include <ft900_usb.h>
#include <ft900_usbd.h>
#include <ft900_usb_uvc.h>
#include <ft900_startup_dfu.h>

/* UART support for printf output. */
#include "tinyprintf.h"

#include "usbd_uvc_v1_1.h"
#include "camera.h"

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

/* GLOBAL VARIABLES ****************************************************************/

/* LOCAL VARIABLES *****************************************************************/

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

		// String 3 (Serial Number): "xxxx0001" where "xxxx" is the camera module ID.
		UNICODE_LEN(8), L'x', L'x', L'x', L'x', L'0', L'0', L'0', L'1',

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
};

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
				0x00 /* configuration.bMaxPower */ // 0mA
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
 @brief Configuration Descriptors.
 @details Configuration descriptors may need to be modified to turn from type
 USB_DESCRIPTOR_TYPE_CONFIGURATION to USB_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION.
 These structures are defined programmatically depending on the modes that the
 camera interface adds.
 */
char *config_descriptor_buffer_hs;
char *config_descriptor_buffer_fs;

/**
 @brief Active Alternate Setting
 @details Current active alternate setting for the USB interface.
 */
static uint8_t usb_alt = 0;

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
USB_UVC_VideoProbeAndCommitControls uvc_probe_def_hs = {
		USB_UVC_VS_PROBE_COMMIT_CONTROL_BMHINT_FRAMINGINFO, /* bmHint */
		1, /*  bFormatIndex */
		1, /*  bFrameIndex */
		0, /*  dwFrameInterval */
		0, /*  wKeyFrameRate */
		0, /*  wPFrameRate */
		0, /*  wCompQuality */
		0, /*  wCompWindowSize */
		0, /*  wDelay */
		0, /*  dwMaxVideoFrameSize */
		sizeof(USB_UVC_Payload_Header), /*  dwMaxPayloadTransferSize */
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

/** @brief Format index for uncompressed streams.
 */
uint8_t uvc_format_index_uncompressed = 0;

/* MACROS **************************************************************************/

/* LOCAL FUNCTIONS / INLINES *******************************************************/

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
			camera_set_state(CAMERA_STREAMING_START);
		}
		else
		{
			camera_set_state(CAMERA_STREAMING_STOP);
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
		int8_t frame_rate;
		uint16_t width, height;
		uint8_t index = 0;
		uint8_t format;
		uint8_t frame;
		uint8_t count;
		int8_t i;

		// Check for valid format index set.
		if (probecommit->bFormatIndex == uvc_format_index_uncompressed)
		{
			format = CAMERA_FORMAT_UNCOMPRESSED;
		}
		else
		{
			return USBD_ERR_NOT_SUPPORTED;
		}

		// Match frame index to camera module reference.
		count = camera_mode_get_frame_count(format);
		for (frame = 0; frame < count; frame++)
		{
			index = camera_mode_get_frame(format,
					frame, &width, &height);
			if (index == probecommit->bFrameIndex)
			{
				break;
			}
			index = 0;
		}

		// If camera module reference found.
		if (index)
		{
			// Get total number of frame rates for this frame index.
			count = camera_mode_get_frame_rate_count(format, frame);
			// Get default frame rate for this frame index.
			frame_rate = camera_mode_get_frame_rate(format,	frame, 0);

			// If frame interval hint is set then check the requested frame interval.
			if (probecommit->bmFramingInfo & USB_UVC_VS_PROBE_COMMIT_CONTROL_BMHINT_FRAMINGINFO)
			{
				for (i = 0; i < count; i++)
				{
					frame_rate = camera_mode_get_frame_rate(format, frame, i);

					// Check frame interval is supported.
					if (probecommit->dwFrameInterval == 10000000 / frame_rate)
					{
						status = USBD_OK;
						break;
					}
				}
			}
			else
			{
				status = USBD_OK;
			}
#ifdef USB_ENDPOINT_USE_ISOC
			probecommit->dwMaxPayloadTransferSize = camera_mode_get_sample_size(format, frame,
					UVC_DATA_EP_SIZE_HS - sizeof(USB_UVC_Payload_Header)) + sizeof(USB_UVC_Payload_Header);
#else // !USB_ENDPOINT_USE_ISOC
			probecommit->dwMaxPayloadTransferSize = camera_mode_get_sample_size(format, frame, 0) +
					sizeof(USB_UVC_Payload_Header);
#endif // USB_ENDPOINT_USE_ISOC
			probecommit->dwMaxVideoFrameSize = width * height * FORMAT_UC_BBP;
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

	memset(uvc_probe_ret, 0, sizeof(USB_UVC_VideoProbeAndCommitControls));

	// bFormatIndex and bFrameIndex must have been specified by a SET_CUR or
	// the default setting.
	uvc_probe_ret->bFormatIndex = uvc_probe_cur->bFormatIndex;
	uvc_probe_ret->bFrameIndex = uvc_probe_cur->bFrameIndex;
	uvc_probe_ret->bmHint = uvc_probe_cur->bmHint;
	uvc_probe_ret->bmFramingInfo = uvc_probe_cur->bmFramingInfo;

	status = class_vs_check_probecommit(uvc_probe_ret);

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
	//uint16_t bbp = 0;

	uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_INVALID_REQUEST;

	camera_stop();

	// Check for valid format index set.
	if ((commit->bmFramingInfo & USB_UVC_VS_PROBE_COMMIT_CONTROL_BMFRAMINGINFO_FRAMEIDFIELD) &&
			(commit->bmFramingInfo & USB_UVC_VS_PROBE_COMMIT_CONTROL_BMFRAMINGINFO_EOFFIELD) &&
			(commit->bPreferedVersion == USB_VIDEO_CLASS_VERSION_MINOR))
	{
		int8_t frame_rate;
		uint16_t width, height;
		uint16_t sample;
		uint16_t payload;
		uint8_t index = 0;
		uint8_t format;
		uint8_t frame;
		uint8_t count;
		int8_t i;

		// Check for valid format index set.
		if (commit->bFormatIndex == uvc_format_index_uncompressed)
		{
			format = CAMERA_FORMAT_UNCOMPRESSED;
		}
		else
		{
			return USBD_ERR_NOT_SUPPORTED;
		}

		// Match frame index to camera module reference.
		count = camera_mode_get_frame_count(format);
		for (frame = 0; frame < count; frame++)
		{
			index = camera_mode_get_frame(format,
					frame, &width, &height);
			if (index == commit->bFrameIndex)
			{
				break;
			}
			index = 0;
		}

		// If camera module reference found.
		if (index)
		{
			// Get total number of frame rates for this frame index.
			count = camera_mode_get_frame_rate_count(format, frame);
			// Get default frame rate for this frame index.
			frame_rate = camera_mode_get_frame_rate(format,	frame, 0);
			// Get the sample size for USB.
#ifdef USB_ENDPOINT_USE_ISOC
			sample = camera_mode_get_sample_size(format, frame,
					UVC_DATA_EP_SIZE_HS - sizeof(USB_UVC_Payload_Header));
#else // !USB_ENDPOINT_USE_ISOC
			sample = camera_mode_get_sample_size(format, frame, 0);
#endif // USB_ENDPOINT_USE_ISOC
			payload = sample + sizeof(USB_UVC_Payload_Header);
			if (payload == commit->dwMaxPayloadTransferSize)
			{
				// If frame interval hint is set then check the requested frame interval.
				if (commit->bmFramingInfo & USB_UVC_VS_PROBE_COMMIT_CONTROL_BMHINT_FRAMINGINFO)
				{
					for (i = 0; i < count; i++)
					{
						frame_rate = camera_mode_get_frame_rate(format, frame, i);

						// Check frame interval is supported.
						if (commit->dwFrameInterval == 10000000 / frame_rate)
						{
							status = USBD_OK;
							break;
						}
					}
				}
			}
			else
			{
				status = USBD_OK;
			}

			if (status == USBD_OK)
			{
				camera_set(width, height, frame_rate, format, sample);
				// Check the sample length is suitable for an isochronous endpoint where it
				// must transmit the whole sample with a header in a single packet.

#ifdef USB_ENDPOINT_USE_ISOC
				if (camera_get_sample() + sizeof(USB_UVC_Payload_Header) > UVC_DATA_EP_SIZE_HS)
				{
					// Cause a STALL if the configuration is illegal.
					status = USBD_ERR_INVALID_PARAMETER;
					uvc_error_control = USB_UVC_REQUEST_ERROR_CODE_CONTROL_OUT_OF_RANGE;

					camera_set_sample(0);
					camera_set_state(CAMERA_STREAMING_STOP);
				}
#endif // USB_ENDPOINT_USE_ISOC
			}
		}
	}

	if (status == USBD_OK)
	{
#ifndef USB_ENDPOINT_USE_ISOC
		camera_set_state(CAMERA_STREAMING_START);
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
			USB_configuration_descriptor *pConfigDescriptor;

			if (usb_speed == USBD_SPEED_HIGH)
			{
				pConfigDescriptor = (void *)config_descriptor_buffer_hs;
			}
			else
			{
				pConfigDescriptor = (void *)config_descriptor_buffer_fs;
			}

			if (length > pConfigDescriptor->wTotalLength) // too many bytes requested
				length = pConfigDescriptor->wTotalLength; // Entire structure.

			src = (uint8_t *)pConfigDescriptor;
			pConfigDescriptor->bDescriptorType = USB_DESCRIPTOR_TYPE_CONFIGURATION;
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
			USB_configuration_descriptor *pConfigDescriptor;

			if (usb_speed == USBD_SPEED_HIGH)
			{
				pConfigDescriptor = (void *)config_descriptor_buffer_fs;
			}
			else
			{
				pConfigDescriptor = (void *)config_descriptor_buffer_hs;
			}
			if (length > pConfigDescriptor->wTotalLength) // too many bytes requested
				length = pConfigDescriptor->wTotalLength; // Entire structure.

			src = (uint8_t *)pConfigDescriptor;
			pConfigDescriptor->bDescriptorType = USB_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION;
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
				BRIDGE_DEBUG_PRINTF("Clear feature EP %x\r\n", req->wIndex);

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
	camera_set_state(CAMERA_STREAMING_STOP);
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

	camera_stop();

	return;
}

int8_t usb_uvc_has_commit()
{
	if ((uvc_commit.bFormatIndex > FORMAT_INDEX_TYPE_NONE)
			&& (uvc_commit.bFormatIndex < FORMAT_INDEX_MAX))
	{
		return (uvc_commit.bFormatIndex != FORMAT_INDEX_TYPE_NONE);
	}
	return 0;
}

int8_t usb_uvc_is_uncompressed()
{
	if ((uvc_commit.bFormatIndex > FORMAT_INDEX_TYPE_NONE)
			&& (uvc_commit.bFormatIndex < FORMAT_INDEX_MAX))
	{
		return (uvc_commit.bFormatIndex == uvc_format_index_uncompressed);
	}
	return 0;
}

uint8_t usb_uvc_get_alt()
{
	return usb_alt;
}

#define ADD_CONFIG_DESCRIPTOR(A, B) memcpy(A, &B, B.bLength); A += B.bLength;
#define ADD_CONFIG_DESCRIPTOR_LEN(B, C) C += B.bLength;
#define MIN(a,b) ((a<b)?a:b)

void usb_uvc_build_configuration(uint16_t module)
{
	uint8_t *pCdEnd_hs;
	uint16_t lenConfigDescriptor_hs = 0;
	uint16_t lenCSInterfaceDescriptor_hs = 0;
	uint16_t lenCSInputDescriptor_hs = 0;

	USB_configuration_descriptor *pConfigDescriptor_hs;
	/* Single interface header descriptor. */
	USB_UVC_VC_CSInterfaceHeaderDescriptor(1) *pCSInterfaceDescriptor_hs;
	/* Input header descriptors for Uncompressed and MJPEG. */
	USB_UVC_VS_CSInterfaceInputHeaderDescriptor(2) *pCSInputDescriptor_hs;

	uint8_t *pCdEnd_fs;
	uint16_t lenConfigDescriptor_fs = 0;
	uint16_t lenCSInterfaceDescriptor_fs = 0;

	USB_configuration_descriptor *pConfigDescriptor_fs;
	USB_UVC_VC_CSInterfaceHeaderDescriptor(1) *pCSInterfaceDescriptor_fs;

	uint8_t countFrameUncompressed = camera_mode_get_frame_count(CAMERA_FORMAT_UNCOMPRESSED);
	uint8_t countFormats = MIN(countFrameUncompressed, 1);
	uint16_t countFrameRatesUncompressed = 0;
	uint8_t defaultFormat = 1;
	uint8_t defaultFrame = 1;

	uint16_t i;
	uint8_t slen;
	uint8_t serial;
	uint8_t *src;

	uint16_t len_hs;
	uint16_t len_fs;

	uint16_t width, height;
	int8_t frame_rate;
	uint8_t frame_index;

	for (i = 0; i < countFrameUncompressed; i++)
	{
		countFrameRatesUncompressed += camera_mode_get_frame_rate_count(CAMERA_FORMAT_UNCOMPRESSED, i);
	}

	len_hs =
			sizeof(USB_configuration_descriptor) +
			sizeof(USB_UVC_interface_association_descriptor) +

			sizeof(USB_UVC_VC_StandardInterfaceDescriptor) +
			sizeof(struct UVC_VC_config_descriptor) +
			sizeof(USB_UVC_VC_StandardInterruptEndpointDescriptor) +
			sizeof(USB_UVC_VC_CSEndpointDescriptor) +

			sizeof(USB_UVC_VS_StandardInterfaceDescriptor) +
			sizeof(USB_UVC_VS_CSInterfaceInputHeaderDescriptor(countFormats)) +

#ifndef USB_ENDPOINT_USE_ISOC
			sizeof(USB_UVC_VS_BulkVideoDataEndpointDescriptor) +
#else // !USB_ENDPOINT_USE_ISOC
			sizeof(USB_UVC_VS_StandardInterfaceDescriptor) +
			sizeof(USB_UVC_VS_IsochronousVideoDataEndpointDescriptor) +
#endif // USB_ENDPOINT_USE_ISOC

#ifdef USB_INTERFACE_USE_DFU
			sizeof(USB_interface_descriptor) +
			sizeof(USB_dfu_functional_descriptor) +
#endif // USB_INTERFACE_USE_DFU
			0;

	if (countFrameUncompressed)
	{
		len_hs += sizeof(USB_UVC_VS_UncompressedVideoFormatDescriptor) +
				(sizeof(USB_UVC_VS_UncompressedVideoFrameDescriptorDiscrete(0)) * countFrameUncompressed) +
				sizeof(USB_UVC_ColorMatchingDescriptor);
		len_hs += (sizeof(unsigned long) * countFrameRatesUncompressed);
	}

	len_fs =
			sizeof(USB_configuration_descriptor) +
			sizeof(USB_UVC_interface_association_descriptor) +

			sizeof(USB_UVC_VC_StandardInterfaceDescriptor) +
			sizeof(struct UVC_VC_config_descriptor) +
			sizeof(USB_UVC_VC_StandardInterruptEndpointDescriptor) +
			sizeof(USB_UVC_VC_CSEndpointDescriptor) +

#ifdef USB_INTERFACE_USE_DFU
			sizeof(USB_interface_descriptor dfu_interface) +
			sizeof(USB_dfu_functional_descriptor dfu_functional) +
#endif // USB_INTERFACE_USE_DFU
			0;

	config_descriptor_buffer_hs = (char *)malloc(len_hs);
	config_descriptor_buffer_fs = (char *)malloc(len_fs);

	memset(config_descriptor_buffer_hs, 0, len_hs);
	memset(config_descriptor_buffer_fs, 0, len_fs);

	pCdEnd_hs = (uint8_t *)config_descriptor_buffer_hs;
	pCdEnd_fs = (uint8_t *)config_descriptor_buffer_fs;

	{
		USB_configuration_descriptor c = {
				sizeof(USB_configuration_descriptor), /* configuration.bLength */
				USB_DESCRIPTOR_TYPE_CONFIGURATION, /* configuration.bDescriptorType */
				len_hs, /* configuration.wTotalLength */
#ifdef USB_INTERFACE_USE_DFU
				0x03, /* configuration.bNumInterfaces */
#else // !USB_INTERFACE_USE_DFU
				0x02, /* configuration.bNumInterfaces */
#endif // USB_INTERFACE_USE_DFU
				0x01, /* configuration.bConfigurationValue */
				0x00, /* configuration.iConfiguration */
				USB_CONFIG_BMATTRIBUTES_VALUE, /* configuration.bmAttributes */
				//0xFA, /* configuration.bMaxPower */           // 500mA
				0x00, /* configuration.bMaxPower */           // 0mA
		};
		pConfigDescriptor_hs = (USB_configuration_descriptor *)pCdEnd_hs;
		pConfigDescriptor_fs = (USB_configuration_descriptor *)pCdEnd_fs;

		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);

		ADD_CONFIG_DESCRIPTOR(pCdEnd_fs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_fs);

	};
	{
		USB_UVC_interface_association_descriptor c = {
				sizeof(USB_UVC_interface_association_descriptor), /* interface_association.bLength */
				USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION, /* interface_association.bDescriptorType */
				0, /* interface_association.bFirstInterface */
				2, /* interface_association.bInterfaceCount */
				USB_CLASS_VIDEO, /* interface_association.bFunctionClass */
				USB_SUBCLASS_VIDEO_INTERFACE_COLLECTION, /* interface_association.bFunctionSubClass */
				USB_PROTOCOL_VIDEO_UNDEFINED, /* interface_association.bFunctionProtocol */
				2, /* interface_association.iFunction */  // "FT900 UVC"
		};
		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);

		ADD_CONFIG_DESCRIPTOR(pCdEnd_fs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_fs);
	};

	// ---- INTERFACE DESCRIPTOR ----
	{
		USB_UVC_VC_StandardInterfaceDescriptor c = {
				sizeof(USB_UVC_VC_StandardInterfaceDescriptor), /* interface_video_control.bLength */
				USB_DESCRIPTOR_TYPE_INTERFACE, /* interface_video_control.bDescriptorType */
				0, /* interface_video_control.bInterfaceNumber */
				0x00, /* interface_video_control.bAlternateSetting */
				0x01, /* interface_video_control.bNumEndpoints */
				USB_CLASS_VIDEO, /* interface_video_control.bInterfaceClass */ // UVC Class
				USB_SUBCLASS_VIDEO_VIDEOCONTROL, /* interface_video_control.bInterfaceSubClass */ // Abstract Control Model
				USB_PROTOCOL_VIDEO_UNDEFINED, /* interface_video_control.bInterfaceProtocol */ // No built-in protocol
				2, /* interface_video_control.iInterface */ // Same as IAD iFunction.
		};
		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);

		ADD_CONFIG_DESCRIPTOR(pCdEnd_fs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_fs);
	};

	{
		// ---- Class specific VC Interface Header Descriptor ----
		{
			USB_UVC_VC_CSInterfaceHeaderDescriptor(1) c = {
					sizeof(USB_UVC_VC_CSInterfaceHeaderDescriptor(1)), /* vc_header.bLength */
					USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* vc_header.bDescriptorType */
					USB_UVC_DESCRIPTOR_SUBTYPE_VC_HEADER, /* vc_header.bDescriptorSubtype */
					(USB_VIDEO_CLASS_VERSION_MAJOR << 8) | (USB_VIDEO_CLASS_VERSION_MINOR << 4), /* vc_header.bpCdEndUVC */
					sizeof(struct UVC_VC_config_descriptor), /* vc_header.wTotalLength */
					CLK_FREQ_48MHz, /* vc_header.dwClockFrequency */
					0x01, /* vc_header.bInCollection */
					{0x01,} /* vc_header.baInterfaceNr */
			};

			pCSInterfaceDescriptor_hs = (void *)pCdEnd_hs;
			pCSInterfaceDescriptor_fs = (void *)pCdEnd_fs;

			ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
			ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
			ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInterfaceDescriptor_hs);

			ADD_CONFIG_DESCRIPTOR(pCdEnd_fs, c);
			ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_fs);
			ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInterfaceDescriptor_fs);
		};

	// ---- Input Terminal Descriptor - Camera ----
	{
		USB_UVC_VC_CameraTerminalDescriptor(2) c = {
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
		};

		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR(pCdEnd_fs, c);

		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInterfaceDescriptor_hs);

		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_fs);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInterfaceDescriptor_fs);
	};

	// ---- Output Terminal Descriptor ----
	{
		USB_UVC_VC_OutputTerminalDescriptor c = {
				sizeof(USB_UVC_VC_OutputTerminalDescriptor), /* camera_output.bLength */
				USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* camera_output.bDescriptorType */
				USB_UVC_DESCRIPTOR_SUBTYPE_VC_OUTPUT_TERMINAL, /* camera_output.bDescriptorSubtype */
				ENTITY_ID_OUTPUT, /* camera_output.bTerminalID */
				USB_UVC_TT_STREAMING, /* camera_output.wTerminalType */
				0x00, /* camera_output.bAssocTerminal */
				ENTITY_ID_PROCESSING, /* camera_output.bSourceID */
				0x00, /* camera_output.iTerminal */
		};

		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR(pCdEnd_fs, c);

		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInterfaceDescriptor_hs);

		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_fs);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInterfaceDescriptor_fs);
	};

	// ---- Processing Unit Descriptor ----
	{
		USB_UVC_VC_ProcessingUnitDescriptor(2) c = {
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
		};

		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR(pCdEnd_fs, c);

		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInterfaceDescriptor_hs);

		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_fs);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInterfaceDescriptor_fs);
	};

	pCSInterfaceDescriptor_hs->wTotalLength = lenCSInterfaceDescriptor_hs;
	pCSInterfaceDescriptor_fs->wTotalLength = lenCSInterfaceDescriptor_fs;
	};

	// ---- ENDPOINT DESCRIPTOR ----
	{
		USB_UVC_VC_StandardInterruptEndpointDescriptor c = {
				sizeof(USB_UVC_VC_StandardInterruptEndpointDescriptor), /* endpoint_int_in.bLength */
				USB_DESCRIPTOR_TYPE_ENDPOINT, /* endpoint_int_in.bDescriptorType */
				USB_ENDPOINT_DESCRIPTOR_EPADDR_IN | UVC_EP_INTERRUPT, /* endpoint_int_in.bEndpointAddress */
				USB_ENDPOINT_DESCRIPTOR_ATTR_INTERRUPT, /* endpoint_int_in.bmAttributes */
				UVC_INTERRUPT_EP_SIZE, /* endpoint_int_in.wMaxPacketSize */
				0x08, /* endpoint_int_in.bInterval */
		};

		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);

		ADD_CONFIG_DESCRIPTOR(pCdEnd_fs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_fs);
	};

	// ---- Class Specific ENDPOINT DESCRIPTOR ----
	{
		USB_UVC_VC_CSEndpointDescriptor c = {
				sizeof(USB_UVC_VC_CSEndpointDescriptor), /* endpoint_int_descriptor.bLength */
				USB_UVC_DESCRIPTOR_TYPE_CS_ENDPOINT, /* endpoint_int_descriptor.bDescriptorType */
				USB_UVC_DESCRIPTOR_SUBTYPE_EP_INTERRUPT, /* endpoint_int_descriptor.bDescriptorSubType */
				UVC_INTERRUPT_EP_SIZE, /* endpoint_int_descriptor.wMaxTransferSize */
		};

		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);

		ADD_CONFIG_DESCRIPTOR(pCdEnd_fs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_fs);
	};


	// ---- Standard Video Streaming Interface Descriptor ----
	{
		USB_UVC_VS_StandardInterfaceDescriptor c = {
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
		};

		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
	};

	{
		// ---- Class-specific Video Streaming Input Header Descriptor ----
		{
			USB_UVC_VS_CSInterfaceInputHeaderDescriptor(0) c = {
					sizeof(USB_UVC_VS_CSInterfaceInputHeaderDescriptor(0)), /* vs_header.bLength */
					USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* vs_header.bDescriptorType */
					USB_UVC_DESCRIPTOR_SUBTYPE_VS_INPUT_HEADER, /* vs_header.bDescriptorSubType */
					countFormats, /* vs_header.bNumFormats */
					0, /* vs_header.wTotalLength */
					USB_ENDPOINT_DESCRIPTOR_EPADDR_IN | UVC_EP_DATA_IN, /* vs_header.bEndpointAddress */
					0x00, /* vs_header.bmInfo */
					0x03, /* vs_header.bTerminalLink */
					0x00, /* vs_header.bStillCaptureMethod */
					0x00, /* vs_header.bTriggerSupport */
					0x00, /* vs_header.bTriggerUsage */
					0x01, /* vs_header.bControlSize */
					//{
					//0x00, /* vs_header.bmaControls format 1 */
					//},
			};

			pCSInputDescriptor_hs = (void *)pCdEnd_hs;

			ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);

			if (countFrameUncompressed)
			{
				pCdEnd_hs++;
				pCSInputDescriptor_hs->bLength += sizeof(unsigned char);
				lenConfigDescriptor_hs++;
				lenCSInputDescriptor_hs++;
			}

			ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
			ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInputDescriptor_hs);
		}

		countFormats = 1;

		// ---- Class specific Uncompressed VS Format Descriptor ----
		if (countFrameUncompressed)
		{
			uvc_format_index_uncompressed = countFormats;

			USB_UVC_VS_UncompressedVideoFormatDescriptor c = {
					sizeof(USB_UVC_VS_UncompressedVideoFormatDescriptor), /* format.bLength */
					USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* format.bDescriptorType */
					USB_UVC_DESCRIPTOR_SUBTYPE_VS_FORMAT_UNCOMPRESSED, /* format.bDescriptorSubType */
					countFormats, /* format.bFormatIndex */
					countFrameUncompressed, /* format.bNumFrameDescriptors */
					PAYLOAD_FORMAT_UNCOMPRESSED, /* format.guidFormat[16] */
					PAYLOAD_BBP_UNCOMPRESSED, /* format.bBitsPerPixel */
					1, /* format.bDefaultFrameIndex */
					FRAME_RATIO_X, /* format.bAspectRatioX */
					FRAME_RATIO_Y, /* format.bAspectRatioY */
					0x00, /* format.bmInterlaceFlags */
					0x00, /* format.bCopyProtect */
			};

			ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
			ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
			ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInputDescriptor_hs);

			countFrameUncompressed = 0;

			// ---- Class specific Uncompressed VS Frame Descriptor ----
			do
			{
				frame_index = camera_mode_get_frame(CAMERA_FORMAT_UNCOMPRESSED, countFrameUncompressed,
						&width, &height);
				if (frame_index == 0)
				{
					break;
				}

				countFrameRatesUncompressed = camera_mode_get_frame_rate_count(CAMERA_FORMAT_UNCOMPRESSED,
						countFrameUncompressed);
				/* Get first frame rate for default. */
				frame_rate = camera_mode_get_frame_rate(CAMERA_FORMAT_UNCOMPRESSED, countFrameUncompressed,
						0);
				{
					USB_UVC_VS_UncompressedVideoFrameDescriptorDiscrete(0) c = {
							sizeof(USB_UVC_VS_UncompressedVideoFrameDescriptorDiscrete(countFrameRatesUncompressed)), /* frame.bLength */
							USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* frame.bDescriptorType */
							USB_UVC_DESCRIPTOR_SUBTYPE_VS_FRAME_UNCOMPRESSED, /* frame.bDescriptorSubType */
							frame_index, /* frame.bFrameIndex */
							0x00, /* frame.bmCapabilities */
							width, /* frame.wWidth */
							height, /* frame.wHeight */
							(width * height * FORMAT_UC_BBP) * frame_rate * 8, /* frame.dwMinBitRate */
							(width * height * FORMAT_UC_BBP) * frame_rate * 8, /* frame.dwMaxBitRate */
							(width * height * FORMAT_UC_BBP), /* frame.dwMaxVideoFrameBufferSize */
							10000000 / frame_rate, /* frame.dwDefaultFrameInterval */
							countFrameRatesUncompressed, /* frame.bFrameIntervalType */
					};

					for (i = 0; i < countFrameRatesUncompressed; i++)
					{
						frame_rate = camera_mode_get_frame_rate(CAMERA_FORMAT_UNCOMPRESSED,
								countFrameUncompressed, i);
						c.dwFrameInterval[i] = 10000000 / frame_rate; /* frame.dwFrameInterval */
					}

					ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
					ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
					ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInputDescriptor_hs);
				}
				countFrameUncompressed++;
			} while (frame_index);

			// ---- Class specific Color Matching Descriptor ----
			{
				USB_UVC_ColorMatchingDescriptor c = {
						sizeof(USB_UVC_ColorMatchingDescriptor),  /* desc.bLength */
						USB_UVC_DESCRIPTOR_TYPE_CS_INTERFACE, /* desc.bDescriptorType */
						USB_UVC_DESCRIPTOR_SUBTYPE_VS_COLORFORMAT, /* desc.bDescriptorSubType */
						1, /* desc.bColorPrimaries */
						1, /* desc.bTransferCharacteristics */
						4, /* desc.bMatrixCoefficients */
				};

				ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
				ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
				ADD_CONFIG_DESCRIPTOR_LEN(c, lenCSInputDescriptor_hs);
			};

			countFormats++;
		};
	};

	pCSInputDescriptor_hs->wTotalLength = lenCSInputDescriptor_hs;

#ifndef USB_ENDPOINT_USE_ISOC
	// ---- ENDPOINT DESCRIPTOR ----
	{
		USB_UVC_VS_BulkVideoDataEndpointDescriptor c = {
				sizeof(USB_UVC_VS_BulkVideoDataEndpointDescriptor), /* endpoint_bulk_in.bLength */
				USB_DESCRIPTOR_TYPE_ENDPOINT, /* endpoint_bulk_in.bDescriptorType */
				USB_ENDPOINT_DESCRIPTOR_EPADDR_IN | UVC_EP_DATA_IN, /* endpoint_bulk_in.bEndpointAddress */
				USB_ENDPOINT_DESCRIPTOR_ATTR_BULK, /* endpoint_bulk_in.bmAttributes */
				UVC_DATA_EP_SIZE_HS, /* endpoint_bulk_in.wMaxPacketSize */
				0x00, /* endpoint_bulk_in.bInterval */
		};
		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
	};
#else // !USB_ENDPOINT_USE_ISOC
	// ---- Standard Video Streaming Interface Descriptor ----
	{
		USB_UVC_VS_StandardInterfaceDescriptor c = {
				sizeof(USB_UVC_VS_StandardInterfaceDescriptor), /* interface_video_stream.bLength */
				USB_DESCRIPTOR_TYPE_INTERFACE, /* interface_video_stream.bDescriptorType */
				1, /* interface_video_stream.bInterfaceNumber */
				0x01, /* interface_video_stream.bAlternateSetting */
				0x01, /* interface_video_stream.bNumEndpoints */
				USB_CLASS_VIDEO, /* interface_video_stream.bInterfaceClass */
				USB_SUBCLASS_VIDEO_VIDEOSTREAMING, /* interface_video_stream.bInterfaceSubClass */
				USB_PROTOCOL_VIDEO_UNDEFINED, /* interface_video_stream.bInterfaceProtocol */
				0x00 /* interface_video_stream.iInterface */
		};

		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
	};

	// ---- ENDPOINT DESCRIPTOR ----
	{
		USB_UVC_VS_IsochronousVideoDataEndpointDescriptor c = {
				sizeof(USB_UVC_VS_IsochronousVideoDataEndpointDescriptor), /* endpoint_bulk_in.bLength */
				USB_DESCRIPTOR_TYPE_ENDPOINT, /* endpoint_bulk_in.bDescriptorType */
				USB_ENDPOINT_DESCRIPTOR_EPADDR_IN | UVC_EP_DATA_IN, /* endpoint_bulk_in.bEndpointAddress */
				USB_ENDPOINT_DESCRIPTOR_ATTR_ISOCHRONOUS /*|
			USB_ENDPOINT_DESCRIPTOR_ISOCHRONOUS_ASYNCHRONOUS*/, /* endpoint_bulk_in.bmAttributes */
			/*USB_ENDPOINT_DESCRIPTOR_MAXPACKET_ADDN_TRANSACTION_1 |*/ UVC_DATA_EP_SIZE_HS, /* endpoint_bulk_in.wMaxPacketSize */
			0x01, /* endpoint_bulk_in.bInterval */
		};

		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
	};
#endif // USB_ENDPOINT_USE_ISOC

#ifdef USB_INTERFACE_USE_DFU
	// ---- INTERFACE DESCRIPTOR for DFU Interface ----
	{
		USB_interface_descriptor c = {
				sizeof(USB_interface_descriptor), /* dfu_interface.bLength */
				USB_DESCRIPTOR_TYPE_INTERFACE, /* dfu_interface.bDescriptorType */
				DFU_USB_INTERFACE_RUNTIME, /* dfu_interface.bInterfaceNumber */
				0x00, /* dfu_interface.bAlternateSetting */
				0x00, /* dfu_interface.bNumEndpoints */
				USB_CLASS_APPLICATION, /* dfu_interface.bInterfaceClass */ // bInterfaceClass: Application Specific Class
				USB_SUBCLASS_DFU, /* dfu_interface.bInterfaceSubClass */ // bInterfaceSubClass: Device Firmware Update
				USB_PROTOCOL_DFU_RUNTIME, /* dfu_interface.bInterfaceProtocol */ // bInterfaceProtocol: Runtime Protocol
				0x05 /* dfu_interface.iInterface */       // * iInterface: "DFU Interface"
		};

		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
	};

	// ---- FUNCTIONAL DESCRIPTOR for DFU Interface ----
	{
		USB_dfu_functional_descriptor c = {
				sizeof(USB_dfu_functional_descriptor), /* dfu_functional.bLength */
				USB_DESCRIPTOR_TYPE_DFU_FUNCTIONAL, /* dfu_functional.bDescriptorType */
				DFU_ATTRIBUTES, /* dfu_functional.bmAttributes */  	// bmAttributes
				DFU_TIMEOUT, /* dfu_functional.wDetatchTimeOut */ // wDetatchTimeOut
				DFU_TRANSFER_SIZE, /* dfu_functional.wTransferSize */     // wTransferSize
				USB_BCD_VERSION_DFU_1_1 /* dfu_functional.bpCdEndDfuVersion */ // bpCdEndDfuVersion: DFU Version 1.1
		};

		ADD_CONFIG_DESCRIPTOR(pCdEnd_hs, c);
		ADD_CONFIG_DESCRIPTOR_LEN(c, lenConfigDescriptor_hs);
	};
#endif // USB_INTERFACE_USE_DFU

	pConfigDescriptor_hs->wTotalLength = lenConfigDescriptor_hs;
	pConfigDescriptor_fs->wTotalLength = lenConfigDescriptor_fs;

	// Find the serial number string in the string descriptor table
	serial = 3; // serial number is the third string
	i = 0;
	src = (uint8_t *)string_descriptor;

#define TO_HEX(A) ((A) <= 9 ? '0' + (A) : 'A' - 10 + (A))

	while ((slen = ((uint8_t *)string_descriptor)[i]) > 0)
	{
		src = (uint8_t *) &((uint8_t *)string_descriptor)[i];
		if (serial == 0)
		{
			break;
		}
		i += slen;
		serial--;
	}
	if (serial == 0)
	{
		src[2] = TO_HEX((module >> 12)&0x0f);
		src[4] = TO_HEX((module >> 8)&0x0f);
		src[6] = TO_HEX((module >> 4)&0x0f);
		src[8] = TO_HEX((module >> 0)&0x0f);
	}

	uvc_probe.bFrameIndex = uvc_probe_def_hs.bFrameIndex = defaultFrame;
	uvc_probe.bFormatIndex = uvc_probe_def_hs.bFormatIndex = defaultFormat;
	// Setup the default probe settings.
	class_vs_check_probecommit(&uvc_probe_def_hs);

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
}

int8_t usb_uvc_bandwidth_ok(uint16_t width, uint16_t height, uint8_t frame_rate)
{
#ifdef USB_ENDPOINT_USE_ISOC
	if (width * height * 2 * frame_rate >
				((UVC_DATA_EP_SIZE_HS - sizeof(USB_UVC_Payload_Header)) * 8 * 1000))
	{
		// The number of bytes to send is greater than the theoretical
		// maximum number of bytes that may be sent by an isochronous
		// endpoint. Access constraints are 1 icochronous transfer per
		// microframe on Rev A and Rev B devices.
		return 0;
	}
	return 1;
#else // !USB_ENDPOINT_USE_ISOC
	// BULK transfers are not limited for bandwidth through bus access
	// constraints.
	return 1;
#endif // USB_ENDPOINT_USE_ISOC
}

uint16_t usb_uvc_init()
{
	uint16_t packet_len = 0;
	usb_speed = USBD_get_bus_speed();

	USBD_create_endpoint(UVC_EP_INTERRUPT, USBD_EP_INT, USBD_DIR_IN,
			UVC_INTERRUPT_USBD_EP_SIZE, USBD_DB_OFF, NULL /*ep_cb*/);

	if (usb_speed == USBD_SPEED_HIGH)
	{
#ifdef USB_ENDPOINT_USE_ISOC
		USBD_create_endpoint(UVC_EP_DATA_IN, USBD_EP_ISOC, USBD_DIR_IN,
				UVC_DATA_USBD_EP_SIZE_HS, USBD_DB_OFF, NULL /*ep_cb*/);
#else // !USB_ENDPOINT_USE_ISOC
		USBD_create_endpoint(UVC_EP_DATA_IN, USBD_EP_BULK, USBD_DIR_IN,
				UVC_DATA_USBD_EP_SIZE_HS, USBD_DB_ON, NULL /*ep_cb*/);
#endif // USB_ENDPOINT_USE_ISOC
		packet_len = UVC_DATA_EP_SIZE_HS;
	}
	else
	{
#ifdef USB_ENDPOINT_USE_ISOC
		USBD_create_endpoint(UVC_EP_DATA_IN, USBD_EP_ISOC, USBD_DIR_IN,
				UVC_DATA_USBD_EP_SIZE_FS, USBD_DB_ON, NULL /*ep_cb*/);
		packet_len = UVC_DATA_EP_SIZE_FS;
#endif // USB_ENDPOINT_USE_ISOC
	}
	return packet_len;
}

void usb_uvc_setup()
{
	USBD_ctx usb_ctx;

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
	usb_ctx.sof_cb = NULL;
	usb_ctx.lpm_cb = NULL;
	usb_ctx.speed = USBD_SPEED_HIGH;

	// Initialise the USB device with a control endpoint size
	// of 8 to 64 bytes. This must match the size for bMaxPacketSize0
	// defined in the device descriptor.
	usb_ctx.ep0_size = USB_CONTROL_EP_SIZE;

	USBD_initialise(&usb_ctx);
}

