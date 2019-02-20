enum {
	PinClk = 15,
	PinDataOut = 14,
	PinDataIn = 13,
};

static void
spi_start(void)
{
	/* setup pins */
	pin_mode(PinClk, OUTPUT);
	pin_write(PinClk, 0);
	pin_mode(PinDataOut, OUTPUT);
	pin_write(PinDataOut, 0);
	pin_mode(PinDataIn, INPUT);

}

static byte
spi_xfer(byte data)
{ 
	/* how it works:
	 * - writing 1 to USITC bit toggles (external) clock pin - it must
	 *   be set to "0" before transfer otherwise the clock output
	 *   will have inverted polarity
	 * - writing 1 to USICLK bit shifts register one bit
	 */
	byte hi = _BV(USIWM0) | _BV(USITC);
	byte lo = _BV(USIWM0) | _BV(USITC) | _BV(USICLK);

	USIDR = data;

#define ONE_BIT() { USICR = hi; USICR = lo; }
	ONE_BIT();
	ONE_BIT();
	ONE_BIT();
	ONE_BIT();
	ONE_BIT();
	ONE_BIT();
	ONE_BIT();
	ONE_BIT();
#undef ONE_BIT

	return USIDR;
}


/* bigbang */
static byte
spi_xfer1(byte x)
{
	byte out = 0;
	for (byte i = 0; i < 8; i++) {
		pin_write(PinDataOut, x & 0x80);
		pin_write(PinClk, 1);
		pin_write(PinClk, 0);
		x <<= 1;
		out = (out << 1) | pin_read(PinDataIn);
	}
	return out;
}
