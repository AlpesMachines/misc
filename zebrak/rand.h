static inline uint16_t
lfsr16_next(uint16_t x)
{
	uint16_t y = x >> 1;
	if (x & 1)
		y ^= 0xb400;
	return y;
}

static inline uint8_t
lfsr8_next(uint8_t x)
{
	uint8_t y = x >> 1;
	if (x & 1)
		y ^= 0xb8;
	return y;
}

extern volatile uint16_t rand_state;

static inline uint16_t
rand_next(void)
{
	rand_state = lfsr16_next(rand_state);
	return rand_state;
}
