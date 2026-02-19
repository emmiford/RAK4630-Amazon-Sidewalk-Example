/*
 * Command Authentication — HMAC-SHA256 verification
 *
 * Contains a minimal standalone SHA-256 implementation and HMAC wrapper.
 * No external crypto dependencies — suitable for the ~4KB OTA-updatable app.
 */

#include <cmd_auth.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  SHA-256 (FIPS 180-4) — minimal standalone implementation          */
/* ------------------------------------------------------------------ */

#define SHA256_BLOCK_SIZE  64
#define SHA256_DIGEST_SIZE 32

typedef struct {
	uint32_t state[8];
	uint64_t count;
	uint8_t  buf[SHA256_BLOCK_SIZE];
} sha256_ctx_t;

static const uint32_t K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static inline uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t sig0(uint32_t x) { return rotr(x,  2) ^ rotr(x, 13) ^ rotr(x, 22); }
static inline uint32_t sig1(uint32_t x) { return rotr(x,  6) ^ rotr(x, 11) ^ rotr(x, 25); }
static inline uint32_t gam0(uint32_t x) { return rotr(x,  7) ^ rotr(x, 18) ^ (x >>  3); }
static inline uint32_t gam1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

static inline uint32_t be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] <<  8) | (uint32_t)p[3];
}

static inline void be32_put(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >>  8);
	p[3] = (uint8_t)(v);
}

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t block[64])
{
	uint32_t W[64];
	uint32_t a, b, c, d, e, f, g, h;

	for (int i = 0; i < 16; i++) {
		W[i] = be32(block + i * 4);
	}
	for (int i = 16; i < 64; i++) {
		W[i] = gam1(W[i-2]) + W[i-7] + gam0(W[i-15]) + W[i-16];
	}

	a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
	e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

	for (int i = 0; i < 64; i++) {
		uint32_t t1 = h + sig1(e) + ch(e, f, g) + K[i] + W[i];
		uint32_t t2 = sig0(a) + maj(a, b, c);
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}

	ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
	ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx)
{
	ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
	ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
	ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
	ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
	ctx->count = 0;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
	size_t idx = (size_t)(ctx->count & 0x3F);
	ctx->count += len;

	while (len > 0) {
		size_t copy = SHA256_BLOCK_SIZE - idx;
		if (copy > len) {
			copy = len;
		}
		memcpy(ctx->buf + idx, data, copy);
		idx += copy;
		data += copy;
		len -= copy;

		if (idx == SHA256_BLOCK_SIZE) {
			sha256_transform(ctx, ctx->buf);
			idx = 0;
		}
	}
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t digest[32])
{
	uint64_t bits = ctx->count * 8;
	size_t idx = (size_t)(ctx->count & 0x3F);

	/* Padding */
	ctx->buf[idx++] = 0x80;
	if (idx > 56) {
		memset(ctx->buf + idx, 0, SHA256_BLOCK_SIZE - idx);
		sha256_transform(ctx, ctx->buf);
		idx = 0;
	}
	memset(ctx->buf + idx, 0, 56 - idx);

	/* Length in bits (big-endian) */
	ctx->buf[56] = (uint8_t)(bits >> 56);
	ctx->buf[57] = (uint8_t)(bits >> 48);
	ctx->buf[58] = (uint8_t)(bits >> 40);
	ctx->buf[59] = (uint8_t)(bits >> 32);
	ctx->buf[60] = (uint8_t)(bits >> 24);
	ctx->buf[61] = (uint8_t)(bits >> 16);
	ctx->buf[62] = (uint8_t)(bits >>  8);
	ctx->buf[63] = (uint8_t)(bits);
	sha256_transform(ctx, ctx->buf);

	for (int i = 0; i < 8; i++) {
		be32_put(digest + i * 4, ctx->state[i]);
	}
}

/* ------------------------------------------------------------------ */
/*  HMAC-SHA256 (RFC 2104)                                            */
/* ------------------------------------------------------------------ */

static void hmac_sha256(const uint8_t *key, size_t key_len,
			const uint8_t *msg, size_t msg_len,
			uint8_t digest[32])
{
	sha256_ctx_t ctx;
	uint8_t k_pad[SHA256_BLOCK_SIZE];
	uint8_t inner_digest[SHA256_DIGEST_SIZE];

	/* Key preparation — if key > block size, hash it; else zero-pad */
	uint8_t k_prepared[SHA256_BLOCK_SIZE];
	memset(k_prepared, 0, SHA256_BLOCK_SIZE);
	if (key_len > SHA256_BLOCK_SIZE) {
		sha256_init(&ctx);
		sha256_update(&ctx, key, key_len);
		sha256_final(&ctx, k_prepared);
	} else {
		memcpy(k_prepared, key, key_len);
	}

	/* Inner hash: SHA256(K ^ ipad || msg) */
	for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
		k_pad[i] = k_prepared[i] ^ 0x36;
	}
	sha256_init(&ctx);
	sha256_update(&ctx, k_pad, SHA256_BLOCK_SIZE);
	sha256_update(&ctx, msg, msg_len);
	sha256_final(&ctx, inner_digest);

	/* Outer hash: SHA256(K ^ opad || inner_digest) */
	for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
		k_pad[i] = k_prepared[i] ^ 0x5c;
	}
	sha256_init(&ctx);
	sha256_update(&ctx, k_pad, SHA256_BLOCK_SIZE);
	sha256_update(&ctx, inner_digest, SHA256_DIGEST_SIZE);
	sha256_final(&ctx, digest);
}

/* ------------------------------------------------------------------ */
/*  Command authentication API                                         */
/* ------------------------------------------------------------------ */

static uint8_t auth_key[CMD_AUTH_KEY_SIZE];
static bool key_set;

int cmd_auth_set_key(const uint8_t *key, size_t key_len)
{
	if (!key || key_len != CMD_AUTH_KEY_SIZE) {
		return -1;
	}
	memcpy(auth_key, key, CMD_AUTH_KEY_SIZE);
	key_set = true;
	return 0;
}

bool cmd_auth_is_configured(void)
{
	return key_set;
}

bool cmd_auth_verify(const uint8_t *payload, size_t payload_len,
		     const uint8_t *tag)
{
	if (!key_set || !payload || !tag) {
		return false;
	}

	uint8_t digest[SHA256_DIGEST_SIZE];
	hmac_sha256(auth_key, CMD_AUTH_KEY_SIZE, payload, payload_len, digest);

	/* Constant-time comparison of first CMD_AUTH_TAG_SIZE bytes */
	uint8_t diff = 0;
	for (int i = 0; i < CMD_AUTH_TAG_SIZE; i++) {
		diff |= digest[i] ^ tag[i];
	}
	return diff == 0;
}
