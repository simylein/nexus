const report = (err) => {
	const text = (err) => {
		if (err.statusText) {
			return err.statusText;
		}
		switch (err.status) {
			case 400:
				return 'Bad Request';
			case 401:
				return 'Unauthorized';
			case 403:
				return 'Forbidden';
			case 404:
				return 'Not Found';
			case 405:
				return 'Method Not Allowed';
			case 414:
				return 'URI Too Long';
			case 431:
				return 'Request Header Fields Too Large';
			case 500:
				return 'Internal Server Error';
			case 502:
				return 'Bad Gateway';
			case 503:
				return 'Service Unavailable';
			case 505:
				return 'HTTP Version Not Supported';
			case 507:
				return 'Insufficient Storage';
			default:
				return null;
		}
	};

	if (err instanceof TypeError) {
		return notification('info', 'Seems like you might be offline');
	}
	const message = `Request failed with ${err.status ?? 0} ${text(err) ?? null}`;
	if (err.status < 500) {
		return notification('warning', message);
	}
	return notification('error', message);
};
