#include "main.h"

static const char HEXDIGITS[16] = "0123456789abcdef";

void data2hex(uint8_t *data, int len, char *buf)
{
	char *p = buf;
	int i;

	for (i=0; i<len; i++) {
		*p++ = HEXDIGITS[(data[i] >> 4) & 15];
		*p++ = HEXDIGITS[data[i] & 15];
	}
	*p = 0;
}
