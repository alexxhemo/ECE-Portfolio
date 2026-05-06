#ifndef I2C_H
#define I2C_H

#include <avr/io.h>
#include <stdbool.h>
#include <util/twi.h>

#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif
// Define the I2C pins
#define I2C_PORT PORTA
#define I2C_SDA PIN2_bm
#define I2C_SCL PIN3_bm
#define TW_WRITE 0
#define TW_READ 1

void TWI_Stop();

void TWI_Host_Initialize();

// TWI1 (alternate hardware instance) helpers
void TWI1_Stop(void);
void TW1_init_100k(void);
void TWI1_Host_Initialize(void);
int TWI1_Address(uint8_t Address, uint8_t mode);
int TWI1_Transmit_Data(uint8_t data);
int TWI1_Receive_Data();
void TWI1_Host_Write(uint8_t Address, uint8_t Command, uint8_t Data);
int TWI1_Host_Read(uint8_t Address, uint8_t Command);

// LCD definitions
void LCD_print(const char *str);
void secondLine(void);
void LCD_sendCommand(uint8_t cmd);
void LCD_sendData(uint8_t data);
void LCD_init(void);

// Backlight (RGB) control for DF Robot module at address 0x6B
void BACKLIGHT_setReg(uint8_t reg, uint8_t data);
void BACKLIGHT_setRGB(uint8_t r, uint8_t g, uint8_t b);
void BACKLIGHT_init(void);
void BACKLIGHT_setBacklight100(void);

void BACKLIGHT_setBacklight50(void);

// Probe and auto-detect backlight address/register mapping. Returns 0 if
// detected.
int BACKLIGHT_probe_and_configure(void);

// Attempt to send address+mode. Returns 0 on success, -1 on timeout/error.
int TWI_Address(uint8_t Address, uint8_t mode);

int TWI_Transmit_Data(uint8_t data);

// Receive one byte. Returns byte (0..255) on success, -1 on timeout/error.
int TWI_Receive_Data();

// Function used for writing to TC74 temp sensor
void TWI_Host_Write(uint8_t Address, uint8_t Command, uint8_t Data);

// Function used for reading from TC74 temp sensor
// Returns received byte (0..255) on success, -1 on error.
int TWI_Host_Read(uint8_t Address, uint8_t Command);

#endif
