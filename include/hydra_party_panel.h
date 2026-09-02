#ifndef GUARD_HYDRA_PARTY_PANEL_H
#define GUARD_HYDRA_PARTY_PANEL_H

// HYDRA: Left-hand party panel shown alongside the field start menu.
// Draws 6 compact party boxes (party-menu "wide" slot graphic) on the left of
// the live overworld, navigable with the D-pad while the start menu is open.
//
// Lifecycle is driven entirely by start_menu.c:
//   - HydraPartyPanel_Open()  is called once the start-menu window is up
//     (InitStartMenuStep), so it also rebuilds on the return-to-field reopen path.
//   - HydraPartyPanel_Close() is called from RemoveExtraStartMenuWindows(),
//     the single choke point every start-menu exit/transition passes through,
//     so panel sprites are always freed before a screen switch (no leaks).
//   - HydraPartyPanel_HandleInput() is called from HandleStartMenuInput() and
//     returns TRUE when it consumed the frame's input (focus is on the panel).

// Result of a panel input frame, so the start menu knows what to do next.
enum HydraPanelInputResult
{
    HYDRA_PANEL_INPUT_NONE,      // panel did not consume input; start menu handles it
    HYDRA_PANEL_INPUT_CONSUMED,  // panel handled input this frame; start menu should stop
    HYDRA_PANEL_INPUT_CLOSE,     // panel requests the whole start menu close (B on panel)
    HYDRA_PANEL_INPUT_OPEN_PARTY, // A on a mon: open the standard party menu (also used by ITEM)
    HYDRA_PANEL_INPUT_SUMMARY,     // A -> action menu -> SUMMARY: open the summary screen for the mon
    HYDRA_PANEL_INPUT_GIVE,        //HYDRA A -> action menu -> ITEM>GIVE: open the bag to give an item
};

void HydraPartyPanel_Open(void);
void HydraPartyPanel_Close(void);
bool8 HydraPartyPanel_IsOpen(void);
bool8 HydraPartyPanel_HasFocus(void);
void HydraPartyPanel_CloseKeepGray(void); //HYDRA close panel UI, keep grayscale (submenu transitions)
void HydraPartyPanel_ReapplyGrayscale(void); //HYDRA re-grey after the return-from-submenu fade settles
enum HydraPanelInputResult HydraPartyPanel_HandleInput(void);
u8 HydraPartyPanel_GetSlot(void); //HYDRA which party slot the panel cursor is on (for SUMMARY)
void HydraPartyPanel_RestoreColors(void); //HYDRA remove grayscale + restore colours (keep-gray SAVE flow)
bool8 HydraPartyPanel_IsGrayApplied(void); //HYDRA grayscale on screen (even if panel UI torn down)
void HydraPartyPanel_BeginSave(void); //HYDRA entering the SAVE dialog: keep grayscale, cancel re-open won't re-apply

#endif // GUARD_HYDRA_PARTY_PANEL_H
