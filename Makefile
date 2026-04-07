MCU = attiny202
F_CPU = 20000000UL
DFP = $(HOME)/attiny_dfp
CC = $(HOME)/avr8-gnu-toolchain-linux_x86_64/bin/avr-gcc
CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -std=c11 -I src -B $(DFP)/gcc/dev/$(MCU)/ -isystem $(DFP)/include
ASFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Wall -I src -B $(DFP)/gcc/dev/$(MCU)/ -isystem $(DFP)/include

SRC_C = $(wildcard src/*.c)
SRC_S = $(wildcard src/*.S)
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC_C)) $(patsubst src/%.S,obj/%.o,$(SRC_S))
TARGET = gcio-fw.elf

all: obj $(TARGET)

obj:
	mkdir -p obj

$(TARGET): $(OBJ)
	$(CC) -mmcu=$(MCU) -B $(DFP)/gcc/dev/$(MCU)/ -o $@ $^

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.S
	$(CC) $(ASFLAGS) -c -o $@ $<

size: $(TARGET)
	avr-size --mcu=$(MCU) --format=avr $(TARGET)

hex: $(TARGET)
	avr-objcopy -O ihex -R .eeprom $(TARGET) gcio-fw.hex

clean:
	rm -rf obj $(TARGET) gcio-fw.hex

.PHONY: all size hex clean