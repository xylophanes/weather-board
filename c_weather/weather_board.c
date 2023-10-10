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

#define VERSION "3.00"

#define FALSE   0
#define TRUE    255


/*------------------*/
/* Global variables */
/*------------------*/

int do_verbose    = FALSE;


/*-------*/
/* Local */
/*-------*/

static int    i2c_address             = 0x76;
static float  uv_index;
static float  vis;
static float  ir;
static float  pressure;
static float  temperature;
static float  humidity;
static float  dew_point;
static float  altitude;
static char   logfile_name[256]       = "";
static char   rollover_timeStr[256]   = "";
static char   rollover_periodStr[256] = "";
static int    do_rollover             = FALSE;
static int    do_rollstop             = FALSE;
static int    do_rollcont             = FALSE;
static int    do_rollover_enabled     = FALSE;
static time_t rperiod                 = (-1);
static time_t nowsecs                 = (-1);
static time_t rollsecs                = (-1);




/*--------------------------*/
/* For altitude calculation */
/*--------------------------*/

float SEALEVELPRESSURE_HPA = 1024.25;




/*-----------------
 * Clear the screen
 *---------------*/

void clearScreen(void)
{
  const char* CLEAR_SCREE_ANSI = "\e[1;1H\e[2J";
  write(STDOUT_FILENO,CLEAR_SCREE_ANSI,12);
}




/*---------------------------------------------
 * Handle SIGUSR1 which forces a (log) rollover
 * SIGUSR2 which stops data collection
 *-------------------------------------------*/

static int signal_handler(int signum)

{	if(signum == SIGUSR1)
		do_rollover = TRUE;
	else if(signum == SIGUSR2)
		do_rollstop = TRUE;
	else if(signum == SIGCONT)
		do_rollcont = TRUE;

	return(0);
}




/*--------------------------------------------------
 *  Get current time and date in human readable form
 -------------------------------------------------*/

void strhostdate(char *date, char *time, char *datetime)

{   time_t tval;
    double usecs;

    char f1[256]        = "",
         f2[256]        = "",
         f3[256]        = "",
         f4[256]        = "",
         strusecs[256]  = "",
	 tmpdate[256]   = "";

    //struct timeb tspec;
    struct timespec tspec;

    //(void)ftime(&tspec);
    //tval     = (time_t)tspec.time;
    //usecs    = (double)tspec.millitm/1000.0;

    (void)clock_gettime(CLOCK_REALTIME,&tspec);
    tval  = tspec.tv_sec;
    usecs = (double)(tspec.tv_nsec) / 1000000.0;

    (void)strcpy(tmpdate,ctime(&tval));
    tmpdate[strlen(tmpdate) - 1] = '\0';
    (void)sscanf(tmpdate,"%s%s%s%s",f1,f2,f3,f4);


    /*--------------------*/
    /* Fill in time field */
    /*--------------------*/

    if(time != (char *)NULL)
       (void)sprintf(time,"%s",f4);


    /*-----------------------------*/
    /* Fill in abridged date field */
    /*-----------------------------*/

    if(date != (char *)NULL)
       (void)sprintf(date,"%s.%s",f3,f2);


    /*------------------------------------*/
    /* Fill in fully qualified date field */
    /*------------------------------------*/

    if(datetime != (char *)NULL) {
       (void)sprintf(strusecs,"%.2f",usecs);
       (void)sprintf(datetime,"%s.%s.%s-%s",f1,f2,f3,f4);
    }
}



/*------------------*/
/* main entry point */
/*------------------*/

int main(int argc, char *argv[])
{
	int  i;
	int  now;
	int  rhour;
	int  rminute;
	int  rsecond;
	int  hour;
	int  minute;
	int  second;
	int  tty_mode              = FALSE;
	int  t_period              = 10;
	int  status                = 0;
	int  WBVersion             = 2;
	int  argd                  = 1;
	char *device               = "/dev/i2c-1";
	char rollover_timeStr[256] = "";
	char eff_logfile_name[256] = "";
	char dateStr[256]          = "";
	char timeStr[256]          = "";
	char datetimeStr[256]      = "";
	FILE *stream               = (FILE *)NULL;


        /*--------------------*/
        /* Parse command tail */
        /*--------------------*/

	if(argc > 1) {
		for(i=0; i<argc; ++i) {


		   /*--------------------------*/
		   /* Display help information */
		   /*--------------------------*/

		   if(strcmp(argv[1],"-help") == 0 || strcmp(argv[1],"-usage") == 0) {
		      (void)fprintf(stderr,"\n    Weather Board (version %s)\n",VERSION);
		      (void)fprintf(stderr,"    M.A. O'Neill, Tumbling Dice, 2016-2023\n\n");
   		      (void)fprintf(stderr,"    Usage : [sudo] weather_board [-usage | -help]\n");
       		      (void)fprintf(stderr,"            |\n");
	              (void)fprintf(stderr,"            [-pperiod <secs:60>]\n");
             	      (void)fprintf(stderr,"            [-logfile <file name> [-rollover <hh:mm:ss:00:00:00> | -rperiod <hh:mm:ss>]]\n");
              	      (void)fprintf(stderr,"            [i2c node:/dev/i2c-1]\n");
	              (void)fprintf(stderr,"            [ >& <error/status log>]\n\n");
	              (void)fprintf(stderr,"            Signals\n\n");
	              (void)fprintf(stderr,"            SIGUSR1: force rollover\n");
	              (void)fprintf(stderr,"            SIGUSR2: stop data collection\n");
	              (void)fprintf(stderr,"            SIGCONT: resume data collection\n\n");
                      (void)fflush(stderr);

	              exit(1);
                   } 


	           /*----------------*/
	           /* Verbosity flag */
	           /*----------------*/

	           else if(strcmp(argv[i],"-verbose") == 0) {
	              do_verbose = TRUE;
	              ++argd;
	           }


	           /*-----------------*/
	           /* Set time period */
	           /*-----------------*/

	           else if(strcmp(argv[i],"-pperiod") == 0) {
 	              if(i == argc - 1 || argv[i + 1][0] == '-') {

		         if(do_verbose == TRUE) {
		            (void)fprintf(stderr,"weatherboard ERROR: expecting time period in seconds\n");
		            (void)fflush(stderr);
		         }

		         exit(-1);
                      }

                      if(sscanf(argv[i+1],"%d",&t_period) != 1 || t_period <= 0) {

   		         if(do_verbose == TRUE) {
		            (void)fprintf(stderr,"weatherboard ERROR: expecting time period in seconds (integer > 0)\n");
		            (void)fflush(stderr);
		         }

		         exit(-1);
	              }

	              argd += 2;
	              ++i;
                   }


      	           /*------------------*/
	           /* Set logfile name */
	           /*------------------*/

                   else if(strcmp(argv[i],"-logfile") == 0) {
                      if(i == argc - 1 || argv[i+1][0] == '-') {

                         if(do_verbose == TRUE) {
                            (void)fprintf(stderr,"weatherboard ERROR: expecting logfile name\n");
                            (void)fflush(stderr);
                         }

                         exit(-1);
                      }

   	              if(strcmp(argv[i+1],"tty") == 0)
		         tty_mode = TRUE;
	              else
	                 (void)strcpy(logfile_name,argv[i+1]);

                      argd += 2;
                      ++i;
                   }


	           /*---------------------------*/
	           /* Set logfile rollover time */
	           /*---------------------------*/


                   else if(rperiod == (-1) && strcmp(logfile_name,"") != 0 && strcmp(logfile_name,"tty") && strcmp(argv[i],"-rollover") == 0) {
                      if(i == argc -1 || argv[i + 1][0] == '-') {

                         if(do_verbose == TRUE) {
                            (void)fprintf(stderr,"weatherboard ERROR: expecting rollover time <hh:mm:ss> (24 hour clock)\n");
                            (void)fflush(stderr);
                         }

                         exit(-1);
                      }

                      (void)strcpy(rollover_timeStr,argv[i+1]);
		      do_rollover_enabled = TRUE;

                      argd += 2;
                      ++i;
                   }


                   /*---------------------------*/
                   /* Set logfile rollover time */
                   /*---------------------------*/


                   else if(strcmp(rollover_timeStr,"") == 0 && strcmp(logfile_name,"") != 0 && strcmp(logfile_name,"tty") && strcmp(argv[i],"-rperiod") == 0) {
                      if(i == argc -1 || argv[i + 1][0] == '-') {

		         char tmpstr[256] = "";

                         if(do_verbose == TRUE) {
                            (void)fprintf(stderr,"weatherboard ERROR: expecting rollover period <hh:mm:ss> (24 hour clock)\n");
                            (void)fflush(stderr);
                         }

                         exit(-1);
                      }

		      if(sscanf(argv[i+1],"%d:%d:%d",&hour,&minute,&second) != 3) {
		          if(do_verbose == TRUE) {
                            (void)fprintf(stderr,"weatherboard ERROR: expecting rollover period <hh:mm:ss> (24 hour clock)\n");
                            (void)fflush(stderr);
                         }

                         exit(-1);
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

	if(argc > 1 && argd == argc - 1)
	   device = argv[argd];
	else if(argd < argc) {
	   if(do_verbose == TRUE) {
	      (void)fprintf(stderr,"weatherboard ERROR: unparsed command line parameters (have: %d, parsed: %d)\n",argc,argd);
	      (void)fflush(stderr);
	   }

	    exit(-1);
	}


        /*----------------------------------------*/
        /* Start communication with weather board */
        /*----------------------------------------*/

	si1132_begin(device);

        /*--------------------*/
        /* Display parameters */
        /*--------------------*/

        if(do_verbose == TRUE) {

	   (void)fprintf(stderr,"\n    Weather Board (version %s)\n",VERSION);
	   (void)fprintf(stderr,"    M.A. O'Neill, Tumbling Dice, 2016-2023\n\n");

           if(strcmp(logfile_name,"tty") == 0)
              (void)fprintf(stderr,"    terminal output mode\n");
           else {
	      if(strcmp(logfile_name,"") == 0)
                 (void)fprintf(stderr,"    logfile           :  standard output\n",logfile_name);
	      else
                 (void)fprintf(stderr,"    logfile (basename):  %s\n",logfile_name);

	      if(strcmp(rollover_timeStr,"") != 0)
                 (void)fprintf(stderr,"    rollover time     :  %s\n",rollover_timeStr);
	      else if(rperiod != (-1))
		 (void)fprintf(stderr,"    rollover period   :  %s (%d seconds)\n",rollover_periodStr,rperiod);
           }

           (void)fprintf(stderr,"    time period       :  %04d seconds\n",t_period);
           (void)fprintf(stderr,"    i2c bus           :  %s (sensors at i2c addresses 0x%d and 0x%d)\n\n",device,Si1132_ADDR,BMP180_ADDRESS);
           (void)fflush(stderr);
        }

	if(strcmp(logfile_name,"tty") == 0)
	   (void)sleep(5);

	if (bme280_begin(device) < 0) {
		si702x_begin(device);
		bmp180_begin(device);
		WBVersion = 1;
	}


	/*------------------------*/
	/* Set up initial logfile */
	/*------------------------*/

	if(strcmp(logfile_name,"") != 0) {
		if(strcmp(rollover_timeStr,"") != 0 || rperiod != (-1)) {
			strhostdate((char *)NULL,(char *)NULL,datetimeStr);
			(void)sprintf(eff_logfile_name,"%s.%s",logfile_name,datetimeStr);
		} else
			(void)strcpy(eff_logfile_name,logfile_name);

		if((stream = fopen(eff_logfile_name,"w")) == (FILE *)NULL) {
			if(do_verbose == TRUE) {
				(void)fprintf(stderr,"weatherboard ERROR: could not open logfile \"%s\"\n",eff_logfile_name);
				(void)fflush(stderr);
			}

			exit(-1);
		}
	}



	/*------------------------*/
	/* Set up signal handlers */
	/*------------------------*/

	if(strcmp(logfile_name,"") != 0 && strcmp(logfile_name,"tty") != 0) {
	   (void)signal(SIGUSR1,(void *)&signal_handler);
	   (void)signal(SIGUSR2,(void *)&signal_handler);
	   (void)signal(SIGCONT,(void *)&signal_handler);
        }

	/*-----------*/
        /* Main loop */
	/*-----------*/

	if(rperiod != (-1))
	   nowsecs = time((time_t *)NULL);

	while (1) {

		int itemperature,
		    ihumidity,
		    ipressure;

                char dateStr[256]     = "",
		     timeStr[256]     = "",
		     datetimeStr[256] = "";


                /*----------*/
                /* Get time */
                /*----------*/

                strhostdate((char *)NULL,(char *)NULL,datetimeStr);

		/*-----------------------------------*/
		/* Produce "pretty" output if we are */
		/* connected to a terminal           */
		/*-----------------------------------*/

		if(tty_mode == TRUE && isatty(1) == 1) {

			clearScreen();

			(void)fprintf(stdout,"\n    Weather Board (version %s)\n",VERSION);
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
		        (void)sleep(t_period);

		} else if(stream != (FILE *)NULL) {

                        /*-------------------------------------*/
                        /* Otherwise produce list style output */
                        /*-------------------------------------*/

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
                        (void)fprintf(stream,"%s  uvi: %4.2f  vis: %6.2f lux  ir: %6.2f lux  t: %6.2f C  humidity: %4.2f %%  dew point %6.2f C  pressure: %6.2f hpa\n",
                                                                                                                                                           datetimeStr,
                                                                                                                                                              uv_index,
                                                                                                                                                                   vis,
                                                                                                                                                                    ir,
                                                                                                                                                           temperature,
                                                                                                                                                              humidity,
																   		      	     dew_point,
                                                                                                                                                              pressure);
                        (void)fflush(stream);
		        (void)sleep(t_period);


			/*-------------------------*/
			/* Do we need to rollover? */
			/*-------------------------*/


			if(do_rollover_enabled == TRUE) {
			   if(strcmp(rollover_timeStr,"") != 0) {
		              strhostdate(dateStr,timeStr,(char *)NULL);

			      (void)sscanf(rollover_timeStr,"%d:%d:%d",&rhour,&rminute,&rsecond);
			      (void)sscanf(timeStr,         "%d:%d:%d",&hour, &minute, &second);

			      rollsecs = (time_t)(rhour*3600 + rminute*60 + rsecond);
			      nowsecs  = (time_t)(hour*3600  + minute*60  + second);

			      if(nowsecs >= rollsecs && nowsecs < rollsecs + t_period)
			         do_rollover = TRUE;
                           }
		           else if(time((time_t *)NULL) - nowsecs >= rperiod) {
			      nowsecs     = time((time_t *)NULL);
			      do_rollover = TRUE;
			   }
                        }


			/*---------------------------*/
			/* Are we going to rollover? */
			/*---------------------------*/

			if(do_rollover == TRUE) {
			   (void)fclose(stream); 

		           strhostdate((char *)NULL,(char *)NULL,datetimeStr);
			   (void)sprintf(eff_logfile_name,"%s.%s",logfile_name,datetimeStr);

			   if((stream = fopen(eff_logfile_name,"w")) == (FILE *)NULL) {
	
			      if(do_verbose == TRUE) {
			         (void)fprintf(stderr,"weatherboard ERROR: problem rolling over (new logfile \"%s\")\n",eff_logfile_name);
				 (void)fflush(stderr);
			      } 	
	
		 	      (void)exit(-1);
			   }
			   else {
			      if(do_verbose == TRUE) {
                                 (void)fprintf(stderr,"weatherboard rolling over (new logfile \"%s\")\n",eff_logfile_name);
                                 (void)fflush(stderr);
                              }
			  }

	                   do_rollover = FALSE;	
			}
			else if(do_rollstop == TRUE) {


			   /*-------------------------*/
			   /* Wait for rollover event */
			   /*-------------------------*/

			   while(do_rollover == FALSE || do_rollcont == FALSE)
				(void)sleep(1);

			   if(do_rollover == TRUE)
			      (void)fclose(stream);

			   do_rollstop = FALSE;
			   do_rollcont = FALSE;
			}

		} else {

			/*-------------------------------------*/
			/* Otherwise produce list style output */
			/*-------------------------------------*/

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
			(void)fprintf(stdout,"%s  uvi: %4.2f  vis: %6.2f lux  ir: %6.2f lux  t: %6.2f C  humidity: %4.2f %%  dew point %6.2f C  pressure: %6.2f hpa\n",
																                           datetimeStr,
																                              uv_index,
																                                   vis,
																                                    ir,
																                           temperature,
																                              humidity,
																			     dew_point,
																                              pressure);
			(void)fflush(stdout);
		        (void)sleep(t_period);
		}

	}

	return 0;
}
