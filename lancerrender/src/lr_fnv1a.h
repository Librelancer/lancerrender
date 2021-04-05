#ifndef _LR_FNV1A_H_
#define _LR_FNV1A_H_

#define FNV1_PRIME_32 16777619U
#define FNV1_OFFSET_32 2166136261U
static inline uint32_t fnv1a_32(const void *input, int len)
{
    const unsigned char *data = input;
    const unsigned char *end = data + len;
    uint32_t hash = FNV1_PRIME_32;
    for (; data != end; ++data)
    {
        hash ^= *data;
        hash *= FNV1_PRIME_32;
    }
    return hash;
}

#endif