const fade = (element) => {
	for (let ind = element.classList.length - 1; ind >= 0; ind--) {
		const cls = element.classList[ind];
		if (
			(cls.startsWith('text-') && cls.endsWith('0')) ||
			(cls.startsWith('dark:text-') && cls.endsWith('0')) ||
			(cls.startsWith('background-') && cls.endsWith('0')) ||
			(cls.startsWith('dark:background-') && cls.endsWith('0'))
		) {
			element.classList.remove(cls);
		}
	}
};
