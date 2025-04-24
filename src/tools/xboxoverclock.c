/*
	xbox_oc
	----------------
	Copyright (C) 2022-2023 wutno (https://github.com/GXTX)
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "globals.h"

#define CPU_BASE_MULTIPLIER 5.5f
#define BASE_CLOCK_INT 16667
#define BASE_CLOCK_FLOAT 16.667f

// See nouveau
#define NV_PMC_ENABLE                0xFD000200
#define NV_PBUS_DEBUG_1              0xFD001084
#define NV_PRAMDAC_NVPLL_COEFF       0xFD680500
#define NV_PRAMDAC_PLL_SETUP_CONTROL 0xFD680510
#define NV_PRAMDAC_SEL_CLK           0xFD680524
#define NV_PRAMDAC_GENERAL_CONTROL   0xFD680600


ULONG original_fsb, wanted_fsb, hidden_fsb;
ULONG original_nvclk, wanted_nvclk;
int original_m, original_n, original_p, original_mp;
int wanted_m, wanted_n, wanted_p, wanted_mp;
ULONG pci_buff, cpu_coeff;

// https://github.com/WulfyStylez/XBOverclock
void calc_clock_params(int clk, int *n, int *m)
{
	int work1, work2, work4;

	work1 = clk / BASE_CLOCK_FLOAT;
	work2 = (clk * 2) / BASE_CLOCK_FLOAT;
	work4 = (clk * 4) / BASE_CLOCK_FLOAT;

	if (work2 * 2 != work4) {
		*n = work4;
		*m = 4;
	} else if (work1 * 2 != work2) {
		*n = work2;
		*m = 2;
	} else {
		*n = work1;
		*m = 1;
	}
}

ULONG get_GPU_frequency()
{
    ULONG nvclk_reg, current_nvclk;
    nvclk_reg = *((volatile ULONG *)NV_PRAMDAC_NVPLL_COEFF);

    current_nvclk = BASE_CLOCK_INT * ((nvclk_reg & 0xFF00) >> 8);
    current_nvclk /= 1 << ((nvclk_reg & 0x70000) >> 16);
    current_nvclk /= nvclk_reg & 0xFF;
    current_nvclk /= 1000;

    return current_nvclk;
}

void outputClocks()
{
	if (!wanted_mp)
		wanted_mp++;
	calc_clock_params(wanted_fsb, &wanted_n, &wanted_m);
	hidden_fsb = (BASE_CLOCK_FLOAT / wanted_m) * wanted_n;

	ULONG cpu_clk = (int)(wanted_fsb * CPU_BASE_MULTIPLIER);
	ULONG mem_clk = ((BASE_CLOCK_FLOAT / wanted_m) * (wanted_p * 2 * wanted_n)) / (2 * wanted_mp);

	cpu_coeff = (pci_buff & ~0x00FFFFFF) | (wanted_mp << 20) | (wanted_p << 16) | (wanted_n << 8) | wanted_m;

	debugPrint("\nFSB: %03luMHz, CPU: %03luMHz, RAM: %3luMHz\n", wanted_fsb, cpu_clk, mem_clk);
	debugPrint("NVCLK : %03luMHz\n", wanted_nvclk);
}

static inline __attribute__((always_inline)) void writeCPUClocks(ULONG coeff)
{
	KeEnterCriticalRegion();

    __asm
    {
		pushad
		pushfd

		cli
		sfence

		// We just want to give the CPU time to settle down, probably doesn't need to be this much
		mov eax, (__UINT32_MAX__ / 16)
		test eax, eax
LOOP:
		nop
		dec eax
		jnz LOOP

		mov dx, 0xCF8
		mov eax, 0x8000036C
		out dx, eax

		mov dx, 0xCFC
		mov eax, coeff
		out dx, eax

		mov eax, (__UINT32_MAX__ / 16)
		test eax, eax
LOOP2:
		nop
		dec eax
		jnz LOOP2

		sfence
		sti

		popfd
		popad
    }

	KeLeaveCriticalRegion();
}

void xboxoverclock(char *)
{
	// Read in the current FSB setting
	pci_buff = 0;
	HalReadWritePCISpace(0, 0x60, 0x6C, &pci_buff, sizeof(pci_buff), FALSE);

	original_m  = wanted_m  = pci_buff & 0xFF;
	original_n  = wanted_n  = (pci_buff >> 8) & 0xFF;
	original_p  = wanted_p  = (pci_buff >> 16) & 0xF;
	original_mp = wanted_mp = (pci_buff >> 20) & 0xF;

	original_fsb = (BASE_CLOCK_FLOAT / wanted_m) * wanted_n;
	wanted_fsb = original_fsb;

	// GPU
	original_nvclk = get_GPU_frequency();
	wanted_nvclk = original_nvclk;

	outputClocks();

	debugPrint("\nThis tool may cause irreparable harm to your Xbox.\n");
	debugPrint("This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	debugPrint("After applying, the machine should return to your dashboard, if it does not: the box has frozen and you should reboot and try again.\n");
	debugPrint("\n==============================\n");
	debugPrint("Use the left and right \"DPAD\" to change the FSB.\n");
	debugPrint("Use the up and down \"DPAD\" to change the NVCLK.\n");
	debugPrint("Use \"X\" and \"B\" to change the memory divider.\n");
	debugPrint("Press \"Start\" to apply (which will auto reboot).\n");
	debugPrint("Press \"Back\" to exit.");
	debugResetCursor();

	ULONGLONG counter = 0;

	while (1) {
		XVideoWaitForVBlank();
		debugResetCursor();
		debugPrint("%llu\n", counter++);
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_CONTROLLERBUTTONDOWN) {
				switch (event.cbutton.button) {
				case SDL_CONTROLLER_BUTTON_DPAD_LEFT: --wanted_fsb; break;
				case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: ++wanted_fsb; break;
				case SDL_CONTROLLER_BUTTON_DPAD_DOWN: --wanted_nvclk; break;
				case SDL_CONTROLLER_BUTTON_DPAD_UP: ++wanted_nvclk; break;
				case SDL_CONTROLLER_BUTTON_X: ++wanted_mp; break;
				case SDL_CONTROLLER_BUTTON_B: --wanted_mp; break;
				case SDL_CONTROLLER_BUTTON_START:
					SDL_Quit(); // have less stuff running

					int n, m;
					ULONG coeff;

					if (wanted_nvclk != original_nvclk) {
						calc_clock_params((++wanted_nvclk) * 2, &n, &m);
						debugClearScreen();
						debugPrint("Setting NVCLK to: %03dMHz\n", (BASE_CLOCK_INT * n / m) / 2 / 1000);

						coeff = (*((volatile ULONG *)NV_PRAMDAC_NVPLL_COEFF) & ~0x0000FFFF) | (n << 8) | m;

						// Stop the clocks...
						*((volatile ULONG *)NV_PMC_ENABLE) = 0;
						*((volatile ULONG *)NV_PBUS_DEBUG_1) |= 0xC000000;
						*((volatile ULONG *)NV_PRAMDAC_PLL_SETUP_CONTROL) = 0x0F;

						// Twiddle some registers to make +300MHz stable on all revs 
						*((volatile ULONG *)NV_PRAMDAC_SEL_CLK) |= 0x85;
						*((volatile ULONG *)NV_PRAMDAC_GENERAL_CONTROL) |= 0x20000000;

						*((volatile ULONG *)NV_PRAMDAC_NVPLL_COEFF) = coeff;
					}

					// If we're overclocking we need to make sure we write this first
					if (wanted_mp != original_mp && wanted_fsb > original_fsb) {
						debugClearScreen();
						debugPrint("Setting MemDiv to: %d\n", wanted_mp);

						// We don't use cpu_coeff as it might have FSB bits changed and we can't set both at the same time.
						coeff = (pci_buff & ~0x00F00000) | (wanted_mp << 20);
						writeCPUClocks(coeff);
					}

					if (wanted_fsb != original_fsb) {
						debugClearScreen();
						debugPrint("Setting FSB to: %03dMHz\n", (int)(BASE_CLOCK_FLOAT * wanted_n / wanted_m));

						writeCPUClocks(cpu_coeff);
					}

					HalReturnToFirmware(HalQuickRebootRoutine);
					break; // unreachable
				case SDL_CONTROLLER_BUTTON_BACK:
					goto the_end;
					break; // unreachable
				default:
					break;
				}
				
				debugResetCursor();
				outputClocks();
			}
		}
	}

the_end:
	//SDL_Quit();
	return;
}
