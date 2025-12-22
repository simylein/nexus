const colorPreambleLen = (element, preambleLen) => {
	switch (true) {
		case preambleLen < 8:
			return element.classList.add('text-green-600', 'dark:text-green-400', 'background-green-50', 'dark:background-green-950');
		case preambleLen >= 8 && preambleLen < 11:
			return element.classList.add('text-emerald-600', 'dark:text-emerald-400', 'background-emerald-50', 'dark:background-emerald-950');
		case preambleLen >= 11 && preambleLen < 14:
			return element.classList.add('text-teal-600', 'dark:text-teal-400', 'background-teal-50', 'dark:background-teal-950');
		case preambleLen >= 14 && preambleLen < 16:
			return element.classList.add('text-cyan-600', 'dark:text-cyan-400', 'background-cyan-50', 'dark:background-cyan-950');
		case preambleLen >= 16 && preambleLen < 18:
			return element.classList.add('text-sky-600', 'dark:text-sky-400', 'background-sky-50', 'dark:background-sky-950');
		case preambleLen >= 18 && preambleLen < 20:
			return element.classList.add('text-blue-600', 'dark:text-blue-400', 'background-blue-50', 'dark:background-blue-950');
		case preambleLen >= 20:
			return element.classList.add('text-indigo-600', 'dark:text-indigo-400', 'background-indigo-50', 'dark:background-indigo-950');
	}
};
