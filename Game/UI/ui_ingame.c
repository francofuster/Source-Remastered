/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
/*
=======================================================================

INGAME MENU

Menu redesign: the match is dimmed, Shenron wraps a solid panel (the old
frame let his body cross the text) and the entries are ki buttons.

=======================================================================
*/

#include "ui_local.h"

#define INGAME_SHENRON			"interface/menu/shenron.png"

#define ID_SETUP				13
#define ID_PLAYER				14
#define ID_CAMERA				15
#define ID_LEAVEARENA			16
#define ID_RESTART				17
#define ID_QUIT					18
#define ID_RESUME				19
#define ID_BOTS					20

#define PAUSE_ENTRIES			7

// PC: a column of buttons. Phone: RESUME on top, the rest in a 2 x 3 grid
typedef struct {
	int		panelX, panelY, panelW, panelH;
	int		shenronX, shenronY, shenronW, shenronH;
	int		titleSize;
} pauseLayout_t;

static const pauseLayout_t pauseLayouts[2] = {
	{ 483, 164, 314, 381,	381, 8, 576, 707,	17 },
	{ 212, 54, 420, 304,	268, 4, 310, 380,	11 }
};

#define PL	( pauseLayouts[s_ingame.menu.mobile ? 1 : 0] )

typedef struct {
	menuframework_s	menu;
	menubitmap_s	entries[PAUSE_ENTRIES];
	qhandle_t		shenron;
} ingamemenu_t;

static ingamemenu_t	s_ingame;

static const int pauseIds[PAUSE_ENTRIES] = { ID_RESUME, ID_SETUP, ID_PLAYER, ID_BOTS, ID_CAMERA, ID_RESTART, ID_LEAVEARENA };
static const char *pauseLabels[PAUSE_ENTRIES] = { "RESUME", "OPTIONS", "FIGHTER", "BOTS", "CAMERA", "RESTART", "LEAVE MATCH" };

static void InGame_EntryRect( int index, int *x, int *y, int *w, int *h ) {
	if ( !s_ingame.menu.mobile ) {
		*x = 527; *y = 210 + index * 44; *w = 244; *h = 38;
		return;
	}
	if ( index == 0 ) {
		*x = 232; *y = 90; *w = 380; *h = 56;
		return;
	}
	*x = 232 + ( ( index - 1 ) % 2 ) * 194;
	*y = 156 + ( ( index - 1 ) / 2 ) * 60;
	*w = 186;
	*h = 52;
}

/*
=================
InGame_LeaveAction
=================
*/
static void InGame_LeaveAction( qboolean result ) {
	if ( !result ) {
		return;
	}
	// JUHOX: reset edit mode
#if MAPLENSFLARES
	trap_Cvar_Set("g_editmode", "0");
#endif
	trap_Cmd_ExecuteText( EXEC_APPEND, "disconnect\n" );
}


/*
=================
InGame_RestartAction
=================
*/
static void InGame_RestartAction( qboolean result ) {
	if( !result ) {
		return;
	}

	UI_PopMenu();
	trap_Cmd_ExecuteText( EXEC_APPEND, "map_restart 0\n" );
}


/*
=================
InGame_QuitAction
=================
*/
static void InGame_QuitAction( qboolean result ) {
	if( !result ) {
		return;
	}
	UI_PopMenu();
	UI_CreditMenu();
}


/*
=================
InGame_Event
=================
*/
void InGame_Event( void *ptr, int notification ) {
	if( notification != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_SETUP:
		UI_SystemSettingsMenu();
		break;

	case ID_PLAYER:
		UI_PlayerModelMenu();
		break;

	case ID_CAMERA:
		UI_CameraMenu();
		break;

	case ID_BOTS:
		UI_PauseBotsMenu();
		break;

	case ID_LEAVEARENA:
		// it ended the match at once: ask first
		UI_ConfirmMenu( "LEAVE MATCH?", 0, InGame_LeaveAction );
		break;

	case ID_RESTART:
		UI_ConfirmMenu( "RESTART GAME?", 0, InGame_RestartAction );
		break;

	case ID_QUIT:
		UI_ConfirmMenu( "EXIT GAME?",  0, InGame_QuitAction );
		break;

	case ID_RESUME:
		UI_PopMenu();
		break;
	}
}

/*
=================
InGame_DrawEntry
=================
*/
static void InGame_DrawEntry( void *self ) {
	menubitmap_s	*b;
	int				index;
	float			focus;
	float			enter;
	qboolean		disabled;
	vec4_t			warn;

	b = (menubitmap_s *)self;
	index = b - s_ingame.entries;
	disabled = ( b->generic.flags & QMF_GRAYED ) != 0;
	focus = disabled ? 0.0f : UI_DFocus( &s_ingame.menu, self );
	enter = UI_AnimFrac( uis.transitionTime, 60 + index * 40, 260 );
	UI_DKiButton( b->generic.x - 24.0f * ( 1.0f - enter ) + ( s_ingame.menu.mobile ? 0.0f : 6.0f * focus ), b->generic.y + ( UI_TouchPressed( self ) ? 2.0f : 0.0f ),
		b->width, b->height, pauseLabels[index], ( s_ingame.menu.mobile && index == 0 ) ? "Back to the fight" : NULL, focus, disabled, qfalse );

	// dragon ball marker on the left of the focused entry (keyboard and pad)
	if ( focus > 0.0f && !s_ingame.menu.mobile ) {
		trap_R_SetColor( NULL );
		UI_DPic( PL.panelX + 10, b->generic.y + 5 + 1.5f * sin( uis.realtime / 300.0f ), 24, 29, uis.dragonBall );
	}
	if ( pauseIds[index] == ID_LEAVEARENA && focus <= 0.0f ) {
		warn[0] = 0.94f; warn[1] = 0.63f; warn[2] = 0.53f; warn[3] = s_ingame.menu.mobile ? 0.6f : 0.35f;
		UI_DFill( b->generic.x + 4, b->generic.y + b->height - 3, b->width * 0.6f, s_ingame.menu.mobile ? 2.0f : 1.5f, warn );
	}
}

/*
=================
InGame_MenuDraw
=================
*/
/*
=================
InGame_DrawTopStrip

the strip over the match: map and KO limit on the left (PAUSED > BOTS on
the bots screen), who is playing on the right; anchored to the screen edges
=================
*/
static void InGame_Players( char *out, int size, qboolean withMap, const char *map ) {
	char	info[MAX_INFO_STRING];
	int		humans, bots, i;

	humans = bots = 0;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		trap_GetConfigString( CS_PLAYERS + i, info, sizeof( info ) );
		if ( !info[0] ) {
			continue;
		}
		if ( Info_ValueForKey( info, "skill" )[0] ) {
			bots++;
		}
		else {
			humans++;
		}
	}
	Com_sprintf( out, size, "%s%s%s  -  %i human%s  -  %i bot%s", withMap ? map : "", withMap ? "  -  " : "",
		trap_Cvar_VariableValue( "sv_running" ) ? "Local match" : "Online",
		humans, humans == 1 ? "" : "s", bots, bots == 1 ? "" : "s" );
}

// phone canvas: a 40 dp strip
static void InGame_DrawTopStripPhone( qboolean botsScreen, float e, const float *shade ) {
	char	info[MAX_INFO_STRING];
	char	map[64];
	char	players[128];
	int		value;

	UI_DFill( -200 - e, 0, 1244 + 2 * e, 40, shade );
	trap_GetConfigString( CS_SERVERINFO, info, sizeof( info ) );
	Q_strncpyz( map, Info_ValueForKey( info, "mapname" ), sizeof( map ) );
	Q_strupr( map );
	if ( botsScreen ) {
		InGame_Players( players, sizeof( players ), qtrue, map );
		UI_DText( 814 + e, 13, players, 13, UI_RIGHT, zeq_text2 );
		return;
	}
	UI_DProp( 30 - e, 13, map, 13, UI_LEFT, zeq_white );
	value = atoi( Info_ValueForKey( info, "fraglimit" ) );
	UI_DText( 44 - e + UI_DPropWidth( map, 13 ), 13, value ? va( "KO limit %i", value ) : "No KO limit", 13, UI_LEFT, zeq_text2 );
	InGame_Players( players, sizeof( players ), qfalse, map );
	UI_DText( 814 + e, 13, players, 13, UI_RIGHT, zeq_text2 );
}

void InGame_DrawTopStrip( qboolean botsScreen ) {
	char		info[MAX_INFO_STRING];
	char		map[64];
	char		players[128];
	int			humans;
	int			bots;
	int			i;
	int			value;
	float		e;
	vec4_t		shade;

	e = UI_DExtra();
	shade[0] = 0.02f; shade[1] = 0.024f; shade[2] = 0.10f; shade[3] = 0.6f;
	if ( uis.dMobile ) {
		InGame_DrawTopStripPhone( botsScreen, e, shade );
		return;
	}
	UI_DFill( -400 - e, 0, 2080 + 2 * e, 64, shade );
	trap_GetConfigString( CS_SERVERINFO, info, sizeof( info ) );
	Q_strncpyz( map, Info_ValueForKey( info, "mapname" ), sizeof( map ) );
	Q_strupr( map );
	if ( botsScreen ) {
		UI_DProp( 40 - e, 24, "PAUSED  >", 15, UI_LEFT, zeq_text3 );
		UI_DProp( 54 - e + UI_DPropWidth( "PAUSED  >", 15 ), 22, "BOTS", 19, UI_LEFT, zeq_white );
	}
	else {
		UI_DProp( 40 - e, 22, map, 19, UI_LEFT, zeq_white );
		value = atoi( Info_ValueForKey( info, "fraglimit" ) );
		UI_DText( 60 - e + UI_DPropWidth( map, 19 ), 22, value ? va( "KO limit %i", value ) : "No KO limit", 19, UI_LEFT, zeq_text2 );
	}
	humans = bots = 0;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		trap_GetConfigString( CS_PLAYERS + i, info, sizeof( info ) );
		if ( !info[0] ) {
			continue;
		}
		if ( Info_ValueForKey( info, "skill" )[0] ) {
			bots++;
		}
		else {
			humans++;
		}
	}
	Com_sprintf( players, sizeof( players ), "%s%s%s  -  %i human%s  -  %i bot%s", botsScreen ? map : "", botsScreen ? "  -  " : "",
		trap_Cvar_VariableValue( "sv_running" ) ? "Local match" : "Online",
		humans, humans == 1 ? "" : "s", bots, bots == 1 ? "" : "s" );
	UI_DText( 1240 + e, 22, players, 19, UI_RIGHT, zeq_text2 );
}

/*
=================
InGame_MenuDraw
=================
*/
static void InGame_MenuDraw( void ) {
	static const char *hints[] = { "UD", "Navigate", "ENTER", "Select", "ESC", "Resume" };
	float		bias;
	vec4_t		shade;
	vec4_t		edge;
	vec4_t		top;
	vec4_t		mid;
	vec4_t		warm;

	// dim the match behind
	bias = ( uis.scale > 0.0f ) ? ( uis.bias / uis.scale ) : 0.0f;
	shade[0] = 0.02f; shade[1] = 0.024f; shade[2] = 0.10f; shade[3] = 0.55f;
	UI_FillRect( -bias - uis.drawOffsetX, 0, 640 + 2 * bias, 480, shade );
	InGame_DrawTopStrip( qfalse );

	// Shenron around the panel: rounded, navy fading to a warm glow at the bottom
	trap_R_SetColor( NULL );
	UI_DPic( PL.shenronX, PL.shenronY, PL.shenronW, PL.shenronH, s_ingame.shenron );
	edge[0] = 0.33f; edge[1] = 0.59f; edge[2] = 1.0f; edge[3] = 0.35f;
	UI_DRoundGlow( PL.panelX, PL.panelY, PL.panelW, PL.panelH, 24, edge );
	top[0] = 0.063f; top[1] = 0.075f; top[2] = 0.22f; top[3] = 0.97f;
	mid[0] = 0.039f; mid[1] = 0.047f; mid[2] = 0.149f; mid[3] = 0.97f;
	warm[0] = 0.259f; warm[1] = 0.165f; warm[2] = 0.094f; warm[3] = 0.97f;
	UI_DRoundPanel( PL.panelX, PL.panelY, PL.panelW, PL.panelH, 12, top, mid, 0.72f, warm );
	edge[0] = 0.43f; edge[1] = 0.56f; edge[2] = 0.94f; edge[3] = 1.0f;
	UI_DRoundEdge( PL.panelX, PL.panelY, PL.panelW, PL.panelH, 12, edge );
	edge[0] = edge[1] = edge[2] = 1.0f; edge[3] = 0.18f;
	UI_DFill( PL.panelX + 12, PL.panelY + 1.5f, PL.panelW - 24, 1, edge );
	if ( s_ingame.menu.mobile ) {
		UI_DProp( PL.panelX + 20, PL.panelY + 14, "PAUSED", PL.titleSize, UI_LEFT, zeq_gold );
	}
	else {
		UI_DSectionTitle( PL.panelX + 26, PL.panelY + 20, "PAUSED" );
	}

	Menu_Draw( &s_ingame.menu );

	UI_DHintsCentered( hints, 3 );
}


/*
=================
InGame_MenuInit
=================
*/
void InGame_MenuInit( void ) {
	int i;

	memset( &s_ingame, 0 ,sizeof(ingamemenu_t) );

	InGame_Cache();
	s_ingame.menu.mobile = UI_MobileLayout();
	UI_DCanvas( s_ingame.menu.mobile );

	s_ingame.menu.wrapAround = qtrue;
	s_ingame.menu.fullscreen = qfalse;
	s_ingame.menu.draw = InGame_MenuDraw;

	for ( i = 0; i < PAUSE_ENTRIES; i++ ) {
		s_ingame.entries[i].generic.type		= MTYPE_BITMAP;
		s_ingame.entries[i].generic.flags		= QMF_NODEFAULTINIT;
		s_ingame.entries[i].generic.id			= pauseIds[i];
		s_ingame.entries[i].generic.callback	= InGame_Event;
		s_ingame.entries[i].generic.ownerdraw	= InGame_DrawEntry;
		{
			int x, y, w, h;

			InGame_EntryRect( i, &x, &y, &w, &h );
			UI_DItem( &s_ingame.entries[i], x, y, w, h );
		}

		// restart and bots belong to the host
		if ( ( pauseIds[i] == ID_RESTART || pauseIds[i] == ID_BOTS ) && !trap_Cvar_VariableValue( "sv_running" ) ) {
			s_ingame.entries[i].generic.flags |= QMF_GRAYED;
		}
		Menu_AddItem( &s_ingame.menu, &s_ingame.entries[i] );
	}
}


/*
=================
InGame_Cache
=================
*/
void InGame_Cache( void ) {
	s_ingame.shenron = trap_R_RegisterShaderNoMip( INGAME_SHENRON );
}


/*
=================
UI_InGameMenu
=================
*/
void UI_InGameMenu( void ) {
	// force as top level menu
	uis.menusp = 0;

	// set menu cursor to a nice location
	uis.cursorx = 320;
	uis.cursory = 240;

	InGame_MenuInit();
	UI_PushMenu( &s_ingame.menu );
}
