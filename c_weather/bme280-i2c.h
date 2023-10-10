#ifndef __BME280_I2C_H__
#define __BME280_I2C_H__
#include "bme280.h"


/*---------------------*/
/* Function prototypes */
/*---------------------*/

s32 bme280_begin          (const char *device);
float bme280_readAltitude (int pressure, float seaLevel);

s8 I2C_routine            (void);

s8 BME280_I2C_bus_write   (u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt);
s8 BME280_I2C_bus_read    (u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt);

void BME280_delay_msek    (u16 msek);

#endif //__BME280_I2C_H__
