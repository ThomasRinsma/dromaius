#ifndef GBEMU_H
#define GBEMU_H

#include <stdint.h>
#include <SDL2/SDL.h>

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

typedef struct timer_s {
	uint8_t div;
	uint8_t tima;
	uint8_t tma;
	uint8_t tac;
	int cycleCount;
	int cycleCountDiv;
	int maxCount[4];
} cputimer_t;

typedef struct cpu_s {
	regs_t r;
	uint8_t intsOn; // bool
	uint8_t intFlags;
	uint8_t oldIntFlags;
	uint8_t ints;
	cputimer_t timer;
	int halted;
	int c;
	int dc;
} cpu_t;

typedef enum mbc_e {
	MTYPE_NONE,
	MTYPE_MBC1,
	MTYPE_MBC2,
	MTYPE_MBC3,
	MTYPE_MBC4,
	MTYPE_MBC5,
	MTYPE_MMM01,
	MTYPE_OTHER,
} mbc_t;

/*
	00  ROM ONLY                 
	01  MBC1                     
	02  MBC1+RAM                 
	03  MBC1+RAM+BATTERY       

	05  MBC2                     
	06  MBC2+BATTERY  

	08  ROM+RAM                  
	09  ROM+RAM+BATTERY  

	0B  MMM01                    
	0C  MMM01+RAM                
	0D  MMM01+RAM+BATTERY    

	0F  MBC3+TIMER+BATTERY       
	10  MBC3+TIMER+RAM+BATTERY   
	11  MBC3                     
	12  MBC3+RAM
	13  MBC3+RAM+BATTERY

	15  MBC4
	16  MBC4+RAM
	17  MBC4+RAM+BATTERY

	19  MBC5
	1A  MBC5+RAM
	1B  MBC5+RAM+BATTERY
	1C  MBC5+RUMBLE
	1D  MBC5+RUMBLE+RAM
	1E  MBC5+RUMBLE+RAM+BATTERY


	FC  POCKET CAMERA
	FD  BANDAI TAMA5
	FE  HuC3
	FF  HuC1+RAM+BATTERY
*/

typedef struct mem_s {
	uint8_t *bios;
	uint8_t *rom;
	uint8_t *workram;
	uint8_t *extram;
	uint8_t *zeropageram;
	uint8_t inputRow[2];
	uint8_t inputWire;
	int biosLoaded;

	size_t romlen;
	uint8_t ramSize;
	/*
		00h - None
		01h - 2 KBytes
		02h - 8 Kbytes
		03h - 32 KBytes (4 banks of 8KBytes each)
	*/

	mbc_t mbc;

	int ramEnabled;
	uint8_t bankMode; // 0 = ROM, 1 = RAM
	uint8_t ramBank;
	uint8_t romBank;
	uint8_t rtcReg;
	uint8_t rtc[5];
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
	GPU_FLAG_SPRITESIZE = 0x04, // (0=8x8, 1=8x16)
	GPU_FLAG_TILEMAP = 0x08, // (0=9800-9BFF, 1=9C00-9FFF)
	GPU_FLAG_TILESET = 0x10, // (0=8800-97FF, 1=8000-8FFF)
	GPU_FLAG_WINDOW = 0x20,
	GPU_FLAG_WINDOWTILEMAP = 0x40, // (0=9800-9BFF, 1=9C00-9FFF)
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

typedef struct audio_s {
	struct channel1_t {
		uint8_t isEnabled; // bool
		uint8_t isRestarted; // bool
		uint32_t ctr;

		// Duty (4 bits)
		uint8_t duty;

		// Sweep
		uint8_t sweepTime;
		uint8_t sweepDir;
		uint8_t sweepExp;
		double lastSweep;

		// Envelope
		uint8_t envVol;
		uint8_t envDir;
		uint8_t envSteps;

		// General
		uint8_t soundLen;
		uint16_t freq;
		uint8_t isCont; // bool
	} ch1;

	struct channel2_t {
		uint8_t isEnabled; // bool
		uint8_t isRestarted; // bool
		uint32_t ctr;

		// Duty (4 bits)
		uint8_t duty;

		// Envelope
		uint8_t envVol;
		uint8_t envDir;
		uint8_t envSteps;

		// General
		uint8_t soundLen;
		uint16_t freq;
		uint8_t isCont; // bool
	} ch2;

	struct channel3_t {
		uint8_t isEnabled; // bool
		uint8_t isRestarted; // bool
		uint32_t ctr;
		uint32_t waveCtr;

		// General
		uint8_t volume;
		uint8_t soundLen;
		uint16_t freq;
		uint8_t isCont; // bool
	} ch3;

	struct channel4_t {
		uint8_t isEnabled; // bool
		uint8_t isRestarted; // bool
		uint32_t ctr;

		// Freq spec
		uint8_t s;
		uint8_t r;
		// TODO: bit 3 (softness?)

		// Envelope
		uint8_t envVol;
		uint8_t envDir;
		uint8_t envSteps;

		// General
		uint8_t soundLen;
		uint8_t isCont; // bool
	} ch4;

	uint8_t isEnabled; // bool
	uint8_t waveRam[16]; // 32 nibbles

	// TODO: channel / sound selection (FF25)
	// TODO: master channel volumes / Vin (FF24)

} audio_t;


// cpu.c
void initCPU();
void setFlag(uint8_t flag);
void resetFlag(uint8_t flag);
int getFlag(uint8_t flag);
void doIncReg(uint8_t *regp);
void doDecReg(uint8_t *regp);
void doAddReg(uint8_t *regp, uint8_t val);
void doAddRegWithCarry(uint8_t *regp, uint8_t val);
void doSubReg(uint8_t *regp, uint8_t val);
void doSubRegWithCarry(uint8_t *regp, uint8_t val);
void doAddHL(uint16_t val);
void doRotateLeftWithCarry(uint8_t *regp);
void doRotateRightWithCarry(uint8_t *regp);
void doRotateLeft(uint8_t *regp);
void doRotateRight(uint8_t *regp);
void doShiftLeft(uint8_t *regp);
void doShiftRight(uint8_t *regp);
void doShiftRightL(uint8_t *regp);
void doSwapNibbles(uint8_t *regp);
void doBit(uint8_t *regp, uint8_t bit);
void doRes(uint8_t *regp, uint8_t bit);
void doSet(uint8_t *regp, uint8_t bit);
void doAndRegA(uint8_t val);
void doXorRegA(uint8_t val);
void doOrRegA(uint8_t val);
void doCpRegA(uint8_t val);
void doOpcodeUNIMP();
void printRegisters();
void doExtraOP(uint8_t op);
void handleTimers();
void handleInterrupts();
int executeInstruction();

// gpu.c
void initDisplay();
void initGPU();
void freeGPU();
uint8_t gpuReadIOByte(uint16_t addr);
void gpuWriteIOByte(uint8_t b, uint16_t addr);
void setPixelColor(int x, int y, uint8_t color);
void setDebugPixelColor(int x, int y, uint8_t color);
void printGPUDebug();
void renderDebugBackground();
void renderScanline();
void updateTile(uint8_t b, uint16_t addr);
void gpuBuildSpriteData(uint8_t b, uint16_t addr);
void renderFrame();
void stepGPU();

// mem.c
void initMemory(uint8_t *rombuffer, size_t romlen);
void freeMemory();
uint8_t readByte(uint16_t addr);
uint16_t readWord(uint16_t addr);
void writeByte(uint8_t b, uint16_t addr);
void writeWord(uint16_t w, uint16_t addr);
void handleGameInput(int state, SDL_Keycode key);
void dumpToFile();

// audio.c
void initAudio();
void freeAudio();
void audioWriteByte(uint8_t b, uint16_t addr);
void play_audio(void *userdata, uint8_t *stream, int len);

#endif
