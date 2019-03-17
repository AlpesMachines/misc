enum {
	PatchMagic1 = 'M',
	PatchMagic2 = 'E',
	Npatches = 10,
};

typedef struct PatchMemory PatchMemory;

struct PatchMemory {
	byte magic[3];
	Patch patch[Npatches];
};

static PatchMemory patchmem EEMEM;

static void
patchmem_init(Patch *empty)
{
	STATIC_ASSERT(sizeof(PatchMemory) <= 1024, patch_memory_doesnt_fit_to_eeprom);

	/* check magic bytes */
	byte magic[3], exp_magic[3];
	exp_magic[0] = PatchMagic1;
	exp_magic[1] = PatchMagic2;
	exp_magic[2] = sizeof(Patch);
	eeprom_read_block(magic, patchmem.magic, sizeof(magic));
	if (memcmp(magic, exp_magic, sizeof(magic)) == 0) {
		/* patch memory is initialized */
		return;
	}

	/* write magic bytes */
	eeprom_update_block(exp_magic, patchmem.magic, sizeof(exp_magic));

	/* write empty patches */
	for (byte i = 0; i < Npatches; i++) {
		eeprom_update_block(empty, &patchmem.patch[i], sizeof(*empty));
	}
	printf("patchmem initialized\n");
}

static void
load_patch(Patch *patch, byte num)
{
	if (num >= Npatches)
		return;
	eeprom_read_block(patch, &patchmem.patch[num], sizeof(*patch));
}

static void
save_patch(Patch *patch, byte num)
{
	if (num >= Npatches)
		return;
	eeprom_update_block(patch, &patchmem.patch[num], sizeof(*patch));
}
