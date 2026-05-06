#include "rtc.h"
#include <avr/interrupt.h>

static void (*rtc_callback)(void) = 0;

void rtc_init(void) {
    // Enable internal 32.768kHz oscillator
    // In AVR128DB48, the 32KHz OSC is enabled by requesting it in CLKCTRL or by the peripheral
    // RTC uses CLK_RTC which defaults to OSC32K
    
    // Wait for all registers to be synchronized
    while (RTC.STATUS > 0);

    // Configure RTC Clock Source
    // By default, RTC uses 1.024kHz from OSC32K (DIV32)
    // We want 1Hz. 
    // If we use PIT (Periodic Interrupt Timer):
    // PIT Clock is same as RTC Clock.
    // If RTC Clock is 1.024kHz.
    // We need 1024 cycles for 1 second.
    
    // Let's use the 32.768kHz directly if possible, or the 1kHz derived.
    // RTC.CLKSEL is in CLKCTRL? No, RTC.CLKSEL register.
    
    RTC.CLKSEL = RTC_CLKSEL_OSC32K_gc; // Select 32.768kHz Internal Oscillator

    // Wait for synchronization
    while (RTC.STATUS > 0);
    
    // Configure PIT for 1 second period
    // 32768 cycles = 1 second
    RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm;
    
    // Enable PIT Interrupt
    RTC.PITINTCTRL = RTC_PI_bm;
}

void rtc_set_callback(void (*callback)(void)) {
    rtc_callback = callback;
}

void rtc_enable(void) {
    // RTC is enabled by the PITEN bit in PITCTRLA, which we set in init.
    // But we can also ensure the main RTC is enabled if we were using CNT.
    // For PIT, just ensuring PITEN is enough.
    // But let's make sure interrupts are enabled globally (user must do sei())
}

void rtc_disable(void) {
    RTC.PITCTRLA &= ~RTC_PITEN_bm;
}

ISR(RTC_PIT_vect) {
    // Clear flag
    RTC.PITINTFLAGS = RTC_PI_bm;
    
    if (rtc_callback) {
        rtc_callback();
    }
}
