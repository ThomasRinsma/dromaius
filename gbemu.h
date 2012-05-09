#ifndef GBEMU_H
#define GBEMU_H

#include <stdint.h>
#include <SDL.h>

#define HEADER_START        0x134

#define GPU_TILEMAP_ADDR1   0x1800
#define GPU_TILEMAP_ADDR0   0x1C00

#define CPU_CLOCKS_PER_FRAME 17556

typedef struct keymap_s {
	int start;
	int select;
	int left;
	int up;
	int right;
	int down;
	int b;
	int a;
} keymap_t;

typedef struct settings_s {
	int debug;
	keymap_t keymap;
} settings_t;

typedef struct romheader_s {
	char	gamename[15];
	uint8_t	colorbyte;		// 0x80 = yes
	uint16_t newlicensee;
	uint8_t	sgbfeatures;	// 0x03 = yes
	uint8_t	type;
	uint8_t	romsize;
	uint8_t	ramsize;
	uint8_t	country;		// 0x00 = japan, 0x01 = other
	uint8_t	oldlicensee;
	uint8_t	headersum;
	uint16_t romsum;
} romheader_t;

typedef struct regs_s {
	uint8_t a;
	uint8_t b;
	uint8_t c;
	uint8_t d;
	uint8_t e;
	uint8_t h;
	uint8_t l;
	uint8_t f;		// flags
	uint16_t pc;	// program counter
	uint16_t sp;	// stack pointer
} regs_t;

typedef struct cpu_s {
	regs_t r;
	int ints;
	int c;
	int dc;
} cpu_t;

typedef struct mem_s {
	uint8_t *bios;
	uint8_t *rom;
	uint8_t *workram;
	uint8_t *extram;
	uint8_t *zeropageram;
	uint8_t inputRow[2];
	uint8_t inputWire;
	size_t romlen;
	int biosLoaded;
} mem_t;

typedef enum gpumode_s {
	GPU_MODE_HBLANK,
	GPU_MODE_VBLANK,
	GPU_MODE_OAM,
	GPU_MODE_VRAM
} gpumode_t;

typedef enum gpuflags_s {
	GPU_FLAG_BG,
	GPU_FLAG_SPRITES,
	GPU_FLAG_SPRITESIZE,
	GPU_FLAG_TILEMAP,
	GPU_FLAG_TILESET,
	GPU_FLAG_WINDOW,
	GPU_FLAG_WINDOWTILEMAP,
	GPU_FLAG_LCD
} gpuflags_t;

typedef struct gpuregs_s {
	gpuflags_t flags;
	uint8_t line;
	uint8_t scx;
	uint8_t scy;
} gpuregs_t;

typedef struct gpu_s {
	uint8_t ***tileset;
	uint8_t *vram;
	uint8_t *oam;
	uint8_t palette[4];
	gpuregs_t r;
	gpumode_t mode;
	int mclock;
} gpu_t;


#endif
