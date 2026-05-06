#define F_CPU 16000000UL
#define __AVR_AVR128DB48__
#include "aos_timer.h"
#include <avr/interrupt.h>

// Single global callback for TCA1 OVF and optional CMP callbacks
static volatile aos_tca1_callback_t s_tca1_cb = 0;
static volatile aos_tca1_callback_t s_tca1_cmp_cb[3] = {0,0,0};

// Single global callback for TCA0 OVF and optional CMP callbacks
static volatile aos_tca0_callback_t s_tca0_cb = 0;
static volatile aos_tca0_callback_t s_tca0_cmp_cb[3] = {0,0,0};

// TCB callbacks per instance (0..3) and mode (0=OVF,1=CAPT)
typedef struct {
  aos_tcb_callback_t ovf_cb;
  aos_tcb_callback_t capt_cb;
} tcb_cb_t;

static volatile tcb_cb_t s_tcb_cb[4] = {{0}};

// TCD callbacks
static volatile aos_tcd_callback_t s_tcd_ovf_cb = 0;
static volatile aos_tcd_callback_t s_tcd_triga_cb = 0;
static volatile aos_tcd_callback_t s_tcd_trigb_cb = 0;

// Optional ownership guards
static const char* s_owner_tca0 = 0;
static const char* s_owner_tca1 = 0;
static const char* s_owner_tcd0 = 0;
static const char* s_owner_tcb[4] = {0,0,0,0};

void aos_tca1_configure(uint16_t per_value, uint8_t ctrla_flags, uint8_t ctrlb_flags) {
  // Put timer in known state
  TCA1.SINGLE.CTRLA = 0; // disable while reconfiguring
  TCA1.SINGLE.CTRLB = ctrlb_flags;
  TCA1.SINGLE.PER = per_value;

  // Do not enable INTCTRL here; aos_tca1_enable controls that
  TCA1.SINGLE.INTCTRL = 0; 

  // Apply clock selection and enable flag if present
  TCA1.SINGLE.CTRLA = ctrla_flags & (TCA_SINGLE_ENABLE_bm | TCA_SINGLE_CLKSEL_gm);
}

void aos_tca1_register(aos_tca1_callback_t cb) {
  s_tca1_cb = cb;
}

void aos_tca1_unregister(void) {
  s_tca1_cb = 0;
}

void aos_tca1_enable(void) {
  // Clear any pending flag
  TCA1.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
  // Enable overflow interrupt
  TCA1.SINGLE.INTCTRL |= TCA_SINGLE_OVF_bm;
}

void aos_tca1_disable(void) {
  // Disable overflow interrupt and stop timer
  TCA1.SINGLE.INTCTRL &= ~TCA_SINGLE_OVF_bm;
  TCA1.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm;
  // Clear any pending flag
  TCA1.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

ISR(TCA1_OVF_vect) {
  // Dispatch to registered callback if present
  aos_tca1_callback_t cb = s_tca1_cb;
  if (cb) {
    cb();
  }
  // Clear the overflow flag
  TCA1.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

void aos_tca1_on_cmp0(aos_tca1_callback_t cb) {
  s_tca1_cmp_cb[0] = cb;
  if (cb) TCA1.SINGLE.INTCTRL |= TCA_SINGLE_CMP0_bm; else TCA1.SINGLE.INTCTRL &= ~TCA_SINGLE_CMP0_bm;
}

void aos_tca1_on_cmp1(aos_tca1_callback_t cb) {
  s_tca1_cmp_cb[1] = cb;
  if (cb) TCA1.SINGLE.INTCTRL |= TCA_SINGLE_CMP1_bm; else TCA1.SINGLE.INTCTRL &= ~TCA_SINGLE_CMP1_bm;
}

void aos_tca1_on_cmp2(aos_tca1_callback_t cb) {
  s_tca1_cmp_cb[2] = cb;
  if (cb) TCA1.SINGLE.INTCTRL |= TCA_SINGLE_CMP2_bm; else TCA1.SINGLE.INTCTRL &= ~TCA_SINGLE_CMP2_bm;
}

void aos_tca1_set_compare(uint8_t channel, uint16_t value) {
  switch(channel & 0x03) {
    case 0: TCA1.SINGLE.CMP0BUF = value; break;
    case 1: TCA1.SINGLE.CMP1BUF = value; break;
    case 2: TCA1.SINGLE.CMP2BUF = value; break;
    default: break;
  }
}

ISR(TCA1_CMP0_vect) {
  if (s_tca1_cmp_cb[0]) s_tca1_cmp_cb[0]();
  TCA1.SINGLE.INTFLAGS = TCA_SINGLE_CMP0_bm;
}

ISR(TCA1_CMP1_vect) {
  if (s_tca1_cmp_cb[1]) s_tca1_cmp_cb[1]();
  TCA1.SINGLE.INTFLAGS = TCA_SINGLE_CMP1_bm;
}

ISR(TCA1_CMP2_vect) {
  if (s_tca1_cmp_cb[2]) s_tca1_cmp_cb[2]();
  TCA1.SINGLE.INTFLAGS = TCA_SINGLE_CMP2_bm;
}

// ---------- TCA0 Dispatcher ----------

void aos_tca0_configure(uint16_t per_value, uint8_t ctrla_flags, uint8_t ctrlb_flags) {
  TCA0.SINGLE.CTRLA = 0; // stop while configuring
  TCA0.SINGLE.CTRLB = ctrlb_flags;
  TCA0.SINGLE.EVCTRL &= ~(TCA_SINGLE_CNTAEI_bm | TCA_SINGLE_CNTBEI_bm);
  TCA0.SINGLE.PER = per_value;
  TCA0.SINGLE.INTCTRL = 0;
  TCA0.SINGLE.CTRLA = ctrla_flags & (TCA_SINGLE_ENABLE_bm | TCA_SINGLE_CLKSEL_gm);
}

void aos_tca0_update_period(uint16_t per_value) { TCA0.SINGLE.PER = per_value; }

void aos_tca0_register(aos_tca0_callback_t cb) { s_tca0_cb = cb; }

void aos_tca0_unregister(void) { s_tca0_cb = 0; }

void aos_tca0_enable(void) {
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
  TCA0.SINGLE.INTCTRL |= TCA_SINGLE_OVF_bm;
}

void aos_tca0_disable(void) {
  TCA0.SINGLE.INTCTRL &= ~TCA_SINGLE_OVF_bm;
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

// Legacy helper used by UI modules
void update_tca0_frequency(uint32_t new_freq) {
  uint32_t period = F_CPU / (new_freq * 64UL) - 1UL;
  aos_tca0_update_period((uint16_t)period);
}

ISR(TCA0_OVF_vect) {
  aos_tca0_callback_t cb = s_tca0_cb;
  if (cb) {
    cb();
  }
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

void aos_tca0_on_cmp0(aos_tca0_callback_t cb) {
  s_tca0_cmp_cb[0] = cb;
  if (cb) TCA0.SINGLE.INTCTRL |= TCA_SINGLE_CMP0_bm; else TCA0.SINGLE.INTCTRL &= ~TCA_SINGLE_CMP0_bm;
}

void aos_tca0_on_cmp1(aos_tca0_callback_t cb) {
  s_tca0_cmp_cb[1] = cb;
  if (cb) TCA0.SINGLE.INTCTRL |= TCA_SINGLE_CMP1_bm; else TCA0.SINGLE.INTCTRL &= ~TCA_SINGLE_CMP1_bm;
}

void aos_tca0_on_cmp2(aos_tca0_callback_t cb) {
  s_tca0_cmp_cb[2] = cb;
  if (cb) TCA0.SINGLE.INTCTRL |= TCA_SINGLE_CMP2_bm; else TCA0.SINGLE.INTCTRL &= ~TCA_SINGLE_CMP2_bm;
}

void aos_tca0_set_compare(uint8_t channel, uint16_t value) {
  switch(channel & 0x03) {
    case 0: TCA0.SINGLE.CMP0BUF = value; break;
    case 1: TCA0.SINGLE.CMP1BUF = value; break;
    case 2: TCA0.SINGLE.CMP2BUF = value; break;
    default: break;
  }
}

void aos_tca0_enable_cmp_outputs(uint8_t mask) {
  uint8_t ctrlb = TCA0.SINGLE.CTRLB & TCA_SINGLE_WGMODE_gm; // keep mode
  if (mask & (1<<0)) ctrlb |= TCA_SINGLE_CMP0EN_bm;
  if (mask & (1<<1)) ctrlb |= TCA_SINGLE_CMP1EN_bm;
  if (mask & (1<<2)) ctrlb |= TCA_SINGLE_CMP2EN_bm;
  TCA0.SINGLE.CTRLB = ctrlb;
}

ISR(TCA0_CMP0_vect) {
  if (s_tca0_cmp_cb[0]) s_tca0_cmp_cb[0]();
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP0_bm;
}

ISR(TCA0_CMP1_vect) {
  if (s_tca0_cmp_cb[1]) s_tca0_cmp_cb[1]();
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP1_bm;
}

ISR(TCA0_CMP2_vect) {
  if (s_tca0_cmp_cb[2]) s_tca0_cmp_cb[2]();
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP2_bm;
}

// ---------- TCB Periodic helpers ----------

static inline uint8_t tcb_index(volatile TCB_t* tcb) {
  if (tcb == &TCB0) return 0;
  if (tcb == &TCB1) return 1;
  if (tcb == &TCB2) return 2;
  if (tcb == &TCB3) return 3;
  return 0; // default to 0 if unknown
}

void aos_tcb_configure_periodic(volatile TCB_t* tcb, uint16_t top, uint8_t clkSel) {
  tcb->CTRLA = 0; // disable during config
  tcb->CTRLB = TCB_CNTMODE_INT_gc; // periodic interrupt mode
  tcb->EVCTRL = 0;                 // no event input
  tcb->CCMPL = (uint8_t)(top & 0xFF);
  tcb->CCMPH = (uint8_t)(top >> 8);
  tcb->INTCTRL = 0; // enabled via register()
  tcb->CTRLA = (clkSel & TCB_CLKSEL_gm) | TCB_ENABLE_bm;
}

void aos_tcb_register(volatile TCB_t* tcb, aos_tcb_callback_t cb, bool use_capt) {
  uint8_t idx = tcb_index(tcb);
  if (use_capt) {
    s_tcb_cb[idx].capt_cb = cb;
    tcb->INTCTRL |= TCB_CAPT_bm;
  } else {
    s_tcb_cb[idx].ovf_cb = cb;
    tcb->INTCTRL |= TCB_OVF_bm;
  }
}

void aos_tcb_enable(volatile TCB_t* tcb) { tcb->CTRLA |= TCB_ENABLE_bm; }

void aos_tcb_disable(volatile TCB_t* tcb) { tcb->CTRLA &= ~TCB_ENABLE_bm; }

ISR(TCB0_INT_vect) {
  uint8_t flags = TCB0.INTFLAGS;
  if ((flags & TCB_CAPT_bm) && s_tcb_cb[0].capt_cb) s_tcb_cb[0].capt_cb();
  if ((flags & TCB_OVF_bm) && s_tcb_cb[0].ovf_cb) s_tcb_cb[0].ovf_cb();
  TCB0.INTFLAGS = flags; // clear handled flags
}

ISR(TCB1_INT_vect) {
  uint8_t flags = TCB1.INTFLAGS;
  if ((flags & TCB_CAPT_bm) && s_tcb_cb[1].capt_cb) s_tcb_cb[1].capt_cb();
  if ((flags & TCB_OVF_bm) && s_tcb_cb[1].ovf_cb) s_tcb_cb[1].ovf_cb();
  TCB1.INTFLAGS = flags;
}

ISR(TCB2_INT_vect) {
  uint8_t flags = TCB2.INTFLAGS;
  if ((flags & TCB_CAPT_bm) && s_tcb_cb[2].capt_cb) s_tcb_cb[2].capt_cb();
  if ((flags & TCB_OVF_bm) && s_tcb_cb[2].ovf_cb) s_tcb_cb[2].ovf_cb();
  TCB2.INTFLAGS = flags;
}

ISR(TCB3_INT_vect) {
  uint8_t flags = TCB3.INTFLAGS;
  if ((flags & TCB_CAPT_bm) && s_tcb_cb[3].capt_cb) s_tcb_cb[3].capt_cb();
  if ((flags & TCB_OVF_bm) && s_tcb_cb[3].ovf_cb) s_tcb_cb[3].ovf_cb();
  TCB3.INTFLAGS = flags;
}

// ---------- TCD0 minimal helpers ----------

void aos_tcd_configure(uint8_t clkSel, uint8_t cntPres, uint8_t syncPres, uint8_t wgmode) {
  TCD0.CTRLA = 0; // disable while configuring
  TCD0.CTRLB = (wgmode);
  // mask in prescalers and clock select; do not enable here
  TCD0.CTRLA = (clkSel & TCD_CLKSEL_gm) | (cntPres & TCD_CNTPRES_gm) | (syncPres & TCD_SYNCPRES_gm);
  // interrupts off by default
  TCD0.INTCTRL = 0;
}

void aos_tcd_set_compare(uint16_t a_set, uint16_t a_clr, uint16_t b_set, uint16_t b_clr) {
  TCD0.CMPASET = a_set;
  TCD0.CMPACLR = a_clr;
  TCD0.CMPBSET = b_set;
  TCD0.CMPBCLR = b_clr;
}

void aos_tcd_enable_outputs(uint8_t mask) {
  // Best-effort: OR in mask, caller supplies appropriate CMPxEN bits per FAULTCTRL
  TCD0.FAULTCTRL |= mask;
}

void aos_tcd_enable(void) {
  TCD0.CTRLA |= TCD_ENABLE_bm;
}

void aos_tcd_disable(void) {
  TCD0.CTRLA &= ~TCD_ENABLE_bm;
}

void aos_tcd_on_ovf(aos_tcd_callback_t cb) {
  s_tcd_ovf_cb = cb;
  if (cb) TCD0.INTCTRL |= TCD_OVF_bm; else TCD0.INTCTRL &= ~TCD_OVF_bm;
}

void aos_tcd_on_trig_a(aos_tcd_callback_t cb) {
  s_tcd_triga_cb = cb;
  if (cb) TCD0.INTCTRL |= TCD_TRIGA_bm; else TCD0.INTCTRL &= ~TCD_TRIGA_bm;
}

void aos_tcd_on_trig_b(aos_tcd_callback_t cb) {
  s_tcd_trigb_cb = cb;
  if (cb) TCD0.INTCTRL |= TCD_TRIGB_bm; else TCD0.INTCTRL &= ~TCD_TRIGB_bm;
}

ISR(TCD0_OVF_vect) {
  if (s_tcd_ovf_cb) s_tcd_ovf_cb();
  TCD0.INTFLAGS = TCD_OVF_bm;
}

ISR(TCD0_TRIG_vect) {
  uint8_t flags = TCD0.INTFLAGS;
  if ((flags & TCD_TRIGA_bm) && s_tcd_triga_cb) s_tcd_triga_cb();
  if ((flags & TCD_TRIGB_bm) && s_tcd_trigb_cb) s_tcd_trigb_cb();
  TCD0.INTFLAGS = flags & (TCD_TRIGA_bm | TCD_TRIGB_bm);
}

// ---------- Ownership guards ----------

bool aos_timer_claim_tca0(const char* owner_tag) {
  if (!s_owner_tca0 || s_owner_tca0 == owner_tag) { s_owner_tca0 = owner_tag; return true; }
  return false;
}

bool aos_timer_release_tca0(const char* owner_tag) {
  if (s_owner_tca0 == owner_tag || !s_owner_tca0) { s_owner_tca0 = 0; return true; }
  return false;
}

bool aos_timer_claim_tca1(const char* owner_tag) {
  if (!s_owner_tca1 || s_owner_tca1 == owner_tag) { s_owner_tca1 = owner_tag; return true; }
  return false;
}

bool aos_timer_release_tca1(const char* owner_tag) {
  if (s_owner_tca1 == owner_tag || !s_owner_tca1) { s_owner_tca1 = 0; return true; }
  return false;
}

static inline uint8_t tcb_to_idx(volatile TCB_t* tcb) { return tcb_index(tcb); }

bool aos_timer_claim_tcb(volatile TCB_t* tcb, const char* owner_tag) {
  uint8_t idx = tcb_to_idx(tcb);
  if (!s_owner_tcb[idx] || s_owner_tcb[idx] == owner_tag) { s_owner_tcb[idx] = owner_tag; return true; }
  return false;
}

bool aos_timer_release_tcb(volatile TCB_t* tcb, const char* owner_tag) {
  uint8_t idx = tcb_to_idx(tcb);
  if (s_owner_tcb[idx] == owner_tag || !s_owner_tcb[idx]) { s_owner_tcb[idx] = 0; return true; }
  return false;
}

bool aos_timer_claim_tcd0(const char* owner_tag) {
  if (!s_owner_tcd0 || s_owner_tcd0 == owner_tag) { s_owner_tcd0 = owner_tag; return true; }
  return false;
}

bool aos_timer_release_tcd0(const char* owner_tag) {
  if (s_owner_tcd0 == owner_tag || !s_owner_tcd0) { s_owner_tcd0 = 0; return true; }
  return false;
}
