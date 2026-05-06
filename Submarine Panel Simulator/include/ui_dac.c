#include "ui_dac.h"
#include "uart.h"
#include "dac.h"
#include "ui.h"
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//================================
// DACLab External Variables (from main.c)
//================================
extern void update_tca0_frequency(uint32_t new_freq);
extern void update_sine_wave_scaled(int amplitude_percent);
extern volatile uint32_t freq;
extern volatile uint16_t sine_index;
extern volatile int amplitude_percent;
extern volatile float amplitude_scaler;
extern volatile int dac_update;

//================================
// DACLab State Machine
//================================
static volatile int ui_mode = 0;  // 0: waiting, 1: entering freq, 2: entering ampl
static volatile uint32_t ui_temp_freq = 0;
static volatile int ui_temp_amp_percent = 0;
static volatile bool daclab_active = false;
static volatile bool suppress_status = false;  // Suppress status display right after launch

//================================
// DACLab State Machine Handler
//================================
static void dac_question_handler(char keypress_val) {
  int keypress_int = keypress_val - '0';
  
  // Clear suppress flag on first user interaction (any printable char or Enter)
  if (suppress_status && (keypress_val == '\r' || keypress_val == '\n' || 
                          (keypress_val >= 32 && keypress_val <= 126))) {
    suppress_status = false;
  }

  switch (ui_mode) {
  case 0: // Wait for initial question answer
    if (keypress_val == 'f' || keypress_val == 'F') {
      uart_send_char(keypress_val);  // Echo the F/A choice
      ui_mode = 1;
      aos_send("\r\nFrequency (10-1k Hz): ");
    } else if (keypress_val == 'a' || keypress_val == 'A') {
      uart_send_char(keypress_val);  // Echo the F/A choice
      ui_mode = 2;
      aos_send("\r\nEnter Amplitude Percent (10-100): ");
    } else if (keypress_val == 'e' || keypress_val == 'E') {
      // Check for "EXIT" command
      uart_send_char(keypress_val);
      dac_lab_exit();
    } else if (keypress_val == '\r' || keypress_val == '\n') {
      aos_send("\r\n");
    } else if (keypress_val >= 32 && keypress_val <= 126) {
      // Ignore other printable characters silently
    }
    break;

  case 1: // Update frequency - accept digits or Enter
    if (keypress_int >= 0 && keypress_int <= 9) { // Valid digit input
      uart_send_char(keypress_val);  // Echo the digit, nothing else
      ui_temp_freq = (ui_temp_freq * 10) + keypress_int;
    } else if (keypress_val == '\r' || keypress_val == '\n') { // Enter key
      if (ui_temp_freq >= 10 && ui_temp_freq <= 1000) {
        cli();
        freq = ui_temp_freq;
        update_tca0_frequency(ui_temp_freq);
        sei();
        aos_printf("\r\nFrequency set to %lu Hz\r\n", freq);
        aos_send("Do you want to change frequency or amplitude (F/A)? ");
      } else {
        aos_printf("\r\n%lu out of range (10-1000)\r\n", ui_temp_freq);
        aos_send("Do you want to change frequency or amplitude (F/A)? ");
      }
      ui_temp_freq = 0;
      ui_mode = 0;
    } else if (keypress_val == '\b' || keypress_val == 127) { // Backspace
      uart_send_char(keypress_val);
      ui_temp_freq = ui_temp_freq / 10;
    } else if (keypress_val >= 32 && keypress_val <= 126) {
      // Invalid character - ignore it silently
    }
    break;

  case 2: // Amplitude Level - accept digits or Enter
    if (keypress_int >= 0 && keypress_int <= 9) { // Valid digit input
      uart_send_char(keypress_val);  // Echo the digit, nothing else
      ui_temp_amp_percent = (ui_temp_amp_percent * 10) + keypress_int;
    } else if (keypress_val == '\r' || keypress_val == '\n') { // Enter key
      if (ui_temp_amp_percent >= 10 && ui_temp_amp_percent <= 100) {
        cli();
        amplitude_percent = ui_temp_amp_percent;
        amplitude_scaler = (float)ui_temp_amp_percent / 100.0f;
        update_sine_wave_scaled(ui_temp_amp_percent);
        sei();
        aos_printf("\r\nAmplitude percent set to %d%%\r\n", amplitude_percent);
        aos_send("Do you want to change frequency or amplitude (F/A)? ");
      } else {
        aos_printf("\r\n%d out of range (10-100)\r\n", ui_temp_amp_percent);
        aos_send("Do you want to change frequency or amplitude (F/A)? ");
      }
      ui_temp_amp_percent = 0;
      ui_mode = 0;
    } else if (keypress_val == '\b' || keypress_val == 127) { // Backspace
      uart_send_char(keypress_val);
      ui_temp_amp_percent = ui_temp_amp_percent / 10;
    } else if (keypress_val >= 32 && keypress_val <= 126) {
      // Invalid character - ignore it silently
    }
    break;
  }
}

//================================
// DACLab Public Interface
//================================

void dac_lab_init(void) {
  ui_mode = 0;
  ui_temp_freq = 0;
  ui_temp_amp_percent = 0;
  daclab_active = true;
  suppress_status = true;  // Suppress status display during welcome
  // Enable DAC output when entering DACLab
  extern volatile int dac_update;
  dac_update = 1;
  // Don't reset status_display_counter - let it count naturally
}

void dac_lab_process(void) {
  // Process ONE character from UART input (non-blocking)
  char ch;
  
  if (uart_receive_char(&ch)) {
    dac_question_handler(ch);
  }
}

void dac_lab_show_welcome(void) {
  aos_send("\r\n");
  aos_send("+-----------------------------------------------------------+\r\n");
  aos_send("|                          DACLab                           |\r\n");
  aos_send("|                          ACTIVE!                          |\r\n");
  aos_send("+-----------------------------------------------------------+\r\n");
  aos_send("\r\nWaveform Generator Status:\r\n");
  aos_send("  - Sine wave output on DAC0 (PD6)\r\n");
  aos_send("  - Default: 500 Hz, 100% amplitude\r\n");
  aos_send("\r\nInteractive Mode:\r\n");
  aos_send("  Type F to change Frequency\r\n");
  aos_send("  Type A to change Amplitude\r\n");
  aos_send("  Type EXIT to return to AOS\r\n\r\n");
  
  aos_send("Current Freq: 500 Hz\r\n");
  aos_send("Amplitude Percent: 100%\r\n");
  aos_send("Do you want to change frequency or amplitude (F/A)? ");
  
  // Wait for UART to fully transmit
  _delay_ms(100);
}

void dac_lab_status_display(void) {
  aos_send("\r\nSTATUS: Freq: ");
  char freq_str[16];
  itoa(freq, freq_str, 10);
  aos_send(freq_str);
  aos_send(" Hz, Amplitude: ");
  char amp_str[16];
  itoa(amplitude_percent, amp_str, 10);
  aos_send(amp_str);
  aos_send("%\r\n");
}

bool dac_lab_is_active(void) {
  return daclab_active;
}

bool dac_lab_should_suppress_status(void) {
  return suppress_status;
}

void dac_lab_exit(void) {
  // Disable DAC output when exiting DACLab
  extern volatile int dac_update;
  dac_update = 0;
  daclab_active = false;
  aos_send("\r\nExiting DACLab, returning to AOS...\r\n\r\n");
}
