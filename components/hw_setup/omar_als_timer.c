/* Copyright (c) 2019 Currant Inc. All Rights Reserved.
 *
 * omar_als_timer.c - configures a timer to sneak a peek
 * at the ambient light sensor ("als") once every second.
 *
 */

#include <stdio.h>
#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "hw_setup.h"
#include "omar_als_timer.h"

#define TIMER_DIVIDER               80                                  

/*
 * The timer alternates between 2 alarm periods.
 * A longer TIMER_PEEK_ALARM period of 1 second,
 * and a shorter period of TIMER_ALSREAD_ALARM
 * that waits 250 microseconds to read the 
 * ambient light sensor after the LEDs are turned
 * off, to give the output of the ALS time to fall.
 */ 
#define TIMER_PEEK_ALARM            (1000000)
#define TIMER_ALSREAD_ALARM         (250)

/*
 * Use TIMER_0 from TIMER_GROUP_0:
 */
#define ALS_TIMER_GROUP             (TIMER_GROUP_0)
#define ALS_TIMER                   (TIMER_0)

/*
 * A simple structure to pass events
 * from the timer interrupt handler to 
 * the main program.
 */
typedef enum {
    ALS_PEEK_TIMER = 0,
    ALS_READ_TIMER
} als_timer_t;

typedef struct {
    als_timer_t timer; 
    int als;
    uint64_t timer_counter_value;
} timer_event_t;

xQueueHandle timer_queue;

/*
 * als_timer_isr() ALS timer interrupt handler:
 *
 * Note:
 * We don't call the timer API here because they are not declared with IRAM_ATTR.
 * If we're okay with the timer irq not being serviced while SPI flash cache is disabled,
 * we can allocate this interrupt without the ESP_INTR_FLAG_IRAM flag and use the normal API.
 */
void IRAM_ATTR als_timer_isr(void *para)
{
    static bool peek_alarm = true;
    uint64_t next_alarm;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    TIMERG0.hw_timer[ALS_TIMER].update = 1;
    uint64_t timer_counter_value = 
        ((uint64_t) TIMERG0.hw_timer[ALS_TIMER].cnt_high) << 32
        | TIMERG0.hw_timer[ALS_TIMER].cnt_low;

    /* The counter value as seen now is 1 more than the limit we set, so re-adjust: */
    timer_counter_value -= 1;
    
    /* Prepare to send event data back to main task:*/
    timer_event_t evt;

    if (peek_alarm) {
        evt.timer = ALS_PEEK_TIMER;
        evt.als = -1;
        next_alarm = timer_counter_value + TIMER_ALSREAD_ALARM;
    } else {
        evt.timer = ALS_READ_TIMER;
        evt.als = 42;
        next_alarm = timer_counter_value + TIMER_PEEK_ALARM;
    }

    evt.timer_counter_value = timer_counter_value;

    /* Clear the interrupt, and udpate the next alarm time for the timer:*/
    TIMERG0.int_clr_timers.t0 = 1;

    TIMERG0.hw_timer[ALS_TIMER].alarm_high = (uint32_t) (next_alarm >> 32);
    TIMERG0.hw_timer[ALS_TIMER].alarm_low = (uint32_t) next_alarm;

    /* The alarm must be re-enabled each time it is triggered:*/
    TIMERG0.hw_timer[ALS_TIMER].config.alarm_en = TIMER_ALARM_EN;
    
    /* Now just send the event data back to the main program task */
    xQueueSendFromISR(timer_queue, &evt, NULL);

    /* Finally, switch the state: */
    peek_alarm = !peek_alarm;
    
}

static void als_timer_task(void *arg)
{
    while (1) {
        timer_event_t evt;
        xQueueReceive(timer_queue, &evt, portMAX_DELAY);

        switch (evt.timer) {

        case ALS_PEEK_TIMER:
            printf("\r\n%s(): ALS peek timer fired at counter = 0x%08x%08x (%d). ALS = %d.\r\n",
                   __func__,
                   (uint32_t) (evt.timer_counter_value >> 32),
                   (uint32_t) (evt.timer_counter_value),
                   (uint32_t) (evt.timer_counter_value),
                   evt.als);
            break;
            
        case ALS_READ_TIMER:
            printf("\r\n%s(): ALS read timer fired at counter = 0x%08x%08x (%d). ALS = %d.\r\n",
                   __func__,
                   (uint32_t) (evt.timer_counter_value >> 32),
                   (uint32_t) (evt.timer_counter_value),
                   (uint32_t) (evt.timer_counter_value),
                   evt.als);
            break;
            
        default:
            printf("\r\n%s(): Uknown als timer event type: 0x%02x\r\n",
                   __func__,
                   evt.timer);
            break;
            
        }
    }   
}


xQueueHandle timer_queue;


static void als_timer_init(void)
{
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = false;
    timer_init(ALS_TIMER_GROUP, ALS_TIMER, &config);

    // Configure to count up from 0:
    timer_set_counter_value(ALS_TIMER_GROUP, ALS_TIMER, 0);

    // Initially, the alarm is set for 1 second;
    // once the alarm fires, we'll re-set the alarm
    // value for the much shorter 250 usec ALS
    // read interval:
    timer_set_alarm_value(ALS_TIMER_GROUP, ALS_TIMER, TIMER_PEEK_ALARM);

    timer_isr_register(ALS_TIMER_GROUP, ALS_TIMER, als_timer_isr, 
        NULL, ESP_INTR_FLAG_IRAM, NULL);
    timer_enable_intr(ALS_TIMER_GROUP, ALS_TIMER);

    // Defer calling "timer_start(ALS_TIMER_GROUP, ALS_TIMER);"
    // till later. Use the "enable_als_timer()" api for that
    // (this api can also pause things)

}

/*
 * Functions exported via omar_als_timer.h:
 */

void timer_setup(void)
{
    timer_queue = xQueueCreate(10, sizeof(timer_event_t));
    als_timer_init();
    xTaskCreate(als_timer_task, "als_timer_task", 2048, NULL, 5, NULL);
}

void enable_als_timer(bool on)
{
    if (on) {
        timer_start(ALS_TIMER_GROUP, ALS_TIMER);
    } else {
        timer_pause(ALS_TIMER_GROUP, ALS_TIMER);
    }
}

