
#include "kbl_loader.h"


static int kbl_dlsym_strcmp(const char *s1, const char *s2){

	while((*s1 != 0 && *s2 != 0) && *s1 == *s2){
		s1++;
		s2++;
	}

	return *s1 - *s2;
}

int kbl_dlsym_core(DLSymVariable *tree, const char *name, void **result){

	DLSymVariable *dlsym_tree = tree;

	while(dlsym_tree != NULL){

		if(kbl_dlsym_strcmp(name, dlsym_tree->name) == 0){
			*result = (void *)dlsym_tree->value;
			return 0;
		}

		dlsym_tree = dlsym_tree->next;
	}

	return -1;
}

int kbl_dlsym(const char *name, void **result){

	KblLoaderArgs *loader_argp = (KblLoaderArgs *)VITA_KBL_LOADER_ARGS_BASE;

	return kbl_dlsym_core(loader_argp->dlsym_root, name, result);
}
