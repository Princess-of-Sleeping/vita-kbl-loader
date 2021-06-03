TARGET   = kbl-loader
TARGET_OBJS  = main.o resume.o
BOOTSTRAP_OBJS = payload_bootstrap.o payload_bootstrap_main.o
BOOTSTRAP_NSKBL_OBJS = payload_bootstrap_ns_main.o payload_bootstrap_ns_main_c.o enso/syscon.o enso/lowio.o

LIBS =	-ltaihenForKernel_stub -lSceSysclibForDriver_stub -lSceSysmemForDriver_stub \
	-lSceThreadmgrForDriver_stub \
	-lSceCpuForDriver_stub -lSceUartForKernel_stub -lScePervasiveForDriver_stub \
	-lSceSysconForDriver_stub -lScePowerForDriver_stub -lSceIofilemgrForDriver_stub \
	-lSceSysrootForKernel_stub -ltaihenModuleUtils_stub -lSceDebugForDriver_stub -lSceSblAIMgrForDriver_stub

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
AS      = $(PREFIX)-as
OBJCOPY = $(PREFIX)-objcopy
CFLAGS  = -Wl,-q -Wall -O0 -nostartfiles -mcpu=cortex-a9 -mthumb-interwork -DFW_360
ASFLAGS =

all: $(TARGET).skprx

%.skprx: %.velf
	vita-make-fself -c $< $@

%.velf: %.elf
	vita-elf-create -e $(TARGET).yml $< $@



payload_bootstrap.elf: $(BOOTSTRAP_OBJS)
	$(CC) -T payload_bootstrap.ld -nostartfiles -nostdlib $^ -o $@ -lgcc

payload_bootstrap.bin: payload_bootstrap.elf
	$(OBJCOPY) -S -O binary $^ $@

payload_bootstrap_bin.o: payload_bootstrap.bin
	$(OBJCOPY) -I binary -O elf32-littlearm --binary-architecture arm $^ $@



payload_bootstrap_ns.elf: $(BOOTSTRAP_NSKBL_OBJS)
	$(CC) -T payload_bootstrap_ns.ld -nostartfiles -nostdlib $^ -o $@ -lgcc

payload_bootstrap_ns.bin: payload_bootstrap_ns.elf
	$(OBJCOPY) -S -O binary $^ $@

payload_bootstrap_ns_bin.o: payload_bootstrap_ns.bin
	$(OBJCOPY) -I binary -O elf32-littlearm --binary-architecture arm $^ $@



$(TARGET).elf: $(TARGET_OBJS) payload_bootstrap_bin.o payload_bootstrap_ns_bin.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

.PHONY: all clean send

clean:
	@rm -rf $(TARGET).skprx $(TARGET).velf $(TARGET).elf $(TARGET_OBJS) $(BOOTSTRAP_OBJS) \
	        payload_bootstrap.elf payload_bootstrap.bin payload_bootstrap_bin.o \
	        payload_bootstrap_ns.elf payload_bootstrap_ns.bin payload_bootstrap_ns_main_c.o payload_bootstrap_ns_bin.o payload_bootstrap_ns_main.o

send: $(TARGET).skprx
	curl --ftp-method nocwd -T $(TARGET).skprx ftp://$(PSVITAIP):1337/ux0:/data/tai/kplugin.skprx
	@echo "Sent."
