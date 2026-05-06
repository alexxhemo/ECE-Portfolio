#ifndef DAC_H_
#define DAC_H_
#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif
#include <avr/io.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>
/**
 *@file dac.h
 *@brief Contains functions to initialize the DAC0 peripheral and generate a
 * sine wave in memory
 *@author Arturo Salinas
 */

// Sine wave tables
extern uint16_t sineWave[64];         // Original unscaled sine table
extern uint16_t sine_wave_scaled[64]; // Amplitude-scaled sine table

void sine_wave_init(void);

void update_sine_wave_scaled(int amplitude_percent);

void DAC_init(void);

void DAC0_setVal(uint16_t value);

#endif // DAC_H_
