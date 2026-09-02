#include "global.h"
#include "bg.h"
#include "international_string_util.h"
#include "menu.h"
#include "map_name_popup.h"
#include "item_popup.h"
#include "event_data.h"
#include "palette.h"
#include "string_util.h"
#include "text.h"
#include "text_window.h"
#include "window.h"

// EWRAM
static EWRAM_DATA bool8 sItemPopupActive = FALSE;
static EWRAM_DATA u8 sItemPopupName[MAP_POPUP_STRING_BUFFER_LENGTH] = {0};

static const u8 sItemPopupTextColors[3] = {TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sText_SpaceX[] = _(" x");

bool8 IsItemPopupActive(void)
{
    return sItemPopupActive;
}

void ClearItemPopup(void)
{
    sItemPopupActive = FALSE;
}

// Called from the item-pickup scripts via `callnative Item_pickup_popup`.
// The item name is already buffered in gStringVar2 by the script.
// Set VAR_RESULT to the quantity before calling (e.g., setvar VAR_RESULT, 2).
// If VAR_RESULT > 1, displays "Item Name xN", otherwise just "Item Name".
void Item_pickup_popup(void)
{
    sItemPopupActive = TRUE;
    
    // Check if we have multiple items (quantity stored in gSpecialVar_Result by script)
    if (gSpecialVar_Result > 1)
    {
        // Build string: "Item Name x{quantity}"
        StringCopy(sItemPopupName, gStringVar2);
        StringAppend(sItemPopupName, sText_SpaceX);
        ConvertIntToDecimalStringN(gStringVar1, gSpecialVar_Result, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringAppend(sItemPopupName, gStringVar1);
    }
    else
    {
        // Single item - just copy the name
        StringCopy(sItemPopupName, gStringVar2);
    }
    
    StartPopUpTask();
}

// Renders the item name inside the player's selected menu frame.
// Invoked by the shared popup task (Task_MapNamePopUpWindow) when an item popup is active.
void ShowItemPopUpWindow(void)
{
    u8 itemName[MAP_POPUP_STRING_BUFFER_LENGTH];
    u8 windowId;
    u32 widthPx;
    u32 fontId;
    u8 x;

    AddMapNamePopUpWindow();
    windowId = GetMapNamePopUpWindowId();
    StringCopy(itemName, sItemPopupName);

    LoadUserWindowBorderGfx(windowId, 0x21D, BG_PLTT_ID(14)); // frame border tiles + palette -> slot 14
    Menu_LoadStdPalAt(BG_PLTT_ID(15));                        // white interior + gray text -> slot 15
    FillWindowPixelBuffer(windowId, PIXEL_FILL(1));           // white interior (slot 15, index 1)
    DrawTextBorderOuter(windowId, 0x21D, 14);                 // border drawn with slot 14
    PutWindowTilemap(windowId);

    widthPx = GetWindowAttribute(windowId, WINDOW_WIDTH) * 8;
    fontId = GetFontIdToFit(itemName, FONT_NORMAL, -1, widthPx);
    x = GetStringCenterAlignXOffset(fontId, itemName, widthPx);
    AddTextPrinterParameterized3(windowId, fontId, x, 1, sItemPopupTextColors, TEXT_SKIP_DRAW, itemName);
    CopyWindowToVram(windowId, COPYWIN_FULL);
}