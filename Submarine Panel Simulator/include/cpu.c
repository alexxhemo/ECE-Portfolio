/**
 * @file cpu.c
 * @brief CPU and Clock Initialization for AVR128DB48
 * @author Arturo Salinas
 * @date 2015-03-20
 */
#include "cpu.h"
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <avr/io.h>

void CLOCK_XOSCHF_16M_init(void) {
  /* Enable crystal oscillator
   * with frequency range 16Mhz and 4K cycles start-up time
   */
  ccp_write_io((uint8_t *)&CLKCTRL.XOSCHFCTRLA,
               CLKCTRL_RUNSTDBY_bm | CLKCTRL_CSUTHF_4K_gc |
                   CLKCTRL_FRQRANGE_16M_gc | CLKCTRL_SELHF_XTAL_gc |
                   CLKCTRL_ENABLE_bm);

  // Confirm the crystal oscillator start-up
  while (!(CLKCTRL.MCLKSTATUS & CLKCTRL_EXTS_bm)) {
    ; // Do nothing
  }

  // set the main clock to use XOSCH as source and enable the CLKOUT pin
  ccp_write_io((uint8_t *)&CLKCTRL.MCLKCTRLA,
               CLKCTRL_CLKSEL_EXTCLK_gc | CLKCTRL_CLKOUT_bm);

  // Wait for system oscillator chaning to complete
  while (CLKCTRL.MCLKSTATUS & CLKCTRL_SOSC_bm) {
    ; // Do nothing
  }

  // Clear RUNSTDBY for power save when not in use
  ccp_write_io((uint8_t *)&CLKCTRL.XOSCHFCTRLA,
               CLKCTRL.XOSCHFCTRLA & ~CLKCTRL_RUNSTDBY_bm);
}

void CLOCK_XOSCHF_24M_init(void) {
  /* Enable crystal oscillator
   * with frequency range 24Mhz and 4K cycles start-up time
   */
  ccp_write_io((uint8_t *)&CLKCTRL.XOSCHFCTRLA,
               CLKCTRL_RUNSTDBY_bm | CLKCTRL_CSUTHF_4K_gc |
                   CLKCTRL_FRQRANGE_24M_gc | CLKCTRL_SELHF_XTAL_gc |
                   CLKCTRL_ENABLE_bm);

  // Confirm the crystal oscillator start-up
  while (!(CLKCTRL.MCLKSTATUS & CLKCTRL_EXTS_bm)) {
    ; // Do nothing
  }

  // set the main clock to use XOSCH as source and enable the CLKOUT pin
  ccp_write_io((uint8_t *)&CLKCTRL.MCLKCTRLA,
               CLKCTRL_CLKSEL_EXTCLK_gc | CLKCTRL_CLKOUT_bm);

  // Wait for system oscillator chaning to complete
  while (CLKCTRL.MCLKSTATUS & CLKCTRL_SOSC_bm) {
    ; // Do nothing
  }

  // Clear RUNSTDBY for power save when not in use
  ccp_write_io((uint8_t *)&CLKCTRL.XOSCHFCTRLA,
               CLKCTRL.XOSCHFCTRLA & ~CLKCTRL_RUNSTDBY_bm);
}

void CLOCK_CFD_CLKMAIN_init(void) {
  ccp_write_io((uint8_t *)&CLKCTRL.MCLKCTRLA,
               CLKCTRL_CFDSRC_CLKMAIN_gc | CLKCTRL_CFDEN_bm);

  ccp_write_io((uint8_t *)&CLKCTRL.MCLKINTCTRL,
               CLKCTRL_INTTYPE_bm | CLKCTRL_CFD_bm);
}

void CLOCK_OSC_16M_init(void) {
  // ccp_write_io((void *)&(CLKCTRL.OSCHFCTRLA), (CLKCTRL_FRQSEL_16M_gc));
  CPU_CCP = CCP_IOREG_gc;                     // unlock protected IO regs
  CLKCTRL.OSCHFCTRLA = CLKCTRL_FRQSEL_16M_gc; // internal HF osc 16 MHz
}

void CLOCK_OSC_24M_init(void) {
  // ccp_write_io((void *)&(CLKCTRL.OSCHFCTRLA), (CLKCTRL_FRQSEL_24M_gc));
  CPU_CCP = CCP_IOREG_gc;                     // unlock protected IO regs
  CLKCTRL.OSCHFCTRLA = CLKCTRL_FRQSEL_24M_gc; // internal HF osc 24 MHz
}
