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

void initCPU() {
	cpu.r.a = 0;
	cpu.r.b = 0;
	cpu.r.c = 0;
	cpu.r.d = 0;
	cpu.r.e = 0;
	cpu.r.h = 0;
	cpu.r.l = 0;
	cpu.r.f = 0;
	cpu.r.pc = 0;
	cpu.r.sp = 0;
	
	cpu.ints = 0;
	cpu.c = 0;
}

void doOpcodeADD(uint8_t val) {
	uint8_t tmp = cpu.r.a;
	cpu.r.a += val;
	cpu.r.f = (cpu.r.a < tmp) ? F_CARRY : 0;
	if(cpu.r.a == 0x00) {
		cpu.r.f |= F_ZERO;
	}
	if((cpu.r.a ^ val ^ tmp) & F_CARRY) {
		cpu.r.f |= F_HCARRY;
	}
	cpu.c += 1;
}

void doOpcodeADC(uint8_t val) {
	uint8_t tmp = cpu.r.a;
	cpu.r.a += val + ((cpu.r.f & F_CARRY) ? 1 : 0);
	cpu.r.f = (cpu.r.a < tmp) ? F_CARRY : 0;
	if(cpu.r.a == 0x00) {
		cpu.r.f |= F_ZERO;
	}
	if((cpu.r.a ^ val ^ tmp) & F_CARRY) {
		cpu.r.f |= F_HCARRY;
	}
	cpu.c += 1;
}

void doOpcodeSUB(uint8_t val) {
	uint8_t tmp = cpu.r.a;
	cpu.r.a -= val;
	cpu.r.f = (cpu.r.a > tmp) ? (F_CARRY | F_SUBSTRACT) : F_CARRY;
	if(cpu.r.a == 0x00) {
		cpu.r.f |= F_ZERO;
	}
	if((cpu.r.a ^ val ^ tmp) & F_CARRY) {
		cpu.r.f |= F_HCARRY;
	}
	cpu.c += 1;
}

void doOpcodeSBC(uint8_t val) {
	uint8_t tmp = cpu.r.a;
	cpu.r.a -= val + ((cpu.r.f & F_CARRY) ? 1 : 0);
	cpu.r.f = (cpu.r.a > tmp) ? (F_CARRY | F_SUBSTRACT) : F_CARRY;
	if(cpu.r.a == 0x00) {
		cpu.r.f |= F_ZERO;
	}
	if((cpu.r.a ^ val ^ tmp) & F_CARRY) {
		cpu.r.f |= F_HCARRY;
	}
	cpu.c += 1;
}

void doOpcodeAND(uint8_t val) {
	cpu.r.a &= val;
	cpu.r.f = cpu.r.a ? 0x00 : F_ZERO;
	cpu.c += 1;
}

void doOpcodeXOR(uint8_t val) {
	cpu.r.a ^= val;
	cpu.r.f = cpu.r.a ? 0x00 : F_ZERO;
	cpu.c += 1;
}

void doOpcodeOR(uint8_t val) {
	cpu.r.a |= val;
	cpu.r.f = cpu.r.a ? 0x00 : F_ZERO;
	cpu.c += 1;
}

void doOpcodeCP(uint8_t val) {
	uint8_t tmp;
	tmp = cpu.r.a;
	tmp -= val;
	cpu.r.f = ((tmp > cpu.r.a) ? (F_SUBSTRACT & F_CARRY) : F_SUBSTRACT);
	if(tmp == 0x00) {
		cpu.r.f |= F_ZERO;
	}
	if((cpu.r.a ^ val ^ tmp) & F_CARRY) {
		cpu.r.f |= F_HCARRY;
	}
	cpu.c += 1;
}

void doOpcodeRST(uint8_t val) {
	printf("RST (%d) called.\n", val);
	exit(1);
}

void doOpcodeUNIMP() {
	printf("Unimplemented instruction (0x%02X) at 0x%04X.\n", readByte(cpu.r.pc-1), cpu.r.pc-1);
	//exit(1);
}

void printRegisters() {
	printf("{a=%02X,b=%02X,c=%02X,d=%02X,e=%02X,h=%02X,l=%02X,pc=%04X,sp=%04X}, cycles=%d\n",
		cpu.r.a, cpu.r.b, cpu.r.c, cpu.r.d, cpu.r.e, cpu.r.h, cpu.r.l, cpu.r.pc, cpu.r.sp, cpu.c);
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

void doExtraOP(uint8_t inst) {
	int tmp, oper, bit;
	uint8_t *ptr;
	
	if(inst & 0x80) { // bit operations
		bit = (inst & 0x38) >> 3; // bit nr.
		ptr = numToRegPtr(inst & 0x07);
		if(inst & 0x40) { // SET
			if(!ptr) {
				writeWord(readWord((cpu.r.h << 8) + cpu.r.l) | (0x01 << bit), (cpu.r.h << 8) + cpu.r.l);
				cpu.c += 4;
			} else {
				*ptr |= (0x01 << bit);
				cpu.c += 2;
			}
		} else { // RES
			if(!ptr) {
				writeWord(readWord((cpu.r.h << 8) + cpu.r.l) & ~(0x01 << bit), (cpu.r.h << 8) + cpu.r.l);
				cpu.c += 4;
			} else {
				*ptr &= ~(0x01 << bit);
				cpu.c += 2;
			}
		}
	}
	else {
		bit = (inst & 0x08) ? 1 : 0;
		ptr = numToRegPtr(inst & 0x07);
		
		if(!ptr) {
			oper = readWord((cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
		} else {
			oper = *ptr;
		}
		
		switch((inst & 0x30) >> 4) {
			case 0x00: // RRC D / SLC D
				if(bit) {
					tmp = (oper & 0x01);
					oper = (oper >> 1) + (tmp ? 0x80 : 0);
				} else {
					tmp = (oper & 0x80);
					oper = (oper << 1) + (tmp ? 0x01 : 0);
				}
				cpu.r.f = (oper ? 0x00 : F_ZERO);
				cpu.r.f = (cpu.r.f & ~F_CARRY) + (tmp ? F_CARRY : 0);
				cpu.c += 2;
				break;
				
			case 0x01: // RR D / RL D
				if(bit) {
					tmp = (oper & 0x01) ? F_CARRY : 0;
					oper = (oper >> 1) + ((cpu.r.f & F_CARRY) ? 0x80 : 0);
				} else {
					tmp = (oper & 0x80) ? F_CARRY : 0;
					oper = (oper << 1) + ((cpu.r.f & F_CARRY) ? 0x01 : 0);
				}	
				cpu.r.f = (oper ? 0x00 : F_ZERO);
				cpu.r.f = (cpu.r.f & ~F_CARRY) + tmp;
				cpu.c += 2;
				break;
				
			case 0x02: // SRA D / SLA D
				if(bit) {
					tmp = (oper & 0x01) ? F_CARRY : 0;
					oper = (oper >> 1) + (oper & 0x80);
				} else {
					tmp = (oper & 0x80) ? F_CARRY : 0;
					oper = oper << 1;
				}
				cpu.r.f = (oper ? 0x00 : F_ZERO);
				cpu.r.f = (cpu.r.f & ~F_CARRY) + tmp;
				cpu.c += 2;
				break;
				
			case 0x03:
				if(bit) { // SRL D
					tmp = (oper & 0x01) ? F_CARRY : 0;
					oper = oper >> 1;
					cpu.r.f = (oper ? 0x00 : F_ZERO);
					cpu.r.f = (cpu.r.f & ~F_CARRY) + tmp;
					cpu.c += 2;
				} else { // SWAP D
					tmp = oper;
					oper = ((tmp & 0x0F) << 4) | ((tmp & 0xF0) >> 4);
					cpu.r.f = (oper ? 0x00 : F_ZERO);
					cpu.c += 1;
				}
				break;	
		}
		
		if(!ptr) {
			writeWord((cpu.r.h << 8) + cpu.r.l, oper);
		} else {
			*ptr = oper;
		}
	}
}

int executeInstruction() {
	uint8_t inst;
	uint32_t utmp32, utmp32_2;
	int16_t tmp16, tmp16_2;
	uint16_t utmp16, utmp16_2;
	int8_t tmp8, tmp8_2;
	uint8_t utmp8, utmp8_2;
	
	inst = readByte(cpu.r.pc);
	if(settings.debug && cpu.c%100==0) {
		printf("Read instruction 0x%02X at 0x%04X. (%d)\n", inst, cpu.r.pc, cpu.r.pc);
	}
	cpu.r.pc++;
	cpu.dc = cpu.c;
	
	switch(inst) {
		case 0x00: // NOP
			cpu.c += 1;
			break;
			
		case 0x01: // LD BC, nn
			cpu.r.c = readByte(cpu.r.pc);
			cpu.r.b = readByte(cpu.r.pc+1);
			cpu.r.pc += 2;
			cpu.c += 3;
			break;
			
		case 0x02: // LD (BC), A
			writeByte(cpu.r.a, (cpu.r.b << 8) + cpu.r.c);
			cpu.c += 2;
			break;
			
		case 0x03: // INC BC
			cpu.r.c++;
			if(!cpu.r.c) cpu.r.b++;
			cpu.c += 1;
			break;
			
		case 0x04: // INC B
			cpu.r.b++;
			cpu.r.f = 0;
			if(!cpu.r.b) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x05: // DEC B
			cpu.r.b--;
			cpu.r.f = 0;
			if(!cpu.r.b) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x06: // LD B, n
			cpu.r.b = readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.c += 2;
			break;
			
		case 0x07: // RLC A
			utmp8 = (cpu.r.a & 0x80);
			cpu.r.a = (cpu.r.a << 1) + (utmp8 ? 1 : 0);
			cpu.r.f = (cpu.r.f & ~F_CARRY) + (utmp8 ? F_CARRY : 0);
			cpu.c += 1;
			break;
			
		case 0x08: // LD (nn), SP
			utmp8 = readByte(cpu.r.pc);
			utmp8_2 = readByte(cpu.r.pc+1);
			writeWord(cpu.r.sp, (utmp8 << 8) + utmp8_2);
			cpu.r.pc += 2;
			cpu.c += 2; // TODO: Geen idee of dit klopt
			break;
			
		case 0x09: // ADD HL, BC
			utmp32 = (cpu.r.h << 8) + cpu.r.l;
			utmp32 += (cpu.r.b << 8) + cpu.r.c;
			if(utmp32 > 0xFFFF) cpu.r.f |= F_CARRY;
			else cpu.r.f &= ~F_CARRY;
			cpu.r.h = (utmp32 >> 8) & 255;
			cpu.r.l = (utmp32 & 255);
			cpu.c += 3;
			break;
			
		case 0x0A: // LD A, (BC)
			cpu.r.a = readByte((cpu.r.b << 8) + cpu.r.c);
			cpu.c += 2;
			break;
			
		case 0x0B: // DEC BC
			cpu.r.c--;
			if(cpu.r.c == 0xFF) cpu.r.b--;
			cpu.c += 1;
			break;
			
		case 0x0C: // INC C
			cpu.r.c++;
			cpu.r.f = 0;
			if(!cpu.r.c) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x0D: // DEC C
			cpu.r.c--;
			cpu.r.f = 0;
			if(!cpu.r.c) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x0E: // LD C, n
			cpu.r.c = readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.c += 2;
			break;
			
		case 0x0F: // RRC A
			utmp8 = (cpu.r.a & 1);
			cpu.r.a = (cpu.r.a >> 1) + (utmp8 ? 0x80 : 0);
			cpu.r.f = (cpu.r.f & ~F_CARRY) & (utmp8 ? F_CARRY : 0);
			cpu.c += 1;
			break;
			
		case 0x10: // STOP
			// TODO: Dit nog implementeren
			cpu.c += 1;
			break;
			
		case 0x11: // LD DE, nn
			cpu.r.e = readByte(cpu.r.pc);
			cpu.r.d = readByte(cpu.r.pc+1);
			cpu.r.pc += 2;
			cpu.c += 3;
			break;
		
		case 0x12: // LD (DE), A
			writeByte(cpu.r.a, (cpu.r.d << 8) + cpu.r.e);
			cpu.c += 2;
			break;
			
		case 0x13: // INC DE
			cpu.r.e++;
			if(!cpu.r.e) cpu.r.d++;
			cpu.c += 1;
			break;
			
		case 0x14: // INC D
			cpu.r.d++;
			cpu.r.f = 0;
			if(!cpu.r.d) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x15: // DEC D
			cpu.r.d--;
			cpu.r.f = 0;
			if(!cpu.r.d) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x16: // LD D, n
			cpu.r.d = readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.c += 2;
			break;
			
		case 0x17: // RL A
			utmp8 = (cpu.r.f & F_CARRY);
			cpu.r.a = (cpu.r.a << 1) + (utmp8 ? 1 : 0);
			cpu.r.f = (cpu.r.f & ~F_CARRY) & (utmp8 ? F_CARRY : 0);
			cpu.c += 1;
			break;
		
		case 0x18: // JR n
			tmp8 = (int8_t)readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.r.pc += tmp8;
			cpu.c += 3;
			break;
			
		case 0x19: // ADD HL, DE
			utmp16 = (cpu.r.h << 8) + cpu.r.l;
			utmp16 += (cpu.r.d << 8) + cpu.r.e;
			if(utmp16 > 0xFFFF) cpu.r.f |= F_CARRY;
			else cpu.r.f &= ~F_CARRY;
			cpu.r.h = (utmp16 >> 8) & 255;
			cpu.r.l = (utmp16 & 255);
			cpu.c += 3;
			break;
			
		case 0x1A: // LD A, (DE)
			cpu.r.a = readByte((cpu.r.d << 8) + cpu.r.e);
			cpu.c += 2;
			break;
			
		case 0x1B: // DEC DE
			cpu.r.e--;
			if(cpu.r.e == 0xFF) cpu.r.d--;
			cpu.c += 1;
			break;
			
		case 0x1C: // INC E
			cpu.r.e++;
			cpu.r.f = 0;
			if(!cpu.r.e) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x1D: // DEC E
			cpu.r.e--;
			cpu.r.f = 0;
			if(!cpu.r.e) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x1E: // LD E, n
			cpu.r.e = readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.c += 2;
			break;
			
		case 0x1F: // RR a
			utmp8 = (cpu.r.f & F_CARRY);
			cpu.r.a = (cpu.r.a >> 1) + (utmp8 ? 0x80 : 0);
			cpu.r.f = (cpu.r.f & ~F_CARRY) & (utmp8 ? F_CARRY : 0);
			cpu.c += 1;
			break;
			
		case 0x20: // JR NZ, n
			tmp8 = (int8_t)readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.c += 2;
			if(!(cpu.r.f & F_ZERO)) {
				cpu.r.pc += tmp8;
				cpu.c++;
			}
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
			if(cpu.r.l == 0x00) {
				cpu.r.h++;
			}
			cpu.c += 2;
			break;
			
		case 0x23: // INC HL
			cpu.r.l++;
			if(!cpu.r.l) cpu.r.h++;
			cpu.c += 1;
			break;
			
		case 0x24: // INC H
			cpu.r.h++;
			cpu.r.f = 0;
			if(!cpu.r.h) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x25: // DEC H
			cpu.r.h--;
			cpu.r.f = 0;
			if(!cpu.r.h) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x26: // LD H, n
			cpu.r.h = readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.c += 2;
			break;
			
		case 0x27: // DAA
			utmp8 = cpu.r.a;
			if((cpu.r.f & F_HCARRY) || (cpu.r.a & 0x0F) > 0x09) {
				cpu.r.a += 0x06;
			}
			cpu.r.f &= ~F_CARRY;
			if((cpu.r.f & F_HCARRY) || (utmp8 > 0x99)) {
				cpu.r.a += 0x60;
				cpu.r.f |= F_CARRY;
			}
			cpu.c += 1;
			break;
			
		case 0x28: // JR Z, n
			tmp8 = (int8_t)readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.c += 2;
			if((cpu.r.f & F_ZERO)) {
				cpu.r.pc += tmp8;
				cpu.c++;
			}
			break;
			
		case 0x29: // ADD HL, HL
			utmp32 = (cpu.r.h << 8) + cpu.r.l;
			utmp32 += utmp32;
			if(utmp32 > 0xFFFF) cpu.r.f |= F_CARRY;
			else cpu.r.f &= ~F_CARRY;
			cpu.r.h = (utmp32 >> 8) & 255;
			cpu.r.l = (utmp32 & 255);
			cpu.c += 3;
			break;
			
		case 0x2A: // LDI A, (HL)
			cpu.r.a = readByte((cpu.r.h << 8) + cpu.r.l);
			cpu.r.l++;
			if(cpu.r.l == 0x00) {
				cpu.r.h++;
			}
			cpu.c += 2;
			break;
			
		case 0x2B: // DEC HL
			cpu.r.l--;
			if(cpu.r.l == 0xFF) cpu.r.h--;
			cpu.c += 1;
			break;
			
		case 0x2C: // INC L
			cpu.r.l++;
			cpu.r.f = 0;
			if(!cpu.r.l) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x2D: // DEC L
			cpu.r.l--;
			cpu.r.f = 0;
			if(!cpu.r.l) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x2E: // LD L, n
			cpu.r.l = readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.c += 2;
			break;
			
		case 0x2F: // CPL
			cpu.r.a = ~cpu.r.a;
			cpu.r.f = cpu.r.a ? 0x00 : F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x30: // JR NC, n
			tmp8 = (int8_t)readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.c += 2;
			if(!(cpu.r.f & F_CARRY)) {
				cpu.r.pc += tmp8;
				cpu.c++;
			}
			break;
			
		case 0x31: // LD SP, nn
			cpu.r.sp = readWord(cpu.r.pc);
			cpu.r.pc += 2;
			cpu.c += 3;
			break;
			
		case 0x32: // LDD (HL), A
			writeByte(cpu.r.a, (cpu.r.h << 8) + cpu.r.l);
			cpu.r.l--;
			if(cpu.r.l == 0xFF) {
				cpu.r.h--;
			}
			cpu.c += 2;
			break;
			
		case 0x33: // INC SP
			cpu.r.sp++;
			cpu.c += 1;
			break;
			
		case 0x34: // INC (HL)
			utmp16 = (uint16_t)(readByte((cpu.r.h << 8) + cpu.r.l) + 1);
			writeByte((uint8_t)(utmp16 & 0xFF), (cpu.r.h << 8) + cpu.r.l);
			cpu.r.f = utmp16 ? 0x00 : F_ZERO;
			cpu.c += 3;
			break;
			
		case 0x35: // DEC (HL)
			utmp16 = (uint16_t)(readByte((cpu.r.h << 8) + cpu.r.l) - 1);
			writeByte((uint8_t)(utmp16 & 0xFF), (cpu.r.h << 8) + cpu.r.l);
			cpu.r.f = utmp16 ? 0x00 : F_ZERO;
			cpu.c += 3;
			break;
			
		case 0x36: // LD (HL), n
			writeByte(readByte(cpu.r.pc), (cpu.r.h << 8) + cpu.r.l);
			cpu.r.pc++;
			cpu.c += 3;
			break;
			
		case 0x37: // SCF
			cpu.r.f |= F_CARRY;
			cpu.c += 1;
			break;
			
		case 0x38: // JR C, n
			tmp8 = (int8_t)readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.c += 2;
			if((cpu.r.f & F_CARRY)) {
				cpu.r.pc += tmp8;
				cpu.c++;
			}
			break;
			
		case 0x39: // ADD HL, SP
			utmp32 = (cpu.r.h << 8) + cpu.r.l;
			utmp32 += cpu.r.sp;
			if(utmp32 > 0xFFFF) cpu.r.f |= F_CARRY;
			else cpu.r.f &= ~F_CARRY;
			cpu.r.h = (utmp32 >> 8) & 255;
			cpu.r.l = (utmp32 & 255);
			cpu.c += 3;
			break;
			
		case 0x3A: // LDD A, (HL)
			cpu.r.a = readByte((cpu.r.h << 8) + cpu.r.l);
			cpu.r.l--;
			if(cpu.r.l == 0xFF) {
				cpu.r.h--;
			}
			cpu.c += 2;
			break;
			
		case 0x3B: // DEC SP
			cpu.r.sp--;
			cpu.c += 1;
			break;
			
		case 0x3C: // INC A
			cpu.r.a++;
			cpu.r.f = 0;
			if(!cpu.r.a) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x3D: // DEC A
			cpu.r.a--;
			cpu.r.f = 0;
			if(!cpu.r.a) cpu.r.f |= F_ZERO;
			cpu.c += 1;
			break;
			
		case 0x3E: // LD A, n
			cpu.r.a = readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.c += 2;
			break;
			
		case 0x3F: // CCF (actually toggles)
			utmp8 = (cpu.r.f & F_CARRY) ? 0x00 : F_CARRY;
			cpu.r.f = (cpu.r.f & ~F_CARRY) + utmp8;
			cpu.c += 1;
			break;
			
		case 0x40: // LD B, B
			cpu.r.b = cpu.r.b; // Fry: Not sure if meaningful, but emulate anyway.
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
			cpu.r.c = cpu.r.c;
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
			cpu.r.d = cpu.r.d;
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
			cpu.r.e = cpu.r.e;
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
			cpu.r.h = cpu.r.h;
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
			cpu.r.l = cpu.r.l;
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
			// TODO
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
			writeByte(cpu.r.a, (cpu.r.h << 8) + cpu.r.l);
			cpu.c += 2;
			break;

		case 0x7F: // LD A, A
			cpu.r.a = cpu.r.a;
			cpu.c += 1;
			break;

		case 0x80: // ADD A, B
			doOpcodeADD(cpu.r.b);
			break;

		case 0x81: // ADD A, C
			doOpcodeADD(cpu.r.c);
			break;

		case 0x82: // ADD A, D
			doOpcodeADD(cpu.r.d);
			break;

		case 0x83: // ADD A, E
			doOpcodeADD(cpu.r.e);
			break;

		case 0x84: // ADD A, H
			doOpcodeADD(cpu.r.h);
			break;

		case 0x85: // ADD A, L
			doOpcodeADD(cpu.r.l);
			break;

		case 0x86: // ADD A, (HL)
			utmp8 = readByte((cpu.r.h << 8) + cpu.r.l);
			doOpcodeADD(utmp8);
			cpu.c += 1;
			break;

		case 0x87: // ADD A, A
			doOpcodeADD(cpu.r.a);
			break;

		case 0x88: // ADC A, B
			doOpcodeADC(cpu.r.b);
			break;

		case 0x89: // ADC A, C
			doOpcodeADC(cpu.r.c);
			break;

		case 0x8A: // ADC A, D
			doOpcodeADC(cpu.r.d);
			break;

		case 0x8B: // ADC A, E
			doOpcodeADC(cpu.r.e);
			break;

		case 0x8C: // ADC A, H
			doOpcodeADC(cpu.r.h);
			break;

		case 0x8D: // ADC A, L
			doOpcodeADC(cpu.r.l);
			break;

		case 0x8E: // ADC A, (HL)
			utmp8 = readByte((cpu.r.h << 8) + cpu.r.l);
			doOpcodeADC(utmp8);
			cpu.c += 1;
			break;

		case 0x8F: // ADC A, A
			doOpcodeADC(cpu.r.a);
			break;

		case 0x90: // SUB A, B
			doOpcodeSUB(cpu.r.b);
			break;

		case 0x91: // SUB A, C
			doOpcodeSUB(cpu.r.c);
			break;

		case 0x92: // SUB A, D
			doOpcodeSUB(cpu.r.d);
			break;

		case 0x93: // SUB A, E
			doOpcodeSUB(cpu.r.e);
			break;

		case 0x94: // SUB A, H
			doOpcodeSUB(cpu.r.h);
			break;

		case 0x95: // SUB A, L
			doOpcodeSUB(cpu.r.l);
			break;

		case 0x96: // SUB A, (HL)
			utmp8 = readByte((cpu.r.h << 8) + cpu.r.l);
			doOpcodeSUB(utmp8);
			cpu.c += 1;
			break;

		case 0x97: // SUB A, A
			doOpcodeSUB(cpu.r.a);
			break;

		case 0x98: // SBC A, B
			doOpcodeSBC(cpu.r.b);
			break;

		case 0x99: // SBC A, C
			doOpcodeSBC(cpu.r.c);
			break;

		case 0x9A: // SBC A, D
			doOpcodeSBC(cpu.r.d);
			break;

		case 0x9B: // SBC A, E
			doOpcodeSBC(cpu.r.e);
			break;

		case 0x9C: // SBC A, H
			doOpcodeSBC(cpu.r.h);
			break;

		case 0x9D: // SBC A, L
			doOpcodeSBC(cpu.r.l);
			break;

		case 0x9E: // SBC A, (HL)
			utmp8 = readByte((cpu.r.h << 8) + cpu.r.l);
			doOpcodeSBC(utmp8);
			cpu.c += 1;
			break;

		case 0x9F: // SBC A, A
			doOpcodeSBC(cpu.r.a);
			break;

		case 0xA0: // AND B
			doOpcodeAND(cpu.r.b);
			break;

		case 0xA1: // AND C
			doOpcodeAND(cpu.r.c);
			break;

		case 0xA2: // AND D
			doOpcodeAND(cpu.r.d);
			break;

		case 0xA3: // AND E
			doOpcodeAND(cpu.r.e);
			break;

		case 0xA4: // AND H
			doOpcodeAND(cpu.r.h);
			break;

		case 0xA5: // AND L
			doOpcodeAND(cpu.r.l);
			break;

		case 0xA6: // AND (HL)
			utmp8 = readByte((cpu.r.h << 8) + cpu.r.l);
			doOpcodeAND(utmp8);
			cpu.c += 1;
			break;

		case 0xA7: // AND A
			doOpcodeAND(cpu.r.a);
			break;

		case 0xA8: // XOR B
			doOpcodeXOR(cpu.r.b);
			break;

		case 0xA9: // XOR C
			doOpcodeXOR(cpu.r.c);
			break;

		case 0xAA: // XOR D
			doOpcodeXOR(cpu.r.d);
			break;

		case 0xAB: // XOR E
			doOpcodeXOR(cpu.r.e);
			break;

		case 0xAC: // XOR H
			doOpcodeXOR(cpu.r.h);
			break;

		case 0xAD: // XOR L
			doOpcodeXOR(cpu.r.l);
			break;

		case 0xAE: // XOR (HL)
			utmp8 = readByte((cpu.r.h << 8) + cpu.r.l);
			doOpcodeXOR(utmp8);
			cpu.c += 1;
			break;

		case 0xAF: // XOR A
			doOpcodeXOR(cpu.r.a);
			break;

		case 0xB0: // OR B
			doOpcodeOR(cpu.r.b);
			break;

		case 0xB1: // OR C
			doOpcodeOR(cpu.r.c);
			break;

		case 0xB2: // OR D
			doOpcodeOR(cpu.r.d);
			break;

		case 0xB3: // OR E
			doOpcodeOR(cpu.r.e);
			break;

		case 0xB4: // OR H
			doOpcodeOR(cpu.r.h);
			break;

		case 0xB5: // OR L
			doOpcodeOR(cpu.r.l);
			break;

		case 0xB6: // OR (HL)
			utmp8 = readByte((cpu.r.h << 8) + cpu.r.l);
			doOpcodeOR(utmp8);
			cpu.c += 1;
			break;

		case 0xB7: // OR A
			doOpcodeOR(cpu.r.a);
			break;

		case 0xB8: // CP B
			doOpcodeCP(cpu.r.b);
			break;

		case 0xB9: // CP C
			doOpcodeCP(cpu.r.c);
			break;

		case 0xBA: // CP D
			doOpcodeCP(cpu.r.d);
			break;

		case 0xBB: // CP E
			doOpcodeCP(cpu.r.e);
			break;

		case 0xBC: // CP H
			doOpcodeCP(cpu.r.h);
			break;

		case 0xBD: // CP L
			doOpcodeCP(cpu.r.l);
			break;

		case 0xBE: // CP (HL)
			utmp8 = readByte((cpu.r.h << 8) + cpu.r.l);
			doOpcodeCP(utmp8);
			cpu.c += 1;
			break;

		case 0xBF: // CP A
			doOpcodeCP(cpu.r.a);
			break;

		case 0xC0: // RET NZ
			cpu.r.c += 1;
			if(!(cpu.r.f & F_ZERO)) {
				cpu.r.pc = readWord(cpu.r.sp);
				cpu.r.sp += 2;
				cpu.r.c += 2;
			}
			break;

		case 0xC1: // POP BC
			cpu.r.c = readByte(cpu.r.sp);
			cpu.r.sp++;
			cpu.r.b = readByte(cpu.r.sp);
			cpu.r.sp++;
			cpu.c += 3;
			break;

		case 0xC2: // JP NZ, nn
			cpu.c += 3;
			if(!(cpu.r.f & F_ZERO)) {
				cpu.r.pc = readWord(cpu.r.pc);
				cpu.c += 1;
			} else {
				cpu.r.pc += 2;
			}
			break;

		case 0xC3: // JP nn
			cpu.r.pc = readWord(cpu.r.pc);
			cpu.c += 3;
			break;

		case 0xC4: // CALL NZ, nn
			cpu.c += 3;
			if(!(cpu.r.f & F_ZERO)) {
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc+2, cpu.r.sp);
				cpu.r.pc = readWord(cpu.r.pc);
				cpu.c += 2;
			} else {
				cpu.r.pc += 2;
			}
			break;

		case 0xC5: // PUSH BC
			cpu.r.sp--;
			writeByte(cpu.r.b, cpu.r.sp);
			cpu.r.sp--;
			writeByte(cpu.r.c, cpu.r.sp);
			cpu.c += 3;
			break;

		case 0xC6: // ADD A, n
			utmp8 = readByte(cpu.r.pc);
			cpu.r.pc++;
			doOpcodeADD(utmp8);
			cpu.c += 1;
			break;

		case 0xC7: // RST 0
			doOpcodeRST(0x00);
			break;

		case 0xC8: // RET Z
			cpu.r.c += 1;
			if(cpu.r.f & F_ZERO) {
				cpu.r.pc = readWord(cpu.r.sp);
				cpu.r.sp += 2;
				cpu.r.c += 2;
			}
			break;

		case 0xC9: // RET
			cpu.r.pc = readWord(cpu.r.sp);
			cpu.r.sp += 2;
			cpu.r.c += 3;
			break;

		case 0xCA: // JP Z, nn
			cpu.c += 3;
			if(cpu.r.f & F_ZERO) {
				cpu.r.pc = readWord(cpu.r.pc);
				cpu.c += 1;
			} else {
				cpu.r.pc += 2;
			}
			break;

		case 0xCB: // Extra instructions
			utmp8 = readByte(cpu.r.pc);
			cpu.r.pc++;
			doExtraOP(utmp8);
			break;

		case 0xCC: // CALL Z, nn
			cpu.c += 3;
			if(cpu.r.f & F_ZERO) {
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc+2, cpu.r.sp);
				cpu.r.pc = readWord(cpu.r.pc);
				cpu.c += 2;
			} else {
				cpu.r.pc += 2;
			}
			break;

		case 0xCD: // CALL nn
			cpu.r.sp -= 2;
			writeWord(cpu.r.pc+2, cpu.r.sp);
			cpu.r.pc = readWord(cpu.r.pc);
			cpu.c += 5;
			break;

		case 0xCE: // ADC A, n
			utmp8 = readByte(cpu.r.pc);
			cpu.r.pc++;
			doOpcodeADC(utmp8);
			cpu.c += 1;
			break;

		case 0xCF: // RST 8
			doOpcodeRST(0x08);
			break;

		case 0xD0: // RET NC
			cpu.r.c += 1;
			if(!(cpu.r.f & F_CARRY)) {
				cpu.r.pc = readWord(cpu.r.sp);
				cpu.r.sp += 2;
				cpu.r.c += 2;
			}
			break;

		case 0xD1: // POP DE
			cpu.r.e = readByte(cpu.r.sp);
			cpu.r.sp++;
			cpu.r.d = readByte(cpu.r.sp);
			cpu.r.sp++;
			cpu.c += 3;
			break;

		case 0xD2: // JP NC, nn
			cpu.c += 3;
			if(!(cpu.r.f & F_CARRY)) {
				cpu.r.pc = readWord(cpu.r.pc);
				cpu.c += 1;
			} else {
				cpu.r.pc += 2;
			}
			break;

		case 0xD3: // unimplemented
			doOpcodeUNIMP(cpu);
			break;

		case 0xD4: // CALL NC, nn
			cpu.c += 3;
			if(!(cpu.r.f & F_CARRY)) {
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc+2, cpu.r.sp);
				cpu.r.pc = readWord(cpu.r.pc);
				cpu.c += 2;
			} else {
				cpu.r.pc += 2;
			}
			break;

		case 0xD5: // PUSH DE
			cpu.r.sp--;
			writeByte(cpu.r.d, cpu.r.sp);
			cpu.r.sp--;
			writeByte(cpu.r.e, cpu.r.sp);
			cpu.c += 3;
			break;

		case 0xD6: // SUB A, n
			utmp8 = readByte(cpu.r.pc);
			cpu.r.pc++;
			doOpcodeSUB(utmp8);
			cpu.c += 1;
			break;

		case 0xD7: // RST 10
			doOpcodeRST(0x10);
			break;

		case 0xD8: // RET C
			cpu.r.c += 1;
			if(cpu.r.f & F_CARRY) {
				cpu.r.pc = readWord(cpu.r.sp);
				cpu.r.sp += 2;
				cpu.r.c += 2;
			}
			break;

		case 0xD9: // RETI
			cpu.ints = 1;
			cpu.r.pc = readWord(cpu.r.sp);
			cpu.r.sp += 2;
			cpu.r.c += 3;
			break;

		case 0xDA: // JP C, nn
			cpu.c += 3;
			if(cpu.r.f & F_CARRY) {
				cpu.r.pc = readWord(cpu.r.pc);
				cpu.c += 1;
			} else {
				cpu.r.pc += 2;
			}
			break;

		case 0xDB: // unimplemented
			doOpcodeUNIMP();
			break;

		case 0xDC: // CALL C, nn
			cpu.c += 3;
			if(cpu.r.f & F_CARRY) {
				cpu.r.sp -= 2;
				writeWord(cpu.r.pc+2, cpu.r.sp);
				cpu.r.pc = readWord(cpu.r.pc);
				cpu.c += 2;
			} else {
				cpu.r.pc += 2;
			}
			break;

		case 0xDD: // unimplemented
			doOpcodeUNIMP();
			break;

		case 0xDE: // SBC A, n
			utmp8 = readByte(cpu.r.pc);
			cpu.r.pc++;
			doOpcodeSBC(utmp8);
			cpu.c += 1;
			break;

		case 0xDF: // RST 18
			doOpcodeRST(0x18);
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
			cpu.c += 3;
			break;

		case 0xE6: // AND n
			utmp8 = readByte(cpu.r.pc);
			cpu.r.pc++;
			doOpcodeAND(utmp8);
			cpu.c += 1;
			break;

		case 0xE7: // RST 20
			doOpcodeRST(0x20);
			break;

		case 0xE8: // ADD SP, n
			tmp8 = (int8_t)readByte(cpu.r.pc);
			cpu.r.pc++;
			cpu.r.sp += tmp8;
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
			utmp8 = readByte(cpu.r.pc);
			cpu.r.pc++;
			doOpcodeXOR(utmp8);
			cpu.c += 1;
			break;

		case 0xEF: // RST 28
			doOpcodeRST(0x28);
			break;

		case 0xF0: // LDH A, (n)
			cpu.r.a = readByte(0xFF00 + readByte(cpu.r.pc));
			cpu.r.pc++;
			cpu.c += 3;
			break;

		case 0xF1: // POP AF
			cpu.r.f = readByte(cpu.r.sp);
			cpu.r.sp++;
			cpu.r.a = readByte(cpu.r.sp);
			cpu.r.sp++;
			cpu.c += 3;
			break;

		case 0xF2: // unimplemented TODO: actually LDH a, (C)?
			//writeByte(cpu.r.a, 0xFF00 + cpu.r.c);
			//cpu.c += 2;
			
			//readByte(0xFF00 + cpu.r.c
			
			doOpcodeUNIMP();
			break;

		case 0xF3: // DI
			cpu.ints = 0;
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
			cpu.c += 3;
			break;

		case 0xF6: // OR n
			utmp8 = readByte(cpu.r.pc);
			cpu.r.pc++;
			doOpcodeOR(utmp8);
			cpu.c += 1;
			break;

		case 0xF7: // RST 30
			doOpcodeRST(0x30);
			break;

		case 0xF8: // LDHL SP, d
			tmp16 = (int8_t)readByte(cpu.r.pc);
			cpu.r.pc++;
			tmp16 += cpu.r.sp;
			cpu.r.h = (tmp16 >> 8) & 0xFF;
			cpu.r.l = tmp16 & 0xFF;
			cpu.c += 3;
			break;

		case 0xF9: // LD SP, HL (onzeker over cycles)
			cpu.r.sp = (cpu.r.h << 8) + cpu.r.l;
			cpu.c += 1;
			break;

		case 0xFA: // LD A, (nn)
			cpu.r.a = readByte(readWord(cpu.r.pc));
			cpu.r.pc += 2;
			cpu.c += 4;
			break;

		case 0xFB: // EI
			cpu.ints = 1;
			cpu.c += 1;
			break;

		case 0xFC: // unimplemented
		case 0xFD: // unimplemented
			doOpcodeUNIMP();
			break;

		case 0xFE: // CP n
			utmp8 = readByte(cpu.r.pc);
			cpu.r.pc++;
			doOpcodeCP(utmp8);
			cpu.c += 1;
			break;

		case 0xFF: // RST 38
			doOpcodeRST(0x38);
			break;
			
		default:
			printf("There's a glitch in the matrix, this shouldn't happen.\n");
			return 0;
	}
	
	cpu.dc = cpu.c - cpu.dc; // delta cycles
	
	return 1;
}
