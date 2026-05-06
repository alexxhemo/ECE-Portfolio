/**
 * @file labtest3.c
 * @author Arturo Salinas
 * @date 2025-10-27
 * @brief LabTest 3 Implementation - Adaptive LED & Potentiometer-Controlled DAC
 */

#define F_CPU 16000000UL
#define __AVR_AVR128DB48__
#include "labtest3.h"
#include "aos_timer.h"
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>

#define F_CPU 16000000UL

//================================
// LabTest3 State Variables
//================================

static volatile bool labtest3_active = false;

/** Part 1: LED Animation */
static volatile uint16_t adc_reading = 0;  // Current 12-bit ADC value
static volatile uint16_t adc_sum = 0;      // Running sum for averaging
static volatile uint8_t adc_count = 0;     // Sample count
static volatile uint16_t adc_averaged = 0; // Averaged reading
static volatile labtest3_led_mode_t led_mode = LABTEST3_MODE_OFF;
static volatile bool led_mode_frozen = false; // Frozen by button press

/** Part 2: DAC Waveform - Use external variables from main.c */
extern volatile uint32_t freq;        // Frequency in Hz from main.c
extern volatile uint16_t sine_index;  // Sine table index from main.c
extern volatile int dac_update;       // DAC update flag from main.c
extern uint16_t sine_wave_scaled[64]; // Scaled sine table from dac.c

static volatile labtest3_dac_mode_t dac_mode = LABTEST3_DAC_FIXED;
static volatile uint32_t dac_frequency_hz =
    10; // Current frequency in Hz (integer for timer calculations)
static volatile float dac_amplitude = 0.0f; // Amplitude 0-3.3V
static volatile uint32_t sweep_time_ms = 0; // Time in sweep mode
static volatile bool sweep_mode_active =
    false; // Sweep enabled by button - starts in fixed mode

/** DAC Sine Table - Local copy for amplitude scaling */
static const uint16_t sine_table[64] = {
    512, 562, 612, 660, 707, 752, 794, 833, 868, 900, 927, 950, 968,
    982, 991, 995, 995, 991, 982, 968, 950, 927, 900, 868, 833, 794,
    752, 707, 660, 612, 562, 512, 461, 411, 363, 316, 271, 229, 190,
    155, 123, 95,  72,  53,  39,  30,  25,  24,  28,  37,  50,  68,
    90,  116, 146, 180, 218, 259, 303, 350, 399, 449, 500, 551};

static volatile int amplitude_percent = 100;

/** Timing - millisecond counter */
static volatile uint32_t labtest3_ms_counter =
    0; // Millisecond counter from TCA1
static volatile uint8_t blink_state =
    0; // Current blink LED state (0=OFF, 1=ON)

/** Button state tracking for dual press detection */
static volatile bool pb5_pressed = false;
static volatile bool pb2_pressed = false;
static volatile uint32_t dual_button_start_ms = 0;

/** EEPROM Settings */
typedef struct {
  uint8_t led_mode;
  uint8_t led_frozen;
  uint8_t dac_mode;
  uint8_t sweep_enabled;
} labtest3_eeprom_settings_t;

static labtest3_eeprom_settings_t eeprom_settings EEMEM;

//================================
// External Functions
//================================

extern void aos_send(const char *str);
extern void aos_printf(const char *format, ...);

//================================
// ADC Functions
//================================

static void init_adc(void) {
  // Configure ADC0 for AIN8 (PORTE0)
  ADC0.MUXPOS = ADC_MUXPOS_AIN8_gc;

  // DIV8 prescaler for 16MHz clock → 2MHz ADC clock
  ADC0.CTRLC = ADC_PRESC_DIV8_gc;

  // 12-bit resolution, VDD reference, 16 sample delay
  ADC0.CTRLD = ADC_INITDLY_DLY16_gc;

  // Set 12-bit resolution (RESSEL = 2)
  ADC0.CTRLB = ADC_RESSEL_12BIT_gc;

  // VDD reference voltage
  VREF.ADC0REF = VREF_REFSEL_VDD_gc;

  // Enable ADC with free-running mode
  ADC0.CTRLA = ADC_ENABLE_bm | ADC_FREERUN_bm;

  // Start first conversion
  ADC0.COMMAND = ADC_STCONV_bm;
}

/**
 * @brief Read ADC and add to averaging buffer
 */
static void adc_sample(void) {
  // Check if conversion complete
  if (ADC0.INTFLAGS & ADC_RESRDY_bm) {
    adc_reading = ADC0.RES;
    ADC0.INTFLAGS = ADC_RESRDY_bm;

    // Add to running sum
    adc_sum += adc_reading;
    adc_count++;

    // Start next conversion
    ADC0.COMMAND = ADC_STCONV_bm;
  }
}

/**
 * @brief Get averaged ADC value (called every 100ms)
 */
static uint16_t get_averaged_adc(void) {
  if (adc_count >= LABTEST3_ADC_AVERAGE_COUNT) {
    adc_averaged = adc_sum / LABTEST3_ADC_AVERAGE_COUNT;
    adc_sum = 0;
    adc_count = 0;
    return adc_averaged;
  }
  return adc_averaged; // Return last averaged value
}

/**
 * @brief Convert 12-bit ADC reading to voltage (0-3.3V)
 */
static float adc_to_voltage(uint16_t adc_val) {
  return ((float)adc_val / 4095.0f) * 3.3f;
}

//================================
// LED Control Functions (PORTD)
//================================

static void init_led_port(void) {
  PORTD.DIRSET =
      0b10111111; // Port D configured as output except for Port D6 (DAC)
  PORTD.OUTCLR = 0b10111111; // All off
}

/**
 * @brief Initialize PWM on PIN C1 using TCA0 WO1
 * Note: TCA0 is configured in SINGLESLOPE mode in main.c for PWM capability
 */
static void init_pwm_c1(void) {
  // Route TCA0 to PORTC: WO0→PC0, WO1→PC1, WO2→PC2
  PORTMUX.TCAROUTEA =
      (PORTMUX.TCAROUTEA & ~PORTMUX_TCA0_gm) | PORTMUX_TCA0_PORTC_gc;

  // Set PC1 as output
  PORTC.DIRSET = PIN1_bm;

  // Enable Compare Channel 1 output (WO1)
  TCA0.SINGLE.CTRLB |= TCA_SINGLE_CMP1EN_bm;

  // Set initial duty cycle to 50% for testing
  TCA0.SINGLE.CMP1BUF = TCA0.SINGLE.PER / 2;
}

static void led_mode_off(void) {
  PORTD.OUTCLR = 0b10111111; // All LEDs off
  blink_state = 0;           // Reset blink state
}

static void led_mode_chase(void) {
  // Night rider chase effect on PORTD (skipping PD6 which is DAC output)
  // Valid LEDs: PD0, PD1, PD2, PD3, PD4, PD5, PD7 (7 LEDs total)
  static uint32_t last_chase_update_ms = 0;
  uint32_t current_ms = labtest3_ms_counter;

  // Update chase every 100ms
  if ((current_ms - last_chase_update_ms) >= 100) {
    last_chase_update_ms = current_ms;

    static const uint8_t chase_pins[7] = {0, 1, 2, 3,
                                          4, 5, 7}; // Skip pin 6 (DAC)
    static uint8_t chase_index = 0;

    PORTD.OUTCLR = 0b10111111;                     // Clear all LEDs except PD6
    PORTD.OUTSET = (1 << chase_pins[chase_index]); // Set current LED

    chase_index = (chase_index + 1) % 7; // Cycle through 7 LEDs
  }
}

static void led_mode_blink_8hz(void) {

  uint8_t desired_state = (labtest3_ms_counter & 0x40) ? 1 : 0;

  // Only update PORTD if state changed
  if (desired_state != blink_state) {
    blink_state = desired_state;
    if (desired_state) {
      PORTD.OUTSET = 0b10111111; // Turn ON
    } else {
      PORTD.OUTCLR = 0b10111111; // Turn OFF
    }
  }
}

//================================
// DAC Functions (PIN D6)
//================================

/**
 * @brief Update DAC frequency and timer period
 */
static void frequency_update(uint32_t new_freq) {
  if (new_freq != dac_frequency_hz) {
    dac_frequency_hz = new_freq;
    freq = new_freq; // Update global freq variable from main.c
    uint32_t per_value = F_CPU / (64UL * freq) - 1;
    TCA0.SINGLE.PER = per_value;
  }
}

/**
 * @brief Update sine table amplitude scaling
 */
static void amplitude_update(int new_amplitude_percent) {
  amplitude_percent = new_amplitude_percent;
  float amplitude_scaler = (float)(amplitude_percent) / 100.0f;

  // Update the global sine_wave_scaled table from main.c/dac.c
  for (int i = 0; i < 64; i++) {
    int raw = sine_table[i];
    sine_wave_scaled[i] = (uint16_t)(raw * amplitude_scaler);
  }
}

static void init_dac(void) {
  // Configure DAC0 on PIN D6
  PORTD.PIN6CTRL &= ~PORT_ISC_gm;
  PORTD.PIN6CTRL |= PORT_ISC_INPUT_DISABLE_gc;
  PORTD.PIN6CTRL &= ~PORT_PULLUPEN_bm;

  // Enable DAC, 10-bit mode
  DAC0.CTRLA = DAC_ENABLE_bm | DAC_OUTEN_bm;
  VREF.DAC0REF = VREF_REFSEL_VDD_gc | VREF_ALWAYSON_bm;

  // Initialize scaled sine table with full amplitude
  amplitude_update(100);

  // Enable DAC updates (use existing TCA0 timer from main.c)
  dac_update = 1;

  _delay_us(50);
}

//================================
// Timer TCA1 Initialization
//================================

static void init_timer_tca1(void) {
  // Use AOS timer dispatcher for 1ms base tick
  aos_tca1_start_1ms();
}

// Global timing variables - managed entirely by TCA1 ISR
static volatile uint32_t adc_sample_timer = 0;   // 10ms ADC sampling
static volatile uint32_t adc_process_timer = 0;  // 100ms processing
static volatile uint32_t uart_part1_timer = 0;   // 3000ms Part 1 output
static volatile uint32_t uart_part2_timer = 0;   // 2000ms Part 2 output
static volatile uint32_t button_check_timer = 0; // 50ms button debounce

/**
 * @brief TCA1 ISR - LabTest3
 */
static void labtest3_tca1_handler(void) {
  labtest3_ms_counter++;

  // Increment all timing counters
  adc_sample_timer++;
  adc_process_timer++;
  uart_part1_timer++;
  uart_part2_timer++;
  button_check_timer++;

  // ADC Sampling every 10ms
  if (adc_sample_timer >= 10) {
    adc_sample_timer = 0;
    adc_sample(); // Sample ADC directly from ISR context
  }
}

//================================
// Button & Interrupt Handlers
//================================

static void init_buttons(void) {
  // Configure PB5 and PB2 as inputs with pull-ups (no interrupts to avoid
  // freezing)
  PORTB.DIRCLR = PIN5_bm | PIN2_bm;
  PORTB.PIN5CTRL = PORT_PULLUPEN_bm;
  PORTB.PIN2CTRL = PORT_PULLUPEN_bm;
}

//================================
// EEPROM Functions
//================================

static void eeprom_save_settings(void) {
  eeprom_settings.led_mode = led_mode;
  eeprom_settings.led_frozen = led_mode_frozen;
  eeprom_settings.dac_mode = dac_mode;
  eeprom_settings.sweep_enabled = sweep_mode_active;

  eeprom_write_block(&eeprom_settings, &eeprom_settings,
                     sizeof(labtest3_eeprom_settings_t));
}

static void eeprom_load_settings(void) {
  eeprom_read_block(&eeprom_settings, &eeprom_settings,
                    sizeof(labtest3_eeprom_settings_t));

  // Check if EEPROM is empty (0xFF) or contains invalid data
  if (eeprom_settings.led_mode == 0xFF || eeprom_settings.led_mode > 3 ||
      eeprom_settings.dac_mode > 1 || eeprom_settings.led_frozen > 1 ||
      eeprom_settings.sweep_enabled > 1) {
    // Default settings - EEPROM corrupted or empty
    aos_printf("[LabTest3] EEPROM invalid/empty - using defaults\r\n");
    eeprom_settings.led_mode = LABTEST3_MODE_OFF;
    eeprom_settings.led_frozen = 0;
    eeprom_settings.dac_mode = LABTEST3_DAC_FIXED;
    eeprom_settings.sweep_enabled = 0; // Sweep starts OFF by default
  }

  // Apply loaded settings
  led_mode = eeprom_settings.led_mode;
  led_mode_frozen =
      (bool)eeprom_settings.led_frozen; // Ensure proper boolean conversion
  dac_mode = eeprom_settings.dac_mode;
  // Restore sweep mode from EEPROM (unlike earlier - don't force OFF)
  sweep_mode_active = (bool)eeprom_settings.sweep_enabled;
}

/**
 * @brief Verify EEPROM contents and print diagnostic information
 */
static void eeprom_verify_contents(void) {
  labtest3_eeprom_settings_t temp_settings;
  eeprom_read_block(&temp_settings, &eeprom_settings,
                    sizeof(labtest3_eeprom_settings_t));

  aos_printf("\r\n=== EEPROM Contents Verification ===\r\n");
  aos_printf("LED Mode: %d (0=OFF, 1=CHASE, 2=BLINK, 3=PWM)\r\n",
             temp_settings.led_mode);
  aos_printf("LED Frozen: %d (0=No, 1=Yes)\r\n", temp_settings.led_frozen);
  aos_printf("DAC Mode: %d (0=Fixed, 1=Sweep)\r\n", temp_settings.dac_mode);
  aos_printf("Sweep Enabled: %d (0=No, 1=Yes)\r\n",
             temp_settings.sweep_enabled);

  if (temp_settings.led_mode == 0xFF) {
    aos_printf("EEPROM Status: EMPTY (contains 0xFF - using defaults)\r\n");
  } else {
    aos_printf("EEPROM Status: VALID (contains saved settings)\r\n");
  }
  aos_printf("=====================================\r\n\r\n");
}

/**
 * @brief Non-blocking button handler (called from main process loop)
 */
static void handle_buttons(void) {
  // Read current button states (active low - pressed = 0)
  bool pb5_current = !(PORTB.IN & PIN5_bm);
  bool pb2_current = !(PORTB.IN & PIN2_bm);

  // Debounce tracking
  static bool pb5_last = false;
  static bool pb2_last = false;
  static uint32_t last_button_check_ms = 0;

  uint32_t current_ms = labtest3_ms_counter;

  // Only check buttons every 50ms for debouncing
  if ((current_ms - last_button_check_ms) < 50) {
    return;
  }
  last_button_check_ms = current_ms;

  // Detect dual button press (both pressed simultaneously)
  if (pb5_current && pb2_current) {
    if (!pb5_pressed || !pb2_pressed) {
      // Start of dual press
      dual_button_start_ms = current_ms;
      pb5_pressed = true;
      pb2_pressed = true;
    } else if ((current_ms - dual_button_start_ms) > 1000) {
      // Held for 1 second - trigger EEPROM save
      aos_printf("\r\n[LabTest3] DUAL BUTTON PRESS - Saving to EEPROM...\r\n");
      eeprom_save_settings();
      aos_printf("[LabTest3] Settings saved!\r\n");
      eeprom_verify_contents(); // Show what was saved

      // Reset to prevent multiple saves
      dual_button_start_ms = current_ms + 2000; // Prevent repeat for 2 seconds
    }
  } else {
    // Handle individual button presses (only if not dual pressing)
    if (!pb5_current && !pb2_current) {
      pb5_pressed = false;
      pb2_pressed = false;
    }

    // PB5 freeze toggle (only if PB2 not pressed)
    if (pb5_current && !pb5_last && !pb2_current) {
      led_mode_frozen = !led_mode_frozen;
      if (led_mode_frozen) {
        aos_printf("\r\n[LabTest3] LED Mode FROZEN at Mode %d\r\n", led_mode);
      } else {
        aos_printf("\r\n[LabTest3] LED Mode UNFROZEN\r\n");
      }
    }

    // PB2 sweep toggle (only if PB5 not pressed)
    if (pb2_current && !pb2_last && !pb5_current) {
      sweep_mode_active = !sweep_mode_active;
      sweep_time_ms = 0;

      if (sweep_mode_active) {
        aos_printf("\r\n[LabTest3] DAC Sweep Mode ENABLED\r\n");
        dac_mode = LABTEST3_DAC_SWEEP;
      } else {
        aos_printf(
            "\r\n[LabTest3] DAC Sweep Mode DISABLED (Fixed Frequency)\r\n");
        dac_mode = LABTEST3_DAC_FIXED;
      }
    }
  }

  // Update last states
  pb5_last = pb5_current;
  pb2_last = pb2_current;
}

//================================
// EEPROM Functions
//================================
// LabTest3 Public Functions
//================================

void labtest3_init(void) {
  labtest3_active = true;

  // Initialize hardware
  init_adc();
  init_dac(); // This will set initial frequency and amplitude
  init_led_port();
  init_pwm_c1(); // Initialize PWM on PIN C1 for Mode 3
  init_buttons();
  // Register timer handler and start 1ms tick for LabTest3
  aos_tca1_register(labtest3_tca1_handler);
  init_timer_tca1(); // Initialize 1ms timer for LabTest3 (using TCA1)

  aos_printf("[LabTest3] PWM initialized: TCA0 WO1 on PIN C1\r\n");
  aos_printf("[LabTest3] TCA0.SINGLE.CTRLB = 0x%02X\r\n", TCA0.SINGLE.CTRLB);
  aos_printf("[LabTest3] TCA0.SINGLE.PER = %u, CMP1BUF = %u\r\n",
             TCA0.SINGLE.PER, TCA0.SINGLE.CMP1BUF);

  // Set initial DAC frequency (TCA0 already initialized in main.c)
  frequency_update(10); // Start at 10 Hz

  // Load saved settings from EEPROM
  eeprom_load_settings();

  // TEMPORARY: Force unfrozen for debugging
  led_mode_frozen = false;
  aos_printf("[LabTest3] FORCING unfrozen state for debugging\r\n");

  // Debug: Show what was loaded
  aos_printf("[LabTest3] Loaded: mode=%d, frozen=%d, dac_mode=%d, sweep=%d\r\n",
             led_mode, led_mode_frozen, dac_mode, sweep_mode_active);

  // Verify and display EEPROM contents for debugging
  eeprom_verify_contents();

  // Initialize timing variables
  labtest3_ms_counter = 0;
  adc_count = 0;
  adc_sum = 0;
}

void labtest3_process(void) {
  // LabTest3 PRIORITY PROCESSING - timer handles most timing

  // Process ADC averaging and calculations every 100ms (timer-driven)
  if (adc_process_timer >= 100) {
    adc_process_timer = 0;

    // Get averaged ADC value
    uint16_t avg_adc = get_averaged_adc();
    float vpot = adc_to_voltage(avg_adc);

    if (!led_mode_frozen) {
      // Formula: mode = ((int)(Vpot * 100)) % 4
      led_mode = ((int)(vpot * 100.0f)) % 4;
    }

    // Update PWM duty cycle every 100ms when in MODE 3 (even when frozen)
    if (led_mode == LABTEST3_MODE_PWM) {
      // Formula: duty % = (Vpot/Vref) * 100
      float duty_percent = (vpot / 3.3f) * 100.0f;

      // Clamp duty cycle to 0-100%
      if (duty_percent > 100.0f)
        duty_percent = 100.0f;
      if (duty_percent < 0.0f)
        duty_percent = 0.0f;

      // Convert to compare value based on current TCA0 period
      uint16_t compare_value =
          (uint16_t)((duty_percent / 100.0f) * (float)TCA0.SINGLE.PER);

      // Update PWM duty cycle on PIN C1 (TCA0 WO1)
      TCA0.SINGLE.CMP1BUF = compare_value;
    }

    // Part 2: Update DAC frequency and amplitude
    // Amplitude formula from spec: Amplitude = Vpot/3.3 (normalized to 0-1)
    dac_amplitude = vpot / 3.3f; // Normalized amplitude (0 to 1)

    // Update amplitude scaling
    int new_amplitude_percent = (int)(dac_amplitude * 100.0f);
    if (new_amplitude_percent < 0)
      new_amplitude_percent = 0;
    if (new_amplitude_percent > 100)
      new_amplitude_percent = 100;
    amplitude_update(new_amplitude_percent);

    if (sweep_mode_active) {
      sweep_time_ms += 100;         // Add 100ms to sweep timer
      if (sweep_time_ms >= 10000) { // 10 seconds = full sweep up and down
        sweep_time_ms = 0;
      }

      // Calculate frequency: 10Hz to 100Hz over 5 seconds, back to 10Hz over 5
      // seconds
      uint32_t new_freq;
      if (sweep_time_ms < 5000) {
        // f(t) = 10 + 18*t where t is in seconds
        new_freq = 10 + (18 * sweep_time_ms / 1000);
      } else {
        // f(t) = 100 - 18*(t-5) where t is in seconds
        new_freq = 100 - (18 * (sweep_time_ms - 5000) / 1000);
      }
      frequency_update(new_freq);
    } else {
      // Fixed frequency mode: f = 10 + (Vpot * 90) Hz
      uint32_t new_freq = 10 + (uint32_t)(vpot * 90.0f);
      if (new_freq < 10)
        new_freq = 10;
      if (new_freq > 100)
        new_freq = 100;
      frequency_update(new_freq);
    }
  }

  // UART Output Part 1 every 3 seconds (timer-driven)
  if (uart_part1_timer >= 3000) {
    uart_part1_timer = 0;

    float vpot = adc_to_voltage(adc_averaged);
    float duty_percent = (vpot / 3.3f) * 100.0f;

    // Format floats using dtostrf to avoid printf issues
    char volt_str[16];
    char duty_str[16];
    dtostrf(vpot, 4, 2, volt_str);
    dtostrf(duty_percent, 4, 2, duty_str);

    // UART Format: "Voltage = XX.XX V, Duty = XX.XX %, Mode = X
    // (frozen/unfrozen)"
    const char *freeze_status = led_mode_frozen ? "frozen" : "unfrozen";
    aos_printf("Voltage = %s V, Duty = %s %%, Mode = %d (%s)\r\n", volt_str,
               duty_str, led_mode, freeze_status);
  }

  // UART Output Part 2 every 2 seconds (timer-driven)
  if (uart_part2_timer >= 2000) {
    uart_part2_timer = 0;

    float vpot = adc_to_voltage(adc_averaged);

    // Calculate actual amplitude voltage (dac_amplitude is 0-1 normalized)
    float amplitude_voltage = dac_amplitude * 3.3f;

    // Format floats
    char vpot_str[16];
    char amp_str[16];
    dtostrf(vpot, 3, 2, vpot_str);
    dtostrf(amplitude_voltage, 3, 2, amp_str);

    // Get mode string based on sweep active state
    const char *dac_mode_str = sweep_mode_active ? "Sweep" : "Fixed";

    // UART Format: "Vpot = X.XX V, A = X.XX V, f = XXX Hz, Mode = Sweep/Fixed"
    aos_printf("Vpot = %s V, A = %s V, f = %lu Hz, Mode = %s\r\n", vpot_str,
               amp_str, (unsigned long)dac_frequency_hz, dac_mode_str);
  }

  // Handle button presses (timer-driven debouncing)
  if (button_check_timer >= 50) {
    button_check_timer = 0;
    handle_buttons();
  }

  // Update LED animation based on current mode (always)
  switch (led_mode) {
  case LABTEST3_MODE_OFF:
    led_mode_off();
    break;
  case LABTEST3_MODE_CHASE:
    led_mode_chase();
    break;
  case LABTEST3_MODE_BLINK_8HZ:
    led_mode_blink_8hz();
    break;
  case LABTEST3_MODE_PWM:
    // PWM duty cycle is updated every 100ms in the timer block above
    // Just ensure PORTD LEDs are off (PWM is on PIN C1)
    PORTD.OUTCLR = 0b10111111;
    break;
  }

  // DAC waveform generation is handled automatically by TCA0 ISR in main.c
  // No action needed here - just ensure frequency_update() and
  // amplitude_update() are called above when processing ADC values
}

void labtest3_show_welcome(void) {
  aos_send("\r\n");
  aos_send("+=====================================================+\r\n");
  aos_send("|                   LABTEST 3                        |\r\n");
  aos_send("|  Adaptive LED Patterns & DAC Waveform Generation   |\r\n");
  aos_send("+=====================================================+\r\n\r\n");

  aos_send("=== SYSTEM STATUS ===\r\n");
  aos_send("  * Main Loop: Continuous ADC sampling\r\n");
  aos_send("  * Update Period: Every 100ms (ADC averaging complete)\r\n");
  aos_send("  * Type 'E' to exit and return to AOS\r\n\r\n");
}

bool labtest3_is_active(void) { return labtest3_active; }

void labtest3_exit(void) {
  // Save current settings to EEPROM
  eeprom_save_settings();

  labtest3_active = false;
  // Release TCA1 so other labs can use it
  aos_tca1_disable();
  aos_tca1_unregister();
  aos_send("\r\nExiting LabTest3, returning to AOS...\r\n\r\n");

  _delay_ms(500);
}
