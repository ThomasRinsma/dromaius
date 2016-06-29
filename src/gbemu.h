#ifndef INCLUDED_GBEMU_H
#define INCLUDED_GBEMU_H

#include <cstdint>
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



/*

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
*/
#endif
