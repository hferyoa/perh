OldaleTown_PokemonCenter_1F_MapScripts::
	map_script MAP_SCRIPT_ON_TRANSITION, OldaleTown_PokemonCenter_1F_OnTransition
	map_script MAP_SCRIPT_ON_RESUME, CableClub_OnResume
	.byte 0

OldaleTown_PokemonCenter_1F_OnTransition:
	setrespawn HEAL_LOCATION_OLDALE_TOWN
	call Common_EventScript_UpdateBrineyLocation
	end

OldaleTown_PokemonCenter_1F_EventScript_Nurse::
	setvar VAR_0x800B, LOCALID_OLDALE_NURSE
	call Common_EventScript_PkmnCenterNurse
	waitmessage
	waitbuttonpress
	release
	end

OldaleTown_PokemonCenter_1F_EventScript_Gentleman::
	msgbox OldaleTown_PokemonCenter_1F_Text_TrainersCanUsePC, MSGBOX_NPC
	end

OldaleTown_PokemonCenter_1F_EventScript_Boy::
	msgbox OldaleTown_PokemonCenter_1F_Text_PokemonCentersAreGreat, MSGBOX_NPC
	end

OldaleTown_PokemonCenter_1F_EventScript_Girl::
	lock
	faceplayer
	goto_if_set FLAG_SYS_POKEDEX_GET, OldaleTown_PokemonCenter_1F_EventScript_WirelessClubAvailable
	msgbox OldaleTown_PokemonCenter_1F_Text_WirelessClubNotAvailable, MSGBOX_DEFAULT
	release
	end

OldaleTown_PokemonCenter_1F_EventScript_WirelessClubAvailable::
	msgbox OldaleTown_PokemonCenter_1F_Text_TradedInWirelessClub, MSGBOX_DEFAULT
	release
	end

OldaleTown_PokemonCenter_1F_Text_TrainersCanUsePC:
	.string "That PC in the corner there is\n"
	.string "for any POKéMON TRAINER to use.\p"
	.string "Naturally, that means you're welcome\n"
	.string "to use it, too.$"

OldaleTown_PokemonCenter_1F_Text_PokemonCentersAreGreat:
	.string "POKéMON CENTERS are great!\p"
	.string "You can use their services as much\n"
	.string "as you like, and it's all for free.\l"
	.string "You never have to worry!$"

OldaleTown_PokemonCenter_1F_Text_WirelessClubNotAvailable:
	.string "The POKéMON WIRELESS CLUB on\n"
	.string "the second floor was built recently.\p"
	.string "But they say they're still making\n"
	.string "adjustments.$"

OldaleTown_PokemonCenter_1F_Text_TradedInWirelessClub:
	.string "The POKéMON WIRELESS CLUB on\n"
	.string "the second floor was built recently.\p"
	.string "I traded POKéMON right away.$"

OldaleTown_PokemonCenter_1F_EventScript_WeaknessTrader::
	lock
	faceplayer
	checkflag FLAG_WEAKNESS_TRADE_DONE
	goto_if_set OldaleTown_PokemonCenter_1F_EventScript_WeaknessTrader_Done
	special WeaknessTradeSearch
	compare VAR_RESULT, FALSE
	goto_if_eq OldaleTown_PokemonCenter_1F_EventScript_WeaknessTrader_NoMatch
	msgbox OldaleTown_PokemonCenter_1F_Text_WeaknessTrader_Offer, MSGBOX_YESNO
	compare VAR_RESULT, NO
	goto_if_eq OldaleTown_PokemonCenter_1F_EventScript_WeaknessTrader_Declined
	special WeaknessTradeExecute
	setflag FLAG_WEAKNESS_TRADE_DONE
	msgbox OldaleTown_PokemonCenter_1F_Text_WeaknessTrader_Done, MSGBOX_DEFAULT
	release
	end

OldaleTown_PokemonCenter_1F_EventScript_WeaknessTrader_Done::
	msgbox OldaleTown_PokemonCenter_1F_Text_WeaknessTrader_AlreadyDone, MSGBOX_NPC
	end

OldaleTown_PokemonCenter_1F_EventScript_WeaknessTrader_NoMatch::
	msgbox OldaleTown_PokemonCenter_1F_Text_WeaknessTrader_NoMatch, MSGBOX_DEFAULT
	release
	end

OldaleTown_PokemonCenter_1F_EventScript_WeaknessTrader_Declined::
	msgbox OldaleTown_PokemonCenter_1F_Text_WeaknessTrader_Declined, MSGBOX_DEFAULT
	release
	end

OldaleTown_PokemonCenter_1F_Text_WeaknessTrader_Offer:
	.string "Oh! You have a {STR_VAR_1}!\p"
	.string "I could trade you a {STR_VAR_2}\n"
	.string "for it. Will that do?$"

OldaleTown_PokemonCenter_1F_Text_WeaknessTrader_Done:
	.string "Enjoy your new {STR_VAR_2}!$"

OldaleTown_PokemonCenter_1F_Text_WeaknessTrader_AlreadyDone:
	.string "We already made a trade.\n"
	.string "I hope your new POKéMON\l"
	.string "is treating you well!$"

OldaleTown_PokemonCenter_1F_Text_WeaknessTrader_NoMatch:
	.string "Hmm... I don't have anything\n"
	.string "that would suit you right now.$"

OldaleTown_PokemonCenter_1F_Text_WeaknessTrader_Declined:
	.string "No rush! Come back if you\n"
	.string "change your mind.$"