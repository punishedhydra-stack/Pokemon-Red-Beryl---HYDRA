#include "global.h"
#include "hydra_hotbar.h"
#include "main.h"
#include "task.h"
#include "menu.h"
#include "window.h"
#include "text.h"
#include "string_util.h"
#include "sound.h"
#include "item.h"
#include "item_icon.h"
#include "item_use.h"
#include "item_menu.h"
#include "pokemon.h"
#include "party_menu.h"
#include "field_move.h"
#include "region_map.h"
#include "field_effect.h"
#include "event_data.h"
#include "field_player_avatar.h"
#include "field_control_avatar.h"
#include "event_object_movement.h"
#include "script.h"
#include "overworld.h"
#include "field_weather.h"
#include "field_screen_effect.h"
#include "palette.h"
#include "map_name_popup.h"
#include "constants/songs.h"
#include "constants/items.h"
#include "constants/weather.h"  //HYDRA for WEATHER_FOG_HORIZONTAL
#include "field_effect_helpers.h"  // For UpdateShadowFieldEffect
#include "sprite.h"               // For gFieldEffectObjectTemplatePointers




// Arrow sprite graphics
extern const u32 gHotbarArrowSpriteGfx[];
extern const u16 gHotbarArrowSpritePal[];
extern const u32 gHotbarPlusSpriteGfx[];
extern const u16 gHotbarPlusSpritePal[];


#define SLOT_PX        24
#define HOTBAR_X_NUDGE 4
#define ICON_TAG_BASE  (0x7A10 | BLEND_IMMUNE_FLAG)
#define ARROW_TAG_BASE (0x7A20 | BLEND_IMMUNE_FLAG)
#define CARRY_TAG_BASE (0x7A30 | BLEND_IMMUNE_FLAG)
#define PLUS_TAG_BASE (0x7A40 | BLEND_IMMUNE_FLAG)

#define HOTBAR_MODE_NORMAL 0
#define HOTBAR_MODE_ASSIGN 1
#define GRAB_NONE 0xFF

#define GRAYSCALE_PAL_TAG  0x7B00  // Unused palette tag for grayscale

// Grayscale system variables
static EWRAM_DATA u8 sGrayscaleSpriteIds[64] = {0};
static EWRAM_DATA u8 sGrayscaleSpriteOriginalPals[64] = {0};
static EWRAM_DATA u8 sGrayscaleSpriteCount = 0;
static EWRAM_DATA u16 sSavedBGPalettes[512] = {0};
static EWRAM_DATA u16 sReopenSavedUnfaded[PLTT_BUFFER_SIZE] = {0}; //HYDRA true colors saved during bag-reopen grey reveal
static struct TimeBlendSettings sReopenSavedTimeBlend; //HYDRA saved time-of-day blend during bag-reopen grey reveal
bool8 gHydraFlyMapFromHotbar = FALSE; //HYDRA see hydra_hotbar.h
static EWRAM_DATA bool8 sHotbarOpen = FALSE;
static EWRAM_DATA u8 sHotbarWindowId = 0;
static EWRAM_DATA u8 sHotbarCursor = 0;
static EWRAM_DATA u8 sIconSpriteIds[HOTBAR_SLOTS] = {0};
static EWRAM_DATA u8 sArrowSpriteId = 0;
static EWRAM_DATA u8 sCarrySpriteId = 0;
static EWRAM_DATA u8 sHotbarMode = 0;
static EWRAM_DATA u8 sGrab = 0;
static EWRAM_DATA u16 sPendingItem = 0;
static EWRAM_DATA bool8 sPickActive = FALSE;
static EWRAM_DATA u8 sPickSlot = 0;
static EWRAM_DATA bool8 sJustOpened = FALSE;
static EWRAM_DATA u8 sFieldMovePending = 0;
static EWRAM_DATA u8 sPlusSpriteIds[HOTBAR_SLOTS] = {0};

static u8 sSavedSpritePals[MAX_SPRITES];  // Store original palette numbers
static u16 sSavedPalette0[16];  
static u16 sSavedPaletteTags[MAX_SPRITES];
static u16 sSavedPaletteColors[MAX_SPRITES][16]; 

// Near the top of hydra_hotbar.c, after includes, add:
//static const u16 gGrayscalePal[] = INCBIN_U16("graphics/pokemon/grayscale.pal");

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

static const struct WindowTemplate sHotbarWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 3,
    .tilemapTop = 17,
    .width = 24,
    .height = 2,
    .paletteNum = 15,
    .baseBlock = 0x40,
};

static const struct SpriteSheet sHotbarArrowSpriteSheet = {
    .data = gHotbarArrowSpriteGfx,
    .size = 128,
    .tag = ARROW_TAG_BASE
};

static const struct SpritePalette sHotbarArrowSpritePalette = {
    .data = gHotbarArrowSpritePal,
    .tag = ARROW_TAG_BASE
};

static const struct OamData sOamData_HotbarArrow = {
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .matrixNum = 0,
    .priority = 0,
    .paletteNum = 0,
};

static const struct SpriteTemplate sSpriteTemplate_HotbarArrow = {
    .tileTag = ARROW_TAG_BASE,
    .paletteTag = ARROW_TAG_BASE, //HYDRA was 0x1100 (unloaded) -> bind to the arrow's own BLEND_IMMUNE palette so TOD/weather never tints it
    .oam = &sOamData_HotbarArrow,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sHotbarPlusSpriteSheet = {
    .data = gHotbarPlusSpriteGfx,
    .size = 128,
    .tag = PLUS_TAG_BASE
};

static const struct SpritePalette sHotbarPlusSpritePalette = {
    .data = gHotbarPlusSpritePal,
    .tag = PLUS_TAG_BASE
};

static const struct OamData sOamData_HotbarPlus = {
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .matrixNum = 0,
    .priority = 0,
    .paletteNum = 0,
};

static const struct SpriteTemplate sSpriteTemplate_HotbarPlus = {
    .tileTag = PLUS_TAG_BASE,
    .paletteTag = PLUS_TAG_BASE, //HYDRA was 0x1100 (unloaded) -> bind to the plus's own BLEND_IMMUNE palette so TOD/weather never tints it
    .oam = &sOamData_HotbarPlus,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// Forward declarations
static void Task_Hotbar(u8 taskId);
static void Task_HotbarToBag(u8 taskId);
static void Task_HotbarToFieldMove(u8 taskId);
static void Hotbar_TryUseFieldMove(u8 taskId);
static void Task_HotbarReopenWait(u8 taskId);
static void FieldCB_HydraHotbarReopen(void);
static void Hotbar_BuildUI(void);
static void Hotbar_TeardownVisuals(void);
static void Hotbar_TeardownVisualsKeepGray(void); //HYDRA bag-path teardown that keeps grayscale
static void Hotbar_Close(void);
static void Hotbar_UpdateArrow(void);
static void Hotbar_LoadIcons(void);
static void Hotbar_FreeIcons(void);
static void Hotbar_RefreshSlotSprites(void);
static void Hotbar_UpdateCarry(void);
static void Hotbar_LoadCarry(void);
static void Hotbar_FreeCarry(void);
static void Hotbar_LiftGrabIcon(void);
static bool8 Hotbar_UseSelected(void);

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
// hotbar is open. Works by capping each grey's brightness at GRAY_SPRITE_MAX (0-31):
//   31 = no darkening (off)   lower = darker   (e.g. 20 dims highlights, 12 is quite dark)
// A cap is used (not a multiply) on purpose: it is IDEMPOTENT, so it can be re-run on the bag
// reopen reveal without the sprites getting darker each pass. BG and UI are NOT affected.
// Keep this value in sync with the same knob in hydra_party_panel.c for a consistent look.
#define GRAY_SPRITE_MAX  20

//HYDRA cap a NEUTRAL grey colour (r==g==b) to GRAY_SPRITE_MAX. Safe to run repeatedly.
static u16 DarkenGray(u16 grayColor)
{
    u8 v = grayColor & 0x1F;
    if (v > GRAY_SPRITE_MAX)
        v = GRAY_SPRITE_MAX;
    return (v) | (v << 5) | (v << 10);
}

//HYDRA true if OBJ palette slot `palNum` should be desaturated. Kept in their own palettes:
// the shadow field effect (TAG_WEATHER_START) so it stays dark like colour mode, the blend-
// immune UI palettes, and the grayscale ramp itself.
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

static void ApplyGrayscaleEffect(void)
{
    int i, j;
    struct ObjectEvent *objEvent;
    u8 grayPalNum = 0xFF;
    
    // Initialize saved palette arrays
    for (i = 0; i < MAX_SPRITES; i++) {
        sSavedSpritePals[i] = 0xFF;
        sSavedPaletteTags[i] = 0xFFFF;
    }
    for (i = 0; i < 16; i++) {
        sSavedPalette0[i] = 0;
    }
    
    // === SAVE ORIGINAL PALETTE 0 ===
    for (i = 0; i < 16; i++) {
        sSavedPalette0[i] = gPlttBufferFaded[256 + i];
    }
    
    // === DESATURATE BG PALETTES (skip palette 0 for now) ===
    for (i = 0; i < 256; i++) {
        sSavedBGPalettes[i] = gPlttBufferFaded[i];
        gPlttBufferFaded[i] = DesaturateColor(gPlttBufferFaded[i]);
    }
    // Save all OBJ palettes, then desaturate each slot except shadow / UI / grayscale.
    for (i = 256; i < 512; i++)
        sSavedBGPalettes[i] = gPlttBufferFaded[i];
    //HYDRA straight per-slot desaturation (the old reversed pal-0 desaturation inverted whatever
    // sat on OBJ palette 0 - e.g. the shadow - turning it light).
    {
        int q;
        for (q = 0; q < 16; q++) {
            if (!HydraShouldGrayObjPal(q))
                continue;
            for (i = 0; i < 16; i++)
                gPlttBufferFaded[256 + q * 16 + i] = DarkenGray(DesaturateColor(gPlttBufferFaded[256 + q * 16 + i])); //HYDRA grey + darken sprite
        }
    }
    
    // === STEP 1: PROCESS ALL OBJECT EVENTS FIRST (save info, free palettes) ===
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
        
        // Skip player
        if (spriteId == gPlayerAvatar.spriteId)
            continue;
        
        // Skip hotbar UI
        u16 tileTag = sprite->template->tileTag;
        if (tileTag == ARROW_TAG_BASE || tileTag == PLUS_TAG_BASE || 
            (tileTag >= ICON_TAG_BASE && tileTag < ICON_TAG_BASE + HOTBAR_SLOTS) ||
            tileTag == CARRY_TAG_BASE)
            continue;
        
        // Skip shadows
        if (sprite->callback == UpdateShadowFieldEffect)
            continue;
        
        // Save original palette number
        u8 palNum = sprite->oam.paletteNum;
        sSavedSpritePals[spriteId] = palNum;
        
        // Save palette data for restoration
        if (palNum < 16) {
            u16 actualTag = GetSpritePaletteTagByPaletteNum(palNum);
            sSavedPaletteTags[spriteId] = actualTag;
            
            for (j = 0; j < 16; j++) {
                sSavedPaletteColors[spriteId][j] = gPlttBufferUnfaded[256 + (palNum * 16) + j]; //HYDRA save ORIGINAL colors, not weather-altered faded
            }
        }
        
        // Free the palette slot (temporarily set to 0 to mark as "being processed")
        // Don't actually free yet - we do that after grayscale is loaded
    }
    
    // === STEP 2: NOW FREE ALL THE PALETTES (after saving all info) ===
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
        
        u16 tileTag = sprite->template->tileTag;
        if (tileTag == ARROW_TAG_BASE || tileTag == PLUS_TAG_BASE || 
            (tileTag >= ICON_TAG_BASE && tileTag < ICON_TAG_BASE + HOTBAR_SLOTS) ||
            tileTag == CARRY_TAG_BASE)
            continue;
        
        if (sprite->callback == UpdateShadowFieldEffect)
            continue;
        
        u8 palNum = sSavedSpritePals[spriteId];
        if (palNum != 0 && palNum < 16) {
            u16 actualTag = sSavedPaletteTags[spriteId];
            if (actualTag != 0xFFFF) {
                FreeSpritePaletteByTag(actualTag);
            }
        }
    }
    
    // === STEP 3: LOAD GRAYSCALE PALETTE (darkened by the GRAY_SPRITE_MAX knob) after freeing slots ===
    u16 grayDark[16]; //HYDRA darkened copy of the overworld-sprite ramp (all NPCs/followers use it)
    for (i = 0; i < 16; i++)
        grayDark[i] = DarkenGray(sGrayscalePalette[i]);
    struct SpritePalette grayscalePal = {
        .data = grayDark,
        .tag = GRAYSCALE_PAL_TAG
    };
    grayPalNum = LoadSpritePalette(&grayscalePal);
    
    // === STEP 4: ASSIGN ALL SPRITES TO GRAYSCALE PALETTE ===
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
        
        u16 tileTag = sprite->template->tileTag;
        if (tileTag == ARROW_TAG_BASE || tileTag == PLUS_TAG_BASE || 
            (tileTag >= ICON_TAG_BASE && tileTag < ICON_TAG_BASE + HOTBAR_SLOTS) ||
            tileTag == CARRY_TAG_BASE)
            continue;
        
        if (sprite->callback == UpdateShadowFieldEffect)
            continue;
        
        // Assign to grayscale palette
         if (grayPalNum != 0xFF) {
            sprite->oam.paletteNum = grayPalNum;
        }
    }

    // HYDRA: Flush OAM now so sprites are recorded on the grayscale slot BEFORE
    // Hotbar_LoadIcons loads item palettes into the just-freed slots. Palette
    // writes hit gPlttBufferFaded immediately, but oam.paletteNum only reaches
    // hardware at the next BuildOamBuffer. Without this flush, a VBlank landing
    // between the item-palette load and frame-end BuildOamBuffer shows the new
    // item palettes with stale OAM still pointing sprites at their old slots =
    // the 1-frame wrong-palette flash.
    BuildOamBuffer(); //HYDRA
}



static void RemoveGrayscaleEffect(void)
{
    int i;   //HYDRA j no longer needed after restore rewrite
    struct ObjectEvent *objEvent;
    
    // === RESTORE BG PALETTES ===
    for (i = 0; i < 512; i++) {
        gPlttBufferFaded[i] = sSavedBGPalettes[i];
    }
    
    // === RESTORE ORIGINAL PALETTE 0 COLORS ===
    for (i = 0; i < 16; i++) {
        gPlttBufferFaded[256 + i] = sSavedPalette0[i];
    }
    
    // === FREE GRAYSCALE PALETTE ===
    FreeSpritePaletteByTag(GRAYSCALE_PAL_TAG);
    
    // Process ObjectEvents - restore their palettes
    for (objEvent = gObjectEvents; objEvent < &gObjectEvents[OBJECT_EVENTS_COUNT]; objEvent++) {
        if (!objEvent->active || objEvent->spriteId == MAX_SPRITES)
            continue;
        
        u8 spriteId = objEvent->spriteId;
        struct Sprite *sprite = &gSprites[spriteId];
        
        if (!sprite->inUse)
            continue;
        
        // Skip player
        if (spriteId == gPlayerAvatar.spriteId)
            continue;
        
        // Skip hotbar UI
        u16 tileTag = sprite->template->tileTag;
        if (tileTag == ARROW_TAG_BASE || tileTag == PLUS_TAG_BASE || 
            (tileTag >= ICON_TAG_BASE && tileTag < ICON_TAG_BASE + HOTBAR_SLOTS) ||
            tileTag == CARRY_TAG_BASE)
            continue;
        
        // Skip shadows - they were never palette swapped
        if (sprite->callback == UpdateShadowFieldEffect)
            continue;
        
        // Check if we saved a palette for this sprite
        if (sSavedSpritePals[spriteId] != 0xFF && sSavedSpritePals[spriteId] < 16) {
            
            u8 originalPalNum = sSavedSpritePals[spriteId];
            u16 actualTag = sSavedPaletteTags[spriteId];
            
            // If it was on palette 0, it's already restored above
            if (originalPalNum == 0) {
                sprite->oam.paletteNum = 0;
                continue;
            }
            
            // HYDRA: Rebuild this sprite's palette from ORIGINAL (unfaded) colors,
            // then let the engine re-apply the current weather/time. The old code
            // reloaded the saved (weather-altered) colors via LoadSpritePalette, which
            // also writes gPlttBufferUnfaded. Under drought that corrupted the originals
            // so every open/close re-brightened NPCs until they maxed to white/black.
            // Loading clean originals keeps gPlttBufferUnfaded pristine.
            u8 slot = IndexOfSpritePaletteTag(actualTag);
            if (slot == 0xFF)   // not already reloaded by an earlier shared-palette sprite
            {
                struct SpritePalette tempPal;
                tempPal.tag = actualTag;
                tempPal.data = sSavedPaletteColors[spriteId]; // original unfaded colors
                slot = LoadSpritePalette(&tempPal);
                if (slot != 0xFF && gWeatherPtr->currWeather != WEATHER_FOG_HORIZONTAL)
                    UpdateSpritePaletteWithWeather(slot, FALSE); //HYDRA re-derive weather from originals
            }
            sprite->oam.paletteNum = (slot != 0xFF) ? slot : 0; //HYDRA
        }
    }

    //HYDRA: The player sprite is skipped by the loop above (restored only via the BG copy).
    // Re-apply the current weather/time to the player's palette slot too, so on the bag-
    // reopen path (where the saved colors are untinted) the trainer gets the same time-of-
    // day tint as every other sprite instead of being left untinted.
    if (gPlayerAvatar.spriteId < MAX_SPRITES)
    {
        struct Sprite *playerSprite = &gSprites[gPlayerAvatar.spriteId];
        if (playerSprite->inUse && gWeatherPtr->currWeather != WEATHER_FOG_HORIZONTAL)
            UpdateSpritePaletteWithWeather(playerSprite->oam.paletteNum, FALSE); //HYDRA
    }
}

bool8 HydraHotbar_IsOpen(void)
{
    return sHotbarOpen;
}

static bool8 SlotIsItem(u16 slot)
{
    return slot != HOTBAR_SLOT_EMPTY && !(slot & HOTBAR_FIELDMOVE_FLAG);
}

static bool8 SlotIsFieldMove(u16 slot)
{
    return slot != HOTBAR_SLOT_EMPTY && (slot & HOTBAR_FIELDMOVE_FLAG);
}

static u16 Hotbar_SlotIconItem(u16 slot)
{
    if (SlotIsItem(slot))
        return slot;
    if (SlotIsFieldMove(slot))
        return GetTMHMItemIdFromMoveId(FieldMove_GetMoveId((enum FieldMove)(slot & ~HOTBAR_FIELDMOVE_FLAG)));
    return ITEM_NONE;
}

static enum FieldMove Hotbar_MoveToFieldMove(enum Move move)
{
    u32 i;
    for (i = 0; i < FIELD_MOVES_COUNT; i++)
    {
        if (FieldMove_GetMoveId((enum FieldMove)i) == move)
            return (enum FieldMove)i;
    }
    return FIELD_MOVES_COUNT;
}

static bool8 Hotbar_FieldMoveAllowed(enum FieldMove fm)
{
    switch (fm)
    {
    case FIELD_MOVE_MILK_DRINK:
    case FIELD_MOVE_SOFT_BOILED:
        return FALSE;
    default:
        return (fm < FIELD_MOVES_COUNT);
    }
}

static bool8 Hotbar_PartyKnowsMove(enum Move move)
{
    u32 i, j;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];
        if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        for (j = 0; j < MAX_MON_MOVES; j++)
        {
            if (GetMonData(mon, MON_DATA_MOVE1 + j) == move)
                return TRUE;
        }
    }
    return FALSE;
}

static bool8 Hotbar_CanUseFromBag(enum FieldMove fm)
{
    enum Move move = FieldMove_GetMoveId(fm);
    enum Item tm = GetTMHMItemIdFromMoveId(move);
    u32 i;

    if (!IsFieldMoveUnlocked(fm) || tm == ITEM_NONE || !CheckBagHasItem(tm, 1))
        return FALSE;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];
        if (GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE
         && !GetMonData(mon, MON_DATA_IS_EGG)
         && CanLearnTeachableMove(GetMonData(mon, MON_DATA_SPECIES), move))
            return TRUE;
    }
    return FALSE;
}

static bool8 Hotbar_IsWeatherFieldMove(enum FieldMove fm)
{
    return fm == FIELD_MOVE_RAIN_DANCE || fm == FIELD_MOVE_SUNNY_DAY
        || fm == FIELD_MOVE_SANDSTORM  || fm == FIELD_MOVE_HAIL;
}

static bool8 Hotbar_FieldMoveEligible(enum FieldMove fm)
{
    if (!Hotbar_FieldMoveAllowed(fm) || !IsFieldMoveUnlocked(fm))
        return FALSE;

    if (Hotbar_IsWeatherFieldMove(fm))
    {
        enum Move move = FieldMove_GetMoveId(fm);
        enum Item tm = GetTMHMItemIdFromMoveId(move);
        return Hotbar_PartyKnowsMove(move) && tm != ITEM_NONE && CheckBagHasItem(tm, 1);
    }

    return (Hotbar_PartyKnowsMove(FieldMove_GetMoveId(fm)) || Hotbar_CanUseFromBag(fm));
}

static void Hotbar_SelectFieldMoveMon(enum Move move)
{
    u32 i, j;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];
        if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        for (j = 0; j < MAX_MON_MOVES; j++)
        {
            if (GetMonData(mon, MON_DATA_MOVE1 + j) == move)
            {
                gPartyMenu.slotId = i;
                return;
            }
        }
    }
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];
        if (GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE
         && !GetMonData(mon, MON_DATA_IS_EGG)
         && CanLearnTeachableMove(GetMonData(mon, MON_DATA_SPECIES), move))
        {
            gPartyMenu.slotId = i;
            return;
        }
    }
}

static void Hotbar_LoadPlusSprites(void)
{
    u32 i;
    s16 originX = sHotbarWindowTemplate.tilemapLeft * 8 - 4 + HOTBAR_X_NUDGE;
    s16 centerY = sHotbarWindowTemplate.tilemapTop * 8 + 8;

    LoadSpriteSheet(&sHotbarPlusSpriteSheet);
    LoadSpritePalette(&sHotbarPlusSpritePalette);

    for (i = 0; i < HOTBAR_SLOTS; i++)
    {
        sPlusSpriteIds[i] = SPRITE_NONE;
        if (gSaveBlock1Ptr->hotbar[i] == HOTBAR_SLOT_EMPTY)
        {
            u8 spriteId = CreateSprite(&sSpriteTemplate_HotbarPlus,
                                       originX + i * SLOT_PX + SLOT_PX / 2,
                                       centerY, 1);
            if (spriteId != MAX_SPRITES)
            {
                gSprites[spriteId].oam.priority = 0;
                sPlusSpriteIds[i] = spriteId;
            }
        }
    }
}

static void Hotbar_FreePlusSprites(void)
{
    u32 i;
    for (i = 0; i < HOTBAR_SLOTS; i++)
    {
        if (sPlusSpriteIds[i] != SPRITE_NONE && sPlusSpriteIds[i] < MAX_SPRITES)
        {
            DestroySprite(&gSprites[sPlusSpriteIds[i]]);
            sPlusSpriteIds[i] = SPRITE_NONE;
        }
    }
    FreeSpriteTilesByTag(PLUS_TAG_BASE);
    FreeSpritePaletteByTag(PLUS_TAG_BASE);
}

static void Hotbar_DrawContents(void)
{
    FillWindowPixelBuffer(sHotbarWindowId, PIXEL_FILL(1));
    CopyWindowToVram(sHotbarWindowId, COPYWIN_GFX);
}

static void Hotbar_LoadArrow(void)
{
    LoadSpriteSheet(&sHotbarArrowSpriteSheet);
    LoadSpritePalette(&sHotbarArrowSpritePalette); //HYDRA arrow palette was never loaded; needed so the arrow binds to its own BLEND_IMMUNE palette
    sArrowSpriteId = CreateSprite(&sSpriteTemplate_HotbarArrow, 0, 0, 0);
    if (sArrowSpriteId != MAX_SPRITES)
        Hotbar_UpdateArrow();
}

static void Hotbar_UpdateArrow(void)
{
    if (sArrowSpriteId != SPRITE_NONE && sArrowSpriteId < MAX_SPRITES)
    {
        u16 x = (sHotbarWindowTemplate.tilemapLeft * 8 + HOTBAR_X_NUDGE) + (sHotbarCursor * SLOT_PX) + SLOT_PX/2;
        u16 y = (sHotbarWindowTemplate.tilemapTop * 8) - 12;
        gSprites[sArrowSpriteId].x = x;
        gSprites[sArrowSpriteId].y = y;
    }
}

static void Hotbar_FreeArrow(void)
{
    if (sArrowSpriteId != SPRITE_NONE && sArrowSpriteId < MAX_SPRITES)
    {
        FreeSpriteTilesByTag(ARROW_TAG_BASE);
        FreeSpritePaletteByTag(ARROW_TAG_BASE); //HYDRA free the arrow palette we now load
        DestroySprite(&gSprites[sArrowSpriteId]);
        sArrowSpriteId = SPRITE_NONE;
    }
}

static void Hotbar_LoadIcons(void)
{
    u32 i;
    s16 originX = sHotbarWindowTemplate.tilemapLeft * 8 + HOTBAR_X_NUDGE;
    s16 centerY = sHotbarWindowTemplate.tilemapTop * 8 + 8;

    for (i = 0; i < HOTBAR_SLOTS; i++)
    {
        u16 iconItem = Hotbar_SlotIconItem(gSaveBlock1Ptr->hotbar[i]);
        sIconSpriteIds[i] = SPRITE_NONE;
        if (iconItem != ITEM_NONE)
        {
            u8 spriteId = AddItemIconSprite(ICON_TAG_BASE + i, ICON_TAG_BASE + i, iconItem);
            if (spriteId != MAX_SPRITES)
            {
                gSprites[spriteId].x = originX + i * SLOT_PX + SLOT_PX / 2;
                gSprites[spriteId].y = centerY + 4;
                gSprites[spriteId].oam.priority = 0;
                sIconSpriteIds[i] = spriteId;
            }
        }
    }
}

static void Hotbar_FreeIcons(void)
{
    u32 i;
    for (i = 0; i < HOTBAR_SLOTS; i++)
    {
        if (sIconSpriteIds[i] != SPRITE_NONE && sIconSpriteIds[i] < MAX_SPRITES)
        {
            FreeSpriteTilesByTag(ICON_TAG_BASE + i);
            FreeSpritePaletteByTag(ICON_TAG_BASE + i);
            DestroySprite(&gSprites[sIconSpriteIds[i]]);
            sIconSpriteIds[i] = SPRITE_NONE;
        }
    }
}

static void Hotbar_RefreshSlotSprites(void)
{
    Hotbar_FreeIcons();
    Hotbar_FreePlusSprites();
    Hotbar_LoadPlusSprites();
    Hotbar_LoadIcons();
}

static void Hotbar_LiftGrabIcon(void)
{
    if (sGrab < HOTBAR_SLOTS && sIconSpriteIds[sGrab] != SPRITE_NONE && sIconSpriteIds[sGrab] < MAX_SPRITES)
        gSprites[sIconSpriteIds[sGrab]].y2 -= 6;
}

static void Hotbar_LoadCarry(void)
{
    sCarrySpriteId = SPRITE_NONE;
    if (SlotIsItem(sPendingItem))
    {
        u8 spriteId = AddItemIconSprite(CARRY_TAG_BASE, CARRY_TAG_BASE, sPendingItem);
        if (spriteId != MAX_SPRITES)
        {
            sCarrySpriteId = spriteId;
            gSprites[spriteId].oam.priority = 0;
            Hotbar_UpdateCarry();
        }
    }
}

static void Hotbar_UpdateCarry(void)
{
    if (sCarrySpriteId != SPRITE_NONE && sCarrySpriteId < MAX_SPRITES)
    {
        s16 baseX = (sHotbarWindowTemplate.tilemapLeft + 1) * 8 + HOTBAR_X_NUDGE;
        s16 baseY = (sHotbarWindowTemplate.tilemapTop + 1) * 8;
        gSprites[sCarrySpriteId].x2 = baseX + sHotbarCursor * SLOT_PX + SLOT_PX / 2;
        gSprites[sCarrySpriteId].y2 = baseY + 12 - 6;
    }
}

static void Hotbar_FreeCarry(void)
{
    if (sCarrySpriteId != SPRITE_NONE && sCarrySpriteId < MAX_SPRITES)
    {
        FreeSpriteTilesByTag(CARRY_TAG_BASE);
        FreeSpritePaletteByTag(CARRY_TAG_BASE);
        DestroySprite(&gSprites[sCarrySpriteId]);
        sCarrySpriteId = SPRITE_NONE;
    }
}

static void Hotbar_BuildUI(void)
{
    // Apply grayscale FIRST - before sHotbarOpen is set
    ApplyGrayscaleEffect();
    
    // NOW set sHotbarOpen after grayscale is fully applied
    sHotbarOpen = TRUE;
    FreezeObjectEvents(); //HYDRA pause overworld (NPCs/followers/wild) while hotbar is open
    
    sCarrySpriteId = SPRITE_NONE;
    LoadMessageBoxAndBorderGfx();
    sHotbarWindowId = AddWindow(&sHotbarWindowTemplate);
    DrawStdWindowFrame(sHotbarWindowId, FALSE);
    CopyWindowToVram(sHotbarWindowId, COPYWIN_FULL);

    Hotbar_LoadPlusSprites();
    Hotbar_LoadIcons();
    Hotbar_LoadArrow();
    if (sHotbarMode == HOTBAR_MODE_ASSIGN)
        Hotbar_LoadCarry();
    Hotbar_DrawContents();

    sJustOpened = TRUE;
    CreateTask(Task_Hotbar, 0x50);
}

static void Hotbar_TeardownVisuals(void)
{
    Hotbar_FreeIcons();      // Frees 8 icon palettes first
    Hotbar_FreePlusSprites();
    Hotbar_FreeArrow();
    Hotbar_FreeCarry();
    ClearStdWindowAndFrame(sHotbarWindowId, TRUE);
    RemoveWindow(sHotbarWindowId);
    sHotbarWindowId = WINDOW_NONE;
    sHotbarOpen = FALSE;
    UnfreezeObjectEvents(); //HYDRA resume overworld when hotbar closes
    
    RemoveGrayscaleEffect(); // Now has 8+ free slots to restore overworld palettes
}

static void Hotbar_TeardownVisualsKeepGray(void)
{
    //HYDRA: Tear down only the hotbar UI but LEAVE grayscale applied, so the fade-out to
    // the bag stays grey - no color frame and no sprite palette reassignment. The bag
    // reloads the field on return, where FieldCB_HydraHotbarReopen re-applies grayscale.
    Hotbar_FreeIcons();
    Hotbar_FreePlusSprites();
    Hotbar_FreeArrow();
    Hotbar_FreeCarry();
    ClearStdWindowAndFrame(sHotbarWindowId, TRUE);
    RemoveWindow(sHotbarWindowId);
    sHotbarWindowId = WINDOW_NONE;
    sHotbarOpen = FALSE;
    // Intentionally NOT calling RemoveGrayscaleEffect() (keep the screen grey) nor
    // UnfreezeObjectEvents() (the field is being torn down for the bag anyway).
}

static void Hotbar_Close(void)
{
    Hotbar_TeardownVisuals();
    UnlockPlayerFieldControls();
}

static bool8 Hotbar_UseSelected(void)
{
    u16 slot = gSaveBlock1Ptr->hotbar[sHotbarCursor];

    if (!SlotIsItem(slot))
        return FALSE;
    if (CheckBagHasItem(slot, 1) != TRUE)
    {
        gSaveBlock1Ptr->hotbar[sHotbarCursor] = HOTBAR_SLOT_EMPTY;
        return FALSE;
    }

    Hotbar_TeardownVisuals();
    //LockPlayerFieldControls();
    FreezeObjectEvents();
    PlayerFreeze();
    StopPlayerAvatar();
    gSpecialVar_ItemId = slot;
    {
        u8 taskId = CreateTask(GetItemFieldFunc(slot), 8);
        gTasks[taskId].data[3] = TRUE;
    }
    return TRUE;
}

static void Task_HotbarToBag(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        CleanupOverworldWindowsAndTilemaps();
        GoToBagMenu(ITEMMENULOCATION_FIELD, POCKET_KEY_ITEMS, CB2_HydraHotbar_ReturnToField);
    }
}

static void Hotbar_TryUseFieldMove(u8 taskId)
{
    u16 slot = gSaveBlock1Ptr->hotbar[sHotbarCursor];
    enum FieldMove fm = (enum FieldMove)(slot & ~HOTBAR_FIELDMOVE_FLAG);
    enum Move move = FieldMove_GetMoveId(fm);

    if (!Hotbar_FieldMoveEligible(fm))
    {
        gSaveBlock1Ptr->hotbar[sHotbarCursor] = HOTBAR_SLOT_EMPTY;
        Hotbar_RefreshSlotSprites();
        PlaySE(SE_BOO);
        return;
    }

    Hotbar_SelectFieldMoveMon(move);

    if (SetUpFieldMove(fm) != TRUE)
    {
        PlaySE(SE_BOO);
        return;
    }

    if (fm == FIELD_MOVE_FLY || fm == FIELD_MOVE_TELEPORT)
    {
        Hotbar_TeardownVisualsKeepGray(); //HYDRA keep grayscale through the fade to the fly map
        gHydraFlyMapFromHotbar = TRUE;    //HYDRA cancelling the fly map returns to the hotbar, not the party menu
        gFieldCallback2 = NULL;
        gPostMenuFieldCallback = NULL;
        sFieldMovePending = fm;
        FadeScreen(FADE_TO_BLACK, 0);
        gTasks[taskId].func = Task_HotbarToFieldMove;
        return;
    }

    Hotbar_TeardownVisuals(); //HYDRA non-fly field moves return to normal colored gameplay

    UnlockPlayerFieldControls();
    {
        MainCallback effect = gPostMenuFieldCallback;
        gFieldCallback2 = NULL;
        gPostMenuFieldCallback = NULL;
        if (effect != NULL)
            effect();
    }
    DestroyTask(taskId);
}

static void Task_HotbarToFieldMove(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        UnlockPlayerFieldControls();
        gTeleportViaFlyMap = (sFieldMovePending == FIELD_MOVE_TELEPORT);
        SetMainCallback2(CB2_OpenFlyMap);
    }
}

bool8 HydraHotbar_TryAssignPickedHM(u16 itemId)
{
    enum Move move = ItemIdToBattleMoveId(itemId);
    enum FieldMove fm;

    if (!sPickActive)
        return FALSE;
    fm = Hotbar_MoveToFieldMove(move);
    if (fm >= FIELD_MOVES_COUNT || !Hotbar_FieldMoveEligible(fm))
        return FALSE;

    gSaveBlock1Ptr->hotbar[sPickSlot] = HOTBAR_FIELDMOVE_FLAG | fm;
    return TRUE;
}

static void Task_HotbarReopenWait(u8 taskId)
{
    if (!gPaletteFade.active && IsWeatherNotFadingIn() == TRUE)
    {
        int i;
        DestroyTask(taskId);
        sPickActive = FALSE;
        sGrab = GRAB_NONE;
        gTimeBlend = sReopenSavedTimeBlend; //HYDRA restore time-of-day blend after the grey reveal

        //HYDRA: Restore the true colors we grayed for the fade reveal, then rebuild the
        // colored 'faded' buffer as a normal open would see it, so Hotbar_BuildUI's
        // grayscale save/restore captures the real colors (not the grey fade result).
        for (i = 0; i < PLTT_BUFFER_SIZE; i++)
            gPlttBufferUnfaded[i] = sReopenSavedUnfaded[i];
        CpuFastCopy(gPlttBufferUnfaded, gPlttBufferFaded, PLTT_BUFFER_SIZE * 2);
        if (MapHasNaturalLight(gMapHeader.mapType))
        {
            UpdateTimeOfDay();
            UpdatePalettesWithTime(PALETTES_MAP);
        }
        Hotbar_BuildUI();
    }
}

static void FieldCB_HydraHotbarReopen(void)
{
    //HYDRA: Desaturate the fade SOURCE (unfaded) so FadeInFromBlack reveals a grey world
    // instead of the real colors (kills the 1-frame color screen on bag reopen). The true
    // colors are saved and restored once the fade finishes (Task_HotbarReopenWait).
    int i;
    for (i = 0; i < PLTT_BUFFER_SIZE; i++)
        sReopenSavedUnfaded[i] = gPlttBufferUnfaded[i];
    for (i = 0; i < 256; i++)
        gPlttBufferUnfaded[i] = DesaturateColor(gPlttBufferUnfaded[i]);
    {
        int q;
        for (q = 0; q < 16; q++) {
            if (!HydraShouldGrayObjPal(q)) //HYDRA keep the shadow dark on the bag-reopen reveal too
                continue;
            for (i = 0; i < 16; i++)
                gPlttBufferUnfaded[256 + q * 16 + i] = DarkenGray(DesaturateColor(gPlttBufferUnfaded[256 + q * 16 + i])); //HYDRA grey + darken sprite (reopen reveal)
        }
    }
    FadeInFromBlack();
    //HYDRA: neutralize the time-of-day tint for this reveal so the fade shows PURE grey,
    // not TOD-tinted grey. The TOD fade reads gTimeBlend by pointer each frame; zeroing the
    // blend coeffs makes TimeMixPalettes a passthrough. Restored in Task_HotbarReopenWait.
    // Done AFTER FadeInFromBlack because FadeScreen internally recomputes gTimeBlend.
    sReopenSavedTimeBlend = gTimeBlend;
    gTimeBlend.startBlend.coeff = 0;
    gTimeBlend.startBlend.isTint = 0;
    gTimeBlend.endBlend.coeff = 0;
    gTimeBlend.endBlend.isTint = 0;
    //LockPlayerFieldControls();
    CreateTask(Task_HotbarReopenWait, 0x50);
}

void CB2_HydraHotbar_ReturnToField(void)
{
    gFieldCallback = FieldCB_HydraHotbarReopen;
    SetMainCallback2(CB2_ReturnToField);
}

void HydraHotbar_SetPendingAssign(u16 itemId)
{
    sPendingItem = itemId;
    sHotbarMode = HOTBAR_MODE_ASSIGN;
}

bool8 HydraHotbar_IsPicking(void)
{
    return sPickActive;
}

bool8 HydraHotbar_TryAssignPicked(u16 itemId)
{
    if (!sPickActive)
        return FALSE;
    gSaveBlock1Ptr->hotbar[sPickSlot] = itemId;
    return TRUE;
}

static void Task_Hotbar(u8 taskId)
{   
    if (sJustOpened)
    {
        sJustOpened = FALSE;
        return;
    }

    if (JOY_NEW(DPAD_LEFT))
    {
        sHotbarCursor = (sHotbarCursor == 0) ? HOTBAR_SLOTS - 1 : sHotbarCursor - 1;
        PlaySE(SE_SELECT);
        Hotbar_UpdateArrow();
        if (sHotbarMode == HOTBAR_MODE_ASSIGN)
            Hotbar_UpdateCarry();
    }
    else if (JOY_NEW(DPAD_RIGHT))
    {
        sHotbarCursor = (sHotbarCursor + 1) % HOTBAR_SLOTS;
        PlaySE(SE_SELECT);
        Hotbar_UpdateArrow();
        if (sHotbarMode == HOTBAR_MODE_ASSIGN)
            Hotbar_UpdateCarry();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        if (sHotbarMode == HOTBAR_MODE_ASSIGN)
        {
            gSaveBlock1Ptr->hotbar[sHotbarCursor] = sPendingItem;
            sPendingItem = HOTBAR_SLOT_EMPTY;
            sHotbarMode = HOTBAR_MODE_NORMAL;
            Hotbar_FreeCarry();
            Hotbar_RefreshSlotSprites();
            PlaySE(SE_SELECT);
        }
        else if (sGrab != GRAB_NONE)
        {
            PlaySE(SE_BOO);
        }
        else if (SlotIsItem(gSaveBlock1Ptr->hotbar[sHotbarCursor]))
        {
            if (Hotbar_UseSelected() == TRUE)
                DestroyTask(taskId);
            else
                PlaySE(SE_BOO);
        }
        else if (SlotIsFieldMove(gSaveBlock1Ptr->hotbar[sHotbarCursor]))
        {
            Hotbar_TryUseFieldMove(taskId);
        }
        else
        {
            PlaySE(SE_SELECT);
            sPickActive = TRUE;
            sPickSlot = sHotbarCursor;
            Hotbar_TeardownVisualsKeepGray(); //HYDRA keep grayscale through the fade to the bag
            FadeScreen(FADE_TO_BLACK, 0);
            gTasks[taskId].func = Task_HotbarToBag;
        }
    }
    else if (JOY_NEW(SELECT_BUTTON))
    {
        if (sHotbarMode == HOTBAR_MODE_ASSIGN)
        {
            PlaySE(SE_BOO);
        }
        else if (sGrab != GRAB_NONE)
        {
            if (sGrab != sHotbarCursor)
            {
                u16 tmp = gSaveBlock1Ptr->hotbar[sGrab];
                gSaveBlock1Ptr->hotbar[sGrab] = gSaveBlock1Ptr->hotbar[sHotbarCursor];
                gSaveBlock1Ptr->hotbar[sHotbarCursor] = tmp;
            }
            sGrab = GRAB_NONE;
            Hotbar_RefreshSlotSprites();
            PlaySE(SE_SELECT);
        }
        else if (gSaveBlock1Ptr->hotbar[sHotbarCursor] != HOTBAR_SLOT_EMPTY)
        {
            sGrab = sHotbarCursor;
            Hotbar_LiftGrabIcon();
            PlaySE(SE_SELECT);
        }
        else
        {
            PlaySE(SE_BOO);
        }
    }
    else if (JOY_NEW(B_BUTTON))
    {
        if (sHotbarMode == HOTBAR_MODE_ASSIGN)
        {
            sPendingItem = HOTBAR_SLOT_EMPTY;
            sHotbarMode = HOTBAR_MODE_NORMAL;
            Hotbar_FreeCarry();
            PlaySE(SE_SELECT);
        }
        else if (sGrab != GRAB_NONE)
        {
            sGrab = GRAB_NONE;
            Hotbar_RefreshSlotSprites();
            PlaySE(SE_SELECT);
        }
        else
        {
            PlaySE(SE_SELECT);
            Hotbar_Close();
            DestroyTask(taskId);
        }
    }
    else if (JOY_NEW(START_BUTTON))
    {
        if (sHotbarMode == HOTBAR_MODE_NORMAL && sGrab == GRAB_NONE
         && gSaveBlock1Ptr->hotbar[sHotbarCursor] != HOTBAR_SLOT_EMPTY)
        {
            gSaveBlock1Ptr->hotbar[sHotbarCursor] = HOTBAR_SLOT_EMPTY;
            Hotbar_RefreshSlotSprites();
            PlaySE(SE_SELECT);
        }
        else
        {
            PlaySE(SE_BOO);
        }
    }
}

void HydraHotbar_OpenFromField(void)
{
    if (sHotbarOpen)
        return;

    PlaySE(SE_SELECT);
    HideMapNamePopUpWindow();
    // REMOVED: sHotbarOpen = TRUE from here
    
    LockPlayerFieldControls();

    sHotbarMode = HOTBAR_MODE_NORMAL;
    sGrab = GRAB_NONE;
    sPendingItem = HOTBAR_SLOT_EMPTY;
    sPickActive = FALSE;

    Hotbar_BuildUI();
}