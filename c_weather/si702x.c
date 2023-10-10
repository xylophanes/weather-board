#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include "si702x.h"


/*------------------*/
/* Global variables */
/*------------------*/

int si702xFd;


/*-----------*/
/* Functions */
/*-----------*/

int si702x_begin(const char *device)
{
	int status = 0;

	si702xFd = open(device, O_RDWR);
	if (si702xFd < 0) {
		(void)fprintf(stderr,"    si702x ERROR: open failed\n");
		(void)fflush(stderr);

		return(-1);
	}

	status = ioctl(si702xFd, I2C_SLAVE, ID_SI7020);
	if (status < 0) {
		(void)fprintf(stderr,"    si702x ERROR: ioctl error\n");
		(void)fflush(stderr);

		(void)close(si702xFd);
		return(-1);
	}
}


float Si702x_readTemperature(void)
{
	float temp;
	unsigned int rawTemp;

	Si702x_I2C_write8(CMD_MEASURE_TEMPERATURE_HOLD);

	rawTemp = Si702x_I2C_read16(CMD_MEASURE_TEMPERATURE_HOLD);
	temp = (rawTemp*175.72/65536) - 46.85;

	return (temp);
}


float Si702x_readHumidity(void)
{
	float humi;
	unsigned int rawHumi;

	Si702x_I2C_write8(CMD_MEASURE_HUMIDITY_HOLD);
	(void)usleep(10000);

	rawHumi = Si702x_I2C_read16(CMD_MEASURE_HUMIDITY_HOLD);
	(void)usleep(10000);

	humi = (rawHumi*125.0/65536) - 6;

	return (humi);
}


unsigned short Si702x_I2C_read16(unsigned char reg)
{
	unsigned char rbuf[2] = "";

	(void)write(si702xFd, &reg, 1);
	(void)read(si702xFd, rbuf, 2);

	return (unsigned short)(rbuf[0] << 8 | rbuf[1]);
}


void Si702x_I2C_write8(unsigned char val)
{
	unsigned char wbuf[1] = "";

	wbuf[1] = val;
	(void)write(si702xFd, wbuf, 1);
}
