#include "g_local.h"
#include "g_botlite.h"

void BotLite_RunSkill3Search( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	BotLite_RunManagedSearch( bot, clientNum, snapshot );
}

botlite_tactic_t BotLite_SelectSkill3Tactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	return BotLite_SelectManagedCombatTactic( bot, clientNum, snapshot );
}

void BotLite_RunSkill3Tactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, botlite_tactic_t tactic ) {
	BotLite_RunManagedCombatTactic( bot, clientNum, snapshot, tactic );
}
