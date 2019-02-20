enum {
	Mcp4921_CS = 6,
};

static void
mcp4921_init(void)
{
	pin_mode(Mcp4921_CS, OUTPUT);
	pin_write(Mcp4921_CS, 1);
}


static void
mcp4921_write(uint16_t x)
{
	/* mask just to be sure */
	x &= 4095u;
	/* SHDN=0, GA=0, A/B=0, BUF=0 */
	x |= 0x3000;
	/* pull down */
	pin_write(Mcp4921_CS, 0);
	/* transfer */
	spi_xfer(x >> 8);
	spi_xfer(x);
	/* pull up*/
	pin_write(Mcp4921_CS, 1);
}
