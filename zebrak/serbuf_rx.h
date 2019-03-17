#define PREFIX(x) serbuf_rx_ ## x
#define ELEM byte
#define SIZE 32
#define VOLATILE
#include "ring.h"

static struct serbuf_rx_str serbuf_rx;
static byte dropped_rx;

static void
serbuf_rx_read(void)
{
	if (serial_readable()) {
		byte b = serial_readnow();
		if (serbuf_rx_writable(&serbuf_rx) > 0) {
			serbuf_rx_overwrite(&serbuf_rx, b);
		} else {
			if (dropped_rx < 255)
				dropped_rx++;
		}
	}
}
