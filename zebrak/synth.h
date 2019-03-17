#define SYNTH_BLOCK_SIZE 32
#define N_CHANS 4
enum {
	FilteredOut = 1,
	NoChannel = 0xff,
};

typedef struct Patch Patch;
typedef struct SynthState SynthState;

struct Patch {
	uint8_t level, crunchiness;
	uint8_t wav_type;
	uint8_t amp_env_time, mod_env_time;
	uint8_t amp_env_type, mod_env_type;
	int8_t pch_mod_amount;
	byte mod_loop_on;
	byte out;
	/* filter */
	uint8_t cutoff;
	int8_t fil_mod_amount;
	int8_t fil_env_amount;
};

struct SynthState {
	byte playing, group;
	byte out;
	Patch *patch;
	/* some things may be moved out of this structure and kept in "patch"
	 * so that real-time modulation is possible
	 */
	uint8_t qnote;
	uint8_t level, crunchiness;
	uint8_t amp_level, amp_level_noise;
	uint16_t amp_env_phase, amp_env_phase_inc;
	uint16_t osc_phase, osc_phase_inc;
	uint16_t mod_env_phase, mod_env_phase_inc;
	uint16_t filter;
	byte mod_loop_on;
	int8_t pch_mod_amount;
	const int8_t *wave_data;
	const uint8_t *amp_env_data;
	const uint8_t *mod_env_data;
};

static volatile struct audiobuf_str audiobuf;
#define FILTER_BUF_SIZE (ARRAY_SIZE(audiobuf.data) / SYNTH_BLOCK_SIZE)
static volatile uint16_t filterbuf[FILTER_BUF_SIZE];
static SynthState state[N_CHANS];
static byte last_chan;
static byte chan_on_out[N_CHANS];

static void
filterbuf_write(uint16_t val)
{
	filterbuf[audiobuf.wp / SYNTH_BLOCK_SIZE] = val;
}

static inline byte
filterbuf_readable(void)
{
	return (audiobuf.rp & (SYNTH_BLOCK_SIZE - 1)) == 0;
}

static inline uint16_t
filterbuf_read(void)
{
	return filterbuf[audiobuf.rp / SYNTH_BLOCK_SIZE];
}

static void
stop_voice(byte i)
{
	state[i].amp_env_phase = 0xffff;
	state[i].amp_env_phase_inc = 0;
	state[i].playing = 0;
}

static void
stop_mod_env(byte i)
{
	if (!state[i].mod_loop_on) {
		state[i].mod_env_phase = 0xffff;
		state[i].mod_env_phase_inc = 0;
	}
}

static uint16_t
add_modulation(uint16_t val, uint8_t mod, int8_t amount)
{
	if (amount < 0) {
		uint8_t a = (~amount) << 1;
		return U16U16SubSat(val, U8U8Mul(mod, a));
	} else {
		uint8_t a = amount << 1;
		return U16U16AddSat(val, U8U8Mul(mod, a));
	}
}

static void
do_modulations(void)
{
	for (byte i = 0; i < N_CHANS; i++) {
		state[i].amp_env_phase += state[i].amp_env_phase_inc;
		if (state[i].amp_env_phase < state[i].amp_env_phase_inc) {
			stop_voice(i);
		}
		state[i].mod_env_phase += state[i].mod_env_phase_inc;
		if (state[i].mod_env_phase < state[i].mod_env_phase_inc) {
			stop_mod_env(i);
		}
		uint16_t pitch = (uint16_t)state[i].qnote << 8;
		uint8_t mod_val = InterpolateSampleU8(state[i].mod_env_data, state[i].mod_env_phase);
		pitch = U16S16AddSat(pitch, S8U8Mul(state[i].pch_mod_amount, mod_val));
		pitch += U8U8Mul(rand_next(), state[i].crunchiness >> 1);

		state[i].osc_phase_inc = InterpolateIncreasingU16(
			note_phase_inc_lut,
			pitch);

		uint8_t env_level = InterpolateSampleU8(state[i].amp_env_data, state[i].amp_env_phase);
		uint8_t level = U8U8MulShift8(
			env_level,
			state[i].level);
		state[i].amp_level_noise = U8U8MulShift8(
			level,
			state[i].crunchiness);
		state[i].amp_level = U8U8MulShift8(
			level,
			~state[i].crunchiness);

		/* filter */
		Patch *patch = state[i].patch;
		uint16_t filter = patch->cutoff << 8;
		filter = add_modulation(filter, mod_val, patch->fil_mod_amount);
		filter = add_modulation(filter, env_level, patch->fil_env_amount);
		filter = InterpolateIncreasingU16(cutoff_to_dac_lut, filter);
		state[i].filter = filter;
	}
}

static inline uint8_t
clamp_out(int16_t out)
{
	if (out < 0)
		return 0;
	if (out > 255)
		return 255;
	return out;
}


static void
render_block(void)
{
	do_modulations();
	if (chan_on_out[FilteredOut] != NoChannel) {
		filterbuf_write(state[chan_on_out[FilteredOut]].filter);
	}
	byte noise = rand_next();
	for (byte i = 0; i < SYNTH_BLOCK_SIZE; i++) {
		int16_t a = 128, b = 128;
		/* we can share just one noise source amongs all channels */
		/* TODO: noise could be just accumulated and moved out of the loop */
		noise = (noise * 73) + 1;
		for (byte i = 0; i < N_CHANS; i++) {
			byte samp = InterpolateSampleS8(state[i].wave_data, state[i].osc_phase);
			state[i].osc_phase += state[i].osc_phase_inc;

			if (state[i].out == 0) {
				a += ((int8_t)samp * (int16_t)state[i].amp_level) >> 7;
				a += S8U8MulShift8(noise, state[i].amp_level_noise);
			} else {
				b += ((int8_t)samp * (int16_t)state[i].amp_level) >> 7;
				b += S8U8MulShift8(noise, state[i].amp_level_noise);
			}
		}
		struct outs outs;
		outs.a = clamp_out(a);
		outs.b = clamp_out(b);
		audiobuf_overwrite(&audiobuf, outs);
	}
}

static void
render_blocks(void)
{
	while (audiobuf_writable(&audiobuf) >= SYNTH_BLOCK_SIZE) {
		render_block();
	}
}

static void
render_silence(void)
{
	struct outs outs = { .a = 128, .b = 128 };
	while (audiobuf_writable(&audiobuf) > 0) {
		audiobuf_overwrite(&audiobuf, outs);
	}
}

static void
synth_init(void)
{
	for (byte i = 0; i < N_CHANS; i++) {
		state[i].wave_data = waveform_sin;
		state[i].mod_env_data = envelope_exp;
		state[i].amp_env_data = envelope_exp;
	}
}

static void
synth_play_on(Patch *patch, byte i, byte group, byte note)
{
	SynthState *st = &state[i];

        st->playing = 1;
        st->group = group;
	st->patch = patch;
	st->qnote = note << 1;
	st->osc_phase = 0;
	st->amp_env_phase = 0;
	st->amp_env_phase_inc = pgm_read_word(
		&env_phase_inc_lut[patch->amp_env_time]);
	st->mod_env_phase = 0;
	st->mod_env_phase_inc = pgm_read_word(
		&env_phase_inc_lut[patch->mod_env_time]);
	st->level = patch->level;
	st->crunchiness = patch->crunchiness;
	st->pch_mod_amount = patch->pch_mod_amount;
	st->mod_loop_on = patch->mod_loop_on;

	st->mod_env_data = (void *) pgm_read_word(
		&env_tables[patch->mod_env_type]);
	st->amp_env_data = (void *) pgm_read_word(
		&env_tables[patch->amp_env_type]);
	st->wave_data = (void *) pgm_read_word(
		&wav_tables[patch->wav_type]);

	st->out = patch->out;
}

static void
fix_chan_outs(void)
{
	for (byte i = 0; i < N_OUTS; i++) {
		byte ch = chan_on_out[i];
		if (ch != NoChannel) {
			if (state[ch].out != i)
				chan_on_out[i] = NoChannel;
		}
	}
	for (byte i = 0; i < N_CHANS; i++) {
		byte out = state[i].out;
		if (chan_on_out[out] == NoChannel) {
			chan_on_out[out] = i;
		}
	}
}

static void
synth_play(Patch *patch, byte group, byte note)
{
	if (group > 0) {
		for (byte i = 0; i < N_CHANS; i++) {
			if (state[i].playing && state[i].group == group) {
				stop_voice(i);
			}
		}
	}

	last_chan = (last_chan + 1) % N_CHANS;
	for (byte i = 0; i < N_CHANS; i++) {
		if (!state[last_chan].playing)
			break;
		last_chan = (last_chan + 1) % N_CHANS;
	}

	synth_play_on(patch, last_chan, group, note);
	chan_on_out[patch->out] = last_chan;
	fix_chan_outs();
}


static void
rand_patch(Patch *patch)
{
	patch->level = 0x7f;
	if ((rand_next() & 7) == 0)
		patch->crunchiness = rand_next();
	else
		patch->crunchiness = 0;

	patch->wav_type = rand_next() % ARRAY_SIZE(wav_tables);
	patch->mod_env_type = rand_next() % ARRAY_SIZE(env_tables);
	patch->amp_env_type = rand_next() % ARRAY_SIZE(env_tables);
	patch->amp_env_time = rand_next();
	patch->mod_env_time = rand_next();
	if ((rand_next() & 7) == 0)
		patch->pch_mod_amount = rand_next();
	else
		patch->pch_mod_amount = 0;
	patch->mod_loop_on = rand_next() & 1;
	patch->cutoff = rand_next();
	patch->fil_mod_amount = rand_next();
	patch->fil_env_amount = rand_next();
}

static void
empty_patch(Patch *patch)
{
	patch->level = 127;
	patch->crunchiness = 0;
	patch->amp_env_type = 0;
	patch->mod_env_type = 0;
	patch->wav_type = 0;
	patch->amp_env_time = 70;
	patch->mod_env_time = 70;
	patch->pch_mod_amount = 0;
	patch->mod_loop_on = 0;
	patch->out = 0;
	patch->cutoff = 255;
	patch->fil_mod_amount = 0;
	patch->fil_env_amount = 0;
}


static void
metro_patch(Patch *patch)
{
	empty_patch(patch);
	patch->crunchiness = 5;
	patch->wav_type = 6;
	patch->amp_env_time = 40;
}

static void
print_patch(Patch *sins)
{
	printf("wav%d o%d env=(%d %d) mod=(%d %d %d) p=(m%d) f=(%d m%d e%d) l=%d c=%d\n",
		sins->wav_type,
		sins->out,
		sins->amp_env_type,
		sins->amp_env_time,
		sins->mod_env_type,
		sins->mod_env_time,
		sins->mod_loop_on,
		sins->pch_mod_amount,
		sins->cutoff,
		sins->fil_mod_amount,
		sins->fil_env_amount,
		sins->level,
		sins->crunchiness);
}
