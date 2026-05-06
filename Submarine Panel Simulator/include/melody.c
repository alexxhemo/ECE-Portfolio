/**
 * @file melody.c
 * @author Arturo Salinas
 * @date 2025-11-02
 * @brief Melody player implementation using DAC
 */
#define F_CPU 16000000UL
#include "melody.h"
#include "dac.h"
#include "aos_timer.h"
#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
// No TCB callback needed; TCA0 ISR in main.c updates DAC in sine mode
static void delay_ms(uint16_t ms) {
    for (uint16_t i = 0; i < ms; i++) {
        // 16 MHz / 16000 cycles = 1ms
        for (volatile uint16_t j = 0; j < 2000; j++) {
            __asm__ __volatile__("nop");
        }
    }
}

// MIDI melody data
const melody_note_t midi1[81] = {
    {E4, 136, 136},
    {A4, 136, 136},
    {A4, 136, 136},
    {Cb5, 136, 136},
    {Cb5, 136, 136},
    {A4, 136, 409},
    {E4, 136, 136},
    {E4, 136, 136},
    {E4, 136, 273},
    {E4, 136, 0},
    {B4, 136, 0},
    {A4, 136, 0},
    {Gb4, 136, 0},
    {Fb4, 136, 0},
    {E4, 136, 682},
    {E4, 136, 136},
    {A4, 136, 136},
    {A4, 136, 136},
    {Cb5, 136, 136},
    {Cb5, 136, 136},
    {A4, 136, 409},
    {E4, 136, 136},
    {A4, 136, 136},
    {Gb4, 136, 136},
    {Fb4, 136, 0},
    {Gb4, 136, 0},
    {A4, 136, 136},
    {Db4, 136, 136},
    {E4, 136, 682},
    {E4, 136, 136},
    {Gb4, 136, 136},
    {Gb4, 136, 136},
    {A4, 136, 0},
    {Gb4, 136, 0},
    {Fb4, 136, 0},
    {Gb4, 136, 0},
    {A4, 136, 409},
    {E4, 136, 136},
    {A4, 136, 136},
    {Gb4, 136, 136},
    {Gb4, 136, 136},
    {Gb4, 136, 0},
    {D5, 136, 0},
    {B4, 136, 0},
    {Gb4, 136, 0},
    {A4, 136, 682},
    {A4, 136, 136},
    {Fb4, 136, 136},
    {Fb4, 136, 136},
    {Fb4, 136, 136},
    {A4, 136, 136},
    {A4, 136, 409},
    {E4, 136, 136},
    {E4, 136, 136},
    {E4, 136, 273},
    {E4, 136, 0},
    {B4, 136, 136},
    {Gb4, 136, 136},
    {A4, 136, 682},
    {A4, 136, 136},
    {Gb4, 136, 0},
    {Fb4, 136, 0},
    {Fb4, 136, 136},
    {Fb4, 136, 0},
    {A4, 136, 0},
    {Gb4, 136, 0},
    {B4, 136, 0},
    {A4, 136, 409},
    {E4, 136, 136},
    {E4, 136, 136},
    {E4, 136, 273},
    {E4, 136, 0},
    {B4, 136, 136},
    {Gb4, 136, 136},
    {A4, 136, 682},
    {Cb5, 136, 136},
    {E5, 136, 0},
    {B4, 136, 0},
    {Cb5, 136, 0},
    {A4, 136, 0},
    {Fb4, 136, 0},
};

const size_t midi1_length = sizeof(midi1) / sizeof(midi1[0]);

// Square wave generation state
static volatile bool tone_active = false;
static volatile uint16_t current_frequency = 0;
// Use DAC sine engine via TCA0; control flag defined in main.c
extern volatile int dac_update; // 1 = output sine, 0 = pause
// Playback speed multiplier (1 = normal, 2 = twice as fast)
static volatile uint8_t g_speed_mul = 1;

// No periodic callback required here; DAC stepping handled by TCA0 ISR in main.c

void melody_init(void) {
    tone_active = false;
    current_frequency = 0;
}

void melody_tone(uint16_t frequency) {
    if (frequency == 0) {
        melody_no_tone();
        return;
    }
    
    current_frequency = frequency;
    
    // Stop any TCB0 usage for square output
    aos_tcb_disable(&TCB0);
    TCB0.INTCTRL = 0;

    // Ensure DAC analog output is configured
    DAC_init();

    // Use sine wave engine: enable output and set frequency
    dac_update = 1;
    update_tca0_frequency(frequency);

    tone_active = true;
}

void melody_no_tone(void) {
    tone_active = false;
    aos_tcb_disable(&TCB0);
    TCB0.INTCTRL = 0;
    // Pause DAC sine output and bias to mid-level
    dac_update = 0;
    DAC0_setVal(512);
}

void melody_play(const melody_note_t *notes, size_t length) {
    for (size_t i = 0; i < length; i++) {
        // Play note
        melody_tone(notes[i].frequency);
        uint16_t dur = notes[i].duration;
        if (g_speed_mul > 1) dur = dur / g_speed_mul;
        if (dur) delay_ms(dur);
        
        // Stop note
        melody_no_tone();
        
        // Pause between notes
        uint16_t pau = notes[i].pause;
        if (g_speed_mul > 1) pau = pau / g_speed_mul;
        if (pau > 0) {
            delay_ms(pau);
        }
    }
}

bool melody_is_playing(void) {
    return tone_active;
}

void melody_set_speed(uint8_t multiplier) {
    if (multiplier == 0) multiplier = 1;
    g_speed_mul = multiplier;
}

void melody_play_arduino(const int* melody, const int* noteDurations, size_t length, uint8_t speedMul) {
    // Optional tempo override for this song
    uint8_t prev = g_speed_mul;
    if (speedMul == 0) speedMul = 1;
    g_speed_mul = speedMul;

    for (size_t i = 0; i < length; i++) {
        int note = melody[i];
        int denom = noteDurations[i];
        if (denom <= 0) denom = 4; 
        uint16_t note_ms = 1000U / (uint16_t)denom;
        if (g_speed_mul > 1) note_ms = note_ms / g_speed_mul;

        if (note > 0) {
            melody_tone((uint16_t)note);
        } else {
            melody_no_tone();
        }

        uint16_t pause_ms = (uint16_t)((uint32_t)note_ms + (uint32_t)note_ms / 3U);
        delay_ms(pause_ms);

        melody_no_tone();
    }


    g_speed_mul = prev;
}

// Arduino-compatible wrappers; pin is ignored (uses DAC0/PD6)
void tone(uint8_t pin, unsigned int frequency, unsigned long duration_ms) {
    (void)pin; (void)duration_ms;
    DAC_init();
    melody_tone((uint16_t)frequency);
}

void noTone(uint8_t pin) {
    (void)pin;
    melody_no_tone();
}
