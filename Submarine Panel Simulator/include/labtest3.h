/**
 * @file labtest3.h
 * @author Arturo Salinas
 * @date 2025-10-27
 * @brief LabTest 3 - Adaptive LED Pattern & Potentiometer-Controlled DAC
 *
 * Multi-part lab implementing:
 * 1. Adaptive LED animation modes based on potentiometer voltage
 * 2. Potentiometer-controlled DAC waveform (sine wave with sweep capability)
 * 3. EEPROM persistence for mode and frequency settings
 *
 * Architecture: Non-blocking task-based design compatible with AOS
 * - labtest3_init(): Initialize hardware and load EEPROM settings
 * - labtest3_process(): Main non-blocking process loop
 * - labtest3_show_welcome(): Display welcome banner
 * - labtest3_is_active(): Query if lab is currently running
 * - labtest3_exit(): Clean up and return to AOS
 */

#ifndef LABTEST3_H_
#define LABTEST3_H_

#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif
#include <stdbool.h>
#include <stdint.h>

//================================
// LabTest3 Configuration
//================================

/** ADC Channel for potentiometer (AIN8 = PORTE0) */
#define LABTEST3_ADC_CHANNEL AIN8

/** ADC resolution: 12-bit (0-4095) */
#define LABTEST3_ADC_RESOLUTION 12

/** Number of ADC readings to average (10 readings every 10ms = 100ms total) */
#define LABTEST3_ADC_AVERAGE_COUNT 10

/** Update period for averaged voltage (milliseconds) */
#define LABTEST3_UPDATE_PERIOD_MS 100

/** UART output period (milliseconds) */
#define LABTEST3_UART_OUTPUT_PERIOD_MS 3000

/** EEPROM storage addresses */
#define LABTEST3_EEPROM_ADDR 0x10

//================================
// LED Animation Modes
//================================

typedef enum {
  LABTEST3_MODE_OFF = 0,       ///< All LEDs off
  LABTEST3_MODE_CHASE = 1,     ///< Night rider chase effect
  LABTEST3_MODE_BLINK_8HZ = 2, ///< All LEDs blink at 8Hz
  LABTEST3_MODE_PWM = 3        ///< Single LED with PWM brightness
} labtest3_led_mode_t;

//================================
// DAC Waveform Modes
//================================

typedef enum {
  LABTEST3_DAC_FIXED = 0, ///< Fixed frequency mode
  LABTEST3_DAC_SWEEP = 1  ///< Frequency sweep mode (10Hz to 100Hz)
} labtest3_dac_mode_t;

//================================
// LabTest3 Interface Functions
//================================

/**
 * @brief Initialize LabTest3 module
 *
 * Sets up:
 * - ADC for potentiometer reading
 * - Timers for sampling and LED animation
 * - DAC for waveform output
 * - PB5/PB2 button interrupts
 * - Loads saved settings from EEPROM
 */
void labtest3_init(void);

/**
 * @brief Main LabTest3 process function (non-blocking)
 *
 * Should be called regularly from main loop.
 * Handles:
 * - ADC sampling and averaging
 * - LED animation updates
 * - DAC waveform generation
 * - UART status reporting
 */
void labtest3_process(void);

/**
 * @brief Display welcome banner and instructions
 */
void labtest3_show_welcome(void);

/**
 * @brief Query if LabTest3 is currently active
 *
 * @return true if LabTest3 is running, false otherwise
 */
bool labtest3_is_active(void);

/**
 * @brief Clean up and exit LabTest3
 *
 * Saves current settings to EEPROM before exiting.
 */
void labtest3_exit(void);

#endif /* LABTEST3_H_ */
