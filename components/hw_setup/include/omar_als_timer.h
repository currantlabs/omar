/* Copyright (c) 2019 Currant Inc. All Rights Reserved.
 *
 * omar_als_timer.c - sets up a timer which fires once a second, turns off the leds briefly
 * and then takes a reading with the ambient light sensor.
 *
 */
#pragma once

#define OMAR_ALS_TIMER_GROUP        (TIMER_GROUP_0)

/*
 * The primary timer fires once every 
 * OMAR_ALS_PRIMARY_INTERVAL seconds.
 * When the primary timer fires, it pauses
 * the led pwm, shutting the leds off
 * if they are on.
 * 
 * TYPICAL VALUE: A "sampling interval" of
 * one or two seconds should be enough
 * to keep track of changes in ambient
 * light levels and adjust the led brightness
 * accordingly.
 */
#define OMAR_ALS_PRIMARY_TIMER      (TIMER_0)
#define OMAR_ALS_PRIMARY_INTERVAL   (2.0)

/*
 * The secondary timer is started by the
 * primary timer each time it fires. 
 * It's purpose is to give the output
 * of the ambient light sensing 
 * phototransistor time to change 
 * from the from the value it registered
 * with one or both of omar's leds on,
 * to the value registered in response
 * to ambient light alone.
 * 
 * When the secondary timer fires, it
 * samples the ambient light sensor
 * output, and then re-enables the
 * led pwm so the leds that were on
 * at the time the primary timer fired
 * (if there were any) turn on again.
 *
 * TYPICAL VALUE: Something less than
 * 500 usec, because if the leds go off
 * for a period of time of about 1 msec
 * or greater, the flicker becomes visible
 * to the naked eye.
 */
#define OMAR_ALS_SECONDARY_TIMER    (TIMER_1)
#define OMAR_ALS_SECONDARY_INTERVAL (0.000010)


void timer_setup(void); // initialize the timer, and a task to process timer events
void enable_als_timer(bool on); // if "on" is true call "timer_start()", else "timer_pause()"
