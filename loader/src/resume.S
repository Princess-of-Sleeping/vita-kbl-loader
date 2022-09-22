
	.align 4
	.balign 4
	.text
	.cpu cortex-a9
	.arch armv7-a
	.syntax unified
	.thumb
	.thumb_func
	.fpu neon
	.arch_extension sec

	.global resume_function
	.type   resume_function, %function
resume_function:
	@ r0 TTBR1
	@ r1 CONTEXTIDR
	@ r2 DACR

	dsb
	movs r3, #0
	mcr p15, 0, r3, c13, c0, 1 @ CONTEXTIDR

	isb
	mcr p15, #0, r0, c2, c0, #1 @ TTBR1

	isb
	dsb
	mcr p15, #0, r1, c13, c0, #1 @ CONTEXTIDR
	mcr p15, #0, r2, c3, c0, #1  @ Undocumented DACR register

	dsb
	isb
	movs r0, #0
	mcr p15, #0, r0, c8, c7, #0 @ TLBIALL

	// Sync cores
	mrc p15, #0, r0, c0, c0, #5
	ands r0, r0, #0xf
	movw r1, #:lower16:coresync
	movt r1, #:upper16:coresync
	cmp r0, #0x0
	it eq
	streq r0, [r1, #0x0]

sync1:
	ldrb r2, [r1, #0x0]
	cmp r0, r2
	itt ne
	wfene
	bne sync1
	ldrh r2, [r1,#0x0]
	adds r2, #0x1
	adds r2, r2, #0x100
	strh r2, [r1, #0x0]
	dsb
	sev

sync2:
	ldrb r2, [r1, #0x1]
	cmp r2, #4
	itt ne
	wfene
	bne sync2

	@ Setting stack point
	movw r0, #:lower16:resume_stack_base
	movt r0, #:upper16:resume_stack_base
	ldr r0, [r0]
	add r0, #0x4000 @ To stack top

	@ core_sp = stack_point - (cpu_id * 0x400)
	mrc p15, 0, r1, c0, c0, 5
	mov r2, #0x400
	and r1, #0xF
	mul r1, r1, r2
	sub r0, r1
	mov sp, r0

	@ first core lock for sync
	movw r0, #:lower16:corelock_ctx
	movt r0, #:upper16:corelock_ctx
	bl ksceKernelCorelockInitialize

	movw r0, #:lower16:corelock_ctx
	movt r0, #:upper16:corelock_ctx
	movs r1, #0
	bl ksceKernelCorelockLock

	movw r0, #:lower16:corelock_ctx
	movt r0, #:upper16:corelock_ctx
	bl ksceKernelCorelockUnlock

	@ second core lock
	movw r0, #:lower16:corelock_ctx
	movt r0, #:upper16:corelock_ctx
	movs r1, #0
	bl ksceKernelCorelockLock

	@ Get CPU ID
	mrc p15, 0, r0, c0, c0, 5
	and r0, #0xF
	cmp r0, #0
	bne cpu1_3_cont

	@ ================ core0 only ================

	@ Identity map the scratchpad using a 1MiB section
	movw r2, #:lower16:sys_ttbr0_vbase
	movt r2, #:upper16:sys_ttbr0_vbase
	ldr r2, [r2]

	ldr ip, r_scratchpad
	lsrs ip, #20

	adds r2, r2, ip, lsl #2

	movw r3, #:lower16:0x91402
	movt r3, #:upper16:0x91402
	orrs r3, r3, ip, lsl #20

	str r3, [r2]
	mcr p15, #0, r2, c7, c14, #1 @ DCCIMVAC (Data Cache line Clean and Invalidate by VA to PoC)

	dsb
	mcr p15, #0, r0, c8, c7, #0 @ TLBIALL (Unified TLB Invalidate All)

	dsb
	isb

	@ Copy the payload bootstrap to the scratchpad
	ldr r0, r_scratchpad
	ldr r1, =scratchpad_data
	ldr r2, =scratchpad_data_len
	ldr r2, [r2]
	bl resume_memcpy

	ldr r0, r_scratchpad
	ldr r1, =scratchpad_data_len
	ldr r1, [r1]
	bl dcache_clean_range

	@ ================ core0 only ================

cpu1_3_cont:
	movw r0, #:lower16:corelock_ctx
	movt r0, #:upper16:corelock_ctx
	bl ksceKernelCorelockUnlock

	@ Jump to the payload bootsrap!
	ldr lr, r_scratchpad
	bx lr

r_scratchpad:
	.word 0x1F000000

@ r0 = addr, r1 = size
@ Uses: r0, r1
dcache_clean_range:
	add r1, r0
	bic r0, #0x1F
	dsb
1:
	mcr p15, 0, r0, c7, c10, 1 @ DCCMVAC (Data Cache line Clean by VA to PoC)
	add r0, #32
	cmp r0, r1
	blo 1b
	dsb
	bx lr

@ r0 = dst, r1 = src, r2 = size
@ Uses: r0, r1, r2, r3
resume_memcpy:
	and ip, r2, #3
	bic r2, r2, #3

	cmp r2, #0
	beq r_memcpy_byte

r_memcpy_word:
	ldmia r1!, {r3}
	stmia r0!, {r3}
	subs r2, #4
	bne r_memcpy_word

	cmp ip, #0
	beq r_memcpy_exit

r_memcpy_byte:
	ldrb r3, [r1], #1
	strb r3, [r0], #1
	subs ip, #1
	bne r_memcpy_byte

r_memcpy_exit:
	bx lr

	.data

corelock_ctx:
	.word 0
	.word 0

coresync:
	.word 0
