#include "po6030_camera.h"
#include <ft900.h>
#include "tinyprintf.h"
#include "camera.h"

#define PO6030_I2C_ADDR (0xDC) // 7-bit address 0x6E left-shifted by 1 to work with this I2C library

#define BANK_REGISTER 0x3
#define BANK_A 0x0
#define BANK_B 0x1
#define BANK_C 0x2
#define BANK_D 0x3

static inline void i2c_write_byte(uint8_t address, uint8_t reg, uint8_t command)
{
	i2cm_write(address, reg, &command, 1);
}

static inline uint8_t i2c_read_byte(uint8_t address, uint8_t reg)
{
	uint8_t value;
	i2cm_read(address, reg, &value, 1);
	return value;
}

uint16_t po6030_init(void)
{
	// Wait a short period of time to allow camera to initialise
	delayms(10);

	tfp_printf("Camera sensor must be initialised separately over I2C from Raspberry Pi!\r\n");
	return(0x6030);

	////////////////////
	// Read device ID //
	////////////////////

	tfp_printf("Reading camera ID...\r\n");

	i2c_write_byte(PO6030_I2C_ADDR, BANK_REGISTER, BANK_B);

	uint8_t device_id_h = i2c_read_byte(PO6030_I2C_ADDR, 0x00);
	uint8_t device_id_l = i2c_read_byte(PO6030_I2C_ADDR, 0x01);

	tfp_printf("Device ID: 0x%02x%02x\r\n", device_id_h, device_id_l);

    //////////////////////////////////////////////////////////////////////////////////////////////
    // Set image output size to VGA mode (640x480) (see table on page 60 of PO6030K data sheet) //
    //////////////////////////////////////////////////////////////////////////////////////////////

    int x1 = 7;
    int y1 = 7;

    int x2 = 646;
    int y2 = 486;

    i2c_write_byte(PO6030_I2C_ADDR, BANK_REGISTER, BANK_B); // Switch to Bank B
    i2c_write_byte(PO6030_I2C_ADDR, 0x50, (x1 >> 8) & 0xFF); // Window_X1_H
    i2c_write_byte(PO6030_I2C_ADDR, 0x51, x1 & 0xFF); // Window_X1_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x52, (y1 >> 8) & 0xFF); // Window_Y1_H
    i2c_write_byte(PO6030_I2C_ADDR, 0x53, y1 & 0xFF); // Window_Y1_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x54, (x2 >> 8) & 0xFF); // Window_X2_H
    i2c_write_byte(PO6030_I2C_ADDR, 0x55, x2 & 0xFF); // Window_X2_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x56, (y2 >> 8) & 0xFF); // Window_Y2_H
    i2c_write_byte(PO6030_I2C_ADDR, 0x57, y2 & 0xFF); // Window_Y2_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x61, 0x0C); // VsyncStartRow_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x63, 0xEC); // VsyncStopRow_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x80, 0x20); // Scale_X
    i2c_write_byte(PO6030_I2C_ADDR, 0x81, 0x20); // Scale_Y
    i2c_write_byte(PO6030_I2C_ADDR, 0x82, 0x01); // Reserved
    i2c_write_byte(PO6030_I2C_ADDR, 0x68, 0x00); // SyncControl0

    i2c_write_byte(PO6030_I2C_ADDR, BANK_REGISTER, BANK_C); // Switch to Bank C
    i2c_write_byte(PO6030_I2C_ADDR, 0x11, 0x25); // AEWin_X_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x13, 0x1C); // AEWin_Y_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x14, 0x02); // AEWinWidth_H
    i2c_write_byte(PO6030_I2C_ADDR, 0x15, 0x60); // AEWinWidth_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x16, 0x01); // AEWinHeight_H
    i2c_write_byte(PO6030_I2C_ADDR, 0x17, 0xBE); // AEWinHeight_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x19, 0xE5); // AECenterWin_X_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x1B, 0x87); // AECenterWin_Y_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x1D, 0xA0); // AECenterWidth_L
    i2c_write_byte(PO6030_I2C_ADDR, 0x1F, 0xA0); // AECenterHeight_L

    //////////////////////////////////////////////////////////////////////////////
    // Set output format to YCbYCr (see table on page 67 of PO6030K data sheet) //
    //////////////////////////////////////////////////////////////////////////////

    i2c_write_byte(PO6030_I2C_ADDR, BANK_REGISTER, BANK_B); // Switch to Bank B
    i2c_write_byte(PO6030_I2C_ADDR, 0x38, 0x02); // Format = Y Cb Y Cr
    i2c_write_byte(PO6030_I2C_ADDR, 0x90, 0xE0); // CS max = YCbCr range
    i2c_write_byte(PO6030_I2C_ADDR, 0x91, 0x37); // Y contrast
    i2c_write_byte(PO6030_I2C_ADDR, 0x92, 0x10); // Y brightness
    i2c_write_byte(PO6030_I2C_ADDR, 0x93, 0xEB); // Y max = YCbCr range

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Rotate the image by 180 degrees, by flipping both horizontally and vertically (see page 78 of PO6030K data sheet) //
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    i2c_write_byte(PO6030_I2C_ADDR, BANK_REGISTER, BANK_B); // Switch to Bank B
    i2c_write_byte(PO6030_I2C_ADDR, 0x68, 0x60); // SyncControl0 - enable Hsync and Vsync drop

    i2c_write_byte(PO6030_I2C_ADDR, BANK_REGISTER, BANK_A); // Switch to Bank A
    i2c_write_byte(PO6030_I2C_ADDR, 0x90, 0xF5); // BayerControl01 - enable horizontal and vertical mirror

    delayms(35); // Must wait 1 (preview) frame time

    i2c_write_byte(PO6030_I2C_ADDR, BANK_REGISTER, BANK_B); // Switch to Bank B
    i2c_write_byte(PO6030_I2C_ADDR, 0x68, 0x00); // SyncControl0 - disable Hsync and Vsync drop

    ////////////////////////////////////////////////////////////////
    // Set frame size to 960x512 to slow the frame rate to 15 fps //
    ////////////////////////////////////////////////////////////////

    int frame_width = 959;
	int frame_height = 511;

	i2c_write_byte(PO6030_I2C_ADDR, BANK_REGISTER, BANK_B); // Switch to Bank B

	i2c_write_byte(PO6030_I2C_ADDR, 0x48, (frame_width >> 8) & 0xFF); // Frame width, high
	i2c_write_byte(PO6030_I2C_ADDR, 0x49, frame_width & 0xFF); // Frame width, low

	// These registers are mentioned on page 7 of data sheet, but page 26 suggests they have something to do with flicker?
	i2c_write_byte(PO6030_I2C_ADDR, 0x29, (frame_height >> 8) & 0xFF); // Frame height, high
	i2c_write_byte(PO6030_I2C_ADDR, 0x2A, frame_height & 0xFF); // Frame height, low

	if(device_id_h == MSB(PO6030_PID) && device_id_l == LSB(PO6030_PID))
	{
		return ((device_id_h << 8) | device_id_l);
	}

	return 0;
}

void po6030_start(void)
{

}

void po6030_stop(void)
{

}

void po6030_set(int resolution, int frame_rate, int format)
{
	if(format == CAMERA_FORMAT_UNCOMPRESSED)
		tfp_printf("Format: Uncompressed\r\n");

	switch(resolution)
	{
	case CAMERA_MODE_QVGA:
		tfp_printf("Resolution: QVGA\r\n");
		break;
	case CAMERA_MODE_VGA:
		tfp_printf("Resolution: VGA\r\n");
		break;
	}

	tfp_printf("Framerate: %d\r\n", frame_rate);
}
