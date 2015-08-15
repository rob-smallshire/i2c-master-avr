# Target name is current directory name
TARGET = $(lastword $(subst /, ,$(CURDIR)))

# Hardware
F_CPU = 16000000UL
MCU = atmega2560

# Connection
PORT = /dev/ttyACM0
BAUD = 115200 

# Tools
CC = avr-gcc
CXX = avr-g++
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
AVRSIZE = avr-size
AVRDUDE = avrdude

TARGET_ARCH= -mmcu=$(MCU)

PROGRAMMER_TYPE = wiring
PROGRAMMER_ARGS = -F -V -P $(PORT) -b $(BAUD)

CPPFLAGS = -DF_CPU=$(F_CPU)

OPTIMIZATION= -Os
CODEGEN= -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -ffunction-sections -fdata-sections 
WARNINGS= -Wall -Wextra


CC=avr-gcc

CFLAGS=-std=gnu99 $(OPTIMIZATION) $(WARNINGS) $(CODEGEN)

CXX=avr-g++
CXXFLAGS= $(OPTIMIZATION) $(WARNINGS) $(CODEGEN)

LDFLAGS = -Wl,-Map,$(TARGET).map -Wl,--gc-sections -Wl,-u,vfprintf -lprintf_flt -lm

CSOURCES=echo.c uart.c debounce.c joystick.c

CXXSOURCES=

OBJECTS=$(CSOURCES:.c=.o)
OBJECTS+=$(CXXSOURCES:.cpp=.o)

%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $(CFLAGS) $(TARGET_ARCH) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.d: %.cpp
	@set -e; rm -f $@; \
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) $(TARGET_ARCH) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

include $(CSOURCES:.c=.d)
include $(CXXSOURCES:.cpp=.d)

$(TARGET).elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) $(TARGET_ARCH) $^ $(LDLIBS) -o $@

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%.eeprom: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@ 

%.lst: %.elf
	$(OBJDUMP) -S $< > $@

# Default target
.DEFAULT_GOAL := hex

# Phony targets
.PHONY: hex disasm size clean

hex: $(TARGET).hex

disasm: $(TARGET).lst

eeprom: $(TARGET).eeprom

size:  $(TARGET).elf
	$(AVRSIZE) -C --mcu=$(MCU) $(TARGET).elf

clean:
	rm -f $(TARGET).elf $(TARGET).hex $(TARGET).map $(TARGET).lst $(TARGET).eeprom $(OBJECTS)

flash: $(TARGET).hex 
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -U flash:w:$<

serial:
	gtkterm --port $(PORT) --speed 9600 &
