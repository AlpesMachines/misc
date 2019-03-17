enum {
	MidiBufMax = 4,
	MidiConsoleMaxOct = 127 / 12,
	MidiConsoleVelocity = 127,
};

typedef struct MidiParser MidiParser;
typedef struct MidiConsole MidiConsole;
typedef void (*midiparser_callback_fn_t)(byte *buf, byte len);
typedef byte (*midikeyboard_callback_fn_t)(byte chan, char key);

struct MidiConsole {
	byte enabled;
	byte oct, chan;
	byte hex;
};

struct MidiParser {
	byte ptr;
	byte buf[MidiBufMax];
	MidiConsole con;
	byte con_selects_chan;
};


/* TODO: pass buf, len to the function */
static byte
midi_len(byte type)
{
	if (type  < 0x80)
		return 1;
	switch (type & 0xf0) {
		case 0x80: /* Note Off: note, velocity  */
		case 0x90: /* Note On: note, velocity */
		case 0xa0: /* Aftertouch: key, velocity */
		case 0xb0: /* Control Change: control, value; cc 120..127 have special meaning */
		case 0xe0: /* Pitch bend: low bits, high bits => 14bit */
			return 3;
		case 0xc0: /* Program change: program */
		case 0xd0: /* Channel pressure: value */
			return 2;
		case 0xf0:
			switch (type) {
				case 0xf0: /* sysex */
					/* HINT: if msg[last-1] != 0xf7, return len+1 */
					return 1;
				case 0xf4: /* undef */
				case 0xf5: /* undef */
				case 0xf6: /* undef */
					return 1;
				case 0xf7: /* end of sysex */
					return 1;
				case 0xf1: /* time quarter */
				case 0xf3: /* song select */
					return 2;
				case 0xf2: /* song position */
					return 3;
				case 0xf8: /* timing clock */
				case 0xf9: /* undef */
				case 0xfa: /* start */
				case 0xfb: /* continue */
				case 0xfc: /* stop */
				case 0xfd: /* undef */
				case 0xfe: /* alive */
				case 0xff: /* reset */
					return 1;
			}
		default:
			return 1;
	}
}

static const PROGMEM char midikeys[] = "awsedftgyhujkolp;'";
static const PROGMEM char hexrow1[] = "1234567890-=";
static const PROGMEM char hexrow2[] = "qwertyuiop[]";
static const PROGMEM char hexrow3[] = "asdfghjkl;'\\";
static const PROGMEM char hexrow4[] = "<zxcvbnm,./";


static void
midikeyboard_trig(MidiParser *mp, byte note, midiparser_callback_fn_t callback)
{
	MidiConsole *con = &mp->con;

	/* noteon */
	mp->buf[0] = 0x90 | con->chan;
	mp->buf[1] = note;
	mp->buf[2] = MidiConsoleVelocity;
	callback(mp->buf, 3);

	/* noteoff */
	mp->buf[0] = 0x80 | con->chan;
	mp->buf[1] = note;
	mp->buf[2] = 0;
	callback(mp->buf, 3);

	printf("%d\n", note);
}

static void
printonoff(const char *s, byte val)
{
	printf("%s: %s\n", s, val ? "on" : "off");
}

#define Ctrl(x) ((x) - '@')

static void
midikeyboard_run(MidiParser *mp, byte in, midiparser_callback_fn_t callback, midikeyboard_callback_fn_t userfn)
{
	MidiConsole *con = &mp->con;

	if (con->hex) {
		const char *ptr;
		ptr = strchr_P(hexrow1, in);
		if (ptr != 0)
			return midikeyboard_trig(mp, (ptr - hexrow1) * 7, callback);
		ptr = strchr_P(hexrow2, in);
		if (ptr != 0)
			return midikeyboard_trig(mp, 3 + (ptr - hexrow2) * 7, callback);
		ptr = strchr_P(hexrow3, in);
		if (ptr != 0)
			return midikeyboard_trig(mp, 3 + 3 + (ptr - hexrow3) * 7, callback);
		ptr = strchr_P(hexrow4, in);
		if (ptr != 0)
			return midikeyboard_trig(mp, 3 + 3 + 3 - 7 + (ptr - hexrow4) * 7, callback);
	} else {
		const char *ptr = strchr_P(midikeys, in);
		if (ptr != 0) {
			byte note = ptr - midikeys + con->oct * 12;
			return midikeyboard_trig(mp, note, callback);
		}
	}

	switch (in) {
		case '0' ... '9':
			con->chan = (in - '0' + 9) % 10;
			printf("ch%d\n", con->chan);
			break;
		case 'z':
			if (con->oct > 0)
				con->oct--;
			break;
		case 'x':
			if (con->oct < MidiConsoleMaxOct - 1)
				con->oct++;
			break;
		case Ctrl('H'):
			con->hex = !con->hex;
			printonoff("hex", con->hex);
			break;
		default:
			if (userfn(con->chan, in) == 0) {
				printf("?\n");
			}
			break;
	}
}

static void
midiparser_init(MidiParser *mp)
{
	mp->ptr = 0;
	mp->buf[0] = 0;
	mp->con.oct = 3;
	mp->con.enabled = 1;
	mp->con.chan = 0;
	mp->con.hex = 0;
}

static void
midiparser_run(MidiParser *mp, byte in, midiparser_callback_fn_t callback, midikeyboard_callback_fn_t userfn)
{
	/* realtime message? */
	if (in >= 0xf8) {
		callback(&in, 1);
		return;
	}

	/* first byte of message? */
	if (mp->ptr == 0) {
		if (in < 0x80) {
			if (mp->con.enabled) {
				midikeyboard_run(mp, in, callback, userfn);
				return;
			} else {
				/* handle running status */
				byte rs = mp->buf[0];
				if (rs < 0x80 || rs >= 0xf0)
					return;
				mp->ptr++;
			}
		}
	}

	/* save new byte */
	mp->buf[mp->ptr++] = in;

	/* message completed? */
	if (mp->ptr >= midi_len(mp->buf[0])) {
		if (mp->con_selects_chan) {
			byte cmd = mp->buf[0] & 0xf0;
			if (cmd != 0xf0) {
				cmd |= mp->con.chan;
				mp->buf[0] = cmd;
			}
		}
		callback(mp->buf, mp->ptr);
		mp->ptr = 0;
		return;
	}
	/* message too long? */
	if (mp->ptr >= MidiBufMax)
		mp->ptr = 0;
}
