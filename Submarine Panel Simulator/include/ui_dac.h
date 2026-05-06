#ifndef UI_DAC_H_
#define UI_DAC_H_

#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif
/**
 * @file ui_dac.h
 * @author Arturo Salinas
 * @date 2025-10-27
 * @brief DACLab - Programmable Waveform Generator Lab Module
 *
 * Interactive waveform generator with:
 * - Variable frequency control (10-1000 Hz)
 * - Variable amplitude control (10-100%)
 * - Real-time status reporting
 * - Non-blocking UART interface
 */

#include <stdbool.h>

/**
 * @brief Initialize DACLab module
 */
void dac_lab_init(void);

/**
 * @brief Process DACLab input (call from main loop)
 *
 * Non-blocking - processes one character at a time
 */
void dac_lab_process(void);

/**
 * @brief Display DACLab welcome banner
 */
void dac_lab_show_welcome(void);

/**
 * @brief Display status every 5 seconds
 */
void dac_lab_status_display(void);

/**
 * @brief Check if DACLab is currently active
 * @return true if DACLab is running, false if user exited
 */
bool dac_lab_is_active(void);

/**
 * @brief Check if status display should be suppressed
 * @return true if we should suppress status display (welcome being shown)
 */
bool dac_lab_should_suppress_status(void);

/**
 * @brief Exit DACLab and return to AOS
 */
void dac_lab_exit(void);

#endif /* UI_DAC_H_ */
