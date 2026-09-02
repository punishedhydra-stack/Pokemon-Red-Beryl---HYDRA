#ifndef GUARD_TIME_TINT_H
#define GUARD_TIME_TINT_H

#include "palette.h" // struct BlendSettings

//HYDRA Per-hour day/night color tint (24 unique hourly tints, smoothly interpolated).
// Single source of truth used by BOTH the overworld (via UpdateTimeOfDay ->
// TimeMixPalettes) and the battle backgrounds (battle_tint.c). Each hour is a
// per-channel multiply toward a tint color, weighted by that hour's blend%.

//HYDRA TEMPORARY fast preview: number of REAL seconds per in-game hour for the day/night
// tint ONLY (does not touch the actual game clock / saves). Set to 0 to disable, so the
// tint follows the real clock again. 5 => full 24-hour cycle in ~2 minutes.
#define TIME_TINT_DEBUG_SECONDS_PER_HOUR 0

// Current time used for tinting: fast frame-counter preview when the debug macro is > 0,
// otherwise the real clock.
void TimeTint_GetEffectiveTime(u32 *hours, u32 *minutes);

// Overworld: baked BlendSettings for an hour (tint multiplier, ready for TimeMixPalettes).
struct BlendSettings TimeTint_GetHourBlend(u32 hour);

// Interpolation weight for TimeMixPalettes: 256 at :00 (all of the current hour) down
// to 0 at :60 (all of the next hour). Matches the engine's weight direction.
u16 TimeTint_GetInterpWeight(u32 minutes);

// Battle backgrounds: apply the current wall-clock hourly tint to a palette buffer in
// place (skips color 0 of each 16-color slot). Uses the exact same math the overworld
// tint uses, so battle backgrounds match the overworld hour-for-hour.
void TimeTint_ApplyToBattlePalette(u16 *palette, u16 count);

#endif // GUARD_TIME_TINT_H
