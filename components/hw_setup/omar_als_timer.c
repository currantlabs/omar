/* Copyright (c) 2019 Currant Inc. All Rights Reserved.
 *
 * omar_als_timer.c - sets up a timer which fires once a second, turns off the leds briefly
 * and then takes a reading with the ambient light sensor.
 *
 */


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "esp_err.h"
#include "hw_setup.h"

static void pause(uint32_t timer_sel);
static void resume(uint32_t timer_sel);
static void timer_example_evt_task(void *arg);

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (9.0) // sample test interval for the first timer
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload

/*
 * A sample structure to pass events
 * from the timer interrupt handler to the main program.
 */
typedef struct {
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;

xQueueHandle timer_queue;

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
    if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        evt.type = TEST_WITH_RELOAD;
        TIMERG0.int_clr_timers.t0 = 1;
    } else {
        evt.type = -1; // not supported even type
    }

    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

    /* Now just send the event data back to the main program task */
    xQueueSendFromISR(timer_queue, &evt, NULL);
}

/*
 * Initialize selected timer of the timer group 0
 *
 * timer_idx - the timer number to initialize
 * auto_reload - should the timer auto reload on alarm?
 * timer_interval_sec - the interval of alarm to set
 */
static void example_tg0_timer_init(int timer_idx, 
    bool auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = auto_reload;
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr, 
        (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}

void timer_setup(void)
{
    timer_queue = xQueueCreate(10, sizeof(timer_event_t));
    example_tg0_timer_init(TIMER_0, TEST_WITH_RELOAD,    TIMER_INTERVAL0_SEC);
    xTaskCreate(timer_example_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);
}

static void pause(uint32_t timer_sel)
{
    ledc_mode_t speed_mode = OMAR_LEDC_SPEED_MODE;

    if (ledc_timer_pause(speed_mode, timer_sel) != ESP_OK) {
        printf("\t\t\t\t\t\t\t\t%s(): ledc_timer_pause(%d, %d) errored\n", 
               __func__,
               speed_mode,
               timer_sel);
    }

    
}

static void resume(uint32_t timer_sel)
{
    ledc_mode_t speed_mode = OMAR_LEDC_SPEED_MODE;

    if (ledc_timer_resume(speed_mode, timer_sel) != ESP_OK) {
        printf("\t\t\t\t\t\t\t\t%s(): ledc_timer_resume(%d, %d) errored\n", 
               __func__,
               speed_mode,
               timer_sel);
    }

    
}

static void timer_example_evt_task(void *arg)
{

    while (1) {
        timer_event_t evt;
        xQueueReceive(timer_queue, &evt, portMAX_DELAY);

        /* Print information that the timer reported an event */
        if (evt.type == TEST_WITH_RELOAD) {
            printf("\n\t\t\t\t\t\t\t\t  Example timer with auto reload\n");
        } else {
            printf("\n\t\t\t\t\t\t\t\t  UNKNOWN EVENT TYPE\n");
        }
        printf("\t\t\t\t\t\t\t\tGroup[%d], timer[%d] alarm event\n", evt.timer_group, evt.timer_idx);

        printf("\t\t\t\t\t\t\t\tPausing ledc timer select OMAR_LEDC_TIMER (0x%02x):\n", OMAR_LEDC_TIMER);
        pause(OMAR_LEDC_TIMER);
        vTaskDelay(1500 / portTICK_PERIOD_MS);
        printf("\t\t\t\t\t\t\t\tResuming ledc timer select OMAR_LEDC_TIMER (0x%02x):\n", OMAR_LEDC_TIMER);
        resume(OMAR_LEDC_TIMER);
            

    }
}


