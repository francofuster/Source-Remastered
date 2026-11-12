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

// PC on the 1280x720 canvas, phone on the 844x390 one (dp)
typedef struct {
	int		buttonX, buttonY, buttonW, buttonH, buttonStep;
	int		logoX, logoY, logoW;
	int		shadowX, shadowY, shadowW, shadowH;
	int		playerX, playerY, playerW, playerH;
	int		plateX, plateY, plateW, plateH;
} mainLayout_t;

static const mainLayout_t mainLayouts[2] = {
	{ 60, 214, 330, 66, 80,		40, 30, 270,	930, 584, 280, 90,	825, -240, 480, 1215,	934, 640, 304, 44 },
	{ 48, 92, 262, 58, 66,		26, 20, 128,	598, 262, 200, 60,	565, -150, 260, 658,	560, 312, 258, 52 }
};

#define ML	( mainLayouts[s_main.menu.mobile ? 1 : 0] )

typedef struct {
	menuframework_s	menu;

	menubitmap_s	multiplayer;
	menubitmap_s	fighter;
	menubitmap_s	setup;
	menubitmap_s	exit;
	menufield_s		name;
	char			nameSeen[MAX_EDIT_LINE];	// the name cvar when the field last took it
	menubitmap_s	nameButton;		// phone: the plate opens FIGHTER
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
static const char *mainCaptionsTouch[4] = {
	"Join a server or host a match",
	"Character, skin and name",
	"Touch controls, video and audio",
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
		UI_PlayerInfo_SetInfo( &s_main.playerinfo, ANIM_FLY_IDLE, ANIM_FLY_IDLE, viewangles, vec3_origin, WP_NONE, qfalse );
	}

	// floating over its shadow
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
	index = ( b->generic.y - ML.buttonY ) / ML.buttonStep;
	// x already carries the edge offset of wide screens
	enter = UI_AnimFrac( uis.transitionTime, 80 + index * 60, 320 );
	focus = UI_DFocus( &s_main.menu, self );
	shift = -50.0f * ( 1.0f - enter ) + ( s_main.menu.mobile ? 0.0f : 10.0f * focus );
	UI_DKiButton( b->generic.x + shift, b->generic.y + ( UI_TouchPressed( self ) ? 2.0f : 0.0f ), b->width, b->height, mainLabels[index],
		s_main.menu.mobile ? mainCaptionsTouch[index] : mainCaptions[index], focus, qfalse, !s_main.menu.mobile );
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
MainMenu_DrawNameButton

phone: the plate is a button to FIGHTER (a text field could not open the
keyboard) with the name on the right. The plate keeps the proportions of
its picture (212 x 34); the button around it is taller, for the finger
=================
*/
static void MainMenu_DrawNameButton( void *self ) {
	menubitmap_s	*b;
	float			focus;
	float			w, h, y;
	vec4_t			edge;

	b = (menubitmap_s *)self;
	focus = UI_DFocus( &s_main.menu, self );
	w = b->width + 14;
	h = w * 34.0f / 212.0f;
	y = b->generic.y + ( b->height - h ) * 0.5f;
	UI_DNamePlate( b->generic.x - 10, y, w, h );
	if ( focus > 0.0f ) {
		edge[0] = 0.47f; edge[1] = 0.63f; edge[2] = 1.0f; edge[3] = 0.5f + 0.4f * focus;
		UI_DFrame( b->generic.x, y + 1, b->width, h - 2, edge );
	}
	UI_DText( b->generic.x + b->width - 14, b->generic.y + b->height * 0.5f - 11, s_main.name.field.buffer, 21, UI_RIGHT|UI_DROPSHADOW, zeq_white );
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

/*
===============
MainMenu_SyncName

The main menu stays under FIGHTER, which can change the name. The field
took the name once, when the menu was made: a newer name has to replace
it, or the plate kept the old one and MainMenu_Save wrote it back. An edit
made here (PC) is kept, since the cvar did not change meanwhile.
===============
*/
static void MainMenu_SyncName( void ) {
	char	name[MAX_EDIT_LINE];

	trap_Cvar_VariableStringBuffer( "name", name, sizeof( name ) );
	if ( !strcmp( name, s_main.nameSeen ) ) {
		return;
	}
	Q_strncpyz( s_main.name.field.buffer, name, sizeof( s_main.name.field.buffer ) );
	Q_strncpyz( s_main.nameSeen, name, sizeof( s_main.nameSeen ) );
	s_main.name.field.cursor = strlen( s_main.name.field.buffer );
	s_main.name.field.scroll = 0;
}

void MainMenu_Save ( void ) {
	MainMenu_SyncName();
	trap_Cvar_Set("name", s_main.name.field.buffer );
	Q_strncpyz( s_main.nameSeen, s_main.name.field.buffer, sizeof( s_main.nameSeen ) );
}



/*
===============
MainMenu_Key

ESC (and Back on Android) asks to quit: popping the last menu only made
the engine open the main menu again
===============
*/
static sfxHandle_t MainMenu_Key( int key ) {
	if ( key == K_ESCAPE || key == K_MOUSE2 ) {
		MainMenu_Save();
		UI_ConfirmMenu( "Quit?", 0, MainMenu_ExitAction );
		return menu_out_sound;
	}
	return Menu_DefaultKey( &s_main.menu, key );
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
	static const char *hints[] = { "UD", "Navigate", "ENTER", "Select" };

	uis.menuamount = -1;
	uis.hideEarth = qfalse;
	uis.showFrame = qfalse;

	UI_DLogo( ML.logoX - UI_DExtra(), ML.logoY, ML.logoW );
	trap_R_SetColor( NULL );
	UI_DPic( ML.shadowX + UI_DExtra(), ML.shadowY, ML.shadowW, ML.shadowH, uis.blobShadow );

	if ( uis.activemenu == &s_errorMessage.menu ) {
		UI_DGlass( 400, 250, 480, 200 );
		UI_DProp( 640, 274, "ERROR", 20, UI_CENTER, zeq_gold );
		UI_DText( 640, 320, s_errorMessage.errorMessage, 18, UI_CENTER, zeq_white );
		UI_DText( 640, 410, "Press any key", 16, UI_CENTER, zeq_text3 );
		return;
	}

	MainMenu_SyncName();
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
	UI_DItem( b, ML.buttonX - UI_DExtra(), ML.buttonY + index * ML.buttonStep, ML.buttonW, ML.buttonH );
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
	s_main.menu.key = MainMenu_Key;
	s_main.menu.fullscreen = qtrue;
	s_main.menu.wrapAround = qtrue;
	s_main.menu.showlogo = qtrue;
	s_main.menu.mobile = UI_MobileLayout();
	UI_DCanvas( s_main.menu.mobile );

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
	vx = ML.plateX + UI_DExtra(); vy = ML.plateY; vw = ML.plateW; vh = ML.plateH;
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
	vx = ML.playerX + UI_DExtra(); vy = ML.playerY; vw = ML.playerW; vh = ML.playerH;	// floating above the blob shadow
	UI_DToV( &vx, &vy, &vw, &vh );
	s_main.player.generic.x					= vx;
	s_main.player.generic.y					= vy;
	s_main.player.width						= vw;
	s_main.player.height					= vh;

	Menu_AddItem( &s_main.menu,	&s_main.multiplayer );
	Menu_AddItem( &s_main.menu,	&s_main.fighter );
	Menu_AddItem( &s_main.menu,	&s_main.setup );
	Menu_AddItem( &s_main.menu,	&s_main.exit );
	if ( s_main.menu.mobile ) {
		s_main.nameButton.generic.type		= MTYPE_BITMAP;
		s_main.nameButton.generic.flags		= QMF_NODEFAULTINIT;
		s_main.nameButton.generic.id		= ID_MODEL;
		s_main.nameButton.generic.callback	= Main_MenuEvent;
		s_main.nameButton.generic.ownerdraw	= MainMenu_DrawNameButton;
		UI_DItem( &s_main.nameButton, ML.plateX + UI_DExtra(), ML.plateY, ML.plateW, ML.plateH );
		Menu_AddItem( &s_main.menu, &s_main.nameButton );
	}
	else {
		Menu_AddItem( &s_main.menu, &s_main.name );
	}
	Q_strncpyz(s_main.name.field.buffer,UI_Cvar_VariableString("name"),sizeof(s_main.name.field.buffer) );
	Q_strncpyz( s_main.nameSeen, s_main.name.field.buffer, sizeof( s_main.nameSeen ) );
	Menu_AddItem( &s_main.menu, &s_main.player );
	trap_Key_SetCatcher( KEYCATCH_UI );
	uis.menusp = 0;
	UI_PushMenu ( &s_main.menu );
	uis.menuamount = -1;
	uis.hideEarth = qfalse;
}
