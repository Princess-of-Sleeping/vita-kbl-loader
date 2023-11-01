
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/dmac.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/kernel/utils.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/power.h>
#include <psp2kern/sblaimgr.h>
#include <psp2kern/syscon.h>
#include <taihen.h>
#include "../../kbl_loader.h"


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


void *dlsym_base;
SceUID dlsym_heapid = -1;



DLSymVariable *g_dlsym_root;

int dlsym_register(const char *name, uint32_t value){

	if(dlsym_heapid < 0){
		return 0;
	}

	DLSymVariable *dlsym;

	dlsym = ksceKernelAllocHeapMemory(dlsym_heapid, sizeof(*dlsym));
	if(dlsym == NULL){
		return -1;
	}

	int namelen = strlen(name);

	char *new_name = ksceKernelAllocHeapMemory(dlsym_heapid, namelen + 1);
	if(new_name == NULL){
		ksceKernelFreeHeapMemory(dlsym_heapid, dlsym);
		return -1;
	}

	new_name[namelen] = 0;
	memcpy(new_name, name, namelen);

	dlsym->next = g_dlsym_root;
	dlsym->name = new_name;
	dlsym->value = value;

	g_dlsym_root = dlsym;

	// ksceKernelPrintf("dlsym registered: %s (0x%08X)\n", name, value);

	return 0;
}

int dlsym_relocate(void){

	DLSymVariable *dlsym = g_dlsym_root, *next;

	while(dlsym != NULL){
		next = dlsym->next;
		ksceKernelVAtoPA(dlsym->next, (SceUIntPtr *)&(dlsym->next));
		ksceKernelVAtoPA(dlsym->name, (SceUIntPtr *)&(dlsym->name));
		dlsym = next;
	}

	return 0;
}

int parse_line(char *in, char **cont, char **next){

	while(*in != 0 && (*in == ' ' || *in == '\t')){
		in++;
	}

	if(*in == 0){
		return -1;
	}

	char *name_end = strchr(in, ' ');
	if(name_end == NULL){
		name_end = strchr(in, '\t');
	}

	if(name_end == NULL){
		name_end = strchr(in, 0);
	}

	char *new_name = ksceKernelAllocHeapMemory(0x1000B, (name_end - in) + 1);
	if(new_name == NULL){
		return -1;
	}

	new_name[name_end - in] = 0;
	memcpy(new_name, in, name_end - in);

	*cont = new_name;

	if(*name_end == 0){
		*next = NULL;
	}else{
		*next = name_end;
	}

	// ksceKernelPrintf("new_name : \"%s\"\n", new_name);
	// ksceKernelFreeHeapMemory(0x1000B, new_name);

	return 0;
}

int load_config(const char *script, const char *script_end){

	const char *current_script = script;

	while(current_script != script_end){

		const char *next = memchr(current_script, '\n', script_end - current_script);
		if(next == NULL){
			next = script_end - 1;
		}

		int ch = *current_script;
		if(ch != '#' && ch != '\n'){
			int len = next - current_script + 1;
			char *line = ksceKernelAllocHeapMemory(0x1000B, len + 1);
			line[len] = 0;
			memcpy(line, current_script, len);

			char *cmd, *n, *x, *y;

			parse_line(line, &cmd, &n);

			if(strncmp(cmd, "set", 3) == 0 && len > 4){

				parse_line(n, &x, &n);

				if(n != NULL){
					parse_line(n, &y, &n);

					dlsym_register(x, strtol(y, NULL, 16));
					ksceKernelFreeHeapMemory(0x1000B, y);
				}

				ksceKernelFreeHeapMemory(0x1000B, x);

			}else if(strncmp(cmd, "include", 7) == 0 && len > 8){
			}

			ksceKernelFreeHeapMemory(0x1000B, cmd);
			ksceKernelFreeHeapMemory(0x1000B, line);
		}

		current_script = &(next[1]);
	}

	return 0;
}

int load_config_by_path(const char *path){

	int res;
	SceUID fd, memid;
	SceOff offset;
	void *base;

	fd = ksceIoOpen(path, SCE_O_RDONLY, 0);
	if(fd < 0){
		ksceKernelPrintf("sceIoOpen 0x%X (%s)\n", fd, path);
		return fd;
	}

	memid = -1;

	do {
		offset = ksceIoLseek(fd, 0LL, SCE_SEEK_END);
		if(offset < 0){
			ksceKernelPrintf("sceIoLseek 0x%X\n", (int)offset);
			res = (int)offset;
			break;
		}

		res = ksceKernelAllocMemBlock("ConfigBuffer", 0x1020D006, ((int)offset + 0xFFF) & ~0xFFF, NULL);
		if(res < 0){
			ksceKernelPrintf("sceKernelAllocMemBlock 0x%X\n", res);
			break;
		}

		memid = res;

		res = ksceKernelGetMemBlockBase(memid, &base);
		if(res < 0){
			ksceKernelPrintf("sceKernelGetMemBlockBase 0x%X\n", res);
			break;
		}

		ksceIoLseek(fd, 0LL, SCE_SEEK_SET);

		res = ksceIoRead(fd, base, (int)offset);
		if(res < 0){
			ksceKernelPrintf("sceIoRead 0x%X\n", res);
			break;
		}

		if(res != (int)offset){
			ksceKernelPrintf("sceIoRead 0x%X != 0x%X\n", res, (int)offset);
			res = -1;
			break;
		}

		ksceIoClose(fd);
		fd = -1;

		res = load_config(base, base + res);
		if(res < 0){
			ksceKernelPrintf("load_config 0x%X\n", res);
			break;
		}

		res = 0;
	} while(0);

	if(memid >= 0){
		ksceKernelFreeMemBlock(memid);
	}

	if(fd >= 0){
		ksceIoClose(fd);
	}

	return res;
}

int setup_dlsym_ram(void){

	int res;
	SceUID memid;
	SceKernelAllocMemBlockKernelOpt opt;
	SceKernelHeapCreateOpt heap_opt;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = VITA_KBL_LOADER_DLSYM_BASE;

	res = ksceKernelAllocMemBlock("dlsym", 0x6020D006, SCE_KERNEL_1MiB, &opt);
	if(res < 0){
		ksceKernelPrintf("sceKernelAllocMemBlock 0x%X\n", res);
		return res;
	}

	memid = res;

	res = ksceKernelGetMemBlockBase(memid, &dlsym_base);
	if(res < 0){
		ksceKernelPrintf("sceKernelGetMemBlockBase 0x%X\n", res);
		return res;
	}

	memset(&heap_opt, 0, sizeof(heap_opt));
	heap_opt.size = sizeof(heap_opt);
	heap_opt.attr = 0x200;
	heap_opt.field_C = memid;

	res = ksceKernelCreateHeap("dlsym_heap", SCE_KERNEL_1MiB, &heap_opt);
	if(res < 0){
		ksceKernelPrintf("sceKernelCreateHeap 0x%X\n", res);
		return res;
	}

	dlsym_heapid = res;

	return 0;
}

/*
 * 0x1F000000-0x1F007FFF : scratchpad payload
 * 0x4C000000-0x4C003FFF : SceKblParam and payload args
 * 0x51000000-0x51FFFFFF : NSKBL and HEN payload
 */

int loader_main(SceSize arg_len, void *argp){

	SceKblParam *pKblParam;
	SceUID fd, memid;
	void *base;
	int is_do_patch, res;

	if(resume_stack_base == NULL){
		memid = ksceKernelAllocMemBlock("ResumeStackBase", 0x1020D006, 0x4000, NULL);
		if(memid < 0)
			return memid;

		ksceKernelGetMemBlockBase(memid, &resume_stack_base);
	}

	setup_dlsym_ram();

	/*
	 * Copy the sysroot buffer to physically contiguous memory.
	 */
	pKblParam = ksceKernelSysrootGetKblParam();
	if(pKblParam == NULL)
		return -1;

	if(ksceSblAimgrIsTool() != 0){
		// dipsw_set(&(pKblParam->dipsw), 201); // Assert level to 1
		// dipsw_set(&(pKblParam->dipsw), 202); // Assert level to 2
		// dipsw_set(&(pKblParam->dipsw), 204); // Debug level to 1
		// dipsw_set(&(pKblParam->dipsw), 205); // Debug level to 2
		// dipsw_set(&(pKblParam->dipsw), 206); // Allow syscall debug
		// dipsw_set(&(pKblParam->dipsw), 224); // Allow system debug thread
		dipsw_set(&(pKblParam->dipsw), 0xD7); // Allow remote
		pKblParam->dipsw.aslr_seed = 0; // Disable ASLR

		memset(pKblParam->qa_flags, 0xFF, 0x10);
		pKblParam->qa_flags[0xB] &= ~0x10; // Disable allow MagicGate
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
	}

	ksceKernelPrintf("Non-Secure KBL loading\n");

	SceUInt8 bootimage_hash[0x20];

	memset(bootimage_hash, 0xFF, sizeof(bootimage_hash));

	load_config_by_path("host0:data/kbl-loader/config.txt");

	/*
	 * Loading boot image of after resume
	 */
	fd = ksceIoOpen("host0:data/kbl-loader/nskbl.bin", 1, 0);
	if(fd < 0)
		fd = ksceIoOpen("sd0:data/kbl-loader/nskbl.bin", 1, 0);
	if(fd < 0)
		fd = ksceIoOpen("ux0:data/kbl-loader/nskbl.bin", 1, 0);

	is_do_patch = (fd >= 0);

	if(fd < 0)
		fd = ksceIoOpen("host0:data/kbl-loader/bootimage.bin", 1, 0);
	if(fd < 0)
		fd = ksceIoOpen("sd0:data/kbl-loader/bootimage.bin", 1, 0);
	if(fd < 0)
		fd = ksceIoOpen("ux0:data/kbl-loader/bootimage.bin", 1, 0);

	if(fd >= 0){
		ksceIoLseek(fd, 0LL, SCE_SEEK_SET);
		res = ksceIoRead(fd, base, 0x1000000);
		ksceIoClose(fd);

		if(res > 0){
			ksceSha256Digest(base, res, bootimage_hash);
		}

		ksceKernelPrintf("Non-Secure KBL loading OK\n");
	}else{
		ksceKernelPrintf("Non-Secure KBL loading Failed\n");
		ksceKernelPrintf("-> Setting opcode to 0xB6 (invalid code)\n");
		memset(base, 0xB6, 0x1000000);
	}

	ksceKernelPrintf("Boot hook loading\n");

	/*
	 * Copy boot hook image
	 */
	if(boot_data_len <= 0x10000){
		memcpy(base + 0xFF0000, boot_data, boot_data_len);
		ksceKernelPrintf("Boot hook loading OK\n");
	}else{
		memset(base + 0xFF0000, 0xB6, 0x10000);
		ksceKernelPrintf("Boot hook loading Failed\n");
		ksceKernelPrintf("-> boot_data_len 0x%X\n", boot_data_len);
	}

	ksceKernelPrintf("Enso loading\n");

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
		ksceKernelPrintf("Enso loading OK\n");
	}else{
		ksceKernelPrintf("Enso loading Failed\n");
		ksceKernelPrintf("-> Setting opcode to 0xB6 (invalid code)\n");
		memset(base + 0xF00000, 0xB6, 0x8000);
		is_do_patch = 0;
	}

	ksceKernelFreeMemBlock(memid);
	memid = -1;
	base = NULL;

	if(1){
		SceKernelAllocMemBlockKernelOpt opt;
		memset(&opt, 0, sizeof(opt));

		opt.size = sizeof(opt);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
		opt.paddr = VITA_KBL_LOADER_ARGS_BASE;

		memid = ksceKernelAllocMemBlock("ResumeBase", 0x6020D006, 0x4000, &opt);
		ksceKernelGetMemBlockBase(memid, &base);

		KblLoaderArgs *argp = (KblLoaderArgs *)base;

		memcpy(&(argp->kbl_param), pKblParam, sizeof(*pKblParam));

		argp->is_do_patch = is_do_patch;
		memcpy(&(argp->hash), bootimage_hash, sizeof(bootimage_hash));

		if(g_dlsym_root != NULL){
			ksceKernelVAtoPA(g_dlsym_root, (SceUIntPtr *)&(argp->dlsym_root));
			dlsym_relocate();
		}

		ksceKernelFreeMemBlock(memid);
		memid = -1;
		base = NULL;
	}

	SceSyscon_ksceSysconResetDevice_hook_uid = taiHookFunctionExportForKernel(
		0x10005,
		&SceSyscon_ksceSysconResetDevice_ref,
		"SceSyscon",
		0x60A35F64,
		0x8A95D35C,
		ksceSysconResetDevice_hook_func
	);

	SceSyscon_ksceSysconSendCommand_hook_uid = taiHookFunctionExportForKernel(
		0x10005,
		&SceSyscon_ksceSysconSendCommand_ref,
		"SceSyscon",
		0x60A35F64,
		0xE26488B9,
		ksceSysconSendCommand_hook_func
	);

	ksceKernelGetPaddr(&resume_ctx, &resume_ctx_paddr);

	kscePowerRequestStandby();

	return 0;
}

void _start() __attribute__((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args){

	SceUID thid;

	do {
		thid = ksceKernelCreateThread("KblLoadThread", loader_main, 0x78, 0x4000, 0, 0, NULL);
		if(thid < 0){
			SCE_KERNEL_PRINTF_LEVEL(0, "sceKernelCreateThread 0x%X\n", thid);
			break;
		}

		ksceKernelStartThread(thid, 0, NULL);
	} while(0);

	return SCE_KERNEL_START_SUCCESS;
}
