#ifndef GBEMU_H
#define GBEMU_H

#include <stdint.h>
#include <SDL.h>

#define HEADER_START        0x134

#define GPU_TILEMAP_ADDR0   0x1800
#define GPU_TILEMAP_ADDR1   0x1C00

#define CPU_CLOCKS_PER_FRAME 17556 // 70224 / 4 clock cycles

typedef struct keymap_s {
	int start;
	int startDown;
	
	int select;
	int selectDown;
	
	int left;
	int leftDown;
	
	int up;
	int upDown;
	
	int right;
	int rightDown;
	
	int down;
	int downDown;
	
	int b;
	int bDown;
	
	int a;
	int aDown;
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

typedef enum interrupts_s {
	INT_VBLANK = 0x01,
	INT_LCDSTAT = 0x02,
	INT_TIMER = 0x04,
	INT_SERIAL = 0x08,
	INT_JOYPAD = 0x10
} interrupts_t;

typedef struct cpu_s {
	regs_t r;
	uint8_t intsOn; // bool
	uint8_t intFlags;
	uint8_t ints;
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

	int ramEnabled;
	uint8_t bankMode; // 0 = ROM, 1 = RAM
	uint8_t ramBank;
	uint8_t romBank;
} mem_t;

typedef enum gpumode_s {
	GPU_MODE_HBLANK,
	GPU_MODE_VBLANK,
	GPU_MODE_OAM,
	GPU_MODE_VRAM
} gpumode_t;

typedef enum gpuflags_s {
	GPU_FLAG_BG = 0x01,
	GPU_FLAG_SPRITES = 0x02,
	GPU_FLAG_SPRITESIZE = 0x04,
	GPU_FLAG_TILEMAP = 0x08,
	GPU_FLAG_TILESET = 0x10,
	GPU_FLAG_WINDOW = 0x20,
	GPU_FLAG_WINDOWTILEMAP = 0x40,
	GPU_FLAG_LCD = 0x80
} gpuflags_t;

typedef struct gpuregs_s {
	gpuflags_t flags;
	uint8_t line;
	uint8_t lineComp;
	uint8_t scx;
	uint8_t scy;
} gpuregs_t;

typedef struct sprite_s {
	uint8_t x;
	uint8_t y;
	uint8_t tile;
	uint8_t flags;
} sprite_t;

typedef enum spriteflags_s {
	SPRITE_FLAG_PALETTE = 0x10,
	SPRITE_FLAG_XFLIP = 0x20,
	SPRITE_FLAG_YFLIP = 0x40,
	SPRITE_FLAG_PRIORITY = 0x80
} spriteflags_t;

typedef struct gpu_s {
	uint8_t ***tileset;
	sprite_t *spritedata;
	uint8_t *vram;
	uint8_t *oam;
	uint8_t bgpalette[4];
	uint8_t objpalette[2][4];
	gpuregs_t r;
	gpumode_t mode;
	int mclock;
	int hBlankInt;
	int vBlankInt;
	int OAMInt;
	int CoinInt;
} gpu_t;


#endif
