/*
 * @file uart.c
 * @author Arturo Salinas
 * @date 2025
 */

#define RX_BUFSIZE 80 /* Size of internal line buffer used by uart_getchar()*/

#include "uart.h"
#include "circularbuff.h"
#include <string.h> /* Only needed for string operations in implementation */

//================================
// Global State
//================================
/* Circular buffer storage arrays */

static uint8_t tx_buffer_storage[UART_BUFFER_SIZE];
static uint8_t rx_buffer_storage[UART_BUFFER_SIZE];

/* Circular buffer handles */

static cbuf_handle_t uart_tx_buffer = NULL;
static cbuf_handle_t uart_rx_buffer = NULL;

/* Currently configured USART (for ISRs) */
static USART_t *active_usart = NULL;

//================================
// Internal Buffer Operations
//================================
static bool buffer_put(cbuf_handle_t buf, char data) {
  if (buf == NULL)
    return false;
  return (circular_buf_try_put(buf, (uint8_t)data) == 0);
}

static bool buffer_get(cbuf_handle_t buf, char *data) {
  if (buf == NULL || data == NULL)
    return false;
  uint8_t temp;
  if (circular_buf_get(buf, &temp) == 0) {
    *data = (char)temp;
    return true;
  }
  return false;
}

//================================
// Modern Interrupt-Driven API Implementation
//================================

bool uart_send_char(char c) {
  bool success = buffer_put(uart_tx_buffer, c);
  if (success && active_usart) {
    // Enable DRE interrupt to start transmission
    active_usart->CTRLA |= USART_DREIE_bm;
  }
  return success;
}

uint8_t uart_send_string(const char *str) {
  uint8_t count = 0;
  while (*str && uart_send_char(*str)) {
    str++;
    count++;
  }
  return count;
}

bool uart_receive_char(char *data) { return buffer_get(uart_rx_buffer, data); }

uint8_t uart_tx_free_space(void) {
  if (uart_tx_buffer == NULL)
    return 0;
  return (uint8_t)(circular_buf_capacity(uart_tx_buffer) -
                   circular_buf_size(uart_tx_buffer));
}

uint8_t uart_rx_available(void) {
  if (uart_rx_buffer == NULL)
    return 0;
  return (uint8_t)circular_buf_size(uart_rx_buffer);
}

//================================
// ISR Helper Functions (called from main.c ISRs)
//================================

void uart_rx_isr_handler(char received_char) {
  buffer_put(uart_rx_buffer, received_char);
}

bool uart_tx_isr_handler(char *data_to_send) {
  return buffer_get(uart_tx_buffer, data_to_send);
}

//================================
// Legacy stdio Implementation
//================================

#ifdef __XC8__
static FILE uartFile = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, F_PERM);
/* on gcc, we can put stream-specific udata in the FILE struct.  XC8 doesn't
   allow that, so we have to create a global udata which means that we can
   have only one stream at a time
 */
static void *_udata;
#define fdev_set_udata(stream, u) _udata = u
#define fdev_get_udata(stream) _udata
#else
static FILE uartFile =
    FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);
#endif

/* the following functions must be implemented by the MCU specific USART code
void *usart_init(uint8_t, uint32_t, uint32_t);
void usart_transmit_data(void *, char);
void usart_wait_until_transmit_ready(void *);
int usart_receive_data(void *ptr);
*/

/*
 * Initialize the UART to 9600 Bd, tx/rx, 8N1.
 */
FILE *uart_init(uint8_t usartnum, uint32_t baud_rate, uint32_t f_clk_per,
                FILE *stream) {
  if (stream) {
    *stream = uartFile;
  } else {
    stdout = &uartFile;
    stdin = &uartFile;
    stderr = &uartFile;
    stream = &uartFile;
  }

  void *usart = usart_init(usartnum, baud_rate, f_clk_per);
  fdev_set_udata(stream, usart);

  // Store active USART for interrupt-driven functions
  active_usart = (USART_t *)usart;

  // Initialize circular buffers
  uart_tx_buffer = circular_buf_init(tx_buffer_storage, UART_BUFFER_SIZE);
  uart_rx_buffer = circular_buf_init(rx_buffer_storage, UART_BUFFER_SIZE);

  // Enable receive interrupt for interrupt-driven API
  if (active_usart) {
    active_usart->CTRLA |= USART_RXCIE_bm;
  }

  return stream;
}

/*
 * Send character c down the UART Tx, wait until tx holding register
 * is empty.
 */
int uart_putchar(char c, FILE *stream) {
  if (c == '\a') {
    fputs("*ring*\n", stderr);
    return 0;
  }

  if (c == '\n') {
    uart_putchar('\r', stream);
  }

  void *usart = fdev_get_udata(stream);
  usart_wait_until_transmit_ready(usart);
  usart_transmit_data(usart, c);

  return 0;
}

/*
 * Receive a character from the UART Rx.
 *
 * This features a simple line-editor that allows to delete and
 * re-edit the characters entered, until either CR or NL is entered.
 * Printable characters entered will be echoed using uart_putchar().
 *
 * Editing characters:
 *
 * . \b (BS) or \177 (DEL) delete the previous character
 * . ^u kills the entire input buffer
 * . ^w deletes the previous word
 * . ^r sends a CR, and then reprints the buffer
 * . \t will be replaced by a single space
 *
 * All other control characters will be ignored.
 *
 * The internal line buffer is RX_BUFSIZE (80) characters long, which
 * includes the terminating \n (but no terminating \0).  If the buffer
 * is full (i. e., at RX_BUFSIZE-1 characters in order to keep space for
 * the trailing \n), any further input attempts will send a \a to
 * uart_putchar() (BEL character), although line editing is still
 * allowed.
 *
 * Input errors while talking to the UART will cause an immediate
 * return of -1 (error indication).  Notably, this will be caused by a
 * framing error (e. g. serial line "break" condition), by an input
 * overrun, and by a parity error (if parity was enabled and automatic
 * parity recognition is supported by hardware).
 *
 * Successive calls to uart_getchar() will be satisfied from the
 * internal buffer until that buffer is emptied again.
 */
int uart_getchar(FILE *stream) {
  uint8_t c;
  char *cp, *cp2;
  static char b[RX_BUFSIZE];
  static char *rxp;

  if (rxp == 0) {
    for (cp = b;;) {
      void *usart = fdev_get_udata(stream);
      c = usart_receive_data(usart);

      /* behaviour similar to Unix stty ICRNL */
      if (c == '\r')
        c = '\n';
      if (c == '\n') {
        *cp = c;
        uart_putchar(c, stream);
        rxp = b;
        break;
      } else if (c == '\t')
        c = ' ';

      if ((c >= (uint8_t)' ' && c <= (uint8_t)'\x7e') || c >= (uint8_t)'\xa0') {
        if (cp == b + RX_BUFSIZE - 1)
          uart_putchar('\a', stream);
        else {
          *cp++ = c;
          uart_putchar(c, stream);
        }
        continue;
      }

      switch (c) {
      case 'c' & 0x1f:
        return -1;

      case '\b':
      case '\x7f':
        if (cp > b) {
          uart_putchar('\b', stream);
          uart_putchar(' ', stream);
          uart_putchar('\b', stream);
          cp--;
        }
        break;

      case 'r' & 0x1f:
        uart_putchar('\r', stream);
        for (cp2 = b; cp2 < cp; cp2++)
          uart_putchar(*cp2, stream);
        break;

      case 'u' & 0x1f:
        while (cp > b) {
          uart_putchar('\b', stream);
          uart_putchar(' ', stream);
          uart_putchar('\b', stream);
          cp--;
        }
        break;

      case 'w' & 0x1f:
        while (cp > b && cp[-1] != ' ') {
          uart_putchar('\b', stream);
          uart_putchar(' ', stream);
          uart_putchar('\b', stream);
          cp--;
        }
        break;
      }
    }
  }

  c = *rxp++;
  if (c == '\n')
    rxp = 0;

  return c;
}

void *usart_init(uint8_t usartnum, uint32_t baud_rate, uint32_t f_clk_per) {
  USART_t *usart;

  if (usartnum == 0) {
    usart = &USART0;
    // enable USART0 TX pin
    PORTA.DIRSET = PIN0_bm;
  } else if (usartnum == 1) {
    usart = &USART1;
    // enable USART1 TX pin
    PORTC.DIRSET = PIN0_bm;
  } else if (usartnum == 2) {
    usart = &USART2;
    // enable USART2 TX pin
    PORTF.DIRSET = PIN0_bm;
  }
#ifdef USART3
  else if (usartnum == 3) {
    usart = &USART3;
    // enable USART3 TX pin
    PORTB.DIRSET = PIN0_bm;
  }
#endif
#ifdef USART4
  else if (usartnum == 4) {
    usart = &USART4;
    // enable USART4 TX pin
    PORTE.DIRSET = PIN0_bm;
  }
#endif
#ifdef USART5
  else if (usartnum == 5) {
    usart = &USART5;
    // enable USART5 TX pin
    PORTG.DIRSET = PIN0_bm;
  }
#endif
  else {
    usart = NULL;
  }
  // 1. Set the baud rate USARTn.BAUD
  usart->BAUD = (((float)f_clk_per) * 64.0 / (16.0 * (float)baud_rate)) + 0.5;
  // 2. Set the frame format and mode of operation (USARTn.CTRLC)
  usart->CTRLC = USART_CHSIZE_8BIT_gc; /* 8 data bits, no parity, 1 stop bit */
  // 3. Configure the TXD pin as an output (done above)
  // 4. Enable the transmitter and the receiver (USARTn.CTRLB)
  usart->CTRLB |= (USART_RXEN_bm | USART_TXEN_bm); /* tx/rx enable */
  return usart;
}

void usart_transmit_data(void *ptr, char c) {
  USART_t *usart = (USART_t *)ptr;
  usart->TXDATAL = c;
}

void usart_wait_until_transmit_ready(void *ptr) {
  USART_t *usart = (USART_t *)ptr;
  loop_until_bit_is_set(usart->STATUS, USART_DREIF_bp);
}

int usart_receive_data(void *ptr) {
  USART_t *usart = (USART_t *)ptr;

  uint8_t c;
  loop_until_bit_is_set(
      usart->STATUS,
      USART_RXCIF_bp); // Loop until the Receive Complete Interrupt Flag is set
  char rcv_status = usart->RXDATAH; // Receive the High Byte of the data.

  if (rcv_status & USART_FERR_bm) { // Frame Error
    c = usart->RXDATAL;             /* clear error by reading data */
    return _FDEV_EOF;
  }
  if (rcv_status & USART_BUFOVF_bm) { // Buffer Overflow
    c = usart->RXDATAL;               /* clear error by reading data */
    return _FDEV_ERR;
  }
  c = usart->RXDATAL; // c = the lower byte of the received data
  return c;
}
