/*---------*/
/* Defines */
/*---------*/

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


/*--------------------*/
/* Imported variables */
/*--------------------*/

extern int bmp180Fd;

extern short ac1,
             ac2,
	     ac3,
	     b1,
	     b2,
	     mb,
	     mc,
	     md;

extern unsigned short ac4,
                      ac5,
		      ac6;

extern unsigned char oversampling;


/*--------------------*/
/* Imported functions */
/*--------------------*/

extern int            bmp180_begin(const char *device);
extern void           BMP180_I2C_writeCommand(unsigned char reg, unsigned char value);
extern unsigned char  BMP180_I2C_read8(unsigned char reg);
extern unsigned short BMP180_I2C_read16(unsigned char reg);
extern short          BMP180_I2C_reads6(unsigned char reg);

extern void           readCoefficients(void);
extern float          readRawTemperature();
extern float          readRawPressure();

extern int            computeB5(int ut);

extern float          BMP180_readPressure(void);
extern float          BMP180_readTemperature(void);

extern float          BMP180_readSealevelPressure(float altitude_meters);
extern float          BMP180_readAltitude(float sealevelPressure);
