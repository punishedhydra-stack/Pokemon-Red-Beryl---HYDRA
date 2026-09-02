#ifndef GUARD_ITEM_POPUP_H
#define GUARD_ITEM_POPUP_H

// Called from the item-pickup scripts via `callnative Item_pickup_popup`.
void Item_pickup_popup(void);

// TRUE while the shared popup task is showing an item (vs. a map name).
bool8 IsItemPopupActive(void);

// Draw callback used by the popup task to render the item name.
void ShowItemPopUpWindow(void);

// Clears the "showing item" state (called when a map-name popup starts).
void ClearItemPopup(void);

#endif // GUARD_ITEM_POPUP_H
