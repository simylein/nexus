const colorCr = (element, cr) => {
	switch (true) {
		case cr === 5:
			return element.classList.add('text-green-600', 'dark:text-green-400', 'background-green-50', 'dark:background-green-950');
		case cr === 6:
			return element.classList.add('text-teal-600', 'dark:text-teal-400', 'background-teal-50', 'dark:background-teal-950');
		case cr === 7:
			return element.classList.add('text-sky-600', 'dark:text-sky-400', 'background-sky-50', 'dark:background-sky-950');
		case cr === 8:
			return element.classList.add('text-indigo-600', 'dark:text-indigo-400', 'background-indigo-50', 'dark:background-indigo-950');
	}
};
