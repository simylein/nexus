#pragma once

#include <stdbool.h>
#include <stdint.h>

int sx1278_sleep(int fd);
int sx1278_standby(int fd);

int sx1278_frequency(int fd, uint32_t frequency);
int sx1278_tx_power(int fd, uint8_t power);
int sx1278_coding_rate(int fd, uint8_t cr);
int sx1278_bandwidth(int fd, uint32_t bandwidth);
int sx1278_spreading_factor(int fd, uint8_t sf);
int sx1278_checksum(int fd, bool crc);
int sx1278_sync_word(int fd, uint8_t word);

int sx1278_snr(int fd, int8_t *snr);
int sx1278_rssi(int fd, int16_t *rssi);

int sx1278_transmit(int fd, uint8_t (*data)[256], uint8_t length);
int sx1278_receive(int fd, uint8_t (*data)[256], uint8_t *length);
