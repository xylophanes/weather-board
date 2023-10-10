#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "bme280-i2c.h"


/*------------------*/
/* Global variables */
/*------------------*/

int    bme280Fd;
struct bme280_t bme280;


/*-----------*/
/* Functions */
/*-----------*/

s32 bme280_begin(const char *device)
{
	int status   = 0;
	s32 com_rslt = 0;

	bme280Fd = open(device, O_RDWR);
	if (bme280Fd < 0) {
		(void)fprintf(stderr,"    bme280 ERROR: open failed\n");
		(void)fflush(stderr);

		return(-1);
	}

	status = ioctl(bme280Fd, I2C_SLAVE, BME280_I2C_ADDRESS1);
	if (status < 0) {
		(void)fprintf(stderr,"    bme280 ERROR: ioctl error\n");
		(void)fflush(stderr);

		(void)close(bme280Fd);
		return (-1);
	}

	I2C_routine();
	if (bme280_init(&bme280) < 0) {
		return(-1);
	}

	com_rslt += bme280_set_power_mode(BME280_NORMAL_MODE);
	com_rslt += bme280_set_oversamp_humidity(BME280_OVERSAMP_2X);
	com_rslt += bme280_set_oversamp_pressure(BME280_OVERSAMP_2X);
	com_rslt += bme280_set_oversamp_temperature(BME280_OVERSAMP_2X);

	(void)usleep(100000);
	return(com_rslt);
}


float bme280_readAltitude(int pressure, float seaLevel)
{
	float atmospheric = (float)pressure/100.0;
	return(44330.0 * (1.0 - pow(atmospheric/seaLevel, 0.1903)));
}


s8 BME280_I2C_bus_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 iError   = BME280_INIT_VALUE;
	u8 stringpos = BME280_INIT_VALUE;

	unsigned char wbuf[2] = "";

	for (stringpos = BME280_INIT_VALUE; stringpos < cnt; stringpos++) {
		wbuf[1] = *(reg_data + stringpos);
		wbuf[0] = reg_addr + stringpos;
		(void)write(bme280Fd, wbuf, 2);
	}

	return ((s8)iError);
}


s8 BME280_I2C_bus_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 iError   = BME280_INIT_VALUE;
	u8 stringpos = BME280_INIT_VALUE;

	unsigned char rbuf[2] = "";
	int i;

	for (stringpos = BME280_INIT_VALUE; stringpos < cnt; stringpos++) {
		rbuf[0] = reg_addr + stringpos;
		(void)write(bme280Fd, rbuf, 1);
		(void)read(bme280Fd, rbuf, 1);

		if (rbuf[0] < 0)
		   iError = -1;
		else
	           *(reg_data + stringpos) = rbuf[0];
	}
	return ((s8)iError);
}


void BME280_delay_msek(u16 msek)
{
	usleep(msek*1000);
}


s8 I2C_routine(void) {
	bme280.bus_write = BME280_I2C_bus_write;
	bme280.bus_read = BME280_I2C_bus_read;
	bme280.dev_addr = BME280_I2C_ADDRESS1;
	bme280.delay_msec = BME280_delay_msek;

	return (BME280_INIT_VALUE);
}
