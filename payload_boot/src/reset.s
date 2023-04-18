
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

	.data
