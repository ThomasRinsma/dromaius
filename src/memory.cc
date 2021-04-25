#include <fstream>
#include <iterator>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <map>
#include "dromaius.h"

constexpr uint8_t Memory::bios[256];

Memory::~Memory()
{
	if (initialized) {
		freeBuffers();
	}
}

void Memory::initialize()
{
	// Initialize state
	biosLoaded = true;
	ramEnabled = true;
	bankMode = 0;
	ramBank = 0;
	romBank = 1;

	// Clear RAM buffers
	memset(workram, 0x00, sizeof(workram));
	memset(extram, 0x00, sizeof(extram));
	memset(zeropageram, 0x00, sizeof(zeropageram));

	initialized = true;
}

void Memory::freeBuffers()
{
	if (romLoaded) {
		delete[] rom;
	}
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
	auto romheader = (Memory::romheader_t *)(&rom[HEADER_START]);
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

	// For debugging, also try to load a similarly named symbols list file
	std::filesystem::path symfile = filename;
	symfile.replace_extension(".sym");
	tryParseSymbolsFile(symfile);
	return true;
}

void Memory::unloadRom() {
	if (romLoaded) {
		delete[] rom;
		romLoaded = false;
	}
}

void Memory::tryParseSymbolsFile(std::string filename)
{
	printf("Trying to load symbols file '%s'...\n", filename.c_str());

	// Read sym file
	try {
		std::ifstream symFile(filename, std::ios::in);

		// Line elements
		std::string line, symbol, bankAndAddr, hexBank, hexAddr;
		uint8_t bank;
		uint16_t addr;

		size_t symCnt = 0, lineCnt = 0;
		while (std::getline(symFile, line))
		{
			std::istringstream iss(line);
			try {
				// Parse bank nr
				iss >> bankAndAddr;

				hexBank = bankAndAddr.substr(0, bankAndAddr.find(":"));
				hexAddr = bankAndAddr.substr(bankAndAddr.find(":")+1);

				bank = std::stoi(hexBank, 0, 16);
				addr = std::stoi(hexAddr, 0, 16);

				// Parse symbol name
				iss >> symbol;

				// Two-way lookup
				addrToSymbol[{bank, addr}] = symbol;
				symbolToAddr[symbol] = {bank, addr};

				symCnt++;
			} catch (std::exception &e) {
				// NOP
			}
			lineCnt++;
		}
		printf("  parsed %d symbols from %d lines \n", symCnt, lineCnt);


	} catch (std::exception &e) {
		printf("  no symbol file or error while parsing symbol file\n");
	}
	
}

std::string Memory::getSymbolFromAddress(uint8_t bank, uint16_t addr)
{
	try {
		return addrToSymbol.at({bank, addr});
	} catch (std::exception &e) {
		return "";
	}

}

// Returns pair of <bank, addr>
std::pair<uint8_t, uint16_t> Memory::getAddressFromSymbol(uint8_t bank, std::string &symbol)
{
	try {
		return symbolToAddr.at(symbol);
	} catch (std::exception &e) {
		return {0, 0};
	}

}

std::string Memory::getCartridgeTypeString(uint8_t type) {

	switch (type) {
		case 0x00: return "ROM ONLY";
		case 0x01: return "MBC1";
		case 0x02: return "MBC1+RAM";
		case 0x03: return "MBC1+RAM+BATTERY";
		case 0x05: return "MBC2";
		case 0x06: return "MBC2+BATTERY";
		case 0x08: return "ROM+RAM *";
		case 0x09: return "ROM+RAM+BATTERY *";
		case 0x0B: return "MMM01";
		case 0x0C: return "MMM01+RAM";
		case 0x0D: return "MMM01+RAM+BATTERY";
		case 0x0F: return "MBC3+TIMER+BATTERY";
		case 0x10: return "MBC3+TIMER+RAM+BATTERY **";
		case 0x11: return "MBC3";
		case 0x12: return "MBC3+RAM **";
		case 0x13: return "MBC3+RAM+BATTERY **";
		case 0x19: return "MBC5";
		case 0x1A: return "MBC5+RAM";
		case 0x1B: return "MBC5+RAM+BATTERY";
		case 0x1C: return "MBC5+RUMBLE";
		case 0x1D: return "MBC5+RUMBLE+RAM";
		case 0x1E: return "MBC5+RUMBLE+RAM+BATTERY";
		case 0x20: return "MBC6";
		case 0x22: return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
		case 0xFC: return "POCKET CAMERA";
		case 0xFD: return "BANDAI TAMA5";
		case 0xFE: return "HuC3";
		case 0xFF: return "HuC1+RAM+BATTERY";
		default: return "(unknown)";
	}
}

std::string Memory::getCartridgeRomSizeString(uint8_t size) {
	switch (size) {
		case 0x00: return "32 KB";
		case 0x01: return "64 KB";
		case 0x02: return "128 KB";
		case 0x03: return "256 KB";
		case 0x04: return "512 KB";
		case 0x05: return "1 MB";
		case 0x06: return "2 MB";
		case 0x07: return "4 MB";
		case 0x08: return "8 MB";
		case 0x52: return "1.1 MB";
		case 0x53: return "1.2 MB";
		case 0x54: return "1.5 MB";
		default: return "(unknown)";
	}
}

std::string Memory::getCartridgeRamSizeString(uint8_t size) {
	switch (size) {
		case 0x00: return "No RAM";
		case 0x02: return "8 KB";
		case 0x03: return "32 KB";
		case 0x04: return "128 KB";
		case 0x05: return "64 KB";
		default: return "(unknown)";
	}
}


std::string Memory::getRegionName(uint16_t addr)
{
	switch (addr & 0xF000) {
		case 0x0000:
		case 0x1000:
		case 0x2000:
		case 0x3000:
			if (biosLoaded) {
				if (addr < 0x0100) {
					return "BIOS";
				}
			}
			return "ROM0";

		case 0x4000:
		case 0x5000:
		case 0x6000:
		case 0x7000:
			return "ROM1"; // TODO bank

		case 0x8000:
		case 0x9000:
			return "VRAM";

		case 0xA000:
		case 0xB000:
			return "EXTRAM";

		case 0xC000:
		case 0xD000:
		case 0xE000:
			return "WRAM";

		case 0xF000:
			if (addr < 0xFE00) { // Working RAM shadow
				return "WRAM(S)";
			}
			
			if ((addr & 0x0F00) == 0x0E00) {
				if (addr < 0xFEA0) {
					return "OAM";
				}
				return "UNK?";
			}
			else {
				switch (addr & 0xFFF0) {
					case 0xFF00:
						return "REGS*";

					case 0xFF40:
					case 0xFF50:
					case 0xFF60:
					case 0xFF70:
						return "REGS(G)";

					case 0xFF80:
					case 0xFF90:
					case 0xFFA0:
					case 0xFFB0:
					case 0xFFC0:
					case 0xFFD0:
					case 0xFFE0:
						return "ZERO";

					case 0xFFF0:
						return "REGS(I)";

					default:
						return "UNK?";
				}
			}
			return "UNK?";
	}
	return "UNK?";
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
			return emu->graphics.vram[addr & 0x1FFF];
			
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
					return emu->graphics.oam[addr & 0xFF];
				}
				else {
					return 0;
				}
			}
			else {
				if (addr == 0xFFFF) { // interrupt enable
					return emu->cpu.ints;
				}
				else if (addr >= 0xFF80) { // Zero page
					return zeropageram[addr & 0x7F];
				}
				else if (addr >= 0xFF40) { // I/O
					return emu->graphics.readByte(addr - 0xFF40);
				}
				else if (addr == 0xFF0F) { // interrupt flags
					return emu->cpu.intFlags;
				}
				else if (addr == 0xFF04) {
					return emu->cpu.timer.div;
				}
				else if (addr == 0xFF05) {
					return emu->cpu.timer.tima;
				}
				else if (addr == 0xFF06) {
					return emu->cpu.timer.tma;
				}
				else if (addr == 0xFF07) {
					return emu->cpu.timer.tac;
				}
				else if (addr == 0xFF00) { 
					if (emu->input.wire == 0x10) {
						return emu->input.row[0];
					}
					if (emu->input.wire == 0x20) {
						return emu->input.row[1];
					}
					return 0;
				}

				
				return 0;
			}
		
	}
	printf("Memory error! 0x%02X\n", addr);
	return 0;
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
			emu->graphics.vram[addr & 0x1FFF] = b;
			//if (b == 0 && addr >= 0x9800 && addr < 0x9C00)
			//{
			//	printf("wrote b=%d to vram @ 0x%04X.\n", b, addr);

			//	printRegisters();
			//}
			emu->graphics.updateTile(b, addr & 0x1FFE);
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
					emu->graphics.oam[addr & 0xFF] = b;
					emu->graphics.buildSpriteData(b, addr - 0xFE00);
					return;
				}
				else {
					// writes here do nothing
					return;
				}
			}
			else {
				if (addr == 0xFFFF) { // interrupt enable
					//printf("emu->cpu.ints = %d\n", b);
					emu->cpu.ints = b;
					return;
				}
				else if (addr >= 0xFF80) { // Zero page
					zeropageram[addr & 0x7F] = b;
					return;
				}
				else if (addr >= 0xFF40) { // I/O
					emu->graphics.writeByte(b, addr - 0xFF40);
					return;
				}
				else if (addr == 0xFF0F) {
					//printf("emu->cpu.intFlags = %d\n", b);
					emu->cpu.intFlags = b; // TODO: Can we allow program to set these?
					return;
				}
				else if (addr == 0xFF04) {
					emu->cpu.timer.div = 0x00; // writing resets the timer
					return;
				}
				else if (addr == 0xFF05) {
					emu->cpu.timer.tima = b;
					return;
				}
				else if (addr == 0xFF06) {
					emu->cpu.timer.tma = b;
					return;
				}
				else if (addr == 0xFF07) {
					emu->cpu.timer.tac = b;
					return;
				}
				else if (addr == 0xFF00) {
					emu->input.wire = b & 0x30;
					return;
				}
				else if (addr >= 0xFF10 && addr <= 0xFF26) {
					emu->audio.writeByte(b, addr);
				}
				else if (addr >= 0xFF30 && addr <= 0xFF3F) {
					emu->audio.waveRam[addr - 0xFF30] = b;
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
