#ifndef RTC_H
#define RTC_H

#include <avr/io.h>
#include <stdbool.h>

// Initialize RTC with 32.768kHz internal oscillator
// Configures PIT for 1Hz interrupts (1 second period)
void rtc_init(void);

// Register a callback function to be called every second
void rtc_set_callback(void (*callback)(void));

// Enable/Disable RTC
void rtc_enable(void);
void rtc_disable(void);

#endif // RTC_H
