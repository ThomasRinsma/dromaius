#ifndef GBEMU_H
#define GBEMU_H

#include <stdint.h>
#include <SDL.h>

#define HEADER_START 0x134

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
	int t;
} cpu_t;

typedef struct mem_s {
	uint8_t *bios;
	uint8_t *rom;
	uint8_t *workram;
	uint8_t *extram;
	uint8_t *zeropageram;
	int romlen;
	int biosLoaded;
} mem_t;

typedef enum gpumode_s {
	GPU_MODE_HBLANK,
	GPU_MODE_VBLANK,
	GPU_MODE_OAM,
	GPU_MODE_VRAM
} gpumode_t;

typedef struct gpuflags_s {
	uint8_t bgtoggle;
	uint8_t lcdtoggle;
	uint8_t bgmap;
	uint8_t bgtile;
	uint8_t scx;
	uint8_t scy;
} gpuflags_t;

typedef struct gpu_s {
	uint8_t ***tileset;
	uint8_t *vram;
	uint8_t *oam;
	uint8_t palette[4];
	gpuflags_t f;
	gpumode_t mode;
	uint8_t line;
	int mclock;
} gpu_t;


#endif
