static void
serial_start(uint32_t baud_rate)
{
	uint16_t prescaler;

	/* compute prescaler */
	prescaler = (F_CPU / 4L / baud_rate - 1) / 2;

	/* set prescaler */
	UBRR0H = prescaler >> 8;
	UBRR0L = prescaler;

	/* set mode: 8bit, no parity, 1 stop bit */
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);

	/* set turbo */
	UCSR0A |= _BV(U2X0);

	/* enable RX, TX */
	UCSR0B = _BV(TXEN0) | _BV(RXEN0);
}

static inline void serial_enable_txint(void) { UCSR0B |= _BV(UDRIE0); }
static inline void serial_disable_txint(void) { UCSR0B &= ~_BV(UDRIE0); }
static inline void serial_enable_rxint(void) { UCSR0B |= _BV(RXCIE0); }
static inline void serial_disable_rxint(void) { UCSR0B &= ~_BV(RXCIE0); }

static inline byte serial_writable(void) { return UCSR0A & _BV(UDRE0); }
static inline byte serial_readable(void) { return UCSR0A & _BV(RXC0); }
static inline void serial_overwrite(byte c) { UDR0 = c; }
static inline byte serial_readnow(void) { return UDR0; }

static inline void
serial_write(byte c)
{
	while (!serial_writable())
		;
	serial_overwrite(c);
}

static inline byte
serial_read(byte c)
{
	while (!serial_readable())
		;
	return serial_readnow();
}

#define RX_INT() ISR(USART_RX_vect)
#define TX_INT() ISR(USART_UDRE_vect)
