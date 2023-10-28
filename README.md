## This is a forked project for Odroid WEATHER-BOARD (versions 1 and 2)

You can find more details at the the WEATHER BOARD wiki:

https://wiki.odroid.com/accessory/sensor/weather-board/weather-board

## c_weather

Functionality has been added to:

1. stream weather sensor data to (rotatable) log file. File is rotated at a user specified time
with 24 hour periud (at hh:mm:ss), or after an arbitrary user specified perior (hh:mm:ss).
unless a rotation is forced via SIGUSR1.

2. stream weather sensir data to standard output.

3. send weather sensor data to FIFO (weatherpipe) on reciept of SIGUSR2 so other processes
   can slave weather_board and use it to probe the weatherboard sensors.

## Weather sensor data format


    <datetime>    uvi: <float>   vis: <float>lux    ir: <float>lux   temp: <float>C   humidity: <float>%%    dew point <float>C    pressure: <float>hpa

Where:

* uvi is UV index.
* vis is visible light flux.
* ir is infrared light flux.
* temp is temperature (degrees Celsius).
* humidity is percent relative humidity.
* dew point is (wet bulb) temperature depression (degrees Celsius).
* pressure is atmospheric pressure in Hectopascals.

## Usage

    Weather Board (version 3.00)
    M.A. O'Neill, Tumbling Dice, 2016-2023

    Usage : [sudo] weather_board [-usage | -help]
            |
            [-uperiod <update period in secs:60>]
            [-ttymode:FALSE] | [-logfile <log file name> [-rollover <hh:mm:ss:00:00:00> | -rperiod <hh:mm:ss>]]
            [i2c node:/dev/i2c-1]
            [ >& <error/status log>]

            Signals
            =======

            SIGUSR1  (10): force file rollover
            SIGINT   (02):
            SIGQUIT  (03):
            SIGHUP   (01):
            SIGERM   (15): exit gracefully

            SIGUSR2  (12): write latest data to weatherpipe

Note that although the (now obseleted) boad is manufactured by HardKernel, it communicates via
I2C bit banging and so it will also work with Raspberry 3/3B/4/5 and probably other ARM-based SBC's.

