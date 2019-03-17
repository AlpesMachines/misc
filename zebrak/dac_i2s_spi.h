enum {
	DacLeft = 1,
	DacRight = 0,
};

STATIC_ASSERT(PinBCK == 13, only_spi_pins1);
STATIC_ASSERT(PinDIN == 11, only_spi_pins2);
STATIC_ASSERT(PinWS == 10, only_spi_pins3);

static void
spi_start(void)
{
	/* configure SPI controller as:
	 * - master, MSB
	 * - full speed (10MHz)
	 * - sample on leading rising edge
	 * - no interrupts
	 */
	SPCR = _BV(SPE) | _BV(MSTR);
	SPSR = _BV(SPI2X);
}

static inline byte spi_done(void) { return SPSR & _BV(SPIF); }
static inline byte
spi_xfer(byte data)
{ 
	SPDR = data;
	while (!spi_done())
		;
	return SPDR;
}

static void
i2s_send_sample(int16_t sample, byte chan)
{
	pin_write(PinWS, chan);
	spi_xfer(sample >> 8);
	spi_xfer(sample);
}

static void
i2s_start(void)
{
	pin_mode(PinBCK, OUTPUT);
	pin_mode(PinWS, OUTPUT);
	pin_mode(PinDIN, OUTPUT);

	spi_start();
}
