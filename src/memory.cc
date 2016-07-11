#include <fstream>
#include <iostream>
#include "gbemu.h"

#include "audio.h"
#include "cpu.h"
#include "graphics.h"
#include "input.h"
#include "memory.h"

extern Graphics graphics;
extern Audio audio;
extern Input input;
extern CPU cpu;

constexpr uint8_t Memory::bios[256];

Memory::~Memory()
{
	if (initialized) {
		freeBuffers();
	}
}

void Memory::initialize()
{
	// Set rom buffer to null
	rom = nullptr;

	// Allocate 
	workram = new uint8_t[0x2000];  // 8 kb
	extram = new uint8_t[0x2000];   // 8 kb
	zeropageram = new uint8_t[128]; // 128 bytes

	// MBC-related
	ramEnabled = true;
	bankMode = 0;
	ramBank = 0;
	romBank = 1;

	biosLoaded = true;

	initialized = true;
}

void Memory::freeBuffers()
{
	if (romLoaded) {
		delete[] rom;
	}
	
	delete[] workram;
	delete[] extram;
	delete[] zeropageram;
}

bool Memory::loadRom(std::string const &filename)
{
	if (romLoaded) {
		delete[] rom;
	}

	// Read rom file
	std::ifstream romFile(filename, std::ios::in | std::ios::binary | std::ios::ate);

	if (not romFile)
	{
		// TODO: exceptions
		std::cerr << "Failed to open rom \"" << filename << "\"." << std::endl;
		return false;
	}

	// Rom length in bytes
	romLen = romFile.tellg();

	// Allocate buffer and copy rom into buffer
	romFile.seekg(0, std::ios::beg);
	rom = new uint8_t[romLen];
	romFile.read((char *)rom, romLen);

	// Close file
	romFile.close();

	romLoaded = true;

	// Read the ROM header
	romheader_s *romheader = (romheader_s *)(&rom[HEADER_START]);
	//printf("CGB: 0x%02X, SGB: 0x%02X, OLIC: 0x%02X, NLIC: 0x%04X, country: 0x%02X\n",
	//	romheader->colorbyte, romheader->sgbfeatures, romheader->oldlicensee,
	//	romheader->newlicensee, romheader->country);
	//printf("type: 0x%02X, romsize: 0x%02X, ramsize: 0x%02X\n\n",
	//	romheader->type, romheader->romsize, romheader->ramsize);

	// Set ram size (2/8/32 KByte)
	ramSize = romheader->ramsize;

	// Set MBC type
	if (romheader->type == 0x00) {
		mbc = MBC::NONE;
	} else if (romheader->type < 0x05) {
		mbc = MBC::MBC1;
	} else if (romheader->type == 0x05 || romheader->type == 0x06) {
		mbc = MBC::MBC2;
	} else if (romheader->type >= 0x0F && romheader->type < 0x15) {
		mbc = MBC::MBC3;
	} else if (romheader->type >= 0x15 && romheader->type < 0x19) {
		mbc = MBC::MBC4;
	} else if (romheader->type >= 0x19 && romheader->type < 0x1E) {
		mbc = MBC::MBC5;
	} else {
		mbc = MBC::OTHER;
	}

	return true;
}

uint8_t Memory::readByte(uint16_t addr)
{
	switch (addr & 0xF000) {
		// BIOS / ROM0
		case 0x0000:
			if (biosLoaded) {
				if (addr < 0x0100) {
					return bios[addr];
				}
				else if (addr == 0x0100) {
					biosLoaded = false;
				}
			}
			return rom[addr];
		
		// ROM0
		case 0x1000:
		case 0x2000:
		case 0x3000:
			return rom[addr];
			
		// ROM1, bank
		case 0x4000:
		case 0x5000:
		case 0x6000:
		case 0x7000:
			if (mbc == MBC::NONE) {
				return rom[addr];
			}
			else {
				// TODO: is this the same for all MBCs?
				// At least MBC1, 2 (only 16 banks) and 3
				return rom[((romBank - 1) * 0x4000) + addr];
			}
			
		// VRAM
		case 0x8000:
		case 0x9000:
			return graphics.vram[addr & 0x1FFF];
			
		// External RAM
		case 0xA000:
		case 0xB000:
			if (!ramEnabled) {
				printf("Read from disabled external RAM.\n");
			}

			if (mbc == MBC::NONE) {
				return extram[addr & 0x1FFF];
			}
			else if (mbc == MBC::MBC1) {
				if (ramSize == 0x03) { // 4 banks of 8kb
					return extram[(ramBank * 0x2000) + (addr & 0x1FFF)];
				}
				else { // either 2KB or 8KB total, no banks
					return extram[addr & 0x1FFF];
				}
			}
			else if (mbc == MBC::MBC2) {
				if (addr <= 0xA1FF) {
					// TODO: not sure how to handle only using the lower nibble
					return extram[addr & 0x1FFF] & 0x0F;
				}
				else {
					printf("Read from MBC2 RAM outside limit.\n");
				}
			}
			else if (mbc == MBC::MBC3) {
				// Ram mode
				if (rtcReg == 0) {
					if (ramSize == 0x03) { // 4 banks of 8kb
						return extram[(ramBank * 0x2000) + (addr & 0x1FFF)];
					}
					// TODO: not sure if this else is needed here for MBC3
					else { // either 2KB or 8KB total, no banks
						return extram[addr & 0x1FFF];
					}
				} 
				// RTC reg mode
				else {
					return rtc[rtcReg - 0x08];
				}
			}
			
			
		// Working RAM
		case 0xC000:
		case 0xD000:
		case 0xE000:
			return workram[addr & 0x1FFF];
			
		case 0xF000:
			if (addr < 0xFE00) { // Working RAM shadow
				return workram[addr & 0x1FFF];
			}
			
			if ((addr & 0x0F00) == 0x0E00) {
				if (addr < 0xFEA0) { // Object Attribute Memory
			    	return graphics.oam[addr & 0xFF];
			    }
			    else {
			    	return 0;
			    }
			}
			else {
				if (addr == 0xFFFF) { // interrupt enable
					return cpu.ints;
				}
				else if (addr >= 0xFF80) { // Zero page
					return zeropageram[addr & 0x7F];
				}
				else if (addr >= 0xFF40) { // I/O
					return graphics.readByte(addr - 0xFF40);
				}
				else if (addr == 0xFF0F) { // interrupt flags
					return cpu.intFlags;
				}
				else if (addr == 0xFF04) {
					return cpu.timer.div;
				}
				else if (addr == 0xFF05) {
					return cpu.timer.tima;
				}
				else if (addr == 0xFF06) {
					return cpu.timer.tma;
				}
				else if (addr == 0xFF07) {
					return cpu.timer.tac;
				}
				else if (addr == 0xFF00) { 
					if (input.wire == 0x10) {
						return input.row[0];
					}
					if (input.wire == 0x20) {
						return input.row[1];
					}
					return 0;
				}

				
				return 0;
			}
		
	}
	printf("Memory error! 0x%02X\n", addr);
}

uint16_t Memory::readWord(uint16_t addr)
{
	return readByte(addr) + (readByte(addr + 1) << 8);
}

void Memory::writeByte(uint8_t b, uint16_t addr)
{
	switch (addr & 0xF000) {
		// Enable or disable external RAM
		case 0x0000:
		case 0x1000:
			if (mbc == MBC::NONE) {
				if (biosLoaded) {
					if (addr < 0x0100) {
						printf("Writing to BIOS!\n");
						//bios[addr] = b;
						return;
					}
					else if (addr == 0x0100) {
						biosLoaded = 0;
					}
				}
				
				rom[addr] = b;
				return;
			}
			else if (mbc == MBC::MBC1 || mbc == MBC::MBC3) {
				ramEnabled = ((b & 0xF) == 0xA);
			}
			else if (mbc == MBC::MBC2) {
				// Least sign. bit of upper addr. byte should be 0
				if (!(addr & 0x0100)) {
					ramEnabled = ((b & 0xF) == 0xA);
				}
				else {
					printf("MBC2 RAM enable with wrong bit.\n");
				}
			}
			break;

		// Set the lower 5 bits of ROM bank nr
		case 0x2000:
		case 0x3000: 
			if (mbc == MBC::NONE) {
				//printf("write to 0x2000-0x3FFF without MBC\n");
				//rom[addr] = b;
			}
			else if (mbc == MBC::MBC1) {
				// Set lower 5 bits
				romBank = (b & 0x1F) | (romBank & 0xE0);

				// Apply rom bank glitch
				if (romBank == 0x00 || romBank == 0x20 ||
					romBank == 0x40 || romBank == 0x60) {
					romBank++;
				}
			}
			else if (mbc == MBC::MBC2) {
				// Least sign. bit of upper addr. byte should be 1
				if (addr & 0x0100) {
					// Only 16 banks for MBC2
					romBank = (b & 0x0F);
				}
				else {
					printf("MBC2 ROM bank select with wrong bit.\n");
				}
			}
			else if (mbc == MBC::MBC3) {
				// lower 7 bits select rom bank
				romBank = (b & 0x7F);

				// Fix rom bank glitch (bank 0 --> bank 1)
				if (romBank == 0x00) {
					romBank++;
				}
			}
			return;
			
		// Set RAM bank nr or upper 2 bits of ROM bank nr
		case 0x4000:
		case 0x5000:
			if (mbc == MBC::NONE) {
				//printf("write to 0x4000-0x5FFF without MBC\n");
				//rom[addr] = b;
			}
			else if (mbc == MBC::MBC1) {
				if (bankMode) { // RAM
					ramBank = (b & 0x3);
				}
				else { // ROM, upper 2 bits
					romBank = (romBank & 0x1F) | ((b & 0x3) << 5);

					// Apply rom bank glitch
					// TODO: only for MBC1 or also for 2?
					if (romBank == 0x00 || romBank == 0x20 ||
						romBank == 0x40 || romBank == 0x60) {
						romBank++;
					}
				}
				
			}
			else if (mbc == MBC::MBC3) {
				// Either select ram bank or select RTC register
				if (b <= 0x03) {
					ramBank = b;
					rtcReg = 0;
				}
				else if (b >= 0x08 && b <= 0x0C) {
					rtcReg = b;
				}
				else {
					printf("invalid selector written (MBC3) (%x)\n", b);
				}
			}
			return;

		// ROM or RAM banking mode select
		case 0x6000:
		case 0x7000:
			if (mbc == MBC::NONE) {
				//printf("write to 0x6000-0x7FFF without MBC\n");
				//rom[addr] = b;
			}
			else if (mbc == MBC::MBC1) {
				bankMode = (b & 0x1);
			}
			else if (mbc == MBC::MBC3) {
				printf("TODO: latch RTC time");
			}
			return;
			
		// VRAM
		case 0x8000:
		case 0x9000:
			graphics.vram[addr & 0x1FFF] = b;
			//if (b == 0 && addr >= 0x9800 && addr < 0x9C00)
			//{
			//	printf("wrote b=%d to vram @ 0x%04X.\n", b, addr);

			//	printRegisters();
			//}
			graphics.updateTile(b, addr & 0x1FFE);
			return;
			
		// External RAM
		case 0xA000:
		case 0xB000:
			if (!ramEnabled)
				printf("Write to disabled external RAM.\n");

			if (mbc == MBC::NONE) {
				extram[addr & 0x1FFF] = b;
			}
			else if (mbc == MBC::MBC1) {
				if (ramSize == 0x03) { // 4 banks of 8kb
					extram[(ramBank * 0x2000) + (addr & 0x1FFF)] = b;
				}
				else { // either 2KB or 8KB total, no banks
					extram[addr & 0x1FFF] = b;
				}
			}
			else if (mbc == MBC::MBC2) {
				if (addr <= 0xA1FF) {
					// TODO: not sure how to handle only using the lower nibble
					extram[addr & 0x1FFF] = (b & 0x0F);
				}
				else {
					printf("Write to MBC2 RAM outside limit.\n");
				}
			}
			else if (mbc == MBC::MBC3) {
				// Ram mode
				if (rtcReg == 0) {
					if (ramSize == 0x03) { // 4 banks of 8kb
						extram[(ramBank * 0x2000) + (addr & 0x1FFF)] = b;
					}
					// TODO: not sure if this else is needed here for MBC3
					else { // either 2KB or 8KB total, no banks
						extram[addr & 0x1FFF] = b;
					}
				}
				// RTC reg mode
				else {
					rtc[rtcReg - 0x08] = b;
				}
			}
			else {
				printf("0xA000-0xBFFF unimplemented else, TODO\n");
			}

			return;
			
		// Working RAM
		case 0xC000:
		case 0xD000:
		case 0xE000:
			workram[addr & 0x1FFF] = b;
			return;
			
		case 0xF000:
			if (addr < 0xFE00) { // Working RAM shadow
				workram[addr & 0x1FFF] = b;
				return;
			}
			if ((addr & 0x0F00) == 0x0E00) {
				if (addr < 0xFEA0) { // Object Attribute Memory
			    	graphics.oam[addr & 0xFF] = b;
			    	graphics.buildSpriteData(b, addr - 0xFE00);
			    	return;
			    }
			    else {
			    	// writes here do nothing
			    	return;
			    }
			}
			else {
				if (addr == 0xFFFF) { // interrupt enable
					//printf("cpu.ints = %d\n", b);
					cpu.ints = b;
					return;
				}
				else if (addr >= 0xFF80) { // Zero page
					zeropageram[addr & 0x7F] = b;
					return;
				}
				else if (addr >= 0xFF40) { // I/O
					graphics.writeByte(b, addr - 0xFF40);
					return;
				}
				else if (addr == 0xFF0F) {
					//printf("cpu.intFlags = %d\n", b);
					cpu.intFlags = b; // TODO: Can we allow program to set these?
					return;
				}
				else if (addr == 0xFF04) {
					cpu.timer.div = 0x00; // writing resets the timer
					return;
				}
				else if (addr == 0xFF05) {
					cpu.timer.tima = b;
					return;
				}
				else if (addr == 0xFF06) {
					cpu.timer.tma = b;
					return;
				}
				else if (addr == 0xFF07) {
					cpu.timer.tac = b;
					return;
				}
				else if (addr == 0xFF00) {
					input.wire = b & 0x30;
					return;
				}
				else if (addr >= 0xFF10 && addr <= 0xFF26) {
					audio.writeByte(b, addr);
				}
				else if (addr >= 0xFF30 && addr <= 0xFF3F) {
					audio.waveRam[addr - 0xFF30] = b;
				}			
			}
	}
}

void Memory::writeWord(uint16_t w, uint16_t addr)
{
	writeByte(w & 0xFF, addr);
	writeByte(w >> 8, addr + 1);
}

void Memory::dumpToFile(std::string const &filename) {
	std::ofstream outFile(filename, std::ofstream::binary);
	
	for (int i = 0; i < 0xFFFF; ++i) {
		outFile << readByte(i);
	}
}
