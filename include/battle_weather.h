#ifndef GUARD_BATTLE_WEATHER_H
#define GUARD_BATTLE_WEATHER_H

//HYDRA Continuous in-battle weather overlay: rain/snow/hail particles, an overworld-style
// sandstorm (scrolling sand sprites) and a pulsing sun tint. Call once per frame from the battle
// main loop; it starts/stops the overlay task as the active battle weather requires.
void UpdateBattleWeatherOverlay(void);

//HYDRA Force a clean reload of the overlay gfx after the battle screen is rebuilt from a menu
// sub-screen (party/bag), which wipes our tiles from VRAM but leaves the load-tracking stale.
void BattleWeather_ForceReload(void);

//HYDRA TRUE while end-of-turn hail/sandstorm damage is being applied (set in battle_end_turn.c) so the
// in-battle weather overlay stays visible through the damage instead of going dormant.
extern bool8 gInBattleWeatherDamage;

#endif // GUARD_BATTLE_WEATHER_H
