OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(_start)

SECTIONS
{
	. = 0x51FF0000;
	.text : {
		*(.text)
		*(.text*)
	}
	.data : {
		*(.data*)
	}
}
