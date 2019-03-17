enum {
	DacLeft = 1,
	DacRight = 0,
};

static inline void
i2s_send_bit(byte bit, byte chan)
{
	pin_write(PinWS, chan);
	pin_write(PinDIN, bit);
	pin_write(PinBCK, 1);
	pin_write(PinBCK, 0);
	//printf("%d", bit);
}

#define SEND_BIT(i) ({ if (sample & (1 << i)) { pin_write(PinDIN, 1); } else { pin_write(PinDIN, 0); } pin_write(PinBCK, 1); pin_write(PinBCK, 0); })
static void
i2s_send_sample(int16_t sample, byte chan)
{
#if 0
	pin_write(PinWS, chan);
	for (char bit = 15; bit >= 0; bit--) {
		i2s_send_bit((sample >> bit) & 1, chan);
	}
#else
	pin_write(PinWS, chan);
	SEND_BIT(15);
	SEND_BIT(14);
	SEND_BIT(13);
	SEND_BIT(12);
	SEND_BIT(11);
	SEND_BIT(10);
	SEND_BIT(9);
	SEND_BIT(8);
	SEND_BIT(7);
	SEND_BIT(6);
	SEND_BIT(5);
	SEND_BIT(4);
	SEND_BIT(3);
	SEND_BIT(2);
	SEND_BIT(1);
	SEND_BIT(0);
#endif
}

static void
i2s_send_mono(int16_t sample)
{
	i2s_send_sample(sample, 0);
	i2s_send_sample(sample, 1);
}

static void
i2s_start(void)
{
	pin_mode(PinBCK, OUTPUT);
	pin_mode(PinWS, OUTPUT);
	pin_mode(PinDIN, OUTPUT);
}
