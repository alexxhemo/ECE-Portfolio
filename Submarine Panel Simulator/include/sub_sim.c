/**
 * @file sub_sim.c
 * @author Arturo Salinas
 * @date 2025-11-19
 * @brief Submarine Simulator Implementation
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif

#include "sub_sim.h"
#include "i2c_lib_S25.h"
#include "lcd.h"
#include "melody.h"
#include "pitches.h"
#include "rtc.h"
#include "uart.h"
#include "ui.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>
// debugging enable or disable
#define DEBUG 0

//================================
// Pin Assignments (AVR128DB48)
//================================
// Circuit Breaker Buttons (Active Low with Pull-ups)
#define BTN_SYS_RESET_PIN PIN2_bm   // PB2 (already configured in main.c)
#define BTN_O2_CB_PIN PIN3_bm       // PC3
#define BTN_RX_CB_PIN PIN4_bm       // PC4
#define BTN_REACTOR_PWR_PIN PIN5_bm // PC5
#define BTN_MAIN_BKR_PIN PIN2_bm    // PC2

// LED Y-Pattern (Active High)
#define LED_TOP_RIGHT_PIN PIN0_bm  // PD0 Yellow - Air Quality
#define LED_TOP_LEFT_PIN PIN1_bm   // PD1 Blue - Torpedo Inbound
#define LED_BOT_TOP_RIGHT PIN2_bm  // PD2 Red - Backup O2 In Service
#define LED_BOT_TOP_LEFT PIN3_bm   // PD3 Red - Primary O2 In Service
#define LED_TOP_CENTER_PIN PIN4_bm // PD4 White - Reactor Main Power
#define LED_BOT_CENTER_PIN PIN5_bm // PD5 Green - Reactor Status

/*
    Master Potentiometer
    - PE0 (AIN8) → Master Pot
    - Pot ends: VCC (3.3V) ↔ GND

    Joystick
    - PE1 (AIN9) → Joystick X
    - PE2 (AIN10) → Joystick Y

    MiCS-6814 Gas Sensing Nodes
    You will have 3 voltage dividers, each feeding one ADC pin:
    - PE3 (AIN11) → CO sensor
    - PF4 (AIN12) → NH₃ sensor
    - PF5 (AIN13) → NO₂ sensor
 */

// ADC Channel for Potentiometer
#define POT_ADC_CHANNEL ADC_MUXPOS_AIN8_gc // PE0

// ADC Channel for Joystick
// Center ~ 2040,2040
// Max ~ 4000, 4000
// Min ~ 20, 20
#define JOY_X_CHANNEL ADC_MUXPOS_AIN9_gc  // PE1
#define JOY_Y_CHANNEL ADC_MUXPOS_AIN10_gc // PE2

// ADC Channel for Gas Sensor
#define CO_SENS_CHANNEL ADC_MUXPOS_AIN11_gc  // PE3
#define NH3_SENS_CHANNEL ADC_MUXPOS_AIN20_gc // PF4
#define NO2_SENS_CHANNEL ADC_MUXPOS_AIN21_gc // PF5

//================================
// Sound / Melody Definitions
//================================
#define Gb5 831
#define E2 82
#define E6 1319
#define Gb6 1661
#define F5 698
#define C2 65
#define D2 73
#define Fb5 740
#define C6 1047
#define E4 330
#define Fb6 1480
#define G5 784
#define Ab5 932
#define Db2 78
#define Cb2 69
#define G1 49
#define B6 1976
#define Cb6 1109

// Anchors Aweigh Melody Definitions
#define Ab3 233
#define F3 175
#define A3 220
#define C4 262
#define D4 294
#define F4 349
#define G4 392
// E4 330 is already defined
#define B3 247
#define G3 196
#define A4 440

static const int mission_complete_notes[][3] = {
    {F3, 923, 0},   {A3, 462, 0},   {C4, 462, 0}, {D4, 692, 0},  {A3, 231, 0},
    {D4, 923, 0},   {F4, 923, 0},   {G4, 462, 0}, {C4, 462, 0},  {F4, 923, 0},
    {D4, 923, 0},   {F4, 462, 0},   {D4, 462, 0}, {C4, 462, 0},  {D4, 462, 0},
    {E4, 462, 0},   {F4, 462, 0},   {B3, 462, 0}, {D4, 462, 0},  {G4, 462, 0},
    {F4, 462, 0},   {E4, 231, 231}, {C4, 462, 0}, {Ab3, 462, 0}, {G3, 462, 0},
    {F3, 923, 0},   {A3, 462, 0},   {C4, 462, 0}, {D4, 692, 0},  {A3, 231, 0},
    {D4, 923, 0},   {F4, 923, 0},   {G4, 462, 0}, {C4, 462, 0},  {F4, 923, 0},
    {D4, 923, 0},   {F4, 462, 0},   {D4, 462, 0}, {C4, 462, 0},  {D4, 462, 0},
    {E4, 462, 0},   {F4, 462, 0},   {A4, 346, 0}, {C4, 115, 0},  {B3, 231, 0},
    {C4, 231, 0},   {A4, 346, 0},   {C4, 115, 0}, {B3, 231, 0},  {C4, 231, 0},
    {F3, 231, 231}, {F4, 231, 0},
};

static void sub_sim_play_mission_complete(void) {
  for (size_t i = 0;
       i < sizeof(mission_complete_notes) / sizeof(mission_complete_notes[0]);
       i++) {
    int freq = mission_complete_notes[i][0];
    // Speed up tempo by 20% (multiply duration by 0.8)
    int dur = (int)(mission_complete_notes[i][1] * 0.8);
    int pause = (int)(mission_complete_notes[i][2] * 0.8);

    melody_tone(freq);
    for (volatile uint16_t j = 0; j < dur; j++)
      _delay_ms(1);

    melody_no_tone();
    if (pause > 0) {
      for (volatile uint16_t j = 0; j < pause; j++)
        _delay_ms(1);
    }
  }
}

// Dive Alarm Melody (Frequency, Duration ms, Pause ms)
// Note: Pause is delay AFTER note stops.
// Replaced by programmatic sweep in sub_sim_play_dive_alarm

// Sound Modes
typedef enum {
  SOUND_SILENT,
  SOUND_SONAR_PING,    // Normal dive: B5 ping every 3s
  SOUND_GENERAL_ALARM, // 400Hz decay (pulse)
  SOUND_REACTOR_ALARM  // 830Hz -> 1390Hz siren
} sound_mode_t;

// Sound State
static volatile sound_mode_t current_sound_mode = SOUND_SILENT;
static uint32_t sound_last_tick = 0;
static uint8_t sound_state = 0;

//================================
// Event / Casualty Definitions
//================================
typedef enum { EVENT_LOW_O2, EVENT_REACTOR, EVENT_TORPEDO } sim_event_t;

static sim_event_t phase_events[3] = {EVENT_LOW_O2, EVENT_REACTOR,
                                      EVENT_TORPEDO};
static sim_event_t current_active_event = EVENT_LOW_O2; // Placeholder

// Event State Variables (Moved from functions to allow reset)
static bool o2_alarm_triggered = false;
static bool o2_awaiting_response = false;
static bool backup_o2_active = false; // Track if we are on backup O2

static bool rx_alarm_triggered = false;
static bool rx_awaiting_transfer = false;

static bool torp_alarm_triggered = false;
static bool torp_minigame_active = false;
static uint8_t torp_last_dir = 0;

//================================
// Forward Declarations
//================================
static void sub_sim_led_set(uint8_t pattern);
static void sub_sim_led_flash(uint8_t times, uint16_t delay_ms);
static void sub_sim_set_sound_mode(sound_mode_t mode);
static void sub_sim_update_sound(void);
static void sub_sim_play_dive_alarm(void);
static void sub_sim_handle_setup(void);

// Event Handlers
static bool sub_sim_run_event_low_o2(void);
static bool sub_sim_run_event_reactor(void);
static bool sub_sim_run_event_torpedo(void);
static bool sub_sim_run_event_generic(sim_event_t event);
static void sub_sim_reset_event_flags(void);

//================================
// State Variables
//================================
// Removed old phase handlers
static void sub_sim_handle_phase_100(void);

//================================
// State Variables
//================================
static volatile bool active = false;
static volatile dive_state_t current_state = DIVE_IDLE;
static volatile uint16_t dive_duration_seconds = 0;
static volatile uint16_t dive_seconds_elapsed = 0; // RTC driven
static volatile uint32_t dive_start_tick = 0;
static volatile uint32_t current_tick = 0;
static volatile uint32_t phase_start_tick = 0;
static volatile uint32_t response_timeout_ticks = 0;

// Dive phase timestamps (in seconds now, for RTC)
static volatile uint16_t phase_25_sec = 0;
static volatile uint16_t phase_50_sec = 0;
static volatile uint16_t phase_75_sec = 0;
static volatile uint16_t phase_100_sec = 0;

// Phase completion flags
static volatile bool phase_25_complete = false;
static volatile bool phase_50_complete = false;
static volatile bool phase_75_complete = false;
static volatile bool phase_100_complete = false;

// Alarm state
static volatile bool alarm_active = false;

// Torpedo evasion minigame state
static volatile uint8_t torpedo_sequence_index = 0;
// 1=UP, 2=DOWN, 3=LEFT, 4=RIGHT
static uint8_t torpedo_sequence[8];

static void sub_sim_generate_torpedo_sequence(void) {
  for (int i = 0; i < 8; i++) {
    torpedo_sequence[i] = (rand() % 4) + 1;
  }
}

static const char *sub_sim_get_dir_str(uint8_t dir) {
  switch (dir) {
  case 1:
    return "MOVE UP!!!!     ";
  case 2:
    return "DIVE! DOWN!!!!  ";
  case 3:
    return "MOVE LEFT!!!!   ";
  case 4:
    return "MOVE RIGHT!!!!  ";
  default:
    return "HOLD STEADY!    ";
  }
}

//================================
// Public Interface Implementation
//================================

// RTC Callback
static void sub_sim_rtc_tick_handler(void) {
  if (active && current_state >= DIVE_DESCENDING &&
      current_state < DIVE_COMPLETE) {
    dive_seconds_elapsed++;
  }
}

void sub_sim_init(void) {
  // Configure button inputs (active low with pull-ups) on PORTC
  // PC3, PC4, PC5, PC2
  PORTC.DIRCLR =
      BTN_O2_CB_PIN | BTN_RX_CB_PIN | BTN_REACTOR_PWR_PIN | BTN_MAIN_BKR_PIN;
  PORTC.PIN3CTRL = PORT_PULLUPEN_bm;
  PORTC.PIN4CTRL = PORT_PULLUPEN_bm;
  PORTC.PIN5CTRL = PORT_PULLUPEN_bm;
  PORTC.PIN2CTRL = PORT_PULLUPEN_bm;

  // Configure LED outputs (active high) on PORTD
  PORTD.DIRSET = LED_TOP_RIGHT_PIN | LED_TOP_LEFT_PIN | LED_TOP_CENTER_PIN |
                 LED_BOT_TOP_LEFT | LED_BOT_TOP_RIGHT | LED_BOT_CENTER_PIN;
  PORTD.OUTCLR = 0xFF; // All off initially (Reactor Shutdown State)

  // Initialize melody system for sonar pings
  melody_init();

  // Initialize ADC
  sub_sim_init_adc();

  // Initialize RTC
  rtc_init();
  rtc_set_callback(sub_sim_rtc_tick_handler);

  // Initial LED State: Lamp Test then Shutdown State
  sub_sim_led_flash(1, 500); // Flash all once
  PORTD.OUTCLR = 0xFF;       // All off

  // Reset state
  current_state = DIVE_IDLE;
  active = true;
  current_tick = 0;
  dive_seconds_elapsed = 0;
  phase_25_complete = false;
  phase_50_complete = false;
  phase_75_complete = false;
  phase_100_complete = false;
  alarm_active = false;

  sub_sim_reset_event_flags();
}

static void sub_sim_reset_event_flags(void) {
  o2_alarm_triggered = false;
  o2_awaiting_response = false;
  backup_o2_active = false;
  rx_alarm_triggered = false;
  rx_awaiting_transfer = false;
  torp_alarm_triggered = false;
  torp_minigame_active = false;
  torp_last_dir = 0;
  torpedo_sequence_index = 0;
}

static void sub_sim_shuffle_events(void) {
  // Simple shuffle of the 3 events
  phase_events[0] = EVENT_LOW_O2;
  phase_events[1] = EVENT_REACTOR;
  phase_events[2] = EVENT_TORPEDO;

  for (int i = 2; i > 0; i--) {
    int j = rand() % (i + 1);
    sim_event_t temp = phase_events[i];
    phase_events[i] = phase_events[j];
    phase_events[j] = temp;
  }
}

static void sub_sim_play_dive_alarm(void) {
  for (int i = 0; i < 3; i++) {
    // Rise
    for (uint16_t f = 250; f < 800; f += 15) {
      melody_tone(f);
      _delay_ms(5);
    }

    // Fall
    for (uint16_t f = 800; f > 200; f -= 5) {
      melody_tone(f);
      _delay_ms(5);
    }

    melody_no_tone();
    _delay_ms(500); // Pause between blasts
  }
}

static void sub_sim_set_sound_mode(sound_mode_t mode) {
  if (current_sound_mode != mode) {
    current_sound_mode = mode;
    melody_no_tone(); // Stop current sound immediately
    sound_state = 0;
    sound_last_tick = current_tick;
  }
}

static void sub_sim_update_sound(void) {
  // Flash LEDs based on state
  static uint32_t last_flash_tick = 0;
  static bool flash_state = false;

  // Faster flash for casualties (100ms)
  if (current_tick - last_flash_tick >= (SUB_SIM_TICKS_PER_SECOND / 10)) {
    last_flash_tick = current_tick;
    flash_state = !flash_state;

    // --- Air Quality / Low O2 Logic ---
    // PD0 (Air Quality)
    // Flash if: Low O2 Event Active
    // Note: Removed gas_alarm check to prevent flashing before event

    // Check if O2 event is currently running
    bool o2_running = false;
    if ((current_state == DIVE_PHASE_25 && phase_events[0] == EVENT_LOW_O2 &&
         !phase_25_complete) ||
        (current_state == DIVE_PHASE_50 && phase_events[1] == EVENT_LOW_O2 &&
         !phase_50_complete) ||
        (current_state == DIVE_PHASE_75 && phase_events[2] == EVENT_LOW_O2 &&
         !phase_75_complete)) {
      o2_running = true;
    }

    // Invert logic: High Resistance (Clean Air) = High Voltage (4095)
    // Low Resistance (Gas) = Low Voltage (< 2500)
    bool gas_detected = (sub_sim_read_co() < 2500);

    if (o2_running || gas_detected) {
      if (flash_state)
        PORTD.OUTSET = LED_TOP_RIGHT_PIN;
      else
        PORTD.OUTCLR = LED_TOP_RIGHT_PIN;
    } else {
      // Steady ON if normal lineup established (after setup)
      if (current_state >= DIVE_DESCENDING)
        PORTD.OUTSET = LED_TOP_RIGHT_PIN;
    }

    // --- Torpedo Logic ---
    // PD1 (Torpedo)
    // Flash if Torpedo Event Active
    bool torp_running = false;
    if ((current_state == DIVE_PHASE_25 && phase_events[0] == EVENT_TORPEDO &&
         !phase_25_complete) ||
        (current_state == DIVE_PHASE_50 && phase_events[1] == EVENT_TORPEDO &&
         !phase_50_complete) ||
        (current_state == DIVE_PHASE_75 && phase_events[2] == EVENT_TORPEDO &&
         !phase_75_complete)) {
      torp_running = true;
    }

    if (torp_running) {
      if (flash_state)
        PORTD.OUTSET = LED_TOP_LEFT_PIN;
      else
        PORTD.OUTCLR = LED_TOP_LEFT_PIN;
    } else {
      // Steady ON if normal lineup established
      if (current_state >= DIVE_DESCENDING)
        PORTD.OUTSET = LED_TOP_LEFT_PIN;
    }

    // --- Reactor Logic ---
    // PD5 (Reactor Status)
    // Flash if Reactor Event Active
    bool rx_running = false;
    if ((current_state == DIVE_PHASE_25 && phase_events[0] == EVENT_REACTOR &&
         !phase_25_complete) ||
        (current_state == DIVE_PHASE_50 && phase_events[1] == EVENT_REACTOR &&
         !phase_50_complete) ||
        (current_state == DIVE_PHASE_75 && phase_events[2] == EVENT_REACTOR &&
         !phase_75_complete)) {
      rx_running = true;
    }

    if (rx_running) {
      if (flash_state)
        PORTD.OUTSET = LED_BOT_CENTER_PIN;
      else
        PORTD.OUTCLR = LED_BOT_CENTER_PIN;
    } else {
      // Steady ON if normal lineup established (Reactor Critical or later)
      if (current_state >= DIVE_REACTOR_START) {
        // Note: In DIVE_REACTOR_START, it turns on at step 1.
        // We can just leave it be if it was turned on there.
        // But here we might overwrite it.
        // Let's assume if we are past DIVE_DESCENDING it should be ON.
        if (current_state >= DIVE_DESCENDING)
          PORTD.OUTSET = LED_BOT_CENTER_PIN;
      }
    }

    // --- O2 Scrubber Status ---
    // PD3 (Primary) vs PD2 (Backup)
    if (current_state >= DIVE_DESCENDING) {
      if (backup_o2_active) {
        PORTD.OUTSET = LED_BOT_TOP_RIGHT; // Backup ON
        PORTD.OUTCLR = LED_BOT_TOP_LEFT;  // Primary OFF
      } else {
        PORTD.OUTCLR = LED_BOT_TOP_RIGHT; // Backup OFF
        PORTD.OUTSET = LED_BOT_TOP_LEFT;  // Primary ON
      }
    }
  }

  // If silent, ensure tone is off
  if (current_sound_mode == SOUND_SILENT) {
    return;
  }

  uint32_t elapsed = current_tick - sound_last_tick;

  switch (current_sound_mode) {
  case SOUND_SONAR_PING:
    // Ping every 3 seconds
    // State 0: Wait for 3s (or play immediately if first time?)
    // Let's play immediately on switch, then wait.
    // State 0: Playing B5 (150ms)
    // State 1: Waiting (3850ms) - Total 4s period
    if (sound_state == 0) {
      if (!melody_is_playing())
        melody_tone(988); // B5
      if (elapsed >= 150) {
        melody_no_tone();
        sound_last_tick = current_tick;
        sound_state = 1;
      }
    } else {
      if (elapsed >= 3850) {
        sound_last_tick = current_tick;
        sound_state = 0;
      }
    }
    break;

  case SOUND_GENERAL_ALARM:
    // 400Hz decay tone, 520ms with repeat
    // Implemented as: 400Hz ON for 400ms, OFF for 120ms
    if (sound_state == 0) { // ON
      if (!melody_is_playing())
        melody_tone(400);
      if (elapsed >= 400) {
        melody_no_tone();
        sound_last_tick = current_tick;
        sound_state = 1;
      }
    } else { // OFF
      if (elapsed >= 120) {
        sound_last_tick = current_tick;
        sound_state = 0;
      }
    }
    break;

  case SOUND_REACTOR_ALARM:
    // 830Hz (300ms) -> 1390Hz (200ms)
    if (sound_state == 0) { // 830Hz
      // We can't easily check frequency, so just set it if not set recently?
      // melody_tone is cheap enough to call repeatedly?
      // It disables TCB, inits DAC... maybe not super cheap.
      // But we need to ensure it's playing.
      // Let's assume if we are in this state, we set it once.
      // But we don't have "enter state" logic here easily.
      // We can check elapsed == 0?
      if (elapsed == 0)
        melody_tone(830);

      if (elapsed >= 300) {
        sound_last_tick = current_tick;
        sound_state = 1;
        melody_tone(1390);
      }
    } else { // 1390Hz
      if (elapsed >= 200) {
        sound_last_tick = current_tick;
        sound_state = 0;
        melody_tone(830);
      }
    }
    break;

  default:
    break;
  }
}

void sub_sim_process(void) {
  if (!active)
    return;

  // Update sound system
  sub_sim_update_sound();

  // Check for EXIT command from UART
  char ch;
  static char exit_buffer[5] = {0};
  static uint8_t exit_index = 0;

  if (uart_receive_char(&ch)) {
    // Echo character
    uart_send_char(ch);

    // Check for EXIT command
    if (ch == '\r' || ch == '\n') {
      exit_buffer[exit_index] = '\0';
      // Check if command is EXIT
      if (strcmp(exit_buffer, "EXIT") == 0 ||
          strcmp(exit_buffer, "exit") == 0) {
        sub_sim_exit();
        aos_printf("\r\nReturning to AOS...\r\n");
        aos_printf("AOS> ");
        exit_index = 0;
        return;
      }
      exit_index = 0;
    } else if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
      if (exit_index < 4) {
        exit_buffer[exit_index++] = ch;
      }
    }
  }

  switch (current_state) {
  case DIVE_IDLE:
    // Waiting for user to start simulation
    break;

  case DIVE_SETUP:
    sub_sim_handle_setup();
    break;

  case DIVE_REACTOR_START:
    // Reactor Startup Sequence
    // 1. Display "REACTOR STARTUP"
    // 2. Wait for "CLOSE MAIN BKR"
    {
      static uint8_t startup_step = 0;
      static uint32_t startup_timer = 0;

      if (startup_step == 0) {
        sub_sim_lcd_print("REACTOR STARTUP ", "STANDBY...      ");
        // Simulate startup delay
        startup_timer = current_tick;
        startup_step = 1;
      } else if (startup_step == 1) {
        if (current_tick - startup_timer > (2 * SUB_SIM_TICKS_PER_SECOND)) {
          sub_sim_lcd_print("REACTOR CRITICAL", "CLOSE MAIN BKR  ");
          // Reactor Critical: Turn ON Reactor Status LED (PD5)
          PORTD.OUTSET = LED_BOT_CENTER_PIN;
          startup_step = 2;
        }
      } else if (startup_step == 2) {
        // Wait for Main Breaker button press (PC2)
        if (sub_sim_button_main_bkr_pressed()) {
          _delay_ms(50); // Debounce

          // Turn ON Main Power LED (White - PD4)
          PORTD.OUTSET = LED_TOP_CENTER_PIN;

          // Establish Normal Lineup
          // PD0 (Air Quality) ON
          // PD1 (Torpedo) ON
          // PD3 (Primary O2) ON
          PORTD.OUTSET =
              LED_TOP_RIGHT_PIN | LED_TOP_LEFT_PIN | LED_BOT_TOP_LEFT;

          sub_sim_lcd_print("MAIN POWER ON!   ", "REACTOR IS CRITICAL!  ");
          _delay_ms(2000);
          sub_sim_lcd_print("ALL SPACES ARE...  ", "RIGGED FOR DIVE! ");
          _delay_ms(2000);
          sub_sim_lcd_print("COMMENCING DIVE ", "COMMANDER....   ");
          _delay_ms(2000);

          sub_sim_lcd_print("DIVE... DIVE..! ", "                ");
          sub_sim_play_dive_alarm();

          // Delay before first ping
          _delay_ms(2000);

          dive_start_tick = current_tick;
          dive_seconds_elapsed = 0;
          current_state = DIVE_DESCENDING;
          sub_sim_set_sound_mode(SOUND_SONAR_PING);

          sub_sim_lcd_print("DESCENDING...   ", "DEPTH: 0 FT     ");
        }
      }
    }
    break;

  case DIVE_READY:
    // Deprecated state, logic moved to DIVE_REACTOR_START
    break;

  case DIVE_DESCENDING:
    // Update depth display
    {
      static uint16_t last_depth = 0xFFFF;
      uint16_t current_depth = 0;
      if (dive_duration_seconds > 0) {
        current_depth = (uint16_t)((uint32_t)dive_seconds_elapsed * 800UL /
                                   dive_duration_seconds);
      }

      if (current_depth != last_depth) {
        char depth_str[17];
        snprintf(depth_str, sizeof(depth_str), "DEPTH: %d FT    ",
                 current_depth);
        sub_sim_lcd_print("DESCENDING...   ", depth_str);
        last_depth = current_depth;
      }
    }

    // Check for phase transitions using RTC seconds
    if (!phase_25_complete && dive_seconds_elapsed >= phase_25_sec) {
      current_state = DIVE_PHASE_25;
      phase_start_tick = current_tick;
      response_timeout_ticks =
          SUB_SIM_RESPONSE_TIMEOUT_SECONDS * SUB_SIM_TICKS_PER_SECOND;
    } else if (!phase_50_complete && dive_seconds_elapsed >= phase_50_sec) {
      current_state = DIVE_PHASE_50;
      phase_start_tick = current_tick;
      response_timeout_ticks =
          SUB_SIM_RESPONSE_TIMEOUT_SECONDS * SUB_SIM_TICKS_PER_SECOND;
    } else if (!phase_75_complete && dive_seconds_elapsed >= phase_75_sec) {
      current_state = DIVE_PHASE_75;
      phase_start_tick = current_tick;
      response_timeout_ticks =
          SUB_SIM_RESPONSE_TIMEOUT_SECONDS * SUB_SIM_TICKS_PER_SECOND;
    } else if (!phase_100_complete && dive_seconds_elapsed >= phase_100_sec) {
      current_state = DIVE_PHASE_100;
    }
    break;

  case DIVE_PHASE_25:
    if (sub_sim_run_event_generic(phase_events[0])) {
      phase_25_complete = true;
      current_state = DIVE_DESCENDING;
      // Check next phase
      if (dive_seconds_elapsed >= phase_50_sec) {
        current_state = DIVE_PHASE_50;
      }
    }
    break;

  case DIVE_PHASE_50:
    if (sub_sim_run_event_generic(phase_events[1])) {
      phase_50_complete = true;
      current_state = DIVE_DESCENDING;
      // Check next phase
      if (dive_seconds_elapsed >= phase_75_sec) {
        current_state = DIVE_PHASE_75;
      }
    }
    break;

  case DIVE_PHASE_75:
    if (sub_sim_run_event_generic(phase_events[2])) {
      phase_75_complete = true;
      current_state = DIVE_DESCENDING;
      // Check next phase
      if (dive_seconds_elapsed >= phase_100_sec) {
        current_state = DIVE_PHASE_100;
      }
    }
    break;

  case DIVE_PHASE_100:
    sub_sim_handle_phase_100();
    break;

  case DIVE_SURFACING:
    sub_sim_lcd_print("SURFACING...    ", "MISSION SUCCESS ");
    sub_sim_play_dive_alarm();
    _delay_ms(3000);
    current_state = DIVE_COMPLETE;
    break;

  case DIVE_COMPLETE:
    // Return to main menu (setup screen)
    sub_sim_show_welcome();
    break;

  case DIVE_FAILED:
    sub_sim_lcd_print("DIVE FAILED     ", "MISSION FAILED  ");
    sub_sim_play_dive_alarm();
    sub_sim_led_flash(10, 100);
    _delay_ms(3000);
    active = false;
    break;
  }
}

bool sub_sim_is_active(void) { return active; }

void sub_sim_show_welcome(void) {
  aos_printf("\r\n");
  aos_printf(
      "+-----------------------------------------------------------+\r\n");
  aos_printf(
      "|           SUBMARINE SIMULATOR - FINAL PROJECT             |\r\n");
  aos_printf(
      "|           BY: Arturo Salinas and Alex Xhemo               |\r\n");
  aos_printf(
      "|                      ACTIVE!                              |\r\n");
  aos_printf(
      "+-----------------------------------------------------------+\r\n");
  aos_printf("\r\n");
  aos_printf("System Status:\r\n");
  aos_printf("    O       O   (Top Left - O2 Fault, Top Right - Torpedo "
             "Evasion) \r\n");
  aos_printf("      O   O     (primary O2 or secondary O2 online)\r\n");
  aos_printf("        O       (Main Circuit breaker)\r\n");
  aos_printf("        O       (Reactor Status) \r\n");
  aos_printf("  - Potentiometer: Dive duration selector (0-300 sec)\r\n");
  aos_printf("\r\n");
  aos_printf("Circuit Breakers (Buttons):\r\n");
  aos_printf("  - BUT1: Reactor Startup\r\n");
  aos_printf("  - BUT2: Diesel Generator Startup\r\n");
  aos_printf("  - BUT3: O2 Circuit Breaker\r\n");
  aos_printf("  - BUT4: Main PWR Breaker\r\n");
  aos_printf("\r\n");
  aos_printf("Commence Reactor Startup To Begin Mission\r\n");
  aos_printf("Type EXIT to return to AOS\r\n");
  aos_printf("\r\n");

  // Display on LCD
  sub_sim_lcd_print("SUB SIMULATOR   ", "SET DIVE TIME   ");
  _delay_ms(1000);

  current_state = DIVE_SETUP;
}

void sub_sim_exit(void) {
  active = false;
  current_state = DIVE_IDLE;
  sub_sim_set_sound_mode(SOUND_SILENT);
  sub_sim_led_set(0x00);
  sub_sim_lcd_print("EXITING...      ", "RETURN TO AOS   ");
  _delay_ms(1000);
  LCD_sendCommand(0x01); // Clear display
}

void sub_sim_tick_handler(void) {
  if (active) {
    current_tick++;
  }
}

//================================
// GPIO Button Interface
//================================

bool sub_sim_button_sys_reset_pressed(void) {
  // Deprecated/Unused in this mapping
  return false;
}

bool sub_sim_button_o2_cb_pressed(void) { return !(PORTC.IN & BTN_O2_CB_PIN); }

bool sub_sim_button_rx_cb_pressed(void) { return !(PORTC.IN & BTN_RX_CB_PIN); }

bool sub_sim_button_reactor_startup_pressed(void) {
  return !(PORTC.IN & BTN_REACTOR_PWR_PIN);
}

bool sub_sim_button_main_bkr_pressed(void) {
  return !(PORTC.IN & BTN_MAIN_BKR_PIN);
}

//================================
// Status Display
//================================

dive_state_t sub_sim_get_state(void) { return current_state; }

uint16_t sub_sim_get_dive_duration(void) { return dive_duration_seconds; }

uint32_t sub_sim_get_elapsed_time(void) {
  return (current_tick - dive_start_tick) / (uint32_t)SUB_SIM_TICKS_PER_SECOND;
}

//================================
// Internal Helper Functions
//================================

void sub_sim_lcd_print(const char *line1, const char *line2) {
  LCD_sendCommand(0x80); // Set cursor to line 1
  LCD_print(line1);
  LCD_sendCommand(0xC0); // Set cursor to line 2
  LCD_print(line2);
}

static void sub_sim_led_set(uint8_t pattern) {
  // Set LED pattern (bits 0-5 correspond to PD0-PD5)
  // PD0: LED_TOP_RIGHT_PIN
  // PD1: LED_TOP_LEFT_PIN
  // PD2: LED_BOT_TOP_RIGHT
  // PD3: LED_BOT_TOP_LEFT
  // PD4: LED_TOP_CENTER_PIN
  // PD5: LED_BOT_CENTER_PIN

  if (pattern & (1 << 0))
    PORTD.OUTSET = PIN0_bm;
  else
    PORTD.OUTCLR = PIN0_bm;
  if (pattern & (1 << 1))
    PORTD.OUTSET = PIN1_bm;
  else
    PORTD.OUTCLR = PIN1_bm;
  if (pattern & (1 << 2))
    PORTD.OUTSET = PIN2_bm;
  else
    PORTD.OUTCLR = PIN2_bm;
  if (pattern & (1 << 3))
    PORTD.OUTSET = PIN3_bm;
  else
    PORTD.OUTCLR = PIN3_bm;
  if (pattern & (1 << 4))
    PORTD.OUTSET = PIN4_bm;
  else
    PORTD.OUTCLR = PIN4_bm;
  if (pattern & (1 << 5))
    PORTD.OUTSET = PIN5_bm;
  else
    PORTD.OUTCLR = PIN5_bm;
}

static void sub_sim_led_flash(uint8_t times, uint16_t delay_ms) {
  for (uint8_t i = 0; i < times; i++) {
    sub_sim_led_set(0x3F); // All LEDs on (6 LEDs)
    // Variable delay using loops
    for (volatile uint16_t j = 0; j < delay_ms; j++) {
      for (volatile uint16_t k = 0; k < (F_CPU / 1000000UL); k++) {
        __asm__ __volatile__("nop");
      }
    }
    sub_sim_led_set(0x00); // All LEDs off
    for (volatile uint16_t j = 0; j < delay_ms; j++) {
      for (volatile uint16_t k = 0; k < (F_CPU / 1000000UL); k++) {
        __asm__ __volatile__("nop");
      }
    }
  }
}

// Removed sub_sim_alarm_on/off as they are replaced by sub_sim_set_sound_mode

void sub_sim_init_adc(void) {
  // Disable digital input buffers for analog pins to save power and reduce
  // noise
  PORTE.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc;                    // PE0 (Pot)
  PORTE.PIN1CTRL = PORT_ISC_INPUT_DISABLE_gc;                    // PE1 (Joy X)
  PORTE.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;                    // PE2 (Joy Y)
  PORTE.PIN3CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc; // PE3 (CO)
  PORTF.PIN4CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc; // PF4 (NH3)
  PORTF.PIN5CTRL = PORT_PULLUPEN_bm | PORT_ISC_INPUT_DISABLE_gc; // PF5 (NO2)

  // Configure ADC0
  // Use slower clock (DIV32 = 500kHz) and longer sampling time to prevent
  // crosstalk
  ADC0.CTRLC = ADC_PRESC_DIV32_gc;
  ADC0.CTRLD = ADC_INITDLY_DLY16_gc;
  ADC0.SAMPCTRL = 10;                // Extend sampling duration
  VREF.ADC0REF = VREF_REFSEL_VDD_gc; // VDD as reference (3.3V)
  ADC0.CTRLA = ADC_ENABLE_bm;        // Enable ADC
}

uint16_t sub_sim_read_pot(void) {
  // Read potentiometer value from ADC
  ADC0.MUXPOS = POT_ADC_CHANNEL;
  // Dummy read to settle MUX/S&H
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  // Actual read
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  return ADC0.RES;
}
uint16_t sub_sim_read_joy_x(void) {
  // Read potentiometer value from ADC
  ADC0.MUXPOS = JOY_X_CHANNEL;
  // Dummy read to settle MUX/S&H
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  // Actual read
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  return ADC0.RES;
}

uint16_t sub_sim_read_joy_y(void) {
  // Read potentiometer value from ADC
  ADC0.MUXPOS = JOY_Y_CHANNEL;
  // Dummy read to settle MUX/S&H
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  // Actual read
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  return ADC0.RES;
}

uint16_t sub_sim_read_co(void) {
  // Read potentiometer value from ADC
  ADC0.MUXPOS = CO_SENS_CHANNEL;
  // Dummy read to settle MUX/S&H
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  // Actual read
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  return ADC0.RES;
}

uint16_t sub_sim_read_nh3(void) {
  // Read potentiometer value from ADC
  ADC0.MUXPOS = NH3_SENS_CHANNEL;
  // Dummy read to settle MUX/S&H
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  // Actual read
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  return ADC0.RES;
}

uint16_t sub_sim_read_no2(void) {
  // Read potentiometer value from ADC
  ADC0.MUXPOS = NO2_SENS_CHANNEL;
  // Dummy read to settle MUX/S&H
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  // Actual read
  ADC0.COMMAND = ADC_STCONV_bm;
  while (ADC0.COMMAND & ADC_STCONV_bm)
    ;
  return ADC0.RES;
}

static void sub_sim_handle_setup(void) {
  static uint32_t last_update_tick = 0;
  static uint8_t display_toggle = 0;

  // Update display every 200ms
  if (current_tick - last_update_tick >= (SUB_SIM_TICKS_PER_SECOND / 5)) {
    last_update_tick = current_tick;
    display_toggle++;

    // Read potentiometer (0-4095 for 12-bit ADC)
    uint16_t pot_value = sub_sim_read_pot();

    // Map to 0-300 seconds
    dive_duration_seconds = (uint16_t)((pot_value * 300UL) / 4095UL);

    // Display on LCD - Alternate every ~3 seconds (15 * 200ms = 3000ms)
    char line2[18];
    snprintf(line2, sizeof(line2), "DURATION: %3d S ", dive_duration_seconds);

    if (display_toggle < 15) {
      sub_sim_lcd_print("SET DIVE TIME   ", line2);
    } else {
      sub_sim_lcd_print("PRESS RX START  ", "TO START (PC5)  ");
    }

    if (display_toggle >= 30) {
      display_toggle = 0;
    }

    // Debug: Check buttons and print to UART if pressed (to verify wiring)
    if (sub_sim_button_reactor_startup_pressed() && DEBUG)
      aos_printf("DEBUG: PC5 Pressed\r\n");
    if (sub_sim_button_o2_cb_pressed() && DEBUG)
      aos_printf("DEBUG: PC3 Pressed\r\n");
    if (sub_sim_button_rx_cb_pressed() && DEBUG)
      aos_printf("DEBUG: PC4 Pressed\r\n");
  }

  // Wait for reactor startup button
  if (sub_sim_button_reactor_startup_pressed()) {
    _delay_ms(50); // Debounce

    if (dive_duration_seconds == 0) {
      sub_sim_lcd_print("ERROR: SET TIME ", "DURATION > 0    ");
      _delay_ms(2000);
      return;
    }

    // Print selected duration to UART once
    aos_printf("Dive Duration set to: %d seconds\r\n", dive_duration_seconds);

    // Calculate phase timestamps (in seconds for RTC)
    phase_25_sec = dive_duration_seconds / 4;
    phase_50_sec = dive_duration_seconds / 2;
    phase_75_sec = (dive_duration_seconds * 3) / 4;
    phase_100_sec = dive_duration_seconds;

    // Transition to Reactor Startup Sequence
    current_state = DIVE_REACTOR_START;

    // Shuffle casualties for this dive
    srand(current_tick);
    sub_sim_shuffle_events();
    sub_sim_generate_torpedo_sequence();
  }
}

static bool sub_sim_run_event_low_o2(void) {
  if (!o2_alarm_triggered) {
    sub_sim_set_sound_mode(SOUND_GENERAL_ALARM);
    sub_sim_lcd_print("ALARM: LOW O2   ", "RESET O2 CB     ");
    aos_printf("\r\n[CASUALTY] LOW OXYGEN WARNING\r\n");
    aos_printf("Press O2 Circuit Breaker (PC3) within 20 seconds!\r\n");
    o2_alarm_triggered = true;
    o2_awaiting_response = true;
    phase_start_tick = current_tick;
  }

  if (o2_awaiting_response) {
    // Update LCD with sensor value
    uint16_t co_val = sub_sim_read_co();
    char line2[17];
    snprintf(line2, sizeof(line2), "CO LEVEL: %4d  ", co_val);
    sub_sim_lcd_print("ALARM: LOW O2   ", line2);

    // Check for timeout
    if (current_tick - phase_start_tick >= response_timeout_ticks) {
      sub_sim_set_sound_mode(SOUND_SILENT);
      sub_sim_lcd_print("SURFACE...      ", "SURFACE...      ");
      aos_printf("\r\n[FAILED] No response - Emergency surface!\r\n");
      _delay_ms(2000);
      current_state = DIVE_FAILED;
      return false; // Failed
    }

    // Check for O2 CB button press
    if (sub_sim_button_o2_cb_pressed()) {
      _delay_ms(50); // Debounce
      sub_sim_set_sound_mode(SOUND_SILENT);
      sub_sim_lcd_print("O2 RESET OK     ", "CONTINUING...   ");
      aos_printf("\r\n[SUCCESS] O2 scrubbers reset!\r\n");

      // Switch to Backup O2
      backup_o2_active = true;

      _delay_ms(2000);

      o2_alarm_triggered = false;
      o2_awaiting_response = false;
      sub_sim_set_sound_mode(SOUND_SONAR_PING);
      return true; // Complete
    }
  }
  return false; // Not complete yet
}

static bool sub_sim_run_event_reactor(void) {
  if (!rx_alarm_triggered) {
    sub_sim_set_sound_mode(SOUND_REACTOR_ALARM);
    sub_sim_lcd_print("RX PLANT CASUALTY", "SNORKLE!!!!     ");
    sub_sim_lcd_print("RX PLANT CASUALTY", "SNORKLE!!!!     ");
    aos_printf("\r\n[CASUALTY] REACTOR PLANT CASUALTY\r\n");
    aos_printf("Reduce to periscope depth, transfer to emergency diesel\r\n");
    aos_printf("Press Reactor CB (PC4) to transfer power!\r\n");
    rx_alarm_triggered = true;
    rx_awaiting_transfer = true;
    phase_start_tick = current_tick;
  }

  if (rx_awaiting_transfer) {
    // Check for Rx CB button press
    if (sub_sim_button_rx_cb_pressed()) {
      _delay_ms(50); // Debounce
      sub_sim_set_sound_mode(SOUND_SILENT);
      sub_sim_lcd_print("PWR TRANSFER OK ", "DRILL COMPLETE  ");
      aos_printf("\r\n[SUCCESS] Power transferred to emergency diesel!\r\n");
      aos_printf("Reactor casualty was a drill - continue mission\r\n");
      _delay_ms(1000); // Reduced from 3000

      sub_sim_lcd_print("RETURN TO DEPTH ", "MISSION DEPTH   ");
      _delay_ms(1000); // Reduced from 2000

      rx_alarm_triggered = false;
      rx_awaiting_transfer = false;
      sub_sim_set_sound_mode(SOUND_SONAR_PING);
      return true; // Complete
    }
  }
  return false;
}

static bool sub_sim_run_event_torpedo(void) {
  if (!torp_alarm_triggered) {
    sub_sim_set_sound_mode(SOUND_GENERAL_ALARM);
    sub_sim_lcd_print("TORPEDO EVASION ",
                      sub_sim_get_dir_str(torpedo_sequence[0]));
    aos_printf("\r\n[CASUALTY] TORPEDO EVASION!\r\n");
    aos_printf("All ahead flank - cavitate!\r\n");
    aos_printf("Follow the commands on the LCD!\r\n");
    torp_alarm_triggered = true;
    torp_minigame_active = true;
    torpedo_sequence_index = 0;
    torp_last_dir = 0;
    phase_start_tick = current_tick;
  }

  if (torp_minigame_active) {
    // Check for timeout
    if (current_tick - phase_start_tick >= response_timeout_ticks) {
      sub_sim_set_sound_mode(SOUND_SILENT);
      sub_sim_lcd_print("SURFACE...      ", "SURFACE...      ");
      aos_printf("\r\n[FAILED] Torpedo impact!\r\n");
      _delay_ms(2000);
      current_state = DIVE_FAILED;
      return false;
    }

    // Read Joystick
    uint16_t joy_x = sub_sim_read_joy_x();
    uint16_t joy_y = sub_sim_read_joy_y();
    uint8_t current_dir = 0; // 0=Center

    // Thresholds: Center~3160
    // UP/RIGHT > 3800, DOWN/LEFT < 1000
    if (joy_y > 3800)
      current_dir = 2; // DOWN (Flipped)
    else if (joy_y < 1000)
      current_dir = 1; // UP (Flipped)
    else if (joy_x > 3800)
      current_dir = 4; // RIGHT
    else if (joy_x < 1000)
      current_dir = 3; // LEFT

    // Only register move on transition from Center/Different to New Dir
    if (current_dir != 0 && current_dir != torp_last_dir) {
      // A new direction was engaged
      uint8_t expected_dir = torpedo_sequence[torpedo_sequence_index];

      if (current_dir == expected_dir) {
        _delay_ms(50); // Debounce/Wait
        torpedo_sequence_index++;

        aos_printf("[Step %d/8 complete]\r\n", torpedo_sequence_index);

        if (torpedo_sequence_index >= 8) {
          // Sequence complete
          sub_sim_set_sound_mode(SOUND_SILENT);
          sub_sim_lcd_print("TORPEDO EVADED  ", "SUCCESS!        ");
          aos_printf("\r\n[SUCCESS] Torpedo evaded!\r\n");
          _delay_ms(2000);

          torp_alarm_triggered = false;
          torp_minigame_active = false;
          sub_sim_set_sound_mode(SOUND_SONAR_PING);
          return true; // Complete
        } else {
          // Show next command
          sub_sim_lcd_print(
              "EVADING...      ",
              sub_sim_get_dir_str(torpedo_sequence[torpedo_sequence_index]));
        }
      } else {
        // Wrong move - Reset sequence
        if (torpedo_sequence_index > 0) {
          aos_printf("Wrong move! Resetting sequence.\r\n");
          sub_sim_lcd_print("WRONG MOVE!     ", "RESTART SEQ     ");

          // Flash Backlight Red
          BACKLIGHT_setRGB(255, 0, 0);
          _delay_ms(200);
          BACKLIGHT_setBacklight100(); // Restore White

          _delay_ms(300);
          torpedo_sequence_index = 0;
          sub_sim_lcd_print("EVADING...      ",
                            sub_sim_get_dir_str(torpedo_sequence[0]));
        }
      }
    }
    torp_last_dir = current_dir;
  }
  return false;
}

static bool sub_sim_run_event_generic(sim_event_t event) {
  current_active_event = event;
  switch (event) {
  case EVENT_LOW_O2:
    return sub_sim_run_event_low_o2();
  case EVENT_REACTOR:
    return sub_sim_run_event_reactor();
  case EVENT_TORPEDO:
    return sub_sim_run_event_torpedo();
  default:
    return true;
  }
}

static void sub_sim_handle_phase_100(void) {
  sub_sim_set_sound_mode(SOUND_SILENT);

  sub_sim_lcd_print("DIVE COMPLETE   ", "3 EFPH BURNED   ");
  aos_printf("\r\n[DIVE COMPLETE]\r\n");
  aos_printf(
      "Nuclear fuel expenditure: 3 EFPH (Effective Full Power Hours)\r\n");
  aos_printf("Congratulations, Captain!\r\n");

  sub_sim_play_mission_complete();

  sub_sim_led_flash(5, 200);
  _delay_ms(3000);

  phase_100_complete = true;
  current_state = DIVE_SURFACING;
}
