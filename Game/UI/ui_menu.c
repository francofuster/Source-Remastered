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

MAIN MENU

Menu redesign: laid out on the 1280x720 design canvas (see ui_shared.c).
The 3D earth and the 3D player stay, everything else is the new art.

=======================================================================
*/


#include "ui_local.h"

#define ID_MULTIPLAYER			11
#define ID_SETUP				12
#define ID_FIGHTER				13
#define ID_EXIT					17

#define	ID_MODEL				18
#define MAX_NAMELENGTH			20

#define MAIN_BUTTON_X			60
#define MAIN_BUTTON_Y			214
#define MAIN_BUTTON_W			330
#define MAIN_BUTTON_H			66
#define MAIN_BUTTON_STEP		80

typedef struct {
	menuframework_s	menu;

	menubitmap_s	multiplayer;
	menubitmap_s	fighter;
	menubitmap_s	setup;
	menubitmap_s	exit;
	menufield_s		name;
	menubitmap_s	player;
	playerInfo_t	playerinfo;
	char			playerModel[MAX_QPATH];
} mainmenu_t;

static mainmenu_t s_main;
void MainMenu_Save(void);

typedef struct {
	menuframework_s menu;
	char errorMessage[4096];
} errorMessage_t;

static errorMessage_t s_errorMessage;

static const char *mainLabels[4] = { "PLAY", "FIGHTER", "OPTIONS", "EXIT" };
static const char *mainCaptions[4] = {
	"Join a server or host a match",
	"Character, skin and name",
	"Controls, video and audio",
	"Leave ZEQ2 Lite"
};

/*
=================
MainMenu_DrawPlayer
=================
*/
static void MainMenu_DrawPlayer( void *self ) {
	menubitmap_s	*b;
	vec3_t			viewangles;
	char			buf[MAX_QPATH];

	trap_Cvar_VariableStringBuffer( "model", buf, sizeof( buf ) );
	if ( strcmp( buf, s_main.playerModel ) != 0 ) {
		UI_PlayerInfo_SetModel( &s_main.playerinfo, buf );
		strcpy( s_main.playerModel, buf );

		viewangles[YAW]   = 180 - 30;
		viewangles[PITCH] = 0;
		viewangles[ROLL]  = 0;
		UI_PlayerInfo_SetInfo( &s_main.playerinfo, ANIM_IDLE, ANIM_IDLE, viewangles, vec3_origin, WP_NONE, qfalse );
	}

	b = (menubitmap_s*) self;
	UI_DrawPlayer( b->generic.x, b->generic.y, b->width, b->height, &s_main.playerinfo, uis.realtime / PLAYER_MODEL_SPEED );
}

/*
=================
MainMenu_DrawButton

big slanted button with its caption, they slide in one after the other
=================
*/
static void MainMenu_DrawButton( void *self ) {
	menubitmap_s	*b;
	int				index;
	float			enter;
	float			focus;
	float			shift;

	b = (menubitmap_s *)self;
	index = ( b->generic.y - MAIN_BUTTON_Y ) / MAIN_BUTTON_STEP;
	// x already carries the edge offset of wide screens
	enter = UI_AnimFrac( uis.transitionTime, 80 + index * 60, 320 );
	focus = UI_DFocus( &s_main.menu, self );
	shift = -50.0f * ( 1.0f - enter ) + 10.0f * focus;
	UI_DKiButton( b->generic.x + shift, b->generic.y, b->width, b->height, mainLabels[index], mainCaptions[index], focus, qfalse, qtrue );
}

/*
=================
MainMenu_DrawName

name plate under the fighter: the model on the left, the editable
player name on the right
=================
*/
static void MainMenu_DrawName( void *self ) {
	menufield_s		*f;
	qboolean		focus;
	char			*txt;
	char			clean[64];
	float			x, y;
	float			e;
	vec4_t			edge;

	f = (menufield_s*)self;
	focus = ( f->generic.parent->cursor == f->generic.menuPosition );

	e = UI_DExtra();
	UI_DNamePlate( 920 + e, 638, 320, 48 );
	if ( focus ) {
		edge[0] = 0.47f; edge[1] = 0.63f; edge[2] = 1.0f;
		edge[3] = 0.6f + 0.3f * sin( uis.realtime / 220.0f );
		UI_DFrame( 934 + e, 640, 304, 44, edge );
	}

	// only the name, from the right; colors shown unless it is being edited
	txt = f->field.buffer;
	Q_strncpyz( clean, txt, sizeof( clean ) );
	if ( focus ) {
		Q_CleanStr( clean );
	}
	x = 1222 + e;
	y = 648;
	UI_DText( x, y, focus ? clean : txt, 26, UI_RIGHT|UI_DROPSHADOW, zeq_white );
	if ( focus && ( ( uis.realtime / 400 ) & 1 ) ) {
		UI_DFill( x + 3, y + 2, 2, 24, zeq_white );
	}
}

/*
=================
MainMenu_ExitAction
=================
*/
static void MainMenu_ExitAction( qboolean result ) {
	if( !result ) {
		return;
	}
	UI_PopMenu();
	UI_CreditMenu();
}



/*
=================
Main_MenuEvent
=================
*/
void Main_MenuEvent (void* ptr, int event) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_MULTIPLAYER:
		MainMenu_Save();
		UI_ArenaServersMenu();
		break;
	case ID_SETUP:
		MainMenu_Save();
		UI_SystemSettingsMenu();
		break;
	case ID_FIGHTER:
	case ID_MODEL:
		MainMenu_Save();
		UI_PlayerModelMenu();
		break;
	case ID_EXIT:
		MainMenu_Save();
		UI_ConfirmMenu( "Quit?", 0, MainMenu_ExitAction );
		break;
	}
}


/*
===============
MainMenu_Cache
===============
*/
void MainMenu_Cache( void ) {}
void MainMenu_Save ( void ) {
	trap_Cvar_Set("name", s_main.name.field.buffer );
}



sfxHandle_t ErrorMessage_Key(int key)
{
	trap_Cvar_Set( "com_errorMessage", "" );
	UI_MainMenu();
	return (menu_null_sound);
}

/*
===============
Main_MenuDraw
TTimo: this function is common to the main menu and errorMessage menu
===============
*/

static void Main_MenuDraw( void ) {
	static const char *hints[] = { "ARROWS", "Navigate", "ENTER", "Select" };

	uis.menuamount = -1;
	uis.hideEarth = qfalse;
	uis.showFrame = qfalse;

	UI_DLogo( 40 - UI_DExtra(), 30, 270 );
	UI_DProp( 1244 + UI_DExtra(), 32, "REMASTER  v1.3", 13, UI_RIGHT, zeq_text3 );
	trap_R_SetColor( NULL );
	UI_DPic( 930 + UI_DExtra(), 584, 280, 90, uis.blobShadow );

	if ( uis.activemenu == &s_errorMessage.menu ) {
		UI_DGlass( 400, 250, 480, 200 );
		UI_DProp( 640, 274, "ERROR", 20, UI_CENTER, zeq_gold );
		UI_DText( 640, 320, s_errorMessage.errorMessage, 18, UI_CENTER, zeq_white );
		UI_DText( 640, 410, "Press any key", 16, UI_CENTER, zeq_text3 );
		return;
	}

	Menu_Draw( &s_main.menu );
	UI_DHints( hints, 2 );
}


/*
===============
UI_MainMenu

The main menu only comes up when not in a game,
so make sure that the attract loop server is down
and that local cinematics are killed
===============
*/
static void MainMenu_Button( menubitmap_s *b, int id, int index ) {
	b->generic.type			= MTYPE_BITMAP;
	b->generic.flags		= QMF_NODEFAULTINIT;
	b->generic.id			= id;
	b->generic.callback		= Main_MenuEvent;
	b->generic.ownerdraw	= MainMenu_DrawButton;
	UI_DItem( b, MAIN_BUTTON_X - UI_DExtra(), MAIN_BUTTON_Y + index * MAIN_BUTTON_STEP, MAIN_BUTTON_W, MAIN_BUTTON_H );
}

void UI_MainMenu( void ) {
	float	vx, vy, vw, vh;

	trap_S_StopBackgroundTrack();
	trap_S_StartBackgroundTrack("music/menu.ogg", "music/menu.ogg");
	trap_Cvar_Set( "sv_killserver", "1" );

	// set menu cursor to a nice location
	uis.cursorx = 320;
	uis.cursory = 240;

#if MAPLENSFLARES	// JUHOX: reset g_editmode
	trap_Cvar_Set("g_editmode", "0");
#endif

	memset( &s_main, 0 ,sizeof(mainmenu_t) );
	memset( &s_errorMessage, 0 ,sizeof(errorMessage_t) );


	// com_errorMessage would need that too
	MainMenu_Cache();

	trap_Cvar_VariableStringBuffer( "com_errorMessage", s_errorMessage.errorMessage, sizeof(s_errorMessage.errorMessage) );
	if (strlen(s_errorMessage.errorMessage))
	{
		s_errorMessage.menu.draw = Main_MenuDraw;
		s_errorMessage.menu.key = ErrorMessage_Key;
		s_errorMessage.menu.fullscreen = qtrue;
		s_errorMessage.menu.wrapAround = qtrue;
		s_errorMessage.menu.showlogo = qtrue;

		trap_Key_SetCatcher( KEYCATCH_UI );
		uis.menusp = 0;
		UI_PushMenu ( &s_errorMessage.menu );

		return;
	}

	s_main.menu.draw = Main_MenuDraw;
	s_main.menu.fullscreen = qtrue;
	s_main.menu.wrapAround = qtrue;
	s_main.menu.showlogo = qtrue;

	MainMenu_Button( &s_main.multiplayer, ID_MULTIPLAYER, 0 );
	MainMenu_Button( &s_main.fighter, ID_FIGHTER, 1 );
	MainMenu_Button( &s_main.setup, ID_SETUP, 2 );
	MainMenu_Button( &s_main.exit, ID_EXIT, 3 );

	// the name plate is the edit field of the player name
	s_main.name.generic.type				= MTYPE_FIELD;
	s_main.name.generic.flags				= QMF_NODEFAULTINIT;
	s_main.name.generic.ownerdraw			= MainMenu_DrawName;
	s_main.name.field.widthInChars			= 14;
	s_main.name.field.maxchars				= 14;
	vx = 934 + UI_DExtra(); vy = 640; vw = 304; vh = 44;
	UI_DToV( &vx, &vy, &vw, &vh );
	s_main.name.generic.x					= vx;
	s_main.name.generic.y					= vy;
	s_main.name.generic.left				= vx;
	s_main.name.generic.top					= vy;
	s_main.name.generic.right				= vx + vw;
	s_main.name.generic.bottom				= vy + vh;

	s_main.player.generic.type				= MTYPE_BITMAP;
	s_main.player.generic.flags				= QMF_SILENT;
	s_main.player.generic.ownerdraw			= MainMenu_DrawPlayer;
	s_main.player.generic.id				= ID_MODEL;
	s_main.player.generic.callback			= Main_MenuEvent;
	// the fighter stands over its name plate on every aspect ratio
	vx = 890 + UI_DExtra(); vy = -255; vw = 480; vh = 1215;
	UI_DToV( &vx, &vy, &vw, &vh );
	s_main.player.generic.x					= vx;
	s_main.player.generic.y					= vy;
	s_main.player.width						= vw;
	s_main.player.height					= vh;

	Menu_AddItem( &s_main.menu,	&s_main.multiplayer );
	Menu_AddItem( &s_main.menu,	&s_main.fighter );
	Menu_AddItem( &s_main.menu,	&s_main.setup );
	Menu_AddItem( &s_main.menu,	&s_main.exit );
	Menu_AddItem( &s_main.menu, &s_main.name );
	Q_strncpyz(s_main.name.field.buffer,UI_Cvar_VariableString("name"),sizeof(s_main.name.field.buffer) );
	Menu_AddItem( &s_main.menu, &s_main.player );
	trap_Key_SetCatcher( KEYCATCH_UI );
	uis.menusp = 0;
	UI_PushMenu ( &s_main.menu );
	uis.menuamount = -1;
	uis.hideEarth = qfalse;
}
