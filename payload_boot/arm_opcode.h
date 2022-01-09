
#ifndef _ARM_OPCODE_H_
#define _ARM_OPCODE_H_

int get_movw_opcode(void *dst, unsigned int target_reg, uint16_t val);
int get_movt_opcode(void *dst, unsigned int target_reg, uint16_t val);

#endif
