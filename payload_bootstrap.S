	.align 4
	.text
	.cpu cortex-a9

	.globl _start
@ r0 = payload src paddr, r1 = payload dst paddr, r2 = payload size, r3 = sysroot buffer paddr
_start:
	dsb

	@ Disable interrupts and enter System mode
	cpsid aif, #0x1F

	mov r8, r0
	mov r9, r1
	mov r10, r2
	mov r11, r3

	@ DACR unrestricted
	mov r0, #0xFFFFFFFF
	mcr p15, 0, r0, c3, c0, 0

	ldr r0, =sync_point_1
	bl cpus_sync

	@ Clean and invalidate the entire Dcache
	bl dcache_clean_inv_all

	@ Now we are in an identity-mapped region, let's disable
	@ the MMU, the Dcache and the Icache
	mrc p15, 0, r0, c1, c0, 0
	bic r0, #1 << 0		@ MMU
	bic r0, #1 << 1		@ Dcache?
	bic r0, #1 << 2		@ Dcache
	bic r0, #1 << 12	@ Icache
	mcr p15, 0, r0, c1, c0, 0

	@ Invalidate the entire Dcache
	bl dcache_inv_all

	mov r0, #0
	mcr p15, 0, r0, c7, c5, 6 @ BPIALL (Branch Predictor Invalidate All)
	isb
	mcr p15, 0, r0, c7, c5, 0 @ ICIALLU (Icache Invalidate All to PoU)
	dsb
	mcr p15, 0, r0, c8, c7, 0 @ TLBIALL (Unified TLB Invalidate All)
	isb

	@ Get CPU ID
	mrc p15, 0, r0, c0, c0, 5
	and r0, #0xF
	cmp r0, #0
	bne cpu1_3_cont

	movs r0, #1
	movw ip, #0x16A
	smc #0

	// copy kblparam
	movw r0, #:lower16:0x40200100
	movt r0, #:upper16:0x40200100
	mov r1, r11
	movw r2, #0x200
	bl memcpy

	movw sp, #:lower16:0x44000000
	movt sp, #:upper16:0x44000000

	bl payload_bootstrap_main

/*
	// Pscode to 0x105
	movw r0, #:lower16:0x40200100
	movt r0, #:upper16:0x40200100
	movw r1, #0x501
	strh r1, [r0, #0xA4]
*/

	// Memory access test
	movw r0, #:lower16:0x51000100
	movt r0, #:upper16:0x51000100
	ldr r1, [r0]

	movw r0, #:lower16:0x51000102
	movt r0, #:upper16:0x51000102
	ldrh r1, [r0]

	movw r0, #:lower16:0x51000103
	movt r0, #:upper16:0x51000103
	ldrb r1, [r0]


	// clean SceKernelL2PageTable000
	movw r0, #:lower16:0x40201000
	movt r0, #:upper16:0x40201000
	movs r1, #0
	movw r2, #0x1000
	bl memset

	movw r0, #:lower16:0x40202000
	movt r0, #:upper16:0x40202000
	movs r1, #0
	movw r2, #0x2000
	bl memset

	// clean sysroot
	movw r0, #:lower16:0x40204000
	movt r0, #:upper16:0x40204000
	movs r1, #0
	movw r2, #:lower16:0x4000
	bl memset

	// clean TTBR0
	movw r0, #:lower16:0x40208000
	movt r0, #:upper16:0x40208000
	movs r1, #0
	movw r2, #0x4000
	bl memset

	movw r0, #:lower16:0x40208000
	movt r0, #:upper16:0x40208000
	bl sceKernelCpuDcacheCleanInvalidateMVAC

	// clean TTBR1
	movw r0, #:lower16:0x4020C000
	movt r0, #:upper16:0x4020C000
	movs r1, #0
	movw r2, #0x4000
	bl memset

	movw r0, #:lower16:0x4020C000
	movt r0, #:upper16:0x4020C000
	bl sceKernelCpuDcacheCleanInvalidateMVAC

	// clean SceKernelL2Vector
	movw r0, #:lower16:0x40210000
	movt r0, #:upper16:0x40210000
	movs r1, #0
	movw r2, #0x4000
	bl memset

cpu1_3_cont:
	ldr r0, =sync_point_2
	bl cpus_sync

	movw r0, #:lower16:0x51000000
	movt r0, #:upper16:0x51000000

	bx r0

	@ Jump to the payload!
	mov r0, r11
	mov lr, r9
	bx lr

@ Uses: r0, r1
dcache_clean_inv_all:
	mov r0, #0
1:
	mcr p15, 0, r0, c7, c14, 2 @ DCCISW (Data cache clean and invalidate by set/way)
	adds r0, r0, #0x40000000
	bcc 1b
	adds r0, #0x20
	lsrs r1, r0, #0xD
	beq 1b
	dsb
	bx lr

@ Uses: r0, r1
dcache_inv_all:
	mov r0, #0
1:
	mcr p15, 0, r0, c7, c6, 2 @ DCISW (Data cache invalidate by set/way)
	adds r0, r0, #0x40000000
	bcc 1b
	adds r0, #0x20
	lsrs r1, r0, #0xD
	beq 1b
	dsb
	bx lr

@ r0 = sync point address
@ Uses: r0, r1, r2
cpus_sync:
	mrc p15, 0, r1, c0, c0, 5
	and r1, #0xF
	cmp r1, #0
	streq r1, [r0]
1:
	ldrb r2, [r0]
	cmp r1, r2
	wfene
	bne 1b
	ldrh r2, [r0]
	adds r2, #1
	adds r2, r2, #0x100
	strh r2, [r0]
	dsb
	sev
1:
	ldrb r2, [r0, #1]
	cmp r2, #4
	wfene
	bne 1b
	bx lr

@ r0 = dst, r1 = src, r2 = size
@ Uses: r0, r1, r2, r3
memcpy:
	ldmia r1!, {r3}
	stmia r0!, {r3}
	subs r2, #4
	bne memcpy
	bx lr

memset:
	adds r2, r0

memset_loop:
	stmia r0!, {r1}
	cmp r0, r2
	bne memset_loop
	bx lr

sceKernelCpuDcacheCleanInvalidateMVAC:
	mcr p15, #0, r0, c7, c14, #1
	dsb sy
	bx lr


	.global log_hook_tmp
	.type   log_hook_tmp, %function
log_hook_tmp:

	bl log_hook_main

        movw r1, #0x3b18
        movw r2, #0x3858
        movt r1, #0x5102

	movw r0, #:lower16:0x51000C33
	movt r0, #:upper16:0x51000C33

	bx r0

	.data
sync_point_1: .word 0
sync_point_2: .word 0
