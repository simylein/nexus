const getLimit = () => {
	const limit = new URLSearchParams(window.location.search).get('limit');
	return +(limit ?? 16);
};

const setLimit = (limit) => {
	const params = new URLSearchParams(window.location.search);
	params.set('limit', limit);
	window.history.replaceState({}, '', `${window.location.pathname}?${params.toString()}`);
};

const getOffset = () => {
	const offset = new URLSearchParams(window.location.search).get('offset');
	return +(offset ?? 0);
};

const setOffset = (offset) => {
	const params = new URLSearchParams(window.location.search);
	params.set('offset', offset);
	window.history.replaceState({}, '', `${window.location.pathname}?${params.toString()}`);
};
