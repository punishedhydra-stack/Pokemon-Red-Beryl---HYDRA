#include "global.h"
#include "item_ball.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "script.h"
#include "sprite.h"
#include "task.h"
#include "constants/event_objects.h"
#include "constants/items.h"
#include "sound.h"
#include "constants/songs.h"

//HYDRA Pokébox open->flash->despawn timing, all in frames (~60fps). Tune everything here.
#define POKEBOX_OPEN1_DURATION   10  // hold the "open1" frame (pic-table frame 1)
#define POKEBOX_OPEN2_DURATION   12  // hold the "open2" frame (pic-table frame 2)
#define POKEBOX_BLINK_INTERVAL   4   // frames between visibility toggles during the flash
#define POKEBOX_BLINK_TOGGLES    6   // number of on/off toggles before the ball vanishes

// Pic-table frame indices (see sPicTable_Pokebox).
#define POKEBOX_FRAME_OPEN1      1
#define POKEBOX_FRAME_OPEN2      2

static u32 GetItemBallAmountFromTemplate(u32);
static u32 GetItemBallIdFromTemplate(u32);
static void Task_PokeboxOpenAnim(u8 taskId);

static u32 GetItemBallAmountFromTemplate(u32 itemBallId)
{
    u32 amount = gMapHeader.events->objectEvents[itemBallId].movementRangeX;

    if (amount > MAX_BAG_ITEM_CAPACITY)
        return MAX_BAG_ITEM_CAPACITY;

    return (amount == 0) ? 1 : amount;
}

static u32 GetItemBallIdFromTemplate(u32 itemBallId)
{
    enum Item itemId = gMapHeader.events->objectEvents[itemBallId].trainerRange_berryTreeId;

    return (itemId >= ITEMS_COUNT) ? (ITEM_NONE + 1) : itemId;
}

void GetItemBallIdAndAmountFromTemplate(void)
{
    u32 itemBallId = (gSpecialVar_LastTalked - 1);
    gSpecialVar_Result = GetItemBallIdFromTemplate(itemBallId);
    gSpecialVar_0x8009 = GetItemBallAmountFromTemplate(itemBallId);
}

// ---- HYDRA item-ball open -> flash -> despawn animation ----
// The Pokébox object event is inanimate and frozen by the script's `lock`, so we drive its shown
// frame straight into VRAM every tick (exactly like the incubator) and flash it by toggling the
// OBJECT EVENT's invisibility (exactly like the berry-tree sparkle) -- the engine copies
// objectEvent->invisible onto the sprite every frame, so toggling sprite->invisible directly would
// just be overwritten. Called from Common_EventScript_FindItem via `callnative StartPokeboxOpenAnim`
// immediately followed by `waitstate`: this spins up the field task and stops the script; when the
// task finishes it calls ScriptContext_Enable() so the script resumes at `removeobject`.
//
// data[0] = objectEventId, data[1] = phase (0 open1, 1 open2, 2 flash, 3 = nothing to do),
// data[2] = frame timer within the phase, data[3] = number of flash toggles done so far.
static void Task_PokeboxOpenAnim(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    struct ObjectEvent *objectEvent = &gObjectEvents[data[0]];
    struct Sprite *sprite;

    // Nothing to animate (phase 3) or the object went away: just release the script.
    if (data[1] == 3 || !objectEvent->active)
    {
        DestroyTask(taskId);
        ScriptContext_Enable();
        return;
    }

    sprite = &gSprites[objectEvent->spriteId];
    sprite->animPaused = TRUE;

    switch (data[1])
    {
    case 0: // opening, first frame
        RequestSpriteFrameImageCopy(POKEBOX_FRAME_OPEN1, sprite->oam.tileNum, sprite->images);
        if (++data[2] >= POKEBOX_OPEN1_DURATION)
        {
            data[2] = 0;
            data[1] = 1;
            PlaySE(SE_CLICK); // HYDRA play the open click ONCE, on the transition into the open frame
        }
        break;
    case 1: // opening, second frame
        RequestSpriteFrameImageCopy(POKEBOX_FRAME_OPEN2, sprite->oam.tileNum, sprite->images);
        if (++data[2] >= POKEBOX_OPEN2_DURATION)
        {
            data[2] = 0;
            data[1] = 2;
        }
        break;
    case 2: // flash on the open frame, then vanish for good
        RequestSpriteFrameImageCopy(POKEBOX_FRAME_OPEN2, sprite->oam.tileNum, sprite->images);
        if (++data[2] >= POKEBOX_BLINK_INTERVAL)
        {
            data[2] = 0;
            data[3]++;
            objectEvent->invisible = (data[3] & 1); // toggle via the object event, not the sprite
            if (data[3] >= POKEBOX_BLINK_TOGGLES)
            {
                objectEvent->invisible = TRUE;
                sprite->invisible = TRUE;
                DestroyTask(taskId);
                ScriptContext_Enable(); // resume the pickup script -> removeobject
            }
        }
        break;
    }
}

void StartPokeboxOpenAnim(void)
{
    u8 i;
    u8 taskId = CreateTask(Task_PokeboxOpenAnim, 0x50);
    s16 *data = gTasks[taskId].data;

    data[0] = 0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;

    // HYDRA Find the item-ball object event by GRAPHICS ID. The map objects use
    // OBJ_EVENT_GFX_ITEM_BALL, and this scan is the version that actually works.
    // (A later edit replaced this with a VAR_LAST_TALKED / TryGetObjectEventIdByLocalIdAndMap
    // lookup, which does NOT resolve item balls -> the task exited immediately with no anim/sound.
    // That was the regression; this restores last night's working fix.)
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (gObjectEvents[i].active && gObjectEvents[i].graphicsId == OBJ_EVENT_GFX_ITEM_BALL)
        {
            data[0] = i;
            return;
        }
    }

    data[1] = 3; // not found -> the task releases the script on its next tick
}
