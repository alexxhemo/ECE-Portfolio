#ifndef UART_H_
#define UART_H_

/**
 * @file uart.h
 * @author Arturo Salinas
 * @date 2025
 * @brief Unified UART driver with interrupt-driven and stdio support
 *
 * This driver provides both modern non-blocking interrupt-driven functions
 * and legacy stdio compatibility for printf/scanf operations.
 */

#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif
/* Standard includes needed for interface */
#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>
// #include <avr/ioavr128db48.h>
#include "circularbuff.h"
#include <avr/interrupt.h>
#include <stdbool.h>

//================================
// Circular Buffer Configuration
//================================

/** @brief Size of internal UART circular buffers. Must be power of 2. */
#define UART_BUFFER_SIZE 64

//================================
// Convenience Macros
//================================

/** @brief Check if UART transmit buffer is full */
#define UART_TX_BUFFER_FULL() (uart_tx_free_space() == 0)

/** @brief Check if UART transmit buffer is empty */
#define UART_TX_BUFFER_EMPTY() (uart_tx_free_space() == UART_BUFFER_SIZE)

/** @brief Check if UART receive buffer is full */
#define UART_RX_BUFFER_FULL() (uart_rx_available() == UART_BUFFER_SIZE)

/** @brief Check if UART receive buffer is empty */
#define UART_RX_BUFFER_EMPTY() (uart_rx_available() == 0)

/**
 * @brief UART driver now uses the external circular buffer implementation
 * @note The circular buffer handles are managed internally by the driver.
 *       Users should use the provided UART API functions.
 */

//================================
// Unified UART Interface
//================================

/**
 * @defgroup uart_api UART Driver API
 * @brief Unified UART driver supporting both interrupt-driven and stdio
 * operations
 *
 * This driver provides a complete UART solution with two complementary
 * interfaces:
 *
 * ## Modern Interrupt-Driven API
 * - Non-blocking operations suitable for real-time applications
 * - Automatic buffering via circular buffers
 * - Functions: uart_send_char(), uart_send_string(), uart_receive_char()
 * - Status functions: uart_tx_free_space(), uart_rx_available()
 *
 * ## Legacy stdio Interface
 * - Blocking operations compatible with printf(), scanf(), etc.
 * - Line editing capabilities (backspace, ctrl+u, etc.)
 * - Automatic CR+LF conversion for terminal compatibility
 *
 * ## Usage Example
 * @code
 * #include "uart.h"
 *
 * int main(void) {
 *     // Initialize UART
 *     uart_init(3, 9600, F_CPU, NULL);
 *     sei(); // Enable global interrupts
 *
 *     // Use stdio interface
 *     printf("Hello World!\n");
 *
 *     // Use interrupt-driven interface
 *     uart_send_string("Non-blocking message\n");
 *
 *     char ch;
 *     if (uart_receive_char(&ch)) {
 *         // Process received character
 *     }
 *
 *     return 0;
 * }
 *
 * // Required ISRs (replace USART3 with your USART number)
 * ISR(USART3_RXC_vect) {
 *     uart_rx_isr_handler(USART3.RXDATAL);
 * }
 *
 * ISR(USART3_DRE_vect) {
 *     char ch;
 *     if (uart_tx_isr_handler(&ch)) {
 *         USART3.TXDATAL = ch;
 *     } else {
 *         USART3.CTRLA &= ~USART_DREIE_bm;
 *     }
 * }
 * @endcode
 *
 * @{
 */

/**
 * @brief Initialize UART with both interrupt-driven and stdio support
 *
 * This function sets up the specified USART peripheral for both modern
 * interrupt-driven communication and legacy stdio operations (printf/scanf).
 * It automatically enables the receive interrupt for the unified API.
 *
 * @param usart_num USART peripheral number (0, 1, 2, or 3)
 * @param baud_rate Desired baud rate (e.g., 9600, 115200)
 * @param f_clk_per Peripheral clock frequency in Hz (typically F_CPU)
 * @param stream If non-NULL, initializes this FILE stream. If NULL,
 *               stdin, stdout, and stderr are automatically configured.
 * @return Pointer to the initialized FILE stream for stdio operations
 * @note After calling this function, both interrupt-driven and stdio
 *       functions are ready to use. Global interrupts must be enabled
 *       separately with sei().
 */
FILE *uart_init(uint8_t usart_num, uint32_t baud_rate, uint32_t f_clk_per,
                FILE *stream);

//================================
// Modern Interrupt-Driven API
//================================

/**
 * @brief Send a single character (non-blocking, interrupt-driven)
 *
 * Queues a character for transmission via interrupt-driven I/O.
 * If successful, transmission will begin automatically when the
 * transmitter is ready.
 *
 * @param c Character to send (0-255)
 * @return true if character queued successfully, false if transmit buffer is
 * full
 * @note This function is non-blocking. Use uart_tx_free_space() to check
 *       buffer availability before calling if needed.
 */
bool uart_send_char(char c);

/**
 * @brief Send a null-terminated string (non-blocking, interrupt-driven)
 *
 * Queues as many characters as possible from the string into the transmit
 * buffer. Transmission occurs automatically via interrupts.
 *
 * @param str Null-terminated string to send (must not be NULL)
 * @return Number of characters successfully queued (0 to strlen(str))
 * @note If the return value is less than strlen(str), the transmit buffer
 *       became full. Call again later to send remaining characters.
 */
uint8_t uart_send_string(const char *str);

/**
 * @brief Receive a character (non-blocking)
 *
 * Retrieves one character from the receive buffer if available.
 * Characters are automatically received via interrupt service routine.
 *
 * @param data Pointer to store the received character (must not be NULL)
 * @return true if character was available and retrieved, false if receive
 * buffer is empty
 * @note This function is non-blocking. Use uart_rx_available() to check
 *       if data is available before calling if needed.
 */
bool uart_receive_char(char *data);

/**
 * @brief Check transmit buffer free space
 *
 * Returns the number of characters that can be queued for transmission
 * without blocking.
 *
 * @return Number of free slots in transmit buffer (0 to UART_BUFFER_SIZE)
 * @note Useful for flow control or checking if uart_send_char() will succeed
 */
uint8_t uart_tx_free_space(void);

/**
 * @brief Check receive buffer data availability
 *
 * Returns the number of characters waiting in the receive buffer.
 *
 * @return Number of characters available for reading (0 to UART_BUFFER_SIZE)
 * @note Useful for determining how many uart_receive_char() calls will succeed
 */
uint8_t uart_rx_available(void);

//================================
// ISR Integration Functions (call these from your ISRs)
//================================

/**
 * @brief Handle received character in RX complete ISR
 *
 * Call this function from your USART RX complete interrupt service routine
 * to properly queue received characters into the internal buffer.
 *
 * @param received_char Character received from USART.RXDATAL register
 * @note This function should be called immediately after reading RXDATAL
 *       to prevent data loss. It handles all buffer management automatically.
 *
 * Example usage in ISR:
 * @code
 * ISR(USART3_RXC_vect) {
 *     char ch = USART3.RXDATAL;
 *     uart_rx_isr_handler(ch);
 * }
 * @endcode
 */
void uart_rx_isr_handler(char received_char);

/**
 * @brief Handle transmit request in DRE ISR
 *
 * Call this function from your USART Data Register Empty interrupt service
 * routine to get the next character to transmit from the internal buffer.
 *
 * @param data_to_send Pointer to store next character for transmission (must
 * not be NULL)
 * @return true if character is available to send, false if transmit buffer is
 * empty
 * @note When this function returns false, you should disable the DRE interrupt
 *       to prevent continuous interrupts. The interrupt will be re-enabled
 *       automatically when new data is queued via uart_send_char().
 *
 * Example usage in ISR:
 * @code
 * ISR(USART3_DRE_vect) {
 *     char ch;
 *     if (uart_tx_isr_handler(&ch)) {
 *         USART3.TXDATAL = ch;
 *     } else {
 *         USART3.CTRLA &= ~USART_DREIE_bm; // Disable DRE interrupt
 *     }
 * }
 * @endcode
 */
bool uart_tx_isr_handler(char *data_to_send);

//================================
// Legacy stdio Interface (for printf compatibility)
//================================

/**
 * @brief Send one character via stdio interface (blocking)
 *
 * This function is used internally by printf() and other stdio functions.
 * It provides blocking transmission suitable for stdio operations.
 *
 * @param c Character to send (0-255)
 * @param stream FILE stream pointer (configured by uart_init())
 * @return Always returns 0 on success
 * @note This function blocks until the character can be sent. For non-blocking
 *       operation, use uart_send_char() instead. Newlines are automatically
 *       converted to CR+LF for terminal compatibility.
 */
int uart_putchar(char c, FILE *stream);

/**
 * @brief Receive one character via stdio interface (blocking)
 *
 * This function is used internally by scanf() and other stdio functions.
 * It provides blocking reception with line editing capabilities.
 *
 * @param stream FILE stream pointer (configured by uart_init())
 * @return Received character (0-255), _FDEV_EOF on end of file, or _FDEV_ERR on
 * error
 * @note This function blocks until a character is received. It includes line
 *       editing features like backspace, ctrl+u (kill line), etc. For
 *       non-blocking operation, use uart_receive_char() instead.
 */
int uart_getchar(FILE *stream);

//================================
// Low-Level Hardware Interface
//================================
/**
 * @brief Initialize USART hardware peripheral
 *
 * Low-level function to configure the specified USART peripheral.
 * Called internally by uart_init().
 *
 * @param usartnum USART peripheral number (0-5, depending on MCU)
 * @param baud_rate Desired baud rate in bits per second
 * @param f_clk_per Peripheral clock frequency in Hz
 * @return Pointer to USART_t structure, or NULL if usartnum is invalid
 * @note This function also configures the appropriate TX pin as output
 */
void *usart_init(uint8_t usartnum, uint32_t baud_rate, uint32_t f_clk_per);

/**
 * @brief Transmit a character directly to USART (blocking)
 *
 * Low-level function that writes a character directly to the USART
 * data register. Used internally by stdio functions.
 *
 * @param ptr Pointer to USART_t structure (from usart_init())
 * @param c Character to transmit
 * @note This function assumes the USART is ready for transmission
 */
void usart_transmit_data(void *ptr, char c);

/**
 * @brief Wait until USART is ready for transmission
 *
 * Low-level function that blocks until the USART transmit register
 * is empty and ready for new data.
 *
 * @param ptr Pointer to USART_t structure (from usart_init())
 * @note Used internally by stdio functions for blocking transmission
 */
void usart_wait_until_transmit_ready(void *ptr);

/**
 * @brief Receive a character directly from USART (blocking)
 *
 * Low-level function that blocks until a character is received
 * from the USART peripheral.
 *
 * @param ptr Pointer to USART_t structure (from usart_init())
 * @return Received character (0-255), _FDEV_EOF on framing error,
 *         or _FDEV_ERR on buffer overflow
 * @note Used internally by stdio functions. Handles error detection
 *       and clearing automatically.
 */
int usart_receive_data(void *ptr);

#ifdef __XC8__
#define _FDEV_EOF -2
#define _FDEV_ERR -1
#endif

/* Compatibility macro for loop_until_bit_is_set if not defined */
#ifndef loop_until_bit_is_set
#define loop_until_bit_is_set(sfr, bit)                                        \
  do {                                                                         \
  } while (bit_is_clear(sfr, bit))
#endif

#ifndef bit_is_clear
#define bit_is_clear(sfr, bit) (!(sfr & (1 << bit)))
#endif

/** @} */ // End of uart_api group

#endif /* UART_H_ */
