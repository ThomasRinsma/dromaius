#include <cstdlib>
#include <cstdio>
#include <iostream>
#include "dromaius.h"

void CPU::initialize()
{
	r.a = 0x01;
	r.b = 0x00;
	r.c = 0x13;
	r.d = 0x00;
	r.e = 0xD8;
	r.h = 0x01;
	r.l = 0x4D;
	r.f = 0xB0;//0;
	r.sp = 0xFFFE;
	
	// Jump over bios
	r.pc = 0x0100;
	callStackPush(0x0000, r.pc);
	
	intsOn = false;//1;
	intFlags = 0;
	ints = 0;

	timer.div = 0;
	timer.tima = 0;
	timer.tma = 0;
	timer.tac = 0;

	timer.cycleCount = 0;
	timer.cycleCountDiv = 0;
	timer.maxCount[0] = 256;
	timer.maxCount[1] = 4;
	timer.maxCount[2] = 16;
	timer.maxCount[3] = 64;

	halted = false;
	fastForward = false;
	stepMode = false;
	stepInst = false;
	stepFrame = false;

	c = 0;
}

void CPU::setFlag(Flag flag)
{
	r.f |= flag;
}

void CPU::resetFlag(Flag flag)
{
	r.f &= ~flag;
}

int CPU::getFlag(Flag flag)
{
	return (r.f & flag) ? 1 : 0;
}

void CPU::doIncReg(uint8_t *regp)
{
	if (*regp == 0xFF) {
		*regp = 0;
		setFlag(Flag::ZERO);
	} else {
		++(*regp);
		resetFlag(Flag::ZERO);
	}

	if ((*regp & 0x0F) == 0) {
		setFlag(Flag::HCARRY);
	} else {
		resetFlag(Flag::HCARRY);
	}
	
	resetFlag(Flag::SUBTRACT);
}

void CPU::doDecReg(uint8_t *regp)
{
	uint8_t tmp = *regp;

	if (*regp == 0) {
		*regp = 0xFF;
	} else {
		--(*regp);
	}

	if ((tmp & 0x0F) < (*regp & 0x0F)) {
		setFlag(Flag::HCARRY);
	} else {
		resetFlag(Flag::HCARRY);
	}

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	setFlag(Flag::SUBTRACT);
}

void CPU::doAddReg(uint8_t *regp, uint8_t val)
{
	if ((*regp & 0x0F) + (val & 0x0F) > 0x0F) {
		setFlag(Flag::HCARRY);
	} else {
		resetFlag(Flag::HCARRY);
	}

	if ((*regp + val) > 0xFF) {
		setFlag(Flag::CARRY);
	} else {
		resetFlag(Flag::CARRY);
	}

	*regp = ((*regp + val) & 0xFF);

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::SUBTRACT);
}

void CPU::doAddRegWithCarry(uint8_t *regp, uint8_t val)
{
	uint8_t carry = getFlag(Flag::CARRY);
	uint16_t tmp = *regp + val + carry;

	if (tmp > 0xFF) {
		setFlag(Flag::CARRY);
	} else {
		resetFlag(Flag::CARRY);
	}

	if (((*regp & 0x0F) + (val & 0x0F) + carry) > 0x0F) {
		setFlag(Flag::HCARRY);
	} else {
		resetFlag(Flag::HCARRY);
	}

	*regp = tmp & 0xFF;

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::SUBTRACT);
}

void CPU::doSubReg(uint8_t *regp, uint8_t val)
{
	if (*regp == val) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	if ((*regp & 0x0F) < (val & 0x0F)) {
		setFlag(Flag::HCARRY);
	} else {
		resetFlag(Flag::HCARRY);
	}

	if (*regp < val) {
		*regp = (0x100 - (val - *regp));
		setFlag(Flag::CARRY);
	} else {
		*regp -= val;
		resetFlag(Flag::CARRY);
	}

	setFlag(Flag::SUBTRACT);
}

void CPU::doSubRegWithCarry(uint8_t *regp, uint8_t val)
{
	uint8_t carry = getFlag(Flag::CARRY);
	int16_t tmp = *regp - val - carry;

	if (tmp < 0) {
		tmp += 0x100;
		setFlag(Flag::CARRY);
	} else {
		resetFlag(Flag::CARRY);
	}

	if (((*regp & 0x0F) - (val & 0x0F) - carry) < 0) {
		setFlag(Flag::HCARRY);
	} else {
		resetFlag(Flag::HCARRY);
	}

	*regp = tmp;

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	setFlag(Flag::SUBTRACT);
}

void CPU::doAddHL(uint16_t val)
{
	uint32_t tmp = (r.h << 8) + r.l;

	if ((tmp & 0xFFF) + (val & 0xFFF) > 0xFFF) {
		setFlag(Flag::HCARRY);
	} else {
		resetFlag(Flag::HCARRY);
	}

	if ((tmp + val) > 0xFFFF) {
		setFlag(Flag::CARRY);
	} else {
		resetFlag(Flag::CARRY);
	}

	tmp = ((tmp + val) & 0xFFFF);

	r.h = tmp >> 8;
	r.l = tmp & 0xFF;

	resetFlag(Flag::SUBTRACT);
}

void CPU::doRotateLeftWithCarry(uint8_t *regp)
{
	uint8_t tmp = (*regp & 0x80);
	*regp <<= 1;

	if (getFlag(Flag::CARRY)) {
		*regp |= 0x01;
	}

	if (tmp == 0) {
		resetFlag(Flag::CARRY);
	} else {
		setFlag(Flag::CARRY);
	}

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::SUBTRACT);
	resetFlag(Flag::HCARRY);
}

void CPU::doRotateRightWithCarry(uint8_t *regp)
{
	uint8_t tmp = (*regp & 0x01);
	*regp >>= 1;

	if (getFlag(Flag::CARRY)) {
		*regp |= 0x80;
	}

	if (tmp == 0) {
		resetFlag(Flag::CARRY);
	} else {
		setFlag(Flag::CARRY);
	}

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::SUBTRACT);
	resetFlag(Flag::HCARRY);
}

void CPU::doRotateLeft(uint8_t *regp)
{
	if ((*regp & 0x80) == 0) {
		resetFlag(Flag::CARRY);
	} else {
		setFlag(Flag::CARRY);
	}

	*regp <<= 1;
	if (!getFlag(Flag::CARRY)) {
		*regp &= 0xFE;
	} else {
		*regp |= 0x01;
	}

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::HCARRY);
	resetFlag(Flag::SUBTRACT);
}

void CPU::doRotateRight(uint8_t *regp)
{
	if ((*regp & 0x01) == 0) {
		resetFlag(Flag::CARRY);
	} else {
		setFlag(Flag::CARRY);
	}

	*regp >>= 1;
	if (!getFlag(Flag::CARRY)) {
		*regp &= 0x7F;
	} else {
		*regp |= 0x80;
	}

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::HCARRY);
	resetFlag(Flag::SUBTRACT);
}

void CPU::doShiftLeft(uint8_t *regp)
{
	if ((*regp & 0x80) == 0) {
		resetFlag(Flag::CARRY);
	} else {
		setFlag(Flag::CARRY);
	}

	*regp <<= 1;

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::HCARRY);
	resetFlag(Flag::SUBTRACT);
}

void CPU::doShiftRight(uint8_t *regp)
{
	if ((*regp & 0x01) == 0) {
		resetFlag(Flag::CARRY);
	} else {
		setFlag(Flag::CARRY);
	}

	*regp >>= 1;
	if ((*regp & 0x40) == 0) {
		*regp &= 0x7F;
	} else {
		*regp |= 0x80;
	}

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::HCARRY);
	resetFlag(Flag::SUBTRACT);
}

void CPU::doShiftRightL(uint8_t *regp)
{
	if ((*regp & 0x01) == 0) {
		resetFlag(Flag::CARRY);
	} else {
		setFlag(Flag::CARRY);
	}

	*regp >>= 1;
	*regp &= 0x7F;

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::SUBTRACT);
	resetFlag(Flag::HCARRY);
}

void CPU::doSwapNibbles(uint8_t *regp)
{
	*regp = ((*regp & 0xF0) >> 4 | (*regp & 0x0F) << 4);

	if (*regp == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::HCARRY);
	resetFlag(Flag::SUBTRACT);
	resetFlag(Flag::CARRY);
}

void CPU::doBit(uint8_t *regp, uint8_t bit)
{
	if ((*regp & (0x01 << bit)) == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::SUBTRACT);
	setFlag(Flag::HCARRY);
}

void CPU::doRes(uint8_t *regp, uint8_t bit)
{
	*regp &= ~(0x01 << bit);
}

void CPU::doSet(uint8_t *regp, uint8_t bit)
{
	*regp |= (0x01 << bit);
}


void CPU::doAndRegA(uint8_t val)
{
	r.a = r.a & val;

	if (r.a == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::SUBTRACT);
	resetFlag(Flag::CARRY);
	setFlag(Flag::HCARRY); // TODO: is this right?
}

void CPU::doXorRegA(uint8_t val)
{
	r.a = r.a ^ val;

	if (r.a == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::SUBTRACT);
	resetFlag(Flag::CARRY);
	resetFlag(Flag::HCARRY);
}

void CPU::doOrRegA(uint8_t val)
{
	r.a = r.a | val;

	if (r.a == 0) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	resetFlag(Flag::SUBTRACT);
	resetFlag(Flag::CARRY);
	resetFlag(Flag::HCARRY);
}

void CPU::doCpRegA(uint8_t val)
{
	if ((r.a & 0x0F) < (val & 0x0F)) {
		setFlag(Flag::HCARRY);
	} else {
		resetFlag(Flag::HCARRY);
	}

	if (r.a < val) {
		setFlag(Flag::CARRY);
	} else {
		resetFlag(Flag::CARRY);
	}

	if (r.a == val) {
		setFlag(Flag::ZERO);
	} else {
		resetFlag(Flag::ZERO);
	}

	setFlag(Flag::SUBTRACT);
}

void CPU::doOpcodeUNIMP()
{
	printf("Unimplemented instruction (0x%02X) at 0x%04X.\n", emu->memory.readByte(r.pc-1), r.pc-1);
	//exit(1);
}

int lastInst;
void CPU::printRegisters()
{
	char instStr[100];
	instructionToString(r.pc, instStr);
	printf("%s\n", instStr);
	printf("  {rombank=%d,rambank=%d,a=%02X,f=%02X,b=%02X,c=%02X,d=%02X,e=%02X,h=%02X,l=%02X,pc=%04X,sp=%04X,inst=%02X}\n",
		emu->memory.romBank, emu->memory.ramBank, r.a, r.f, r.b, r.c, r.d, r.e, r.h, r.l, r.pc, r.sp, lastInst);
}

uint8_t *CPU::numToRegPtr(uint8_t num)
{
	switch (num % 8) {
		case 0: return &r.b;
		case 1: return &r.c;
		case 2: return &r.d;
		case 3: return &r.e;
		case 4: return &r.h;
		case 5: return &r.l;
		case 7: return &r.a;
		default: return NULL;
	}
}

void CPU::doExtraOP(uint8_t op)
{
	uint8_t *regp;
	uint8_t tmp;
	uint8_t bitnr;

	// RLC
	if (op <= 0x07) {
		if ((op & 0x07) == 0x06) {
			tmp = emu->memory.readByte((r.h << 8) + r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doRotateLeft(regp);

		if ((op & 0x07) == 0x06) {
			emu->memory.writeByte(tmp, (r.h << 8) + r.l);
			c += 2;
		}

		c += 2;
	}
	// RRC
	else if (op > 0x07 && op <= 0x0F) {
		if ((op & 0x07) == 0x06) {
			tmp = emu->memory.readByte((r.h << 8) + r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doRotateRight(regp);

		if ((op & 0x07) == 0x06) {
			emu->memory.writeByte(tmp, (r.h << 8) + r.l);
			c += 2;
		}

		c += 2;
	}
	// RL
	else if (op > 0x0F && op <= 0x17) {
		if ((op & 0x07) == 0x06) {
			tmp = emu->memory.readByte((r.h << 8) + r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doRotateLeftWithCarry(regp);

		if ((op & 0x07) == 0x06) {
			emu->memory.writeByte(tmp, (r.h << 8) + r.l);
			c += 2;
		}

		c += 2;
	}
	// RR
	else if (op > 0x17 && op <= 0x1F) {
		if ((op & 0x07) == 0x06) {
			tmp = emu->memory.readByte((r.h << 8) + r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doRotateRightWithCarry(regp);

		if ((op & 0x07) == 0x06) {
			emu->memory.writeByte(tmp, (r.h << 8) + r.l);
			c += 2;
		}

		c += 2;
	}
	// SLA
	else if (op > 0x1F && op <= 0x27) {
		if ((op & 0x07) == 0x06) {
			tmp = emu->memory.readByte((r.h << 8) + r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doShiftLeft(regp);

		if ((op & 0x07) == 0x06) {
			emu->memory.writeByte(tmp, (r.h << 8) + r.l);
			c += 2;
		}

		c += 2;
	}
	// SRA
	else if (op > 0x27 && op <= 0x2F) {
		if ((op & 0x07) == 0x06) {
			tmp = emu->memory.readByte((r.h << 8) + r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doShiftRight(regp);

		if ((op & 0x07) == 0x06) {
			emu->memory.writeByte(tmp, (r.h << 8) + r.l);
			c += 2;
		}

		c += 2;
	}
	// SWAP
	else if (op > 0x2F && op <= 0x37) {
		if ((op & 0x07) == 0x06) {
			tmp = emu->memory.readByte((r.h << 8) + r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doSwapNibbles(regp);

		if ((op & 0x07) == 0x06) {
			emu->memory.writeByte(tmp, (r.h << 8) + r.l);
			c += 2;
		}

		c += 2;
	}
	// SRL
	else if (op > 0x37 && op <= 0x3F) {
		if ((op & 0x07) == 0x06) {
			tmp = emu->memory.readByte((r.h << 8) + r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doShiftRightL(regp);

		if ((op & 0x07) == 0x06) {
			emu->memory.writeByte(tmp, (r.h << 8) + r.l);
			c += 2;
		}

		c += 2;
	}
	// BIT
	else if (op > 0x3F && op <= 0x7F) {
		bitnr = (op - 0x40) / 8;

		if ((op & 0x07) == 0x06) {
			tmp = emu->memory.readByte((r.h << 8) + r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doBit(regp, bitnr);

		if ((op & 0x07) == 0x06) {
			emu->memory.writeByte(tmp, (r.h << 8) + r.l);
			c += 1;
		}

		c += 2;
	}
	// RES
	else if (op > 0x7F && op <= 0xBF) {
		bitnr = (op - 0x80) / 8;

		if ((op & 0x07) == 0x06) {
			tmp = emu->memory.readByte((r.h << 8) + r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doRes(regp, bitnr);

		if ((op & 0x07) == 0x06) {
			emu->memory.writeByte(tmp, (r.h << 8) + r.l);
			c += 2;
		}

		c += 2;
	}
	// SET
	else {
		bitnr = (op - 0xC0) / 8;

		if ((op & 0x07) == 0x06) {
			tmp = emu->memory.readByte((r.h << 8) + r.l);
			regp = &tmp;
		} else {
			regp = numToRegPtr(op & 0x07);
		}

		doSet(regp, bitnr);

		if ((op & 0x07) == 0x06) {
			emu->memory.writeByte(tmp, (r.h << 8) + r.l);
			c += 2;
		}

		c += 2;
	}
}


void CPU::handleTimers()
{
	// Timer
	timer.cycleCountDiv += c - dc;
	timer.cycleCount += c - dc;

	// Divider always counts, independent of TAC
	timer.div++;

	if (timer.tac & 0x04) { // started
		if (timer.cycleCountDiv >= 64) {
			timer.cycleCountDiv -= 64;

			//printf("div = %d.\n", timer.div);
		}
		if (timer.cycleCount >= timer.maxCount[timer.tac & 0x03]) {
			timer.cycleCount -= timer.maxCount[timer.tac & 0x03];

			timer.tima++;
			//printf("tima = %d.\n", timer.tima);
			if (timer.tima == 0) {
				timer.tima = timer.tma;

				intFlags |= Int::TIMER;
				//printf("tima = tma = %d. intsOn=%d, ints=%d, intFlags=%d (int CPU::flag set)\n", 
				//	timer.tima, intsOn, ints, intFlags);
			}
		}
	}
}

void CPU::callStackPush(uint16_t oldpc, uint16_t pc)
{
	//for(int i = 0; i < callStackDepth+1; ++i) printf("--");
	//printf("push: %04X -> %04X\n", oldpc, pc);
	// Just wrap if the callstack becomes really high..
	if (callStackDepth == CPU_CALL_STACK_SIZE - 1) {
		callStackDepth = 0;
		printf("callstack overflow\n");
	}
	callStack[callStackDepth++] = pc;
}

void CPU::callStackPop(uint16_t oldpc, uint16_t pc)
{
	//for(int i = 0; i < callStackDepth; ++i) printf("--");
	//printf("pop: %04X -> %04X\n", oldpc, pc);
	callStackDepth--;

	if (callStackDepth < 0) {
		callStackDepth = 0;
		//printf("callstack underflow\n");
	}
}

void CPU::handleInterrupts()
{
	// Interrupts
	if (intsOn and (ints & intFlags)) {
		uint8_t interrupts = intFlags & ints; // mask enabled interrupt(s)
		intsOn = false;

		//if (halted)
		//{
		//	printf("halted was 1\n");
		//	halted = 0;
		//}
		
		// Push PC;
		r.sp -= 2;
		emu->memory.writeWord(r.pc, r.sp);

		if (interrupts & Int::VBLANK) {
			intFlags &= ~Int::VBLANK;
			r.pc = 0x40;
		}
		else if (interrupts & Int::LCDSTAT) {
			intFlags &= ~Int::LCDSTAT;
			r.pc = 0x48;
		}
		else if (interrupts & Int::TIMER) {
			printf("timer interrupt!\n");
			intFlags &= ~Int::TIMER;
			r.pc = 0x50;
		}
		else if (interrupts & Int::SERIAL) {
			intFlags &= ~Int::SERIAL;
			r.pc = 0x58;
		}
		else if (interrupts & Int::JOYPAD) {
			intFlags &= ~Int::JOYPAD;
			r.pc = 0x60;
		}	

		c += 8;
	}
}


int CPU::executeInstruction()
{
	uint8_t inst;
	uint32_t utmp32, utmp32_2;
	int16_t tmp16, tmp16_2;
	uint16_t utmp16, utmp16_2;
	int8_t tmp8, tmp8_2;
	uint8_t utmp8, utmp8_2;
	uint16_t oldpc;
	
	dc = c;

	handleInterrupts();

	if (emu->settings.debug) {
		// printRegisters();

		// Print trace of executed symbols
		std::string symbol = emu->memory.getSymbolFromAddress(emu->memory.romBank, r.pc);
		if (not symbol.empty()) {
			printf("hit symbol: %s\n", symbol.c_str());
		}
	}

	if (not halted) {
		inst = emu->memory.readByte(r.pc);
		lastInst = inst;

		r.pc++;
		switch (inst) {
			case 0x00: // NOP
				c += 1;
				break;
				
			case 0x01: // LD BC, nn
				r.c = emu->memory.readByte(r.pc);
				r.b = emu->memory.readByte(r.pc + 1);
				r.pc += 2;
				c += 3;
				break;
				
			case 0x02: // LD (BC), A
				emu->memory.writeByte(r.a, (r.b << 8) + r.c);
				c += 2;
				break;
				
			case 0x03: // INC BC
				r.c++;
				if (not r.c) {
					r.b++;
				}
				c += 2;
				break;
				
			case 0x04: // INC B
				doIncReg(&r.b);
				c += 1;
				break;
				
			case 0x05: // DEC B
				doDecReg(&r.b);
				c += 1;
				break;
				
			case 0x06: // LD B, n
				r.b = emu->memory.readByte(r.pc);
				r.pc++;
				c += 2;
				break;
				
			case 0x07: // RLC A
				utmp8 = r.a & 0x80;
				r.a <<= 1;

				if (utmp8 == 0) {
					resetFlag(Flag::CARRY);
					r.a &= 0xFE;
				} else {
					setFlag(Flag::CARRY);
					r.a |= 0x01;
				}

				resetFlag(Flag::ZERO);
				resetFlag(Flag::SUBTRACT);
				resetFlag(Flag::HCARRY);
				
				c += 1;
				break;
				
			case 0x08: // LD (nn), SP
				utmp8 = emu->memory.readByte(r.pc);
				utmp8_2 = emu->memory.readByte(r.pc + 1);
				emu->memory.writeWord(r.sp, (utmp8 << 8) + utmp8_2);
				r.pc += 2;
				c += 5;
				break;
				
			case 0x09: // ADD HL, BC
				doAddHL((r.b << 8) + r.c);
				c += 2;
				break;
				
			case 0x0A: // LD A, (BC)
				r.a = emu->memory.readByte((r.b << 8) + r.c);
				c += 2;
				break;
				
			case 0x0B: // DEC BC
				r.c--;
				if (r.c == 0xFF) {
					r.b--;
				}
				c += 2;
				break;
				
			case 0x0C: // INC C
				doIncReg(&r.c);
				c += 1;
				break;
				
			case 0x0D: // DEC C
				doDecReg(&r.c);
				c += 1;
				break;
				
			case 0x0E: // LD C, n
				r.c = emu->memory.readByte(r.pc);
				r.pc++;
				c += 2;
				break;
				
			case 0x0F: // RRC A
				utmp8 = (r.a & 0x01);
				r.a >>= 1;

				if (utmp8 == 0) {
					resetFlag(Flag::CARRY);
					r.a &= 0x7F;
				} else {
					setFlag(Flag::CARRY);
					r.a |= 0x80;
				}

				resetFlag(Flag::ZERO);
				resetFlag(Flag::SUBTRACT);
				resetFlag(Flag::HCARRY);

				c += 1;
				break;
				
			case 0x10: // STOP
				// TODO: Implement this
				printf("STOP instruction\n");
				timer.div = 0; // STOP resets the timer
				c += 1;
				break;
				
			case 0x11: // LD DE, nn
				r.e = emu->memory.readByte(r.pc);
				r.d = emu->memory.readByte(r.pc + 1);
				r.pc += 2;
				c += 3;
				break;
			
			case 0x12: // LD (DE), A
				emu->memory.writeByte(r.a, (r.d << 8) + r.e);
				c += 2;
				break;
				
			case 0x13: // INC DE
				r.e++;
				if (!r.e) {
					r.d++;
				}
				c += 2;
				break;
				
			case 0x14: // INC D
				doIncReg(&r.d);
				c += 1;
				break;
				
			case 0x15: // DEC D
				doDecReg(&r.d);
				c += 1;
				break;
				
			case 0x16: // LD D, n
				r.d = emu->memory.readByte(r.pc);
				r.pc++;
				c += 2;
				break;
				
			case 0x17: // RL A
				utmp8 = (r.f & Flag::CARRY);
				doRotateLeftWithCarry(&r.a);
				resetFlag(Flag::ZERO); // Always reset zero flag!
				c += 1;
				break;
			
			case 0x18: // JR n
				tmp8 = (int8_t)emu->memory.readByte(r.pc);
				r.pc++;
				r.pc += tmp8;
				c += 3;
				break;
				
			case 0x19: // ADD HL, DE
				doAddHL((r.d << 8) + r.e);
				c += 2;
				break;
				
			case 0x1A: // LD A, (DE)
				r.a = emu->memory.readByte((r.d << 8) + r.e);
				c += 2;
				break;
				
			case 0x1B: // DEC DE
				r.e--;
				if (r.e == 0xFF) {
					r.d--;
				}
				c += 2;
				break;
				
			case 0x1C: // INC E
				doIncReg(&r.e);
				c += 1;
				break;
				
			case 0x1D: // DEC E
				doDecReg(&r.e);
				c += 1;
				break;
				
			case 0x1E: // LD E, n
				r.e = emu->memory.readByte(r.pc);
				r.pc++;
				c += 2;
				break;
				
			case 0x1F: // RR a
				doRotateRightWithCarry(&r.a);
				resetFlag(Flag::ZERO);
				c += 1;
				break;
				
			case 0x20: // JR NZ, n
				tmp8 = (int8_t)emu->memory.readByte(r.pc);
				r.pc++;
				if (!getFlag(Flag::ZERO)) {
					r.pc += tmp8;
					c += 1;
				}
				c += 2;
				break;
				
			case 0x21: // LD HL, nn
				r.l = emu->memory.readByte(r.pc);
				r.h = emu->memory.readByte(r.pc+1);
				r.pc += 2;
				c += 3;
				break;
				
			case 0x22: // LDI (HL), A
				emu->memory.writeByte(r.a, (r.h << 8) + r.l);
				r.l++;
				if (r.l == 0x00) {
					r.h++;
				}
				c += 2;
				break;
				
			case 0x23: // INC HL
				r.l++;
				if (r.l == 0x00) {
					r.h++;
				}
				c += 2;
				break;
				
			case 0x24: // INC H
				doIncReg(&r.h);
				c += 1;
				break;
				
			case 0x25: // DEC H
				doDecReg(&r.h);
				c += 1;
				break;
				
			case 0x26: // LD H, n
				r.h = emu->memory.readByte(r.pc);
				r.pc++;
				c += 2;
				break;
				
			case 0x27: // DAA
				utmp16 = r.a;
				
				if (not getFlag(Flag::SUBTRACT)) {
					if (getFlag(Flag::HCARRY) || (utmp16 & 0x0F) > 9) {
						utmp16 += 0x06;
					}

					if (getFlag(Flag::CARRY) || utmp16 > 0x9F) {
						utmp16 += 0x60;
					}
				} else {
					if (getFlag(Flag::HCARRY)) {
						utmp16 = (utmp16 - 6) & 0xFF;
					}

					if (getFlag(Flag::CARRY)) {
						utmp16 -= 0x60;
					}
				}

				resetFlag(Flag::HCARRY);

				if ((utmp16 & 0x100) == 0x100) {
					setFlag(Flag::CARRY);
				}

				utmp16 &= 0xFF;

				if (utmp16 == 0) {
					setFlag(Flag::ZERO);
				} else {
					resetFlag(Flag::ZERO);
				}

				r.a = utmp16;

				c += 1;
				break;
				
			case 0x28: // JR Z, n
				tmp8 = (int8_t)emu->memory.readByte(r.pc);
				r.pc++;
				if (getFlag(Flag::ZERO)) {
					r.pc += tmp8;
					c += 1;
				}
				c += 2;
				break;
				
			case 0x29: // ADD HL, HL
				doAddHL((r.h << 8) + r.l);
				c += 2;
				break;
				
			case 0x2A: // LDI A, (HL)
				r.a = emu->memory.readByte((r.h << 8) + r.l);
				r.l++;
				if (r.l == 0x00) {
					r.h++;
				}
				c += 2;
				break;
				
			case 0x2B: // DEC HL
				r.l--;
				if (r.l == 0xFF) {
					r.h--;
				}
				c += 2;
				break;
				
			case 0x2C: // INC L
				doIncReg(&r.l);
				c += 1;
				break;
				
			case 0x2D: // DEC L
				doDecReg(&r.l);
				c += 1;
				break;
				
			case 0x2E: // LD L, n
				r.l = emu->memory.readByte(r.pc);
				r.pc++;
				c += 2;
				break;
				
			case 0x2F: // CPL
				r.a = ~r.a;

				setFlag(Flag::SUBTRACT);
				setFlag(Flag::HCARRY);

				c += 1;
				break;
				
			case 0x30: // JR NC, n
				tmp8 = (int8_t)emu->memory.readByte(r.pc);
				r.pc++;
				if (not getFlag(Flag::CARRY)) {
					r.pc += tmp8;
					c += 1;
				}
				c += 2;
				break;
				
			case 0x31: // LD SP, nn
				r.sp = emu->memory.readWord(r.pc);
				r.pc += 2;
				c += 3;
				break;
				
			case 0x32: // LDD (HL), A
				emu->memory.writeByte(r.a, (r.h << 8) + r.l);
				r.l--;
				if (r.l == 0xFF) {
					r.h--;
				}
				c += 2;
				break;
				
			case 0x33: // INC SP
				r.sp++;
				c += 2;
				break;
				
			case 0x34: // INC (HL)
				utmp8 = emu->memory.readByte((r.h << 8) + r.l);
				doIncReg(&utmp8);
				emu->memory.writeByte(utmp8, (r.h << 8) + r.l);
				c += 3;
				break;
				
			case 0x35: // DEC (HL)
				utmp8 = emu->memory.readByte((r.h << 8) + r.l);
				doDecReg(&utmp8);
				emu->memory.writeByte(utmp8, (r.h << 8) + r.l);
				c += 3;
				break;
				
			case 0x36: // LD (HL), n
				emu->memory.writeByte(emu->memory.readByte(r.pc), (r.h << 8) + r.l);
				r.pc++;
				c += 3;
				break;
				
			case 0x37: // SCF
				setFlag(Flag::CARRY);
				resetFlag(Flag::SUBTRACT);
				resetFlag(Flag::HCARRY);
				c += 1;
				break;
				
			case 0x38: // JR C, n
				tmp8 = (int8_t)emu->memory.readByte(r.pc);
				r.pc++;
				if (getFlag(Flag::CARRY)) {
					r.pc += tmp8;
					c += 1;
				}
				c += 2;
				break;
				
			case 0x39: // ADD HL, SP
				doAddHL(r.sp);
				c += 2;
				break;
				
			case 0x3A: // LDD A, (HL)
				r.a = emu->memory.readByte((r.h << 8) + r.l);
				r.l--;
				if (r.l == 0xFF) {
					r.h--;
				}
				c += 2;
				break;
				
			case 0x3B: // DEC SP
				r.sp--;
				c += 2;
				break;
				
			case 0x3C: // INC A
				doIncReg(&r.a);
				c += 1;
				break;
				
			case 0x3D: // DEC A
				doDecReg(&r.a);
				c += 1;
				break;
				
			case 0x3E: // LD A, n
				r.a = emu->memory.readByte(r.pc);
				r.pc++;
				c += 2;
				break;
				
			case 0x3F: // CCF (actually toggles)
				resetFlag(Flag::SUBTRACT);
				resetFlag(Flag::HCARRY);

				if (not getFlag(Flag::CARRY)) {
					setFlag(Flag::CARRY);
				} else {
					resetFlag(Flag::CARRY);
				}

				c += 1;
				break;
				
			case 0x40: // LD B, B
				// NOP
				c += 1;
				break;
				
			case 0x41: // LD B, C
				r.b = r.c;
				c += 1;
				break;
				
			case 0x42: // LD B, D
				r.b = r.d;
				c += 1;
				break;
				
			case 0x43: // LD B, E
				r.b = r.e;
				c += 1;
				break;
				
			case 0x44: // LD B, H
				r.b = r.h;
				c += 1;
				break;
				
			case 0x45: // LD B, L
				r.b = r.l;
				c += 1;
				break;
				
			case 0x46: // LD B, (HL)
				r.b = emu->memory.readByte((r.h << 8) + r.l);
				c += 2;
				break;
				
			case 0x47: // LD B, A
				r.b = r.a;
				c += 1;
				break;
				
			case 0x48: // LD C, B
				r.c = r.b;
				c += 1;
				break;
				
			case 0x49: // LD C, C
				// NOP
				c += 1;
				break;
				
			case 0x4A: // LD C, D
				r.c = r.d;
				c += 1;
				break;
				
			case 0x4B: // LD C, E
				r.c = r.e;
				c += 1;
				break;
				
			case 0x4C: // LD C, H
				r.c = r.h;
				c += 1;
				break;
				
			case 0x4D: // LD C, L
				r.c = r.l;
				c += 1;
				break;
				
			case 0x4E: // LD C, (HL)
				r.c = emu->memory.readByte((r.h << 8) + r.l);
				c += 2;
				break;
				
			case 0x4F: // LD C, A
				r.c = r.a;
				c += 1;
				break;
				
			case 0x50: // LD D, B
				r.d = r.b;
				c += 1;
				break;
				
			case 0x51: // LD D, C
				r.d = r.c;
				c += 1;
				break;
				
			case 0x52: // LD D, D
				// NOP
				c += 1;
				break;
				
			case 0x53: // LD D, E
				r.d = r.e;
				c += 1;
				break;
				
			case 0x54: // LD D, H
				r.d = r.h;
				c += 1;
				break;
				
			case 0x55: // LD D, L
				r.d = r.l;
				c += 1;
				break;
				
			case 0x56: // LD D, (HL)
				r.d = emu->memory.readByte((r.h << 8) + r.l);
				c += 2;
				break;
				
			case 0x57: // LD D, A
				r.d = r.a;
				c += 1;
				break;
				
			case 0x58: // LD E, B
				r.e = r.b;
				c += 1;
				break;
				
			case 0x59: // LD E, C
				r.e = r.c;
				c += 1;
				break;
				
			case 0x5A: // LD E, D
				r.e = r.d;
				c += 1;
				break;
				
			case 0x5B: // LD E, E
				// NOP
				c += 1;
				break;
				
			case 0x5C: // LD E, H
				r.e = r.h;
				c += 1;
				break;
				
			case 0x5D: // LD E, L
				r.e = r.l;
				c += 1;
				break;
				
			case 0x5E: // LD E, (HL)
				r.e = emu->memory.readByte((r.h << 8) + r.l);
				c += 2;
				break;
				
			case 0x5F: // LD E, A
				r.e = r.a;
				c += 1;
				break;
				
			case 0x60: // LD H, B
				r.h = r.b;
				c += 1;
				break;
				
			case 0x61: // LD H, C
				r.h = r.c;
				c += 1;
				break;
				
			case 0x62: // LD H, D
				r.h = r.d;
				c += 1;
				break;
				
			case 0x63: // LD H, E
				r.h = r.e;
				c += 1;
				break;
				
			case 0x64: // LD H, H
				// NOP
				c += 1;
				break;
				
			case 0x65: // LD H, L
				r.h = r.l;
				c += 1;
				break;
				
			case 0x66: // LD H, (HL)
				r.h = emu->memory.readByte((r.h << 8) + r.l);
				c += 2;
				break;
				
			case 0x67: // LD H, A
				r.h = r.a;
				c += 1;
				break;
				
			case 0x68: // LD L, B
				r.l = r.b;
				c += 1;
				break;
				
			case 0x69: // LD L, C
				r.l = r.c;
				c += 1;
				break;
				
			case 0x6A: // LD L, D
				r.l = r.d;
				c += 1;
				break;
				
			case 0x6B: // LD L, E
				r.l = r.e;
				c += 1;
				break;
				
			case 0x6C: // LD L, H
				r.l = r.h;
				c += 1;
				break;
				
			case 0x6D: // LD L, L
				// NOP
				c += 1;
				break;
				
			case 0x6E: // LD L, (HL)
				r.l = emu->memory.readByte((r.h << 8) + r.l);
				c += 2;
				break;
				
			case 0x6F: // LD L, A
				r.l = r.a;
				c += 1;
				break;
				
			case 0x70: // LD (HL), B
				emu->memory.writeByte(r.b, (r.h << 8) + r.l);
				c += 2;
				break;

			case 0x71: // LD (HL), C
				emu->memory.writeByte(r.c, (r.h << 8) + r.l);
				c += 2;
				break;

			case 0x72: // LD (HL), D
				emu->memory.writeByte(r.d, (r.h << 8) + r.l);
				c += 2;
				break;

			case 0x73: // LD (HL), E
				emu->memory.writeByte(r.e, (r.h << 8) + r.l);
				c += 2;
				break;

			case 0x74: // LD (HL), H
				emu->memory.writeByte(r.h, (r.h << 8) + r.l);
				c += 2;
				break;

			case 0x75: // LD (HL), L
				emu->memory.writeByte(r.l, (r.h << 8) + r.l);
				c += 2;
				break;

			case 0x76: // HALT
				// store interrupt flags
				oldIntFlags = intFlags;
				halted = true;
				
				//intsOn = true;
				//printf("HALT instruction, intsOn=%d, IF=%02X\n", intsOn, intFlags);
				break;

			case 0x77: // LD (HL), A
				emu->memory.writeByte(r.a, (r.h << 8) + r.l);
				c += 2;
				break;

			case 0x78: // LD A, B
				r.a = r.b;
				c += 1;
				break;

			case 0x79: // LD A, C
				r.a = r.c;
				c += 1;
				break;

			case 0x7A: // LD A, D
				r.a = r.d;
				c += 1;
				break;

			case 0x7B: // LD A, E
				r.a = r.e;
				c += 1;
				break;

			case 0x7C: // LD A, H
				r.a = r.h;
				c += 1;
				break;

			case 0x7D: // LD A, L
				r.a = r.l;
				c += 1;
				break;

			case 0x7E: // LD A, (HL)
				r.a = emu->memory.readByte((r.h << 8) + r.l);
				c += 2;
				break;

			case 0x7F: // LD A, A
				// NOP
				c += 1;
				break;

			case 0x80: // ADD A, B
				doAddReg(&r.a, r.b);
				c += 1;
				break;

			case 0x81: // ADD A, C
				doAddReg(&r.a, r.c);
				c += 1;
				break;

			case 0x82: // ADD A, D
				doAddReg(&r.a, r.d);
				c += 1;
				break;

			case 0x83: // ADD A, E
				doAddReg(&r.a, r.e);
				c += 1;
				break;

			case 0x84: // ADD A, H
				doAddReg(&r.a, r.h);
				c += 1;
				break;

			case 0x85: // ADD A, L
				doAddReg(&r.a, r.l);
				c += 1;
				break;

			case 0x86: // ADD A, (HL)
				doAddReg(&r.a, emu->memory.readByte((r.h << 8) + r.l));
				c += 2;
				break;

			case 0x87: // ADD A, A
				doAddReg(&r.a, r.a);
				c += 1;
				break;

			case 0x88: // ADC A, B
				doAddRegWithCarry(&r.a, r.b);
				c += 1;
				break;

			case 0x89: // ADC A, C
				doAddRegWithCarry(&r.a, r.c);
				c += 1;
				break;

			case 0x8A: // ADC A, D
				doAddRegWithCarry(&r.a, r.d);
				c += 1;
				break;

			case 0x8B: // ADC A, E
				doAddRegWithCarry(&r.a, r.e);
				c += 1;
				break;

			case 0x8C: // ADC A, H
				doAddRegWithCarry(&r.a, r.h);
				c += 1;
				break;

			case 0x8D: // ADC A, L
				doAddRegWithCarry(&r.a, r.l);
				c += 1;
				break;

			case 0x8E: // ADC A, (HL)
				doAddRegWithCarry(&r.a, emu->memory.readByte((r.h << 8) + r.l));
				c += 2;
				break;

			case 0x8F: // ADC A, A
				doAddRegWithCarry(&r.a, r.a);
				c += 1;
				break;

			case 0x90: // SUB A, B
				doSubReg(&r.a, r.b);
				c += 1;
				break;

			case 0x91: // SUB A, C
				doSubReg(&r.a, r.c);
				c += 1;
				break;

			case 0x92: // SUB A, D
				doSubReg(&r.a, r.d);
				c += 1;
				break;

			case 0x93: // SUB A, E
				doSubReg(&r.a, r.e);
				c += 1;
				break;

			case 0x94: // SUB A, H
				doSubReg(&r.a, r.h);
				c += 1;
				break;

			case 0x95: // SUB A, L
				doSubReg(&r.a, r.l);
				c += 1;
				break;

			case 0x96: // SUB A, (HL)
				doSubReg(&r.a, emu->memory.readByte((r.h << 8) + r.l));
				c += 2;
				break;

			case 0x97: // SUB A, A
				doSubReg(&r.a, r.a);
				c += 1;
				break;

			case 0x98: // SBC A, B
				doSubRegWithCarry(&r.a, r.b);
				c += 1;
				break;

			case 0x99: // SBC A, C
				doSubRegWithCarry(&r.a, r.c);
				c += 1;
				break;

			case 0x9A: // SBC A, D
				doSubRegWithCarry(&r.a, r.d);
				c += 1;
				break;

			case 0x9B: // SBC A, E
				doSubRegWithCarry(&r.a, r.e);
				c += 1;
				break;

			case 0x9C: // SBC A, H
				doSubRegWithCarry(&r.a, r.h);
				c += 1;
				break;

			case 0x9D: // SBC A, L
				doSubRegWithCarry(&r.a, r.l);
				c += 1;
				break;

			case 0x9E: // SBC A, (HL)
				doSubRegWithCarry(&r.a, emu->memory.readByte((r.h << 8) + r.l));
				c += 2;
				break;

			case 0x9F: // SBC A, A
				doSubRegWithCarry(&r.a, r.a);
				c += 1;
				break;

			case 0xA0: // AND B
				doAndRegA(r.b);
				c += 1;
				break;

			case 0xA1: // AND C
				doAndRegA(r.c);
				c += 1;
				break;

			case 0xA2: // AND D
				doAndRegA(r.d);
				c += 1;
				break;

			case 0xA3: // AND E
				doAndRegA(r.e);
				c += 1;
				break;

			case 0xA4: // AND H
				doAndRegA(r.h);
				c += 1;
				break;

			case 0xA5: // AND L
				doAndRegA(r.l);
				c += 1;
				break;

			case 0xA6: // AND (HL)
				doAndRegA(emu->memory.readByte((r.h << 8) + r.l));
				c += 2;
				break;

			case 0xA7: // AND A
				doAndRegA(r.a);
				c += 1;
				break;

			case 0xA8: // XOR B
				doXorRegA(r.b);
				c += 1;
				break;

			case 0xA9: // XOR C
				doXorRegA(r.c);
				c += 1;
				break;

			case 0xAA: // XOR D
				doXorRegA(r.d);
				c += 1;
				break;

			case 0xAB: // XOR E
				doXorRegA(r.e);
				c += 1;
				break;

			case 0xAC: // XOR H
				doXorRegA(r.h);
				c += 1;
				break;

			case 0xAD: // XOR L
				doXorRegA(r.l);
				c += 1;
				break;

			case 0xAE: // XOR (HL)
				doXorRegA(emu->memory.readByte((r.h << 8) + r.l));
				c += 2;
				break;

			case 0xAF: // XOR A
				doXorRegA(r.a);
				c += 1;
				break;

			case 0xB0: // OR B
				doOrRegA(r.b);
				c += 1;
				break;

			case 0xB1: // OR C
				doOrRegA(r.c);
				c += 1;
				break;

			case 0xB2: // OR D
				doOrRegA(r.d);
				c += 1;
				break;

			case 0xB3: // OR E
				doOrRegA(r.e);
				c += 1;
				break;

			case 0xB4: // OR H
				doOrRegA(r.h);
				c += 1;
				break;

			case 0xB5: // OR L
				doOrRegA(r.l);
				c += 1;
				break;

			case 0xB6: // OR (HL)
				doOrRegA(emu->memory.readByte((r.h << 8) + r.l));
				c += 2;
				break;

			case 0xB7: // OR A
				doOrRegA(r.a);
				c += 1;
				break;

			case 0xB8: // CP B
				doCpRegA(r.b);
				c += 1;
				break;

			case 0xB9: // CP C
				doCpRegA(r.c);
				c += 1;
				break;

			case 0xBA: // CP D
				doCpRegA(r.d);
				c += 1;
				break;

			case 0xBB: // CP E
				doCpRegA(r.e);
				c += 1;
				break;

			case 0xBC: // CP H
				doCpRegA(r.h);
				c += 1;
				break;

			case 0xBD: // CP L
				doCpRegA(r.l);
				c += 1;
				break;

			case 0xBE: // CP (HL)
				doCpRegA(emu->memory.readByte((r.h << 8) + r.l));
				c += 2;
				break;

			case 0xBF: // CP A
				doCpRegA(r.a);
				c += 1;
				break;

			case 0xC0: // RET NZ
				if (not getFlag(Flag::ZERO)) {
					oldpc = r.pc;
					r.pc = emu->memory.readWord(r.sp);
					callStackPop(oldpc, r.pc);
					r.sp += 2;
					c += 3;
				}
				c += 2;
				break;

			case 0xC1: // POP BC
				r.c = emu->memory.readByte(r.sp);
				r.sp++;
				r.b = emu->memory.readByte(r.sp);
				r.sp++;
				c += 3;
				break;

			case 0xC2: // JP NZ, nn
				if (not getFlag(Flag::ZERO)) {
					r.pc = emu->memory.readWord(r.pc);
					c += 1;
				} else {
					r.pc += 2;
				}
				c += 3;
				break;

			case 0xC3: // JP nn
				r.pc = emu->memory.readWord(r.pc);
				c += 4;
				break;

			case 0xC4: // CALL NZ, nn
				if (not getFlag(Flag::ZERO)) {
					r.sp -= 2;
					emu->memory.writeWord(r.pc + 2, r.sp);
					oldpc = r.pc;
					r.pc = emu->memory.readWord(r.pc);
					callStackPush(oldpc, r.pc);
					c += 3;
				} else {
					r.pc += 2;
				}
				c += 3;
				break;

			case 0xC5: // PUSH BC
				r.sp--;
				emu->memory.writeByte(r.b, r.sp);
				r.sp--;
				emu->memory.writeByte(r.c, r.sp);
				c += 4;
				break;

			case 0xC6: // ADD A, n
				utmp8 = emu->memory.readByte(r.pc);
				r.pc++;
				doAddReg(&r.a, utmp8);
				c += 2;
				break;

			case 0xC7: // RST 00
				r.sp -= 2;
				emu->memory.writeWord(r.pc, r.sp);
				r.pc = 0x00;
				c += 4;
				break;

			case 0xC8: // RET Z
				if (getFlag(Flag::ZERO)) {
					oldpc = r.pc;
					r.pc = emu->memory.readWord(r.sp);
					callStackPop(oldpc, r.pc);
					r.sp += 2;
					c += 3;
				}
				c += 2;
				break;

			case 0xC9: // RET
				oldpc = r.pc;
				r.pc = emu->memory.readWord(r.sp);
				callStackPop(oldpc, r.pc);
				r.sp += 2;
				c += 4;
				break;

			case 0xCA: // JP Z, nn
				if (getFlag(Flag::ZERO)) {
					r.pc = emu->memory.readWord(r.pc);
					c += 1;
				} else {
					r.pc += 2;
				}
				c += 3;
				break;

			case 0xCB: // Extra instructions
				utmp8 = emu->memory.readByte(r.pc);
				r.pc++;
				doExtraOP(utmp8);
				break;

			case 0xCC: // CALL Z, nn
				if (getFlag(Flag::ZERO)) {
					r.sp -= 2;
					emu->memory.writeWord(r.pc + 2, r.sp);
					oldpc = r.pc;
					r.pc = emu->memory.readWord(r.pc);
					callStackPush(oldpc, r.pc);
					c += 3;
				} else {
					r.pc += 2;
				}
				c += 3;
				break;

			case 0xCD: // CALL nn
				r.sp -= 2;
				emu->memory.writeWord(r.pc + 2, r.sp);
				oldpc = r.pc;
				r.pc = emu->memory.readWord(r.pc);
				callStackPush(oldpc, r.pc);
				c += 6;
				break;

			case 0xCE: // ADC A, n
				doAddRegWithCarry(&r.a, emu->memory.readByte(r.pc));
				r.pc++;
				c += 2;
				break;

			case 0xCF: // RST 08
				r.sp -= 2;
				emu->memory.writeWord(r.pc, r.sp);
				r.pc = 0x08;
				c += 4;
				break;

			case 0xD0: // RET NC
				if (not getFlag(Flag::CARRY)) {
					oldpc = r.pc;
					r.pc = emu->memory.readWord(r.sp);
					callStackPop(oldpc, r.pc);
					r.sp += 2;
					c += 3;
				}
				c += 2;
				break;

			case 0xD1: // POP DE
				r.e = emu->memory.readByte(r.sp);
				r.sp++;
				r.d = emu->memory.readByte(r.sp);
				r.sp++;
				c += 3;
				break;

			case 0xD2: // JP NC, nn
				if (not getFlag(Flag::CARRY)) {
					r.pc = emu->memory.readWord(r.pc);
					c += 1;
				} else {
					r.pc += 2;
				}
				c += 3;
				break;

			case 0xD3: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xD4: // CALL NC, nn
				if (not getFlag(Flag::CARRY)) {
					r.sp -= 2;
					emu->memory.writeWord(r.pc + 2, r.sp);
					oldpc = r.pc;
					r.pc = emu->memory.readWord(r.pc);
					callStackPush(oldpc, r.pc);
					c += 3;
				} else {
					r.pc += 2;
				}
				c += 3;
				break;

			case 0xD5: // PUSH DE
				r.sp--;
				emu->memory.writeByte(r.d, r.sp);
				r.sp--;
				emu->memory.writeByte(r.e, r.sp);
				c += 4;
				break;

			case 0xD6: // SUB A, n
				doSubReg(&r.a, emu->memory.readByte(r.pc));
				r.pc++;
				c += 2;
				break;

			case 0xD7: // RST 10
				r.sp -= 2;
				emu->memory.writeWord(r.pc, r.sp);
				r.pc = 0x10;
				c += 4;
				break;

			case 0xD8: // RET C
				if (getFlag(Flag::CARRY)) {
					oldpc = r.pc;
					r.pc = emu->memory.readWord(r.sp);
					callStackPop(oldpc, r.pc);
					r.sp += 2;
					c += 3;
				}
				c += 2;
				break;

			case 0xD9: // RETI
				intsOn = true;
				//printf("reti, intsOn = 1\n");
				
				r.pc = emu->memory.readWord(r.sp);
				r.sp += 2;
				
				c += 4;
				break;

			case 0xDA: // JP C, nn
				if (getFlag(Flag::CARRY)) {
					r.pc = emu->memory.readWord(r.pc);
					c += 1;
				} else {
					r.pc += 2;
				}
				c += 3;
				break;

			case 0xDB: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xDC: // CALL C, nn
				if (getFlag(Flag::CARRY)) {
					r.sp -= 2;
					emu->memory.writeWord(r.pc + 2, r.sp);
					oldpc = r.pc;
					r.pc = emu->memory.readWord(r.pc);
					callStackPush(oldpc, r.pc);
					c += 3;
				} else {
					r.pc += 2;
				}
				c += 3;
				break;

			case 0xDD: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xDE: // SBC A, n
				doSubRegWithCarry(&r.a, emu->memory.readByte(r.pc));
				r.pc++;
				c += 2;
				break;

			case 0xDF: // RST 18
				r.sp -= 2;
				emu->memory.writeWord(r.pc, r.sp);
				r.pc = 0x18;
				c += 4;
				break;

			case 0xE0: // LDH (n), A
				emu->memory.writeByte(r.a, 0xFF00 + emu->memory.readByte(r.pc));
				r.pc++;
				c += 3;
				break;

			case 0xE1: // POP HL
				r.l = emu->memory.readByte(r.sp);
				r.sp++;
				r.h = emu->memory.readByte(r.sp);
				r.sp++;
				c += 3;
				break;

			case 0xE2: // LDH (C), A
				emu->memory.writeByte(r.a, 0xFF00 + r.c);
				c += 2;
				break;

			case 0xE3: // unimplemented
			case 0xE4: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xE5: // PUSH HL
				r.sp--;
				emu->memory.writeByte(r.h, r.sp);
				r.sp--;
				emu->memory.writeByte(r.l, r.sp);
				c += 4;
				break;

			case 0xE6: // AND n
				doAndRegA(emu->memory.readByte(r.pc));
				r.pc++;
				c += 2;
				break;

			case 0xE7: // RST 20
				r.sp -= 2;
				emu->memory.writeWord(r.pc, r.sp);
				r.pc = 0x20;
				c += 4;
				break;

			case 0xE8: // ADD SP, n
				tmp8 = (int8_t)emu->memory.readByte(r.pc);
				r.pc++;

				utmp16 = r.sp + tmp8;
				if ((utmp16 & 0xFF) < (r.sp & 0xFF)) {
					setFlag(Flag::CARRY);
				} else {
					resetFlag(Flag::CARRY);
				}

				if ((utmp16 & 0x0F) < (r.sp & 0x0F)) {
					setFlag(Flag::HCARRY);
				} else {
					resetFlag(Flag::HCARRY);
				}

				r.sp = utmp16;
				resetFlag(Flag::ZERO);
				resetFlag(Flag::SUBTRACT);

				c += 4;
				break;

			case 0xE9: // JP (HL)
				r.pc = (r.h << 8) + r.l;
				c += 1;
				break;

			case 0xEA: // LD (nn), A
				emu->memory.writeByte(r.a, emu->memory.readWord(r.pc));
				r.pc += 2;
				c += 4;
				break;

			case 0xEB: // unimplemented
			case 0xEC: // unimplemented
			case 0xED: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xEE: // XOR n
				doXorRegA(emu->memory.readByte(r.pc));
				r.pc++;
				c += 2;
				break;

			case 0xEF: // RST 28
				r.sp -= 2;
				emu->memory.writeWord(r.pc, r.sp);
				r.pc = 0x28;
				c += 4;
				break;

			case 0xF0: // LDH A, (n)
				r.a = emu->memory.readByte(0xFF00 + emu->memory.readByte(r.pc));
				r.pc++;
				c += 3;
				break;

			case 0xF1: // POP AF
				r.f = emu->memory.readByte(r.sp) & 0xF0;
				r.sp++;
				r.a = emu->memory.readByte(r.sp);
				r.sp++;
				c += 3;
				break;

			case 0xF2: // LDH a, (C)?
				r.a = emu->memory.readByte(0xFF00 + r.c);
				c += 2;
				break;

			case 0xF3: // DI
				intsOn = false;
				//printf("DI, intsOn = 0\n");
				c += 1;
				break;

			case 0xF4: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xF5: // PUSH AF
				r.sp--;
				emu->memory.writeByte(r.a, r.sp);
				r.sp--;
				emu->memory.writeByte(r.f, r.sp);
				c += 4;
				break;

			case 0xF6: // OR n
				doOrRegA(emu->memory.readByte(r.pc));
				r.pc++;
				c += 2;
				break;

			case 0xF7: // RST 30
				r.sp -= 2;
				emu->memory.writeWord(r.pc, r.sp);
				r.pc = 0x30;
				c += 4;
				break;

			case 0xF8: // LDHL SP, d
				tmp8 = (int8_t)emu->memory.readByte(r.pc);
				r.pc++;
				utmp16 = (r.sp + tmp8) & 0xFFFF;

				if ((utmp16 & 0x0F) < (r.sp & 0x0F)) {
					setFlag(Flag::HCARRY);
				} else {
					resetFlag(Flag::HCARRY);
				}

				if ((utmp16 & 0xFF) < (r.sp & 0xFF)) {
					setFlag(Flag::CARRY);
				} else {
					resetFlag(Flag::CARRY);
				}

				resetFlag(Flag::ZERO);
				resetFlag(Flag::SUBTRACT);

				r.h = utmp16 >> 8;
				r.l = utmp16 & 0xFF;

				c += 3;
				break;

			case 0xF9: // LD SP, HL
				r.sp = (r.h << 8) + r.l;
				c += 2;
				break;

			case 0xFA: // LD A, (nn)
				r.a = emu->memory.readByte(emu->memory.readWord(r.pc));
				r.pc += 2;
				c += 4;
				break;

			case 0xFB: // EI
				intsOn = true;
				//printf("EI, intsOn = 1\n");
				c += 1;
				break;

			case 0xFC: // unimplemented
			case 0xFD: // unimplemented
				doOpcodeUNIMP();
				break;

			case 0xFE: // CP n
				doCpRegA(emu->memory.readByte(r.pc));
				r.pc++;
				c += 2;
				break;

			case 0xFF: // RST 38
				r.sp -= 2;
				emu->memory.writeWord(r.pc, r.sp);
				r.pc = 0x38;
				c += 4;
				break;
				
			default:
				printf("There's a glitch in the matrix, this shouldn't happen.\n");
				return 0;
		}
	} else {
		// IF has changed, stop halting
		//if (intFlags != oldIntFlags) {
		if (intFlags) {
			halted = false;
			//printf("Done halting\n");
		}

		//printf("halting: intFlags=%d, oldIntFlags=%d.\n", intFlags, oldIntFlags);

		// still increase clock
		c += 1;
	}

	handleTimers();

	dc = c - dc; // delta cycles, TODO: after interrupts?

	//printRegisters();
	
	return 1;
}

inline const char *CPU::numToRegName(uint8_t num)
{
	switch (num % 8) {
		case 0: return "B";
		case 1: return "C";
		case 2: return "D";
		case 3: return "E";
		case 4: return "H";
		case 5: return "L";
		case 6: return "(HL)";
		case 7: return "A";
		default: return "NULL";
	}
}


uint16_t CPU::instructionToString(uint16_t pc, char *instStr)
{
	uint8_t inst = emu->memory.readByte(pc);
	uint16_t npc = pc + 1;
	uint8_t extraOp, bitnr;

	switch (inst) {
		case 0x00:
			sprintf(instStr, "NOP");
			break;
			
		case 0x01: // LD BC, nn
			sprintf(instStr, "LD BC, 0x%04X", emu->memory.readWord(npc));
			break;
			npc += 2;
			
		case 0x02: // LD (BC), A
			sprintf(instStr, "LD (BC), A");
			break;
			
		case 0x03: // INC BC
			sprintf(instStr, "INC BC");
			break;
			
		case 0x04: // INC B
			sprintf(instStr, "INC B");
			break;
			
		case 0x05: // DEC B
			sprintf(instStr, "DEC B");
			break;
			
		case 0x06: // LD B, n
			sprintf(instStr, "LD B, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x07: // RLC A
			sprintf(instStr, "RLC A");
			break;
			
		case 0x08: // LD (nn), SP
			sprintf(instStr, "LD (0x%04X), SP", emu->memory.readWord(npc));
			npc += 2;
			break;
			
		case 0x09: // ADD HL, BC
			sprintf(instStr, "ADD HL, BC");
			break;
			
		case 0x0A: // LD A, (BC)
			sprintf(instStr, "LD A, (BC)");
			break;
			
		case 0x0B: // DEC BC
			sprintf(instStr, "DEC BC");
			break;
			
		case 0x0C: // INC C
			sprintf(instStr, "INC C");
			break;
			
		case 0x0D: // DEC C
			sprintf(instStr, "DEC C");
			break;
			
		case 0x0E: // LD C, n
			sprintf(instStr, "LD C, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x0F: // RRC A
			sprintf(instStr, "RRC A");
			break;
			
		case 0x10: // STOP
			sprintf(instStr, "STOP");
			break;
			
		case 0x11: // LD DE, nn
			sprintf(instStr, "LD DE, 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;
		
		case 0x12: // LD (DE), A
			sprintf(instStr, "LD (DE), A");
			break;
			
		case 0x13: // INC DE
			sprintf(instStr, "INC DE");
			break;
			
		case 0x14: // INC D
			sprintf(instStr, "INC D");
			break;
			
		case 0x15: // DEC D
			sprintf(instStr, "DEC D");
			break;
			
		case 0x16: // LD D, n
			sprintf(instStr, "LD D, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x17: // RL A
			sprintf(instStr, "RL A");
			break;
		
		case 0x18: // JR n
			sprintf(instStr, "JR 0x%02X (%d)", emu->memory.readByte(npc), (int8_t)emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x19: // ADD HL, DE
			sprintf(instStr, "ADD HL, DE");
			break;
			
		case 0x1A: // LD A, (DE)
			sprintf(instStr, "LD A, (DE)");
			break;
			
		case 0x1B: // DEC DE
			sprintf(instStr, "DEC DE");
			break;
			
		case 0x1C: // INC E
			sprintf(instStr, "INC E");
			break;
			
		case 0x1D: // DEC E
			sprintf(instStr, "DEC E");
			break;
			
		case 0x1E: // LD E, n
			sprintf(instStr, "LD E, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x1F: // RR a
			sprintf(instStr, "RR a");
			break;
			
		case 0x20: // JR NZ, n
			sprintf(instStr, "JR NZ, 0x%02X (%d)", emu->memory.readByte(npc), (int8_t)emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x21: // LD HL, nn
			sprintf(instStr, "LD HL, 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;
			
		case 0x22: // LDI (HL), A
			sprintf(instStr, "LDI (HL), A");
			break;
			
		case 0x23: // INC HL
			sprintf(instStr, "INC HL");
			break;
			
		case 0x24: // INC H
			sprintf(instStr, "INC H");
			break;
			
		case 0x25: // DEC H
			sprintf(instStr, "DEC H");
			break;
			
		case 0x26: // LD H, n
			sprintf(instStr, "LD H, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x27: // DAA
			sprintf(instStr, "DAA");
			break;
			
		case 0x28: // JR Z, n
			sprintf(instStr, "JR Z, 0x%02X (%d)", emu->memory.readByte(npc), (int8_t)emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x29: // ADD HL, HL
			sprintf(instStr, "ADD HL, HL");
			break;
			
		case 0x2A: // LDI A, (HL)
			sprintf(instStr, "LDI A, (HL)");
			break;
			
		case 0x2B: // DEC HL
			sprintf(instStr, "DEC HL");
			break;
			
		case 0x2C: // INC L
			sprintf(instStr, "INC L");
			break;
			
		case 0x2D: // DEC L
			sprintf(instStr, "DEC L");
			break;
			
		case 0x2E: // LD L, n
			sprintf(instStr, "LD L, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x2F: // CPL
			sprintf(instStr, "CPL");
			break;
			
		case 0x30: // JR NC, n
			sprintf(instStr, "JR NC, 0x%02X (%d)", emu->memory.readByte(npc), (int8_t)emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x31: // LD SP, nn
			sprintf(instStr, "LD SP, 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;
			
		case 0x32: // LDD (HL), A
			sprintf(instStr, "LDD (HL), A");
			break;
			
		case 0x33: // INC SP
			sprintf(instStr, "INC SP");
			break;
			
		case 0x34: // INC (HL)
			sprintf(instStr, "INC (HL)");
			break;
			
		case 0x35: // DEC (HL)
			sprintf(instStr, "DEC (HL)");
			break;
			
		case 0x36: // LD (HL), n
			sprintf(instStr, "LD (HL), 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x37: // SCF
			sprintf(instStr, "SCF");
			break;
			
		case 0x38: // JR C, n
			sprintf(instStr, "JR C, 0x%02X (%d)", emu->memory.readByte(npc), (int8_t)emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x39: // ADD HL, SP
			sprintf(instStr, "ADD HL, SP");
			break;
			
		case 0x3A: // LDD A, (HL)
			sprintf(instStr, "LDD A, (HL)");
			break;
			
		case 0x3B: // DEC SP
			sprintf(instStr, "DEC SP");
			break;
			
		case 0x3C: // INC A
			sprintf(instStr, "INC A");
			break;
			
		case 0x3D: // DEC A
			sprintf(instStr, "DEC A");
			break;
			
		case 0x3E: // LD A, n
			sprintf(instStr, "LD A, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;
			
		case 0x3F: // CCF (actually toggles)
			sprintf(instStr, "CCF");
			break;
			
		case 0x40: // LD B, B
			sprintf(instStr, "LD B, B");
			break;
			
		case 0x41: // LD B, C
			sprintf(instStr, "LD B, C");
			break;
			
		case 0x42: // LD B, D
			sprintf(instStr, "LD B, D");
			break;
			
		case 0x43: // LD B, E
			sprintf(instStr, "LD B, E");
			break;
			
		case 0x44: // LD B, H
			sprintf(instStr, "LD B, H");
			break;
			
		case 0x45: // LD B, L
			sprintf(instStr, "LD B, L");
			break;
			
		case 0x46: // LD B, (HL)
			sprintf(instStr, "LD B, (HL)");
			break;
			
		case 0x47: // LD B, A
			sprintf(instStr, "LD B, A");
			break;
			
		case 0x48: // LD C, B
			sprintf(instStr, "LD C, B");
			break;
			
		case 0x49: // LD C, C
			sprintf(instStr, "LD C, C");
			break;
			
		case 0x4A: // LD C, D
			sprintf(instStr, "LD C, D");
			break;
			
		case 0x4B: // LD C, E
			sprintf(instStr, "LD C, E");
			break;
			
		case 0x4C: // LD C, H
			sprintf(instStr, "LD C, H");
			break;
			
		case 0x4D: // LD C, L
			sprintf(instStr, "LD C, L");
			break;
			
		case 0x4E: // LD C, (HL)
			sprintf(instStr, "LD C, (HL)");
			break;
			
		case 0x4F: // LD C, A
			sprintf(instStr, "LD C, A");
			break;
			
		case 0x50: // LD D, B
			sprintf(instStr, "LD D, B");
			break;
			
		case 0x51: // LD D, C
			sprintf(instStr, "LD D, C");
			break;
			
		case 0x52: // LD D, D
			sprintf(instStr, "LD D, D");
			break;
			
		case 0x53: // LD D, E
			sprintf(instStr, "LD D, E");
			break;
			
		case 0x54: // LD D, H
			sprintf(instStr, "LD D, H");
			break;
			
		case 0x55: // LD D, L
			sprintf(instStr, "LD D, L");
			break;
			
		case 0x56: // LD D, (HL)
			sprintf(instStr, "LD D, (HL)");
			break;
			
		case 0x57: // LD D, A
			sprintf(instStr, "LD D, A");
			break;
			
		case 0x58: // LD E, B
			sprintf(instStr, "LD E, B");
			break;
			
		case 0x59: // LD E, C
			sprintf(instStr, "LD E, C");
			break;
			
		case 0x5A: // LD E, D
			sprintf(instStr, "LD E, D");
			break;
			
		case 0x5B: // LD E, E
			sprintf(instStr, "LD E, E");
			break;
			
		case 0x5C: // LD E, H
			sprintf(instStr, "LD E, H");
			break;
			
		case 0x5D: // LD E, L
			sprintf(instStr, "LD E, L");
			break;
			
		case 0x5E: // LD E, (HL)
			sprintf(instStr, "LD E, (HL)");
			break;
			
		case 0x5F: // LD E, A
			sprintf(instStr, "LD E, A");
			break;
			
		case 0x60: // LD H, B
			sprintf(instStr, "LD H, B");
			break;
			
		case 0x61: // LD H, C
			sprintf(instStr, "LD H, C");
			break;
			
		case 0x62: // LD H, D
			sprintf(instStr, "LD H, D");
			break;
			
		case 0x63: // LD H, E
			sprintf(instStr, "LD H, E");
			break;
			
		case 0x64: // LD H, H
			sprintf(instStr, "LD H, H");
			break;
			
		case 0x65: // LD H, L
			sprintf(instStr, "LD H, L");
			break;
			
		case 0x66: // LD H, (HL)
			sprintf(instStr, "LD H, (HL)");
			break;
			
		case 0x67: // LD H, A
			sprintf(instStr, "LD H, A");
			break;
			
		case 0x68: // LD L, B
			sprintf(instStr, "LD L, B");
			break;
			
		case 0x69: // LD L, C
			sprintf(instStr, "LD L, C");
			break;
			
		case 0x6A: // LD L, D
			sprintf(instStr, "LD L, D");
			break;
			
		case 0x6B: // LD L, E
			sprintf(instStr, "LD L, E");
			break;
			
		case 0x6C: // LD L, H
			sprintf(instStr, "LD L, H");
			break;
			
		case 0x6D: // LD L, L
			sprintf(instStr, "LD L, L");
			break;
			
		case 0x6E: // LD L, (HL)
			sprintf(instStr, "LD L, (HL)");
			break;
			
		case 0x6F: // LD L, A
			sprintf(instStr, "LD L, A");
			break;
			
		case 0x70: // LD (HL), B
			sprintf(instStr, "LD (HL), B");
			break;

		case 0x71: // LD (HL), C
			sprintf(instStr, "LD (HL), C");
			break;

		case 0x72: // LD (HL), D
			sprintf(instStr, "LD (HL), D");
			break;

		case 0x73: // LD (HL), E
			sprintf(instStr, "LD (HL), E");
			break;

		case 0x74: // LD (HL), H
			sprintf(instStr, "LD (HL), H");
			break;

		case 0x75: // LD (HL), L
			sprintf(instStr, "LD (HL), L");
			break;

		case 0x76: // HALT
			sprintf(instStr, "HALT");
			break;

		case 0x77: // LD (HL), A
			sprintf(instStr, "LD (HL), A");
			break;

		case 0x78: // LD A, B
			sprintf(instStr, "LD A, B");
			break;

		case 0x79: // LD A, C
			sprintf(instStr, "LD A, C");
			break;

		case 0x7A: // LD A, D
			sprintf(instStr, "LD A, D");
			break;

		case 0x7B: // LD A, E
			sprintf(instStr, "LD A, E");
			break;

		case 0x7C: // LD A, H
			sprintf(instStr, "LD A, H");
			break;

		case 0x7D: // LD A, L
			sprintf(instStr, "LD A, L");
			break;

		case 0x7E: // LD A, (HL)
			sprintf(instStr, "LD A, (HL)");
			break;

		case 0x7F: // LD A, A
			sprintf(instStr, "LD A, A");
			break;

		case 0x80: // ADD A, B
			sprintf(instStr, "ADD A, B");
			break;

		case 0x81: // ADD A, C
			sprintf(instStr, "ADD A, C");
			break;

		case 0x82: // ADD A, D
			sprintf(instStr, "ADD A, D");
			break;

		case 0x83: // ADD A, E
			sprintf(instStr, "ADD A, E");
			break;

		case 0x84: // ADD A, H
			sprintf(instStr, "ADD A, H");
			break;

		case 0x85: // ADD A, L
			sprintf(instStr, "ADD A, L");
			break;

		case 0x86: // ADD A, (HL)
			sprintf(instStr, "ADD A, (HL)");
			break;

		case 0x87: // ADD A, A
			sprintf(instStr, "ADD A, A");
			break;

		case 0x88: // ADC A, B
			sprintf(instStr, "ADC A, B");
			break;

		case 0x89: // ADC A, C
			sprintf(instStr, "ADC A, C");
			break;

		case 0x8A: // ADC A, D
			sprintf(instStr, "ADC A, D");
			break;

		case 0x8B: // ADC A, E
			sprintf(instStr, "ADC A, E");
			break;

		case 0x8C: // ADC A, H
			sprintf(instStr, "ADC A, H");
			break;

		case 0x8D: // ADC A, L
			sprintf(instStr, "ADC A, L");
			break;

		case 0x8E: // ADC A, (HL)
			sprintf(instStr, "ADC A, (HL)");
			break;

		case 0x8F: // ADC A, A
			sprintf(instStr, "ADC A, A");
			break;

		case 0x90: // SUB A, B
			sprintf(instStr, "SUB A, B");
			break;

		case 0x91: // SUB A, C
			sprintf(instStr, "SUB A, C");
			break;

		case 0x92: // SUB A, D
			sprintf(instStr, "SUB A, D");
			break;

		case 0x93: // SUB A, E
			sprintf(instStr, "SUB A, E");
			break;

		case 0x94: // SUB A, H
			sprintf(instStr, "SUB A, H");
			break;

		case 0x95: // SUB A, L
			sprintf(instStr, "SUB A, L");
			break;

		case 0x96: // SUB A, (HL)
			sprintf(instStr, "SUB A, (HL)");
			break;

		case 0x97: // SUB A, A
			sprintf(instStr, "SUB A, A");
			break;

		case 0x98: // SBC A, B
			sprintf(instStr, "SBC A, B");
			break;

		case 0x99: // SBC A, C
			sprintf(instStr, "SBC A, C");
			break;

		case 0x9A: // SBC A, D
			sprintf(instStr, "SBC A, D");
			break;

		case 0x9B: // SBC A, E
			sprintf(instStr, "SBC A, E");
			break;

		case 0x9C: // SBC A, H
			sprintf(instStr, "SBC A, H");
			break;

		case 0x9D: // SBC A, L
			sprintf(instStr, "SBC A, L");
			break;

		case 0x9E: // SBC A, (HL)
			sprintf(instStr, "SBC A, (HL)");
			break;

		case 0x9F: // SBC A, A
			sprintf(instStr, "SBC A, A");
			break;

		case 0xA0: // AND B
			sprintf(instStr, "AND B");
			break;

		case 0xA1: // AND C
			sprintf(instStr, "AND C");
			break;

		case 0xA2: // AND D
			sprintf(instStr, "AND D");
			break;

		case 0xA3: // AND E
			sprintf(instStr, "AND E");
			break;

		case 0xA4: // AND H
			sprintf(instStr, "AND H");
			break;

		case 0xA5: // AND L
			sprintf(instStr, "AND L");
			break;

		case 0xA6: // AND (HL)
			sprintf(instStr, "AND (HL)");
			break;

		case 0xA7: // AND A
			sprintf(instStr, "AND A");
			break;

		case 0xA8: // XOR B
			sprintf(instStr, "XOR B");
			break;

		case 0xA9: // XOR C
			sprintf(instStr, "XOR C");
			break;

		case 0xAA: // XOR D
			sprintf(instStr, "XOR D");
			break;

		case 0xAB: // XOR E
			sprintf(instStr, "XOR E");
			break;

		case 0xAC: // XOR H
			sprintf(instStr, "XOR H");
			break;

		case 0xAD: // XOR L
			sprintf(instStr, "XOR L");
			break;

		case 0xAE: // XOR (HL)
			sprintf(instStr, "XOR (HL)");
			break;

		case 0xAF: // XOR A
			sprintf(instStr, "XOR A");
			break;

		case 0xB0: // OR B
			sprintf(instStr, "OR B");
			break;

		case 0xB1: // OR C
			sprintf(instStr, "OR C");
			break;

		case 0xB2: // OR D
			sprintf(instStr, "OR D");
			break;

		case 0xB3: // OR E
			sprintf(instStr, "OR E");
			break;

		case 0xB4: // OR H
			sprintf(instStr, "OR H");
			break;

		case 0xB5: // OR L
			sprintf(instStr, "OR L");
			break;

		case 0xB6: // OR (HL)
			sprintf(instStr, "OR (HL)");
			break;

		case 0xB7: // OR A
			sprintf(instStr, "OR A");
			break;

		case 0xB8: // CP B
			sprintf(instStr, "CP B");
			break;

		case 0xB9: // CP C
			sprintf(instStr, "CP C");
			break;

		case 0xBA: // CP D
			sprintf(instStr, "CP D");
			break;

		case 0xBB: // CP E
			sprintf(instStr, "CP E");
			break;

		case 0xBC: // CP H
			sprintf(instStr, "CP H");
			break;

		case 0xBD: // CP L
			sprintf(instStr, "CP L");
			break;

		case 0xBE: // CP (HL)
			sprintf(instStr, "CP (HL)");
			break;

		case 0xBF: // CP A
			sprintf(instStr, "CP A");
			break;

		case 0xC0: // RET NZ
			sprintf(instStr, "RET NZ");
			break;

		case 0xC1: // POP BC
			sprintf(instStr, "POP BC");
			break;

		case 0xC2: // JP NZ, nn
			sprintf(instStr, "JP NZ, 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;

		case 0xC3: // JP nn
			sprintf(instStr, "JP 0x%04X", emu->memory.readWord(npc));
			break;

		case 0xC4: // CALL NZ, nn
			sprintf(instStr, "CALL NZ, 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;

		case 0xC5: // PUSH BC
			sprintf(instStr, "PUSH BC");
			break;

		case 0xC6: // ADD A, n
			sprintf(instStr, "ADD A, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xC7: // RST 00
			sprintf(instStr, "RST 00");
			break;

		case 0xC8: // RET Z
			sprintf(instStr, "RET Z");
			break;

		case 0xC9: // RET
			sprintf(instStr, "RET");
			break;

		case 0xCA: // JP Z, nn
			sprintf(instStr, "JP Z, 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;

		case 0xCB: // Extra instructions
			extraOp = emu->memory.readWord(npc);
			npc++;
			// RLC
			if (extraOp <= 0x07) {
				sprintf(instStr, "RLC %s", numToRegName(extraOp & 0x07));
			}
			// RRC
			else if (extraOp > 0x07 && extraOp <= 0x0F) {
				sprintf(instStr, "RRC %s", numToRegName(extraOp & 0x07));
			}
			// RL
			else if (extraOp > 0x0F && extraOp <= 0x17) {
				sprintf(instStr, "RL %s", numToRegName(extraOp & 0x07));
			}
			// RR
			else if (extraOp > 0x17 && extraOp <= 0x1F) {
				sprintf(instStr, "RR %s", numToRegName(extraOp & 0x07));
			}
			// SLA
			else if (extraOp > 0x1F && extraOp <= 0x27) {
				sprintf(instStr, "SLA %s", numToRegName(extraOp & 0x07));
			}
			// SRA
			else if (extraOp > 0x27 && extraOp <= 0x2F) {
				sprintf(instStr, "SRA %s", numToRegName(extraOp & 0x07));
			}
			// SWAP
			else if (extraOp > 0x2F && extraOp <= 0x37) {
				sprintf(instStr, "SWAP %s", numToRegName(extraOp & 0x07));
			}
			// SRL
			else if (extraOp > 0x37 && extraOp <= 0x3F) {
				sprintf(instStr, "SRL %s", numToRegName(extraOp & 0x07));
			}
			// BIT
			else if (extraOp > 0x3F && extraOp <= 0x7F) {
				bitnr = (extraOp - 0x40) / 8;
				sprintf(instStr, "BIT %s, %d", numToRegName(extraOp & 0x07), bitnr);
			}
			// RES
			else if (extraOp > 0x7F && extraOp <= 0xBF) {
				bitnr = (extraOp - 0x80) / 8;
				sprintf(instStr, "RES %s, %d", numToRegName(extraOp & 0x07), bitnr);
			}
			// SET
			else {
				bitnr = (extraOp - 0xC0) / 8;
				sprintf(instStr, "SET %s, %d", numToRegName(extraOp & 0x07), bitnr);
			}
			break;

		case 0xCC: // CALL Z, nn
			sprintf(instStr, "CALL Z, 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;

		case 0xCD: // CALL nn
			sprintf(instStr, "CALL 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;

		case 0xCE: // ADC A, n
			sprintf(instStr, "ADC A, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xCF: // RST 08
			sprintf(instStr, "RST 08");
			break;

		case 0xD0: // RET NC
			sprintf(instStr, "RET NC");
			break;

		case 0xD1: // POP DE
			sprintf(instStr, "POP DE");
			break;

		case 0xD2: // JP NC, nn
			sprintf(instStr, "JP NC, 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;

		case 0xD3: // unimplemented
			sprintf(instStr, "(UNIMP)");
			break;

		case 0xD4: // CALL NC, nn
			sprintf(instStr, "CALL NC, 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;

		case 0xD5: // PUSH DE
			sprintf(instStr, "PUSH DE");
			break;

		case 0xD6: // SUB A, n
			sprintf(instStr, "SUB A, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xD7: // RST 10
			sprintf(instStr, "RST 10");
			break;

		case 0xD8: // RET C
			sprintf(instStr, "RET C");
			break;

		case 0xD9: // RETI
			sprintf(instStr, "RETI");
			break;

		case 0xDA: // JP C, nn
			sprintf(instStr, "JP C, 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;

		case 0xDB: // unimplemented
			sprintf(instStr, "(UNIMP)");
			break;

		case 0xDC: // CALL C, nn
			sprintf(instStr, "CALL C, 0x%04X", emu->memory.readWord(npc));
			npc += 2;
			break;

		case 0xDD: // unimplemented
			sprintf(instStr, "(UNIMP)");
			break;

		case 0xDE: // SBC A, n
			sprintf(instStr, "SBC A, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xDF: // RST 18
			sprintf(instStr, "RST 18");
			break;

		case 0xE0: // LDH (n), A
			sprintf(instStr, "LD (0xFF%02X), A", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xE1: // POP HL
			sprintf(instStr, "POP HL");
			break;

		case 0xE2: // LDH (C), A
			sprintf(instStr, "LDH (C), A");
			break;

		case 0xE3: // unimplemented
		case 0xE4: // unimplemented
			sprintf(instStr, "(UNIMP)");
			break;

		case 0xE5: // PUSH HL
			sprintf(instStr, "PUSH HL");
			break;

		case 0xE6: // AND n
			sprintf(instStr, "AND 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xE7: // RST 20
			sprintf(instStr, "RST 20");
			break;

		case 0xE8: // ADD SP, n
			sprintf(instStr, "ADD SP, 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xE9: // JP (HL)
			sprintf(instStr, "JP (HL)");
			break;

		case 0xEA: // LD (nn), A
			sprintf(instStr, "LD (0x%04X), A", emu->memory.readWord(npc));
			npc += 2;
			break;

		case 0xEB: // unimplemented
		case 0xEC: // unimplemented
		case 0xED: // unimplemented
			sprintf(instStr, "(UNIMP)");
			break;

		case 0xEE: // XOR n
			sprintf(instStr, "XOR 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xEF: // RST 28
			sprintf(instStr, "RST 28");
			break;

		case 0xF0: // LDH A, (n)
			sprintf(instStr, "LDH A, (0x%02X)", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xF1: // POP AF
			sprintf(instStr, "POP AF");
			break;

		case 0xF2: // LDH a, (C)?
			sprintf(instStr, "LDH a, (C)");
			break;

		case 0xF3: // DI
			sprintf(instStr, "DI");
			break;

		case 0xF4: // unimplemented
			sprintf(instStr, "(UNIMP)");
			break;

		case 0xF5: // PUSH AF
			sprintf(instStr, "PUSH AF");
			break;

		case 0xF6: // OR n
			sprintf(instStr, "OR 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xF7: // RST 30
			sprintf(instStr, "RST 30");
			break;

		case 0xF8: // LDHL SP, d
			sprintf(instStr, "LD HL, SP+0x%02X", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xF9: // LD SP, HL
			sprintf(instStr, "LD SP, HL");
			break;

		case 0xFA: // LD A, (nn)
			sprintf(instStr, "LD A, (0x%04X)", emu->memory.readWord(npc));
			npc += 2;
			break;

		case 0xFB: // EI
			sprintf(instStr, "EI");
			break;

		case 0xFC: // unimplemented
		case 0xFD: // unimplemented
			sprintf(instStr, "(UNIMP)");
			break;

		case 0xFE: // CP n
			sprintf(instStr, "CP 0x%02X", emu->memory.readByte(npc));
			npc++;
			break;

		case 0xFF: // RST 38
			npc = 0x38;
			sprintf(instStr, "RST 38");
			break;
	}

	return npc;
}

void CPU::disassemble(uint16_t pc, size_t instCnt, char *buf) {
	// TODO: Assumes max inst str len is 25
	char instBuf[25];
	uint16_t npc;

	for (int i = 0; i < instCnt; ++i) {
		npc = instructionToString(pc, instBuf);
		sprintf(buf + strlen(buf), "%c%04X: %s\n", (i==0)?'>':' ', pc, instBuf);

		pc = npc;
	}
}
