const loading = (element, add, remove, text, height, width) => {
	element.classList.add(...add, 'background-neutral-200', 'dark:background-neutral-800');
	element.classList.remove('w-fit', 'background-red-200', 'dark:background-red-800', ...remove);
	if (text !== undefined && text !== null) {
		element.innerText = text;
	}
	if (height !== undefined && height !== null) {
		element.style.height = `${height}%`;
	}
	if (width !== undefined && width !== null) {
		element.style.width = `${width}%`;
	}
	element.onclick = null;
};

const paint = (element, add, remove, text, height, width) => {
	element.classList.add(...add, 'w-fit');
	element.classList.remove('background-neutral-200', 'dark:background-neutral-800', 'background-red-200', 'dark:background-red-800', ...remove);
	if (text !== undefined && text !== null) {
		element.innerText = text;
	}
	if (height !== undefined && height !== null) {
		element.style.height = `${height}%`;
	}
	if (width !== undefined && width !== null) {
		element.style.width = `${width}%`;
	}
};

const error = (element, add, remove, text, height, width) => {
	element.classList.add(...add, 'background-red-200', 'dark:background-red-800');
	element.classList.remove('w-fit', 'background-neutral-200', 'dark:background-neutral-800', ...remove);
	if (text !== undefined && text !== null) {
		element.innerText = text;
	}
	if (height !== undefined && height !== null) {
		element.style.height = `${height}%`;
	}
	if (width !== undefined && width !== null) {
		element.style.width = `${width}%`;
	}
	element.onclick = null;
};
