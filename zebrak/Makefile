CC=avr-gcc
CFLAGS=-Os -g -ggdb -Wall -mmcu=atmega328p -DF_CPU=16000000UL -MP -MD -std=gnu99 -Wno-unused-function -Wno-unused-local-typedefs
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
PROG=zebrak
SRC=zebrak.c
PROGRAMMER=arduino
AVRDUDE_TARGET=atmega328p

all: $(PROG).hex

$(PROG).elf: $(SRC:.c=.o)

zebrak.o: tables.h
%.elf: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	avr-size $@
%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%_eeprom.hex: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@

tables.h: utils/gen_tables.py
	python3 $< > tables.h

.PHONY: clean

clean:
	-rm *.elf *.o *.d *.hex

arduino: PROGRAMMER=arduino
arduino: prog

usbasp: PROGRAMMER=usbasp-clone
usbasp: prog

prog: $(PROG).hex
	@if [ $$(avr-size $< | awk 'NR==2 { print $$2 }') -ge 28672 ]; then \
		echo "too big"; \
	else \
		avrdude -p $(AVRDUDE_TARGET) -c $(PROGRAMMER) -b 115200 -P /dev/ttyUSB0 -v -e -D -U flash:w:$(PROG).hex; \
	fi

-include $(SRC:.c=.d)
