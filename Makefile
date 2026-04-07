# === Firmware (ATtiny202) ===
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

# === Simulation (host x86, C++) ===
SIM_CXX = g++
SIM_CXXFLAGS = -Wall -std=c++17 -g -I src -I sim -DSIMULATION
SIM_CFLAGS = -Wall -std=c11 -g -I src -I sim -DSIMULATION
SIM_SRC_CPP = $(wildcard sim/*.cpp)
SIM_SHARED = src/tables.c src/main.c
SIM_OBJ = $(patsubst sim/%.cpp,obj/sim_%.o,$(SIM_SRC_CPP)) $(patsubst src/%.c,obj/sim_shared_%.o,$(SIM_SHARED))
SIM_TARGET = gcio-sim

# === Firmware targets ===
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

# === Simulation targets ===
sim: obj $(SIM_TARGET)

$(SIM_TARGET): $(SIM_OBJ)
	$(SIM_CXX) -o $@ $^

obj/sim_%.o: sim/%.cpp
	$(SIM_CXX) $(SIM_CXXFLAGS) -c -o $@ $<

obj/sim_shared_%.o: src/%.c
	gcc $(SIM_CFLAGS) -c -o $@ $<

run-sim: sim
	./$(SIM_TARGET)

# === Cleanup ===
clean:
	rm -rf obj $(TARGET) $(SIM_TARGET) gcio-fw.hex

.PHONY: all sim run-sim size hex clean
