#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

int
main(void)
{
	uint8_t buf[32];	

	for (;;) {
		int ret = read(0, buf, sizeof(buf));
		if (ret == 0)
			exit(0);
		if (ret < 0) {
			perror("read");
			exit(1);
		}
		struct timeval tv;
		gettimeofday(&tv, 0);
		printf("%ld.%06ld", tv.tv_sec, tv.tv_usec);
		for (int i = 0; i < ret; i++) {
			printf(" %02x", buf[i]);
		}
		printf("\n");
	}
}
