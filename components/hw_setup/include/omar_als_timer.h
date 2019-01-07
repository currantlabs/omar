/* Copyright (c) 2019 Currant Inc. All Rights Reserved.
 *
 * omar_als_timer.c - sets up a timer which fires once a second, turns off the leds briefly
 * and then takes a reading with the ambient light sensor.
 *
 */
#pragma once

void timer_setup(void); // initialize the timer, and a task to process timer events
void enable_als_timer(bool on); // if "on" is true call "timer_start()", else "timer_pause()"
