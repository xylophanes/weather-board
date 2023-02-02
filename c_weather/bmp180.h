#ifndef __BMP180_H__
#define __BMP180_H__
#define BMP180_ADDRESS		0x77

#define BMP180_ULTRALOWPOWER	0
#define BMP180_STANDARD		1
#define BMP180_HIGHRES		2
#define BMP180_ULTRAHIGHRES	3

#define BMP180_CAL_AC1		0xAA 
#define BMP180_CAL_AC2		0xAC
#define BMP180_CAL_AC3		0xAE
#define BMP180_CAL_AC4		0xB0
#define BMP180_CAL_AC5		0xB2
#define BMP180_CAL_AC6		0xB4
#define BMP180_CAL_B1		0xB6
#define BMP180_CAL_B2		0xB8
#define BMP180_CAL_MB		0xBA
#define BMP180_CAL_MC		0xBC
#define BMP180_CAL_MD		0xBE

#define BMP180_CHIPID		0xD0
#define BMP180_VERSION		0xD1
#define BMP180_SOFTRESET	0xE0
#define BMP180_CONTROL		0xF4
#define BMP180_TEMPDATA		0xF6
#define BMP180_PRESSUREDATA	0xF6
#define BMP180_READTEMPCMD	0x2E
#define BMP180_READPRESSURECMD	0x34

int bmp180_begin(const char *device);
void BMP180_I2C_writeCommand(unsigned char reg, unsigned char value);
unsigned char BMP180_I2C_read8(unsigned char reg);
unsigned short BMP180_I2C_read16(unsigned char reg);
short BMP180_I2C_reads6(unsigned char reg);

void readCoefficients(void);
float readRawTemperature();
float readRawPressure();

int computeB5(int ut);

float BMP180_readPressure(void);
float BMP180_readTemperature(void);

float BMP180_readSealevelPressure(float altitude_meters);
float BMP180_readAltitude(float sealevelPressure);
#endif //__BMP180_H__
