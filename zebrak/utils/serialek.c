#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

void
pexit(const char *s)
{
	perror(s);
	exit(1);
}

struct {
	int bauds, bconst;
} speeds[] = {
	{ 0, B0 },
	{ 50, B50 },
	{ 75, B75 },
	{ 110, B110 },
	{ 134, B134 },
	{ 150, B150 },
	{ 200, B200 },
	{ 300, B300 },
	{ 600, B600 },
	{ 1200, B1200 },
	{ 1800, B1800 },
	{ 2400, B2400 },
	{ 4800, B4800 },
	{ 9600, B9600 },
	{ 19200, B19200 },
	{ 38400, B38400 },
	{ 57600, B57600 },
	{ 115200, B115200 },
	{ 230400, B230400 },
	{ -1, },
};

struct termios orig_termios;

void disable_raw_mode(void)
{
	tcsetattr(0, TCSAFLUSH, &orig_termios);
}
void enable_raw_mode(void)
{
	tcgetattr(0, &orig_termios);
	atexit(disable_raw_mode);
	struct termios raw = orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
	raw.c_iflag &= ~(IXON | ICRNL);
	tcsetattr(0, TCSAFLUSH, &raw);
}

int
setrawserial(int fd, int speed)
{
	struct termios tty;
	memset (&tty, 0, sizeof tty);

	if (tcgetattr (fd, &tty) != 0) {
		perror("tcgetattr");
		return -1;
	}

	cfsetospeed (&tty, speed);
	cfsetispeed (&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;

	tty.c_iflag &= ~(IGNBRK | ICRNL);
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cc[VMIN]  = 1;
	tty.c_cc[VTIME] = 0;

	tty.c_iflag &= ~(IXON | IXOFF | IXANY);

	tty.c_cflag |= (CLOCAL | CREAD);
	tty.c_cflag &= ~(PARENB | PARODD);
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr (fd, TCSANOW, &tty) != 0) {
		perror("tcsetattr");
		return -1;
	}
	return 0;
}

ssize_t
write0(int fd, void *buf, size_t count)
{
	ssize_t done = 0;
	ssize_t ret;

	while (done < count) {
		ret = write(fd, buf + done, count - done);
		if (ret <= 0)
			return ret;
		done += count;
	}
	return done;
}


void
copy(int fd1, int fd2, int drop_cr)
{
	unsigned char buf[128];
	int ret;

	ret = read(fd1, buf, sizeof(buf));
	if (ret < 0)
		pexit("read");
	if (ret == 0)
		exit(0);
	int n = ret;

	if (drop_cr) {
		int j = 0;
		for (int i = 0; i < n; i++) {
			buf[j] = buf[i];
			if (buf[j] != '\r')
				j++;
		}
		n = j;
	}

	ret = write0(fd2, buf, n);
	if (ret < 0)
		pexit("write");
	if (ret == 0)
		exit(0);
}

int
main(int argc, char *argv[])
{
	int fd, ret;
	int bauds, speed;
	const char *device;
	struct pollfd fds[16];
	int i;

	if (argc < 3) {
		fprintf(stderr, "usage: %s <serial_device> <speed> [midi1] [midi2] ...\n", argv[0]);
		exit(1);
	}

	device = argv[1];
	bauds = atoi(argv[2]);
	speed = -1;

	for (i = 0; speeds[i].bauds >= 0; i++) {
		if (speeds[i].bauds == bauds) {
			speed = speeds[i].bconst;
			break;
		}
	}
	if (speed == -1) {
		fprintf(stderr, "unsupported speed %d\n", bauds);
		exit(1);
	}

	/* setup descriptors */
	memset(fds, 0, sizeof(fds));

	/* add stdin */
	fds[0].fd = 0;
	fds[0].events = POLLIN;

	/* open serial */
	fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0)
		pexit(device);
	if (setrawserial(fd, speed) != 0)
		pexit("setrawserial");
	fds[1].fd = fd;
	fds[1].events = POLLIN;

	/* open midis */
	int nfds = 2;
	for (i = 3; i < argc; i++) {
		if (nfds >= ARRAY_SIZE(fds)) {
			fprintf(stderr, "too many devices\n");
			exit(1);
		}
		const char *device = argv[i];
		int fd = open(device, O_RDONLY);
		if (fd < 0)
			pexit(device);
		fds[nfds].fd = fd;
		fds[nfds].events = POLLIN;
		nfds++;
	}

	/* setup stdin */
	enable_raw_mode();

	/* loop */
	for (;;) {
		ret = poll(fds, nfds, -1);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			pexit("poll");
		}
		if (ret == 0)
			continue;

		if (fds[0].revents != 0)
			copy(fds[0].fd, fds[1].fd, 0);
		if (fds[1].revents != 0)
			copy(fds[1].fd, 1, 1);

		for (int i = 2; i < nfds; i++) {
			if (fds[i].revents != 0)
				copy(fds[i].fd, fds[1].fd, 0);
		}
	}
}
