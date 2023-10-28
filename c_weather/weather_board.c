/*---------------------------------------------
 * Weatherboard main program 
 * Modified by M.A. O'Neill, Tumbling Dice 2023
 *-------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "bme280-i2c.h"
#include "si1132.h"
#include "si702x.h"
#include "bmp180.h"


/*-------------------*/
/* Local definitions */
/*-------------------*/
/*----------------------------*/
/* Weatheboard version number */
/*----------------------------*/

#define WEATHERBOARD_VERSION "3.00"


/*-------------------------------*/
/* Default sensor polling period */
/*-------------------------------*/

#define DEFAULT_UPDATE_PERIOD  60


/*----------------*/
/* Boolean values */
/*----------------*/

#define FALSE                  0
#define TRUE                   255
#define _BOOLEAN               int
#define _PRIVATE               static
#define _PUBLIC


/*-------------*/
/* String size */
/*-------------*/

#define SSIZE                  256


/*------------------*/
/* Global variables */
/*------------------*/

_PUBLIC   _BOOLEAN do_verbose                         = FALSE;


/*-----------------*/
/* Local variables */
/*-----------------*/

_PRIVATE int               i2c_address                = 0x76;
_PRIVATE float             uv_index;
_PRIVATE float             vis;
_PRIVATE float             ir;
_PRIVATE float             pressure;
_PRIVATE float             temperature;
_PRIVATE float             humidity;
_PRIVATE float             dew_point;
_PRIVATE float             altitude;
_PRIVATE unsigned char     logfile_name[SSIZE]        = "";
_PRIVATE unsigned char     rollover_timeStr[SSIZE]    = "";
_PRIVATE unsigned char     rollover_periodStr[SSIZE]  = "";
_PRIVATE  _BOOLEAN          do_rollover               = FALSE;
_PRIVATE  _BOOLEAN          do_rollover_enabled       = FALSE;
_PRIVATE time_t            rperiod                    = (-1);
_PRIVATE time_t            nowsecs                    = (-1);
_PRIVATE time_t            rollsecs                   = (-1);


/*--------------------------*/
/* For altitude calculation */
/*--------------------------*/

_PRIVATE float              SEALEVELPRESSURE_HPA      = 1024.25;




/*------------------*/
/* Clear the screen */
/*------------------*/

_PRIVATE void clearScreen(void)
{
  const char* CLEAR_SCREE_ANSI = "\e[1;1H\e[2J";
  (void)write(STDOUT_FILENO,CLEAR_SCREE_ANSI,12);
}




/*----------------*/
/* Handle signals */
/*----------------*/

_PRIVATE int signal_handler(unsigned int signum)

{	if (signum == SIGUSR1) {
	   if (do_rollover_enabled == TRUE)
		do_rollover = TRUE;
        }


	/*------*/
	/* Exit */
	/*------*/

	else if (signum == SIGABRT  ||
	         signum == SIGQUIT  ||
	         signum == SIGINT   ||
		 signum == SIGHUP   ||
		 signum == SIGTERM   )
        {  (void)unlink("/tmp/weatherpipe");

	   if (do_verbose == TRUE)
           {  (void)fprintf(stderr,"\n    weather-board: **** aborted\n\n");
	      (void)fflush(stderr);
           }
  
	   exit (255);
	}


	/*--------------------------------*/
	/* Broken weatherpipe (no reader) */
	/*--------------------------------*/


	else if (signum == SIGPIPE) {
	   if (do_verbose == TRUE)
           {  (void)fprintf(stderr,"\n    weather-board WARNING: weatherpipe has no reader\n\n");
	      (void)fflush(stderr);
           }
	}


	/*---------------------------*/
	/* Write data to weatherpipe */
	/*---------------------------*/

	else if (signum == SIGUSR2)
	{  FILE          *pipestream         = (FILE *)NULL;
           unsigned char datetimeStr[SSIZE]  = "";


	   if ((pipestream = fopen("/tmp/weatherpipe","w")) == (FILE *)NULL) {
	      if (do_verbose == TRUE)
              {  (void)fprintf(stderr,"\n    weather-board WARNING: failed to open weatherpipe for writingr\n\n");
	         (void)fflush(stderr);
              }
	   }
	

	   else {
              (void)fprintf(pipestream,"%s  uvi: %8.2f  vis: %8.2f lux  ir: %8.2f lux  temp: %8.2f C  humidity: %8.2f %%  dew point %8.2f C  pressure: %8.2f hpa\n",
                                                                                                                                                        datetimeStr,
                                                                                                                                                           uv_index,
                                                                                                                                                                vis,
                                                                                                                                                                 ir,
                                                                                                                                                        temperature,
                                                                                                                                                           humidity,
                                                                                                                                                          dew_point,
                                                                                                                                                           pressure);
              (void)fflush(pipestream);
	      (void)fclose(pipestream);
	   }
        }

	return(0);
}




/*---------------------------------------------------*/
/*  Get current time and date in human readable form */
/*---------------------------------------------------*/

_PRIVATE void strhostdate(unsigned char *date, char *time, unsigned char *datetime)

{   time_t tval;
    double usecs;

    unsigned char f1[SSIZE]        = "",
                  f2[SSIZE]        = "",
                  f3[SSIZE]        = "",
                  f4[SSIZE]        = "",
                  strusecs[SSIZE]  = "",
	          tmpdate[SSIZE]   = "";

    struct timespec tspec;

    (void)clock_gettime(CLOCK_REALTIME,&tspec);
    tval  = tspec.tv_sec;
    usecs = (double)(tspec.tv_nsec) / 1000000.0;

    (void)strcpy(tmpdate,ctime(&tval));
    tmpdate[strlen(tmpdate) - 1] = '\0';
    (void)sscanf(tmpdate,"%s%s%s%s",f1,f2,f3,f4);


    /*--------------------*/
    /* Fill in time field */
    /*--------------------*/

    if (time != (char *)NULL)
       (void)sprintf(time,"%s",f4);


    /*-----------------------------*/
    /* Fill in abridged date field */
    /*-----------------------------*/

    if (date != (unsigned char *)NULL)
       (void)sprintf(date,"%s.%s",f3,f2);


    /*------------------------------------*/
    /* Fill in fully qualified date field */
    /*------------------------------------*/

    if (datetime != (unsigned char *)NULL) {
       (void)sprintf(strusecs,"%.2f",usecs);
       (void)sprintf(datetime,"%s.%s.%s-%s",f1,f2,f3,f4);
    }
}




/*-------------------------------------------------------*/
/* TRUE if /dev/null opened on specified file descriptor */
/*-------------------------------------------------------*/

_PRIVATE _BOOLEAN datasink(int fdes)

{   unsigned char link[SSIZE]    = "",
	          procStr[SSIZE] = "";
		  ;
    ssize_t       rval;

    (void)snprintf(procStr,SSIZE,"/proc/self/fd/%d",fdes);
    rval       = readlink(procStr,link,sizeof(link));
    link[rval] = '\0';

    if (strcmp(link, "/dev/null") == 0)
       return(TRUE);

    return(FALSE);
}


/*------------------*/
/* main entry point */
/*------------------*/

int main(int argc, char *argv[])
{
	unsigned int   i;
	unsigned int   now;
	unsigned int   rhour;
	unsigned int   rminute;
	unsigned int   rsecond;
	unsigned int   hour;
	unsigned int   minute;
	unsigned int   second;
	_BOOLEAN       tty_mode                 = FALSE;
	unsigned int   update_period            = DEFAULT_UPDATE_PERIOD;
	unsigned int   status                   = 0;
	unsigned int   WBVersion                = 2;
	unsigned int   argd                     = 1;
	unsigned char  *device                  = "/dev/i2c-1";
	unsigned char  rollover_timeStr[SSIZE]  = "";
	unsigned char  eff_logfile_name[SSIZE]  = "";
	unsigned char  dateStr[SSIZE]           = "";
	unsigned char  timeStr[SSIZE]           = "";
	unsigned char  datetimeStr[SSIZE]       = "";
	FILE           *stream                  = (FILE *)NULL;


        /*--------------------*/
        /* Parse command tail */
        /*--------------------*/

	if (argc > 1) {
		for (i=0; i<argc; ++i) {


		   /*--------------------------*/
		   /* Display help information */
		   /*--------------------------*/

		   if (strcmp(argv[1],"-help") == 0 || strcmp(argv[1],"-usage") == 0) {
		      (void)fprintf(stderr,"\n    Weather Board (version %s)\n", WEATHERBOARD_VERSION);
		      (void)fprintf(stderr,"    M.A. O'Neill, Tumbling Dice, 2016-2023\n\n");
   		      (void)fprintf(stderr,"    Usage : [sudo] weather_board [-usage | -help]\n");
       		      (void)fprintf(stderr,"            |\n");
	              (void)fprintf(stderr,"            [-uperiod <update period in secs:%d>]\n", DEFAULT_UPDATE_PERIOD);
             	      (void)fprintf(stderr,"            [-ttymode:FALSE] | [-logfile <log file name> [-rollover <hh:mm:ss:00:00:00> | -rperiod <hh:mm:ss>]]\n");
              	      (void)fprintf(stderr,"            [i2c node:/dev/i2c-1]\n");
	              (void)fprintf(stderr,"            [ >& <error/status log>]\n\n");
	              (void)fprintf(stderr,"            Signals\n");
	              (void)fprintf(stderr,"            =======\n\n");
	              (void)fprintf(stderr,"            SIGUSR1  (%02d): force file rollover\n\n", SIGUSR1);
		      (void)fprintf(stderr,"            SIGINT   (%02d):\n", SIGINT);
		      (void)fprintf(stderr,"            SIGQUIT  (%02d):\n", SIGQUIT);
		      (void)fprintf(stderr,"            SIGHUP   (%02d):\n", SIGHUP);
		      (void)fprintf(stderr,"            SIGERM   (%02d): exit gracefully\n\n", SIGTERM);
	              (void)fprintf(stderr,"            SIGUSR2  (%02d): write latest data to weatherpipe\n\n", SIGUSR2);
                      (void)fflush(stderr);

	              exit(1);
                   } 


	           /*----------------*/
	           /* Verbosity flag */
	           /*----------------*/

	           else if (strcmp(argv[i],"-verbose") == 0) {
	              do_verbose = TRUE;
	              ++argd;
	           }


		   /*-------------------------------*/
		   /* Pretty print data to terminal */
		   /*-------------------------------*/

		   else if (strcmp(argv[i+1],"ttymode") == 0)
		   {  tty_mode = TRUE;
                      ++argd;
                   }


	           /*-------------------*/
	           /* Set update period */
	           /*-------------------*/

	           else if (strcmp(argv[i],"-uperiod") == 0) {
 	              if (i == argc - 1 || argv[i + 1][0] == '-') {


			 /*-------*/
			 /* Error */
			 /*-------*/

		         if (do_verbose == TRUE) {
		            (void)fprintf(stderr,"    weatherboard ERROR: expecting update period in seconds\n");
		            (void)fflush(stderr);
		         }

		         exit(255);
                      }

                      if (sscanf(argv[i+1],"%d",&update_period) != 1 || update_period <= 0) {


			 /*-------*/
			 /* Error */
			 /*-------*/

   		         if (do_verbose == TRUE) {
		            (void)fprintf(stderr,"    weatherboard ERROR: expecting time period in seconds (integer > 0)\n");
		            (void)fflush(stderr);
		         }

		         exit(255);
	              }

	              argd += 2;
	              ++i;
                   }


      	           /*------------------*/
	           /* Set logfile name */
	           /*------------------*/

                   else if (strcmp(argv[i],"-logfile") == 0) {


		      /*-------*/
		      /* Error */
		      /*-------*/

                      if (i == argc - 1 || argv[i+1][0] == '-') {

                         if (do_verbose == TRUE) {
                            (void)fprintf(stderr,"    weatherboard ERROR: expecting logfile name\n");
                            (void)fflush(stderr);
                         }

                         exit(255);
                      }

	              (void)strcpy(logfile_name,argv[i+1]);

                      argd += 2;
                      ++i;
                   }


	           /*---------------------------*/
	           /* Set logfile rollover time */
	           /*---------------------------*/


                   else if (rperiod == (-1) && strcmp(logfile_name,"") != 0 && strcmp(logfile_name,"tty") && strcmp(argv[i],"-rollover") == 0) {


		      /*-------*/
		      /* Error */
		      /*-------*/

                      if (i == argc -1 || argv[i + 1][0] == '-') {

                         if (do_verbose == TRUE) {
                            (void)fprintf(stderr,"    weatherboard ERROR: expecting rollover time <hh:mm:ss> (24 hour clock)\n");
                            (void)fflush(stderr);
                         }

                         exit(255);
                      }

                      (void)strcpy(rollover_timeStr,argv[i+1]);
		      do_rollover_enabled = TRUE;

                      argd += 2;
                      ++i;
                   }


                   /*---------------------------*/
                   /* Set logfile rollover time */
                   /*---------------------------*/


                   else if (strcmp(rollover_timeStr,"") == 0 && strcmp(logfile_name,"") != 0 && strcmp(logfile_name,"tty") && strcmp(argv[i],"-rperiod") == 0) {


		      /*-------*/
		      /* Error */
		      /*-------*/

                      if (i == argc -1 || argv[i + 1][0] == '-') {

		         unsigned char tmpstr[SSIZE] = "";

                         if (do_verbose == TRUE) {
                            (void)fprintf(stderr,"    weatherboard ERROR: expecting rollover period <hh:mm:ss> (24 hour clock)\n");
                            (void)fflush(stderr);
                         }

                         exit(255);
                      }


		      /*--------------*/
		      /* Format error */
		      /*--------------*/

		      if (sscanf(argv[i+1],"%d:%d:%d",&hour,&minute,&second) != 3) {
		         if (do_verbose == TRUE) {
                            (void)fprintf(stderr,"    weatherboard ERROR: expecting rollover period <hh:mm:ss> (24 hour clock)\n");
                            (void)fflush(stderr);
                         }

                         exit(255);
		      }

		      (void)strcpy(rollover_periodStr,argv[i+1]);

	              rperiod             = (time_t)(hour*3600 + minute*60 + second);
		      do_rollover_enabled = TRUE;

                      argd += 2;
                      ++i;
                   }
               }
        }


	/*-------------------------*/
	/* Get i2c bus device name */
	/*-------------------------*/

	if (argc > 1 && argd == argc - 1) {

           if (access(argv[argd], R_OK | W_OK) == (01)) {

	      if (do_verbose == TRUE) {
                 (void)fprintf(stderr,"    weatherboard ERROR: expecting rollover period <hh:mm:ss> (24 hour clock)\n");
                 (void)fflush(stderr);
              }

	      exit(255);
           }

	   device = argv[argd];
	}


	/*-----------------------------*/
        /* Unparsed command line items */
	/*-----------------------------*/

	else if (argd < argc) {
	   if (do_verbose == TRUE) {
	      (void)fprintf(stderr,"    weatherboard ERROR: unparsed command line parameters (have: %d, parsed: %d)\n",argc,argd);
	      (void)fflush(stderr);
	   }

	    exit(255);
	}


        /*--------------------*/
	/* Create weatherpipe */
        /*--------------------*/

        if (access("/tmp/weatherpipe", F_OK) == (-1))
           (void)mkfifo("/tmp/weatherpipe",0666);



	/*------------------------------------------------*/
	/* Error - weather-board instance already running */
	/*------------------------------------------------*/

        else {
	   if (do_verbose == TRUE) {
	      (void)fprintf(stderr,"    weatherboard ERROR: unparsed command line parameters (have: %d, parsed: %d)\n",argc,argd);
	      (void)fflush(stderr);
	   }

	    exit(255);
        }


        /*----------------------------------------*/
        /* Start communication with weather board */
        /*----------------------------------------*/

	si1132_begin(device);


        /*--------------------*/
        /* Display parameters */
        /*--------------------*/

        if (do_verbose == TRUE) {

	   (void)fprintf(stderr,"\n    Weather Board (version %s)\n",WEATHERBOARD_VERSION);
	   (void)fprintf(stderr,"    M.A. O'Neill, Tumbling Dice, 2016-2023\n\n");

           if (strcmp(logfile_name,"tty") == 0)
              (void)fprintf(stderr,"    terminal output mode\n");
           else {
	      if (strcmp(logfile_name,"") == 0)
                 (void)fprintf(stderr,"    logfile           :  standard output\n",logfile_name);
	      else
                 (void)fprintf(stderr,"    logfile (basename):  %s\n",logfile_name);

	      if (strcmp(rollover_timeStr,"") != 0)
                 (void)fprintf(stderr,"    rollover time     :  %s\n",rollover_timeStr);
	      else if (rperiod != (-1))
		 (void)fprintf(stderr,"    rollover period   :  %s (%d seconds)\n",rollover_periodStr,rperiod);
           }

           (void)fprintf(stderr,"    update period     :  %04d seconds\n",update_period);
           (void)fprintf(stderr,"    i2c bus           :  %s (sensors at i2c addresses 0x%d and 0x%d)\n\n",device,Si1132_ADDR,BMP180_ADDRESS);
           (void)fflush(stderr);
        }

	//if (strcmp(logfile_name,"tty") == 0)
	//   (void)sleep(5);

	if (bme280_begin(device) < 0) {
		si702x_begin(device);
		bmp180_begin(device);
		WBVersion = 1;
	}


	/*------------------------*/
	/* Set up initial logfile */
	/*------------------------*/

	if (strcmp(logfile_name,"") != 0) {
		if (strcmp(rollover_timeStr,"") != 0 || rperiod != (-1)) {
		   strhostdate((char *)NULL,(char *)NULL,datetimeStr);
		   (void)sprintf(eff_logfile_name,"%s.%s",logfile_name,datetimeStr);
		} else
		    (void)strcpy(eff_logfile_name,logfile_name);

		if ((stream = fopen(eff_logfile_name,"w")) == (FILE *)NULL) {
		   if (do_verbose == TRUE) {
		      (void)fprintf(stderr,"    weatherboard ERROR: could not open logfile \"%s\"\n",eff_logfile_name);
		      (void)fflush(stderr);
		   }

		   exit(255);
		}
	}



	/*------------------------*/
	/* Set up signal handlers */
	/*------------------------*/

	if (strcmp(logfile_name,"") != 0 && strcmp(logfile_name,"tty") != 0)
	   (void)signal(SIGUSR1, (void *)&signal_handler);

        (void)signal(SIGABRT, (void *)&signal_handler);
	(void)signal(SIGQUIT, (void *)&signal_handler);
        (void)signal(SIGINT,  (void *)&signal_handler);
	(void)signal(SIGHUP,  (void *)&signal_handler);
	(void)signal(SIGTERM, (void *)&signal_handler);
        (void)signal(SIGPIPE, (void *)&signal_handler);
	(void)signal(SIGUSR2, (void *)&signal_handler);


	/*-----------*/
        /* Main loop */
	/*-----------*/

	if (rperiod != (-1))
	   nowsecs = time((time_t *)NULL);

	while (1) {

		unsigned int  itemperature,
		              ihumidity,
		              ipressure;

                unsigned char dateStr[SSIZE]      = "",
		              timeStr[SSIZE]      = "",
		              datetimeStr[SSIZE]  = "";


                /*----------*/
                /* Get time */
                /*----------*/

                strhostdate((char *)NULL,(char *)NULL,datetimeStr);

		/*-----------------------------------*/
		/* Produce "pretty" output if we are */
		/* connected to a terminal           */
		/*-----------------------------------*/

		if (tty_mode == TRUE && isatty(1) == 1) {

			clearScreen();

			(void)fprintf(stdout,"\n    Weather Board (version %s)\n",WEATHERBOARD_VERSION);
	                (void)fprintf(stdout,"    M.A. O'Neill, Tumbling Dice, 2016-2023\n");
			(void)fprintf(stdout,"\n    %s\n\n",datetimeStr);

			(void)fprintf(stdout,"    ======== si1132 ========\n");
			(void)fprintf(stdout,"    UV_index     : %4.2f\n",    Si1132_readUV()/100.0);
			(void)fprintf(stdout,"    Visible      : %6.2f Lux\n",Si1132_readVisible()/100.0);
			(void)fprintf(stdout,"    IR           : %6.2f Lux\n",Si1132_readIR()/100.0);

			if (WBVersion == 2) {
				bme280_read_pressure_temperature_humidity( &ipressure, &itemperature, &ihumidity);

				(void)fprintf(stdout,"    ======== bme280 ========\n");
				(void)fprintf(stdout,"    temperature : %4.2f 'C\n", (float)itemperature/100.0);
				(void)fprintf(stdout,"    humidity    : %4.2f %%\n", (float)ihumidity/1024.0);
				(void)fprintf(stdout,"    dew point   : %4.2f C\n",  (float)(itemperature/100.0) - ((100.0 - (float)ihumidity/1024.0)) / 5.0);
				(void)fprintf(stdout,"    pressure    : %6.2f hPa\n",(float)ipressure/100.0 + 10.0);
				(void)fflush(stdout);
			} else {
				(void)fprintf(stdout,"    ======== bmp180 ========\n");
				(void)fprintf(stdout,"    temperature : %4.2f 'C\n",  BMP180_readTemperature());
				(void)fprintf(stdout,"    pressure    : %6.2f hPa\n", BMP180_readPressure()/100);
				(void)fprintf(stdout,"    ======== si7020 ========\n");
				(void)fprintf(stdout,"    temperature : %4.2f 'C\n",  Si702x_readTemperature());
				(void)fprintf(stdout,"    humidity    : %4.2f %%\n",  Si702x_readHumidity());
			}

			(void)fflush(stdout);
		        (void)sleep(update_period);

		}
	

	        /*----------------------*/	
	        /* Logging data to file */
	        /*----------------------*/
                /*-------------------*/
                /* List style output */
                /*-------------------*/

		else if (stream != (FILE *)NULL) {

                        uv_index = Si1132_readUV()/100.0;
                        vis      = Si1132_readVisible()/100.0;
                        ir       = Si1132_readIR()/100.0;

                        if (WBVersion == 2) {

                                bme280_read_pressure_temperature_humidity( &ipressure, &itemperature, &ihumidity);

                                temperature = (double)itemperature / 100.0;
                                humidity    = (double)ihumidity    / 1000.0;
                                pressure    = (double)ipressure    / 100.0 + 10.0;
                        } else {
                                temperature = (BMP180_readTemperature() + Si702x_readTemperature()) / 2.0;
                                humidity    = Si702x_readHumidity();
                                pressure    = BMP180_readPressure();
                                altitude    = BMP180_readAltitude(SEALEVELPRESSURE_HPA);

                        }


			/*----------------------------------------------------------------*/
			/* See Lawerence et al. 2005 for details of dew point calculation */
			/*----------------------------------------------------------------*/

			dew_point   = temperature - ((100.0 - humidity) / 5.0);
                        (void)fprintf(stream,"%s  uvi: %8.2f  vis: %8.2f lux  ir: %8.2f lux  temp: %8.2f C  humidity: %8.2f %%  dew point %8.2f C  pressure: %8.2f hpa\n",
                                                                                                                                                              datetimeStr,
                                                                                                                                                                 uv_index,
                                                                                                                                                                      vis,
                                                                                                                                                                       ir,
                                                                                                                                                              temperature,
                                                                                                                                                                 humidity,
																   		      	        dew_point,
                                                                                                                                                                 pressure);
                        (void)fflush(stream);


			/*-------------------------*/
			/* Sleep until next update */
			/*-------------------------*/

		        (void)sleep(update_period);


			/*-------------------------*/
			/* Do we need to rollover? */
			/*-------------------------*/


			if (do_rollover_enabled == TRUE) {
			   if (strcmp(rollover_timeStr,"") != 0) {
		              strhostdate(dateStr,timeStr,(char *)NULL);

			      (void)sscanf(rollover_timeStr,"%d:%d:%d",&rhour,&rminute,&rsecond);
			      (void)sscanf(timeStr,         "%d:%d:%d",&hour, &minute, &second);

			      rollsecs = (time_t)(rhour*3600 + rminute*60 + rsecond);
			      nowsecs  = (time_t)(hour*3600  + minute*60  + second);

			      if (nowsecs >= rollsecs && nowsecs < rollsecs + update_period)
			         do_rollover = TRUE;
                           }
		           else if (time((time_t *)NULL) - nowsecs >= rperiod) {
			      nowsecs     = time((time_t *)NULL);
			      do_rollover = TRUE;
			   }
                        }


			/*---------------------------*/
			/* Are we going to rollover? */
			/*---------------------------*/

			if (do_rollover == TRUE) {
			   (void)fclose(stream); 

		           strhostdate((char *)NULL,(char *)NULL,datetimeStr);
			   (void)sprintf(eff_logfile_name,"%s.%s",logfile_name,datetimeStr);

			   if ((stream = fopen(eff_logfile_name,"w")) == (FILE *)NULL) {
	
			      if (do_verbose == TRUE) {
			         (void)fprintf(stderr,"    weatherboard ERROR: problem rolling over (new logfile \"%s\")\n",eff_logfile_name);
				 (void)fflush(stderr);
			      } 	
	
		 	      (void)exit(255);
			   }

			   else {
			      if (do_verbose == TRUE) {
                                 (void)fprintf(stderr,"    weatherboard rolling over (new logfile \"%s\")\n",eff_logfile_name);
                                 (void)fflush(stderr);
                              }
			  }

	                   do_rollover = FALSE;	
			}

		}


	        /*----------------------------------------------------*/	
	        /* Looging to stdout (if not redirected to /dev/null) */
	        /*----------------------------------------------------*/
                /*-------------------*/
                /* List style output */
                /*-------------------*/


		else  {

			uv_index = Si1132_readUV()/100.0;
			vis      = Si1132_readVisible()/100.0;
			ir       = Si1132_readIR()/100.0;

			if (WBVersion == 2) {

                                bme280_read_pressure_temperature_humidity( &ipressure, &itemperature, &ihumidity);

				temperature = (double)itemperature / 100.0;
				humidity    = (double)ihumidity    / 1024.0;
				pressure    = (double)ipressure    / 100.0 + 10.0;
			} else {
				temperature = (BMP180_readTemperature() + Si702x_readTemperature()) / 2.0;
				humidity    = Si702x_readHumidity();
				pressure    = BMP180_readPressure();
				altitude    = BMP180_readAltitude(SEALEVELPRESSURE_HPA);

			}


			/*----------------------------------------------------------------*/
			/* See Lawerence et al. 2005 for details of dew point calculation */
			/*----------------------------------------------------------------*/

			dew_point   = temperature -((100.0 - humidity) / 5.0);

			if (datasink(1) == FALSE) {
                           (void)fprintf(stdout,"%s  uvi: %8.2f  vis: %8.2f lux  ir: %8.2f lux  temp: %8.2f C  humidity: %8.2f %%  dew point %8.2f C  pressure: %8.2f hpa\n",
                                                                                                                                                                 datetimeStr,
                                                                                                                                                                    uv_index,
                                                                                                                                                                         vis,
                                                                                                                                                                          ir,
                                                                                                                                                                 temperature,
                                                                                                                                                                    humidity,
                                                                                                                                                                   dew_point,
                                                                                                                                                                    pressure);

                           (void)fflush(stdout);
			}


			/*-------------------------*/
			/* Sleep until next update */
			/*-------------------------*/

		        (void)sleep(update_period);
		}

	}

	exit(0);
}
