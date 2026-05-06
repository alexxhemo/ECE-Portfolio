/**
 * @file ui_eeprom.h
 * @author Arturo Salinas
 * @date 2025-10-27
 * @brief EEPROMLab - Password Manager using EEPROM
 *
 * Interactive EEPROM application for:
 * - Reading/displaying stored password from EEPROM
 * - Verifying user-entered password
 * - Updating password via button press (PB5)
 * - Providing visual feedback (LED on PB3)
 *
 * Architecture: Non-blocking task-based design
 * - eeprom_lab_init(): Initialize EEPROM system and read stored password
 * - eeprom_lab_process(): Handle input, verify, and update cycles
 * - eeprom_lab_show_welcome(): Display welcome banner and instructions
 * - eeprom_lab_exit(): Clean up and return to AOS
 * - eeprom_lab_is_active(): Query if lab is currently running
 */

#ifndef UI_EEPROM_H_
#define UI_EEPROM_H_

#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif

#include <stdbool.h>
#include <stdint.h>

//================================
// EEPROM Configuration
//================================

/**
 * Password storage addresses in EEPROM
 * AVR128DB48 EEPROM: 256 bytes (0x00-0xFF)
 * Using first 4 bytes for default password, next 4 for current password
 */
#define EEPROM_DEFAULT_PASSWORD_ADDR 0x00 // Stored at 0x00-0x03
#define EEPROM_CURRENT_PASSWORD_ADDR 0x04 // Stored at 0x04-0x07
#define PASSWORD_LENGTH 4

//================================
// EEPROMLab Interface Functions
//================================

/**
 * @brief Initialize EEPROMLab
 *
 * Sets up EEPROM system and reads stored password from memory.
 * Must be called before using other EEPROMLab functions.
 */
void eeprom_lab_init(void);

/**
 * @brief Main EEPROMLab process function (non-blocking)
 *
 * Should be called regularly from main loop when EEPROMLab is active.
 * Handles:
 * - User password input via UART
 * - Password verification logic
 * - LED feedback
 * - Button monitoring for password update
 * - EXIT command handling
 */
void eeprom_lab_process(void);

/**
 * @brief Display welcome banner and instructions
 *
 * Shows:
 * - Application title
 * - Current password status
 * - Operating instructions
 * - How to use UPDATE mode
 * - How to exit
 */
void eeprom_lab_show_welcome(void);

/**
 * @brief Clean up and exit EEPROMLab
 *
 * Shuts down EEPROM operations and returns to AOS command mode.
 */
void eeprom_lab_exit(void);

/**
 * @brief Query if EEPROMLab is currently active
 *
 * @return true if EEPROMLab is running, false otherwise
 */
bool eeprom_lab_is_active(void);

#endif /* UI_EEPROM_H_ */
