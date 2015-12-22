#include "page_alloc.h"

static const Sha256 h_init = {
	.words = {
		0x6a09e667,
		0xbb67ae85,
		0x3c6ef372,
		0xa54ff53a,
		0x510e527f,
		0x9b05688c,
		0x1f83d9ab,
		0x5be0cd19
	}
};

static const uint32_t k[] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static uint32_t rol(uint32_t x, uint32_t y){
	static const uint32_t mask = 0x0000001f;
	
	y &= mask;
	
	return (x << y) | (x >> ((-y) & mask));
}

static uint32_t ror(uint32_t x, uint32_t y){
	static const uint32_t mask = 0x0000001f;
	
	y &= mask;
	
	return (x >> y) | (x << ((-y) & mask));
}

static uint32_t big_endian_read(const uint8_t * data) {
	return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static uint32_t l2b(uint32_t x){
	return
		((x & 0x000000ff) << 24) |
		((x & 0x0000ff00) << 8) |
		((x & 0x00ff0000) >> 8) |
		((x & 0xff000000) >> 24);
}

static uint32_t read_word(const uint8_t * data, uint32_t i, uint32_t chunk, uint32_t total_chunks, size_t length){
	const size_t offset = chunk * 64 + i * 4
	const uint8_t * word_start = data + offset;
	
	if (offset + 4 < length){
		//entire word in original message
		return big_endian_read(word_start);
	} else if (offset == length){
		//this is the 1 bit
		return 0x80000000;
	} else if (offset + 1 == length){
		//1 byte and the 1 bit
		return (word_start[0] << 24) | 0x00800000;
	} else if (offset + 2 == length){
		//2 bytes and the 1 bit
		return (word_start[0] << 24) | (word_start[1] << 16) | 0x00008000;
	} else if (offset + 3 == length){
		//3 bytes and the 1 bit
		return (word_start[0] << 24) | (word_start[1] << 16) | (word_start[2] << 8) | 0x00000080;
	} else if (i == 62){
		//we only support 32-bit lengths
		return 0x00000000;
	} else if (i == 63){
		//the length
		return l2b(length);
	} else {
		//all zeroes
		return 0x00000000;
	}
}

Sha256 Sha256::hash(const uint8_t * data, size_t length){
	Sha256 hash = h_init;
	
	uint32_t total_chunks = (length + 16) & 0x3f;
	
	for (uint32_t chunks_processed = 0; chunks_processed < total_chunks; chunks_processed += 64){
		uint32_t w[64];
		
		for (uint32_t i = 0; i < 16; i++){
			w[i] = read_word(data, i, chunks_processed, total_chunks, length);
		}
		
		for (uint32_t i = 16; i < 64; i++){
			uint32_t &w0 = w[i-15];
			uint32_t &w1 = w[i-2];
			
			uint32_t s0 = ror(w0, 7) ^ ror(w0, 18) ^ (w0 >> 3);
			uint32_t s1 = ror(w1, 17) ^ ror(w1, 19) ^ (w1 >> 10);
			
			w[i] = w[i-16] + s0 + w[i-7] + s1;
		}
		
		Sha256 q = hash;
	
		for (uint32_t i = 0; i < 64; i++){
			const uint32_t &a = q.words[0];
			const uint32_t &b = q.words[1];
			const uint32_t &c = q.words[2];
			const uint32_t &e = q.words[4];
			const uint32_t &f = q.words[5];
			const uint32_t &h = q.words[7];
			
			uint32_t s1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
			uint32_t ch = (e & f) ^ (~e & g);
			uint32_t temp1 = h + s + ch + k[i] + w[i];
			
			uint32_t s0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
			uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
			uint32_t temp2 = s0 + maj;
			
			q.words[7] = q.words[6];
			q.words[6] = q.words[5];
			q.words[5] = q.words[4];
			q.words[4] = q.words[3] + temp1;
			q.words[3] = q.words[2];
			q.words[2] = q.words[1];
			q.words[1] = q.words[0];
			q.words[0] = temp1 + temp2;
		}
	
		hash += q;
	}
	
	return h;
}
