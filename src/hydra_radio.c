#include "global.h"
#include "hydra_radio.h"
#include "main.h"
#include "task.h"
#include "menu.h"
#include "window.h"
#include "text.h"
#include "string_util.h"
#include "sound.h"
#include "overworld.h"
#include "script.h"
#include "event_data.h"
#include "secret_base.h"
#include "constants/songs.h"
#include "constants/vars.h"
#include "constants/decorations.h"

// HYDRA radio decoration: a sound-test-style track browser (modelled on the debug Music menu).
// Reused, unchanged, from debug.c (which is always compiled):
extern const u8 *const sSongNames[];           // song id -> "MUS_..." name string (NULL if none)
extern const u8 *const gText_DigitIndicator[]; // "{LEFT_ARROW}+1{RIGHT_ARROW}", "+10", ...
extern u32 FindSong(int type, int mode, u32 fromSongId); // debug.c; skips invalid ids
// Mirror of debug.c's enum SongType / enum FindSongMode values:
#define HR_SONG_MUS       1
#define HR_SONG_FIRST_GE  0
#define HR_SONG_FIRST_GT  1
#define HR_SONG_LAST_LT   2

#define RADIO_DIGITS 4
static const s32 sRadioPow10[RADIO_DIGITS] = { 1, 10, 100, 1000 };

// "Music ID" relabelled to "Track #" per request; rest matches the debug sound test.
static const u8 sRadioText_Format[]  = _("Track No.: {STR_VAR_3}   {START_BUTTON} Stop\n{STR_VAR_1}\n{STR_VAR_2}");
static const u8 sRadioText_Dashes[]  = _("----------");

// Same geometry as the debug sound-test window it mirrors.
static const struct WindowTemplate sRadioWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 9,
    .tilemapTop = 1,
    .width = 20,
    .height = 6,
    .paletteNum = 15,
    .baseBlock = 1,
};

#define tWindowId    data[0]
#define tInput       data[1] // currently displayed song id
#define tCurrentSong data[2] // song actually playing right now (MUS_DUMMY if stopped)
#define tDigit       data[3] // which decimal digit the arrows step

static void RadioMenu_Print(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    const u8 *name = sSongNames[tInput];

    if (name == NULL)
        name = sRadioText_Dashes;

    StringCopy(gStringVar2, gText_DigitIndicator[tDigit]);
    ConvertIntToDecimalStringN(gStringVar3, tInput, STR_CONV_MODE_LEADING_ZEROS, RADIO_DIGITS);
    StringCopyPadded(gStringVar1, name, CHAR_SPACE, 30);
    StringExpandPlaceholders(gStringVar4, sRadioText_Format);

    FillWindowPixelBuffer(tWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized(tWindowId, FONT_NORMAL, gStringVar4, 0, 0, 0, NULL);
    CopyWindowToVram(tWindowId, COPYWIN_GFX);
}

static void RadioMenu_Close(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    // Remember whatever is playing (or off) so it auto-plays next time you enter your base.
    VarSet(VAR_HYDRA_RADIO_TRACK, tCurrentSong);
    if (tCurrentSong == MUS_DUMMY)
        PlayNewMapMusic(gMapHeader.music); // stopped -> restore the base's default music

    ClearStdWindowAndFrameToTransparent(tWindowId, TRUE);
    RemoveWindow(tWindowId);
    DestroyTask(taskId);
    ScriptContext_Enable(); // resume SecretBase_EventScript_RadioInteract past its waitstate
}

static void Task_RadioMenu(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (JOY_NEW(DPAD_UP))
    {
        for (s32 i = 0; i < sRadioPow10[tDigit]; i++)
            tInput = FindSong(HR_SONG_MUS, HR_SONG_FIRST_GT, tInput);
        RadioMenu_Print(taskId);
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        for (s32 i = 0; i < sRadioPow10[tDigit]; i++)
            tInput = FindSong(HR_SONG_MUS, HR_SONG_LAST_LT, tInput);
        RadioMenu_Print(taskId);
    }
    else if (JOY_NEW(DPAD_LEFT))
    {
        if (tDigit > 0)
            tDigit--;
        RadioMenu_Print(taskId);
    }
    else if (JOY_NEW(DPAD_RIGHT))
    {
        if (tDigit < RADIO_DIGITS - 1)
            tDigit++;
        RadioMenu_Print(taskId);
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlayNewMapMusic(tInput); // preview + make it the current track
        tCurrentSong = tInput;
    }
    else if (JOY_NEW(START_BUTTON))
    {
        StopMapMusic();
        tCurrentSong = MUS_DUMMY;
    }
    else if (JOY_NEW(B_BUTTON))
    {
        RadioMenu_Close(taskId);
    }
}

// Special opened from SecretBase_EventScript_RadioInteract (which lockall's + waitstate's).
void HydraRadio_OpenMenu(void)
{
    u8 taskId = CreateTask(Task_RadioMenu, 0x50);
    s16 *data = gTasks[taskId].data;
    u16 saved = VarGet(VAR_HYDRA_RADIO_TRACK);
    u8 windowId;

    LoadMessageBoxAndBorderGfx();
    windowId = AddWindow(&sRadioWindowTemplate);
    DrawStdWindowFrame(windowId, FALSE);
    CopyWindowToVram(windowId, COPYWIN_FULL); // map the window onto the BG so it's actually visible

    tWindowId    = windowId;
    tCurrentSong = saved; // whatever is currently playing (set by PlayOnEnter), or off
    tInput       = (saved != MUS_DUMMY) ? saved : FindSong(HR_SONG_MUS, HR_SONG_FIRST_GE, MUS_DUMMY);
    tDigit       = 0;

    RadioMenu_Print(taskId);
}

#undef tWindowId
#undef tInput
#undef tCurrentSong
#undef tDigit

static bool32 PlayerBaseHasRadio(void)
{
    u32 i;
    for (i = 0; i < DECOR_MAX_SECRET_BASE; i++)
    {
        if (gSaveBlock1Ptr->secretBases[0].decorations[i] == DECOR_RADIO)
            return TRUE;
    }
    return FALSE;
}

// The chosen radio track, IF the player is standing in their own secret base with a radio placed.
// Returns MUS_DUMMY otherwise. GetCurrLocationDefaultMusic() calls this so the track becomes the
// base's default music -- it then plays on load (no timing race) and survives any music reset.
u16 HydraRadio_GetBaseMusicOverride(void)
{
    if (!CurMapIsSecretBase())
        return MUS_DUMMY;
    if (VarGet(VAR_CURRENT_SECRET_BASE) != 0) // 0 == the player's own base
        return MUS_DUMMY;
    if (!PlayerBaseHasRadio())
        return MUS_DUMMY;

    return VarGet(VAR_HYDRA_RADIO_TRACK); // MUS_DUMMY when set to "off"
}

// Applies the chosen radio track as the current music when the player is standing in their OWN
// secret base and a radio is placed. Idempotent: it only (re)starts the song if it isn't already
// playing, so it's safe to call from the map-resume / return-to-field hooks (which run AFTER the
// map's default music is set, beating the load-time music race that stopped it persisting).
void HydraRadio_PlayOnEnter(void)
{
    u16 song;

    if (!CurMapIsSecretBase())                // must be inside a secret base map
        return;
    if (VarGet(VAR_CURRENT_SECRET_BASE) != 0) // 0 == the player's own base (not a visited one)
        return;
    if (!PlayerBaseHasRadio())
        return;

    song = VarGet(VAR_HYDRA_RADIO_TRACK);
    if (song != MUS_DUMMY && GetCurrentMapMusic() != song) // don't restart if it's already playing
        PlayNewMapMusic(song);
}
