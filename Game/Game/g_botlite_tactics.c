#include "g_local.h"
#include "g_botlite.h"

botlite_tactic_t BotLite_SelectSkill1Tactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	return BotLite_SelectManagedCombatTactic( bot, clientNum, snapshot );
}

botlite_tactic_t BotLite_SelectSkill2Tactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	return BotLite_SelectManagedCombatTactic( bot, clientNum, snapshot );
}
