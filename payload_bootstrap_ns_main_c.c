/*
 * Non-secure bootloader main hook
 */

#include <stdint.h>
#include <stddef.h>

#include "enso/nsbl.h"
#include "enso/functions.h"
#include "enso/syscon.h"

int (* sceKernelPrintf)(const char *fmt, ...) = (void *)(0x510137A9 | 1);

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

int memcpy_(void *dst, const void *src, int len){

	while(len != 0){
		*(char *)(dst) = *(char *)(src);
		dst += 1;
		src += 1;
		len -= 1;
	}

	return 0;
}

int psp2bootconfig_load_hook(void);

int psp2bootconfig_load_hook_main(void){
	sceKernelPrintf("ScePsp2BootConfig loading\n");

	SceBootArgs *boot_args = (*sysroot_ctx_ptr)->boot_args;

	syscon_common_write(1, 0x888, 2); // sceSysconCtrlSdPower

	sceKernelPrintf("sceSysconCtrlSdPower OK\n");

	int res, retry_count = 3;

	do {
		res = sceSdifInitializeSdDevice(pSdifCtxForGcsd->dev_index, &(pSdifCtxForGcsd->pSdifPartCtx));
		retry_count--;
	} while(retry_count != 0 && res < 0);

	sceKernelPrintf("sceSdifInitializeSdDevice:0x%X\n", res);

	if(pSdifCtxForGcsd->pSdifPartCtx != NULL)
		fat_init_dev(fs_read_ctx_gcsd, 0x100000, (const void *)(0x510010c5), (SceKblSdifCtx *)(0x51028018));


	if(*(int *)(0x51F00000) == 0xB6B6B6B6){
		sceKernelPrintf("Not has second enso\n");
		sceKernelPrintf("-> skip second enso\n");
		return 0;
	}

	// allocate buffer for code
	int blk;
	void *base;
	void (*f)();

	blk = sceKernelAllocMemBlock("ScePayload", 0x1020D006, 0x8000, NULL);

	sceKernelPrintf("sceKernelAllocMemBlock:0x%X\n", blk);

	sceKernelGetMemBlockBase(blk, &base);

	memcpy(base, (void *)0x51F00000, 0x8000);

	sceKernelRemapBlock(blk, 0x1020D005);
	clean_dcache(base, 0x8000);
	flush_icache();
	f = (void*)(base + 1);

	f();

	return 0;
}

static int (* sceKernelLoadModuleForPidInternal)(int pid, const char *path, int flags, void *option) = (void *)(0x51017830 | 1);

static int sceKernelLoadModule(const char *path, int flags, void *option){

	if((flags & 0xfff8260f) == 0){

		if(strncmp(path, "os0:kd/deci4p_sdbgp.skprx", 25) == 0)
			path = "sd0:kd/deci4p_sdbgp.skprx";

		if(strncmp(path, "os0:kd/deci4p_sdrfp.skprx", 25) == 0)
			path = "sd0:kd/deci4p_sdrfp.skprx";

		if(strncmp(path, "os0:kd/sdbgsdio.skprx", 21) == 0)
			path = "sd0:kd/sdbgsdio.skprx";

		if(strncmp(path, "os0:psp2bootconfig.skprx", 24) == 0)
			path = "sd0:psp2bootconfig.skprx";

		int res;

		res = sceKernelLoadModuleForPidInternal(0x10005, path, flags, option);

		if(res < 0)
			sceKernelPrintf("%s res:0x%X %s 0x%X\n", __FUNCTION__, res, path, flags);

		return res;
	}

	sceKernelPrintf("%s flags error\n", __FUNCTION__);

	return 0x8002000A;
}

int g_sigpatch_disabled = 0;
int g_homebrew_decrypt = 0;
#define SBLAUTHMGR_OFFSET_PATCH_ARG (168)

int sceSblAuthMgrAuthHeader_patch(uint32_t ctx, const void *header, int len, void *args);

static int sceSblAuthMgrAuthHeader(uint32_t ctx, const void *header, int len, void *args){
    int ret = sceSblAuthMgrAuthHeader_patch(ctx, header, len, args);
    if (!g_sigpatch_disabled) {
        // DACR_OFF(
            g_homebrew_decrypt = (ret < 0);
        // );

        if (g_homebrew_decrypt) {
            *(uint32_t *)(args + SBLAUTHMGR_OFFSET_PATCH_ARG) = 0x40;
            ret = 0;
        }
    }
    return ret;
}

int sceSblAuthMgrSetupAuthSegment_patch(int ctx, uint32_t segidx);

static int sceSblAuthMgrSetupAuthSegment(int ctx, uint32_t segidx){

	if(!g_sigpatch_disabled){
		if (g_homebrew_decrypt) {
			return 2; // always compressed!
		}
	}

	return sceSblAuthMgrSetupAuthSegment_patch(ctx, segidx);
}

int sceSblAuthMgrAuthSegment_patch(uint32_t ctx, void *buf, int sz);

static int sceSblAuthMgrAuthSegment(uint32_t ctx, void *buf, int sz){
    if (!g_sigpatch_disabled) {
        if (g_homebrew_decrypt) {
            return 0;
        }
    }
    return sceSblAuthMgrAuthSegment_patch(ctx, buf, sz);
}

int payload_bootstrap_ns_main_c(void){

	uintptr_t patch_func;
	int opcode[3];

	patch_func = (uintptr_t)psp2bootconfig_load_hook;

	get_movw_opcode(&opcode[0], 1, (uint16_t)(patch_func));
	get_movt_opcode(&opcode[1], 1, (uint16_t)(patch_func >> 16));

	opcode[2] = 0xBF004788; // blx r1, nop

	memcpy_((void *)0x51001674, opcode, sizeof(opcode));

	clean_dcache((void *)0x51001660, 0x40);
	flush_icache();

	// for load kernel module sd0:
	if(sceSblAimgrIsTool() != 0 || *(uint16_t *)(0x40200100 + 0xA2) == 0x101){
		patch_func = (uintptr_t)sceKernelLoadModule;

		get_movw_opcode(&opcode[0], 12, (uint16_t)(patch_func));
		get_movt_opcode(&opcode[1], 12, (uint16_t)(patch_func >> 16));

		opcode[2] = 0xBF004760; // blx ip, nop

		memcpy_((void *)0x51017930, opcode, sizeof(opcode));

		clean_dcache((void *)0x51017920, 0x40);
		flush_icache();
	}

	patch_func = (uintptr_t)sceSblAuthMgrAuthHeader;

	get_movw_opcode(&opcode[0], 12, (uint16_t)(patch_func));
	get_movt_opcode(&opcode[1], 12, (uint16_t)(patch_func >> 16));

	opcode[2] = 0xBF004760; // blx ip, nop

	memcpy_((void *)0x51016d68, opcode, sizeof(opcode));

	clean_dcache((void *)0x51016d60, 0x40);
	flush_icache();

	patch_func = (uintptr_t)sceSblAuthMgrSetupAuthSegment;

	get_movw_opcode(&opcode[0], 12, (uint16_t)(patch_func));
	get_movt_opcode(&opcode[1], 12, (uint16_t)(patch_func >> 16));

	opcode[2] = 0xBF004760; // blx ip, nop

	memcpy_((void *)0x51016e58, opcode, sizeof(opcode));

	clean_dcache((void *)0x51016e40, 0x40);
	flush_icache();

	patch_func = (uintptr_t)sceSblAuthMgrAuthSegment;

	get_movw_opcode(&opcode[0], 12, (uint16_t)(patch_func));
	get_movt_opcode(&opcode[1], 12, (uint16_t)(patch_func >> 16));

	opcode[2] = 0xBF004760; // blx ip, nop

	memcpy_((void *)0x51016e94, opcode, sizeof(opcode));

	clean_dcache((void *)0x51016e80, 0x40);
	flush_icache();

	return 0;
}
