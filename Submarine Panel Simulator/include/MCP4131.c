#include "MCP4131.h"
#include <avr/interrupt.h>
#include <util/delay.h>
#ifndef MCP4131_SPI_PORT
#define MCP4131_SPI_PORT PORTA
#endif

#ifndef MCP4131_MOSI_PIN
#define MCP4131_MOSI_PIN PIN4_bm
#endif

#ifndef MCP4131_SCK_PIN
#define MCP4131_SCK_PIN PIN6_bm
#endif

#ifndef MCP4131_CS_PIN
#define MCP4131_CS_PIN PIN7_bm
#endif

// Variables for interrupt-driven SPI
volatile uint8_t spi_upper_byte = 0;
volatile uint8_t spi_lower_byte = 0;
volatile uint8_t spi_state = 0; // 0 = idle, 1 = sending upper byte, 2 = sending lower byte
volatile bool spi_busy = false;

// SPI Initialization
void SPI_Host_Init(void) {
  // Step 1: Configure SPI pins as output
  SPI_PORT.DIR |=
      SPI_MOSI | SPI_SCK | SPI_SS; // Set MOSI, SCK, and SS as output
  SPI_PORT.OUTSET = SPI_SS;        // Ensure SS idles high
  _delay_ms(20);
  // Step 2: Configure SPI as Master, set clock rate to 125kHz
  SPI0.CTRLA = SPI_ENABLE_bm          // Enable SPI
               | SPI_MASTER_bm        // Set SPI as Master mode
               | SPI_PRESC_DIV128_gc; // Set clock prescaler to divide by 128
                                      // (this gives 125 kHz for 16 MHz clock)
    SPI0.CTRLB = SPI_MODE_0_gc;

  // Step 3: Enable SPI interrupt (Bit 0 - IE in INTCTRL register for Normal Mode)
  SPI0.INTCTRL |= SPI_IE_bm; // Enable SPI interrupt
}

static inline void mcp4131_spi_transfer(uint8_t data) {
  SPI0.DATA = data;
  /* Wait for transfer complete */
  while (!(SPI0.INTFLAGS & SPI_TXCIF_bm))
    ;
  /* Reading SPI0.DATA clears both RXCIF and TXCIF flags */
  (void)SPI0.DATA;
}

static inline void mcp4131_write_wiper(uint8_t value) {
  /* Clamp to device range */
  if (value > 128)
    value = 128;
  /* Begin transaction */
  MCP4131_SPI_PORT.OUT &= ~MCP4131_CS_PIN;
  /* First byte: address 0 (wiper0), command 00 (write), upper two data bits = 0
   */
  mcp4131_spi_transfer(0x00);
  /* Second byte: lower 8 bits of wiper data */
  mcp4131_spi_transfer(value);
  /* End transaction */
  MCP4131_SPI_PORT.OUT |= MCP4131_CS_PIN;
}

uint8_t SPI_Host_Transceiver(uint8_t rec_data, uint8_t pushed_data) {
  // Step 1: Set SS to low to activate the slave
  SPI_PORT.OUTCLR = SPI_SS; // Set SS low to select the slave

  // Step 2: Send data to the slave
  SPI0.DATA =
      pushed_data; // Load the data to be sent into the SPI Data Register

  // Step 3: Wait for the transmission to complete
  loop_until_bit_is_set(SPI0.INTFLAGS, SPI_IF_bp);

  // Step 4: Read the received data from the SPI Data Register (not used here
  // but could be handled)
  rec_data = SPI0.DATA;

  // Step 5: Set SS (Slave Select) to high to deselect the slave
  SPI_PORT.OUTSET = SPI_SS; // Set SS high to de-select the slave

  return rec_data; // Return the received data (if needed)
}

static inline void mcp4131_increment(void) {
  MCP4131_SPI_PORT.OUT &= ~MCP4131_CS_PIN;
  mcp4131_spi_transfer(0x04);
  MCP4131_SPI_PORT.OUT |= MCP4131_CS_PIN;
}

static inline void mcp4131_decrement(void) {
  MCP4131_SPI_PORT.OUT &= ~MCP4131_CS_PIN;
  mcp4131_spi_transfer(0x08);
  MCP4131_SPI_PORT.OUT |= MCP4131_CS_PIN;
}


// MCP4131 Send 16-bit Data (No SS toggling between bytes) - Polling version
void MCP4131_SPI_Send(uint8_t upper_data, uint8_t lower_data) {
  // Set SS low to activate MCP4131 slave
  SPI_PORT.OUTCLR = SPI_SS;

  // Send upper byte
  SPI0.DATA = upper_data;
  loop_until_bit_is_set(SPI0.INTFLAGS, SPI_IF_bp);

  // Send lower byte
  SPI0.DATA = lower_data;
  loop_until_bit_is_set(SPI0.INTFLAGS, SPI_IF_bp);

  // Set SS high to deactivate MCP4131 slave
  SPI_PORT.OUTSET = SPI_SS;
}

// MCP4131 Send 16-bit Data using interrupts
bool MCP4131_SPI_Send_Interrupt(uint8_t upper_data, uint8_t lower_data) {
  if (spi_busy) {
    return false; // Another transfer is still in progress
  }

  spi_upper_byte = upper_data;
  spi_lower_byte = lower_data;
  spi_state = 1; // Start with upper byte
  spi_busy = true;

  // Clear any lingering interrupt flag before starting
  SPI0.INTFLAGS = SPI_IF_bm;

  // Set SS low to activate MCP4131 slave
  SPI_PORT.OUTCLR = SPI_SS;

  // Initiate transfer; ISR advances the state machine
  SPI0.DATA = spi_upper_byte;
  return true;
}

// Check if SPI is busy
bool MCP4131_SPI_Busy(void) {
  return spi_busy;
}

// SPI Interrupt Service Routine
ISR(SPI0_INT_vect) {
  uint8_t flags = SPI0.INTFLAGS;
  uint8_t received = SPI0.DATA;
  (void)flags;
  (void)received;

  if (spi_state == 1) {
    // Upper byte complete, now send lower byte
    spi_state = 2;
    SPI0.DATA = spi_lower_byte;
  } else if (spi_state == 2) {
    // Lower byte complete, deactivate slave
    SPI_PORT.OUTSET = SPI_SS; // Set SS high
    spi_state = 0;            // Return to idle
    spi_busy = false;
  } else {
    // Unexpected state, release bus and recover
    SPI_PORT.OUTSET = SPI_SS;
    spi_state = 0;
    spi_busy = false;
  }
}
