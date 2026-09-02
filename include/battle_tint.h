#ifndef GUARD_BATTLE_TINT_H
#define GUARD_BATTLE_TINT_H

#include "global.h"

//HYDRA Loads the battle background's 3-slot palette with the current 24-hour day/night
// tint applied (see time_tint.c), so battle backgrounds match the overworld hour-for-hour.
// Called from battle_bg.c in place of the raw LoadPalette.
void LoadTintedBattleEnvPalette(const u16 *palette);

#endif // GUARD_BATTLE_TINT_H
