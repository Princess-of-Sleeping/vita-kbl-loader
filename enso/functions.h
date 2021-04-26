/*
 * functions.h
 * Copyright (C) 2020 Princess of Sleeping
 */
#ifndef _NSKBL_FUNCTIONS_H_
#define _NSKBL_FUNCTIONS_H_

#include <inttypes.h>
#include "nsbl.h"

typedef int SceUID;
typedef unsigned int SceSize;

#ifdef FW_360

typedef struct SceKblSdifCtx SceKblSdifCtx;

typedef int (* SceKernelReadSectorCallback)(SceKblSdifCtx *pCtx, SceSize sector_pos, SceSize sector_num, void *data);

static int (* init_dev_fs)(void) = (void *)(0x5100124D);

typedef struct SceKblSdifCtx {
	int dev_index;     // 0:emmc, 1:gcsd
	void *pSdifPartCtx;
} SceKblSdifCtx;

typedef struct SceKblFsReadCtx { // size is 0x5C
	int flags;
	int data_0x04;                     // some type?
	void *data_0x08;                   // some heap
	void *data_0x0C;                   // align data_0x08
	void *data_0x10;                   // some heap
	void *data_0x14;                   // align data_0x14
	unsigned int entry_sector;         // emmc:os0 sector pos, gcsd:always 0
	unsigned int rsvd_sector;
	unsigned int data_0x20;            // entry_sector + rsvd_sector
	unsigned int fat_size;
	unsigned int num_fats;
	void *data_0x2C;
	void *data_0x30;
	unsigned int root_entry_sector;
	int data_0x38;
	int data_0x3C;
	int data_0x40;
	int data_0x44;
	unsigned int allocation_sector;
	unsigned int allocation_unit_size; // sector_size * allocation_sector
	unsigned int sector_size;          // 0x200
	SceKernelReadSectorCallback cb;
	SceKblSdifCtx *pSceKblSdifCtx;
} SceKblFsReadCtx;

static SceKblSdifCtx *pSdifCtxForGcsd = (SceKblSdifCtx *)(0x51028018);

static SceKblFsReadCtx *fs_read_ctx_gcsd = (SceKblFsReadCtx *)(0x51167728);
static SceKblFsReadCtx *fs_read_ctx_emmc = (SceKblFsReadCtx *)(0x51167784);

static void *kbl_some_storage = (void *)(0x511677E0);

static SceUID *pSceSfatHeapId = (SceUID *)(0x51167184);

// a1 size is 0x5C
// flags : if have 0x10000, need SCE MBR, else Raw FAT16
static int (* fat_init_dev)(SceKblFsReadCtx *a1, int flags, const void *cb, SceKblSdifCtx *a4) = (void*)(0x5101FD19);

/*
 * SceIofilemgr
 */
static int (* sceIoOpen)(const char *path, int flags, int mode)     = (void *)(0x510017D5);
static uint64_t (* sceIoLseek)(int fd, uint64_t offset, int whence) = (void *)(0x510018A9);
static int (* sceIoRead)(int fd, void *data, unsigned int *size)    = (void *)(0x510018E1);
static int (* sceIoClose)(int fd)                                   = (void *)(0x51001901);

/*
 * SceSdif
 */
#define SCE_SDIF_DEV_GAME_CARD 1

static void *(* sceSdifGetSdContextGlobal)(int type) = (void *)(0x5101A9F5);
static int (* sceSdifInitializeSdDevice)(int sd_ctx_index, void **result) = (void *)(0x5101D821);
static int (* sceSdifReadSectorSd)(void *part_ctx, SceSize sector_pos, void *data, SceSize sector_num) = (void *)(0x5101E671);

/*
 * SceSysmem
 */
static int (* sceKernelSysrootIsSafeMode)(void) = (void *)(0x510128FD);

static void *(* sceKernelAllocHeapMemoryWithOpt)(SceUID uid, SceSize size, void *pOpt) = (void *)(0x5100F28D);
static int (* sceKernelFreeHeapMemory)(SceUID uid, void *ptr) = (void *)(0x5100F341);

static void *(* sceKernelMalloc)(SceSize len) = (void *)(0x5100F429);
static void (* sceKernelFree)(void *ptr) = (void *)(0x5100F441);

static int (* sceKernelUidRelease)(SceUID uid) = (void *)(0x51006A51);

static int (* sceKblPrintf)(const char *fmt, ...) = (void *)(0x510137A9);

static SceSize (* strnlen)(const char *s, SceSize maxlen) = (void *)(0x51013B81);
static char *(* strncpy)(char *dst, const char *src, SceSize len) = (void *)(0x510144A1);

static int (* sceSblAimgrIsTool)(void) = (void *)(0x51017139);

/*
 * SceKernelModulemgr
 */
static SceObject *(* get_module_object)(SceUID modid) = (void *)(0x51017649);

#else
#error "No firmware defined or firmware not supported."
#endif

#endif
