class Binary {
	data = null;
	offset = 0;
	constructor(buffer) {
		this.data = new DataView(buffer);
	}
	bool() {
		return this.data.getUint8(this.offset++) == 1;
	}
	byte() {
		return this.data.getUint8(this.offset++);
	}
	int(bits) {
		const bytes = bits / 8;
		if (bytes > 4) {
			let value = 0n;
			for (let i = 0; i < bytes; i++) {
				const byte = BigInt(this.data.getUint8(this.offset++));
				value = (value << 8n) | byte;
			}
			const sign = 1n << (bits - 1n);
			return value & sign ? value - (1n << bits) : value;
		} else {
			let value = 0;
			for (let i = 0; i < bytes; i++) {
				const byte = this.data.getUint8(this.offset++);
				value = (value << 8) | byte;
			}
			const sign = 1 << (bits - 1);
			return value & sign ? value - (1 << bits) : value;
		}
	}
	uint(bits) {
		const bytes = bits / 8;
		if (bytes > 4) {
			let value = 0n;
			for (let i = 0; i < bytes; i++) {
				const byte = BigInt(this.data.getUint8(this.offset++));
				value = (value << 8n) | byte;
			}
			return value;
		} else {
			let value = 0;
			for (let i = 0; i < bytes; i++) {
				const byte = this.data.getUint8(this.offset++);
				value = (value << 8) | byte;
			}
			return value;
		}
	}
	hex(length) {
		const chars = [];
		while (chars.length < length) {
			const byte = this.data.getUint8(this.offset++);
			chars.push(byte.toString(16).padStart(2, '0'));
		}
		return chars.join('');
	}
	uuid() {
		const chars = [];
		for (let i = 0; i < 16; i++) {
			const byte = this.data.getUint8(this.offset++);
			chars.push(byte.toString(16).padStart(2, '0'));
		}
		return chars.join('');
	}
	string() {
		const chars = [];
		while (true) {
			const byte = this.data.getUint8(this.offset++);
			if (byte === 0) {
				break;
			}
			chars.push(String.fromCharCode(byte));
		}
		return chars.join('');
	}
}
