CC = gcc
CFLAGS = -g `pkg-config --cflags --libs libwiringpi2`
OBJGROUP = si1132.o bme280-i2c.o bme280.o weather_board.o

EXTRA_LIBS = -lwiringPi -lwiringPiDev -lm -lpthread -lrt -lcrypt

all: weather_board

weather_board: $(OBJGROUP)
	$(CC) -o weather_board $(OBJGROUP) $(EXTRA_LIBS)

clean:
	rm *.o weather_board
