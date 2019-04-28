#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "defs.h"
#include "utils.h"
#include "pins.h"
#include "serial.h"
#include "print.h"
#include "midi.h"
#include "midi-monovoice.h"
#include "spi.h"
#include "mcp4921.h"

enum {
	PinTrig = 4,
	PinLed = 5,
};

enum {
	PitchBendSemi = 2, /* range: 0...3 to not overflow */
	BaseNote = 12,
	OctaveRange = 5,
	SemiRange = OctaveRange*12,
	DacMaxVal = 4096 - 1,
	//DacMaxVal = 5000,
	DacRangeMult = DacMaxVal / 15,
};


typedef struct State State;

struct State {
	MonoVoice mv;
	byte pitchbend;
	byte last_note;
	Midiparser midi_parser;
};

static State state;

static byte
accepted_chan(byte chan)
{
	return chan < 8;
}

static uint16_t
note_to_dac_val(byte note, byte pitchbend)
{
	if (note < BaseNote)
		note = BaseNote;
	if (note > BaseNote + SemiRange)
		note = BaseNote + SemiRange;

#ifdef USE_FLOAT
	float semi = note - BaseNote;
	semi += ((int)pitchbend - 0x40) * PitchBendSemi / 64.0;
	int16_t dac_val = semi * (DacRange - 1) / (float)SemiRange;
#else
	/* convert note */
	/* this is: dac_val = note / 60.0 * 4095.0; */
	int16_t dac_val = ((uint16_t)(note - BaseNote) * DacRangeMult) >> 2;
	/* convert pitch bend (beware overflow) */
	/* beware overflow */
	if (pitchbend > 0x40) {
		/* dac_val = PitchBendSemi / 60 * 4095.0 * ((pitchbend - 0x40) / 0x40) */
		dac_val += ((uint16_t) (pitchbend - 0x40) * (DacRangeMult * PitchBendSemi)) >> 8;
	} else {
		dac_val -= (((uint16_t) (0x40 - pitchbend) * (DacRangeMult * PitchBendSemi)) >> 8);
	}
#endif
	if (dac_val < 0)
		dac_val = 0;
	if (dac_val >= 4095)
		dac_val = 4095;

	DPRINT("dac_val=%d\n", dac_val);
	return dac_val;
}

static void
update_volts(void)
{
	MonoVoice *mv = &state.mv;
	if (mv->n_playing > 0) {
		state.last_note = mv->notes[mv->n_playing - 1];
	}

	uint16_t dac_val = note_to_dac_val(state.last_note, state.pitchbend);
	mcp4921_write(dac_val);
	pin_write(PinTrig, mv->n_playing > 0);
	pin_write(PinLed, mv->n_playing == 0);
}

static void
midi_msg_handler(byte *buf, byte len)
{
	byte cmd = buf[0] & 0xf0;
	byte chan = buf[0] & 0x0f;
	byte arg1 = buf[1];
	byte arg2 = buf[2];

	if (!accepted_chan(chan))
		return;

	DHEXDUMP(buf, len);
	if (cmd == 0x90) {
		/* noteon */
		monovoice_add(&state.mv, arg1);
	}
	if (cmd == 0x80) {
		/* noteoff */
		monovoice_del(&state.mv, arg1);
	}
	if (cmd == 0xe0) {
		/* pitchbend, arg1=low bits, arg2=high bits */
		state.pitchbend = arg2;
	}
	update_volts();
}

RX_INT()
{
	if (serial_readable()) {
		byte b = serial_readnow();
		midiparser_run(&state.midi_parser, b, midi_msg_handler);
	}
}

static void
hw_init(void)
{
	pin_mode(PinTrig, OUTPUT);
	pin_mode(PinLed, OUTPUT);
	pin_write(PinLed, 0);
#ifdef CONSOLE
	serial_start(115200);
#else
	serial_start(31250);
#endif
	serial_enable_rxint();
	spi_start();
	mcp4921_init();
	update_volts();
}

static void
state_init(void)
{
	state.mv.n_playing = 0;
	state.pitchbend = 0x40;
	midiparser_init(&state.midi_parser);
}

int
main(void)
{
	state_init();
	hw_init();
	sei();

	DPRINT("hello from midi-to-cv\n");

	for (;;) {
		//pin_toggle(PinLed);
		_delay_ms(500);
	}
}
