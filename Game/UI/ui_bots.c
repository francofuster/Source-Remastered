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
}

static void UI_Bots_AddDefault( int def ) {
	if ( def < 0 || def >= numBotDefs || rosterCount >= UI_MAX_ROSTER ) {
		return;
	}
	roster[rosterCount].def = def;
	roster[rosterCount].skill = botDefs[def].skill;
	roster[rosterCount].style = UI_Bots_FindStyle( botDefs[def].archetype );
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
		rosterCount++;
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
		trap_Cvar_Set( va( "g_queuedBot%i", i ), va( "%s %i %s", botDefs[roster[i].def].id,
			roster[i].skill, style ? style->file : "" ) );
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

ROSTER MENU

=======================================================================
*/

#define ID_ROSTER_ADD		10
#define ID_ROSTER_RANDOM	11
#define ID_ROSTER_CLEAR		12
#define ID_ROSTER_DONE		13
#define ID_ROSTER_ROW		20		// + slot
#define ID_ROSTER_REMOVE	40		// + slot
#define ID_ROSTER_NEWROW	60

#define ROSTER_LIST_X		160
#define ROSTER_LIST_Y		36
#define ROSTER_LIST_W		300
#define ROSTER_ROW_Y		100
#define ROSTER_ROW_H		28
#define ROSTER_INFO_X		470
#define ROSTER_INFO_W		160

typedef struct {
	menuframework_s	menu;
	menutext_s		add;
	menutext_s		random;
	menutext_s		clear;
	menutext_s		done;
	menubitmap_s	rows[UI_MAX_ROSTER];
	menubitmap_s	removes[UI_MAX_ROSTER];
	menubitmap_s	newRow;
	int				shownSlot;		// bot shown in the info panel
	int				shownTime;		// when it changed, restarts the bars
} rosterMenu_t;

static rosterMenu_t s_roster;

static void UI_BotRoster_Refresh( void );

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
		rosterCount = 0;
		UI_Bots_SaveRoster();
		UI_BotRoster_Refresh();
		break;
	case ID_ROSTER_DONE:
		UI_PopMenu();
		break;
	}
}

static float UI_BotRoster_RowFocus( void *item ) {
	if ( Menu_ItemAtCursor( &s_roster.menu ) != item ) {
		return 0.0f;
	}
	return UI_AnimFrac( uis.focusTime, 0, 160 );
}

static void UI_BotRoster_DrawRow( void *self ) {
	menubitmap_s	*b;
	int				slot;
	float			y;
	float			focus;
	uiBotDef_t		*def;
	uiBotStyle_t	*style;
	float			enter;

	b = (menubitmap_s *)self;
	slot = b->generic.id - ID_ROSTER_ROW;
	if ( slot < 0 || slot >= rosterCount ) {
		return;
	}
	def = &botDefs[roster[slot].def];
	style = UI_Bots_Style( roster[slot].style );
	y = b->generic.top;
	focus = UI_BotRoster_RowFocus( self );

	UI_DrawFocusRow( b->generic.left, y, b->generic.right - b->generic.left, ROSTER_ROW_H - 4, focus );
	UI_DrawPortrait( b->generic.left + 4, y + 2, 20, UI_Bots_Portrait( def->player ), qfalse, focus );
	UI_DrawStringSized( b->generic.left + 30, y + 2, def->name, UI_LEFT|UI_DROPSHADOW, zeq_white, 7, 12 );
	UI_DrawStringSized( b->generic.left + 30, y + 13, def->player, UI_LEFT, zeq_text3, 5, 9 );

	// the pips light up in turn when the screen opens, row after row
	enter = UI_AnimFrac( uis.transitionTime, 120 + slot * 40, 400 );
	UI_DrawPips( b->generic.left + 150, y + 6, roster[slot].skill, 3, enter );
	UI_DrawStringSized( b->generic.left + 150, y + 13, UI_Bots_SkillName( roster[slot].skill ), UI_LEFT, zeq_text2, 5, 9 );
	UI_DrawStringSized( b->generic.left + 196, y + 7, style ? style->label : "", UI_LEFT, zeq_text2, 6, 11 );
}

static void UI_BotRoster_DrawRemove( void *self ) {
	menubitmap_s	*b;
	vec4_t			color;
	qboolean		hot;

	b = (menubitmap_s *)self;
	hot = ( Menu_ItemAtCursor( &s_roster.menu ) == self );
	color[0] = 0.94f; color[1] = 0.63f; color[2] = 0.53f; color[3] = hot ? 1.0f : 0.6f;
	UI_DrawRect( b->generic.left, b->generic.top, 12, 12, color );
	UI_DrawStringSized( b->generic.left + 6, b->generic.top, "x", UI_CENTER, color, 7, 12 );
}

static void UI_BotRoster_DrawNewRow( void *self ) {
	menubitmap_s	*b;
	float			focus;
	vec4_t			edge;

	b = (menubitmap_s *)self;
	focus = UI_BotRoster_RowFocus( self );
	edge[0] = 0.29f; edge[1] = 0.39f; edge[2] = 0.74f; edge[3] = 0.6f + 0.4f * focus;
	UI_DrawFocusRow( b->generic.left, b->generic.top, b->generic.right - b->generic.left, ROSTER_ROW_H - 4, focus );
	UI_DrawRect( b->generic.left, b->generic.top, b->generic.right - b->generic.left, ROSTER_ROW_H - 4, edge );
	UI_DrawStringSized( b->generic.left + 12, b->generic.top + 6, va( "+  Add a bot to slot %i", rosterCount + 2 ), UI_LEFT, zeq_ki, 7, 12 );
}

static void UI_BotRoster_Draw( void ) {
	char			name[64];
	char			model[64];
	float			y;
	uiRosterSlot_t	*slot;
	uiBotDef_t		*def;
	uiBotStyle_t	*style;
	int				i;
	float			anim;
	float			cx;
	static const char *hints[] = { "ENTER", "Edit", "DEL", "Remove", "ESC", "Back" };

	// the side strip is shared, set it again after coming back from a sub screen
	uis.menuamount = 4;
	uis.hideEarth = qtrue;

	// list panel
	UI_DrawGlassPanel( ROSTER_LIST_X, ROSTER_LIST_Y, ROSTER_LIST_W, 398 );
	UI_DrawSectionTitle( ROSTER_LIST_X + 14, ROSTER_LIST_Y + 12, va( "FIGHTERS  %i OF 12", rosterCount + 1 ) );
	UI_DrawStringSized( ROSTER_LIST_X + ROSTER_LIST_W - 14, ROSTER_LIST_Y + 12, va( "1 human  %i bots", rosterCount ), UI_RIGHT, zeq_text3, 6, 11 );

	// the human player, not editable here
	UI_Bots_HumanInfo( name, sizeof( name ), model, sizeof( model ) );
	y = ROSTER_LIST_Y + 34;
	UI_DrawPortrait( ROSTER_LIST_X + 16, y + 2, 20, UI_Bots_Portrait( model ), qfalse, 0.0f );
	UI_DrawStringSized( ROSTER_LIST_X + 42, y + 2, name, UI_LEFT|UI_DROPSHADOW, zeq_white, 7, 12 );
	UI_DrawStringSized( ROSTER_LIST_X + 42, y + 13, va( "you  %s", model ), UI_LEFT, zeq_text3, 5, 9 );

	Menu_Draw( &s_roster.menu );

	// info panel for the focused (or last focused) bot
	UI_DrawGlassPanel( ROSTER_INFO_X, ROSTER_LIST_Y, ROSTER_INFO_W, 398 );
	slot = UI_Bots_RosterSlot( s_roster.shownSlot );
	cx = ROSTER_INFO_X + ROSTER_INFO_W * 0.5f;
	if ( !slot ) {
		UI_DrawSectionTitle( ROSTER_INFO_X + 14, ROSTER_LIST_Y + 12, "NO BOTS" );
		UI_DrawStringSized( ROSTER_INFO_X + 14, ROSTER_LIST_Y + 36, "Add bots with ADD BOT", UI_LEFT, zeq_text2, 6, 11 );
		UI_DrawStringSized( ROSTER_INFO_X + 14, ROSTER_LIST_Y + 50, "or RANDOM on the left.", UI_LEFT, zeq_text2, 6, 11 );
	}
	else {
		def = &botDefs[slot->def];
		style = UI_Bots_Style( slot->style );
		anim = UI_AnimFrac( s_roster.shownTime, 0, 450 );
		UI_DrawPortrait( cx - 32, ROSTER_LIST_Y + 18, 64, UI_Bots_Portrait( def->player ), qtrue, 0.0f );
		UI_DrawPropScaled( cx, ROSTER_LIST_Y + 96, UI_Bots_DisplayName( def ), UI_CENTER|UI_DROPSHADOW, zeq_white, 0.55f );
		UI_DrawStringSized( cx, ROSTER_LIST_Y + 116, va( "%s  -  %s", def->name, def->player ), UI_CENTER, zeq_text3, 6, 11 );
		UI_DrawPips( cx - 13, ROSTER_LIST_Y + 136, slot->skill, 3, anim );
		UI_DrawStringSized( cx, ROSTER_LIST_Y + 144, va( "%s  %s", UI_Bots_SkillName( slot->skill ), style ? style->label : "" ), UI_CENTER, zeq_text2, 6, 11 );
		if ( style ) {
			for ( i = 0; i < 6; i++ ) {
				UI_DrawStatBar( ROSTER_INFO_X + 14, ROSTER_LIST_Y + 172 + i * 18, ROSTER_INFO_W - 28, statLabels[i], style->values[i],
					UI_AnimFrac( s_roster.shownTime, 40 * i, 420 ) );
			}
		}
		UI_DrawStringSized( cx, ROSTER_LIST_Y + 370, "ENTER on a row to edit", UI_CENTER, zeq_text3, 5, 9 );
	}

	UI_DrawKeyHints( hints, 3 );
}

static sfxHandle_t UI_BotRoster_Key( int key ) {
	menucommon_s *item;

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
	int i;

	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		if ( i < rosterCount ) {
			s_roster.rows[i].generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
			s_roster.removes[i].generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		}
		else {
			s_roster.rows[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
			s_roster.removes[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		}
	}

	// the add row sits right under the last bot
	if ( rosterCount < UI_MAX_ROSTER ) {
		s_roster.newRow.generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		s_roster.newRow.generic.top = ROSTER_ROW_Y + rosterCount * ROSTER_ROW_H;
		s_roster.newRow.generic.bottom = s_roster.newRow.generic.top + ROSTER_ROW_H - 4;
	}
	else {
		s_roster.newRow.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
	}

	if ( s_roster.shownSlot >= rosterCount ) {
		s_roster.shownSlot = rosterCount - 1;
		s_roster.shownTime = uis.realtime;
	}

	// keep the cursor on something that still exists
	if ( s_roster.menu.cursor >= 0 && s_roster.menu.cursor < s_roster.menu.nitems &&
		( ((menucommon_s *)s_roster.menu.items[s_roster.menu.cursor])->flags & QMF_INACTIVE ) ) {
		Menu_SetCursorToItem( &s_roster.menu, rosterCount < UI_MAX_ROSTER ? (void *)&s_roster.newRow : (void *)&s_roster.add );
	}
}

static void UI_BotRoster_SideButton( menutext_s *t, int id, int slot, const char *label ) {
	t->generic.type		= MTYPE_PTEXT;
	t->generic.flags	= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	t->generic.id		= id;
	t->generic.callback	= UI_BotRoster_Event;
	t->generic.x		= 28;
	t->generic.y		= 119 + slot * 38;
	t->string			= (char *)label;
	t->style			= UI_LEFT|UI_DROPSHADOW|UI_TINYFONT;
	t->color			= color_white;
}

void UI_BotRosterMenu( void ) {
	int i;

	UI_Bots_Load();

	memset( &s_roster, 0, sizeof( s_roster ) );
	s_roster.menu.draw = UI_BotRoster_Draw;
	s_roster.menu.key = UI_BotRoster_Key;
	s_roster.menu.fullscreen = qtrue;
	s_roster.menu.wrapAround = qtrue;
	s_roster.shownSlot = rosterCount ? 0 : -1;
	s_roster.shownTime = uis.realtime;

	UI_BotRoster_SideButton( &s_roster.add, ID_ROSTER_ADD, 0, "ADD BOT" );
	UI_BotRoster_SideButton( &s_roster.random, ID_ROSTER_RANDOM, 1, "RANDOM" );
	UI_BotRoster_SideButton( &s_roster.clear, ID_ROSTER_CLEAR, 2, "CLEAR" );
	UI_BotRoster_SideButton( &s_roster.done, ID_ROSTER_DONE, 3, "DONE" );

	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		s_roster.rows[i].generic.type		= MTYPE_BITMAP;
		s_roster.rows[i].generic.flags		= QMF_NODEFAULTINIT;
		s_roster.rows[i].generic.id			= ID_ROSTER_ROW + i;
		s_roster.rows[i].generic.callback	= UI_BotRoster_Event;
		s_roster.rows[i].generic.ownerdraw	= UI_BotRoster_DrawRow;
		s_roster.rows[i].generic.left		= ROSTER_LIST_X + 12;
		s_roster.rows[i].generic.right		= ROSTER_LIST_X + ROSTER_LIST_W - 30;
		s_roster.rows[i].generic.top		= ROSTER_ROW_Y + i * ROSTER_ROW_H;
		s_roster.rows[i].generic.bottom		= s_roster.rows[i].generic.top + ROSTER_ROW_H - 4;

		s_roster.removes[i].generic.type		= MTYPE_BITMAP;
		s_roster.removes[i].generic.flags		= QMF_NODEFAULTINIT|QMF_MOUSEONLY;
		s_roster.removes[i].generic.id			= ID_ROSTER_REMOVE + i;
		s_roster.removes[i].generic.callback	= UI_BotRoster_Event;
		s_roster.removes[i].generic.ownerdraw	= UI_BotRoster_DrawRemove;
		s_roster.removes[i].generic.left		= ROSTER_LIST_X + ROSTER_LIST_W - 26;
		s_roster.removes[i].generic.right		= s_roster.removes[i].generic.left + 12;
		s_roster.removes[i].generic.top			= ROSTER_ROW_Y + i * ROSTER_ROW_H + 6;
		s_roster.removes[i].generic.bottom		= s_roster.removes[i].generic.top + 12;
	}

	s_roster.newRow.generic.type		= MTYPE_BITMAP;
	s_roster.newRow.generic.flags		= QMF_NODEFAULTINIT;
	s_roster.newRow.generic.id			= ID_ROSTER_NEWROW;
	s_roster.newRow.generic.callback	= UI_BotRoster_Event;
	s_roster.newRow.generic.ownerdraw	= UI_BotRoster_DrawNewRow;
	s_roster.newRow.generic.left		= ROSTER_LIST_X + 12;
	s_roster.newRow.generic.right		= ROSTER_LIST_X + ROSTER_LIST_W - 12;

	Menu_AddItem( &s_roster.menu, &s_roster.add );
	Menu_AddItem( &s_roster.menu, &s_roster.random );
	Menu_AddItem( &s_roster.menu, &s_roster.clear );
	Menu_AddItem( &s_roster.menu, &s_roster.done );
	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		Menu_AddItem( &s_roster.menu, &s_roster.rows[i] );
		Menu_AddItem( &s_roster.menu, &s_roster.removes[i] );
	}
	Menu_AddItem( &s_roster.menu, &s_roster.newRow );

	UI_BotRoster_Refresh();
	UI_PushMenu( &s_roster.menu );
	uis.menuamount = 4;
	uis.hideEarth = qtrue;
	uis.showFrame = qfalse;

	// start on the first bot so the info panel has something to show
	if ( rosterCount ) {
		Menu_SetCursorToItem( &s_roster.menu, &s_roster.rows[0] );
	}
}

/*
=======================================================================

EDIT MENU: one bot

=======================================================================
*/

#define ID_EDIT_ACCEPT		10
#define ID_EDIT_BACK		11
#define ID_EDIT_CHAR		20		// + def
#define ID_EDIT_SKILL		60		// + skill
#define ID_EDIT_STYLE		70		// + style

#define EDIT_CHAR_X			160
#define EDIT_CHAR_Y			36
#define EDIT_CHAR_W			230
#define EDIT_CELL			52
#define EDIT_RIGHT_X		400
#define EDIT_RIGHT_W		230

typedef struct {
	menuframework_s	menu;
	menutext_s		accept;
	menutext_s		back;
	menubitmap_s	chars[UI_MAX_BOTDEFS];
	menubitmap_s	skills[3];
	menubitmap_s	styles[UI_MAX_STYLES];
	int				slot;			// roster slot, -1 for a new bot
	int				def;
	int				skill;
	int				style;
	qboolean		touched;		// level or style chosen by hand
	qboolean		live;			// editing a bot of the running match
	int				liveClient;		// its client number, -1 to add a new one
	int				defTime;
	int				styleTime;
	int				charRows;
} editMenu_t;

static editMenu_t s_edit;

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
	int				target;
	int				step;

	item = (menucommon_s *)Menu_ItemAtCursor( &s_edit.menu );
	if ( !item ) {
		return Menu_DefaultKey( &s_edit.menu, key );
	}
	id = item->id;
	step = 0;
	target = -1;

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
			target = id - ID_EDIT_CHAR + step;
			Menu_SetCursorToItem( &s_edit.menu, &s_edit.chars[target] );
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
			step = -2;
		}
		else if ( key == K_DOWNARROW || key == K_KP_DOWNARROW ) {
			step = 2;
		}
		if ( step && id - ID_EDIT_STYLE + step >= 0 && id - ID_EDIT_STYLE + step < numBotStyles ) {
			Menu_SetCursorToItem( &s_edit.menu, &s_edit.styles[id - ID_EDIT_STYLE + step] );
			return menu_move_sound;
		}
	}
	return Menu_DefaultKey( &s_edit.menu, key );
}

static float UI_BotEdit_Focus( void *item ) {
	if ( Menu_ItemAtCursor( &s_edit.menu ) != item ) {
		return 0.0f;
	}
	return UI_AnimFrac( uis.focusTime, 0, 160 );
}

static void UI_BotEdit_Event( void *ptr, int event ) {
	int id;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	id = ((menucommon_s *)ptr)->id;

	if ( id >= ID_EDIT_CHAR && id < ID_EDIT_CHAR + UI_MAX_BOTDEFS ) {
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

	switch ( id ) {
	case ID_EDIT_ACCEPT:
		if ( s_edit.live ) {
			// a bot loads its level and style when it joins: replace it
			if ( s_edit.liveClient >= 0 ) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "clientkick %i\n", s_edit.liveClient ) );
			}
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "addbot %s %i style=%s\n", botDefs[s_edit.def].id, s_edit.skill,
				UI_Bots_Style( s_edit.style ) ? UI_Bots_Style( s_edit.style )->file : "" ) );
			UI_PopMenu();
			break;
		}
		if ( s_edit.slot < 0 ) {
			if ( rosterCount >= UI_MAX_ROSTER ) {
				break;
			}
			s_edit.slot = rosterCount++;
		}
		roster[s_edit.slot].def = s_edit.def;
		roster[s_edit.slot].skill = s_edit.skill;
		roster[s_edit.slot].style = s_edit.style;
		UI_Bots_SaveRoster();
		UI_PopMenu();
		UI_BotRoster_Refresh();
		UI_BotRoster_Show( s_edit.slot );
		break;
	case ID_EDIT_BACK:
		UI_PopMenu();
		break;
	}
}

static void UI_BotEdit_DrawChar( void *self ) {
	menubitmap_s	*b;
	int				def;
	float			focus;
	qboolean		selected;

	b = (menubitmap_s *)self;
	def = b->generic.id - ID_EDIT_CHAR;
	focus = UI_BotEdit_Focus( self );
	selected = ( def == s_edit.def );
	UI_DrawPortrait( b->generic.left + 4, b->generic.top, 40, UI_Bots_Portrait( botDefs[def].player ), selected, focus );
	UI_DrawStringSized( b->generic.left + 24, b->generic.top + 44, botDefs[def].player, UI_CENTER,
		( selected || focus > 0.0f ) ? zeq_white : zeq_text2, 5, 9 );
}

static void UI_BotEdit_DrawSkill( void *self ) {
	menubitmap_s	*b;
	int				skill;
	float			focus;
	float			x, y, w, h;
	vec4_t			fill;
	vec4_t			edge;

	b = (menubitmap_s *)self;
	skill = b->generic.id - ID_EDIT_SKILL + 1;
	focus = UI_BotEdit_Focus( self );
	x = b->generic.left;
	y = b->generic.top;
	w = b->generic.right - b->generic.left;
	h = b->generic.bottom - b->generic.top;

	if ( skill == s_edit.skill ) {
		fill[0] = zeq_gold[0]; fill[1] = zeq_gold[1]; fill[2] = zeq_gold[2]; fill[3] = 0.16f;
		edge[0] = zeq_gold[0]; edge[1] = zeq_gold[1]; edge[2] = zeq_gold[2]; edge[3] = 1.0f;
	}
	else {
		fill[0] = 0.02f; fill[1] = 0.03f; fill[2] = 0.12f; fill[3] = 0.55f;
		edge[0] = 0.18f; edge[1] = 0.27f; edge[2] = 0.56f; edge[3] = 1.0f;
	}
	UI_FillRect( x, y, w, h, fill );
	UI_DrawFocusRow( x, y, w, h, focus );
	UI_DrawRect( x, y, w, h, edge );

	UI_DrawPips( x + 6, y + 6, skill, 3, UI_AnimFrac( uis.transitionTime, 100 + skill * 60, 300 ) );
	UI_DrawPropScaled( x + 6, y + 14, skillNames[skill], UI_LEFT, zeq_white, 0.34f );
	UI_DrawStringSized( x + 6, y + 30, skillLines[skill][0], UI_LEFT, zeq_text2, 5, 9 );
	UI_DrawStringSized( x + 6, y + 40, skillLines[skill][1], UI_LEFT, zeq_text2, 5, 9 );
}

static void UI_BotEdit_DrawStyle( void *self ) {
	menubitmap_s	*b;
	int				style;
	float			focus;
	float			x, y, w, h;
	vec4_t			fill;
	vec4_t			edge;

	b = (menubitmap_s *)self;
	style = b->generic.id - ID_EDIT_STYLE;
	focus = UI_BotEdit_Focus( self );
	x = b->generic.left;
	y = b->generic.top;
	w = b->generic.right - b->generic.left;
	h = b->generic.bottom - b->generic.top;

	if ( style == s_edit.style ) {
		fill[0] = zeq_ki[0]; fill[1] = zeq_ki[1]; fill[2] = zeq_ki[2]; fill[3] = 0.35f;
		edge[0] = 0.47f; edge[1] = 0.70f; edge[2] = 1.0f; edge[3] = 1.0f;
	}
	else {
		fill[0] = 0.02f; fill[1] = 0.03f; fill[2] = 0.12f; fill[3] = 0.6f;
		edge[0] = 0.24f; edge[1] = 0.33f; edge[2] = 0.66f; edge[3] = 0.6f + 0.4f * focus;
	}
	UI_FillRect( x, y, w, h, fill );
	UI_DrawFocusRow( x, y, w, h, focus * 0.6f );
	UI_DrawRect( x, y, w, h, edge );
	UI_DrawStringSized( x + w * 0.5f, y + 3, botStyles[style].label, UI_CENTER,
		( style == s_edit.style || focus > 0.0f ) ? zeq_white : zeq_text2, 6, 11 );
}

static void UI_BotEdit_Draw( void ) {
	float			y;
	int				i;
	uiBotDef_t		*def;
	uiBotStyle_t	*style;
	float			charBottom;
	float			pop;
	static const char *hints[] = { "ENTER", "Select", "ESC", "Back" };

	uis.menuamount = 2;
	uis.hideEarth = qtrue;
	charBottom = EDIT_CHAR_Y + 30 + s_edit.charRows * 62;

	UI_DrawGlassPanel( EDIT_CHAR_X, EDIT_CHAR_Y, EDIT_CHAR_W, charBottom - EDIT_CHAR_Y + 96 );
	UI_DrawSectionTitle( EDIT_CHAR_X + 14, EDIT_CHAR_Y + 12, "CHARACTER" );

	UI_DrawGlassPanel( EDIT_RIGHT_X, EDIT_CHAR_Y, EDIT_RIGHT_W, 398 );
	UI_DrawSectionTitle( EDIT_RIGHT_X + 14, EDIT_CHAR_Y + 12, "LEVEL" );
	UI_DrawSectionTitle( EDIT_RIGHT_X + 14, EDIT_CHAR_Y + 104, "STYLE" );

	Menu_Draw( &s_edit.menu );

	// chosen character, the name pops in when it changes
	def = UI_Bots_Def( s_edit.def );
	if ( def ) {
		pop = UI_AnimFrac( s_edit.defTime, 0, 260 );
		UI_DrawPropScaled( EDIT_CHAR_X + 14 + 10.0f * ( 1.0f - pop ), charBottom + 10, UI_Bots_DisplayName( def ), UI_LEFT|UI_DROPSHADOW, zeq_white, 0.7f );
		UI_DrawStringSized( EDIT_CHAR_X + 14, charBottom + 36, va( "%s  -  %s", def->name, def->player ), UI_LEFT, zeq_text2, 7, 12 );
		UI_DrawStringSized( EDIT_CHAR_X + 14, charBottom + 52, va( "Roster default: %s, %s", UI_Bots_SkillName( def->skill ),
			UI_Bots_Style( UI_Bots_FindStyle( def->archetype ) ) ? UI_Bots_Style( UI_Bots_FindStyle( def->archetype ) )->label : "" ),
			UI_LEFT, zeq_text3, 5, 9 );
	}

	// style values of the chosen style
	style = UI_Bots_Style( s_edit.style );
	if ( style ) {
		y = EDIT_CHAR_Y + 124 + ( ( numBotStyles + 1 ) / 2 ) * 20 + 14;
		for ( i = 0; i < 6; i++ ) {
			UI_DrawStatBar( EDIT_RIGHT_X + 14, y + i * 18, EDIT_RIGHT_W - 28, statLabels[i], style->values[i],
				UI_AnimFrac( s_edit.styleTime, 40 * i, 420 ) );
		}
	}

	UI_DrawKeyHints( hints, 2 );
}

void UI_BotEditMenu( int slot ) {
	int i;

	UI_Bots_Load();
	if ( !numBotDefs ) {
		return;
	}

	memset( &s_edit, 0, sizeof( s_edit ) );
	s_edit.menu.draw = UI_BotEdit_Draw;
	s_edit.menu.key = UI_BotEdit_Key;
	s_edit.menu.fullscreen = qtrue;
	s_edit.menu.wrapAround = qtrue;
	s_edit.slot = ( slot >= 0 && slot < rosterCount ) ? slot : -1;
	s_edit.defTime = uis.realtime;
	s_edit.styleTime = uis.realtime;

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

	s_edit.accept.generic.type		= MTYPE_PTEXT;
	s_edit.accept.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_edit.accept.generic.id		= ID_EDIT_ACCEPT;
	s_edit.accept.generic.callback	= UI_BotEdit_Event;
	s_edit.accept.generic.x			= 28;
	s_edit.accept.generic.y			= 119;
	s_edit.accept.string			= ( s_edit.slot < 0 ) ? "ADD BOT" : "SAVE";
	s_edit.accept.style				= UI_LEFT|UI_DROPSHADOW|UI_TINYFONT;
	s_edit.accept.color				= color_white;

	s_edit.back.generic.type		= MTYPE_PTEXT;
	s_edit.back.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_edit.back.generic.id			= ID_EDIT_BACK;
	s_edit.back.generic.callback	= UI_BotEdit_Event;
	s_edit.back.generic.x			= 28;
	s_edit.back.generic.y			= 157;
	s_edit.back.string				= "BACK";
	s_edit.back.style				= UI_LEFT|UI_DROPSHADOW|UI_TINYFONT;
	s_edit.back.color				= color_white;

	s_edit.charRows = ( numBotDefs + 3 ) / 4;
	for ( i = 0; i < numBotDefs; i++ ) {
		s_edit.chars[i].generic.type		= MTYPE_BITMAP;
		s_edit.chars[i].generic.flags		= QMF_NODEFAULTINIT;
		s_edit.chars[i].generic.id			= ID_EDIT_CHAR + i;
		s_edit.chars[i].generic.callback	= UI_BotEdit_Event;
		s_edit.chars[i].generic.ownerdraw	= UI_BotEdit_DrawChar;
		s_edit.chars[i].generic.left		= EDIT_CHAR_X + 12 + ( i % 4 ) * EDIT_CELL;
		s_edit.chars[i].generic.top			= EDIT_CHAR_Y + 32 + ( i / 4 ) * 62;
		s_edit.chars[i].generic.right		= s_edit.chars[i].generic.left + 48;
		s_edit.chars[i].generic.bottom		= s_edit.chars[i].generic.top + 54;
	}

	for ( i = 0; i < 3; i++ ) {
		s_edit.skills[i].generic.type		= MTYPE_BITMAP;
		s_edit.skills[i].generic.flags		= QMF_NODEFAULTINIT;
		s_edit.skills[i].generic.id			= ID_EDIT_SKILL + i;
		s_edit.skills[i].generic.callback	= UI_BotEdit_Event;
		s_edit.skills[i].generic.ownerdraw	= UI_BotEdit_DrawSkill;
		s_edit.skills[i].generic.left		= EDIT_RIGHT_X + 12 + i * 70;
		s_edit.skills[i].generic.top		= EDIT_CHAR_Y + 32;
		s_edit.skills[i].generic.right		= s_edit.skills[i].generic.left + 66;
		s_edit.skills[i].generic.bottom		= s_edit.skills[i].generic.top + 54;
	}

	for ( i = 0; i < numBotStyles; i++ ) {
		s_edit.styles[i].generic.type		= MTYPE_BITMAP;
		s_edit.styles[i].generic.flags		= QMF_NODEFAULTINIT;
		s_edit.styles[i].generic.id			= ID_EDIT_STYLE + i;
		s_edit.styles[i].generic.callback	= UI_BotEdit_Event;
		s_edit.styles[i].generic.ownerdraw	= UI_BotEdit_DrawStyle;
		s_edit.styles[i].generic.left		= EDIT_RIGHT_X + 12 + ( i % 2 ) * 104;
		s_edit.styles[i].generic.top		= EDIT_CHAR_Y + 124 + ( i / 2 ) * 20;
		s_edit.styles[i].generic.right		= s_edit.styles[i].generic.left + 100;
		s_edit.styles[i].generic.bottom		= s_edit.styles[i].generic.top + 16;
	}

	Menu_AddItem( &s_edit.menu, &s_edit.accept );
	Menu_AddItem( &s_edit.menu, &s_edit.back );
	for ( i = 0; i < numBotDefs; i++ ) {
		Menu_AddItem( &s_edit.menu, &s_edit.chars[i] );
	}
	for ( i = 0; i < 3; i++ ) {
		Menu_AddItem( &s_edit.menu, &s_edit.skills[i] );
	}
	for ( i = 0; i < numBotStyles; i++ ) {
		Menu_AddItem( &s_edit.menu, &s_edit.styles[i] );
	}

	UI_PushMenu( &s_edit.menu );
	uis.menuamount = 2;
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
static void UI_BotEditLive( int clientNum, int def, int skill, int style ) {
	UI_BotEditMenu( -1 );
	if ( !numBotDefs ) {
		return;
	}
	s_edit.live = qtrue;
	s_edit.liveClient = clientNum;
	s_edit.accept.string = ( clientNum >= 0 ) ? "APPLY" : "ADD BOT";
	if ( clientNum >= 0 && def >= 0 ) {
		s_edit.def = def;
		s_edit.skill = skill;
		s_edit.style = style;
		s_edit.touched = qtrue;
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

#define PB_X				110
#define PB_Y				40
#define PB_W				420
#define PB_H				400
#define PB_ROW_Y			(PB_Y + 52)
#define PB_ROW_H			26
#define PB_BUTTON_W			96

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
		UI_BotEditLive( bot->client, bot->def, bot->skill, bot->style );
		return;
	}
	if ( id >= ID_PB_KICK && id < ID_PB_KICK + UI_MAX_ROSTER ) {
		UI_PauseBots_Kick( id - ID_PB_KICK );
		return;
	}

	switch ( id ) {
	case ID_PB_ADD:
		if ( s_pauseBots.count < UI_MAX_ROSTER ) {
			UI_BotEditLive( -1, -1, 0, 0 );
		}
		break;
	case ID_PB_FREEZE:
		trap_Cvar_Set( "bot_pause", trap_Cvar_VariableValue( "bot_pause" ) ? "0" : "1" );
		break;
	case ID_PB_KICKALL:
		trap_Cmd_ExecuteText( EXEC_APPEND, "remove_allbots\n" );
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
	return UI_AnimFrac( uis.focusTime, 0, 160 );
}

static void UI_PauseBots_DrawRow( void *self ) {
	menubitmap_s	*b;
	int				row;
	pauseBot_t		*bot;
	uiBotStyle_t	*style;
	float			focus;
	float			y;

	b = (menubitmap_s *)self;
	row = b->generic.id - ID_PB_ROW;
	if ( row < 0 || row >= s_pauseBots.count ) {
		return;
	}
	bot = &s_pauseBots.bots[row];
	style = UI_Bots_Style( bot->style );
	focus = UI_PauseBots_Focus( self );
	y = b->generic.top;

	UI_DrawFocusRow( b->generic.left, y, b->generic.right - b->generic.left, PB_ROW_H - 4, focus );
	UI_DrawPortrait( b->generic.left + 4, y + 1, 20, UI_Bots_Portrait( bot->model ), qfalse, focus );
	UI_DrawStringSized( b->generic.left + 30, y + 1, bot->name, UI_LEFT|UI_DROPSHADOW, zeq_white, 7, 12 );
	UI_DrawStringSized( b->generic.left + 30, y + 12, va( "%s  -  slot %i", bot->model, bot->client ), UI_LEFT, zeq_text3, 5, 9 );
	UI_DrawPips( b->generic.left + 190, y + 5, bot->skill, 3, UI_AnimFrac( uis.transitionTime, 120 + row * 40, 400 ) );
	UI_DrawStringSized( b->generic.left + 190, y + 12, UI_Bots_SkillName( bot->skill ), UI_LEFT, zeq_text2, 5, 9 );
	UI_DrawStringSized( b->generic.left + 240, y + 6, style ? style->label : "", UI_LEFT, zeq_text2, 6, 11 );
}

static void UI_PauseBots_DrawKick( void *self ) {
	menubitmap_s	*b;
	vec4_t			color;
	qboolean		hot;

	b = (menubitmap_s *)self;
	hot = ( Menu_ItemAtCursor( &s_pauseBots.menu ) == self );
	color[0] = 0.94f; color[1] = 0.63f; color[2] = 0.53f; color[3] = hot ? 1.0f : 0.6f;
	UI_DrawRect( b->generic.left, b->generic.top, 12, 12, color );
	UI_DrawStringSized( b->generic.left + 6, b->generic.top, "x", UI_CENTER, color, 7, 12 );
}

static void UI_PauseBots_DrawButton( void *self ) {
	menubitmap_s	*b;
	float			focus;
	const char		*label;
	float			*color;
	qboolean		frozen;

	b = (menubitmap_s *)self;
	focus = UI_PauseBots_Focus( self );
	frozen = trap_Cvar_VariableValue( "bot_pause" ) != 0;
	switch ( b->generic.id ) {
	case ID_PB_ADD:		label = "ADD BOT"; break;
	case ID_PB_FREEZE:	label = frozen ? "UNFREEZE" : "FREEZE"; break;
	case ID_PB_KICKALL:	label = "KICK ALL"; break;
	default:			label = "BACK"; break;
	}

	if ( b->generic.flags & QMF_GRAYED ) {
		focus = 0.0f;
		color = zeq_text3;
	}
	else if ( focus > 0.0f ) {
		color = zeq_white;
	}
	else if ( b->generic.id == ID_PB_FREEZE && frozen ) {
		color = zeq_gold;
	}
	else {
		color = zeq_text2;
	}

	UI_DrawKiBar( b->generic.left, b->generic.top, b->generic.right - b->generic.left, b->generic.bottom - b->generic.top, focus > 0.0f, focus );
	UI_DrawPropScaled( ( b->generic.left + b->generic.right ) * 0.5f - 4, b->generic.top + 7, label,
		UI_CENTER|UI_DROPSHADOW|( focus > 0.0f ? UI_PULSE : 0 ), color, 0.42f );
}

// rows and buttons follow the live list
static void UI_PauseBots_Sync( void ) {
	int				i;
	menucommon_s	*item;

	UI_PauseBots_Scan();
	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		if ( i < s_pauseBots.count ) {
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
		Menu_SetCursorToItem( &s_pauseBots.menu, s_pauseBots.count ? (void *)&s_pauseBots.rows[0] : (void *)&s_pauseBots.add );
	}
}

static void UI_PauseBots_Draw( void ) {
	float			bias;
	vec4_t			shade;
	qboolean		frozen;
	static const char *hints[] = { "ENTER", "Edit", "DEL", "Kick", "ESC", "Back" };

	UI_PauseBots_Sync();
	frozen = trap_Cvar_VariableValue( "bot_pause" ) != 0;

	// dim the running game behind
	bias = ( uis.scale > 0.0f ) ? ( uis.bias / uis.scale ) : 0.0f;
	shade[0] = 0.02f; shade[1] = 0.02f; shade[2] = 0.10f; shade[3] = 0.62f;
	UI_FillRect( -bias - uis.drawOffsetX, 0, 640 + 2 * bias, 480, shade );

	UI_DrawGlassPanel( PB_X, PB_Y, PB_W, PB_H );
	UI_DrawSectionTitle( PB_X + 14, PB_Y + 12, "BOTS IN THIS MATCH" );
	UI_DrawStringSized( PB_X + PB_W - 14, PB_Y + 12, va( "%i of %i", s_pauseBots.count, UI_MAX_ROSTER ), UI_RIGHT, zeq_text3, 6, 11 );
	UI_DrawStringSized( PB_X + 14, PB_Y + 30, frozen ? "Bots are frozen until you unfreeze them." : "Bots keep fighting while this menu is open.",
		UI_LEFT, frozen ? zeq_gold : zeq_text2, 6, 11 );

	if ( !s_pauseBots.count ) {
		UI_DrawStringSized( PB_X + PB_W * 0.5f, PB_Y + 150, "No bots in this match.", UI_CENTER, zeq_white, 8, 14 );
		UI_DrawStringSized( PB_X + PB_W * 0.5f, PB_Y + 170, "ADD BOT brings one in.", UI_CENTER, zeq_text2, 7, 12 );
	}

	Menu_Draw( &s_pauseBots.menu );

	UI_DrawKeyHints( hints, 3 );
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
	return Menu_DefaultKey( &s_pauseBots.menu, key );
}

static void UI_PauseBots_Button( menubitmap_s *b, int id, int index ) {
	b->generic.type			= MTYPE_BITMAP;
	b->generic.flags		= QMF_NODEFAULTINIT;
	b->generic.id			= id;
	b->generic.callback		= UI_PauseBots_Event;
	b->generic.ownerdraw	= UI_PauseBots_DrawButton;
	b->generic.left			= PB_X + 14 + index * ( PB_BUTTON_W + 2 );
	b->generic.top			= PB_Y + PB_H - 44;
	b->generic.right		= b->generic.left + PB_BUTTON_W;
	b->generic.bottom		= b->generic.top + 28;
}

void UI_PauseBotsMenu( void ) {
	int i;

	UI_Bots_Load();

	memset( &s_pauseBots, 0, sizeof( s_pauseBots ) );
	s_pauseBots.menu.draw = UI_PauseBots_Draw;
	s_pauseBots.menu.key = UI_PauseBots_Key;
	s_pauseBots.menu.fullscreen = qfalse;
	s_pauseBots.menu.wrapAround = qtrue;

	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		s_pauseBots.rows[i].generic.type		= MTYPE_BITMAP;
		s_pauseBots.rows[i].generic.flags		= QMF_NODEFAULTINIT|QMF_HIDDEN|QMF_INACTIVE;
		s_pauseBots.rows[i].generic.id			= ID_PB_ROW + i;
		s_pauseBots.rows[i].generic.callback	= UI_PauseBots_Event;
		s_pauseBots.rows[i].generic.ownerdraw	= UI_PauseBots_DrawRow;
		s_pauseBots.rows[i].generic.left		= PB_X + 12;
		s_pauseBots.rows[i].generic.right		= PB_X + PB_W - 32;
		s_pauseBots.rows[i].generic.top			= PB_ROW_Y + i * PB_ROW_H;
		s_pauseBots.rows[i].generic.bottom		= s_pauseBots.rows[i].generic.top + PB_ROW_H - 4;

		s_pauseBots.kicks[i].generic.type		= MTYPE_BITMAP;
		s_pauseBots.kicks[i].generic.flags		= QMF_NODEFAULTINIT|QMF_MOUSEONLY|QMF_HIDDEN|QMF_INACTIVE;
		s_pauseBots.kicks[i].generic.id			= ID_PB_KICK + i;
		s_pauseBots.kicks[i].generic.callback	= UI_PauseBots_Event;
		s_pauseBots.kicks[i].generic.ownerdraw	= UI_PauseBots_DrawKick;
		s_pauseBots.kicks[i].generic.left		= PB_X + PB_W - 28;
		s_pauseBots.kicks[i].generic.right		= s_pauseBots.kicks[i].generic.left + 12;
		s_pauseBots.kicks[i].generic.top		= PB_ROW_Y + i * PB_ROW_H + 5;
		s_pauseBots.kicks[i].generic.bottom		= s_pauseBots.kicks[i].generic.top + 12;
	}

	UI_PauseBots_Button( &s_pauseBots.add, ID_PB_ADD, 0 );
	UI_PauseBots_Button( &s_pauseBots.freeze, ID_PB_FREEZE, 1 );
	UI_PauseBots_Button( &s_pauseBots.kickall, ID_PB_KICKALL, 2 );
	UI_PauseBots_Button( &s_pauseBots.back, ID_PB_BACK, 3 );

	for ( i = 0; i < UI_MAX_ROSTER; i++ ) {
		Menu_AddItem( &s_pauseBots.menu, &s_pauseBots.rows[i] );
		Menu_AddItem( &s_pauseBots.menu, &s_pauseBots.kicks[i] );
	}
	Menu_AddItem( &s_pauseBots.menu, &s_pauseBots.add );
	Menu_AddItem( &s_pauseBots.menu, &s_pauseBots.freeze );
	Menu_AddItem( &s_pauseBots.menu, &s_pauseBots.kickall );
	Menu_AddItem( &s_pauseBots.menu, &s_pauseBots.back );

	UI_PushMenu( &s_pauseBots.menu );
	trap_Cvar_Set( "cl_paused", "0" );
	UI_PauseBots_Sync();
	Menu_SetCursorToItem( &s_pauseBots.menu, s_pauseBots.count ? (void *)&s_pauseBots.rows[0] : (void *)&s_pauseBots.add );
}
