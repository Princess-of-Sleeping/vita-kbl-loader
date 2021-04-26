
#include <stdint.h>
#include <stddef.h>

int get_movw_opcode(void *dst, unsigned int target_reg, uint16_t val)
{

	if(dst == NULL)
		return -1;

	if(target_reg > 0xF)
		return -2;

	((uint8_t *)dst)[0] = ((val >> 0xC) & 0xF) | 0x40;
	((uint8_t *)dst)[1] = ((val & 0x800) != 0) ? 0xF6 : 0xF2;
	((uint8_t *)dst)[2] = val & 0xFF;
	((uint8_t *)dst)[3] = (((val & ~0x800) & 0xF00) >> 4) | target_reg;

	return 0;
}

int get_movt_opcode(void *dst, unsigned int target_reg, uint16_t val){

	if(dst == NULL)
		return -1;

	if(target_reg > 0xF)
		return -2;

	((uint8_t *)dst)[0] = ((val >> 0xC) & 0xF) | 0xC0;
	((uint8_t *)dst)[1] = ((val & 0x800) != 0) ? 0xF6 : 0xF2;
	((uint8_t *)dst)[2] = val & 0xFF;
	((uint8_t *)dst)[3] = (((val & ~0x800) & 0xF00) >> 4) | target_reg;

	return 0;
}


static void (*clean_dcache)(void *dst, int len) = (void*)0x5101456D;
static void (*flush_icache)() = (void*)0x51014521;
int (* sceDebugPrintf2)(int msg_type_flag, void *ctx, const char *fmt,...) = (void *)(0x510138B0 | 1);
int (* sceKernelPrintf)(const char *fmt, ...) = (void *)(0x510137A9 | 1);

int log_hook_tmp(void);

int log_hook_main(void){

	if(0){
		sceKernelPrintf("Hello from resume enso\n");
		sceKernelPrintf("log_hook_main:%p\n", log_hook_main);
		sceKernelPrintf("0x51FF0000:0x%08X\n", *(int *)(0x51FF0000));
	}

	clean_dcache((void *)(0x51FF0000), 0x10000);
	flush_icache();

	void (* payload_main)(void) = (void *)(0x51FF0000);
	payload_main();

	return 0;
}

int memcpy_(void *dst, const void *src, int len){

	while(len != 0){
		*(char *)(dst) = *(char *)(src);
		dst += 1;
		src += 1;
		len -= 1;
	}

	return 0;
}

int payload_bootstrap_main(void){

	uintptr_t patch_func;
	int opcode[3];

	patch_func = (uintptr_t)log_hook_tmp;

	get_movw_opcode(&opcode[0], 1, (uint16_t)(patch_func));
	get_movt_opcode(&opcode[1], 1, (uint16_t)(patch_func >> 16));

	opcode[2] = 0xBF004788; // blx r1, nop

	memcpy_((void *)0x51000C26, opcode, sizeof(opcode));

	void *pKblParam = (void *)(0x40200100);
	char *pQAF = (char *)(pKblParam + 0x20);

	// if with enso
	if((pQAF[0] & 1) != 0)
		*(uint16_t *)(0x510200C6) = 0x2101; // movs r1, #1

	// SceModuleExports *pExportInfo = (SceModuleExports *)0x51027550;

	return 0;
}
