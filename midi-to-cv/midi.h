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

#define MIDI_BUFMAX 4
typedef struct Midiparser Midiparser;
typedef void (*midiparser_process_fn_t)(byte *buf, byte len);

struct Midiparser {
	byte ptr;
	byte running_status;
	byte buf[MIDI_BUFMAX];
};

static void
midiparser_init(Midiparser *mp)
{
	mp->running_status = 0;
	mp->ptr = 0;
}

static void
midiparser_run(Midiparser *mp, byte in, midiparser_process_fn_t callback)
{
	/* realtime message? */
	if (in >= 0xf8) {
		callback(&in, 1);
		return;
	}

	/* first byte of message? */
	if (mp->ptr == 0) {
		if (in < 0x80) {
			/* handle running status */
			if (mp->running_status < 0x80)
				return;
			mp->buf[mp->ptr++] = mp->running_status;
		} else if (in < 0xf0) {
			/* new running status */
			mp->running_status = in;
		} else {
			mp->running_status = 0;
		}
	}

	/* save new byte */
	mp->buf[mp->ptr++] = in;

	/* message completed? */
	if (mp->ptr >= midi_len(mp->buf[0])) {
		callback(mp->buf, mp->ptr);
		mp->ptr = 0;
		return;
	}
	/* message too long? */
	if (mp->ptr >= MIDI_BUFMAX)
		mp->ptr = 0;
}
