//Original program:
/*Copyright (C) 2013 Andreas Ehliar

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:
*/
//Program modifed in 2015 by Daniel Haverås and Adam Richert, for the purpose of implementing a Simulink Model in real-time

#include "ModelCopy2.h"
#include "rtwtypes.h"
#include "ModelCopy2_types.h"
#include "ModelCopy2.c"
#include "ModelCopy2_data.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <signal.h>
#include <math.h> 
#include "rt_look.c"
#include "rt_look1d.c"
#include "rt_matrx.c"
#include "rt_nonfinite.c"
#include "rtGetInf.c"
#include "rtGetNaN.c"
#include "fixedpoint.h"
#include "fixpoint_spec.h"
#include "sfun_psbdqc.c"
#include <wiringPi.h>
#include "Getwind.c" 
#define MAX_LOGENTRIES  20000000
static unsigned int logindex;
static struct timespec timestamps[MAX_LOGENTRIES];
static void logtimestamp(void)
{
        clock_gettime(CLOCK_MONOTONIC, &timestamps[logindex++]);
        if(logindex > MAX_LOGENTRIES){
                logindex = 0;
        }
}
 
static void dumptimestamps(int unused)
{
        FILE *fp = fopen("timestamps3.txt","w");
        int i;
        for(i=0; i < logindex; i++){
                if(timestamps[i].tv_sec > 0){
                        fprintf(fp,"%d.%09d\n", (int) timestamps[i].tv_sec,
                                (int) timestamps[i].tv_nsec);
                }
        }
        fclose(fp);
        exit(0);
}
 
// Adds "delay" nanoseconds to timespecs and sleeps until that time
static void sleep_until(struct timespec *ts, int delay)
{
         
        ts->tv_nsec += delay;
        if(ts->tv_nsec >= 1000*1000*1000){
                ts->tv_nsec -= 1000*1000*1000;
                ts->tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ts,  NULL);
}
 
 
float rt_OneStep(int inputwind)
{
  static boolean_T OverrunFlag = 0;

  /* '<Root>/VIND' */
  static real_T arg_VIND;

  /* '<Root>/PUT' */
  static real_T arg_PUT;

  /* Disable interrupts here */
  //nointerrupts();
  /* Check for overrun */
  if (OverrunFlag) {
    rtmSetErrorStatus(ModelCopy2_M, "Overrun");
    return 0;
  }
 
  OverrunFlag = true;
 
  /* Save FPU context here (if necessary) */
  /* Re-enable timer or interrupt here */
  /* Set model inputs here */
  arg_VIND = inputwind;
  /* Step the model */
  arg_PUT = ModelCopy2_custom(arg_VIND);
 
  /* Get model outputs here */
 
  /* Indicate task complete */
  OverrunFlag = false;
 
  /* Disable interrupts here */
  /* Restore FPU context here (if necessary) */
  /* Enable interrupts here */
  //interrupts();
return arg_PUT;
}
 
//int_T main(int_T argc, const char_T *argv)
int_T main(void)
{
 
    struct timespec ts;
    unsigned int delay = 1000*1000; // Note: Delay in ns
 
    /* Initialize model */
    ModelCopy2_initialize();
    wiringPiSetupGpio();
    pinMode(17,INPUT);
    pinMode(18,INPUT);
    pinMode(7,INPUT);
    pinMode(22,INPUT);
    pinMode(23,INPUT);
 
    signal(SIGINT, dumptimestamps);
    clock_gettime(CLOCK_MONOTONIC, &ts);
 
    // Lock memory to ensure no swapping is done.
    if(mlockall(MCL_FUTURE|MCL_CURRENT)){
    fprintf(stderr,"WARNING: Failed to lock memory\n");
    }
     
    // Set our thread to real time priority
    struct sched_param sp;
    sp.sched_priority = 30;
    if(pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp)){
                fprintf(stderr,"WARNING: Failed to set stepper thread"
                        "to real-time priority\n");
    }
    int pincounter= 0;
    int trip = 0;
    int inputwind = GetWind();
    while(1){
	inputwind = GetWind();

        sleep_until(&ts,delay); logtimestamp();; 
        int stuff = rt_OneStep(inputwind);
	int Qout = ModelCopy2_B.QUT;
	pincounter++; 

	if (pincounter ==100){
		printf("\n");

	 	pincounter = 0;
       	 	printf("    Wind Speed = %d m/s             Active Power (P) = %d W",inputwind,stuff);
		printf("             Reactive Power (Q) = %d var",Qout);

		if (inputwind >=30){
		trip = 1;
		}
		if(trip){
			printf("              Wind generator shut down because of storm. Manual restart required.");
		}
 	}
    }
 
}
