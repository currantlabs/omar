/* Copyright (c) 2019 Currant Inc. All Rights Reserved.
 *
 * omar_als_timer.c - sets up a timer which fires once a second, turns off the leds briefly
 * and then takes a reading with the ambient light sensor.
 *
 */
#pragma once

/* /\* */
/*  * To support grabbing a few seconds' worth of samples: */
/*  *\/ */
/* #define ALS_SAMPLE_COUNT            (4096) */
/* #define OMAR_ALS_SAMPLER_TIMER      (TIMER_1) */

/* // 2.083 milliseconds is 1/8 of a single 60Hz period: */
/* #define OMAR_ALS_SAMPLER_INTERVAL   (0.002083) */

/* typedef enum { */
/*     HEXDUMP_REPORT_FORMAT = 0, */
/*     SINGLECOLUMNDECIMAL_REPORT_FORMAT */
/* } als_backroundsample_reportformat_t; */


void timer_setup(void); // initialize the timer, and a task to process timer events
void enable_als_timer(bool on); // if "on" is true call "timer_start()", else "timer_pause()"

// apis for starting an als sample capture session, reporting results:
/* void start_als_sample_capture(void); */
/* void report_als_samples(als_backroundsample_reportformat_t format); */

