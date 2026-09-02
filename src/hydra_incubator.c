// HYDRA Egg incubator secret base decoration logic.
//
// The incubator is an animated sprite decoration (OBJ_EVENT_GFX_INCUBATOR). When the
// player presses A on it, SecretBase_EventScript_IncubatorInteract runs these specials.
// Each incubator's state lives in gSaveBlock1Ptr->hydraIncubators[], keyed by the
// decoration's base-local tile position so up to HYDRA_NUM_INCUBATORS work independently.
//
// The egg is stored as a BoxPokemon; its MON_DATA_FRIENDSHIP is the hatch countdown,
// exactly like a party egg (decremented by the step hook added in a later stage).

#include "global.h"
#include "event_data.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "egg_hatch.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "sprite.h"
#include "task.h"
#include "hydra_incubator.h"
#include "constants/event_objects.h"

// Which incubator tile the player is currently interacting with, remembered across the
// ChoosePartyMon round-trip so StoreChosenEgg knows where to put the egg.
static EWRAM_DATA s8 sTargetX = 0;
static EWRAM_DATA s8 sTargetY = 0;

static struct HydraIncubator *FindIncubatorAt(s8 x, s8 y)
{
    u32 i;
    for (i = 0; i < HYDRA_NUM_INCUBATORS; i++)
    {
        struct HydraIncubator *inc = &gSaveBlock1Ptr->hydraIncubators[i];
        if (inc->occupied && inc->x == (u8)x && inc->y == (u8)y)
            return inc;
    }
    return NULL;
}

static s32 FindFreeIncubatorSlot(void)
{
    u32 i;
    for (i = 0; i < HYDRA_NUM_INCUBATORS; i++)
    {
        if (!gSaveBlock1Ptr->hydraIncubators[i].occupied)
            return i;
    }
    return -1;
}

// Snaps the incubator sprite standing at base-local (x,y) to a specific pic-table frame
// (0 = empty/frame 1). Used to reset a sprite the moment its egg is removed or hatched, so it
// updates without needing to reload the map.
static void SetIncubatorSpriteFrameAt(s8 x, s8 y, u8 frameIndex)
{
    u32 i;
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        struct ObjectEvent *oe = &gObjectEvents[i];
        struct Sprite *sprite;

        if (!oe->active
         || (s8)(oe->currentCoords.x - MAP_OFFSET) != x
         || (s8)(oe->currentCoords.y - MAP_OFFSET) != y)
            continue;

        sprite = &gSprites[oe->spriteId];
        sprite->animPaused = TRUE;
        RequestSpriteFrameImageCopy(frameIndex, sprite->oam.tileNum, sprite->images);
        return;
    }
}

// Records the incubator tile in front of the player and reports its state in VAR_RESULT:
// 0 = empty, 1 = an egg is incubating, 2 = the egg is ready to hatch.
void HydraIncubator_BeginInteract(void)
{
    s16 x, y;
    struct HydraIncubator *inc;

    GetXYCoordsOneStepInFrontOfPlayer(&x, &y);
    sTargetX = x - MAP_OFFSET;
    sTargetY = y - MAP_OFFSET;

    inc = FindIncubatorAt(sTargetX, sTargetY);
    if (inc == NULL)
    {
        gSpecialVar_Result = 0; // empty -> offer to place an egg
        return;
    }

    // Has an egg. VAR_RESULT = 1, and VAR_0x8005 = a progress bucket for the flavour text.
    // MON_DATA_FRIENDSHIP holds the remaining egg cycles (0 == ready to hatch).
    gSpecialVar_Result = 1;
    {
        u32 cycles = GetBoxMonData(&inc->egg, MON_DATA_FRIENDSHIP);
        if (cycles == 0)
            gSpecialVar_0x8005 = 4;   // ready to hatch
        else if (cycles <= 5)
            gSpecialVar_0x8005 = 0;   // "making sounds, almost ready"
        else if (cycles <= 10)
            gSpecialVar_0x8005 = 1;   // "occasionally moves, should hatch soon"
        else if (cycles <= 40)
            gSpecialVar_0x8005 = 2;   // "what will hatch, will take some time"
        else
            gSpecialVar_0x8005 = 3;   // "will take a long time"
    }
}

// Called every overworld step. Advances every occupied incubator's egg toward hatching
// at the same rate a party egg would (same step thresholds and Flame Body speed-up).
void HydraIncubator_Step(void)
{
    static u16 sStepCounter = 0;
    u32 i;
    u8 toSub;
    bool32 anyOccupied = FALSE;

    for (i = 0; i < HYDRA_NUM_INCUBATORS; i++)
    {
        if (gSaveBlock1Ptr->hydraIncubators[i].occupied)
        {
            anyOccupied = TRUE;
            break;
        }
    }
    if (!anyOccupied)
        return;

    // Belt-and-braces: make sure the sprite-animation task is running whenever there is an
    // egg to show, even if the base-load hook that normally starts it didn't run.
    HydraIncubator_StartAnimTask();

    sStepCounter++;
    if (((P_EGG_CYCLE_LENGTH <= GEN_3 || P_EGG_CYCLE_LENGTH == GEN_7) && sStepCounter >= 256)
     || (P_EGG_CYCLE_LENGTH == GEN_4 && sStepCounter >= 255)
     || ((P_EGG_CYCLE_LENGTH == GEN_5 || P_EGG_CYCLE_LENGTH == GEN_6) && sStepCounter >= 257)
     || (P_EGG_CYCLE_LENGTH >= GEN_8 && sStepCounter >= 128))
    {
        sStepCounter = 0;
        toSub = GetEggCyclesToSubtract();
        for (i = 0; i < HYDRA_NUM_INCUBATORS; i++)
        {
            struct HydraIncubator *inc = &gSaveBlock1Ptr->hydraIncubators[i];
            u32 cycles;

            if (!inc->occupied)
                continue;

            cycles = GetBoxMonData(&inc->egg, MON_DATA_FRIENDSHIP);
            if (cycles != 0)
            {
                cycles = (cycles >= toSub) ? cycles - toSub : 0;
                SetBoxMonData(&inc->egg, MON_DATA_FRIENDSHIP, &cycles);
            }
        }
    }
}

// Takes the egg back out of the incubator the player is facing and returns it to the party.
// VAR_RESULT: 1 = returned to party, 0 = party is full (or no egg here).
void HydraIncubator_RemoveEgg(void)
{
    struct HydraIncubator *inc = FindIncubatorAt(sTargetX, sTargetY);
    u32 count = gPartiesCount[B_TRAINER_PLAYER];

    if (inc == NULL || count >= PARTY_SIZE)
    {
        gSpecialVar_Result = 0; // no egg here, or party is full
        return;
    }

    // The party is kept compact, so slot [count] is the first empty slot.
    BoxMonToMon(&inc->egg, &gParties[B_TRAINER_PLAYER][count]);
    inc->occupied = FALSE;
    CalculatePlayerPartyCount();
    SetIncubatorSpriteFrameAt(sTargetX, sTargetY, 0); // sprite back to the empty frame now

    gSpecialVar_Result = 1; // returned to party
}

// Prepares a ready (0-cycle) incubator egg to hatch in place: moves it into an empty party
// slot and points VAR_0x8004 at it, so the script can reuse the vanilla EventScript_EggHatch
// cutscene ("Huh?" + hatch animation). Clears the incubator slot.
// VAR_RESULT: 1 = ready to run the hatch cutscene, 0 = party full (can't hatch here).
void HydraIncubator_PrepareHatch(void)
{
    struct HydraIncubator *inc = FindIncubatorAt(sTargetX, sTargetY);
    u32 count = gPartiesCount[B_TRAINER_PLAYER];

    if (inc == NULL || count >= PARTY_SIZE)
    {
        gSpecialVar_Result = 0; // party full - make room first
        return;
    }

    BoxMonToMon(&inc->egg, &gParties[B_TRAINER_PLAYER][count]);
    inc->occupied = FALSE;
    CalculatePlayerPartyCount();
    SetIncubatorSpriteFrameAt(sTargetX, sTargetY, 0); // sprite back to the empty frame now

    gSpecialVar_0x8004 = count; // the party slot EventScript_EggHatch will hatch
    gSpecialVar_Result = 1;
}

// ---- State-driven sprite animation ----
// The incubator object event is inanimate and the object-event anim system keeps it paused, so
// this small field task drives the displayed frame directly from save state each frame by
// copying the frame image into the sprite's VRAM:
//   frame 1 = empty, frames 2/3 = incubating (toggled), frame 4 = ready to hatch.
// Started when the base's decoration sprites spawn; cleared with all field tasks on map exit.
static void Task_IncubatorAnim(u8 taskId)
{
    u32 i;
    u16 timer = ++gTasks[taskId].data[0];

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        struct ObjectEvent *objEvent = &gObjectEvents[i];
        struct Sprite *sprite;
        struct HydraIncubator *inc;
        u8 frameIndex;

        if (!objEvent->active)
            continue;

        // Identify an occupied incubator purely by tile position -- the same base-local tile
        // the egg was stored under -- so this never depends on the object event's dynamic
        // graphics id. Empty tiles / other objects return NULL and are left alone.
        inc = FindIncubatorAt(objEvent->currentCoords.x - MAP_OFFSET, objEvent->currentCoords.y - MAP_OFFSET);
        if (inc == NULL)
            continue;

        if (GetBoxMonData(&inc->egg, MON_DATA_FRIENDSHIP) == 0)
            frameIndex = 3;                       // ready -> frame 4
        else
            frameIndex = (timer & 0x10) ? 2 : 1;  // incubating -> toggle frames 2/3

        // Drive the frame image straight into the sprite's VRAM (same call the engine's own
        // animation uses), bypassing the object event anim system which keeps it paused.
        sprite = &gSprites[objEvent->spriteId];
        sprite->animPaused = TRUE;
        RequestSpriteFrameImageCopy(frameIndex, sprite->oam.tileNum, sprite->images);
    }
}

// Ensures the sprite-animation task is running whenever there is an egg to show. Safe to call
// from anywhere (base load, after placing an egg, or on any field resume) -- it only spins up
// the task when an incubator actually holds an egg, and never creates a duplicate.
void HydraIncubator_StartAnimTask(void)
{
    u32 i;

    for (i = 0; i < HYDRA_NUM_INCUBATORS; i++)
    {
        if (gSaveBlock1Ptr->hydraIncubators[i].occupied)
        {
            if (!FuncIsActiveTask(Task_IncubatorAnim))
                CreateTask(Task_IncubatorAnim, 0x50);
            return;
        }
    }
}

// Runs after ChoosePartyMon (chosen party slot in VAR_0x8004). Moves the chosen egg out
// of the party and into the empty incubator tile recorded above.
// VAR_RESULT: 1 = stored, 0 = cancelled, 2 = not an egg, 3 = no free incubator slot.
void HydraIncubator_StoreChosenEgg(void)
{
    u32 slot = gSpecialVar_0x8004;
    struct Pokemon *mon;
    s32 freeSlot;
    struct HydraIncubator *inc;

    if (slot >= PARTY_SIZE)
    {
        gSpecialVar_Result = 0; // cancelled out of the party menu
        return;
    }

    mon = &gParties[B_TRAINER_PLAYER][slot];
    if (GetMonData(mon, MON_DATA_IS_EGG) != TRUE)
    {
        gSpecialVar_Result = 2; // not an egg
        return;
    }

    freeSlot = FindFreeIncubatorSlot();
    if (freeSlot < 0)
    {
        gSpecialVar_Result = 3; // every incubator slot is already full
        return;
    }

    inc = &gSaveBlock1Ptr->hydraIncubators[freeSlot];
    inc->egg = mon->box;
    inc->x = (u8)sTargetX;
    inc->y = (u8)sTargetY;
    inc->occupied = TRUE;

    // Remove the egg from the party (same as depositing into the daycare).
    ZeroMonData(mon);
    CompactPartySlots();
    CalculatePlayerPartyCount();

    // Opening the party picker reset the field tasks, so re-arm the animation task now -- the
    // incubator starts cycling the moment the egg goes in, without needing to take a step.
    HydraIncubator_StartAnimTask();

    gSpecialVar_Result = 1; // stored
}
