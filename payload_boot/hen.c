
#include <stdint.h>
#include <stddef.h>
#include <psp2kern/types.h>
#include "enso/functions.h"
#include "arm_opcode.h"
#include "hen.h"

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

	g_homebrew_decrypt = (ret < 0);

	if(g_homebrew_decrypt){
		*(uint32_t *)(args + SBLAUTHMGR_OFFSET_PATCH_ARG) = 0x40;
		ret = 0;
	}

	return ret;
}

int sceSblAuthMgrSetupAuthSegment_patch(int ctx, uint32_t segidx);

static int sceSblAuthMgrSetupAuthSegment(int ctx, uint32_t segidx){

	if(g_homebrew_decrypt){
		return 2; // always compressed!
	}

	return sceSblAuthMgrSetupAuthSegment_patch(ctx, segidx);
}

int sceSblAuthMgrAuthSegment_patch(uint32_t ctx, void *buf, int sz);

static int sceSblAuthMgrAuthSegment(uint32_t ctx, void *buf, int sz){

	if(g_homebrew_decrypt){
		return 0;
	}

	return sceSblAuthMgrAuthSegment_patch(ctx, buf, sz);
}

int nskbl_install_hen(void){

	uintptr_t patch_func;
	int opcode[3];

	// for load kernel module sd0:
	if(sceSblAimgrIsTool() != 0 || *(uint16_t *)(0x40200100 + 0xA2) == 0x101){
		patch_func = (uintptr_t)sceKernelLoadModule;

		get_movw_opcode(&opcode[0], 12, (uint16_t)(patch_func));
		get_movt_opcode(&opcode[1], 12, (uint16_t)(patch_func >> 16));

		opcode[2] = 0xBF004760; // blx ip, nop

		memcpy((void *)0x51017930, opcode, sizeof(opcode));

		clean_dcache((void *)0x51017920, 0x40);
		flush_icache();

		patch_func = (uintptr_t)sceSblAuthMgrAuthHeader;

		get_movw_opcode(&opcode[0], 12, (uint16_t)(patch_func));
		get_movt_opcode(&opcode[1], 12, (uint16_t)(patch_func >> 16));

		opcode[2] = 0xBF004760; // blx ip, nop

		memcpy((void *)0x51016d68, opcode, sizeof(opcode));

		clean_dcache((void *)0x51016d60, 0x40);
		flush_icache();

		patch_func = (uintptr_t)sceSblAuthMgrSetupAuthSegment;

		get_movw_opcode(&opcode[0], 12, (uint16_t)(patch_func));
		get_movt_opcode(&opcode[1], 12, (uint16_t)(patch_func >> 16));

		opcode[2] = 0xBF004760; // blx ip, nop

		memcpy((void *)0x51016e58, opcode, sizeof(opcode));

		clean_dcache((void *)0x51016e40, 0x40);
		flush_icache();

		patch_func = (uintptr_t)sceSblAuthMgrAuthSegment;

		get_movw_opcode(&opcode[0], 12, (uint16_t)(patch_func));
		get_movt_opcode(&opcode[1], 12, (uint16_t)(patch_func >> 16));

		opcode[2] = 0xBF004760; // blx ip, nop

		memcpy((void *)0x51016e94, opcode, sizeof(opcode));

		clean_dcache((void *)0x51016e80, 0x40);
		flush_icache();
	}

    return 0;
}
