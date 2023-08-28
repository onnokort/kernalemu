// KERNAL Emulator
// Copyright (c) 2009-2021 Michael Steil
// All rights reserved. License: 2-clause BSD

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include "glue.h"
#include "memory.h"

uint16_t ram_bot, ram_top;

//
// interface for fake6502
//

uint8_t RAM[65536];
uint8_t REU[16*1024*1024];

const uint16_t REU_CMD_ADDR = 0xdf01;


uint8_t read6502(uint16_t address)
{
    return RAM[address];
}

void reu() {
    // likely buggy, very minimal REU emulation
    uint8_t cmd = RAM[REU_CMD_ADDR];
    uint16_t c64addr = RAM[0xdf02] + 256*RAM[0xdf03];
    uint32_t reu_addr = RAM[0xdf04] + 256*RAM[0xdf05] + 65536*RAM[0xdf06];
    uint16_t xfer_len = RAM[0xdf07] + 256*RAM[0xdf08];

    if (c64addr + xfer_len > 65536) return;
    if (reu_addr + xfer_len > 16777216) return;

    if (cmd == 0b10010001) { // read
        memcpy(RAM + c64addr, REU + reu_addr, xfer_len);
    }
    else if (cmd == 0b10010000) { // write
        memcpy(REU + reu_addr, RAM + c64addr, xfer_len);
    }
}

void write6502(uint16_t address, uint8_t value)
{
    RAM[address] = value;
    if (address == REU_CMD_ADDR) {
        reu();
    }
}

// RAMTAS - Perform RAM test
void
RAMTAS()
{
	switch (machine) {
		case MACHINE_PET:
		case MACHINE_PET4:
			// impossible
			break;
		case MACHINE_VIC20:
			ram_bot = 0x1200; // unexpanded: 0x1000
			ram_top = 0x8000; // unexpanded: 0x1e00
			break;
		case MACHINE_C64:
			ram_bot = 0x0800;
			ram_top = c64_has_external_rom ? 0x8000 : 0xa000;
			break;
		case MACHINE_TED:
			ram_bot = 0x1000;
			ram_top = 0xfd00;
			break;
		case MACHINE_C128:
			ram_bot = 0x1c00;
			ram_top = 0xff00;
			break;
		case MACHINE_C65:
			ram_bot = 0x2000;
			ram_top = 0xff00;
			break;
	}

	// clear zero page
	memset(RAM, 0, 0x100);
	// clear 0x200-0x3ff
	memset(&RAM[0x0200], 0, 0x200);
}

// MEMTOP - Read/set the top of memory
void
MEMTOP()
{
	if (status & 1) {
		x = ram_top & 0xFF;
		y = ram_top >> 8;
	} else {
		ram_top = x | (y << 8);
	}
}

// MEMBOT - Read/set the bottom of memory
void
MEMBOT()
{
	if (status & 1) {
		x = ram_bot & 0xFF;
		y = ram_bot >> 8;
	} else {
		ram_bot = x | (y << 8);
	}
}
