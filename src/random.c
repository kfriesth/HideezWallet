#include "main.h"

void get_random_bytes(void *data, int len)
{
	int clen = 0;
	memset(data, 0, len);
	while (clen < len) {
		u8 avl = 0;
		sd_rand_application_bytes_available_get(&avl);
		if (avl > len-clen) avl = len-clen;
		sd_rand_application_vector_get((u8 *)data+clen, avl);
		clen += avl;
	}
}

uint32_t random32(void)
{
	uint32_t v;
	get_random_bytes(&v, sizeof(uint32_t));
	return v;
}

