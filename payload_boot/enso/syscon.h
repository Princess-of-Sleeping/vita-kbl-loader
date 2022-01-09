/*
 * syscon.h
 */
#ifndef _NSKBL_SYSCON_H_
#define _NSKBL_SYSCON_H_

#define CTRL_BUTTON_HELD(ctrl, button) !((ctrl) & (button))

#define SCE_SYSCON_CTRL_UP        (1 << 0)
#define SCE_SYSCON_CTRL_RIGHT     (1 << 1)
#define SCE_SYSCON_CTRL_DOWN      (1 << 2)
#define SCE_SYSCON_CTRL_LEFT      (1 << 3)
#define SCE_SYSCON_CTRL_TRIANGLE  (1 << 4)
#define SCE_SYSCON_CTRL_CIRCLE    (1 << 5)
#define SCE_SYSCON_CTRL_CROSS     (1 << 6)
#define SCE_SYSCON_CTRL_SQUARE    (1 << 7)
#define SCE_SYSCON_CTRL_SELECT    (1 << 8)
#define SCE_SYSCON_CTRL_L         (1 << 9)
#define SCE_SYSCON_CTRL_R         (1 << 10)
#define SCE_SYSCON_CTRL_START     (1 << 11)
#define SCE_SYSCON_CTRL_PSBUTTON  (1 << 12)
#define SCE_SYSCON_CTRL_POWER     (1 << 14)
#define SCE_SYSCON_CTRL_VOLUP     (1 << 16)
#define SCE_SYSCON_CTRL_VOLDOWN   (1 << 17)
#define SCE_SYSCON_CTRL_HEADPHONE (1 << 27)

void syscon_common_read(unsigned int *buffer, unsigned short cmd);
void syscon_common_write(unsigned int data, unsigned short cmd, unsigned int length);

void syscon_ctrl_read(unsigned int *data);

#endif
