/*
 * functions.h
 * Copyright (C) 2020 Princess of Sleeping
 */
#ifndef _NSKBL_FUNCTIONS_H_
#define _NSKBL_FUNCTIONS_H_

#include <psp2kern/types.h>
#include <psp2kern/kernel/kbl/kbl.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysmem.h>

typedef struct SceKblSdifCtx {
	int dev_index;     // 0:emmc, 1:gcsd
	void *pSdifPartCtx;
} SceKblSdifCtx;

typedef int (* SceKernelReadSectorCallback)(SceKblSdifCtx *pCtx, SceSize sector_pos, SceSize sector_num, void *data);

typedef struct SceKblFsReadCtx { // size is 0x5C-bytes
	int flags;
	int data_0x04;                     // some type?
	void *data_0x08;                   // some heap
	void *data_0x0C;                   // align data_0x08
	void *data_0x10;                   // some heap
	void *data_0x14;                   // align data_0x14
	unsigned int entry_sector;         // emmc:os0 sector position, gcsd:always 0
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

#define FW_360

#ifdef FW_360

#define pSdifCtxForGcsd ((SceKblSdifCtx *)0x51028018)

/*
static SceKblFsReadCtx *fs_read_ctx_gcsd = (SceKblFsReadCtx *)(0x51167728);
static SceKblFsReadCtx *fs_read_ctx_emmc = (SceKblFsReadCtx *)(0x51167784);

static void *kbl_some_storage = (void *)(0x511677E0);

static SceUID *pSceSfatHeapId = (SceUID *)(0x51167184);

static int (* init_dev_fs)(void) = (void *)(0x5100124D);
*/

// flags : if have 0x10000, need SCE MBR, else Raw FAT16
// static int (* fat_init_dev)(SceKblFsReadCtx *a1, int flags, const void *cb, SceKblSdifCtx *a4) = (void*)(0x5101FD19);

void ksceKernelCpuDcacheCleanMVACRange(void *base, int range);
void ksceKernelCpuIcacheInvalidateAllUIS(void);

#define sceKernelPrintf ((__typeof__(&ksceDebugPrintf))0x510137A9)

#define flush_icache() ((__typeof__(&ksceKernelCpuIcacheInvalidateAllUIS))0x51014521)()
#define clean_dcache(base, range) ((__typeof__(&ksceKernelCpuDcacheCleanMVACRange))0x5101456D)(base, range)

#define strncmp(_s1_, _s2_, _len_) ((__typeof__(&strncmp))0x51013B30)(_s1_, _s2_, _len_)
#define memset(dst, ch, len) ((__typeof__(&memset))0x51013AD1)(dst, ch, len)
#define memcpy(dst, src, len) ((__typeof__(&memcpy))0x51013A51)(dst, src, len)

#define sceSblAimgrIsTool() ((int (*)(void))0x51017139)()

/*
 * SceSdif
 */
#define SCE_SDIF_DEV_GAME_CARD 1

#define sceSdifReadSectorMmc(_ctx_, _sector_pos_, _data_, _nSector_) ((int (*)(void *ctx, SceSize sector_pos, void *data, SceSize nSector))0x5101C5C9)(_ctx_, _sector_pos_, _data_, _nSector_)

#define sceSdifGetSdContextGlobal(_type_) ((void *(*)(int type))0x5101A9F5)(_type_)
#define sceSdifInitializeSdDevice(_sd_ctx_index_, _result_) ((int (*)(int sd_ctx_index, void **result))0x5101D821)(_sd_ctx_index_, _result_)


#define sceKernelAllocMemBlock(name, type, size, opt) ((__typeof__(&ksceKernelAllocMemBlock))0x510086C1)(name, type, size, opt)
#define sceKernelGetMemBlockBase(memid, base) ((__typeof__(&ksceKernelGetMemBlockBase))0x510040E5)(memid, base)
#define sceKernelRemapBlock(memid, memtype) ((__typeof__(&ksceKernelRemapBlock))0x510086D1)(memid, memtype)




/*
 * SceIofilemgr
 */
/*
static int (* sceIoOpen)(const char *path, int flags, int mode)     = (void *)(0x510017D5);
static uint64_t (* sceIoLseek)(int fd, uint64_t offset, int whence) = (void *)(0x510018A9);
static int (* sceIoRead)(int fd, void *data, unsigned int *size)    = (void *)(0x510018E1);
static int (* sceIoClose)(int fd)                                   = (void *)(0x51001901);
*/

/*
 * SceSysmem
 */
/*
static int (* sceKernelSysrootIsSafeMode)(void) = (void *)(0x510128FD);

static void *(* sceKernelAllocHeapMemoryWithOpt)(SceUID uid, SceSize size, void *pOpt) = (void *)(0x5100F28D);
static int (* sceKernelFreeHeapMemory)(SceUID uid, void *ptr) = (void *)(0x5100F341);

static void *(* sceKernelMalloc)(SceSize len) = (void *)(0x5100F429);
static void (* sceKernelFree)(void *ptr) = (void *)(0x5100F441);

static int (* sceKernelUidRelease)(SceUID uid) = (void *)(0x51006A51);

static SceSize (* strnlen)(const char *s, SceSize maxlen) = (void *)(0x51013B81);
static char *(* strncpy)(char *dst, const char *src, SceSize len) = (void *)(0x510144A1);
*/

SceUID sceKernelLoadModuleForPidInternal(SceUID pid, const char *path, int flags, void *option);

#define sceKernelLoadModuleForPidInternal(pid, path, flags, option) ((__typeof__(&sceKernelLoadModuleForPidInternal))0x51017831)(pid, path, flags, option)

#else
#error "No firmware defined or firmware not supported."
#endif

#endif
