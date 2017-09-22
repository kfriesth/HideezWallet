#include "main.h"

// DBL_INT_ADD treats two unsigned ints a and b as one 64-bit integer and adds c to it
#define DBL_INT_ADD(a,b,c) if ((a) > 0xffffffff - (c)) ++(b); (a) += (c);
#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

static void sha1_transform(struct sha_ctx *ctx, uint32_t v[]);
static void sha256_transform(struct sha_ctx *ctx, uint32_t v[]);

static const struct sha_initial_ctx sha256_initial_ctx = {
	32, sha256_transform, { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 }
};
static const struct sha_initial_ctx sha1_initial_ctx = {
	20, sha1_transform, { 0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0 }
};

static const u32 k[64] =
{
   0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
   0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
   0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
   0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
   0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
   0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
   0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
   0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void brev(void *d, const void *s, int n)
{
	int i;
	for (i=0; i<n; i++) {
		u32 v = ((u32 *)s)[i];
		((u32 *)d)[i] = (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
	}
}

static void sha256_transform(struct sha_ctx *ctx, uint32_t v[])
{  
	u32 i,t1,t2;
	u32 *m = (u32 *)(ctx->data);

		for (i=0; i<64; ++i) {
			t1 = v[7] + EP1(v[4]) + CH(v[4],v[5],v[6]) + k[i] + m[i%16];
			t2 = EP0(v[0]) + MAJ(v[0],v[1],v[2]);
			v[7] = v[6];
			v[6] = v[5];
			v[5] = v[4];
			v[4] = v[3] + t1;
			v[3] = v[2];
			v[2] = v[1];
			v[1] = v[0];
			v[0] = t1 + t2;
			m[i%16] = SIG1(m[(i+16-2)%16]) + m[(i+16-7)%16] + SIG0(m[(i+16-15)%16]) + m[i%16];
		}

}

static void sha1_transform(struct sha_ctx *ctx, uint32_t v[])
{
    unsigned int i;
		uint32_t *m = (uint32_t *)(ctx->data);

  	for(i=0; i<80; i++) {
        int t;
				t = m[i%16] + v[4] + rol(v[0],5);
        if(i<40){
            if(i<20) t+= ((v[1]&(v[2]^v[3]))^v[3])    +0x5A827999;
            else     t+= ( v[1]^v[2]     ^v[3])    +0x6ED9EBA1;
        }else{
            if(i<60) t+= (((v[1]|v[2])&v[3])|(v[1]&v[2]))+0x8F1BBCDC;
            else     t+= ( v[1]^v[2]     ^v[3])    +0xCA62C1D6;
        }
        v[4]= v[3];
        v[3]= v[2];
        v[2]= rol(v[1],30);
        v[1]= v[0];
        v[0]= t;
        m[i%16]= rol(m[(i+16-3)%16]^m[(i+16-8)%16]^m[(i+16-14)%16]^m[i%16],1);
    }

}

int sha_init(struct sha_ctx *ctx, int method)
{
	const struct sha_initial_ctx *ictx = (method == METHOD_SHA1) ? &sha1_initial_ctx : &sha256_initial_ctx;
	umemcpy(ctx, ictx, sizeof(struct sha_initial_ctx));
	ctx->datalen = 0; 
	return sizeof(struct sha_ctx);
}

void sha_update(struct sha_ctx *ctx, const uint8_t* data, unsigned int len)
{
    unsigned int i, j, k;
		uint32_t v[8];

    j = ctx->datalen & 63;
    ctx->datalen += len;

  	for( i = 0; i < len; i++ ){
        ctx->data[ j++ ] = data[i];
        if( 64 == j ){
						umemcpy(v, ctx->state, ctx->hashsize);
						brev(ctx->data, ctx->data, 16);
						ctx->transform(ctx, v);
						for (k=0; k<ctx->hashsize/4; k++) {
							ctx->state[k] += v[k];
						}
            j = 0;
        }
    }
}

void sha_final(struct sha_ctx *ctx, uint8_t *digest)
{
		uint32_t finalcount[2];

		finalcount[0] = ctx->datalen >> 29;
    finalcount[1] = ctx->datalen << 3;
		brev(finalcount, finalcount, 2);

    sha_update(ctx, (uint8_t *)"\200", 1);
    while ((ctx->datalen & 63) != 56) {
        sha_update(ctx, (uint8_t *)"", 1);
    }
    sha_update(ctx, (uint8_t *)&finalcount, 8); /* Should cause a transform() */

		brev(&ctx->state, &ctx->state, ctx->hashsize / 4);
		umemcpy(digest, ctx->state, ctx->hashsize);
}

static void hmac_update_with_secret(struct sha_ctx *ctx, const u8 *secret, int len, u8 xor)
{
	int i;
	u8 v;

	for (i=0; i<len; i++) {
		v = secret[i] ^ xor;
		sha_update(ctx, &v, 1);
	}
	for (i=len; i<64; i++) {
		v = xor;
		sha_update(ctx, &v, 1);
	}
}

// secret should be no longer than 64 bytes! For longer secrets,
// pass them through hash function as specified in RFC-2104

void hmac_sign(struct sha_ctx *ctx, int method, struct hmac_data *hm)
{
	// inner digest
	sha_init(ctx, method);
	hmac_update_with_secret(ctx, hm->secret, hm->secretLen, 0x36);
	sha_update(ctx, hm->msg, hm->msgLen);
	sha_final(ctx, hm->hash);
	
	// outer digest
	sha_init(ctx, method);
	hmac_update_with_secret(ctx, hm->secret, hm->secretLen, 0x5c);
	sha_update(ctx, hm->hash, ctx->hashsize);
	sha_final(ctx, hm->hash);
}


