/*
 * lowio.h
 */
#ifndef _NSKBL_LOWIO_H_
#define _NSKBL_LOWIO_H_

#define dmb() asm volatile("dmb\n\t")
#define dsb() asm volatile("dsb\n\t")

#define GPIO_PORT_MODE_INPUT   0
#define GPIO_PORT_MODE_OUTPUT  1
#define GPIO_PORT_OLED         0
#define GPIO_PORT_SYSCON_OUT   3
#define GPIO_PORT_SYSCON_IN    4
#define GPIO_PORT_GAMECARD_LED 6
#define GPIO_PORT_PS_LED       7
#define GPIO_PORT_HDMI_BRIDGE  15
#define GPIO0_BASE_ADDR        0xE20A0000
#define GPIO1_BASE_ADDR        0xE0100000
#define GPIO_REGS(i)           ((void *)((i) == 0 ? GPIO0_BASE_ADDR : GPIO1_BASE_ADDR))

#define SPI_BASE_ADDR          0xE0A00000
#define SPI_REGS(i)            ((void *)(SPI_BASE_ADDR + (i) * 0x10000))

void gpio_port_set(int bus, int port);
void gpio_port_clear(int bus, int port);

int gpio_query_intr(int bus, int port);
int gpio_acquire_intr(int bus, int port);

void spi_write_start(int bus);
void spi_write_end(int bus);
void spi_write(int bus, unsigned int data);

int spi_read_available(int bus);
int spi_read(int bus);
void spi_read_end(int bus);

#endif
