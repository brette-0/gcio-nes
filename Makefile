MCU = atmega168
F_CPU = 20000000UL
CC = avr-gcc
CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -std=c11 -I src
ASFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Wall -I src

SIMAVR_DIR = $(HOME)/simavr

SRC_C = $(wildcard src/*.c)
SRC_S = $(wildcard src/*.S)
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC_C)) $(patsubst src/%.S,obj/%.o,$(SRC_S))
TARGET = gc2nes.elf

all: obj $(TARGET)

obj:
	mkdir -p obj

$(TARGET): $(OBJ)
	$(CC) -mmcu=$(MCU) -o $@ $^

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.S
	$(CC) $(ASFLAGS) -c -o $@ $<

sim: $(TARGET)
	$(SIMAVR_DIR)/simavr/run_avr -m $(MCU) -f $(F_CPU) $(TARGET)

clean:
	rm -rf obj $(TARGET)

.PHONY: all sim clean
