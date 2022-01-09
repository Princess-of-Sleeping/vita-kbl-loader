
#include <stdint.h>
#include <stddef.h>
#include "../payload_boot/arm_opcode.h"

void wait_time(int usec){

	uint64_t end = *(uint64_t *)(0xE20B6000) + usec;

	while(*(volatile uint64_t *)(0xE20B6000) < end);
}

void set_gpo(int bit){
	*(uint32_t *)(0xE20A0000 + 0xC) = 0xFF0000;
	*(uint32_t *)(0xE20A0000 + 0x8) = 0xFF0000 & (bit << 16);
}

__attribute__((__noreturn__))
void scratchpad_undef_handler(const void *lr){

	set_gpo(0xAA);

	while(1);
}

__attribute__((__noreturn__))
void scratchpad_svc_handler(const void *lr){

	set_gpo(0xAB);

	while(1);
}

__attribute__((__noreturn__))
void scratchpad_prefetch_handler(const void *lr){

	set_gpo(0xAC);

	while(1);
}

__attribute__((__noreturn__))
void scratchpad_abort_handler(const void *lr){

	uint32_t DFAR = 0;

	asm volatile (
		"mrc p15, #0, %0, c6, c0, #0\n\t"
		:"=r"(DFAR)
	);

	set_gpo(0xAD);

	wait_time(1 * 1000 * 1000);

	set_gpo(0xFF);

	while(1){

		wait_time(4 * 1000 * 1000);
		set_gpo(DFAR >> 24);

		wait_time(4 * 1000 * 1000);
		set_gpo(DFAR >> 16);

		wait_time(4 * 1000 * 1000);
		set_gpo(DFAR >> 8);

		wait_time(4 * 1000 * 1000);
		set_gpo(DFAR);

		wait_time(4 * 1000 * 1000);
		set_gpo(0xFF);
	}

	while(1);
}

__attribute__((__noreturn__))
void scratchpad_irq_handler(const void *lr){

	set_gpo(0xAE);

	while(1);
}

__attribute__((__noreturn__))
void scratchpad_fiq_handler(const void *lr){

	set_gpo(0xAF);

	while(1);
}

void *memcpy2(void *dst, const void *src, int len){

	if((((uintptr_t)dst | (uintptr_t)src) & 3) == 0){
		// return memcpy(dst, src, len);
	}

	// maybe cannot unaligned access on MMU non mapped
	void *end = dst + len;

	while(dst != end){
		*(char *)(dst) = *(char *)(src);
		dst += 1; src += 1;
	}

	return end - len;
}

int log_hook_tmp(void);

int payload_bootstrap_main(void){

	uintptr_t patch_func;
	int opcode[3];

	patch_func = (uintptr_t)log_hook_tmp;

	get_movw_opcode(&opcode[0], 1, (uint16_t)(patch_func));
	get_movt_opcode(&opcode[1], 1, (uint16_t)(patch_func >> 16));

	opcode[2] = 0xBF004788; // blx r1, nop

	memcpy2((void *)0x51000C26, opcode, sizeof(opcode));

	// SceModuleExports *pExportInfo = (SceModuleExports *)0x51027550;

	return 0;
}
