#include "format.h"
#include "logger.h"
#include "spi.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

const uint8_t reg_fifo = 0x00;
const uint8_t reg_op_mode = 0x01;
const uint8_t reg_frf_msb = 0x06;
const uint8_t reg_frf_mid = 0x07;
const uint8_t reg_frf_lsb = 0x08;
const uint8_t reg_pa_config = 0x09;
const uint8_t reg_fifo_addr = 0x0d;
const uint8_t reg_tx_addr = 0x0e;
const uint8_t reg_rx_addr = 0x10;
const uint8_t reg_irq_flags = 0x12;
const uint8_t reg_packet_len = 0x13;
const uint8_t reg_packet_snr = 0x19;
const uint8_t reg_packet_rssi = 0x1a;
const uint8_t reg_payload_len = 0x22;
const uint8_t reg_modem_config_1 = 0x1d;
const uint8_t reg_modem_config_2 = 0x1e;
const uint8_t reg_sync_word = 0x39;

int sx1278_sleep(int fd) {
	if (spi_write_register(fd, reg_op_mode, 0x80) == -1) {
		return -1;
	};

	uint8_t op_mode;
	while (true) {
		if (spi_read_register(fd, reg_op_mode, &op_mode) == -1) {
			return -1;
		};
		if ((op_mode & 0x07) == 0x00) {
			break;
		}
		usleep(500);
	}

	trace("sleep op_mode 0x%02x\n", op_mode);
	return 0;
}

int sx1278_standby(int fd) {
	if (spi_write_register(fd, reg_op_mode, 0x81) == -1) {
		return -1;
	};

	uint8_t op_mode;
	while (true) {
		if (spi_read_register(fd, reg_op_mode, &op_mode) == -1) {
			return -1;
		};
		if ((op_mode & 0x07) == 0x01) {
			break;
		}
		usleep(500);
	}

	trace("standby op_mode 0x%02x\n", op_mode);
	return 0;
}

int sx1278_tx(int fd) {
	if (spi_write_register(fd, reg_op_mode, 0x83) == -1) {
		return -1;
	};

	uint8_t op_mode;
	while (true) {
		if (spi_read_register(fd, reg_op_mode, &op_mode) == -1) {
			return -1;
		};
		if ((op_mode & 0x07) == 0x03) {
			break;
		}
		usleep(500);
	}

	trace("transmit op_mode 0x%02x\n", op_mode);
	return 0;
}

int sx1278_rx(int fd) {
	if (spi_write_register(fd, reg_op_mode, 0x85) == -1) {
		return -1;
	};

	uint8_t op_mode;
	while (true) {
		if (spi_read_register(fd, reg_op_mode, &op_mode) == -1) {
			return -1;
		};
		if ((op_mode & 0x07) == 0x05) {
			break;
		}
		usleep(500);
	}

	trace("receive op_mode 0x%02x\n", op_mode);
	return 0;
}

int sx1278_frequency(int fd, uint32_t frequency) {
	uint32_t frf = (uint32_t)(frequency * (1ull << 19) / (32 * 1000 * 1000));

	if (spi_write_register(fd, reg_frf_msb, (frf >> 16) & 0xff) == -1) {
		return -1;
	}
	if (spi_write_register(fd, reg_frf_mid, (frf >> 8) & 0xff) == -1) {
		return -1;
	}
	if (spi_write_register(fd, reg_frf_lsb, frf & 0xff) == -1) {
		return -1;
	}

	uint8_t frf_msb;
	uint8_t frf_mid;
	uint8_t frf_lsb;
	if (spi_read_register(fd, reg_frf_msb, &frf_msb) == -1) {
		return -1;
	}
	if (spi_read_register(fd, reg_frf_mid, &frf_mid) == -1) {
		return -1;
	}
	if (spi_read_register(fd, reg_frf_lsb, &frf_lsb) == -1) {
		return -1;
	}

	trace("frequency %uhz frf 0x%02x%02x%02x\n", frequency, frf_msb, frf_mid, frf_lsb);
	return 0;
}

int sx1278_tx_power(int fd, uint8_t power) {
	if (power < 2 || power > 17) {
		error("tx power must be between %d and %d\n", 2, 17);
		return -1;
	}

	uint8_t pa_config = 0x80 | (power - 2);
	if (spi_write_register(fd, reg_pa_config, pa_config) == -1) {
		return -1;
	}

	uint8_t value;
	if (spi_read_register(fd, reg_pa_config, &value) == -1) {
		return -1;
	}

	trace("tx power %hhudbm pa_config 0x%02x\n", power, value);
	return 0;
}

int sx1278_coding_rate(int fd, uint8_t cr) {
	if (cr < 5 || cr > 8) {
		error("coding rate must be between %d and %d\n", 5, 8);
		return -1;
	}

	uint8_t modem_config_1;
	if (spi_read_register(fd, reg_modem_config_1, &modem_config_1) == -1) {
		return -1;
	}

	modem_config_1 = (uint8_t)((modem_config_1 & 0xf1) | ((cr - 4) << 1));

	if (spi_write_register(fd, reg_modem_config_1, modem_config_1) == -1) {
		return -1;
	}

	uint8_t value;
	if (spi_read_register(fd, reg_modem_config_1, &value) == -1) {
		return -1;
	}

	trace("coding rate 4/%d modem_config_1 0x%02x\n", cr, value);
	return 0;
}

int sx1278_bandwidth(int fd, uint32_t bandwidth) {
	uint8_t bw_bits;

	switch (bandwidth) {
	case 7800:
		bw_bits = 0x0;
		break;
	case 10400:
		bw_bits = 0x1;
		break;
	case 15600:
		bw_bits = 0x2;
		break;
	case 20800:
		bw_bits = 0x3;
		break;
	case 31250:
		bw_bits = 0x4;
		break;
	case 41700:
		bw_bits = 0x5;
		break;
	case 62500:
		bw_bits = 0x6;
		break;
	case 125000:
		bw_bits = 0x7;
		break;
	case 250000:
		bw_bits = 0x8;
		break;
	case 500000:
		bw_bits = 0x9;
		break;
	default:
		error("bandwidth must be one of 7800 10400 15600 20800 31250 41700 62500 125000 250000 500000\n");
		return -1;
	}

	uint8_t modem_config_1;
	if (spi_read_register(fd, reg_modem_config_1, &modem_config_1) == -1) {
		return -1;
	}

	modem_config_1 = (modem_config_1 & 0x0f) | (bw_bits << 4);

	if (spi_write_register(fd, reg_modem_config_1, modem_config_1) == -1) {
		return -1;
	}

	uint8_t value;
	if (spi_read_register(fd, reg_modem_config_1, &value) == -1) {
		return -1;
	}

	trace("bandwidth %uhz modem_config_1 0x%02x\n", bandwidth, value);
	return 0;
}

int sx1278_spreading_factor(int fd, uint8_t sf) {
	if (sf < 6 || sf > 12) {
		error("spreading factor must be between %d and %d\n", 6, 12);
		return -1;
	}

	uint8_t modem_config_2;
	if (spi_read_register(fd, reg_modem_config_2, &modem_config_2) == -1) {
		return -1;
	}

	modem_config_2 = (modem_config_2 & 0x0f) | (sf << 4);

	if (spi_write_register(fd, reg_modem_config_2, modem_config_2) == -1) {
		return -1;
	}

	uint8_t value;
	if (spi_read_register(fd, reg_modem_config_2, &value) == -1) {
		return -1;
	}

	trace("spreading factor %hhu modem_config_2 0x%02x\n", sf, value);
	return 0;
}

int sx1278_checksum(int fd, bool crc) {
	uint8_t modem_config_2;
	if (spi_read_register(fd, reg_modem_config_2, &modem_config_2) == -1) {
		return -1;
	}

	modem_config_2 = (modem_config_2 & (uint8_t)~0x04) | ((crc & 1) << 2);

	if (spi_write_register(fd, reg_modem_config_2, modem_config_2) == -1) {
		return -1;
	}

	uint8_t value;
	if (spi_read_register(fd, reg_modem_config_2, &value) == -1) {
		return -1;
	}

	trace("checksum %s modem_config_2 0x%02x\n", human_bool(crc), value);
	return 0;
}

int sx1278_sync_word(int fd, uint8_t word) {
	if (spi_write_register(fd, reg_sync_word, word) == -1) {
		return -1;
	};

	uint8_t sync_word;
	if (spi_read_register(fd, reg_sync_word, &sync_word) == -1) {
		return -1;
	}

	trace("sync word %hhu sync_word 0x%02x\n", word, sync_word);
	return 0;
}

int sx1278_snr(int fd, int8_t *snr) {
	uint8_t packet_snr;
	if (spi_read_register(fd, reg_packet_snr, &packet_snr) == -1) {
		return -1;
	}

	*snr = (int8_t)packet_snr;

	trace("packet_snr 0x%02x\n", packet_snr);
	return 0;
}

int sx1278_rssi(int fd, int16_t *rssi) {
	uint8_t packet_rssi;
	if (spi_read_register(fd, reg_packet_rssi, &packet_rssi) == -1) {
		return -1;
	}

	*rssi = -157 + packet_rssi;

	trace("packet_rssi 0x%02x\n", packet_rssi);
	return 0;
}

int sx1278_transmit(int fd, uint8_t (*data)[256], uint8_t length) {
	if (spi_write_register(fd, reg_fifo_addr, 0x80) == -1) {
		return -1;
	}

	if (spi_write_register(fd, reg_tx_addr, 0x80) == -1) {
		return -1;
	}
	char buffer[512];
	uint16_t buffer_len = 0;
	for (uint8_t index = 0; index < length; index++) {
		if (spi_write_register(fd, reg_fifo, (*data)[index]) == -1) {
			return -1;
		}
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%02x", (*data)[index]);
	}

	if (spi_write_register(fd, reg_payload_len, length) == -1) {
		return -1;
	}

	if (sx1278_tx(fd) == -1) {
		return -1;
	}

	uint8_t irq_flags;
	while (true) {
		if (spi_read_register(fd, reg_irq_flags, &irq_flags) == -1) {
			return -1;
		};
		if (irq_flags & 0x08) {
			trace("transmitting completed irq_flags 0x%02x\n", irq_flags);
			break;
		}
		usleep(500);
	}
	trace("transmitted data %.*s\n", buffer_len, buffer);

	if (spi_write_register(fd, reg_irq_flags, 0xff) == -1) {
		return -1;
	}

	if (spi_read_register(fd, reg_irq_flags, &irq_flags) == -1) {
		return -1;
	};

	trace("acknowledged irq_flags 0x%02x\n", irq_flags);
	return 0;
}

int sx1278_receive(int fd, uint8_t (*data)[256], uint8_t *length) {
	if (sx1278_rx(fd) == -1) {
		return -1;
	}

	uint8_t irq_flags;
	while (true) {
		if (spi_read_register(fd, reg_irq_flags, &irq_flags) == -1) {
			return -1;
		};
		if (irq_flags & 0x40) {
			trace("receiving completed irq_flags 0x%02x\n", irq_flags);
			break;
		}
		usleep(500);
	}

	uint8_t rx_addr;
	if (spi_read_register(fd, reg_rx_addr, &rx_addr) == -1) {
		return -1;
	}

	uint8_t packet_len;
	if (spi_read_register(fd, reg_packet_len, &packet_len) == -1) {
		return -1;
	}

	if (spi_write_register(fd, reg_fifo_addr, rx_addr) == -1) {
		return -1;
	}

	char buffer[512];
	uint16_t buffer_len = 0;
	for (uint8_t index = 0; index < packet_len; index++) {
		if (spi_read_register(fd, reg_fifo, &(*data)[index]) == -1) {
			return -1;
		}
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%02x", (*data)[index]);
	}
	trace("received data %.*s\n", buffer_len, buffer);

	*length = packet_len;

	if (irq_flags & 0x20) {
		warn("checksum failed discarding packet length %hhu\n", packet_len);
		*length = 0;
	}

	if (spi_write_register(fd, reg_irq_flags, 0xff) == -1) {
		return -1;
	}

	if (spi_read_register(fd, reg_irq_flags, &irq_flags) == -1) {
		return -1;
	};

	trace("acknowledged irq_flags 0x%02x\n", irq_flags);
	return 0;
}
