
	.balign 4
	.text
	.cpu cortex-a9
	.arch armv7-a
	.syntax unified
	.arm
	.fpu neon

	.globl _start
	.type  _start, %function
	.section .text._start
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
	.word scratchpad_reset

skUndefined:
	.word scratchpad_undef

skSvc:
	.word scratchpad_svc

skPrefetch:
	.word scratchpad_prefetch

skAbort:
	.word scratchpad_abort

skIrq:
	.word scratchpad_irq

skFiq:
	.word scratchpad_fiq

skReserved:
	.word 0xFFFFFFFF

	.set CORE_STACK_SIZE, 0x400
	.set STACK_POINT_SVC, 0x4A01C000
	.set STACK_POINT_UND, STACK_POINT_SVC + (CORE_STACK_SIZE * 4)
	.set STACK_POINT_ABT, STACK_POINT_UND + (CORE_STACK_SIZE * 4)
	.set STACK_POINT_FIQ, STACK_POINT_ABT + (CORE_STACK_SIZE * 4)
	.set STACK_POINT_SYS, STACK_POINT_FIQ + (CORE_STACK_SIZE * 4)

	.globl scratchpad_reset
scratchpad_reset:
	@ Disable interrupts and enter System mode
	cpsid aif, #0x1F
	clrex

	@ mrc p15, 0, r0, c1, c0, 0
	mov r0, #0
	bic r0, r0, #0x40000000 @ Thumb Exception enable.
	bic r0, r0, #0x20000000 @ Access flag enable bit.
	bic r0, r0, #0x10000000 @ TEX remap enable bit.
	bic r0, r0, #0x2000000  @ Exception Endianness bit.
	bic r0, r0, #0x2000     @ Vectors bit.
	bic r0, r0, #0x1000     @ Instruction cache enable bit
	bic r0, r0, #0x800      @ Branch prediction enable bit
	bic r0, r0, #0x400      @ SWP/SWPB enable bit
	bic r0, r0, #0x4        @ Cache enable bit
	bic r0, r0, #0x2        @ Alignment bit
	bic r0, r0, #0x1        @ MPU enable bit (MMU)
	mcr p15, #0, r0, c1, c0, #0 @ SCTLR

	mov r0, #0
	mcr p15, #0, r0, c7, c1, #0 @ ICIALLUIS
	dsb
	mcr p15, #0, r0, c7, c1, #6 @ BPIALLIS
	isb
	mcr p15, #0, r0, c7, c5, #0 @ ICIALLU
	dsb

dcisw_set:
	mov r1, #0
dcisw_way:
	orr r2, r1, r0
	mcr p15, #0, r2, c7, c6, #2 @ DCISW
	add r1, r1, #0x40000000
	cmp r1, #0x0
	bne dcisw_way
	add r0, r0, #0x20
	cmp r0, #0x2000
	bne dcisw_set

	dsb
	mov r0, #0x0
	mcr p15, #0, r0, c7, c5, #6 @ BPIALL

	isb
	mcr p15, #0, r0, c8, c3, #0 @ TLBIALLIS
	mov r0, #0
	mcr p15, #0, r0, c7, c5, #0 @ ICIALLU
	mov r0, #0
	mcr p15, #0, r0, c8, c7, #0 @ TLBIALL
	mcr p15, #0, r0, c2, c0, #2 @ TTBCR
	mov r0, #0
	mcr p15, #0, r0, c7, c5, #6 @ BPIALL

	isb
	mrc p15, #0, r1, c1, c0, #0 @ SCTLR
	bic r1, r1, #0x2
	mcr p15, #0, r1, c1, c0, #0

	mrc p15, #0, r3, c0, c0, #5 @ MPIDR
	and r3, r3, #0xF

	@ Settings stack points
	mov r0, #CORE_STACK_SIZE
	mul r0, r3, r0
	mov r1, #0

	@ SVC
	cpsid if, #0x13
	movw sp, #:lower16:STACK_POINT_SVC
	movt sp, #:upper16:STACK_POINT_SVC
	sub sp, sp, r0
	mov lr, r1
	msr spsr_cxsf, r1

	@ Undefined
	cpsid if, #0x1B
	movw sp, #:lower16:STACK_POINT_UND
	movt sp, #:upper16:STACK_POINT_UND
	sub sp, sp, r0
	mov lr, r1
	msr spsr_cxsf, r1

	@ Abort
	cpsid if, #0x17
	movw sp, #:lower16:STACK_POINT_ABT
	movt sp, #:upper16:STACK_POINT_ABT
	sub sp, sp, r0
	mov lr, r1
	msr spsr_cxsf, r1

	@ IRQ
	cpsid if, #0x12
	movw sp, #:lower16:STACK_POINT_FIQ
	movt sp, #:upper16:STACK_POINT_FIQ
	sub sp, sp, r0
	mov lr, r1
	msr spsr_cxsf,r1

	@ FIQ with init banked registers
	cpsid if, #0x11
	movw r2, #:lower16:0xDEADBEEF
	movt r2, #:upper16:0xDEADBEEF
	mov r8, r2
	mov r9, r2
	mov r10, r2
	mov r11, r2
	mov r12, r2
	mov sp, r1
	mov lr, r1
	msr spsr_cxsf, r1

	@ System
	cpsid if, #0x1F
	movw sp, #:lower16:STACK_POINT_SYS
	movt sp, #:upper16:STACK_POINT_SYS
	sub sp, sp, r0, lsl #2
	mov lr, r1
	msr spsr_cxsf, r1

	@ Setting reset vector
	movw r0, #:lower16:0x00000000
	movt r0, #:upper16:0x00000000
	mcr p15, #0, r0, c12, c0, #0 @ VBAR

	cmp r3, #0
	wfene
	bne jump2reset_main
	sev

jump2reset_main:
	mrs r0, cpsr
	bic r0, r0, #0x100
	msr cpsr_x, r0
	ldr pc, reset_main_pointer

reset_main_pointer:
	.word reset_main

	.globl scratchpad_undef
	.type  scratchpad_undef, %function
scratchpad_undef:
	sub lr, lr, #0x8
	stmdb sp!, {r0, r1, r2, r3, r12, lr}
	mov r0, lr
	blx scratchpad_undef_handler
	clrex
	ldmia sp!, {r0, r1, r2, r3, r12, pc}^

	.globl scratchpad_svc
	.type  scratchpad_svc, %function
scratchpad_svc:
	sub lr, lr, #0x8
	stmdb sp!, {r0, r1, r2, r3, r12, lr}
	mov r0, lr
	blx scratchpad_svc_handler
	clrex
	ldmia sp!, {r0, r1, r2, r3, r12, pc}^

	.globl scratchpad_prefetch
	.type  scratchpad_prefetch, %function
scratchpad_prefetch:
	sub lr, lr, #0x8
	stmdb sp!, {r0, r1, r2, r3, r12, lr}
	mov r0, lr
	blx scratchpad_prefetch_handler
	clrex
	ldmia sp!, {r0, r1, r2, r3, r12, pc}^

	.globl scratchpad_abort
	.type  scratchpad_abort, %function
scratchpad_abort:
	sub lr, lr, #0x8
	stmdb sp!, {r0, r1, r2, r3, r12, lr}
	mov r0, lr
	blx scratchpad_abort_handler
	clrex
	ldmia sp!, {r0, r1, r2, r3, r12, pc}^

	.globl scratchpad_irq
	.type  scratchpad_irq, %function
scratchpad_irq:
	sub lr, lr, #0x8
	stmdb sp!, {r0, r1, r2, r3, r12, lr}
	mov r0, lr
	blx scratchpad_irq_handler
	clrex
	ldmia sp!, {r0, r1, r2, r3, r12, pc}^

	.globl scratchpad_fiq
	.type  scratchpad_fiq, %function
scratchpad_fiq:
	sub lr, lr, #0x8
	stmdb sp!, {r0, r1, r2, r3, r12, lr}
	mov r0, lr
	blx scratchpad_fiq_handler
	clrex
	ldmia sp!, {r0, r1, r2, r3, r12, pc}^

	.data
