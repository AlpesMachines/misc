/* you define: */
/*
#define PREFIX(x) audiobuf_ ## x
#define ELEM byte
#define SIZE 256
*/
#define T struct PREFIX(str)
#define P(x) PREFIX(x)
#ifdef VOLATILE
#define V volatile
#else
#define V
#endif

T {
	byte rp, wp;
	ELEM data[SIZE];
};


static inline byte
P(writable)(V T *s)
{
	return (s->rp - s->wp - 1) & (SIZE - 1);
}

static inline byte
P(readable)(V T *s)
{
	return (s->wp - s->rp) & (SIZE - 1);
}

static inline ELEM
P(readnow)(V T *s)
{
	ELEM e = s->data[s->rp++];
	s->rp &= SIZE - 1;
	return e;
}

static inline void
P(overwrite)(V T *s, ELEM e)
{
	s->data[s->wp++] = e;
	s->wp &= SIZE - 1;
}

#undef P
#undef T
#undef V
#undef PREFIX
#undef ELEM
#undef SIZE
#undef VOLATILE
