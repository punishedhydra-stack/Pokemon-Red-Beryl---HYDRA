#ifndef GUARD_HYDRA_INCUBATOR_H
#define GUARD_HYDRA_INCUBATOR_H

// HYDRA Egg incubator secret base decoration.
// Up to HYDRA_NUM_INCUBATORS per base, each keyed by its base-local tile position and
// holding one egg (a BoxPokemon) in gSaveBlock1Ptr->hydraIncubators. Script specials:
//   HydraIncubator_BeginInteract  -> VAR_RESULT 0 empty / 1 incubating / 2 ready
//   HydraIncubator_StoreChosenEgg -> VAR_RESULT 1 stored / 0 cancelled / 2 not-an-egg / 3 full

void HydraIncubator_BeginInteract(void);
void HydraIncubator_StoreChosenEgg(void);
void HydraIncubator_RemoveEgg(void);
void HydraIncubator_PrepareHatch(void); // move a ready egg to the party so EventScript_EggHatch can hatch it
void HydraIncubator_Step(void); // called each overworld step to advance incubating eggs
void HydraIncubator_StartAnimTask(void); // starts the sprite-animation task when a base loads

#endif // GUARD_HYDRA_INCUBATOR_H
