#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define PROGMEM
#include "../minty-tables.h"

int
main(int argc, char *argv[])
{
	int n = 0, t = 'e';
	if (argc < 3) {
		printf("usage\n");
		exit(1);
	}
	t = argv[1][0];
	n = atoi(argv[2]);
	const uint8_t *p;
	int len;
	if (t == 'e') {
		p = env_table[n];
		len = 128;
	} else if (t == 'w') {
		p = wav_table[n];
		len = 256;
	} else {
		exit(1);
	}
	fwrite(p, 1, len, stdout);
	return 0;
}
