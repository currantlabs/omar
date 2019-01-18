/* Copyright (c) 2019 Currant Inc. All Rights Reserved.
 *
 * omar_als_timer.c - sets up a timer which fires once a second, turns off the leds briefly
 * and then takes a reading with the ambient light sensor.
 *
 */


#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task_wdt.h"
#include "driver/ledc.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "esp_err.h"
#include "hw_setup.h"
#include "omar_als_timer.h"

// Enable OMAR_ALS_TIMER_VERBOSE to see lots of debug spew
//#define OMAR_ALS_TIMER_VERBOSE

static void pause_led_pwm(void);
static void resume_led_pwm(void);
static void timer_example_evt_task(void *arg);
static void hexdump_als_samples(void);

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define AUTO_RELOAD_ON        1 // testing will be done with auto reload
#define AUTO_RELOAD_OFF       0 // no auto reload
/*
 * A sample structure to pass events
 * from the timer interrupt handler to the main program.
 */
typedef struct {
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
    int als_reading;
} timer_event_t;

xQueueHandle timer_queue;

static double timer_periods[] = {
    OMAR_ALS_PRIMARY_INTERVAL,  
    OMAR_ALS_SECONDARY_INTERVAL,
    OMAR_ALS_SAMPLER_INTERVAL
};

static bool als_sample_mode = false;
static uint32_t als_sample_count = 0;
static int als_sample_array[ALS_SAMPLE_COUNT] = {-1};

void set_als_timer_period(als_timer_t timer, double period)
{

    // Flip negative time periods
    if (period < 0) {
        period *= -1.0;
    }


    double primarytimerperiod =
        (timer == PRIMARY_TIMER
          ?
         period
         :
         timer_periods[PRIMARY_TIMER]);
          

    double secondarytimerperiod =
        (timer == SECONDARY_TIMER
          ?
         period
         :
         timer_periods[SECONDARY_TIMER]);
          

    if (secondarytimerperiod >= primarytimerperiod/2.0) {
        printf("%s(): Error/Abort - the primary als timer period (%.8f seconds) must be at least twice as long as the secondary period (%.8f seconds)\n",
               __func__,
               primarytimerperiod,
               secondarytimerperiod);

        return;
    }

    if (fabs(period) <= NANOSECOND) {
        printf("%s(): Aborting call for timer %d because the interval is too short: %0.12f seconds\n",
               __func__,
               timer,
               period);

        return;
    }

    printf("\n%s(): Setting %s period to %0.8f seconds\n",
           __func__,
           (timer == PRIMARY_TIMER ? "primary timer" : "secondary timer"),
           period);


    timer_periods[timer] = period;

}

double get_als_timer_period(als_timer_t timer)
{
    return timer_periods[timer];
}


#if defined(OMAR__PRINT_DETAILED_COUNTER_INFO)
/*
 * A simple helper function to print the raw timer counter value
 * and the counter value converted to seconds
 */
static void inline print_timer_counter(uint64_t counter_value)
{
    printf("\t\t\t\t\t\t\t\tCounter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
                                    (uint32_t) (counter_value));
    printf("\t\t\t\t\t\t\t\tTime   : %.8f s\n", (double) counter_value / TIMER_SCALE);
}
#endif

/*
 * Timer group0 ISR handler
 *
 * Note:
 * We don't call the timer API here because they are not declared with IRAM_ATTR.
 * If we're okay with the timer irq not being serviced while SPI flash cache is disabled,
 * we can allocate this interrupt without the ESP_INTR_FLAG_IRAM flag and use the normal API.
 */
void IRAM_ATTR timer_group0_isr(void *para)
{
    int timer_idx = (int) para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    TIMERG0.hw_timer[timer_idx].update = 1;
    uint64_t timer_counter_value = 
        ((uint64_t) TIMERG0.hw_timer[timer_idx].cnt_high) << 32
        | TIMERG0.hw_timer[timer_idx].cnt_low;

    /* Prepare basic event data
       that will be then sent back to the main program task */
    timer_event_t evt;
    evt.timer_group = 0;
    evt.timer_idx = timer_idx;
    evt.timer_counter_value = timer_counter_value;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if ((intr_status & BIT(timer_idx)) && timer_idx == OMAR_ALS_PRIMARY_TIMER) {

        evt.type = timer_idx;
        TIMERG0.int_clr_timers.t0 = 1;

        /* After the alarm has been triggered
           we need enable it again, so it is triggered the next time */
        TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

        evt.als_reading = -1; // Flag to show we didn't read the adc here

        /* Now just send the event data back to the main program task */

        xQueueSendFromISR(timer_queue, &evt, NULL);


    } else if ((intr_status & BIT(timer_idx)) && timer_idx == OMAR_ALS_SECONDARY_TIMER) {

        if (als_sample_mode) {
            
            if (als_sample_count < ALS_SAMPLE_COUNT) {
                als_sample_array[als_sample_count++] = als_raw();

                // Re-enable the alarm since we've still got samples to take
                TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;
            } else {
                // Disable the sampling:
                als_sample_mode = false;

                // We've taken all the sample, prepare the event:
                evt.type = 42;

                // Pause the als sample timer:
                timer_pause(OMAR_ALS_TIMER_GROUP, OMAR_ALS_SAMPLER_TIMER);

                xQueueSendFromISR(timer_queue, &evt, NULL);

            }



        } else {

            evt.type = timer_idx;
            TIMERG0.int_clr_timers.t1 = 1;

            evt.als_reading = als_raw();

            /* The secondary timer is a "one-shot" timer, so don't
               enable it again here */


            xQueueSendFromISR(timer_queue, &evt, NULL);

        }

    } else {
        evt.type = -1; // not supported even type
    }



}

/*
 * Initialize selected timer of the timer group 0
 *
 * timer_idx - the timer number to initialize
 * auto_reload - should the timer auto reload on alarm?
 * timer_interval_sec - the interval of alarm to set
 */
static void omar_als_timer_init(int timer_idx, 
    bool auto_reload, double timer_interval_sec)
{

    if (fabs(timer_interval_sec) <= NANOSECOND) {
        printf("%s(): Aborting call for timer %d because the interval is too short: %0.12f seconds\n",
               __func__,
               timer_idx,
               timer_interval_sec);
        return;
    }

    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = auto_reload;
    timer_init(OMAR_ALS_TIMER_GROUP, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(OMAR_ALS_TIMER_GROUP, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(OMAR_ALS_TIMER_GROUP, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(OMAR_ALS_TIMER_GROUP, timer_idx);
    timer_isr_register(OMAR_ALS_TIMER_GROUP, timer_idx, timer_group0_isr, 
        (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

}

void start_als_sample_capture(void)
{

    if (als_sample_mode) {
        printf("%s(): Already taking als samples...\n", __func__);
        return;
    }

    als_sample_mode = true;
    als_sample_count = 0;

    // Start the timer!
    omar_als_timer_init(OMAR_ALS_SECONDARY_TIMER, 
                        AUTO_RELOAD_ON, 
                        get_als_timer_period(ALS_SAMPLE_TIMER));


    timer_start(OMAR_ALS_TIMER_GROUP, OMAR_ALS_SAMPLER_TIMER);
}

static void hexdump_als_samples(void)
{
    int address = 0;

    for (int i=0; i<=ALS_SAMPLE_COUNT/16; i++) {
        if (i*16 == ALS_SAMPLE_COUNT) {
            break;
        }
        printf("0x%04x: ", address + i*16);
        for (int j=0; i*16+j < ALS_SAMPLE_COUNT && j < 16; j++) {
            printf("0x%02x ", als_sample_array[i*16+j]);
        }
        printf("\n");
    }
    
}

void report_als_samples(void)
{
    if (als_sample_mode) {
        printf("%s(): Still taking als samples...\n", __func__);
        return;
    }

    hexdump_als_samples();
    

}

void enable_als_timer(bool on)
{
    if (on) {
        timer_start(OMAR_ALS_TIMER_GROUP, OMAR_ALS_PRIMARY_TIMER);
    } else {
        timer_pause(OMAR_ALS_TIMER_GROUP, OMAR_ALS_PRIMARY_TIMER);
    }
}

void timer_setup(void)
{
    timer_queue = xQueueCreate(10, sizeof(timer_event_t));
    omar_als_timer_init(OMAR_ALS_PRIMARY_TIMER, AUTO_RELOAD_ON, get_als_timer_period(PRIMARY_TIMER));
    xTaskCreate(timer_example_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);
}

typedef struct {
    uint32_t led0_dutycycle;
    uint32_t led1_dutycycle;
} led_dutycycle_state_t;
    
static led_dutycycle_state_t led_state = {0, 0};

static void pause_led_pwm(void)
{
    led_state.led0_dutycycle = led_get_brightness(OMAR_WHITE_LED0);
    led_state.led1_dutycycle = led_get_brightness(OMAR_WHITE_LED1);

    led_set_brightness(OMAR_WHITE_LED0, 0);
    led_set_brightness(OMAR_WHITE_LED1, 0);
}

static void resume_led_pwm(void)
{
    led_set_brightness(OMAR_WHITE_LED0, led_state.led0_dutycycle);
    led_set_brightness(OMAR_WHITE_LED1, led_state.led1_dutycycle);
}

static void timer_example_evt_task(void *arg)
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

        /* Print information that the timer reported an event */
        if (evt.type == OMAR_ALS_PRIMARY_TIMER) {
            /* Turn the LEDs off, and arm the secondary timer 
             * to read the als adc after a short delay of
             * OMAR_ALS_SECONDARY_INTERVAL:
             */
            pause_led_pwm();

            omar_als_timer_init(OMAR_ALS_SECONDARY_TIMER, AUTO_RELOAD_OFF, get_als_timer_period(SECONDARY_TIMER));
            timer_start(OMAR_ALS_TIMER_GROUP, OMAR_ALS_SECONDARY_TIMER);
            printf("\t\t\t\t\t\t\t\t<primary>\n");

        
        } else if (evt.type == OMAR_ALS_SECONDARY_TIMER) {

            /* Once you've got your reading,
             * re-enable the pwm led controller
             * so the lights turn on again:
             */
            resume_led_pwm();

            // Print out the als reading taken inside the timer interrupt:
            printf("\t\t\t\t\t\t\t\t[als=%d]\n", evt.als_reading);

            // Pause the secondary timer:
            timer_pause(OMAR_ALS_TIMER_GROUP, OMAR_ALS_SECONDARY_TIMER);
            
        } else if (evt.type == 42) {
            // The sampling of als output is finished, print the report:
          printf("%s(): Ambient light sensor sampling finished\n", __func__) ;
        } else {
            printf("\n\t\t\t\t\t\t\t\t  UNKNOWN EVENT TYPE\n");
        }

        



#if defined(OMAR_ALS_TIMER_VERBOSE)
        printf("\t\t\t\t\t\t\t\tResuming ledc timer select OMAR_LEDC_TIMER (0x%02x):\n", OMAR_LEDC_TIMER);
#endif
            

    }
}


