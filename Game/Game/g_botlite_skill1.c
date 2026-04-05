#include "g_local.h"
#include "g_botlite.h"

void BotLite_RunSkill1Search( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	BotLite_RunManagedSearch( bot, clientNum, snapshot );
}

void BotLite_RunSkill1Tactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, botlite_tactic_t tactic ) {
	BotLite_RunManagedCombatTactic( bot, clientNum, snapshot, tactic );
}
