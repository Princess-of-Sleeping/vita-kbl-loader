/*
 * Non-secure bootloader main hook
 */

#include <stdint.h>
#include <stddef.h>
#include <psp2kern/types.h>
#include "enso/functions.h"
#include "enso/syscon.h"
#include "arm_opcode.h"
#include "hen.h"

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

void *sceSdifGetSdContextPartValidateMmc(int type){

	void *sdif_ctx = sceSdifGetSdContextGlobal(type);
	if(sdif_ctx == NULL)
		return sdif_ctx;

	if(*(uint32_t *)(sdif_ctx + 0x2410) != 1)
		return NULL;

	return *(void **)(sdif_ctx + 0x2414);
}

int enso_first_patch(void){

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
						*(uint16_t *)(0x510200C6) = 0x2101; // movs r1, #1

						clean_dcache((void *)0x510200C0, 0x20);
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
	int blk;
	void *base;
	void (*f)();

	blk = sceKernelAllocMemBlock("SceEnsoSecondPayload", 0x1020D006, 0x8000, NULL);
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

	f = (void *)(base + 1);
	f();

	return 0;
}

int psp2bootconfig_load_hook(void);

int psp2bootconfig_load_hook_main(void){

	syscon_common_write(1, 0x888, 2); // sceSysconCtrlSdPower

	if(pSdifCtxForGcsd->pSdifPartCtx == NULL){
		int res, retry_count = 3;

		do {
			res = sceSdifInitializeSdDevice(pSdifCtxForGcsd->dev_index, &(pSdifCtxForGcsd->pSdifPartCtx));
			retry_count--;
		} while(retry_count != 0 && res < 0);

		if(res >= 0){
			nskbl_install_hen();
		}
	}

	enso_first_patch();

	return 0;
}

int boot_main(void){

	uintptr_t patch_func;
	int opcode[3];

	patch_func = (uintptr_t)psp2bootconfig_load_hook;

	get_movw_opcode(&opcode[0], 1, (uint16_t)(patch_func));
	get_movt_opcode(&opcode[1], 1, (uint16_t)(patch_func >> 16));

	opcode[2] = 0xBF004788; // blx r1, nop

	memcpy((void *)0x510012e8, opcode, sizeof(opcode));
	clean_dcache((void *)0x510012e0, 0x20);
	flush_icache();

	return 0;
}
