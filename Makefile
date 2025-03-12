include ./make.conf

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build

CC = avr-gcc
OBJCOPY = avr-objcopy
CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -I$(INCLUDE_DIR) -std=c23
LDFLAGS = -mmcu=$(MCU)

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

ELF = $(BUILD_DIR)/program.elf
HEX = $(BUILD_DIR)/program.hex

all: $(HEX)

# $(HEX) is the final program we upload to the controller (build/program.hex)
# and $(ELF) would be the linked binary (build/program.elf)
# so to build $(HEX) we need to build $(ELF)
$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

# $@ is the name of the target $(ELF) (build/program.elf)
# $^ is the name of all pre-requisites (all the .o files in this case)
# links all compiled object files to the target
$(ELF): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# $@ is the name of the target (eg. file.o)
# $< is the first pre-requisite (eg. file.c in this case)
# %.c makes this target for all .c files in our $(SRC_DIR)
# | $(BUILD_DIR) means that the $(BUILD_DIR) target is required
# for this target but it's never considered out of date
# a so called order-only pre-requisite
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# $(BUILD_DIR) target simply creates the build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

flash: $(HEX)
	avrdude -c $(PROGRAMMER) -p $(MCU) -P $(PORT) -b $(BAUD_RATE) -U flash:w:$<:i

# tells make that these are not file targets
# so even if our project contains a file with one of these
# names it is not associated with this target
.PHONY: all clean flash

