CC=avr-gcc
CFLAGS=-Os -g -Wall -mmcu=attiny2313 -DF_CPU=16000000UL -MP -MD -std=gnu99
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
PROGRAMMER=usbasp-clone
PORT=ussb
AVRDUDE_TARGET=t2313
SRC=midi-to-cv.c

all: midi-to-cv.elf

%.elf: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

.PHONY: clean

clean:
	-rm *.elf *.o *.d *.hex

fuses:
	avrdude -p $(AVRDUDE_TARGET) -c $(PROGRAMMER) -e -U lfuse:w:0xFF:m

%.prog: %.hex
	avrdude -p $(AVRDUDE_TARGET) -c $(PROGRAMMER) -v -e \
	 -U flash:w:$<

-include $(SRC:.c=.d)
