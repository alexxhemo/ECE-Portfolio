#ifndef UI_ADC_H_
#define UI_ADC_H_

#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif
/**
 * @file ui_adc.h
 * @author Arturo Salinas
 * @date 2025-10-27
 * @brief ADCLab - Analog-to-Digital Converter Voltmeter Lab Module
 *
 * Simple voltmeter that measures voltages between 0-3.3V with ~0.8mV
 * resolution. Connects to potentiometer on AIN8 (PORTE0). Reads ADC every 1
 * second using task-based timer (no delays). Uses full 12-bit ADC resolution.
 */

#include <stdbool.h>

/**
 * @brief Initialize ADCLab module
 */
void adc_lab_init(void);

/**
 * @brief Process ADCLab (call from main loop)
 *
 * Non-blocking - handles ADC reads and voltage display
 */
void adc_lab_process(void);

/**
 * @brief Display ADCLab welcome banner
 */
void adc_lab_show_welcome(void);

/**
 * @brief Check if ADCLab is currently active
 * @return true if ADCLab is running, false if user exited
 */
bool adc_lab_is_active(void);

/**
 * @brief Exit ADCLab and return to AOS
 */
void adc_lab_exit(void);

#endif /* UI_ADC_H_ */
