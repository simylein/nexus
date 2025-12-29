const colorRssi = (element, rssi, sf) => {
	const floor = -116 - (sf - 6) * 2.25;
	const width = 9 + (sf - 6) * 0.25;
	switch (true) {
		case rssi < floor + width * 1:
			return element.classList.add('text-red-600', 'dark:text-red-400', 'background-red-50', 'dark:background-red-950');
		case rssi >= floor + width * 1 && rssi < floor + width * 2:
			return element.classList.add('text-orange-600', 'dark:text-orange-400', 'background-orange-50', 'dark:background-orange-950');
		case rssi >= floor + width * 2 && rssi < floor + width * 3:
			return element.classList.add('text-amber-600', 'dark:text-amber-400', 'background-amber-50', 'dark:background-amber-950');
		case rssi >= floor + width * 3 && rssi < floor + width * 4:
			return element.classList.add('text-yellow-600', 'dark:text-yellow-400', 'background-yellow-50', 'dark:background-yellow-950');
		case rssi >= floor + width * 4 && rssi < floor + width * 5:
			return element.classList.add('text-lime-600', 'dark:text-lime-400', 'background-lime-50', 'dark:background-lime-950');
		case rssi >= floor + width * 5:
			return element.classList.add('text-green-600', 'dark:text-green-400', 'background-green-50', 'dark:background-green-950');
	}
};

const colorSnr = (element, snr, sf) => {
	const floor = -3 - (sf - 6) * 2.25;
	const width = 2 + (sf - 6) * 0.25;
	switch (true) {
		case snr < floor + width * 1:
			return element.classList.add('text-red-600', 'dark:text-red-400', 'background-red-50', 'dark:background-red-950');
		case snr >= floor + width * 1 && snr < floor + width * 2:
			return element.classList.add('text-orange-600', 'dark:text-orange-400', 'background-orange-50', 'dark:background-orange-950');
		case snr >= floor + width * 2 && snr < floor + width * 3:
			return element.classList.add('text-amber-600', 'dark:text-amber-400', 'background-amber-50', 'dark:background-amber-950');
		case snr >= floor + width * 3 && snr < floor + width * 4:
			return element.classList.add('text-yellow-600', 'dark:text-yellow-400', 'background-yellow-50', 'dark:background-yellow-950');
		case snr >= floor + width * 4 && snr < floor + width * 5:
			return element.classList.add('text-lime-600', 'dark:text-lime-400', 'background-lime-50', 'dark:background-lime-950');
		case snr >= floor + width * 5:
			return element.classList.add('text-green-600', 'dark:text-green-400', 'background-green-50', 'dark:background-green-950');
	}
};
