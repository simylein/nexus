const colorChecksum = (element, checksum) => {
	switch (true) {
		case checksum === false:
			return element.classList.add('text-red-600', 'dark:text-red-400', 'background-red-50', 'dark:background-red-950');
		case checksum === true:
			return element.classList.add('text-green-600', 'dark:text-green-400', 'background-green-50', 'dark:background-green-950');
	}
};
