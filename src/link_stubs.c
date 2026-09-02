// HYDRA: Stub definitions for single-player-only builds (FREE_LINK == TRUE).
// Holds the minimal link symbols surviving/deferred code references WITHOUT the
// large EWRAM buffers. Grows per stage. Empty translation unit when FREE_LINK is FALSE.
#include "global.h"
#include "main.h"
#include "task.h"
#include "pokemon.h"
#include "event_data.h"
#include "union_room.h"
#include "cable_club.h"
#include "trade.h"
#include "mystery_gift.h"
#include "mystery_gift_menu.h"
#include "mystery_event_script.h"
#include "event_scripts.h"
#include "link.h"
#include "link_rfu.h"
#include "constants/species.h"

#if FREE_LINK

// ---- Stage 2: overworld/menu link app layer (union room, cable club, mystery gift,
//      wonder news, eReader, wireless status). Removed modules -> minimal stubs. ----

// Data referenced by surviving/deferred dead paths (moved out of union_room.c)
EWRAM_DATA u8 gPlayerCurrActivity = 0;
EWRAM_DATA struct RfuGameCompatibilityData gRfuPartnerCompatibilityData = {0};
EWRAM_DATA enum Species gUnionRoomOfferedSpecies = SPECIES_NONE;
EWRAM_DATA enum Type gUnionRoomRequestedMonType = TYPE_NONE;

static u16 sHydraDummyQuestionnaire[NUM_QUESTIONNAIRE_WORDS];
static void Task_HydraLinkNull(u8 taskId) { DestroyTask(taskId); }

// union_room / cable_club
bool32 InUnionRoom(void) { return FALSE; }
void SetUsingUnionRoomStartMenu(void) {}
void InitUnionRoomChatRegisteredTexts(void) {}
u8 CreateTask_ReestablishCableClubLink(void) { return CreateTask(Task_HydraLinkNull, 0); }
void CreateTask_EnterCableClubSeat(TaskFunc followupFunc) {}
bool32 GetLinkTrainerCardColor(u8 linkPlayerIndex) { return FALSE; }
void Task_WaitForLinkPlayerConnection(u8 taskId) { DestroyTask(taskId); }
void Task_ReconnectWithLinkPlayers(u8 taskId) { DestroyTask(taskId); }


// mystery gift / mystery event / wonder news
void ClearMysteryGift(void) {}
u16 *GetQuestionnaireWordsPtr(void) { return sHydraDummyQuestionnaire; }
bool32 ValidateSavedWonderCard(void) { return FALSE; }
u16 MysteryGift_GetCardStat(u32 stat) { return 0; }
void MysteryGift_TryIncrementStat(u32 stat, u32 trainerId) {}
void CB2_MysteryGiftEReader(void) {}
void SetMysteryEventScriptStatus(u32 status) {}
u16 GetRecordMixingGift(void) { return 0; }
u16 WonderNews_GetRewardInfo(void) { return 0; }

// field specials defined in the removed modules (keep symbol, no-op; specials.inc index-stable)
void TryBecomeLinkLeader(void) {}
void TryJoinLinkGroup(void) {}
void RunUnionRoom(void) {}
void InitUnionRoom(void) {}
void Script_ResetUnionRoomTrade(void) {}
void TryBattleLinkup(void) {}
void TryTradeLinkup(void) {}
void TryRecordMixLinkup(void) {}
void ValidateMixingGameLanguage(void) {}
void TryBerryBlenderLinkup(void) {}
void TryContestGModeLinkup(void) {}
void TryContestEModeLinkup(void) {}
void CableClubSaveGame(void) {}
void CleanupLinkRoomState(void) {}
void ExitLinkRoom(void) {}
void PlayerEnteredTradeSeat(void) {}
void ColosseumPlayerSpotTriggered(void) {}
void Script_ShowLinkTrainerCard(void) {}
void TrySetBattleTowerLinkType(void) {}
void ShowWirelessCommunicationScreen(void) {}
bool16 BufferUnionRoomPlayerName(void) { return FALSE; }
void Script_StartWiredTrade(void) {}

// ==== Stage 3: link/RFU core function stubs (no-link-state returns) ====

// ---- Stage 3: link/RFU core data globals kept (gLink + gRfu DELETED for EWRAM) ----
static struct RfuGameData sHydraDummyRfuGameData; //HYDRA backing for GetHostRfuGameData stub
EWRAM_DATA u16 gBlockRecvBuffer[MAX_RFU_PLAYERS][BLOCK_BUFFER_SIZE / 2] = {};
EWRAM_DATA u8 gBlockSendBuffer[BLOCK_BUFFER_SIZE] = {};
COMMON_DATA u16 gLinkPartnersHeldKeys[6] = {0};
EWRAM_DATA struct LinkPlayer gLinkPlayers[MAX_RFU_PLAYERS] = {};
COMMON_DATA u32 gLinkStatus = 0;
EWRAM_DATA u16 gLinkType = 0;
EWRAM_DATA struct LinkPlayer gLocalLinkPlayer = {};
COMMON_DATA bool8 gReceivedRemoteLinkPlayers = 0;
ALIGNED(4) EWRAM_DATA u16 gRecvCmds[MAX_RFU_PLAYERS][CMD_LENGTH] = {0};
COMMON_DATA u16 gSendCmd[CMD_LENGTH] = {0};
COMMON_DATA u8 gShouldAdvanceLinkState = 0;
COMMON_DATA bool8 gWirelessCommType = 0;
EWRAM_DATA u8 gWirelessStatusIndicatorSpriteId = 0;

u8 BitmaskAllOtherLinkPlayers(void) { return 0; }
void CB2_LinkError(void) {}
void CheckLinkPlayersMatchSaved(void) {}
void CheckShouldAdvanceLinkState(void) {}
void ClearLinkCallback(void) {}
void ClearLinkCallback_2(void) {}
void ClearLinkRfuCallback(void) {}
void ClearRecvCommands(void) {}
void CloseLink(void) {}
void ConvertLinkPlayerName(struct LinkPlayer *player) {}
void CopyHostRfuGameDataAndUsername(struct RfuGameData *gameData, u8 *username) {}
void CreateTask_RfuIdle(void) {}
void CreateTask_RfuReconnectWithParent(const u8 *name, u16 trainerId) {}
void CreateWirelessStatusIndicatorSprite(u8 x, u8 y) {}
void DestroyTask_RfuIdle(void) {}
void DestroyWirelessStatusIndicatorSprite(void) {}
bool8 DoesLinkPlayerCountMatchSaved(void) { return 0; }
u8 GetBlockReceivedStatus(void) { return 0; }
struct RfuGameData *GetHostRfuGameData(void) { return &sHydraDummyRfuGameData; }
u8 GetLinkPlayerCount(void) { return 0; }
u8 GetLinkPlayerCountAsBitFlags(void) { return 0; }
u8 GetLinkPlayerCount_2(void) { return 0; }
u8 GetLinkPlayerDataExchangeStatusTimed(int minPlayers, int maxPlayers) { return 0; }
u8 GetLinkPlayerInfoFlags(s32 playerId) { return 0; }
u32 GetLinkPlayerTrainerId(u8 who) { return 0; }
u32 GetLinkRecvQueueLength(void) { return 0; }
u8 GetMultiplayerId(void) { return 0; }
void GetOtherPlayersInfoFlags(void) {}
u8 GetSavedLinkPlayerCountAsBitFlags(void) { return 0; }
u8 GetSavedPlayerCount(void) { return 0; }
bool8 GetSioMultiSI(void) { return 0; }
bool8 HandleLinkConnection(void) { return 0; }
bool8 HasLinkErrorOccurred(void) { return 0; }
bool32 HasTrainerLeftPartnersList(u16 trainerId, const u8 *name) { return 0; }
void InitRFU(void) {}
void InitializeRfuLinkManager_EnterUnionRoom(void) {}
void InitializeRfuLinkManager_JoinGroup(void) {}
void InitializeRfuLinkManager_LinkLeader(u32 groupMax) {}
bool8 IsLinkConnectionEstablished(void) { return 0; }
bool8 IsLinkMaster(void) { return 0; }
bool8 IsLinkPlayerDataExchangeComplete(void) { return TRUE; }
bool32 IsLinkRecvQueueAtOverworldMax(void) { return 0; }
bool8 IsLinkRfuTaskFinished(void) { return TRUE; }
bool8 IsLinkTaskFinished(void) { return TRUE; }
bool32 IsRfuCommunicatingWithAllChildren(void) { return 0; }
bool32 IsRfuRecvQueueEmpty(void) { return TRUE; }
bool32 IsSendingKeysToLink(void) { return 0; }
bool32 IsUnionRoomListenTaskActive(void) { return 0; }
bool8 IsWirelessAdapterConnected(void) { return 0; }
u32 LinkDummy_Return2(void) { return 0; }
void LinkRfu_CreateConnectionAsParent(void) {}
void LinkRfu_FatalError(void) {}
void LinkRfu_Shutdown(void) {}
void LinkRfu_StopManagerAndFinalizeSlots(void) {}
void LinkRfu_StopManagerBeforeEnteringChat(void) {}
void LinkVSync(void) {}
bool32 Link_AnyPartnersPlayingFRLG_JP(void) { return 0; }
bool32 Link_AnyPartnersPlayingRubyOrSapphire(void) { return 0; }
bool8 LmanAcceptSlotFlagIsNotZero(void) { return 0; }
void LoadWirelessStatusIndicatorSpriteGfx(void) {}
void OpenLink(void) {}
void OpenLinkTimed(void) {}
bool32 PlayerHasMetTrainerBefore(u16 id, u8 *name) { return 0; }
void RequestDisconnectSlotByTrainerNameAndId(const u8 *name, u16 id) {}
void ResetBlockReceivedFlag(u8 who) {}
void ResetBlockReceivedFlags(void) {}
void ResetHostRfuGameData(void) {}
void ResetLinkPlayerCount(void) {}
void ResetLinkPlayers(void) {}
void ResetSerial(void) {}
u8 RfuGetStatus(void) { return 0; }
bool32 RfuHasErrored(void) { return 0; }
void RfuSetErrorParams(u32 errorInfo) {}
void RfuSetIgnoreError(bool32 enable) {}
void RfuSetNormalDisconnectMode(void) {}
void RfuSetStatus(u8 status, u16 errorInfo) {}
bool32 RfuTryDisconnectLeavingChildren(void) { return 0; }
void RfuVSync(void) {}
void Rfu_DisconnectPlayerById(u32 playerIdx) {}
bool8 Rfu_GetCompatiblePlayerData(struct RfuGameData *gameData, u8 *username, u8 idx) { return 0; }
s32 Rfu_GetIndexOfNewestChild(u8 bits) { return 0; }
bool8 Rfu_GetWonderDistributorPlayerData(struct RfuGameData *gameData, u8 *username, u8 idx) { return 0; }
bool32 Rfu_IsPlayerExchangeActive(void) { return 0; }
void Rfu_SendPacket(void *data) {}
void Rfu_SetCloseLinkCallback(void) {}
u8 Rfu_SetLinkRecovery(bool32 enable) { return 0; }
void Rfu_SetLinkStandbyCallback(void) {}
void Rfu_StopPartnerSearch(void) {}
void SaveLinkPlayers(u8 playerCount) {}
void SaveLinkTrainerNames(void) {}
bool8 SendBlock(u8 unused, const void *src, u16 size) { return TRUE; }
bool8 SendBlockRequest(u8 blockReqType) { return TRUE; }
void SendLeaveGroupNotice(void) {}
void SendRfuStatusToPartner(u8 status, u16 trainerId, const u8 *name) {}
void SerialCB(void) {}
void SetBerryBlenderLinkCallback(void) {}
void SetCloseLinkCallback(void) {}
void SetCloseLinkCallbackAndType(u16 type) {}
void SetCloseLinkCallbackHandleJP(void) {}
void SetHostRfuGameData(u8 activity, u32 partnerInfo, bool32 startedActivity) {}
void SetHostRfuWonderFlags(bool32 hasNews, bool32 hasCard) {}
void SetLinkDebugValues(u32 seed, u32 flags) {}
void SetLinkStandbyCallback(void) {}
void SetLocalLinkPlayerId(u8 playerId) {}
void SetSuppressLinkErrorMessage(bool8 flag) {}
void SetTradeBoardRegisteredMonInfo(u32 type, enum Species species, u32 level) {}
void SetUnionRoomChatPlayerData(u32 numPlayers) {}
void SetWirelessCommType0(void) {}
void SetWirelessCommType1(void) {}
bool32 ShouldCheckForUnionRoom(void) { return 0; }
void StartSendingKeysToLink(void) {}
void StopUnionRoomLinkManager(void) {}
void Task_DestroySelf(u8 taskId) {}
void Timer3Intr(void) {}
void TryConnectToUnionRoomParent(const u8 *name, struct RfuGameData *parent, u8 activity) {}
void UpdateGameData_GroupLockedIn(bool8 startedActivity) {}
void UpdateGameData_SetActivity(u8 activity, u32 partnerInfo, bool32 startedActivity) {}
void UpdateWirelessStatusIndicatorSprite(void) {}
bool32 WaitRfuState(bool32 force) { return TRUE; }
u32 WaitSendRfuStatusToPartner(u16 trainerId, const u8 *name) { return 0; }
void WipeTrainerNameRecords(void) {}

u8 GetWirelessCommType(void) { return 0; } //HYDRA special (data/specials.inc)

#endif // FREE_LINK
