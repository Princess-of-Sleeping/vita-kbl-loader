/* second.c -- bootloader patches
 *
 * Copyright (C) 2023 Princess of Sleeping
 */

#include <inttypes.h>
#include <psp2kern/kernel/iofilemgr.h>
#include "modulemgr_3.10_3.74.h"
#include "../kbl_loader.h"



#define sceKernelGetDACR() ({ \
	SceUIntPtr v; \
	asm volatile ("mrc p15, #0, %0, c3, c0, #0\n": "=r"(v)); \
	v; \
	})

#define sceKernelSetDACR(value) ({ \
	asm volatile ("mcr p15, #0, %0, c3, c0, #0\n":: "r"((value))); \
	})


typedef struct SceNskblModuleInfo2 { // size is 4 on FW 3.60
	const char *filename;
} __attribute__((packed)) SceNskblModuleInfo2;

int (* sceKernelBootLoadModules)(const SceNskblModuleInfo2 *pList, SceUID *pModuleIdList, SceUInt32 count, SceBool use_tool_extended_memory) = 0;

int (* sceKernelPrintf)(const char *fmt, ...) = 0;
int (* sceGUIDReleaseObject)(SceUID guid) = 0;

SceUIDModuleObject *(* sceKernelReferModule)(SceUID moduleId) = 0;

int (* strncmp)(const char *s1, const char *s2, int n) = 0;
void *(* memcpy)(void *dst, const void *src, int len) = 0;

void (* sceKernelDcacheCleanRange)(void *start, int size) = 0;
void (* sceKernelL1IcacheInvalidateEntireAllCore)(void) = 0;

SceUID (* sceIoOpen)(const char *path, int flags, int mode) = 0;
int (* sceIoClose)(SceUID fd) = 0;
SceSSize (* sceIoRead)(SceUID fd, void *data, SceSize size) = 0;
SceOff (* sceIoLseek)(SceUID fd, SceOff offset, int where) = 0;

SceUInt32 display_ps_logo = 0;
SceUInt32 sysstatemgr_is_allow_plaintext = 0;
SceUInt32 sysstatemgr_manu_path = 0;
SceUInt32 sysstatemgr_host0_path = 0;
SceUInt32 sysstatemgr_sd0_path = 0;
SceUInt32 sysstatemgr_ux0_path = 0;
SceUInt32 enso_patch = 0;

// for SceKblForKernel
const SceModuleExport *pExport = NULL;

// SceKernelProcess functions
SceUInt32 (* sceKernelSysrootGetStatus)(void) = 0;

int (* SceSysmem_sceKernelPrintf)(const char *fmt, ...) = 0;
int (* sceSblAuthMgrAuthHeader)(int handle, const void *pSelfHeader, SceSize SelfHeaderSize, void *ctx130) = 0;
int (* sceSblAuthMgrSetupAuthSegment)(int handle, int segment_number) = 0;
int (* sceSblAuthMgrAuthSegment)(int handle, void *buffer, SceSize len) = 0;

static SceBool is_fself_module = SCE_FALSE;

static int (* sceMmcReadSector)(void* ctx, int sector, char* buffer, int nSectors) = NULL;
static void *(* sceSdifGetSdContextPartValidateMmc)(int sd_ctx_index) = NULL;


const DLSymList dlsym_list[] = {
	{
		.name = "SceKblForKernel",
		.result = (void **)&pExport
	},
	DLSYM_ADD_LIST(sceGUIDReleaseObject),
	DLSYM_ADD_LIST(sceKernelReferModule),
	DLSYM_ADD_LIST(strncmp),
	DLSYM_ADD_LIST(memcpy),
	DLSYM_ADD_LIST(sceKernelDcacheCleanRange),
	DLSYM_ADD_LIST(sceKernelL1IcacheInvalidateEntireAllCore),
	DLSYM_ADD_LIST(enso_patch),
	DLSYM_ADD_LIST(display_ps_logo),
	DLSYM_ADD_LIST(sysstatemgr_is_allow_plaintext),
	DLSYM_ADD_LIST(sysstatemgr_manu_path),
	DLSYM_ADD_LIST(sysstatemgr_host0_path),
	DLSYM_ADD_LIST(sysstatemgr_sd0_path),
	DLSYM_ADD_LIST(sysstatemgr_ux0_path)
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

static int sceSblAuthMgrAuthHeader_patch(int handle, const void *pSelfHeader, SceSize SelfHeaderSize, void *ctx130){

	int res;
	SceUInt32 dacr;

	res = sceSblAuthMgrAuthHeader(handle, pSelfHeader, SelfHeaderSize, ctx130);

	// SceSysmem_sceKernelPrintf("sceSblAuthMgrAuthHeader 0x%X\n", res);

	dacr = sceKernelGetDACR();
	sceKernelSetDACR(dacr | 0xFFFF0000);

	if(res == 0x800F0B35 && sceKernelSysrootGetStatus() < 0x1000){
		is_fself_module = SCE_TRUE;
		res = 0;
	}else{
		is_fself_module = SCE_FALSE;
	}

	sceKernelSetDACR(dacr);

	return res;
}

static int sceSblAuthMgrSetupAuthSegment_patch(int handle, int segment_number){

	int res;

	if(is_fself_module == SCE_TRUE){
		res = 2;
	}else{
		res = sceSblAuthMgrSetupAuthSegment(handle, segment_number);
	}

	// SceSysmem_sceKernelPrintf("sceSblAuthMgrSetupAuthSegment 0x%X\n", res);

	return res;
}

static int sceSblAuthMgrAuthSegment_patch(int handle, void *buffer, SceSize len){

	int res;

	if(is_fself_module == SCE_TRUE){
		res = 0;
	}else{
		res = sceSblAuthMgrAuthSegment(handle, buffer, len);
	}

	// SceSysmem_sceKernelPrintf("sceSblAuthMgrAuthSegment 0x%X\n", res);

	return res;
}

#define unlikely(expr) __builtin_expect(!!(expr), 0)

static int sceMmcReadSector_patch(void* ctx, int sector, char* buffer, int nSectors){

	int ret;

	if(unlikely(sector == 0 && nSectors > 0)){
		// printf("read sector async 0 for %d at context 0x%08X\n", nSectors, ctx);
		if(sceSdifGetSdContextPartValidateMmc(0) == ctx){
			// printf("patching sector 0 read to sector 1\n");
			ret = sceMmcReadSector(ctx, 1, buffer, 1);
			if(ret >= 0 && nSectors > 1){
				ret = sceMmcReadSector(ctx, 1, buffer + 0x200, nSectors-1);
			}
			return ret;
		}
	}

	return sceMmcReadSector(ctx, sector, buffer, nSectors);
}

int module_set_export_ptr(SceModuleCB *Module, SceNID libnid, SceNID funcnid, ScePVoid **function){

	SceModuleExport *Export;

	for(int i=0;i<Module->ExportCounter;i++){
		Export = Module->LibEntVector[i].Export;
		if(Export->libnid == libnid){
			for(int k=0;k<Export->entry_num_function;k++){
				if(Export->table_nid[k] == funcnid){
					*function = &(Export->table_entry[k]);
					return 0;
				}
			}
		}
	}

	return -1;
}

int module_get_export(SceModuleCB *Module, SceNID libnid, SceNID funcnid, ScePVoid *function){

	int res;
	ScePVoid *function_ptr;

	res = module_set_export_ptr(Module, libnid, funcnid, &function_ptr);
	if(res < 0){
		return -1;
	}

	*function = *function_ptr;

	return 0;
}

int module_set_export(SceModuleCB *Module, SceNID libnid, SceNID funcnid, ScePVoid function){

	SceModuleExport *Export;

	for(int i=0;i<Module->ExportCounter;i++){
		Export = Module->LibEntVector[i].Export;
		if(Export->libnid == libnid){
			for(int k=0;k<Export->entry_num_function;k++){
				if(Export->table_nid[k] == funcnid){
					Export->table_entry[k] = function;
					// sceKernelDcacheCleanRange((ScePVoid)((SceUIntPtr)(&(Export->table_entry[k])) & ~0x1F), 0x20);
					return 0;
				}
			}
		}
	}

	return -1;
}

int module_set_import(SceModuleCB *Module, SceNID libnid, SceNID funcnid, ScePVoid function){

	SceModuleImport *Import;
	SceUIntPtr *code;
	SceUIntPtr f;

	for(int i=0;i<Module->ImportCounter;i++){
		Import = Module->ClientVector[i].Import;
		if(Import->libnid == libnid){
			for(int k=0;k<Import->entry_num_function;k++){
				if(Import->table_func_nid[k] == funcnid){
					f = (SceUIntPtr)(function);
					code = Import->table_function[k];

					code[0] = 0xE300C000 | (f & 0xFFF) | ((f << 4) & 0xF0000);
					f = f >> 16;
					code[1] = 0xE340C000 | (f & 0xFFF) | ((f << 4) & 0xF0000);

					return 0;
				}
			}
		}
	}

	return -1;
}

int module_set_import_nid(SceModuleCB *Module, SceNID libnid, SceNID funcnid, SceNID to){

	SceModuleImport *Import;

	for(int i=0;i<Module->ImportCounter;i++){
		Import = Module->ClientVector[i].Import;
		if(Import->libnid == libnid){
			for(int k=0;k<Import->entry_num_function;k++){
				if(Import->table_func_nid[k] == funcnid){
					Import->table_func_nid[k] = to;
					return 0;
				}
			}
		}
	}

	return -1;
}

static int sceKernelBootLoadModules_patch(const SceNskblModuleInfo2 *pList, SceUID *pModuleIdList, SceUInt32 count, SceBool use_tool_extended_memory){

	int res;
	SceUInt32 dacr;

	res = sceKernelBootLoadModules(pList, pModuleIdList, count, use_tool_extended_memory);
	if(res >= 0){
		for(int i=0;i<count;i++){
			if(pList[i].filename == NULL){
				continue;
			}

			// sceKernelPrintf("0x%08X: %s\n", pModuleIdList[i], pList[i].filename);

			SceUID moduleId = pModuleIdList[i];
			if(moduleId < 0){
				continue;
			}

			if(strncmp(pList[i].filename, "display.skprx", 13) == 0){

				SceUIDModuleObject *pUIDModuleObject = sceKernelReferModule(moduleId);
				if(pUIDModuleObject != NULL){

					void *text = pUIDModuleObject->moduleCB.segment[0].vaddr;

					dacr = sceKernelGetDACR();
					sceKernelSetDACR(dacr | 0xFFFF0000);
					if(display_ps_logo != 0){
						*(SceUInt8 *)(text + display_ps_logo) = 0xC7;
					}
					sceKernelSetDACR(dacr);

					sceGUIDReleaseObject(moduleId);
				}

			}else if(strncmp(pList[i].filename, "iofilemgr.skprx", 15) == 0){

				SceUIDModuleObject *pUIDModuleObject = sceKernelReferModule(moduleId);
				if(pUIDModuleObject != NULL){

					dacr = sceKernelGetDACR();
					sceKernelSetDACR(dacr | 0xFFFF0000);

					// Allow to sd0: unit
					module_set_import_nid(&(pUIDModuleObject->moduleCB), 0x3691DA45, 0x55392965, 0xEEB867C0); // sceSysrootIsManufacturingMode to return !SCE_FALSE

					sceKernelSetDACR(dacr);

					sceGUIDReleaseObject(moduleId);
				}

			}else if(strncmp(pList[i].filename, "sdstor.skprx", 12) == 0){

				SceUIDModuleObject *pUIDModuleObject = sceKernelReferModule(moduleId);
				if(pUIDModuleObject != NULL){

					dacr = sceKernelGetDACR();
					sceKernelSetDACR(dacr | 0xFFFF0000);

					// Allow to sd0: mount
					module_set_import_nid(&(pUIDModuleObject->moduleCB), 0x2ED7F97A, 0x55392965, 0x67aab627); // sceSysrootUseExternalStorage to return !SCE_FALSE

					sceKernelSetDACR(dacr);

					sceGUIDReleaseObject(moduleId);
				}

			}else if(strncmp(pList[i].filename, "exfatfs.skprx", 13) == 0){

				SceUIDModuleObject *pUIDModuleObject = sceKernelReferModule(moduleId);
				if(pUIDModuleObject != NULL){

					dacr = sceKernelGetDACR();
					sceKernelSetDACR(dacr | 0xFFFF0000);

					// Allow to sd0: mount
					module_set_import_nid(&(pUIDModuleObject->moduleCB), 0x2ED7F97A, 0x55392965, 0x67aab627); // sceSysrootUseExternalStorage to return !SCE_FALSE
					module_set_import_nid(&(pUIDModuleObject->moduleCB), 0x2ED7F97A, 0x7B7F8171, 0x67aab627); // sceKernelIsAllowSdCardFromMgmt to return !SCE_FALSE

					sceKernelSetDACR(dacr);

					sceGUIDReleaseObject(moduleId);
				}

			}else if(strncmp(pList[i].filename, "sdif.skprx", 10) == 0 && *(uint16_t *)(enso_patch) == 0x2101){

				SceUIDModuleObject *pUIDModuleObject = sceKernelReferModule(moduleId);
				if(pUIDModuleObject != NULL){

					dacr = sceKernelGetDACR();
					sceKernelSetDACR(dacr | 0xFFFF0000);

					module_get_export(&(pUIDModuleObject->moduleCB), 0x96D306FA, 0x6F8D529B, (ScePVoid *)&sceMmcReadSector);
					module_get_export(&(pUIDModuleObject->moduleCB), 0x96D306FA, 0x6A71987F, (ScePVoid *)&sceSdifGetSdContextPartValidateMmc);
					module_set_export(&(pUIDModuleObject->moduleCB), 0x96D306FA, 0x6F8D529B, sceMmcReadSector_patch);

					sceKernelSetDACR(dacr);

					sceGUIDReleaseObject(moduleId);
				}

			}else if(strncmp(pList[i].filename, "sysstatemgr.skprx", 17) == 0){

				SceUIDModuleObject *pUIDModuleObject = sceKernelReferModule(moduleId);
				if(pUIDModuleObject != NULL){

					void *text = pUIDModuleObject->moduleCB.segment[0].vaddr;

					dacr = sceKernelGetDACR();
					sceKernelSetDACR(dacr | 0xFFFF0000);

					// Allow to loading boot config from sd0:/ux0:
					// module_set_import_nid(&(pUIDModuleObject->moduleCB), 0x3691DA45, 0x55392965, 0xEEB867C0); // sceSysrootIsManufacturingMode to return !SCE_FALSE

					// maybe not need this? because we have qaf spoof already.
					*(SceUInt32 *)(text + sysstatemgr_is_allow_plaintext) = 0x2001BF00; // movs r0, #1
					*(SceUInt32 *)(text + sysstatemgr_manu_path) = 0x2001BF00; // movs r0, #1

					if(sysstatemgr_host0_path != 0){
						*(const char **)(text + sysstatemgr_host0_path) = "host0:psp2config_development.txt";
						sceKernelDcacheCleanRange((void *)((SceUIntPtr)(text + sysstatemgr_host0_path) & ~0x1F), (((SceUIntPtr)(text + sysstatemgr_host0_path) & 0x1F) + 4 + 0x1F) & ~0x1F);
						sceKernelL1IcacheInvalidateEntireAllCore();
					}

					*(const char **)(text + sysstatemgr_sd0_path) = "sd0:/psp2config.txt";
					*(const char **)(text + sysstatemgr_ux0_path) = "ur0:/tai/bootconfig.txt";

					sceKernelSetDACR(dacr);

					sceGUIDReleaseObject(moduleId);
				}

			}else if(strncmp(pList[i].filename, "authmgr.skprx", 13) == 0){

				SceUIDModuleObject *pUIDModuleObject = sceKernelReferModule(moduleId);
				if(pUIDModuleObject != NULL){

					dacr = sceKernelGetDACR();
					sceKernelSetDACR(dacr | 0xFFFF0000);

					module_get_export(&(pUIDModuleObject->moduleCB), 0x7ABF5135, 0xF3411881, (ScePVoid *)&sceSblAuthMgrAuthHeader);
					module_get_export(&(pUIDModuleObject->moduleCB), 0x7ABF5135, 0x89CCDA2C, (ScePVoid *)&sceSblAuthMgrSetupAuthSegment);
					module_get_export(&(pUIDModuleObject->moduleCB), 0x7ABF5135, 0xBC422443, (ScePVoid *)&sceSblAuthMgrAuthSegment);

					module_set_export(&(pUIDModuleObject->moduleCB), 0x7ABF5135, 0xF3411881, (ScePVoid *)&sceSblAuthMgrAuthHeader_patch);
					module_set_export(&(pUIDModuleObject->moduleCB), 0x7ABF5135, 0x89CCDA2C, (ScePVoid *)&sceSblAuthMgrSetupAuthSegment_patch);
					module_set_export(&(pUIDModuleObject->moduleCB), 0x7ABF5135, 0xBC422443, (ScePVoid *)&sceSblAuthMgrAuthSegment_patch);

					sceKernelSetDACR(dacr);

					sceGUIDReleaseObject(moduleId);
				}

			}else if(strncmp(pList[i].filename, "sysmem.skprx", 12) == 0){

				SceUIDModuleObject *pUIDModuleObject = sceKernelReferModule(moduleId);
				if(pUIDModuleObject != NULL){


					dacr = sceKernelGetDACR();
					sceKernelSetDACR(dacr | 0xFFFF0000);

					module_get_export(&(pUIDModuleObject->moduleCB), 0x3691DA45, 0x5C426B19, (ScePVoid *)&sceKernelSysrootGetStatus);
					module_get_export(&(pUIDModuleObject->moduleCB), 0x88758561, 0x391B74B7, (ScePVoid *)&SceSysmem_sceKernelPrintf);

					sceKernelSetDACR(dacr);

					sceGUIDReleaseObject(moduleId);
				}
			}
		}
	}

	return res;
}

void module_start(SceUInt32 bl_hash){

	int res;

	SceUInt32 dacr;

	dacr = sceKernelGetDACR();
	sceKernelSetDACR(dacr | 0xFFFF0000);

	res = kbl_dlsym("sceKernelPrintf", (void **)&sceKernelPrintf);
	sceKernelSetDACR(dacr);

	if(res < 0){
		return;
	}

	dacr = sceKernelGetDACR();
	sceKernelSetDACR(dacr | 0xFFFF0000);
	dlsym_resolve();
	sceKernelSetDACR(dacr);

	if(pExport != NULL){

		SceNID *pNIDTable = (SceNID *)pExport->table_nid;
		ScePVoid *pEntTable = (ScePVoid *)pExport->table_entry;

		for(int i=0;i<pExport->entry_num_function;i++){
			if(pNIDTable[i] == 0x6D7A1F18){
				dacr = sceKernelGetDACR();
				sceKernelSetDACR(dacr | 0xFFFF0000);
				sceKernelBootLoadModules = pEntTable[i];
				pEntTable[i] = sceKernelBootLoadModules_patch;
				sceKernelSetDACR(dacr);
				break;
			}
		}

		sceKernelPrintf("The enso has been started.\n");
	}
}

__attribute__ ((section (".text.start"))) void start(SceUInt32 bl_hash){
	module_start(bl_hash);
}
