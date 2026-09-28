#include "../lib/config.h"
#include "../lib/error.h"
#include "../lib/logger.h"
#include <fcntl.h>
#include <linux/gpio.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

int gpio_init_edge(const char *device, uint8_t pin) {
	int fd = open(device, O_RDONLY);

	if (fd == -1) {
		error("failed to open %s because %s\n", device, errno_str());
		return -1;
	}

	struct gpioevent_request req = {
			.lineoffset = pin,
			.handleflags = GPIOHANDLE_REQUEST_INPUT,
			.eventflags = GPIOEVENT_REQUEST_RISING_EDGE,
	};
	sprintf(req.consumer_label, "%s-gpio%hhu", name, pin);

	if (ioctl(fd, GPIO_GET_LINEEVENT_IOCTL, &req) == -1) {
		error("failed to get gpio event because %s\n", errno_str());
		return -1;
	}

	if (close(fd) == -1) {
		error("failed to close %s because %s\n", device, errno_str());
		return -1;
	}

	return req.fd;
}

int gpio_init_output(const char *device, uint8_t pin, uint8_t value) {
	int fd = open(device, O_RDONLY);

	if (fd == -1) {
		error("failed to open %s because %s\n", device, errno_str());
		return -1;
	}

	struct gpiohandle_request req = {
			.lines = 1,
			.lineoffsets = {pin},
			.flags = GPIOHANDLE_REQUEST_OUTPUT,
			.default_values = {value},
	};

	sprintf(req.consumer_label, "%s-gpio%hhu", name, pin);

	if (ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req) == -1) {
		error("failed to get gpio output because %s\n", errno_str());
		return -1;
	}

	if (close(fd) == -1) {
		error("failed to close %s because %s\n", device, errno_str());
		return -1;
	}

	return req.fd;
}

int gpio_wait_edge(int gpio_fd) {
	struct gpioevent_data event;
	if (read(gpio_fd, &event, sizeof(event)) != sizeof(event)) {
		error("failed to read gpio edge event because %s\n", errno_str());
		return -1;
	}

	return 0;
}
