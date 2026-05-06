// LCD interface for AIP31068 LCD controllers
// By John Chandy, 2024

#include <inttypes.h>

#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif

void LCDinitialize(void); // Initializes LCD
void LCDdataWrite(uint8_t);
void LCDclr(void);                // Clears LCD
void LCDhome(void);               // LCD cursor home
void LCDstring(char *);           // Outputs string to LCD
void LCDgotoXY(uint8_t, uint8_t); // Cursor to X Y position
void LCDcopyStringFromFlash(const uint8_t *, uint8_t,
                            uint8_t); // copies flash string to LCD at x,y
void LCDshiftRight(uint8_t);          // shift by n characters Right
void LCDshiftLeft(uint8_t);           // shift by n characters Left
void LCDcursorOn(void);               // cursor ON
void LCDcursorOnBlink(void);          // Blinking cursor ON
void LCDcursorOff(void);              // Both underline and blinking cursor OFF
void LCDblank(void);                  // LCD blank but not cleared
void LCDvisible(void);                // LCD visible
void LCDcursorLeft(uint8_t);          // shift cursor left by n
void LCDcursorRight(uint8_t);         // shift cursor right by n

// DF Robot / AIP31068 I2C LCD definitions expected by the implementation
// (Address 0x3E for the LCD controller; control bytes: 0x00 = command, 0x40 =
// data)
#define LCD_ADDR 0x3E
#define LCD_CMD_MODE 0x00
#define LCD_DATA_MODE 0x40

// Implementations present in `include/i2c_lib_S25.c`
void LCD_init(void);
void LCD_sendCommand(uint8_t cmd);
void LCD_sendData(uint8_t data);
void LCD_print(const char *str);
void secondLine(void);
