/* channel allocation:
 *
 * 1-2	poly
 * 3-10	mono (8-9 choke each other, 10 is usually a metronome)
 * 11	drum (plays 3-9 on white keys)
 * 12	switch to alt melody
 * 13	swap banks
 *
 * channels 9 and 10 choke each other
 *
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/power.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "defs.h"
#include "utils.h"
#include "buttons.h"
#include "timer.h"
#include "serial.h"
#include "serbuf_rx.h"
#include "print.h"
#include "pins.h"
#include "pin-config.h"
#include "rand.h"
#include "midi.h"
#ifdef HAS_KNOBS
#include "adc.h"
#endif
#include "op-mutable.h"
#define F_TICKS 15625
#include "ticks.h"
#include "audiobuf.h"
#include "tables.h"
#include "synth.h"
#ifdef HAS_FILTER
#include "dac_i2s_spi.h"
#endif
#include "patch.h"

/*
 * Forward declaration
 */

static byte midi_playing;
static void seq_midi_tick(void);
static void seq_set_mode(byte mode);

/*
 * Saleae debug
 */

#ifdef SALEAE_DEBUG
#define debug_pin_on(pin) pin_write(pin, 1)
#define debug_pin_off(pin) pin_write(pin, 0)
#else
#define debug_pin_on(pin)
#define debug_pin_off(pin)
#endif

/*
 * System
 */

volatile uint16_t rand_state = 0x200;
uint16_t frame_count = 0;


/*
 * Audio
 */

#define BPM_TO_TICKS(bpm) (F_CPU/256*15/((unsigned long)(bpm)*4*6))

static uint16_t tick_counter;
static uint16_t tick_duration = BPM_TO_TICKS(120);

#if TimerClock == 0
ISR(TIMER0_COMPA_vect)
#elif TimerClock == 1
ISR(TIMER1_COMPA_vect)
#elif TimerClock == 2
ISR(TIMER2_COMPA_vect)
#endif
{
	debug_pin_on(PinSaleaIsr);
#if 1
	if (audiobuf_readable(&audiobuf) == 0) {
		timer_output(TimerAudio, TIMER_PIN_A, 0x80);
		timer_output(TimerAudio, TIMER_PIN_B, 0x80);
		return;
	}
#endif
#ifdef HAS_FILTER
#ifndef BENCHMARK
	if (filterbuf_readable()) {
		uint16_t filter = filterbuf_read() - 0x8000;
		i2s_send_sample(0x8000, 0);
		i2s_send_sample(filter, 1);
	}
#endif
#endif
	struct outs outs = audiobuf_readnow(&audiobuf);
	timer_output(TimerAudio, TIMER_PIN_A, outs.b);
	timer_output(TimerAudio, TIMER_PIN_B, outs.a);
	ticks++;
#ifdef INTERNAL_SEQ
	if (midi_playing) {
		if (tick_counter == 0) {
			seq_midi_tick();
			tick_counter = tick_duration;
		}
		tick_counter--;
	}
#endif
	debug_pin_off(PinSaleaIsr);
}


static void
audio_start(void)
{
	/* 16MHz / 256 =~ 4 * 15KHz */
	timer_set_mode(TimerAudio, TIMER_FAST_PWM, PRESCALER_1X);
	timer_enable_out(TimerAudio, TIMER_PIN_A, TIMER_PIN_PWM_NONINV);
	timer_enable_out(TimerAudio, TIMER_PIN_B, TIMER_PIN_PWM_NONINV);
	timer_output(TimerAudio, TIMER_PIN_A, 0x80);
	timer_output(TimerAudio, TIMER_PIN_B, 0x80);

	/* 16MHz / 8 / 128 =~ 15KHz */
	timer_set_mode(TimerClock, TIMER_CTC, PRESCALER_8X);
	timer_output(TimerClock, TIMER_PIN_A, 127);
	timer_enable_irq(TimerClock, TIMER_IRQ_A);
}

/*
 * Instrs
 */

enum {
	Ninstruments = 11,
	DrumInstr = Ninstruments - 1,
	MaxNotes = 64,
	NoteMask = MaxNotes - 1,
};

typedef struct Instr Instr;
struct Instr {
	Patch patch;
	byte group;
	byte last_note;
	byte pattern[MaxNotes];
	byte mute, bank;
};

static Instr instrs[Ninstruments];
static byte cur_ins;

static byte
note_to_inst(byte i)
{
	return (i + 7) % 12;
}

static Instr *
get_cur_instr(void)
{
	byte ins = cur_ins;
	return &instrs[ins];
}


static Instr *
get_cur_instr_or_drum(void)
{
	byte ins = cur_ins;
	if (ins == DrumInstr)
		ins = note_to_inst(instrs[ins].last_note);
	return &instrs[ins];
}

static Instr *
get_note_instr_or_drum(byte note)
{
	byte ins = cur_ins;
	if (ins == DrumInstr)
		ins = note_to_inst(note);
	return &instrs[ins];
}

static void
instr_init(void)
{
	Patch *empty = &instrs[0].patch;
	empty_patch(empty);
	patchmem_init(empty);

	for (byte i = 0; i < DrumInstr; i++) {
		Instr *instr = &instrs[i];
		load_patch(&instr->patch, i);
		byte group = 0;
		if (i >= 8 && i <= 9) {
			group = 8;
		} else if (i >= 2) {
			group = i;
		} else {
			group = 0;
		}
		instr->group = group;
		instr->last_note = rand_next() % 40 + 10;
	}
	/* make a metronome */
	for (byte i = 0; i < MaxNotes; i += 4)
		instrs[9].pattern[i] = i % 16 == 0 ? 50 : 40;
	metro_patch(&instrs[9].patch);

	cur_ins = 0;
}

static void
instr_rand(void)
{
	Instr *instr = get_cur_instr_or_drum();
	rand_patch(&instr->patch);
}

static void
instr_set_param(int8_t p, byte val)
{
	Patch *patch = &get_cur_instr_or_drum()->patch;
	printf("p%d=%d\n", p, val);
	if (p == 0)
		patch->level = ((uint16_t) val * 5) >> 2;
	else if (p == 1)
		patch->crunchiness = val * 2;
	else if (p == 2)
		patch->amp_env_time = val * 2;
	else if (p == 3) {
		patch->mod_env_time = val * 2;
#if 0
		uint16_t x = (uint16_t) val << 9;
		i2s_send_sample(x - 0x8000, 1);
		i2s_send_sample(x - 0x8000, 0);
#endif
	}
	else if (p == 4)
		patch->pch_mod_amount = val * 2 - 128;
	else if (p == 5)
		patch->fil_mod_amount = val * 2 - 128;
	else if (p == 6)
		patch->fil_env_amount = val * 2 - 128;
	else if (p == 7)
		patch->cutoff = val * 2;
	else if (p < 0 && val > 0) {
		if (p == -8)
			patch->wav_type = (patch->wav_type + 1) % ARRAY_SIZE(wav_tables);
		else if (p == -7)
			patch->mod_env_type = (patch->mod_env_type + 1) % ARRAY_SIZE(env_tables);
		else if (p == -6)
			patch->amp_env_type = (patch->amp_env_type + 1) % ARRAY_SIZE(env_tables);
		else if (p == -5)
			patch->mod_loop_on ^= 1;
		print_patch(patch);
	}
#if 0
	else if (p == 2)
		patch->env_type = clamp(val, 0, ARRAY_SIZE(env_table) - 1);
	else if (p == 3)
		patch->wav_type = clamp(val, 0, ARRAY_SIZE(wav_table) - 1);
	else if (p == 4)
		patch->volume = val / 8 - 8;
	else if (p == 5)
		seq_set_mode(val);
#endif
}

static void
instr_play_on(byte ins, byte note)
{
	if (ins >= Ninstruments)
		return;

	Instr *instr = &instrs[ins];

	if (ins == DrumInstr) {
		ins = note_to_inst(note);
		instr = &instrs[ins];
		note = instr->last_note;
	}
	synth_play(&instr->patch, instr->group, note);
	instr->last_note = note;
}

static void
instr_play(byte note)
{
	instr_play_on(cur_ins, note);
}

static void
set_instr(byte i)
{
	if (i >= Ninstruments)
		return;
	cur_ins = i;
}

static void
unmute_all(void)
{
	for (byte i = 0; i < Ninstruments; i++)
		instrs[i].mute = 0;
}

/*
 * patterns
 */

static byte seq_steps, seq_mask;
static byte seq_single_loop = 0;

static void
seq_set_len(byte len)
{
	seq_steps = len;
	seq_mask = len - 1;
}

static void
seq_set_mode(byte mode)
{
	switch (mode) {
		case 0:
		case 1:
		case 2:
		case 3:
			seq_set_len(1 << (mode + 4));
			break;
		case 4:
			seq_set_len(128);
			seq_single_loop = 1;
			break;
	}
}

static void
swap_patterns(Instr *instr1, Instr *instr2)
{
	byte *pat1 = instr1->pattern;
	byte *pat2 = instr2->pattern;
	byte b1 = instr1->bank;
	byte b2 = instr2->bank;

	byte n = seq_steps;
	while (n--) {
		byte x = pat1[b1];
		pat1[b1] = pat2[b2];
		pat2[b2] = x;
		b1 = (b1 + 1) & NoteMask;
		b2 = (b2 + 1) & NoteMask;
	}
}

static void
clear_pattern(Instr *instr)
{
	byte *pat = instr->pattern;
	byte b = instr->bank;

	byte n = seq_steps;
	while (n--) {
		pat[b] = 0;
		b = (b + 1) & NoteMask;
	}
}

static byte start_ins;

static byte
seq_get_note(byte ins, byte step, byte bar)
{
	Instr *instr = &instrs[ins];
	byte off = (instr->bank + step) & NoteMask;
	return instr->pattern[off];
}

/*
 * Tiny loop sequencer
 */
static byte record_on;
static byte midi_tick, midi_step, midi_bar;
static byte midi_last_tick;
static byte eat_next;

static void
seq_midi_start(void)
{
	midi_playing = 1;
	midi_tick = 5;
	midi_last_tick = 0;
	midi_step = -1;
	midi_bar = -1;
	start_ins = cur_ins;
	tick_counter = 0;
}

static void
seq_midi_stop(void)
{
	midi_playing = 0;
	//record_on = 0;
}

static void
seq_midi_tick(void)
{
	if (!midi_playing)
		return;
	midi_tick++;
	if (midi_tick == 6)
		pin_write(PinTrigOut, 1);
	if (midi_tick == 1)
		pin_write(PinTrigOut, 0);
	if (midi_tick >= 6) {
		midi_tick = 0;
		midi_step++;
		midi_step &= seq_mask;
		if (midi_step == 0) {
			midi_bar++;
			if (midi_bar > 0 && seq_single_loop) {
				seq_midi_stop();
			}
		}

		Instr *cur_instr = get_cur_instr_or_drum();
		for (byte i = 0; i < Ninstruments - 1; i++) {
			Instr *instr = &instrs[i];
			byte note = seq_get_note(i, midi_step, midi_bar);
			if (note) {
				if (instr == cur_instr && eat_next)
					eat_next = 0;
				else if (!instr->mute)
					instr_play_on(i, note);
			}
		}
	}
}

static void
seq_figure_rec(void)
{
	if (record_on) {
		Instr *instr = get_cur_instr();
		clear_pattern(instr);
		eat_next = 0;
	}
}

static void
seq_midi_note(byte note)
{
	if (!midi_playing)
		return;
	if (!record_on)
		return;

	Instr *instr = get_note_instr_or_drum(note);
	byte rec_step = midi_step & seq_mask;
	if (midi_tick >= 3) {
		rec_step = (midi_step + 1) & seq_mask;
		eat_next = 1;

	}
	if (cur_ins == DrumInstr) {
		note = instr->last_note;
	}

	instr->pattern[(instr->bank + rec_step) & NoteMask] = note;
}


static void
try_swap_pats(uint16_t imask)
{
	byte ilist[2];
	byte n;

	n = 0;
	for (byte i = 0; i < 10; i++) {
		if (imask & (1 << i)) {
			if (n >= 2)
				return;
			printf("ilist%d: %d\n", n, i);
			ilist[n++] = i;
		}
	}
	if (n < 2)
		return;
	printf("swapping %d and %d\n", ilist[0], ilist[1]);
	swap_patterns(&instrs[ilist[0]], &instrs[ilist[1]]);
}


/*
 * Serial: MIDI
 */

static MidiParser midi_parser;
static uint16_t sel_chan;

static void
midi_msg_handler(byte *buf, byte len)
{
	byte cmd = buf[0] & 0xf0;
	byte chan = buf[0] & 0x0f;
	byte par = buf[1];
	byte val = buf[2];

	if (cmd == 0xf0) {
		if (buf[0] == 0xfa)
			seq_midi_start();
		if (buf[0] == 0xf8)
			seq_midi_tick();
		if (buf[0] == 0xfc)
			seq_midi_stop();
		return;
	}
	/* convert note-on with 0 velocity to note-off */
	if (cmd == 0x90 && val == 0)
		cmd = 0x80;

	if (cmd != 0x90 && cmd != 0x80 && cmd != 0xb0 && cmd != 0xe0)
		return;
	if (cmd == 0x90 && chan == 11) {
		/* mute */
		byte ins = note_to_inst(par);
		if (ins < Ninstruments)
			instrs[ins].mute ^= 1;
	}
	if (cmd == 0x90 && chan == 12) {
		/* alt */
		byte ins = note_to_inst(par);
		if (ins < Ninstruments) {
			instrs[ins].bank += seq_steps;
			instrs[ins].bank &= NoteMask;
		}
	}
	if (cmd == 0x90 && chan == 13) {
		/* swap on */
		byte ins = note_to_inst(par);
		sel_chan |= 1 << ins;
		try_swap_pats(sel_chan);
	}
	if (cmd == 0x80 && chan == 13) {
		/* swap off */
		byte ins = note_to_inst(par);
		sel_chan &= ~(1 << ins);
	}
	if (cmd == 0x90 && chan == 14) {
		byte mode = note_to_inst(par);
		seq_set_mode(mode);
	}
	if (chan >= 11) {
		if (cmd == 0xb0) {
			if (par == 0x3d) {
				/* unmute all */
				if (val > 0)
					unmute_all();
			}
			if (par == 0x3e) {
				/* random sound */
				if (val > 0)
					instr_rand();
			}
			if (par == 0x3f) {
				/* set record */
				byte new = val != 0;
				if (record_on != new) {
					record_on = new;
					seq_figure_rec();
				}
			}
			if (par >= 0x40)
				instr_set_param(par - 0x40, val);
		}
	}
	if (chan >= 11)
		return;

	set_instr(chan);

	if (cmd == 0x90 && val > 0) {
		instr_play(par);
		seq_midi_note(par);
	}
	if (cmd == 0xb0) {
		if (par == 0x21) {
			instr_set_param(0, val);
		} else {
			instr_set_param(par - 9, val);
		}
	}
	if (cmd == 0xe0) {
		instr_set_param(1, val);
	}
}

/*
 * Serial: Console
 */

static byte param;
static byte in_perfmode;

static void
show_mutes(void)
{
	for (byte i = 0; i < Ninstruments - 1; i++) {
		Instr *instr = &instrs[i];
		putchar(instr->mute ? '_' : '0' + i);
	}
	putchar('\n');
}

static byte
handle_userkey(byte chan, char c)
{
	set_instr(chan);

	Instr *instr = get_cur_instr_or_drum();
	byte instr_no = instr - instrs;
	switch (c) {
		case ',':
			param--;
			printf("param=%d\n", param);
			break;
		case '.':
			param++;
			printf("param=%d\n", param);
			break;
		case Ctrl('O'):
			instr->patch.out = (instr->patch.out + 1) % N_OUTS;
			print_patch(&instr->patch);
			break;
		case Ctrl('P'):
			printf("perf\n");
			in_perfmode = 1;
			break;
		case Ctrl('B'):
			//rand_state = frame_count;
			instr_rand();
			break;
		case Ctrl('L'):
			load_patch(&instr->patch, instr_no);
			printf("loaded\n");
			print_patch(&instr->patch);
			break;
		case Ctrl('W'):
			save_patch(&instr->patch, instr_no);
			printf("saved\n");
			break;
		case Ctrl('I'):
			empty_patch(&instr->patch);
			print_patch(&instr->patch);
			break;
		case Ctrl('M'):
			instr->mute ^= 1;
			break;
		case Ctrl('R'):
			record_on ^= 1;
			printf("rec=%d\n", record_on);
			seq_figure_rec();
			break;
		case ' ':
			if (midi_playing) {
				seq_midi_stop();
			} else {
				seq_midi_start();
			}
			break;
		default:
			return 0;
	}
	return 1;
}

static void
handle_perfmode(char c)
{
	switch (c) {
	case Ctrl('P'):
		printf("perf off\n");
		in_perfmode = 0;
		break;
	case '0' ... '9': {
		byte chan = (c - '0' + 9) % 10;
		Instr *instr = &instrs[chan];
		instr->mute ^= 1;
		show_mutes();
		break;
		}
	default:
		handle_userkey(0, c);
		break;
	}
}


RX_INT()
{
	debug_pin_on(PinSaleaSerial);
	serbuf_rx_read();
	debug_pin_off(PinSaleaSerial);
}

/*
 * Main
 */

int
main(void) 
{
	wdt_disable();
	power_adc_disable();
	power_usart0_enable();
	power_twi_disable();
	power_timer0_enable();
	power_timer1_enable();
	power_timer2_enable();

#ifdef CONSOLE
	serial_start(115200);
	serial_stdio_init();
	printf("hello from zebrak, tick duration=%d\n", tick_duration);
#else
	serial_start(31250);
#endif
	midiparser_init(&midi_parser);
	midi_parser.con_selects_chan = 1;
	serial_enable_rxint();

	pin_mode(PinLed, OUTPUT);
	pin_mode(timer_pin_no(TimerAudio, TIMER_PIN_A), OUTPUT);
	pin_mode(timer_pin_no(TimerAudio, TIMER_PIN_B), OUTPUT);
	pin_mode(PinButton, INPUT_PULLUP);

#ifdef SALEAE_DEBUG
	pin_mode(PinSaleaIsr, OUTPUT);
	debug_pin_off(PinSaleaIsr);
	/* connected to OCR0A timer */
	pin_mode(PinSaleaIsrEdge, OUTPUT);
	debug_pin_off(PinSaleaIsrEdge);
	pin_mode(PinSaleaUser, OUTPUT);
	debug_pin_off(PinSaleaUser);
	pin_mode(PinSaleaSerial, OUTPUT);
	debug_pin_off(PinSaleaSerial);
	pin_mode(PinSaleaError, OUTPUT);
	debug_pin_off(PinSaleaError);
#endif
	pin_mode(PinTrigOut, OUTPUT);
	pin_write(PinTrigOut, 0);

	synth_init();
	instr_init();
	seq_set_mode(2);

	audio_start();
#ifdef HAS_FILTER
	i2s_start();
#endif
#ifdef HAS_KNOBS
	power_adc_enable();
	adc_init();
	pin_mode(PinPot1, INPUT);
	pin_mode(PinPot2, INPUT);
#endif
	render_silence();

	sei();

	byte hist = 0;

	uint16_t pot_prev[6];

	for (;;) {
#ifdef BENCHMARK
		uint16_t start = get_ticks();
		for (uint16_t i = 0; i < F_TICKS / 32; i++) {
			render_block();
#ifdef HAS_FILTER
			i2s_send_sample(0x1234, 1);
			i2s_send_sample(0x5678, 0);
#endif
		}
		uint16_t d_ticks = get_ticks() - start;
		printf("ticks=%d, ms=%d\n", d_ticks, ticks_to_ms(d_ticks));
#endif
		debug_pin_on(PinSaleaUser);
		render_blocks();

		while (serbuf_rx_readable(&serbuf_rx)) {
			byte b = serbuf_rx_readnow(&serbuf_rx);
			if (in_perfmode)
				handle_perfmode(b);
			else
				midiparser_run(&midi_parser, b, midi_msg_handler, handle_userkey);
		}
		scan_but(&hist, pin_read(PinButton));
#if 0
		if (hist == 0x80) {
			printf("button\n");
			if (midi_playing) {
				record_on ^= 1;
				seq_figure_rec();
			} else {
				rand_state += frame_count;
				instr_rand();
			}
		}
		if (midi_playing) {
			pin_write(PinLed, record_on);
		} else {
			if (hist == 0)
				pin_write(PinLed, 0);
			else
				pin_write(PinLed, 1);
		}
#endif

#ifdef HAS_KNOBS
		for (byte i = 3; i <= 4; i++) {
			byte out;
			if (pot_scan(i, &pot_prev[i], &out)) {
				byte p = param + i - 3;
				instr_set_param(p, out);
			}
		}
#endif

		debug_pin_off(PinSaleaUser);
		frame_count++;
	}

	return 0;
}
