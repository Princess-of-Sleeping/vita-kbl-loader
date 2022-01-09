
	.syntax unified
	.cpu cortex-a9
	.text
	.balign 4

	.global _start
	.type   _start, %function
	.section .text._start
_start:
	ldr pc, boot_main_ptr

boot_main_ptr:
	.word boot_main

	.global psp2bootconfig_load_hook
	.type   psp2bootconfig_load_hook, %function
psp2bootconfig_load_hook:
	@ Get CPU ID
	mrc p15, 0, ip, c0, c0, 5
	and ip, #0xF
	cmp ip, #0
	bne return2load_kernel

	push {r0, r2, r3}
	blx psp2bootconfig_load_hook_main
	pop {r0, r2, r3}

return2load_kernel:
	@ 0x510012e8 on 3.60
	movt r0, #0x5116
	movt r3, #0x5102
	mov r1, #0x110000

	movw ip, #:lower16:0x510012e8
	movt ip, #:upper16:0x510012e8
	add ip, #0xC @ Back to original code
	orr ip, #1   @ Thumb
	blx ip

	.global sceSblAuthMgrAuthHeader_patch
	.type   sceSblAuthMgrAuthHeader_patch, %function
sceSblAuthMgrAuthHeader_patch:

	cmp  r0, #0x1
	push {r4, r5, r6, r7, r8, lr}
	mov r5, r1
	mov r7, r2
	mov r4, r3

	movw ip, #:lower16:0x51016d75
	movt ip, #:upper16:0x51016d75

	bx ip

	.global sceSblAuthMgrSetupAuthSegment_patch
	.type   sceSblAuthMgrSetupAuthSegment_patch, %function
sceSblAuthMgrSetupAuthSegment_patch:

	cmp r0, #0x1
	push {r3, r4, r5, lr}
	mov r5, r1

	// Must to not jump
	bne 0x51016e88
	movw  r3, #0xf0f8

	movw ip, #:lower16:0x51016e65
	movt ip, #:upper16:0x51016e65

	bx ip

	.global sceSblAuthMgrAuthSegment_patch
	.type   sceSblAuthMgrAuthSegment_patch, %function
sceSblAuthMgrAuthSegment_patch:

	cmp  r0, #0x1
	push {r4, r5, r6, r7, r8, lr }
	mov  r4, r1
	mov  r6, r2

	// Must to not jump
	bne 0x51016f1c

	movw ip, #:lower16:0x51016ea1
	movt ip, #:upper16:0x51016ea1

	bx ip
