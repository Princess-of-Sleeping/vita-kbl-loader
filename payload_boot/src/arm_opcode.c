
#include <stdint.h>
#include <stddef.h>
#include "arm_opcode.h"


int get_movw_opcode(void *dst, unsigned int target_reg, uint16_t val){

	if(dst == NULL)
		return -1;

	if(target_reg > 0xF)
		return -2;

	((uint8_t *)dst)[0] = ((val >> 0xC) & 0xF) | 0x40;
	((uint8_t *)dst)[1] = ((val & 0x800) != 0) ? 0xF6 : 0xF2;
	((uint8_t *)dst)[2] = val & 0xFF;
	((uint8_t *)dst)[3] = (((val & ~0x800) & 0xF00) >> 4) | target_reg;

	return 0;
}

int get_movt_opcode(void *dst, unsigned int target_reg, uint16_t val){

	if(dst == NULL)
		return -1;

	if(target_reg > 0xF)
		return -2;

	((uint8_t *)dst)[0] = ((val >> 0xC) & 0xF) | 0xC0;
	((uint8_t *)dst)[1] = ((val & 0x800) != 0) ? 0xF6 : 0xF2;
	((uint8_t *)dst)[2] = val & 0xFF;
	((uint8_t *)dst)[3] = (((val & ~0x800) & 0xF00) >> 4) | target_reg;

	return 0;
}

int arm_thumb_branch_decode(uint32_t pc, const uint16_t *inst, unsigned int *type, uint32_t *result){

	uint32_t offset = 0;

	unsigned int b_type;

	uint16_t inst0 = inst[0];
	uint16_t inst1 = inst[1];

	if((inst0 & 0xF800) != 0xF000 || (inst1 & 0x8000) == 0){
		return -1; // not branch inst
	}

	b_type = ((inst1 >> 12) & 1) | ((inst1 >> 13) & 2);

	switch(inst1 & 0x5000){
	case 0: // b<cond>.w
		break;
	case 0x4000: // blx
		if((inst1 & 1) != 0){
			return -1; // not branch inst
		}

		pc &= ~3;
	case 0x1000: // b.w
	case 0x5000: // bl
		pc += 4;
		offset += ((inst1 & 0x7FF) << 1);
		offset -= ((inst1 & 0x800) << 11);
		offset -= ((inst1 & 0x2000) << 10);
		offset += 0xC00000;
		offset += ((inst0 & 0x3FF) << 12);

		if((inst0 & 0x400) != 0){
			// offset = (offset & ~0xFFC00000) | (~(offset & 0xFFC00000) & 0xFFC00000);
			offset ^= 0xFFC00000;
		}

		pc += offset;

		break;
	}

	if(b_type == ARM_THUBM_TYPE_BL){
		pc |= 1;
	}

	if(type != NULL){
		*type = b_type;
	}

	*result = pc;

	// printf("Target %04X %04X -> pc 0x%08X (%d)\n", inst0, inst1, pc, *type);

	return 0;
}

int arm_thumb_branch_encode(uint32_t pc, uint32_t target_pc, unsigned int type, uint16_t *result){

	pc += 4;

	if((target_pc - pc) > 0x1000000 && (pc - target_pc) > 0x1000000){
		// printf("invalid branch range. should be in +-%d-MiB.\n", 16);
		return -1;
	}

	if(type > 3){
		// printf("invalid branch type.\n");
		return -1;
	}

	if(type == ARM_THUBM_TYPE_B_COND){
		// printf("branch b<cond> is not supported.\n");
		return -1;
	}

	if(type == ARM_THUBM_TYPE_BLX){
		pc &= ~3;
		target_pc &= ~3;
	}

	int32_t offset = (target_pc - pc);

	uint32_t inst0 = 0xF000 | ((offset >> 12) & 0x3FF);
	uint32_t inst1 = 0x8000 | ((offset >> 1) & 0x7FF);

	inst1 |= ((type & 1) << 12) | ((type & 2) << 13);

	if(offset < 0){
		offset = ~offset;
		inst0 |= 0x400;
	}

	if((offset & 0x800000) == 0){
		inst1 |= 0x2000; // - 0x800000
	}

	if((offset & 0x400000) == 0){
		inst1 |= 0x800; // - 0x400000
	}

	if(type == ARM_THUBM_TYPE_BLX){
		inst1 &= ~1;
	}

	// printf("pc=0x%08X target_pc=0x%08X -> %04X %04X\n", _pc, target_pc, inst0, inst1);

	result[0] = inst0;
	result[1] = inst1;

	return 0;
}
