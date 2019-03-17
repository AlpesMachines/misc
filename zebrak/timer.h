enum {
	TIMER_0,
	TIMER_1,
	TIMER_2,
};

enum {
	TIMER_PIN_A,
	TIMER_PIN_B,
};

enum {
	TIMER_IRQ_OVF = 1,
	TIMER_IRQ_A = 2,
	TIMER_IRQ_B = 4,
};

#define TIMER_REGDEF(name,t0,t1,t2) \
	ALWAYS_INLINE static inline volatile byte *get_ ## name(byte timer) { \
		switch (timer) { case 0: return t0; case 1: return t1; case 2: return t2; } \
		return 0; \
	}

TIMER_REGDEF(tccra, &TCCR0A, 0, &TCCR2A)
TIMER_REGDEF(tccrb, &TCCR0B, 0, &TCCR2B)
TIMER_REGDEF(timsk, &TIMSK0, 0, &TIMSK2)
TIMER_REGDEF(tifr, &TIFR0, 0, &TIFR2)
TIMER_REGDEF(tcnt, &TCNT0, 0, &TCNT2)
TIMER_REGDEF(ocra, &OCR0A, 0, &OCR2A)
TIMER_REGDEF(ocrb, &OCR0B, 0, &OCR2B)

enum {
	TIMER_NON_PWM = 0,
	TIMER_PHASE_CORRECT_PWM = 1,
	TIMER_CTC = 2,
	TIMER_FAST_PWM = 3,
};

enum {
	TIMER_PIN_DISCONN = 0,
	TIMER_PIN_TOGGLE = 1,
	TIMER_PIN_CLEAR = 2,
	TIMER_PIN_PWM_NONINV = 2,
	TIMER_PIN_SET = 3,
	TIMER_PIN_PWM_INV = 3,
};

enum {
	PRESCALER_NOCLOCK,
	PRESCALER_1X,
	PRESCALER_8X,
	PRESCALER_64X,
	PRESCALER_256X,
	PRESCALER_1024X,
};

ALWAYS_INLINE static inline void
timer_set_mode(byte timer, byte mode, byte prescaler)
{
	*get_tccra(timer) = mode;
	*get_tccrb(timer) = prescaler;
	*get_timsk(timer) = 0;
	*get_tcnt(timer) = 0;
}

ALWAYS_INLINE static inline void
timer_enable_out(byte timer, byte pin, byte mode)
{
	if (pin == TIMER_PIN_A)
		*get_tccra(timer) |= mode << 6;
	else if (pin == TIMER_PIN_B)
		*get_tccra(timer) |= mode << 4;
}

ALWAYS_INLINE static inline void
timer_enable_irq(byte timer, byte what)
{
	*get_timsk(timer) |= what;
}

ALWAYS_INLINE static inline void
timer_output(byte timer, byte pin, byte value)
{
	if (pin == TIMER_PIN_A)
		*get_ocra(timer) = value;
	else if (pin == TIMER_PIN_B)
		*get_ocrb(timer) = value;
}

ALWAYS_INLINE static inline byte
timer_pin_no(byte timer, byte pin)
{
	switch (timer) {
		case 0: return pin == TIMER_PIN_A ? 6 : 5;
		case 1: return pin == TIMER_PIN_A ? 9 : 10;
		case 2: return pin == TIMER_PIN_A ? 11 : 3;
	}
	return 0;
}
