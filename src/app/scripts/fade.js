const fade = (element) => {
	for (let ind = element.classList.length - 1; ind >= 0; ind--) {
		const cls = element.classList[ind];
		if (cls.startsWith('text-') || cls.startsWith('dark:text-') || cls.startsWith('background-') || cls.startsWith('dark:background-')) {
			element.classList.remove(cls);
		}
	}
};
