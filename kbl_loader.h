
#ifndef _VITA_KBL_LOADER_H_
#define _VITA_KBL_LOADER_H_


#include <stdint.h>
#include <psp2kern/kernel/kbl/kbl.h>


typedef struct DLSymVariable {
	struct DLSymVariable *next;
	char *name;
	uint32_t value;
} DLSymVariable;

typedef struct KblLoaderArgs {
    SceKblParam kbl_param;
    SceUInt8 hash[0x20];
    int is_do_patch;
    DLSymVariable *dlsym_root;
} KblLoaderArgs;

#define VITA_KBL_LOADER_DLSYM_BASE (0x52000000)
#define VITA_KBL_LOADER_ARGS_BASE (0x4C000000)


int kbl_dlsym(const char *name, void **result);


typedef struct DLSymList {
	const char *name;
	void **result;
} DLSymList;

#define DLSYM_ADD_LIST(__name__) { .name = #__name__, .result = (void **)&__name__ }



#endif
