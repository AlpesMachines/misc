static inline int
serial_putchar(char c, FILE *stream)
{
#ifdef CONSOLE
	if (c == '\n')
		serial_write('\r');
	serial_write(c);
#endif
	return 0;
}


FILE uart_stdout = FDEV_SETUP_STREAM(serial_putchar, NULL, _FDEV_SETUP_WRITE);

static inline void
serial_stdio_init(void)
{
	stdout = &uart_stdout;
}
