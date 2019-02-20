/* mono-voice allocation with last-note priority */
enum {
	MidiMaxVoices = 8,
};

typedef struct MonoVoice MonoVoice;
struct MonoVoice {
	byte n_playing;
	byte notes[MidiMaxVoices];
};

static void
monovoice_add(MonoVoice *mv, byte note)
{
	if (mv->n_playing >= MidiMaxVoices)
		return;
	mv->notes[mv->n_playing++] = note;
}

static void
monovoice_del(MonoVoice *mv, byte note)
{
	byte k = 0;
	for (byte i = 0; i < mv->n_playing; i++) {
		mv->notes[k] = mv->notes[i];
		if (mv->notes[i] != note)
			k++;
	}
	mv->n_playing = k;
}
