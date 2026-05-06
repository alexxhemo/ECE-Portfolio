/**
 * @file main.c
 * @author Arturo Salinas
 * @date 2025-10-27
 * @brief Main system - Arturo's OS with Submarine Simulation Module
 */

#define F_CPU 16000000UL
#define __AVR_AVR128DB48__
#include "include/aos_timer.h"
#include "include/cpu.h"
#include "include/dac.h"
#include "include/uart.h"
#include "include/ui.h"
#include "include/i2c_lib_S25.h"
#include "include/sub_sim.h"
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>

#define BAUD_RATE 9600

// Global variables for DACLab (waveform generation)
volatile uint32_t freq = 500;          // Current frequency in Hz (10-1000)
volatile uint16_t sine_index = 0;      // Current position in sine table
volatile int amplitude_percent = 100;  // Current amplitude (10-100%)
volatile float amplitude_scaler = 1.0; // Amplitude scaling factor (0.1 to 1.0)
volatile int dac_update = 1;           // DAC update enabled by default

// Timer-based counters for 5-second status display
volatile uint32_t status_display_counter = 0;
volatile bool display_status_flag = false;

// Sine Wave Table (64 samples for one period)
extern uint16_t sineWave[64];

// TCA1 Tick Handler for SubSim
static void tca1_tick_handler(void) {
  sub_sim_tick_handler();
}

//*********************************
// Sine Table Initialization
// ********************************

//********************************
// LED Initialization
//********************************
void init_led() {
  PORTD.DIRSET = 0b10111111; // PD0-5, PD7 as output (skip PD6 for DAC)
  PORTD.OUTCLR = 0xFF;       // all off
  PORTB.DIRSET = PIN3_bm;    // configure PB3 as output
  PORTB.OUTSET = PIN3_bm;    // drive high = LED off
  PORTC.DIRSET = PIN1_bm;    // PC1 as output
  PORTC.OUTCLR = PIN1_bm;
}

//*********************************
// Button Initialization
// ********************************
void init_button() {
  PORTB.DIRCLR = PIN2_bm;            // Onboard button - input
  PORTB.PIN2CTRL = PORT_PULLUPEN_bm; // Enable pull-up only
  PORTB.DIRCLR = PIN5_bm;            // External button - input
  PORTB.PIN5CTRL = PORT_PULLUPEN_bm; // Enable pull-up only
}

//************************************************
// Timer Init - TCA0 (centralized via AOS Timer)
//************************************************
// Configure TCA0 for DAC sample stepping and PWM output on WO1 (PC1)
static void init_tca0_configured(void) {
  // Single-slope PWM mode, DIV1 clock, enable
  uint32_t period = F_CPU / (freq * 64UL) - 1UL;
  aos_tca0_configure((uint16_t)period,
                     TCA_SINGLE_CLKSEL_DIV1_gc | TCA_SINGLE_ENABLE_bm,
                     TCA_SINGLE_WGMODE_SINGLESLOPE_gc);
}

// External variables from ui_adc.c for ADCLab timing
extern volatile bool adclab_active;
extern volatile uint32_t adc_countdown;

static void tca0_dac_handler(void) {
  // DAC Update: Output next sample from pre-scaled sine table at each timer
  // interrupt
  if (dac_update) {
    DAC0_setVal(sine_wave_scaled[sine_index]);
    sine_index++;
    if (sine_index >= 64) {
      sine_index = 0;
    }
  }

  // Always count samples for status display (independent of dac_update flag)
  status_display_counter++;

  uint32_t samples_per_5_seconds = freq * 64UL * 5UL;
  if (status_display_counter >= samples_per_5_seconds) {
    status_display_counter = 0;
    display_status_flag = true;
  }
}

//*****************************************************************************
// USART Interrupt Service Routines
//*****************************************************************************
ISR(USART3_RXC_vect) {
  char receivedChar = USART3.RXDATAL;
  uart_rx_isr_handler(receivedChar);
}

ISR(USART3_DRE_vect) {
  char data_to_send;
  if (uart_tx_isr_handler(&data_to_send)) {
    USART3.TXDATAL = data_to_send;
  } else {
    USART3.CTRLA &= ~USART_DREIE_bm;
  }
}

/*
ISR(USART3_RXC_vect) {
  keypress_val = USART3.RXDATAL;
  keypress_int = keypress_val - '0';
  keypress_flag = 1;
}
*/
// ****************************************************************************
// Main Function
// ****************************************************************************
int main(void) {
  uint32_t F_CLK_PER = F_CPU;

  // Initialize system components
  CLOCK_XOSCHF_16M_init();

  init_led();    // Initialize LEDs (PORTD, PORTB, PORTC)
  init_button(); // Initialize buttons (PB2, PB5)

  // Initialize I2C LCD Display (uses TWI1)
  TWI1_Host_Initialize();
  LCD_init();
  BACKLIGHT_init();
  BACKLIGHT_setBacklight100();
  LCD_sendCommand(0x01); // Clear display
  _delay_ms(2);
  LCD_print("System Init");
  secondLine();
  LCD_print("In Progress...");
  _delay_ms(500);
  // Initialize UI command processing system
  ui_init();
  
  // Initialize ADC for debug view in AOS menu
  sub_sim_init_adc();

  // Initialize UART for command interface
  uart_init(3, BAUD_RATE, F_CLK_PER, NULL);
  ui_set_system_info(F_CLK_PER, BAUD_RATE);

  // Initialize DAC for waveform output
  DAC_init();

  // Compute sine wave table at startup
  sine_wave_init();

  // Initialize and enable TCA0 via AOS Timer
  init_tca0_configured();
  aos_tca0_register(tca0_dac_handler);
  aos_tca0_enable();

  // Initialize and enable TCA1 for 1ms system tick
  aos_tca1_start_1ms();
  aos_tca1_register(tca1_tick_handler);

  // Enable global interrupts
  sei();
  aos_printf("UART Initialized at 9600 baud\r\n");
  aos_printf("DAC0 Initialized to Output at Pin D.6\n");

  aos_printf("Configured DAC0 with output enable and run in standby\n");
  aos_printf("Set DAC0 reference voltage to VDD (3.3V) with always on mode\n");
  // PORTD.DIRSET = 0b00000001;

  // Show welcome message
  ui_show_welcome();

  // DAC will be enabled when DACLab is launched
  dac_update = 0;

  // Main loop
  while (1) {
    // Check if we're in SubSim mode
    if (sub_sim_is_active()) {
      // SubSim mode - handle submarine simulation
      sub_sim_process();

      // Display status every 5 seconds (but suppress during welcome)
      if (display_status_flag) {
        display_status_flag = false;
        // Optional: Add periodic status display if needed
      }
    } else {
      // AOS command mode - normal debugging interface
      ui_process_commands();
    }
  }

  return 0;
}
