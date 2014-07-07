
#include "murmur_hash_2_64_a.h"

/* \see http://en.wikipedia.org/wiki/MurmurHash
 * \see http://www.maatkit.org/
 * \see https://code.google.com/p/maatkit/issues/attachmentText?id=19&aid=7029841249934490324&name=MurmurHash64.cpp&token=3b615cc6c16c91de800419e5e95ed1ba
 * \see https://code.google.com/p/smhasher/source/browse/trunk/MurmurHash2.cpp (MurmurHash64A)
 */
uint64_t bsal_murmur_hash_2_64_a(const void *key, int len, unsigned int seed)
{
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;

    uint64_t h = seed ^ len;

    const uint64_t *data = (const uint64_t *)key;
    const uint64_t *end = data + (len / 8);

    while (data != end) {
        uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char * data2 = (const unsigned char*)data;

    switch (len & 7) {
        case 7: h ^= (uint64_t)(data2[6]) << 48;
        case 6: h ^= (uint64_t)(data2[5]) << 40;
        case 5: h ^= (uint64_t)(data2[4]) << 32;
        case 4: h ^= (uint64_t)(data2[3]) << 24;
        case 3: h ^= (uint64_t)(data2[2]) << 16;
        case 2: h ^= (uint64_t)(data2[1]) << 8;
        case 1: h ^= (uint64_t)(data2[0]);
                h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

