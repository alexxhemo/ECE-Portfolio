#include "i2c_lib_S25.h"
#include "lcd.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <util/delay.h>

// Backlight register definitions (from DFRobot examples)
#define BL_REG_MODE1 0x00
#define BL_REG_MODE2 0x01
#define BL_REG_OUTPUT 0x08

// PWM registers for RGB channels - 0x6B uses different mapping!
// For address 0x6B: RED=0x06, GREEN=0x05, BLUE=0x04, and requires 0x07=0xFF
#define BL_REG_RED_6B 0x06
#define BL_REG_GREEN_6B 0x05
#define BL_REG_BLUE_6B 0x04

// Mutable register mapping (will be updated by probe if needed)
static uint8_t bl_reg_red = BL_REG_RED_6B;
static uint8_t bl_reg_green = BL_REG_GREEN_6B;
static uint8_t bl_reg_blue = BL_REG_BLUE_6B;

// Backlight address discovered at probe time (fallback to
// I2C_BACKLIGHT_ADDRESS)
#ifndef I2C_BACKLIGHT_ADDRESS
#define I2C_BACKLIGHT_ADDRESS 0x6B
#endif
static uint8_t bl_i2c_addr = I2C_BACKLIGHT_ADDRESS;

// Low-level write helper that reports success (0) or failure (-1)
static int BACKLIGHT_writeRegister_probe(uint8_t address, uint8_t reg,
                                         uint8_t data) {
  if (TWI1_Address(address, TW_WRITE) != 0) {
    TWI1_Stop();
    return -1;
  }
  if (TWI1_Transmit_Data(reg) != 0) {
    TWI1_Stop();
    return -1;
  }
  if (TWI1_Transmit_Data(data) != 0) {
    TWI1_Stop();
    return -1;
  }
  TWI1_Stop();
  return 0;
}

int BACKLIGHT_probe_and_configure(void) {
  // For 0x6B specifically, use the DFRobot library initialization sequence
  if (I2C_BACKLIGHT_ADDRESS == 0x6B) {
    printf("BACKLIGHT: Using 0x6B with DFRobot init sequence\n");
    bl_i2c_addr = 0x6B;
    bl_reg_red = BL_REG_RED_6B;     // 0x06
    bl_reg_green = BL_REG_GREEN_6B; // 0x05
    bl_reg_blue = BL_REG_BLUE_6B;   // 0x04

    // DFRobot specific init for 0x6B - critical sequence!
    // From reference: setReg(0x2F, 0x00); setReg(0x00, 0x20); etc.
    BACKLIGHT_writeRegister_probe(0x6B, 0x2F, 0x00);
    BACKLIGHT_writeRegister_probe(0x6B, 0x00, 0x20);
    BACKLIGHT_writeRegister_probe(0x6B, 0x01, 0x00);
    BACKLIGHT_writeRegister_probe(0x6B, 0x02, 0x01);
    BACKLIGHT_writeRegister_probe(0x6B, 0x03, 0x04);

    printf("BACKLIGHT: 0x6B configured with R=0x06 G=0x05 B=0x04\n");
    return 0;
  }

  // For other addresses, try probe logic
  uint8_t addrCandidates[] = {I2C_BACKLIGHT_ADDRESS, 0x60, 0x62};
  const size_t nAddr = sizeof(addrCandidates) / sizeof(addrCandidates[0]);

  uint8_t rgbPerms[][3] = {
      {0x06, 0x05, 0x04}, // 0x6B mapping
      {0x04, 0x03, 0x02}, // common alt
      {0x02, 0x03, 0x04},
      {0x04, 0x02, 0x03},
  };
  const size_t nPerms = sizeof(rgbPerms) / sizeof(rgbPerms[0]);

  for (size_t a = 0; a < nAddr; ++a) {
    uint8_t addr = addrCandidates[a];

    // Address responded. Try RGB register permutations
    for (size_t p = 0; p < nPerms; ++p) {
      uint8_t rreg = rgbPerms[p][0];
      uint8_t greg = rgbPerms[p][1];
      uint8_t breg = rgbPerms[p][2];

      // Try writing full intensity to each and see if writes ACK
      int r_ok = BACKLIGHT_writeRegister_probe(addr, rreg, 0xFF) == 0;
      int g_ok = BACKLIGHT_writeRegister_probe(addr, greg, 0xFF) == 0;
      int b_ok = BACKLIGHT_writeRegister_probe(addr, breg, 0xFF) == 0;

      if (r_ok && g_ok && b_ok) {
        bl_i2c_addr = addr;
        bl_reg_red = rreg;
        bl_reg_green = greg;
        bl_reg_blue = breg;
        printf("BACKLIGHT: detected addr 0x%02X, regs R=0x%02X G=0x%02X "
               "B=0x%02X\n",
               addr, rreg, greg, breg);
        return 0;
      }
    }
    printf("BACKLIGHT: addr 0x%02X no working RGB mapping found\n", addr);
  }

  printf("BACKLIGHT: probe failed, using defaults\n");
  return -1;
}

void TWI_Stop() { TWI0.MCTRLB |= TWI_MCMD_STOP_gc; }

void TWI_Host_Initialize() {
  // Step 1: Set communication rate
  // #define TWI0_BAUD(F_SCL, T_RISE) \
  // ((((((float)CLK_PER / (float)F_SCL)) - 10 - ((float)CLK_PER * T_RISE))) /
  // 2)
  TWI0.MBAUD = 95; // SCLK = 80kHz; rise time = 10ns

  // Step 2: enable the I2C
  TWI0.MCTRLA |= TWI_ENABLE_bm;

  // Step 3: force the bus state to IDLE
  TWI0.MSTATUS |= TWI_BUSSTATE_IDLE_gc;
}

// Stop for TWI1
void TWI1_Stop() { TWI1.MCTRLB |= TWI_MCMD_STOP_gc; }

// Initialize TWI1 (used for LCD/backlight)
void TWI1_Host_Initialize() {
  TWI1.MBAUD = 95; // ~80kHz
  TWI1.MCTRLA |= TWI_ENABLE_bm;
  TWI1.MSTATUS = TWI_BUSSTATE_IDLE_gc;
}

// TWI0 address send helper (returns 0 on ACK, -1 on error)
int TWI_Address(uint8_t Address, uint8_t mode) {
  uint32_t timeout = 20000;
  uint8_t addressWithMode = (Address << 1) | (mode & 0x1);
  TWI0.MADDR = addressWithMode;
  uint32_t flagMask =
      (mode == TW_WRITE) ? (1UL << TWI_WIF_bp) : (1UL << TWI_RIF_bp);
  while (!(TWI0.MSTATUS & flagMask) && timeout--) {
    ;
  }
  if (timeout == 0) {
    return -1;
  }
  if (TWI0.MSTATUS & TWI_RXACK_bm) {
    TWI_Stop();
    return -1;
  }
  if ((TWI0.MSTATUS & TWI_ARBLOST_bm) || (TWI0.MSTATUS & TWI_BUSERR_bm)) {
    return -1;
  }
  return 0;
}

int TWI1_Address(uint8_t Address, uint8_t mode) {
  // Attempt to send address and wait for the relevant interrupt flag.
  // Use a timeout to avoid blocking forever if the bus is stuck.
  uint32_t timeout = 20000;

  // Step 1: Shift Address left by 1 and OR with mode
  uint8_t addressWithMode = (Address << 1) | (mode & 0x1);

  // Step 2: Set the Address Register to updated address
  TWI1.MADDR = addressWithMode;

  // Choose flag mask based on mode
  uint32_t flagMask =
      (mode == TW_WRITE) ? (1UL << TWI_WIF_bp) : (1UL << TWI_RIF_bp);

  // Wait for the interrupt flag or timeout
  while (!(TWI1.MSTATUS & flagMask) && timeout--) {
    ;
  }

  if (timeout == 0) {
    // timeout
    return -1;
  }

  // If client NACKed, issue STOP and return error
  if (TWI1.MSTATUS & TWI_RXACK_bm) {
    TWI1_Stop();
    return -1;
  }

  // Check for arbitration lost or bus error
  if ((TWI1.MSTATUS & TWI_ARBLOST_bm) || (TWI1.MSTATUS & TWI_BUSERR_bm)) {
    return -1;
  }

  return 0;
}

int TWI_Transmit_Data(uint8_t data) {
  // Step 1: Start the data transfer by writing the data to register
  TWI0.MDATA = data;

  // Step 2: Wait for the Write Interrupt Flag (WIF) to be set with timeout
  uint32_t timeout = 10000;
  while (!(TWI0.MSTATUS & TWI_WIF_bm) && timeout--) {
    ;
  }

  if (timeout == 0) {
    return -1;
  }

  // Step 3: Check for errors after the flag is set
  if (TWI0.MSTATUS & TWI_ARBLOST_bm) {
    return -1;
  } else if (TWI0.MSTATUS & TWI_BUSERR_bm) {
    return -1;
  } else {
    return 0;
  }
}

int TWI_Receive_Data() {
  // Wait until the Read Interrupt Flag (RIF) is set or timeout
  uint32_t timeout = 10000;
  while (!(TWI0.MSTATUS & TWI_RIF_bm) && timeout--) {
    ;
  }

  if (timeout == 0) {
    return -1;
  }

  int data = (int)TWI0.MDATA;

  // Respond with NACK (end of transfer)
  TWI0.MCTRLB |= TWI_ACKACT_bm;

  return data;
}

// Function used for writing to TC74 temp sensor
void TWI_Host_Write(uint8_t Address, uint8_t Command, uint8_t Data) {
  // Step 1: Write the Address
  if (TWI_Address(Address, TW_WRITE) != 0) {
    TWI_Stop();
    return;
  }

  // Step 2: Transmit the Command/Register you want to write to
  if (TWI_Transmit_Data(Command) != 0) {
    TWI_Stop();
    return;
  }

  // Step 3: Transmit the data you want to write to the register
  if (TWI_Transmit_Data(Data) != 0) {
    TWI_Stop();
    return;
  }

  // Step 4: Stop Transmission
  TWI_Stop();
}

// Function used for reading from TC74 temp sensor
int TWI_Host_Read(uint8_t Address, uint8_t Command) {
  // Step 1: Write the Address (write mode)
  if (TWI_Address(Address, TW_WRITE) != 0) {
    TWI_Stop();
    return -1;
  }

  // Step 2: Transmit the Command/Register you want to read from
  if (TWI_Transmit_Data(Command) != 0) {
    TWI_Stop();
    return -1;
  }

  // Step 3: Repeated start and send address with read
  if (TWI_Address(Address, TW_READ) != 0) {
    TWI_Stop();
    return -1;
  }

  // Step 4: Receive Data
  int data = TWI_Receive_Data();

  // Step 5: Stop Transmission
  TWI_Stop();

  // Step 6: Return Data or -1 on error
  return data;
}

int TWI1_Transmit_Data(uint8_t data) {
  // Step 1: Start the data transfer by writing the data to register
  TWI1.MDATA = data;

  // Step 2: Wait for the Write Interrupt Flag (WIF) to be set with timeout
  uint32_t timeout = 10000;
  while (!(TWI1.MSTATUS & TWI_WIF_bm) && timeout--) {
    ;
  }

  if (timeout == 0) {
    return -1;
  }

  // Step 3: Check for errors after the flag is set
  if (TWI1.MSTATUS & TWI_ARBLOST_bm) {
    return -1;
  } else if (TWI1.MSTATUS & TWI_BUSERR_bm) {
    return -1;
  } else {
    return 0;
  }
}

int TWI1_Receive_Data() {
  // Wait until the Read Interrupt Flag (RIF) is set or timeout
  uint32_t timeout = 10000;
  while (!(TWI1.MSTATUS & TWI_RIF_bm) && timeout--) {
    ;
  }

  if (timeout == 0) {
    return -1;
  }

  int data = (int)TWI1.MDATA;

  // Respond with NACK (end of transfer)
  TWI1.MCTRLB |= TWI_ACKACT_bm;

  return data;
}

// Function used for writing to TC74 temp sensor
void TWI1_Host_Write(uint8_t Address, uint8_t Command, uint8_t Data) {
  // Step 1: Write the Address
  if (TWI1_Address(Address, TW_WRITE) != 0) {
    TWI1_Stop();
    return;
  }

  // Step 2: Transmit the Command/Register you want to write to
  if (TWI1_Transmit_Data(Command) != 0) {
    TWI1_Stop();
    return;
  }

  // Step 3: Transmit the data you want to write to the register
  if (TWI1_Transmit_Data(Data) != 0) {
    TWI1_Stop();
    return;
  }

  // Step 4: Stop Transmission
  TWI1_Stop();
}

// Function used for reading
int TWI1_Host_Read(uint8_t Address, uint8_t Command) {
  // Step 1: Write the Address (write mode)
  if (TWI1_Address(Address, TW_WRITE) != 0) {
    TWI1_Stop();
    return -1;
  }

  // Step 2: Transmit the Command/Register you want to write to
  if (TWI1_Transmit_Data(Command) != 0) {
    TWI1_Stop();
    return -1;
  }

  // Step 3: Repeated start and send address with read
  if (TWI1_Address(Address, TW_READ) != 0) {
    TWI1_Stop();
    return -1;
  }

  // Step 4: Receive Data
  int data = TWI1_Receive_Data();

  // Step 5: Stop Transmission
  TWI1_Stop();

  // Step 6: Return Data or -1 on error
  return data;
}

void LCD_sendData(uint8_t data) {
  TWI1.MADDR = (LCD_ADDR << 1);
  while (!(TWI1.MSTATUS & TWI_WIF_bm))
    ;
  TWI1.MDATA = LCD_DATA_MODE;
  while (!(TWI1.MSTATUS & TWI_WIF_bm))
    ;
  TWI1.MDATA = data;
  while (!(TWI1.MSTATUS & TWI_WIF_bm))
    ;
  TWI1.MCTRLB = TWI_MCMD_STOP_gc;
}

void LCD_init(void) {
  _delay_ms(50); // Wait for LCD reset
  LCD_sendCommand(0x38);
  // Function set: 2 lines, 5x8 font
  LCD_sendCommand(0x0C);
  // Display ON, cursor OFF
  LCD_sendCommand(0x01); // Clear display
  LCD_sendCommand(0x06);
  // Entry mode: increment
  _delay_ms(2);
}

void LCD_sendCommand(uint8_t cmd) {
  TWI1.MADDR = (LCD_ADDR << 1); // Write mode
  while (!(TWI1.MSTATUS & TWI_WIF_bm))
    ;
  TWI1.MDATA = LCD_CMD_MODE;
  while (!(TWI1.MSTATUS & TWI_WIF_bm))
    ;
  TWI1.MDATA = cmd;
  while (!(TWI1.MSTATUS & TWI_WIF_bm))
    ;
  TWI1.MCTRLB = TWI_MCMD_STOP_gc;
}

void secondLine(void) {
  // Moves the cursor to second line (DDRAM address 0x40)
  LCD_sendCommand(0xC0);
}

void LCD_print(const char *str) {
  while (*str) {
    LCD_sendData(*str++);
  }
}

void BACKLIGHT_setReg(uint8_t reg, uint8_t data) {
  // Write reg,data to backlight controller via TWI1
  TWI1_Host_Write(bl_i2c_addr, reg, data);
}

void BACKLIGHT_setRGB(uint8_t r, uint8_t g, uint8_t b) {
  // Write PWM values for R,G,B using detected mapping/address
  TWI1_Host_Write(bl_i2c_addr, bl_reg_red, r);
  TWI1_Host_Write(bl_i2c_addr, bl_reg_green, g);
  TWI1_Host_Write(bl_i2c_addr, bl_reg_blue, b);

  // For 0x6B specifically, must also write 0xFF to register 0x07
  if (bl_i2c_addr == 0x6B) {
    TWI1_Host_Write(bl_i2c_addr, 0x07, 0xFF);
  }
}

void BACKLIGHT_init(void) {
  // Probe and auto-configure the backlight controller mapping. This
  // addresses modules like DFR0555 that use slightly different addresses
  // or register maps and may NACK certain reads — we only perform writes.
  int ok = BACKLIGHT_probe_and_configure();
  (void)ok; // ok logged via UART

  // Note: For 0x6B, probe already did the full init sequence.
  // For other addresses, do standard init here if needed.
  if (bl_i2c_addr != 0x6B) {
    BACKLIGHT_setReg(BL_REG_MODE1, 0x00);
    BACKLIGHT_setReg(BL_REG_MODE2, 0x20);
    BACKLIGHT_setReg(BL_REG_OUTPUT, 0xFF);
  }

  // Default to white backlight
  BACKLIGHT_setRGB(255, 255, 255);
}

void BACKLIGHT_setBacklight100(void) { BACKLIGHT_setRGB(255, 255, 255); }

void BACKLIGHT_setBacklight50(void) { BACKLIGHT_setRGB(128, 128, 128); }
