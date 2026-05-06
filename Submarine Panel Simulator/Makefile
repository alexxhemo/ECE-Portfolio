
MCU     = avr128db48
CC      = avr-gcc
CFLAGS  = -g -Wall -Os -mmcu=$(MCU) -mcall-prologues -Iinclude
LDFLAGS = -Wl,-gc-sections -Wl,-relax
TARGET  = main

# --- Sources & Objects ---
SRC  = main.c $(wildcard include/*.c)
OBJ  = $(SRC:%.c=build/%.o)

# --- Default Target ---
all: build/$(TARGET).hex

# --- Build Rules ---
build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/$(TARGET).elf: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

build/$(TARGET).hex: build/$(TARGET).elf
	avr-objcopy -R .eeprom -O ihex $< $@

# --- Flash ---
program: build/$(TARGET).hex
	avrdude -p $(MCU) -c pkobn_updi -P usb -U flash:w:$<

# --- Clean ---
clean:
	rm -rf build
