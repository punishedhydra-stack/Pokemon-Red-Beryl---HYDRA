#include "global.h"
#include "owe_sprite_cache.h"
#include "decompress.h" // IsCompressedData, GetDecompressedDataSize, FastLZ77UnCompWram

//HYDRA EWRAM budget for decompressed overworld-encounter sprites. Tunable: if the build
// reports an EWRAM overflow, lower this; if you have headroom and want every species on a
// busy route cached, raise it. Any sprite that doesn't fit simply falls back to the normal
// live-decompress path (still correct, just not cached).
#define OWE_CACHE_BYTES   (24 * 1024)
#define OWE_CACHE_MAX     24  // max distinct sprites cached at once

struct OWECacheEntry
{
    const u32 *src; // key: the ROM compressed-data pointer (info->images->data)
    u32 offset;     // start of the decompressed data within the buffer
    u32 size;       // decompressed byte size
};

// u32-typed buffer so it is word-aligned (FastLZ77UnCompWram writes in words).
static EWRAM_DATA u32 sCacheWords[OWE_CACHE_BYTES / 4] = {0};
static EWRAM_DATA struct OWECacheEntry sCacheEntries[OWE_CACHE_MAX] = {0};
static EWRAM_DATA u32 sCacheCount = 0;
static EWRAM_DATA u32 sCacheUsed = 0; // bytes used, always kept 4-aligned

#define CacheBytes ((u8 *)sCacheWords)

void OWESpriteCache_Reset(void)
{
    sCacheCount = 0;
    sCacheUsed = 0;
}

const u8 *OWESpriteCache_GetOrLoad(const u32 *compressedSrc, u32 *sizeOut)
{
    u32 i, size, alignedSize;
    const u8 *result;

    if (compressedSrc == NULL)
        return NULL;

    // Already cached?
    for (i = 0; i < sCacheCount; i++)
    {
        if (sCacheEntries[i].src == compressedSrc)
        {
            *sizeOut = sCacheEntries[i].size;
            return &CacheBytes[sCacheEntries[i].offset];
        }
    }

    // Not cacheable / no room -> caller uses the normal decompress path.
    if (sCacheCount >= OWE_CACHE_MAX || !IsCompressedData(compressedSrc))
        return NULL;

    size = GetDecompressedDataSize(compressedSrc);
    alignedSize = (size + 3u) & ~3u;
    if (size == 0 || sCacheUsed + alignedSize > OWE_CACHE_BYTES)
        return NULL;

    FastLZ77UnCompWram(compressedSrc, &CacheBytes[sCacheUsed]);
    sCacheEntries[sCacheCount].src = compressedSrc;
    sCacheEntries[sCacheCount].offset = sCacheUsed;
    sCacheEntries[sCacheCount].size = size;
    *sizeOut = size;
    result = &CacheBytes[sCacheUsed];
    sCacheCount++;
    sCacheUsed += alignedSize;
    return result;
}
