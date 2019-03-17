enum {
	PotThreshold = 7,
	PotBits = 7,
};

static byte admux_val = 0;

static void
adc_init(void)
{
	/* bring power to ADC */
	PRR &= ~PRADC;
	/* set prescaler to 7 (16MHz/128 == 125KHz) */
	ADCSRA = (ADCSRA & ~7) | 7;
	/* enable ADC */
	ADCSRA |= _BV(ADEN);

	/* set reference to Vcc */
	admux_val |= (1 << 6);
	/* set alignment to right */
	admux_val &= ~_BV(ADLAR);
}

static uint16_t
adc_scan(byte input)
{
	uint16_t out;

	/* select pin */
	ADMUX = admux_val | input;

	/* start conversion */
	ADCSRA |= _BV(ADSC);
	/* wait for it to end */
	while (ADCSRA & _BV(ADSC))
		;
	/* read result */
	out = ADCL;
	out |= (uint16_t) ADCH << 8;

	return out;
}

#define POT_SCALE(x) ((x) >> (10 - PotBits))

/* needs PotBits <= 8 */
static byte
pot_scan(byte input, uint16_t *prev, uint8_t *out_p)
{
	uint16_t old, new;
	int16_t diff;

	/* scan pot */
	old = *prev;
	new = adc_scan(input);

	/* is the difference greater than threshold? */
	diff = (int16_t)old - new;
	if (diff < 0)
		diff = -diff;
	if (diff < PotThreshold) {
		/* value didn't change */
		return 0;
	}

	/* store new value */
	*prev = new;
	/* if scaled value changed, store it */
	byte out = POT_SCALE(new);
	if (out != POT_SCALE(old)) {
		*out_p = out;
		return 1;
	} else {
		return 0;
	}
}
