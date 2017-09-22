#ifndef _SHA_H_
#define _SHA_H_
#include <stdint.h>

#define METHOD_SHA1   1
#define METHOD_SHA256 2

struct sha_ctx
{
	int hashsize;
	void (*transform)(struct sha_ctx *, uint32_t []);
	uint32_t state[8];
	uint8_t data[64];
	uint32_t datalen;
};

struct sha_initial_ctx
{
	int hashsize;
	void (*transform)(struct sha_ctx *, uint32_t []);
	uint32_t state[8];
};

struct hmac_data
{
	const u8 *secret;
	u32 secretLen;
	const u8 *msg;
	u32 msgLen;
	u8 *hash;
};

int  sha_init(struct sha_ctx *ctx, int method);
void sha_update(struct sha_ctx *ctx, const uint8_t* data, unsigned int len);
void sha_final(struct sha_ctx *ctx, uint8_t *digest);

void hmac_sign(struct sha_ctx *ctx, int method, struct hmac_data *hm);

#endif
