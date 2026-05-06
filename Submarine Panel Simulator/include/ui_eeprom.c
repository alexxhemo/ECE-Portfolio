#define F_CPU 16000000UL
#include "ui_eeprom.h"
#include "uart.h"
#include "ui.h"
#include "aos_timer.h"
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <string.h>
#include <util/delay.h>

//================================
// EEPROM Variable Declaration
//================================

#define EEPROM_ADDR 0x00
#define BUFFER_SIZE 64
// Declare EEPROM storage location for password (4 bytes, no null terminator in
// EEPROM)
uint8_t EEMEM eeprom_stored_password[PASSWORD_LENGTH] = {'0', '0', '0', '0'};

//================================
// EEPROMLab State Variables
//================================

static volatile bool eepromlab_active = false;
static volatile uint8_t stored_password[PASSWORD_LENGTH + 1] = {
    0}; // +1 for null terminator
static volatile uint8_t input_password[PASSWORD_LENGTH + 1] = {
    0}; // +1 for null terminator
static volatile uint8_t input_index = 0;
static volatile uint8_t lab_mode = 0; // 0=verify, 1=update, 2=success

// Non-blocking timers (driven by TCA1 1ms tick)
static volatile uint16_t blink_timer_ms = 0;
static volatile uint8_t blink_remaining = 0; // number of toggles left

//================================
// External Functions
//================================

extern void aos_send(const char *str);
extern void aos_printf(const char *format, ...);

//================================
// EEPROM Read/Write Functions
//================================

/**
 * @brief Read password from EEPROM (4 bytes + null terminator)
 *
 * @param buffer Pointer to buffer to store password (must be 5 bytes)
 */
static void eeprom_read_password(uint8_t *buffer) {
  eeprom_read_block(buffer, eeprom_stored_password, PASSWORD_LENGTH);
  buffer[PASSWORD_LENGTH] = '\0'; // Ensure null termination
}

/**
 * @brief Write password to EEPROM (4 bytes only, no null terminator)
 *
 * @param buffer Pointer to password buffer (null-terminated string)
 */
static void eeprom_write_password(const uint8_t *buffer) {
  eeprom_update_block(buffer, eeprom_stored_password, PASSWORD_LENGTH);
}

//================================
// LED Control Functions
//================================

/**
 * @brief Initialize LED on PB3
 */
static void init_led(void) {
  PORTB.DIRSET = PIN3_bm; // PB3 as output
  PORTB.OUTSET = PIN3_bm; // LED off initially (active low)
}

static void led_off(void) { PORTB.OUTSET = PIN3_bm; }

//================================
// Button Monitoring
//================================


/**
 * @brief Initialize button on PB5
 */
static void init_button(void) {
  PORTB.DIRCLR = PIN5_bm;            // PB5 as input
  PORTB.PIN5CTRL = PORT_PULLUPEN_bm; // Enable pull-up
}

//================================
// EEPROMLab Public Functions
//================================

// 1ms tick handler for non-blocking timing
static void eeprom_tca1_handler(void) {
  if (blink_timer_ms > 0) {
    blink_timer_ms--;
    if (blink_timer_ms == 0 && blink_remaining > 0) {
      // Toggle LED and reload timer
      PORTB.OUTTGL = PIN3_bm;
      blink_remaining--;
      // Only reload if more toggles needed
      if (blink_remaining > 0) blink_timer_ms = 250; // 250ms interval
    }
  }
}

void eeprom_lab_init(void) {
  eepromlab_active = true;
  lab_mode = 0; // Start in verification mode

  // Initialize hardware
  init_led();
  init_button();

  // Read stored password from EEPROM
  eeprom_read_password((uint8_t *)stored_password);

  // Optional: display stored password once on startup (debug)
  // aos_printf("\r\nStored Password: %s\r\n", (const char*)stored_password);

  // Initialize input buffer
  input_index = 0;
  memset((void *)input_password, 0, PASSWORD_LENGTH + 1);

  // Start 1ms tick for non-blocking timing
  aos_tca1_register(eeprom_tca1_handler);
  aos_tca1_start_1ms();
}

typedef struct {
  char buffer[BUFFER_SIZE];
  volatile uint8_t head;
  volatile uint8_t tail;
  volatile uint8_t count;
} circular_buffer_eeprom_t;

volatile circular_buffer_eeprom_t rx_buffer = (circular_buffer_eeprom_t){0};

volatile uint8_t password_pos = 0;
volatile int mode = 0;
volatile int password_change_init = 0;
// NOTE: UART RX interrupt is handled centrally in main.c (and uart.c helpers).
// Defining a second ISR here caused multiple definition/linker errors.
// We now consume UART input via uart_receive_char() in the process loop.

ISR(PORTB_PORT_vect) {
  if (PORTB.INTFLAGS & PIN5_bm) {
    if (mode == 1) {
      mode = 0;
    } else {
      mode = 1;
      password_change_init = 1;
    }
  }

  PORTB.INTFLAGS |= PIN5_bm;
}

void eeprom_lab_process(void) {
  // Handle EXIT command
  char c;
  if (uart_receive_char(&c)) {
    // Echo mask during entry (unless success mode)
    if (lab_mode != 2 && c != '\r') {
      uart_send_char('*');
    }
    // Exit to AOS
    if (c == 'e' || c == 'E') {
      eeprom_lab_exit();
      return;
    }

    // Handle password entry
    if (c == '\r') {
      // Ignore empty submission
      if (password_pos == 0) {
        // prompt again
        aos_send("\r\nEnter Password: ");
      } else {
        input_password[password_pos] = '\0';
        password_pos = 0;

        uint8_t eep_pwd[PASSWORD_LENGTH + 1] = {0};
        eeprom_read_password((uint8_t *)eep_pwd);

        if (mode == 0) {
          // Verify mode
          if (strncmp((const char*)input_password, (const char*)eep_pwd, PASSWORD_LENGTH) == 0) {
            aos_send("\r\nPassword Correct\r\n");
            lab_mode = 2; // success
            // Turn LED solid ON for success (active low)
            PORTB.OUTCLR = PIN3_bm;
          } else {
            aos_send("\r\nPassword Incorrect. Try Again.\r\nEnter Password: ");
            // Schedule 3 blinks (6 toggles) at 250ms
            blink_remaining = 6;
            blink_timer_ms = 250;
          }
        } else if (mode == 1) {
          // Update mode: write first 4 chars
          eeprom_write_password((const uint8_t *)input_password);
          aos_send("\r\nPassword Updated. Enter Password: ");
          // Clear LED (off)
          PORTB.OUTSET = PIN3_bm;
          mode = 0; // back to verify
        }
      }
    } else if (password_pos < PASSWORD_LENGTH) {
      input_password[password_pos++] = (uint8_t)c;
    }
  }

  // If user pressed PB5 (mode toggle) via ISR flag
  if (password_change_init) {
    aos_send("\r\nChange Password: ");
    password_pos = 0;
    memset((void*)input_password, 0, PASSWORD_LENGTH + 1);
    password_change_init = 0;
  }
}

void eeprom_lab_show_welcome(void) {
  aos_send(
      "\r\n+-----------------------------------------------------------+\r\n");
  aos_send("|                      EEPROMLab                            |\r\n");
  aos_send("|                  PASSWORD MANAGER                         |\r\n");
  aos_send("+-----------------------------------------------------------+\r\n");
  aos_send("\r\nPassword Collected From EEPROM\r\n");
  aos_send("\r\nInstructions:\r\n");
  aos_send("  - Enter 4-character password to verify access\r\n");
  aos_send("  - Press PB5 button to change password\r\n");
  aos_send("  - Type E to exit to AOS\r\n");
  aos_send("\r\nEnter Password (4 characters): ");

  _delay_ms(100);
}

void eeprom_lab_exit(void) {
  // Disable EEPROM operations
  eepromlab_active = false;

  // Turn off LED
  led_off();

  aos_send("\r\nExiting EEPROMLab, returning to AOS...\r\n\r\n");
  // Stop 1ms tick
  aos_tca1_disable();
  aos_tca1_unregister();
}

bool eeprom_lab_is_active(void) { return eepromlab_active; }
