
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/dmac.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/power.h>
#include <psp2kern/sblaimgr.h>
#include <psp2kern/syscon.h>
#include <taihen.h>

typedef struct SceSysconResumeContext {
	SceSize size;
	SceUInt32 unk;
	SceUIntVAddr buff_vaddr;
	SceUIntVAddr resume_func_vaddr;
	SceUInt32 SCTLR;
	SceUInt32 ACTLR;
	SceUInt32 CPACR;
	SceUInt32 TTBR0;
	SceUInt32 TTBR1;
	SceUInt32 TTBCR;
	SceUInt32 DACR;
	SceUInt32 PRRR;
	SceUInt32 NMRR;
	SceUInt32 VBAR;
	SceUInt32 CONTEXTIDR;
	SceUInt32 TPIDRURW;
	SceUInt32 TPIDRURO;
	SceUInt32 TPIDRPRW;
	SceUInt32 unk2[6];
	SceUInt64 time;
} SceSysconResumeContext;

extern const uint8_t boot_data[];
extern const unsigned int boot_data_len;

extern void resume_function(void);

static tai_hook_ref_t SceSyscon_ksceSysconResetDevice_ref;
static SceUID SceSyscon_ksceSysconResetDevice_hook_uid = -1;
static tai_hook_ref_t SceSyscon_ksceSysconSendCommand_ref;
static SceUID SceSyscon_ksceSysconSendCommand_hook_uid = -1;

static SceSysconResumeContext resume_ctx;
static uintptr_t resume_ctx_paddr;
static unsigned int resume_ctx_buff[32];

void *resume_stack_base;
void *sys_ttbr0_vbase;

void *ksceKernelSysrootGetModulePrivate(SceUInt32 index);

typedef struct SceKernelUIDHeapClass { // size is 0x34-bytes
	SceClass sceUIDHeapClass;
	int data_0x2C;
	int data_0x30;
} SceKernelUIDHeapClass;

typedef struct SceKernelUIDFixedHeapClass { // size is 0x3C-bytes
	SceClass sceUIDFixedHeapClass;
	int data_0x2C;
	int data_0x30;
	int data_0x34;
	int data_0x38;
} SceKernelUIDFixedHeapClass;

// ModulePrivate3
typedef struct SceKernelSystemMemory { // size is 0xDC-bytes
	SceSize size;
	int data_0x04;
	void *data_0x08; // size is 0xB0-bytes. Related to SceUIDEntryHeapClass
	void *data_0x0C; // size is 0x8-bytes.

	void *data_0x10; // size is 0xAC-bytes. ScePhyMemPartKD
	void *data_0x14; // size is 0xAC-bytes. ScePhyMemPartTool
	int data_0x18;
	int data_0x1C; // copied from KernelBootArgs

	int data_0x20; // copied from KernelBootArgs
	int data_0x24; // copied from KernelBootArgs
	int data_0x28;
	int data_0x2C;

	int data_0x30;
	int data_0x34; // copied from KernelBootArgs
	void *data_0x38; // size is 0xAC-bytes. Related to SceKernelNameHeapHash
	int data_0x3C;

	void *data_0x40; // size is 0x80-bytes. Related to partition
	void *pKernelHeapObject;
	void *data_0x48; // size is 0xC4-bytes. like malloc context
	SceSize number_of_memory_base_list;

	struct {
		uintptr_t base;
		SceSize length;
	} address_base[4];

	// some TTBR stuff
	int data_0x70; // ex:0
	SceUInt32 *ttbr0_vbase; // copied from KernelBootArgs
	SceUInt32 *ttbr1_vbase; // copied from KernelBootArgs
	SceSize ttbr0_mgmt_size; // copied from KernelBootArgs

	SceSize ttbr1_mgmt_size; // copied from KernelBootArgs
	// some TTBR stuff

	int data_0x84;
	int data_0x88;
	int data_0x8C;

	int data_0x90;
	int data_0x94;
	SceClass *pUIDClass;
	SceClass *pUIDDLinkClass;

	// offset:0xA0
	SceKernelUIDHeapClass      *pKernelUIDHeapClass;
	SceKernelUIDFixedHeapClass *pKernelUIDFixedHeapClass;
	void *data_0xA8; // size is 0x3C-bytes
	int data_0xAC;

	SceClass *pUIDMemBlockClass;
	SceClass *pUIDTinyPartitionClass;
	SceClass *pUIDPartitionClass;
	void *data_0xBC; // size is 0x34-bytes. Related to SceUIDKernelHeapClass

	void *data_0xC0; // size is 0xC-bytes
	int data_0xC4; // size is 0x18-bytes. Related to partition
	int data_0xC8; // size is 0x28-bytes. Related to partition
	SceUID kernelHeapUncachedId;

	void *data_0xD0; // SceAS object pointer
	void *data_0xD4; // size is 0xC-bytes. Related to SceUIDAddressSpaceClass
	int data_0xD8;
} SceKernelSystemMemory;

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

	// ksceKernelCpuDcacheAndL2WritebackRange(&resume_ctx, sizeof(resume_ctx));

	SceKernelSystemMemory *pSystemMemory = ksceKernelSysrootGetModulePrivate(3);
	if(pSystemMemory != NULL){
		sys_ttbr0_vbase = pSystemMemory->ttbr0_vbase;
	}
}

static int ksceSysconResetDevice_hook_func(int type, int mode)
{
	/*
	 * The Vita OS thinks it's about to poweroff, but we will instead
	 * setup the payload and trigger a soft reset.
	 */
	if (type == SCE_SYSCON_RESET_TYPE_POWEROFF) {
		setup_payload();
		type = SCE_SYSCON_RESET_TYPE_SOFT_RESET;
	}

	return TAI_CONTINUE(int, SceSyscon_ksceSysconResetDevice_ref, type, mode);
}

static int ksceSysconSendCommand_hook_func(int cmd, void *buffer, unsigned int size)
{
	/*
	 * Change the resume context to ours.
	 */
	if (cmd == SCE_SYSCON_CMD_RESET_DEVICE && size == 4)
		buffer = &resume_ctx_paddr;

	return TAI_CONTINUE(int, SceSyscon_ksceSysconSendCommand_ref, cmd, buffer, size);
}

void dipsw_set(void *pDipsw, SceUInt8 bit){
	*(SceUInt32 *)(pDipsw + ((bit >> 5) << 2)) |= 1 << (bit & 0x1F);
}

int loader_main(void){

	SceKblParam *pKblParam;
	uintptr_t image_paddr = 0;
	SceUID fd, memid;
	void *base;

	if(resume_stack_base == NULL){
		memid = ksceKernelAllocMemBlock("ResumeStackBase", 0x1020D006, 0x4000, NULL);
		if(memid < 0)
			return memid;

		ksceKernelGetMemBlockBase(memid, &resume_stack_base);
	}

	/*
	 * Copy the sysroot buffer to physically contiguous memory.
	 */
	pKblParam = ksceKernelSysrootGetKblParam();
	if(pKblParam == NULL)
		return -1;

	if(ksceSblAimgrIsTool() != 0){
		dipsw_set(&(pKblParam->dipsw), 0xD7); // Allow remote
		pKblParam->dipsw.aslr_seed = 0; // Disable ASLR

		memset(pKblParam->qa_flags, 0xFF, 0x10);
		pKblParam->qa_flags[0xB] &= ~0x10; // Disable allow MagicGate
	}

	// PS TV DEX Testing
	if(0){
		pKblParam->pscode.company_code     = __builtin_bswap16(0x0000);
		pKblParam->pscode.product_code     = __builtin_bswap16(0x102); // DEX
		pKblParam->pscode.product_sub_code = __builtin_bswap16(0x602); // PS TV Prototype
		// pKblParam->pscode.factory_code     = __builtin_bswap16(0x3);
	}

	if(1){
		SceKernelAllocMemBlockKernelOpt opt;
		memset(&opt, 0, sizeof(opt));

		opt.size = sizeof(opt);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
		opt.paddr = 0x51000000;

		memid = ksceKernelAllocMemBlock("KernelBootLoaderBase", 0x6020D006, 0x1000000, &opt);
		ksceKernelGetMemBlockBase(memid, &base);

		ksceDmacMemset(base, 0, 0x1000000);

		ksceKernelGetPaddr(base, &image_paddr);
		ksceDebugPrintf("image_paddr: %p\n", image_paddr);
	}

	ksceDebugPrintf("NS KBL loading\n");

	/*
	 * Loading boot image of after resume
	 */
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

	/*
	 * Copy boot hook image
	 */
	if(boot_data_len <= 0x10000){
		memcpy(base + 0xFF0000, boot_data, boot_data_len);
	}else{
		memset(base + 0xFF0000, 0xB6, 0x10000);
	}

	ksceDebugPrintf("Enso loading\n");

	/*
	 * Loading Enso
	 */
	fd = ksceIoOpen("host0:data/kbl-loader/enso_second.bin", 1, 0);
	if(fd < 0)
		fd = ksceIoOpen("sd0:data/kbl-loader/enso_second.bin", 1, 0);
	if(fd < 0)
		fd = ksceIoOpen("ux0:data/kbl-loader/enso_second.bin", 1, 0);

	if(fd >= 0){
		memset(base + 0xF00000, 0, 0x8000);
		ksceIoLseek(fd, 0LL, SCE_SEEK_SET);
		ksceIoRead(fd, base + 0xF00000, 0x8000);
		ksceIoClose(fd);
		ksceDebugPrintf("Enso loading OK\n");
	}else{
		ksceDebugPrintf("Enso loading Failed\n");
		ksceDebugPrintf("-> Setting opcode to 0xB6 (invalid code)\n");
		memset(base + 0xF00000, 0xB6, 0x8000);
	}

	ksceKernelFreeMemBlock(memid);
	memid = -1;
	base = NULL;

	if(1){
		SceKernelAllocMemBlockKernelOpt opt;
		memset(&opt, 0, sizeof(opt));

		opt.size = sizeof(opt);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
		opt.paddr = 0x48000000;

		memid = ksceKernelAllocMemBlock("ResumeBase", 0x6020D006, 0x200000, &opt);
		ksceKernelGetMemBlockBase(memid, &base);

		memcpy(base, pKblParam, sizeof(*pKblParam));

		ksceKernelFreeMemBlock(memid);
		memid = -1;
		base = NULL;
	}

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

void _start() __attribute__((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args){

	loader_main();

	return SCE_KERNEL_START_SUCCESS;
}
