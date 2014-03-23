#include <stdio.h>
#include <stdlib.h>
#include "gbemu.h"

#define F_ZERO		0x80
#define F_SUBSTRACT	0x40
#define F_HCARRY	0x20
#define F_CARRY		0x10

extern settings_t settings;
extern mem_t mem;
extern cpu_t cpu;
extern gpu_t gpu;

regs_t registerStore;

void initCPU() {
	cpu.r.a = 0x01;
	cpu.r.b = 0x00;
	cpu.r.c = 0x13;
	cpu.r.d = 0x00;
	cpu.r.e = 0xD8;
	cpu.r.h = 0x01;
	cpu.r.l = 0x4D;
	cpu.r.f = 0xB0;//0;
	cpu.r.pc = 0x0000;
	cpu.r.sp = 0xFFFE;
	
	cpu.intsOn = 1;//1;
	cpu.intFlags = 0;
	cpu.ints = 0;

	cpu.timer.div = 0;
	cpu.timer.tima = 0;
	cpu.timer.tma = 0;
	cpu.timer.tac = 0;

	cpu.timer.cycleCount = 0;
	cpu.timer.cycleCountDiv = 0;


	/*

		Bit 2    - Timer Stop  (0=Stop, 1=Start)
		Bits 1-0 - Input Clock Select
		           00:   4096 Hz  (every 1024|256 cycles)
		           01: 262144 Hz  (every 16|4 cycles)
		           10:  65536 Hz  (every 64|16 cycles)
		           11:  16384 Hz  (every 256|64 cycles)

		(DMG runs at 4194304 Hz)
	*/
	cpu.timer.maxCount[0] = 256;
	cpu.timer.maxCount[1] = 4;
	cpu.timer.maxCount[2] = 16;
	cpu.timer.maxCount[3] = 64;

	cpu.halted = 0;

	cpu.c = 0;
}

void setFlag(uint8_t flag) {
	cpu.r.f |= flag;
}

void resetFlag(uint8_t flag) {
	cpu.r.f &= ~flag;
}

int getFlag(uint8_t flag) {
	return (cpu.r.f & flag) ? 1 : 0;
}

void doIncReg(uint8_t *regp) {
	if (*regp == 0xFF) {
		*regp = 0;
		setFlag(F_ZERO);
	} else {
		++(*regp);
		resetFlag(F_ZERO);
	}

	if ((*regp & 0x0F) == 0) {
		setFlag(F_HCARRY);
	} else {
		resetFlag(F_HCARRY);
	}
	
	resetFlag(F_SUBSTRACT);
}

void doDecReg(uint8_t *regp) {
	uint8_t tmp = *regp;

	if (*regp == 0) {
		*regp = 0xFF;
	} else {
		--(*regp);
	}

	if ((tmp & 0x0F) < (*regp & 0x0F)) {
		setFlag(F_HCARRY);
	} else {
		resetFlag(F_HCARRY);
	}

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	setFlag(F_SUBSTRACT);
}

void doAddReg(uint8_t *regp, uint8_t val) {
	if ((*regp & 0x0F) + (val & 0x0F) > 0x0F) {
		setFlag(F_HCARRY);
	} else {
		resetFlag(F_HCARRY);
	}

	if ((*regp + val) > 0xFF) {
		setFlag(F_CARRY);
	} else {
		resetFlag(F_CARRY);
	}

	*regp = ((*regp + val) & 0xFF);

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_SUBSTRACT);
}

void doAddRegWithCarry(uint8_t *regp, uint8_t val) {
	uint8_t carry = getFlag(F_CARRY);
	uint16_t tmp = *regp + val + carry;

	if (tmp > 0xFF) {
		setFlag(F_CARRY);
	} else {
		resetFlag(F_CARRY);
	}

	if (((*regp & 0x0F) + (val & 0x0F) + carry) > 0x0F) {
		setFlag(F_HCARRY);
	} else {
		resetFlag(F_HCARRY);
	}

	*regp = tmp & 0xFF;

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_SUBSTRACT);
}

void doSubReg(uint8_t *regp, uint8_t val) {
	if (*regp == val) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	if ((*regp & 0x0F) < (val & 0x0F)) {
		setFlag(F_HCARRY);
	} else {
		resetFlag(F_HCARRY);
	}

	if (*regp < val) {
		*regp = (0x100 - (val - *regp));
		setFlag(F_CARRY);
	} else {
		*regp -= val;
		resetFlag(F_CARRY);
	}

	setFlag(F_SUBSTRACT);
}

void doSubRegWithCarry(uint8_t *regp, uint8_t val) {
	uint8_t carry = getFlag(F_CARRY);
	int16_t tmp = *regp - val - carry;

	if (tmp < 0) {
		tmp += 0x100;
		setFlag(F_CARRY);
	} else {
		resetFlag(F_CARRY);
	}

	if (((*regp & 0x0F) - (val & 0x0F) - carry) < 0) {
		setFlag(F_HCARRY);
	} else {
		resetFlag(F_HCARRY);
	}

	*regp = tmp;

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	setFlag(F_SUBSTRACT);
}

void doAddHL(uint16_t val) {
	uint32_t tmp = (cpu.r.h << 8) + cpu.r.l;

	if ((tmp & 0xFFF) + (val & 0xFFF) > 0xFFF) {
		setFlag(F_HCARRY);
	} else {
		resetFlag(F_HCARRY);
	}

	if ((tmp + val) > 0xFFFF) {
		setFlag(F_CARRY);
	} else {
		resetFlag(F_CARRY);
	}

	tmp = ((tmp + val) & 0xFFFF);

	cpu.r.h = tmp >> 8;
	cpu.r.l = tmp & 0xFF;

	resetFlag(F_SUBSTRACT);
}

void doRotateLeftWithCarry(uint8_t *regp) {
	uint8_t tmp = (*regp & 0x80);
	*regp <<= 1;

	if (getFlag(F_CARRY)) {
		*regp |= 0x01;
	}

	if (tmp == 0) {
		resetFlag(F_CARRY);
	} else {
		setFlag(F_CARRY);
	}

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_SUBSTRACT);
	resetFlag(F_HCARRY);
}

void doRotateRightWithCarry(uint8_t *regp) {
	uint8_t tmp = (*regp & 0x01);
	*regp >>= 1;

	if (getFlag(F_CARRY)) {
		*regp |= 0x80;
	}

	if (tmp == 0) {
		resetFlag(F_CARRY);
	} else {
		setFlag(F_CARRY);
	}

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_SUBSTRACT);
	resetFlag(F_HCARRY);
}

void doRotateLeft(uint8_t *regp) {
	if ((*regp & 0x80) == 0) {
		resetFlag(F_CARRY);
	} else {
		setFlag(F_CARRY);
	}

	*regp <<= 1;
	if (!getFlag(F_CARRY)) {
		*regp &= 0xFE;
	} else {
		*regp |= 0x01;
	}

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_HCARRY);
	resetFlag(F_SUBSTRACT);
}

void doRotateRight(uint8_t *regp) {
	if ((*regp & 0x01) == 0) {
		resetFlag(F_CARRY);
	} else {
		setFlag(F_CARRY);
	}

	*regp >>= 1;
	if (!getFlag(F_CARRY)) {
		*regp &= 0x7F;
	} else {
		*regp |= 0x80;
	}

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_HCARRY);
	resetFlag(F_SUBSTRACT);
}

void doShiftLeft(uint8_t *regp) {
	if ((*regp & 0x80) == 0) {
		resetFlag(F_CARRY);
	} else {
		setFlag(F_CARRY);
	}

	*regp <<= 1;

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_HCARRY);
	resetFlag(F_SUBSTRACT);
}

void doShiftRight(uint8_t *regp) {
	if ((*regp & 0x01) == 0) {
		resetFlag(F_CARRY);
	} else {
		setFlag(F_CARRY);
	}

	*regp >>= 1;
	if ((*regp & 0x40) == 0) {
		*regp &= 0x7F;
	} else {
		*regp |= 0x80;
	}

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_HCARRY);
	resetFlag(F_SUBSTRACT);
}

void doShiftRightL(uint8_t *regp) {
	if ((*regp & 0x01) == 0) {
		resetFlag(F_CARRY);
	} else {
		setFlag(F_CARRY);
	}

	*regp >>= 1;
	*regp &= 0x7F;

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_SUBSTRACT);
	resetFlag(F_HCARRY);
}

void doSwapNibbles(uint8_t *regp) {
	*regp = ((*regp & 0xF0) >> 4 | (*regp & 0x0F) << 4);

	if (*regp == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_HCARRY);
	resetFlag(F_SUBSTRACT);
	resetFlag(F_CARRY);
}

void doBit(uint8_t *regp, uint8_t bit) {
	if ((*regp & (0x01 << bit)) == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_SUBSTRACT);
	setFlag(F_HCARRY);
}

void doRes(uint8_t *regp, uint8_t bit) {
	*regp &= ~(0x01 << bit);
}

void doSet(uint8_t *regp, uint8_t bit) {
	*regp |= (0x01 << bit);
}


void doAndRegA(uint8_t val) {
	cpu.r.a = cpu.r.a & val;

	if (cpu.r.a == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_SUBSTRACT);
	resetFlag(F_CARRY);
	setFlag(F_HCARRY); // TODO: is this right?
}

void doXorRegA(uint8_t val) {
	cpu.r.a = cpu.r.a ^ val;

	if (cpu.r.a == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_SUBSTRACT);
	resetFlag(F_CARRY);
	resetFlag(F_HCARRY);
}

void doOrRegA(uint8_t val) {
	cpu.r.a = cpu.r.a | val;

	if (cpu.r.a == 0) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	resetFlag(F_SUBSTRACT);
	resetFlag(F_CARRY);
	resetFlag(F_HCARRY);
}

void doCpRegA(uint8_t val) {
	if ((cpu.r.a & 0x0F) < (val & 0x0F)) {
		setFlag(F_HCARRY);
	} else {
		resetFlag(F_HCARRY);
	}

	if (cpu.r.a < val) {
		setFlag(F_CARRY);
	} else {
		resetFlag(F_CARRY);
	}

	if (cpu.r.a == val) {
		setFlag(F_ZERO);
	} else {
		resetFlag(F_ZERO);
	}

	setFlag(F_SUBSTRACT);
}

void doOpcodeUNIMP() {
	printf("Unimplemented instruction (0x%02X) at 0x%04X.\n", readByte(cpu.r.pc-1), cpu.r.pc-1);
	//exit(1);
}

int lastInst;
void printRegisters() {
	printf("{a=%02X,f=%02X,b=%02X,c=%02X,d=%02X,e=%02X,h=%02X,l=%02X,pc=%04X,sp=%04X,inst=%02X}\n",
		cpu.r.a, cpu.r.f, cpu.r.b, cpu.r.c, cpu.r.d, cpu.r.e, cpu.r.h, cpu.r.l, cpu.r.pc, cpu.r.sp, lastInst);
}

uint8_t *numToRegPtr(uint8_t num) {
	switch(num % 8) {
		case 0: return &cpu.r.b;
		case 1: return &cpu.r.c;
		case 2: return &cpu.r.d;
		case 3: return &cpu.r.e;
		case 4: return &cpu.r.h;
		case 5: return &cpu.r.l;
		case 7: return &cpu.r.a;
		default: return NULL;
	}
}

void doExtraOP(uint8_t op) {
	uint8_t *regp;
	uint8_t tmp;
	uint8_t bitnr;

	// RLC
	if (op <= 0x07) {
		if ((op & 0x07) == 0x06) {
			tmp = readByte((cpu.r.h << 8) + cpu.r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doRotateLeft(regp);

		if ((op & 0x07) == 0x06) {
			writeByte(tmp, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
		}

		cpu.c += 2;
	}
	// RRC
	else if (op > 0x07 && op <= 0x0F) {
		if ((op & 0x07) == 0x06) {
			tmp = readByte((cpu.r.h << 8) + cpu.r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doRotateRight(regp);

		if ((op & 0x07) == 0x06) {
			writeByte(tmp, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
		}

		cpu.c += 2;
	}
	// RL
	else if(op > 0x0F && op <= 0x17) {
		if ((op & 0x07) == 0x06) {
			tmp = readByte((cpu.r.h << 8) + cpu.r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doRotateLeftWithCarry(regp);

		if ((op & 0x07) == 0x06) {
			writeByte(tmp, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
		}

		cpu.c += 2;
	}
	// RR
	else if(op > 0x17 && op <= 0x1F) {
		if ((op & 0x07) == 0x06) {
			tmp = readByte((cpu.r.h << 8) + cpu.r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doRotateRightWithCarry(regp);

		if ((op & 0x07) == 0x06) {
			writeByte(tmp, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
		}

		cpu.c += 2;
	}
	// SLA
	else if(op > 0x1F && op <= 0x27) {
		if ((op & 0x07) == 0x06) {
			tmp = readByte((cpu.r.h << 8) + cpu.r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doShiftLeft(regp);

		if ((op & 0x07) == 0x06) {
			writeByte(tmp, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
		}

		cpu.c += 2;
	}
	// SRA
	else if(op > 0x27 && op <= 0x2F) {
		if ((op & 0x07) == 0x06) {
			tmp = readByte((cpu.r.h << 8) + cpu.r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doShiftRight(regp);

		if ((op & 0x07) == 0x06) {
			writeByte(tmp, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
		}

		cpu.c += 2;
	}
	// SWAP
	else if(op > 0x2F && op <= 0x37) {
		if ((op & 0x07) == 0x06) {
			tmp = readByte((cpu.r.h << 8) + cpu.r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doSwapNibbles(regp);

		if ((op & 0x07) == 0x06) {
			writeByte(tmp, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
		}

		cpu.c += 2;
	}
	// SRL
	else if(op > 0x37 && op <= 0x3F) {
		if ((op & 0x07) == 0x06) {
			tmp = readByte((cpu.r.h << 8) + cpu.r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doShiftRightL(regp);

		if ((op & 0x07) == 0x06) {
			writeByte(tmp, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
		}

		cpu.c += 2;
	}
	// BIT
	else if(op > 0x3F && op <= 0x7F) {
		bitnr = (op - 0x40) / 8;

		if ((op & 0x07) == 0x06) {
			tmp = readByte((cpu.r.h << 8) + cpu.r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doBit(regp, bitnr);

		if ((op & 0x07) == 0x06) {
			writeByte(tmp, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 1;
		}

		cpu.c += 2;
	}
	// RES
	else if(op > 0x7F && op <= 0xBF) {
		bitnr = (op - 0x80) / 8;

		if ((op & 0x07) == 0x06) {
			tmp = readByte((cpu.r.h << 8) + cpu.r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doRes(regp, bitnr);

		if ((op & 0x07) == 0x06) {
			writeByte(tmp, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
		}

		cpu.c += 2;
	}
	// SET
	else {
		bitnr = (op - 0xC0) / 8;

		if ((op & 0x07) == 0x06) {
			tmp = readByte((cpu.r.h << 8) + cpu.r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doSet(regp, bitnr);

		if ((op & 0x07) == 0x06) {
			writeByte(tmp, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
		}

		cpu.c += 2;
	}
}


void handleTimers() {
	// Timer
	cpu.timer.cycleCountDiv += cpu.c - cpu.dc;
	cpu.timer.cycleCount += cpu.c - cpu.dc;

	if (cpu.timer.tac & 0x04) { // started
		if (cpu.timer.cycleCountDiv >= 64) {
			cpu.timer.cycleCountDiv -= 64;

			cpu.timer.div++;
			//printf("div = %d.\n", cpu.timer.div);
		}
		if (cpu.timer.cycleCount >= cpu.timer.maxCount[cpu.timer.tac & 0x03]) {
			cpu.timer.cycleCount -= cpu.timer.maxCount[cpu.timer.tac & 0x03];

			cpu.timer.tima++;
			//printf("tima = %d.\n", cpu.timer.tima);
			if (cpu.timer.tima == 0) {
				cpu.timer.tima = cpu.timer.tma;

				cpu.intFlags |= INT_TIMER;
				//printf("tima = tma = %d. cpu.intsOn=%d, cpu.ints=%d, cpu.intFlags=%d (int flag set)\n", 
				//	cpu.timer.tima, cpu.intsOn, cpu.ints, cpu.intFlags);
			}
		}
	}
}

void handleInterrupts() {
	// Interrupts
	if (cpu.intsOn && (cpu.ints & cpu.intFlags)) {
		uint8_t interrupts = cpu.intFlags & cpu.ints; // mask enabled interrupt(s)
		cpu.intsOn = 0;

		//if (cpu.halted)
		//{
		//	printf("halted was 1\n");
		//	cpu.halted = 0;
		//}
		
		// Push PC;
		cpu.r.sp -= 2;
		writeWord(cpu.r.pc, cpu.r.sp);

		if (interrupts & INT_VBLANK) {
			cpu.intFlags &= ~INT_VBLANK;
			cpu.r.pc = 0x40;
		}
		else if (interrupts & INT_LCDSTAT) {
			cpu.intFlags &= ~INT_LCDSTAT;
			cpu.r.pc = 0x48;
		}
		else if (interrupts & INT_TIMER) {
			printf("timer interrupt!\n");
			cpu.intFlags &= ~INT_TIMER;
			cpu.r.pc = 0x50;
		}
		else if (interrupts & INT_SERIAL) {
			cpu.intFlags &= ~INT_SERIAL;
			cpu.r.pc = 0x58;
		}
		else if (interrupts & INT_JOYPAD) {
			cpu.intFlags &= ~INT_JOYPAD;
			cpu.r.pc = 0x60;
		}	

		cpu.c += 8;
	}
}


int executeInstruction() {
	uint8_t inst;
	uint32_t utmp32, utmp32_2;
	int16_t tmp16, tmp16_2;
	uint16_t utmp16, utmp16_2;
	int8_t tmp8, tmp8_2;
	uint8_t utmp8, utmp8_2;
	
	cpu.dc = cpu.c;


	handleInterrupts();

	if(settings.debug) {
		//printf("Read instruction 0x%02X at 0x%04X. (%d)\n", inst, cpu.r.pc, cpu.r.pc);
		printRegisters();
	}

	if (!cpu.halted) {
		inst = readByte(cpu.r.pc);
		lastInst = inst;


		cpu.r.pc++;


		switch (inst) {
			case 0x00: // NOP
				cpu.c += 1;
				break;
				
			case 0x01: // LD BC, nn
				cpu.r.c = readByte(cpu.r.pc);
				cpu.r.b = readByte(cpu.r.pc + 1);
				cpu.r.pc += 2;
				cpu.c += 3;
				break;
				
			case 0x02: // LD (BC), A
				writeByte(cpu.r.a, (cpu.r.b << 8) + cpu.r.c);
				cpu.c += 2;
				break;
				
			case 0x03: // INC BC
				cpu.r.c++;
				if (!cpu.r.c) {
					cpu.r.b++;
				}
				cpu.c += 2;
				break;
				
			case 0x04: // INC B
				doIncReg(&cpu.r.b);
				cpu.c += 1;
				break;
				
			case 0x05: // DEC B
				doDecReg(&cpu.r.b);
				cpu.c += 1;
				break;
				
			case 0x06: // LD B, n
				cpu.r.b = readByte(cpu.r.pc);
				cpu.r.pc++;
				cpu.c += 2;
				break;
				
			case 0x07: // RLC A
				utmp8 = cpu.r.a & 0x80;
				cpu.r.a <<= 1;

				if (utmp8 == 0) {
					resetFlag(F_CARRY);
					cpu.r.a &= 0xFE;
				} else {
					setFlag(F_CARRY);
					cpu.r.a |= 0x01;
				}

				resetFlag(F_ZERO);
				resetFlag(F_SUBSTRACT);
				resetFlag(F_HCARRY);
				
				cpu.c += 1;
				break;
				
			case 0x08: // LD (nn), SP
				utmp8 = readByte(cpu.r.pc);
				utmp8_2 = readByte(cpu.r.pc + 1);
				writeWord(cpu.r.sp, (utmp8 << 8) + utmp8_2);
				cpu.r.pc += 2;
				cpu.c += 5;
				break;
				
			case 0x09: // ADD HL, BC
				doAddHL((cpu.r.b << 8) + cpu.r.c);
				cpu.c += 2;
				break;
				
			case 0x0A: // LD A, (BC)
				cpu.r.a = readByte((cpu.r.b << 8) + cpu.r.c);
				cpu.c += 2;
				break;
				
			case 0x0B: // DEC BC
				cpu.r.c--;
				if (cpu.r.c == 0xFF) {
					cpu.r.b--;
				}
				cpu.c += 2;
				break;
				
			case 0x0C: // INC C
				doIncReg(&cpu.r.c);
				cpu.c += 1;
				break;
				
			case 0x0D: // DEC C
				doDecReg(&cpu.r.c);
				cpu.c += 1;
				break;
				
			case 0x0E: // LD C, n
				cpu.r.c = readByte(cpu.r.pc);
				cpu.r.pc++;
				cpu.c += 2;
				break;
				
			case 0x0F: // RRC A
				utmp8 = (cpu.r.a & 0x01);
				cpu.r.a >>= 1;

				if (utmp8 == 0) {
					resetFlag(F_CARRY);
					cpu.r.a &= 0x7F;
				} else {
					setFlag(F_CARRY);
					cpu.r.a |= 0x80;
				}

				resetFlag(F_ZERO);
				resetFlag(F_SUBSTRACT);
				resetFlag(F_HCARRY);

				cpu.c += 1;
				break;
				
			case 0x10: // STOP
				// TODO: Implement this
				printf("STOP instruction\n");
				cpu.c += 1;
				break;
				
			case 0x11: // LD DE, nn
				cpu.r.e = readByte(cpu.r.pc);
				cpu.r.d = readByte(cpu.r.pc + 1);
				cpu.r.pc += 2;
				cpu.c += 3;
				break;
			
			case 0x12: // LD (DE), A
				writeByte(cpu.r.a, (cpu.r.d << 8) + cpu.r.e);
				cpu.c += 2;
				break;
				
			case 0x13: // INC DE
				cpu.r.e++;
				if (!cpu.r.e) {
					cpu.r.d++;
				}
				cpu.c += 2;
				break;
				
			case 0x14: // INC D
				doIncReg(&cpu.r.d);
				cpu.c += 1;
				break;
				
			case 0x15: // DEC D
				doDecReg(&cpu.r.d);
				cpu.c += 1;
				break;
				
			case 0x16: // LD D, n
				cpu.r.d = readByte(cpu.r.pc);
				cpu.r.pc++;
				cpu.c += 2;
				break;
				
			case 0x17: // RL A
				utmp8 = (cpu.r.f & F_CARRY);
				doRotateLeftWithCarry(&cpu.r.a);
				resetFlag(F_ZERO); // Always reset zero flag!
				cpu.c += 1;
				break;
			
			case 0x18: // JR n
				tmp8 = (int8_t)readByte(cpu.r.pc);
				cpu.r.pc++;
				cpu.r.pc += tmp8;
				cpu.c += 3;
				break;
				
			case 0x19: // ADD HL, DE
				doAddHL((cpu.r.d << 8) + cpu.r.e);
				cpu.c += 2;
				break;
				
			case 0x1A: // LD A, (DE)
				cpu.r.a = readByte((cpu.r.d << 8) + cpu.r.e);
				cpu.c += 2;
				break;
				
			case 0x1B: // DEC DE
				cpu.r.e--;
				if (cpu.r.e == 0xFF) {
					cpu.r.d--;
				}
				cpu.c += 2;
				break;
				
			case 0x1C: // INC E
				doIncReg(&cpu.r.e);
				cpu.c += 1;
				break;
				
			case 0x1D: // DEC E
				doDecReg(&cpu.r.e);
				cpu.c += 1;
				break;
				
			case 0x1E: // LD E, n
				cpu.r.e = readByte(cpu.r.pc);
				cpu.r.pc++;
				cpu.c += 2;
				break;
				
			case 0x1F: // RR a
				doRotateRightWithCarry(&cpu.r.a);
				resetFlag(F_ZERO);
				cpu.c += 1;
				break;
				
			case 0x20: // JR NZ, n
				tmp8 = (int8_t)readByte(cpu.r.pc);
				cpu.r.pc++;
				if (!getFlag(F_ZERO)) {
					cpu.r.pc += tmp8;
					cpu.c += 1;
				}
				cpu.c += 2;
				break;
				
			case 0x21: // LD HL, nn
				cpu.r.l = readByte(cpu.r.pc);
				cpu.r.h = readByte(cpu.r.pc+1);
				cpu.r.pc += 2;
				cpu.c += 3;
				break;
				
			case 0x22: // LDI (HL), A
				writeByte(cpu.r.a, (cpu.r.h << 8) + cpu.r.l);
				cpu.r.l++;
				if (cpu.r.l == 0x00) {
					cpu.r.h++;
				}
				cpu.c += 2;
				break;
				
			case 0x23: // INC HL
				cpu.r.l++;
				if (cpu.r.l == 0x00) {
					cpu.r.h++;
				}
				cpu.c += 2;
				break;
				
			case 0x24: // INC H
				doIncReg(&cpu.r.h);
				cpu.c += 1;
				break;
				
			case 0x25: // DEC H
				doDecReg(&cpu.r.h);
				cpu.c += 1;
				break;
				
			case 0x26: // LD H, n
				cpu.r.h = readByte(cpu.r.pc);
				cpu.r.pc++;
				cpu.c += 2;
				break;
				
			case 0x27: // DAA
				utmp16 = cpu.r.a;
				
				if (!getFlag(F_SUBSTRACT)) {
					if (getFlag(F_HCARRY) || (utmp16 & 0x0F) > 9) {
						utmp16 += 0x06;
					}

					if (getFlag(F_CARRY) || utmp16 > 0x9F) {
						utmp16 += 0x60;
					}
				} else {
					if (getFlag(F_HCARRY)) {
						utmp16 = (utmp16 - 6) & 0xFF;
					}

					if (getFlag(F_CARRY)) {
						utmp16 -= 0x60;
					}
				}

				resetFlag(F_HCARRY);

				if ((utmp16 & 0x100) == 0x100) {
					setFlag(F_CARRY);
				}

				utmp16 &= 0xFF;

				if (utmp16 == 0) {
					setFlag(F_ZERO);
				} else {
					resetFlag(F_ZERO);
				}

				cpu.r.a = utmp16;

				cpu.c += 1;
				break;
				
			case 0x28: // JR Z, n
				tmp8 = (int8_t)readByte(cpu.r.pc);
				cpu.r.pc++;
				if (getFlag(F_ZERO)) {
					cpu.r.pc += tmp8;
					cpu.c += 1;
				}
				cpu.c += 2;
				break;
				
			case 0x29: // ADD HL, HL
				doAddHL((cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;
				
			case 0x2A: // LDI A, (HL)
				cpu.r.a = readByte((cpu.r.h << 8) + cpu.r.l);
				cpu.r.l++;
				if (cpu.r.l == 0x00) {
					cpu.r.h++;
				}
				cpu.c += 2;
				break;
				
			case 0x2B: // DEC HL
				cpu.r.l--;
				if (cpu.r.l == 0xFF) {
					cpu.r.h--;
				}
				cpu.c += 2;
				break;
				
			case 0x2C: // INC L
				doIncReg(&cpu.r.l);
				cpu.c += 1;
				break;
				
			case 0x2D: // DEC L
				doDecReg(&cpu.r.l);
				cpu.c += 1;
				break;
				
			case 0x2E: // LD L, n
				cpu.r.l = readByte(cpu.r.pc);
				cpu.r.pc++;
				cpu.c += 2;
				break;
				
			case 0x2F: // CPL
				cpu.r.a = ~cpu.r.a;

				setFlag(F_SUBSTRACT);
				setFlag(F_HCARRY);

				cpu.c += 1;
				break;
				
			case 0x30: // JR NC, n
				tmp8 = (int8_t)readByte(cpu.r.pc);
				cpu.r.pc++;
				if (!getFlag(F_CARRY)) {
					cpu.r.pc += tmp8;
					cpu.c += 1;
				}
				cpu.c += 2;
				break;
				
			case 0x31: // LD SP, nn
				cpu.r.sp = readWord(cpu.r.pc);
				cpu.r.pc += 2;
				cpu.c += 3;
				break;
				
			case 0x32: // LDD (HL), A
				writeByte(cpu.r.a, (cpu.r.h << 8) + cpu.r.l);
				cpu.r.l--;
				if (cpu.r.l == 0xFF) {
					cpu.r.h--;
				}
				cpu.c += 2;
				break;
				
			case 0x33: // INC SP
				cpu.r.sp++;
				cpu.c += 2;
				break;
				
			case 0x34: // INC (HL)
				utmp8 = readByte((cpu.r.h << 8) + cpu.r.l);
				doIncReg(&utmp8);
				writeByte(utmp8, (cpu.r.h << 8) + cpu.r.l);
				cpu.c += 3;
				break;
				
			case 0x35: // DEC (HL)
				utmp8 = readByte((cpu.r.h << 8) + cpu.r.l);
				doDecReg(&utmp8);
				writeByte(utmp8, (cpu.r.h << 8) + cpu.r.l);
				cpu.c += 3;
				break;
				
			case 0x36: // LD (HL), n
				writeByte(readByte(cpu.r.pc), (cpu.r.h << 8) + cpu.r.l);
				cpu.r.pc++;
				cpu.c += 3;
				break;
				
			case 0x37: // SCF
				setFlag(F_CARRY);
				resetFlag(F_SUBSTRACT);
				resetFlag(F_HCARRY);
				cpu.c += 1;
				break;
				
			case 0x38: // JR C, n
				tmp8 = (int8_t)readByte(cpu.r.pc);
				cpu.r.pc++;
				if (getFlag(F_CARRY)) {
					cpu.r.pc += tmp8;
					cpu.c += 1;
				}
				cpu.c += 2;
				break;
				
			case 0x39: // ADD HL, SP
				doAddHL(cpu.r.sp);
				cpu.c += 2;
				break;
				
			case 0x3A: // LDD A, (HL)
				cpu.r.a = readByte((cpu.r.h << 8) + cpu.r.l);
				cpu.r.l--;
				if (cpu.r.l == 0xFF) {
					cpu.r.h--;
				}
				cpu.c += 2;
				break;
				
			case 0x3B: // DEC SP
				cpu.r.sp--;
				cpu.c += 2;
				break;
				
			case 0x3C: // INC A
				doIncReg(&cpu.r.a);
				cpu.c += 1;
				break;
				
			case 0x3D: // DEC A
				doDecReg(&cpu.r.a);
				cpu.c += 1;
				break;
				
			case 0x3E: // LD A, n
				cpu.r.a = readByte(cpu.r.pc);
				cpu.r.pc++;
				cpu.c += 2;
				break;
				
			case 0x3F: // CCF (actually toggles)
				resetFlag(F_SUBSTRACT);
				resetFlag(F_HCARRY);

				if (!getFlag(F_CARRY)) {
					setFlag(F_CARRY);
				} else {
					resetFlag(F_CARRY);
				}

				cpu.c += 1;
				break;
				
			case 0x40: // LD B, B
				// NOP
				cpu.c += 1;
				break;
				
			case 0x41: // LD B, C
				cpu.r.b = cpu.r.c;
				cpu.c += 1;
				break;
				
			case 0x42: // LD B, D
				cpu.r.b = cpu.r.d;
				cpu.c += 1;
				break;
				
			case 0x43: // LD B, E
				cpu.r.b = cpu.r.e;
				cpu.c += 1;
				break;
				
			case 0x44: // LD B, H
				cpu.r.b = cpu.r.h;
				cpu.c += 1;
				break;
				
			case 0x45: // LD B, L
				cpu.r.b = cpu.r.l;
				cpu.c += 1;
				break;
				
			case 0x46: // LD B, (HL)
				cpu.r.b = readByte((cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;
				
			case 0x47: // LD B, A
				cpu.r.b = cpu.r.a;
				cpu.c += 1;
				break;
				
			case 0x48: // LD C, B
				cpu.r.c = cpu.r.b;
				cpu.c += 1;
				break;
				
			case 0x49: // LD C, C
				// NOP
				cpu.c += 1;
				break;
				
			case 0x4A: // LD C, D
				cpu.r.c = cpu.r.d;
				cpu.c += 1;
				break;
				
			case 0x4B: // LD C, E
				cpu.r.c = cpu.r.e;
				cpu.c += 1;
				break;
				
			case 0x4C: // LD C, H
				cpu.r.c = cpu.r.h;
				cpu.c += 1;
				break;
				
			case 0x4D: // LD C, L
				cpu.r.c = cpu.r.l;
				cpu.c += 1;
				break;
				
			case 0x4E: // LD C, (HL)
				cpu.r.c = readByte((cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;
				
			case 0x4F: // LD C, A
				cpu.r.c = cpu.r.a;
				cpu.c += 1;
				break;
				
			case 0x50: // LD D, B
				cpu.r.d = cpu.r.b;
				cpu.c += 1;
				break;
				
			case 0x51: // LD D, C
				cpu.r.d = cpu.r.c;
				cpu.c += 1;
				break;
				
			case 0x52: // LD D, D
				// NOP
				cpu.c += 1;
				break;
				
			case 0x53: // LD D, E
				cpu.r.d = cpu.r.e;
				cpu.c += 1;
				break;
				
			case 0x54: // LD D, H
				cpu.r.d = cpu.r.h;
				cpu.c += 1;
				break;
				
			case 0x55: // LD D, L
				cpu.r.d = cpu.r.l;
				cpu.c += 1;
				break;
				
			case 0x56: // LD D, (HL)
				cpu.r.d = readByte((cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;
				
			case 0x57: // LD D, A
				cpu.r.d = cpu.r.a;
				cpu.c += 1;
				break;
				
			case 0x58: // LD E, B
				cpu.r.e = cpu.r.b;
				cpu.c += 1;
				break;
				
			case 0x59: // LD E, C
				cpu.r.e = cpu.r.c;
				cpu.c += 1;
				break;
				
			case 0x5A: // LD E, D
				cpu.r.e = cpu.r.d;
				cpu.c += 1;
				break;
				
			case 0x5B: // LD E, E
				// NOP
				cpu.c += 1;
				break;
				
			case 0x5C: // LD E, H
				cpu.r.e = cpu.r.h;
				cpu.c += 1;
				break;
				
			case 0x5D: // LD E, L
				cpu.r.e = cpu.r.l;
				cpu.c += 1;
				break;
				
			case 0x5E: // LD E, (HL)
				cpu.r.e = readByte((cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;
				
			case 0x5F: // LD E, A
				cpu.r.e = cpu.r.a;
				cpu.c += 1;
				break;
				
			case 0x60: // LD H, B
				cpu.r.h = cpu.r.b;
				cpu.c += 1;
				break;
				
			case 0x61: // LD H, C
				cpu.r.h = cpu.r.c;
				cpu.c += 1;
				break;
				
			case 0x62: // LD H, D
				cpu.r.h = cpu.r.d;
				cpu.c += 1;
				break;
				
			case 0x63: // LD H, E
				cpu.r.h = cpu.r.e;
				cpu.c += 1;
				break;
				
			case 0x64: // LD H, H
				// NOP
				cpu.c += 1;
				break;
				
			case 0x65: // LD H, L
				cpu.r.h = cpu.r.l;
				cpu.c += 1;
				break;
				
			case 0x66: // LD H, (HL)
				cpu.r.h = readByte((cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;
				
			case 0x67: // LD H, A
				cpu.r.h = cpu.r.a;
				cpu.c += 1;
				break;
				
			case 0x68: // LD L, B
				cpu.r.l = cpu.r.b;
				cpu.c += 1;
				break;
				
			case 0x69: // LD L, C
				cpu.r.l = cpu.r.c;
				cpu.c += 1;
				break;
				
			case 0x6A: // LD L, D
				cpu.r.l = cpu.r.d;
				cpu.c += 1;
				break;
				
			case 0x6B: // LD L, E
				cpu.r.l = cpu.r.e;
				cpu.c += 1;
				break;
				
			case 0x6C: // LD L, H
				cpu.r.l = cpu.r.h;
				cpu.c += 1;
				break;
				
			case 0x6D: // LD L, L
				// NOP
				cpu.c += 1;
				break;
				
			case 0x6E: // LD L, (HL)
				cpu.r.l = readByte((cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;
				
			case 0x6F: // LD L, A
				cpu.r.l = cpu.r.a;
				cpu.c += 1;
				break;
				
			case 0x70: // LD (HL), B
				writeByte(cpu.r.b, (cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;

			case 0x71: // LD (HL), C
				writeByte(cpu.r.c, (cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;

			case 0x72: // LD (HL), D
				writeByte(cpu.r.d, (cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;

			case 0x73: // LD (HL), E
				writeByte(cpu.r.e, (cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;

			case 0x74: // LD (HL), H
				writeByte(cpu.r.h, (cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;

			case 0x75: // LD (HL), L
				writeByte(cpu.r.l, (cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;

			case 0x76: // HALT
				// store interrupt flags
				cpu.oldIntFlags = cpu.intFlags;
				cpu.halted = 1;
				
				//cpu.intsOn = 1;
				printf("HALT instruction, intsOn=%d, IF=%02X\n", cpu.intsOn, cpu.intFlags);
				break;

			case 0x77: // LD (HL), A
				writeByte(cpu.r.a, (cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;

			case 0x78: // LD A, B
				cpu.r.a = cpu.r.b;
				cpu.c += 1;
				break;

			case 0x79: // LD A, C
				cpu.r.a = cpu.r.c;
				cpu.c += 1;
				break;

			case 0x7A: // LD A, D
				cpu.r.a = cpu.r.d;
				cpu.c += 1;
				break;

			case 0x7B: // LD A, E
				cpu.r.a = cpu.r.e;
				cpu.c += 1;
				break;

			case 0x7C: // LD A, H
				cpu.r.a = cpu.r.h;
				cpu.c += 1;
				break;

			case 0x7D: // LD A, L
				cpu.r.a = cpu.r.l;
				cpu.c += 1;
				break;

			case 0x7E: // LD A, (HL)
				cpu.r.a = readByte((cpu.r.h << 8) + cpu.r.l);
				cpu.c += 2;
				break;

			case 0x7F: // LD A, A
				// NOP
				cpu.c += 1;
				break;

			case 0x80: // ADD A, B
				doAddReg(&cpu.r.a, cpu.r.b);
				cpu.c += 1;
				break;

			case 0x81: // ADD A, C
				doAddReg(&cpu.r.a, cpu.r.c);
				cpu.c += 1;
				break;

			case 0x82: // ADD A, D
				doAddReg(&cpu.r.a, cpu.r.d);
				cpu.c += 1;
				break;

			case 0x83: // ADD A, E
				doAddReg(&cpu.r.a, cpu.r.e);
				cpu.c += 1;
				break;

			case 0x84: // ADD A, H
				doAddReg(&cpu.r.a, cpu.r.h);
				cpu.c += 1;
				break;

			case 0x85: // ADD A, L
				doAddReg(&cpu.r.a, cpu.r.l);
				cpu.c += 1;
				break;

			case 0x86: // ADD A, (HL)
				doAddReg(&cpu.r.a, readByte((cpu.r.h << 8) + cpu.r.l));
				cpu.c += 2;
				break;

			case 0x87: // ADD A, A
				doAddReg(&cpu.r.a, cpu.r.a);
				cpu.c += 1;
				break;

			case 0x88: // ADC A, B
				doAddRegWithCarry(&cpu.r.a, cpu.r.b);
				cpu.c += 1;
				break;

			case 0x89: // ADC A, C
				doAddRegWithCarry(&cpu.r.a, cpu.r.c);
				cpu.c += 1;
				break;

			case 0x8A: // ADC A, D
				doAddRegWithCarry(&cpu.r.a, cpu.r.d);
				cpu.c += 1;
				break;

			case 0x8B: // ADC A, E
				doAddRegWithCarry(&cpu.r.a, cpu.r.e);
				cpu.c += 1;
				break;

			case 0x8C: // ADC A, H
				doAddRegWithCarry(&cpu.r.a, cpu.r.h);
				cpu.c += 1;
				break;

			case 0x8D: // ADC A, L
				doAddRegWithCarry(&cpu.r.a, cpu.r.l);
				cpu.c += 1;
				break;

			case 0x8E: // ADC A, (HL)
				doAddRegWithCarry(&cpu.r.a, readByte((cpu.r.h << 8) + cpu.r.l));
				cpu.c += 2;
				break;

			case 0x8F: // ADC A, A
				doAddRegWithCarry(&cpu.r.a, cpu.r.a);
				cpu.c += 1;
				break;

			case 0x90: // SUB A, B
				doSubReg(&cpu.r.a, cpu.r.b);
				cpu.c += 1;
				break;

			case 0x91: // SUB A, C
				doSubReg(&cpu.r.a, cpu.r.c);
				cpu.c += 1;
				break;

			case 0x92: // SUB A, D
				doSubReg(&cpu.r.a, cpu.r.d);
				cpu.c += 1;
				break;

			case 0x93: // SUB A, E
				doSubReg(&cpu.r.a, cpu.r.e);
				cpu.c += 1;
				break;

			case 0x94: // SUB A, H
				doSubReg(&cpu.r.a, cpu.r.h);
				cpu.c += 1;
				break;

			case 0x95: // SUB A, L
				doSubReg(&cpu.r.a, cpu.r.l);
				cpu.c += 1;
				break;

			case 0x96: // SUB A, (HL)
				doSubReg(&cpu.r.a, readByte((cpu.r.h << 8) + cpu.r.l));
				cpu.c += 2;
				break;

			case 0x97: // SUB A, A
				doSubReg(&cpu.r.a, cpu.r.a);
				cpu.c += 1;
				break;

			case 0x98: // SBC A, B
				doSubRegWithCarry(&cpu.r.a, cpu.r.b);
				cpu.c += 1;
				break;

			case 0x99: // SBC A, C
				doSubRegWithCarry(&cpu.r.a, cpu.r.c);
				cpu.c += 1;
				break;

			case 0x9A: // SBC A, D
				doSubRegWithCarry(&cpu.r.a, cpu.r.d);
				cpu.c += 1;
				break;

			case 0x9B: // SBC A, E
				doSubRegWithCarry(&cpu.r.a, cpu.r.e);
				cpu.c += 1;
				break;

			case 0x9C: // SBC A, H
				doSubRegWithCarry(&cpu.r.a, cpu.r.h);
				cpu.c += 1;
				break;

			case 0x9D: // SBC A, L
				doSubRegWithCarry(&cpu.r.a, cpu.r.l);
				cpu.c += 1;
				break;

			case 0x9E: // SBC A, (HL)
				doSubRegWithCarry(&cpu.r.a, readByte((cpu.r.h << 8) + cpu.r.l));
				cpu.c += 2;
				break;

			case 0x9F: // SBC A, A
				doSubRegWithCarry(&cpu.r.a, cpu.r.a);
				cpu.c += 1;
				break;

			case 0xA0: // AND B
				doAndRegA(cpu.r.b);
				cpu.c += 1;
				break;

			case 0xA1: // AND C
				doAndRegA(cpu.r.c);
				cpu.c += 1;
				break;

			case 0xA2: // AND D
				doAndRegA(cpu.r.d);
				cpu.c += 1;
				break;

			case 0xA3: // AND E
				doAndRegA(cpu.r.e);
				cpu.c += 1;
				break;

			case 0xA4: // AND H
				doAndRegA(cpu.r.h);
				cpu.c += 1;
				break;

			case 0xA5: // AND L
				doAndRegA(cpu.r.l);
				cpu.c += 1;
				break;

			case 0xA6: // AND (HL)
				doAndRegA(readByte((cpu.r.h << 8) + cpu.r.l));
				cpu.c += 2;
				break;

			case 0xA7: // AND A
				doAndRegA(cpu.r.a);
				cpu.c += 1;
				break;

			case 0xA8: // XOR B
				doXorRegA(cpu.r.b);
				cpu.c += 1;
				break;

			case 0xA9: // XOR C
				doXorRegA(cpu.r.c);
				cpu.c += 1;
				break;

			case 0xAA: // XOR D
				doXorRegA(cpu.r.d);
				cpu.c += 1;
				break;

			case 0xAB: // XOR E
				doXorRegA(cpu.r.e);
				cpu.c += 1;
				break;

			case 0xAC: // XOR H
				doXorRegA(cpu.r.h);
				cpu.c += 1;
				break;

			case 0xAD: // XOR L
				doXorRegA(cpu.r.l);
				cpu.c += 1;
				break;

			case 0xAE: // XOR (HL)
				doXorRegA(readByte((cpu.r.h << 8) + cpu.r.l));
				cpu.c += 2;
				break;

			case 0xAF: // XOR A
				doXorRegA(cpu.r.a);
				cpu.c += 1;
				break;

			case 0xB0: // OR B
				doOrRegA(cpu.r.b);
				cpu.c += 1;
				break;

			case 0xB1: // OR C
				doOrRegA(cpu.r.c);
				cpu.c += 1;
				break;

			case 0xB2: // OR D
				doOrRegA(cpu.r.d);
				cpu.c += 1;
				break;

			case 0xB3: // OR E
				doOrRegA(cpu.r.e);
				cpu.c += 1;
				break;

			case 0xB4: // OR H
				doOrRegA(cpu.r.h);
				cpu.c += 1;
				break;

			case 0xB5: // OR L
				doOrRegA(cpu.r.l);
				cpu.c += 1;
				break;

			case 0xB6: // OR (HL)
				doOrRegA(readByte((cpu.r.h << 8) + cpu.r.l));
				cpu.c += 2;
				break;

			case 0xB7: // OR A
				doOrRegA(cpu.r.a);
				cpu.c += 1;
				break;

			case 0xB8: // CP B
				doCpRegA(cpu.r.b);
				cpu.c += 1;
				break;

			case 0xB9: // CP C
				doCpRegA(cpu.r.c);
				cpu.c += 1;
				break;

			case 0xBA: // CP D
				doCpRegA(cpu.r.d);
				cpu.c += 1;
				break;

			case 0xBB: // CP E
				doCpRegA(cpu.r.e);
				cpu.c += 1;
				break;

			case 0xBC: // CP H
				doCpRegA(cpu.r.h);
				cpu.c += 1;
				break;

			case 0xBD: // CP L
				doCpRegA(cpu.r.l);
				cpu.c += 1;
				break;

			case 0xBE: // CP (HL)
				doCpRegA(readByte((cpu.r.h << 8) + cpu.r.l));
				cpu.c += 2;
				break;

			case 0xBF: // CP A
				doCpRegA(cpu.r.a);
				cpu.c += 1;
				break;

			case 0xC0: // RET NZ
				if (!getFlag(F_ZERO)) {
					cpu.r.pc = readWord(cpu.r.sp);
					cpu.r.sp += 2;
					cpu.c += 3;
				}
				cpu.c += 2;
				break;

			case 0xC1: // POP BC
				cpu.r.c = readByte(cpu.r.sp);
				cpu.r.sp++;
				cpu.r.b = readByte(cpu.r.sp);
				cpu.r.sp++;
				cpu.c += 3;
				break;

			case 0xC2: // JP NZ, nn
				if (!getFlag(F_ZERO)) {
					cpu.r.pc = readWord(cpu.r.pc);
					cpu.c += 1;
				} else {
					cpu.r.pc += 2;
				}
				cpu.c += 3;
				break;

			case 0xC3: // JP nn
				cpu.r.pc = readWord(cpu.r.pc);
				cpu.c += 4;
				break;

			case 0xC4: // CALL NZ, nn
				if (!getFlag(F_ZERO)) {
					cpu.r.sp -= 2;
					writeWord(cpu.r.pc + 2, cpu.r.sp);
					cpu.r.pc = readWord(cpu.r.pc);
					cpu.c += 3;
				} else {
					cpu.r.pc += 2;
				}
				cpu.c += 3;
				break;

			case 0xC5: // PUSH BC
				cpu.r.sp--;
				writeByte(cpu.r.b, cpu.r.sp);
				cpu.r.sp--;
				writeByte(cpu.r.c, cpu.r.sp);
				cpu.c += 4;
				break;

			case 0xC6: // ADD A, n
				utmp8 = readByte(cpu.r.pc);
				cpu.r.pc++;
				doAddReg(&cpu.r.a, utmp8);
				cpu.c += 2;
				break;

			case 0xC7: // RST 00
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc, cpu.r.sp);
				cpu.r.pc = 0x00;
				cpu.c += 4;
				break;

			case 0xC8: // RET Z
				cpu.c += 1;
				if(getFlag(F_ZERO)) {
					cpu.r.pc = readWord(cpu.r.sp);
					cpu.r.sp += 2;
					cpu.c += 3;
				}
				cpu.c += 2;
				break;

			case 0xC9: // RET
				cpu.r.pc = readWord(cpu.r.sp);
				cpu.r.sp += 2;
				cpu.c += 4;
				break;

			case 0xCA: // JP Z, nn
				if (getFlag(F_ZERO)) {
					cpu.r.pc = readWord(cpu.r.pc);
					cpu.c += 1;
				} else {
					cpu.r.pc += 2;
				}
				cpu.c += 3;
				break;

			case 0xCB: // Extra instructions
				utmp8 = readByte(cpu.r.pc);
				cpu.r.pc++;
				doExtraOP(utmp8);
				break;

			case 0xCC: // CALL Z, nn
				if(getFlag(F_ZERO)) {
					cpu.r.sp -= 2;
					writeWord(cpu.r.pc + 2, cpu.r.sp);
					cpu.r.pc = readWord(cpu.r.pc);
					cpu.c += 3;
				} else {
					cpu.r.pc += 2;
				}
				cpu.c += 3;
				break;

			case 0xCD: // CALL nn
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc + 2, cpu.r.sp);
				cpu.r.pc = readWord(cpu.r.pc);
				cpu.c += 6;
				break;

			case 0xCE: // ADC A, n
				doAddRegWithCarry(&cpu.r.a, readByte(cpu.r.pc));
				cpu.r.pc++;
				cpu.c += 2;
				break;

			case 0xCF: // RST 08
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc, cpu.r.sp);
				cpu.r.pc = 0x08;
				cpu.c += 4;
				break;

			case 0xD0: // RET NC
				cpu.c += 1;
				if (!getFlag(F_CARRY)) {
					cpu.r.pc = readWord(cpu.r.sp);
					cpu.r.sp += 2;
					cpu.c += 3;
				}
				cpu.c += 2;
				break;

			case 0xD1: // POP DE
				cpu.r.e = readByte(cpu.r.sp);
				cpu.r.sp++;
				cpu.r.d = readByte(cpu.r.sp);
				cpu.r.sp++;
				cpu.c += 3;
				break;

			case 0xD2: // JP NC, nn
				if (!getFlag(F_CARRY)) {
					cpu.r.pc = readWord(cpu.r.pc);
					cpu.c += 1;
				} else {
					cpu.r.pc += 2;
				}
				cpu.c += 3;
				break;

			case 0xD3: // unimplemented
				doOpcodeUNIMP(cpu);
				break;

			case 0xD4: // CALL NC, nn
				if (!getFlag(F_CARRY)) {
					cpu.r.sp -= 2;
					writeWord(cpu.r.pc + 2, cpu.r.sp);
					cpu.r.pc = readWord(cpu.r.pc);
					cpu.c += 3;
				} else {
					cpu.r.pc += 2;
				}
				cpu.c += 3;
				break;

			case 0xD5: // PUSH DE
				cpu.r.sp--;
				writeByte(cpu.r.d, cpu.r.sp);
				cpu.r.sp--;
				writeByte(cpu.r.e, cpu.r.sp);
				cpu.c += 4;
				break;

			case 0xD6: // SUB A, n
				doSubReg(&cpu.r.a, readByte(cpu.r.pc));
				cpu.r.pc++;
				cpu.c += 2;
				break;

			case 0xD7: // RST 10
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc, cpu.r.sp);
				cpu.r.pc = 0x10;
				cpu.c += 4;
				break;

			case 0xD8: // RET C
				cpu.c += 1;
				if (getFlag(F_CARRY)) {
					cpu.r.pc = readWord(cpu.r.sp);
					cpu.r.sp += 2;
					cpu.c += 3;
				}
				cpu.c += 2;
				break;

			case 0xD9: // RETI
				cpu.intsOn = 1;
				//printf("reti, intsOn = 1\n");
				
				cpu.r.pc = readWord(cpu.r.sp);
				cpu.r.sp += 2;
				
				cpu.c += 4;
				break;

			case 0xDA: // JP C, nn
				if (getFlag(F_CARRY)) {
					cpu.r.pc = readWord(cpu.r.pc);
					cpu.c += 1;
				} else {
					cpu.r.pc += 2;
				}
				cpu.c += 3;
				break;

			case 0xDB: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xDC: // CALL C, nn
				if (getFlag(F_CARRY)) {
					cpu.r.sp -= 2;
					writeWord(cpu.r.pc + 2, cpu.r.sp);
					cpu.r.pc = readWord(cpu.r.pc);
					cpu.c += 3;
				} else {
					cpu.r.pc += 2;
				}
				cpu.c += 3;
				break;

			case 0xDD: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xDE: // SBC A, n
				doSubRegWithCarry(&cpu.r.a, readByte(cpu.r.pc));
				cpu.r.pc++;
				cpu.c += 2;
				break;

			case 0xDF: // RST 18
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc, cpu.r.sp);
				cpu.r.pc = 0x18;
				cpu.c += 4;
				break;

			case 0xE0: // LDH (n), A
				writeByte(cpu.r.a, 0xFF00 + readByte(cpu.r.pc));
				cpu.r.pc++;
				cpu.c += 3;
				break;

			case 0xE1: // POP HL
				cpu.r.l = readByte(cpu.r.sp);
				cpu.r.sp++;
				cpu.r.h = readByte(cpu.r.sp);
				cpu.r.sp++;
				cpu.c += 3;
				break;

			case 0xE2: // LDH (C), A
				writeByte(cpu.r.a, 0xFF00 + cpu.r.c);
				cpu.c += 2;
				break;

			case 0xE3: // unimplemented
			case 0xE4: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xE5: // PUSH HL
				cpu.r.sp--;
				writeByte(cpu.r.h, cpu.r.sp);
				cpu.r.sp--;
				writeByte(cpu.r.l, cpu.r.sp);
				cpu.c += 4;
				break;

			case 0xE6: // AND n
				doAndRegA(readByte(cpu.r.pc));
				cpu.r.pc++;
				cpu.c += 2;
				break;

			case 0xE7: // RST 20
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc, cpu.r.sp);
				cpu.r.pc = 0x20;
				cpu.c += 4;
				break;

			case 0xE8: // ADD SP, n
				tmp8 = (int8_t)readByte(cpu.r.pc);
				cpu.r.pc++;

				utmp16 = cpu.r.sp + tmp8;
				if ((utmp16 & 0xFF) < (cpu.r.sp & 0xFF)) {
					setFlag(F_CARRY);
				} else {
					resetFlag(F_CARRY);
				}

				if ((utmp16 & 0x0F) < (cpu.r.sp & 0x0F)) {
					setFlag(F_HCARRY);
				} else {
					resetFlag(F_HCARRY);
				}

				cpu.r.sp = utmp16;
				resetFlag(F_ZERO);
				resetFlag(F_SUBSTRACT);

				cpu.c += 4;
				break;

			case 0xE9: // JP (HL)
				cpu.r.pc = (cpu.r.h << 8) + cpu.r.l;
				cpu.c += 1;
				break;

			case 0xEA: // LD (nn), A
				writeByte(cpu.r.a, readWord(cpu.r.pc));
				cpu.r.pc += 2;
				cpu.c += 4;
				break;

			case 0xEB: // unimplemented
			case 0xEC: // unimplemented
			case 0xED: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xEE: // XOR n
				doXorRegA(readByte(cpu.r.pc));
				cpu.r.pc++;
				cpu.c += 2;
				break;

			case 0xEF: // RST 28
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc, cpu.r.sp);
				cpu.r.pc = 0x28;
				cpu.c += 4;
				break;

			case 0xF0: // LDH A, (n)
				cpu.r.a = readByte(0xFF00 + readByte(cpu.r.pc));
				cpu.r.pc++;
				cpu.c += 3;
				break;

			case 0xF1: // POP AF
				cpu.r.f = readByte(cpu.r.sp) & 0xF0;
				cpu.r.sp++;
				cpu.r.a = readByte(cpu.r.sp);
				cpu.r.sp++;
				cpu.c += 3;
				break;

			case 0xF2: // LDH a, (C)?
				cpu.r.a = readByte(0xFF00 + cpu.r.c);
				cpu.c += 2;
				break;

			case 0xF3: // DI
				cpu.intsOn = 0;
				//printf("DI, intsOn = 0\n");
				cpu.c += 1;
				break;

			case 0xF4: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xF5: // PUSH AF
				cpu.r.sp--;
				writeByte(cpu.r.a, cpu.r.sp);
				cpu.r.sp--;
				writeByte(cpu.r.f, cpu.r.sp);
				cpu.c += 4;
				break;

			case 0xF6: // OR n
				doOrRegA(readByte(cpu.r.pc));
				cpu.r.pc++;
				cpu.c += 2;
				break;

			case 0xF7: // RST 30
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc, cpu.r.sp);
				cpu.r.pc = 0x30;
				cpu.c += 4;
				break;

			case 0xF8: // LDHL SP, d
				tmp8 = (int8_t)readByte(cpu.r.pc);
				cpu.r.pc++;
				utmp16 = (cpu.r.sp + tmp8) & 0xFFFF;

				if ((utmp16 & 0x0F) < (cpu.r.sp & 0x0F)) {
					setFlag(F_HCARRY);
				} else {
					resetFlag(F_HCARRY);
				}

				if ((utmp16 & 0xFF) < (cpu.r.sp & 0xFF)) {
					setFlag(F_CARRY);
				} else {
					resetFlag(F_CARRY);
				}

				resetFlag(F_ZERO);
				resetFlag(F_SUBSTRACT);

				cpu.r.h = utmp16 >> 8;
				cpu.r.l = utmp16 & 0xFF;

				cpu.c += 3;
				break;

			case 0xF9: // LD SP, HL
				cpu.r.sp = (cpu.r.h << 8) + cpu.r.l;
				cpu.c += 2;
				break;

			case 0xFA: // LD A, (nn)
				cpu.r.a = readByte(readWord(cpu.r.pc));
				cpu.r.pc += 2;
				cpu.c += 4;
				break;

			case 0xFB: // EI
				cpu.intsOn = 1;
				//printf("EI, intsOn = 1\n");
				cpu.c += 1;
				break;

			case 0xFC: // unimplemented
			case 0xFD: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xFE: // CP n
				doCpRegA(readByte(cpu.r.pc));
				cpu.r.pc++;
				cpu.c += 2;
				break;

			case 0xFF: // RST 38
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc, cpu.r.sp);
				cpu.r.pc = 0x38;
				cpu.c += 4;
				break;
				
			default:
				printf("There's a glitch in the matrix, this shouldn't happen.\n");
				return 0;
		}
	} else {
		// IF has changed, stop halting
		if (cpu.intFlags != cpu.oldIntFlags) {
			cpu.halted = 0;
		}

		// still increase clock
		cpu.c += 1;
	}

	handleTimers();

	cpu.dc = cpu.c - cpu.dc; // delta cycles, TODO: after interrupts?

	//printRegisters();
	
	return 1;
}
