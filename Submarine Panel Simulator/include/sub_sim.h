/*
 * @file sub_sim.h
 * @author Arturo Salinas
 * @date 2025-11-19
 * @brief Submarine Simulator Panel - Final Project
 *
 * A submarine simulator with the following features:
 * - Dive duration selection (0-60 minutes)
 * - Dive phase management (1/4, 1/2, 3/4, full)
 * - Circuit breaker simulation (O2, Rx, Main Power)
 * - LED status indicators (Y-pattern)
 * - LCD status display
 * - Sonar ping alarm system
 * - Event-driven dive scenarios
 */

#ifndef SUB_SIM_H_
#define SUB_SIM_H_

#include "aos_timer.h"
#include "circularbuff.h"
#include "dac.h"
#include "i2c_lib_S25.h"
#include "lcd.h"
#include "melody.h"
#include "uart.h"
#include "ui.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif
//================================
// Configuration Constants
//================================
#define SUB_SIM_MAX_DIVE_SECONDS 300
#define SUB_SIM_TICK_MS 1.0f // 1ms tick period
#define SUB_SIM_TICKS_PER_SECOND (1000.0f / SUB_SIM_TICK_MS)
#define SUB_SIM_RESPONSE_TIMEOUT_SECONDS 20

//================================
// Dive Phase States
//================================
typedef enum {
  DIVE_IDLE = 0,
  DIVE_SETUP,
  DIVE_REACTOR_START,
  DIVE_READY,
  DIVE_DESCENDING,
  DIVE_PHASE_25,
  DIVE_PHASE_50,
  DIVE_PHASE_75,
  DIVE_PHASE_100,
  DIVE_SURFACING,
  DIVE_COMPLETE,
  DIVE_FAILED
} dive_state_t;

//================================
// Public Interface
//================================

/**
 * @brief Initialize submarine simulator
 * Sets up GPIO, timers, LCD, and other peripherals
 */
void sub_sim_init(void);

/**
 * @brief Process submarine simulator (non-blocking)
 * Should be called regularly from main loop
 */
void sub_sim_process(void);

/**
 * @brief Check if submarine simulator is active
 * @return true if active, false otherwise
 */
bool sub_sim_is_active(void);

/**
 * @brief Display welcome screen
 */
void sub_sim_show_welcome(void);

/**
 * @brief Exit submarine simulator and return to AOS
 */
void sub_sim_exit(void);

/**
 * @brief RTC tick callback (called every 65.5ms)
 * This function should be registered with the RTC timer
 */
void sub_sim_tick_handler(void);

//================================
// GPIO Button Interface
//================================

/**
 * @brief Check if System Reset button is pressed
 */
bool sub_sim_button_sys_reset_pressed(void);

/**
 * @brief Check if O2 Circuit Breaker button is pressed
 */
bool sub_sim_button_o2_cb_pressed(void);

/**
 * @brief Check if Reactor Circuit Breaker button is pressed
 */
bool sub_sim_button_rx_cb_pressed(void);

/**
 * @brief Check if Reactor Startup (Main Power) button is pressed
 */
bool sub_sim_button_reactor_startup_pressed(void);

/**
 * @brief Check if Main Breaker button is pressed
 */
bool sub_sim_button_main_bkr_pressed(void);

//================================
// Status Display
//================================

/**
 * @brief Get current dive state
 */
dive_state_t sub_sim_get_state(void);

/**
 * @brief Get dive duration in seconds
 */
uint16_t sub_sim_get_dive_duration(void);

/**
 * @brief Get elapsed dive time in seconds
 */
uint32_t sub_sim_get_elapsed_time(void);

/**
 * @brief Initialize ADC for sensors
 */
void sub_sim_init_adc(void);

/**
 * @brief Read potentiometer value (0-4095)
 */
uint16_t sub_sim_read_pot(void);

/**
 * @brief Read Joystick X value (0-4095)
 */
uint16_t sub_sim_read_joy_x(void);

/**
 * @brief Read Joystick Y value (0-4095)
 */
uint16_t sub_sim_read_joy_y(void);

/**
 * @brief Read CO Sensor value (0-4095)
 */
uint16_t sub_sim_read_co(void);

/**
 * @brief Read NH3 Sensor value (0-4095)
 */
uint16_t sub_sim_read_nh3(void);

/**
 * @brief Read NO2 Sensor value (0-4095)
 */
uint16_t sub_sim_read_no2(void);

/**
 * @brief Print to LCD (line 1 and line 2)
 */
void sub_sim_lcd_print(const char *line1, const char *line2);

#endif /* SUB_SIM_H_ */
