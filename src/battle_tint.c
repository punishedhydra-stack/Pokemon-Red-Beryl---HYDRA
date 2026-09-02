#include "global.h"
#include "battle_tint.h"
#include "palette.h"    // LoadPalette, BG_PLTT_ID, PLTT_SIZE_4BPP
#include "time_tint.h"  // TimeTint_ApplyToBattlePalette

//HYDRA Battle background day/night tint. The per-hour tint math and table live in
// time_tint.c (shared with the overworld) so the two always match. This just copies
// the environment's 3-slot palette, tints it for the current hour, and loads it.
void LoadTintedBattleEnvPalette(const u16 *palette)
{
    u16 tinted[48]; // 3 slots x 16 colors

    CpuCopy16(palette, tinted, 3 * PLTT_SIZE_4BPP);
    TimeTint_ApplyToBattlePalette(tinted, 48);
    LoadPalette(tinted, BG_PLTT_ID(2), 3 * PLTT_SIZE_4BPP);
}
