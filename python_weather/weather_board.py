#!/usr/bin/python
import SI1132
import BME280
import sys
import time
import os

if len(sys.argv) != 2:
    print("Usage: weather_board.py <i2c device file>")
    sys.exit()

si1132 = SI1132.SI1132(sys.argv[1])
bme280 = BME280.BME280(sys.argv[1], 0x03, 0x02, 0x02, 0x02)

def get_altitude(pressure, seaLevel):
    atmospheric = pressure / 100.0
    return 44330.0 * (1.0 - pow(atmospheric/seaLevel, 0.1903))

while True:
    os.system('clear')
    print("======== si1132 ========")
    print("UV_index : %.2f" % (si1132.readUV() / 100.0))
    print("Visible : %d Lux" % int(si1132.readVisible()))
    print("IR : %d Lux" % int(si1132.readIR()))
    print("======== bme280 ========")
    print("temperature : %.2f 'C" % bme280.read_temperature())
    print("humidity : %.2f %%" % bme280.read_humidity())
    p = bme280.read_pressure()
    print("pressure : %.2f hPa" % (p / 100.0))
    print("altitude : %.2f m" % get_altitude(p, 1024.25))
    time.sleep(1)
