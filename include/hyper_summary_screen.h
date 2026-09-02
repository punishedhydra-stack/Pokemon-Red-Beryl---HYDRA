#ifndef GUARD_HYPER_SUMMARY_SCREEN_H
#define GUARD_HYPER_SUMMARY_SCREEN_H

// HYDRA: Clone of the Pokemon summary screen used by the HYPER PC secret base
// decoration. This is where Nature/Ability/IV/EV editing will be built so the
// normal summary screen stays untouched. Reuses the summary screen enums
// (PokemonSummaryScreenMode, PSS_PAGE_*, etc.) from pokemon_summary_screen.h.

#include "main.h"

void ShowHyperSummaryScreen(u8 mode, void *mons, u8 monIndex, u8 maxMonIndex, void (*callback)(void));

// HYDRA: Set when the HYPER PC summary is first opened; stays set across a move
// relearner round-trip so move_relearner.c can return to the HYPER summary
// (battle moves page) instead of exiting to the overworld.
extern MainCallback gInitialHyperSummaryScreenCallback;

// HYDRA: The HYPER PC unlocks its editing features by badge count. Until the 3rd
// badge the decoration can only rename Pokemon.
//   3rd badge -> Nature, 5th -> EVs, 7th -> IVs, 8th -> Ability
bool32 HyperPC_CanEditNature(void);
bool32 HyperPC_CanEditEvs(void);
bool32 HyperPC_CanEditIvs(void);
bool32 HyperPC_CanEditAbility(void);
bool32 HyperPC_CanEditAnything(void);

// Called at line ~1255, defined at ~4583 in hyper_summary_screen.c. The original
// file got this prototype from pokemon_summary_screen.h under its pre-rename
// name (SummaryScreen_SetAnimDelayTaskId), so the clone needs its own.
void HyperSummaryScreen_SetAnimDelayTaskId(u8 taskId);

#endif // GUARD_HYPER_SUMMARY_SCREEN_H
