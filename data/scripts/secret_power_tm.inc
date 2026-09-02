@ HYDRA The Secret Power man was moved from Route 111 to Route 102 so the player
@ gets Secret Power (and secret bases / the HYPER PC) much earlier. Everything in
@ this file was renamed Route111_* -> Route102_* to match. Unlike the original,
@ the script sets its hide flag before removeobject so he stays gone on re-entry.

Route102_Text_MakingRoomUseTMToMakeYourOwn:
	.string "What's that?\n"
	.string "What am I doing?\p"
	.string "I'm thinking about making my own room\n"
	.string "here using a POKéMON move.\p"
	.string "I know! I'll give you this TM.\n"
	.string "Will you use it to make your own room?$"

Route102_Text_ExplainSecretPower:
	.string "Find a big tree that looks like it might\n"
	.string "drop some vines.\p"
	.string "Use SECRET POWER in front of the tree.\n"
	.string "Some vines should get free and drop\l"
	.string "down, so you can climb the tree.\p"
	.string "You'll find a lot of space for putting\n"
	.string "your favorite things.\p"
	.string "It'll be your own secret room…\n"
	.string "A SECRET BASE!\p"
	.string "You should make one, too.\p"
	.string "Another thing, you don't always have\n"
	.string "to make your SECRET BASE in a tree.\p"
	.string "Try using SECRET POWER on rock walls\n"
	.string "that have small indents in them.\p"
	.string "I'm going to look for other places, too.\n"
	.string "Okay, bye!$"

Route102_Text_DontWantThis:
	.string "Oh, you don't want this?\n"
	.string "If you change your mind, tell me, okay?$"

Route102_Text_DontHaveAnyRoom:
	.string "Oh, you don't have any room for this.\p"
	.string "I'll hold on to it, so come back for it\n"
	.string "another time, okay?$"

Route102_EventScript_SecretPowerMan::
	lock
	faceplayer
	msgbox Route102_Text_MakingRoomUseTMToMakeYourOwn, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, YES, Route102_EventScript_GiveSecretPower
	msgbox Route102_Text_DontWantThis, MSGBOX_DEFAULT
	release
	end

Route102_EventScript_GiveSecretPower::
	giveitem ITEM_TM_SECRET_POWER
	goto_if_eq VAR_RESULT, FALSE, Route102_EventScript_NoRoomForSecretPower
	msgbox Route102_Text_ExplainSecretPower, MSGBOX_DEFAULT
	closemessage
	setflag FLAG_RECEIVED_SECRET_POWER
	clearflag FLAG_HIDE_SLATEPORT_CITY_TM_SALESMAN
	setflag FLAG_HIDE_ROUTE_102_SECRET_POWER_MAN
	applymovement VAR_LAST_TALKED, Route102_Movement_SecretPowerManExit
	waitmovement 0
	removeobject VAR_LAST_TALKED
	release
	end

Route102_EventScript_NoRoomForSecretPower::
	msgbox Route102_Text_DontHaveAnyRoom, MSGBOX_DEFAULT
	release
	end

@ HYDRA Walk-off for the Route 102 Secret Power man: 1 down, then 15 left.
Route102_Movement_SecretPowerManExit:
	walk_down
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	walk_left
	step_end
