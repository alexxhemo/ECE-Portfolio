
#define F_CPU 16000000UL
#define __AVR_AVR128DB48__
#include "ui_adc.h"
#include "aos_timer.h"
#include "uart.h"
#include "ui.h"
#include <avr/interrupt.h>
#include <avr/io.h>
// #include <math.h>
// #include <stdio.h>
#include <stdlib.h>
#include <util/delay.h>

//================================
// ADCLab External Variables
//================================
extern void aos_send(const char *str);
extern void aos_printf(const char *format, ...);

//================================
// ADCLab State
//================================
volatile bool adclab_active = false;
volatile uint32_t adc_countdown = 0;
static const uint32_t ADC_READ_INTERVAL = 1000; // 1 second at ~1ms timer ticks

//================================
// ADC Initialization
//================================
static void init_adc(void) {
  // Configure ADC0 to read AIN8 (PORTE0 - potentiometer)
  ADC0.MUXPOS = ADC_MUXPOS_AIN8_gc;
  ADC0.CTRLC = ADC_PRESC_DIV8_gc;
  ADC0.CTRLD = ADC_INITDLY_DLY16_gc;
  VREF.ADC0REF = VREF_REFSEL_VDD_gc; // VDD as reference (3.3V)
  ADC0.CTRLA = ADC_ENABLE_bm;        // Enable ADC

  // Start first conversion
  ADC0.COMMAND = ADC_STCONV_bm;
  while ((ADC0.COMMAND & (1 << ADC_STCONV_bp)))
    ;
}

// TCA1 1ms handler for ADC Lab
static void adclab_tca1_handler(void) {
  if (adc_countdown > 0) {
    adc_countdown--;
  }
}

//================================
// ADCLab Public Functions
//================================

void adc_lab_init(void) {
  adclab_active = true;
  adc_countdown = ADC_READ_INTERVAL;
  init_adc();
  // Configure and start TCA1 at 1ms tick; register handler
  aos_tca1_register(adclab_tca1_handler);
  aos_tca1_start_1ms();
}

void adc_lab_process(void) {
  // Non-blocking process - timer interrupt handles countdown
  // ADC conversion happens asynchronously in interrupt

  // Check for user input to EXIT
  char ch;
  if (uart_receive_char(&ch)) {
    uart_send_char(ch);
    if (ch == 'e' || ch == 'E') {
      adc_lab_exit();
      return;
    }
  }

  // Check if it's time to read ADC (countdown reached 0 in ISR)
  if (adc_countdown == 0) {
    // Get the last ADC result
    uint16_t adc_raw = ADC0.RES;

    // Convert 12-bit ADC reading to voltage (0-3.3V)
    // ADC range: 0-4095, Voltage range: 0-3.3V
    float voltage = (float)adc_raw / 4096.0f * 3.3f;

    // Format voltage string with 2 decimal places
    char volt_str[16];
    dtostrf(voltage, 4, 2, volt_str);

    // Display voltage
    aos_printf("V = %s V\r\n", volt_str);

    // Start next conversion
    ADC0.COMMAND = ADC_STCONV_bm;
    while ((ADC0.COMMAND & (1 << ADC_STCONV_bp)))
      ;

    // Reset countdown
    adc_countdown = ADC_READ_INTERVAL;
  }
}

void adc_lab_show_welcome(void) {
  aos_send("\r\n");
  aos_send("+-----------------------------------------------------------+\r\n");
  aos_send("|                          ADCLab                           |\r\n");
  aos_send("|                        VOLTMETER                          |\r\n");
  aos_send("+-----------------------------------------------------------+\r\n");
  aos_send("\r\nAnalog-to-Digital Converter Status:\r\n");
  aos_send("  - Voltmeter on ADC0 (PORTE0 - AIN8)\r\n");
  aos_send("  - Input Range: 0 - 3.3V\r\n");
  aos_send("  - Resolution: 12-bit (0.8mV/step)\r\n");
  aos_send("  - Update Rate: 1 second\r\n");
  aos_send("\r\nInstructions:\r\n");
  aos_send("  - Connect potentiometer to PORTE0 (AIN8)\r\n");
  aos_send("  - Twist knob to vary voltage from 0V to 3.3V\r\n");
  aos_send("  - Voltage readings print every 1 second\r\n");
  aos_send("  - Type EXIT to return to AOS\r\n\r\n");

  aos_send("Starting voltage measurements...\r\n\r\n");

  _delay_ms(100);
}

bool adc_lab_is_active(void) { return adclab_active; }

void adc_lab_exit(void) {
  // Disable ADC
  ADC0.CTRLA &= ~ADC_ENABLE_bm;
  // Disable TCA1 and unregister handler so other labs can use it
  aos_tca1_disable();
  aos_tca1_unregister();

  adclab_active = false;
  aos_send("\r\nExiting ADCLab, returning to AOS...\r\n\r\n");
}
