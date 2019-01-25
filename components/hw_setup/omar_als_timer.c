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
#include "esp_task_wdt.h"
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

static xQueueHandle timer_queue;

static int als_normal = -1, als_darkpeek = -1;

#define LEDSOFF (false)
#define LEDSON  (true)

/* static void ledsonoff(bool on) */
/* { */
/*  if (on) { */
/*      led_set_brightness(OMAR_WHITE_LED0, 8000); */
/*      led_set_brightness(OMAR_WHITE_LED1, 8000); */
/*  } else { */
/*      led_set_brightness(OMAR_WHITE_LED0, 0); */
/*      led_set_brightness(OMAR_WHITE_LED1, 0); */
/*  } */
/* } */


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
        evt.timer_counter_value = timer_counter_value;
        xQueueSendFromISR(timer_queue, &evt, NULL);

        als_normal = als_raw();

        /* ledsonoff(LEDSOFF); */

        next_alarm = timer_counter_value + TIMER_ALSREAD_ALARM;

    } else {
        evt.timer = ALS_READ_TIMER;
        evt.als = 42;
        evt.timer_counter_value = timer_counter_value;
        xQueueSendFromISR(timer_queue, &evt, NULL);

        als_darkpeek = als_raw();

        /* ledsonoff(LEDSON); */

        next_alarm = timer_counter_value + TIMER_PEEK_ALARM;
    }


    /* Clear the interrupt, and udpate the next alarm time for the timer:*/
    TIMERG0.int_clr_timers.t0 = 1;

    TIMERG0.hw_timer[ALS_TIMER].alarm_high = (uint32_t) (next_alarm >> 32);
    TIMERG0.hw_timer[ALS_TIMER].alarm_low = (uint32_t) next_alarm;

    /* The alarm must be re-enabled each time it is triggered:*/
    TIMERG0.hw_timer[ALS_TIMER].config.alarm_en = TIMER_ALARM_EN;

    /* Finally, switch the state: */
    peek_alarm = !peek_alarm;
    
}

static void als_timer_task(void *arg)
{
    uint32_t block_time_msec = 250; // block a quarter second so you can pet the watchdog

    // Subscribe this task to the TWDT, check to make sure it's subscribed:
    CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);

    while (1) {
        timer_event_t evt;
        bool gotQueueEvent;

        gotQueueEvent = xQueueReceive(timer_queue, &evt, block_time_msec / portTICK_PERIOD_MS);

        CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);  // Pet the watchdog each time through

        if (!gotQueueEvent) {
            // Timed out waiting for an event, nothing on the queue:
            continue;
        }

        /* printf("Burrrrrrrp\n"); */

        printf("\t\t\t\t als_normal = %d, als_darkpeek = %d\n", als_normal, als_darkpeek);

        /* switch (evt.timer) { */

        /* case ALS_PEEK_TIMER: */
        /*     printf("\r\n%s(): ALS peek timer fired at counter = 0x%08x%08x (%d). ALS = %d.\r\n", */
        /*            __func__, */
        /*            (uint32_t) (evt.timer_counter_value >> 32), */
        /*            (uint32_t) (evt.timer_counter_value), */
        /*            (uint32_t) (evt.timer_counter_value), */
        /*            evt.als); */
        /*     break; */
            
        /* case ALS_READ_TIMER: */
        /*     printf("\r\n%s(): ALS read timer fired at counter = 0x%08x%08x (%d). ALS = %d.\r\n", */
        /*            __func__, */
        /*            (uint32_t) (evt.timer_counter_value >> 32), */
        /*            (uint32_t) (evt.timer_counter_value), */
        /*            (uint32_t) (evt.timer_counter_value), */
        /*            evt.als); */
        /*     break; */
            
        /* default: */
        /*     printf("\r\n%s(): Uknown als timer event type: 0x%02x\r\n", */
        /*            __func__, */
        /*            evt.timer); */
        /*     break; */
            
        /* } */
    }   
}


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

    timer_start(ALS_TIMER_GROUP, ALS_TIMER);

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
        /* ledsonoff(LEDSON); */
        /* timer_start(ALS_TIMER_GROUP, ALS_TIMER); */
    } else {
        /* ledsonoff(LEDSOFF); */
        /* timer_pause(ALS_TIMER_GROUP, ALS_TIMER); */
    }
}

