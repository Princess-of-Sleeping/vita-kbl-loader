OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(start)

SECTIONS
{
  . = 0x77F8000;
  .text   : { *(.text.start) *(.text   .text.*   .gnu.linkonce.t.*) *(.sceStub.text.*) }
  .rodata : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
  . = (. + 0x3F) & ~0x3F;
  .data   : { *(.data   .data.*   .gnu.linkonce.d.*) }
  .bss    : { *(.bss    .bss.*    .gnu.linkonce.b.*) *(COMMON) }
}
