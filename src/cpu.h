#ifndef INCLUDED_CPU_H
#define INCLUDED_CPU_H

#include <cstdint>
struct Dromaius;

#define CPU_CALL_STACK_SIZE 0x100

/*

GB CPU freq:                 4194304 Hz
m-cycles per sec:            1048576 Hz
DIV register, increments at:   16384 Hz (every 64 m-cycles)
*/


struct CPU
{
	enum Int {
		VBLANK  = 0x01,
		LCDSTAT = 0x02,
		TIMER   = 0x04,
		SERIAL  = 0x08,
		JOYPAD  = 0x10
	};

	enum Flag {
		ZERO      = 0x80,
		SUBTRACT  = 0x40,
		HCARRY    = 0x20,
		CARRY     = 0x10,
	};

	struct regs_s {
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
	};

	struct timer_s {
		uint8_t div;
		uint8_t tima;
		uint8_t tma;
		uint8_t tac;
		unsigned long long cycleCount;
		int cycleCountDiv;
		int maxCount[4]; // const
	};

	// Up-reference
	Dromaius *emu;

	regs_s r;
	regs_s registerStore;

	bool intsOn;
	uint8_t intFlags;
	uint8_t oldIntFlags;
	uint8_t ints;
	timer_s timer;

	bool halted;

	bool fastForward;
	bool stepMode;
	bool stepInst;
	bool stepFrame;

	// Debugging
	uint16_t callStack[CPU_CALL_STACK_SIZE];
	int callStackDepth;

	// Cycle count
	unsigned long long c;
	unsigned long long dc;

	void initialize();

	void setFlag(Flag flag);
	void resetFlag(Flag flag);
	int getFlag(Flag flag);
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
	uint8_t *numToRegPtr(uint8_t num);
	void printRegisters();
	void doExtraOP(uint8_t op);
	void handleTimers();
	void handleInterrupts();
	int executeInstruction();
	inline const char *numToRegName(uint8_t num);
	uint16_t instructionToString(uint16_t pc, char *instStr);
	void disassemble(uint16_t pc, size_t instCnt, char *buf);

	void callStackPush(uint16_t oldpc, uint16_t pc);
	void callStackPop(uint16_t oldpc, uint16_t pc);
};

#endif