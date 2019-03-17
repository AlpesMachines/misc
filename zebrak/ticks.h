volatile uint16_t ticks;

#define get_ticks() ({ uint16_t _ret; cli(); _ret = ticks; sei(); _ret; })
#define zero_ticks() ({ cli(); ticks = 0; sei(); })

static uint16_t ticks_to_ms(uint16_t ticks)
{
	return (uint32_t) ticks * 1000 / F_TICKS;
}
