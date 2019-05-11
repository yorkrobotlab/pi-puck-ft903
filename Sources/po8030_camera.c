#include "po8030_camera.h"
#include <ft900.h>
#include "tinyprintf.h"
#include "camera.h"

#define PO8030_I2C_ADDR (0xDC) // 7-bit address 0x6E left-shifted by 1 to work with this I2C library

#define BANK_REGISTER 0x3
#define BANK_A 0x0
#define BANK_B 0x1
#define BANK_C 0x2
#define BANK_D 0x3

// Bank A registers
#define PO8030_REG_FRAMEWIDTH_H 0x04
#define PO8030_REG_FRAMEWIDTH_L 0x05
#define PO8030_REG_FRAMEHEIGHT_H 0x06
#define PO8030_REG_FRAMEHEIGHT_L 0x07
#define PO8030_REG_WINDOWX1_H 0x08
#define PO8030_REG_WINDOWX1_L 0x09
#define PO8030_REG_WINDOWY1_H 0x0A
#define PO8030_REG_WINDOWY1_L 0x0B
#define PO8030_REG_WINDOWX2_H 0x0C
#define PO8030_REG_WINDOWX2_L 0x0D
#define PO8030_REG_WINDOWY2_H 0x0E
#define PO8030_REG_WINDOWY2_L 0x0F
#define PO8030_REG_VSYNCSTARTROW_H 0x10
#define PO8030_REG_VSYNCSTARTROW_L 0x11
#define PO8030_REG_VSYNCSTOPROW_H 0x12
#define PO8030_REG_VSYNCSTOPROW_L 0x13
#define PO8030_REG_INTTIME_H 0x17
#define PO8030_REG_INTTIME_M 0x18
#define PO8030_REG_INTTIME_L 0x19
#define PO8030_REG_WB_RGAIN 0x23
#define PO8030_REG_WB_GGAIN 0x24
#define PO8030_REG_WB_BGAIN 0x25
#define PO8030_REG_AUTO_FWX1_H 0x35
#define PO8030_REG_AUTO_FWX1_L 0x36
#define PO8030_REG_AUTO_FWX2_H 0x37
#define PO8030_REG_AUTO_FWX2_L 0x38
#define PO8030_REG_AUTO_FWY1_H 0x39
#define PO8030_REG_AUTO_FWY1_L 0x3A
#define PO8030_REG_AUTO_FWY2_H 0x3B
#define PO8030_REG_AUTO_FWY2_L 0x3C
#define PO8030_REG_AUTO_CWX1_H 0x3D
#define PO8030_REG_AUTO_CWX1_L 0x3E
#define PO8030_REG_AUTO_CWX2_H 0x3F
#define PO8030_REG_AUTO_CWX2_L 0x40
#define PO8030_REG_AUTO_CWY1_H 0x41
#define PO8030_REG_AUTO_CWY1_L 0x42
#define PO8030_REG_AUTO_CWY2_H 0x43
#define PO8030_REG_AUTO_CWY2_L 0x44
#define PO8030_REG_PAD_CONTROL 0x5B
#define PO8030_REG_SOFTRESET 0x69
#define PO8030_REG_CLKDIV 0x6A
#define PO8030_REG_BAYER_CONTROL_01 0x6C // Vertical/horizontal mirror.
// Bank B registers
#define PO8030_REG_ISP_FUNC_2 0x06 // Embossing, sketch, proximity.
#define PO8030_REG_FORMAT 0x4E
#define PO8030_REG_SKETCH_OFFSET 0x8F
#define PO8030_REG_SCALE_X 0x93
#define PO8030_REG_SCALE_Y 0x94
#define PO8030_REG_SCALE_TH_H 0x95
#define PO8030_REG_SCALE_TH_L 0x96
#define PO8030_REG_CONTRAST 0x9D
#define PO8030_REG_BRIGHTNESS 0x9E
#define PO8030_REG_SYNC_CONTROL0 0xB7
// Bank C registers
#define PO8030_REG_AUTO_CONTROL_1 0x04 // AutoWhiteBalance, AutoExposure.
#define PO8030_REG_EXPOSURE_T 0x12
#define PO8030_REG_EXPOSURE_H 0x13
#define PO8030_REG_EXPOSURE_M 0x14
#define PO8030_REG_EXPOSURE_L 0x15
#define PO8030_REG_SATURATION 0x2C

// Formats
#define FORMAT_CBYCRY 0x00
#define FORMAT_CRYCBY 0x01
#define FORMAT_YCBYCR 0x02
#define FORMAT_YCRYCB 0x03
#define FORMAT_RGGB 0x10
#define FORMAT_GBRG 0x11
#define FORMAT_GRBG 0x12
#define FORMAT_BGGR 0x13
#define FORMAT_RGB565 0x30
#define FORMAT_RGB565_BYTE_SWAP 0x31
#define FORMAT_BGR565 0x32
#define FORMAT_BGR565_BYTE_SWAP 0x33
#define FORMAT_RGB444 0x36
#define FORMAT_RGB444_BYTE_SWAP 0x37
#define FORMAT_DPC_BAYER 0x41
#define FORMAT_YYYY 0x44

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

uint16_t po8030_init(void)
{
	// Wait a short period of time to allow camera to initialise
	delayms(10);

	////////////////////
	// Read device ID //
	////////////////////

	tfp_printf("Reading camera ID...\r\n");

//	i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_B);
	i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_A);

	uint8_t device_id_h = i2c_read_byte(PO8030_I2C_ADDR, 0x00);
	uint8_t device_id_l = i2c_read_byte(PO8030_I2C_ADDR, 0x01);

	tfp_printf("Device ID: 0x%02x%02x\r\n", device_id_h, device_id_l);

	//////////////

    // Window settings.
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_WINDOWX1_H, 0x00);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_WINDOWX1_L, 0x01);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_WINDOWY1_H, 0x00);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_WINDOWY1_L, 0x01);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_WINDOWX2_H, 0x02);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_WINDOWX2_L, 0x80);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_WINDOWY2_H, 0x01);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_WINDOWY2_L, 0xE0);

    // AE full window selection.
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_FWX1_H, 0x00);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_FWX1_L, 0x01);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_FWX2_H, 0x02);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_FWX2_L, 0x80);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_FWY1_H, 0x00);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_FWY1_L, 0x01);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_FWY2_H, 0x01);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_FWY2_L, 0xE0);

    // AE center window selection.
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_CWX1_H, 0x00);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_CWX1_L, 0xD6);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_CWX2_H, 0x01);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_CWX2_L, 0xAB);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_CWY1_H, 0x00);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_CWY1_L, 0xA1);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_CWY2_H, 0x01);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_AUTO_CWY2_L, 0x40);

	i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_B);

    // Scale settings.
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_SCALE_X, 0x20);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_SCALE_Y, 0x20);

	// Format
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_FORMAT, FORMAT_YCBYCR);

	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_SYNC_CONTROL0, 0x00);

	i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_A);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_VSYNCSTARTROW_L, 0x0A);

	// Seems to cause image tearing?
	i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_B);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_SCALE_TH_H, 0x00);
	i2c_write_byte(PO8030_I2C_ADDR, PO8030_REG_SCALE_TH_L, 0x0A);

    //////////////////////////////////////////////////////////////////////////////////////////////
    // Set image output size to VGA mode (640x480) (see table on page 60 of PO8030K data sheet) //
    //////////////////////////////////////////////////////////////////////////////////////////////

//    int x1 = 7;
//    int y1 = 7;
//
//    int x2 = 646;
//    int y2 = 486;
//
//    i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_B); // Switch to Bank B
//    i2c_write_byte(PO8030_I2C_ADDR, 0x08, (x1 >> 8) & 0xFF); // Window_X1_H
//    i2c_write_byte(PO8030_I2C_ADDR, 0x09, x1 & 0xFF); // Window_X1_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x0A, (y1 >> 8) & 0xFF); // Window_Y1_H
//    i2c_write_byte(PO8030_I2C_ADDR, 0x0B, y1 & 0xFF); // Window_Y1_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x0C, (x2 >> 8) & 0xFF); // Window_X2_H
//    i2c_write_byte(PO8030_I2C_ADDR, 0x0D, x2 & 0xFF); // Window_X2_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x0E, (y2 >> 8) & 0xFF); // Window_Y2_H
//    i2c_write_byte(PO8030_I2C_ADDR, 0x0F, y2 & 0xFF); // Window_Y2_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x11, 0x0C); // VsyncStartRow_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x13, 0xEC); // VsyncStopRow_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x93, 0x20); // Scale_X
//    i2c_write_byte(PO8030_I2C_ADDR, 0x94, 0x20); // Scale_Y
////    i2c_write_byte(PO8030_I2C_ADDR, 0x82, 0x01); // Reserved
////    i2c_write_byte(PO8030_I2C_ADDR, 0x68, 0x00); // SyncControl0
//
//    i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_C); // Switch to Bank C
//    i2c_write_byte(PO8030_I2C_ADDR, 0x11, 0x25); // AEWin_X_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x13, 0x1C); // AEWin_Y_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x14, 0x02); // AEWinWidth_H
//    i2c_write_byte(PO8030_I2C_ADDR, 0x15, 0x60); // AEWinWidth_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x16, 0x01); // AEWinHeight_H
//    i2c_write_byte(PO8030_I2C_ADDR, 0x17, 0xBE); // AEWinHeight_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x19, 0xE5); // AECenterWin_X_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x1B, 0x87); // AECenterWin_Y_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x1D, 0xA0); // AECenterWidth_L
//    i2c_write_byte(PO8030_I2C_ADDR, 0x1F, 0xA0); // AECenterHeight_L

    //////////////////////////////////////////////////////////////////////////////
    // Set output format to YCbYCr (see table on page 67 of PO8030K data sheet) //
    //////////////////////////////////////////////////////////////////////////////

//    i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_B); // Switch to Bank B
//    i2c_write_byte(PO8030_I2C_ADDR, 0x38, 0x02); // Format = Y Cb Y Cr
//    i2c_write_byte(PO8030_I2C_ADDR, 0x90, 0xE0); // CS max = YCbCr range
//    i2c_write_byte(PO8030_I2C_ADDR, 0x91, 0x37); // Y contrast
//    i2c_write_byte(PO8030_I2C_ADDR, 0x92, 0x10); // Y brightness
//    i2c_write_byte(PO8030_I2C_ADDR, 0x93, 0xEB); // Y max = YCbCr range

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Rotate the image by 180 degrees, by flipping both horizontally and vertically (see page 78 of PO8030K data sheet) //
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//    i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_B); // Switch to Bank B
//    i2c_write_byte(PO8030_I2C_ADDR, 0x68, 0x60); // SyncControl0 - enable Hsync and Vsync drop
//
//    i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_A); // Switch to Bank A
//    i2c_write_byte(PO8030_I2C_ADDR, 0x90, 0xF5); // BayerControl01 - enable horizontal and vertical mirror
//
//    delayms(35); // Must wait 1 (preview) frame time
//
//    i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_B); // Switch to Bank B
//    i2c_write_byte(PO8030_I2C_ADDR, 0x68, 0x00); // SyncControl0 - disable Hsync and Vsync drop

    ////////////////////////////////////////////////////////////////
    // Set frame size to 960x512 to slow the frame rate to 15 fps //
    ////////////////////////////////////////////////////////////////

//    int frame_width = 959;
//	int frame_height = 511;
//
//	i2c_write_byte(PO8030_I2C_ADDR, BANK_REGISTER, BANK_B); // Switch to Bank B
//
//	i2c_write_byte(PO8030_I2C_ADDR, 0x48, (frame_width >> 8) & 0xFF); // Frame width, high
//	i2c_write_byte(PO8030_I2C_ADDR, 0x49, frame_width & 0xFF); // Frame width, low
//
//	// These registers are mentioned on page 7 of data sheet, but page 26 suggests they have something to do with flicker?
//	i2c_write_byte(PO8030_I2C_ADDR, 0x29, (frame_height >> 8) & 0xFF); // Frame height, high
//	i2c_write_byte(PO8030_I2C_ADDR, 0x2A, frame_height & 0xFF); // Frame height, low

	if(device_id_h == MSB(PO8030_PID) && device_id_l == LSB(PO8030_PID))
	{
		return ((device_id_h << 8) | device_id_l);
	}

	return 0;
}

void po8030_start(void)
{

}

void po8030_stop(void)
{

}

void po8030_set(int resolution, int frame_rate, int format)
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
