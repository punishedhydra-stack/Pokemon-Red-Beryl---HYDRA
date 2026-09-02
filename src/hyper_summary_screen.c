#include "global.h"
#include "main.h"
#include "battle.h"
#include "battle_anim.h"
#include "frontier_util.h"
#include "battle_message.h"
#include "battle_tent.h"
#include "battle_factory.h"
#include "bg.h"
#include "contest.h"
#include "contest_effect.h"
#include "data.h"
#include "daycare.h"
#include "decompress.h"
#include "dynamic_placeholder_text_util.h"
#include "event_data.h"
#include "gpu_regs.h"
#include "graphics.h"
#include "international_string_util.h"
#include "item.h"
#include "link.h"
#include "m4a.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "mon_markings.h"
#include "move_relearner.h"
#include "naming_screen.h"
#include "party_menu.h"
#include "palette.h"
#include "pokeball.h"
#include "pokemon.h"
#include "pokemon_sprite_visualizer.h"
#include "pokemon_storage_system.h"
#include "pokemon_summary_screen.h"
#include "hyper_summary_screen.h" //HYDRA
#include "pokerus.h"
#include "region_map.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "tv.h"
#include "window.h"
#include "constants/battle_move_effects.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/party_menu.h"
#include "constants/region_map_sections.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// Screen titles (upper left)
#define PSS_LABEL_WINDOW_POKEMON_INFO_TITLE 0
#define PSS_LABEL_WINDOW_POKEMON_SKILLS_TITLE 1
#define PSS_LABEL_WINDOW_BATTLE_MOVES_TITLE 2
#define PSS_LABEL_WINDOW_CONTEST_MOVES_TITLE 3

// Button control text (upper right)
#define PSS_LABEL_WINDOW_PROMPT_UTILITY 4 // Handles "Switch", "Info", and "Cancel" prompts. Also handles the "Rename" and "IVs"/"EVs"/"STATS" prompts if P_SUMMARY_SCREEN_RENAME and P_SUMMARY_SCREEN_IV_EV_INFO are true, respectively
#define PSS_LABEL_WINDOW_PROMPT_INFO 5 // unused
#define PSS_LABEL_WINDOW_PROMPT_SWITCH 6 // unused
#define PSS_LABEL_WINDOW_UNUSED1 7

// Info screen
#define PSS_LABEL_WINDOW_POKEMON_INFO_RENTAL 8
#define PSS_LABEL_WINDOW_POKEMON_INFO_TYPE 9

// Skills screen
#define PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_LEFT 10 // HP, Attack, Defense
#define PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_RIGHT 11 // Sp. Attack, Sp. Defense, Speed
#define PSS_LABEL_WINDOW_POKEMON_SKILLS_EXP 12 // EXP, Next Level
#define PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS 13

// Moves screen
#define PSS_LABEL_WINDOW_MOVES_POWER_ACC 14 // Also contains the power and accuracy values
#define PSS_LABEL_WINDOW_MOVES_APPEAL_JAM 15
#define PSS_LABEL_WINDOW_PROMPT_RELEARN 16

// Above/below the Pokémon's portrait (left)
#define PSS_LABEL_WINDOW_PORTRAIT_DEX_NUMBER 17
#define PSS_LABEL_WINDOW_PORTRAIT_NICKNAME 18 // The upper name
#define PSS_LABEL_WINDOW_PORTRAIT_SPECIES 19 // The lower name
#define PSS_LABEL_WINDOW_END 20

// Dynamic fields for the Pokémon Info page
#define PSS_DATA_WINDOW_INFO_ORIGINAL_TRAINER 0
#define PSS_DATA_WINDOW_INFO_ID 1
#define PSS_DATA_WINDOW_INFO_ABILITY 2
#define PSS_DATA_WINDOW_INFO_MEMO 3

// Dynamic fields for the Pokémon Skills page
#define PSS_DATA_WINDOW_SKILLS_HELD_ITEM 0
#define PSS_DATA_WINDOW_SKILLS_RIBBON_COUNT 1
#define PSS_DATA_WINDOW_SKILLS_STATS_LEFT 2 // HP, Attack, Defense
#define PSS_DATA_WINDOW_SKILLS_STATS_RIGHT 3 // Sp. Attack, Sp. Defense, Speed
#define PSS_DATA_WINDOW_EXP 4 // Exp, next level

// Dynamic fields for the Battle Moves and Contest Moves pages.
#define PSS_DATA_WINDOW_MOVE_NAMES 0
#define PSS_DATA_WINDOW_MOVE_PP 1
#define PSS_DATA_WINDOW_MOVE_DESCRIPTION 2

#define MOVE_SELECTOR_SPRITES_COUNT 10
#define TYPE_ICON_SPRITE_COUNT (MAX_MON_MOVES + 1)
// for the spriteIds field in PokemonSummaryScreenData
enum
{
    SPRITE_ARR_ID_MON,
    SPRITE_ARR_ID_BALL,
    SPRITE_ARR_ID_STATUS,
    SPRITE_ARR_ID_TYPE, // 2 for mon types, 5 for move types(4 moves and 1 to learn), used interchangeably, because mon types and move types aren't shown on the same screen
    SPRITE_ARR_ID_MOVE_SELECTOR1 = SPRITE_ARR_ID_TYPE + TYPE_ICON_SPRITE_COUNT, // 10 sprites that make up the selector
    SPRITE_ARR_ID_MOVE_SELECTOR2 = SPRITE_ARR_ID_MOVE_SELECTOR1 + MOVE_SELECTOR_SPRITES_COUNT,
    //HYDRA R/A button icons on the skills page. Drawn as sprites rather than a
    // window because a window's tilemap overwrites whatever it overlaps, and the
    // only free tile row could not fit them where they need to sit.
    SPRITE_ARR_ID_HYDRA_R_BUTTON = SPRITE_ARR_ID_MOVE_SELECTOR2 + MOVE_SELECTOR_SPRITES_COUNT,
    SPRITE_ARR_ID_COUNT
};

#define TILE_EMPTY_APPEAL_HEART  0x1039
#define TILE_FILLED_APPEAL_HEART 0x103A
#define TILE_FILLED_JAM_HEART    0x103C
#define TILE_EMPTY_JAM_HEART     0x103D

static EWRAM_DATA struct PokemonSummaryScreenData
{
    /*0x00*/ union {
        struct Pokemon *mons;
        struct BoxPokemon *boxMons;
    } monList;
    /*0x04*/ MainCallback callback;
    /*0x08*/ struct Sprite *markingsSprite;
    /*0x0C*/ struct Pokemon currentMon;
    /*0x70*/ struct PokeSummary
    {
        enum Species species; // 0x0
        enum Species species2; // 0x2
        u8 isEgg:1; // 0x4
        u8 isShiny:1;
        u8 padding:6;
        u8 level; // 0x5
        u8 ribbonCount; // 0x6
        u8 ailment; // 0x7
        u8 abilityNum; // 0x8
        metloc_u8_t metLocation; // 0x9
        u8 metLevel; // 0xA
        u8 metGame; // 0xB
        u32 pid; // 0xC
        u32 exp; // 0x10
        enum Move moves[MAX_MON_MOVES]; // 0x14
        u8 pp[MAX_MON_MOVES]; // 0x1C
        u16 currentHP; // 0x20
        u16 maxHP; // 0x22
        u16 atk; // 0x24
        u16 def; // 0x26
        u16 spatk; // 0x28
        u16 spdef; // 0x2A
        u16 speed; // 0x2C
        enum Item item; // 0x2E
        u16 friendship; // 0x30
        u8 OTGender; // 0x32
        u8 nature; // 0x33
        u8 ppBonuses; // 0x34
        u8 sanity; // 0x35
        u8 OTName[17]; // 0x36
        u32 OTID; // 0x48
        enum Type teraType;
        u8 mintNature;
    } summary;
    u16 bgTilemapBuffers[PSS_PAGE_COUNT][2][0x400];
    u8 mode;
    u8 skillsPageMode;
    u8 selectedStat; //HYDRA which stat the edit cursor is on (0-2 left col, 3-5 right col)
    bool8 statEditing; //HYDRA TRUE once A is pressed on a stat and the value is being changed
    bool8 limitBeeped; //HYDRA TRUE after the "can't go further" beep fires for the current hold; reset on release
    u8 promptIconX; //HYDRA window-relative X of the A icon, so the R sprite can sit beside it
    u8 infoSelectedField; //HYDRA info page cursor: 0 = ability, 1 = nature
    bool8 infoEditing;    //HYDRA TRUE while cycling that field's value
    bool8 isBoxMon;
    u8 curMonIndex;
    u8 maxMonIndex;
    u8 currPageIndex;
    u8 minPageIndex;
    u8 maxPageIndex;
    bool8 lockMonFlag; // This is used to prevent the player from changing Pokémon in the move deleter select, etc, but it is not needed because the input is handled differently there
    u16 newMove;
    u8 firstMoveIndex;
    u8 secondMoveIndex;
    bool8 lockMovesFlag; // This is used to prevent the player from changing position of moves in a battle or when trading.
    u8 bgDisplayOrder; // Determines the order page backgrounds are loaded while scrolling between them
    bool8 hasRelearnableMoves;
    u8 windowIds[8];
    u8 spriteIds[SPRITE_ARR_ID_COUNT];
    s16 switchCounter; // Used for various switch statement cases that decompress/load graphics or Pokémon data
    u8 unk_filler4[6];
    u8 categoryIconSpriteId;
} *sMonSummaryScreen = NULL;

EWRAM_DATA u8 gLastViewedHyperMonIndex = 0;
static EWRAM_DATA u8 sMoveSlotToReplace = 0;
ALIGNED(4) static EWRAM_DATA u8 sAnimDelayTaskId = 0;
EWRAM_DATA MainCallback gInitialHyperSummaryScreenCallback = NULL; // stores callback from the first time the screen is opened from the party or PC menu

// forward declarations
static bool8 LoadGraphics(void);
static void CB2_InitSummaryScreen(void);
static void InitBGs(void);
static bool8 DecompressGraphics(void);
static void CopyMonToSummaryStruct(struct Pokemon *);
static bool8 ExtractMonDataToSummaryStruct(struct Pokemon *);
static void SetDefaultTilemaps(void);
static void CloseSummaryScreen(u8);
static void Task_HandleInput(u8);
static void ChangeSummaryPokemon(u8, s8);
static void Task_ChangeSummaryMon(u8);
static s8 AdvanceMonIndex(s8);
static s8 AdvanceMultiBattleMonIndex(s8);
static bool8 IsValidToViewInMulti(struct Pokemon *);
static void ChangePage(u8, s8);
static void PssScrollRight(u8);
static void PssScrollRightEnd(u8);
static void PssScrollLeft(u8);
static void PssScrollLeftEnd(u8);
static void TryDrawExperienceProgressBar(void);
static void SwitchToMoveSelection(u8);
static void Task_HandleInput_MoveSelect(u8);
static bool8 HasMoreThanOneMove(void);
static void ChangeSelectedMove(s16 *, s8, u8 *);
static void CloseMoveSelectMode(u8);
static void SwitchToMovePositionSwitchMode(u8);
static void Task_HandleInput_MovePositionSwitch(u8);
static void ExitMovePositionSwitchMode(u8, bool8);
static void SwapMonMoves(struct Pokemon *, u8, u8);
static void SwapBoxMonMoves(struct BoxPokemon *, u8, u8);
static void Task_SetHandleReplaceMoveInput(u8);
static void Task_HandleReplaceMoveInput(u8);
static bool8 CanReplaceMove(void);
static void ShowCantForgetHMsWindow(u8);
static void Task_HandleInputCantForgetHMsMoves(u8);
static void DrawPagination(void);
static void PositionPowerAccSlidingWindow(u16, s16);
static void Task_SlidePowerAccWindow(u8);
static void PositionAppealJamSlidingWindow(u16, s16, enum Move move);
static void Task_SlideAppealJamWindow(u8);
static void PositionStatusSlidingWindow(u16, s16);
static void Task_SlideStatusWindow(u8);
static void TilemapFiveMovesDisplay(u16 *, u16, bool8);
static void DrawPokerusCuredSymbol(struct Pokemon *);
static void DrawExperienceProgressBar(struct Pokemon *);
static void DrawContestMoveHearts(enum Move move);
static void LimitEggSummaryPageDisplay(void);
static void ResetWindows(void);
static void PrintMonInfo(void);
static void PrintNotEggInfo(void);
static void PrintEggInfo(void);
static void PrintGenderSymbol(struct Pokemon *, enum Species);
static void PrintPageNamesAndStats(void);
static void PutPageWindowTilemaps(u8);
static void ClearPageWindowTilemaps(u8);
static void RemoveWindowByIndex(u8);
static void PrintPageSpecificText(u8);
static void CreateTextPrinterTask(u8);
static void PrintInfoPageText(void);
static void Task_PrintInfoPage(u8);
static void PrintMonOTName(void);
static void PrintMonOTID(void);
static void PrintMonAbilityName(void);
static void PrintMonAbilityDescription(void);
static void BufferMonTrainerMemo(void);
static void PrintMonTrainerMemo(void);
static void BufferNatureString(void);
static void GetMetLevelString(u8 *);
static bool8 DoesMonOTMatchOwner(void);
static bool8 DidMonComeFromGBAGames(void);
static bool8 IsInGamePartnerMon(void);
static void PrintEggOTName(void);
static void PrintEggOTID(void);
static void PrintEggState(void);
static void PrintEggMemo(void);
static void Task_PrintSkillsPage(u8);
static void PrintHeldItemName(void);
static void PrintSkillsPageText(void);
static void PrintRibbonCount(void);
static void BufferLeftColumnStats(void);
static void PrintLeftColumnStats(void);
static void BufferRightColumnStats(void);
static void PrintRightColumnStats(void);
static void PrintExpPointsNextLevel(void);
static void PrintBattleMoves(void);
static void Task_PrintBattleMoves(u8);
static void PrintMoveNameAndPP(u8);
static void PrintContestMoves(void);
static void Task_PrintContestMoves(u8);
static void PrintContestMoveDescription(u8);
static void PrintMoveDetails(enum Move move);
static void PrintNewMoveDetailsOrCancelText(void);
static void AddAndFillMoveNamesWindow(void);
static void SwapMovesNamesPP(u8, u8);
static void PrintHMMovesCantBeForgotten(void);
static void ResetSpriteIds(void);
static void SetSpriteInvisibility(u8, bool8);
static void HidePageSpecificSprites(void);
static void SetTypeIcons(void);
static void CreateMoveTypeIcons(void);
static void SetMonTypeIcons(void);
static void SetMoveTypeIcons(void);
static void SetContestMoveTypeIcons(void);
static void SetNewMoveTypeIcon(void);
static void SwapMovesTypeSprites(u8, u8);
static u8 LoadMonGfxAndSprite(struct Pokemon *, s16 *);
static u8 CreateMonSprite(struct Pokemon *);
static void SpriteCB_Pokemon(struct Sprite *);
static void StopPokemonAnimations(void);
static void CreateMonMarkingsSprite(struct Pokemon *);
static void RemoveAndCreateMonMarkingsSprite(struct Pokemon *);
static void CreateCaughtBallSprite(struct Pokemon *);
static void CreateSetStatusSprite(void);
static void CreateMoveSelectorSprites(u8);
static void SpriteCB_MoveSelector(struct Sprite *);
static void DestroyMoveSelectorSprites(u8);
//HYDRA stat edit cursor (skills page)
static void SwitchToStatSelection(u8);
static void Task_HandleInput_StatSelect(u8);
static void Task_HandleInput_StatEdit(u8);
static void AdjustSelectedStat(u8, s32);
static void CloseStatSelectMode(u8);
//HYDRA info page ability/nature edit cursor
static void SwitchToInfoSelection(u8);
static void Task_HandleInput_InfoSelect(u8);
static void Task_HandleInput_InfoEdit(u8);
static void CloseInfoSelectMode(u8);
static void UpdateInfoSelectorSprites(void);
static void CycleSelectedInfoField(s32);
static void CreateStatSelectorSprites(void);
static void UpdateStatSelectorSprites(void);
static void DestroyStatSelectorSprites(void);
static void SpriteCB_StatSelector(struct Sprite *);
static void SetMainMoveSelectorColor(u8);
static void KeepMoveSelectorVisible(u8);
static void SummaryScreen_DestroyAnimDelayTask(void);
static bool32 ShouldShowMoveRelearner(void);
static bool32 ShouldShowRename(void);
static bool32 ShouldShowIvEvPrompt(void);
static void DrawSkillsRButtonIcon(void); //HYDRA (defined later, called from ShowMonSkillsInfo above it)
static bool32 HyperPC_CanEditSkillsMode(u8 mode); //HYDRA badge gate, defined further down
static void BufferLeftColumnIvEvStats(void);
static void ShowUtilityPrompt(s16 mode);
static void ShowMonSkillsInfo(u8 taskId, s16 mode);
static void WriteToStatsTilemapBuffer(u32 length, u32 block, u32 statsCoordX, u32 statsCoordY);
void Hyper_ExtractMonSkillStatsData(struct Pokemon *mon, struct PokeSummary *sum);
void Hyper_ExtractMonSkillIvData(struct Pokemon *mon, struct PokeSummary *sum);
void Hyper_ExtractMonSkillEvData(struct Pokemon *mon, struct PokeSummary *sum);
static void PrintTextOnWindow(u8 windowId, const u8 *string, u8 x, u8 y, u8 lineSpacing, u8 colorId);
static void PrintTextOnWindowWithFont(u8 windowId, const u8 *string, u8 x, u8 y, u8 lineSpacing, u8 colorId, u32 fontId);
static const u8 *GetLetterGrade(u32 stat);
static u8 AddWindowFromTemplateList(const struct WindowTemplate *template, u8 templateId);
static u8 IncrementSkillsStatsMode(u8 mode);
static void ClearStatLabel(u32 length, u32 statsCoordX, u32 statsCoordY);
u32 Hyper_GetAdjustedIvData(struct Pokemon *mon, u32 stat);
static void UpdateMoveRelearnerState(bool32 goingDown);
static void UpdateRelearnPrompt(void);
static struct BoxPokemon *GetCurrentBoxmon(void);

#define IS_MOVE_PAGE(page) (page == PSS_PAGE_BATTLE_MOVES || page == PSS_PAGE_CONTEST_MOVES)

static const struct BgTemplate sBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0,
    },
    {
        .bg = 1,
        .charBaseIndex = 2,
        .mapBaseIndex = 27,
        .screenSize = 1,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0,
    },
    {
        .bg = 2,
        .charBaseIndex = 2,
        .mapBaseIndex = 25,
        .screenSize = 1,
        .paletteMode = 0,
        .priority = 2,
        .baseTile = 0,
    },
    {
        .bg = 3,
        .charBaseIndex = 2,
        .mapBaseIndex = 29,
        .screenSize = 1,
        .paletteMode = 0,
        .priority = 3,
        .baseTile = 0,
    },
};

struct SlidingWindow
{
    const u16 *gfx;
    u16 defaultTile;
    u8 width;
    u8 height;
    u8 left;
    u8 top;
};

static const u16 sStatusTilemap[] = INCBIN_U16("graphics/hyper_summary_screen/status_tilemap.bin");
static const struct SlidingWindow sStatusSlidingWindow1 =
{
    .gfx = sStatusTilemap,
    .defaultTile = 1,
    .width = 10,
    .height = 2,
    .left = 0,
    .top = 18
};
static const struct SlidingWindow sStatusSlidingWindow2 =
{
    .gfx = sStatusTilemap,
    .defaultTile = 1,
    .width = 10,
    .height = 2,
    .left = 0,
    .top = 50
};
static const struct SlidingWindow sPowerAccSlidingWindow =
{
    .gfx = gSummaryScreen_MoveEffect_Battle_Tilemap,
    .defaultTile = 0,
    .width = 10,
    .height = 7,
    .left = 0,
    .top = 45
};
static const struct SlidingWindow sAppealJamSlidingWindow =
{
    .gfx = gSummaryScreen_MoveEffect_Contest_Tilemap,
    .defaultTile = 0,
    .width = 10,
    .height = 7,
    .left = 0,
    .top = 45
};
static const s8 sMultiBattleOrder[] = {0, 2, 3, 1, 4, 5};
static const struct WindowTemplate sSummaryTemplate[] =
{
    [PSS_LABEL_WINDOW_POKEMON_INFO_TITLE] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 0,
        .width = 11,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 1,
    },
    [PSS_LABEL_WINDOW_POKEMON_SKILLS_TITLE] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 0,
        .width = 11,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 23,
    },
    [PSS_LABEL_WINDOW_BATTLE_MOVES_TITLE] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 0,
        .width = 11,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 45,
    },
    [PSS_LABEL_WINDOW_CONTEST_MOVES_TITLE] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 0,
        .width = 11,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 67,
    },
    [PSS_LABEL_WINDOW_PROMPT_UTILITY] = {
        .bg = 0,
        .tilemapLeft = 22,
        .tilemapTop = 0,
        .width = 8,
        .height = 2,
        .paletteNum = 7,
        .baseBlock = 89,
    },
    [PSS_LABEL_WINDOW_PROMPT_INFO] = {
        .bg = 0,
        .tilemapLeft = 22,
        .tilemapTop = 0,
        .width = 8,
        .height = 2,
        .paletteNum = 7,
        .baseBlock = 105,
    },
    [PSS_LABEL_WINDOW_PROMPT_SWITCH] = {
        .bg = 0,
        .tilemapLeft = 22,
        .tilemapTop = 0,
        .width = 8,
        .height = 2,
        .paletteNum = 7,
        .baseBlock = 121,
    },
    [PSS_LABEL_WINDOW_UNUSED1] = {
        .bg = 0,
        .tilemapLeft = 11,
        .tilemapTop = 4,
        .width = 0,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 137,
    },
    [PSS_LABEL_WINDOW_POKEMON_INFO_RENTAL] = {
        .bg = 0,
        .tilemapLeft = 11,
        .tilemapTop = 4,
        .width = 18,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 137,
    },
    [PSS_LABEL_WINDOW_POKEMON_INFO_TYPE] = {
        .bg = 0,
        .tilemapLeft = 11,
        .tilemapTop = 6,
        .width = 18,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 173,
    },
    [PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_LEFT] = {
        .bg = 0,
        .tilemapLeft = 10,
        .tilemapTop = 7,
        .width = 6,
        .height = 6,
        .paletteNum = 6,
        .baseBlock = 209,
    },
    [PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_RIGHT] = {
        .bg = 0,
        .tilemapLeft = 22,
        .tilemapTop = 7,
        .width = 5,
        .height = 6,
        .paletteNum = 6,
        .baseBlock = 245,
    },
    [PSS_LABEL_WINDOW_POKEMON_SKILLS_EXP] = {
        .bg = 0,
        .tilemapLeft = 10,
        .tilemapTop = 14,
        .width = 11,
        .height = 4,
        .paletteNum = 6,
        .baseBlock = 275,
    },
    [PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 18,
        .width = 6,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 319,
    },
    [PSS_LABEL_WINDOW_MOVES_POWER_ACC] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 15,
        .width = 9,
        .height = 4,
        .paletteNum = 6,
        .baseBlock = 331,
    },
    [PSS_LABEL_WINDOW_MOVES_APPEAL_JAM] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 15,
        .width = 5,
        .height = 4,
        .paletteNum = 6,
        .baseBlock = 367,
    },
    [PSS_LABEL_WINDOW_PROMPT_RELEARN] = {
        .bg = 0,
        .tilemapLeft = 18,
        .tilemapTop = 2,
        .width = 11,
        .height = 2,
        .paletteNum = 15,
        .baseBlock = 800,
    },
    [PSS_LABEL_WINDOW_PORTRAIT_DEX_NUMBER] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 2,
        .width = 5,
        .height = 2,
        .paletteNum = 7,
        .baseBlock = 403,
    },
    [PSS_LABEL_WINDOW_PORTRAIT_NICKNAME] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 12,
        .width = 9,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 413,
    },
    [PSS_LABEL_WINDOW_PORTRAIT_SPECIES] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 14,
        .width = 9,
        .height = 4,
        .paletteNum = 6,
        .baseBlock = 431,
    },
    [PSS_LABEL_WINDOW_END] = DUMMY_WIN_TEMPLATE
};
static const struct WindowTemplate sPageInfoTemplate[] =
{
    [PSS_DATA_WINDOW_INFO_ORIGINAL_TRAINER] = {
        .bg = 0,
        .tilemapLeft = 11,
        .tilemapTop = 4,
        .width = 11,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 467,
    },
    [PSS_DATA_WINDOW_INFO_ID] = {
        .bg = 0,
        .tilemapLeft = 22,
        .tilemapTop = 4,
        .width = 7,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 489,
    },
    [PSS_DATA_WINDOW_INFO_ABILITY] = {
        .bg = 0,
        .tilemapLeft = 11,
        .tilemapTop = 9,
        .width = 18,
        .height = 4,
        .paletteNum = 6,
        .baseBlock = 503,
    },
    [PSS_DATA_WINDOW_INFO_MEMO] = {
        .bg = 0,
        .tilemapLeft = 11,
        .tilemapTop = 14,
        .width = 18,
        .height = 6,
        .paletteNum = 6,
        .baseBlock = 575,
    },
};
static const struct WindowTemplate sPageSkillsTemplate[] =
{
    [PSS_DATA_WINDOW_SKILLS_HELD_ITEM] = {
        .bg = 0,
        .tilemapLeft = 10,
        .tilemapTop = 4,
        .width = 10,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 467,
    },
    [PSS_DATA_WINDOW_SKILLS_RIBBON_COUNT] = {
        .bg = 0,
        .tilemapLeft = 20,
        .tilemapTop = 4,
        .width = 10,
        .height = 2,
        .paletteNum = 6,
        .baseBlock = 487,
    },
    [PSS_DATA_WINDOW_SKILLS_STATS_LEFT] = {
        .bg = 0,
        .tilemapLeft = 16,
        .tilemapTop = 7,
        .width = 6,
        .height = 6,
        .paletteNum = 6,
        .baseBlock = 507,
    },
    [PSS_DATA_WINDOW_SKILLS_STATS_RIGHT] = {
        .bg = 0,
        .tilemapLeft = 27,
        .tilemapTop = 7,
        .width = 3,
        .height = 6,
        .paletteNum = 6,
        .baseBlock = 543,
    },
    [PSS_DATA_WINDOW_EXP] = {
        .bg = 0,
        .tilemapLeft = 24,
        .tilemapTop = 14,
        .width = 6,
        .height = 4,
        .paletteNum = 6,
        .baseBlock = 561,
    },
};
static const struct WindowTemplate sPageMovesTemplate[] = // This is used for both battle and contest moves
{
    [PSS_DATA_WINDOW_MOVE_NAMES] = {
        .bg = 0,
        .tilemapLeft = 15,
        .tilemapTop = 4,
        .width = 9,
        .height = 10,
        .paletteNum = 6,
        .baseBlock = 467,
    },
    [PSS_DATA_WINDOW_MOVE_PP] = {
        .bg = 0,
        .tilemapLeft = 24,
        .tilemapTop = 4,
        .width = 6,
        .height = 10,
        .paletteNum = 8,
        .baseBlock = 557,
    },
    [PSS_DATA_WINDOW_MOVE_DESCRIPTION] = {
        .bg = 0,
        .tilemapLeft = 10,
        .tilemapTop = 15,
        .width = 20,
        .height = 4,
        .paletteNum = 6,
        .baseBlock = 617,
    },
};
static const u8 sTextColors[][3] =
{
    {0, 1, 2},
    {0, 3, 4},
    {0, 5, 6},
    {0, 7, 8},
    {0, 9, 10},
    {0, 11, 12},
    {0, 13, 14},
    {0, 7, 8},
    {13, 15, 14},
    {0, 1, 2},
    {0, 3, 4},
    {0, 5, 6},
    {0, 7, 8}
};

static const u8 sButtons_Gfx[][4 * TILE_SIZE_4BPP] = {
    INCGFX_U8("graphics/hyper_summary_screen/a_button.png", ".4bpp"),
    INCGFX_U8("graphics/hyper_summary_screen/b_button.png", ".4bpp"),
};

static void (*const sTextPrinterFunctions[])(void) =
{
    [PSS_PAGE_INFO] = PrintInfoPageText,
    [PSS_PAGE_SKILLS] = PrintSkillsPageText,
    [PSS_PAGE_BATTLE_MOVES] = PrintBattleMoves,
    [PSS_PAGE_CONTEST_MOVES] = PrintContestMoves
};

static const TaskFunc sTextPrinterTasks[] =
{
    [PSS_PAGE_INFO] = Task_PrintInfoPage,
    [PSS_PAGE_SKILLS] = Task_PrintSkillsPage,
    [PSS_PAGE_BATTLE_MOVES] = Task_PrintBattleMoves,
    [PSS_PAGE_CONTEST_MOVES] = Task_PrintContestMoves
};

static const u8 sText_Relearn[] = _("{START_BUTTON} RELEARN"); // future note: don't decap this, because it mimics the summary screen BG graphics which will not get decapped

static const u8 *const sRelearnTexts[MOVE_RELEARNER_COUNT] =
{
    [MOVE_RELEARNER_LEVEL_UP_MOVES] = COMPOUND_STRING("{START_BUTTON} RELEARN LEVEL"),
    [MOVE_RELEARNER_EGG_MOVES] =      COMPOUND_STRING("{START_BUTTON} RELEARN EGG"),
    [MOVE_RELEARNER_TM_MOVES] =       COMPOUND_STRING("{START_BUTTON} RELEARN TM"),
    [MOVE_RELEARNER_TUTOR_MOVES] =    COMPOUND_STRING("{START_BUTTON} RELEARN TUTOR"),
};

static const u8 sMemoNatureTextColor[] = _("{COLOR LIGHT_RED}{SHADOW GREEN}");
static const u8 sMemoMiscTextColor[] = _("{COLOR WHITE}{SHADOW DARK_GRAY}"); // This is also affected by palettes, apparently
static const u8 sStatsLeftColumnLayout[] = _("{DYNAMIC 0}/{DYNAMIC 1}\n{DYNAMIC 2}\n{DYNAMIC 3}");
static const u8 sStatsLeftIVEVColumnLayout[] = _("{DYNAMIC 0}\n{DYNAMIC 1}\n{DYNAMIC 2}");
static const u8 sStatsRightColumnLayout[] = _("{DYNAMIC 0}\n{DYNAMIC 1}\n{DYNAMIC 2}");
static const u8 sMovesPPLayout[] = _("{PP}{DYNAMIC 0}/{DYNAMIC 1}");

#define TAG_MOVE_SELECTOR 30000
#define TAG_MON_STATUS 30001
#define TAG_MOVE_TYPES 30002
#define TAG_MON_MARKINGS 30003
#define TAG_CATEGORY_ICONS 30004
//HYDRA skills page R / A button icons (sprites)
#define TAG_HYDRA_R_BUTTON 30005
#define TAG_HYDRA_BUTTON_PAL 30005 // palette tags are a separate namespace from tile tags

//HYDRA The R icon sits immediately left of the A icon in the top-right prompt
// window, which starts at tile (22,0) = pixel (176,0). The A icon is blitted at
// window-relative iconX, so R centres 16px to its left.
// On the Stats page the A icon is hidden (nothing there is editable), so R slides
// into its slot rather than leaving a gap. The 4 rather than 0 is a small nudge
// left so the spacing to the text matches the IVs and EVs pages by eye -- the
// prompt text there is a different width, so an exact 0 sat slightly too close.
#define HYDRA_BTN_R_CENTRE_X(iconX, aShown) (176 + (iconX) - ((aShown) ? 16 : 4) + 8)
#define HYDRA_BTN_R_CENTRE_Y 7

static const struct OamData sOamData_CategoryIcons =
{
    .size = SPRITE_SIZE(16x16),
    .shape = SPRITE_SHAPE(16x16),
    .priority = 0,
};

const struct CompressedSpriteSheet gHyperSpriteSheet_CategoryIcons =
{
    .data = gCategoryIcons_Gfx,
    .size = 16*16*3/2,
    .tag = TAG_CATEGORY_ICONS,
};

const struct SpritePalette gHyperSpritePal_CategoryIcons =
{
    .data = gCategoryIcons_Pal,
    .tag = TAG_CATEGORY_ICONS
};

static const union AnimCmd sSpriteAnim_CategoryIcon0[] =
{
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_CategoryIcon1[] =
{
    ANIMCMD_FRAME(4, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_CategoryIcon2[] =
{
    ANIMCMD_FRAME(8, 0),
    ANIMCMD_END
};

static const union AnimCmd *const sSpriteAnimTable_CategoryIcons[] =
{
    sSpriteAnim_CategoryIcon0,
    sSpriteAnim_CategoryIcon1,
    sSpriteAnim_CategoryIcon2,
};

const struct SpriteTemplate gHyperSpriteTemplate_CategoryIcons =
{
    .tileTag = TAG_CATEGORY_ICONS,
    .paletteTag = TAG_CATEGORY_ICONS,
    .oam = &sOamData_CategoryIcons,
    .anims = sSpriteAnimTable_CategoryIcons,
};

static const struct OamData sOamData_MoveTypes =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x16),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x16),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};
static const union AnimCmd sSpriteAnim_TypeNone[] = {
    ANIMCMD_FRAME(TYPE_NONE * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeNormal[] = {
    ANIMCMD_FRAME(TYPE_NORMAL * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeFighting[] = {
    ANIMCMD_FRAME(TYPE_FIGHTING * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeFlying[] = {
    ANIMCMD_FRAME(TYPE_FLYING * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypePoison[] = {
    ANIMCMD_FRAME(TYPE_POISON * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeGround[] = {
    ANIMCMD_FRAME(TYPE_GROUND * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeRock[] = {
    ANIMCMD_FRAME(TYPE_ROCK * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeBug[] = {
    ANIMCMD_FRAME(TYPE_BUG * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeGhost[] = {
    ANIMCMD_FRAME(TYPE_GHOST * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeSteel[] = {
    ANIMCMD_FRAME(TYPE_STEEL * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeMystery[] = {
    ANIMCMD_FRAME(TYPE_MYSTERY * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeFire[] = {
    ANIMCMD_FRAME(TYPE_FIRE * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeWater[] = {
    ANIMCMD_FRAME(TYPE_WATER * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeGrass[] = {
    ANIMCMD_FRAME(TYPE_GRASS * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeElectric[] = {
    ANIMCMD_FRAME(TYPE_ELECTRIC * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypePsychic[] = {
    ANIMCMD_FRAME(TYPE_PSYCHIC * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeIce[] = {
    ANIMCMD_FRAME(TYPE_ICE * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeDragon[] = {
    ANIMCMD_FRAME(TYPE_DRAGON * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeDark[] = {
    ANIMCMD_FRAME(TYPE_DARK * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeFairy[] = {
    ANIMCMD_FRAME(TYPE_FAIRY * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_TypeStellar[] = {
    ANIMCMD_FRAME(TYPE_STELLAR * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_CategoryCool[] = {
    ANIMCMD_FRAME((CONTEST_CATEGORY_COOL + NUMBER_OF_MON_TYPES) * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_CategoryBeauty[] = {
    ANIMCMD_FRAME((CONTEST_CATEGORY_BEAUTY + NUMBER_OF_MON_TYPES) * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_CategoryCute[] = {
    ANIMCMD_FRAME((CONTEST_CATEGORY_CUTE + NUMBER_OF_MON_TYPES) * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_CategorySmart[] = {
    ANIMCMD_FRAME((CONTEST_CATEGORY_SMART + NUMBER_OF_MON_TYPES) * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_CategoryTough[] = {
    ANIMCMD_FRAME((CONTEST_CATEGORY_TOUGH + NUMBER_OF_MON_TYPES) * 8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd *const sSpriteAnimTable_MoveTypes[NUMBER_OF_MON_TYPES + CONTEST_CATEGORIES_COUNT] = {
    [TYPE_NONE] = sSpriteAnim_TypeNone,
    [TYPE_NORMAL] = sSpriteAnim_TypeNormal,
    [TYPE_FIGHTING] = sSpriteAnim_TypeFighting,
    [TYPE_FLYING] = sSpriteAnim_TypeFlying,
    [TYPE_POISON] = sSpriteAnim_TypePoison,
    [TYPE_GROUND] = sSpriteAnim_TypeGround,
    [TYPE_ROCK] = sSpriteAnim_TypeRock,
    [TYPE_BUG] = sSpriteAnim_TypeBug,
    [TYPE_GHOST] = sSpriteAnim_TypeGhost,
    [TYPE_STEEL] = sSpriteAnim_TypeSteel,
    [TYPE_MYSTERY] = sSpriteAnim_TypeMystery,
    [TYPE_FIRE] = sSpriteAnim_TypeFire,
    [TYPE_WATER] = sSpriteAnim_TypeWater,
    [TYPE_GRASS] = sSpriteAnim_TypeGrass,
    [TYPE_ELECTRIC] = sSpriteAnim_TypeElectric,
    [TYPE_PSYCHIC] = sSpriteAnim_TypePsychic,
    [TYPE_ICE] = sSpriteAnim_TypeIce,
    [TYPE_DRAGON] = sSpriteAnim_TypeDragon,
    [TYPE_DARK] = sSpriteAnim_TypeDark,
    [TYPE_FAIRY] = sSpriteAnim_TypeFairy,
    [TYPE_STELLAR] = sSpriteAnim_TypeStellar,
    [NUMBER_OF_MON_TYPES + CONTEST_CATEGORY_COOL] = sSpriteAnim_CategoryCool,
    [NUMBER_OF_MON_TYPES + CONTEST_CATEGORY_BEAUTY] = sSpriteAnim_CategoryBeauty,
    [NUMBER_OF_MON_TYPES + CONTEST_CATEGORY_CUTE] = sSpriteAnim_CategoryCute,
    [NUMBER_OF_MON_TYPES + CONTEST_CATEGORY_SMART] = sSpriteAnim_CategorySmart,
    [NUMBER_OF_MON_TYPES + CONTEST_CATEGORY_TOUGH] = sSpriteAnim_CategoryTough,
};

const struct CompressedSpriteSheet gHyperSpriteSheet_MoveTypes =
{
    .data = gMoveTypes_Gfx,
    .size = (NUMBER_OF_MON_TYPES + CONTEST_CATEGORIES_COUNT) * 0x100,
    .tag = TAG_MOVE_TYPES
};
const struct SpriteTemplate gHyperSpriteTemplate_MoveTypes =
{
    .tileTag = TAG_MOVE_TYPES,
    .paletteTag = TAG_MOVE_TYPES,
    .oam = &sOamData_MoveTypes,
    .anims = sSpriteAnimTable_MoveTypes,
};

static const struct OamData sOamData_MoveSelector =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(16x16),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};
static const union AnimCmd sSpriteAnim_MoveSelector0[] = {
    ANIMCMD_FRAME(0, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelector1[] = {
    ANIMCMD_FRAME(4, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelector2[] = {
    ANIMCMD_FRAME(8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelector3[] = {
    ANIMCMD_FRAME(12, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelectorLeft[] = {
    ANIMCMD_FRAME(16, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelectorRight[] = {
    ANIMCMD_FRAME(16, 0, TRUE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelectorMiddle[] = {
    ANIMCMD_FRAME(20, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelector7[] = {
    ANIMCMD_FRAME(24, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelector8[] = {
    ANIMCMD_FRAME(24, 0, TRUE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_MoveSelector9[] = {
    ANIMCMD_FRAME(28, 0, FALSE, FALSE),
    ANIMCMD_END
};
// All except left, middle and right are unused
static const union AnimCmd *const sSpriteAnimTable_MoveSelector[] = {
    sSpriteAnim_MoveSelector0,
    sSpriteAnim_MoveSelector1,
    sSpriteAnim_MoveSelector2,
    sSpriteAnim_MoveSelector3,
    sSpriteAnim_MoveSelectorLeft,
    sSpriteAnim_MoveSelectorRight,
    sSpriteAnim_MoveSelectorMiddle,
    sSpriteAnim_MoveSelector7,
    sSpriteAnim_MoveSelector8,
    sSpriteAnim_MoveSelector9,
};
static const struct CompressedSpriteSheet sMoveSelectorSpriteSheet =
{
    .data = gSummaryMoveSelect_Gfx,
    .size = 0x400,
    .tag = TAG_MOVE_SELECTOR
};
static const struct SpritePalette sMoveSelectorSpritePal =
{
    .data = gSummaryMoveSelect_Pal,
    .tag = TAG_MOVE_SELECTOR
};
static const struct SpriteTemplate sMoveSelectorSpriteTemplate =
{
    .tileTag = TAG_MOVE_SELECTOR,
    .paletteTag = TAG_MOVE_SELECTOR,
    .oam = &sOamData_MoveSelector,
    .anims = sSpriteAnimTable_MoveSelector,
};
static const struct OamData sOamData_StatusCondition =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x8),
    .tileNum = 0,
    .priority = 3,
    .paletteNum = 0,
    .affineParam = 0,
};
static const union AnimCmd sSpriteAnim_StatusPoison[] = {
    ANIMCMD_FRAME(0, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusParalyzed[] = {
    ANIMCMD_FRAME(4, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusSleep[] = {
    ANIMCMD_FRAME(8, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusFrozen[] = {
    ANIMCMD_FRAME(12, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusBurn[] = {
    ANIMCMD_FRAME(16, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusPokerus[] = {
    ANIMCMD_FRAME(20, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusFaint[] = {
    ANIMCMD_FRAME(24, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_StatusFrostbite[] = {
    ANIMCMD_FRAME(28, 0, FALSE, FALSE),
    ANIMCMD_END
};
static const union AnimCmd *const sSpriteAnimTable_StatusCondition[] = {
    sSpriteAnim_StatusPoison,
    sSpriteAnim_StatusParalyzed,
    sSpriteAnim_StatusSleep,
    sSpriteAnim_StatusFrozen,
    sSpriteAnim_StatusBurn,
    sSpriteAnim_StatusPokerus,
    sSpriteAnim_StatusFaint,
    sSpriteAnim_StatusFrostbite,
};
static const struct CompressedSpriteSheet sStatusIconsSpriteSheet =
{
    .data = gStatusGfx_Icons,
    .size = 0x400,
    .tag = TAG_MON_STATUS
};
static const struct SpritePalette sStatusIconsSpritePalette =
{
    .data = gStatusPal_Icons,
    .tag = TAG_MON_STATUS
};
static const struct SpriteTemplate sSpriteTemplate_StatusCondition =
{
    .tileTag = TAG_MON_STATUS,
    .paletteTag = TAG_MON_STATUS,
    .oam = &sOamData_StatusCondition,
    .anims = sSpriteAnimTable_StatusCondition,
};
static const u16 sMarkings_Pal[] = INCGFX_U16("graphics/hyper_summary_screen/markings.pal", ".gbapal");

//HYDRA Shared OBJ palette for the skills page R and A button sprites. The R art
// uses indices 1-4 and the A art uses 13-15, so both fit one 16-colour palette.
// A's colours are copied from summary_screen tiles.png palette 7 so the sprite
// matches the A icon that the window blit used to draw.
static const u16 sHydraButtons_Pal[] = INCGFX_U16("graphics/hyper_summary_screen/buttons.pal", ".gbapal");

static const u8 sHydraRButton_Gfx[] = INCGFX_U8("graphics/hyper_summary_screen/r_button.png", ".4bpp");

static const struct SpriteSheet sHydraRButtonSpriteSheet = {
    .data = sHydraRButton_Gfx,
    .size = 4 * TILE_SIZE_4BPP, // 16x16 = 4 tiles
    .tag = TAG_HYDRA_R_BUTTON,
};

static const struct SpritePalette sHydraButtonSpritePal = {
    .data = sHydraButtons_Pal,
    .tag = TAG_HYDRA_BUTTON_PAL,
};

static const struct OamData sOamData_HydraButton = {
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(16x16),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
};

static const union AnimCmd sSpriteAnim_HydraButton[] = {
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END
};

static const union AnimCmd *const sSpriteAnimTable_HydraButton[] = {
    sSpriteAnim_HydraButton,
};

static const struct SpriteTemplate sSpriteTemplate_HydraRButton = {
    .tileTag = TAG_HYDRA_R_BUTTON,
    .paletteTag = TAG_HYDRA_BUTTON_PAL,
    .oam = &sOamData_HydraButton,
    .anims = sSpriteAnimTable_HydraButton,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// code
static u8 ShowCategoryIcon(enum DamageCategory category)
{
    if (sMonSummaryScreen->categoryIconSpriteId == 0xFF)
        sMonSummaryScreen->categoryIconSpriteId = CreateSprite(&gHyperSpriteTemplate_CategoryIcons, 48, 128, 0);

    gSprites[sMonSummaryScreen->categoryIconSpriteId].invisible = FALSE;
    StartSpriteAnim(&gSprites[sMonSummaryScreen->categoryIconSpriteId], category);
    return sMonSummaryScreen->categoryIconSpriteId;
}

static void DestroyCategoryIcon(void)
{
    if (sMonSummaryScreen->categoryIconSpriteId != 0xFF)
        DestroySprite(&gSprites[sMonSummaryScreen->categoryIconSpriteId]);
    sMonSummaryScreen->categoryIconSpriteId = 0xFF;
}

u32 Hyper_GetAdjustedIvData(struct Pokemon *mon, u32 stat)
{
    if (P_SUMMARY_SCREEN_IV_HYPERTRAIN && GetMonData(mon, MON_DATA_HYPER_TRAINED_HP + stat))
        return MAX_PER_STAT_IVS;
    return GetMonData(mon, MON_DATA_HP_IV + stat);
}

void ShowHyperSummaryScreen(u8 mode, void *mons, u8 monIndex, u8 maxMonIndex, void (*callback)(void))
{
    sMonSummaryScreen = AllocZeroed(sizeof(*sMonSummaryScreen));
    sMonSummaryScreen->mode = mode;
    if (monIndex == PC_MON_CHOSEN)
    {
        sMonSummaryScreen->monList.boxMons = GetBoxedMonPtr(gSpecialVar_MonBoxId, 0);
        sMonSummaryScreen->curMonIndex = gSpecialVar_MonBoxPos;
        sMonSummaryScreen->maxMonIndex = IN_BOX_COUNT - 1;
    }
    else
    {
        sMonSummaryScreen->monList.mons = mons;
        sMonSummaryScreen->curMonIndex = monIndex;
        sMonSummaryScreen->maxMonIndex = maxMonIndex;
    }
    sMonSummaryScreen->callback = callback;
    if (gInitialHyperSummaryScreenCallback == NULL)
        gInitialHyperSummaryScreenCallback = callback;

    if (mode == SUMMARY_MODE_BOX || monIndex == PC_MON_CHOSEN)
        sMonSummaryScreen->isBoxMon = TRUE;
    else
        sMonSummaryScreen->isBoxMon = FALSE;

    switch (mode)
    {
    case SUMMARY_MODE_NORMAL:
    case SUMMARY_MODE_BOX:
    case SUMMARY_MODE_BOX_CURSOR:
    case SUMMARY_MODE_RELEARNER_BATTLE:
    case SUMMARY_MODE_RELEARNER_CONTEST:
        sMonSummaryScreen->minPageIndex = 0;
        sMonSummaryScreen->maxPageIndex = PSS_PAGE_COUNT - 1;
        break;
    case SUMMARY_MODE_LOCK_MOVES:
        sMonSummaryScreen->minPageIndex = 0;
        sMonSummaryScreen->maxPageIndex = PSS_PAGE_COUNT - 1;
        sMonSummaryScreen->lockMovesFlag = TRUE;
        break;
    case SUMMARY_MODE_SELECT_MOVE:
        sMonSummaryScreen->minPageIndex = PSS_PAGE_BATTLE_MOVES;
        sMonSummaryScreen->maxPageIndex = PSS_PAGE_COUNT - 1;
        sMonSummaryScreen->lockMonFlag = TRUE;
        break;
    }

    if (mode == SUMMARY_MODE_RELEARNER_BATTLE)
        sMonSummaryScreen->currPageIndex = PSS_PAGE_BATTLE_MOVES;
    else if (mode == SUMMARY_MODE_RELEARNER_CONTEST)
        sMonSummaryScreen->currPageIndex = PSS_PAGE_CONTEST_MOVES;
    else
        sMonSummaryScreen->currPageIndex = sMonSummaryScreen->minPageIndex;

    sMonSummaryScreen->categoryIconSpriteId = 0xFF;
    HyperSummaryScreen_SetAnimDelayTaskId(TASK_NONE);

    if (gMonSpritesGfxPtr == NULL)
        CreateMonSpritesGfxManager(MON_SPR_GFX_MANAGER_A, MON_SPR_GFX_MODE_NORMAL);

    if (mode != SUMMARY_MODE_SELECT_MOVE && mode != SUMMARY_MODE_RELEARNER_BATTLE && mode != SUMMARY_MODE_RELEARNER_CONTEST)
        gMoveRelearnerState = MOVE_RELEARNER_LEVEL_UP_MOVES;

    SetMainCallback2(CB2_InitSummaryScreen);
}

void ShowSelectMoveHyperSummaryScreen(struct Pokemon *mons, u8 monIndex, void (*callback)(void), u16 newMove)
{
    ShowHyperSummaryScreen(SUMMARY_MODE_SELECT_MOVE, mons, monIndex, gPartiesCount[B_TRAINER_PLAYER] - 1, callback);
    sMonSummaryScreen->newMove = newMove;
}

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void VBlank(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_InitSummaryScreen(void)
{
    while (MenuHelpers_ShouldWaitForLinkRecv() != TRUE && LoadGraphics() != TRUE && MenuHelpers_IsLinkActive() != TRUE);
}

static bool8 LoadGraphics(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankHBlankCallbacksToNull();
        ResetVramOamAndBgCntRegs();
        ClearScheduledBgCopiesToVram();
        gMain.state++;
        break;
    case 1:
        ScanlineEffect_Stop();
        gMain.state++;
        break;
    case 2:
        ResetPaletteFade();
        gPaletteFade.bufferTransferDisabled = 1;
        gMain.state++;
        break;
    case 3:
        ResetSpriteData();
        gMain.state++;
        break;
    case 4:
        FreeAllSpritePalettes();
        gMain.state++;
        break;
    case 5:
        InitBGs();
        sMonSummaryScreen->switchCounter = 0;
        gMain.state++;
        break;
    case 6:
        if (DecompressGraphics() != FALSE)
            gMain.state++;
        break;
    case 7:
        ResetWindows();
        gMain.state++;
        break;
    case 8:
        DrawPagination();
        gMain.state++;
        break;
    case 9:
        CopyMonToSummaryStruct(&sMonSummaryScreen->currentMon);
        sMonSummaryScreen->switchCounter = 0;
        gMain.state++;
        break;
    case 10:
        if (ExtractMonDataToSummaryStruct(&sMonSummaryScreen->currentMon) != 0)
            gMain.state++;
        break;
    case 11:
        PrintMonInfo();
        gMain.state++;
        break;
    case 12:
        PrintPageNamesAndStats();
        gMain.state++;
        break;
    case 13:
        PrintPageSpecificText(sMonSummaryScreen->currPageIndex);
        gMain.state++;
        break;
    case 14:
        SetDefaultTilemaps();
        gMain.state++;
        break;
    case 15:
        UpdateMoveRelearnerState(FALSE);
        PutPageWindowTilemaps(sMonSummaryScreen->currPageIndex);
        gMain.state++;
        break;
    case 16:
        ResetSpriteIds();
        CreateMoveTypeIcons();
        sMonSummaryScreen->switchCounter = 0;
        gMain.state++;
        break;
    case 17:
        sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON] = LoadMonGfxAndSprite(&sMonSummaryScreen->currentMon, &sMonSummaryScreen->switchCounter);
        if (sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON] != SPRITE_NONE)
        {
            sMonSummaryScreen->switchCounter = 0;
            gMain.state++;
        }
        break;
    case 18:
        CreateMonMarkingsSprite(&sMonSummaryScreen->currentMon);
        gMain.state++;
        break;
    case 19:
        CreateCaughtBallSprite(&sMonSummaryScreen->currentMon);
        gMain.state++;
        break;
    case 20:
        CreateSetStatusSprite();
        gMain.state++;
        break;
    case 21:
        SetTypeIcons();
        gMain.state++;
        break;
    case 22:
        if (sMonSummaryScreen->mode != SUMMARY_MODE_SELECT_MOVE)
            CreateTask(Task_HandleInput, 0);
        else
            CreateTask(Task_SetHandleReplaceMoveInput, 0);
        gMain.state++;
        break;
    case 23:
        BlendPalettes(PALETTES_ALL, 16, 0);
        gMain.state++;
        break;
    case 24:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        gPaletteFade.bufferTransferDisabled = 0;
        gMain.state++;
        break;
    default:
        SetVBlankCallback(VBlank);
        SetMainCallback2(MainCB2);
        return TRUE;
    }
    return FALSE;
}

static void InitBGs(void)
{
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
    SetBgTilemapBuffer(1, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_BATTLE_MOVES][0]);
    SetBgTilemapBuffer(2, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_SKILLS][0]);
    SetBgTilemapBuffer(3, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_INFO][0]);
    ResetAllBgsCoordinates();
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    ScheduleBgCopyTilemapToVram(3);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    ShowBg(0);
    ShowBg(1);
    ShowBg(2);
    ShowBg(3);
}

static bool8 DecompressGraphics(void)
{
    switch (sMonSummaryScreen->switchCounter)
    {
    case 0:
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(1, &gSummaryScreen_Gfx, 0, 0, 0);
        sMonSummaryScreen->switchCounter++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != 1)
        {
            DecompressDataWithHeaderWram(gSummaryPage_Info_Tilemap, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_INFO][0]);
            sMonSummaryScreen->switchCounter++;
        }
        break;
    case 2:
        DecompressDataWithHeaderWram(gSummaryPage_InfoEgg_Tilemap, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_INFO][1]);
        sMonSummaryScreen->switchCounter++;
        break;
    case 3:
        DecompressDataWithHeaderWram(gSummaryPage_Skills_Tilemap, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_SKILLS][1]);
        sMonSummaryScreen->switchCounter++;
        break;
    case 4:
        DecompressDataWithHeaderWram(gSummaryPage_BattleMoves_Tilemap, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_BATTLE_MOVES][1]);
        sMonSummaryScreen->switchCounter++;
        break;
    case 5:
        DecompressDataWithHeaderWram(gSummaryPage_ContestMoves_Tilemap, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_CONTEST_MOVES][1]);
        sMonSummaryScreen->switchCounter++;
        break;
    case 6:
        LoadPalette(gSummaryScreen_Pal, BG_PLTT_ID(0), 8 * PLTT_SIZE_4BPP);
        LoadPalette(&gPPTextPalette, BG_PLTT_ID(8) + 1, PLTT_SIZEOF(16 - 1));
        //HYDRA skills page R/A button icons (sprites)
        LoadSpriteSheet(&sHydraRButtonSpriteSheet);
        LoadSpritePalette(&sHydraButtonSpritePal);
        sMonSummaryScreen->switchCounter++;
        break;
    case 7:
        LoadCompressedSpriteSheet(&gHyperSpriteSheet_MoveTypes);
        sMonSummaryScreen->switchCounter++;
        break;
    case 8:
        LoadCompressedSpriteSheet(&sMoveSelectorSpriteSheet);
        sMonSummaryScreen->switchCounter++;
        break;
    case 9:
        LoadCompressedSpriteSheet(&sStatusIconsSpriteSheet);
        sMonSummaryScreen->switchCounter++;
        break;
    case 10:
        LoadSpritePalette(&sStatusIconsSpritePalette);
        sMonSummaryScreen->switchCounter++;
        break;
    case 11:
        LoadSpritePalette(&sMoveSelectorSpritePal);
        sMonSummaryScreen->switchCounter++;
        break;
    case 12:
        LoadPalette(gMoveTypes_Pal, OBJ_PLTT_ID(13), 3 * PLTT_SIZE_4BPP);
        LoadCompressedSpriteSheet(&gHyperSpriteSheet_CategoryIcons);
        LoadSpritePalette(&gHyperSpritePal_CategoryIcons);
        sMonSummaryScreen->switchCounter = 0;
        return TRUE;
    }
    return FALSE;
}

static struct BoxPokemon *GetCurrentBoxmon(void)
{
    if (sMonSummaryScreen->isBoxMon)
        return &sMonSummaryScreen->monList.boxMons[sMonSummaryScreen->curMonIndex];
    return &sMonSummaryScreen->monList.mons[sMonSummaryScreen->curMonIndex].box;
}

static void CopyMonToSummaryStruct(struct Pokemon *mon)
{
    if (!sMonSummaryScreen->isBoxMon)
    {
        struct Pokemon *partyMon = sMonSummaryScreen->monList.mons;
        *mon = partyMon[sMonSummaryScreen->curMonIndex];
    }
    else
    {
        struct BoxPokemon *boxMon = sMonSummaryScreen->monList.boxMons;
        BoxMonToMon(&boxMon[sMonSummaryScreen->curMonIndex], mon);
    }
}

static bool8 ExtractMonDataToSummaryStruct(struct Pokemon *mon)
{
    u32 i;
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    // Spread the data extraction over multiple frames.
    switch (sMonSummaryScreen->switchCounter)
    {
    case 0:
        sum->species = GetMonData(mon, MON_DATA_SPECIES);
        sum->species2 = GetMonData(mon, MON_DATA_SPECIES_OR_EGG);
        sum->exp = GetMonData(mon, MON_DATA_EXP);
        sum->level = GetMonData(mon, MON_DATA_LEVEL);
        sum->abilityNum = GetMonData(mon, MON_DATA_ABILITY_NUM);
        sum->item = GetMonData(mon, MON_DATA_HELD_ITEM);
        sum->pid = GetMonData(mon, MON_DATA_PERSONALITY);
        sum->sanity = GetMonData(mon, MON_DATA_SANITY_IS_BAD_EGG);

        if (sum->sanity)
            sum->isEgg = TRUE;
        else
            sum->isEgg = GetMonData(mon, MON_DATA_IS_EGG);

        break;
    case 1:
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            sum->moves[i] = GetMonData(mon, MON_DATA_MOVE1+i);
            sum->pp[i] = GetMonData(mon, MON_DATA_PP1+i);
        }
        sum->ppBonuses = GetMonData(mon, MON_DATA_PP_BONUSES);
        break;
    case 2:
        Hyper_ExtractMonSkillStatsData(mon, sum);
        break;
    case 3:
        GetMonData(mon, MON_DATA_OT_NAME, sum->OTName);
        ConvertInternationalString(sum->OTName, GetMonData(mon, MON_DATA_LANGUAGE));
        sum->ailment = GetMonAilment(mon);
        sum->OTGender = GetMonData(mon, MON_DATA_OT_GENDER);
        sum->OTID = GetMonData(mon, MON_DATA_OT_ID);
        sum->metLocation = GetMonData(mon, MON_DATA_MET_LOCATION);
        sum->metLevel = GetMonData(mon, MON_DATA_MET_LEVEL);
        sum->metGame = GetMonData(mon, MON_DATA_MET_GAME);
        sum->friendship = GetMonData(mon, MON_DATA_FRIENDSHIP);
        break;
    default:
        sum->ribbonCount = GetMonData(mon, MON_DATA_RIBBON_COUNT);
        sum->teraType = GetMonData(mon, MON_DATA_TERA_TYPE);
        sum->isShiny = GetMonData(mon, MON_DATA_IS_SHINY);
        return TRUE;
    }
    sMonSummaryScreen->switchCounter++;
    return FALSE;
}

static void SetDefaultTilemaps(void)
{
    if ((sMonSummaryScreen->currPageIndex != PSS_PAGE_BATTLE_MOVES && sMonSummaryScreen->currPageIndex != PSS_PAGE_CONTEST_MOVES)
        || sMonSummaryScreen->mode == SUMMARY_MODE_RELEARNER_BATTLE
        || sMonSummaryScreen->mode == SUMMARY_MODE_RELEARNER_CONTEST)
    {
        PositionPowerAccSlidingWindow(0, 0xFF);
        PositionAppealJamSlidingWindow(0, 0xFF, 0);
    }
    else
    {
        DrawContestMoveHearts(sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex]);
        TilemapFiveMovesDisplay(sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_BATTLE_MOVES][0], 3, FALSE);
        TilemapFiveMovesDisplay(sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_CONTEST_MOVES][0], 1, FALSE);
        SetBgTilemapBuffer(1, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_CONTEST_MOVES][0]);
        SetBgTilemapBuffer(2, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_BATTLE_MOVES][0]);
        ChangeBgX(2, 0x10000, BG_COORD_ADD);
        ClearWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_SPECIES);
        ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS);
    }

    // these blocks handle preparing the gfx to return straight to the respective move info screens
    if (sMonSummaryScreen->mode == SUMMARY_MODE_RELEARNER_BATTLE)
    {
        SetBgTilemapBuffer(1, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_SKILLS][0]);
        SetBgTilemapBuffer(2, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_BATTLE_MOVES][0]);
        SetBgAttribute(1, BG_ATTR_PRIORITY, 2);
        SetBgAttribute(2, BG_ATTR_PRIORITY, 1);
        ChangeBgX(1, 0x10000, BG_COORD_ADD);
        ChangeBgX(2, 0x10000, BG_COORD_ADD);
        ShowBg(1);
        ShowBg(2);
    }
    else if (sMonSummaryScreen->mode == SUMMARY_MODE_RELEARNER_CONTEST)
    {
        sMonSummaryScreen->bgDisplayOrder = 1;
        SetBgTilemapBuffer(1, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_CONTEST_MOVES][0]);
        SetBgTilemapBuffer(2, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_BATTLE_MOVES][0]);
        SetBgAttribute(1, BG_ATTR_PRIORITY, 1);
        SetBgAttribute(2, BG_ATTR_PRIORITY, 2);
        ChangeBgX(1, 0x10000, BG_COORD_ADD);
        ChangeBgX(2, 0x10000, BG_COORD_ADD);
        ShowBg(1);
        ShowBg(2);
    }

    if (sMonSummaryScreen->summary.ailment == AILMENT_NONE)
        PositionStatusSlidingWindow(0, 0xFF);
    else if ((sMonSummaryScreen->currPageIndex != PSS_PAGE_BATTLE_MOVES && sMonSummaryScreen->currPageIndex != PSS_PAGE_CONTEST_MOVES)
            || sMonSummaryScreen->mode == SUMMARY_MODE_RELEARNER_BATTLE
            || sMonSummaryScreen->mode == SUMMARY_MODE_RELEARNER_CONTEST)
        PutWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS);

    LimitEggSummaryPageDisplay();
    DrawPokerusCuredSymbol(&sMonSummaryScreen->currentMon);
}

static void FreeSummaryScreen(void)
{
    FreeAllWindowBuffers();
    Free(sMonSummaryScreen);
}

static void BeginCloseSummaryScreen(u8 taskId)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = CloseSummaryScreen;
}

static void CloseSummaryScreen(u8 taskId)
{
    if (MenuHelpers_ShouldWaitForLinkRecv() != TRUE && !gPaletteFade.active)
    {
        if (sMonSummaryScreen->callback == gInitialHyperSummaryScreenCallback)
            gInitialHyperSummaryScreenCallback = NULL;
        SetMainCallback2(sMonSummaryScreen->callback);
        gLastViewedHyperMonIndex = sMonSummaryScreen->curMonIndex;
        SummaryScreen_DestroyAnimDelayTask();
        ResetSpriteData();
        FreeAllSpritePalettes();
        StopCryAndClearCrySongs();
        m4aMPlayVolumeControl(&gMPlayInfo_BGM, TRACKS_ALL, 0x100);
        if (gMonSpritesGfxPtr == NULL)
            DestroyMonSpritesGfxManager(MON_SPR_GFX_MANAGER_A);
        FreeSummaryScreen();
        DestroyTask(taskId);
    }
}

// Update skills page tilemap
static void ChangeStatLabel(s16 mode)
{
    if (!P_SUMMARY_SCREEN_IV_EV_TILESET)
        return;

    u32 statsBlock = 169;
    u32 ivsBlock = 221;
    u32 evsBlock = 218;

    u32 statsCoordX = 44;
    u32 statsCoordY = 102;

    u32 statsLength = 3;
    u32 ivEvLength = 2;

    ClearStatLabel(statsLength, statsCoordX, statsCoordY);

    switch (mode)
    {
    case SUMMARY_SKILLS_MODE_STATS:
        WriteToStatsTilemapBuffer(statsLength, statsBlock, statsCoordX, statsCoordY);
        break;
    case SUMMARY_SKILLS_MODE_IVS:
        WriteToStatsTilemapBuffer(ivEvLength, ivsBlock, statsCoordX, statsCoordY);
        break;
    case SUMMARY_SKILLS_MODE_EVS:
        WriteToStatsTilemapBuffer(ivEvLength, evsBlock, statsCoordX, statsCoordY);
        break;
    }
    CopyBgTilemapBufferToVram(1);
}

static void WriteToStatsTilemapBuffer(u32 length, u32 block, u32 statsCoordX, u32 statsCoordY)
{
    u32 i;

    for (i = 0; i <= length; i++)
        FillBgTilemapBufferRect(1, block + i, statsCoordX + i, statsCoordY, 1, 1, 2);
}

static void ClearStatLabel(u32 length, u32 statsCoordX, u32 statsCoordY)
{
    u32 blankStatsBlock = 1241;

    u32 i;
    u32 blankOffset = 3;

    for (i = 0; i <= length; i++)
        FillBgTilemapBufferRect(1, blankStatsBlock, statsCoordX + blankOffset + i, statsCoordY, 1, 1, 2);
}

static void HandleMoveRelearnerInput(u8 taskId)
{
    if (JOY_NEW(START_BUTTON))
    {
        sMonSummaryScreen->callback = CB2_InitLearnMove;
        gRelearnMode = sMonSummaryScreen->currPageIndex;
        gSpecialVar_MonBoxPos = sMonSummaryScreen->curMonIndex;
        if (sMonSummaryScreen->isBoxMon)
        {
            gSpecialVar_0x8004 = PC_MON_CHOSEN;
            gSpecialVar_MonBoxPos = sMonSummaryScreen->curMonIndex;
        }
        else
        {
            gSpecialVar_0x8004 = sMonSummaryScreen->curMonIndex;
        }
        StopPokemonAnimations();
        PlaySE(SE_SELECT);
        BeginCloseSummaryScreen(taskId);
    }
    else if (JOY_NEW(R_BUTTON)) // R means increase. Level -> Egg -> TM -> Tutor
    {
        gMoveRelearnerState++;
        UpdateMoveRelearnerState(FALSE);
        PlaySE(SE_SELECT);
    }
    else if (JOY_NEW(L_BUTTON)) // L means decrease. Level <- Egg <- TM <- Tutor
    {
        gMoveRelearnerState--;
        UpdateMoveRelearnerState(TRUE);
        PlaySE(SE_SELECT);
    }
}

static void Task_HandleInput(u8 taskId)
{
    if (MenuHelpers_ShouldWaitForLinkRecv() != TRUE && !gPaletteFade.active)
    {
        if (JOY_NEW(DPAD_UP))
        {
            ChangeSummaryPokemon(taskId, -1);
        }
        else if (JOY_NEW(DPAD_DOWN))
        {
            ChangeSummaryPokemon(taskId, 1);
        }
        else if (JOY_NEW(DPAD_LEFT))
        {
            ChangePage(taskId, -1);
        }
        else if (JOY_NEW(DPAD_RIGHT))
        {
            ChangePage(taskId, 1);
        }
        else if (JOY_NEW(A_BUTTON))
        {
            if (sMonSummaryScreen->currPageIndex != PSS_PAGE_SKILLS)
            {
                if (sMonSummaryScreen->currPageIndex == PSS_PAGE_INFO)
                {
                    //HYDRA A opens the ability/nature edit cursor, but only once at
                    // least one of them is unlocked (nature 3rd badge, ability 8th).
                    if (HyperPC_CanEditNature() || HyperPC_CanEditAbility())
                    {
                        PlaySE(SE_SELECT);
                        SwitchToInfoSelection(taskId);
                    }
                }
                else if (IS_MOVE_PAGE(sMonSummaryScreen->currPageIndex))
                {
                    PlaySE(SE_SELECT);
                    SwitchToMoveSelection(taskId);
                }
            }
            //HYDRA A on the skills page enters stat-selection mode (red cursor).
            // Cycling Stats/IVs/EVs moved to the R button below.
            // Only IVs and EVs are editable, and each needs its badge (EVs 5th,
            // IVs 7th). Stats are always read-only.
            if (sMonSummaryScreen->currPageIndex == PSS_PAGE_SKILLS
             && HyperPC_CanEditSkillsMode(sMonSummaryScreen->skillsPageMode))
            {
                PlaySE(SE_SELECT);
                SwitchToStatSelection(taskId);
            }
        }
        //HYDRA cycle Stats -> IVs -> EVs with R instead of A. Guarded to the skills
        // page so R on the move pages still falls through to the move relearner.
        else if (JOY_NEW(R_BUTTON) && sMonSummaryScreen->currPageIndex == PSS_PAGE_SKILLS)
        {
            if (ShouldShowIvEvPrompt())
            {
                ShowMonSkillsInfo(taskId, IncrementSkillsStatsMode(sMonSummaryScreen->skillsPageMode));
                PlaySE(SE_SELECT);
            }
        }
        else if (JOY_NEW(B_BUTTON))
        {
            StopPokemonAnimations();
            PlaySE(SE_SELECT);
            BeginCloseSummaryScreen(taskId);
        }
        else if (DEBUG_POKEMON_SPRITE_VISUALIZER && JOY_NEW(SELECT_BUTTON) && !gMain.inBattle)
        {
            sMonSummaryScreen->callback = CB2_Pokemon_Sprite_Visualizer;
            StopPokemonAnimations();
            PlaySE(SE_SELECT);
            CloseSummaryScreen(taskId);
        }
        else if (ShouldShowMoveRelearner() && IS_MOVE_PAGE(sMonSummaryScreen->currPageIndex))
        {
            HandleMoveRelearnerInput(taskId);
        }
    }
}

static u8 IncrementSkillsStatsMode(u8 mode)
{
    switch (mode)
    {
    case SUMMARY_SKILLS_MODE_STATS:
        if (P_SUMMARY_SCREEN_EV_ONLY)
        {
            sMonSummaryScreen->skillsPageMode = SUMMARY_SKILLS_MODE_EVS;
            return SUMMARY_SKILLS_MODE_EVS;
        }
        else
        {
            sMonSummaryScreen->skillsPageMode = SUMMARY_SKILLS_MODE_IVS;
            return SUMMARY_SKILLS_MODE_IVS;
        }

    case SUMMARY_SKILLS_MODE_IVS:
        if (P_SUMMARY_SCREEN_IV_ONLY)
        {
            sMonSummaryScreen->skillsPageMode = SUMMARY_SKILLS_MODE_STATS;
            return SUMMARY_SKILLS_MODE_STATS;
        }
        else
        {
            sMonSummaryScreen->skillsPageMode = SUMMARY_SKILLS_MODE_EVS;
            return SUMMARY_SKILLS_MODE_EVS;
        }
    case SUMMARY_SKILLS_MODE_EVS:
    default:
        sMonSummaryScreen->skillsPageMode = SUMMARY_SKILLS_MODE_STATS;
        return SUMMARY_SKILLS_MODE_STATS;
    }

}

static void ShowMonSkillsInfo(u8 taskId, s16 mode)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    struct Pokemon *mon = &sMonSummaryScreen->currentMon;

    FillWindowPixelBuffer(sMonSummaryScreen->windowIds[PSS_DATA_WINDOW_SKILLS_STATS_LEFT], 0);
    FillWindowPixelBuffer(sMonSummaryScreen->windowIds[PSS_DATA_WINDOW_SKILLS_STATS_RIGHT], 0);

    if (sMonSummaryScreen->currPageIndex == PSS_PAGE_SKILLS)
    {
        ChangeStatLabel(mode);
        ShowUtilityPrompt(mode);
        DrawSkillsRButtonIcon(); //HYDRA
    }

    if (mode == SUMMARY_SKILLS_MODE_STATS)
    {
        Hyper_ExtractMonSkillStatsData(mon, sum);
        BufferLeftColumnStats();
    }
    else if (mode == SUMMARY_SKILLS_MODE_IVS)
    {
        Hyper_ExtractMonSkillIvData(mon, sum);
        BufferLeftColumnIvEvStats();
    }
    else if (mode == SUMMARY_SKILLS_MODE_EVS)
    {
        Hyper_ExtractMonSkillEvData(mon, sum);
        BufferLeftColumnIvEvStats();
    }

    PrintLeftColumnStats();
    BufferRightColumnStats();
    PrintRightColumnStats();
    gTasks[taskId].func = Task_HandleInput;
}

void Hyper_ExtractMonSkillStatsData(struct Pokemon *mon, struct PokeSummary *sum)
{
    sum->nature = GetNature(mon);
    sum->mintNature = GetMonData(mon, MON_DATA_HIDDEN_NATURE);
    sum->currentHP = GetMonData(mon, MON_DATA_HP);
    sum->maxHP = GetMonData(mon, MON_DATA_MAX_HP);
    sum->atk = GetMonData(mon, MON_DATA_ATK);
    sum->def = GetMonData(mon, MON_DATA_DEF);
    sum->spatk = GetMonData(mon, MON_DATA_SPATK);
    sum->spdef = GetMonData(mon, MON_DATA_SPDEF);
    sum->speed = GetMonData(mon, MON_DATA_SPEED);
}

void Hyper_ExtractMonSkillIvData(struct Pokemon *mon, struct PokeSummary *sum)
{
    sum->currentHP = Hyper_GetAdjustedIvData(mon, STAT_HP);
    sum->atk = Hyper_GetAdjustedIvData(mon, STAT_ATK);
    sum->def =  Hyper_GetAdjustedIvData(mon, STAT_DEF);
    sum->spatk = Hyper_GetAdjustedIvData(mon, STAT_SPATK);
    sum->spdef = Hyper_GetAdjustedIvData(mon, STAT_SPDEF);
    sum->speed = Hyper_GetAdjustedIvData(mon, STAT_SPEED);
}

void Hyper_ExtractMonSkillEvData(struct Pokemon *mon, struct PokeSummary *sum)
{
    sum->currentHP = GetMonData(mon, MON_DATA_HP_EV);
    sum->atk = GetMonData(mon, MON_DATA_ATK_EV);
    sum->def = GetMonData(mon, MON_DATA_DEF_EV);
    sum->spatk = GetMonData(mon, MON_DATA_SPATK_EV);
    sum->spdef = GetMonData(mon, MON_DATA_SPDEF_EV);
    sum->speed = GetMonData(mon, MON_DATA_SPEED_EV);
}

bool32 Hyper_HasAnyRelearnableMoves(enum MoveRelearnerStates state)
{
    return CanBoxMonRelearnMoves(GetCurrentBoxmon(), state);
}

static void UpdateMoveRelearnerState(bool32 goingDown)
{
    s32 state;

    sMonSummaryScreen->hasRelearnableMoves = FALSE;
    for (u32 i = 0; i < MOVE_RELEARNER_COUNT; i++)
    {
        state = (gMoveRelearnerState + i * (goingDown ? -1 : 1)) % MOVE_RELEARNER_COUNT;
        if (Hyper_HasAnyRelearnableMoves(state))
        {
            sMonSummaryScreen->hasRelearnableMoves = TRUE;
            gMoveRelearnerState = state;
            break;
        }
    }
    UpdateRelearnPrompt();
}

static void ChangeSummaryPokemon(u8 taskId, s8 delta)
{
    s8 monId;

    if (!sMonSummaryScreen->lockMonFlag)
    {
        if (sMonSummaryScreen->isBoxMon == TRUE)
        {
            if (sMonSummaryScreen->currPageIndex != PSS_PAGE_INFO)
            {
                if (delta == 1)
                    delta = 0;
                else
                    delta = 2;
            }
            else
            {
                if (delta == 1)
                    delta = 1;
                else
                    delta = 3;
            }
            monId = AdvanceStorageMonIndex(sMonSummaryScreen->monList.boxMons, sMonSummaryScreen->curMonIndex, sMonSummaryScreen->maxMonIndex, delta);
        }
        else if (IsMultiBattle() == TRUE && !AreMultiPartiesFullTeams())
        {
            monId = AdvanceMultiBattleMonIndex(delta);
        }
        else
        {
            monId = AdvanceMonIndex(delta);
        }

        if (monId != -1)
        {
            PlaySE(SE_SELECT);
            if (sMonSummaryScreen->summary.ailment != AILMENT_NONE)
            {
                SetSpriteInvisibility(SPRITE_ARR_ID_STATUS, TRUE);
                ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS);
                ScheduleBgCopyTilemapToVram(0);
                PositionStatusSlidingWindow(0, 2);
            }
            sMonSummaryScreen->curMonIndex = monId;
            gTasks[taskId].data[0] = 0;
            gTasks[taskId].func = Task_ChangeSummaryMon;
        }
    }
}

static void Task_ChangeSummaryMon(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 0:
        StopCryAndClearCrySongs();
        break;
    case 1:
        SummaryScreen_DestroyAnimDelayTask();
        DestroySpriteAndFreeResources(&gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]]);
        break;
    case 2:
        DestroySpriteAndFreeResources(&gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_BALL]]);
        break;
    case 3:
        CopyMonToSummaryStruct(&sMonSummaryScreen->currentMon);
        sMonSummaryScreen->switchCounter = 0;
        break;
    case 4:
        if (ExtractMonDataToSummaryStruct(&sMonSummaryScreen->currentMon) == FALSE)
            return;

        if (P_SUMMARY_SCREEN_RENAME && sMonSummaryScreen->currPageIndex == PSS_PAGE_INFO)
            ShowUtilityPrompt(SUMMARY_MODE_NORMAL);

        if (ShouldShowIvEvPrompt() && sMonSummaryScreen->currPageIndex == PSS_PAGE_SKILLS)
        {
            sMonSummaryScreen->skillsPageMode = SUMMARY_SKILLS_MODE_STATS;
            ChangeStatLabel(SUMMARY_SKILLS_MODE_STATS);
        }

        if (P_SUMMARY_SCREEN_MOVE_RELEARNER && IS_MOVE_PAGE(sMonSummaryScreen->currPageIndex))
        {
            gMoveRelearnerState = MOVE_RELEARNER_LEVEL_UP_MOVES;
            UpdateMoveRelearnerState(FALSE);
        }
        break;
    case 5:
        RemoveAndCreateMonMarkingsSprite(&sMonSummaryScreen->currentMon);
        break;
    case 6:
        CreateCaughtBallSprite(&sMonSummaryScreen->currentMon);
        break;
    case 7:
        if (sMonSummaryScreen->summary.ailment != AILMENT_NONE)
            PositionStatusSlidingWindow(10, -2);
        DrawPokerusCuredSymbol(&sMonSummaryScreen->currentMon);
        data[1] = 0;
        break;
    case 8:
        sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON] = LoadMonGfxAndSprite(&sMonSummaryScreen->currentMon, &data[1]);
        if (sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON] == SPRITE_NONE)
            return;
        gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].data[2] = 1;
        TryDrawExperienceProgressBar();
        data[1] = 0;
        break;
    case 9:
        SetTypeIcons();
        break;
    case 10:
        PrintMonInfo();
        break;
    case 11:
        PrintPageSpecificText(sMonSummaryScreen->currPageIndex);
        LimitEggSummaryPageDisplay();
        break;
    case 12:
        gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].data[2] = 0;
        break;
    default:
        if (!MenuHelpers_ShouldWaitForLinkRecv() && !FuncIsActiveTask(Task_SlideStatusWindow))
        {
            data[0] = 0;
            gTasks[taskId].func = Task_HandleInput;
        }
        return;
    }
    data[0]++;
}

static s8 AdvanceMonIndex(s8 delta)
{
    struct Pokemon *mon = sMonSummaryScreen->monList.mons;

    if (sMonSummaryScreen->currPageIndex == PSS_PAGE_INFO)
    {
        if (delta == -1 && sMonSummaryScreen->curMonIndex == 0)
            return -1;
        else if (delta == 1 && sMonSummaryScreen->curMonIndex >= sMonSummaryScreen->maxMonIndex)
            return -1;
        else
            return sMonSummaryScreen->curMonIndex + delta;
    }
    else
    {
        s8 index = sMonSummaryScreen->curMonIndex;

        do
        {
            index += delta;
            if (index < 0 || index > sMonSummaryScreen->maxMonIndex)
                return -1;
        } while (GetMonData(&mon[index], MON_DATA_IS_EGG));
        return index;
    }
}

static s8 AdvanceMultiBattleMonIndex(s8 delta)
{
    struct Pokemon *mons = sMonSummaryScreen->monList.mons;
    s8 index, arrId = 0;
    u8 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (sMultiBattleOrder[i] == sMonSummaryScreen->curMonIndex)
        {
            arrId = i;
            break;
        }
    }

    while (TRUE)
    {
        const s8 *order = sMultiBattleOrder;

        arrId += delta;
        if (arrId < 0 || arrId >= PARTY_SIZE)
            return -1;
        index = order[arrId];
        if (IsValidToViewInMulti(&mons[index]) == TRUE)
            return index;
    }
}

static bool8 IsValidToViewInMulti(struct Pokemon *mon)
{
    if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE)
        return FALSE;
    else if (sMonSummaryScreen->curMonIndex != 0 || !GetMonData(mon, MON_DATA_IS_EGG))
        return TRUE;
    else
        return FALSE;
}

static void ChangePage(u8 taskId, s8 delta)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    s16 *data = gTasks[taskId].data;
    u32 currPageIndex;

    if (summary->isEgg)
        return;
    else if (delta == -1 && sMonSummaryScreen->currPageIndex == sMonSummaryScreen->minPageIndex)
        return;
    else if (delta == 1 && sMonSummaryScreen->currPageIndex == sMonSummaryScreen->maxPageIndex)
        return;

    PlaySE(SE_SELECT);
    ClearPageWindowTilemaps(sMonSummaryScreen->currPageIndex);
    currPageIndex = sMonSummaryScreen->currPageIndex += delta;
    data[0] = 0;
    if (delta == 1)
        SetTaskFuncWithFollowupFunc(taskId, PssScrollRight, gTasks[taskId].func);
    else
        SetTaskFuncWithFollowupFunc(taskId, PssScrollLeft, gTasks[taskId].func);
    CreateTextPrinterTask(currPageIndex);
    HidePageSpecificSprites();

    if (currPageIndex == PSS_PAGE_SKILLS
        || (currPageIndex + delta) == PSS_PAGE_SKILLS)
    {
        struct Pokemon *mon = &sMonSummaryScreen->currentMon;

        if (sMonSummaryScreen->skillsPageMode != SUMMARY_SKILLS_MODE_STATS)
            sMonSummaryScreen->skillsPageMode = SUMMARY_SKILLS_MODE_STATS;

        ShowUtilityPrompt(sMonSummaryScreen->skillsPageMode);
        Hyper_ExtractMonSkillStatsData(mon, summary);
        BufferLeftColumnStats();
        BufferRightColumnStats();
    }
    else
    {
        ShowUtilityPrompt(SUMMARY_MODE_NORMAL);
    }
}

static void PssScrollRight(u8 taskId) // Scroll right
{
    s16 *data = gTasks[taskId].data;
    if (data[0] == 0)
    {
        if (sMonSummaryScreen->bgDisplayOrder == 0)
        {
            data[1] = 1;
            SetBgAttribute(1, BG_ATTR_PRIORITY, 1);
            SetBgAttribute(2, BG_ATTR_PRIORITY, 2);
            ScheduleBgCopyTilemapToVram(1);
        }
        else
        {
            data[1] = 2;
            SetBgAttribute(2, BG_ATTR_PRIORITY, 1);
            SetBgAttribute(1, BG_ATTR_PRIORITY, 2);
            ScheduleBgCopyTilemapToVram(2);
        }
        ChangeBgX(data[1], 0, BG_COORD_SET);
        SetBgTilemapBuffer(data[1], sMonSummaryScreen->bgTilemapBuffers[sMonSummaryScreen->currPageIndex][0]);
        ShowBg(1);
        ShowBg(2);
    }
    ChangeBgX(data[1], 0x2000, BG_COORD_ADD);
    data[0] += 32;
    if (data[0] > 0xFF)
        gTasks[taskId].func = PssScrollRightEnd;
}

static void PssScrollRightEnd(u8 taskId) // display right
{
    s16 *data = gTasks[taskId].data;
    sMonSummaryScreen->bgDisplayOrder ^= 1;
    data[1] = 0;
    data[0] = 0;
    DrawPagination();
    PutPageWindowTilemaps(sMonSummaryScreen->currPageIndex);
    SetTypeIcons();
    TryDrawExperienceProgressBar();
    SwitchTaskToFollowupFunc(taskId);
}

static void PssScrollLeft(u8 taskId) // Scroll left
{
    s16 *data = gTasks[taskId].data;
    // to fix a specific lag in writing to the stat label
    if (sMonSummaryScreen->currPageIndex == PSS_PAGE_SKILLS)
        ChangeStatLabel(SUMMARY_SKILLS_MODE_STATS);
    if (data[0] == 0)
    {
        if (sMonSummaryScreen->bgDisplayOrder == 0)
            data[1] = 2;
        else
            data[1] = 1;
        ChangeBgX(data[1], 0x10000, BG_COORD_SET);
    }
    ChangeBgX(data[1], 0x2000, BG_COORD_SUB);
    data[0] += 32;
    if (data[0] > 0xFF)
        gTasks[taskId].func = PssScrollLeftEnd;
}

static void PssScrollLeftEnd(u8 taskId) // display left
{
    s16 *data = gTasks[taskId].data;
    if (sMonSummaryScreen->bgDisplayOrder == 0)
    {
        SetBgAttribute(1, BG_ATTR_PRIORITY, 1);
        SetBgAttribute(2, BG_ATTR_PRIORITY, 2);
        ScheduleBgCopyTilemapToVram(2);
    }
    else
    {
        SetBgAttribute(2, BG_ATTR_PRIORITY, 1);
        SetBgAttribute(1, BG_ATTR_PRIORITY, 2);
        ScheduleBgCopyTilemapToVram(1);
    }
    if (sMonSummaryScreen->currPageIndex > 1)
    {
        SetBgTilemapBuffer(data[1], sMonSummaryScreen->bgTilemapBuffers[sMonSummaryScreen->currPageIndex - 1][0]);
        ChangeBgX(data[1], 0x10000, BG_COORD_SET);
    }
    ShowBg(1);
    ShowBg(2);
    sMonSummaryScreen->bgDisplayOrder ^= 1;
    data[1] = 0;
    data[0] = 0;
    DrawPagination();
    PutPageWindowTilemaps(sMonSummaryScreen->currPageIndex);
    SetTypeIcons();
    TryDrawExperienceProgressBar();
    SwitchTaskToFollowupFunc(taskId);
}

static void TryDrawExperienceProgressBar(void)
{
    if (sMonSummaryScreen->currPageIndex == PSS_PAGE_SKILLS)
        DrawExperienceProgressBar(&sMonSummaryScreen->currentMon);
}

static void SwitchToMoveSelection(u8 taskId)
{
    enum Move move;

    sMonSummaryScreen->firstMoveIndex = 0;
    move = sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex];
    ClearWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_SPECIES);
    if (!gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_STATUS]].invisible)
        ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS);
    PositionPowerAccSlidingWindow(9, -3);
    PositionAppealJamSlidingWindow(9, -3, move);
    if (!sMonSummaryScreen->lockMovesFlag)
    {
        if (ShouldShowMoveRelearner())
            ClearWindowTilemap(PSS_LABEL_WINDOW_PROMPT_RELEARN);

        ShowUtilityPrompt(SUMMARY_MODE_SELECT_MOVE);
    }
    else
    {
        ShowUtilityPrompt(SUMMARY_MODE_NORMAL);
    }

    TilemapFiveMovesDisplay(sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_BATTLE_MOVES][0], 3, FALSE);
    TilemapFiveMovesDisplay(sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_CONTEST_MOVES][0], 1, FALSE);
    PrintMoveDetails(move);
    PrintNewMoveDetailsOrCancelText();
    SetNewMoveTypeIcon();
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    CreateMoveSelectorSprites(SPRITE_ARR_ID_MOVE_SELECTOR1);
    gTasks[taskId].func = Task_HandleInput_MoveSelect;
}

static void Task_HandleInput_MoveSelect(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (MenuHelpers_ShouldWaitForLinkRecv() != TRUE)
    {
        if (JOY_NEW(DPAD_UP))
        {
            data[0] = 4;
            ChangeSelectedMove(data, -1, &sMonSummaryScreen->firstMoveIndex);
        }
        else if (JOY_NEW(DPAD_DOWN))
        {
            data[0] = 4;
            ChangeSelectedMove(data, 1, &sMonSummaryScreen->firstMoveIndex);
        }
        else if (JOY_NEW(A_BUTTON))
        {
            if (sMonSummaryScreen->lockMovesFlag == TRUE
                || (sMonSummaryScreen->newMove == MOVE_NONE && sMonSummaryScreen->firstMoveIndex == MAX_MON_MOVES))
            {
                PlaySE(SE_SELECT);
                ShowUtilityPrompt(SUMMARY_MODE_NORMAL);
                CloseMoveSelectMode(taskId);
            }
            else if (HasMoreThanOneMove() == TRUE)
            {
                PlaySE(SE_SELECT);
                ShowUtilityPrompt(SUMMARY_MODE_SELECT_MOVE);
                SwitchToMovePositionSwitchMode(taskId);
            }
            else
            {
                PlaySE(SE_FAILURE);
            }
        }
        else if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            CloseMoveSelectMode(taskId);
        }
    }
}

static bool8 HasMoreThanOneMove(void)
{
    u8 i;
    for (i = 1; i < MAX_MON_MOVES; i++)
    {
        if (sMonSummaryScreen->summary.moves[i] != 0)
            return TRUE;
    }
    return FALSE;
}

static void ChangeSelectedMove(s16 *taskData, s8 direction, u8 *moveIndexPtr)
{
    s8 i, newMoveIndex;
    enum Move move;

    PlaySE(SE_SELECT);
    newMoveIndex = *moveIndexPtr;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        newMoveIndex += direction;
        if (newMoveIndex > taskData[0])
            newMoveIndex = 0;
        else if (newMoveIndex < 0)
            newMoveIndex = taskData[0];

        if (newMoveIndex == MAX_MON_MOVES)
        {
            move = sMonSummaryScreen->newMove;
            break;
        }
        move = sMonSummaryScreen->summary.moves[newMoveIndex];
        if (move != 0)
            break;
    }
    DrawContestMoveHearts(move);
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    PrintMoveDetails(move);
    if ((*moveIndexPtr == MAX_MON_MOVES && sMonSummaryScreen->newMove == MOVE_NONE)
        || taskData[1] == 1)
    {
        ClearWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_SPECIES);
        if (!gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_STATUS]].invisible)
            ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS);
        ScheduleBgCopyTilemapToVram(0);
        PositionPowerAccSlidingWindow(9, -3);
        PositionAppealJamSlidingWindow(9, -3, move);
    }
    if (*moveIndexPtr != MAX_MON_MOVES
        && newMoveIndex == MAX_MON_MOVES
        && sMonSummaryScreen->newMove == MOVE_NONE)
    {
        ClearWindowTilemap(PSS_LABEL_WINDOW_MOVES_POWER_ACC);
        ClearWindowTilemap(PSS_LABEL_WINDOW_MOVES_APPEAL_JAM);
        DestroyCategoryIcon();
        ScheduleBgCopyTilemapToVram(0);
        PositionPowerAccSlidingWindow(0, 3);
        PositionAppealJamSlidingWindow(0, 3, 0);
    }

    *moveIndexPtr = newMoveIndex;
    // Get rid of the 'flicker' effect(while idle) when scrolling.
    if (moveIndexPtr == &sMonSummaryScreen->firstMoveIndex)
        KeepMoveSelectorVisible(SPRITE_ARR_ID_MOVE_SELECTOR1);
    else
        KeepMoveSelectorVisible(SPRITE_ARR_ID_MOVE_SELECTOR2);
}

static void CloseMoveSelectMode(u8 taskId)
{
    DestroyMoveSelectorSprites(SPRITE_ARR_ID_MOVE_SELECTOR1);
    if (ShouldShowMoveRelearner())
        PutWindowTilemap(PSS_LABEL_WINDOW_PROMPT_RELEARN);

    ShowUtilityPrompt(SUMMARY_MODE_NORMAL);
    PrintMoveDetails(0);
    TilemapFiveMovesDisplay(sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_BATTLE_MOVES][0], 3, TRUE);
    TilemapFiveMovesDisplay(sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_CONTEST_MOVES][0], 1, TRUE);
    AddAndFillMoveNamesWindow(); // This function seems to have no effect.
    if (sMonSummaryScreen->firstMoveIndex != MAX_MON_MOVES)
    {
        ClearWindowTilemap(PSS_LABEL_WINDOW_MOVES_POWER_ACC);
        ClearWindowTilemap(PSS_LABEL_WINDOW_MOVES_APPEAL_JAM);
        DestroyCategoryIcon();
        PositionPowerAccSlidingWindow(0, 3);
        PositionAppealJamSlidingWindow(0, 3, 0);
    }
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    gTasks[taskId].func = Task_HandleInput;
}

static void SwitchToMovePositionSwitchMode(u8 taskId)
{
    sMonSummaryScreen->secondMoveIndex = sMonSummaryScreen->firstMoveIndex;
    SetMainMoveSelectorColor(1);
    CreateMoveSelectorSprites(SPRITE_ARR_ID_MOVE_SELECTOR2);
    gTasks[taskId].func = Task_HandleInput_MovePositionSwitch;
}

static void Task_HandleInput_MovePositionSwitch(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (MenuHelpers_ShouldWaitForLinkRecv() != TRUE)
    {
        if (JOY_NEW(DPAD_UP))
        {
            data[0] = 3;
            ChangeSelectedMove(&data[0], -1, &sMonSummaryScreen->secondMoveIndex);
        }
        else if (JOY_NEW(DPAD_DOWN))
        {
            data[0] = 3;
            ChangeSelectedMove(&data[0], 1, &sMonSummaryScreen->secondMoveIndex);
        }
        else if (JOY_NEW(A_BUTTON))
        {
            if (sMonSummaryScreen->firstMoveIndex == sMonSummaryScreen->secondMoveIndex)
                ExitMovePositionSwitchMode(taskId, FALSE);
            else
                ExitMovePositionSwitchMode(taskId, TRUE);
        }
        else if (JOY_NEW(B_BUTTON))
        {
            ExitMovePositionSwitchMode(taskId, FALSE);
        }
    }
}

static void ExitMovePositionSwitchMode(u8 taskId, bool8 swapMoves)
{
    enum Move move;

    PlaySE(SE_SELECT);
    SetMainMoveSelectorColor(0);
    DestroyMoveSelectorSprites(SPRITE_ARR_ID_MOVE_SELECTOR2);

    if (swapMoves == TRUE)
    {
        if (!sMonSummaryScreen->isBoxMon)
        {
            struct Pokemon *mon = sMonSummaryScreen->monList.mons;
            SwapMonMoves(&mon[sMonSummaryScreen->curMonIndex], sMonSummaryScreen->firstMoveIndex, sMonSummaryScreen->secondMoveIndex);
        }
        else
        {
            struct BoxPokemon *boxMon = sMonSummaryScreen->monList.boxMons;
            SwapBoxMonMoves(&boxMon[sMonSummaryScreen->curMonIndex], sMonSummaryScreen->firstMoveIndex, sMonSummaryScreen->secondMoveIndex);
        }
        CopyMonToSummaryStruct(&sMonSummaryScreen->currentMon);
        SwapMovesNamesPP(sMonSummaryScreen->firstMoveIndex, sMonSummaryScreen->secondMoveIndex);
        SwapMovesTypeSprites(sMonSummaryScreen->firstMoveIndex, sMonSummaryScreen->secondMoveIndex);
        sMonSummaryScreen->firstMoveIndex = sMonSummaryScreen->secondMoveIndex;
    }

    move = sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex];
    PrintMoveDetails(move);
    DrawContestMoveHearts(move);
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    gTasks[taskId].func = Task_HandleInput_MoveSelect;
}

static void SwapMonMoves(struct Pokemon *mon, u8 moveIndex1, u8 moveIndex2)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;

    enum Move move1 = summary->moves[moveIndex1];
    enum Move move2 = summary->moves[moveIndex2];
    u8 move1pp = summary->pp[moveIndex1];
    u8 move2pp = summary->pp[moveIndex2];
    u8 ppBonuses = summary->ppBonuses;

    // Calculate PP bonuses
    u8 ppUpMask1 = gPPUpGetMask[moveIndex1];
    u8 ppBonusMove1 = (ppBonuses & ppUpMask1) >> (moveIndex1 * 2);
    u8 ppUpMask2 = gPPUpGetMask[moveIndex2];
    u8 ppBonusMove2 = (ppBonuses & ppUpMask2) >> (moveIndex2 * 2);
    ppBonuses &= ~ppUpMask1;
    ppBonuses &= ~ppUpMask2;
    ppBonuses |= (ppBonusMove1 << (moveIndex2 * 2)) + (ppBonusMove2 << (moveIndex1 * 2));

    // Swap the moves
    SetMonData(mon, MON_DATA_MOVE1 + moveIndex1, &move2);
    SetMonData(mon, MON_DATA_MOVE1 + moveIndex2, &move1);
    SetMonData(mon, MON_DATA_PP1 + moveIndex1, &move2pp);
    SetMonData(mon, MON_DATA_PP1 + moveIndex2, &move1pp);
    SetMonData(mon, MON_DATA_PP_BONUSES, &ppBonuses);

    summary->moves[moveIndex1] = move2;
    summary->moves[moveIndex2] = move1;

    summary->pp[moveIndex1] = move2pp;
    summary->pp[moveIndex2] = move1pp;

    summary->ppBonuses = ppBonuses;
}

static void SwapBoxMonMoves(struct BoxPokemon *mon, u8 moveIndex1, u8 moveIndex2)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;

    enum Move move1 = summary->moves[moveIndex1];
    enum Move move2 = summary->moves[moveIndex2];
    u8 move1pp = summary->pp[moveIndex1];
    u8 move2pp = summary->pp[moveIndex2];
    u8 ppBonuses = summary->ppBonuses;

    // Calculate PP bonuses
    u8 ppUpMask1 = gPPUpGetMask[moveIndex1];
    u8 ppBonusMove1 = (ppBonuses & ppUpMask1) >> (moveIndex1 * 2);
    u8 ppUpMask2 = gPPUpGetMask[moveIndex2];
    u8 ppBonusMove2 = (ppBonuses & ppUpMask2) >> (moveIndex2 * 2);
    ppBonuses &= ~ppUpMask1;
    ppBonuses &= ~ppUpMask2;
    ppBonuses |= (ppBonusMove1 << (moveIndex2 * 2)) + (ppBonusMove2 << (moveIndex1 * 2));

    // Swap the moves
    SetBoxMonData(mon, MON_DATA_MOVE1 + moveIndex1, &move2);
    SetBoxMonData(mon, MON_DATA_MOVE1 + moveIndex2, &move1);
    SetBoxMonData(mon, MON_DATA_PP1 + moveIndex1, &move2pp);
    SetBoxMonData(mon, MON_DATA_PP1 + moveIndex2, &move1pp);
    SetBoxMonData(mon, MON_DATA_PP_BONUSES, &ppBonuses);

    summary->moves[moveIndex1] = move2;
    summary->moves[moveIndex2] = move1;

    summary->pp[moveIndex1] = move2pp;
    summary->pp[moveIndex2] = move1pp;

    summary->ppBonuses = ppBonuses;
}

static void Task_SetHandleReplaceMoveInput(u8 taskId)
{
    SetNewMoveTypeIcon();
    CreateMoveSelectorSprites(SPRITE_ARR_ID_MOVE_SELECTOR1);
    gTasks[taskId].func = Task_HandleReplaceMoveInput;
}

static void Task_HandleReplaceMoveInput(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (MenuHelpers_ShouldWaitForLinkRecv() != TRUE)
    {
        if (gPaletteFade.active != TRUE)
        {
            if (JOY_NEW(DPAD_UP))
            {
                data[0] = 4;
                ChangeSelectedMove(data, -1, &sMonSummaryScreen->firstMoveIndex);
            }
            else if (JOY_NEW(DPAD_DOWN))
            {
                data[0] = 4;
                ChangeSelectedMove(data, 1, &sMonSummaryScreen->firstMoveIndex);
            }
            else if (JOY_NEW(DPAD_LEFT) || GetLRKeysPressed() == MENU_L_PRESSED)
            {
                ChangePage(taskId, -1);
            }
            else if (JOY_NEW(DPAD_RIGHT) || GetLRKeysPressed() == MENU_R_PRESSED)
            {
                ChangePage(taskId, 1);
            }
            else if (JOY_NEW(A_BUTTON))
            {
                if (CanReplaceMove() == TRUE)
                {
                    StopPokemonAnimations();
                    PlaySE(SE_SELECT);
                    sMoveSlotToReplace = sMonSummaryScreen->firstMoveIndex;
                    gSpecialVar_0x8005 = sMoveSlotToReplace;
                    gSpecialVar_Result = TRUE;
                    BeginCloseSummaryScreen(taskId);
                }
                else
                {
                    PlaySE(SE_FAILURE);
                    ShowCantForgetHMsWindow(taskId);
                }
            }
            else if (JOY_NEW(B_BUTTON))
            {
                StopPokemonAnimations();
                PlaySE(SE_SELECT);
                sMoveSlotToReplace = MAX_MON_MOVES;
                gSpecialVar_0x8005 = MAX_MON_MOVES;
                gSpecialVar_Result = FALSE;
                BeginCloseSummaryScreen(taskId);
            }
        }
    }
}

static bool8 CanReplaceMove(void)
{
    if (sMonSummaryScreen->firstMoveIndex == MAX_MON_MOVES
        || sMonSummaryScreen->newMove == MOVE_NONE
        || !CannotForgetMove(sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex]))
        return TRUE;
    else
        return FALSE;
}

static void ShowCantForgetHMsWindow(u8 taskId)
{
    ClearWindowTilemap(PSS_LABEL_WINDOW_MOVES_POWER_ACC);
    ClearWindowTilemap(PSS_LABEL_WINDOW_MOVES_APPEAL_JAM);
    gSprites[sMonSummaryScreen->categoryIconSpriteId].invisible = TRUE;
    ScheduleBgCopyTilemapToVram(0);
    PositionPowerAccSlidingWindow(0, 3);
    PositionAppealJamSlidingWindow(0, 3, 0);
    PrintHMMovesCantBeForgotten();
    gTasks[taskId].func = Task_HandleInputCantForgetHMsMoves;
}

// This redraws the power/accuracy window when the player scrolls out of the "HM Moves can't be forgotten" message
static void Task_HandleInputCantForgetHMsMoves(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    enum Move move;
    if (FuncIsActiveTask(Task_SlidePowerAccWindow) != 1)
    {
        if (JOY_NEW(DPAD_UP))
        {
            data[1] = 1;
            data[0] = 4;
            ChangeSelectedMove(&data[0], -1, &sMonSummaryScreen->firstMoveIndex);
            data[1] = 0;
            gTasks[taskId].func = Task_HandleReplaceMoveInput;
        }
        else if (JOY_NEW(DPAD_DOWN))
        {
            data[1] = 1;
            data[0] = 4;
            ChangeSelectedMove(&data[0], 1, &sMonSummaryScreen->firstMoveIndex);
            data[1] = 0;
            gTasks[taskId].func = Task_HandleReplaceMoveInput;
        }
        else if (JOY_NEW(DPAD_LEFT) || GetLRKeysPressed() == MENU_L_PRESSED)
        {
            if (sMonSummaryScreen->currPageIndex != PSS_PAGE_BATTLE_MOVES)
            {
                ClearWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_SPECIES);
                if (!gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_STATUS]].invisible)
                    ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS);
                move = sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex];
                gTasks[taskId].func = Task_HandleReplaceMoveInput;
                ChangePage(taskId, -1);
                PositionPowerAccSlidingWindow(9, -2);
                PositionAppealJamSlidingWindow(9, -2, move);
            }
        }
        else if (JOY_NEW(DPAD_RIGHT) || GetLRKeysPressed() == MENU_R_PRESSED)
        {
            if (sMonSummaryScreen->currPageIndex != PSS_PAGE_CONTEST_MOVES)
            {
                ClearWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_SPECIES);
                if (!gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_STATUS]].invisible)
                    ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS);
                move = sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex];
                gTasks[taskId].func = Task_HandleReplaceMoveInput;
                ChangePage(taskId, 1);
                PositionPowerAccSlidingWindow(9, -2);
                PositionAppealJamSlidingWindow(9, -2, move);
            }
        }
        else if (JOY_NEW(A_BUTTON | B_BUTTON))
        {
            ClearWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_SPECIES);
            if (!gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_STATUS]].invisible)
                ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS);
            move = sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex];
            PrintMoveDetails(move);
            ScheduleBgCopyTilemapToVram(0);
            PositionPowerAccSlidingWindow(9, -3);
            PositionAppealJamSlidingWindow(9, -3, move);
            gTasks[taskId].func = Task_HandleReplaceMoveInput;
        }
    }
}

u8 HyperGetMoveSlotToReplace(void)
{
    return sMoveSlotToReplace;
}

static void DrawPagination(void) // Updates the pagination dots at the top of the summary screen
{
    u16 *tilemap = Alloc(8 * PSS_PAGE_COUNT);
    u8 i;

    for (i = 0; i < PSS_PAGE_COUNT; i++)
    {
        u8 j = i * 2;

        if (i < sMonSummaryScreen->minPageIndex)
        {
            tilemap[j + 0] = 0x40;
            tilemap[j + 1] = 0x40;
            tilemap[j + 2 * PSS_PAGE_COUNT] = 0x50;
            tilemap[j + 2 * PSS_PAGE_COUNT + 1] = 0x50;
        }
        else if (i > sMonSummaryScreen->maxPageIndex)
        {
            tilemap[j + 0] = 0x4A;
            tilemap[j + 1] = 0x4A;
            tilemap[j + 2 * PSS_PAGE_COUNT] = 0x5A;
            tilemap[j + 2 * PSS_PAGE_COUNT + 1] = 0x5A;
        }
        else if (i < sMonSummaryScreen->currPageIndex)
        {
            tilemap[j + 0] = 0x46;
            tilemap[j + 1] = 0x47;
            tilemap[j + 2 * PSS_PAGE_COUNT] = 0x56;
            tilemap[j + 2 * PSS_PAGE_COUNT + 1] = 0x57;
        }
        else if (i == sMonSummaryScreen->currPageIndex)
        {
            if (i != sMonSummaryScreen->maxPageIndex)
            {
                tilemap[j + 0] = 0x41;
                tilemap[j + 1] = 0x42;
                tilemap[j + 2 * PSS_PAGE_COUNT] = 0x51;
                tilemap[j + 2 * PSS_PAGE_COUNT + 1] = 0x52;
            }
            else
            {
                tilemap[j + 0] = 0x4B;
                tilemap[j + 1] = 0x4C;
                tilemap[j + 2 * PSS_PAGE_COUNT] = 0x5B;
                tilemap[j + 2 * PSS_PAGE_COUNT + 1] = 0x5C;
            }
        }
        else if (i != sMonSummaryScreen->maxPageIndex)
        {
            tilemap[j + 0] = 0x43;
            tilemap[j + 1] = 0x44;
            tilemap[j + 2 * PSS_PAGE_COUNT] = 0x53;
            tilemap[j + 2 * PSS_PAGE_COUNT + 1] = 0x54;
        }
        else
        {
            tilemap[j + 0] = 0x48;
            tilemap[j + 1] = 0x49;
            tilemap[j + 2 * PSS_PAGE_COUNT] = 0x58;
            tilemap[j + 2 * PSS_PAGE_COUNT + 1] = 0x59;
        }
    }
    CopyToBgTilemapBufferRect_ChangePalette(3, tilemap, 11, 0, PSS_PAGE_COUNT * 2, 2, 16);
    ScheduleBgCopyTilemapToVram(3);
    Free(tilemap);
}

static void CopyNColumnsToTilemap(const struct SlidingWindow *slidingWindow, u16 *tilemapDest, u8 visibleColumns, bool8 isOpeningToTheLeft)
{
    u16 i;
    u16 *alloced = Alloc(slidingWindow->width * 2 * slidingWindow->height);
    CpuFill16(slidingWindow->defaultTile, alloced, slidingWindow->width * 2 * slidingWindow->height);
    if (slidingWindow->width != visibleColumns)
    {
        if (!isOpeningToTheLeft)
        {
            for (i = 0; i < slidingWindow->height; i++)
                CpuCopy16(&slidingWindow->gfx[visibleColumns + slidingWindow->width * i], &alloced[slidingWindow->width * i], (slidingWindow->width - visibleColumns) * 2);
        }
        else
        {
            for (i = 0; i < slidingWindow->height; i++)
                CpuCopy16(&slidingWindow->gfx[slidingWindow->width * i], &alloced[visibleColumns + slidingWindow->width * i], (slidingWindow->width - visibleColumns) * 2);
        }
    }

    for (i = 0; i < slidingWindow->height; i++)
        CpuCopy16(&alloced[slidingWindow->width * i], &tilemapDest[(slidingWindow->top + i) * 32 + slidingWindow->left], slidingWindow->width * 2);

    Free(alloced);
}

#define tScrollingSpeed data[0]
#define tVisibleColumns data[1]
#define tMove           data[2]

static void PositionPowerAccSlidingWindow(u16 visibleColumns, s16 speed)
{
    if (speed > sPowerAccSlidingWindow.width)
        speed = sPowerAccSlidingWindow.width;
    if (speed == 0 || speed == sPowerAccSlidingWindow.width)
    {
        CopyNColumnsToTilemap(&sPowerAccSlidingWindow, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_BATTLE_MOVES][0], speed, TRUE);
    }
    else
    {
        u8 taskId = FindTaskIdByFunc(Task_SlidePowerAccWindow);
        if (taskId == TASK_NONE)
            taskId = CreateTask(Task_SlidePowerAccWindow, 8);
        gTasks[taskId].tScrollingSpeed = speed;
        gTasks[taskId].tVisibleColumns = visibleColumns;
    }
}

static void Task_SlidePowerAccWindow(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    tVisibleColumns += tScrollingSpeed;
    if (tVisibleColumns < 0)
    {
        tVisibleColumns = 0;
    }
    else if (tVisibleColumns > sPowerAccSlidingWindow.width)
    {
        tVisibleColumns = sPowerAccSlidingWindow.width;
    }
    CopyNColumnsToTilemap(&sPowerAccSlidingWindow, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_BATTLE_MOVES][0], tVisibleColumns, TRUE);
    if (tVisibleColumns <= 0 || tVisibleColumns >= sPowerAccSlidingWindow.width)
    {
        if (tScrollingSpeed < 0)
        {
            if (sMonSummaryScreen->currPageIndex == PSS_PAGE_BATTLE_MOVES)
                PutWindowTilemap(PSS_LABEL_WINDOW_MOVES_POWER_ACC);
        }
        else
        {
            if (!gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_STATUS]].invisible)
                PutWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS);
            PutWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_SPECIES);
        }
        ScheduleBgCopyTilemapToVram(0);
        DestroyTask(taskId);
    }
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
}

static void PositionAppealJamSlidingWindow(u16 visibleColumns, s16 speed, enum Move move)
{
    if (speed > sAppealJamSlidingWindow.width)
        speed = sAppealJamSlidingWindow.width;

    if (speed == 0 || speed == sAppealJamSlidingWindow.width)
    {
        CopyNColumnsToTilemap(&sAppealJamSlidingWindow, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_CONTEST_MOVES][0], speed, TRUE);
    }
    else
    {
        u8 taskId = FindTaskIdByFunc(Task_SlideAppealJamWindow);
        if (taskId == TASK_NONE)
            taskId = CreateTask(Task_SlideAppealJamWindow, 8);
        gTasks[taskId].tScrollingSpeed = speed;
        gTasks[taskId].tVisibleColumns = visibleColumns;
        gTasks[taskId].tMove = move;
    }
}

static void Task_SlideAppealJamWindow(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    tVisibleColumns += tScrollingSpeed;
    if (tVisibleColumns < 0)
    {
        tVisibleColumns = 0;
    }
    else if (tVisibleColumns > sAppealJamSlidingWindow.width)
    {
        tVisibleColumns = sAppealJamSlidingWindow.width;
    }
    CopyNColumnsToTilemap(&sAppealJamSlidingWindow, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_CONTEST_MOVES][0], tVisibleColumns, TRUE);
    if (tVisibleColumns <= 0 || tVisibleColumns >= sAppealJamSlidingWindow.width)
    {
        if (tScrollingSpeed < 0)
        {
            if (sMonSummaryScreen->currPageIndex == PSS_PAGE_CONTEST_MOVES && FuncIsActiveTask(PssScrollRight) == 0)
                PutWindowTilemap(PSS_LABEL_WINDOW_MOVES_APPEAL_JAM);
            DrawContestMoveHearts(tMove);
        }
        else
        {
            if (!gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_STATUS]].invisible)
            {
                PutWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS);
            }
            PutWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_SPECIES);
        }
        ScheduleBgCopyTilemapToVram(0);
        DestroyTask(taskId);
    }
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
}

static void PositionStatusSlidingWindow(u16 visibleColumns, s16 speed)
{
    if (speed > sStatusSlidingWindow1.width)
        speed = sStatusSlidingWindow1.width;
    if (speed == 0 || speed == sStatusSlidingWindow1.width)
    {
        CopyNColumnsToTilemap(&sStatusSlidingWindow1, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_INFO][0], speed, FALSE);
        CopyNColumnsToTilemap(&sStatusSlidingWindow2, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_INFO][0], speed, FALSE);
    }
    else
    {
        u8 taskId = CreateTask(Task_SlideStatusWindow, 8);
        gTasks[taskId].tScrollingSpeed = speed;
        gTasks[taskId].tVisibleColumns = visibleColumns;
    }
}

static void Task_SlideStatusWindow(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    tVisibleColumns += tScrollingSpeed;
    if (tVisibleColumns < 0)
        tVisibleColumns = 0;
    else if (tVisibleColumns > sStatusSlidingWindow1.width)
        tVisibleColumns = sStatusSlidingWindow1.width;
    CopyNColumnsToTilemap(&sStatusSlidingWindow1, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_INFO][0], tVisibleColumns, FALSE);
    CopyNColumnsToTilemap(&sStatusSlidingWindow2, sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_INFO][0], tVisibleColumns, FALSE);
    ScheduleBgCopyTilemapToVram(3);
    if (tVisibleColumns <= 0 || tVisibleColumns >= sStatusSlidingWindow1.width)
    {
        if (tScrollingSpeed < 0)
        {
            CreateSetStatusSprite();
            PutWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS);
            ScheduleBgCopyTilemapToVram(0);
        }
        DestroyTask(taskId);
    }
}

#undef tScrollingSpeed
#undef tVisibleColumns
#undef tMove

// Toggles the "Cancel" window that appears when selecting a move
static void TilemapFiveMovesDisplay(u16 *dst, u16 palette, bool8 remove)
{
    u16 i, id;

    palette *= 0x1000;
    id = 0x56A;
    if (!remove)
    {
        for (i = 0; i < 20; i++)
        {
            dst[id + i] = gSummaryScreen_MoveEffect_Cancel_Tilemap[i] + palette;
            dst[id + i + 0x20] = gSummaryScreen_MoveEffect_Cancel_Tilemap[i] + palette;
            dst[id + i + 0x40] = gSummaryScreen_MoveEffect_Cancel_Tilemap[i + 20] + palette;
        }
    }
    else // Remove
    {
        for (i = 0; i < 20; i++)
        {
            dst[id + i] = gSummaryScreen_MoveEffect_Cancel_Tilemap[i + 20] + palette;
            dst[id + i + 0x20] = gSummaryScreen_MoveEffect_Cancel_Tilemap[i + 40] + palette;
            dst[id + i + 0x40] = gSummaryScreen_MoveEffect_Cancel_Tilemap[i + 40] + palette;
        }
    }
}

static void DrawPokerusCuredSymbol(struct Pokemon *mon) // This checks if the mon has been cured of pokerus
{
    if (ShouldPokemonShowCuredPokerus(mon))
    {
        sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_INFO][0][0x223] = 0x2C;
        sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_INFO][1][0x223] = 0x2C;
    }
    else
    {
        sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_INFO][0][0x223] = 0x81A;
        sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_INFO][1][0x223] = 0x81A;
    }
    ScheduleBgCopyTilemapToVram(3);
}

static void SetMonPicBackgroundPalette(bool8 isMonShiny)
{
    if (!isMonShiny)
        SetBgTilemapPalette(3, 1, 4, 8, 8, 0);
    else
        SetBgTilemapPalette(3, 1, 4, 8, 8, 5);
    ScheduleBgCopyTilemapToVram(3);
}

static void DrawExperienceProgressBar(struct Pokemon *unused)
{
    s64 numExpProgressBarTicks;
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    u16 *dst;
    u8 i;

    if (summary->level < MAX_LEVEL)
    {
        u32 expBetweenLevels = gExperienceTables[gSpeciesInfo[summary->species].growthRate][summary->level + 1] - gExperienceTables[gSpeciesInfo[summary->species].growthRate][summary->level];
        u32 expSinceLastLevel = summary->exp - gExperienceTables[gSpeciesInfo[summary->species].growthRate][summary->level];

        // Calculate the number of 1-pixel "ticks" to illuminate in the experience progress bar.
        // There are 8 tiles that make up the bar, and each tile has 8 "ticks". Hence, the numerator
        // is multiplied by 64.
        numExpProgressBarTicks = expSinceLastLevel * 64 / expBetweenLevels;
        if (numExpProgressBarTicks == 0 && expSinceLastLevel != 0)
            numExpProgressBarTicks = 1;
    }
    else
    {
        numExpProgressBarTicks = 0;
    }

    dst = &sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_SKILLS][1][0x255];
    for (i = 0; i < 8; i++)
    {
        if (numExpProgressBarTicks > 7)
            dst[i] = 0x206A;
        else
            dst[i] = 0x2062 + (numExpProgressBarTicks % 8);
        numExpProgressBarTicks -= 8;
        if (numExpProgressBarTicks < 0)
            numExpProgressBarTicks = 0;
    }

    if (GetBgTilemapBuffer(1) == sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_SKILLS][0])
        ScheduleBgCopyTilemapToVram(1);
    else
        ScheduleBgCopyTilemapToVram(2);
}

static void DrawContestMoveHearts(enum Move move)
{
    u16 *tilemap = sMonSummaryScreen->bgTilemapBuffers[PSS_PAGE_CONTEST_MOVES][1];
    u8 i;

    if (move != MOVE_NONE)
    {
        // Draw appeal hearts
        u8 effectValue = gContestEffects[GetMoveContestEffect(move)].appeal;
        if (effectValue != 0xFF)
            effectValue /= 10;

        for (i = 0; i < MAX_CONTEST_MOVE_HEARTS; i++)
        {
            if (effectValue != 0xFF && i < effectValue)
                tilemap[(i / 4 * 32) + (i & 3) + 0x1E6] = TILE_FILLED_APPEAL_HEART;
            else
                tilemap[(i / 4 * 32) + (i & 3) + 0x1E6] = TILE_EMPTY_APPEAL_HEART;
        }

        // Draw jam hearts
        effectValue = gContestEffects[GetMoveContestEffect(move)].jam;
        if (effectValue != 0xFF)
            effectValue /= 10;

        for (i = 0; i < MAX_CONTEST_MOVE_HEARTS; i++)
        {
            if (effectValue != 0xFF && i < effectValue)
                tilemap[(i / 4 * 32) + (i & 3) + 0x226] = TILE_FILLED_JAM_HEART;
            else
                tilemap[(i / 4 * 32) + (i & 3) + 0x226] = TILE_EMPTY_JAM_HEART;
        }
    }
}

static void LimitEggSummaryPageDisplay(void) // If the Pokémon is an egg, limit the number of pages displayed to 1
{
    if (sMonSummaryScreen->summary.isEgg)
        ChangeBgX(3, 0x10000, BG_COORD_SET);
    else
        ChangeBgX(3, 0, BG_COORD_SET);
}

static void ResetWindows(void)
{
    u8 i;

    InitWindows(sSummaryTemplate);
    DeactivateAllTextPrinters();
    for (i = 0; i < PSS_LABEL_WINDOW_END; i++)
        FillWindowPixelBuffer(i, PIXEL_FILL(0));
    for (i = 0; i < ARRAY_COUNT(sMonSummaryScreen->windowIds); i++)
        sMonSummaryScreen->windowIds[i] = WINDOW_NONE;
}

static void PrintTextOnWindowWithFont(u8 windowId, const u8 *string, u8 x, u8 y, u8 lineSpacing, u8 colorId, u32 fontId)
{
    AddTextPrinterParameterized4(windowId, fontId, x, y, 0, lineSpacing, sTextColors[colorId], 0, string);
}

static void PrintTextOnWindow(u8 windowId, const u8 *string, u8 x, u8 y, u8 lineSpacing, u8 colorId)
{
    PrintTextOnWindowWithFont(windowId, string, x, y, lineSpacing, colorId, FONT_NORMAL);
}

static void PrintTextOnWindowToFitPx(u8 windowId, const u8 *string, u8 x, u8 y, u8 lineSpacing, u8 colorId, u32 width)
{
    u32 fontId = GetFontIdToFit(string, FONT_NORMAL, 0, width);
    PrintTextOnWindowWithFont(windowId, string, x, y, lineSpacing, colorId, fontId);
}

static void PrintTextOnWindowToFit(u8 windowId, const u8 *string, u8 x, u8 y, u8 lineSpacing, u8 colorId)
{
    PrintTextOnWindowToFitPx(windowId, string, x, y, lineSpacing, colorId, WindowWidthPx(windowId));
}

static void PrintMonInfo(void)
{
    FillWindowPixelBuffer(PSS_LABEL_WINDOW_PORTRAIT_DEX_NUMBER, PIXEL_FILL(0));
    FillWindowPixelBuffer(PSS_LABEL_WINDOW_PORTRAIT_NICKNAME, PIXEL_FILL(0));
    FillWindowPixelBuffer(PSS_LABEL_WINDOW_PORTRAIT_SPECIES, PIXEL_FILL(0));
    if (!sMonSummaryScreen->summary.isEgg)
        PrintNotEggInfo();
    else
        PrintEggInfo();
    ScheduleBgCopyTilemapToVram(0);
}

static void PrintNotEggInfo(void)
{
    struct Pokemon *mon = &sMonSummaryScreen->currentMon;
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    u16 dexNum = SpeciesToPokedexNum(summary->species);

    if (dexNum != 0xFFFF)
    {
        u8 digitCount = (NATIONAL_DEX_COUNT > 999 && IsNationalPokedexEnabled()) ? 4 : 3;
        StringCopy(gStringVar1, &gText_NumberClear01[0]);
        ConvertIntToDecimalStringN(gStringVar2, dexNum, STR_CONV_MODE_LEADING_ZEROS, digitCount);
        StringAppend(gStringVar1, gStringVar2);
        if (!IsMonShiny(mon))
        {
            PrintTextOnWindow(PSS_LABEL_WINDOW_PORTRAIT_DEX_NUMBER, gStringVar1, 0, 1, 0, 1);
            SetMonPicBackgroundPalette(FALSE);
        }
        else
        {
            PrintTextOnWindow(PSS_LABEL_WINDOW_PORTRAIT_DEX_NUMBER, gStringVar1, 0, 1, 0, 7);
            SetMonPicBackgroundPalette(TRUE);
        }
        PutWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_DEX_NUMBER);
    }
    else
    {
        ClearWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_DEX_NUMBER);
        if (!IsMonShiny(mon))
            SetMonPicBackgroundPalette(FALSE);
        else
            SetMonPicBackgroundPalette(TRUE);
    }
    StringCopy(gStringVar1, gText_LevelSymbol);
    ConvertIntToDecimalStringN(gStringVar2, summary->level, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringAppend(gStringVar1, gStringVar2);
    PrintTextOnWindow(PSS_LABEL_WINDOW_PORTRAIT_SPECIES, gStringVar1, 24, 17, 0, 1);
    GetMonNickname(mon, gStringVar1);
    PrintTextOnWindowToFitPx(PSS_LABEL_WINDOW_PORTRAIT_NICKNAME, gStringVar1, 0, 1, 0, 1, WindowWidthPx(PSS_LABEL_WINDOW_PORTRAIT_NICKNAME) - 9);
    PrintTextOnWindow(PSS_LABEL_WINDOW_PORTRAIT_SPECIES, gText_Slash, 0, 1, 0, 1);
    PrintTextOnWindowToFitPx(PSS_LABEL_WINDOW_PORTRAIT_SPECIES, GetSpeciesName(summary->species2), 6, 1, 0, 1, WindowWidthPx(PSS_LABEL_WINDOW_PORTRAIT_SPECIES) - 9);
    PrintGenderSymbol(mon, summary->species2);
    PutWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_NICKNAME);
    PutWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_SPECIES);
}

static void PrintEggInfo(void)
{
    GetMonNickname(&sMonSummaryScreen->currentMon, gStringVar1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_PORTRAIT_NICKNAME, gStringVar1, 0, 1, 0, 1);
    PutWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_NICKNAME);
    ClearWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_DEX_NUMBER);
    ClearWindowTilemap(PSS_LABEL_WINDOW_PORTRAIT_SPECIES);
}

static void PrintGenderSymbol(struct Pokemon *mon, enum Species species)
{
    if (species != SPECIES_NIDORAN_M && species != SPECIES_NIDORAN_F)
    {
        switch (GetMonGender(mon))
        {
        case MON_MALE:
            PrintTextOnWindow(PSS_LABEL_WINDOW_PORTRAIT_SPECIES, gText_MaleSymbol, 57, 17, 0, 3);
            break;
        case MON_FEMALE:
            PrintTextOnWindow(PSS_LABEL_WINDOW_PORTRAIT_SPECIES, gText_FemaleSymbol, 57, 17, 0, 4);
            break;
        }
    }
}

static void PrintAOrBButtonIcon(u8 windowId, bool8 bButton, u32 x)
{
    const u8 *button;
    if (!bButton)
        button = sButtons_Gfx[0];
    else
        button = sButtons_Gfx[1];

    BlitBitmapToWindow(windowId, button, x, 0, 16, 16);
}

//HYDRA Shows the R button icon beside the STATS/IVS/EVS label (R cycles between
// them), plus an A button icon just right of it on the IV/EV pages only, since
// only those are editable.
//
// These are sprites, not window blits. A window writes its whole rectangle into
// the BG tilemap, wiping whatever it overlaps, and the icons need to sit across
// tile rows that the ITEM box and the stat value windows already occupy. Sprites
// float above the BGs and touch no tilemap, so they can go anywhere.
static void DrawSkillsRButtonIcon(void)
{
    u8 *rId = &sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_HYDRA_R_BUTTON];
    bool8 onSkills = (sMonSummaryScreen->currPageIndex == PSS_PAGE_SKILLS && ShouldShowIvEvPrompt());
    // Must match the condition ShowUtilityPrompt uses to draw the A icon, or the
    // spacing between R and the text goes wrong.
    bool8 aShown = HyperPC_CanEditSkillsMode(sMonSummaryScreen->skillsPageMode);

    if (*rId == SPRITE_NONE)
        *rId = CreateSprite(&sSpriteTemplate_HydraRButton, HYDRA_BTN_R_CENTRE_X(0, TRUE), HYDRA_BTN_R_CENTRE_Y, 0);

    if (*rId != SPRITE_NONE)
    {
        // Park it immediately left of the A icon. The A icon moves with the prompt
        // text (which is right-aligned, so STATS/IVs/EVs each sit differently), so
        // this is recomputed every time the prompt is redrawn.
        gSprites[*rId].x = HYDRA_BTN_R_CENTRE_X(sMonSummaryScreen->promptIconX, aShown);
        gSprites[*rId].y = HYDRA_BTN_R_CENTRE_Y;
        gSprites[*rId].invisible = !onSkills;
    }
}

static void PrintPageNamesAndStats(void)
{
    int statsXPos;

    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_INFO_TITLE, gText_PkmnInfo, 2, 1, 0, 1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_SKILLS_TITLE, gText_PkmnSkills, 2, 1, 0, 1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_BATTLE_MOVES_TITLE, gText_BattleMoves, 2, 1, 0, 1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_CONTEST_MOVES_TITLE, gText_ContestMoves, 2, 1, 0, 1);

    ShowUtilityPrompt(SUMMARY_MODE_NORMAL);

    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_INFO_RENTAL, gText_RentalPkmn, 0, 1, 0, 1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_INFO_TYPE, gText_TypeSlash, 0, 1, 0, 0);
    statsXPos = 6 + GetStringCenterAlignXOffset(FONT_NORMAL, gText_HP4, 42);
    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_LEFT, gText_HP4, statsXPos, 1, 0, 1);
    statsXPos = 6 + GetStringCenterAlignXOffset(FONT_NORMAL, gText_Attack3, 42);
    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_LEFT, gText_Attack3, statsXPos, 17, 0, 1);
    statsXPos = 6 + GetStringCenterAlignXOffset(FONT_NORMAL, gText_Defense3, 42);
    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_LEFT, gText_Defense3, statsXPos, 33, 0, 1);
    statsXPos = 2 + GetStringCenterAlignXOffset(FONT_NORMAL, gText_SpAtk4, 36);
    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_RIGHT, gText_SpAtk4, statsXPos, 1, 0, 1);
    statsXPos = 2 + GetStringCenterAlignXOffset(FONT_NORMAL, gText_SpDef4, 36);
    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_RIGHT, gText_SpDef4, statsXPos, 17, 0, 1);
    statsXPos = 2 + GetStringCenterAlignXOffset(FONT_NORMAL, gText_Speed2, 36);
    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_RIGHT, gText_Speed2, statsXPos, 33, 0, 1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_SKILLS_EXP, gText_ExpPoints, 6, 1, 0, 1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_SKILLS_EXP, gText_NextLv, 6, 17, 0, 1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATUS, gText_Status, 2, 1, 0, 1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_MOVES_POWER_ACC, gText_Power, 0, 1, 0, 1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_MOVES_POWER_ACC, gText_Accuracy2, 0, 17, 0, 1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_MOVES_APPEAL_JAM, gText_Appeal, 0, 1, 0, 1);
    PrintTextOnWindow(PSS_LABEL_WINDOW_MOVES_APPEAL_JAM, gText_Jam, 0, 17, 0, 1);
}

static void PutPageWindowTilemaps(u8 page)
{
    u8 i;

    ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_INFO_TITLE);
    ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_TITLE);
    ClearWindowTilemap(PSS_LABEL_WINDOW_BATTLE_MOVES_TITLE);
    ClearWindowTilemap(PSS_LABEL_WINDOW_CONTEST_MOVES_TITLE);

    switch (page)
    {
    case PSS_PAGE_INFO:
        PutWindowTilemap(PSS_LABEL_WINDOW_POKEMON_INFO_TITLE);
        PutWindowTilemap(PSS_LABEL_WINDOW_PROMPT_UTILITY);
        if (InBattleFactory() == TRUE || InSlateportBattleTent() == TRUE)
            PutWindowTilemap(PSS_LABEL_WINDOW_POKEMON_INFO_RENTAL);
        PutWindowTilemap(PSS_LABEL_WINDOW_POKEMON_INFO_TYPE);
        break;
    case PSS_PAGE_SKILLS:
        PutWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_TITLE);
        PutWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_LEFT);
        PutWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_RIGHT);
        PutWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_EXP);
        if (ShouldShowIvEvPrompt())
            PutWindowTilemap(PSS_LABEL_WINDOW_PROMPT_UTILITY);
        break;
    case PSS_PAGE_BATTLE_MOVES:
        PutWindowTilemap(PSS_LABEL_WINDOW_BATTLE_MOVES_TITLE);
        PutWindowTilemap(PSS_LABEL_WINDOW_PROMPT_UTILITY);
        if (sMonSummaryScreen->mode == SUMMARY_MODE_SELECT_MOVE)
        {
            if (sMonSummaryScreen->newMove != MOVE_NONE || sMonSummaryScreen->firstMoveIndex != MAX_MON_MOVES)
                PutWindowTilemap(PSS_LABEL_WINDOW_MOVES_POWER_ACC);
        }
        else
        {
            if (ShouldShowMoveRelearner())
                PutWindowTilemap(PSS_LABEL_WINDOW_PROMPT_RELEARN);
        }
        break;
    case PSS_PAGE_CONTEST_MOVES:
        PutWindowTilemap(PSS_LABEL_WINDOW_CONTEST_MOVES_TITLE);
        PutWindowTilemap(PSS_LABEL_WINDOW_PROMPT_UTILITY);
        if (sMonSummaryScreen->mode == SUMMARY_MODE_SELECT_MOVE)
        {
            if (sMonSummaryScreen->newMove != MOVE_NONE || sMonSummaryScreen->firstMoveIndex != MAX_MON_MOVES)
                PutWindowTilemap(PSS_LABEL_WINDOW_MOVES_APPEAL_JAM);
        }
        else
        {
            if (ShouldShowMoveRelearner())
                PutWindowTilemap(PSS_LABEL_WINDOW_PROMPT_RELEARN);
        }
        break;
    }

    for (i = 0; i < ARRAY_COUNT(sMonSummaryScreen->windowIds); i++)
        PutWindowTilemap(sMonSummaryScreen->windowIds[i]);

    ScheduleBgCopyTilemapToVram(0);
}

static void ClearPageWindowTilemaps(u8 page)
{
    u8 i;

    switch (page)
    {
    case PSS_PAGE_INFO:
        ClearWindowTilemap(PSS_LABEL_WINDOW_PROMPT_UTILITY);
        if (InBattleFactory() == TRUE || InSlateportBattleTent() == TRUE)
            ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_INFO_RENTAL);
        ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_INFO_TYPE);
        ClearWindowTilemap(PSS_LABEL_WINDOW_PROMPT_RELEARN);
        break;
    case PSS_PAGE_SKILLS:
        ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_LEFT);
        ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_STATS_RIGHT);
        ClearWindowTilemap(PSS_LABEL_WINDOW_POKEMON_SKILLS_EXP);
        if (ShouldShowIvEvPrompt())
            ClearWindowTilemap(PSS_LABEL_WINDOW_PROMPT_UTILITY);
        ClearWindowTilemap(PSS_LABEL_WINDOW_PROMPT_RELEARN);
        break;
    case PSS_PAGE_BATTLE_MOVES:
        if (sMonSummaryScreen->mode == SUMMARY_MODE_SELECT_MOVE)
        {
            if (sMonSummaryScreen->newMove != MOVE_NONE || sMonSummaryScreen->firstMoveIndex != MAX_MON_MOVES)
            {
                ClearWindowTilemap(PSS_LABEL_WINDOW_MOVES_POWER_ACC);
                gSprites[sMonSummaryScreen->categoryIconSpriteId].invisible = TRUE;
            }
        }

        ClearWindowTilemap(PSS_LABEL_WINDOW_PROMPT_RELEARN);
        break;
    case PSS_PAGE_CONTEST_MOVES:
        if (sMonSummaryScreen->mode == SUMMARY_MODE_SELECT_MOVE)
        {
            if (sMonSummaryScreen->newMove != MOVE_NONE || sMonSummaryScreen->firstMoveIndex != MAX_MON_MOVES)
                ClearWindowTilemap(PSS_LABEL_WINDOW_MOVES_APPEAL_JAM);
        }

        ClearWindowTilemap(PSS_LABEL_WINDOW_PROMPT_RELEARN);
        break;
    }

    for (i = 0; i < ARRAY_COUNT(sMonSummaryScreen->windowIds); i++)
        RemoveWindowByIndex(i);

    ScheduleBgCopyTilemapToVram(0);
}

static u8 AddWindowFromTemplateList(const struct WindowTemplate *template, u8 templateId)
{
    u8 *windowIdPtr = &sMonSummaryScreen->windowIds[templateId];
    if (*windowIdPtr == WINDOW_NONE)
    {
        *windowIdPtr = AddWindow(&template[templateId]);
        FillWindowPixelBuffer(*windowIdPtr, PIXEL_FILL(0));
    }
    return *windowIdPtr;
}

static void RemoveWindowByIndex(u8 windowIndex)
{
    u8 *windowIdPtr = &sMonSummaryScreen->windowIds[windowIndex];
    if (*windowIdPtr != WINDOW_NONE)
    {
        ClearWindowTilemap(*windowIdPtr);
        RemoveWindow(*windowIdPtr);
        *windowIdPtr = WINDOW_NONE;
    }
}

static void PrintPageSpecificText(u8 pageIndex)
{
    u16 i;
    for (i = 0; i < ARRAY_COUNT(sMonSummaryScreen->windowIds); i++)
    {
        if (sMonSummaryScreen->windowIds[i] != WINDOW_NONE)
            FillWindowPixelBuffer(sMonSummaryScreen->windowIds[i], PIXEL_FILL(0));
    }
    sTextPrinterFunctions[pageIndex]();
}

static void CreateTextPrinterTask(u8 pageIndex)
{
    CreateTask(sTextPrinterTasks[pageIndex], 16);
}

static void PrintInfoPageText(void)
{
    if (sMonSummaryScreen->summary.isEgg)
    {
        PrintEggOTName();
        PrintEggOTID();
        PrintEggState();
        PrintEggMemo();
    }
    else
    {
        PrintMonOTName();
        PrintMonOTID();
        PrintMonAbilityName();
        PrintMonAbilityDescription();
        BufferMonTrainerMemo();
        PrintMonTrainerMemo();
    }
}

static void Task_PrintInfoPage(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    switch (data[0])
    {
    case 1:
        PrintMonOTName();
        break;
    case 2:
        PrintMonOTID();
        break;
    case 3:
        PrintMonAbilityName();
        break;
    case 4:
        PrintMonAbilityDescription();
        break;
    case 5:
        BufferMonTrainerMemo();
        break;
    case 6:
        PrintMonTrainerMemo();
        break;
    case 7:
        DestroyTask(taskId);
        return;
    }
    data[0]++;
}

static void PrintMonOTName(void)
{
    int x, windowId;
    if (InBattleFactory() != TRUE && InSlateportBattleTent() != TRUE)
    {
        windowId = AddWindowFromTemplateList(sPageInfoTemplate, PSS_DATA_WINDOW_INFO_ORIGINAL_TRAINER);
        PrintTextOnWindow(windowId, gText_OTSlash, 0, 1, 0, 1);
        x = GetStringWidth(FONT_NORMAL, gText_OTSlash, 0);
        if (sMonSummaryScreen->summary.OTGender == 0)
            PrintTextOnWindow(windowId, sMonSummaryScreen->summary.OTName, x, 1, 0, 5);
        else
            PrintTextOnWindow(windowId, sMonSummaryScreen->summary.OTName, x, 1, 0, 6);
    }
}

static void PrintMonOTID(void)
{
    int xPos;
    if (InBattleFactory() != TRUE && InSlateportBattleTent() != TRUE)
    {
        ConvertIntToDecimalStringN(StringCopy(gStringVar1, gText_IDNumber2), (u16)sMonSummaryScreen->summary.OTID, STR_CONV_MODE_LEADING_ZEROS, 5);
        xPos = GetStringRightAlignXOffset(FONT_NORMAL, gStringVar1, 56);
        PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, PSS_DATA_WINDOW_INFO_ID), gStringVar1, xPos, 1, 0, 1);
    }
}

static void PrintMonAbilityName(void)
{
    enum Ability ability = GetAbilityBySpecies(sMonSummaryScreen->summary.species, sMonSummaryScreen->summary.abilityNum);
    PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, PSS_DATA_WINDOW_INFO_ABILITY), gAbilitiesInfo[ability].name, 0, 1, 0, 1);
}

static void PrintMonAbilityDescription(void)
{
    enum Ability ability = GetAbilityBySpecies(sMonSummaryScreen->summary.species, sMonSummaryScreen->summary.abilityNum);
    PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, PSS_DATA_WINDOW_INFO_ABILITY), gAbilitiesInfo[ability].description, 0, 17, 0, 0);
}

static void BufferMonTrainerMemo(void)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    const u8 *text;

    DynamicPlaceholderTextUtil_Reset();
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, sMemoNatureTextColor);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(1, sMemoMiscTextColor);
    BufferNatureString();

    if (InBattleFactory() == TRUE || InSlateportBattleTent() == TRUE || IsInGamePartnerMon() == TRUE)
    {
        DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, gText_XNature);
    }
    else
    {
        u8 *metLevelString = Alloc(32);
        u8 *metLocationString = Alloc(32);
        GetMetLevelString(metLevelString);

        if (sum->metLocation < MAPSEC_NONE)
        {
            GetMapNameHandleAquaHideout(metLocationString, sum->metLocation);
            DynamicPlaceholderTextUtil_SetPlaceholderPtr(4, metLocationString);
        }

        if (DoesMonOTMatchOwner() == TRUE)
        {
            if (sum->metLevel == 0)
                text = (sum->metLocation >= MAPSEC_NONE) ? gText_XNatureHatchedSomewhereAt : gText_XNatureHatchedAtYZ;
            else
                text = (sum->metLocation >= MAPSEC_NONE) ? gText_XNatureMetSomewhereAt : gText_XNatureMetAtYZ;
        }
        else if (sum->metLocation == METLOC_FATEFUL_ENCOUNTER)
        {
            text = gText_XNatureFatefulEncounter;
        }
        else if (sum->metLocation != METLOC_IN_GAME_TRADE && DidMonComeFromGBAGames())
        {
            text = (sum->metLocation >= MAPSEC_NONE) ? gText_XNatureObtainedInTrade : gText_XNatureProbablyMetAt;
        }
        else
        {
            text = gText_XNatureObtainedInTrade;
        }

        DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, text);
        Free(metLevelString);
        Free(metLocationString);
    }
}

static void PrintMonTrainerMemo(void)
{
    PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, PSS_DATA_WINDOW_INFO_MEMO), gStringVar4, 0, 1, 0, 0);
}

static void BufferNatureString(void)
{
    struct PokemonSummaryScreenData *sumStruct = sMonSummaryScreen;
    //HYDRA show the EFFECTIVE nature (mint) rather than the personality-derived one,
    // so edits made here are reflected. MON_DATA_HIDDEN_NATURE reads back the true
    // nature when no mint has been applied, so this is identical for unedited mons.
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(2, gNaturesInfo[sumStruct->summary.mintNature].name);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(5, gText_EmptyString5);
}

static void GetMetLevelString(u8 *output)
{
    u8 level = sMonSummaryScreen->summary.metLevel;
    if (level == 0)
        level = EGG_HATCH_LEVEL;
    ConvertIntToDecimalStringN(output, level, STR_CONV_MODE_LEFT_ALIGN, 3);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(3, output);
}

static bool8 DoesMonOTMatchOwner(void)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    u32 trainerId;
    u8 gender;

    if (sMonSummaryScreen->monList.mons == gParties[B_TRAINER_OPPONENT_A])
    {
        u8 multiID = GetMultiplayerId() ^ 1;
        trainerId = gLinkPlayers[multiID].trainerId & 0xFFFF;
        gender = gLinkPlayers[multiID].gender;
        StringCopy(gStringVar1, gLinkPlayers[multiID].name);
    }
    else
    {
        trainerId = GetPlayerIDAsU32() & 0xFFFF;
        gender = gSaveBlock2Ptr->playerGender;
        StringCopy(gStringVar1, gSaveBlock2Ptr->playerName);
    }

    if (gender != sum->OTGender || trainerId != (sum->OTID & 0xFFFF) || StringCompareWithoutExtCtrlCodes(gStringVar1, sum->OTName))
        return FALSE;
    else
        return TRUE;
}

static bool8 DidMonComeFromGBAGames(void)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    if (sum->metGame > 0 && sum->metGame <= VERSION_LEAF_GREEN)
        return TRUE;
    return FALSE;
}

bool8 Hyper_DidMonComeFromRSE(void)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    if (sum->metGame > 0 && sum->metGame <= VERSION_EMERALD)
        return TRUE;
    return FALSE;
}

static bool8 IsInGamePartnerMon(void)
{
    if (gPartyMenu.layout == PARTY_LAYOUT_MULTI_FULL)
    {
        return FALSE;
    }
    else if (gPartyMenu.layout == PARTY_LAYOUT_MULTI_FULL_PARTNER)
    {
        return TRUE;
    }
    else if ((gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER) && gMain.inBattle)
    {
        if (sMonSummaryScreen->curMonIndex == 1 || sMonSummaryScreen->curMonIndex == 4 || sMonSummaryScreen->curMonIndex == 5)
            return TRUE;
    }
    return FALSE;
}

static void PrintEggOTName(void)
{
    u32 windowId = AddWindowFromTemplateList(sPageInfoTemplate, PSS_DATA_WINDOW_INFO_ORIGINAL_TRAINER);
    u32 width = GetStringWidth(FONT_NORMAL, gText_OTSlash, 0);
    PrintTextOnWindow(windowId, gText_OTSlash, 0, 1, 0, 1);
    PrintTextOnWindow(windowId, gText_FiveMarks, width, 1, 0, 1);
}

static void PrintEggOTID(void)
{
    int x;
    StringCopy(gStringVar1, gText_IDNumber2);
    StringAppend(gStringVar1, gText_FiveMarks);
    x = GetStringRightAlignXOffset(FONT_NORMAL, gStringVar1, 56);
    PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, PSS_DATA_WINDOW_INFO_ID), gStringVar1, x, 1, 0, 1);
}

static void PrintEggState(void)
{
    const u8 *text;
    struct PokeSummary *sum = &sMonSummaryScreen->summary;

    if (sMonSummaryScreen->summary.sanity == TRUE)
        text = gText_EggWillTakeALongTime;
    else if (sum->friendship <= 5)
        text = gText_EggAboutToHatch;
    else if (sum->friendship <= 10)
        text = gText_EggWillHatchSoon;
    else if (sum->friendship <= 40)
        text = gText_EggWillTakeSomeTime;
    else
        text = gText_EggWillTakeALongTime;

    PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, PSS_DATA_WINDOW_INFO_ABILITY), text, 0, 1, 0, 0);
}

static void PrintEggMemo(void)
{
    const u8 *text;
    struct PokeSummary *sum = &sMonSummaryScreen->summary;

    if (sMonSummaryScreen->summary.sanity != 1)
    {
        if (sum->metLocation == METLOC_FATEFUL_ENCOUNTER)
            text = gText_PeculiarEggNicePlace;
        else if (DidMonComeFromGBAGames() == FALSE || DoesMonOTMatchOwner() == FALSE)
            text = gText_PeculiarEggTrade;
        else if (sum->metLocation == METLOC_SPECIAL_EGG)
            text = (Hyper_DidMonComeFromRSE() == TRUE) ? gText_EggFromHotSprings : gText_EggFromTraveler;
        else
            text = gText_OddEggFoundByCouple;
    }
    else
    {
        text = gText_OddEggFoundByCouple;
    }

    PrintTextOnWindow(AddWindowFromTemplateList(sPageInfoTemplate, PSS_DATA_WINDOW_INFO_MEMO), text, 0, 1, 0, 0);
}

static void PrintSkillsPageText(void)
{
    PrintHeldItemName();
    PrintRibbonCount();
    if (ShouldShowIvEvPrompt())
        ShowUtilityPrompt(SUMMARY_SKILLS_MODE_STATS);
    BufferLeftColumnStats();
    PrintLeftColumnStats();
    BufferRightColumnStats();
    PrintRightColumnStats();
    PrintExpPointsNextLevel();
}

static void Task_PrintSkillsPage(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 1:
        PrintHeldItemName();
        break;
    case 2:
        PrintRibbonCount();
        break;
    case 3:
        ChangeStatLabel(SUMMARY_SKILLS_MODE_STATS);
        break;
    case 4:
        BufferLeftColumnStats();
        break;
    case 5:
        PrintLeftColumnStats();
        break;
    case 6:
        BufferRightColumnStats();
        break;
    case 7:
        PrintRightColumnStats();
        //HYDRA Draw the R icon on the same frame the stat numbers appear. It used
        // to be drawn back at case 3 with the label, which made it pop in early.
        DrawSkillsRButtonIcon();
        break;
    case 8:
        PrintExpPointsNextLevel();
        break;
    case 9:
        DestroyTask(taskId);
        return;
    }
    data[0]++;
}

static void PrintHeldItemName(void)
{
    const u8 *text;
    u32 fontId;
    int x;

    if (sMonSummaryScreen->summary.item == ITEM_ENIGMA_BERRY_E_READER
        && IsMultiBattle() == TRUE
        && (sMonSummaryScreen->curMonIndex == 1 || sMonSummaryScreen->curMonIndex == 4 || sMonSummaryScreen->curMonIndex == 5))
    {
        text = GetItemName(ITEM_ENIGMA_BERRY_E_READER);
    }
    else if (sMonSummaryScreen->summary.item == ITEM_NONE)
    {
        text = gText_None;
    }
    else
    {
        CopyItemName(sMonSummaryScreen->summary.item, gStringVar1);
        text = gStringVar1;
    }

    fontId = GetFontIdToFit(text, FONT_NORMAL, 0, WindowTemplateWidthPx(&sPageSkillsTemplate[PSS_DATA_WINDOW_SKILLS_HELD_ITEM]) - 8);
    x = GetStringCenterAlignXOffset(fontId, text, 72) + 6;
    PrintTextOnWindowWithFont(AddWindowFromTemplateList(sPageSkillsTemplate, PSS_DATA_WINDOW_SKILLS_HELD_ITEM), text, x, 1, 0, 0, fontId);
}

static void PrintRibbonCount(void)
{
    const u8 *text;
    int x;

    if (sMonSummaryScreen->summary.ribbonCount == 0)
    {
        text = gText_None;
    }
    else
    {
        ConvertIntToDecimalStringN(gStringVar1, sMonSummaryScreen->summary.ribbonCount, STR_CONV_MODE_RIGHT_ALIGN, 2);
        StringExpandPlaceholders(gStringVar4, gText_RibbonsVar1);
        text = gStringVar4;
    }

    x = GetStringCenterAlignXOffset(FONT_NORMAL, text, 70) + 6;
    PrintTextOnWindow(AddWindowFromTemplateList(sPageSkillsTemplate, PSS_DATA_WINDOW_SKILLS_RIBBON_COUNT), text, x, 1, 0, 0);
}

static void BufferStat(u8 *dst, enum Stat statIndex, u32 stat, u32 strId, u32 n)
{
    static const u8 sTextNatureDown[] = _("{COLOR}{08}");
    static const u8 sTextNatureUp[] = _("{COLOR}{05}");
    static const u8 sTextNatureNeutral[] = _("{COLOR}{01}");
    u8 *txtPtr;

    if (statIndex == 0 || !P_SUMMARY_SCREEN_NATURE_COLORS || gNaturesInfo[sMonSummaryScreen->summary.mintNature].statUp == gNaturesInfo[sMonSummaryScreen->summary.mintNature].statDown)
        txtPtr = StringCopy(dst, sTextNatureNeutral);
    else if (statIndex == gNaturesInfo[sMonSummaryScreen->summary.mintNature].statUp)
        txtPtr = StringCopy(dst, sTextNatureUp);
    else if (statIndex == gNaturesInfo[sMonSummaryScreen->summary.mintNature].statDown)
        txtPtr = StringCopy(dst, sTextNatureDown);
    else
        txtPtr = StringCopy(dst, sTextNatureNeutral);

    if (!P_SUMMARY_SCREEN_IV_EV_VALUES
        && sMonSummaryScreen->skillsPageMode == SUMMARY_SKILLS_MODE_IVS)
        StringAppend(dst, GetLetterGrade(stat));
    else
        ConvertIntToDecimalStringN(txtPtr, stat, STR_CONV_MODE_RIGHT_ALIGN, n);

    DynamicPlaceholderTextUtil_SetPlaceholderPtr(strId, dst);
}

static const u8 *GetLetterGrade(u32 stat)
{
    static const u8 gText_GradeF[] = _("F");
    static const u8 gText_GradeD[] = _("D");
    static const u8 gText_GradeC[] = _("C");
    static const u8 gText_GradeB[] = _("B");
    static const u8 gText_GradeA[] = _("A");
    static const u8 gText_GradeS[] = _("S");

    if (stat <= 0)
        return gText_GradeF;
    else if (stat <= 15)
        return gText_GradeD;
    else if (stat <= 25)
        return gText_GradeC;
    else if (stat <= 29)
        return gText_GradeB;
    else if (stat <= 30)
        return gText_GradeA;
    else
        return gText_GradeS;
}

static void BufferLeftColumnStats(void)
{
    u8 *currentHPString = Alloc(20);
    u8 *maxHPString = Alloc(20);
    u8 *attackString = Alloc(20);
    u8 *defenseString = Alloc(20);

    DynamicPlaceholderTextUtil_Reset();

    BufferStat(currentHPString, STAT_HP, sMonSummaryScreen->summary.currentHP, 0, 3);
    BufferStat(maxHPString, STAT_HP, sMonSummaryScreen->summary.maxHP, 1, 3);
    BufferStat(attackString, STAT_ATK, sMonSummaryScreen->summary.atk, 2, 7);
    BufferStat(defenseString, STAT_DEF, sMonSummaryScreen->summary.def, 3, 7);

    DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, sStatsLeftColumnLayout);

    Free(currentHPString);
    Free(maxHPString);
    Free(attackString);
    Free(defenseString);
}

static void BufferLeftColumnIvEvStats(void)
{
    u8 *hpIvEvString = Alloc(20);
    u8 *attackIvEvString = Alloc(20);
    u8 *defenseIvEvString = Alloc(20);

    DynamicPlaceholderTextUtil_Reset();

    BufferStat(hpIvEvString, STAT_HP, sMonSummaryScreen->summary.currentHP, 0, 7);
    BufferStat(attackIvEvString, STAT_ATK, sMonSummaryScreen->summary.atk, 1, 7);
    BufferStat(defenseIvEvString, STAT_DEF, sMonSummaryScreen->summary.def, 2, 7);

    DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, sStatsLeftIVEVColumnLayout);

    Free(hpIvEvString);
    Free(attackIvEvString);
    Free(defenseIvEvString);
}

static void PrintLeftColumnStats(void)
{
    int x;

    if (sMonSummaryScreen->skillsPageMode == SUMMARY_SKILLS_MODE_IVS && !P_SUMMARY_SCREEN_IV_EV_VALUES)
        x = GetStringRightAlignXOffset(FONT_NORMAL, gStringVar4, 46);
    else
        x = 4;

    PrintTextOnWindow(AddWindowFromTemplateList(sPageSkillsTemplate, PSS_DATA_WINDOW_SKILLS_STATS_LEFT), gStringVar4, x, 1, 0, 0);
}

static void BufferRightColumnStats(void)
{
    DynamicPlaceholderTextUtil_Reset();

    BufferStat(gStringVar1, STAT_SPATK, sMonSummaryScreen->summary.spatk, 0, 3);
    BufferStat(gStringVar2, STAT_SPDEF, sMonSummaryScreen->summary.spdef, 1, 3);
    BufferStat(gStringVar3, STAT_SPEED, sMonSummaryScreen->summary.speed, 2, 3);

    DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, sStatsRightColumnLayout);
}

static void PrintRightColumnStats(void)
{
    int x;

    if (sMonSummaryScreen->skillsPageMode == SUMMARY_SKILLS_MODE_IVS && !P_SUMMARY_SCREEN_IV_EV_VALUES)
        x = GetStringRightAlignXOffset(FONT_NORMAL, gStringVar4, 20);
    else
        x = 2;

    PrintTextOnWindow(AddWindowFromTemplateList(sPageSkillsTemplate, PSS_DATA_WINDOW_SKILLS_STATS_RIGHT), gStringVar4, x, 1, 0, 0);
}

static void PrintExpPointsNextLevel(void)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    u8 windowId = AddWindowFromTemplateList(sPageSkillsTemplate, PSS_DATA_WINDOW_EXP);
    int x;
    u32 expToNextLevel;

    ConvertIntToDecimalStringN(gStringVar1, sum->exp, STR_CONV_MODE_RIGHT_ALIGN, 7);
    x = GetStringRightAlignXOffset(FONT_NORMAL, gStringVar1, 42) + 2;
    PrintTextOnWindow(windowId, gStringVar1, x, 1, 0, 0);

    if (sum->level < MAX_LEVEL)
        expToNextLevel = gExperienceTables[gSpeciesInfo[sum->species].growthRate][sum->level + 1] - sum->exp;
    else
        expToNextLevel = 0;

    ConvertIntToDecimalStringN(gStringVar1, expToNextLevel, STR_CONV_MODE_RIGHT_ALIGN, 6);
    x = GetStringRightAlignXOffset(FONT_NORMAL, gStringVar1, 42) + 2;
    PrintTextOnWindow(windowId, gStringVar1, x, 17, 0, 0);
}

static void PrintBattleMoves(void)
{
    PrintMoveNameAndPP(0);
    PrintMoveNameAndPP(1);
    PrintMoveNameAndPP(2);
    PrintMoveNameAndPP(3);
    if (sMonSummaryScreen->mode == SUMMARY_MODE_SELECT_MOVE)
    {
        PrintNewMoveDetailsOrCancelText();
        if (sMonSummaryScreen->firstMoveIndex == MAX_MON_MOVES)
        {
            if (sMonSummaryScreen->newMove != MOVE_NONE)
                PrintMoveDetails(sMonSummaryScreen->newMove);
        }
        else
        {
            PrintMoveDetails(sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex]);
        }
    }
}

static void Task_PrintBattleMoves(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 1:
        PrintMoveNameAndPP(0);
        break;
    case 2:
        PrintMoveNameAndPP(1);
        break;
    case 3:
        PrintMoveNameAndPP(2);
        break;
    case 4:
        PrintMoveNameAndPP(3);
        break;
    case 5:
        if (sMonSummaryScreen->mode == SUMMARY_MODE_SELECT_MOVE)
            PrintNewMoveDetailsOrCancelText();
        break;
    case 6:
        if (sMonSummaryScreen->mode == SUMMARY_MODE_SELECT_MOVE)
        {
            if (sMonSummaryScreen->firstMoveIndex == MAX_MON_MOVES)
                data[1] = sMonSummaryScreen->newMove;
            else
                data[1] = sMonSummaryScreen->summary.moves[sMonSummaryScreen->firstMoveIndex];
        }
        break;
    case 7:
        if (sMonSummaryScreen->mode == SUMMARY_MODE_SELECT_MOVE)
        {
            if (sMonSummaryScreen->newMove != MOVE_NONE || sMonSummaryScreen->firstMoveIndex != MAX_MON_MOVES)
                PrintMoveDetails(data[1]);
        }
        break;
    case 8:
        DestroyTask(taskId);
        return;
    }
    data[0]++;
}

static void PrintMoveNameAndPP(u8 moveIndex)
{
    u8 pp;
    int ppState, x;
    const u8 *text;
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    u8 moveNameWindowId = AddWindowFromTemplateList(sPageMovesTemplate, PSS_DATA_WINDOW_MOVE_NAMES);
    u8 ppValueWindowId = AddWindowFromTemplateList(sPageMovesTemplate, PSS_DATA_WINDOW_MOVE_PP);
    enum Move move = summary->moves[moveIndex];

    if (move != 0)
    {
        pp = CalculatePPWithBonus(move, summary->ppBonuses, moveIndex);
        PrintTextOnWindowToFit(moveNameWindowId, GetMoveName(move), 0, moveIndex * 16 + 1, 0, 1);
        ConvertIntToDecimalStringN(gStringVar1, summary->pp[moveIndex], STR_CONV_MODE_RIGHT_ALIGN, 2);
        ConvertIntToDecimalStringN(gStringVar2, pp, STR_CONV_MODE_RIGHT_ALIGN, 2);
        DynamicPlaceholderTextUtil_Reset();
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, gStringVar1);
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(1, gStringVar2);
        DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, sMovesPPLayout);
        text = gStringVar4;
        ppState = GetCurrentPpToMaxPpState(summary->pp[moveIndex], pp) + 9;
        x = GetStringRightAlignXOffset(FONT_NORMAL, text, 44);
    }
    else
    {
        PrintTextOnWindow(moveNameWindowId, gText_OneDash, 0, moveIndex * 16 + 1, 0, 1);
        text = gText_TwoDashes;
        ppState = 12;
        x = GetStringCenterAlignXOffset(FONT_NORMAL, text, 44);
    }

    PrintTextOnWindow(ppValueWindowId, text, x, moveIndex * 16 + 1, 0, ppState);
}

static void PrintMovePowerAndAccuracy(enum Move moveIndex)
{
    const u8 *text;
    if (moveIndex != MOVE_NONE)
    {
        FillWindowPixelRect(PSS_LABEL_WINDOW_MOVES_POWER_ACC, PIXEL_FILL(0), 53, 0, 19, 32);

        u32 power = GetMovePower(moveIndex);
        if (power < 2)
        {
            text = gText_ThreeDashes;
        }
        else
        {
            ConvertIntToDecimalStringN(gStringVar1, power, STR_CONV_MODE_RIGHT_ALIGN, 3);
            text = gStringVar1;
        }

        PrintTextOnWindow(PSS_LABEL_WINDOW_MOVES_POWER_ACC, text, 53, 1, 0, 0);

        u32 accuracy = GetMoveAccuracy(moveIndex);
        if (accuracy == 0)
        {
            text = gText_ThreeDashes;
        }
        else
        {
            ConvertIntToDecimalStringN(gStringVar1, accuracy, STR_CONV_MODE_RIGHT_ALIGN, 3);
            text = gStringVar1;
        }

        PrintTextOnWindow(PSS_LABEL_WINDOW_MOVES_POWER_ACC, text, 53, 17, 0, 0);
    }
}

static void PrintContestMoves(void)
{
    PrintMoveNameAndPP(0);
    PrintMoveNameAndPP(1);
    PrintMoveNameAndPP(2);
    PrintMoveNameAndPP(3);

    if (sMonSummaryScreen->mode == SUMMARY_MODE_SELECT_MOVE)
    {
        PrintNewMoveDetailsOrCancelText();
        PrintContestMoveDescription(sMonSummaryScreen->firstMoveIndex);
    }
}

static void Task_PrintContestMoves(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 1:
        PrintMoveNameAndPP(0);
        break;
    case 2:
        PrintMoveNameAndPP(1);
        break;
    case 3:
        PrintMoveNameAndPP(2);
        break;
    case 4:
        PrintMoveNameAndPP(3);
        break;
    case 5:
        if (sMonSummaryScreen->mode == SUMMARY_MODE_SELECT_MOVE)
            PrintNewMoveDetailsOrCancelText();
        break;
    case 6:
        if (sMonSummaryScreen->mode == SUMMARY_MODE_SELECT_MOVE)
        {
            if (sMonSummaryScreen->newMove != MOVE_NONE || sMonSummaryScreen->firstMoveIndex != MAX_MON_MOVES)
                PrintContestMoveDescription(sMonSummaryScreen->firstMoveIndex);
        }
        break;
    case 7:
        DestroyTask(taskId);
        return;
    }
    data[0]++;
}

static void PrintContestMoveDescription(u8 moveSlot)
{
    enum Move move;

    if (moveSlot == MAX_MON_MOVES)
        move = sMonSummaryScreen->newMove;
    else
        move = sMonSummaryScreen->summary.moves[moveSlot];

    if (move != MOVE_NONE)
    {
        u8 windowId = AddWindowFromTemplateList(sPageMovesTemplate, PSS_DATA_WINDOW_MOVE_DESCRIPTION);
        PrintTextOnWindow(windowId, gContestEffects[GetMoveContestEffect(move)].description, 6, 1, 0, 0);
    }
}

static void PrintMoveDetails(enum Move move)
{
    u8 windowId = AddWindowFromTemplateList(sPageMovesTemplate, PSS_DATA_WINDOW_MOVE_DESCRIPTION);
    FillWindowPixelBuffer(windowId, PIXEL_FILL(0));
    if (move != MOVE_NONE)
    {
        if (sMonSummaryScreen->currPageIndex == PSS_PAGE_BATTLE_MOVES)
        {
            if (B_SHOW_CATEGORY_ICON == TRUE)
                ShowCategoryIcon(GetBattleMoveCategory(move));
            PrintMovePowerAndAccuracy(move);
            PrintTextOnWindow(windowId, GetMoveDescription(move), 6, 1, 0, 0);
        }
        else
        {
            PrintTextOnWindow(windowId, gContestEffects[GetMoveContestEffect(move)].description, 6, 1, 0, 0);
        }
        PutWindowTilemap(windowId);
    }
    else
    {
        ClearWindowTilemap(windowId);
    }

    ScheduleBgCopyTilemapToVram(0);
}

static void PrintNewMoveDetailsOrCancelText(void)
{
    u8 windowId1 = AddWindowFromTemplateList(sPageMovesTemplate, PSS_DATA_WINDOW_MOVE_NAMES);
    u8 windowId2 = AddWindowFromTemplateList(sPageMovesTemplate, PSS_DATA_WINDOW_MOVE_PP);

    if (sMonSummaryScreen->newMove == MOVE_NONE)
    {
        PrintTextOnWindow(windowId1, gText_Cancel, 0, 65, 0, 1);
    }
    else
    {
        enum Move move = sMonSummaryScreen->newMove;

        if (sMonSummaryScreen->currPageIndex == PSS_PAGE_BATTLE_MOVES)
            PrintTextOnWindowToFit(windowId1, GetMoveName(move), 0, 65, 0, 6);
        else
            PrintTextOnWindowToFit(windowId1, GetMoveName(move), 0, 65, 0, 5);

        ConvertIntToDecimalStringN(gStringVar1, GetMovePP(move), STR_CONV_MODE_RIGHT_ALIGN, 2);
        DynamicPlaceholderTextUtil_Reset();
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, gStringVar1);
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(1, gStringVar1);
        DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, sMovesPPLayout);
        PrintTextOnWindow(windowId2, gStringVar4, GetStringRightAlignXOffset(FONT_NORMAL, gStringVar4, 44), 65, 0, 12);
    }
}

static void AddAndFillMoveNamesWindow(void)
{
    u8 windowId = AddWindowFromTemplateList(sPageMovesTemplate, PSS_DATA_WINDOW_MOVE_NAMES);
    FillWindowPixelRect(windowId, PIXEL_FILL(0), 0, 66, 72, 16);
    CopyWindowToVram(windowId, COPYWIN_GFX);
}

static void SwapMovesNamesPP(u8 moveIndex1, u8 moveIndex2)
{
    u8 windowId1 = AddWindowFromTemplateList(sPageMovesTemplate, PSS_DATA_WINDOW_MOVE_NAMES);
    u8 windowId2 = AddWindowFromTemplateList(sPageMovesTemplate, PSS_DATA_WINDOW_MOVE_PP);

    FillWindowPixelRect(windowId1, PIXEL_FILL(0), 0, moveIndex1 * 16, 72, 16);
    FillWindowPixelRect(windowId1, PIXEL_FILL(0), 0, moveIndex2 * 16, 72, 16);

    FillWindowPixelRect(windowId2, PIXEL_FILL(0), 0, moveIndex1 * 16, 48, 16);
    FillWindowPixelRect(windowId2, PIXEL_FILL(0), 0, moveIndex2 * 16, 48, 16);

    PrintMoveNameAndPP(moveIndex1);
    PrintMoveNameAndPP(moveIndex2);
}

static void PrintHMMovesCantBeForgotten(void)
{
    u8 windowId = AddWindowFromTemplateList(sPageMovesTemplate, PSS_DATA_WINDOW_MOVE_DESCRIPTION);
    FillWindowPixelBuffer(windowId, PIXEL_FILL(0));
    PrintTextOnWindow(windowId, gText_HMMovesCantBeForgotten2, 6, 1, 0, 0);
}

static void ResetSpriteIds(void)
{
    u8 i;

    for (i = 0; i < ARRAY_COUNT(sMonSummaryScreen->spriteIds); i++)
        sMonSummaryScreen->spriteIds[i] = SPRITE_NONE;
}

static void DestroySpriteInArray(u8 spriteArrayId)
{
    if (sMonSummaryScreen->spriteIds[spriteArrayId] != SPRITE_NONE)
    {
        DestroySprite(&gSprites[sMonSummaryScreen->spriteIds[spriteArrayId]]);
        sMonSummaryScreen->spriteIds[spriteArrayId] = SPRITE_NONE;
    }
}

static void SetSpriteInvisibility(u8 spriteArrayId, bool8 invisible)
{
    gSprites[sMonSummaryScreen->spriteIds[spriteArrayId]].invisible = invisible;
}

static void HidePageSpecificSprites(void)
{
    // Keeps Pokémon, caught ball and status sprites visible.
    u8 i;

    for (i = SPRITE_ARR_ID_TYPE; i < ARRAY_COUNT(sMonSummaryScreen->spriteIds); i++)
    {
        if (sMonSummaryScreen->spriteIds[i] != SPRITE_NONE)
            SetSpriteInvisibility(i, TRUE);
    }
}

static void SetTypeIcons(void)
{
    switch (sMonSummaryScreen->currPageIndex)
    {
    case PSS_PAGE_INFO:
        SetMonTypeIcons();
        break;
    case PSS_PAGE_BATTLE_MOVES:
        SetMoveTypeIcons();
        SetNewMoveTypeIcon();
        break;
    case PSS_PAGE_CONTEST_MOVES:
        SetContestMoveTypeIcons();
        SetNewMoveTypeIcon();
        break;
    }
}

static void CreateMoveTypeIcons(void)
{
    u8 i;

    for (i = SPRITE_ARR_ID_TYPE; i < SPRITE_ARR_ID_TYPE + TYPE_ICON_SPRITE_COUNT; i++)
    {
        if (sMonSummaryScreen->spriteIds[i] == SPRITE_NONE)
            sMonSummaryScreen->spriteIds[i] = CreateSprite(&gHyperSpriteTemplate_MoveTypes, 0, 0, 2);

        SetSpriteInvisibility(i, TRUE);
    }
}

void Hyper_SetTypeSpritePosAndPal(enum Type typeId, u8 x, u8 y, u8 spriteArrayId)
{
    struct Sprite *sprite = &gSprites[sMonSummaryScreen->spriteIds[spriteArrayId]];
    StartSpriteAnim(sprite, typeId);
    if (typeId < NUMBER_OF_MON_TYPES)
        sprite->oam.paletteNum = gTypesInfo[typeId].palette;
    else
        sprite->oam.paletteNum = gContestCategoryInfo[typeId - NUMBER_OF_MON_TYPES].palette;
    sprite->x = x + 16;
    sprite->y = y + 8;
    SetSpriteInvisibility(spriteArrayId, FALSE);
}

static void SetMonTypeIcons(void)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    if (summary->isEgg)
    {
        Hyper_SetTypeSpritePosAndPal(TYPE_MYSTERY, 120, 48, SPRITE_ARR_ID_TYPE);
        SetSpriteInvisibility(SPRITE_ARR_ID_TYPE + 1, TRUE);
    }
    else
    {
        Hyper_SetTypeSpritePosAndPal(GetSpeciesType(summary->species, 0), 120, 48, SPRITE_ARR_ID_TYPE);
        if (GetSpeciesType(summary->species, 0) != GetSpeciesType(summary->species, 1))
        {
            Hyper_SetTypeSpritePosAndPal(GetSpeciesType(summary->species, 1), 160, 48, SPRITE_ARR_ID_TYPE + 1);
            SetSpriteInvisibility(SPRITE_ARR_ID_TYPE + 1, FALSE);
        }
        else
        {
            SetSpriteInvisibility(SPRITE_ARR_ID_TYPE + 1, TRUE);
        }
        if (P_SHOW_TERA_TYPE >= GEN_9)
        {
            Hyper_SetTypeSpritePosAndPal(summary->teraType, 200, 48, SPRITE_ARR_ID_TYPE + 2);
        }
    }
}

static enum BattlerId GetCurrentBattlerFromSumIndex(u32 sumIndex)
{
    for (u32 battler = B_BATTLER_0; battler < gBattlersCount; battler++)
    {
        if (!IsOnPlayerSide(battler))
            continue;

        if (gBattlerPartyIndexes[battler] == sumIndex)
            return battler;
    }

    return B_BATTLER_0;
}

static enum Type SummaryScreen_GetDynamicMoveType(struct Pokemon *mon, enum Move move, enum Type type)
{
    if (!P_SHOW_DYNAMIC_TYPES)
        return type;

    if (gBattleStruct == NULL)
        return CheckDynamicMoveType(mon, move, 0, MON_OUTSIDE_BATTLE);

    u32 partyIndex = sMonSummaryScreen->curMonIndex;
    bool32 isDouble = IsDoubleBattle();

    if ((isDouble && partyIndex > 1) || (!isDouble && partyIndex > 0))
        return CheckDynamicMoveType(mon, move, 0, MON_OUTSIDE_BATTLE);

    return CheckDynamicMoveType(mon, move, GetCurrentBattlerFromSumIndex(sMonSummaryScreen->curMonIndex), MON_IN_BATTLE);
}

static void SetMoveTypeIcons(void)
{
    u32 i;
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    struct Pokemon *mon = &sMonSummaryScreen->currentMon;
    enum Type type;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (summary->moves[i] != MOVE_NONE)
        {
            type = GetMoveType(summary->moves[i]);
            type = SummaryScreen_GetDynamicMoveType(mon, summary->moves[i], type);
            Hyper_SetTypeSpritePosAndPal(type, 85, 32 + (i * 16), i + SPRITE_ARR_ID_TYPE);
        }
        else
        {
            SetSpriteInvisibility(i + SPRITE_ARR_ID_TYPE, TRUE);
        }
    }
}

static void SetContestMoveTypeIcons(void)
{
    u8 i;
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (summary->moves[i] != MOVE_NONE)
            Hyper_SetTypeSpritePosAndPal(NUMBER_OF_MON_TYPES + GetMoveContestCategory(summary->moves[i]), 85, 32 + (i * 16), i + SPRITE_ARR_ID_TYPE);
        else
            SetSpriteInvisibility(i + SPRITE_ARR_ID_TYPE, TRUE);
    }
}

static void SetNewMoveTypeIcon(void)
{
    struct Pokemon *mon = &sMonSummaryScreen->currentMon;
    enum Type type = GetMoveType(sMonSummaryScreen->newMove);
    type = SummaryScreen_GetDynamicMoveType(mon, sMonSummaryScreen->newMove, type);

    if (sMonSummaryScreen->newMove == MOVE_NONE)
    {
        SetSpriteInvisibility(SPRITE_ARR_ID_TYPE + 4, TRUE);
    }
    else
    {
        if (sMonSummaryScreen->currPageIndex == PSS_PAGE_BATTLE_MOVES)
        {
            Hyper_SetTypeSpritePosAndPal(type, 85, 96, SPRITE_ARR_ID_TYPE + 4);
        }
        else
        {
            Hyper_SetTypeSpritePosAndPal(NUMBER_OF_MON_TYPES + GetMoveContestCategory(sMonSummaryScreen->newMove), 85, 96, SPRITE_ARR_ID_TYPE + 4);
        }
    }
}

static void SwapMovesTypeSprites(u8 moveIndex1, u8 moveIndex2)
{
    struct Sprite *sprite1 = &gSprites[sMonSummaryScreen->spriteIds[moveIndex1 + SPRITE_ARR_ID_TYPE]];
    struct Sprite *sprite2 = &gSprites[sMonSummaryScreen->spriteIds[moveIndex2 + SPRITE_ARR_ID_TYPE]];

    u8 temp = sprite1->animNum;
    sprite1->animNum = sprite2->animNum;
    sprite2->animNum = temp;

    temp = sprite1->oam.paletteNum;
    sprite1->oam.paletteNum = sprite2->oam.paletteNum;
    sprite2->oam.paletteNum = temp;

    sprite1->animBeginning = TRUE;
    sprite1->animEnded = FALSE;
    sprite2->animBeginning = TRUE;
    sprite2->animEnded = FALSE;
}

static u8 LoadMonGfxAndSprite(struct Pokemon *mon, s16 *state)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;

    switch (*state)
    {
    default:
        return CreateMonSprite(mon);
    case 0:
        if (gMain.inBattle)
        {
            HandleLoadSpecialPokePicIsEgg(TRUE,
                                     gMonSpritesGfxPtr->spritesGfx[B_POSITION_OPPONENT_LEFT],
                                     summary->species,
                                     summary->pid,
                                     summary->isEgg);
        }
        else
        {
            if (gMonSpritesGfxPtr != NULL)
            {
                HandleLoadSpecialPokePicIsEgg(TRUE,
                                         gMonSpritesGfxPtr->spritesGfx[B_POSITION_OPPONENT_LEFT],
                                         summary->species,
                                         summary->pid,
                                         summary->isEgg);
            }
            else
            {
                HandleLoadSpecialPokePicIsEgg(TRUE,
                                         MonSpritesGfxManager_GetSpritePtr(MON_SPR_GFX_MANAGER_A, B_POSITION_OPPONENT_LEFT),
                                         summary->species,
                                         summary->pid,
                                         summary->isEgg);
            }
        }
        (*state)++;
        return 0xFF;
    case 1:
        LoadSpritePaletteWithTag(GetMonSpritePalFromSpeciesAndPersonalityIsEgg(summary->species, summary->isShiny, summary->pid, summary->isEgg), summary->species2);
        SetMultiuseSpriteTemplateToPokemon(summary->species2, B_POSITION_OPPONENT_LEFT);
        (*state)++;
        return 0xFF;
    }
}

static void PlayMonCry(void)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    if (!summary->isEgg)
    {
        if (ShouldPlayNormalMonCry(&sMonSummaryScreen->currentMon) == TRUE)
            PlayCry_ByMode(summary->species2, 0, CRY_MODE_NORMAL);
        else
            PlayCry_ByMode(summary->species2, 0, CRY_MODE_WEAK);
    }
}

static u8 CreateMonSprite(struct Pokemon *unused)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;
    u8 spriteId = CreateSprite(&gMultiuseSpriteTemplate, 40, 64, 5);

    FreeSpriteOamMatrix(&gSprites[spriteId]);
    gSprites[spriteId].data[0] = summary->species2;
    gSprites[spriteId].data[2] = 0;
    gSprites[spriteId].callback = SpriteCB_Pokemon;
    gSprites[spriteId].oam.priority = 0;

    if (!IsMonSpriteNotFlipped(summary->species2))
        gSprites[spriteId].hFlip = TRUE;
    else
        gSprites[spriteId].hFlip = FALSE;

    return spriteId;
}

static void SpriteCB_Pokemon(struct Sprite *sprite)
{
    struct PokeSummary *summary = &sMonSummaryScreen->summary;

    if (!gPaletteFade.active && sprite->data[2] != 1)
    {
        sprite->data[1] = IsMonSpriteNotFlipped(sprite->data[0]);
        PlayMonCry();
        PokemonSummaryDoMonAnimation(sprite, sprite->data[0], summary->isEgg);
    }
}

// Track and then destroy Task_PokemonSummaryAnimateAfterDelay
// Normally destroys itself but it can be interrupted before the animation starts
void HyperSummaryScreen_SetAnimDelayTaskId(u8 taskId)
{
    sAnimDelayTaskId = taskId;
}

static void SummaryScreen_DestroyAnimDelayTask(void)
{
    if (sAnimDelayTaskId != TASK_NONE)
    {
        DestroyTask(sAnimDelayTaskId);
        sAnimDelayTaskId = TASK_NONE;
    }
}

static bool32 UNUSED IsMonAnimationFinished(void)
{
    if (gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].callback == SpriteCallbackDummy)
        return FALSE;
    else
        return TRUE;
}

static void StopPokemonAnimations(void)  // A subtle effect, this function stops Pokémon animations when leaving the PSS
{
    u16 i;
    u16 paletteIndex;

    gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].animPaused = TRUE;
    gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].callback = SpriteCallbackDummy;
    StopPokemonAnimationDelayTask();

    paletteIndex = OBJ_PLTT_ID(gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MON]].oam.paletteNum);

    for (i = 0; i < 16; i++)
    {
        u16 id = i + paletteIndex;
        gPlttBufferUnfaded[id] = gPlttBufferFaded[id];
    }
}

static void CreateMonMarkingsSprite(struct Pokemon *mon)
{
    struct Sprite *sprite = CreateMonMarkingAllCombosSprite(TAG_MON_MARKINGS, TAG_MON_MARKINGS, sMarkings_Pal);

    sMonSummaryScreen->markingsSprite = sprite;
    if (sprite != NULL)
    {
        StartSpriteAnim(sprite, GetMonData(mon, MON_DATA_MARKINGS));
        sMonSummaryScreen->markingsSprite->x = 60;
        sMonSummaryScreen->markingsSprite->y = 26;
        sMonSummaryScreen->markingsSprite->oam.priority = 1;
    }
}

static void RemoveAndCreateMonMarkingsSprite(struct Pokemon *mon)
{
    DestroySprite(sMonSummaryScreen->markingsSprite);
    FreeSpriteTilesByTag(TAG_MON_MARKINGS);
    CreateMonMarkingsSprite(mon);
}

static void CreateCaughtBallSprite(struct Pokemon *mon)
{
    enum PokeBall ball = GetMonData(mon, MON_DATA_POKEBALL);

    LoadBallGfx(ball);
    sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_BALL] = CreateSprite(&gPokeBalls[ball].spriteTemplate, 16, 136, 0);
    gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_BALL]].callback = SpriteCallbackDummy;
    gSprites[sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_BALL]].oam.priority = 3;
}

static void CreateSetStatusSprite(void)
{
    u8 *spriteId = &sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_STATUS];
    u8 statusAnim;

    if (*spriteId == SPRITE_NONE)
        *spriteId = CreateSprite(&sSpriteTemplate_StatusCondition, 64, 152, 0);

    statusAnim = GetMonAilment(&sMonSummaryScreen->currentMon);
    if (statusAnim != 0)
    {
        StartSpriteAnim(&gSprites[*spriteId], statusAnim - 1);
        SetSpriteInvisibility(SPRITE_ARR_ID_STATUS, FALSE);
    }
    else
    {
        SetSpriteInvisibility(SPRITE_ARR_ID_STATUS, TRUE);
    }
}

static void CreateMoveSelectorSprites(u8 idArrayStart)
{
    u8 i;
    u8 *spriteIds = &sMonSummaryScreen->spriteIds[idArrayStart];

    if (sMonSummaryScreen->currPageIndex >= PSS_PAGE_BATTLE_MOVES)
    {
        u8 subpriority = 0;
        if (idArrayStart == SPRITE_ARR_ID_MOVE_SELECTOR1)
            subpriority = 1;

        for (i = 0; i < MOVE_SELECTOR_SPRITES_COUNT; i++)
        {
            spriteIds[i] = CreateSprite(&sMoveSelectorSpriteTemplate, i * 16 + 89, 40, subpriority);
            if (i == 0)
                StartSpriteAnim(&gSprites[spriteIds[i]], 4); // left
            else if (i == 9)
                StartSpriteAnim(&gSprites[spriteIds[i]], 5); // right, actually the same as left, but flipped
            else
                StartSpriteAnim(&gSprites[spriteIds[i]], 6); // middle

            gSprites[spriteIds[i]].callback = SpriteCB_MoveSelector;
            gSprites[spriteIds[i]].data[0] = idArrayStart;
            gSprites[spriteIds[i]].data[1] = 0;
        }
    }
}

static void SpriteCB_MoveSelector(struct Sprite *sprite)
{
    if (sprite->animNum > 3 && sprite->animNum < 7)
    {
        sprite->data[1] = (sprite->data[1] + 1) & 0x1F;
        if (sprite->data[1] > 24)
            sprite->invisible = TRUE;
        else
            sprite->invisible = FALSE;
    }
    else
    {
        sprite->data[1] = 0;
        sprite->invisible = FALSE;
    }

    if (sprite->data[0] == SPRITE_ARR_ID_MOVE_SELECTOR1)
        sprite->y2 = sMonSummaryScreen->firstMoveIndex * 16;
    else
        sprite->y2 = sMonSummaryScreen->secondMoveIndex * 16;
}

static void DestroyMoveSelectorSprites(u8 firstArrayId)
{
    u8 i;
    for (i = 0; i < MOVE_SELECTOR_SPRITES_COUNT; i++)
        DestroySpriteInArray(firstArrayId + i);
}

//HYDRA ---------------- Stat edit cursor (skills page) ----------------
// Reuses the move selector sprites and graphics. The selector sheet/palette are
// loaded for every page during init, and the sprite slots are only ever used on
// the move pages, so they are free to borrow here.
//
// Geometry derived from the skills page stat windows:
//   left  value window = tile (16,7) 6x6 -> x 128..176
//   right value window = tile (27,7) 3x6 -> x 216..240
//   3 rows of 16px, window top tile 7   -> y 56
// Exact cell bounds, taken from the window templates:
//   left  label window tiles 10..15 (x  80..128) + value window tiles 16..21 (x 128..176)
//         -> left cell  x  80..176 = 96px = 6 sprites
//   right label window tiles 22..26 (x 176..216) + value window tiles 27..29 (x 216..240)
//         -> right cell x 176..240 = 64px = 4 sprites
// Box edges in screen pixels, nudged from those raw cell bounds to sit correctly
// around the text. The box is built as: left cap at LEFT+8, middles every 16px,
// right cap placed by edge at RIGHT-8 (overlapping the last middle when needed).
// Positioning the right cap by edge rather than on a fixed 16px stride lets a box
// be any pixel width, so these edges can be nudged freely.
#define STAT_SEL_LEFT_COL_LEFT    82   // left column box spans x  82..178
#define STAT_SEL_LEFT_COL_RIGHT   178
#define STAT_SEL_RIGHT_COL_LEFT   174  // right column box spans x 174..240
#define STAT_SEL_RIGHT_COL_RIGHT  240
// Max box pieces. The stats cursor only needs 6, but the info page cursor is sized
// to the ability/nature text and can be wider. Capped by MOVE_SELECTOR_SPRITES_COUNT
// because these borrow that sprite array (10 slots => 160px widest box).
#define STAT_SEL_SPRITE_COUNT     MOVE_SELECTOR_SPRITES_COUNT
#define STAT_SEL_BASE_Y        65   // centre of first stat row (window top y=56, +8, nudged 1px down)
#define STAT_SEL_ROW_HEIGHT    16
#define NUM_EDITABLE_STATS     6

static void CreateStatSelectorSprites(void)
{
    u8 i;
    u8 *spriteIds = &sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MOVE_SELECTOR1];

    for (i = 0; i < STAT_SEL_SPRITE_COUNT; i++)
    {
        spriteIds[i] = CreateSprite(&sMoveSelectorSpriteTemplate, STAT_SEL_LEFT_COL_LEFT + 8 + i * 16, STAT_SEL_BASE_Y, 0);
        gSprites[spriteIds[i]].callback = SpriteCB_StatSelector;
        gSprites[spriteIds[i]].data[0] = i;
        gSprites[spriteIds[i]].data[1] = 0;
        gSprites[spriteIds[i]].data[2] = 1;
    }
    UpdateStatSelectorSprites();
}

static void UpdateStatSelectorSprites(void)
{
    u8 i;
    u8 *spriteIds = &sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MOVE_SELECTOR1];
    u8 stat = sMonSummaryScreen->selectedStat;
    bool8 rightCol = (stat >= 3);
    u8 row = stat % 3;
    u16 leftEdge  = rightCol ? STAT_SEL_RIGHT_COL_LEFT  : STAT_SEL_LEFT_COL_LEFT;
    u16 rightEdge = rightCol ? STAT_SEL_RIGHT_COL_RIGHT : STAT_SEL_LEFT_COL_RIGHT;
    // Enough 16px pieces to span the box, rounded up
    u8 count = (rightEdge - leftEdge + 15) / 16;
    //HYDRA Red cursor (anims 4/5/6 = left/right/middle) while just picking a stat;
    // blue cursor (anims 7/8/9, +3) once a stat is selected for editing. Frames 24/28
    // in move_select.png are the blue variant, so blue = red anim + 3.
    u8 leftAnim   = sMonSummaryScreen->statEditing ? 7 : 4;
    u8 rightAnim  = sMonSummaryScreen->statEditing ? 8 : 5;
    u8 middleAnim = sMonSummaryScreen->statEditing ? 9 : 6;

    for (i = 0; i < STAT_SEL_SPRITE_COUNT; i++)
    {
        struct Sprite *sprite = &gSprites[spriteIds[i]];

        // Re-sync the blink phase of every sprite whenever the cursor moves,
        // so the box always flashes as one piece.
        sprite->data[1] = 0;

        if (i >= count) // narrower column: park the spare sprite
        {
            sprite->data[2] = 0;
            sprite->invisible = TRUE;
            continue;
        }

        sprite->data[2] = 1;
        sprite->y = STAT_SEL_BASE_Y + row * STAT_SEL_ROW_HEIGHT;

        if (i == count - 1)
        {
            // Right cap is pinned to the right edge, overlapping the last middle
            // if the box is not an exact multiple of 16px wide.
            sprite->x = rightEdge - 8;
            StartSpriteAnim(sprite, rightAnim);
        }
        else if (i == 0)
        {
            sprite->x = leftEdge + 8;
            StartSpriteAnim(sprite, leftAnim);      // left cap
        }
        else
        {
            sprite->x = leftEdge + 8 + i * 16;
            StartSpriteAnim(sprite, middleAnim);      // middle
        }
    }
}

static void SpriteCB_StatSelector(struct Sprite *sprite)
{
    // Advance the blink counter for EVERY sprite, including one parked off the
    // narrower column. If a parked sprite stops counting it comes back out of
    // phase and the box flashes in pieces.
    sprite->data[1] = (sprite->data[1] + 1) & 0x1F;

    if (sprite->data[2] == 0)
    {
        sprite->invisible = TRUE;
        return;
    }

    // Solid while editing a value, blinking while just picking a field.
    if (sMonSummaryScreen->statEditing || sMonSummaryScreen->infoEditing)
        sprite->invisible = FALSE;
    else
        sprite->invisible = (sprite->data[1] > 24);
}

static void DestroyStatSelectorSprites(void)
{
    u8 i;
    for (i = 0; i < STAT_SEL_SPRITE_COUNT; i++)
        DestroySpriteInArray(SPRITE_ARR_ID_MOVE_SELECTOR1 + i);
}

static void SwitchToStatSelection(u8 taskId)
{
    sMonSummaryScreen->selectedStat = 0;
    sMonSummaryScreen->statEditing = FALSE;
    CreateStatSelectorSprites();
    gTasks[taskId].func = Task_HandleInput_StatSelect;
}

static void CloseStatSelectMode(u8 taskId)
{
    sMonSummaryScreen->statEditing = FALSE;
    DestroyStatSelectorSprites();
    gTasks[taskId].func = Task_HandleInput;
}

static void Task_HandleInput_StatSelect(u8 taskId)
{
    if (MenuHelpers_ShouldWaitForLinkRecv() == TRUE || gPaletteFade.active)
        return;

    if (JOY_NEW(DPAD_UP))
    {
        if (sMonSummaryScreen->selectedStat % 3 != 0)
        {
            sMonSummaryScreen->selectedStat--;
            PlaySE(SE_SELECT);
            UpdateStatSelectorSprites();
        }
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        if (sMonSummaryScreen->selectedStat % 3 != 2)
        {
            sMonSummaryScreen->selectedStat++;
            PlaySE(SE_SELECT);
            UpdateStatSelectorSprites();
        }
    }
    else if (JOY_NEW(DPAD_LEFT) || JOY_NEW(DPAD_RIGHT))
    {
        // 3 stats per column, so +3 wraps between the left and right columns
        sMonSummaryScreen->selectedStat = (sMonSummaryScreen->selectedStat + 3) % NUM_EDITABLE_STATS;
        PlaySE(SE_SELECT);
        UpdateStatSelectorSprites();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        // Start editing the highlighted stat
        PlaySE(SE_SELECT);
        sMonSummaryScreen->statEditing = TRUE;
        sMonSummaryScreen->limitBeeped = FALSE; //HYDRA fresh edit session, allow a limit beep
        UpdateStatSelectorSprites(); //HYDRA recolor the cursor blue for edit mode
        gTasks[taskId].func = Task_HandleInput_StatEdit;
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        CloseStatSelectMode(taskId);
    }
}

//HYDRA Display order of the stat cursor is HP, ATTACK, DEFENSE (left column) then
// SP.ATK, SP.DEF, SPEED (right column). The MON_DATA enum stores SPEED *before*
// SPATK/SPDEF, so these are listed by explicit name rather than by arithmetic --
// using "MON_DATA_HP_IV + stat" here would silently edit the wrong stats.
static const u32 sStatIvFields[NUM_EDITABLE_STATS] = {
    MON_DATA_HP_IV, MON_DATA_ATK_IV, MON_DATA_DEF_IV,
    MON_DATA_SPATK_IV, MON_DATA_SPDEF_IV, MON_DATA_SPEED_IV,
};

static const u32 sStatEvFields[NUM_EDITABLE_STATS] = {
    MON_DATA_HP_EV, MON_DATA_ATK_EV, MON_DATA_DEF_EV,
    MON_DATA_SPATK_EV, MON_DATA_SPDEF_EV, MON_DATA_SPEED_EV,
};

// Redraws just the six stat values. Unlike ShowMonSkillsInfo this touches no
// task state and does not rebuild the mode label, so it is safe to call from
// inside the edit loop.
static void RefreshEditedStatValues(void)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    struct Pokemon *mon = &sMonSummaryScreen->currentMon;

    FillWindowPixelBuffer(sMonSummaryScreen->windowIds[PSS_DATA_WINDOW_SKILLS_STATS_LEFT], 0);
    FillWindowPixelBuffer(sMonSummaryScreen->windowIds[PSS_DATA_WINDOW_SKILLS_STATS_RIGHT], 0);

    // Only IVs and EVs are editable, so those are the only two modes possible here
    if (sMonSummaryScreen->skillsPageMode == SUMMARY_SKILLS_MODE_IVS)
        Hyper_ExtractMonSkillIvData(mon, sum);
    else
        Hyper_ExtractMonSkillEvData(mon, sum);

    BufferLeftColumnIvEvStats();
    PrintLeftColumnStats();
    BufferRightColumnStats();
    PrintRightColumnStats();
}

// Applies delta to the highlighted stat, clamped to the legal range, and writes
// it straight to the Pokemon so the change is permanent (not just cosmetic).
static void AdjustSelectedStat(u8 taskId, s32 delta)
{
    bool8 isBox = sMonSummaryScreen->isBoxMon;
    bool8 editingIvs = (sMonSummaryScreen->skillsPageMode == SUMMARY_SKILLS_MODE_IVS);
    struct Pokemon *partyMon = &sMonSummaryScreen->monList.mons[sMonSummaryScreen->curMonIndex];
    struct BoxPokemon *boxMon = GetCurrentBoxmon();
    u32 field = editingIvs ? sStatIvFields[sMonSummaryScreen->selectedStat]
                           : sStatEvFields[sMonSummaryScreen->selectedStat];
    s32 cur, newVal, maxVal;
    u8 writeVal;

    cur = isBox ? GetBoxMonData(boxMon, field) : GetMonData(partyMon, field);

    if (editingIvs)
    {
        maxVal = MAX_PER_STAT_IVS; // 31
    }
    else
    {
        s32 i, total = 0;

        for (i = 0; i < NUM_EDITABLE_STATS; i++)
            total += isBox ? GetBoxMonData(boxMon, sStatEvFields[i]) : GetMonData(partyMon, sStatEvFields[i]);

        // How high this stat may go without pushing the 510 total over, then
        // clamped to the 252 per-stat cap.
        maxVal = MAX_TOTAL_EVS - (total - cur);
        if (maxVal > MAX_PER_STAT_EVS)
            maxVal = MAX_PER_STAT_EVS;
        if (maxVal < 0)
            maxVal = 0;
    }

    newVal = cur + delta;
    if (newVal < 0)
        newVal = 0;
    if (newVal > maxVal)
        newVal = maxVal;

    if (newVal == cur) // already at a limit
    {
        //HYDRA Play the "can't go further" beep only once per hold, and never on top of
        // a still-playing beep, so holding/spamming a direction at the limit no longer
        // stacks the sound into distortion. limitBeeped is re-armed on release above.
        if (!sMonSummaryScreen->limitBeeped && !IsSEPlaying())
        {
            PlaySE(SE_FAILURE);
            sMonSummaryScreen->limitBeeped = TRUE;
        }
        return;
    }

    sMonSummaryScreen->limitBeeped = FALSE; //HYDRA value moved off the limit, re-arm the beep

    writeVal = newVal;

    // Write to the real Pokemon (monList points at the actual party/box data)
    if (isBox)
    {
        SetBoxMonData(boxMon, field, &writeVal);
    }
    else
    {
        SetMonData(partyMon, field, &writeVal);
        CalculateMonStats(partyMon);
    }

    // Keep the screen's working copy in sync so the display refreshes
    SetMonData(&sMonSummaryScreen->currentMon, field, &writeVal);
    CalculateMonStats(&sMonSummaryScreen->currentMon);

    PlaySE(SE_SELECT);

    // NOTE: deliberately NOT ShowMonSkillsInfo() here. That function ends with
    // "gTasks[taskId].func = Task_HandleInput", which would drop us out of edit
    // mode after a single keypress (Up/Down would then switch Pokemon instead of
    // changing the value). It also rebuilds the IVS/EVS label and does a full
    // tilemap VRAM copy, neither of which is needed when only a value changed.
    RefreshEditedStatValues();

    // Belt and braces: guarantee we stay in edit mode no matter what.
    gTasks[taskId].func = Task_HandleInput_StatEdit;
}

//HYDRA ---------------- HYPER PC badge gates ----------------
// The decoration is buyable early (Slateport, once you have Secret Power) but
// starts out only able to rename Pokemon. Each editing feature unlocks with a
// badge, so the tool grows with the run instead of handing over perfect stats
// the moment it is bought.
bool32 HyperPC_CanEditNature(void)
{
    return FlagGet(FLAG_BADGE03_GET);
}

bool32 HyperPC_CanEditEvs(void)
{
    return FlagGet(FLAG_BADGE05_GET);
}

bool32 HyperPC_CanEditIvs(void)
{
    return FlagGet(FLAG_BADGE07_GET);
}

bool32 HyperPC_CanEditAbility(void)
{
    return FlagGet(FLAG_BADGE08_GET);
}

// Nature is the first unlock, so it doubles as "is anything unlocked yet".
bool32 HyperPC_CanEditAnything(void)
{
    return HyperPC_CanEditNature() || HyperPC_CanEditEvs()
        || HyperPC_CanEditIvs()    || HyperPC_CanEditAbility();
}

// TRUE if the stat mode currently being viewed can actually be edited.
static bool32 HyperPC_CanEditSkillsMode(u8 mode)
{
    if (mode == SUMMARY_SKILLS_MODE_IVS)
        return HyperPC_CanEditIvs();
    if (mode == SUMMARY_SKILLS_MODE_EVS)
        return HyperPC_CanEditEvs();
    return FALSE; // Stats are never editable
}

//HYDRA ---------------- Info page ability / nature editor ----------------
// Same borrowed move-selector sprites as the stats cursor; the move pages are the
// only place those are otherwise used, so they are free here too.
//
// Boxes derived from the info page windows:
//   ability window = tile (11,9)  -> px (88,72), name printed on its first line
//   memo window    = tile (11,14) -> px (88,112), nature is the first memo line
// The frame is sized to whatever text it is framing, so long ability names and
// short natures both get a snug box.
#define INFO_SEL_TEXT_X      88   // both the ability name and the nature name start here
#define INFO_SEL_PAD_LEFT    5    // frame extends this far left of the text
#define INFO_SEL_PAD_RIGHT   5    // ...and this far right of it
#define INFO_SEL_MAX_RIGHT   238  // never run off the edge of the screen
#define INFO_SEL_ABILITY_Y   80   // sprite centre row for the ability line
#define INFO_SEL_NATURE_Y    120  // sprite centre row for the nature line
#define INFO_FIELD_ABILITY   0
#define INFO_FIELD_NATURE    1
#define NUM_INFO_EDIT_FIELDS 2

static void UpdateInfoSelectorSprites(void)
{
    u8 i;
    u8 *spriteIds = &sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MOVE_SELECTOR1];
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    bool8 onAbility = (sMonSummaryScreen->infoSelectedField == INFO_FIELD_ABILITY);
    const u8 *text;
    u16 leftEdge = INFO_SEL_TEXT_X - INFO_SEL_PAD_LEFT;
    u16 rightEdge;
    u8 count;
    u16 y = onAbility ? INFO_SEL_ABILITY_Y : INFO_SEL_NATURE_Y;

    // Size the frame to the text it is framing
    if (onAbility)
        text = gAbilitiesInfo[GetAbilityBySpecies(sum->species, sum->abilityNum)].name;
    else
        text = gNaturesInfo[sum->mintNature].name;

    // letterSpacing -1 makes GetStringWidth use the font's real letter spacing.
    // Passing 0 forces zero spacing and under-measures, which made the box too narrow.
    rightEdge = INFO_SEL_TEXT_X + GetStringWidth(FONT_NORMAL, text, -1) + INFO_SEL_PAD_RIGHT;
    if (rightEdge > INFO_SEL_MAX_RIGHT)
        rightEdge = INFO_SEL_MAX_RIGHT;

    count = (rightEdge - leftEdge + 15) / 16;
    if (count > STAT_SEL_SPRITE_COUNT)
        count = STAT_SEL_SPRITE_COUNT;
    if (count < 2) // always need at least a left and a right cap
        count = 2;

    for (i = 0; i < STAT_SEL_SPRITE_COUNT; i++)
    {
        struct Sprite *sprite = &gSprites[spriteIds[i]];

        sprite->data[1] = 0; // resync blink so the box flashes as one piece

        if (i >= count)
        {
            sprite->data[2] = 0;
            sprite->invisible = TRUE;
            continue;
        }

        sprite->data[2] = 1;
        sprite->y = y;

        if (i == count - 1)
        {
            sprite->x = rightEdge - 8;
            StartSpriteAnim(sprite, 5);  // right cap
        }
        else if (i == 0)
        {
            sprite->x = leftEdge + 8;
            StartSpriteAnim(sprite, 4);  // left cap
        }
        else
        {
            sprite->x = leftEdge + 8 + i * 16;
            StartSpriteAnim(sprite, 6);  // middle
        }
    }
}

// Steps the highlighted field by delta and writes it straight to the Pokemon.
static void CycleSelectedInfoField(s32 delta)
{
    struct PokeSummary *sum = &sMonSummaryScreen->summary;
    bool8 isBox = sMonSummaryScreen->isBoxMon;
    struct Pokemon *partyMon = &sMonSummaryScreen->monList.mons[sMonSummaryScreen->curMonIndex];
    struct BoxPokemon *boxMon = GetCurrentBoxmon();
    u32 field;
    s32 cur, newVal;
    u8 writeVal;

    if (sMonSummaryScreen->infoSelectedField == INFO_FIELD_ABILITY)
    {
        s32 slot = sMonSummaryScreen->summary.abilityNum;
        s32 tries;

        field = MON_DATA_ABILITY_NUM;
        cur = slot;

        // Step to the next slot this species actually fills. Plenty of species
        // leave slot 1 empty but still have a hidden ability in slot 2, so a plain
        // modulo would land on ABILITY_NONE.
        for (tries = 0; tries < NUM_ABILITY_SLOTS; tries++)
        {
            slot = (slot + delta) % NUM_ABILITY_SLOTS;
            if (slot < 0)
                slot += NUM_ABILITY_SLOTS;
            if (gSpeciesInfo[sum->species].abilities[slot] != ABILITY_NONE)
                break;
        }
        newVal = slot;
    }
    else
    {
        // MON_DATA_HIDDEN_NATURE is an XOR modifier over the personality-derived
        // nature, so it always reads back a valid nature and setting it does not
        // disturb personality (which also drives gender, shininess and ability slot).
        field = MON_DATA_HIDDEN_NATURE;
        cur = sum->mintNature;
        newVal = (cur + delta) % NUM_NATURES;
        if (newVal < 0)
            newVal += NUM_NATURES;
    }

    if (newVal == cur) // only one legal value (e.g. single-ability species)
    {
        PlaySE(SE_FAILURE);
        return;
    }

    writeVal = newVal;

    if (isBox)
        SetBoxMonData(boxMon, field, &writeVal);
    else
        SetMonData(partyMon, field, &writeVal);

    SetMonData(&sMonSummaryScreen->currentMon, field, &writeVal);
    CalculateMonStats(&sMonSummaryScreen->currentMon);
    if (!isBox)
        CalculateMonStats(partyMon);

    // Mirror into the on-screen summary and redraw the affected text
    if (sMonSummaryScreen->infoSelectedField == INFO_FIELD_ABILITY)
    {
        sum->abilityNum = newVal;
        FillWindowPixelBuffer(sMonSummaryScreen->windowIds[PSS_DATA_WINDOW_INFO_ABILITY], PIXEL_FILL(0));
        PrintMonAbilityName();
        PrintMonAbilityDescription();
    }
    else
    {
        sum->mintNature = newVal;
        FillWindowPixelBuffer(sMonSummaryScreen->windowIds[PSS_DATA_WINDOW_INFO_MEMO], PIXEL_FILL(0));
        BufferMonTrainerMemo();
        PrintMonTrainerMemo();
    }

    // The new name is almost certainly a different width, so re-measure the frame.
    // Without this the box keeps whatever size it had when edit mode was entered.
    UpdateInfoSelectorSprites();

    PlaySE(SE_SELECT);
}

static void SwitchToInfoSelection(u8 taskId)
{
    // Ability needs the 8th badge and nature the 3rd, so start the cursor on
    // whichever is actually unlocked.
    sMonSummaryScreen->infoSelectedField =
        HyperPC_CanEditAbility() ? INFO_FIELD_ABILITY : INFO_FIELD_NATURE;
    sMonSummaryScreen->infoEditing = FALSE;
    CreateStatSelectorSprites(); // generic 16px box builder, shared with the stats cursor
    UpdateInfoSelectorSprites();
    gTasks[taskId].func = Task_HandleInput_InfoSelect;
}

static void CloseInfoSelectMode(u8 taskId)
{
    sMonSummaryScreen->infoEditing = FALSE;
    DestroyStatSelectorSprites();
    gTasks[taskId].func = Task_HandleInput;
}

static void Task_HandleInput_InfoSelect(u8 taskId)
{
    if (MenuHelpers_ShouldWaitForLinkRecv() == TRUE || gPaletteFade.active)
        return;

    if (JOY_NEW(DPAD_UP) || JOY_NEW(DPAD_DOWN))
    {
        // Only two fields, so toggling is enough -- but skip it unless BOTH are
        // unlocked, otherwise the cursor could land on a field you cannot edit.
        if (HyperPC_CanEditAbility() && HyperPC_CanEditNature())
        {
            sMonSummaryScreen->infoSelectedField ^= 1;
            PlaySE(SE_SELECT);
            UpdateInfoSelectorSprites();
        }
        else
        {
            PlaySE(SE_FAILURE);
        }
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        sMonSummaryScreen->infoEditing = TRUE;
        gTasks[taskId].func = Task_HandleInput_InfoEdit;
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        CloseInfoSelectMode(taskId);
    }
}

static void Task_HandleInput_InfoEdit(u8 taskId)
{
    if (MenuHelpers_ShouldWaitForLinkRecv() == TRUE || gPaletteFade.active)
        return;

    if (JOY_NEW(DPAD_UP) || JOY_NEW(DPAD_RIGHT))
    {
        CycleSelectedInfoField(1);
    }
    else if (JOY_NEW(DPAD_DOWN) || JOY_NEW(DPAD_LEFT))
    {
        CycleSelectedInfoField(-1);
    }
    else if (JOY_NEW(A_BUTTON) || JOY_NEW(B_BUTTON))
    {
        // Value is already written, so A just confirms and returns to picking a field
        PlaySE(SE_SELECT);
        sMonSummaryScreen->infoEditing = FALSE;
        gTasks[taskId].func = Task_HandleInput_InfoSelect;
    }
}

static void Task_HandleInput_StatEdit(u8 taskId)
{
    if (MenuHelpers_ShouldWaitForLinkRecv() == TRUE || gPaletteFade.active)
        return;

    //HYDRA Once no direction is held, arm the limit beep again so the next hold (or
    // the next press while spamming at a limit) can play one more error noise.
    if (!JOY_HELD(DPAD_UP | DPAD_DOWN | DPAD_LEFT | DPAD_RIGHT))
        sMonSummaryScreen->limitBeeped = FALSE;

    // JOY_REPEAT so a held direction keeps stepping the value
    if (JOY_REPEAT(DPAD_UP))
        AdjustSelectedStat(taskId, 1);
    else if (JOY_REPEAT(DPAD_DOWN))
        AdjustSelectedStat(taskId, -1);
    else if (JOY_REPEAT(DPAD_RIGHT))
        AdjustSelectedStat(taskId, 10);
    else if (JOY_REPEAT(DPAD_LEFT))
        AdjustSelectedStat(taskId, -10);
    else if (JOY_NEW(A_BUTTON) || JOY_NEW(B_BUTTON))
    {
        // Back to picking a stat. Values are already written, so nothing to commit.
        PlaySE(SE_SELECT);
        sMonSummaryScreen->statEditing = FALSE;
        UpdateStatSelectorSprites(); //HYDRA recolor the cursor back to red for selection mode
        gTasks[taskId].func = Task_HandleInput_StatSelect;
    }
}

static void SetMainMoveSelectorColor(u8 which)
{
    u8 i;
    u8 *spriteIds = &sMonSummaryScreen->spriteIds[SPRITE_ARR_ID_MOVE_SELECTOR1];

    which *= 3;
    for (i = 0; i < MOVE_SELECTOR_SPRITES_COUNT; i++)
    {
        if (i == 0)
            StartSpriteAnim(&gSprites[spriteIds[i]], which + 4);
        else if (i == 9)
            StartSpriteAnim(&gSprites[spriteIds[i]], which + 5);
        else
            StartSpriteAnim(&gSprites[spriteIds[i]], which + 6);
    }
}

static void KeepMoveSelectorVisible(u8 firstSpriteId)
{
    u8 i;
    u8 *spriteIds = &sMonSummaryScreen->spriteIds[firstSpriteId];

    for (i = 0; i < MOVE_SELECTOR_SPRITES_COUNT; i++)
    {
        gSprites[spriteIds[i]].data[1] = 0;
        gSprites[spriteIds[i]].invisible = FALSE;
    }
}

static inline bool32 ShouldShowMoveRelearner(void)
{
    return (P_SUMMARY_SCREEN_MOVE_RELEARNER
         && !sMonSummaryScreen->lockMovesFlag
         && sMonSummaryScreen->mode != SUMMARY_MODE_BOX_CURSOR
         && sMonSummaryScreen->hasRelearnableMoves
         && !InBattleFactory()
         && !InSlateportBattleTent());
}

static inline bool32 ShouldShowRename(void)
{
    return (P_SUMMARY_SCREEN_RENAME
         && !sMonSummaryScreen->lockMovesFlag
         && !sMonSummaryScreen->summary.isEgg
         && sMonSummaryScreen->mode != SUMMARY_MODE_BOX_CURSOR
         && !InBattleFactory()
         && !InSlateportBattleTent()
         && GetPlayerIDAsU32() == sMonSummaryScreen->summary.OTID);
}

static inline bool32 ShouldShowIvEvPrompt(void)
{
    if (P_SUMMARY_SCREEN_IV_EV_BOX_ONLY)
    {
        return (P_SUMMARY_SCREEN_IV_EV_INFO || FlagGet(P_FLAG_SUMMARY_SCREEN_IV_EV_INFO))
            && (sMonSummaryScreen->mode == SUMMARY_MODE_BOX || sMonSummaryScreen->mode == SUMMARY_MODE_BOX_CURSOR);
    }
    else if (!P_SUMMARY_SCREEN_IV_EV_BOX_ONLY)
    {
        return (P_SUMMARY_SCREEN_IV_EV_INFO || FlagGet(P_FLAG_SUMMARY_SCREEN_IV_EV_INFO));
    }
    return FALSE;
}

static inline void ShowUtilityPrompt(s16 mode)
{
    const u8* promptText = NULL;
    const u8* gText_SkillPageIvs = COMPOUND_STRING("IVs");
    const u8* gText_SkillPageEvs = COMPOUND_STRING("EVs");
    const u8* gText_SkillPageStats = COMPOUND_STRING("STATS");
    const u8* gText_Rename = COMPOUND_STRING("EDIT"); //HYDRA was "RENAME"

    if (sMonSummaryScreen->currPageIndex == PSS_PAGE_INFO)
    {
        //HYDRA Only advertise EDIT once nature or ability is actually unlocked
        // (3rd and 8th badge respectively); before that A does nothing here.
        if (HyperPC_CanEditNature() || HyperPC_CanEditAbility())
            promptText = gText_Rename;
        else
            promptText = gText_Cancel2;
    }
    else if (sMonSummaryScreen->currPageIndex == PSS_PAGE_SKILLS)
    {
        if (ShouldShowIvEvPrompt())
        {
            //HYDRA This used to show the NEXT mode in the cycle (a "press to
            // switch to X" hint), which read as the page being mislabelled.
            // Now it names the mode you are actually looking at.
            if (mode == SUMMARY_SKILLS_MODE_STATS)
                promptText = gText_SkillPageStats;
            else if (mode == SUMMARY_SKILLS_MODE_IVS)
                promptText = gText_SkillPageIvs;
            else if (mode == SUMMARY_SKILLS_MODE_EVS)
                promptText = gText_SkillPageEvs;
        }
    }
    else if (sMonSummaryScreen->currPageIndex == PSS_PAGE_BATTLE_MOVES
             || sMonSummaryScreen->currPageIndex == PSS_PAGE_CONTEST_MOVES)
    {
        if (mode == SUMMARY_MODE_SELECT_MOVE && !sMonSummaryScreen->lockMovesFlag)
            promptText = gText_Switch;
        else
            promptText = gText_Info;
    }

    if (promptText == NULL)
    {
        ClearWindowTilemap(PSS_LABEL_WINDOW_PROMPT_UTILITY);
        FillWindowPixelBuffer(PSS_LABEL_WINDOW_PROMPT_UTILITY, PIXEL_FILL(0));
        return;
    }

    FillWindowPixelBuffer(PSS_LABEL_WINDOW_PROMPT_UTILITY, PIXEL_FILL(0));
    PutWindowTilemap(PSS_LABEL_WINDOW_PROMPT_UTILITY);

    int stringXPos = GetStringRightAlignXOffset(FONT_NORMAL, promptText, 62);
    int iconXPos = stringXPos - 16;
    if (iconXPos < 0)
        iconXPos = 0;

    //HYDRA Remember where the A icon lands so the R sprite can be parked directly
    // to its left (see DrawSkillsRButtonIcon).
    sMonSummaryScreen->promptIconX = iconXPos;

    // A sits in its normal place, but is hidden on the skills page whenever the
    // mode on show cannot be edited -- Stats always, and IVs/EVs until their badge
    // is earned. No point advertising a button that does nothing.
    if (!(sMonSummaryScreen->currPageIndex == PSS_PAGE_SKILLS && !HyperPC_CanEditSkillsMode(mode)))
        PrintAOrBButtonIcon(PSS_LABEL_WINDOW_PROMPT_UTILITY, FALSE, iconXPos);

    PrintTextOnWindow(PSS_LABEL_WINDOW_PROMPT_UTILITY, promptText, stringXPos, 1, 0, 0);
}

static void UpdateRelearnPrompt(void)
{
    FillWindowPixelBuffer(PSS_LABEL_WINDOW_PROMPT_RELEARN, PIXEL_FILL(0));
    if (!sMonSummaryScreen->hasRelearnableMoves)
        return;

    const u8 *relearnText;
    if (P_ENABLE_MOVE_RELEARNERS || P_TM_MOVES_RELEARNER || FlagGet(P_FLAG_EGG_MOVES) || FlagGet(P_FLAG_TUTOR_MOVES))
        relearnText = sRelearnTexts[gMoveRelearnerState];
    else
        relearnText = sText_Relearn;

    s32 relearnTextXPos = GetStringRightAlignXOffset(FONT_SMALL, relearnText, TILE_WIDTH * sSummaryTemplate[PSS_LABEL_WINDOW_PROMPT_RELEARN].width);
    PrintTextOnWindowWithFont(PSS_LABEL_WINDOW_PROMPT_RELEARN, relearnText, relearnTextXPos, 4, 0, 0, FONT_SMALL);
}

//HYDRA Nickname/naming-screen callbacks removed. On this screen A now opens the
// ability/nature editor instead of the rename prompt, so nothing called them.
