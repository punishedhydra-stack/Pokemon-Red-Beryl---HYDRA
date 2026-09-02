#include "global.h"
#include "battle.h"           // gBattleWeather
#include "battle_anim.h"      // gAnimScriptActive, gBattleAnimTable
#include "battle_weather.h"
#include "field_weather.h"    // GetCurrentWeather + ApplyDroughtColorMapToBgPalettes / RestoreBgPalettesFromUnfaded (sun tint)
#include "palette.h"          //HYDRA PLTT_ID, gPlttBufferUnfaded/Faded (sandstorm terrain tint)
#include "util.h"             //HYDRA BlendPalette (sandstorm terrain tint)
#include "constants/rgb.h"   //HYDRA RGB() macro for the sandstorm tint color
#include "random.h"           // Random2
#include "sound.h"            // PlaySE, IsSpecialSEPlaying
#include "m4a.h"              // m4aSongNumStop
#include "sprite.h"
#include "decompress.h"      // LoadCompressedSpriteSheet
#include "task.h"
#include "trig.h"             // Sin / gSineTable (smooth drought pulse)
#include "battle_main.h"      // IsBattleActionSelectionActive / IsBattleBeforeFirstTurn
#include "constants/battle.h" // B_WEATHER_*
#include "constants/battle_anim.h" // GET_TRUE_SPRITE_INDEX, ANIM_TAG_HAIL
#include "constants/songs.h"  // SE_RAIN / SE_DOWNPOUR / SE_THUNDERSTORM
#include "constants/weather.h"// WEATHER_* overworld ids

//HYDRA ---- Continuous in-battle weather overlay -----------------------------------
// One task renders whichever weather is active, ONLY in the two idle phases where nothing can faint
// (the pre-first-turn intro and the action menu); it is fully dormant during animations and all turn
// execution/faints (being active during a KO is what used to hang the game).
//
//   RAIN / SNOW  -> particle sprites (our copy of the engine rain-drop / snowflake template, under a
//                   PRIVATE gfx tag so neither the battle-anim gfx tracking nor the engine's own
//                   weather sprites can touch ours).
//   HAIL         -> the OVERWORLD tiny-hail sprite, reproduced EXACTLY: hail_tiny.png / hail.gbapal
//                   (gWeatherHailTinyTiles / gWeatherHailPalette) with the overworld fall -> ground
//                   pause -> vanish -> respawn motion. Priority 2 (like rain) so the UI stays on top. //HYDRA
//   SANDSTORM    -> the OVERWORLD sandstorm graphics (gWeatherSandstormTiles / palette) as scrolling
//                   64x64 OBJ_NORMAL sprites, kept in the upper battle area so they never cover the UI.
//   SUN          -> the overworld harsh-sun COLOR MAP (warm drought tint), gently pulsed over the
//                   terrain palettes only (BG palettes 2-4), so the UI + Pokemon are never touched.
//
// The sun tint edits the terrain palettes each frame from the untinted base and restores them the
// moment we go dormant / the weather ends.

extern const struct SpriteTemplate gRainDropSpriteTemplate;   // battle_anim_water.c
extern const struct SpriteTemplate gSnowFlakesSpriteTemplate; // battle_anim_ice.c
extern const u8  gWeatherSandstormTiles[];                    // field_weather_effect.c (overworld sand)
extern const u16 gSandstormWeatherPalette[];                  // field_weather_effect.c
//HYDRA in-battle hail now reuses the OVERWORLD tiny-hail graphics (was: hail-attack gfx + ice-crystal pop)
extern const u8  gWeatherHailTinyTiles[];                    // field_weather_effect.c (overworld tiny hail, 8x8 4bpp)
extern const u16 gWeatherHailPalette[];                      // field_weather_effect.c (overworld hail palette)

// Tuning.
#define WEATHER_SPAWN_INTERVAL    2    // frames between new particles (lower = heavier)
#define WEATHER_MAX_PARTICLES     14   // hard cap so we can never run the sprite pool dry
#define WEATHER_RAIN_VOLUME       160  // rain loudness (0-256). Lower = quieter.
#define WEATHER_RAIN_VOLUME_FULL  256  // restore value for other SE3 sounds
//HYDRA hail tuned to mirror the overworld tiny-hail sprite (graphics + fall/respawn motion)
#define BATTLE_HAIL_SPRITES       15   // match overworld targetHailSpriteCount
//HYDRA per-sprite RANDOM fall speed + pause so the 15 stones never fall in lockstep waves
#define BATTLE_HAIL_FALL_MIN      8    // fall speed = MIN + (rand % VAR) px/frame ...
#define BATTLE_HAIL_FALL_VAR      5    // ... -> 8..12 px/frame; differing speeds keep stones out of sync
#define BATTLE_HAIL_PAUSE_MIN     2    // ground pause = MIN + (rand % VAR) frames ...
#define BATTLE_HAIL_PAUSE_VAR     5    // ... -> 2..6 frames
#define BATTLE_HAIL_TILE_BYTES    0x20 // one 8x8 4bpp tile = 32 bytes (hail_tiny.png)
//HYDRA sandstorm now fills the WHOLE battle scene (3 rows) and adds the overworld swirl "circle" puffs
#define BATTLE_SAND_COLS          5    // columns of 64x64 fill tiles
#define BATTLE_SAND_ROWS          4    //HYDRA 4 rows (centers 0,64,128,192) so the fill still covers top..bottom while it SCROLLS VERTICALLY (sSandBaseY); 3 static rows froze a thin gap band at y~64
#define BATTLE_SAND_SPRITES       (BATTLE_SAND_COLS * BATTLE_SAND_ROWS) //HYDRA 20 fill sprites (5 cols x 4 rows)
#define BATTLE_SAND_SWIRLS        5    // rising swirl puffs (overworld NUM_SWIRL_SANDSTORM_SPRITES)
#define BATTLE_SAND_TILE_BYTES    0xA00 // full sheet: 64x64 fill (tiles 0-63) + 32x32 swirl (tiles 64-79)
//HYDRA Private tags, well clear of the battle-anim range (10000+) and battle-interface (~0xD6xx).
#define WEATHER_PARTICLE_TILE_TAG 0x4E20
#define WEATHER_PARTICLE_PAL_TAG  0x4E21
#define BATTLE_SAND_TILE_TAG      0x4E22
#define BATTLE_SAND_PAL_TAG       0x4E23
#define BATTLE_HAIL_TILE_TAG      0x4E24
#define BATTLE_HAIL_PAL_TAG       0x4E25
//HYDRA BATTLE_ICE_TILE_TAG / BATTLE_ICE_PAL_TAG removed with the ice-crystal impact pop

enum WeatherOverlayMode { WMODE_NONE, WMODE_PARTICLE, WMODE_HAIL, WMODE_SAND, WMODE_SUN };

static const struct SpriteTemplate *sSrcTemplate = NULL; // engine particle template we mirror, or NULL
static struct SpriteTemplate sWeatherTemplate;           // our private copy of it, with our tags
static u8 sActiveMode = WMODE_NONE;                       // what we last set up (to detect changes)
static bool8 sSandReady = FALSE;                          // sand sprites created
static u16 sSandBaseX = 0;                                // sand horizontal scroll offset
static u16 sSandBaseY = 0;                                //HYDRA sand VERTICAL scroll offset (0..63, wraps) -- mirrors the overworld sprite->y2 drift so the sparse fill never freezes a gap band
static bool8 sSunApplied = FALSE;                         // drought tint currently applied to the terrain palettes?
static bool8 sSandTintApplied = FALSE;                    //HYDRA sandstorm tan wash currently applied to the terrain palettes?
static u16 sSunPulse = 0;                                 // sun pulse phase

//HYDRA TRUE while end-of-turn hail/sandstorm damage is being applied (set in battle_end_turn.c), so the
// overlay stays visible through the damage window instead of going dormant like the rest of execution.
// Cleared here once we're back at the action menu.
bool8 gInBattleWeatherDamage = FALSE;

static void Task_BattleWeatherOverlay(u8 taskId);
static void SpriteCB_BattleSand(struct Sprite *sprite);
static void SpriteCB_BattleHail(struct Sprite *sprite);

//HYDRA Which visual the current weather needs. Follows the BATTLE weather once committed; during the
// intro (before it is set) it mirrors the OVERWORLD weather so it shows from the very start.
static u8 GetWeatherMode(const struct SpriteTemplate **particleSrcOut)
{
    *particleSrcOut = NULL;

    if (gBattleWeather != B_WEATHER_NONE)
    {
        if (gBattleWeather & B_WEATHER_RAIN)      { *particleSrcOut = &gRainDropSpriteTemplate;   return WMODE_PARTICLE; }
        if (gBattleWeather & B_WEATHER_HAIL)        return WMODE_HAIL;
        if (gBattleWeather & B_WEATHER_SNOW)      { *particleSrcOut = &gSnowFlakesSpriteTemplate; return WMODE_PARTICLE; }
        if (gBattleWeather & B_WEATHER_SANDSTORM)   return WMODE_SAND;
        if (gBattleWeather & B_WEATHER_SUN)         return WMODE_SUN;
        return WMODE_NONE;
    }

    if (IsBattleBeforeFirstTurn())
    {
        switch (GetCurrentWeather())
        {
        case WEATHER_RAIN:
        case WEATHER_RAIN_THUNDERSTORM:
        case WEATHER_DOWNPOUR:    *particleSrcOut = &gRainDropSpriteTemplate;   return WMODE_PARTICLE;
        case WEATHER_SNOW:        *particleSrcOut = &gSnowFlakesSpriteTemplate; return WMODE_PARTICLE;
        case WEATHER_HAIL:        return WMODE_HAIL; //HYDRA new overworld hail weather -> in-battle hail
        case WEATHER_SANDSTORM:   return WMODE_SAND;
        case WEATHER_DROUGHT:     return WMODE_SUN;
        }
    }
    return WMODE_NONE;
}

// The rain ambient loop to match its strength (only rain has a sound).
static u16 GetBattleRainSound(void)
{
    switch (GetCurrentWeather())
    {
    case WEATHER_RAIN_THUNDERSTORM: return SE_THUNDERSTORM;
    case WEATHER_DOWNPOUR:          return SE_DOWNPOUR;
    default:                        return SE_RAIN;
    }
}

//HYDRA Silence the looping rain SE (special looping SE on SE3) and restore its channel volume.
static void StopBattleRainSound(void)
{
    m4aSongNumStop(SE_RAIN);
    m4aSongNumStop(SE_DOWNPOUR);
    m4aSongNumStop(SE_THUNDERSTORM);
    m4aMPlayVolumeControl(&gMPlayInfo_SE3, TRACKS_ALL, WEATHER_RAIN_VOLUME_FULL);
}

//HYDRA Our overlay sprites all carry one of our private template pointers, so these can never touch
// the engine's own weather sprites.
static void DestroySpritesByTemplate(const struct SpriteTemplate *t)
{
    u32 i;
    for (i = 0; i < MAX_SPRITES; i++)
        if (gSprites[i].inUse && gSprites[i].template == t)
        {
            if (gSprites[i].oam.affineMode != ST_OAM_AFFINE_OFF) //HYDRA hail is affine -> free its matrix so we never leak one
                FreeSpriteOamMatrix(&gSprites[i]);
            DestroySprite(&gSprites[i]);
        }
}

static u32 CountSpritesByTemplate(const struct SpriteTemplate *t)
{
    u32 i, n = 0;
    for (i = 0; i < MAX_SPRITES; i++)
        if (gSprites[i].inUse && gSprites[i].template == t)
            n++;
    return n;
}

// ---- Particle weathers (rain / snow) ---------------------------------------------

//HYDRA Point our private template at `src` (swapping particle sets if the weather changed) and load a
// private copy of src's gfx under our tags. The tile-tag check self-heals after a Party/Bag reshow.
static void EnsureParticleAssets(const struct SpriteTemplate *src)
{
    if (sSrcTemplate != src)
    {
        DestroySpritesByTemplate(&sWeatherTemplate);
        FreeSpriteTilesByTag(WEATHER_PARTICLE_TILE_TAG);
        FreeSpritePaletteByTag(WEATHER_PARTICLE_PAL_TAG);
        sSrcTemplate = src;
        sWeatherTemplate = *src;
        sWeatherTemplate.tileTag = WEATHER_PARTICLE_TILE_TAG;
        sWeatherTemplate.paletteTag = WEATHER_PARTICLE_PAL_TAG;
    }

    if (GetSpriteTileStartByTag(WEATHER_PARTICLE_TILE_TAG) == TAG_NONE)
    {
        u32 idx = GET_TRUE_SPRITE_INDEX(src->tileTag);
        struct CompressedSpriteSheet sheet = { gBattleAnimTable[idx].pic.data, gBattleAnimTable[idx].pic.size, WEATHER_PARTICLE_TILE_TAG };
        struct SpritePalette palette = { gBattleAnimTable[idx].palette.data, WEATHER_PARTICLE_PAL_TAG };
        LoadCompressedSpriteSheet(&sheet);
        LoadSpritePalette(&palette);
    }
}

static void FreeParticleGfx(void)
{
    DestroySpritesByTemplate(&sWeatherTemplate);
    FreeSpriteTilesByTag(WEATHER_PARTICLE_TILE_TAG);
    FreeSpritePaletteByTag(WEATHER_PARTICLE_PAL_TAG);
    sSrcTemplate = NULL;
}

// ---- Hail (overworld tiny-hail sprite: fall -> ground pause -> vanish -> respawn) ---
//HYDRA Ported to look EXACTLY like the overworld hail (src/field_weather_effect.c UpdateHailSprite):
// the 8x8 hail_tiny.png graphic + palette, the overworld fall/pause/respawn motion, no diagonal drift,
// no ice-crystal impact pop. Priority 2 (same as rain) so the battle UI draws on top of the hail.

static void SpriteCB_BattleHail(struct Sprite *sprite);

//HYDRA 8x8 to match the overworld tiny hail; priority 2 so the UI stays above it, just like rain.
static const struct OamData sBattleHailOam =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(8x8),
    .size = SPRITE_SIZE(8x8),
    .priority = 2,
    .paletteNum = 0,
};

static const struct SpriteTemplate sBattleHailTemplate =
{
    .tileTag = BATTLE_HAIL_TILE_TAG,
    .paletteTag = BATTLE_HAIL_PAL_TAG,
    .oam = &sBattleHailOam,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_BattleHail,
};

//HYDRA Overworld UpdateHailSprite motion. data[0] = fall target, data[1] = phase, data[2] = ground
// pause counter, data[3] = X (fixed for this stone's life), data[4] = current fall offset. The camera
// parallax the overworld version adds is inert in battle (no scrolling camera), so it is dropped; the
// fall/pause/vanish/respawn cycle is what makes it identical to the overworld hail.
static void SpriteCB_BattleHail(struct Sprite *sprite)
{
    switch (sprite->data[1])
    {
    case 0: // falling
        sprite->data[4] += sprite->data[5]; //HYDRA per-sprite speed (data[5]) -> stones never move in lockstep
        if (sprite->data[4] >= sprite->data[0])
        {
            sprite->data[4] = sprite->data[0];
            sprite->data[1] = 1;
            sprite->data[2] = 0;
            sprite->data[6] = BATTLE_HAIL_PAUSE_MIN + (Random2() % BATTLE_HAIL_PAUSE_VAR); //HYDRA random short pause
        }
        break;
    case 1: // hitting ground
        if (++sprite->data[2] > sprite->data[6])
        {
            sprite->data[1] = 2;
            sprite->invisible = TRUE;
        }
        break;
    case 2: //HYDRA respawn: new X, target AND speed each time so stones stay desynced (no re-clumping)
        sprite->data[4] = -32;
        sprite->data[3] = Random2() % DISPLAY_WIDTH;
        sprite->data[0] = 80 + (Random2() % (DISPLAY_HEIGHT - 100)); // exact overworld fall target (80..139)
        sprite->data[5] = BATTLE_HAIL_FALL_MIN + (Random2() % BATTLE_HAIL_FALL_VAR); //HYDRA fresh random speed
        sprite->data[1] = 0;
        sprite->invisible = FALSE;
        break;
    }
    sprite->x = sprite->data[3];
    sprite->y = sprite->data[4];
}

//HYDRA Load the OVERWORLD tiny hail sheet (uncompressed, like the sand path) under our private tags.
static void EnsureHailAssets(void)
{
    if (GetSpriteTileStartByTag(BATTLE_HAIL_TILE_TAG) == TAG_NONE)
    {
        struct SpriteSheet sheet = { gWeatherHailTinyTiles, BATTLE_HAIL_TILE_BYTES, BATTLE_HAIL_TILE_TAG };
        struct SpritePalette palette = { gWeatherHailPalette, BATTLE_HAIL_PAL_TAG };
        LoadSpriteSheet(&sheet);
        LoadSpritePalette(&palette);
    }
}

static void FreeHailGfx(void)
{
    DestroySpritesByTemplate(&sBattleHailTemplate);
    FreeSpriteTilesByTag(BATTLE_HAIL_TILE_TAG);
    FreeSpritePaletteByTag(BATTLE_HAIL_PAL_TAG);
}

// ---- Sandstorm (overworld sand: 64x64 scrolling fill + rising "circle" swirl puffs) ----------

static void SpriteCB_BattleSandSwirl(struct Sprite *sprite);
static void SpriteCB_BattleSandSwirlEntrance(struct Sprite *sprite);

//HYDRA The 64x64 scrolling fill. Opaque (never dims the Pokemon), priority 1 (above the terrain,
// below the battle UI) -- unchanged from before; only the ROW COUNT changed (see EnsureSandAssets).
static const struct OamData sBattleSandOam =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,  //HYDRA opaque -> never dims the Pokemon behind it
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .priority = 1,
    .paletteNum = 0,
};

static const struct SpriteTemplate sBattleSandTemplate =
{
    .tileTag = BATTLE_SAND_TILE_TAG,
    .paletteTag = BATTLE_SAND_PAL_TAG,
    .oam = &sBattleSandOam,
    .anims = gDummySpriteAnimTable,  // default frame -> tiles 0..63 (the fill)
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_BattleSand,
};

//HYDRA The swirl "circle" puff: a 32x32 OBJ that uses the swirl tiles (offset 64 in the SAME sheet),
// the exact graphic + frame the overworld swirl uses. Same opaque/priority as the fill.
static const struct OamData sBattleSandSwirlOam =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .priority = 1,
    .paletteNum = 0,
};

//HYDRA Anim frame 64 = the swirl graphic (tiles 64..79), matching the overworld sSandstormSpriteAnimCmd1.
static const union AnimCmd sBattleSandSwirlAnimCmd[] =
{
    ANIMCMD_FRAME(64, 3),
    ANIMCMD_END,
};
static const union AnimCmd *const sBattleSandSwirlAnims[] = { sBattleSandSwirlAnimCmd };

static const struct SpriteTemplate sBattleSandSwirlTemplate =
{
    .tileTag = BATTLE_SAND_TILE_TAG,
    .paletteTag = BATTLE_SAND_PAL_TAG,
    .oam = &sBattleSandSwirlOam,
    .anims = sBattleSandSwirlAnims,  // default anim -> the swirl frame
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_BattleSandSwirlEntrance,
};

//HYDRA Port of the overworld UpdateSandstormSprite: 64x64 tiles panned across the screen from a shared
// scroll base and wrapped, AND drifted vertically (sSandBaseY) exactly like the overworld's sprite->y2.
// data[0] = this sprite's column (0..4); its row is baked into its y at spawn.
static void SpriteCB_BattleSand(struct Sprite *sprite)
{
    sprite->y2 = -(s16)sSandBaseY; //HYDRA vertical drift so the sparse fill FLOWS instead of freezing a thin gap band at y~64
    sprite->x = sSandBaseX + 32 + sprite->data[0] * 64;
    if (sprite->x >= DISPLAY_WIDTH + 32)
    {
        sprite->x = sSandBaseX + (DISPLAY_WIDTH * 2) - (4 - sprite->data[0]) * 64;
        sprite->x &= 0x1FF;
    }
}

//HYDRA Port of the overworld UpdateSandstormSwirlSprite: rise up the screen (--y, wrap at the top back
// to the bottom) while spiralling in a slowly GROWING circle. data[0] = radius, data[1] = wave index,
// data[2] = radius-growth counter. Sine index masked to stay in-bounds.
static void SpriteCB_BattleSandSwirl(struct Sprite *sprite)
{
    s32 x, y;

    if (--sprite->y < -48)
    {
        sprite->y = DISPLAY_HEIGHT + 48;
        sprite->data[0] = 4;                                   // reset radius on wrap
    }
    x = sprite->data[0] * gSineTable[sprite->data[1] & 0xFF];
    y = sprite->data[0] * gSineTable[(sprite->data[1] + 0x40) & 0xFF]; // +0x40 = cosine
    sprite->x2 = x >> 8;
    sprite->y2 = y >> 8;
    sprite->data[1] = (sprite->data[1] + 10) & 0xFF;
    if (++sprite->data[2] > 8)
    {
        sprite->data[2] = 0;
        sprite->data[0]++;                                     // circle slowly grows as it rises
    }
}

//HYDRA Staggered entrance (overworld sSwirlEntranceDelays) so the swirls don't all appear at once.
static void SpriteCB_BattleSandSwirlEntrance(struct Sprite *sprite)
{
    if (--sprite->data[3] < 0)               // data[3] = entrance delay
        sprite->callback = SpriteCB_BattleSandSwirl;
}

static const u16 sBattleSandSwirlDelays[BATTLE_SAND_SWIRLS] = { 0, 120, 80, 160, 40 };

static void EnsureSandAssets(void)
{
    if (GetSpriteTileStartByTag(BATTLE_SAND_TILE_TAG) == TAG_NONE)
    {
        struct SpriteSheet sheet = { gWeatherSandstormTiles, BATTLE_SAND_TILE_BYTES, BATTLE_SAND_TILE_TAG };
        struct SpritePalette palette = { gSandstormWeatherPalette, BATTLE_SAND_PAL_TAG };
        LoadSpriteSheet(&sheet);
        LoadSpritePalette(&palette);
        sSandReady = FALSE; // gfx (re)loaded -> (re)create the sprites below
    }

    if (!sSandReady)
    {
        u32 i;
        //HYDRA fill: 5 columns x 4 rows. Sprite y is the CENTER (0,64,128,192); with the vertical drift
        // (sSandBaseY, 0..-63) the 4 rows keep the whole scene covered while the sand flows -- 3 static
        // rows froze the sparse fill and left a thin gap band at y~64 (overworld avoids this by scrolling).
        for (i = 0; i < BATTLE_SAND_SPRITES; i++)
        {
            u8 id = CreateSprite(&sBattleSandTemplate, 120, (i / BATTLE_SAND_COLS) * 64, 4);
            if (id != MAX_SPRITES)
                gSprites[id].data[0] = i % BATTLE_SAND_COLS;
        }
        //HYDRA swirls: rising "little circle" puffs, spread across the width (x = 24,72,120,168,216)
        // and staggered in time. data seeded exactly like the overworld CreateSwirlSandstormSprites.
        for (i = 0; i < BATTLE_SAND_SWIRLS; i++)
        {
            u8 id = CreateSprite(&sBattleSandSwirlTemplate, i * 48 + 24, DISPLAY_HEIGHT + 48, 4);
            if (id != MAX_SPRITES)
            {
                gSprites[id].data[0] = 8;                       // radius
                gSprites[id].data[1] = i * 51;                  // staggered circle phase
                gSprites[id].data[2] = 0;                       // radius-growth counter
                gSprites[id].data[3] = sBattleSandSwirlDelays[i]; // entrance delay
            }
        }
        sSandReady = TRUE;
    }
}

static void FreeSandGfx(void)
{
    DestroySpritesByTemplate(&sBattleSandTemplate);
    DestroySpritesByTemplate(&sBattleSandSwirlTemplate);
    FreeSpriteTilesByTag(BATTLE_SAND_TILE_TAG);
    FreeSpritePaletteByTag(BATTLE_SAND_PAL_TAG);
    sSandReady = FALSE;
}

// ---- Sun (drought) terrain tint ---------------------------------------------------

//HYDRA The battle terrain (environment) palette occupies BG palettes 2-4; the UI (BG palettes
// 0,1,5,6,7) and the Pokemon (OBJ) live elsewhere, so tinting just these three leaves the UI + mons
// completely untouched -- the "UI shouldn't be affected" fix is by construction.
#define BATTLE_SUN_PAL_START 2
#define BATTLE_SUN_PAL_COUNT 3

//HYDRA Undo the drought tint: copy the untinted terrain palettes back over the displayed ones.
static void RestoreSun(void)
{
    if (sSunApplied)
    {
        RestoreBgPalettesFromUnfaded(BATTLE_SUN_PAL_START, BATTLE_SUN_PAL_COUNT);
        sSunApplied = FALSE;
    }
}

//HYDRA Drought: gently pulse the overworld harsh-sun COLOR MAP over the terrain palettes -- a warm
// sunlight tint (not a plain white brightness), exactly like the overworld drought. Only the terrain
// palettes are touched, so the UI and Pokemon never change.
static void ApplySunPulse(void)
{
    u16 idx;
    s16 stage;

    sSunPulse++;
    //HYDRA Smooth, fully-periodic SINE pulse: gSineTable wraps perfectly, so unlike the old triangle
    // there is no reversal point where the loop is visible. Centered on stage 4 (brighter) instead of
    // 2-3, breathing gently between ~3 and 5.
    idx = (sSunPulse >> 1) & 0xFF;                // one full sine cycle every ~512 frames
    stage = 4 + Sin(idx, 2);                      // Sin(idx,2) = -2..+2 -> stage ~2..5 around 4
    if (stage > 5)
        stage = 5;

    ApplyDroughtColorMapToBgPalettes(BATTLE_SUN_PAL_START, BATTLE_SUN_PAL_COUNT, (u8)stage);
    sSunApplied = TRUE;
}

// ---- Sandstorm terrain tint -------------------------------------------------------

//HYDRA The overworld sandstorm washes the whole scene tan; reproduce that in battle by blending just
// the terrain palettes (BG 2-4, the same three the sun tint uses) toward a warm sand color. BlendPalette
// reads the untinted gPlttBufferUnfaded and writes gPlttBufferFaded, so re-applying it every frame never
// compounds and the UI + Pokemon palettes are never touched. Undo with RestoreSandTint.
#define BATTLE_SAND_TINT_COLOR RGB(27, 23, 13) // warm tan haze
#define BATTLE_SAND_TINT_COEFF 6               // 6/16 blend -> hazy but the terrain stays clearly visible

static void RestoreSandTint(void)
{
    if (sSandTintApplied)
    {
        RestoreBgPalettesFromUnfaded(BATTLE_SUN_PAL_START, BATTLE_SUN_PAL_COUNT);
        sSandTintApplied = FALSE;
    }
}

static void ApplySandTint(void)
{
    BlendPalette(PLTT_ID(BATTLE_SUN_PAL_START), BATTLE_SUN_PAL_COUNT * 16, BATTLE_SAND_TINT_COEFF, BATTLE_SAND_TINT_COLOR);
    sSandTintApplied = TRUE;
}

// ---- Shared teardown -------------------------------------------------------------

//HYDRA Tear everything down (used on a weather-mode change and when weather ends). Each part is a
// no-op if that kind was not active, so this is always safe to call.
static void TearDownAll(void)
{
    FreeParticleGfx();
    FreeHailGfx();
    FreeSandGfx();
    RestoreSun();
    RestoreSandTint(); //HYDRA lift the sandstorm tan wash too
}

//HYDRA Public: force a clean reload after the battle screen is rebuilt (party/bag return).
void BattleWeather_ForceReload(void)
{
    FreeParticleGfx();
    FreeHailGfx();
    FreeSandGfx();
    sSunApplied = FALSE; // engine rebuilt the screen (fresh palettes) -> re-apply the tint next active frame
    sSandTintApplied = FALSE; //HYDRA same for the sandstorm tan wash
}

// ---- Task ------------------------------------------------------------------------

static void Task_BattleWeatherOverlay(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    const struct SpriteTemplate *particleSrc;
    u8 mode = GetWeatherMode(&particleSrc);

    // Weather switched to a different KIND of visual: tear the old one down first.
    if (mode != sActiveMode)
    {
        TearDownAll();
        sActiveMode = mode;
    }

    // No weather visual anymore: clean up and stop.
    if (mode == WMODE_NONE)
    {
        TearDownAll();
        StopBattleRainSound();
        DestroyTask(taskId);
        return;
    }

    //HYDRA Once we're back at the action menu the weather-damage window is over; end it so the overlay
    // returns to rain's normal timing (dormant through the next turn's execution).
    if (IsBattleActionSelectionActive())
        gInBattleWeatherDamage = FALSE;

    //HYDRA During the hail/sandstorm damage window (gInBattleWeatherDamage) the overlay stays visible NO
    // MATTER WHAT -- including over the hit + faint animations -- so the weather is on screen before,
    // during and after the damage. Outside that window it behaves exactly like rain: active only in the
    // idle phases (intro + action menu) and dormant while any animation plays.
    if (!gInBattleWeatherDamage
     && (gAnimScriptActive || (!IsBattleActionSelectionActive() && !IsBattleBeforeFirstTurn())))
    {
        DestroySpritesByTemplate(&sWeatherTemplate);
        DestroySpritesByTemplate(&sBattleHailTemplate); //HYDRA impact-pop template removed
        DestroySpritesByTemplate(&sBattleSandTemplate);
        DestroySpritesByTemplate(&sBattleSandSwirlTemplate); //HYDRA swirl puffs torn down with the fill
        sSandReady = FALSE; //HYDRA so EnsureSandAssets rebuilds the sand at the next menu, like rain self-heals
        RestoreSun();
        RestoreSandTint(); //HYDRA lift the sandstorm tan wash while the overlay is dormant
        StopBattleRainSound();
        return;
    }

    switch (mode)
    {
    case WMODE_PARTICLE:
        EnsureParticleAssets(particleSrc);
        if (particleSrc == &gRainDropSpriteTemplate)
        {
            if (!IsSpecialSEPlaying())
                PlaySE(GetBattleRainSound());
            m4aMPlayVolumeControl(&gMPlayInfo_SE3, TRACKS_ALL, WEATHER_RAIN_VOLUME);
        }
        else
        {
            StopBattleRainSound();
        }
        if (++data[0] >= WEATHER_SPAWN_INTERVAL)
        {
            data[0] = 0;
            if (CountSpritesByTemplate(&sWeatherTemplate) < WEATHER_MAX_PARTICLES)
                CreateSprite(&sWeatherTemplate, Random2() % DISPLAY_WIDTH, Random2() % (DISPLAY_HEIGHT / 2), 4);
        }
        break;

    case WMODE_HAIL:
        //HYDRA Keep ~15 self-respawning overworld-style hailstones alive (matches targetHailSpriteCount).
        // Each stone respawns itself in SpriteCB_BattleHail, so we only top up when the screen was rebuilt.
        EnsureHailAssets();
        StopBattleRainSound();
        while (CountSpritesByTemplate(&sBattleHailTemplate) < BATTLE_HAIL_SPRITES)
        {
            u16 target;
            u8 id = CreateSprite(&sBattleHailTemplate, 0, 0, 4);
            if (id == MAX_SPRITES)
                break;
            target = 80 + (Random2() % (DISPLAY_HEIGHT - 100));                 // exact overworld fall target
            gSprites[id].data[3] = Random2() % DISPLAY_WIDTH;                    // X (fixed for this stone)
            gSprites[id].data[0] = target;
            //HYDRA stagger: each new stone starts at a RANDOM height in its fall (not all at the top), so a
            // fresh screenful -- including after every return-from-animation -- is scattered, not one wave.
            gSprites[id].data[4] = -32 + (Random2() % (target + 32));           // random start height (-32..target-1)
            gSprites[id].data[5] = BATTLE_HAIL_FALL_MIN + (Random2() % BATTLE_HAIL_FALL_VAR); //HYDRA random speed
            gSprites[id].data[1] = 0;                                            // phase: falling
            gSprites[id].data[2] = 0;
            gSprites[id].invisible = FALSE;
        }
        break;

    case WMODE_SAND:
        EnsureSandAssets();
        ApplySandTint(); //HYDRA tan wash over the terrain -- the overworld sandstorm's full-screen tint
        sSandBaseX = (sSandBaseX - 2) & 0xFF; // scroll left
        sSandBaseY = (sSandBaseY + 1) & 0x3F; //HYDRA scroll UP 1px/frame, wrap every 64px (one tile) -> seamless; keeps the fill flowing so no fixed gap band
        StopBattleRainSound();
        break;

    case WMODE_SUN:
        ApplySunPulse();
        StopBattleRainSound();
        break;
    }
}

// Called once per frame from the battle main loop.
void UpdateBattleWeatherOverlay(void)
{
    const struct SpriteTemplate *unused;
    if (GetWeatherMode(&unused) != WMODE_NONE && !FuncIsActiveTask(Task_BattleWeatherOverlay))
        CreateTask(Task_BattleWeatherOverlay, 5);
}
