/**
 * @file ui.c
 * @author Arturo Salinas
 * @date 2025-09-30
 * @brief Arturo's Operating System - Generic Debugging Interface
 *
 * A comprehensive debugging system that provides:
 * - Register inspection and modification
 * - Memory dump capabilities
 * - System status monitoring
 * - Custom command framework
 * - Non-blocking UART interface
 */
#define __AVR_AVR128DB48__
#include "ui.h"
#include "sub_sim.h"
#include "melody.h"
#include "circularbuff.h"
#include "dac.h"
#include "uart.h"
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//================================
// External Variables from main.c
//================================
extern void update_tca0_frequency(uint32_t new_freq);
extern void update_sine_wave_scaled(int amplitude_percent);

//================================
//================================
extern volatile uint32_t freq;
extern volatile uint16_t sine_index;
extern volatile int amplitude_percent;
extern volatile float amplitude_scaler;
extern volatile int dac_update;
extern const uint16_t sine_table[64];

//================================
// Arturo's OS Configuration
//================================
#define AOS_VERSION "v1.0"
#define AOS_BUILD_DATE __DATE__
#define CMD_BUFFER_SIZE 128
#define MAX_CMD_LENGTH 64
#define OUTPUT_BUFFER_SIZE 256

//================================
// Non-blocking output system
//================================
static char output_buffer[OUTPUT_BUFFER_SIZE];
static uint32_t aos_f_cpu_hz = 0;
static uint32_t aos_uart_baud = 0;

// Non-blocking printf replacement
void aos_printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(output_buffer, sizeof(output_buffer), format, args);
  va_end(args);
  // Ensure entire formatted line is sent (blocking until queued)
  const char *p = output_buffer;
  while (*p) {
    if (!uart_send_char(*p)) {
      // Wait until there is space in TX buffer
      // Simple busy-wait; acceptable for debug output
      continue;
    }
    p++;
  }
}

// Send string with non-blocking UART
void aos_send(const char *str) {
  // Ensure entire string is sent (blocking until queued)
  const char *p = str;
  while (*p) {
    if (!uart_send_char(*p)) {
      continue;
    }
    p++;
  }
}

// Forward declaration (defined after static input buffers)
void ui_reprompt(void);
void ui_set_system_info(uint32_t f_cpu_hz, uint32_t uart_baud) {
  aos_f_cpu_hz = f_cpu_hz;
  aos_uart_baud = uart_baud;
}


//================================
// Internal State
//================================
static uint8_t cmd_line_storage[CMD_BUFFER_SIZE];
static cbuf_handle_t cmd_line_buffer = NULL;
static char current_cmd_line[MAX_CMD_LENGTH];
static uint8_t current_cmd_index = 0;

// Reprint prompt and current input buffer (after async output)
void ui_reprompt(void) {
  aos_send("AOS> ");
  // Echo any currently typed characters again
  for (uint8_t i = 0; i < current_cmd_index; i++) {
    uart_send_char(current_cmd_line[i]);
  }
}

//================================
// Register and Memory Access Structures
//================================
typedef struct {
  const char *name;
  volatile uint8_t *address;
  const char *description;
} register_info_t;

typedef struct {
  const char *peripheral_name;
  const register_info_t *registers;
  uint8_t count;
} peripheral_info_t;

//================================
// Register Definitions for AVR128DB48
//================================
static const register_info_t rtc_registers[] = {
    {"CTRLA", &RTC.CTRLA, "Control A"},
    {"STATUS", &RTC.STATUS, "Status"},
    {"INTCTRL", &RTC.INTCTRL, "Interrupt Control"},
    {"INTFLAGS", &RTC.INTFLAGS, "Interrupt Flags"},
    {"TEMP", &RTC.TEMP, "Temporary"},
    {"DBGCTRL", &RTC.DBGCTRL, "Debug Control"},
    {"CLKSEL", &RTC.CLKSEL, "Clock Select"},
    {"CNTL", &RTC.CNTL, "Counter Low"},
    {"CNTH", &RTC.CNTH, "Counter High"},
    {"PERL", &RTC.PERL, "Period Low"},
    {"PERH", &RTC.PERH, "Period High"},
    {"CMPL", &RTC.CMPL, "Compare Low"},
    {"CMPH", &RTC.CMPH, "Compare High"}};

static const register_info_t usart3_registers[] = {
    {"RXDATAL", &USART3.RXDATAL, "Receive Data Low"},
    {"RXDATAH", &USART3.RXDATAH, "Receive Data High"},
    {"TXDATAL", &USART3.TXDATAL, "Transmit Data Low"},
    {"TXDATAH", &USART3.TXDATAH, "Transmit Data High"},
    {"STATUS", &USART3.STATUS, "Status"},
    {"CTRLA", &USART3.CTRLA, "Control A"},
    {"CTRLB", &USART3.CTRLB, "Control B"},
    {"CTRLC", &USART3.CTRLC, "Control C"},
    {"BAUDL", &USART3.BAUDL, "Baud Low"},
    {"BAUDH", &USART3.BAUDH, "Baud High"}};

static const register_info_t portd_registers[] = {
    {"DIR", &PORTD.DIR, "Direction"},
    {"DIRSET", &PORTD.DIRSET, "Direction Set"},
    {"DIRCLR", &PORTD.DIRCLR, "Direction Clear"},
    {"DIRTGL", &PORTD.DIRTGL, "Direction Toggle"},
    {"OUT", &PORTD.OUT, "Output Value"},
    {"OUTSET", &PORTD.OUTSET, "Output Set"},
    {"OUTCLR", &PORTD.OUTCLR, "Output Clear"},
    {"OUTTGL", &PORTD.OUTTGL, "Output Toggle"},
    {"IN", &PORTD.IN, "Input Value"}};

static const register_info_t dac0_registers[] = {
    {"CTRLA", &DAC0.CTRLA, "DAC0 Control A"},
    {"DAC0REF", &VREF.DAC0REF, "Reference Voltage"},
    {"DATAL", &DAC0.DATAL, "DAC Lower Data[7:6]"},
    {"DATAH", &DAC0.DATAH, "DAC Upper Data [9:2]"}};

static const register_info_t tca0_registers[] = {
    {"CTRLA", &TCA0.SINGLE.CTRLA, "Control A"},
    {"CTRLB", &TCA0.SINGLE.CTRLB, "Control B"},
    {"CTRLC", &TCA0.SINGLE.CTRLC, "Control C"},
    {"CTRLD", &TCA0.SINGLE.CTRLD, "Control D"},
    {"CTRLECLR", &TCA0.SINGLE.CTRLECLR, "Control E Clear"},
    {"CTRLESET", &TCA0.SINGLE.CTRLESET, "Control E Set"},
    {"CTRLFCLR", &TCA0.SINGLE.CTRLFCLR, "Control F Clear"},
    {"CTRLFSET", &TCA0.SINGLE.CTRLFSET, "Control F Set"},
    {"EVCTRL", &TCA0.SINGLE.EVCTRL, "Event Control"},
    {"INTCTRL", &TCA0.SINGLE.INTCTRL, "Interrupt Control"},
    {"INTFLAGS", &TCA0.SINGLE.INTFLAGS, "Interrupt Flags"},
    {"DBGCTRL", &TCA0.SINGLE.DBGCTRL, "Debug Control"}};

static const peripheral_info_t peripherals[] = {
    {"RTC", rtc_registers, sizeof(rtc_registers) / sizeof(rtc_registers[0])},
    {"USART3", usart3_registers,
     sizeof(usart3_registers) / sizeof(usart3_registers[0])},
    {"PORTD", portd_registers,
     sizeof(portd_registers) / sizeof(portd_registers[0])},
    {"TCA0", tca0_registers,
     sizeof(tca0_registers) / sizeof(tca0_registers[0])},
    {"DAC0", dac0_registers,
     sizeof(dac0_registers) / sizeof(dac0_registers[0])}};

//================================
// Command Structure
//================================
typedef struct {
  const char *name;
  void (*handler)(const char *params);
  const char *help_text;
} command_t;

//================================
// Forward Declarations - Arturo's OS Commands
//================================
static void cmd_help(const char *params);
static void cmd_sysinfo(const char *params);
static void cmd_regs(const char *params);
static void cmd_read(const char *params);
static void cmd_write(const char *params);
static void cmd_dump(const char *params);
static void cmd_peek(const char *params);
static void cmd_poke(const char *params);
static void cmd_reset(const char *params);
static void cmd_uart_test(const char *params);
static void cmd_gpio_test(const char *params);
static void cmd_adc_read(const char *params);
static void cmd_timer_info(const char *params);
static void cmd_run_subsim(const char *params);
static void cmd_play_music(const char *params);

static void queue_command_line(const char *cmd_line);
static void execute_next_command(void);

//================================
// Arturo's Operating System Command Table
//================================
static const command_t commands[] = {
    // System Commands
    {"HELP", cmd_help, "HELP                    - Show all commands"},
    {"SYSINFO", cmd_sysinfo,
     "SYSINFO                 - Show system information"},
    {"RESET", cmd_reset, "RESET                   - Software reset"},

    // Register and Memory Commands
    {"REGS", cmd_regs,
     "REGS [peripheral]       - Show registers (RTC, USART3, PORTD, TCA0)"},
    {"READ", cmd_read,
     "READ <address>          - Read from memory address (hex)"},
    {"WRITE", cmd_write,
     "WRITE <address> <value> - Write to memory address (hex)"},
    {"DUMP", cmd_dump, "DUMP <start> [length]   - Memory dump (hex addresses)"},
    {"PEEK", cmd_peek, "PEEK <address>          - Peek at memory location"},
    {"POKE", cmd_poke, "POKE <address> <value>  - Poke value to memory"},

    // Hardware Testing
    {"UART", cmd_uart_test,
     "UART                    - Test UART functionality"},
    {"GPIO", cmd_gpio_test,
     "GPIO <port> <pin> <val> - Test GPIO (D,B,C pin 0-7, val 0/1)"},
    {"ADC", cmd_adc_read, "ADC                     - Read ADC sensors"},
    {"TIMER", cmd_timer_info, "TIMER                   - Show timer status"},

    // Labs & Applications
    {"SUBSIM", cmd_run_subsim,
     "SUBSIM                  - Launch Submarine Simulator"},
    {"PLAYMUSIC", cmd_play_music,
     "PLAYMUSIC               - Play MIDI melody on DAC (PD6)"},
    {NULL, NULL, NULL} // End marker
};

//================================
// Public Interface Implementation
//================================

void ui_init(void) {
  cmd_line_buffer = circular_buf_init(cmd_line_storage, CMD_BUFFER_SIZE);
  current_cmd_index = 0;
}

void ui_process_commands(void) {
  // Process ONE character from UART input (non-blocking, single iteration)
  char ch;
  
  // Static variables for debug display in AOS menu
  static uint32_t last_debug_update = 0;
  static uint8_t debug_page = 0;
  static bool last_o2_state = false;
  // Use TCA1 tick from sub_sim (or just a simple counter if not available, but we have aos_timer)
  // We can use a simple loop counter or read TCA1 if exposed. 
  // Let's use a simple counter for now or assume this is called frequently.
  // Better: use a static counter that increments on each call, assuming main loop is fast.
  // Or use RTC if available. Let's use a simple counter for simplicity as we don't have global tick exposed easily here.
  static uint16_t update_prescaler = 0;

  if (uart_receive_char(&ch)) {
    // Got one character - process it immediately
    // Check if we're in SubSim mode
    if (sub_sim_is_active()) {
      // In SubSim mode - let SubSim handle it (should not reach here)
      // because main loop calls sub_sim_process() instead
      return;
    } else {
      // Normal AOS command mode - accumulate command line
      uart_send_char(ch);

      if (ch == '\n' || ch == '\r') {
        if (current_cmd_index > 0) {
          current_cmd_line[current_cmd_index] = '\0';
          queue_command_line(current_cmd_line);
          current_cmd_index = 0;
          aos_send("\r\n");
        }
      } else if (ch == 8 || ch == 127) { // Backspace or DEL
        if (current_cmd_index > 0) {
          current_cmd_index--;
          aos_send("\b \b");
        }
      } else if (ch >= 32 && ch <= 126) { // Printable characters
        if (current_cmd_index < MAX_CMD_LENGTH - 1) {
          current_cmd_line[current_cmd_index++] = ch;
        }
      }
    }
  }

  // Execute any queued commands (only in AOS command mode)
  if (!sub_sim_is_active()) {
    execute_next_command();

    // Handle Debug Display on LCD when in AOS menu
    update_prescaler++;
    if (update_prescaler > 2000) { // Adjust this value for update rate
      update_prescaler = 0;

      // Check O2 button for page toggle
      bool o2_pressed = sub_sim_button_o2_cb_pressed();
      if (o2_pressed && !last_o2_state) {
        debug_page++;
        if (debug_page > 3) debug_page = 0;
      }
      last_o2_state = o2_pressed;

      char line1[17];
      char line2[17];

      switch (debug_page) {
        case 0: // Main Menu Info
          snprintf(line1, sizeof(line1), "AOS v1.0 READY  ");
          snprintf(line2, sizeof(line2), "TYPE SUBSIM     ");
          break;
        case 1: // Joystick Debug
          snprintf(line1, sizeof(line1), "JOY X:%4d      ", sub_sim_read_joy_x());
          snprintf(line2, sizeof(line2), "JOY Y:%4d      ", sub_sim_read_joy_y());
          break;
        case 2: // Gas Sensor 1
          snprintf(line1, sizeof(line1), "CO :%4d        ", sub_sim_read_co());
          snprintf(line2, sizeof(line2), "NH3:%4d        ", sub_sim_read_nh3());
          break;
        case 3: // Gas Sensor 2 + Pot Raw
          snprintf(line1, sizeof(line1), "NO2:%4d        ", sub_sim_read_no2());
          snprintf(line2, sizeof(line2), "POT:%4d        ", sub_sim_read_pot());
          break;
      }
      sub_sim_lcd_print(line1, line2);
    }
  }
}

void ui_show_welcome(void) {
  aos_send("\r\n");
  aos_send("+-----------------------------------------------------------+\r\n");
  aos_send("|          ARTURO'S OPERATING SYSTEM                     |\r\n");
  aos_send("|                          BOOTED!                          |\r\n");
  aos_send("+-----------------------------------------------------------+\r\n");
  aos_printf("| Version: %-8s         Build: %10s              |\r\n",
             AOS_VERSION, AOS_BUILD_DATE);
  if (aos_f_cpu_hz) {
    aos_printf("| MCU: AVR128DB48           Freq: %10lu Hz             |\r\n",
               (unsigned long)aos_f_cpu_hz);
  } else {
    aos_send("| MCU: AVR128DB48          Freq: (unknown)              |\r\n");
  }
  aos_printf("| UART3: %-6lubaud         Interrupts: ENABLED             |\r\n",
             (unsigned long)(aos_uart_baud ? aos_uart_baud : 9600));
  aos_send("+-----------------------------------------------------------+\r\n\r\n");
  
  aos_send("Type HELP for command list, SYSINFO for system status\r\n\r\n");
  cmd_help(NULL);
  aos_send("AOS> ");
}
//================================
// Internal Helper Functions
//================================

static void queue_command_line(const char *cmd_line) {
  // Add each character of the command line to the circular buffer
  for (const char *p = cmd_line; *p && !circular_buf_full(cmd_line_buffer);
       p++) {
    circular_buf_try_put(cmd_line_buffer, (uint8_t)*p);
  }
  // Add null terminator
  if (!circular_buf_full(cmd_line_buffer)) {
    circular_buf_try_put(cmd_line_buffer, 0);
  }
}

static void execute_next_command(void) {
  if (circular_buf_empty(cmd_line_buffer)) {
    return;
  }

  // Extract command line from circular buffer
  char cmd_line[MAX_CMD_LENGTH];
  uint8_t i = 0;
  uint8_t ch;

  // Read until null terminator or buffer empty
  while (i < MAX_CMD_LENGTH - 1 &&
         circular_buf_get(cmd_line_buffer, &ch) == 0) {
    if (ch == 0)
      break; // End of command line
    cmd_line[i++] = ch;
  }
  cmd_line[i] = '\0';

  if (i == 0)
    return; // Empty command

  // Parse command and parameters
  char cmd_name[16];
  char params[MAX_CMD_LENGTH];
  params[0] = '\0';
  // Capture command and the rest of the line (params)
  int num_parsed = sscanf(cmd_line, "%15s %63[^\r\n]", cmd_name, params);
  if (num_parsed >= 2) {
    // Trim leading spaces in params
    char *p = params;
    while (*p == ' ' || *p == '\t')
      p++;
    if (p != params) {
      memmove(params, p, strlen(p) + 1);
    }
    // Trim trailing spaces
    size_t len = strlen(params);
    while (len > 0 && (params[len - 1] == ' ' || params[len - 1] == '\t')) {
      params[--len] = '\0';
    }
  }

  if (num_parsed < 1)
    return;

  // Convert command to uppercase
  for (int j = 0; cmd_name[j]; j++) {
    if (cmd_name[j] >= 'a' && cmd_name[j] <= 'z') {
      cmd_name[j] = cmd_name[j] - 'a' + 'A';
    }
  }

  // Find and execute command
  bool handled = false;
  for (const command_t *cmd = commands; cmd->name; cmd++) {
    if (strcmp(cmd_name, cmd->name) == 0) {
      cmd->handler((num_parsed > 1) ? params : NULL);
      handled = true;
      break;
    }
  }

  if (!handled) {
    // Unknown command
    aos_printf("Unknown command: %s\r\n", cmd_name);
    aos_send("Type HELP for available commands\r\n\r\n");
  }
  
  // Only send AOS prompt if we're still in AOS mode (not in any lab)
  if (!sub_sim_is_active()) {
    aos_send("AOS> ");
  }
}


//================================
// Command Handler Implementations
//================================

//================================
// Arturo's OS Command Implementations
//================================

static void cmd_help(const char *params) {
  (void)params;
  aos_send("\r\nARTURO'S OPERATING SYSTEM - COMMAND REFERENCE\r\n");
  aos_send("-----------------------------------------------------------\r\n");
  for (const command_t *cmd = commands; cmd->name; cmd++) {
    aos_printf("  %s\r\n", cmd->help_text);
  }
  aos_send("-----------------------------------------------------------\r\n\r\n");
}

static void cmd_sysinfo(const char *params) {
  (void)params;
  aos_send("\r\nSYSTEM INFORMATION\r\n");
  aos_send("-----------------------------------------------------------\r\n");
  if (aos_f_cpu_hz) {
    aos_printf("MCU: AVR128DB48                Clock: %lu Hz\r\n",
               (unsigned long)aos_f_cpu_hz);
  } else {
    aos_send("MCU: AVR128DB48                Clock: (unknown)\r\n");
  }
  aos_printf("UART3 Status: 0x%02X           Baud: 9600\r\n", USART3.STATUS);
#ifdef SP
  aos_printf("Stack Pointer: 0x%04X         \r\n", (unsigned)SP);
#endif
  aos_printf("SREG: 0x%02X                  Interrupts: %s\r\n", SREG,
             (SREG & 0x80) ? "ENABLED" : "DISABLED");

  // Memory usage estimation
  extern char __heap_start, *__brkval;
  int free_memory =
      (int)&free_memory - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
  aos_printf("Free RAM: ~%d bytes\r\n", free_memory);

  // Peripheral status
  aos_printf("RTC Status: 0x%02X             TCA0 Status: Running\r\n",
             RTC.STATUS);
  aos_printf("Command Buffer: %u/%u used\r\n",
             (unsigned)circular_buf_size(cmd_line_buffer),
             (unsigned)CMD_BUFFER_SIZE);
  aos_send(
      "-----------------------------------------------------------\r\n\r\n");
}

static void cmd_regs(const char *params) {
  bool found = false;

  if (strlen(params) == 0) {
    aos_send("\r\nAVAILABLE PERIPHERALS\r\n");
    aos_send("-----------------------------------------------------------\r\n");
    for (uint8_t i = 0; i < sizeof(peripherals) / sizeof(peripherals[0]); i++) {
      aos_printf("  %s\r\n", peripherals[i].peripheral_name);
    }
    aos_send("-----------------------------------------------------------\r\n");
    aos_send("Usage: REGS <peripheral_name>\r\n\r\n");
    return;
  }

  // Convert to uppercase for comparison
  char peripheral_name[16];
  strncpy(peripheral_name, params, 15);
  peripheral_name[15] = '\0';
  for (char *p = peripheral_name; *p; p++) {
    *p = toupper(*p);
  }

  for (uint8_t i = 0; i < sizeof(peripherals) / sizeof(peripherals[0]); i++) {
    if (strcmp(peripheral_name, peripherals[i].peripheral_name) == 0) {
      found = true;
      aos_printf("\r\n%s REGISTERS\r\n", peripherals[i].peripheral_name);
      aos_send(
          "-----------------------------------------------------------\r\n");

      for (uint8_t j = 0; j < peripherals[i].count; j++) {
        volatile uint8_t *addr = peripherals[i].registers[j].address;
        aos_printf("%-12s @ 0x%04X = 0x%02X  (%s)\r\n",
                   peripherals[i].registers[j].name, (uint16_t)addr, *addr,
                   peripherals[i].registers[j].description);
      }
      aos_send("-----------------------------------------------------------"
               "\r\n\r\n");
      break;
    }
  }

  if (!found) {
    aos_printf("Unknown peripheral: %s\r\n", params);
    aos_send("Available: RTC, USART3, PORTD, TCA0, DAC0\r\n\r\n");
  }
}

static void cmd_read(const char *params) {
  if (strlen(params) == 0) {
    aos_send("Usage: READ <hex_address>\r\n");
    aos_send("Example: READ 0x1000\r\n\r\n");
    return;
  }

  uint16_t address = (uint16_t)strtol(params, NULL, 16);
  volatile uint8_t *ptr = (volatile uint8_t *)address;

  aos_printf("Memory Read: 0x%04X = 0x%02X (%d)\r\n", address, *ptr, *ptr);

  // Show binary representation
  aos_send("Binary: ");
  for (int i = 7; i >= 0; i--) {
    aos_send((*ptr & (1 << i)) ? "1" : "0");
  }
  aos_send("\r\n\r\n");
}

static void cmd_write(const char *params) {
  char *token1 = strtok((char *)params, " ");
  char *token2 = strtok(NULL, " ");

  if (!token1 || !token2) {
    aos_send("Usage: WRITE <hex_address> <hex_value>\r\n");
    aos_send("Example: WRITE 0x1000 0xFF\r\n\r\n");
    return;
  }

  uint16_t address = (uint16_t)strtol(token1, NULL, 16);
  uint8_t value = (uint8_t)strtol(token2, NULL, 16);
  volatile uint8_t *ptr = (volatile uint8_t *)address;

  uint8_t old_value = *ptr;
  *ptr = value;

  aos_printf("   Memory Write: 0x%04X\r\n", address);
  aos_printf("   Old: 0x%02X (%d)\r\n", old_value, old_value);
  aos_printf("   New: 0x%02X (%d)\r\n", value, value);
  aos_send("\r\n");
}

static void cmd_dump(const char *params) {
  char *token1 = strtok((char *)params, " ");
  char *token2 = strtok(NULL, " ");

  if (!token1) {
    aos_send("Usage: DUMP <start_address> [length]\r\n");
    aos_send("Example: DUMP 0x1000 16\r\n\r\n");
    return;
  }

  uint16_t start = (uint16_t)strtol(token1, NULL, 16);
  uint8_t length = token2 ? (uint8_t)strtol(token2, NULL, 10) : 16;

  if (length > 64)
    length = 64; // Limit output

  aos_printf("\r\nMemory Dump: 0x%04X (%d bytes)\r\n", start, length);
  aos_send("-----------------------------------------------------------\r\n");

  volatile uint8_t *ptr = (volatile uint8_t *)start;

  for (uint8_t i = 0; i < length; i += 8) {
    aos_printf("%04X: ", start + i);

    // Hex values
    for (uint8_t j = 0; j < 8 && (i + j) < length; j++) {
      aos_printf("%02X ", ptr[i + j]);
    }

    // Padding for alignment
    uint8_t pad = (uint8_t)((i + 8 <= length) ? 0 : (i + 8 - length));
    for (uint8_t j = pad; j > 0; j--) {
      aos_send("   ");
    }

    aos_send(" ");

    // ASCII representation
    for (uint8_t j = 0; j < 8 && (i + j) < length; j++) {
      uint8_t c = ptr[i + j];
      if (c >= 32 && c <= 126) {
        uart_send_char((char)c);
      } else {
        uart_send_char('.');
      }
    }
    aos_send("\r\n");
  }
  aos_send(
      "-----------------------------------------------------------\r\n\r\n");
}

static void cmd_peek(const char *params) {
  cmd_read(params); // Alias for READ
}

static void cmd_poke(const char *params) {
  cmd_write(params); // Alias for WRITE
}

static void cmd_reset(const char *params) {
  (void)params;
  aos_send("Performing software reset...\r\n");
  aos_send("Executing Watchdog Timer Reset\r\n\r\n");

  // Small delay to let UART finish
  for (volatile uint32_t i = 0; i < 100000; i++)
    ;

  // Software reset using watchdog
  CCP = CCP_IOREG_gc;
  WDT.CTRLA = WDT_PERIOD_8CLK_gc | WDT_WINDOW_OFF_gc;
  while (1)
    ; // Wait for reset
}

static void cmd_uart_test(const char *params) {
  (void)params;
  aos_send("\r\nUART3 DIAGNOSTIC TEST\r\n");
  aos_send("-----------------------------------------------------------\r\n");

  aos_printf("USART3.STATUS: 0x%02X\r\n", USART3.STATUS);
  aos_printf("USART3.CTRLA:  0x%02X\r\n", USART3.CTRLA);
  aos_printf("USART3.CTRLB:  0x%02X\r\n", USART3.CTRLB);
  aos_printf("USART3.CTRLC:  0x%02X\r\n", USART3.CTRLC);
  aos_printf("USART3.BAUD:   %d\r\n", (USART3.BAUDH << 8) | USART3.BAUDL);

  aos_printf("TX Buffer Free: %d/%d\r\n", uart_tx_free_space(),
             UART_BUFFER_SIZE);
  aos_printf("RX Buffer Used: %d/%d\r\n", uart_rx_available(),
             UART_BUFFER_SIZE);

  aos_send("\r\nSending test pattern: ");
  for (uint8_t i = 0; i < 10; i++) {
    aos_printf("%d ", i);
  }
  aos_send("\r\n");
  aos_send(
      "-----------------------------------------------------------\r\n\r\n");
}

static void cmd_gpio_test(const char *params) {
  if (params == NULL || *params == '\0') {
    aos_send("Usage: GPIO <port> <pin> <value>\r\n");
    aos_send("Example: GPIO D 3 1  (Set PORTD pin 3 to HIGH)\r\n");
    aos_send("Ports: D, B, C  Pins: 0-7  Values: 0/1\r\n\r\n");
    return;
  }

  // Make a local mutable copy for tokenization
  char buf[32];
  strncpy(buf, params, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  // Tokenize on space or comma
  char *saveptr = NULL;
  char *port_str = strtok_r(buf, " ,\t", &saveptr);
  char *pin_str = strtok_r(NULL, " ,\t", &saveptr);
  char *val_str = strtok_r(NULL, " ,\t", &saveptr);

  if (!port_str || !pin_str || !val_str) {
    aos_send("Usage: GPIO <port> <pin> <value>\r\n");
    aos_send("Example: GPIO D 3 1  (Set PORTD pin 3 to HIGH)\r\n");
    aos_send("Ports: D, B, C  Pins: 0-7  Values: 0/1\r\n\r\n");
    return;
  }

  char port = toupper((unsigned char)port_str[0]);
  uint8_t pin = (uint8_t)atoi(pin_str);
  uint8_t value = (uint8_t)atoi(val_str);

  if (pin > 7 || (value != 0 && value != 1)) {
    aos_send("Invalid pin (0-7) or value (0/1)\r\n\r\n");
    return;
  }

  volatile PORT_t *port_reg;
  switch (port) {
  case 'D':
    port_reg = &PORTD;
    break;
  case 'B':
    port_reg = &PORTB;
    break;
  case 'C':
    port_reg = &PORTC;
    break;
  default:
    aos_send("Invalid port. Use D, B, or C\r\n\r\n");
    return;
  }

  // Set as output
  port_reg->DIRSET = (1 << pin);

  // Set value
  if (value) {
    port_reg->OUTSET = (1 << pin);
  } else {
    port_reg->OUTCLR = (1 << pin);
  }

  aos_printf("PORT%c pin %d set to %s\r\n", port, pin, value ? "HIGH" : "LOW");
  aos_printf("   DIR: 0x%02X  OUT: 0x%02X  IN: 0x%02X\r\n\r\n", port_reg->DIR,
             port_reg->OUT, port_reg->IN);
}

static void cmd_adc_read(const char *params) {
  (void)params;
  aos_send("\r\nADC SENSOR READINGS\r\n");
  aos_send("-----------------------------------------------------------\r\n");
  aos_printf("Potentiometer: %4d\r\n", sub_sim_read_pot());
  aos_printf("Joystick X:    %4d\r\n", sub_sim_read_joy_x());
  aos_printf("Joystick Y:    %4d\r\n", sub_sim_read_joy_y());
  aos_printf("CO Sensor:     %4d\r\n", sub_sim_read_co());
  aos_printf("NH3 Sensor:    %4d\r\n", sub_sim_read_nh3());
  aos_printf("NO2 Sensor:    %4d\r\n", sub_sim_read_no2());
  aos_send("-----------------------------------------------------------\r\n\r\n");
}

static void cmd_timer_info(const char *params) {
  (void)params;
  aos_send("\r\nTIMER STATUS\r\n");
  aos_send("-----------------------------------------------------------\r\n");

  // TCA0 Status
  aos_printf("TCA0.SINGLE.CTRLA:    0x%02X\r\n", TCA0.SINGLE.CTRLA);
  aos_printf("TCA0.SINGLE.CTRLB:    0x%02X\r\n", TCA0.SINGLE.CTRLB);
  aos_printf("TCA0.SINGLE.INTCTRL:  0x%02X\r\n", TCA0.SINGLE.INTCTRL);
  aos_printf("TCA0.SINGLE.INTFLAGS: 0x%02X\r\n", TCA0.SINGLE.INTFLAGS);
  aos_printf("TCA0.SINGLE.CNT:      %u\r\n", TCA0.SINGLE.CNT);
  aos_printf("TCA0.SINGLE.PER:      %u\r\n", TCA0.SINGLE.PER);

  aos_send(
      "-----------------------------------------------------------\r\n\r\n");
}

//================================
// Lab Launcher Commands
//================================

static void cmd_run_subsim(const char *params) {
  (void)params;
  aos_send("\r\n");
  aos_send("Launching Submarine Simulator...\r\n");
  aos_send(".\r\n");
  _delay_ms(500);
  aos_send("..\r\n");
  _delay_ms(500);
  aos_send("...\r\n");
  _delay_ms(500);
  aos_send("AOS Has loaded the Subroutine: SUBSIM\r\n");

  aos_send("(Type EXIT to return to AOS)\r\n");
  
  // Initialize and launch SubSim
  sub_sim_init();
  sub_sim_show_welcome();
}

static void cmd_play_music(const char *params) {
  aos_send("\r\n");
  aos_send("+-----------------------------------------------------------+\r\n");
  aos_send("|                     MELODY PLAYER                         |\r\n");
  aos_send("|                    Playing MIDI...                        |\r\n");
  aos_send("+-----------------------------------------------------------+\r\n");
  aos_send("\r\n");
  aos_send("Playing melody on DAC0 (PD6 buzzer)...\r\n");
  aos_send("Duration depends on tempo\r\n");
  aos_send("Notes: 81 notes from E4 to E5\r\n");
  aos_send("\r\n");
  
  // Initialize melody system first
  melody_init();
  aos_send("Melody initialized\r\n");
  
  // Optional tempo parameter: PLAYMUSIC <tempo>
  int tempo = 5; // default 5x as requested
  if (params && *params) {
    int t = atoi(params);
    if (t >= 1 && t <= 16) tempo = t;
  }
  melody_set_speed((uint8_t)tempo);
  aos_printf("Tempo set: %dx faster\r\n", tempo);
  
  // Play the melody
  melody_play(midi1, midi1_length);
  
  aos_send("\r\n");
  aos_send("Melody complete!\r\n");
  aos_send("\r\n");
}
