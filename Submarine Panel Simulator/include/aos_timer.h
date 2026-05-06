#ifndef AOS_TIMER_H_
#define AOS_TIMER_H_

#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif
#ifdef __cplusplus
extern "C" {
#endif

// Callback signature types
typedef void (*aos_tca1_callback_t)(void);
typedef void (*aos_tca0_callback_t)(void);
typedef void (*aos_tcb_callback_t)(void);
typedef void (*aos_tcd_callback_t)(void);

// Configure TCA1 for 1ms tick at 16MHz with DIV64 (PER=249) by default
// You can call aos_tca1_configure with custom parameters before enabling.
void aos_tca1_configure(uint16_t per_value, uint8_t ctrla_flags,
                        uint8_t ctrlb_flags);

// Register/unregister the TCA1 overflow callback. Pass NULL to unregister.
void aos_tca1_register(aos_tca1_callback_t cb);
void aos_tca1_unregister(void);

// Enable/disable the TCA1 overflow interrupt and timer
void aos_tca1_enable(void);
void aos_tca1_disable(void);

// Convenience: start with common 1ms configuration (PER=249, DIV64)
static inline void aos_tca1_start_1ms(void) {
  aos_tca1_configure(249, TCA_SINGLE_CLKSEL_DIV64_gc | TCA_SINGLE_ENABLE_bm,
                     TCA_SINGLE_WGMODE_NORMAL_gc);
  aos_tca1_enable();
}

// Optional: per-source callbacks for TCA1 compare channels
void aos_tca1_on_cmp0(aos_tca1_callback_t cb);
void aos_tca1_on_cmp1(aos_tca1_callback_t cb);
void aos_tca1_on_cmp2(aos_tca1_callback_t cb);
void aos_tca1_set_compare(uint8_t channel /*0-2*/, uint16_t value);

// ---------- TCA0 (16-bit, used for DAC waveform and PWM) ----------
// Configure TCA0 (e.g., for PWM or FRQ). Caller provides PER/CTRLA/CTRLB
// settings.
void aos_tca0_configure(uint16_t per_value, uint8_t ctrla_flags,
                        uint8_t ctrlb_flags);
void aos_tca0_update_period(uint16_t per_value);
void aos_tca0_register(aos_tca0_callback_t cb);
void aos_tca0_unregister(void);
void aos_tca0_enable(void); // enables OVF interrupt (does not change enable bit
                            // unless included in ctrla_flags)
void aos_tca0_disable(void);

// Keep legacy helper for modules that call this directly
void update_tca0_frequency(uint32_t new_freq);

// Optional: per-source callbacks for TCA0 compare channels
void aos_tca0_on_cmp0(aos_tca0_callback_t cb);
void aos_tca0_on_cmp1(aos_tca0_callback_t cb);
void aos_tca0_on_cmp2(aos_tca0_callback_t cb);
void aos_tca0_set_compare(uint8_t channel /*0-2*/, uint16_t value);
void aos_tca0_enable_cmp_outputs(
    uint8_t mask /* bit0=CMP0EN, bit1=CMP1EN, bit2=CMP2EN */);

// ---------- TCB (periodic timers) ----------
// Minimal API to configure a TCBn in periodic interrupt mode.
// 'tcb' must be &TCB0, &TCB1, &TCB2, or &TCB3.
void aos_tcb_configure_periodic(volatile TCB_t *tcb, uint16_t top,
                                uint8_t clkSel /* TCB_CLKSEL values */);
void aos_tcb_register(volatile TCB_t *tcb, aos_tcb_callback_t cb,
                      bool use_capt /* true: CAPT, false: OVF */);
void aos_tcb_enable(volatile TCB_t *tcb);
void aos_tcb_disable(volatile TCB_t *tcb);

// ---------- TCD0 (12-bit advanced timer) ----------
void aos_tcd_configure(uint8_t clkSel /* TCD_CLKSEL */,
                       uint8_t cntPres /* CNTPRES */,
                       uint8_t syncPres /* SYNCPRES */,
                       uint8_t wgmode /* WGMODE */);
void aos_tcd_set_compare(uint16_t a_set, uint16_t a_clr, uint16_t b_set,
                         uint16_t b_clr);
void aos_tcd_enable_outputs(uint8_t mask /* bit0=CMPA,1=CMPB,2=CMPC,3=CMPD */);
void aos_tcd_enable(void);
void aos_tcd_disable(void);
void aos_tcd_on_ovf(aos_tcd_callback_t cb);
void aos_tcd_on_trig_a(aos_tcd_callback_t cb);
void aos_tcd_on_trig_b(aos_tcd_callback_t cb);

// ---------- Optional ownership guards (best-effort) ----------
bool aos_timer_claim_tca0(const char *owner_tag);
bool aos_timer_release_tca0(const char *owner_tag);
bool aos_timer_claim_tca1(const char *owner_tag);
bool aos_timer_release_tca1(const char *owner_tag);
bool aos_timer_claim_tcb(volatile TCB_t *tcb, const char *owner_tag);
bool aos_timer_release_tcb(volatile TCB_t *tcb, const char *owner_tag);
bool aos_timer_claim_tcd0(const char *owner_tag);
bool aos_timer_release_tcd0(const char *owner_tag);

#ifdef __cplusplus
}
#endif

#endif /* AOS_TIMER_H_ */
