#ifndef GUARD_HYDRA_HOTBAR_H
#define GUARD_HYDRA_HOTBAR_H

// HYDRA quick hotbar (Select button). 8 slots stored in gSaveBlock1Ptr->hotbar[].
// Field: SELECT opens it. In the bar: Left/Right navigate; A uses the highlighted slot
// (or, on an empty slot, opens the bag to register a key item there); SELECT grabs a
// filled slot to move/sort it (SELECT again drops/swaps); B closes.
// Registration also works from the bag: choose a key item -> Register -> pick a slot.
bool8 HydraHotbar_IsOpen(void);
void HydraHotbar_OpenFromField(void); // called by the field SELECT handler

// Bag integration:
void HydraHotbar_SetPendingAssign(u16 itemId); // way 1: bag "Register" queues an item to slot
bool8 HydraHotbar_IsPicking(void);             // way 2: bag was opened to fill a slot
bool8 HydraHotbar_TryAssignPicked(u16 itemId); // way 2: assign the chosen key item to that slot
bool8 HydraHotbar_TryAssignPickedHM(u16 itemId); // way 2: assign a chosen HM (badge + HM + able mon)
void CB2_HydraHotbar_ReturnToField(void);      // bag exit callback -> field + reopen hotbar

extern bool8 gHydraFlyMapFromHotbar; //HYDRA TRUE while a Fly/Teleport fly-map was opened from the hotbar

#endif // GUARD_HYDRA_HOTBAR_H
