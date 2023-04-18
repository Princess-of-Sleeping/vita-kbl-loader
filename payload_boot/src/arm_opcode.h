
#ifndef _ARM_OPCODE_H_
#define _ARM_OPCODE_H_


int get_movw_opcode(void *dst, unsigned int target_reg, uint16_t val);
int get_movt_opcode(void *dst, unsigned int target_reg, uint16_t val);


#define ARM_THUBM_TYPE_B_COND (0)
#define ARM_THUBM_TYPE_B      (1)
#define ARM_THUBM_TYPE_BLX    (2)
#define ARM_THUBM_TYPE_BL     (3)

int arm_thumb_branch_decode(uint32_t pc, const uint16_t *inst, unsigned int *type, uint32_t *result);
int arm_thumb_branch_encode(uint32_t pc, uint32_t target_pc, unsigned int type, uint16_t *result);


#endif
