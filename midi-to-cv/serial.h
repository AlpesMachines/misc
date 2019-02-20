static void
serial_start(uint32_t baud_rate)
{
	uint16_t prescaler;

	/* compute prescaler */
	prescaler = (F_CPU / 4L / baud_rate - 1) / 2;

	/* set prescaler */
	UBRRH = prescaler >> 8;
	UBRRL = prescaler;

	/* set mode: 8bit, no parity, 1 stop bit */
	UCSRC = _BV(UCSZ1) | _BV(UCSZ0);

	/* set turbo */
	UCSRA |= _BV(U2X);

	/* enable RX, TX */
	UCSRB = _BV(TXEN) | _BV(RXEN);
}

static inline void serial_enable_txint(void) { UCSRB |= _BV(UDRIE); }
static inline void serial_disable_txint(void) { UCSRB &= ~_BV(UDRIE); }
static inline void serial_enable_rxint(void) { UCSRB |= _BV(RXCIE); }
static inline void serial_disable_rxint(void) { UCSRB &= ~_BV(RXCIE); }

static inline byte serial_writable(void) { return UCSRA & _BV(UDRE); }
static inline byte serial_readable(void) { return UCSRA & _BV(RXC); }
static inline void serial_overwrite(byte c) { UDR = c; }
static inline byte serial_readnow(void) { return UDR; }

static inline void
serial_write(byte c)
{
	while (!serial_writable())
		;
	serial_overwrite(c);
}

static inline byte
serial_read(void)
{
	while (!serial_readable())
		;
	return serial_readnow();
}

#define RX_INT() ISR(USART_RX_vect)
#define TX_INT() ISR(USART_UDRE_vect)
