/*
=======================================================================

BOTS: roster for the create match flow

Reads the botsys registry (botsys/bots/bots.txt) and the styles
(botsys/archetypes/*.cfg), keeps the list of bots for the next match
in the archived cvar ui_botRoster, and hands it to the game through
the g_queuedBot cvars when the match starts.

Screens:
	UI_BotRosterMenu	fighters of the match, add / edit / remove
	UI_BotEditMenu		character, level and style of one bot

=======================================================================
*/

#include "ui_local.h"

#define BOTS_FILE_SIZE		8192
#define ROSTER_CVAR			"ui_botRoster"
#define ROSTER_NAMES_CVAR	"ui_botRosterNames"	// quoted name per slot, - for the bots.txt one

static uiBotDef_t		botDefs[UI_MAX_BOTDEFS];
static int				numBotDefs;
static uiBotStyle_t		botStyles[UI_MAX_STYLES];
static int				numBotStyles;
static uiRosterSlot_t	roster[UI_MAX_ROSTER];
static int				rosterCount;
static qboolean			botsLoaded;

static const char *skillNames[4] = { "", "Rookie", "Warrior", "Elite" };
static const char *skillLines[4][2] = {
	{ "", "" },
	{ "Melee, boost", "Slow block" },
	{ "Transforms", "Ki charge, dodge" },
	{ "Guard, specials", "Break limit" }
};
static const char *statLabels[6] = { "Aggression", "Rush", "Combos", "Specials", "Block", "Ranged" };

/*
=================
File helpers
=================
*/
static int UI_Bots_ReadFile( const char *path, char *buf, int size ) {
	fileHandle_t	f;
	int				len;

	len = trap_FS_FOpenFile( path, &f, FS_READ );
	if ( len <= 0 ) {
		buf[0] = 0;
		return 0;
	}
	if ( len >= size ) {
		len = size - 1;
	}
	trap_FS_Read( buf, len, f );
	buf[len] = 0;
	trap_FS_FCloseFile( f );
	return len;
}

// copies one line into out and returns the start of the next one
static char *UI_Bots_NextLine( char *p, char *out, int size ) {
	int n;

	n = 0;
	while ( *p && *p != '\n' && *p != '\r' ) {
		if ( n < size - 1 ) {
			out[n++] = *p;
		}
		p++;
	}
	out[n] = 0;
	while ( *p == '\n' || *p == '\r' ) {
		p++;
	}
	return p;
}

// value of key=value inside a line split by sep, empty when missing
static void UI_Bots_KeyValue( const char *line, const char *key, char sep, char *out, int size ) {
	const char	*p;
	int			keyLen;
	int			n;

	out[0] = 0;
	keyLen = strlen( key );
	p = line;
	while ( *p ) {
		while ( *p == ' ' || *p == '\t' || *p == sep ) {
			p++;
		}
		if ( !Q_stricmpn( p, key, keyLen ) && p[keyLen] == '=' ) {
			p += keyLen + 1;
			n = 0;
			while ( *p && *p != sep && *p != '\r' && *p != '\n' ) {
				if ( n < size - 1 ) {
					out[n++] = *p;
				}
				p++;
			}
			while ( n > 0 && ( out[n - 1] == ' ' || out[n - 1] == '\t' ) ) {
				n--;
			}
			out[n] = 0;
			return;
		}
		while ( *p && *p != sep ) {
			p++;
		}
	}
}

static int UI_Bots_SkillFromFile( const char *file ) {
	const char *p;

	for ( p = file; *p; p++ ) {
		if ( *p >= '1' && *p <= '3' ) {
			return *p - '0';
		}
	}
	return 2;
}

/*
=================
UI_Bots_LoadDefs

bot=<id>;name=<display>;player=<folder>;archetype=<file>;skill=<file>
=================
*/
static void UI_Bots_LoadDefs( void ) {
	static char	buf[BOTS_FILE_SIZE];
	char		line[256];
	char		value[64];
	char		*p;
	uiBotDef_t	*def;

	numBotDefs = 0;
	UI_Bots_ReadFile( "botsys/bots/bots.txt", buf, sizeof( buf ) );
	p = buf;
	while ( *p && numBotDefs < UI_MAX_BOTDEFS ) {
		p = UI_Bots_NextLine( p, line, sizeof( line ) );
		if ( line[0] == '#' || line[0] == '/' || !line[0] ) {
			continue;
		}
		UI_Bots_KeyValue( line, "bot", ';', value, sizeof( value ) );
		if ( !value[0] ) {
			continue;
		}
		def = &botDefs[numBotDefs];
		memset( def, 0, sizeof( *def ) );
		Q_strncpyz( def->id, value, sizeof( def->id ) );
		UI_Bots_KeyValue( line, "name", ';', def->name, sizeof( def->name ) );
		UI_Bots_KeyValue( line, "player", ';', def->player, sizeof( def->player ) );
		UI_Bots_KeyValue( line, "archetype", ';', def->archetype, sizeof( def->archetype ) );
		UI_Bots_KeyValue( line, "skill", ';', value, sizeof( value ) );
		def->skill = value[0] ? UI_Bots_SkillFromFile( value ) : 2;
		if ( !def->name[0] ) {
			Q_strncpyz( def->name, def->id, sizeof( def->name ) );
		}
		if ( !def->player[0] ) {
			Q_strncpyz( def->player, def->id, sizeof( def->player ) );
		}
		numBotDefs++;
	}
}

/*
=================
UI_Bots_LoadStyles
=================
*/
static float UI_Bots_StyleValue( const char *text, const char *key ) {
	char	line[128];
	char	value[32];
	char	*p;

	p = (char *)text;
	while ( *p ) {
		p = UI_Bots_NextLine( p, line, sizeof( line ) );
		UI_Bots_KeyValue( line, key, ';', value, sizeof( value ) );
		if ( value[0] ) {
			return atof( value );
		}
	}
	return 0.5f;
}

static void UI_Bots_LoadStyles( void ) {
	static char		list[2048];
	static char		text[2048];
	char			*name;
	int				count;
	int				i;
	int				len;
	uiBotStyle_t	*style;

	numBotStyles = 0;
	count = trap_FS_GetFileList( "botsys/archetypes", "cfg", list, sizeof( list ) );
	name = list;
	for ( i = 0; i < count && numBotStyles < UI_MAX_STYLES; i++ ) {
		len = strlen( name );
		style = &botStyles[numBotStyles];
		memset( style, 0, sizeof( *style ) );
		Q_strncpyz( style->file, name, sizeof( style->file ) );
		Q_strncpyz( style->label, name, sizeof( style->label ) );
		if ( len > 4 && !Q_stricmp( style->label + len - 4, ".cfg" ) ) {
			style->label[len - 4] = 0;
		}
		if ( style->label[0] >= 'a' && style->label[0] <= 'z' ) {
			style->label[0] += 'A' - 'a';
		}
		UI_Bots_ReadFile( va( "botsys/archetypes/%s", name ), text, sizeof( text ) );
		style->values[0] = UI_Bots_StyleValue( text, "aggression" );
		style->values[1] = UI_Bots_StyleValue( text, "rush_tendency" );
		style->values[2] = UI_Bots_StyleValue( text, "combo_commitment" );
		style->values[3] = UI_Bots_StyleValue( text, "special_tendency" );
		style->values[4] = UI_Bots_StyleValue( text, "block_tendency" );
		style->values[5] = UI_Bots_StyleValue( text, "ranged_bias" );
		numBotStyles++;
		name += len + 1;
	}

	// the file list comes in no particular order, show them sorted
	for ( i = 0; i < numBotStyles; i++ ) {
		for ( len = i + 1; len < numBotStyles; len++ ) {
			if ( Q_stricmp( botStyles[len].label, botStyles[i].label ) < 0 ) {
				uiBotStyle_t swap;
				swap = botStyles[i];
				botStyles[i] = botStyles[len];
				botStyles[len] = swap;
			}
		}
	}
}

static int UI_Bots_FindStyle( const char *file ) {
	int i;

	for ( i = 0; i < numBotStyles; i++ ) {
		if ( !Q_stricmp( botStyles[i].file, file ) || !Q_stricmp( botStyles[i].label, file ) ) {
			return i;
		}
	}
	for ( i = 0; i < numBotStyles; i++ ) {
		if ( !Q_stricmp( botStyles[i].file, "balanced.cfg" ) ) {
			return i;
		}
	}
	return 0;
}

static int UI_Bots_FindDef( const char *id ) {
	int i;

	for ( i = 0; i < numBotDefs; i++ ) {
		if ( !Q_stricmp( botDefs[i].id, id ) ) {
			return i;
		}
	}
	return -1;
}

/*
=================
Roster persistence: "id skill style id skill style ..."
=================
*/
static void UI_Bots_SaveRoster( void ) {
	char	buf[MAX_CVAR_VALUE_STRING];
	char	entry[64];
	int		i;

	buf[0] = 0;
	for ( i = 0; i < rosterCount; i++ ) {
		Com_sprintf( entry, sizeof( entry ), "%s%s %i %i", i ? " " : "",
			botDefs[roster[i].def].id, roster[i].skill, roster[i].style );
		if ( strlen( buf ) + strlen( entry ) >= sizeof( buf ) ) {
			break;
		}
		Q_strcat( buf, sizeof( buf ), entry );
	}
	trap_Cvar_Set( ROSTER_CVAR, buf );

	// names in their own cvar, the roster one is already near its size limit
	buf[0] = 0;
	for ( i = 0; i < rosterCount; i++ ) {
		Com_sprintf( entry, sizeof( entry ), "%s\"%s\"", i ? " " : "", roster[i].name[0] ? roster[i].name : "-" );
		if ( strlen( buf ) + strlen( entry ) >= sizeof( buf ) ) {
			break;
		}
		Q_strcat( buf, sizeof( buf ), entry );
	}
	trap_Cvar_Set( ROSTER_NAMES_CVAR, buf );
}

// the name shown and given to the bot
static const char *UI_Bots_SlotName( uiRosterSlot_t *slot ) {
	return slot->name[0] ? slot->name : botDefs[slot->def].name;
}

// names travel inside quotes on the command line and in info strings
static void UI_Bots_CleanName( char *name ) {
	char	*s, *d;

	for ( s = d = name; *s; s++ ) {
		if ( *s == '"' || *s == ';' || *s == '\\' || *s == '%' || (unsigned char)*s < ' ' ) {
			continue;
		}
		*d++ = *s;
	}
	*d = 0;
}

static void UI_Bots_AddDefault( int def ) {
	if ( def < 0 || def >= numBotDefs || rosterCount >= UI_MAX_ROSTER ) {
		return;
	}
	roster[rosterCount].def = def;
	roster[rosterCount].skill = botDefs[def].skill;
	roster[rosterCount].style = UI_Bots_FindStyle( botDefs[def].archetype );
	roster[rosterCount].name[0] = 0;
	rosterCount++;
}

static void UI_Bots_LoadRoster( void ) {
	char	buf[MAX_CVAR_VALUE_STRING];
	char	id[64];
	char	*p;
	char	*token;
	int		def;
	int		skill;
	int		style;

	rosterCount = 0;
	trap_Cvar_Register( NULL, ROSTER_CVAR, "", CVAR_ARCHIVE );
	trap_Cvar_VariableStringBuffer( ROSTER_CVAR, buf, sizeof( buf ) );

	if ( !buf[0] ) {
		// first run: two opponents straight from the registry
		UI_Bots_AddDefault( UI_Bots_FindDef( "vegeta" ) );
		UI_Bots_AddDefault( UI_Bots_FindDef( "piccolo" ) );
		if ( !rosterCount && numBotDefs ) {
			UI_Bots_AddDefault( 0 );
		}
		return;
	}

	p = buf;
	while ( rosterCount < UI_MAX_ROSTER ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			break;
		}
		Q_strncpyz( id, token, sizeof( id ) );
		skill = atoi( COM_Parse( &p ) );
		style = atoi( COM_Parse( &p ) );
		def = UI_Bots_FindDef( id );
		if ( def < 0 ) {
			continue;
		}
		roster[rosterCount].def = def;
		roster[rosterCount].skill = ( skill < 1 ) ? 1 : ( skill > 3 ? 3 : skill );
		roster[rosterCount].style = ( style < 0 || style >= numBotStyles ) ? UI_Bots_FindStyle( botDefs[def].archetype ) : style;
		roster[rosterCount].name[0] = 0;
		rosterCount++;
	}

	trap_Cvar_Register( NULL, ROSTER_NAMES_CVAR, "", CVAR_ARCHIVE );
	trap_Cvar_VariableStringBuffer( ROSTER_NAMES_CVAR, buf, sizeof( buf ) );
	p = buf;
	for ( def = 0; def < rosterCount; def++ ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			break;
		}
		if ( strcmp( token, "-" ) ) {
			Q_strncpyz( roster[def].name, token, sizeof( roster[def].name ) );
		}
	}
}

/*
=================
Public access
=================
*/
void UI_Bots_Load( void ) {
	if ( botsLoaded ) {
		return;
	}
	UI_Bots_LoadDefs();
	UI_Bots_LoadStyles();
	UI_Bots_LoadRoster();
	botsLoaded = qtrue;
}

int UI_Bots_NumDefs( void ) {
	return numBotDefs;
}

uiBotDef_t *UI_Bots_Def( int index ) {
	if ( index < 0 || index >= numBotDefs ) {
		return NULL;
	}
	return &botDefs[index];
}

int UI_Bots_NumStyles( void ) {
	return numBotStyles;
}

uiBotStyle_t *UI_Bots_Style( int index ) {
	if ( index < 0 || index >= numBotStyles ) {
		return NULL;
	}
	return &botStyles[index];
}

int UI_Bots_RosterCount( void ) {
	return rosterCount;
}

uiRosterSlot_t *UI_Bots_RosterSlot( int index ) {
	if ( index < 0 || index >= rosterCount ) {
		return NULL;
	}
	return &roster[index];
}

const char *UI_Bots_SkillName( int skill ) {
	if ( skill < 1 || skill > 3 ) {
		return "";
	}
	return skillNames[skill];
}

// bots.txt names carry a [BOT] tag, titles read better without it
static const char *UI_Bots_DisplayName( uiBotDef_t *def ) {
	if ( !Q_stricmpn( def->name, "[BOT]", 5 ) && def->name[5] ) {
		return def->name + 5;
	}
	return def->name;
}

qhandle_t UI_Bots_Portrait( const char *player ) {
	char model[MAX_QPATH];
	char *slash;

	Q_strncpyz( model, player, sizeof( model ) );
	slash = strchr( model, '/' );
	if ( slash ) {
		*slash = 0;
	}
	return trap_R_RegisterShaderNoMip( va( "players/%s/icon_default.png", model ) );
}

/*
=================
UI_Bots_ExecAddCommands

The game adds the queued bots itself once the level is running
(G_SpawnQueuedBots), the client console cannot send addbot before
it is connected.
=================
*/
void UI_Bots_ExecAddCommands( void ) {
	int				i;
	uiBotStyle_t	*style;

	UI_Bots_Load();
	for ( i = 0; i < rosterCount; i++ ) {
		style = UI_Bots_Style( roster[i].style );
		trap_Cvar_Set( va( "g_queuedBot%i", i ), va( "%s %i %s \"%s\"", botDefs[roster[i].def].id,
			roster[i].skill, style ? style->file : "-", roster[i].name ) );
	}
	trap_Cvar_Set( "g_queuedBots", va( "%i", rosterCount ) );
}

static void UI_Bots_HumanInfo( char *name, int nameSize, char *model, int modelSize ) {
	char *slash;

	trap_Cvar_VariableStringBuffer( "name", name, nameSize );
	Q_CleanStr( name );
	trap_Cvar_VariableStringBuffer( "model", model, modelSize );
	slash = strchr( model, '/' );
	if ( slash ) {
		*slash = 0;
	}
}

/*
=======================================================================

ROSTER MENU (menu redesign, design canvas)

=======================================================================
*/

#define ID_ROSTER_ADD		10
#define ID_ROSTER_RANDOM	11
#define ID_ROSTER_CLEAR		12
#define ID_ROSTER_DONE		13
#define ID_ROSTER_EDIT		14
#define ID_ROSTER_ROW		20		// + slot
#define ID_ROSTER_REMOVE	40		// + slot
#define ID_ROSTER_NEWROW	60
#define ID_ROSTER_EDITROW	70		// + slot

// as the Bots artboard: the human, the bots and the add row share the list
// PC on the 1280x720 canvas; the phone list scrolls instead of shrinking
typedef struct {
	int		rowX, rowW, listY, listH, rowMax;
	int		colLevel, colStyle, colActions;
	int		nameSize, subSize, cellSize, buttonMax;
} rosterLayout_t;

static const rosterLayout_t rosterLayouts[2] = {
	{ 72, 732, 216, 370, 62,	412, 574, 716,	19, 15, 18, 32 },
	{ 34, 776, 114, 186, 62,	388, 516, 696,	16, 12, 15, 48 }
};

#define RL					( rosterLayouts[s_roster.menu.mobile ? 1 : 0] )
#define ROSTER_ROW_X		RL.rowX
#define ROSTER_ROW_W		RL.rowW
#define ROSTER_LIST_Y		RL.listY
#define ROSTER_LIST_H		RL.listH
#define ROSTER_ROW_MAX		RL.rowMax
#define ROSTER_COL_LEVEL	RL.colLevel
#define ROSTER_COL_STYLE	RL.colStyle
#define ROSTER_COL_ACTIONS	RL.colActions

typedef struct {
	menuframework_s	menu;
	menubitmap_s	rows[UI_MAX_ROSTER];
	menubitmap_s	removes[UI_MAX_ROSTER];
	menubitmap_s	edits[UI_MAX_ROSTER];
	menubitmap_s	newRow;
	menubitmap_s	add;
	menubitmap_s	random;
	menubitmap_s	clear;
	menubitmap_s	edit;
	menubitmap_s	done;
	int				shownSlot;		// bot shown in the info panel
	int				shownTime;		// when it changed, restarts the bars
	int				scroll;			// phone: first list row shown (0 is the human)
	playerInfo_t	playerinfo;
	char			shownModel[MAX_QPATH];
} rosterMenu_t;

static rosterMenu_t s_roster;

static void UI_BotRoster_Refresh( void );

static float UI_BotRoster_RowH( void ) {
	float	h;
	int		rows;

	if ( s_roster.menu.mobile ) {
		return ROSTER_ROW_MAX;
	}
	rows = rosterCount + 1 + ( rosterCount < UI_MAX_ROSTER ? 1 : 0 );
	h = (float)ROSTER_LIST_H / rows;
	return ( h > ROSTER_ROW_MAX ) ? ROSTER_ROW_MAX : h;
}

// one list row: portrait, name and a line under it, level, style
static void UI_BotRoster_DrawLine( float y, float h, qhandle_t portrait, const char *name, const char *sub,
	int skill, const char *style, float focus, int animIndex ) {
	vec4_t	left, right;
	float	size, x;

	if ( focus > 0.0f ) {
		left[0] = 0.33f; left[1] = 0.59f; left[2] = 1.0f; left[3] = 0.32f * focus;
		right[0] = 0.33f; right[1] = 0.59f; right[2] = 1.0f; right[3] = 0.04f * focus;
		UI_DRoundRowGlow( ROSTER_ROW_X, y, ROSTER_ROW_W, h, 5, left, right );
		left[0] = 0.47f; left[1] = 0.63f; left[2] = 1.0f; left[3] = 0.45f * focus;
		UI_DRoundEdge( ROSTER_ROW_X, y, ROSTER_ROW_W, h, 5, left );
	}
	size = h - 12;
	if ( size > 44 ) {
		size = 44;
	}
	UI_DPortrait( ROSTER_ROW_X + 12, y + ( h - size ) * 0.5f, size, portrait, qfalse, 0.0f );
	x = ROSTER_ROW_X + 24 + size;
	if ( h >= 42 ) {
		UI_DText( x, y + h * 0.5f - RL.nameSize - 1, name, RL.nameSize, UI_LEFT|UI_DROPSHADOW, zeq_white );
		UI_DText( x, y + h * 0.5f + 1, sub, RL.subSize, UI_LEFT, zeq_text3 );
	}
	else {
		UI_DText( x, y + h * 0.5f - 9, name, 17, UI_LEFT|UI_DROPSHADOW, zeq_white );
		UI_DText( x + UI_DTextWidth( name, 17 ) + 12, y + h * 0.5f - 7, sub, 14, UI_LEFT, zeq_text3 );
	}
	if ( skill > 0 ) {
		UI_DPips( ROSTER_COL_LEVEL, y + h * 0.5f - 4, skill, 3, UI_AnimFrac( uis.transitionTime, 120 + animIndex * 40, 400 ) );
		UI_DText( ROSTER_COL_LEVEL + 3 * RL.cellSize, y + h * 0.5f - RL.cellSize * 0.55f, UI_Bots_SkillName( skill ), RL.cellSize, UI_LEFT, zeq_white );
		UI_DText( ROSTER_COL_STYLE, y + h * 0.5f - RL.cellSize * 0.55f, style, RL.cellSize, UI_LEFT, zeq_white );
	}
	else {
		UI_DText( ROSTER_COL_LEVEL, y + h * 0.5f - RL.cellSize * 0.55f, "-", RL.cellSize, UI_LEFT, zeq_text3 );
		UI_DText( ROSTER_COL_STYLE, y + h * 0.5f - RL.cellSize * 0.55f, "-", RL.cellSize, UI_LEFT, zeq_text3 );
	}
}

// the 3D model of the info panel follows the shown bot
static void UI_Bots_SetPreview( playerInfo_t *pi, char *current, int size, const char *model ) {
	vec3_t viewangles;

	if ( !Q_stricmp( current, model ) ) {
		return;
	}
	Q_strncpyz( current, model, size );
	memset( pi, 0, sizeof( *pi ) );
	viewangles[YAW] = 180 - 25;
	viewangles[PITCH] = 0;
	viewangles[ROLL] = 0;
	UI_PlayerInfo_SetModel( pi, model );
	UI_PlayerInfo_SetInfo( pi, ANIM_IDLE, ANIM_IDLE, viewangles, vec3_origin, WP_NONE, qfalse );
}

static void UI_Bots_DrawPreview( playerInfo_t *pi, float x, float y, float w, float h ) {
	UI_DToV( &x, &y, &w, &h );
	UI_DrawPlayer( x, y, w, h, pi, uis.realtime / PLAYER_MODEL_SPEED );
}

static void UI_BotRoster_Show( int slot ) {
	if ( slot != s_roster.shownSlot ) {
		s_roster.shownSlot = slot;
		s_roster.shownTime = uis.realtime;
	}
}

static void UI_BotRoster_Remove( int slot ) {
	int i;

	if ( slot < 0 || slot >= rosterCount ) {
		return;
	}
	for ( i = slot; i < rosterCount - 1; i++ ) {
		roster[i] = roster[i + 1];
	}
	rosterCount--;
	UI_Bots_SaveRoster();
	UI_BotRoster_Refresh();
}

static void UI_BotRoster_ClearAction( qboolean result ) {
	if ( !result ) {
		return;
	}
	rosterCount = 0;
	UI_Bots_SaveRoster();
	UI_BotRoster_Refresh();
}

static void UI_BotRoster_Event( void *ptr, int event ) {
	int id;
	int def;

	id = ((menucommon_s *)ptr)->id;

	if ( event == QM_GOTFOCUS ) {
		if ( id >= ID_ROSTER_ROW && id < ID_ROSTER_ROW + UI_MAX_ROSTER ) {
			UI_BotRoster_Show( id - ID_ROSTER_ROW );
		}
		return;
	}
	if ( event != QM_ACTIVATED ) {
		return;
	}

	if ( id >= ID_ROSTER_ROW && id < ID_ROSTER_ROW + UI_MAX_ROSTER ) {
		UI_BotEditMenu( id - ID_ROSTER_ROW );
		return;
	}
	if ( id >= ID_ROSTER_REMOVE && id < ID_ROSTER_REMOVE + UI_MAX_ROSTER ) {
		UI_BotRoster_Remove( id - ID_ROSTER_REMOVE );
		return;
	}
	if ( id >= ID_ROSTER_EDITROW && id < ID_ROSTER_EDITROW + UI_MAX_ROSTER ) {
		UI_BotEditMenu( id - ID_ROSTER_EDITROW );
		return;
	}

	switch ( id ) {
	case ID_ROSTER_ADD:
	case ID_ROSTER_NEWROW:
		if ( rosterCount < UI_MAX_ROSTER ) {
			UI_BotEditMenu( -1 );
		}
		break;
	case ID_ROSTER_RANDOM:
		if ( rosterCount < UI_MAX_ROSTER && numBotDefs > 0 ) {
			def = ( rand() + uis.realtime ) % numBotDefs;
			UI_Bots_AddDefault( def );
			UI_Bots_SaveRoster();
			UI_BotRoster_Refresh();
			UI_BotRoster_Show( rosterCount - 1 );
		}
		break;
	case ID_ROSTER_CLEAR:
		// it emptied the list at once: ask first
		UI_ConfirmMenu( "REMOVE ALL BOTS?", 0, UI_BotRoster_ClearAction );
		break;
	case ID_ROSTER_EDIT:
		if ( s_roster.shownSlot >= 0 && s_roster.shownSlot < rosterCount ) {
			UI_BotEditMenu( s_roster.shownSlot );
		}
		break;
	case ID_ROSTER_DONE:
		UI_PopMenu();
		break;
	}
}

static void UI_BotRoster_DrawRow( void *self ) {
	menubitmap_s	*b;
	int				slot;
	float			y;
	float			focus;
	uiBotDef_t		*def;
	uiBotStyle_t	*style;

	b = (menubitmap_s *)self;
	slot = b->generic.id - ID_ROSTER_ROW;
	if ( slot < 0 || slot >= rosterCount ) {
		return;
	}
	def = &botDefs[roster[slot].def];
	style = UI_Bots_Style( roster[slot].style );
	y = b->generic.y;
	focus = UI_DFocus( &s_roster.menu, self );
	// the bot of the info panel stays lit (the phone has no info panel)
	if ( slot == s_roster.shownSlot && focus <= 0.0f && !s_roster.menu.mobile ) {
		focus = 0.6f;
	}
	UI_BotRoster_DrawLine( y, b->height, UI_Bots_Portrait( def->player ), UI_Bots_SlotName( &roster[slot] ),
		va( "%s  -  %s", def->player, UI_Bots_DisplayName( def ) ), roster[slot].skill, style ? style->label : "", focus, slot );
}

static void UI_BotRoster_DrawRemove( void *self ) {
	menubitmap_s	*b;
	vec4_t			color;
	qboolean		hot;

	b = (menubitmap_s *)self;
	hot = UI_DFocus( &s_roster.menu, self ) > 0.0f;
	color[0] = 0.94f; color[1] = 0.63f; color[2] = 0.53f; color[3] = 1.0f;
	UI_DIconButton( b->generic.x, b->generic.y, b->width, UI_ICON_CLOSE, color, hot ? 1.0f : 0.0f );
}

static void UI_BotRoster_DrawEdit( void *self ) {
	menubitmap_s	*b;
	vec4_t			color;
	qboolean		hot;

	b = (menubitmap_s *)self;
	hot = UI_DFocus( &s_roster.menu, self ) > 0.0f;
	color[0] = 0.84f; color[1] = 0.87f; color[2] = 0.98f; color[3] = 1.0f;
	UI_DIconButton( b->generic.x, b->generic.y, b->width, UI_ICON_EDIT, color, hot ? 1.0f : 0.0f );
}

static void UI_BotRoster_DrawNewRow( void *self ) {
	menubitmap_s	*b;
	float			focus;
	vec4_t			edge;

	b = (menubitmap_s *)self;
	focus = UI_DFocus( &s_roster.menu, self );
	// the dashed .add row of the artboard, drawn with a soft solid border
	UI_DFocusRow( b->generic.x, b->generic.y, b->width, b->height, focus );
	edge[0] = 0.29f; edge[1] = 0.39f; edge[2] = 0.74f; edge[3] = 0.6f + 0.4f * focus;
	UI_DRoundEdge( b->generic.x, b->generic.y, b->width, b->height, 5, edge );
	edge[0] = 0.56f; edge[1] = 0.71f; edge[2] = 1.0f; edge[3] = 1.0f;
	UI_DIcon( b->generic.x + 14, b->generic.y + b->height * 0.5f - 9, 18, UI_ICON_PLUS, edge );
	UI_DText( b->generic.x + 44, b->generic.y + b->height * 0.5f - RL.cellSize * 0.55f, va( "Add a bot to slot %i", rosterCount + 2 ), RL.cellSize, UI_LEFT, edge );
}

static void UI_BotRoster_DrawButton( void *self ) {
	menubitmap_s	*b;
	float			focus;
	const char		*label;
	vec4_t			warn;

	b = (menubitmap_s *)self;
	focus = UI_DFocus( &s_roster.menu, self );
	switch ( b->generic.id ) {
	case ID_ROSTER_DONE:
		UI_DCta( b->generic.x, b->generic.y, b->width, b->height, "DONE", focus );
		return;
	case ID_ROSTER_CLEAR:
		warn[0] = 0.235f; warn[1] = 0.063f; warn[2] = 0.04f; warn[3] = 0.35f + 0.35f * focus;
		UI_DRoundFill( b->generic.x, b->generic.y, b->width, b->height, 4, warn );
		warn[0] = 0.94f; warn[1] = 0.63f; warn[2] = 0.53f; warn[3] = 0.5f + 0.5f * focus;
		UI_DRoundEdge( b->generic.x, b->generic.y, b->width, b->height, 4, warn );
		warn[3] = 1.0f;
		UI_DText( b->generic.x + b->width * 0.5f, b->generic.y + b->height * 0.5f - RL.cellSize * 0.55f,
			s_roster.menu.mobile ? "Remove all..." : "Remove all bots", RL.cellSize, UI_CENTER, warn );
		return;
	case ID_ROSTER_ADD:		label = "ADD BOT"; break;
	case ID_ROSTER_RANDOM:	label = "FILL RANDOM"; break;
	default:				label = "EDIT BOT"; break;
	}
	UI_DKiButton( b->generic.x, b->generic.y, b->width, b->height, label, NULL, focus, ( b->generic.flags & QMF_GRAYED ) != 0, qfalse );
}

static void UI_BotRoster_Draw( void ) {
	char			name[64];
	char			model[64];
	uiRosterSlot_t	*slot;
	uiBotDef_t		*def;
	uiBotStyle_t	*style;
	int				i;
	float			anim;
	vec4_t			line;
	static const char *statShort[6] = { "Aggression", "Rush", "Combos", "Specials", "Block", "Ranged" };
	static const char *hints[] = { "UD", "Fighter", "ENTER", "Edit", "DEL", "Remove", "ESC", "Back" };

	uis.menuamount = -1;
	uis.hideEarth = qtrue;

	if ( s_roster.menu.mobile ) {
		UI_DMobileHeader( "PLAY  >  CREATE MATCH  >", "FIGHTERS" );
		UI_DGlass( 26, 88, 792, 212 );
		UI_DProp( 44, 96, va( "IN THIS MATCH  -  %i OF 12", rosterCount + 1 ), 10, UI_LEFT, zeq_gold );
		if ( s_roster.scroll == 0 ) {
			UI_Bots_HumanInfo( name, sizeof( name ), model, sizeof( model ) );
			UI_BotRoster_DrawLine( ROSTER_LIST_Y, UI_BotRoster_RowH() - 4, UI_Bots_Portrait( model ), name, va( "%s  -  you", model ), 0, "", 0.0f, 0 );
		}
		if ( rosterCount + 2 > ROSTER_LIST_H / ROSTER_ROW_MAX ) {
			UI_DText( 800, 96, "drag the list for more", 11, UI_RIGHT, zeq_text3 );
		}
		Menu_Draw( &s_roster.menu );
		return;
	}

	UI_DHeader( "PLAY  >  CREATE MATCH  >", "FIGHTERS" );

	// list panel
	UI_DGlass( 48, 128, 780, 528 );
	UI_DSectionTitle( 72, 152, va( "IN THIS MATCH  -  %i OF 12", rosterCount + 1 ) );
	UI_DText( 804, 152, va( "1 human  -  %i bot%s", rosterCount, rosterCount == 1 ? "" : "s" ), 17, UI_RIGHT, zeq_text3 );
	UI_DText( 84, 182, "FIGHTER", 15, UI_LEFT, zeq_text3 );
	UI_DText( ROSTER_COL_LEVEL, 182, "LEVEL", 15, UI_LEFT, zeq_text3 );
	UI_DText( ROSTER_COL_STYLE, 182, "STYLE", 15, UI_LEFT, zeq_text3 );
	line[0] = 0.35f; line[1] = 0.51f; line[2] = 0.90f; line[3] = 0.4f;
	UI_DFill( 72, 206, 732, 1, line );

	// the human player, not editable here
	UI_Bots_HumanInfo( name, sizeof( name ), model, sizeof( model ) );
	UI_BotRoster_DrawLine( ROSTER_LIST_Y, UI_BotRoster_RowH() - 6, UI_Bots_Portrait( model ), name, va( "%s  -  you", model ),
		0, "", 0.0f, 0 );

	line[3] = 0.25f;
	UI_DFill( 72, 594, 732, 1, line );

	Menu_Draw( &s_roster.menu );

	// info panel of the shown bot
	UI_DGlass( 852, 128, 380, 528 );
	slot = UI_Bots_RosterSlot( s_roster.shownSlot );
	if ( !slot ) {
		UI_DSectionTitle( 876, 152, "NO BOTS" );
		UI_DTextWrapped( 876, 186, 332, "Add bots with ADD BOT, or let RANDOM pick one from the roster of botsys.", 19, zeq_text2 );
	}
	else {
		def = &botDefs[slot->def];
		style = UI_Bots_Style( slot->style );
		anim = UI_AnimFrac( s_roster.shownTime, 0, 450 );
		UI_Bots_SetPreview( &s_roster.playerinfo, s_roster.shownModel, sizeof( s_roster.shownModel ), def->player );
		line[0] = 0.33f; line[1] = 0.59f; line[2] = 1.0f; line[3] = 0.22f;
		UI_DRoundGlow( 972, 210, 140, 110, 60, line );
		trap_R_SetColor( NULL );
		UI_DPic( 952, 326, 180, 46, uis.blobShadow );
		UI_Bots_DrawPreview( &s_roster.playerinfo, 929, 140, 226, 214 );
		UI_DProp( 876, 364, UI_Bots_DisplayName( def ), 24, UI_LEFT|UI_DROPSHADOW, zeq_white );
		UI_DPips( 876, 404, slot->skill, 3, anim );
		UI_DText( 930, 396, va( "%s  -  %s style", UI_Bots_SkillName( slot->skill ), style ? style->label : "" ), 18, UI_LEFT, zeq_text2 );
		if ( style ) {
			for ( i = 0; i < 6; i++ ) {
				UI_DStatBarW( 876, 440 + i * 22, 332, 100, statShort[i], style->values[i], UI_AnimFrac( s_roster.shownTime, 40 * i, 420 ) );
			}
		}
	}

	UI_DHints( hints, 4 );
}

static sfxHandle_t UI_BotRoster_Key( int key ) {
	menucommon_s *item;

	// phone: the wheel and a dragging finger scroll the list
	if ( s_roster.menu.mobile && ( key == K_MWHEELUP || key == K_MWHEELDOWN ) ) {
		s_roster.scroll += ( key == K_MWHEELUP ) ? -1 : 1;
		UI_BotRoster_Refresh();
		return menu_null_sound;
	}

	if ( key == K_DEL || key == K_BACKSPACE || key == K_KP_DEL ) {
		item = (menucommon_s *)Menu_ItemAtCursor( &s_roster.menu );
		if ( item && item->id >= ID_ROSTER_ROW && item->id < ID_ROSTER_ROW + UI_MAX_ROSTER ) {
			UI_BotRoster_Remove( item->id - ID_ROSTER_ROW );
			return menu_out_sound;
		}
	}
	return Menu_DefaultKey( &s_roster.menu, key );
}

static void UI_BotRoster_Refresh( void ) {
	int		i;
	int		visible;
	float	rowH;
	float	button;
	float	y;

	UI_DCanvas( s_roster.menu.mobile );
	rowH = UI_BotRoster_RowH();
	button = rowH - 6 - 8;
	if ( button > RL.buttonMax ) {
		button = RL.buttonMax;
	}
	// phone: rows 0 (the human) to rosterCount + 1 (add) in a window of three
	visible = s_roster.menu.mobile ? ROSTER_LIST_H / ROSTER_ROW_MAX : UI_MAX_ROSTER + 2;
	if ( s_roster.scroll > rosterCount + 2 - visible ) {
		s_roster.scroll = rosterCount + 2 - visible;
	}
	if ( s_roster.scroll < 0 || !s_roster.menu.mobile ) {
		s_roster.scroll = 0;
	}
	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		if ( i < rosterCount && i + 1 >= s_roster.scroll && i + 1 < s_roster.scroll + visible ) {
			y = ROSTER_LIST_Y + ( i + 1 - s_roster.scroll ) * rowH;
			// the row ends before its buttons so each takes its own clicks
			UI_DItem( &s_roster.rows[i], ROSTER_ROW_X, y, ROSTER_COL_ACTIONS - 8 - ROSTER_ROW_X, rowH - 6 );
			UI_DItem( &s_roster.edits[i], ROSTER_COL_ACTIONS, y + ( rowH - 6 - button ) * 0.5f, button, button );
			UI_DItem( &s_roster.removes[i], ROSTER_COL_ACTIONS + button + 8, y + ( rowH - 6 - button ) * 0.5f, button, button );
			s_roster.rows[i].generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
			s_roster.edits[i].generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
			s_roster.removes[i].generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		}
		else {
			s_roster.rows[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
			s_roster.edits[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
			s_roster.removes[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		}
	}

	// the add row sits right under the last bot
	if ( rosterCount < UI_MAX_ROSTER && rosterCount + 1 < s_roster.scroll + visible ) {
		s_roster.newRow.generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		UI_DItem( &s_roster.newRow, ROSTER_ROW_X, ROSTER_LIST_Y + ( rosterCount + 1 - s_roster.scroll ) * rowH, ROSTER_ROW_W, rowH - 6 );
		s_roster.add.generic.flags &= ~QMF_GRAYED;
		s_roster.random.generic.flags &= ~QMF_GRAYED;
	}
	else if ( rosterCount < UI_MAX_ROSTER ) {
		// scrolled out of the phone window
		s_roster.newRow.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		s_roster.add.generic.flags &= ~QMF_GRAYED;
		s_roster.random.generic.flags &= ~QMF_GRAYED;
	}
	else {
		s_roster.newRow.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		s_roster.add.generic.flags |= QMF_GRAYED;
		s_roster.random.generic.flags |= QMF_GRAYED;
	}
	if ( rosterCount ) {
		s_roster.edit.generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		s_roster.clear.generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		if ( s_roster.menu.mobile ) {
			// the info panel and its EDIT BOT are not on the phone: a row edits
			s_roster.edit.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		}
	}
	else {
		s_roster.edit.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		s_roster.clear.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
	}

	if ( s_roster.shownSlot >= rosterCount ) {
		s_roster.shownSlot = rosterCount - 1;
		s_roster.shownTime = uis.realtime;
	}

	// keep the cursor on something that still exists
	if ( s_roster.menu.cursor >= 0 && s_roster.menu.cursor < s_roster.menu.nitems &&
		( ((menucommon_s *)s_roster.menu.items[s_roster.menu.cursor])->flags & ( QMF_INACTIVE|QMF_GRAYED ) ) ) {
		Menu_SetCursorToItem( &s_roster.menu, rosterCount < UI_MAX_ROSTER ? (void *)&s_roster.newRow : (void *)&s_roster.done );
	}
}

static void UI_BotRoster_DItem( menubitmap_s *b, int id, void (*draw)( void * ), int x, int y, int w, int h, int flags ) {
	b->generic.type			= MTYPE_BITMAP;
	b->generic.flags		= QMF_NODEFAULTINIT|flags;
	b->generic.id			= id;
	b->generic.callback		= UI_BotRoster_Event;
	b->generic.ownerdraw	= draw;
	UI_DItem( b, x, y, w, h );
}

void UI_BotRosterMenu( void ) {
	int i;

	UI_Bots_Load();

	memset( &s_roster, 0, sizeof( s_roster ) );
	s_roster.menu.draw = UI_BotRoster_Draw;
	s_roster.menu.key = UI_BotRoster_Key;
	s_roster.menu.fullscreen = qtrue;
	s_roster.menu.wrapAround = qtrue;
	s_roster.menu.mobile = UI_MobileLayout();
	UI_DCanvas( s_roster.menu.mobile );
	s_roster.shownSlot = rosterCount ? 0 : -1;
	s_roster.shownTime = uis.realtime;

	// rows and their buttons are placed by UI_BotRoster_Refresh
	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		UI_BotRoster_DItem( &s_roster.rows[i], ID_ROSTER_ROW + i, UI_BotRoster_DrawRow, 0, 0, 1, 1, 0 );
		UI_BotRoster_DItem( &s_roster.edits[i], ID_ROSTER_EDITROW + i, UI_BotRoster_DrawEdit, 0, 0, 1, 1, QMF_MOUSEONLY );
		UI_BotRoster_DItem( &s_roster.removes[i], ID_ROSTER_REMOVE + i, UI_BotRoster_DrawRemove, 0, 0, 1, 1, QMF_MOUSEONLY );
	}
	UI_BotRoster_DItem( &s_roster.newRow, ID_ROSTER_NEWROW, UI_BotRoster_DrawNewRow, 0, 0, 1, 1, 0 );
	if ( s_roster.menu.mobile ) {
		UI_BotRoster_DItem( &s_roster.add, ID_ROSTER_ADD, UI_BotRoster_DrawButton, 26, 312, 170, 52, 0 );
		UI_BotRoster_DItem( &s_roster.random, ID_ROSTER_RANDOM, UI_BotRoster_DrawButton, 204, 312, 196, 52, 0 );
		UI_BotRoster_DItem( &s_roster.clear, ID_ROSTER_CLEAR, UI_BotRoster_DrawButton, 410, 312, 170, 52, 0 );
		UI_BotRoster_DItem( &s_roster.edit, ID_ROSTER_EDIT, UI_BotRoster_DrawButton, 0, 0, 1, 1, 0 );
		UI_BotRoster_DItem( &s_roster.done, ID_ROSTER_DONE, UI_BotRoster_DrawButton, 664, 312, 154, 52, 0 );
		UI_DBackItem( &s_roster.menu );
	}
	else {
		UI_BotRoster_DItem( &s_roster.add, ID_ROSTER_ADD, UI_BotRoster_DrawButton, 72, 606, 170, 42, 0 );
		UI_BotRoster_DItem( &s_roster.random, ID_ROSTER_RANDOM, UI_BotRoster_DrawButton, 252, 606, 200, 42, 0 );
		UI_BotRoster_DItem( &s_roster.clear, ID_ROSTER_CLEAR, UI_BotRoster_DrawButton, 634, 609, 170, 36, 0 );
		UI_BotRoster_DItem( &s_roster.edit, ID_ROSTER_EDIT, UI_BotRoster_DrawButton, 876, 596, 170, 42, 0 );
		UI_BotRoster_DItem( &s_roster.done, ID_ROSTER_DONE, UI_BotRoster_DrawButton, 1060 + UI_DExtra(), 664, 172, 46, 0 );
	}

	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		Menu_AddItem( &s_roster.menu, &s_roster.rows[i] );
		Menu_AddItem( &s_roster.menu, &s_roster.edits[i] );
		Menu_AddItem( &s_roster.menu, &s_roster.removes[i] );
	}
	Menu_AddItem( &s_roster.menu, &s_roster.newRow );
	Menu_AddItem( &s_roster.menu, &s_roster.add );
	Menu_AddItem( &s_roster.menu, &s_roster.random );
	Menu_AddItem( &s_roster.menu, &s_roster.clear );
	Menu_AddItem( &s_roster.menu, &s_roster.edit );
	Menu_AddItem( &s_roster.menu, &s_roster.done );

	UI_BotRoster_Refresh();
	UI_PushMenu( &s_roster.menu );
	uis.menuamount = -1;
	uis.hideEarth = qtrue;
	uis.showFrame = qfalse;

	// start on the first bot so the info panel has something to show
	if ( rosterCount ) {
		Menu_SetCursorToItem( &s_roster.menu, &s_roster.rows[0] );
	}
}

/*
=======================================================================

EDIT MENU: one bot (menu redesign, design canvas)

=======================================================================
*/

#define ID_EDIT_ACCEPT		10
#define ID_EDIT_CHAR		20		// + def
#define ID_EDIT_SKILL		60		// + skill
#define ID_EDIT_STYLE		70		// + style

#define ID_EDIT_NAME		90
#define ID_EDIT_DEFAULTS	91

// as the BotEdit artboard: two rows of four 76 px portraits (the phone:
// three rows of four 52 dp portraits)
#define EDIT_CELL_W			( s_edit.menu.mobile ? 62 : 93 )
#define EDIT_CELL_H			( s_edit.menu.mobile ? 78 : 107 )
#define EDIT_ROWS			( s_edit.menu.mobile ? 3 : 2 )

typedef struct {
	menuframework_s	menu;
	menubitmap_s	accept;
	menubitmap_s	chars[UI_MAX_BOTDEFS];
	menubitmap_s	skills[3];
	menubitmap_s	styles[UI_MAX_STYLES];
	menufield_s		name;
	menubitmap_s	defaults;
	int				barsY;			// under the style chips
	int				slot;			// roster slot, -1 for a new bot
	int				def;
	int				skill;
	int				style;
	qboolean		touched;		// level or style chosen by hand
	qboolean		live;			// editing a bot of the running match
	int				liveClient;		// its client number, -1 to add a new one
	const char		*acceptLabel;
	int				defTime;
	int				styleTime;
	int				charScroll;		// first row shown
	playerInfo_t	playerinfo;
	char			shownModel[MAX_QPATH];
} editMenu_t;

static editMenu_t s_edit;

/*
=================
UI_BotEdit_LayoutChars

the grid shows three rows of four; with more characters it scrolls by
rows, the arrows and the mouse wheel bring the rest in
=================
*/
static void UI_BotEdit_LayoutChars( void ) {
	int i;
	int row;

	for ( i = 0; i < numBotDefs; i++ ) {
		row = i / 4 - s_edit.charScroll;
		if ( row >= 0 && row < EDIT_ROWS ) {
			UI_DCanvas( s_edit.menu.mobile );
			if ( s_edit.menu.mobile ) {
				UI_DItem( &s_edit.chars[i], 42 + ( i % 4 ) * EDIT_CELL_W, 118 + row * EDIT_CELL_H, 58, 72 );
			}
			else {
				UI_DItem( &s_edit.chars[i], 72 + ( i % 4 ) * EDIT_CELL_W, 176 + row * EDIT_CELL_H, 85, 95 );
			}
			s_edit.chars[i].generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		}
		else {
			s_edit.chars[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		}
	}
}

static void UI_BotEdit_ScrollTo( int def ) {
	int row;

	row = def / 4;
	if ( row < s_edit.charScroll ) {
		s_edit.charScroll = row;
	}
	else if ( row > s_edit.charScroll + EDIT_ROWS - 1 ) {
		s_edit.charScroll = row - EDIT_ROWS + 1;
	}
	UI_BotEdit_LayoutChars();
}

static int UI_BotEdit_FreeDef( void ) {
	int def;
	int i;

	for ( def = 0; def < numBotDefs; def++ ) {
		for ( i = 0; i < rosterCount; i++ ) {
			if ( roster[i].def == def ) {
				break;
			}
		}
		if ( i == rosterCount ) {
			return def;
		}
	}
	return rosterCount % numBotDefs;
}

/*
=================
UI_BotEdit_Key

arrows move inside the character grid, the level cards and the style chips
=================
*/
static sfxHandle_t UI_BotEdit_Key( int key ) {
	menucommon_s	*item;
	int				id;
	int				step;

	item = (menucommon_s *)Menu_ItemAtCursor( &s_edit.menu );
	if ( !item ) {
		return Menu_DefaultKey( &s_edit.menu, key );
	}
	id = item->id;
	step = 0;

	// the wheel scrolls the character grid
	if ( ( key == K_MWHEELUP || key == K_MWHEELDOWN ) && numBotDefs > EDIT_ROWS * 4 ) {
		s_edit.charScroll += ( key == K_MWHEELUP ) ? -1 : 1;
		if ( s_edit.charScroll > ( numBotDefs + 3 ) / 4 - EDIT_ROWS ) {
			s_edit.charScroll = ( numBotDefs + 3 ) / 4 - EDIT_ROWS;
		}
		if ( s_edit.charScroll < 0 ) {
			s_edit.charScroll = 0;
		}
		UI_BotEdit_LayoutChars();
		if ( id >= ID_EDIT_CHAR && id < ID_EDIT_CHAR + numBotDefs && ( item->flags & QMF_HIDDEN ) ) {
			Menu_SetCursorToItem( &s_edit.menu, &s_edit.chars[s_edit.charScroll * 4] );
		}
		return menu_move_sound;
	}

	if ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) {
		step = -1;
	}
	else if ( key == K_RIGHTARROW || key == K_KP_RIGHTARROW ) {
		step = 1;
	}

	if ( id >= ID_EDIT_CHAR && id < ID_EDIT_CHAR + numBotDefs ) {
		if ( key == K_UPARROW || key == K_KP_UPARROW ) {
			step = -4;
		}
		else if ( key == K_DOWNARROW || key == K_KP_DOWNARROW ) {
			step = 4;
		}
		if ( step && id - ID_EDIT_CHAR + step >= 0 && id - ID_EDIT_CHAR + step < numBotDefs ) {
			UI_BotEdit_ScrollTo( id - ID_EDIT_CHAR + step );
			Menu_SetCursorToItem( &s_edit.menu, &s_edit.chars[id - ID_EDIT_CHAR + step] );
			return menu_move_sound;
		}
	}
	else if ( id >= ID_EDIT_SKILL && id < ID_EDIT_SKILL + 3 ) {
		if ( step && id - ID_EDIT_SKILL + step >= 0 && id - ID_EDIT_SKILL + step < 3 ) {
			Menu_SetCursorToItem( &s_edit.menu, &s_edit.skills[id - ID_EDIT_SKILL + step] );
			return menu_move_sound;
		}
	}
	else if ( id >= ID_EDIT_STYLE && id < ID_EDIT_STYLE + numBotStyles ) {
		if ( key == K_UPARROW || key == K_KP_UPARROW ) {
			step = -4;
		}
		else if ( key == K_DOWNARROW || key == K_KP_DOWNARROW ) {
			step = 4;
		}
		if ( step && id - ID_EDIT_STYLE + step >= 0 && id - ID_EDIT_STYLE + step < numBotStyles ) {
			Menu_SetCursorToItem( &s_edit.menu, &s_edit.styles[id - ID_EDIT_STYLE + step] );
			return menu_move_sound;
		}
	}
	return Menu_DefaultKey( &s_edit.menu, key );
}

static void UI_BotEdit_Event( void *ptr, int event ) {
	int id;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	id = ((menucommon_s *)ptr)->id;

	if ( id >= ID_EDIT_CHAR && id < ID_EDIT_CHAR + UI_MAX_BOTDEFS ) {
		if ( !s_edit.name.field.buffer[0] || !strcmp( s_edit.name.field.buffer, botDefs[s_edit.def].name ) ) {
			Q_strncpyz( s_edit.name.field.buffer, botDefs[id - ID_EDIT_CHAR].name, sizeof( s_edit.name.field.buffer ) );
			s_edit.name.field.cursor = strlen( s_edit.name.field.buffer );
		}
		s_edit.def = id - ID_EDIT_CHAR;
		s_edit.defTime = uis.realtime;
		if ( !s_edit.touched ) {
			s_edit.skill = botDefs[s_edit.def].skill;
			s_edit.style = UI_Bots_FindStyle( botDefs[s_edit.def].archetype );
			s_edit.styleTime = uis.realtime;
		}
		return;
	}
	if ( id >= ID_EDIT_SKILL && id < ID_EDIT_SKILL + 3 ) {
		s_edit.skill = id - ID_EDIT_SKILL + 1;
		s_edit.touched = qtrue;
		return;
	}
	if ( id >= ID_EDIT_STYLE && id < ID_EDIT_STYLE + UI_MAX_STYLES ) {
		s_edit.style = id - ID_EDIT_STYLE;
		s_edit.styleTime = uis.realtime;
		s_edit.touched = qtrue;
		return;
	}
	if ( id == ID_EDIT_DEFAULTS ) {
		// on: level and style come from bots.txt again
		if ( s_edit.touched ) {
			s_edit.skill = botDefs[s_edit.def].skill;
			s_edit.style = UI_Bots_FindStyle( botDefs[s_edit.def].archetype );
			s_edit.styleTime = uis.realtime;
			s_edit.touched = qfalse;
		}
		else {
			s_edit.touched = qtrue;
		}
		return;
	}

	if ( id == ID_EDIT_ACCEPT ) {
		UI_Bots_CleanName( s_edit.name.field.buffer );
		if ( !strcmp( s_edit.name.field.buffer, botDefs[s_edit.def].name ) ) {
			s_edit.name.field.buffer[0] = 0;		// the bots.txt one
		}
		if ( s_edit.live ) {
			// a bot loads its level and style when it joins: replace it
			if ( s_edit.liveClient >= 0 ) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "clientkick %i\n", s_edit.liveClient ) );
			}
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "addbot %s %i style=%s%s%s%s\n", botDefs[s_edit.def].id, s_edit.skill,
				UI_Bots_Style( s_edit.style ) ? UI_Bots_Style( s_edit.style )->file : "",
				s_edit.name.field.buffer[0] ? " \"name=" : "", s_edit.name.field.buffer, s_edit.name.field.buffer[0] ? "\"" : "" ) );
			UI_PopMenu();
			return;
		}
		if ( s_edit.slot < 0 ) {
			if ( rosterCount >= UI_MAX_ROSTER ) {
				return;
			}
			s_edit.slot = rosterCount++;
		}
		roster[s_edit.slot].def = s_edit.def;
		roster[s_edit.slot].skill = s_edit.skill;
		roster[s_edit.slot].style = s_edit.style;
		Q_strncpyz( roster[s_edit.slot].name, s_edit.name.field.buffer, sizeof( roster[s_edit.slot].name ) );
		UI_Bots_SaveRoster();
		UI_PopMenu();
		UI_BotRoster_Refresh();
		UI_BotRoster_Show( s_edit.slot );
	}
}

static void UI_BotEdit_DrawChar( void *self ) {
	menubitmap_s	*b;
	int				def;
	float			focus;
	qboolean		selected;

	b = (menubitmap_s *)self;
	def = b->generic.id - ID_EDIT_CHAR;
	focus = UI_DFocus( &s_edit.menu, self );
	selected = ( def == s_edit.def );
	if ( s_edit.menu.mobile ) {
		UI_DPortrait( b->generic.x + 3, b->generic.y, 52, UI_Bots_Portrait( botDefs[def].player ), selected, focus );
		UI_DText( b->generic.x + 29, b->generic.y + 56, botDefs[def].player, 11, UI_CENTER, ( selected || focus > 0.0f ) ? zeq_white : zeq_text3 );
		return;
	}
	UI_DPortrait( b->generic.x + 4.5f, b->generic.y, 76, UI_Bots_Portrait( botDefs[def].player ), selected, focus );
	UI_DText( b->generic.x + 42.5f, b->generic.y + 80, botDefs[def].player, 15, UI_CENTER,
		( selected || focus > 0.0f ) ? zeq_white : zeq_text2 );
}

static void UI_BotEdit_DrawSkill( void *self ) {
	menubitmap_s	*b;
	int				skill;
	float			focus;
	vec4_t			fill;
	vec4_t			edge;

	b = (menubitmap_s *)self;
	skill = b->generic.id - ID_EDIT_SKILL + 1;
	focus = UI_DFocus( &s_edit.menu, self );

	// the .lv card: rounded, the chosen one gold with a glow
	fill[0] = 0.024f; fill[1] = 0.03f; fill[2] = 0.12f; fill[3] = 0.55f;
	if ( skill == s_edit.skill ) {
		edge[0] = zeq_gold[0]; edge[1] = zeq_gold[1]; edge[2] = zeq_gold[2]; edge[3] = 0.3f;
		UI_DRoundGlow( b->generic.x, b->generic.y, b->width, b->height, 16, edge );
		edge[3] = 0.16f;
		UI_DRoundPanel( b->generic.x, b->generic.y, b->width, b->height, 6, edge, fill, 1.0f, fill );
		edge[3] = 1.0f;
	}
	else {
		UI_DRoundFill( b->generic.x, b->generic.y, b->width, b->height, 6, fill );
		edge[0] = 0.18f + 0.29f * focus; edge[1] = 0.27f + 0.36f * focus; edge[2] = 0.56f + 0.44f * focus; edge[3] = 1.0f;
	}
	UI_DRoundEdge( b->generic.x, b->generic.y, b->width, b->height, 6, edge );

	UI_DPips( b->generic.x + 12, b->generic.y + ( s_edit.menu.mobile ? 12 : 14 ), skill, 3, UI_AnimFrac( uis.transitionTime, 100 + skill * 60, 300 ) );
	UI_DProp( b->generic.x + 12, b->generic.y + ( s_edit.menu.mobile ? 28 : 30 ), va( "%s", skillNames[skill] ), s_edit.menu.mobile ? 13 : 14, UI_LEFT, zeq_white );
	if ( !s_edit.menu.mobile ) {
		UI_DTextWrapped( b->generic.x + 12, b->generic.y + 52, b->width - 22, va( "%s, %s", skillLines[skill][0], skillLines[skill][1] ), 16, zeq_text2 );
	}
}

static void UI_BotEdit_DrawStyle( void *self ) {
	menubitmap_s	*b;
	int				style;
	float			focus;
	vec4_t			fill;
	vec4_t			edge;

	b = (menubitmap_s *)self;
	style = b->generic.id - ID_EDIT_STYLE;
	focus = UI_DFocus( &s_edit.menu, self );

	// the .chip pill of the artboard
	if ( style == s_edit.style ) {
		fill[0] = 0.33f; fill[1] = 0.59f; fill[2] = 1.0f; fill[3] = 0.45f;
		UI_DRoundGlow( b->generic.x, b->generic.y, b->width, b->height, 12, fill );
		fill[3] = 0.3f;
		edge[0] = 0.47f; edge[1] = 0.70f; edge[2] = 1.0f; edge[3] = 1.0f;
	}
	else {
		fill[0] = 0.024f; fill[1] = 0.03f; fill[2] = 0.12f; fill[3] = 0.6f;
		edge[0] = 0.24f + 0.23f * focus; edge[1] = 0.33f + 0.37f * focus; edge[2] = 0.66f + 0.34f * focus; edge[3] = 1.0f;
	}
	UI_DRoundFill( b->generic.x, b->generic.y, b->width, b->height, b->height * 0.5f, fill );
	UI_DRoundEdge( b->generic.x, b->generic.y, b->width, b->height, b->height * 0.5f, edge );
	UI_DText( b->generic.x + b->width * 0.5f, b->generic.y + b->height * 0.5f - ( s_edit.menu.mobile ? 8 : 10 ), botStyles[style].label,
		s_edit.menu.mobile ? 14 : 18, UI_CENTER, ( style == s_edit.style || focus > 0.0f ) ? zeq_white : zeq_text2 );
}

static void UI_BotEdit_DrawName( void *self ) {
	menufield_s	*f;
	qboolean	focus;
	char		clean[64];

	f = (menufield_s *)self;
	focus = ( Menu_ItemAtCursor( &s_edit.menu ) == self );
	if ( s_edit.menu.mobile ) {
		UI_DField( 598 + UI_DExtra(), 26, 220, 52, uis.textField == self ? 1.0f : 0.0f );
		UI_DProp( 612 + UI_DExtra(), 32, "NAME", 9, UI_LEFT, zeq_text3 );
		UI_DText( 612 + UI_DExtra(), 47, f->field.buffer, 19, UI_LEFT, zeq_white );
		return;
	}
	UI_DField( 72, 420, 364, 42, focus ? 1.0f : 0.0f );
	Q_strncpyz( clean, f->field.buffer, sizeof( clean ) );
	Q_CleanStr( clean );
	UI_DText( 86, 431, focus ? clean : f->field.buffer, 20, UI_LEFT, zeq_white );
	if ( focus && ( ( uis.realtime / 400 ) & 1 ) ) {
		UI_DFill( 88 + UI_DTextWidth( clean, 20 ), 430, 2, 22, zeq_white );
	}
}

static void UI_BotEdit_DrawDefaults( void *self ) {
	menubitmap_s	*b;
	float			focus;
	vec4_t			fill, edge;

	b = (menubitmap_s *)self;
	focus = UI_DFocus( &s_edit.menu, self );
	fill[0] = 0.024f; fill[1] = 0.03f; fill[2] = 0.12f; fill[3] = 0.5f;
	UI_DRoundFill( b->generic.x, b->generic.y, b->width, b->height, 6, fill );
	edge[0] = 0.18f + 0.29f * focus; edge[1] = 0.27f + 0.36f * focus; edge[2] = 0.56f + 0.44f * focus; edge[3] = 1.0f;
	UI_DRoundEdge( b->generic.x, b->generic.y, b->width, b->height, 6, edge );
	if ( s_edit.menu.mobile ) {
		UI_DText( b->generic.x + 12, b->generic.y + 8, "Roster defaults", 16, UI_LEFT, zeq_white );
		UI_DText( b->generic.x + 12, b->generic.y + 29, "Level and style from bots.txt", 12, UI_LEFT, zeq_text3 );
		UI_DToggle( b->generic.x + b->width - 74, b->generic.y + 15, !s_edit.touched );
		return;
	}
	UI_DText( b->generic.x + 12, b->generic.y + 7, "Roster defaults", 18, UI_LEFT, zeq_white );
	UI_DText( b->generic.x + 12, b->generic.y + 28, "Level and style from bots.txt", 15, UI_LEFT, zeq_text3 );
	UI_DToggle( b->generic.x + b->width - 74, b->generic.y + 15, !s_edit.touched );
}

static void UI_BotEdit_DrawAccept( void *self ) {
	menubitmap_s *b;

	b = (menubitmap_s *)self;
	UI_DCta( b->generic.x, b->generic.y, b->width, b->height, s_edit.acceptLabel, UI_DFocus( &s_edit.menu, self ) );
}

static void UI_BotEdit_Draw( void ) {
	float			y;
	int				i;
	uiBotDef_t		*def;
	uiBotStyle_t	*style;
	vec4_t			line;
	static const int barOrder[6] = { 0, 4, 1, 3, 2, 5 };	// aggression block / rush specials / combos ranged
	static const char *hints[] = { "LR", "Character", "ENTER", "Select", "ESC", "Cancel" };

	uis.menuamount = -1;
	uis.hideEarth = qtrue;

	if ( s_edit.menu.mobile ) {
		UI_DMobileHeader( s_edit.live ? "PAUSED  >  BOTS  >" : "CREATE MATCH  >  FIGHTERS  >",
			s_edit.slot >= 0 || s_edit.liveClient >= 0 ? "EDIT BOT" : "ADD BOT" );
		UI_DGlass( 26, 88, 276, 276 );
		UI_DProp( 44, 96, "CHARACTER", 10, UI_LEFT, zeq_gold );
		if ( numBotDefs > EDIT_ROWS * 4 ) {
			UI_DText( 290, 96, "drag for more", 11, UI_RIGHT, zeq_text3 );
		}
		UI_DGlass( 314, 88, 504, 212 );
		UI_DProp( 332, 96, "LEVEL", 10, UI_LEFT, zeq_gold );
		UI_DProp( 332, 176, "STYLE", 10, UI_LEFT, zeq_gold );
		Menu_Draw( &s_edit.menu );
		return;
	}

	UI_DHeader( s_edit.live ? "PAUSED  >  BOTS  >" : "CREATE MATCH  >  FIGHTERS  >",
		s_edit.slot >= 0 || s_edit.liveClient >= 0 ? "EDIT BOT" : ( s_edit.live ? "ADD BOT" : va( "ADD BOT  -  SLOT %i", rosterCount + 2 ) ) );

	UI_DGlass( 48, 128, 412, 528 );
	UI_DSectionTitle( 72, 152, "CHARACTER" );
	if ( numBotDefs > EDIT_ROWS * 4 ) {
		UI_DText( 436, 150, va( "%s %i-%i of %i %s", s_edit.charScroll > 0 ? "<" : " ", s_edit.charScroll * 4 + 1,
			( s_edit.charScroll * 4 + EDIT_ROWS * 4 < numBotDefs ) ? s_edit.charScroll * 4 + EDIT_ROWS * 4 : numBotDefs, numBotDefs,
			( s_edit.charScroll + EDIT_ROWS ) * 4 < numBotDefs ? ">" : " " ), 15, UI_RIGHT, zeq_text3 );
	}
	UI_DSectionTitle( 72, 398, "NAME" );
	UI_DGlass( 732, 128, 500, 528 );
	UI_DSectionTitle( 756, 152, "LEVEL" );
	UI_DSectionTitle( 756, 302, "STYLE" );

	// the chosen fighter in 3D
	def = UI_Bots_Def( s_edit.def );
	if ( def ) {
		UI_Bots_SetPreview( &s_edit.playerinfo, s_edit.shownModel, sizeof( s_edit.shownModel ), def->player );
		trap_R_SetColor( NULL );
		UI_DPic( 480, 590, 240, 76, uis.blobShadow );
		UI_Bots_DrawPreview( &s_edit.playerinfo, 440, 110, 320, 500 );
	}

	Menu_Draw( &s_edit.menu );

	// values of the chosen style, two columns as the artboard
	style = UI_Bots_Style( s_edit.style );
	if ( style ) {
		for ( i = 0; i < 6; i++ ) {
			UI_DStatBarW( 756 + ( i % 2 ) * 237, s_edit.barsY + ( i / 2 ) * 26, 215, 76, statLabels[barOrder[i]], style->values[barOrder[i]],
				UI_AnimFrac( s_edit.styleTime, 40 * i, 420 ) );
		}
	}

	// behaviour editor, not there yet
	y = s_edit.barsY + 3 * 26 + 10;
	line[0] = 0.35f; line[1] = 0.51f; line[2] = 0.9f; line[3] = 0.25f;
	UI_DFill( 756, y, 452, 1, line );
	UI_DText( 756, y + 12, "Behaviour editor", 18, UI_LEFT, zeq_text2 );
	UI_DText( 756, y + 33, "Custom state machine per character", 15, UI_LEFT, zeq_text3 );
	line[0] = 0.24f; line[1] = 0.33f; line[2] = 0.66f; line[3] = 1.0f;
	UI_DRoundEdge( 1150, y + 20, 58, 24, 3, line );
	UI_DProp( 1179, y + 27, "SOON", 10, UI_CENTER, zeq_text3 );

	UI_DHints( hints, 3 );
}

static void UI_BotEdit_DItem( menubitmap_s *b, int id, void (*draw)( void * ), int x, int y, int w, int h ) {
	b->generic.type			= MTYPE_BITMAP;
	b->generic.flags		= QMF_NODEFAULTINIT;
	b->generic.id			= id;
	b->generic.callback		= UI_BotEdit_Event;
	b->generic.ownerdraw	= draw;
	UI_DItem( b, x, y, w, h );
}

void UI_BotEditMenu( int slot ) {
	int		i;
	float	chipX, chipY, chipW;
	float	vx, vy, vw, vh;

	UI_Bots_Load();
	if ( !numBotDefs ) {
		return;
	}

	memset( &s_edit, 0, sizeof( s_edit ) );
	s_edit.menu.draw = UI_BotEdit_Draw;
	s_edit.menu.key = UI_BotEdit_Key;
	s_edit.menu.fullscreen = qtrue;
	s_edit.menu.wrapAround = qtrue;
	s_edit.menu.mobile = UI_MobileLayout();
	UI_DCanvas( s_edit.menu.mobile );
	s_edit.slot = ( slot >= 0 && slot < rosterCount ) ? slot : -1;
	s_edit.liveClient = -1;
	s_edit.defTime = uis.realtime;
	s_edit.styleTime = uis.realtime;
	s_edit.acceptLabel = ( s_edit.slot < 0 ) ? "ADD BOT" : "SAVE";

	if ( s_edit.slot >= 0 ) {
		s_edit.def = roster[s_edit.slot].def;
		s_edit.skill = roster[s_edit.slot].skill;
		s_edit.style = roster[s_edit.slot].style;
		s_edit.touched = qtrue;
	}
	else {
		// suggest a character that is not in the match yet
		s_edit.def = UI_BotEdit_FreeDef();
		s_edit.skill = botDefs[s_edit.def].skill;
		s_edit.style = UI_Bots_FindStyle( botDefs[s_edit.def].archetype );
	}

	for ( i = 0; i < numBotDefs; i++ ) {
		UI_BotEdit_DItem( &s_edit.chars[i], ID_EDIT_CHAR + i, UI_BotEdit_DrawChar, 0, 0, 85, 95 );
	}
	s_edit.charScroll = 0;
	UI_BotEdit_ScrollTo( s_edit.def );
	if ( s_edit.menu.mobile ) {
		// level cards, then style chips that wrap inside the panel
		UI_DCanvas( qtrue );
		for ( i = 0; i < 3; i++ ) {
			UI_BotEdit_DItem( &s_edit.skills[i], ID_EDIT_SKILL + i, UI_BotEdit_DrawSkill, 330 + i * 160, 112, 152, 56 );
		}
		chipX = 330;
		chipY = 190;
		for ( i = 0; i < numBotStyles; i++ ) {
			chipW = UI_DTextWidth( botStyles[i].label, 14 ) + 28;
			if ( chipW < 64 ) {
				chipW = 64;
			}
			if ( chipX + chipW > 802 ) {
				chipX = 330;
				chipY += 52;
			}
			UI_BotEdit_DItem( &s_edit.styles[i], ID_EDIT_STYLE + i, UI_BotEdit_DrawStyle, chipX, chipY, chipW, 44 );
			chipX += chipW + 8;
		}
	}
	else {
		for ( i = 0; i < 3; i++ ) {
			UI_BotEdit_DItem( &s_edit.skills[i], ID_EDIT_SKILL + i, UI_BotEdit_DrawSkill, 756 + i * 154, 176, 144, 108 );
		}
		chipX = 756;
		chipY = 326;
		for ( i = 0; i < numBotStyles; i++ ) {
			// pills as wide as their label, wrapping inside the panel
			chipW = UI_DTextWidth( botStyles[i].label, 18 ) + 30;
			if ( chipX + chipW > 1208 ) {
				chipX = 756;
				chipY += 40;
			}
			UI_BotEdit_DItem( &s_edit.styles[i], ID_EDIT_STYLE + i, UI_BotEdit_DrawStyle, chipX, chipY, chipW, 32 );
			chipX += chipW + 8;
		}
	}
	s_edit.barsY = chipY + 32 + 24;

	// name, defaults to the bots.txt one of the character
	s_edit.name.generic.type		= MTYPE_FIELD;
	s_edit.name.generic.flags		= QMF_NODEFAULTINIT;
	s_edit.name.generic.id			= ID_EDIT_NAME;
	s_edit.name.generic.ownerdraw	= UI_BotEdit_DrawName;
	s_edit.name.field.widthInChars	= 20;
	s_edit.name.field.maxchars		= 20;
	if ( s_edit.menu.mobile ) {
		vx = 598 + UI_DExtra(); vy = 26; vw = 220; vh = 52;
	}
	else {
		vx = 72; vy = 420; vw = 364; vh = 42;
	}
	UI_DToV( &vx, &vy, &vw, &vh );
	s_edit.name.generic.x			= vx;
	s_edit.name.generic.y			= vy;
	s_edit.name.generic.left		= vx;
	s_edit.name.generic.top			= vy;
	s_edit.name.generic.right		= vx + vw;
	s_edit.name.generic.bottom		= vy + vh;
	Q_strncpyz( s_edit.name.field.buffer, ( s_edit.slot >= 0 && roster[s_edit.slot].name[0] ) ? roster[s_edit.slot].name : botDefs[s_edit.def].name,
		sizeof( s_edit.name.field.buffer ) );
	s_edit.name.field.cursor = strlen( s_edit.name.field.buffer );
	if ( s_edit.menu.mobile ) {
		UI_BotEdit_DItem( &s_edit.defaults, ID_EDIT_DEFAULTS, UI_BotEdit_DrawDefaults, 314, 312, 336, 52 );
		UI_BotEdit_DItem( &s_edit.accept, ID_EDIT_ACCEPT, UI_BotEdit_DrawAccept, 664, 312, 154, 52 );
		UI_DBackItem( &s_edit.menu );
	}
	else {
		UI_BotEdit_DItem( &s_edit.defaults, ID_EDIT_DEFAULTS, UI_BotEdit_DrawDefaults, 72, 480, 364, 52 );
		UI_BotEdit_DItem( &s_edit.accept, ID_EDIT_ACCEPT, UI_BotEdit_DrawAccept, 1060 + UI_DExtra(), 664, 172, 46 );
	}

	for ( i = 0; i < numBotDefs; i++ ) {
		Menu_AddItem( &s_edit.menu, &s_edit.chars[i] );
	}
	Menu_AddItem( &s_edit.menu, &s_edit.name );
	Menu_AddItem( &s_edit.menu, &s_edit.defaults );
	for ( i = 0; i < 3; i++ ) {
		Menu_AddItem( &s_edit.menu, &s_edit.skills[i] );
	}
	for ( i = 0; i < numBotStyles; i++ ) {
		Menu_AddItem( &s_edit.menu, &s_edit.styles[i] );
	}
	Menu_AddItem( &s_edit.menu, &s_edit.accept );

	UI_PushMenu( &s_edit.menu );
	uis.menuamount = -1;
	uis.hideEarth = qtrue;
	uis.showFrame = qfalse;
	Menu_SetCursorToItem( &s_edit.menu, &s_edit.chars[s_edit.def] );
}

/*
=================
UI_BotEditLive

the edit screen for a bot of the running match, clientNum -1 adds one
=================
*/
static void UI_BotEditLive( int clientNum, int def, int skill, int style, const char *name ) {
	UI_BotEditMenu( -1 );
	if ( !numBotDefs ) {
		return;
	}
	s_edit.live = qtrue;
	s_edit.liveClient = clientNum;
	s_edit.acceptLabel = ( clientNum >= 0 ) ? "APPLY" : "ADD BOT";
	if ( clientNum >= 0 && def >= 0 ) {
		s_edit.def = def;
		s_edit.skill = skill;
		s_edit.style = style;
		s_edit.touched = qtrue;
		if ( name && name[0] ) {
			Q_strncpyz( s_edit.name.field.buffer, name, sizeof( s_edit.name.field.buffer ) );
			s_edit.name.field.cursor = strlen( s_edit.name.field.buffer );
		}
		UI_BotEdit_ScrollTo( def );
		Menu_SetCursorToItem( &s_edit.menu, &s_edit.chars[def] );
	}
}

/*
=======================================================================

PAUSE > BOTS: bots of the running match (host only)

Reads the player configstrings every frame, so kicks and joins show up
as soon as the server applies them.

=======================================================================
*/

#define ID_PB_ROW			20		// + row
#define ID_PB_KICK			40		// + row
#define ID_PB_ADD			60
#define ID_PB_FREEZE		61
#define ID_PB_KICKALL		62
#define ID_PB_BACK			63

// design canvas, as the PauseBots artboard: PC on 1280x720, phone on 844x390
typedef struct {
	int		x, y, w, h;				// glass panel
	int		inX, inW;				// inside the glass border
	int		titleY;
	int		colY, colBot, colLevel, colStyle, colSize;
	int		lineY;
	int		rowsY, rowsH, rowMax;	// rows share rowsH (PC) or scroll (phone)
	int		kickX, kickMax;
	int		noteY, noteSize;
	int		barY;
	int		freezeX, freezeY, freezeW, freezeH;
	int		addX, addY, addW, addH;
	int		kickAllX, kickAllY, kickAllW, kickAllH;
	int		slotsX, slotsY, slotsSize;
	int		nameSize, infoSize, cellSize;
	int		portraitMax;
} pbLayout_t;

static const pbLayout_t pbLayouts[2] = {
	{	190, 96, 900, 548,		214, 852,	126,
		172, 226, 622, 826, 15,		194,
		204, 330, 66,			1014, 36,
		537, 17,		566,
		760, 112, 306, 46,
		226, 578, 170, 42,
		904, 581, 150, 36,
		414, 586, 17,
		19, 15, 18,		46 },
	{	26, 88, 792, 212,		38, 768,	0,
		96, 54, 384, 516, 12,		118,
		122, 124, 62,			754, 52,
		262, 13,		0,
		548, 26, 270, 52,
		26, 312, 170, 52,
		618, 312, 200, 52,
		208, 330, 14,
		17, 13, 15,		44 }
};

#define PBL		( pbLayouts[s_pauseBots.menu.mobile ? 1 : 0] )

typedef struct {
	int		client;
	char	name[64];
	char	model[64];
	int		def;
	int		skill;
	int		style;
} pauseBot_t;

typedef struct {
	menuframework_s	menu;
	menubitmap_s	rows[UI_MAX_ROSTER];
	menubitmap_s	kicks[UI_MAX_ROSTER];
	menubitmap_s	add;
	menubitmap_s	freeze;
	menubitmap_s	kickall;
	menubitmap_s	back;
	pauseBot_t		bots[UI_MAX_ROSTER];
	int				count;
	int				scroll;		// phone: first row shown
} pauseBotsMenu_t;

static pauseBotsMenu_t s_pauseBots;

static int UI_Bots_FindDefByModel( const char *model ) {
	int i;

	for ( i = 0; i < numBotDefs; i++ ) {
		if ( !Q_stricmp( botDefs[i].player, model ) ) {
			return i;
		}
	}
	return -1;
}

static void UI_PauseBots_Scan( void ) {
	char		info[MAX_INFO_STRING];
	char		*slash;
	const char	*st;
	int			maxClients;
	int			i;
	pauseBot_t	*bot;

	s_pauseBots.count = 0;
	maxClients = trap_Cvar_VariableValue( "sv_maxclients" );
	if ( maxClients <= 0 || maxClients > MAX_CLIENTS ) {
		maxClients = MAX_CLIENTS;
	}
	for ( i = 0; i < maxClients && s_pauseBots.count < UI_MAX_ROSTER; i++ ) {
		trap_GetConfigString( CS_PLAYERS + i, info, sizeof( info ) );
		if ( !info[0] || !Info_ValueForKey( info, "skill" )[0] ) {
			continue;	// empty slot or a human
		}
		bot = &s_pauseBots.bots[s_pauseBots.count];
		bot->client = i;
		Q_strncpyz( bot->name, Info_ValueForKey( info, "n" ), sizeof( bot->name ) );
		Q_CleanStr( bot->name );
		Q_strncpyz( bot->model, Info_ValueForKey( info, "model" ), sizeof( bot->model ) );
		slash = strchr( bot->model, '/' );
		if ( slash ) {
			*slash = 0;
		}
		bot->skill = atoi( Info_ValueForKey( info, "skill" ) );
		if ( bot->skill < 1 || bot->skill > 3 ) {
			bot->skill = 2;
		}
		bot->def = UI_Bots_FindDefByModel( bot->model );
		st = Info_ValueForKey( info, "st" );
		if ( st[0] ) {
			bot->style = UI_Bots_FindStyle( st );
		}
		else {
			bot->style = UI_Bots_FindStyle( bot->def >= 0 ? botDefs[bot->def].archetype : "" );
		}
		s_pauseBots.count++;
	}
}

static void UI_PauseBots_Kick( int row ) {
	if ( row < 0 || row >= s_pauseBots.count ) {
		return;
	}
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "clientkick %i\n", s_pauseBots.bots[row].client ) );
}

static void UI_PauseBots_KickAllAction( qboolean result ) {
	if ( result ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "remove_allbots\n" );
	}
}

/*
=================
UI_PauseBots_Leave

The in game menu pauses a server with one human (cl_paused). This screen
runs the match so kicks and joins reach the list; going back pauses again.
=================
*/
static void UI_PauseBots_Leave( void ) {
	trap_Cvar_Set( "cl_paused", "1" );
	UI_PopMenu();
}

static void UI_PauseBots_Event( void *ptr, int event ) {
	int			id;
	pauseBot_t	*bot;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	id = ((menucommon_s *)ptr)->id;

	if ( id >= ID_PB_ROW && id < ID_PB_ROW + UI_MAX_ROSTER ) {
		bot = &s_pauseBots.bots[id - ID_PB_ROW];
		UI_BotEditLive( bot->client, bot->def, bot->skill, bot->style, bot->name );
		return;
	}
	if ( id >= ID_PB_KICK && id < ID_PB_KICK + UI_MAX_ROSTER ) {
		UI_PauseBots_Kick( id - ID_PB_KICK );
		return;
	}

	switch ( id ) {
	case ID_PB_ADD:
		if ( s_pauseBots.count < UI_MAX_ROSTER ) {
			UI_BotEditLive( -1, -1, 0, 0, NULL );
		}
		break;
	case ID_PB_FREEZE:
		trap_Cvar_Set( "bot_pause", trap_Cvar_VariableValue( "bot_pause" ) ? "0" : "1" );
		break;
	case ID_PB_KICKALL:
		// it emptied the match at once: ask first
		UI_ConfirmMenu( "KICK ALL BOTS?", 0, UI_PauseBots_KickAllAction );
		break;
	case ID_PB_BACK:
		UI_PauseBots_Leave();
		break;
	}
}

static float UI_PauseBots_Focus( void *item ) {
	if ( Menu_ItemAtCursor( &s_pauseBots.menu ) != item ) {
		return 0.0f;
	}
	if ( UI_TouchMode() && !UI_TouchPressed( item ) ) {
		return 0.0f;
	}
	return UI_AnimFrac( uis.focusTime, 0, 160 );
}

// rows the phone list shows at once
static int UI_PauseBots_Visible( void ) {
	return PBL.rowsH / PBL.rowMax;
}

// PC: rows share the list height, 66 px each until they no longer fit.
// Phone: always 62 dp, the list scrolls instead
static float UI_PauseBots_RowH( void ) {
	float h;

	if ( !s_pauseBots.count || s_pauseBots.menu.mobile ) {
		return PBL.rowMax;
	}
	h = (float)PBL.rowsH / s_pauseBots.count;
	return ( h > PBL.rowMax ) ? PBL.rowMax : h;
}

static void UI_PauseBots_DrawRow( void *self ) {
	menubitmap_s	*b;
	int				row;
	pauseBot_t		*bot;
	uiBotStyle_t	*style;
	float			focus;
	float			x, y, h;
	float			size;
	vec4_t			left;
	vec4_t			right;

	b = (menubitmap_s *)self;
	row = b->generic.id - ID_PB_ROW;
	if ( row < 0 || row >= s_pauseBots.count ) {
		return;
	}
	bot = &s_pauseBots.bots[row];
	style = UI_Bots_Style( bot->style );
	focus = UI_PauseBots_Focus( self );
	y = b->generic.y;
	h = b->height;

	// selected row: ki gradient with a rounded outline
	if ( focus > 0.0f ) {
		left[0] = 0.33f; left[1] = 0.59f; left[2] = 1.0f; left[3] = 0.32f * focus;
		right[0] = 0.33f; right[1] = 0.59f; right[2] = 1.0f; right[3] = 0.04f * focus;
		UI_DRoundRowGlow( PBL.inX, y, PBL.inW, h, 6, left, right );
		left[0] = 0.47f; left[1] = 0.63f; left[2] = 1.0f; left[3] = 0.45f * focus;
		UI_DRoundEdge( PBL.inX, y, PBL.inW, h, 8, left );
	}

	size = h - 14;
	if ( size > PBL.portraitMax ) {
		size = PBL.portraitMax;
	}
	UI_DPortrait( PBL.inX + 12, y + ( h - size ) * 0.5f, size, UI_Bots_Portrait( bot->model ), qfalse, focus );
	x = PBL.inX + 26 + size;
	if ( h >= 44 ) {
		UI_DText( x, y + h * 0.5f - PBL.nameSize - 1, bot->name, PBL.nameSize, UI_LEFT|UI_DROPSHADOW, zeq_white );
		UI_DText( x, y + h * 0.5f + 1, va( "%s  -  slot %i", bot->model, bot->client ), PBL.infoSize, UI_LEFT, zeq_text3 );
	}
	else {
		UI_DText( x, y + h * 0.5f - 9, bot->name, 17, UI_LEFT|UI_DROPSHADOW, zeq_white );
		UI_DText( x + UI_DTextWidth( bot->name, 17 ) + 12, y + h * 0.5f - 7, va( "%s  -  slot %i", bot->model, bot->client ), 14, UI_LEFT, zeq_text3 );
	}
	UI_DPips( PBL.colLevel + 30 * PBL.cellSize / 18, y + h * 0.5f - 4, bot->skill, 3, UI_AnimFrac( uis.transitionTime, 120 + row * 40, 400 ) );
	UI_DText( PBL.colLevel + 84 * PBL.cellSize / 18, y + h * 0.5f - PBL.cellSize * 0.55f, UI_Bots_SkillName( bot->skill ), PBL.cellSize, UI_LEFT, zeq_white );
	UI_DText( PBL.colStyle + ( s_pauseBots.menu.mobile ? 0 : 30 ), y + h * 0.5f - PBL.cellSize * 0.55f, style ? style->label : "", PBL.cellSize, UI_LEFT, zeq_white );
}

static void UI_PauseBots_DrawKick( void *self ) {
	menubitmap_s	*b;
	vec4_t			color;
	qboolean		hot;

	b = (menubitmap_s *)self;
	hot = UI_PauseBots_Focus( self ) > 0.0f || ( !UI_TouchMode() && Menu_ItemAtCursor( &s_pauseBots.menu ) == self );
	// the red .ib kick button of the artboard
	color[0] = 0.235f; color[1] = 0.063f; color[2] = 0.04f; color[3] = hot ? 0.7f : 0.35f;
	UI_DRoundFill( b->generic.x, b->generic.y, b->width, b->height, 4, color );
	color[0] = 0.94f; color[1] = 0.63f; color[2] = 0.53f; color[3] = hot ? 1.0f : 0.5f;
	UI_DRoundEdge( b->generic.x, b->generic.y, b->width, b->height, 4, color );
	color[3] = 1.0f;
	UI_DIcon( b->generic.x + b->width * 0.33f, b->generic.y + b->height * 0.33f, b->width * 0.34f, UI_ICON_CLOSE, color );
}

static void UI_PauseBots_DrawButton( void *self ) {
	menubitmap_s	*b;
	float			focus;
	float			*color;
	qboolean		frozen;
	qboolean		disabled;
	qboolean		mobile;
	vec4_t			edge;

	b = (menubitmap_s *)self;
	focus = UI_PauseBots_Focus( self );
	frozen = trap_Cvar_VariableValue( "bot_pause" ) != 0;
	disabled = ( b->generic.flags & QMF_GRAYED ) != 0;
	mobile = s_pauseBots.menu.mobile;
	if ( disabled ) {
		focus = 0.0f;
	}

	switch ( b->generic.id ) {
	case ID_PB_ADD:
		UI_DKiButton( b->generic.x + ( mobile ? 0.0f : 4.0f * focus ), b->generic.y, b->width, b->height, "ADD BOT", NULL, focus, disabled, qfalse );
		break;

	case ID_PB_FREEZE:
		// switch on the right of the title, as the artboard
		if ( mobile ) {
			edge[0] = 0.33f; edge[1] = 0.59f; edge[2] = 1.0f; edge[3] = 0.12f + 0.25f * focus;
			UI_DRoundFill( b->generic.x, b->generic.y, b->width, b->height, 8, edge );
			edge[3] = 0.35f + 0.5f * focus;
			UI_DRoundEdge( b->generic.x, b->generic.y, b->width, b->height, 8, edge );
			UI_DText( b->generic.x + 14, b->generic.y + 8, "Freeze bots", 16, UI_LEFT, frozen ? zeq_gold : zeq_white );
			UI_DText( b->generic.x + 14, b->generic.y + 29, frozen ? "They stand still until resumed" : "They keep fighting while paused", 12, UI_LEFT, zeq_text3 );
			trap_R_SetColor( NULL );
			UI_DToggle( b->generic.x + b->width - 74, b->generic.y + 15, frozen );
			break;
		}
		if ( focus > 0.0f ) {
			edge[0] = 0.47f; edge[1] = 0.63f; edge[2] = 1.0f; edge[3] = 0.45f * focus;
			UI_DRoundEdge( b->generic.x, b->generic.y, b->width, b->height, 6, edge );
		}
		UI_DText( b->generic.x + b->width - 82, b->generic.y + 3, "Freeze bots", 18, UI_RIGHT, frozen ? zeq_gold : zeq_white );
		UI_DText( b->generic.x + b->width - 82, b->generic.y + 23, frozen ? "They stand still until resumed" : "They keep fighting while paused", 15, UI_RIGHT, zeq_text3 );
		UI_DToggle( b->generic.x + b->width - 72, b->generic.y + 11, frozen );
		break;

	default:
		color = disabled ? zeq_text3 : zeq_white;
		edge[0] = 0.235f; edge[1] = 0.063f; edge[2] = 0.04f; edge[3] = 0.35f + 0.35f * focus;
		UI_DRoundFill( b->generic.x, b->generic.y, b->width, b->height, 4, edge );
		edge[0] = 0.94f; edge[1] = 0.63f; edge[2] = 0.53f; edge[3] = disabled ? 0.25f : 0.5f + 0.5f * focus;
		UI_DRoundEdge( b->generic.x, b->generic.y, b->width, b->height, 4, edge );
		edge[3] = disabled ? 0.4f : 1.0f;
		UI_DText( b->generic.x + b->width * 0.5f, b->generic.y + b->height * 0.5f - ( mobile ? 8 : 10 ), mobile ? "Kick all bots..." : "Kick all bots",
			mobile ? 16 : 19, UI_CENTER, focus > 0.0f ? color : edge );
		break;
	}
}

// rows and buttons follow the live list
static void UI_PauseBots_Sync( void ) {
	int				i;
	int				visible;
	float			rowH;
	float			kick;
	float			y;
	menucommon_s	*item;

	UI_DCanvas( s_pauseBots.menu.mobile );
	UI_PauseBots_Scan();
	rowH = UI_PauseBots_RowH();
	kick = rowH - ( s_pauseBots.menu.mobile ? 10 : 18 );
	if ( kick > PBL.kickMax ) {
		kick = PBL.kickMax;
	}
	visible = s_pauseBots.menu.mobile ? UI_PauseBots_Visible() : UI_MAX_ROSTER;
	if ( s_pauseBots.scroll > s_pauseBots.count - visible ) {
		s_pauseBots.scroll = s_pauseBots.count - visible;
	}
	if ( s_pauseBots.scroll < 0 ) {
		s_pauseBots.scroll = 0;
	}
	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		if ( i < s_pauseBots.count && i >= s_pauseBots.scroll && i < s_pauseBots.scroll + visible ) {
			y = PBL.rowsY + ( i - s_pauseBots.scroll ) * rowH;
			// the row ends before the kick button so both take clicks
			UI_DItem( &s_pauseBots.rows[i], PBL.inX, y, PBL.kickX - 8 - PBL.inX, rowH - ( s_pauseBots.menu.mobile ? 2 : 6 ) );
			UI_DItem( &s_pauseBots.kicks[i], PBL.kickX, y + ( rowH - ( s_pauseBots.menu.mobile ? 2 : 6 ) - kick ) * 0.5f, kick, kick );
			s_pauseBots.rows[i].generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
			s_pauseBots.kicks[i].generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		}
		else {
			s_pauseBots.rows[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
			s_pauseBots.kicks[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		}
	}
	if ( s_pauseBots.count >= UI_MAX_ROSTER ) {
		s_pauseBots.add.generic.flags |= QMF_GRAYED;
	}
	else {
		s_pauseBots.add.generic.flags &= ~QMF_GRAYED;
	}
	if ( s_pauseBots.count ) {
		s_pauseBots.kickall.generic.flags &= ~QMF_GRAYED;
	}
	else {
		s_pauseBots.kickall.generic.flags |= QMF_GRAYED;
	}
	item = (menucommon_s *)Menu_ItemAtCursor( &s_pauseBots.menu );
	if ( !item || ( item->flags & ( QMF_INACTIVE|QMF_GRAYED ) ) ) {
		Menu_SetCursorToItem( &s_pauseBots.menu, s_pauseBots.count ? (void *)&s_pauseBots.rows[s_pauseBots.scroll] : (void *)&s_pauseBots.add );
	}
}

static void UI_PauseBots_Draw( void ) {
	float			bias;
	int				slotsFree;
	int				hidden;
	vec4_t			shade;
	vec4_t			warn;
	static const char *hints[] = { "UD", "Bot", "ENTER", "Edit", "F", "Freeze", "DEL", "Kick", "ESC", "Back" };

	UI_PauseBots_Sync();

	// dim the running game behind
	bias = ( uis.scale > 0.0f ) ? ( uis.bias / uis.scale ) : 0.0f;
	shade[0] = 0.02f; shade[1] = 0.02f; shade[2] = 0.10f; shade[3] = 0.62f;
	UI_FillRect( -bias - uis.drawOffsetX, 0, 640 + 2 * bias, 480, shade );
	if ( s_pauseBots.menu.mobile ) {
		// a darker band keeps the title clear of the HUD under it
		shade[3] = 0.55f;
		UI_DFill( -200 - UI_DExtra(), 0, 1244 + 2 * UI_DExtra(), 80, shade );
		shade[3] = 0.62f;
		UI_DMobileHeader( "PAUSED >", "BOTS" );
	}
	else {
		InGame_DrawTopStrip( qtrue );
	}

	UI_DGlass( PBL.x, PBL.y, PBL.w, PBL.h );
	if ( !s_pauseBots.menu.mobile ) {
		UI_DSectionTitle( PBL.inX + 12, PBL.titleY, "BOTS IN THIS MATCH" );
	}

	// column titles
	UI_DText( PBL.colBot, PBL.colY, "BOT", PBL.colSize, UI_LEFT, zeq_text3 );
	UI_DText( PBL.colLevel, PBL.colY, "LEVEL", PBL.colSize, UI_LEFT, zeq_text3 );
	UI_DText( PBL.colStyle, PBL.colY, "STYLE", PBL.colSize, UI_LEFT, zeq_text3 );
	shade[0] = 0.35f; shade[1] = 0.51f; shade[2] = 0.9f; shade[3] = 0.4f;
	UI_DFill( PBL.inX, PBL.lineY, PBL.inW, 1, shade );

	if ( !s_pauseBots.count ) {
		UI_DText( PBL.x + PBL.w * 0.5f, PBL.rowsY + PBL.rowsH * 0.5f - ( s_pauseBots.menu.mobile ? 22 : 49 ), "No bots in this match.", s_pauseBots.menu.mobile ? 17 : 22, UI_CENTER, zeq_white );
		UI_DText( PBL.x + PBL.w * 0.5f, PBL.rowsY + PBL.rowsH * 0.5f - ( s_pauseBots.menu.mobile ? 0 : 19 ), "ADD BOT brings one in.", s_pauseBots.menu.mobile ? 14 : 18, UI_CENTER, zeq_text2 );
	}
	else {
		warn[0] = 0.96f; warn[1] = 0.81f; warn[2] = 0.52f; warn[3] = 1.0f;
		UI_DIcon( PBL.inX + 12, PBL.noteY + 1, PBL.noteSize + 1, UI_ICON_WARN, zeq_gold );
		if ( s_pauseBots.menu.mobile ) {
			hidden = s_pauseBots.count - UI_PauseBots_Visible();
			UI_DText( PBL.inX + 20 + PBL.noteSize, PBL.noteY, hidden > 0 ? va( "Tap a bot to edit it, drag the list for %i more.", hidden ) :
				"Tap a bot to edit it: it re-enters at a spawn point.", PBL.noteSize, UI_LEFT, warn );
		}
		else {
			UI_DText( PBL.inX + 40, PBL.noteY, "ENTER edits a bot: it re-enters the match at a spawn point.", PBL.noteSize, UI_LEFT, warn );
		}
	}

	// bottom bar
	if ( !s_pauseBots.menu.mobile ) {
		shade[3] = 0.25f;
		UI_DFill( PBL.inX, PBL.barY, PBL.inW, 1, shade );
	}
	slotsFree = UI_MAX_ROSTER - s_pauseBots.count;
	UI_DText( PBL.slotsX, PBL.slotsY, slotsFree == 1 ? "1 slot free" : va( "%i slots free", slotsFree ), PBL.slotsSize, UI_LEFT, zeq_text3 );

	Menu_Draw( &s_pauseBots.menu );

	UI_DHintsCentered( hints, 5 );
}

static sfxHandle_t UI_PauseBots_Key( int key ) {
	menucommon_s *item;

	if ( key == K_DEL || key == K_BACKSPACE || key == K_KP_DEL ) {
		item = (menucommon_s *)Menu_ItemAtCursor( &s_pauseBots.menu );
		if ( item && item->id >= ID_PB_ROW && item->id < ID_PB_ROW + UI_MAX_ROSTER ) {
			UI_PauseBots_Kick( item->id - ID_PB_ROW );
			return menu_out_sound;
		}
	}
	if ( key == K_ESCAPE || key == K_MOUSE2 ) {
		UI_PauseBots_Leave();
		return menu_out_sound;
	}
	if ( key == 'f' || key == 'F' ) {
		trap_Cvar_Set( "bot_pause", trap_Cvar_VariableValue( "bot_pause" ) ? "0" : "1" );
		return menu_move_sound;
	}
	// phone: the wheel and a dragging finger scroll the rows
	if ( s_pauseBots.menu.mobile && ( key == K_MWHEELUP || key == K_MWHEELDOWN ) ) {
		s_pauseBots.scroll += ( key == K_MWHEELUP ) ? -1 : 1;
		UI_PauseBots_Sync();
		return menu_null_sound;
	}
	return Menu_DefaultKey( &s_pauseBots.menu, key );
}

static void UI_PauseBots_Button( menubitmap_s *b, int id, int x, int y, int w, int h ) {
	b->generic.type			= MTYPE_BITMAP;
	b->generic.flags		= QMF_NODEFAULTINIT;
	b->generic.id			= id;
	b->generic.callback		= UI_PauseBots_Event;
	b->generic.ownerdraw	= UI_PauseBots_DrawButton;
	UI_DItem( b, x, y, w, h );
}

void UI_PauseBotsMenu( void ) {
	int i;

	UI_Bots_Load();

	memset( &s_pauseBots, 0, sizeof( s_pauseBots ) );
	s_pauseBots.menu.draw = UI_PauseBots_Draw;
	s_pauseBots.menu.key = UI_PauseBots_Key;
	s_pauseBots.menu.fullscreen = qfalse;
	s_pauseBots.menu.wrapAround = qtrue;
	s_pauseBots.menu.mobile = UI_MobileLayout();
	UI_DCanvas( s_pauseBots.menu.mobile );

	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		s_pauseBots.rows[i].generic.type		= MTYPE_BITMAP;
		s_pauseBots.rows[i].generic.flags		= QMF_NODEFAULTINIT|QMF_HIDDEN|QMF_INACTIVE;
		s_pauseBots.rows[i].generic.id			= ID_PB_ROW + i;
		s_pauseBots.rows[i].generic.callback	= UI_PauseBots_Event;
		s_pauseBots.rows[i].generic.ownerdraw	= UI_PauseBots_DrawRow;

		s_pauseBots.kicks[i].generic.type		= MTYPE_BITMAP;
		s_pauseBots.kicks[i].generic.flags		= QMF_NODEFAULTINIT|QMF_MOUSEONLY|QMF_HIDDEN|QMF_INACTIVE;
		s_pauseBots.kicks[i].generic.id			= ID_PB_KICK + i;
		s_pauseBots.kicks[i].generic.callback	= UI_PauseBots_Event;
		s_pauseBots.kicks[i].generic.ownerdraw	= UI_PauseBots_DrawKick;
	}

	// ESC goes back, the artboard has no BACK button (the phone has its arrow)
	UI_PauseBots_Button( &s_pauseBots.freeze, ID_PB_FREEZE, PBL.freezeX, PBL.freezeY, PBL.freezeW, PBL.freezeH );
	UI_PauseBots_Button( &s_pauseBots.add, ID_PB_ADD, PBL.addX, PBL.addY, PBL.addW, PBL.addH );
	UI_PauseBots_Button( &s_pauseBots.kickall, ID_PB_KICKALL, PBL.kickAllX, PBL.kickAllY, PBL.kickAllW, PBL.kickAllH );

	if ( s_pauseBots.menu.mobile ) {
		UI_DBackItem( &s_pauseBots.menu );
	}
	Menu_AddItem( &s_pauseBots.menu, &s_pauseBots.freeze );
	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		Menu_AddItem( &s_pauseBots.menu, &s_pauseBots.rows[i] );
		Menu_AddItem( &s_pauseBots.menu, &s_pauseBots.kicks[i] );
	}
	Menu_AddItem( &s_pauseBots.menu, &s_pauseBots.add );
	Menu_AddItem( &s_pauseBots.menu, &s_pauseBots.kickall );

	UI_PushMenu( &s_pauseBots.menu );
	trap_Cvar_Set( "cl_paused", "0" );
	UI_PauseBots_Sync();
	Menu_SetCursorToItem( &s_pauseBots.menu, s_pauseBots.count ? (void *)&s_pauseBots.rows[0] : (void *)&s_pauseBots.add );
}
