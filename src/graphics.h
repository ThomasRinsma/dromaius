#ifndef INCLUDED_GRAPHICS_H
#define INCLUDED_GRAPHICS_H

#include <cstdint>

#define GB_SCREEN_WIDTH  160
#define GB_SCREEN_HEIGHT 144

#define DEBUG_WIDTH   (8*16)
#define DEBUG_HEIGHT  (8*24)

//#define WINDOW_SCALE  2

struct Graphics
{
	enum Mode {
		HBLANK,
		VBLANK,
		OAM,
		VRAM
	};

	enum Flag {
		BG            = 0x01,
		SPRITES       = 0x02,
		SPRITESIZE    = 0x04, // (0=8x8, 1=8x16)
		TILEMAP       = 0x08, // (0=9800-9BFF, 1=9C00-9FFF)
		TILESET       = 0x10, // (0=8800-97FF, 1=8000-8FFF)
		WINDOW        = 0x20,
		WINDOWTILEMAP = 0x40, // (0=9800-9BFF, 1=9C00-9FFF)
		LCD           = 0x80
	};


	enum SpriteFlag {
		PALETTE  = 0x10,
		XFLIP    = 0x20,
		YFLIP    = 0x40,
		PRIORITY = 0x80
	};

	struct regs_s {
		uint8_t flags;
		uint8_t line;
		uint8_t lineComp;
		uint8_t scx;
		uint8_t scy;
		uint8_t winx;
		uint8_t winy;
	};

	struct sprite_s {
		uint8_t x;
		uint8_t y;
		uint8_t tile;
		uint8_t flags;
	};


	uint8_t ***tileset;
	sprite_s *spritedata;
	uint8_t *vram;
	uint8_t *oam;
	uint8_t bgpalette[4];
	uint8_t objpalette[2][4];
	regs_s r;
	uint8_t mode;
	int mclock;
	int hBlankInt;
	int vBlankInt;
	int OAMInt;
	int CoinInt;

	// Window stuff
	SDL_Window *mainWindow;

	uint32_t screenTexture;
	uint32_t debugTexture;

	uint32_t *screenPixels;
	uint32_t *debugTilesetPixels;

	int screenScale;


	// debug stuff, TODO move to separate class
	bool showDebugCPU = true;
	bool showDebugGraphics = true;
	bool showDebugAudio = true;


	bool initialized = false;

	~Graphics();

	void initialize();
	void freeBuffers();
	void initDisplay();
	void updateTextures();

	uint8_t readByte(uint16_t addr);
	void writeByte(uint8_t b, uint16_t addr);

	void setPixelColor(int x, int y, uint8_t color);
	void setPixelColorDebug(int x, int y, uint8_t color);
	void setDebugPixelColor(int x, int y, uint8_t color);
	void printDebug();
	void renderDebugTileset();
	void renderScanline();
	void updateTile(uint8_t b, uint16_t addr);
	void buildSpriteData(uint8_t b, uint16_t addr);
	void renderFrame();
	void renderGUI();

	void step();


};

#endif