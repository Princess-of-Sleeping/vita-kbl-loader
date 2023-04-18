/*
 * syscon.c
 * from vita-libbaremetal
 */

#include <inttypes.h>
// #include "nsbl.h"
// #include "functions.h"
#include "syscon.h"
#include "lowio.h"


extern void *(* memset)(void *dst, int ch, int n);
extern void *(* memcpy)(void *dst, const void *src, int n);


#define SYSCON_TX_CMD_LO       0
#define SYSCON_TX_CMD_HI       1
#define SYSCON_TX_LENGTH       2
#define SYSCON_TX_DATA(i)      (3 + (i))
#define SYSCON_RX_STATUS_LO    0
#define SYSCON_RX_STATUS_HI    1
#define SYSCON_RX_LENGTH       2
#define SYSCON_RX_RESULT       3

typedef struct SceSysconPacket {
	unsigned char tx[32];
	unsigned char rx[32];
} SceSysconPacket;

static void syscon_packet_start(SceSysconPacket *packet){
	int i = 0;
	unsigned char cmd_size = packet->tx[2];
	unsigned char tx_total_size = cmd_size + 3;
	unsigned int offset;
	(void)offset;
	gpio_port_clear(0, GPIO_PORT_SYSCON_OUT);
	spi_write_start(0);
	if (cmd_size <= 29) {
		offset = 2;
	}
	do {
		spi_write(0, (packet->tx[i + 1] << 8) | packet->tx[i]);
		i += 2;
	} while (i < tx_total_size);
	spi_write_end(0);
	gpio_port_set(0, GPIO_PORT_SYSCON_OUT);
}

static unsigned char syscon_cmd_sync(SceSysconPacket *packet){

	int i = 0;

	while (!gpio_query_intr(0, GPIO_PORT_SYSCON_IN))
		;

	gpio_acquire_intr(0, GPIO_PORT_SYSCON_IN);

	while (spi_read_available(0)) {
		unsigned int data = spi_read(0);
		packet->rx[i + 0] = (data >> 0) & 0xFF;
		packet->rx[i + 1] = (data >> 8) & 0xFF;
		i += 2;
	}

	spi_read_end(0);
	gpio_port_clear(0, GPIO_PORT_SYSCON_OUT);

	return packet->rx[SYSCON_RX_RESULT];
}

void syscon_common_read(unsigned int *buffer, unsigned short cmd){

	SceSysconPacket packet;

	packet.tx[SYSCON_TX_CMD_LO] = (cmd >> 0) & 0xFF;
	packet.tx[SYSCON_TX_CMD_HI] = (cmd >> 8) & 0xFF;
	packet.tx[SYSCON_TX_LENGTH] = 1;

	memset(packet.rx, -1, sizeof(packet.rx));

	syscon_packet_start(&packet);
	syscon_cmd_sync(&packet);

	memcpy(buffer, &packet.rx[4], packet.rx[SYSCON_RX_LENGTH] - 2);
}

void syscon_common_write(unsigned int data, unsigned short cmd, unsigned int length){

	unsigned int i;
	unsigned char hash, result;
	SceSysconPacket packet;

	packet.tx[SYSCON_TX_CMD_LO] = (cmd >> 0) & 0xFF;
	packet.tx[SYSCON_TX_CMD_HI] = (cmd >> 8) & 0xFF;
	packet.tx[SYSCON_TX_LENGTH] = length;

	packet.tx[SYSCON_TX_DATA(0)] = data & 0xFF;
	packet.tx[SYSCON_TX_DATA(1)] = (data >> 8) & 0xFF;
	packet.tx[SYSCON_TX_DATA(2)] = (data >> 16) & 0xFF;
	packet.tx[SYSCON_TX_DATA(3)] = (data >> 24) & 0xFF;

	hash = 0;
	for (i = 0; i < length + 2; i++)
		hash += packet.tx[i];

	packet.tx[2 + length] = ~hash;

	memset(&packet.tx[SYSCON_TX_DATA(length)], -1, sizeof(packet.rx) - SYSCON_TX_DATA(length));

	do {
		memset(packet.rx, -1, sizeof(packet.rx));
		syscon_packet_start(&packet);

		result = syscon_cmd_sync(&packet);
	} while(result == 0x80 || result == 0x81);
}

void syscon_ctrl_read(unsigned int *data){
	syscon_common_read(data, 0x101); // or 0x104
}
