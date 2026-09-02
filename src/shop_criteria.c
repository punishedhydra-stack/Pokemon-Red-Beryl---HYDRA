#include "global.h"
#include "item.h"
#include "script.h"
#include "event_data.h"
#include "malloc.h"
#include "shop_criteria.h"

static EWRAM_DATA const u16 *sDynamicShopItemListRef = NULL;

// HYDRA - Vial upgrades offered at every normal Poke Mart (gated per item by ShopCriteria_Vial).
static const u16 sVialShopItems[] = { ITEM_SUPER_VIAL, ITEM_HYPER_VIAL, ITEM_MAX_VIAL, ITEM_MASTER_VIAL };

// Remove the UNUSED if you'll use the functions!
static UNUSED bool32 ShopCriteriaByBadgeCount(u32 count);
static UNUSED bool32 ShopCriteriaByFlag(u32 flagId);
static UNUSED bool32 ShopCriteriaByVar(u32 varId, u32 varValue);

void TryBuildDynamicShopItemList(const u16 **ogItemList, u16 *resultingTotal)
{
    sDynamicShopItemListRef = *ogItemList;

    // HYDRA - reserve extra room so the unlocked vial upgrade can be appended.
    u16 *list = AllocZeroed((*resultingTotal + ARRAY_COUNT(sVialShopItems) + 1) * sizeof(u16));
    u32 overallIdx = 0, idx = 0;

    while (idx < *resultingTotal)
    {
        enum Item item = sDynamicShopItemListRef[idx];

        if (IsItemShopCriteriaFulfilled(item))
        {
            list[overallIdx] = item;
            overallIdx++;
        }

        idx++;
    }

    // HYDRA - Offer the single unlocked vial upgrade at every normal Poke Mart.
    for (idx = 0; idx < ARRAY_COUNT(sVialShopItems); idx++)
    {
        u32 j;
        bool32 alreadyListed = FALSE;

        if (!IsItemShopCriteriaFulfilled(sVialShopItems[idx]))
            continue;

        for (j = 0; j < overallIdx; j++)
        {
            if (list[j] == sVialShopItems[idx])
            {
                alreadyListed = TRUE;
                break;
            }
        }

        if (!alreadyListed)
        {
            list[overallIdx] = sVialShopItems[idx];
            overallIdx++;
        }
    }

    list[overallIdx] = ITEM_NONE;

    *ogItemList = list;
    *resultingTotal = overallIdx;
}

void TryFreeDynamicShopItemList(const u16 **ogItemList)
{
    Free((u16 *)*ogItemList);
    *ogItemList = sDynamicShopItemListRef;
}

// Add new Criterias below!

static UNUSED bool32 ShopCriteriaByBadgeCount(u32 count)
{
    u32 badgeCount = 0;

    for (u32 badgeFlag = FLAG_BADGE01_GET; badgeFlag < FLAG_BADGE01_GET + NUM_BADGES; badgeFlag++)
    {
        if (FlagGet(badgeFlag))
            badgeCount++;
    }

    if (badgeCount >= count)
        return TRUE;

    return FALSE;
}

// These two below are somewhat identical to ShopCriteriaByBadgeCount
// but uses only one specific event var/flag check. Useful if you need
// a specific badge flag instead of just the badge total.

static UNUSED bool32 ShopCriteriaByFlag(u32 flagId)
{
    if (FlagGet(flagId))
        return TRUE;

    return FALSE;
}

static UNUSED bool32 ShopCriteriaByVar(u32 varId, u32 varValue)
{
    if (VarGet(varId) >= varValue)
        return TRUE;

    return FALSE;
}

// HYDRA - Show only the single next vial upgrade: gated by owned tier + badge count.
bool32 ShopCriteria_Vial(enum Item item)
{
    u32 owned = GetOwnedVialTier();

    switch (item)
    {
    case ITEM_SUPER_VIAL:
        return (owned == 1 && ShopCriteriaByBadgeCount(2));
    case ITEM_HYPER_VIAL:
        return (owned == 2 && ShopCriteriaByBadgeCount(4));
    case ITEM_MAX_VIAL:
        return (owned == 3 && ShopCriteriaByBadgeCount(7));
    case ITEM_MASTER_VIAL:
        return (owned == 4 && FlagGet(FLAG_SYS_GAME_CLEAR));
    default:
        return TRUE;
    }
}
