Common_EventScript_FindItem::
	callnative GetItemBallIdAndAmountFromTemplate
	goto Item_pickup_popup_Ball
	end

Item_pickup_popup_Ball::
	lock
	faceplayer
	copyvar VAR_0x8004, VAR_RESULT
	copyvar VAR_0x8005, VAR_0x8009
	checkitemspace VAR_0x8004, VAR_0x8005
	goto_if_eq VAR_RESULT, FALSE, Item_pickup_popup_BagFull
	@ --- POPUP IMMEDIATELY ON PRESS A ---
	bufferitemnameplural STR_VAR_2, VAR_0x8004, VAR_0x8005
	callnative Item_pickup_popup    @ Show popup instantly
	playfanfare MUS_OBTAIN_ITEM     @ Start fanfare immediately too
	@ --- NOW PLAY THE ANIMATION ---
	callnative StartPokeboxOpenAnim
	waitstate
	removeobject VAR_LAST_TALKED
	additem VAR_0x8004, VAR_0x8005
	release
	end

Item_pickup_popup_BagFull::
	msgbox gText_TooBadBagIsFull, MSGBOX_DEFAULT
	release
	end
	