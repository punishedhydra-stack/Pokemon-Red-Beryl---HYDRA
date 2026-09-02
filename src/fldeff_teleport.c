#include "global.h"
#include "field_effect.h"
#include "field_player_avatar.h"
#include "fldeff.h"
#include "follower_npc.h"
#include "party_menu.h"
#include "overworld.h"
#include "task.h"
#include "constants/field_effects.h"

static void StartTeleportFieldEffect(void);

bool32 SetUpFieldMove_Teleport(void)
{
    if (!CheckFollowerNPCFlag(FOLLOWER_NPC_FLAG_CAN_LEAVE_ROUTE))
        return FALSE;

    //HYDRA Teleport works from anywhere and is fully handled by the Fly-map path
    //HYDRA (party_menu.c opens CB2_OpenFlyMap, field_effect.c does the plain-fade warp).
    //HYDRA Do NOT set gPostMenuFieldCallback/gFieldCallback2 here - the old FieldCallback_Teleport
    //HYDRA was still firing after the warp, causing the spin animation and the bounce back to the heal spot.
    return TRUE;
}

bool8 FldEff_UseTeleport(void)
{
    u8 taskId = CreateFieldMoveTask();
    gTasks[taskId].data[8] = (u32)StartTeleportFieldEffect >> 16;
    gTasks[taskId].data[9] = (u32)StartTeleportFieldEffect;
    SetPlayerAvatarTransitionFlags(PLAYER_AVATAR_FLAG_ON_FOOT);
    return FALSE;
}

static void StartTeleportFieldEffect(void)
{
    FieldEffectActiveListRemove(FLDEFF_USE_TELEPORT);
    FldEff_TeleportWarpOut();
}
