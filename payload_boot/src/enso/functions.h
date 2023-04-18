/*
 * functions.h
 * Copyright (C) 2020 Princess of Sleeping
 */
#ifndef _NSKBL_FUNCTIONS_H_
#define _NSKBL_FUNCTIONS_H_

#include <psp2kern/types.h>
#include <psp2kern/kernel/kbl/kbl.h>


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


typedef struct KBPMemoryRegion {
    SceUIntPtr pbase;
    SceSize psize;
} KBPMemoryRegion;

typedef struct KBPMappingInfo {
    char *name;         //<! Name of the mapping
    SceUIntPtr pbase;     //<! Base physical address of the mapping
    SceUIntPtr vbase;     //<! Base virtual address of the mapping
    SceSize psize;     //<! Size of the physical range of the mapping??
    SceSize vsize;     //<! Size of the virtual range of the mapping?? - Usually equal to psize, but may be bigger.
    SceUInt32 extraHigh; //<! Sometimes used as extraHigh for some memblock allocations? Exact meaning unknown.
    // psize + extraHigh must always be equal to vsize
} KBPMappingInfo;

typedef struct KBPBootStackInfo {
    unsigned unk0[2];
    SceUIntPtr ttbcr;
    SceUInt32 dacr;
    SceUInt32 asid;
    SceSize size;
    SceUID blkId;      //<! UID of the stack's memblock
    void *stackBottom; //<! = memblock.vbase + size 
} KBPBootStackInfo;

// This structure is passed as argp of modules like sysmem.skprx, etc
// 0x51030100 on NSKBL
typedef struct SceKernelBootParam { // Size is 0x310-bytes in 3.60-3.65 and 4.00. Layout appears to be identical.
	SceSize size;                   //<! Size of this structure
	SceBool secure;                 //<! Is secure state? Always 0 in NSKBL
	SceUInt32 num_memory;            //<! Number of filled elements in the next field
	KBPMemoryRegion memory[4];       //<! Physical memory ranges usable by NSKBL and kernel
	SceKblParam *pKblParam;          //<! Pointer to KBL Param
	unsigned int unk30[5];
	SceUInt32 SoCInfo;               //<! = *(u32*)PERVASIVE_MISC
	SceUInt32 pmisc_unk4;            //<! = *(u32*)(PERVASIVE_MISC + 4)
	SceUInt32 KermitRevision;        //<! = SoCInfo & 0x1FFFF
	SceUInt32 hwDependent[2];        //<! Depends on pKBLParam->hardwareInfo
	KBPMappingInfo ttbr0;            //<! "SceKernelTTBR0", ASLR vbase, psize 0x4000
	unsigned int ttbr0_maxAddress;   //<! Maximal address covered by TTBR0
	unsigned int sizeTTBR0Address;   //<! Size of the address space covered by TTBR0
	KBPMappingInfo ttbr1;            //<! "SceKernelTTBR1", ASLR vbase, psize 0x4000
	unsigned int sizeTTBR1Address;   //<! Size of the address space covered by TTBR1
	void *L2PT000_mapbase;           //<! First vaddr mapped by "SceKernelL2PageTable000"
	KBPMappingInfo reset;            //<! "SceKernelReset", vbase 0, psize 0x1000, extraHigh 0x3000
	KBPMappingInfo excpEntry;        //<! "SceKernelExceptionEntry", ASLR vbase, psize 0x1000, extraHigh 0x1000
	KBPMappingInfo l2pt000;          //<! "SceKernelL2PageTable000", ASLR vbase, psize 0x1000, extraHigh 0x1000
	KBPMappingInfo l2vector;         //<! "SceKernelL2Vector", ASLR vbase, psize 0x4000, extraHigh 0x4000
	KBPMappingInfo sysroot;          //<! "SceSysroot", ASLR vbase, psize 0x4000
	KBPMappingInfo fh32b;            //<! "SceKernelFixedHeap32B", ASLR vbase, psize 0x10000
	KBPMappingInfo fh48b;            //<! "SceKernelFixedHeap48B", ASLR vbase, psize 0x10000
	KBPMappingInfo fh64b;            //<! "SceKernelFixedHeap64B", ASLR vbase, psize 0x10000
	KBPMappingInfo fhUIDEntry;       //<! "SceKernelFixedHeapUIDEntry", ASLR vbase, psize 0x10000
	KBPMappingInfo fhL2Object;       //<! "SceKernelFixedHeapForL2Object", ASLR vbase, psize 0x1000, extraHigh 0x1000
	KBPMappingInfo unk188;           //<! Unused? NOTE: ASLR vbase = randomize in 255 slots, MegaASLR vbase = randomize in 15 slots
	KBPMappingInfo phypage;          //<! "SceKernelPhyPageTable", MegaASLR vbase, psize 0x80000
	KBPMappingInfo phypageHigh;      //<! "SceKernelPhyPageTableHigh", MegaASLR vbase, psize 0x80000, pbase 0x77F00000
	KBPMappingInfo bootkernimg;      //<! "SceBootKernelImage", vbase=pbase=0x51000000, psize 0x1000000, extraHigh 0x1000000
	KBPMappingInfo hwreg;            //<! Unnamed. vbase=pbase=0xE0000000, psize 0x8000000
	void *pCorelock;
	KBPBootStackInfo bootCpu[4];                  //<! bootCpu[X] corresponds to CPUX's info. Stack is pivoted to this when calling main2.
	void *pSysroot;                         //<! Pointer to Sysroot structure
	unsigned int unk288;                          //<! Related to L2PageTable000? Always 0.
	void *pL2PageTable000;                        //<! Base address of the L2PageTable000
	void *resetVector;                            //<! Goes into VBAR. = excpEntry.vbase + 0x100
	void *phyMemPartKD;            //<! "SceKernelPhyMemPartKD"
	void *phyMemPartTool;          //<! "SceKernelPhyMemPartTool". May be NULL
	void *pPageKernelReset;                    //<! PhyPage object describing the pages backing SceKernelReset
	void *pPageL2PageTable000;                 //<! PhyPage object describing the pages backing SceKernelL2PageTable000
	void *pPageSysroot;                        //<! PhyPage object describing the pages backing SceSysroot
	void *pPageTTBR0;                          //<! PhyPage object describing the pages backing SceKernelTTBR0
	void *pPageTTBR1;                          //<! PhyPage object describing the pages backing SceKernelTTBR1
	void *pPageL2Vector;                       //<! PhyPage object describing the pages backing SceKernelL2Vector
	void *pPagePhypage;                        //<! PhyPage object describing the pages backing SceKernelPhyPageTable
	void *pPagePhypageHigh;                    //<! PhyPage object describing the pages backing SceKernelPhyPageTableHigh
	void *pPageBootKernelImage;                //<! PhyPage object describing the pages backing SceBootKernelImage????
	void *pPageFixedHeap32B;                   //<! PhyPage object describing the pages backing SceKernelFixedHeap32B
	void *pPageFixedHeap48B;                   //<! PhyPage object describing the pages backing SceKernelFixedHeap48B
	void *pPageFixedHeap64B;                   //<! PhyPage object describing the pages backing SceKernelFixedHeap64B
	void *pPageFixedHeapForL2Object;           //<! PhyPage object describing the pages backing SceKernelFixedHeapForL2Object
	void *pFixedHeap32B;
	void *pFixedHeap48B;
	void *pFixedHeap64B;
	void *pFixedHeapForL2Object;
	void *pPageUIDHeap;                        //<! PhyPage object describing the pages backing SceKernelFixedUIDHeap??
	void *pUIDHeap;              //<! Pointer to SceKernelFixedUIDHeap object 
	void *pL2PT000Object;            //<! L2PageTableObject for SceKernelL2PageTable000
	void *pPhyPageTblL2PTO;          //<! L2PageTableObject for table used to map the PhyPageTable (located @ SceKernelL2PageTable000.pbase + 0x400)
	void *pPartitionKernel;      //<! Object of "SceKernelRoot" partition
	SceUID uidPartitionKernel;                    //<! UID of object above (0x10009)
	SceUInt32 unk2F8[2];
	void *data_0x300;
	void *pPutcharHandler;
	SceUInt32 minimum_log_level;
	SceUInt32 magic;                              //<! 0x7F407C30
} SceKernelBootParam;


#endif
