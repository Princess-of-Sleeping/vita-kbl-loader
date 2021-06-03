
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/lowio/pervasive.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/display.h>
#include <psp2kern/power.h>
#include <psp2kern/sblaimgr.h>
#include <psp2kern/syscon.h>
#include <psp2kern/uart.h>
#include <taihen.h>
#include "sysroot.h"
#include "config.h"

#define LOG(s, ...) \
	do { \
		char _buffer[256]; \
		snprintf(_buffer, sizeof(_buffer), s, ##__VA_ARGS__); \
		uart0_print(_buffer); \
	} while (0)

void _start() __attribute__ ((weak, alias ("module_start")));

#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))

typedef struct SceSysconResumeContext {
	unsigned int size;
	unsigned int unk;
	unsigned int buff_vaddr;
	unsigned int resume_func_vaddr;
	unsigned int SCTLR;
	unsigned int ACTLR;
	unsigned int CPACR;
	unsigned int TTBR0;
	unsigned int TTBR1;
	unsigned int TTBCR;
	unsigned int DACR;
	unsigned int PRRR;
	unsigned int NMRR;
	unsigned int VBAR;
	unsigned int CONTEXTIDR;
	unsigned int TPIDRURW;
	unsigned int TPIDRURO;
	unsigned int TPIDRPRW;
	unsigned int unk2[6];
	unsigned long long time;
} SceSysconResumeContext;

extern void resume_function(void);

static unsigned int *get_lvl1_page_table_va(void);
static int find_paddr(uint32_t paddr, const void *vaddr_start, unsigned int range, void **found_vaddr);
static int alloc_phycont(unsigned int size, unsigned int alignment,  SceUID *uid, void **addr);

static tai_hook_ref_t SceSyscon_ksceSysconResetDevice_ref;
static SceUID SceSyscon_ksceSysconResetDevice_hook_uid = -1;
static tai_hook_ref_t SceSyscon_ksceSysconSendCommand_ref;
static SceUID SceSyscon_ksceSysconSendCommand_hook_uid = -1;

static SceSysconResumeContext resume_ctx;
static uintptr_t resume_ctx_paddr;
static unsigned int resume_ctx_buff[32];
/*
 * Global variables used by the resume function.
 */
uintptr_t payload_load_paddr;
unsigned int payload_size;
uintptr_t sysroot_buffer_paddr;
void *lvl1_pt_va;

void (* sceKernelCpuDcacheWritebackInvalidateAll)(void);
void (* sceKernelCpuIcacheInvalidateAll)(void);
int (* sceKernelCpuDcacheCleanInvalidateMVAC)(void *a1);
void (* sceKernelCpuIcacheInvalidateRange)(const void *ptr, SceSize len);

int module_get_offset(SceUID pid, SceUID modid, int segidx, uint32_t offset, uintptr_t *dst);
int module_get_export_func(SceUID pid, const char *modname, uint32_t lib_nid, uint32_t func_nid, uintptr_t *func);
#define GetExport(modname, lib_nid, func_nid, func) module_get_export_func(0x10005, modname, lib_nid, func_nid, (uintptr_t *)func)

static void setup_payload(void)
{
	memset(&resume_ctx, 0, sizeof(resume_ctx));
	resume_ctx.size = sizeof(resume_ctx);
	resume_ctx.buff_vaddr = (unsigned int )resume_ctx_buff;
	resume_ctx.resume_func_vaddr = (unsigned int)&resume_function;
	asm volatile("mrc p15, 0, %0, c1, c0, 0\n\t" : "=r"(resume_ctx.SCTLR));
	asm volatile("mrc p15, 0, %0, c1, c0, 1\n\t" : "=r"(resume_ctx.ACTLR));
	asm volatile("mrc p15, 0, %0, c1, c0, 2\n\t" : "=r"(resume_ctx.CPACR));
	asm volatile("mrc p15, 0, %0, c2, c0, 0\n\t" : "=r"(resume_ctx.TTBR0));
	asm volatile("mrc p15, 0, %0, c2, c0, 1\n\t" : "=r"(resume_ctx.TTBR1));
	asm volatile("mrc p15, 0, %0, c2, c0, 2\n\t" : "=r"(resume_ctx.TTBCR));
	asm volatile("mrc p15, 0, %0, c3, c0, 0\n\t" : "=r"(resume_ctx.DACR));
	asm volatile("mrc p15, 0, %0, c10, c2, 0\n\t" : "=r"(resume_ctx.PRRR));
	asm volatile("mrc p15, 0, %0, c10, c2, 1\n\t" : "=r"(resume_ctx.NMRR));
	asm volatile("mrc p15, 0, %0, c12, c0, 0\n\t" : "=r"(resume_ctx.VBAR));
	asm volatile("mrc p15, 0, %0, c13, c0, 1\n\t" : "=r"(resume_ctx.CONTEXTIDR));
	asm volatile("mrc p15, 0, %0, c13, c0, 2\n\t" : "=r"(resume_ctx.TPIDRURW));
	asm volatile("mrc p15, 0, %0, c13, c0, 3\n\t" : "=r"(resume_ctx.TPIDRURO));
	asm volatile("mrc p15, 0, %0, c13, c0, 4\n\t" : "=r"(resume_ctx.TPIDRPRW));
	resume_ctx.time = ksceKernelGetSystemTimeWide();

	ksceKernelCpuDcacheAndL2WritebackRange(&resume_ctx, sizeof(resume_ctx));

	lvl1_pt_va = get_lvl1_page_table_va();

	// LOG("Level 1 page table virtual address: %p\n", lvl1_pt_va);
}

static int ksceSysconResetDevice_hook_func(int type, int mode)
{
	// LOG("ksceSysconResetDevice(0x%08X, 0x%08X)\n", type, mode);

	/*
	 * The Vita OS thinks it's about to poweroff, but we will instead
	 * setup the payload and trigger a soft reset.
	 */
	if (type == SCE_SYSCON_RESET_TYPE_POWEROFF) {
		setup_payload();
		type = SCE_SYSCON_RESET_TYPE_SOFT_RESET;
	}

	// LOG("Resetting the device!\n");

	sceKernelCpuDcacheWritebackInvalidateAll();
	sceKernelCpuIcacheInvalidateAll();

	return TAI_CONTINUE(int, SceSyscon_ksceSysconResetDevice_ref, type, mode);
}

static int ksceSysconSendCommand_hook_func(int cmd, void *buffer, unsigned int size)
{
	// LOG("ksceSysconSendCommand(0x%08X, %p, 0x%08X)\n", cmd, buffer, size);

	/*
	 * Change the resume context to ours.
	 */
	if (cmd == SCE_SYSCON_CMD_RESET_DEVICE && size == 4)
		buffer = &resume_ctx_paddr;

	return TAI_CONTINUE(int, SceSyscon_ksceSysconSendCommand_ref, cmd, buffer, size);
}

void *pa2va(unsigned int pa){
	unsigned int va = 0, vaddr, paddr, i;

	for (i = 0; i < 0x100000; i++) {
		vaddr = i << 12;

		__asm__ volatile(
			"mcr p15, #0, %1, c7, c8, #0\n"
			"mrc p15, #0, %0, c7, c4, #0\n"
			: "=r"(paddr)
			: "r"(vaddr)
		);

		if((pa & ~0xFFF) == (paddr & ~0xFFF)){
			va = vaddr + (pa & 0xFFF);
			break;
		}
	}
	return (void *)va;
}

/*
 * mapping_paddr_by_vaddr
 *
 * size must be a multiple of 0x100000
 */
int (* mapping_vaddr_by_paddr)(void *ttbr, int memtype, int domain, const void *vaddr, SceSize size, unsigned int paddr) = NULL;

unsigned int *pTTBR0, *pTTBR1;

int map_vaddr(int memtype, int domain, uintptr_t vaddr, SceSize size, uintptr_t paddr)
{
	unsigned int *pTTBR;

	if(vaddr < 0x40000000){
		pTTBR = pTTBR0;
	}else{
		pTTBR = pTTBR1;
	}

	mapping_vaddr_by_paddr(pTTBR, memtype, domain, (const void *)vaddr, size, paddr);
	sceKernelCpuDcacheCleanInvalidateMVAC(&pTTBR[(vaddr >> 20) & 0xFFF]);

	return 0;
}

int map_vaddr_init(void)
{
	tai_module_info_t tai_info;
	tai_info.size = sizeof(tai_module_info_t);

	if(taiGetModuleInfoForKernel(0x10005, "SceSysmem", &tai_info) < 0){
		return -1;
	}

	switch(tai_info.module_nid){
	case 0x3380B323: // 3.60
		module_get_offset(0x10005, tai_info.modid, 0, 0x2364C | 1, (uintptr_t *)&mapping_vaddr_by_paddr);
		break;
	case 0x4DC73B57: // 3.65
		module_get_offset(0x10005, tai_info.modid, 0, 0x23618 | 1, (uintptr_t *)&mapping_vaddr_by_paddr);
		break;
	default:
		ksceDebugPrintf("Not supported SceSysmem version\n");
		return -1;
		break;
	}

	unsigned int ttbr0, ttbr1;

	__asm__ volatile(
		"mrc p15, #0, %0, c2, c0, #0\n"
		"mrc p15, #0, %1, c2, c0, #1\n"
		: "=r"(ttbr0), "=r"(ttbr1)
	);

	ttbr0 &= ~0xFFF;
	ttbr1 &= ~0xFFF;

	pTTBR0 = pa2va(ttbr0);
	pTTBR1 = pa2va(ttbr1);

	ksceDebugPrintf("ttbr0:%p\n", ttbr0);
	ksceDebugPrintf("ttbr1:%p\n", ttbr1);

	return 0;
}

int run_on_thread(const void *func)
{
	int ret = 0, res = 0;
	SceUID uid;

	ret = uid = ksceKernelCreateThread("run_on_thread", func, 64, 0x2000, 0, 0, 0);

	if (ret < 0) {
		ksceDebugPrintf("failed to create a thread: 0x%08x\n", ret);
		ret = -1;
		goto cleanup;
	}
	if ((ret = ksceKernelStartThread(uid, 0, NULL)) < 0) {
		ksceDebugPrintf("failed to start a thread: 0x%08x\n", ret);
		ret = -1;
		goto cleanup;
	}
	if ((ret = ksceKernelWaitThreadEnd(uid, &res, NULL)) < 0) {
		ksceDebugPrintf("failed to wait a thread: 0x%08x\n", ret);
		ret = -1;
		goto cleanup;
	}

	ret = res;

cleanup:
	if (uid > 0)
		ksceKernelDeleteThread(uid);

	return ret;
}

extern unsigned char _binary_payload_bootstrap_ns_bin_start;
extern unsigned char _binary_payload_bootstrap_ns_bin_size;

void dipsw_set(void *pDipsw, SceUInt8 bit)
{
	*(int *)(pDipsw + ((bit >> 5) << 2)) |= 1 << (bit & 0x1F);

	// ksceDebugPrintf("0x%X -> byte:0x%X/bit:0x%X\n", bit, (bit >> 5) << 2, 1 << (bit & 0x1F));
}

typedef struct SceKernelMemBlockInfoExDetails {
    uint32_t type;
    SceUID memblk_uid;
    const char *name;
    void *mappedBase;
    SceSize mappedSize;
    SceSize memblock_some_size_or_alignment;
    int extraLow;
    int extraHigh;
    int unk20;
    SceUID unk24; // ex: 0x10045, maybe some pid
    void *SceUIDPhyMemPartClass_obj;
} SceKernelMemBlockInfoExDetails;

typedef struct SceKernelMemBlockInfoEx { // size is 0xAC on FW 0.990, 0xB8 on FW 3.60
    SceSize size; // Size of this structure
    SceKernelMemBlockInfoExDetails details;
    SceSize unk30; // paddr num
    SceSize unk34; // paddr size num?
    void *paddr_list[0x10];
    SceSize size_list[0x10];
} SceKernelMemBlockInfoEx;

int ksceKernelMemBlockGetInfoEx(SceUID uid, SceKernelMemBlockInfoEx *info);

int update_enso_flag(void *sysroot)
{
	int ret;
	int (* sceSdifReadSectorMmc)() = NULL;

	GetExport("SceSdif", 0x96D306FA, 0x6F8D529B, &sceSdifReadSectorMmc);

	SceUID enso_memid = ksceKernelFindMemBlockByAddr(sceSdifReadSectorMmc, 0);
	if(enso_memid < 0){
		ksceDebugPrintf("sceKernelFindMemBlockByAddr failed : 0x%X\n", enso_memid);
		return enso_memid;
	}

	SceKernelMemBlockInfoEx mem_info;
	memset(&mem_info, 0, sizeof(mem_info));
	mem_info.size = sizeof(mem_info);

	ret = ksceKernelMemBlockGetInfoEx(enso_memid, &mem_info);
	if(ret < 0){
		ksceDebugPrintf("sceKernelMemBlockGetInfoEx failed : 0x%X\n", ret);
		return enso_memid;
	}

	char *pQaf = (void *)(((uintptr_t)sysroot) + 0x20);
	char org_flag = pQaf[0];
	pQaf[0] = 0;

	if(mem_info.details.name[0] == 0){
		// enso
		pQaf[0] |= 1;
	}else if(strcmp(mem_info.details.name, "SceSdif") == 0){
		// non enso
		pQaf[0] |= 0;
	}else{
		// custom enso
		pQaf[0] |= org_flag;
	}

	return 0;
}

int loader_main(void)
{
	int ret;
	SceUID sysroot_buffer_uid;
	void *sysroot_buffer_vaddr;
	struct sysroot_buffer *sysroot;

	ret = map_vaddr_init();
	if(ret < 0){
		ksceDebugPrintf("%s:map_vaddr_init failed\n", __FUNCTION__);
		return ret;
	}

	if(GetExport("SceSysmem", 0x54BF2BAB, 0x76DAB4D0, &sceKernelCpuDcacheWritebackInvalidateAll) < 0)
	if(GetExport("SceSysmem", 0xA5195D20, 0x4BBA5C82, &sceKernelCpuDcacheWritebackInvalidateAll) < 0)
		return -1;

	if(GetExport("SceSysmem", 0x54BF2BAB, 0x264DA250, &sceKernelCpuIcacheInvalidateAll) < 0)
	if(GetExport("SceSysmem", 0xA5195D20, 0x803C84BF, &sceKernelCpuIcacheInvalidateAll) < 0)
		return -1;

	if(GetExport("SceSysmem", 0x54BF2BAB, 0xC8E8C9E9, &sceKernelCpuDcacheCleanInvalidateMVAC) < 0)
	if(GetExport("SceSysmem", 0xA5195D20, 0x597426D2, &sceKernelCpuDcacheCleanInvalidateMVAC) < 0)
		return -1;

	if(GetExport("SceSysmem", 0x54BF2BAB, 0xF4C7F578, &sceKernelCpuIcacheInvalidateRange) < 0)
	if(GetExport("SceSysmem", 0xA5195D20, 0x2E637B1D, &sceKernelCpuIcacheInvalidateRange) < 0)
		return -1;

	ksceDebugPrintf("Function init ok\n");

	// LOG("Baremetal loader by xerpi\n");

	/*
	 * Copy the sysroot buffer to physically contiguous memory.
	 */
	sysroot = ksceKernelGetSysrootBuffer();

	if(ksceSblAimgrIsTool() != 0){
		void *pDipsw = (void *)(((uintptr_t)sysroot) + 0x40);
		dipsw_set(pDipsw, 0xD7); // Allow remote
		*(int *)(pDipsw + 0xC) = 0; // Disable ASLR

		char *pQaf = (void *)(((uintptr_t)sysroot) + 0x20);
		memset(&pQaf[1], 0xFF, 0x10 - 1);
		pQaf[0xB] &= ~0x10; // Disable allow MagicGate
	}

	update_enso_flag(sysroot);

	ret = alloc_phycont(sysroot->size, 4096, &sysroot_buffer_uid, &sysroot_buffer_vaddr);
	if (ret < 0) {
		// LOG("Error allocating memory for the Sysroot buffer 0x%08X\n", ret);
		return SCE_KERNEL_START_FAILED;
	}

	ksceKernelCpuUnrestrictedMemcpy(sysroot_buffer_vaddr, sysroot, sysroot->size);

	ksceKernelGetPaddr(sysroot_buffer_vaddr, &sysroot_buffer_paddr);

	map_vaddr(0x1061D007, 0xC, 0x51000000, 0x1000000, 0x51000000);

	if(1){
		ksceDebugPrintf("0x51000000 access test\n");
		int tmp = *(int *)(0x51000000);
		*(int *)(0x51000000) = tmp;

		memset((void *)0x51000000, 0, 0x1000000);

		ksceDebugPrintf("0x51000000 access test OK\n");
	}

	SceUID fd, memid;
	void *base;

	if(1){
		ksceDebugPrintf("NS KBL buffer allocating\n");

		memid = ksceKernelAllocMemBlock("SceBootKernelImage", 0x1050D006, 0x1000000, NULL);
		ksceKernelGetMemBlockBase(memid, &base);

		memset(base, 0, 0x1000000);

		ksceDebugPrintf("NS KBL buffer allocating OK\n");
	}

	if(1){
		ksceDebugPrintf("NS KBL loading\n");

		fd = ksceIoOpen("host0:data/kbl-loader/nskbl.bin", 1, 0);
		if(fd < 0)
			fd = ksceIoOpen("sd0:data/kbl-loader/nskbl.bin", 1, 0);
		if(fd < 0)
			fd = ksceIoOpen("ux0:data/kbl-loader/nskbl.bin", 1, 0);

		if(fd >= 0){
			ksceIoLseek(fd, 0LL, SCE_SEEK_SET);
			ksceIoRead(fd, base, 0x1000000);
			ksceIoClose(fd);
			ksceDebugPrintf("NS KBL loading OK\n");
		}else{
			ksceDebugPrintf("NS KBL loading Failed\n");
			ksceDebugPrintf("-> Setting opcode to 0xB6 (invalid code)\n");
			memset(base, 0xB6, 0x1000000);
		}

		memcpy((void *)0x51000000, base, 0x1000000);
	}

	if(1){
		ksceDebugPrintf("NS KBL hook install\n");

		memcpy((void *)0x51FF0000, &_binary_payload_bootstrap_ns_bin_start, (int)&_binary_payload_bootstrap_ns_bin_size);

		ksceDebugPrintf("NS KBL hook install OK\n");
	}

	if(1){
		ksceDebugPrintf("Enso loading\n");

		fd = ksceIoOpen("host0:data/kbl-loader/enso_second.bin", 1, 0);
		if(fd < 0)
			fd = ksceIoOpen("sd0:data/kbl-loader/enso_second.bin", 1, 0);
		if(fd < 0)
			fd = ksceIoOpen("ux0:data/kbl-loader/enso_second.bin", 1, 0);

		if(fd >= 0){
			memset(base, 0, 0x8000);
			ksceIoLseek(fd, 0LL, SCE_SEEK_SET);
			ksceIoRead(fd, base, 0x8000);
			ksceIoClose(fd);
			ksceDebugPrintf("Enso loading OK\n");
		}else{
			ksceDebugPrintf("Enso loading Failed\n");
			ksceDebugPrintf("-> Setting opcode to 0xB6(invalid code)\n");
			memset(base, 0xB6, 0x8000);
		}

		memcpy((void *)0x51F00000, base, 0x8000);
	}

	ksceKernelCpuDcacheWritebackRange((void *)0x51000000, 0x1000000);
	sceKernelCpuIcacheInvalidateRange((void *)0x51000000, 0x1000000);

	SceSyscon_ksceSysconResetDevice_hook_uid = taiHookFunctionExportForKernel(0x10005,
		&SceSyscon_ksceSysconResetDevice_ref,
		"SceSyscon", 0x60A35F64, 0x8A95D35C, ksceSysconResetDevice_hook_func);

	SceSyscon_ksceSysconSendCommand_hook_uid = taiHookFunctionExportForKernel(0x10005,
		&SceSyscon_ksceSysconSendCommand_ref,
		"SceSyscon", 0x60A35F64, 0xE26488B9, ksceSysconSendCommand_hook_func);

	ksceKernelGetPaddr(&resume_ctx, &resume_ctx_paddr);

	kscePowerRequestStandby();

	return 0;
}

int module_start(SceSize argc, const void *args)
{
	run_on_thread(loader_main);

	return SCE_KERNEL_START_SUCCESS;
}

unsigned int *get_lvl1_page_table_va(void)
{
	uint32_t ttbcr;
	uint32_t ttbr0;
	uint32_t ttbcr_n;
	uint32_t lvl1_pt_pa;
	void *lvl1_pt_va;

	asm volatile(
		"mrc p15, 0, %0, c2, c0, 2\n\t"
		"mrc p15, 0, %1, c2, c0, 0\n\t"
		: "=r"(ttbcr), "=r"(ttbr0));

	ttbcr_n = ttbcr & 7;
	lvl1_pt_pa = ttbr0 & ~((1 << (14 - ttbcr_n)) - 1);

	if (!find_paddr(lvl1_pt_pa, (void *)0, 0xFFFFFFFF, &lvl1_pt_va))
		return NULL;

	return lvl1_pt_va;
}

int find_paddr(uint32_t paddr, const void *vaddr_start, unsigned int range, void **found_vaddr)
{
	const unsigned int step = 0x1000;
	void *vaddr = (void *)vaddr_start;
	const void *vaddr_end = vaddr_start + range;

	for (; vaddr < vaddr_end; vaddr += step) {
		uintptr_t cur_paddr;

		if (ksceKernelGetPaddr(vaddr, &cur_paddr) < 0)
			continue;

		if ((cur_paddr & ~(step - 1)) == (paddr & ~(step - 1))) {
			if (found_vaddr)
				*found_vaddr = vaddr;
			return 1;
		}
	}

	return 0;
}

int alloc_phycont(unsigned int size, unsigned int alignment, SceUID *uid, void **addr)
{
	int ret;
	SceUID mem_uid;
	void *mem_addr;

	SceKernelAllocMemBlockKernelOpt opt;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
	opt.alignment = ALIGN(alignment, 0x1000);
	mem_uid = ksceKernelAllocMemBlock("phycont", 0x30808006, ALIGN(size, 0x1000), &opt);
	if (mem_uid < 0)
		return mem_uid;

	ret = ksceKernelGetMemBlockBase(mem_uid, &mem_addr);
	if (ret < 0) {
		ksceKernelFreeMemBlock(mem_uid);
		return ret;
	}

	if (uid)
		*uid = mem_uid;
	if (addr)
		*addr = mem_addr;

	return 0;
}
