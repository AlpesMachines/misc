static inline void
bit_on(byte *buf, byte x)
{
	buf[x / 8] |= 1 << (x % 8);
}

static inline void
bit_off(byte *buf, byte x)
{
	buf[x / 8] &= ~(1 << (x % 8));
}

static inline void
bit_toggle(byte *buf, byte x)
{
	buf[x / 8] ^= 1 << (x % 8);
}

static inline void
bit_set(byte *buf, byte x, byte on)
{
	if (on)
		bit_on(buf, x);
	else
		bit_off(buf, x);
}

static inline byte
bit_isset(byte *buf, byte x)
{
	return buf[x / 8] & (1 << (x % 8));
}

#define DEF_BITMAP(name, len) byte name[((len) + 7) / 8]

