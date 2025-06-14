#include "error.h"
#include "logger.h"
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <sys/ioctl.h>

int spi_init(const char *device, const uint8_t mode, const uint32_t speed, const uint8_t word_len) {
	int fd = open(device, O_RDWR);

	if (fd == -1) {
		error("failed to open %s because %s\n", device, errno_str());
		return -1;
	}

	if (ioctl(fd, SPI_IOC_WR_MODE, &mode) == -1) {
		error("failed to set spi mode because %s\n", errno_str());
		return -1;
	}

	if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
		error("failed to set spi speed because %s\n", errno_str());
		return -1;
	}

	if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &word_len) == -1) {
		error("failed to set spi word len because %s\n", errno_str());
		return -1;
	}

	return fd;
}

int spi_read_register(int fd, uint8_t reg, uint8_t *value) {
	uint8_t tx_buf[2] = {reg & 0x7f, 0x00};
	uint8_t rx_buf[2];

	struct spi_ioc_transfer transfer = {
			.tx_buf = (uint64_t)tx_buf,
			.rx_buf = (uint64_t)rx_buf,
			.len = sizeof(tx_buf),
	};

	if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) == -1) {
		error("failed to read register %02x because %s\n", reg, errno_str());
		return -1;
	}

	*value = rx_buf[1];
	return 0;
}

int spi_write_register(int fd, uint8_t reg, uint8_t value) {
	uint8_t tx_buf[2] = {reg | 0x80, value};
	uint8_t rx_buf[2];

	struct spi_ioc_transfer transfer = {
			.tx_buf = (uint64_t)tx_buf,
			.rx_buf = (uint64_t)rx_buf,
			.len = sizeof(tx_buf),
	};

	if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) == -1) {
		error("failed to write register %02x because %s\n", reg, errno_str());
		return -1;
	}

	return 0;
}
