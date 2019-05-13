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
#include <usbd_uvc_v1_1.h>

/* UART support for printf output. */
#include "tinyprintf.h"

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

/** @brief 16 bit Camera Module ID
 */
uint16_t module;

/** @brief Current threshold for data received from the camera module.
 */
static uint16_t sample_threshold;

/** @brief Used to wait for vsync (GPIO)
 */
static volatile uint8_t gpio_vsync = 0;

/**
 @brief Stream Properties
 @details A list of file/resource names which indicate the type of
 stream requested. A check is made to the camera interface to see if a
 particular stream is supported. Properties of the stream are used in
 setting up the camera when a stream is started.
 */
static struct stream_properties {
	char *stream_name; /// Name of resource or file for this stream.
	int8_t supported; /// Flag whether stream is supported by camera.
	int8_t format; /// Format of the stream, MJPEG or RAW.
	int8_t rate; /// Frame rate.
	uint16_t width; /// Frame width of stream.
	uint16_t height; /// Frame height of stream.
} streams[] = {
		{
				"qvga.raw", 0,
				CAMERA_FORMAT_UNCOMPRESSED,
				15, 320, 240,
		},
		{
				"vga.raw", 0,
				CAMERA_FORMAT_UNCOMPRESSED,
				15, 640, 480,
		},
};

/* GLOBAL VARIABLES ****************************************************************/

/* LOCAL VARIABLES *****************************************************************/

/**
 @brief Millisecond counter
 @details Count-up timer to provide the elapsed time for network operations.
 */
static uint32_t milliseconds = 0;

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
			update_leds();
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

void vsync_ISR(void)
{
	if (gpio_is_interrupted(8))
	{
		// Signal start of frame received. Will now wait for line data.
		gpio_vsync = 1;
	}
}

void wait_for_vsync()
{
	// Flush all waiting data in the camera buffer.
	gpio_vsync = 0;
	camera_vsync(&gpio_vsync);
}

uint8_t usbd_testing(void)
{
	uint8_t not_connected = 1;
	uint32_t camera_tx_frame_size = 0;
	uint8_t *pstart = NULL;

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

	// Current USB alternate interface
	uint8_t alt = 0;

	// Header for UVC sample transfer.
	static USB_UVC_Payload_Header hdr;

	// Header length always stays the same.
	hdr.bHeaderLength = sizeof(USB_UVC_Payload_Header);

	usb_uvc_setup();

	camera_set_state(CAMERA_STREAMING_OFF);

	for (;;)
	{
		USBD_attach();

		if (USBD_connect() == USBD_OK)
		{
			packet_len = usb_uvc_init();
			usb_uvc_build_configuration(module);

			if (USBD_DFU_is_runtime())
			{
				// Start the UVC emulation code.
				while (USBD_is_connected())
				{
					if (USBD_get_state() == USBD_STATE_CONFIGURED)
					{
						if (not_connected)
						{
							// Now we are connected, draw the keyboard.
							tfp_printf("Starting %d\r\n", packet_len);
							not_connected = 0;
						}
						else
						{
#ifdef USB_ENDPOINT_USE_ISOC
							alt = usb_uvc_get_alt();
#endif // USB_ENDPOINT_USE_ISOC

							// Check a commit has occurred successfully first.
							if (usb_uvc_has_commit())
							{
								// Change in interface made for ISO mode.
								if (camera_get_state() != CAMERA_STREAMING_STARTED)
								{
									// Start or stop the camera.
#ifdef USB_ENDPOINT_USE_ISOC
									// Interface for non-zero-bandwith interface selected.
									if (alt == 1)
#else // !USB_ENDPOINT_USE_ISOC
										// Streaming commit received.
										if (camera_get_state() == CAMERA_STREAMING_START)
#endif // USB_ENDPOINT_USE_ISOC
										{
											/* Start the camera. */
											camera_start();

											sample_threshold = camera_get_sample();
											tfp_printf("Camera starting (sample length %d frame %ld)\r\n", sample_threshold, camera_get_frame_size());

											wait_for_vsync();

											camera_tx_frame_size = 0;
#ifndef USB_ENDPOINT_USE_ISOC
											alt = 1;
#endif // USB_ENDPOINT_USE_ISOC
										}
									// Stream stopping.
										else if (camera_get_state() == CAMERA_STREAMING_STOP)
										{
											// CAMERA_STREAMING_STOP or zero-bandwith interface selected.
											{
												camera_stop();

												cam_disable_interrupt();
												cam_stop();

												tfp_printf("Camera stopping\r\n");
#ifndef USB_ENDPOINT_USE_ISOC
												alt = 0;
#endif // USB_ENDPOINT_USE_ISOC
											}
										}
								}

								if (alt == 1)
								{
									if (!USBD_ep_buffer_full(UVC_EP_DATA_IN))
									{
										/* If we need to get more data for a payload.
										 */
										if (remain_len == 0)
										{
											// Set the header info frame toggle bit.
											hdr.bmHeaderInfo = frame_toggle | 0x80;

											// Send a full line of data if there is data available.
											pstart = camera_read();
											if (pstart)
											{
												len = camera_get_sample();

												if (usb_uvc_is_uncompressed())
												{
													camera_tx_frame_size += len;
													if (camera_tx_frame_size >= camera_get_frame_size())
													{
														// END of frame
														hdr.bmHeaderInfo |= 2;
														frame_toggle++; frame_toggle &= 1;

														len -= (camera_tx_frame_size - camera_get_frame_size());
														camera_tx_frame_size = 0;
													}

													remain_len = len;
												}

												if (remain_len)
												{
													// Add header to USB endpoint buffer.
													// Set flag to allow follow-on data.
													USBD_transfer_ex(UVC_EP_DATA_IN,
															(uint8_t *)&hdr,
															sizeof(USB_UVC_Payload_Header),
															USBD_TRANSFER_EX_PART_NO_SEND,
															0);

													part = USBD_TRANSFER_EX_PART_NORMAL;
													// Send follow-on data for payload.
													// Calculate the size of the remaining data for this
													// packet. It may all be able to be sent in one packet.
													if (len > (packet_len - sizeof(USB_UVC_Payload_Header)))
													{
														len = (packet_len - sizeof(USB_UVC_Payload_Header));
														part = USBD_TRANSFER_EX_PART_NO_SEND;
													}

													USBD_transfer_ex(UVC_EP_DATA_IN,
															pstart,
															len,
															part,
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
										if (remain_len)
										{
											part = USBD_TRANSFER_EX_PART_NORMAL;
											len = remain_len;

											// Do not send the header on all subsequent packets.
											// Send only one packet at a time.
											if (len >= (packet_len))
											{
												len = packet_len;
												part = USBD_TRANSFER_EX_PART_NO_SEND;
											}

											USBD_transfer_ex(UVC_EP_DATA_IN,
													&pstart[sample_threshold - remain_len],
													len,
													part,
													packet_len);

											remain_len -= len;
										}
#endif // USB_ENDPOINT_USE_ISOC
									}
								}
							}
						}
					}
				}
			}
			else
			{
				// In DFU mode. Process USB requests.
				while (USBD_is_connected()) {};
			}

			USBD_detach();
			tfp_printf("Restarting\r\n");
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

	BRIDGE_DEBUG_PRINTF("-------------------------------------------------\r\n");
	BRIDGE_DEBUG_PRINTF("Pi-puck FT903 Firmware v0.9.0\r\n");
	BRIDGE_DEBUG_PRINTF("USB Camera and I2C RGB LED controller\r\n");
	BRIDGE_DEBUG_PRINTF("Built: %s %s\r\n", __DATE__, __TIME__);
	BRIDGE_DEBUG_PRINTF("-------------------------------------------------\r\n");
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
	gpio_function(8, pad_cam_vd);  /* VD */
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
	sys_enable(sys_device_i2c_slave);

	gpio_function(46, pad_i2c1_scl); /* I2C1_SCL */
	gpio_function(47, pad_i2c1_sda); /* I2C1_SDA */
    gpio_pull(46, pad_pull_none);
    gpio_pull(47, pad_pull_none);

	/* Initialise the I2C Slave hardware... */
	i2cs_init(0x38);
	/* Set up the handler for i2cs_dev... */
	i2cs_dev_buffer = i2cs_dev_registers;
	i2cs_dev_buffer_size = 3;
	i2cs_enable_interrupt(MASK_I2CS_FIFO_INT_ENABLE_I2C_INT);
	interrupt_attach(interrupt_i2cs, (uint8_t)interrupt_i2cs, i2cs_dev_ISR);

	// Set up GPIOs for RGB LEDs
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

	// Initialise the camera hardware.
	module = camera_init();
	if (module > 0)
	{
		interrupt_enable_globally();

		BRIDGE_DEBUG_PRINTF("UVC supports:\r\n");
		for (int i = 0; i < sizeof(streams)/sizeof(struct stream_properties); i++)
		{
			if (camera_supports(streams[i].width, streams[i].height,
					streams[i].rate, streams[i].format) == 0)
			{
				int8_t supports = 1;
				char *fmt = "None";

				fmt = "UNCOMPRESSED";
				// Check USB bandwidth constraints on
				supports = usb_uvc_bandwidth_ok(streams[i].width, streams[i].height,
						streams[i].rate);

				if (supports)
				{
					camera_mode_add(streams[i].width, streams[i].height,
							streams[i].rate, streams[i].format);
					streams[i].supported = 1;

					BRIDGE_DEBUG_PRINTF("%s: %dx%d at %dfps %s\r\n", streams[i].stream_name,
							streams[i].width, streams[i].height, streams[i].rate, fmt);
				}
			}
		}

		usbd_testing();

		interrupt_disable_globally();
	}
	else
	{
		tfp_printf("Camera not found\n");
	}

	// Wait forever...
	for (;;);

	return 0;
}
