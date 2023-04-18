/*
 * Non-secure bootloader main hook
 */

#include <stdint.h>
#include <stddef.h>
#include <psp2kern/types.h>
#include "enso/functions.h"
#include "enso/syscon.h"
#include "arm_opcode.h"
#include "../../kbl_loader.h"


typedef struct SceMbrPartEntry { // size is 0x11-bytes
	unsigned int start_lba;
	unsigned int n_sectors;
	unsigned char id;
	unsigned char type;
	unsigned char flag;
	unsigned short acl;
	unsigned int unused;
} __attribute__((packed)) SceMbrPartEntry;

typedef struct SceMbr { // size is 0x200-bytes
	char magic[0x20];
	unsigned int version;
	unsigned int nSector;
	uint64_t unused;

	unsigned int loader_start;
	unsigned int loader_count;
	unsigned int current_bl_lba;
	unsigned int bl_bank0_lba;

	unsigned int bl_bank1_lba;
	unsigned int current_os_lba;
	uint64_t unused2;
	SceMbrPartEntry entries[0x10];
	char unused3[0x9E];
	unsigned short signature;
} __attribute__((packed)) SceMbr;

typedef struct SceKernelAllocMemBlockKernelOpt {
	SceSize size;                   //!< sizeof(SceKernelAllocMemBlockKernelOpt)
	SceUInt32 field_4;
	SceUInt32 attr;                 //!< OR of SceKernelAllocMemBlockAttr
	SceUInt32 field_C;
	SceUInt32 paddr;
	SceSize alignment;
	SceUInt32 extraLow;
	SceUInt32 extraHigh;
	SceUInt32 mirror_blockid;
	SceUID pid;
	SceKernelPaddrList *paddr_list;
	SceUInt32 field_2C;
	SceUInt32 field_30;
	SceUInt32 field_34;
	SceUInt32 field_38;
	SceUInt32 field_3C;
	SceUInt32 field_40;
	SceUInt32 field_44;
	SceUInt32 field_48;
	SceUInt32 field_4C;
	SceUInt32 field_50;
	SceUInt32 field_54;
} SceKernelAllocMemBlockKernelOpt;

int (* sceKernelPrintf)(const char *fmt, ...) = 0;

int (* sceSdifReadSectorMmc)(void *ctx, SceUInt32 sector_pos, void *data, SceUInt32 nSector) = 0;
int (* sceSdReadSector)(void *ctx, SceUInt32 sector_pos, void *data, SceUInt32 nSector) = 0;
void *(* sceSdifGetSdContextGlobal)(int type) = 0;        
int (* sceSdInit)(int type, void **result) = 0;

void *(* memset)(void *dst, int ch, int n) = 0;
void *(* memcpy)(void *dst, const void *src, int n) = 0;
int (* strncmp)(const char *s1, const char *s2, int n) = 0;
int (* snprintf)(char *dst, int n, const char *fmt, ...) = 0;
void (* clean_dcache)(void *start, int size) = 0;
void (* flush_icache)(void) = 0;

SceUID (* sceKernelAllocMemBlock)(const char *name, SceUInt32 type, SceSize size, void *opt) = 0;
int (* sceKernelGetMemBlockBase)(SceUID memid, void **base) = 0;
int (* sceKernelRemapBlock)(SceUID memid, SceUInt32 type) = 0;

void *(* sceKernelAllocHeapMemory)(SceUID heapId, SceSize length) = 0;
int (* sceKernelFreeHeapMemory)(SceUID heapId, void *ptr) = 0;

int (* sceKernelMMUMapSections)(SceUIntPtr l1base, SceUInt32 memtype, int domain, SceUIntPtr vaddr, SceSSize vsize, SceUIntPtr paddr) = 0;
int (* sceKernelMMUUnmapSectionsWithFlags)(SceUIntPtr l1base, int a2, SceUIntPtr address, SceSize size, int flags) = 0;

int MODULE_LOAD_SD0_REDIRECT = 0;
SceUIntPtr enso_patch = 0;
SceKblSdifCtx *sdif_init_param_emmc = 0;
SceKblSdifCtx *sdif_init_param_gcsd = 0;
SceKernelBootParam **ppKBParam = 0;

const DLSymList dlsym_list[] = {
	DLSYM_ADD_LIST(sceSdifReadSectorMmc),
	DLSYM_ADD_LIST(sceSdReadSector),
	DLSYM_ADD_LIST(sceSdifGetSdContextGlobal),
	DLSYM_ADD_LIST(sceSdInit),
	DLSYM_ADD_LIST(memset),
	DLSYM_ADD_LIST(memcpy),
	DLSYM_ADD_LIST(strncmp),
	DLSYM_ADD_LIST(snprintf),
	DLSYM_ADD_LIST(clean_dcache),
	DLSYM_ADD_LIST(flush_icache),
	DLSYM_ADD_LIST(sceKernelAllocMemBlock),
	DLSYM_ADD_LIST(sceKernelGetMemBlockBase),
	DLSYM_ADD_LIST(sceKernelRemapBlock),
	DLSYM_ADD_LIST(sceKernelAllocHeapMemory),
	DLSYM_ADD_LIST(sceKernelFreeHeapMemory),
	DLSYM_ADD_LIST(sceKernelMMUMapSections),
	DLSYM_ADD_LIST(sceKernelMMUUnmapSectionsWithFlags),
	DLSYM_ADD_LIST(ppKBParam),
	DLSYM_ADD_LIST(enso_patch),
	DLSYM_ADD_LIST(sdif_init_param_emmc),
	DLSYM_ADD_LIST(sdif_init_param_gcsd)
};

#define dlsym_list_number ((sizeof(dlsym_list) / sizeof(dlsym_list[0])))

int dlsym_resolve(void){

	int res;

	for(int i=0;i<dlsym_list_number;i++){
		res = kbl_dlsym(dlsym_list[i].name, dlsym_list[i].result);
		if(res < 0){
			sceKernelPrintf("Cannot resolve \"%s\"\n", dlsym_list[i].name);
			return res;
		}

		if(*(dlsym_list[i].result) == NULL){
			sceKernelPrintf("Symbol \"%s\" is NULL\n", dlsym_list[i].name);
			return res;
		}
	}

	return 0;
}

void *sceSdifGetSdContextPartValidateMmc(int type){

	void *sdif_ctx = sceSdifGetSdContextGlobal(type);
	if(sdif_ctx == NULL)
		return sdif_ctx;

	if(*(uint32_t *)(sdif_ctx + 0x2410) != 1)
		return NULL;

	return *(void **)(sdif_ctx + 0x2414);
}

int enso_first_patch(uint32_t bl_hash){

	// A patch to Enso sector redirection
	void *sdif_emmc_ctx = sceSdifGetSdContextPartValidateMmc(0);
	if(sdif_emmc_ctx != NULL){

		SceMbr *pMbr = (SceMbr *)0x51400000;

		memset(pMbr, 'A', sizeof(*pMbr));

		sceSdifReadSectorMmc(sdif_emmc_ctx, 0, pMbr, 1);

		if(strncmp(pMbr->magic, "Sony Computer Entertainment Inc.", 0x20) == 0){
			for(int i=0;i<0x10;i++){
				if(pMbr->entries[i].id == 3 && pMbr->entries[i].flag == 1){
					sceSdifReadSectorMmc(sdif_emmc_ctx, pMbr->entries[i].start_lba, (void *)0x51400200, 1);

					if(*(uint16_t *)(0x51400200 + 0xB) != 0x200){
						*(uint16_t *)(enso_patch) = 0x2101; // movs r1, #1

						clean_dcache((void *)(enso_patch & ~0x1F), 0x20);
						flush_icache();
					}

					break;
				}
			}
		}
	}

	if(*(int *)(0x51F00000) == 0xB6B6B6B6){
		sceKernelPrintf("Not has second enso\n");
		sceKernelPrintf("-> skip second enso\n");
		return 0;
	}

	// Allocate buffer for code
	SceKernelAllocMemBlockKernelOpt opt;
	int blk;
	void *base;
	void (*f)(uint32_t bl_hash);

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 1;
	opt.field_C = 0x77F8000;

	blk = sceKernelAllocMemBlock("SceEnsoSecondPayload", 0x1020D006, 0x8000, &opt);
	if(blk < 0){
		sceKernelPrintf("sceKernelAllocMemBlock failed: 0x%X\n", blk);
		return blk;
	}

	sceKernelGetMemBlockBase(blk, &base);

	memcpy(base, (void *)0x51F00000, 0x8000);
	*(int *)(0x51F00000) = 0xB6B6B6B6;

	sceKernelRemapBlock(blk, 0x1020D005);
	clean_dcache(base, 0x8000);
	flush_icache();

	sceKernelMMUMapSections((*ppKBParam)->ttbr1.vbase, 0x10208006, 4, VITA_KBL_LOADER_ARGS_BASE, SCE_KERNEL_1MiB, VITA_KBL_LOADER_ARGS_BASE);
	sceKernelMMUMapSections((*ppKBParam)->ttbr1.vbase, 0x10208006, 4, VITA_KBL_LOADER_DLSYM_BASE, SCE_KERNEL_1MiB, VITA_KBL_LOADER_DLSYM_BASE);

	f = (void *)(base + 1);
	f(bl_hash);

	sceKernelMMUUnmapSectionsWithFlags((*ppKBParam)->ttbr1.vbase, 0, VITA_KBL_LOADER_DLSYM_BASE, SCE_KERNEL_1MiB, 0);
	sceKernelMMUUnmapSectionsWithFlags((*ppKBParam)->ttbr1.vbase, 0, VITA_KBL_LOADER_ARGS_BASE, SCE_KERNEL_1MiB, 0);

	return 0;
}

__attribute__((noinline, optimize("O2")))
void hex_dump(const void *addr, int len){

	if(addr == NULL)
		return;

	if(len == 0)
		return;

	while(len >= 0x10){
		sceKernelPrintf(
			"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
			((char *)addr)[0x0], ((char *)addr)[0x1], ((char *)addr)[0x2], ((char *)addr)[0x3],
			((char *)addr)[0x4], ((char *)addr)[0x5], ((char *)addr)[0x6], ((char *)addr)[0x7],
			((char *)addr)[0x8], ((char *)addr)[0x9], ((char *)addr)[0xA], ((char *)addr)[0xB],
			((char *)addr)[0xC], ((char *)addr)[0xD], ((char *)addr)[0xE], ((char *)addr)[0xF]
		);
		addr += 0x10;
		len -= 0x10;
	}

	if(len != 0){
		while(len >= 1){
			sceKernelPrintf("%02X ", ((char *)addr)[0x0]);
			addr += 1;
			len -= 1;
		}

		sceKernelPrintf("\n");
	}
}

int (* sceDebugRegisterPutcharHandler)(void *func, void *argp) = 0;

int sceDebugRegisterPutcharHandler_patch(void *func, void *argp){

	int res;

	res = sceDebugRegisterPutcharHandler(func, argp);

	sceKernelPrintf("NS first printf\n");

	KblLoaderArgs *loader_argp = (KblLoaderArgs *)VITA_KBL_LOADER_ARGS_BASE;

	DLSymVariable *dlsym_tree = loader_argp->dlsym_root;

	while(dlsym_tree != NULL){
		sceKernelPrintf("%p %s\n", dlsym_tree->value, dlsym_tree->name);
		dlsym_tree = dlsym_tree->next;
	}

	return res;
}

int (* sceSdStandaloneInit_SfatInit)(void) = 0;

int sceSdStandaloneInit_SfatInit_patch(void){

	int SfatInit_res, res;

	SfatInit_res = sceSdStandaloneInit_SfatInit();
	if(SfatInit_res < 0){
		return SfatInit_res;
	}

	enso_first_patch(0);

	if(sdif_init_param_gcsd->pSdifPartCtx == NULL){

		syscon_common_write(1, 0x888, 2); // sceSysconCtrlSdPower

		int retry = 3;

		do {
			res = sceSdInit(sdif_init_param_gcsd->dev_index, &(sdif_init_param_gcsd->pSdifPartCtx));
			retry--;
		} while(res < 0 && retry > 0);

		if(res >= 0){
			sceSdReadSector(sdif_init_param_gcsd->pSdifPartCtx, 0, (void *)0x51700000, 1);

			clean_dcache((void *)0x51700000, 0x200);
			// hex_dump((void *)0x51700000, 0x200);
		}
	}

	return SfatInit_res;
}

int g_homebrew_decrypt = 0;
#define SBLAUTHMGR_OFFSET_PATCH_ARG (168)

int (* sceSblAuthMgrAuthHeader)(uint32_t ctx, const void *header, int len, void *args) = 0;
int (* sceSblAuthMgrSetupAuthSegment)(int ctx, uint32_t segidx) = 0;
int (* sceSblAuthMgrAuthSegment)(uint32_t ctx, void *buf, int sz) = 0;

SceUID (* loadModuleCommon)(SceUID pid, const char *path, int flags, void *opt) = 0;

static int sceSblAuthMgrAuthHeader_patch(uint32_t ctx, const void *header, int len, void *args){

	int ret = sceSblAuthMgrAuthHeader(ctx, header, len, args);

	g_homebrew_decrypt = (ret < 0);

	if(g_homebrew_decrypt){
		*(uint32_t *)(args + SBLAUTHMGR_OFFSET_PATCH_ARG) = 0x40;
		ret = 0;
	}

	return ret;
}

static int sceSblAuthMgrSetupAuthSegment_patch(int ctx, uint32_t segidx){

	if(g_homebrew_decrypt){
		return 2; // always compressed!
	}

	return sceSblAuthMgrSetupAuthSegment(ctx, segidx);
}

static int sceSblAuthMgrAuthSegment_patch(uint32_t ctx, void *buf, int sz){

	if(g_homebrew_decrypt){
		return 0;
	}

	return sceSblAuthMgrAuthSegment(ctx, buf, sz);
}

SceUID loadModuleCommon_patch(SceUID pid, const char *path, int flags, void *opt){

	SceUID res;

	char *new_path = sceKernelAllocHeapMemory(0x1000B, 0x400);

	if(new_path != NULL && strncmp(path, "os0:", 4) == 0 && MODULE_LOAD_SD0_REDIRECT != 0){
		snprintf(new_path, 0x400, "%s%s", "sd0:", &(path[4]));
		path = new_path;
	}

	res = loadModuleCommon(pid, path, flags, opt);

	sceKernelFreeHeapMemory(0x1000B, new_path);

	return res;
}

// Invoked by scratchpad set upper
int boot_main(void){

	int res;

	res = kbl_dlsym("sceKernelPrintf", (void **)&sceKernelPrintf);
	if(res < 0){
		return res;
	}

	res = dlsym_resolve();
	if(res < 0){
		sceKernelPrintf("dlsym_resolve 0x%X\n", res);
		return res;
	}

	kbl_dlsym("MODULE_LOAD_SD0_REDIRECT", (void **)&MODULE_LOAD_SD0_REDIRECT);

	uint32_t x;

	res = kbl_dlsym("NSKBL_auth_module_sceSblAuthMgrAuthHeader", (void **)&x);
	if(res >= 0){
		arm_thumb_branch_decode(x, (const uint16_t *)x, NULL, (uint32_t *)&sceSblAuthMgrAuthHeader);
		arm_thumb_branch_encode(x, (uint32_t)&sceSblAuthMgrAuthHeader_patch, ARM_THUBM_TYPE_BL, (uint16_t *)x);
	}

	res = kbl_dlsym("NSKBL_auth_module_sceSblAuthMgrSetupAuthSegment", (void **)&x);
	if(res >= 0){
		arm_thumb_branch_decode(x, (const uint16_t *)x, NULL, (uint32_t *)&sceSblAuthMgrSetupAuthSegment);
		arm_thumb_branch_encode(x, (uint32_t)&sceSblAuthMgrSetupAuthSegment_patch, ARM_THUBM_TYPE_BL, (uint16_t *)x);
	}

	res = kbl_dlsym("NSKBL_auth_module_sceSblAuthMgrAuthSegment", (void **)&x);
	if(res >= 0){
		arm_thumb_branch_decode(x, (const uint16_t *)x, NULL, (uint32_t *)&sceSblAuthMgrAuthSegment);
		arm_thumb_branch_encode(x, (uint32_t)&sceSblAuthMgrAuthSegment_patch, ARM_THUBM_TYPE_BL, (uint16_t *)x);
	}

	res = kbl_dlsym("NSKBL_sceKernelLoadModule_loadModuleCommon", (void **)&x);
	if(res >= 0){
		arm_thumb_branch_decode(x, (const uint16_t *)x, NULL, (uint32_t *)&loadModuleCommon);
		arm_thumb_branch_encode(x, (uint32_t)&loadModuleCommon_patch, ARM_THUBM_TYPE_BL, (uint16_t *)x);
	}

	res = kbl_dlsym("NSKBL_boot_sceDebugRegisterPutcharHandler", (void **)&x);
	if(res >= 0){
		arm_thumb_branch_decode(x, (const uint16_t *)x, NULL, (uint32_t *)&sceDebugRegisterPutcharHandler);
		arm_thumb_branch_encode(x, (uint32_t)&sceDebugRegisterPutcharHandler_patch, ARM_THUBM_TYPE_BL, (uint16_t *)x);
	}

	res = kbl_dlsym("NSKBL_sceSdStandaloneInit_SfatInit", (void **)&x);
	if(res >= 0){
		arm_thumb_branch_decode(x, (const uint16_t *)x, NULL, (uint32_t *)&sceSdStandaloneInit_SfatInit);
		arm_thumb_branch_encode(x, (uint32_t)&sceSdStandaloneInit_SfatInit_patch, ARM_THUBM_TYPE_BL, (uint16_t *)x);
	}

	return 0;
}
