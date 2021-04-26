/*
 * Reset Vector RE of the secure kernel bootloader
 * Copyright (C) 2020, Princess of Sleeping
 */

	.syntax unified
	.cpu cortex-a9
	.text
	.align 0

	.global _start
_start:
	ldr pc, skReset
	ldr pc, skUndefined
	ldr pc, skSvc
	ldr pc, skPrefetch
	ldr pc, skAbort
	ldr pc, skReset		@ Reserved
	ldr pc, skIrq
	ldr pc, skFiq

skReset:
	.word payload_bootstrap_ns_main_c

skUndefined:
	.word 0x400203A4

skSvc:
	.word 0x400203BC

skPrefetch:
	.word 0x400203D0

skAbort:
	.word 0x400203E8

skIrq:
	.word 0x40020400

skFiq:
	.word 0x40020414

skReserved:
	.word 0xFFFFFFFF

	.word 0
	.word 0

	ldr pc, skUnknown0
	ldr pc, skUnknown1
	ldr pc, skUnknown2

	.word 0

	ldr pc, skUnknown3
	ldr pc, skUnknown4

skUnknown0:
	.word 0x40020768

skUnknown1:
	.word 0x4002077C

skUnknown2:
	.word 0x40020790

skUnknown3:
	.word 0x400207A4

skUnknown4:
	.word 0x400207B8

	.word 0
	.word 0
	.word 0

	.word 0
	.word 0
	.word 0
	.word 0

	@ Begin some sysroot data
	.word 0x80000001
	.word 0
	.word 0x413E7
	.word 0x20000010
	@ End some sysroot data

	.word 0x9E3199B7	@ some magic, check by secure loader
	.word 0
	.word 0
	.word 0

	.word 0
	.word 0
	.word 0
	.word 0

	.global psp2bootconfig_load_hook
	.type   psp2bootconfig_load_hook, %function
psp2bootconfig_load_hook:

	blx psp2bootconfig_load_hook_main

	mov r2, #0x1
	movw r0, #0x3c08
	movt r4, #0x510b
	str r3, [sp, #0x0]

	movw r1, #:lower16:0x51001681
	movt r1, #:upper16:0x51001681

	blx r1

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
