// KERNAL Emulator
// Copyright (c) 2009-2021 Michael Steil, James Abbatiello et al.
// All rights reserved. License: 2-clause BSD
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>  // getcwd, chdir
#include <windows.h> // GetLocalTime, SetLocalTime
#include <conio.h>   // _kbhit, _getch
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#include "fake6502.h"
#include "glue.h"
#include "dispatch.h"
#include "screen.h"

machine_t machine;
uint16_t c64_has_external_rom;
extern uint8_t REU[16*1024*1024];

static uint16_t
parse_num(char *s)
{
	int base = 10;
	if (s[0] == '$') {
		s++;
		base = 16;
	} else if (s[0] == '0' && s[1] == 'x') {
		s += 2;
		base = 16;
	}
	return strtoul(s, NULL, base);
}

int
main(int argc, char **argv)
{
    FILE *reufile = fopen("reufile.linux", "rb");
    assert(reufile != NULL);
    int readreu = fread(REU, 1, 16777216, reufile);
    printf("READ REU: %d\n",  readreu);
    assert(readreu == 16777216);
    fclose(reufile);

	if (argc <= 1) {
		printf("Usage: %s <filenames> [<arguments>]\n", argv[0]);
		exit(1);
	}

	bool has_start_address = false;
	uint16_t start_address;
	bool has_start_address_indirect = false;
	uint16_t start_address_indirect;
	bool has_machine;
	bool charset_text = false;
	uint8_t columns = 0;

	c64_has_external_rom = false;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "-start")) {
				if (i == argc - 1) {
					printf("%s: -start requires argument!\n", argv[0]);
					exit(1);
				}
				start_address = parse_num(argv[i + 1 ]);
				has_start_address = true;
			} else if (!strcmp(argv[i], "-startind")) {
				if (i == argc - 1) {
					printf("%s: -startind requires argument!\n", argv[0]);
					exit(1);
				}
				start_address_indirect = parse_num(argv[i + 1 ]);
				has_start_address_indirect = true;
			} else if (!strcmp(argv[i], "-machine")) {
				if (i == argc - 1) {
					printf("%s: -machine requires argument!\n", argv[0]);
					exit(1);
				}
				if (!strcmp(argv[i + 1], "pet")) {
					machine = MACHINE_PET;
				} else if (!strcmp(argv[i + 1], "pet4")) {
					machine = MACHINE_PET4;
				} else if (!strcmp(argv[i + 1], "vic20")) {
					machine = MACHINE_VIC20;
				} else if (!strcmp(argv[i + 1], "c64")) {
					machine = MACHINE_C64;
				} else if (!strcmp(argv[i + 1], "ted")) {
					machine = MACHINE_TED;
				} else if (!strcmp(argv[i + 1], "c128")) {
					machine = MACHINE_C128;
				} else if (!strcmp(argv[i + 1], "c65")) {
					machine = MACHINE_C65;
				} else {
					printf("%s: Valid values for \"-machine\" are pet, pet4, vic20, c64, ted, c128, c65!\n", argv[0]);
					exit(1);
				}
				has_machine = true;
			} else if (!strcmp(argv[i], "-text")) {
				charset_text = true;
			} else if (!strcmp(argv[i], "-graphics")) {
				charset_text = false;
			} else if (!strcmp(argv[i], "-columns")) {
				if (i == argc - 1) {
					printf("%s: -columns requires argument!\n", argv[0]);
					exit(1);
				}
				columns = parse_num(argv[i + 1 ]);
			}
			i++;
		} else {
			FILE *binary = fopen(argv[i], "rb");
			if (!binary) {
				printf("Error opening: %s\n", argv[i]);
				exit(1);
			}
			uint8_t lo = fgetc(binary);
			uint8_t hi = fgetc(binary);
			uint16_t load_address = lo | hi << 8;
			size_t nread=fread(&RAM[load_address], 65536 - load_address, 1, binary);
			(void)nread;
			fclose(binary);
			if (load_address == 0x8000) {
				c64_has_external_rom = true;
			}
			bool has_basic_start = false;
			switch (load_address) {
				case 0x401:  // PET
					if (!has_machine) {
						machine = MACHINE_PET4;
					}
					has_basic_start = true;
					break;
				case 0x801:  // C64
					if (!has_machine) {
						machine = MACHINE_C64;
					}
					has_basic_start = true;
					break;
				case 0x1001: // TED
					if (!has_machine) {
						machine = MACHINE_TED;
					}
					has_basic_start = true;
					break;
				case 0x1C01: // C128
					if (!has_machine) {
						machine = MACHINE_C128;
					}
					has_basic_start = true;
					break;
			}
			if (!has_start_address) {
				if (has_basic_start && RAM[load_address + 4] == 0x9e /* SYS */) {
					char *sys_arg = (char *)&RAM[load_address + 5];
					if (*sys_arg == '(') {
						sys_arg++;
					}
					start_address = parse_num(sys_arg);
					has_start_address = true;
				} else {
					start_address = load_address;
					has_start_address = true;
				}
			}
		}
	}

	if (!has_machine) {
		machine = MACHINE_C64;
	}

	reset6502();
	sp = 0xff;

	if (has_start_address_indirect) {
		pc = RAM[start_address_indirect] | (RAM[start_address_indirect + 1] << 8);
	} else if (has_start_address) {
		pc = start_address;
	} else {
		printf("%s: You need to specify at least one binary file!\n", argv[0]);
		exit(1);
	}

	kernal_init();
	screen_init(columns, charset_text);

//	RAM[0xFFFC] = 0xD1;
//	RAM[0xFFFD] = 0xFC;
//	RAM[0xFFFE] = 0x1B;
//	RAM[0xFFFF] = 0xE6;

	for (;;) {
//		printf("pc = %04x; %02x %02x %02x, sp=%02x [%02x, %02x, %02x, %02x, %02x]\n", pc, RAM[pc], RAM[pc+1], RAM[pc+2], sp, RAM[0x100 + sp + 1], RAM[0x100 + sp + 2], RAM[0x100 + sp + 3], RAM[0x100 + sp + 4], RAM[0x100 + sp + 5]);
		while (!RAM[pc]) {
			bool success = kernal_dispatch(machine);
			if (!success) {
				printf("\nunknown PC=$%04X S=$%02X (caller: $%04X)\n", pc, sp, (RAM[0x100 + sp + 1] | (RAM[0x100 + sp + 2] << 8)) + 1);
				exit(1);
			}
		}
		step6502();
	}

	return 0;
}
