#include "dac.h"
#include <avr/pgmspace.h>

// Pre-scaled sine wave table (applies amplitude scaling)
uint16_t sine_wave_scaled[64];

// Pre-calculated sine wave table in PROGMEM (64 samples of one period)
const uint16_t PROGMEM sine_table[64] = {
    512, 562, 612, 660, 707, 752, 794, 833, 868, 900, 927, 950, 968,
    982, 991, 995, 995, 991, 982, 968, 950, 927, 900, 868, 833, 794,
    752, 707, 660, 612, 562, 512, 461, 411, 363, 316, 271, 229, 190,
    155, 123, 95,  72,  53,  39,  30,  25,  24,  28,  37,  50,  68,
    90,  116, 146, 180, 218, 259, 303, 350, 399, 449, 500, 551};

// RAM copy of sine table for use in ISR
uint16_t sineWave[64];

void sine_wave_init(void) {
  uint8_t i;
  // Copy sine table from PROGMEM to RAM for faster ISR access
  for (i = 0; i < 64; i++) {
    sineWave[i] = pgm_read_word(&sine_table[i]);
  }

  // Initialize scaled table with full amplitude (100%)
  // This ensures we have all 64 distinct sine values available
  for (i = 0; i < 64; i++) {
    sine_wave_scaled[i] = sineWave[i];
  }
}

// Update the scaled sine wave table based on amplitude percentage
void update_sine_wave_scaled(int amplitude_percent) {
  float scaler = (float)amplitude_percent / 100.0f;
  for (uint8_t i = 0; i < 64; i++) {
    sine_wave_scaled[i] = (uint16_t)(sineWave[i] * scaler);
  }
}

void DAC_init(void) {
  // The DAC output pin needs to have the digital input buffer and the pull-up
  // resistor disabled in order to reduce its load.
  PORTD.PIN6CTRL &= ~PORT_ISC_gm;
  PORTD.PIN6CTRL |= PORT_ISC_INPUT_DISABLE_gc;
  PORTD.PIN6CTRL &= ~PORT_PULLUPEN_bm;

  // Configure DAC0 with output enable and run in standby
  DAC0.CTRLA = DAC_ENABLE_bm | DAC_OUTEN_bm | DAC_RUNSTDBY_bm;
  // Set reference voltage to VDD (3.3V) with always on mode
  VREF.DAC0REF = VREF_REFSEL_VDD_gc | VREF_ALWAYSON_bm;

  // Wait for VREF startup time
  _delay_us(50);
}

void DAC0_setVal(uint16_t value) {
  DAC0.DATAL = (value & (0x03)) << 6;
  DAC0.DATAH = value >> 2;
}
