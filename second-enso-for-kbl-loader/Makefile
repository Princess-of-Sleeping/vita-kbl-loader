CC=arm-vita-eabi-gcc
CFLAGS=-Os -fno-builtin-printf -fPIC -fno-builtin-memset -Wall -Wextra -Wno-unused-variable -DFW_360
OBJCOPY=arm-vita-eabi-objcopy
LDFLAGS=-nodefaultlibs -nostdlib

second_enso.bin: second
	$(OBJCOPY) -O binary $^ $@

second: second.o
	$(CC) -o $@ $^ $(LDFLAGS) -T second.x

clean:
	-rm -f second.bin *.o
