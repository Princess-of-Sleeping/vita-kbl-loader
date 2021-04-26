/*
 * lowio.c
 * from vita-libbaremetal
 */
#include <inttypes.h>
#include "lowio.h"

void gpio_port_set(int bus, int port){
	volatile unsigned int *gpio_regs = GPIO_REGS(bus);
	gpio_regs[2] |= 1 << port;
	gpio_regs[0xD];
	dsb();
}

void gpio_port_clear(int bus, int port){
	volatile unsigned int *gpio_regs = GPIO_REGS(bus);
	gpio_regs[3] |= 1 << port;
	gpio_regs[0xD];
	dsb();
}

int gpio_query_intr(int bus, int port){
	volatile unsigned int *gpio_regs = GPIO_REGS(bus);
	return (1 << port) & ((gpio_regs[0x0E] & ~gpio_regs[0x07]) |
			      (gpio_regs[0x0F] & ~gpio_regs[0x08]) |
			      (gpio_regs[0x10] & ~gpio_regs[0x09]) |
			      (gpio_regs[0x11] & ~gpio_regs[0x0A]) |
			      (gpio_regs[0x12] & ~gpio_regs[0x0B]));
}

int gpio_acquire_intr(int bus, int port){
	unsigned int ret;
	unsigned int mask = 1 << port;
	volatile unsigned int *gpio_regs = GPIO_REGS(bus);
	ret = mask & ((gpio_regs[0x0E] & ~gpio_regs[0x07]) |
		      (gpio_regs[0x0F] & ~gpio_regs[0x08]) |
		      (gpio_regs[0x10] & ~gpio_regs[0x09]) |
		      (gpio_regs[0x11] & ~gpio_regs[0x0A]) |
		      (gpio_regs[0x12] & ~gpio_regs[0x0B]));
	gpio_regs[0x0E] = mask;
	gpio_regs[0x0F] = mask;
	gpio_regs[0x10] = mask;
	gpio_regs[0x11] = mask;
	gpio_regs[0x12] = mask;
	dsb();
	return ret;
}

void spi_write_start(int bus){
	volatile unsigned int *spi_regs = SPI_REGS(bus);
	while (spi_regs[0xA])
		spi_regs[0];
	spi_regs[0xB];
	spi_regs[9] = 0x600;
}

void spi_write_end(int bus){
	volatile unsigned int *spi_regs = SPI_REGS(bus);
	spi_regs[2] = 0;
	spi_regs[4] = 1;
	spi_regs[4];
	dsb();
}

void spi_write(int bus, unsigned int data){
	volatile unsigned int *spi_regs = SPI_REGS(bus);
	spi_regs[1] = data;
}

int spi_read_available(int bus){
	volatile unsigned int *spi_regs = SPI_REGS(bus);
	return spi_regs[0xA];
}

int spi_read(int bus){
	volatile unsigned int *spi_regs = SPI_REGS(bus);
	return spi_regs[0];
}

void spi_read_end(int bus){
	volatile unsigned int *spi_regs = SPI_REGS(bus);
	spi_regs[4] = 0;
	spi_regs[4];
	dsb();
}
