const resize = (element, length) => {
	let index = 0;
	while (element.children.length !== length) {
		const diff = element.children.length - length;
		if (diff < 0) {
			const copy = element.children[0].cloneNode(true);
			element.appendChild(copy);
		} else if (diff > 0 && element.children.length > 0) {
			element.children[element.children.length - 1].remove();
		}
		index++;
	}
};
