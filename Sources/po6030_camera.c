#include <stdint.h>

#include <ft900.h>
#include "tinyprintf.h"

#define PO6030_I2C_ADDR (0x6E)

#define BANK_REGISTER 0x3
#define BANK_A 0x0
#define BANK_B 0x1
#define BANK_C 0x2
#define BANK_D 0x3

uint16_t po6030_init(void)
{
	// Wait a short period of time to allow camera to initialise
	delayms(10);

    // Initialise the camera
	uint8_t bank = BANK_B;
	i2cm_write(PO6030_I2C_ADDR, BANK_REGISTER, &bank, 1);

	uint8_t device_id_h;
	uint8_t device_id_l;
	i2cm_read(PO6030_I2C_ADDR, 0x00, &device_id_h, 1);
	i2cm_read(PO6030_I2C_ADDR, 0x01, &device_id_l, 1);

	tfp_printf("Device ID: 0x%02x%02x\n", device_id_h, device_id_l);

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

}
