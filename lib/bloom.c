#include <stdlib.h>
#include <inttypes.h>

#include "bloom.h"
#include "../hash.h"
#include "../minmax.h"
#include "../crc/xxhash.h"
#include "../crc/murmur3.h"

struct bloom {
	uint64_t nentries;

	uint32_t *map;
};

#define BITS_PER_INDEX	(sizeof(uint32_t) * 8)
#define BITS_INDEX_MASK	(BITS_PER_INDEX - 1)

struct bloom_hash {
	unsigned int seed;
	uint32_t (*fn)(const void *, uint32_t, uint32_t);
};

struct bloom_hash hashes[] = {
	{
		.seed = 0x8989,
		.fn = jhash,
	},
	{
		.seed = 0x8989,
		.fn = XXH32,
	},
	{
		.seed = 0x8989,
		.fn = murmurhash3,
	},
};

#define N_HASHES	3

#define MIN_ENTRIES	1073741824UL

struct bloom *bloom_new(uint64_t entries)
{
	struct bloom *b;
	size_t no_uints;

	b = malloc(sizeof(*b));
	b->nentries = entries;
	no_uints = (entries + BITS_PER_INDEX - 1) / BITS_PER_INDEX;
	no_uints = max((unsigned long) no_uints, MIN_ENTRIES);
	b->map = calloc(no_uints, sizeof(uint32_t));
	if (!b->map) {
		free(b);
		return NULL;
	}

	return b;
}

void bloom_free(struct bloom *b)
{
	free(b->map);
	free(b);
}

static int __bloom_check(struct bloom *b, uint32_t *data, unsigned int nwords,
			 int set)
{
	uint32_t hash[N_HASHES];
	int i, was_set;

	for (i = 0; i < N_HASHES; i++) {
		hash[i] = hashes[i].fn(data, nwords, hashes[i].seed);
		hash[i] = hash[i] % b->nentries;
	}

	was_set = 0;
	for (i = 0; i < N_HASHES; i++) {
		const unsigned int index = hash[i] / BITS_PER_INDEX;
		const unsigned int bit = hash[i] & BITS_INDEX_MASK;

		if (b->map[index] & (1U << bit))
			was_set++;
		if (set)
			b->map[index] |= 1U << bit;
	}

	return was_set == N_HASHES;
}

int bloom_check(struct bloom *b, uint32_t *data, unsigned int nwords)
{
	return __bloom_check(b, data, nwords, 0);
}

int bloom_set(struct bloom *b, uint32_t *data, unsigned int nwords)
{
	return __bloom_check(b, data, nwords, 1);
}