#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "contiki.h"
#include "sys/rtimer.h"

/* Sensors */
#include "board-peripherals.h"
#include "buzzer.h"

PROCESS(assignment2, "Assignment 2");
AUTOSTART_PROCESSES(&assignment2);

/*-------------------------------  STATE   ----------------------------------*/
typedef enum {
    Idle = 0,
    Active // either buzzing or wait
} State;

static State current_state = Idle;
/*----------------------------  TIMER   -------------------------------------*/
static struct rtimer timer_rtimer;

static rtimer_clock_t motion_timeout_rtimer = RTIMER_SECOND / 2;
static rtimer_clock_t light_timeout_rtimer = RTIMER_SECOND / 2;
static int prev_lux_value = 0;
static int lux_change_threshold = 300;
static int buzzer_freq = 2093;
static int buzzer_duration = 30;
static int normal_room_light = 9000;
static int gyro_threshold = 20000;

static bool is_buzzer_playing = false;
static int buzzer_counter = 0;
/*---------------------------------------------------------------------------*/
static void init(void);
static void init_opt_reading(void);

static bool is_significant_motion(int, int, int, int, int, int);
static void do_mpu_reading(void);

static void check_active(void);
void do_rtimer_timeout(struct rtimer*, void*);

/*---------------------------------------------------------------------------*/
void do_rtimer_timeout(struct rtimer *timer, void *ptr) {
    switch(current_state) {
        case Idle:
            /* Re-arm rtimer, sensing at 2 Hz*/
            rtimer_set(&timer_rtimer, RTIMER_NOW() + motion_timeout_rtimer, 0,  do_rtimer_timeout, NULL);
            do_mpu_reading();
            break;
        case Active:
            /* Based on the docs, contiki-ng only supports one active rtimer, do the sensing at 2 Hz */
            rtimer_set(&timer_rtimer, RTIMER_NOW() + light_timeout_rtimer, 0, do_rtimer_timeout, NULL);
            check_active();
            init_opt_reading();
            break;
    }
}

static void check_active() {
    int value = opt_3001_sensor.value(0);

    if (value == CC26XX_SENSOR_READING_ERROR) {
        printf("Value reading error");
        return;
    }

    bool is_significant_change = abs(value - prev_lux_value) > lux_change_threshold * 100;
    printf("prev: %d current: %d\n", prev_lux_value, value);

    prev_lux_value = value;
    if (is_significant_change) {
        current_state = Idle;
        if (is_buzzer_playing) buzzer_stop();
        buzzer_counter = 0;
        prev_lux_value = normal_room_light;
        is_buzzer_playing = false;

        printf("I am in idle now\n");
        return;
    }

    if (buzzer_counter == 0) {
        if (is_buzzer_playing) { // turn off (Wait), buzzer has been playing more than 3
            buzzer_stop();
            is_buzzer_playing = false;
        } else { // turn on (Buzz)
            buzzer_start(buzzer_freq);
            is_buzzer_playing = true;
        }
    }

    buzzer_counter += 5;
    buzzer_counter %= buzzer_duration;
}

static void do_mpu_reading() {
    int gyro_x, gyro_y, gyro_z, acc_x, acc_y, acc_z;

    gyro_x = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_X);
    gyro_y = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Y);
    gyro_z = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Z);
    acc_x = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_X);
    acc_y = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_Y);
    acc_z = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_Z);

    if (is_significant_motion(gyro_x, gyro_y, gyro_z, acc_x, acc_y, acc_z)) {
        printf("I am in active now\n");
        current_state = Active;

        // set up the first value of light reading, by heuristic, normal room lux is around 9k
        prev_lux_value = normal_room_light;

        opt_3001_sensor.value(0);
        init_opt_reading();
    }
}

bool is_significant_motion(int x_gyro, int y_gyro, int z_gyro, int x_acc, int y_acc, int z_acc) {
    /* based on heuristic, it will be considered a significant motion (i.e. it's moving pretty fast)
     * if either one of the |gyro value| >= 10000 (i.e. 100 deg/sec). The accelerometer in most of the cases
     * is just a proxy value of the gyrometer (it is directly correlated) and tbh, it's not as sensitive
     * as the gyrometer.
     */

    if (abs(x_gyro) >= gyro_threshold ||
        abs(y_gyro) >= gyro_threshold ||
        abs(z_gyro) >= gyro_threshold
    ) {
        printf("%d %d %d\n", abs(x_gyro), y_gyro, z_gyro);
        return true;
    }

    return false;
}
/*---------------------------------------------------------------------------*/

static void init_opt_reading(void) {
    SENSORS_ACTIVATE(opt_3001_sensor); // init light sensor
}

static void init(void) {
    init_opt_reading();
    mpu_9250_sensor.configure(SENSORS_ACTIVE, MPU_9250_SENSOR_TYPE_ALL); // init IMU sensor
    buzzer_init(); // init buzzer
}

PROCESS_THREAD(assignment2, ev, data) {
    PROCESS_BEGIN();
    init();

    while(1) {
        // initially at idle state
        rtimer_set(&timer_rtimer, RTIMER_NOW() + motion_timeout_rtimer, 0,  do_rtimer_timeout, NULL);
        PROCESS_YIELD();
    }

    PROCESS_END();
}
