
	.balign 4
	.text
	.cpu cortex-a9
	.arch armv7-a
	.syntax unified
	.thumb
	.thumb_func
	.fpu neon
	.arch_extension sec

	.globl reset_main
	.type  reset_main, %function
reset_main:
	push {r3, r4}

	@ Get CPU ID
	mrc p15, 0, r4, c0, c0, 5
	and r4, #0xF
	cmp r4, #0
	bne core_nzero_point

	@ ================ Core0 only ================

	@ Clean cache
	movs r0, #1
	movw ip, #0x16A
	smc #0

	@ If you not need patched reboot, just commented out this call.
	bl payload_bootstrap_main

	@ Copy reset vector
	mov r0, #0x1F000000
	mov r1, #0x51000000
	movs r2, #0x40
	bl memcpy2

	@ ================ Core0 only ================

core_nzero_point:
	cmp r4, #0
	it ne
	wfene
	bne jump2reset
	sev

jump2reset:
	pop {r3, r4}
	movs r0, #0
	bx r0

	.data
