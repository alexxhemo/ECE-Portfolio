#ifndef UI_H_
#define UI_H_

/**
 * @file ui.h
 * @author Arturo Salinas
 * @date 2025-09-30
 * @brief Arturo's Operating System - Generic Debugging Interface
 *
 * A comprehensive debugging system that provides:
 * - Register inspection and modification
 * - Memory dump capabilities
 * - System status monitoring
 * - Hardware testing functions
 * - Non-blocking UART interface
 * - Legacy RTC alarm clock functionality
 */

#include "circularbuff.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif
//================================
// Arturo's OS Configuration
//================================
#define AOS_VERSION "v1.0"
#define CMD_BUFFER_SIZE 128
#define MAX_CMD_LENGTH 64

//================================
// Arturo's OS Interface
//================================

/**
 * @brief Initialize Arturo's Operating System
 *
 * Sets up command processing system, buffers, and internal state.
 * Must be called before using any other AOS functions.
 */
void ui_init(void);

/**
 * @brief Process UART commands (non-blocking)
 *
 * This function should be called regularly from the main loop.
 * It collects input from UART, builds command lines, and executes
 * any complete commands that have been received.
 */
void ui_process_commands(void);

/**
 * @brief Display Arturo's OS boot message and help
 *
 * Shows the fancy boot screen with system information and available commands.
 */
void ui_show_welcome(void);

/**
 * @brief Provide runtime system info to UI (for accurate banner)
 * @param f_cpu_hz CPU frequency in Hz
 * @param uart_baud Baud rate configured for UART console
 */
void ui_set_system_info(uint32_t f_cpu_hz, uint32_t uart_baud);

/**
 * @brief Reprint prompt and any partially typed input (after async output)
 */
void ui_reprompt(void);

//================================
// Utility Functions
//================================

/**
 * @brief Non-blocking printf replacement
 *
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
void aos_printf(const char *format, ...);

/**
 * @brief Send string via non-blocking UART
 *
 * @param str String to send
 */
void aos_send(const char *str);

#endif /* UI_H_ */
