#ifndef GUARD_SHOP_CRITERIA_H
#define GUARD_SHOP_CRITERIA_H

void TryBuildDynamicShopItemList(const u16 **ogItemList, u16 *resultingTotal);
void TryFreeDynamicShopItemList(const u16 **ogItemList);
bool32 ShopCriteria_Vial(enum Item item); //HYDRA

// Add new Criterias below!

#endif // GUARD_SHOP_CRITERIA_H
