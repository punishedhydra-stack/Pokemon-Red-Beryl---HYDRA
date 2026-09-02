#ifndef GUARD_OWE_SPRITE_CACHE_H
#define GUARD_OWE_SPRITE_CACHE_H

//HYDRA Pre-decompressed overworld-encounter sprite cache (lives in EWRAM).
// Eliminates the per-spawn LZ77 decompression hitch: a Pokemon's overworld sprite is
// decompressed ONCE into EWRAM, then every spawn/respawn is a fast copy to VRAM instead
// of a live decompress. Keyed by the ROM compressed-data pointer, so it transparently
// covers any variant (form/gender) too.

// Clear the cache (call on map change, before warming the new map's species).
void OWESpriteCache_Reset(void);

// Return a pointer to the DECOMPRESSED tile data for `compressedSrc`, decompressing it into
// the cache on first request. Sets *sizeOut to the decompressed byte size. Returns NULL if
// the data is not LZ77-compressed or the cache has no room -- the caller then falls back to
// the normal live-decompress path (so behaviour is always correct, just slower).
const u8 *OWESpriteCache_GetOrLoad(const u32 *compressedSrc, u32 *sizeOut);

#endif // GUARD_OWE_SPRITE_CACHE_H
