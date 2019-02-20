enum {
	INPUT,
	OUTPUT,
	INPUT_PULLUP,
};

ALWAYS_INLINE static inline byte
pin_mask(byte pin)
{
	if (pin < 8)
		return _BV(pin);
	else if (pin < 8 + 8)
		return _BV(pin - 8);
	else
		return _BV(pin - (8 + 8));
}

ALWAYS_INLINE static inline volatile byte *
pin_out_reg(byte pin)
{
	if (pin < 8)
		return &PORTD;
	else if (pin < 8 + 8)
		return &PORTB;
	else
		return &PORTA;
}

ALWAYS_INLINE static inline volatile byte *
pin_in_reg(byte pin)
{
	if (pin < 8)
		return &PIND;
	else if (pin < 8 + 8)
		return &PINB;
	else
		return &PINA;
}


ALWAYS_INLINE static inline volatile byte *
pin_dir_reg(byte pin)
{
	if (pin < 8)
		return &DDRD;
	else if (pin < 8 + 8)
		return &DDRB;
	else
		return &DDRA;
}


ALWAYS_INLINE static inline void
pin_write(byte pin, byte val)
{
	if (val)
		*pin_out_reg(pin) |= pin_mask(pin);
	else
		*pin_out_reg(pin) &= ~pin_mask(pin);
}

ALWAYS_INLINE static inline void
pin_toggle(byte pin)
{
	*pin_out_reg(pin) ^= pin_mask(pin);
}

ALWAYS_INLINE static inline byte
pin_read(byte pin)
{
	return (*pin_in_reg(pin)) & pin_mask(pin);
}

ALWAYS_INLINE static inline void
pin_mode(byte pin, byte mode)
{
	if (mode == OUTPUT) {
		*pin_dir_reg(pin) |= pin_mask(pin);
	} else if (mode == INPUT) {
		*pin_dir_reg(pin) &= ~pin_mask(pin);
		*pin_out_reg(pin) &= ~pin_mask(pin);
	} else if (mode == INPUT_PULLUP) {
		*pin_dir_reg(pin) &= ~pin_mask(pin);
		*pin_out_reg(pin) |= pin_mask(pin);
	}
}

enum {
	NORMAL,
	INVERTED,
};

ALWAYS_INLINE static inline void
strobe(byte pin, byte mode)
{
	if (mode == NORMAL) {
		pin_write(pin, 1);
		pin_write(pin, 0);
	} else {
		pin_write(pin, 0);
		pin_write(pin, 1);
	}
}

/* shift out most significiant byte first */
ALWAYS_INLINE static inline void
shift_out_msb(byte clock, byte data, byte value)
{
	for (byte i = 0; i < 8; i++) {
		pin_write(data, value & 0x80);
		strobe(clock, NORMAL);
		value <<= 1;
	}
}

ALWAYS_INLINE static inline byte
shift_in_msb(byte clock, byte data)
{
	byte x = 0;
	for (byte i = 0; i < 8; i++) {
		x = (x << 1) | !!pin_read(data);
		strobe(clock, NORMAL);
	}
	return x;
}
