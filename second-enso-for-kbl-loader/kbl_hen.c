/* second.c -- bootloader patches
 *
 * Copyright (C) 2023 Princess of Sleeping
 */

#include <psp2kern/types.h>


#define sceKernelGetDACR() ({ \
	SceUIntPtr v; \
	asm volatile ("mrc p15, #0, %0, c3, c0, #0\n": "=r"(v)); \
	v; \
	})

#define sceKernelSetDACR(value) ({ \
	asm volatile ("mcr p15, #0, %0, c3, c0, #0\n":: "r"((value))); \
	})



extern int (* strncmp)(const char *s1, const char *s2, int n);
extern void *(* memcpy)(void *dst, const void *src, int len);
extern void (* sceKernelDcacheCleanRange)(void *start, int size);
extern void (* sceKernelL1IcacheInvalidateEntireAllCore)(void);


int get_movw_opcode(void *dst, unsigned int target_reg, uint16_t val){

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

void hook_function(SceUIntPtr dst, int dst_size, SceUIntPtr refer, SceUIntPtr function){

	SceUInt16 jump_code[(4 * 3) / 2];
	SceUInt16 hook_code[((4 * 3) + (4 * 2 + 2)) / 2];

	hook_code[0x0] = 0xBF00; // nop
	hook_code[0x1] = 0xBF00; // nop
	hook_code[0x2] = 0xBF00; // nop
	hook_code[0x3] = 0xBF00; // nop
	hook_code[0x4] = 0xBF00; // nop
	hook_code[0x5] = 0xBF00; // nop
	hook_code[0x6] = 0xBF00; // movw
	hook_code[0x7] = 0xBF00; // movw
	hook_code[0x8] = 0xBF00; // movt
	hook_code[0x9] = 0xBF00; // movt
	hook_code[0xA] = 0xBF00; // bx

	memcpy(&(hook_code[0x0]), (void *)dst, dst_size);

	SceUIntPtr f = (dst + dst_size) | 1;

	get_movw_opcode(&(hook_code[0x6]), 0xC, (uint16_t)(f));
	get_movt_opcode(&(hook_code[0x8]), 0xC, (uint16_t)(f >> 16));

	hook_code[0xA] = 0x4760; // bx ip

	memcpy((void *)(refer & ~1), hook_code, sizeof(hook_code));

	sceKernelDcacheCleanRange((void *)(refer & ~0x1F), ((refer & 0x1F) + sizeof(hook_code) + 0x1F) & ~0x1F);

	jump_code[0] = 0xBF00; // movw
	jump_code[1] = 0xBF00; // movw
	jump_code[2] = 0xBF00; // movt
	jump_code[3] = 0xBF00; // movt
	jump_code[4] = 0xBF00; // bx ip
	jump_code[5] = 0xBF00; // nop (padding)

	get_movw_opcode(&(jump_code[0]), 0xC, (uint16_t)(function));
	get_movt_opcode(&(jump_code[2]), 0xC, (uint16_t)(function >> 16));

	jump_code[4] = 0x4760; // bx ip

	memcpy((void *)dst, jump_code, dst_size);

	sceKernelDcacheCleanRange((void *)(dst & ~0x1F), ((dst & 0x1F) + dst_size + 0x1F) & ~0x1F);
	sceKernelL1IcacheInvalidateEntireAllCore();
}

static SceBool is_fself_module = SCE_FALSE;

static int (* sceSblAuthMgrAuthHeader_store)(int handle, void *pSelfHeader, int SelfHeaderSize, void *ctx130) = (void *)(0x51480000 | 1);

static int sceSblAuthMgrAuthHeader_patch(int handle, void *pSelfHeader, int SelfHeaderSize, void *ctx130){

	int res;
	SceUInt32 dacr;

	res = sceSblAuthMgrAuthHeader_store(handle, pSelfHeader, SelfHeaderSize, ctx130);

	// sceKernelPrintf("sceSblAuthMgrAuthHeader 0x%X\n", res);

	dacr = sceKernelGetDACR();
	sceKernelSetDACR(dacr | 0xFFFF0000);
	is_fself_module = res == 0x800F0616;
	sceKernelSetDACR(dacr);

	if(is_fself_module == SCE_TRUE){
		res = 0;
	}

	return res;
}

static int (* sceSblAuthMgrAuthSegment_store)(int handle, void *buffer, SceSize len) = (void *)(0x51480040 | 1);

static int sceSblAuthMgrAuthSegment_patch(int handle, void *buffer, SceSize len){

	int res;

	if(is_fself_module == SCE_TRUE){
		res = 0;
	}else{
		res = sceSblAuthMgrAuthSegment_store(handle, buffer, len);
	}

	// sceKernelPrintf("sceSblAuthMgrAuthSegment 0x%X\n", res);

	return res;
}

static int (* sceSblAuthMgrSetupAuthSegment_store)(int handle, int segment_number) = (void *)(0x51480080 | 1);

static int sceSblAuthMgrSetupAuthSegment_patch(int handle, int segment_number){

	int res;

	if(handle != 1){
		return 0x800F0509;
	}

	if(is_fself_module == SCE_TRUE){
		res = 2; // should be compressed data always ...
	}else{
		res = sceSblAuthMgrSetupAuthSegment_store(handle, segment_number);
	}

	// sceKernelPrintf("sceSblAuthMgrSetupAuthSegment 0x%X\n", res);

	return res;
}

static SceUID (* sceKernelLoadModule_store)(const char *path, int flags, void *opt) = (void *)(0x514800C0 | 1);

static SceUID sceKernelLoadModule_patch(const char *path, int flags, void *opt){

	SceUID res;

	res = sceKernelLoadModule_store(path, flags, opt);

	// sceKernelPrintf("sceKernelLoadModule 0x%X for %s\n", res, path);

	if(res == 0x800F0B33){

		if(strncmp(path, "os0:kd/deci4p_sdbgp.skprx", 25) == 0){
			path = "sd0:kd-350/deci4p_sdbgp.skprx";
		}else if(strncmp(path, "os0:kd/deci4p_sdrfp.skprx", 25) == 0){
			path = "sd0:kd-350/deci4p_sdrfp.skprx";
		}else{
			path = NULL;
		}

		if(path != NULL){
			res = sceKernelLoadModule_store(path, flags, opt);
			// sceKernelPrintf("sceKernelLoadModule 0x%X for %s\n", res, path);
		}
	}

	return res;
}

int kbl_hen_start(void){

	hook_function(0x5102050c, 10, (SceUIntPtr)(sceSblAuthMgrAuthHeader_store) & ~1, (SceUIntPtr)sceSblAuthMgrAuthHeader_patch);
	hook_function(0x51020680, 10, (SceUIntPtr)(sceSblAuthMgrAuthSegment_store) & ~1, (SceUIntPtr)sceSblAuthMgrAuthSegment_patch);
	hook_function(0x51020644, 12, (SceUIntPtr)(sceSblAuthMgrSetupAuthSegment_store) & ~1, (SceUIntPtr)sceSblAuthMgrSetupAuthSegment_patch);

	hook_function(0x51021424, 10, (SceUIntPtr)(sceKernelLoadModule_store) & ~1, (SceUIntPtr)sceKernelLoadModule_patch);

	return 0;
}
