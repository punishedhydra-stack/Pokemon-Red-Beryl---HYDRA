#include "global.h"
#include "hydra_party_panel.h"
#include "main.h"
#include "bg.h"
#include "decompress.h"
#include "malloc.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "sound.h"
#include "sprite.h"
#include "window.h"
#include "text.h"
#include "menu.h"
#include "string_util.h"
#include "strings.h"
#include "graphics.h"
#include "constants/party_menu.h"
#include "constants/songs.h"
#include "constants/battle.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "field_effect_helpers.h"
#include "field_weather.h"     //HYDRA weather/time re-apply on grayscale restore
#include "constants/weather.h" //HYDRA WEATHER_FOG_HORIZONTAL
#include "mail.h"           //HYDRA ItemIsMail for held-item indicator
#include "constants/items.h" //HYDRA ITEM_NONE
#include "overworld.h" //HYDRA UpdateTimeOfDay/UpdatePalettesWithTime on close (re-tint)
#include "pokemon_summary_screen.h" //HYDRA SUMMARY action
#include "party_menu.h"             //HYDRA SwitchPartyMonSlots for the SWITCH action
#include "item.h"                  //HYDRA AddBagItem/CheckBagHasSpace for TAKE

// HYDRA: Left party panel over the field start menu.
// ONE tall BG0 window holds all 6 boxes, drawn pixel-precise. Icons, pokeballs
// and status labels are OBJ sprites. Uses only BG palette bank 13 (safe over the
// overworld); the selection red-outline and fainted red-fill are pixel overpaints,
// so we no longer touch banks 14/15 (the start-menu frame/text banks).

#define PANEL_SLOTS     PARTY_SIZE

// ---- window (tiles) ----
#define PANEL_WIN_LEFT  1            //HYDRA window column (screen px 8). was 2(px16); moved 1 tile left, box nudged +2px (BOX_X) => whole panel -6px
#define PANEL_WIN_TOP   0            //HYDRA window row    (screen px 0)
#define PANEL_WIN_W     10           //HYDRA window width  (80px, screen 16..96). was 9(72px) - too narrow to show the box right border //HYDRA
#define PANEL_WIN_H     20           //HYDRA window height (160px)
#define PANEL_WIN_BASE  0x230
#define PANEL_PAL       13           //HYDRA only BG bank used (safe over field)

// ---- box layout (window-local px; window origin = screen (16,0)) ----
#define PANEL_TOP_PX    2            //HYDRA box 0 top. raise whole panel: decrease
#define BOX_X           3            //HYDRA box left within window (was 1). window moved -8px, box +2px => box screen px11 (was 17), net -6px
#define BOX_W           75           //HYDRA 75px box; right frame column at px73 now fits inside the widened 80px window (rounded corner shows) //HYDRA
#define BOX_H           24           //HYDRA box height (3 tile rows; frame needs 24)
#define BOX_TOP_INSET   2            //HYDRA frame's top border sits 2px below tile-top; fill/outline start here so blue doesn't spill above the box //HYDRA
#define BOX_GAP         2            //HYDRA transparent gap between boxes
#define BOX_STEP        (BOX_H + BOX_GAP)   // 26

// ---- content offsets, relative to each box's top-left (px) ----
#define BAR_HP_DX       18           //HYDRA "HP" label x from box-left. bar left: decrease
#define BAR_BODY_DX     34           //HYDRA bar body x from box-left
#define BAR_BODY_W      34           //HYDRA fillable HP px (was 24; extended right to reach the raised bevel cap) //HYDRA
#define BAR_BLIT_DY     5            //HYDRA bar row y from box-top. bar up: decrease
#define BAR_FILL_DY     7            //HYDRA HP fill y from box-top (bar row + 2)
#define LEVEL_DX        19           //HYDRA level text x from box-left. text left: decrease
#define LEVEL_DY        9            //HYDRA level/gender text y from box-top. text up: decrease
#define GENDER_DX       24           //HYDRA gender x offset from LEVEL_DX

// ---- sprites (absolute screen px) ----
#define ICON_X          14           //HYDRA icon x (was 20). -6px to match the whole-panel left shift
#define ICON_TOP_PX     13           //HYDRA icon 0 y (center)
#define ICON_STEP_PX    26           //HYDRA icon vertical step (= BOX_STEP)
#define BALL_X_OFFSET   2            //HYDRA pokeball x right of icon
#define BALL_Y_OFFSET   8            //HYDRA pokeball y below icon
#define BALL_SELECT_LEFT 4           //HYDRA selected pokeball nudges left + opens
#define SWITCH_SLIDE_FRAMES 12     //HYDRA frames the SWITCH slide takes (higher = slower)
#define STATUS_DX       59           //HYDRA status label x from box-left (screen)
#define STATUS_DY       17           //HYDRA status label y from box-top (screen)
#define HELD_ITEM_DX    8         //HYDRA held-item icon x offset from ICON_X (bottom-left of portrait).
#define HELD_ITEM_DY    11            //HYDRA held-item icon y offset from iconY

// ---- palette indices inside bank 13 ----
#define BLUE_IDX        5            //HYDRA box interior blue
#define RED_IDX         12           //HYDRA selection outline + fainted interior (bright red)

//HYDRA ---- selection-accent colours (SS1/SS2) ----
// bank-13 is full, so the "HP" label is recoloured off slots 14/15 (see HydraPartyPanel_Open) and
// those two freed slots hold one accent colour set at a time: GREEN (switch/move selection) or
// lighter-BLUE (browse highlight). Vanilla gPartyMenuBg_Pal colour IDs are copied in verbatim.
#define ACC_BODY_IDX    14           //HYDRA freed bank-13 slot: accent body LIGHT (top gradient tone)
#define ACC_BODY2_IDX   7            //HYDRA freed via 7->4 dedup: accent body MID (bottom gradient tone)
#define ACC_BORDER_IDX  15           //HYDRA freed bank-13 slot: accent box border colour (single tone)
#define ACC_NONE        0
#define ACC_BLUE        1            //HYDRA browse highlight  (2-tone blue body + orange border)
#define ACC_GREEN       2            //HYDRA switch/move select (2-tone green body + yellow border)
#define ACC_RED         3            //HYDRA fainted highlight/select (lighter-red body, like the party menu)
// vanilla gPartyMenuBg_Pal colour IDs (body is a light->mid gradient; offset2 border is one colour)
#define PAL_ID_GREEN_BODY_HI  100    //HYDRA "selected for action" body light
#define PAL_ID_GREEN_BODY_MID 101    //HYDRA "selected for action" body mid
#define PAL_ID_GREEN_BORDER   167    //HYDRA "selected for action" border yellow
#define PAL_ID_BLUE_BODY_HI   116    //HYDRA "current selection" body light
#define PAL_ID_BLUE_BODY_MID  117    //HYDRA "current selection" body mid
#define PAL_ID_BLUE_BORDER    103    //HYDRA "current selection" border orange
#define PAL_ID_RED_BODY_HI    148    //HYDRA "current selection fainted" body light
#define PAL_ID_RED_BODY_MID   149    //HYDRA "current selection fainted" body mid
#define PAL_ID_RED_BORDER     103    //HYDRA fainted selection border orange (offset2 = curr-sel orange)
#define BODY_SPLIT_H    9            //HYDRA px height of the lighter top body band (rest is the mid tone)

// bg.png tile ids (party-menu box graphic).
#define T_TL 43
#define T_TE 44
#define T_TR 45
#define T_LE 49
#define T_RE 54
#define T_BL 55
#define T_BE 56
#define T_BR 57
#define T_HP_L 52
#define T_HP_R 53
#define T_BAR  51

#define TAG_POKEBALL    (0x5B00 | BLEND_IMMUNE_FLAG) //HYDRA immune so the pokeball keeps colour under TOD/weather
#define TAG_STATUS      (0x5C00 | BLEND_IMMUNE_FLAG) //HYDRA immune so status icons keep colour under TOD/weather

// Panel UI sprite tags to exclude from grayscale
#define ICON_TAG_BASE   0x5D00
#define BALL_TAG_BASE   TAG_POKEBALL
#define STATUS_TAG_BASE TAG_STATUS

static EWRAM_DATA bool8 sPanelOpen = FALSE;
static EWRAM_DATA bool8 sPanelFocus = FALSE;
static EWRAM_DATA u8 sPanelCursor = 0;
static EWRAM_DATA u8 sPanelCount = 0;
static EWRAM_DATA u8 sIconSpriteIds[PANEL_SLOTS];
static EWRAM_DATA u8 sBallSpriteIds[PANEL_SLOTS];
static EWRAM_DATA u8 sStatusSpriteIds[PANEL_SLOTS];
static EWRAM_DATA u8 sHeldItemIds[PANEL_SLOTS]; //HYDRA held-item indicator sprite per slot
//HYDRA action menu (A on a mon -> SUMMARY / SWITCH / ITEM / CANCEL) state
static EWRAM_DATA bool8 sActionOpen = FALSE;      // action menu window is up
static EWRAM_DATA u8 sActionCursor = 0;           // 0=SUMMARY 1=SWITCH 2=ITEM 3=CANCEL
static EWRAM_DATA u8 sActionWin;                 //HYDRA action menu window id (set to WINDOW_NONE in Open; EWRAM allows zero-init only)
static EWRAM_DATA bool8 sSwitchMode = FALSE;      // choosing a second mon to swap with
static EWRAM_DATA u8 sSwitchFrom = 0;             // slot being moved during SWITCH
static EWRAM_DATA bool8 sSwitchSliding = FALSE;   //HYDRA true while the SWITCH slide animation runs
static EWRAM_DATA u8 sSwitchTo = 0;               //HYDRA second slot chosen for the swap (slide target)
static EWRAM_DATA u8 sSlideStep = 0;              //HYDRA current slide frame (0..SWITCH_SLIDE_FRAMES)
static EWRAM_DATA bool8 sMoveMode = FALSE;        //HYDRA choosing a mon to MOVE the held item to
static EWRAM_DATA u8 sMoveFrom = 0;               //HYDRA source slot whose held item is being moved
static EWRAM_DATA bool8 sPartyOrderChanged = FALSE; // a SWITCH happened -> refresh follower on close
static EWRAM_DATA u8 sActionMode = 0;             // 0 = main menu, 1 = ITEM submenu (GIVE/TAKE/MOVE)
static EWRAM_DATA bool8 sReturnFocused = FALSE;   // re-focus the panel on the same mon after a panel submenu (SUMMARY)
static EWRAM_DATA bool8 sGrayApplied = FALSE;     //HYDRA grayscale is currently applied (stays TRUE across the keep-gray save dialog)
static EWRAM_DATA bool8 sSaveKeepGray = FALSE;    //HYDRA in the SAVE dialog (keep grayscale; the field is NOT reloaded, so the cancel re-open must not re-apply it)
static EWRAM_DATA u8 sPanelWin;
static EWRAM_DATA u8 *sBoxGfx = NULL;

// Grayscale system variables
static EWRAM_DATA u16 sSavedBGPalettes[512];
static EWRAM_DATA u16 sSavedUnfadedAll[512]; //HYDRA snapshot of gPlttBufferUnfaded so grayscale survives fades (return-from-submenu) and TOD
static EWRAM_DATA u8 sSavedSpritePals[MAX_SPRITES];
static EWRAM_DATA u16 sSavedPalette0[16];
static EWRAM_DATA u16 sSavedPaletteTags[MAX_SPRITES];
static EWRAM_DATA u16 sSavedPaletteColors[MAX_SPRITES][16];

static const u8 sPanelTextColor[3] = { TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GRAY, TEXT_COLOR_DARK_GRAY };

// ---- pokeball holder sprite (32x32, frame 0 closed / frame 16 open) ----
static const struct OamData sOamData_Ball =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .priority = 0,
};
static const union AnimCmd sBallAnim_Closed[] = { ANIMCMD_FRAME(0, 0), ANIMCMD_END };
static const union AnimCmd sBallAnim_Open[]   = { ANIMCMD_FRAME(16, 0), ANIMCMD_END };
static const union AnimCmd *const sBallAnimTable[] = { sBallAnim_Closed, sBallAnim_Open };
static const struct CompressedSpriteSheet sSpriteSheet_Ball = { gPartyMenuPokeball_Gfx, 0x400, TAG_POKEBALL };
static const struct SpritePalette sSpritePalette_Ball = { gPartyMenuPokeball_Pal, TAG_POKEBALL };
static const struct SpriteTemplate sSpriteTemplate_Ball =
{
    .tileTag = TAG_POKEBALL,
    .paletteTag = TAG_POKEBALL,
    .oam = &sOamData_Ball,
    .anims = sBallAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// ---- status label sprite (32x8). Uses the real party-menu status gfx, our own
// tag so we own load/free. Anim index = ailment - 1 (PSN..FNT). ----
static const struct OamData sOamData_Status =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x8),
    .size = SPRITE_SIZE(32x8),
    .priority = 0,
};
static const union AnimCmd sStatusAnim0[] = { ANIMCMD_FRAME(0,  0), ANIMCMD_END };
static const union AnimCmd sStatusAnim1[] = { ANIMCMD_FRAME(4,  0), ANIMCMD_END };
static const union AnimCmd sStatusAnim2[] = { ANIMCMD_FRAME(8,  0), ANIMCMD_END };
static const union AnimCmd sStatusAnim3[] = { ANIMCMD_FRAME(12, 0), ANIMCMD_END };
static const union AnimCmd sStatusAnim4[] = { ANIMCMD_FRAME(16, 0), ANIMCMD_END };
static const union AnimCmd sStatusAnim5[] = { ANIMCMD_FRAME(20, 0), ANIMCMD_END };
static const union AnimCmd sStatusAnim6[] = { ANIMCMD_FRAME(24, 0), ANIMCMD_END };
static const union AnimCmd sStatusAnim7[] = { ANIMCMD_FRAME(28, 0), ANIMCMD_END };
static const union AnimCmd *const sStatusAnimTable[] =
{
    sStatusAnim0, sStatusAnim1, sStatusAnim2, sStatusAnim3,
    sStatusAnim4, sStatusAnim5, sStatusAnim6, sStatusAnim7,
};
static const struct CompressedSpriteSheet sSpriteSheet_Status = { gStatusGfx_Icons, 0x400, TAG_STATUS };
static const struct SpritePalette sSpritePalette_Status = { gStatusPal_Icons, TAG_STATUS };
static const struct SpriteTemplate sSpriteTemplate_Status =
{
    .tileTag = TAG_STATUS,
    .paletteTag = TAG_STATUS,
    .oam = &sOamData_Status,
    .anims = sStatusAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

//HYDRA held-item indicator - reuses the party menu's held-item graphic + palette (both non-static
// there). Tag 55120 == TAG_HELD_ITEM (0xD770): bit 15 set => blend-immune, so it stays coloured
// under grayscale/TOD/weather. anim 0 = regular item (yellow/red square), anim 1 = mail (envelope).
extern const struct SpriteSheet gSpriteSheet_HeldItem;
extern const u16 gHeldItemPalette[];
#define TAG_HELD_PANEL 55120
static const struct OamData sOamData_HeldPanel =
{
    .y = 0, .affineMode = ST_OAM_AFFINE_OFF, .objMode = ST_OAM_OBJ_NORMAL, .mosaic = FALSE,
    .bpp = ST_OAM_4BPP, .shape = SPRITE_SHAPE(8x8), .x = 0, .matrixNum = 0,
    .size = SPRITE_SIZE(8x8), .tileNum = 0, .priority = 0, .paletteNum = 0, .affineParam = 0,
};
static const union AnimCmd sHeldPanelAnim_Item[] = { ANIMCMD_FRAME(0, 1), ANIMCMD_END };
static const union AnimCmd sHeldPanelAnim_Mail[] = { ANIMCMD_FRAME(1, 1), ANIMCMD_END };
static const union AnimCmd *const sHeldPanelAnims[] = { sHeldPanelAnim_Item, sHeldPanelAnim_Mail };
static const struct SpritePalette sHeldPanelPal = { gHeldItemPalette, TAG_HELD_PANEL };
static const struct SpriteTemplate sSpriteTemplate_HeldPanel =
{
    .tileTag = TAG_HELD_PANEL,
    .paletteTag = TAG_HELD_PANEL,
    .oam = &sOamData_HeldPanel,
    .anims = sHeldPanelAnims,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static void Panel_LoadSprites(void);
static void Panel_FreeSprites(void);
static void Panel_CreateWindow(void);
static void Panel_FreeWindow(void);
static void Panel_DrawAll(void);
static void Panel_DrawBox(u8 slot);
static void Panel_RedrawBox(u8 slot);
static void Panel_Blit(u16 tileId, u8 px, u8 py);
static void Panel_UpdateBall(u8 slot, bool8 selected);
static void Panel_MoveCursor(s8 delta);
//HYDRA action-menu forward declarations (defined just above HydraPartyPanel_HandleInput,
// but Close/CloseKeepGray above call the teardown helper).
static void Panel_OpenActionMenu(void);
static void Panel_CloseActionMenu(void);
static void Panel_TeardownActionUI(void);
static void Panel_DrawActionMenu(void);
static enum HydraPanelInputResult Panel_HandleActionInput(void);
static enum HydraPanelInputResult Panel_HandleSwitchInput(void);
static enum HydraPanelInputResult Panel_HandleMoveInput(void); //HYDRA ITEM>MOVE target picker

// Grayscale functions
static u16 DesaturateColor(u16 color)
{
    u8 r = color & 0x1F;
    u8 g = (color >> 5) & 0x1F;
    u8 b = (color >> 10) & 0x1F;
    u8 gray = (u8)((r * 77 + g * 151 + b * 28) >> 8);
    return (gray) | (gray << 5) | (gray << 10);
}

//HYDRA ===== OVERWORLD-SPRITE DARKEN KNOB =====================================================
// Darkens the greyed overworld sprites (player, NPCs, followers, grass/field effects) while the
// panel is open. Works by capping each grey's brightness at GRAY_SPRITE_MAX (0-31):
//   31 = no darkening (off)   lower = darker   (e.g. 20 dims highlights, 12 is quite dark)
// A cap is used (not a multiply) on purpose: it is IDEMPOTENT, so HydraPartyPanel_ReapplyGrayscale
// can run it every frame during the return-from-submenu fade without the sprites getting darker
// each frame (a multiply would fade them to black). The BG and the panel/menu UI are NOT affected.
#define GRAY_SPRITE_MAX  20

//HYDRA cap a NEUTRAL grey colour (r==g==b) to GRAY_SPRITE_MAX. Safe to run repeatedly.
static u16 DarkenGray(u16 grayColor)
{
    u8 v = grayColor & 0x1F;
    if (v > GRAY_SPRITE_MAX)
        v = GRAY_SPRITE_MAX;
    return (v) | (v << 5) | (v << 10);
}

#define GRAYSCALE_PAL_TAG  0x5E00  //HYDRA dedicated grayscale sprite palette tag (panel)

//HYDRA true if OBJ palette slot `palNum` should be desaturated. Kept in their own palettes:
// the shadow field effect (TAG_WEATHER_START) so it stays dark like in colour mode, the
// blend-immune UI palettes (panel icons/ball/status), and the grayscale ramp itself.
static bool8 HydraShouldGrayObjPal(u8 palNum)
{
    u16 tag = GetSpritePaletteTagByPaletteNum(palNum);
    //HYDRA weather palette (TAG_WEATHER_START / 0x1200) is desaturated now so rain/snow
    // streaks turn grey; the shadow shares that slot but uses the already-grey fog indices,
    // which DesaturateColor leaves unchanged, so the shadow stays identical in colour + grey.
    if (IS_BLEND_IMMUNE_TAG(tag))
        return FALSE;
    if (tag == GRAYSCALE_PAL_TAG)
        return FALSE;
    return TRUE;
}


//HYDRA grayscale ramp applied to all overworld sprites (same ramp the hotbar uses)
static const u16 sGrayscalePalette[16] = {
    0x7FFF,  // 31, 31, 31
    0x7FFF,  // 31, 31, 31
	0x4A52,  // 18, 18, 18  (was 26)
    0x35AD,  // 13, 13, 13  (was 21)
    0x294A,  // 10, 10, 10  (was 18)
    0x2108,  // 8, 8, 8     (was 16)
    0x2529,  // 9, 9, 9    (was 15)
    0x1CE7,  // 7, 7, 7    (was 13)
    0x14A5,  // 5, 5, 5    (was 9)
    0x1084,  // 4, 4, 4    (was 8)
    0x0C63,  // 3, 3, 3    (was 7)
    0x0421,  // 1, 1, 1    (was 5)
    0x0421,  // 1, 1, 1
    0x0421,  // 1, 1, 1
    0x0421,  // 1, 1, 1
    0x0421,  // 1, 1, 1
};

//HYDRA BG palette indices belonging to the panel's own bank (kept in colour, not desaturated)
#define IS_PANEL_BANK(idx) ((idx) >= (PANEL_PAL * 16) && (idx) < ((PANEL_PAL + 1) * 16))
//HYDRA banks 13(panel),14(STD window),15(DLG window) all stay coloured: the start-menu frame/
// window use banks 14/15, so desaturating them greyed the start-menu border (should be colour).
#define IS_UI_BANK(idx) ((idx) >= (PANEL_PAL * 16) && (idx) < 256)

//HYDRA the panel's own UI sprites (icons, balls, status) - never grayscaled
#define IS_PANEL_UI_TAG(t) (((t) >= ICON_TAG_BASE && (t) < ICON_TAG_BASE + PANEL_SLOTS) \
                            || (t) == BALL_TAG_BASE || (t) == STATUS_TAG_BASE)

static void ApplyGrayscaleEffect(void)
{
    int i, j;
    struct ObjectEvent *objEvent;
    u8 grayPalNum = 0xFF;

    for (i = 0; i < MAX_SPRITES; i++) {
        sSavedSpritePals[i] = 0xFF;
        sSavedPaletteTags[i] = 0xFFFF;
    }
    for (i = 0; i < 16; i++)
        sSavedPalette0[i] = 0;

    // === SAVE ORIGINAL OBJ PALETTE 0 ===
    for (i = 0; i < 16; i++)
        sSavedPalette0[i] = gPlttBufferFaded[256 + i];

    //HYDRA snapshot the whole unfaded buffer so we can restore it on close. Grayscale must
    // also cover unfaded because FadeInFromBlack (return from a submenu) and the TOD/weather
    // pipeline rebuild faded FROM unfaded - if unfaded stayed coloured the fade would reveal colour.
    for (i = 0; i < 512; i++)
        sSavedUnfadedAll[i] = gPlttBufferUnfaded[i];

    // === DESATURATE BG PALETTES (skip obj pal 0 for now; keep the panel bank in colour) ===
    for (i = 0; i < 256; i++) {
        sSavedBGPalettes[i] = gPlttBufferFaded[i];
        if (IS_UI_BANK(i)) //HYDRA the side panel + start-menu window banks keep their colour
            continue;
        gPlttBufferFaded[i] = DesaturateColor(gPlttBufferFaded[i]);
    }
    for (i = 256; i < 512; i++)
        sSavedBGPalettes[i] = gPlttBufferFaded[i];
    //HYDRA desaturate each OBJ palette slot except shadow / UI / grayscale. Straight per-slot
    // desaturation replaces the old reversed pal-0 desaturation, which inverted whatever sat on
    // OBJ palette 0 (the shadow) and turned it light.
    {
        int p;
        for (p = 0; p < 16; p++) {
            if (!HydraShouldGrayObjPal(p))
                continue;
            for (i = 0; i < 16; i++)
                gPlttBufferFaded[256 + p * 16 + i] = DarkenGray(DesaturateColor(gPlttBufferFaded[256 + p * 16 + i])); //HYDRA grey + darken sprite
        }
    }

    // === STEP 1: save each overworld sprite's ORIGINAL (unfaded) palette ===
    for (objEvent = gObjectEvents; objEvent < &gObjectEvents[OBJECT_EVENTS_COUNT]; objEvent++)
    {
        u8 spriteId;
        struct Sprite *sprite;
        if (!objEvent->active || objEvent->spriteId == MAX_SPRITES)
            continue;
        spriteId = objEvent->spriteId;
        sprite = &gSprites[spriteId];
        if (!sprite->inUse)
            continue;
        if (spriteId == gPlayerAvatar.spriteId)
            continue;
        if (IS_PANEL_UI_TAG(sprite->template->tileTag))
            continue;
        if (sprite->callback == UpdateShadowFieldEffect)
            continue;

        u8 palNum = sprite->oam.paletteNum;
        sSavedSpritePals[spriteId] = palNum;
        if (palNum < 16) {
            sSavedPaletteTags[spriteId] = GetSpritePaletteTagByPaletteNum(palNum);
            for (j = 0; j < 16; j++)
                sSavedPaletteColors[spriteId][j] = gPlttBufferUnfaded[256 + (palNum * 16) + j]; //HYDRA ORIGINAL colours, not weather-altered faded
        }
    }

    //HYDRA now desaturate the unfaded buffer too (skip the panel's own BG bank so the side
    // panel stays coloured). Done AFTER STEP 1 so the sprite originals saved above are the
    // true colours. The grayscale sprite slot + panel UI palettes are (re)loaded afterwards.
    for (i = 0; i < 256; i++) {
        if (IS_UI_BANK(i)) //HYDRA keep panel + start-menu window banks coloured in unfaded as well
            continue;
        gPlttBufferUnfaded[i] = DesaturateColor(gPlttBufferUnfaded[i]);
    }
    {
        int p;
        for (p = 0; p < 16; p++) {
            if (!HydraShouldGrayObjPal(p)) //HYDRA keep the shadow (and UI/grayscale) out of the grey
                continue;
            for (i = 0; i < 16; i++)
                gPlttBufferUnfaded[256 + p * 16 + i] = DarkenGray(DesaturateColor(gPlttBufferUnfaded[256 + p * 16 + i])); //HYDRA grey + darken sprite (unfaded)
        }
    }

    // === STEP 2: free the overworld sprite palettes ===
    for (objEvent = gObjectEvents; objEvent < &gObjectEvents[OBJECT_EVENTS_COUNT]; objEvent++)
    {
        u8 spriteId;
        struct Sprite *sprite;
        if (!objEvent->active || objEvent->spriteId == MAX_SPRITES)
            continue;
        spriteId = objEvent->spriteId;
        sprite = &gSprites[spriteId];
        if (!sprite->inUse)
            continue;
        if (spriteId == gPlayerAvatar.spriteId)
            continue;
        if (IS_PANEL_UI_TAG(sprite->template->tileTag))
            continue;
        if (sprite->callback == UpdateShadowFieldEffect)
            continue;

        u8 palNum = sSavedSpritePals[spriteId];
        if (palNum != 0 && palNum < 16) {
            u16 actualTag = sSavedPaletteTags[spriteId];
            if (actualTag != 0xFFFF)
                FreeSpritePaletteByTag(actualTag);
        }
    }

    // === STEP 3: load the grayscale palette (darkened by the GRAY_SPRITE_MAX knob) into a freed slot ===
    {
        u16 grayDark[16]; //HYDRA darkened copy of the overworld-sprite ramp (all NPCs/followers use it)
        for (i = 0; i < 16; i++)
            grayDark[i] = DarkenGray(sGrayscalePalette[i]);
        struct SpritePalette grayscalePal = { .data = grayDark, .tag = GRAYSCALE_PAL_TAG };
        grayPalNum = LoadSpritePalette(&grayscalePal);
    }

    // === STEP 4: assign every overworld sprite to the grayscale palette ===
    for (objEvent = gObjectEvents; objEvent < &gObjectEvents[OBJECT_EVENTS_COUNT]; objEvent++)
    {
        u8 spriteId;
        struct Sprite *sprite;
        if (!objEvent->active || objEvent->spriteId == MAX_SPRITES)
            continue;
        spriteId = objEvent->spriteId;
        sprite = &gSprites[spriteId];
        if (!sprite->inUse)
            continue;
        if (spriteId == gPlayerAvatar.spriteId)
            continue;
        if (IS_PANEL_UI_TAG(sprite->template->tileTag))
            continue;
        if (sprite->callback == UpdateShadowFieldEffect)
            continue;
        if (grayPalNum != 0xFF)
            sprite->oam.paletteNum = grayPalNum;
    }

    //HYDRA flush OAM so the grayscale assignment reaches hardware this frame, before the
    // panel loads its own sprite palettes into the just-freed slots. Palette writes hit
    // gPlttBufferFaded immediately but oam.paletteNum only reaches hardware at the next
    // BuildOamBuffer; without this flush a VBlank mid-open shows the wrong palettes for a frame.
    BuildOamBuffer();
    sGrayApplied = TRUE; //HYDRA grayscale now on
}

static void RemoveGrayscaleEffect(void)
{
    int i;
    struct ObjectEvent *objEvent;

    //HYDRA Clear grayscale flag FIRST so ApplyColorMap doesn't block weather reapplication
    // during the restoration process. All subsequent UpdateSpritePaletteWithWeather() calls
    // need to be able to reach ApplyColorMap to reapply the weather darkening tint.
    sGrayApplied = FALSE;

    // === RESTORE BG PALETTES ===
    for (i = 0; i < 512; i++)
        gPlttBufferFaded[i] = sSavedBGPalettes[i];

    // === RESTORE ORIGINAL OBJ PALETTE 0 ===
    for (i = 0; i < 16; i++)
        gPlttBufferFaded[256 + i] = sSavedPalette0[i];

    //HYDRA restore the unfaded buffer we desaturated (skip banks 13/14/15 - panel + start-menu
    // window UI - which were never desaturated; OBJ palettes 256+ are still restored).
    for (i = 0; i < 512; i++) {
        if (IS_UI_BANK(i))
            continue;
        gPlttBufferUnfaded[i] = sSavedUnfadedAll[i];
    }

    // === FREE GRAYSCALE PALETTE ===
    FreeSpritePaletteByTag(GRAYSCALE_PAL_TAG);

    //HYDRA reload the general field-effect palettes (tall grass = FLDEFF_PAL_TAG_GENERAL_1, plus
    // GENERAL_0) from their SOURCE data. The slot snapshot restored above can be garbage: on the
    // return-from-party-menu re-open, ApplyGrayscaleEffect snapshots these slots BEFORE the grass
    // field effect has respawned, so the snapshot holds the placeholder colour (magenta/black).
    // Restoring that snapshot on close showed black/garbage grass until movement respawned it
    // (SS5/SS6). Writing the true source colours + re-applying weather fixes the grass instantly.
    {
        u8 fslot;
        fslot = IndexOfSpritePaletteTag(gSpritePalette_GeneralFieldEffect0.tag);
        if (fslot != 0xFF) {
            LoadPalette(gSpritePalette_GeneralFieldEffect0.data, OBJ_PLTT_ID(fslot), PLTT_SIZE_4BPP);
            if (gWeatherPtr->currWeather != WEATHER_FOG_HORIZONTAL)
                UpdateSpritePaletteWithWeather(fslot, FALSE);
        }
        fslot = IndexOfSpritePaletteTag(gSpritePalette_GeneralFieldEffect1.tag);
        if (fslot != 0xFF) {
            LoadPalette(gSpritePalette_GeneralFieldEffect1.data, OBJ_PLTT_ID(fslot), PLTT_SIZE_4BPP);
            if (gWeatherPtr->currWeather != WEATHER_FOG_HORIZONTAL)
                UpdateSpritePaletteWithWeather(fslot, FALSE);
        }
    }

    for (objEvent = gObjectEvents; objEvent < &gObjectEvents[OBJECT_EVENTS_COUNT]; objEvent++) {
        if (!objEvent->active || objEvent->spriteId == MAX_SPRITES)
            continue;
        u8 spriteId = objEvent->spriteId;
        struct Sprite *sprite = &gSprites[spriteId];
        if (!sprite->inUse)
            continue;
        if (spriteId == gPlayerAvatar.spriteId)
            continue;
        if (IS_PANEL_UI_TAG(sprite->template->tileTag))
            continue;
        if (sprite->callback == UpdateShadowFieldEffect)
            continue;

        if (sSavedSpritePals[spriteId] != 0xFF && sSavedSpritePals[spriteId] < 16) {
            u8 originalPalNum = sSavedSpritePals[spriteId];
            u16 actualTag = sSavedPaletteTags[spriteId];
            if (originalPalNum == 0) {
                sprite->oam.paletteNum = 0;
                continue;
            }
            //HYDRA rebuild from ORIGINAL (unfaded) colours, then re-apply weather/time so
            // gPlttBufferUnfaded is never polluted. This is what caused the TOD tint to
            // "super"-compound when the hotbar was opened right after the start menu.
            u8 slot = IndexOfSpritePaletteTag(actualTag);
            if (slot == 0xFF) {
                struct SpritePalette tempPal;
                tempPal.tag = actualTag;
                tempPal.data = sSavedPaletteColors[spriteId];
                slot = LoadSpritePalette(&tempPal);
                if (slot != 0xFF && gWeatherPtr->currWeather != WEATHER_FOG_HORIZONTAL)
                    UpdateSpritePaletteWithWeather(slot, FALSE);
            }
            sprite->oam.paletteNum = (slot != 0xFF) ? slot : 0;
        }
    }

    //HYDRA re-apply weather/time to the player's palette too (the loop above skips the player).
    if (gPlayerAvatar.spriteId < MAX_SPRITES)
    {
        struct Sprite *playerSprite = &gSprites[gPlayerAvatar.spriteId];
        if (playerSprite->inUse && gWeatherPtr->currWeather != WEATHER_FOG_HORIZONTAL)
            UpdateSpritePaletteWithWeather(playerSprite->oam.paletteNum, FALSE);
    }
    // sGrayApplied is now cleared at the start of this function
}

//HYDRA Re-apply grayscale AFTER the return-from-submenu fade settles. On that path the field
// reloads and the object-event system re-loads sprite palettes (coloured + weather) DURING the
// FadeInFromBlack, overriding the grayscale that ApplyGrayscaleEffect set earlier, and the TOD
// fade re-tints the BG. Called once from Task_WaitForFadeShowStartMenu when the fade is done.
// Only re-desaturates the display (faded) and re-points sprites at the existing grayscale slot;
// it does NOT touch the saved snapshots, so RemoveGrayscaleEffect still restores true colours.
void HydraPartyPanel_ReapplyGrayscale(void)
{
    int i;
    struct ObjectEvent *objEvent;
    u8 grayPalNum;

    if (!sPanelOpen)
        return;
    grayPalNum = IndexOfSpritePaletteTag(GRAYSCALE_PAL_TAG);
    if (grayPalNum == 0xFF)
        return;

    //HYDRA Re-desaturate the BG (except the panel's own bank) in BOTH buffers. Faded strips the
    // return-fade's TOD tint; UNFADED matters because FadeInFromBlack reveals FROM unfaded - the
    // field reload re-loads coloured palettes there mid-fade (the tall grass especially), so
    // greying unfaded too keeps the reveal grey instead of flashing colour for a few frames (SS3->SS4).
    for (i = 0; i < 256; i++) {
        if (IS_UI_BANK(i))
            continue;
        gPlttBufferFaded[i]   = DesaturateColor(gPlttBufferFaded[i]);
        gPlttBufferUnfaded[i] = DesaturateColor(gPlttBufferUnfaded[i]);
    }

    // Re-desaturate each OBJ palette slot (player, tall-grass and other field effects the
    // object-event system re-loaded coloured during the fade) except shadow / UI / grayscale.
    // Both buffers, same reason as the BG above.
    {
        int p;
        for (p = 0; p < 16; p++) {
            if (!HydraShouldGrayObjPal(p))
                continue;
            for (i = 0; i < 16; i++) {
                gPlttBufferFaded[256 + p * 16 + i]   = DarkenGray(DesaturateColor(gPlttBufferFaded[256 + p * 16 + i]));   //HYDRA grey + darken sprite (reapply)
                gPlttBufferUnfaded[256 + p * 16 + i] = DarkenGray(DesaturateColor(gPlttBufferUnfaded[256 + p * 16 + i])); //HYDRA grey + darken sprite (reapply, unfaded)
            }
        }
    }

    // Re-point every overworld sprite back at the grayscale palette slot.
    for (objEvent = gObjectEvents; objEvent < &gObjectEvents[OBJECT_EVENTS_COUNT]; objEvent++) {
        u8 spriteId;
        struct Sprite *sprite;
        if (!objEvent->active || objEvent->spriteId == MAX_SPRITES)
            continue;
        spriteId = objEvent->spriteId;
        sprite = &gSprites[spriteId];
        if (!sprite->inUse)
            continue;
        if (spriteId == gPlayerAvatar.spriteId)
            continue;
        if (IS_PANEL_UI_TAG(sprite->template->tileTag))
            continue;
        if (sprite->callback == UpdateShadowFieldEffect)
            continue;
        sprite->oam.paletteNum = grayPalNum;
    }
    BuildOamBuffer();
}

static struct Pokemon *Panel_Mon(u8 slot)
{
    return &gParties[B_TRAINER_PLAYER][slot];
}

// Faithful GetMonAilment (HP 0 -> FNT), replicated to avoid a heavy include.
static u8 Panel_Ailment(struct Pokemon *mon)
{
    u32 status;

    if (GetMonData(mon, MON_DATA_HP) == 0)
        return AILMENT_FNT;
    status = GetMonData(mon, MON_DATA_STATUS);
    if (status & STATUS1_PSN_ANY)   return AILMENT_PSN;
    if (status & STATUS1_PARALYSIS) return AILMENT_PRZ;
    if (status & STATUS1_SLEEP)     return AILMENT_SLP;
    if (status & STATUS1_FREEZE)    return AILMENT_FRZ;
    if (status & STATUS1_BURN)      return AILMENT_BRN;
    if (status & STATUS1_FROSTBITE) return AILMENT_FRB;
    return AILMENT_NONE;
}

static bool8 Panel_Selected(u8 slot)
{
    return sPanelFocus && slot == sPanelCursor;
}

bool8 HydraPartyPanel_IsOpen(void)
{
    return sPanelOpen;
}

bool8 HydraPartyPanel_HasFocus(void)
{
    return sPanelOpen && sPanelFocus;
}

void HydraPartyPanel_Open(void)
{
    if (sPanelOpen)
        HydraPartyPanel_Close();

    sPanelCount = CalculatePlayerPartyCount();
    if (sPanelCount == 0)
        return;

    if (sPanelCursor >= sPanelCount)
        sPanelCursor = 0;
    sPanelFocus = sReturnFocused; //HYDRA restore focus on the same mon after a panel submenu (SUMMARY)
    sReturnFocused = FALSE;
    sPanelWin = WINDOW_NONE;  // Initialize here instead
    sActionWin = WINDOW_NONE; //HYDRA action-menu state (EWRAM initialisers aren't honoured)
    sActionOpen = FALSE;
    sSwitchMode = FALSE;
    sSwitchSliding = FALSE; //HYDRA never resume a slide from a previous panel session
    sMoveMode = FALSE;      //HYDRA never resume an item-move from a previous panel session

    sBoxGfx = malloc_and_decompress(gPartyMenuBg_Gfx, NULL);

    //HYDRA free bank-13 slots 14 & 15 for the selection accent: the "HP" label tiles were 2-tone
    // yellow(14)/orange(15); repoint their pixels to the single orange already at slot 11 so the
    // label stays orange and slots 14/15 become available for ACC_BODY_IDX/ACC_BORDER_IDX.
    if (sBoxGfx != NULL)
    {
        static const u8 sHpLabelTiles[] = { T_HP_L, T_HP_R };
        u16 ti, bi;
        for (ti = 0; ti < ARRAY_COUNT(sHpLabelTiles); ti++)
        {
            u8 *tile = &sBoxGfx[sHpLabelTiles[ti] * 32];
            for (bi = 0; bi < 32; bi++)
            {
                u8 lo = tile[bi] & 0xF, hi = (tile[bi] >> 4) & 0xF;
                if (lo == 14 || lo == 15) lo = 11;
                if (hi == 14 || hi == 15) hi = 11;
                tile[bi] = (hi << 4) | lo;
            }
        }
    }

    // Bank 13: party box colours (bank 3) + red at 11/12 for low-HP + our red overpaints.
    LoadPalette(&gPartyMenuBg_Pal[48], BG_PLTT_ID(PANEL_PAL), PLTT_SIZE_4BPP);
    LoadPalette(&gPartyMenuBg_Pal[89], BG_PLTT_ID(PANEL_PAL) + 11, PLTT_SIZEOF(2));
    //HYDRA load a DARK red-orange into bank-13 index 6 (unused by the panel). The fainted box remap
    // sends the box's dark-blue (index 8) here so the bottom band turns red instead of staying blue.
    LoadPalette(&gPartyMenuBg_Pal[86], BG_PLTT_ID(PANEL_PAL) + 6, PLTT_SIZEOF(1));

    LoadCompressedSpriteSheet(&sSpriteSheet_Ball);
    LoadCompressedSpriteSheet(&sSpriteSheet_Status);
    LoadSpriteSheet(&gSpriteSheet_HeldItem); //HYDRA held-item indicator gfx (uncompressed)

    // Apply grayscale BEFORE creating panel window/sprites
    //HYDRA a normal open (or the POKEMON-submenu return, where the field reloaded fresh COLOUR
    // palettes) re-applies grayscale. The SAVE-cancel re-open is the exception: the field was NOT
    // reloaded, so grayscale is still on the (unchanged) palettes - re-applying it would snapshot the
    // grey colours as the "originals" and lose the real ones. Skip it there and consume the flag.
    if (!sSaveKeepGray)
    {
        ApplyGrayscaleEffect();
        FreezeObjectEvents(); //HYDRA pause the overworld while the start-menu panel is open
    }
    else
    {
        sSaveKeepGray = FALSE;
    }

    //HYDRA load the panel UI sprite palettes AFTER grayscale so they keep their colour
    // (the in-place OBJ desaturation above would otherwise grey the red pokeball / status).
    LoadSpritePalette(&sSpritePalette_Ball);
    LoadSpritePalette(&sSpritePalette_Status);
    LoadSpritePalette(&sHeldPanelPal); //HYDRA held-item indicator palette (blend-immune)

    Panel_CreateWindow();
    Panel_LoadSprites();
    Panel_DrawAll();

    sPanelOpen = TRUE;
}

void HydraPartyPanel_CloseKeepGray(void)
{
    //HYDRA tear down the panel UI but LEAVE grayscale applied. Used on start-menu -> submenu
    // transitions (POKEMON, BAG, ...), where the screen has already faded to black and the
    // submenu loads its own palettes. Restoring the colour palettes here would flash colour
    // onto the black transition frame. The field reloads on return, re-applying grayscale.
    if (!sPanelOpen)
        return;

    Panel_TeardownActionUI(); //HYDRA free the action-menu window if it was up
    Panel_FreeSprites();
    Panel_FreeWindow();
    FreeSpriteTilesByTag(TAG_POKEBALL);
    FreeSpritePaletteByTag(TAG_POKEBALL);
    FreeSpriteTilesByTag(TAG_STATUS);
    FreeSpritePaletteByTag(TAG_STATUS);
    FreeSpriteTilesByTag(TAG_HELD_PANEL);   //HYDRA held-item indicator
    FreeSpritePaletteByTag(TAG_HELD_PANEL); //HYDRA
    if (sBoxGfx != NULL)
    {
        Free(sBoxGfx);
        sBoxGfx = NULL;
    }
    sPanelOpen = FALSE;
    sPanelFocus = FALSE;
    // Intentionally NOT calling RemoveGrayscaleEffect() nor UnfreezeObjectEvents().
}

void HydraPartyPanel_Close(void)
{
    if (!sPanelOpen)
        return;

    Panel_TeardownActionUI(); //HYDRA free the action-menu window if it was up
    Panel_FreeSprites();
    Panel_FreeWindow();
    FreeSpriteTilesByTag(TAG_POKEBALL);
    FreeSpritePaletteByTag(TAG_POKEBALL);
    FreeSpriteTilesByTag(TAG_STATUS);
    FreeSpritePaletteByTag(TAG_STATUS);
    FreeSpriteTilesByTag(TAG_HELD_PANEL);   //HYDRA held-item indicator
    FreeSpritePaletteByTag(TAG_HELD_PANEL); //HYDRA
    if (sBoxGfx != NULL)
    {
        Free(sBoxGfx);
        sBoxGfx = NULL;
    }
    
    //HYDRA clear sPanelOpen BEFORE RemoveGrayscaleEffect: the weather/TOD guards key off
    // HydraPartyPanel_IsOpen(), and RemoveGrayscaleEffect re-applies weather to the sprites
    // via ApplyColorMap - which would be blocked (leaving them untinted) if we were still open.
    sPanelOpen = FALSE;
    sPanelFocus = FALSE;

    // Remove grayscale AFTER freeing panel sprites
    RemoveGrayscaleEffect();
    UnfreezeObjectEvents(); //HYDRA resume the overworld when the panel closes

    //HYDRA re-apply the weather colour map on close so the weather sprites (rain/ash/snow - all on
    // PALTAG_WEATHER, 0x1200) get their correct colours back. On the return-from-party-menu path the
    // grayscale snapshot restored a STALE faded weather palette; ApplyColorMap rebuilds faded from the
    // (correct) unfaded base that RemoveGrayscaleEffect just restored. Runs only when weather is idle.
    ApplyWeatherColorMapIfIdle(gWeatherPtr->colorMapIndex);

    //HYDRA re-apply the time-of-day tint to the BG immediately on close. On the return-from-party-
    // menu re-open the grayscale snapshot (sSavedBGPalettes) was captured before TOD had settled, so
    // restoring it left the BG at full brightness with no night tint (SS6). RemoveGrayscaleEffect
    // has just restored the TRUE colours into gPlttBufferUnfaded, so re-running the TOD mix rebuilds
    // the tinted faded buffer correctly. UpdateTimeOfDay now runs (sPanelOpen is already FALSE).
    if (MapHasNaturalLight(gMapHeader.mapType))
    {
        UpdateTimeOfDay();
        UpdatePalettesWithTime(PALETTES_MAP);
    }

    //HYDRA if a SWITCH changed the party order (possibly the lead mon), refresh the overworld
    // following pokemon so it matches the new party 0 once the field resumes.
    if (sPartyOrderChanged)
    {
        sPartyOrderChanged = FALSE;
        UpdateFollowingPokemon();
    }
}

//HYDRA restore true colours when grayscale is applied but the panel UI is already torn down
// (the keep-gray SAVE flow). Mirrors the colour-restore half of HydraPartyPanel_Close so that,
// once saving finishes and the menu closes, everything is restored exactly like closing the
// hotbar / start menu.
void HydraPartyPanel_RestoreColors(void)
{
    sSaveKeepGray = FALSE;
    if (!sGrayApplied)
        return;
    RemoveGrayscaleEffect();
    UnfreezeObjectEvents();
    ApplyWeatherColorMapIfIdle(gWeatherPtr->colorMapIndex);
    if (MapHasNaturalLight(gMapHeader.mapType))
    {
        UpdateTimeOfDay();
        UpdatePalettesWithTime(PALETTES_MAP);
    }
}

//HYDRA true whenever the grayscale effect is on the screen - even if the panel UI itself is torn
// down (SAVE dialog). The overworld weather/TOD guards use this so the field stays PURE grey while
// the save dialog is up (sPanelOpen is FALSE there, so IsOpen() alone would let the TOD tick re-tint).
bool8 HydraPartyPanel_IsGrayApplied(void)
{
    return sGrayApplied;
}

//HYDRA mark that the SAVE dialog is opening: keep grayscale, and make the cancel re-open skip
// re-applying it (the field is never reloaded during a save).
void HydraPartyPanel_BeginSave(void)
{
    sSaveKeepGray = TRUE;
}

//HYDRA ================= action menu (SUMMARY / SWITCH / ITEM -> GIVE/TAKE/MOVE) =================
// baseBlock 0x1C8 sits in the free gap between the largest start menu (9 actions ends ~0x1C5) and
// the DLG frame gfx at 0x200 - i.e. BELOW the panel window (0x230..0x2F8), which is the proven-good
// bg0 range. The old 0x300 spilled past bg0's char space and corrupted the overworld tiles.
#define ACTION_WIN_LEFT   13   //HYDRA +1 tile right (windows snap to 8px; ~9px asked)
#define ACTION_WIN_TOP    12   //HYDRA +2 tiles down (16px; ~20px asked - 8px tile granularity)
#define ACTION_WIN_W      7
#define ACTION_WIN_H      7
#define ACTION_WIN_BASE   0x1C8
#define ACTION_COUNT      3

static const u8 sText_ActSummary[] = _("SUMMARY");
static const u8 sText_ActSwitch[]  = _("SWITCH");
static const u8 sText_ActItem[]    = _("ITEM");
static const u8 sText_ActGive[]    = _("GIVE");
static const u8 sText_ActTake[]    = _("TAKE");
static const u8 sText_ActMove[]    = _("MOVE");
static const u8 *const sActionMain[ACTION_COUNT] = { sText_ActSummary, sText_ActSwitch, sText_ActItem };
static const u8 *const sActionItem[ACTION_COUNT] = { sText_ActGive, sText_ActTake, sText_ActMove };

static void Panel_DrawActionMenu(void)
{
    const u8 *const *opts = (sActionMode == 1) ? sActionItem : sActionMain;
    u8 i;
    if (sActionWin == WINDOW_NONE)
        return;
    FillWindowPixelBuffer(sActionWin, PIXEL_FILL(1));
    for (i = 0; i < ACTION_COUNT; i++)
        AddTextPrinterParameterized(sActionWin, FONT_NORMAL, opts[i], 8, i * 16 + 2, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(sActionWin, FONT_NORMAL, gText_SelectorArrow2, 0, sActionCursor * 16 + 2, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sActionWin, COPYWIN_GFX);
}

static void Panel_OpenActionMenu(void)
{
    struct WindowTemplate t;
    t.bg = 0;
    t.tilemapLeft = ACTION_WIN_LEFT;
    t.tilemapTop = ACTION_WIN_TOP;
    t.width = ACTION_WIN_W;
    t.height = ACTION_WIN_H;
    t.paletteNum = 15;   // DLG window palette (kept coloured by IS_UI_BANK)
    t.baseBlock = ACTION_WIN_BASE;
    sActionWin = AddWindow(&t);
    sActionMode = 0;
    sActionCursor = 0;
    DrawStdWindowFrame(sActionWin, FALSE);
    Panel_DrawActionMenu();
    PutWindowTilemap(sActionWin);
    CopyWindowToVram(sActionWin, COPYWIN_FULL);
    sActionOpen = TRUE;
}

static void Panel_CloseActionMenu(void)
{
    if (sActionWin != WINDOW_NONE)
    {
        ClearStdWindowAndFrame(sActionWin, TRUE);
        RemoveWindow(sActionWin);
        sActionWin = WINDOW_NONE;
    }
    sActionOpen = FALSE;
    sActionMode = 0;
}

//HYDRA frees the action window on any teardown (submenu transition or full close)
static void Panel_TeardownActionUI(void)
{
    if (sActionWin != WINDOW_NONE)
    {
        ClearStdWindowAndFrame(sActionWin, TRUE);
        RemoveWindow(sActionWin);
        sActionWin = WINDOW_NONE;
    }
    sActionOpen = FALSE;
    sActionMode = 0;
    sSwitchMode = FALSE;
}

u8 HydraPartyPanel_GetSlot(void)
{
    return sPanelCursor;
}

//HYDRA TAKE the selected mon's held item back into the bag (in-place, no dialog). Refreshes the
// panel so the held-item indicator disappears. Bag-full / not-holding just do nothing.
static void Panel_TakeHeldItem(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][sPanelCursor];
    enum Item item = GetMonData(mon, MON_DATA_HELD_ITEM);
    if (item != ITEM_NONE && CheckBagHasSpace(item, 1))
    {
        enum Item none = ITEM_NONE;
        AddBagItem(item, 1);
        SetMonData(mon, MON_DATA_HELD_ITEM, &none);
        Panel_FreeSprites();
        Panel_LoadSprites();
        Panel_DrawAll();
    }
}

static enum HydraPanelInputResult Panel_HandleActionInput(void)
{
    if (JOY_NEW(DPAD_UP))
    {
        PlaySE(SE_SELECT);
        sActionCursor = (sActionCursor == 0) ? ACTION_COUNT - 1 : sActionCursor - 1;
        Panel_DrawActionMenu();
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(DPAD_DOWN))
    {
        PlaySE(SE_SELECT);
        sActionCursor = (sActionCursor + 1) % ACTION_COUNT;
        Panel_DrawActionMenu();
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        if (sActionMode == 1)          // B in the ITEM submenu -> back to the main action menu
        {
            sActionMode = 0;
            sActionCursor = 2;         // land back on ITEM
            Panel_DrawActionMenu();
        }
        else                           // B in the main menu -> close it (back to the focused panel)
        {
            Panel_CloseActionMenu();
        }
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        if (sActionMode == 0)
        {
            switch (sActionCursor)
            {
            case 0: // SUMMARY
                sReturnFocused = TRUE; //HYDRA come back with the panel focused on this mon
                Panel_CloseActionMenu();
                return HYDRA_PANEL_INPUT_SUMMARY;
            case 1: // SWITCH -> pick a second mon to swap with, in-panel
                Panel_CloseActionMenu();
                sSwitchMode = TRUE;
                sSwitchFrom = sPanelCursor;
                Panel_RedrawBox(sSwitchFrom); //HYDRA show the green accent on the source box now
                return HYDRA_PANEL_INPUT_CONSUMED;
            default: // ITEM -> open the GIVE/TAKE/MOVE submenu in place
                sActionMode = 1;
                sActionCursor = 0;
                Panel_DrawActionMenu();
                return HYDRA_PANEL_INPUT_CONSUMED;
            }
        }
        else // ITEM submenu: GIVE / TAKE / MOVE
        {
            switch (sActionCursor)
            {
            case 0: // GIVE -> open the bag to give an item to this mon (screen transition, like SUMMARY)
                sReturnFocused = TRUE;   //HYDRA come back with the panel focused on this mon
                Panel_CloseActionMenu();
                return HYDRA_PANEL_INPUT_GIVE;
            case 1: // TAKE (in-place)
                Panel_TakeHeldItem();
                Panel_CloseActionMenu();
                return HYDRA_PANEL_INPUT_CONSUMED;
            case 2: // MOVE -> pick another mon to move this mon's held item to (in-panel, no bag)
                {
                    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][sPanelCursor];
                    enum Item held = GetMonData(mon, MON_DATA_HELD_ITEM);
                    if (held == ITEM_NONE || ItemIsMail(held)) //HYDRA nothing to move / mail can't be moved safely
                    {
                        PlaySE(SE_FAILURE);
                        return HYDRA_PANEL_INPUT_CONSUMED; // stay in the item submenu
                    }
                    sMoveFrom = sPanelCursor;
                    sMoveMode = TRUE;
                    Panel_CloseActionMenu();
                    Panel_RedrawBox(sMoveFrom); //HYDRA show the green accent on the source box now
                    return HYDRA_PANEL_INPUT_CONSUMED;
                }
            default:
                Panel_CloseActionMenu();
                return HYDRA_PANEL_INPUT_CONSUMED;
            }
        }
    }
    return HYDRA_PANEL_INPUT_CONSUMED;
}

//HYDRA move the held item of sMoveFrom onto another chosen mon (in-panel, no bag). If the target
// already holds an item the two held items swap (lossless); mail is refused on either side. This
// mirrors the SWITCH target picker, so DPAD moves the cursor, B cancels and A confirms.
static enum HydraPanelInputResult Panel_HandleMoveInput(void)
{
    if (JOY_NEW(DPAD_UP))
    {
        Panel_MoveCursor(-1);
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(DPAD_DOWN))
    {
        Panel_MoveCursor(1);
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        sMoveMode = FALSE;
        Panel_DrawAll(); //HYDRA clear the green accent off the source box
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(A_BUTTON))
    {
        if (sPanelCursor != sMoveFrom)
        {
            struct Pokemon *src = &gParties[B_TRAINER_PLAYER][sMoveFrom];
            struct Pokemon *dst = &gParties[B_TRAINER_PLAYER][sPanelCursor];
            enum Item srcItem = GetMonData(src, MON_DATA_HELD_ITEM);
            enum Item dstItem = GetMonData(dst, MON_DATA_HELD_ITEM);
            if (ItemIsMail(srcItem) || ItemIsMail(dstItem)) //HYDRA never move onto/over mail
            {
                PlaySE(SE_FAILURE);
                sMoveMode = FALSE;
                Panel_DrawAll(); //HYDRA clear the green accent
                return HYDRA_PANEL_INPUT_CONSUMED;
            }
            PlaySE(SE_SELECT);
            SetMonData(dst, MON_DATA_HELD_ITEM, &srcItem); //HYDRA target receives the moved item
            SetMonData(src, MON_DATA_HELD_ITEM, &dstItem); //HYDRA source gets the target's old item (or none)
            sMoveMode = FALSE; //HYDRA clear before redraw so the accent green is not re-applied
            Panel_FreeSprites();
            Panel_LoadSprites();
            Panel_DrawAll();
        }
        else
        {
            PlaySE(SE_SELECT); //HYDRA picked the same mon: nothing to move, just cancel
            sMoveMode = FALSE;
            Panel_DrawAll();
        }
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    return HYDRA_PANEL_INPUT_CONSUMED;
}

//HYDRA position the two swapping mons' sprites (icon/ball/status/held) for the current slide
// frame. Both groups share one per-row step (ICON_STEP_PX == BOX_STEP), so the vertical delta is
// uniform; slot A slides toward slot B and B toward A. SpriteCB_MonIcon only advances the icon
// animation frame (never touches sprite->y), so writing y here is safe.
static void Panel_SlidePositionSprites(void)
{
    u8 a = sSwitchFrom, b = sSwitchTo;
    s16 num = sSlideStep, den = SWITCH_SLIDE_FRAMES;
    s16 aIcon = ICON_TOP_PX + a * ICON_STEP_PX;
    s16 bIcon = ICON_TOP_PX + b * ICON_STEP_PX;
    s16 aBox  = PANEL_TOP_PX + a * BOX_STEP;
    s16 bBox  = PANEL_TOP_PX + b * BOX_STEP;
    s16 aIconY = aIcon + (bIcon - aIcon) * num / den; //HYDRA A -> B
    s16 bIconY = bIcon + (aIcon - bIcon) * num / den; //HYDRA B -> A
    s16 aBoxY  = aBox  + (bBox  - aBox)  * num / den;
    s16 bBoxY  = bBox  + (aBox  - bBox)  * num / den;

    if (sIconSpriteIds[a]   != SPRITE_NONE && sIconSpriteIds[a]   != MAX_SPRITES) gSprites[sIconSpriteIds[a]].y   = aIconY;
    if (sIconSpriteIds[b]   != SPRITE_NONE && sIconSpriteIds[b]   != MAX_SPRITES) gSprites[sIconSpriteIds[b]].y   = bIconY;
    if (sBallSpriteIds[a]   != SPRITE_NONE && sBallSpriteIds[a]   != MAX_SPRITES) gSprites[sBallSpriteIds[a]].y   = aIconY + BALL_Y_OFFSET;
    if (sBallSpriteIds[b]   != SPRITE_NONE && sBallSpriteIds[b]   != MAX_SPRITES) gSprites[sBallSpriteIds[b]].y   = bIconY + BALL_Y_OFFSET;
    if (sStatusSpriteIds[a] != SPRITE_NONE && sStatusSpriteIds[a] != MAX_SPRITES) gSprites[sStatusSpriteIds[a]].y = aBoxY + STATUS_DY;
    if (sStatusSpriteIds[b] != SPRITE_NONE && sStatusSpriteIds[b] != MAX_SPRITES) gSprites[sStatusSpriteIds[b]].y = bBoxY + STATUS_DY;
    if (sHeldItemIds[a]     != SPRITE_NONE && sHeldItemIds[a]     != MAX_SPRITES) gSprites[sHeldItemIds[a]].y     = aIconY + HELD_ITEM_DY;
    if (sHeldItemIds[b]     != SPRITE_NONE && sHeldItemIds[b]     != MAX_SPRITES) gSprites[sHeldItemIds[b]].y     = bIconY + HELD_ITEM_DY;
}

//HYDRA finish the slide: swap the actual party structs, then rebuild the sprites/boxes at their
// final (swapped) positions. Same data swap the old instant path used, just deferred to slide end.
static void Panel_SlideCommit(void)
{
    struct Pokemon tmp = gParties[B_TRAINER_PLAYER][sSwitchFrom];
    gParties[B_TRAINER_PLAYER][sSwitchFrom] = gParties[B_TRAINER_PLAYER][sSwitchTo];
    gParties[B_TRAINER_PLAYER][sSwitchTo] = tmp;
    sPartyOrderChanged = TRUE;   //HYDRA refresh the overworld follower on close
    Panel_FreeSprites();
    Panel_LoadSprites();
    Panel_DrawAll();
    sSwitchSliding = FALSE;
    sSlideStep = 0;
}

static enum HydraPanelInputResult Panel_HandleSwitchInput(void)
{
    if (JOY_NEW(DPAD_UP))
    {
        Panel_MoveCursor(-1);
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(DPAD_DOWN))
    {
        Panel_MoveCursor(1);
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        sSwitchMode = FALSE;
        Panel_DrawAll(); //HYDRA clear the green accent off the source box
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        if (sPanelCursor != sSwitchFrom)
        {
            //HYDRA start the slide instead of swapping instantly. The party-struct swap now happens
            // at slide end (Panel_SlideCommit), mirroring the pokemon party screen's slide but using
            // the panel's own sprites. The per-frame advance lives in HydraPartyPanel_HandleInput.
            sSwitchTo = sPanelCursor;
            sSlideStep = 0;
            sSwitchSliding = TRUE;
            sSwitchMode = FALSE;
            return HYDRA_PANEL_INPUT_CONSUMED;
        }
        sSwitchMode = FALSE;
        Panel_DrawAll(); //HYDRA same mon picked: clear the green accent
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    return HYDRA_PANEL_INPUT_CONSUMED;
}

enum HydraPanelInputResult HydraPartyPanel_HandleInput(void)
{
    if (!sPanelOpen)
        return HYDRA_PANEL_INPUT_NONE;

    if (sSwitchSliding)   //HYDRA run the SWITCH slide to completion; input is ignored while it plays
    {
        sSlideStep++;
        if (sSlideStep >= SWITCH_SLIDE_FRAMES)
            Panel_SlideCommit();
        else
            Panel_SlidePositionSprites();
        return HYDRA_PANEL_INPUT_CONSUMED;
    }

    if (sActionOpen)   //HYDRA action menu (SUMMARY/SWITCH/ITEM/CANCEL) has priority on input
        return Panel_HandleActionInput();
    if (sSwitchMode)   //HYDRA choosing the second mon to swap with
        return Panel_HandleSwitchInput();
    if (sMoveMode)     //HYDRA choosing the mon to MOVE the held item to
        return Panel_HandleMoveInput();

    if (!sPanelFocus)
    {
        if (JOY_NEW(DPAD_LEFT))
        {
            sPanelFocus = TRUE;
            PlaySE(SE_SELECT);
            Panel_UpdateBall(sPanelCursor, TRUE);
            Panel_RedrawBox(sPanelCursor);
            return HYDRA_PANEL_INPUT_CONSUMED;
        }
        return HYDRA_PANEL_INPUT_NONE;
    }

    if (JOY_NEW(DPAD_RIGHT))   //HYDRA Right leaves the panel back to the vanilla start-menu column
    {
        sPanelFocus = FALSE;
        PlaySE(SE_SELECT);
        Panel_UpdateBall(sPanelCursor, FALSE);
        Panel_RedrawBox(sPanelCursor);
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(B_BUTTON))     //HYDRA B closes the whole start menu + panel (same as B on the vanilla menu)
    {
        return HYDRA_PANEL_INPUT_CLOSE;
    }
    if (JOY_NEW(DPAD_UP))
    {
        Panel_MoveCursor(-1);
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(DPAD_DOWN))
    {
        Panel_MoveCursor(1);
        return HYDRA_PANEL_INPUT_CONSUMED;
    }
    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        Panel_OpenActionMenu(); //HYDRA A -> in-place SUMMARY/SWITCH/ITEM/CANCEL menu
        return HYDRA_PANEL_INPUT_CONSUMED;
    }

    return HYDRA_PANEL_INPUT_CONSUMED;
}

static void Panel_MoveCursor(s8 delta)
{
    u8 old = sPanelCursor;
    s8 next = (s8)sPanelCursor + delta;

    if (next < 0)
        next = sPanelCount - 1;
    else if (next >= sPanelCount)
        next = 0;

    if ((u8)next == old)
        return;

    sPanelCursor = (u8)next;
    PlaySE(SE_SELECT);
    Panel_UpdateBall(old, FALSE);
    Panel_UpdateBall(sPanelCursor, TRUE);
    Panel_RedrawBox(old);
    Panel_RedrawBox(sPanelCursor);
}

static void Panel_LoadSprites(void)
{
    u8 i;

    for (i = 0; i < PANEL_SLOTS; i++)
    {
        sIconSpriteIds[i] = SPRITE_NONE;
        sBallSpriteIds[i] = SPRITE_NONE;
        sStatusSpriteIds[i] = SPRITE_NONE;
        sHeldItemIds[i] = SPRITE_NONE; //HYDRA
    }

    for (i = 0; i < sPanelCount; i++)
    {
        struct Pokemon *mon = Panel_Mon(i);
        enum Species species = GetMonData(mon, MON_DATA_SPECIES_OR_EGG);
        u32 personality = GetMonData(mon, MON_DATA_PERSONALITY);
        u8 ailment = Panel_Ailment(mon);
        bool8 fainted = (ailment == AILMENT_FNT);
        bool8 sel = Panel_Selected(i);
        s16 iconY = ICON_TOP_PX + i * ICON_STEP_PX;
        s16 boxLeft = PANEL_WIN_LEFT * 8 + BOX_X;
        s16 boxTop = PANEL_TOP_PX + i * BOX_STEP;
        s16 ballX = ICON_X + BALL_X_OFFSET; //HYDRA ball no longer nudges left on select (open anim only)

        // pokeball holder
        sBallSpriteIds[i] = CreateSprite(&sSpriteTemplate_Ball, ballX, iconY + BALL_Y_OFFSET, 20);
        if (sBallSpriteIds[i] != MAX_SPRITES)
            StartSpriteAnim(&gSprites[sBallSpriteIds[i]], sel ? 1 : 0);

        // mon icon (fainted mons freeze: dummy callback instead of the bobbing one)
        LoadMonIconPalette(species);
        sIconSpriteIds[i] = CreateMonIcon(species, fainted ? SpriteCallbackDummy : SpriteCB_MonIcon,
                                          ICON_X, iconY, 0, personality);
        if (sIconSpriteIds[i] != MAX_SPRITES)
            gSprites[sIconSpriteIds[i]].oam.priority = 0;

        // status label, right of the gender
        sStatusSpriteIds[i] = CreateSprite(&sSpriteTemplate_Status, boxLeft + STATUS_DX, boxTop + STATUS_DY, 0);
        if (sStatusSpriteIds[i] != MAX_SPRITES)
        {
            if (ailment == AILMENT_NONE || ailment == AILMENT_PKRS)
                gSprites[sStatusSpriteIds[i]].invisible = TRUE;
            else
                StartSpriteAnim(&gSprites[sStatusSpriteIds[i]], ailment - 1);
        }

        //HYDRA held-item indicator: shown at the portrait's bottom-left when the mon holds an item
        // (yellow/red square) or mail (envelope), mirroring the pokemon party screen.
        {
            enum Item heldItem = GetMonData(mon, MON_DATA_HELD_ITEM);
            if (heldItem != ITEM_NONE)
            {
                sHeldItemIds[i] = CreateSprite(&sSpriteTemplate_HeldPanel, ICON_X + HELD_ITEM_DX, iconY + HELD_ITEM_DY, 0);
                if (sHeldItemIds[i] != MAX_SPRITES)
                {
                    gSprites[sHeldItemIds[i]].oam.priority = 0;
                    StartSpriteAnim(&gSprites[sHeldItemIds[i]], ItemIsMail(heldItem) ? 1 : 0);
                }
            }
        }
    }
}

static void Panel_FreeSprites(void)
{
    u8 i;

    for (i = 0; i < PANEL_SLOTS; i++)
    {
        if (sIconSpriteIds[i] != SPRITE_NONE && sIconSpriteIds[i] != MAX_SPRITES)
        {
            FreeAndDestroyMonIconSprite(&gSprites[sIconSpriteIds[i]]);
            sIconSpriteIds[i] = SPRITE_NONE;
        }
        if (sBallSpriteIds[i] != SPRITE_NONE && sBallSpriteIds[i] != MAX_SPRITES)
        {
            DestroySprite(&gSprites[sBallSpriteIds[i]]);
            sBallSpriteIds[i] = SPRITE_NONE;
        }
        if (sStatusSpriteIds[i] != SPRITE_NONE && sStatusSpriteIds[i] != MAX_SPRITES)
        {
            DestroySprite(&gSprites[sStatusSpriteIds[i]]);
            sStatusSpriteIds[i] = SPRITE_NONE;
        }
        if (sHeldItemIds[i] != SPRITE_NONE && sHeldItemIds[i] != MAX_SPRITES) //HYDRA
        {
            DestroySprite(&gSprites[sHeldItemIds[i]]);
            sHeldItemIds[i] = SPRITE_NONE;
        }
    }
    FreeMonIconPalettes();
}

static void Panel_UpdateBall(u8 slot, bool8 selected)
{
    if (sBallSpriteIds[slot] == SPRITE_NONE || sBallSpriteIds[slot] == MAX_SPRITES)
        return;
    gSprites[sBallSpriteIds[slot]].x = ICON_X + BALL_X_OFFSET; //HYDRA ball stays put on select (open anim only)
    StartSpriteAnim(&gSprites[sBallSpriteIds[slot]], selected ? 1 : 0);
}

static void Panel_CreateWindow(void)
{
    struct WindowTemplate t;

    t.bg = 0;
    t.tilemapLeft = PANEL_WIN_LEFT;
    t.tilemapTop = PANEL_WIN_TOP;
    t.width = PANEL_WIN_W;
    t.height = PANEL_WIN_H;
    t.paletteNum = PANEL_PAL;
    t.baseBlock = PANEL_WIN_BASE;
    sPanelWin = AddWindow(&t);
}

static void Panel_FreeWindow(void)
{
    if (sPanelWin != WINDOW_NONE)
    {
        ClearWindowTilemap(sPanelWin);
        CopyWindowToVram(sPanelWin, COPYWIN_MAP);
        RemoveWindow(sPanelWin);
        sPanelWin = WINDOW_NONE;
    }
}

//HYDRA when TRUE, Panel_Blit remaps the box graphic's structural blues to reds so a fainted box
// renders fully red (like the standard party menu) without a per-box palette bank: bank-13 index 5
// (box blue) -> 12 (red), index 7 (light blue) -> 11 (orange). Set per box in Panel_DrawBox.
static bool8 sBlitRed = FALSE;
static bool8 sBlitAccent = FALSE; //HYDRA remap box body{4,5}->ACC_BODY_IDX, border{7,8}->ACC_BORDER_IDX
static u8 sTileRemap[32];

//HYDRA one nibble remap for Panel_Blit. Kept out of the inner loop for readability.
// Normal boxes still remap 7->4: slots 4 and 7 are the SAME blue in the base palette, so this is
// lossless, and it keeps slot 7 free to serve as the accent body-mid tone (ACC_BODY2_IDX).
static inline u8 Panel_RemapNibble(u8 v)
{
    if (sBlitRed)   //HYDRA fainted: box blues -> reds (4->11 orange,5->12 red,7->11,8->6 dark red)
    {
        if      (v == 4) return 11; else if (v == 5) return 12;
        else if (v == 7) return 11; else if (v == 8) return 6;
        return v;
    }
    if (sBlitAccent) //HYDRA selection accent: body 4->light,5->mid; border 7,8->one tone; outline 1 kept
    {
        if      (v == 4) return ACC_BODY_IDX;   else if (v == 5) return ACC_BODY2_IDX;
        else if (v == 7 || v == 8) return ACC_BORDER_IDX;
        return v;
    }
    if (v == 7) return 4; //HYDRA normal box: dedup the duplicate blue so slot 7 stays free for accent
    return v;
}

static void Panel_Blit(u16 tileId, u8 px, u8 py)
{
    u32 i;

    if (sBoxGfx == NULL)
        return;

    for (i = 0; i < 32; i++)
    {
        u8 b = sBoxGfx[tileId * 32 + i];
        sTileRemap[i] = (Panel_RemapNibble((b >> 4) & 0xF) << 4) | Panel_RemapNibble(b & 0xF);
    }
    BlitBitmapToWindow(sPanelWin, sTileRemap, px, py, 8, 8);
}

//HYDRA load the current accent colour set into the two freed slots. GREEN for switch/move
// selection, lighter-BLUE for the browse highlight. Colours are the exact vanilla party-menu IDs.
static void Panel_LoadAccentPalette(u8 accent)
{
    u16 hi, mid, border;
    if (accent == ACC_GREEN)     { hi = PAL_ID_GREEN_BODY_HI; mid = PAL_ID_GREEN_BODY_MID; border = PAL_ID_GREEN_BORDER; }
    else if (accent == ACC_RED)  { hi = PAL_ID_RED_BODY_HI;   mid = PAL_ID_RED_BODY_MID;   border = PAL_ID_RED_BORDER;   }
    else                         { hi = PAL_ID_BLUE_BODY_HI;  mid = PAL_ID_BLUE_BODY_MID;  border = PAL_ID_BLUE_BORDER;  }
    LoadPalette(&gPartyMenuBg_Pal[hi],     BG_PLTT_ID(PANEL_PAL) + ACC_BODY_IDX,   PLTT_SIZEOF(1));
    LoadPalette(&gPartyMenuBg_Pal[mid],    BG_PLTT_ID(PANEL_PAL) + ACC_BODY2_IDX,  PLTT_SIZEOF(1));
    LoadPalette(&gPartyMenuBg_Pal[border], BG_PLTT_ID(PANEL_PAL) + ACC_BORDER_IDX, PLTT_SIZEOF(1));
}

//HYDRA which accent a box should show. Green while its mon is the SWITCH/MOVE source or is under
// the red cursor during switch/move; lighter-blue for the plain browse highlight. States are
// mutually exclusive, so every accent box on screen shares one colour set.
static u8 Panel_BoxAccent(u8 slot)
{
    if (sSwitchMode || sSwitchSliding)
        return (slot == sSwitchFrom || slot == sPanelCursor) ? ACC_GREEN : ACC_NONE;
    if (sMoveMode)
        return (slot == sMoveFrom || slot == sPanelCursor) ? ACC_GREEN : ACC_NONE;
    if (sPanelFocus && slot == sPanelCursor)
        return ACC_BLUE;
    return ACC_NONE;
}

static void Panel_DrawAll(void)
{
    u8 i;

    if (sPanelWin == WINDOW_NONE)
        return;

    FillWindowPixelBuffer(sPanelWin, PIXEL_FILL(0));
    for (i = 0; i < sPanelCount; i++)
        Panel_DrawBox(i);
    PutWindowTilemap(sPanelWin);
    CopyWindowToVram(sPanelWin, COPYWIN_FULL);
    RunTextPrinters(); //HYDRA render Lv/gender now so the box never shows for a frame without them (SS5)
}

static void Panel_RedrawBox(u8 slot)
{
    u8 bt, ty0, ty1;

    if (sPanelWin == WINDOW_NONE || slot >= sPanelCount)
        return;

    bt = PANEL_TOP_PX + slot * BOX_STEP;
    Panel_DrawBox(slot);
    ty0 = bt >> 3;
    ty1 = (bt + BOX_H - 1) >> 3;
    CopyWindowRectToVram(sPanelWin, COPYWIN_GFX, 0, ty0, PANEL_WIN_W, ty1 - ty0 + 1);
    RunTextPrinters(); //HYDRA render Lv/gender now so the highlighted box doesn't flash blank (SS5)
}

static void Panel_DrawBox(u8 slot)
{
    struct Pokemon *mon = Panel_Mon(slot);
    u16 hp, maxHp;
    u8 gender, frac, rem, topIdx, botIdx, e;
    u8 bl = BOX_X;
    u8 bt = PANEL_TOP_PX + slot * BOX_STEP;
    u8 rx = BOX_X + BOX_W - 8;
    u8 by = bt + BOX_H - 8;
    bool8 fainted = (Panel_Ailment(mon) == AILMENT_FNT);
    u32 pct;

    {
        u8 sel = Panel_BoxAccent(slot); //HYDRA SS1/SS2 selection state (none / highlight / action)
        if (sel != ACC_NONE)
        {
            //HYDRA fainted + selected shows the party menu's LIGHTER-RED, not blue/green (SS7/SS9 fix)
            u8 theme = fainted ? ACC_RED : sel;
            Panel_LoadAccentPalette(theme);
            sBlitAccent = TRUE;
            sBlitRed = FALSE;
        }
        else
        {
            sBlitAccent = FALSE;
            sBlitRed = fainted; //HYDRA plain fainted box (not selected): the red styling
        }
    }

    if (sBlitAccent)
    {
        //HYDRA 2-tone body gradient (SS1/SS2): lighter top band, mid tone below.
        u8 top = bt + BOX_TOP_INSET;
        FillWindowPixelRect(sPanelWin, ACC_BODY_IDX,  bl, top, BOX_W - 2, BODY_SPLIT_H);
        FillWindowPixelRect(sPanelWin, ACC_BODY2_IDX, bl, top + BODY_SPLIT_H, BOX_W - 2, (BOX_H - BOX_TOP_INSET) - BODY_SPLIT_H);
    }
    else
    {
        FillWindowPixelRect(sPanelWin, fainted ? RED_IDX : BLUE_IDX, bl, bt + BOX_TOP_INSET, BOX_W - 2, BOX_H - BOX_TOP_INSET);
    }

    Panel_Blit(T_TL, bl, bt);
    Panel_Blit(T_TR, rx, bt);
    Panel_Blit(T_BL, bl, by);
    Panel_Blit(T_BR, rx, by);
    for (e = bl + 8; e < rx; e += 8)
    {
        Panel_Blit(T_TE, e, bt);
        Panel_Blit(T_BE, e, by);
    }
    for (e = bt + 8; e < by; e += 8)
    {
        Panel_Blit(T_LE, bl, e);
        Panel_Blit(T_RE, rx, e);
    }

    FillWindowPixelRect(sPanelWin, sBlitAccent ? ACC_BODY2_IDX : (fainted ? RED_IDX : BLUE_IDX), rx, bt + 8, 2, 8);

    //HYDRA HP bar always drawn so the fainted mon shows an empty bar too (like the party screen).
    // For a fainted box sBlitRed (set in Panel_DrawBox) makes Panel_Blit render the bar/frame tiles
    // in red; the empty-bar fills stay grey, which reads as a dark empty bar on the red box (SS2/SS3).
    Panel_Blit(T_HP_L, bl + BAR_HP_DX,       bt + BAR_BLIT_DY);
    Panel_Blit(T_HP_R, bl + BAR_HP_DX + 8,   bt + BAR_BLIT_DY);
    Panel_Blit(T_BAR,  bl + BAR_BODY_DX,       bt + BAR_BLIT_DY);
    Panel_Blit(T_BAR,  bl + BAR_BODY_DX + 8,   bt + BAR_BLIT_DY);
    Panel_Blit(T_BAR,  bl + BAR_BODY_DX + 16,  bt + BAR_BLIT_DY);
    Panel_Blit(T_BAR,  bl + BAR_BODY_DX + 24,  bt + BAR_BLIT_DY);
    FillWindowPixelRect(sPanelWin, 13, bl + BAR_BODY_DX + 32, bt + BAR_BLIT_DY,     1, 1);
    FillWindowPixelRect(sPanelWin, 3,  bl + BAR_BODY_DX + 32, bt + BAR_BLIT_DY + 1, 1, 1);
    FillWindowPixelRect(sPanelWin, 3,  bl + BAR_BODY_DX + 32, bt + BAR_BLIT_DY + 5, 1, 1);
    FillWindowPixelRect(sPanelWin, 13, bl + BAR_BODY_DX + 32, bt + BAR_BLIT_DY + 6, 1, 1);

    hp = GetMonData(mon, MON_DATA_HP);
    maxHp = GetMonData(mon, MON_DATA_MAX_HP);
    pct = (maxHp != 0) ? (hp * 100u) / maxHp : 0;
    if (pct > 50) { topIdx = 10; botIdx = 9; }
    else          { topIdx = 12; botIdx = 11; }

    frac = (maxHp != 0) ? (hp * BAR_BODY_W) / maxHp : 0;
    if (hp > 0 && frac == 0)
        frac = 1;
    rem = BAR_BODY_W - frac;

    FillWindowPixelRect(sPanelWin, topIdx, bl + BAR_BODY_DX, bt + BAR_FILL_DY,     frac, 1);
    FillWindowPixelRect(sPanelWin, botIdx, bl + BAR_BODY_DX, bt + BAR_FILL_DY + 1, frac, 2);
    if (rem)
    {
        FillWindowPixelRect(sPanelWin, 13, bl + BAR_BODY_DX + frac, bt + BAR_FILL_DY,     rem, 1);
        FillWindowPixelRect(sPanelWin, 2,  bl + BAR_BODY_DX + frac, bt + BAR_FILL_DY + 1, rem, 2);
    }

    Panel_Blit(T_RE, rx, bt + BAR_BLIT_DY);

    ConvertIntToDecimalStringN(gStringVar2, GetMonData(mon, MON_DATA_LEVEL), STR_CONV_MODE_LEFT_ALIGN, 3);
    StringCopy(gStringVar1, gText_LevelSymbol);
    StringAppend(gStringVar1, gStringVar2);
    AddTextPrinterParameterized3(sPanelWin, FONT_SMALL, bl + LEVEL_DX, bt + LEVEL_DY, sPanelTextColor, 0, gStringVar1);

    gender = GetMonGender(mon);
    if (gender == MON_MALE)
        AddTextPrinterParameterized3(sPanelWin, FONT_SMALL, bl + LEVEL_DX + GENDER_DX, bt + LEVEL_DY, sPanelTextColor, 0, gText_MaleSymbol);
    else if (gender == MON_FEMALE)
        AddTextPrinterParameterized3(sPanelWin, FONT_SMALL, bl + LEVEL_DX + GENDER_DX, bt + LEVEL_DY, sPanelTextColor, 0, gText_FemaleSymbol);

    //HYDRA selection outline drawn LAST so it sits on top of the box/content, 2px thick to
    // match the party menu's red border.
    if (Panel_Selected(slot))
    {
        FillWindowPixelRect(sPanelWin, RED_IDX, bl, bt + BOX_TOP_INSET, BOX_W - 2, 2);                        // top
        FillWindowPixelRect(sPanelWin, RED_IDX, bl, bt + BOX_H - 2, BOX_W - 2, 2);                            // bottom
        FillWindowPixelRect(sPanelWin, RED_IDX, bl, bt + BOX_TOP_INSET, 2, BOX_H - BOX_TOP_INSET);            // left
        FillWindowPixelRect(sPanelWin, RED_IDX, bl + BOX_W - 4, bt + BOX_TOP_INSET, 2, BOX_H - BOX_TOP_INSET); // right
    }

    sBlitRed = FALSE; //HYDRA reset so later boxes/blits are not accidentally red-remapped
    sBlitAccent = FALSE;
}