
#include <stdint.h>
#include <stddef.h>
#include "../kbl_loader.h"

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


// NSKBL doesn't mmu mapping the scratchpad, so you can't put enso setup here.


int payload_bootstrap_main(void){

	int res;
	void *SCE_NS_KBL_PARAM_BASE;

	KblLoaderArgs *argp = (KblLoaderArgs *)VITA_KBL_LOADER_ARGS_BASE;

	res = kbl_dlsym("SCE_NS_KBL_PARAM_BASE", &SCE_NS_KBL_PARAM_BASE);
	if(res >= 0){
		memcpy2(SCE_NS_KBL_PARAM_BASE, &(argp->kbl_param), 0x200);
	}

	// Invoke NSKBL patcher set upper
	if(*(int *)(0x51FF0000) != 0xB6B6B6B6){
		((int (*)())(0x51FF0000))();
	}

	return 0;
}
