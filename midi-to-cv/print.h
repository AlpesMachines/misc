#include <stdarg.h>

static inline void
serial_putchar(char c)
{
	if (c == '\n')
		serial_write('\r');
	serial_write(c);
}

static char
nibble_to_hex(byte x)
{
	if (x < 10)
		return x + '0';
	else
		return x - 10 + 'a';
}

static void
put_c(char c)
{
	serial_putchar(c);
}

static void
put_s(const char *s)
{
	while (*s)
		put_c(*s++);
}

static void
put_u(unsigned int n)
{
	char buf[8], *p = &buf[0] + sizeof(buf);
	*--p = 0;
	do {
		*--p = n % 10 + '0';
		n /= 10;
	} while (n != 0);
	put_s(p);
}

static void
put_x(unsigned int n)
{
	char buf[8], *p = &buf[0] + sizeof(buf);

	*--p = 0;
	do {
		*--p = nibble_to_hex(n & 15);
		n >>= 4;
	} while (n != 0);
	put_s(p);
}

static void
put_d(int n)
{
	if (n < 0) {
		put_c('-');
		n = -n;
	}
	put_u(n);
}

static void
put_nibble(byte x)
{
	put_c(nibble_to_hex(x));
}

static void
put_x8(uint8_t x)
{
	put_nibble((x >> 4) & 15);
	put_nibble(x & 15);
}

static void
put_formatted(const char *fmt, ...)
{
	va_list ap;
	uint16_t u;
	int16_t d;
	char *s;
	uint8_t c;

	va_start(ap, fmt);
	while ((c = pgm_read_byte_near(fmt++)) != 0) {
		if (c != '%') {
			put_c(c);
			continue;
		}
		switch (pgm_read_byte_near(fmt++)) {
			case 's':
				s = va_arg(ap, char *);
				put_s(s);
				break;
			case 'x':
				u = va_arg(ap, uint16_t);
				put_x(u);
				break;
			case 'u':
				u = va_arg(ap, uint16_t);
				put_u(u);
				break;
			case 'd':
				d = va_arg(ap, int16_t);
				put_d(d);
				break;
			default:
				put_c(c);
				break;
		}
	}
	va_end(ap);
}

static void
put_hexdump(const void *mem, byte len)
{
	const char *bytes = mem;

	for (byte i = 0; i < len; i++) {
		if (i > 0)
			put_c(' ');
		put_x8(bytes[i]);
	}
	put_c('\n');
}

#define PRINT(x, a...) put_formatted(PSTR(x), ##a)
#ifdef CONSOLE
#define DPRINT(x, a...) PRINT(x, ##a)
#define DHEXDUMP(x,len) put_hexdump(x,len)
#else
#define DPRINT(x, a...)
#define DHEXDUMP(x,len)
#endif
