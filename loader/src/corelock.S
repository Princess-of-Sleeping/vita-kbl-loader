
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

	.global ksceKernelCorelockInitialize
	.type   ksceKernelCorelockInitialize, %function

ksceKernelCorelockInitialize:
	movs       r3, #0x4
	movs       r2, #0x1
	str        r2, [r0, #0x0]
	strh       r3, [r0, #0x4]
	strh       r3, [r0, #0x6]
	bx         lr


	.global ksceKernelCorelockLock
	.type   ksceKernelCorelockLock, %function

ksceKernelCorelockLock:
	push       {r3, r4, r5, lr}
	mov        r4, r0
	mov        r5, r1
	blx        ksceKernelCpuId
	dmb        sy
	cmp        r0, r5
	beq        LAB_81022bb0
	ldrh       r3, [r4, #0x6]
	sxth       r2, r0
	adds       r1, r4, #0x6
	sxth       r3, r3
	cmp        r2, r3
	beq        LAB_81022ba8

LAB_81022b9e:
	wfe
	ldrh       r3, [r1, #0x0]
	sxth       r3, r3
	cmp        r3, r2
	bne        LAB_81022b9e

LAB_81022ba8:
	mov        r0, r4
	blx        ksceKernelSpinlockLowLock
	pop        {r3, r4, r5, pc}

LAB_81022bb0:
	strh       r0, [r4, #0x6]
	pop        {r3, r4, r5, pc}


	.global ksceKernelCorelockUnlock
	.type   ksceKernelCorelockUnlock, %function

ksceKernelCorelockUnlock:
	push       {r4, r5, r6, lr}
	mov        r5, r0
	blx        ksceKernelCpuId
	adds       r0, #0x1
	cmp        r0, #0x4
	ite        ne
	uxthne     r0, r0
	moveq      r0, #0x0
	dmb        sy
	adds       r4, r5, #0x4
	strh       r0, [r5, #0x6]
	movs       r1, #0x1
	mov        r0, r4
	blx        ksceKernelAtomicGetAndSub16
	mov        r6, r0
	mov        r0, r5
	blx        ksceKernelSpinlockLowUnlock
	dsb        sy
	sev
	cmp        r6, #0x4
	ldrh       r3, [r5, #0x4]
	beq        LAB_81022bfc
	cmp        r3, #0x4
	beq        LAB_81022c14

LAB_81022bee:
	wfe
	ldrh       r3, [r4, #0x0]
	cmp        r3, #0x4
	bne        LAB_81022bee
	pop        {r4, r5, r6, pc}

LAB_81022bf8:
	wfe
	ldrh       r3, [r4, #0x0]

LAB_81022bfc:
	sxth       r3, r3
	cmp        r3, #0x0
	bgt        LAB_81022bf8
	movs       r3, #0x4
	movs       r2, #0x1
	str        r2, [r5, #0x0]
	strh       r3, [r5, #0x4]
	strh       r3, [r5, #0x6]
	dsb        sy
	sev
	pop        {r4, r5, r6, pc}

LAB_81022c14:
	pop        {r4, r5, r6, pc}




	.data
