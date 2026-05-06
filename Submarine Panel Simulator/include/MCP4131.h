#ifndef MCP4131_H_
#define MCP4131_H_

#ifndef F_CPU
#define F_CPU 16000000UL // 16MHz clock
#endif
#ifndef __AVR_AVR128DB48__
#define __AVR_AVR128DB48__
#endif

#include <avr/io.h>
#include <stdbool.h>

// Define the SPI pins
#define SPI_PORT PORTA
#define SPI_MOSI PIN4_bm
#define SPI_SCK PIN6_bm
#define SPI_SS PIN7_bm

// Function Prototypes
void SPI_Host_Init(void);
uint8_t SPI_Host_Transceiver(uint8_t rec_data, uint8_t pushed_data);

// Needs to send 16 bits to communicate does not need to receive
void MCP4131_SPI_Send(uint8_t upper_data, uint8_t lower_data);

// Interrupt-based functions
bool MCP4131_SPI_Send_Interrupt(uint8_t upper_data, uint8_t lower_data);
bool MCP4131_SPI_Busy(void);

// External variables for ISR
extern volatile uint8_t spi_upper_byte;
extern volatile uint8_t spi_lower_byte;
extern volatile uint8_t spi_state;
extern volatile bool spi_busy;

#endif // MCP4131_H_
