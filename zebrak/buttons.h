static inline void
scan_but(byte *hist, byte state)
{
	byte x = *hist;
	x <<= 1;
	if (state)
		x |= 1;
	*hist = x;
}
