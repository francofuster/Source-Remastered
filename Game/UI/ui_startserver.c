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
=============================================================================

START SERVER MENU *****

=============================================================================
*/


#include "ui_local.h"


#define GAMESERVER_BACK0		"interface/art/back_0"
#define GAMESERVER_BACK1		"interface/art/back_1"
#define GAMESERVER_NEXT0		"interface/art/next_0"
#define GAMESERVER_NEXT1		"interface/art/next_1"
#define GAMESERVER_FRAMEL		"interface/art/frame2_l"
#define GAMESERVER_FRAMER		"interface/art/frame1_r"
#define GAMESERVER_SELECT		"interface/art/maps_select"
#define GAMESERVER_SELECTED		"interface/art/maps_selected"
#define GAMESERVER_FIGHT0		"interface/art/fight_0"
#define GAMESERVER_FIGHT1		"interface/art/fight_1"
#define GAMESERVER_UNKNOWNMAP	"interface/art/unknownmap"
#define GAMESERVER_ARROWS		"interface/art/gs_arrows_0"
#define GAMESERVER_ARROWSL		"interface/art/gs_arrows_l"
#define GAMESERVER_ARROWSR		"interface/art/gs_arrows_r"

#define MAX_MAPROWS		2
#define MAX_MAPCOLS		2
#define MAX_MAPSPERPAGE	4

#define MAX_NAMELENGTH	16
#define ID_GAMETYPE				10
#define ID_PICTURES				11	// 12, 13, 14
#define ID_PREVPAGE				15
#define ID_NEXTPAGE				16
#define ID_STARTSERVERBACK		17
#define ID_STARTSERVERNEXT		18

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	framel;
	menubitmap_s	framer;

	menulist_s		gametype;
	menubitmap_s	mappics[MAX_MAPSPERPAGE];
	menubitmap_s	mapbuttons[MAX_MAPSPERPAGE];
	menubitmap_s	arrows;
	menubitmap_s	prevpage;
	menubitmap_s	nextpage;
	menutext_s		back;
	menutext_s		next;

	menutext_s		mapname;
	menubitmap_s	item_null;

	qboolean		multiplayer;
	int				currentmap;
	int				nummaps;
	int				page;
	int				maxpages;
	int 			maplist[MAX_ARENAS];
} startserver_t;

static startserver_t s_startserver;

static const char *gametype_items[] = {
	"Struggle",
	NULL
};

static int gametype_remap[] = {GT_FFA, GT_TEAM, GT_STRUGGLE,GT_TOURNAMENT, GT_CTF};
static int gametype_remap2[] = {0, 2, 0, 1, 3};


static void UI_ServerOptionsMenu( qboolean multiplayer );


/*
=================
GametypeBits
=================
*/
static int GametypeBits( char *string ) {
	int		bits;
	char	*p;
	char	*token;

	bits = 0;
	p = string;
	while( 1 ) {
		token = COM_ParseExt( &p, qfalse );
		if( token[0] == 0 ) {
			break;
		}

		if( Q_stricmp( token, "ffa" ) == 0 ) {
			bits |= 1 << GT_FFA;
			continue;
		}

		if( Q_stricmp( token, "tourney" ) == 0 ) {
			bits |= 1 << GT_TOURNAMENT;
			continue;
		}
		if( Q_stricmp( token, "struggle" ) == 0 ) {
			bits |= 1 << GT_STRUGGLE;
			continue;
		}
		if( Q_stricmp( token, "single" ) == 0 ) {
			bits |= 1 << GT_SINGLE_PLAYER;
			continue;
		}

		if( Q_stricmp( token, "team" ) == 0 ) {
			bits |= 1 << GT_TEAM;
			continue;
		}

		if( Q_stricmp( token, "ctf" ) == 0 ) {
			bits |= 1 << GT_CTF;
			continue;
		}
	}

	return bits;
}


/*
=================
StartServer_Update
=================
*/
static void StartServer_Update( void ) {
	int				i;
	int				top;
	static	char	picname[MAX_MAPSPERPAGE][64];
	const char		*info;
	char			mapname[MAX_NAMELENGTH];

	top = s_startserver.page*MAX_MAPSPERPAGE;

	for (i=0; i<MAX_MAPSPERPAGE; i++)
	{
		if (top+i >= s_startserver.nummaps)
			break;

		info = UI_GetArenaInfoByNumber( s_startserver.maplist[ top + i ]);
		Q_strncpyz( mapname, Info_ValueForKey( info, "map"), MAX_NAMELENGTH );
		Q_strupr( mapname );

		Com_sprintf( picname[i], sizeof(picname[i]), "maps/%s", mapname );

		s_startserver.mappics[i].generic.flags &= ~QMF_HIGHLIGHT;
		s_startserver.mappics[i].generic.name   = picname[i];
		s_startserver.mappics[i].shader         = 0;

		// reset
		s_startserver.mapbuttons[i].generic.flags |= QMF_PULSEIFFOCUS;
		s_startserver.mapbuttons[i].generic.flags &= ~QMF_INACTIVE;
	}

	for (; i<MAX_MAPSPERPAGE; i++)
	{
		s_startserver.mappics[i].generic.flags &= ~QMF_HIGHLIGHT;
		s_startserver.mappics[i].generic.name   = NULL;
		s_startserver.mappics[i].shader         = 0;

		// disable
		s_startserver.mapbuttons[i].generic.flags &= ~QMF_PULSEIFFOCUS;
		s_startserver.mapbuttons[i].generic.flags |= QMF_INACTIVE;
	}


	// no servers to start
	if( !s_startserver.nummaps ) {
		s_startserver.next.generic.flags |= QMF_INACTIVE;

		// set the map name
		strcpy( s_startserver.mapname.string, "NO MAPS FOUND" );
	}
	else {
		// set the highlight
		s_startserver.next.generic.flags &= ~QMF_INACTIVE;
		i = s_startserver.currentmap - top;
		if ( i >=0 && i < MAX_MAPSPERPAGE )
		{
			s_startserver.mappics[i].generic.flags    |= QMF_HIGHLIGHT;
			s_startserver.mapbuttons[i].generic.flags &= ~QMF_PULSEIFFOCUS;
		}

		// set the map name
		info = UI_GetArenaInfoByNumber( s_startserver.maplist[ s_startserver.currentmap ]);
		Q_strncpyz( s_startserver.mapname.string, Info_ValueForKey( info, "map" ), MAX_NAMELENGTH);
	}

	Q_strupr( s_startserver.mapname.string );
}


/*
=================
StartServer_MapEvent
=================
*/
static void StartServer_MapEvent( void* ptr, int event ) {
	if( event != QM_ACTIVATED) {
		return;
	}

	s_startserver.currentmap = (s_startserver.page*MAX_MAPSPERPAGE) + (((menucommon_s*)ptr)->id - ID_PICTURES);
	StartServer_Update();
}


/*
=================
StartServer_GametypeEvent
=================
*/
static void StartServer_GametypeEvent( void* ptr, int event ) {
	int			i;
	int			count;
	int			gamebits;
	int			matchbits;
	const char	*info;

	if( event != QM_ACTIVATED) {
		return;
	}

	count = UI_GetNumArenas();
	s_startserver.nummaps = 0;
	matchbits = 1 << gametype_remap[s_startserver.gametype.curvalue];
	if( gametype_remap[s_startserver.gametype.curvalue] == GT_FFA ) {
		matchbits |= ( 1 << GT_SINGLE_PLAYER );
	}
	for( i = 0; i < count; i++ ) {
		info = UI_GetArenaInfoByNumber( i );

		gamebits = GametypeBits( Info_ValueForKey( info, "type") );
		if( !( gamebits & matchbits ) ) {
			continue;
		}

		s_startserver.maplist[ s_startserver.nummaps ] = i;
		s_startserver.nummaps++;
	}
	s_startserver.maxpages = (s_startserver.nummaps + MAX_MAPSPERPAGE-1)/MAX_MAPSPERPAGE;
	s_startserver.page = 0;
	s_startserver.currentmap = 0;

	StartServer_Update();
}


/*
=================
StartServer_MenuEvent
=================
*/
static void StartServer_MenuEvent( void* ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_PREVPAGE:
		if( s_startserver.page > 0 ) {
			s_startserver.page--;
			StartServer_Update();
		}
		break;

	case ID_NEXTPAGE:
		if( s_startserver.page < s_startserver.maxpages - 1 ) {
			s_startserver.page++;
			StartServer_Update();
		}
		break;

	case ID_STARTSERVERNEXT:
		trap_Cvar_SetValue( "g_gameType", gametype_remap[s_startserver.gametype.curvalue] );
		UI_ServerOptionsMenu( s_startserver.multiplayer );
		break;

	case ID_STARTSERVERBACK:
		UI_PopMenu();
		break;
	}
}


/*
===============
StartServer_LevelshotDraw
===============
*/
static void StartServer_LevelshotDraw( void *self ) {
	menubitmap_s	*b;
	int				x;
	int				y;
	int				w;
	int				h;
	int				n;
	const char		*info;

	b = (menubitmap_s *)self;

	if( !b->generic.name ) {
		return;
	}

	if( b->generic.name && !b->shader ) {
		b->shader = trap_R_RegisterShaderNoMip( b->generic.name );
		if( !b->shader && b->errorpic ) {
			b->shader = trap_R_RegisterShaderNoMip( b->errorpic );
		}
	}
	x = b->generic.x;
	y = b->generic.y;
	w = b->width;
	h =	b->height;
	if( b->shader ) {
		UI_DrawHandlePic( x, y, w, h, b->shader );
	}

	x = b->generic.x;
	y = b->generic.y + b->height;
	UI_FillRect( x, y, b->width, 28, colorBlack );

	x += b->width / 2;
	y += 4;
	n = s_startserver.page * MAX_MAPSPERPAGE + b->generic.id - ID_PICTURES;

	info = UI_GetArenaInfoByNumber( s_startserver.maplist[ n ]);
	UI_DrawString( x, y, Info_ValueForKey( info, "map" ), UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, color_orange );

	x = b->generic.x;
	y = b->generic.y;
	w = b->width;
	h =	b->height + 28;
}

/*
=================
StartServer_MenuInit
=================
*/
static void StartServer_MenuInit( void ) {
	int	i;
	int	x;
	int	y;
	static char mapnamebuffer[64];

	// zero set all our globals
	memset( &s_startserver, 0 ,sizeof(startserver_t) );

	StartServer_Cache();

	s_startserver.menu.wrapAround = qtrue;
	s_startserver.menu.fullscreen = qtrue;

	s_startserver.banner.generic.type  = MTYPE_BTEXT;
	s_startserver.banner.generic.x	   = 320;
	s_startserver.banner.generic.y	   = 16;
	s_startserver.banner.string        = "GAME SERVER";
	s_startserver.banner.color         = color_white;
	s_startserver.banner.style         = UI_CENTER|UI_DROPSHADOW;

	s_startserver.framel.generic.type  = MTYPE_BITMAP;
	s_startserver.framel.generic.name  = GAMESERVER_FRAMEL;
	s_startserver.framel.generic.flags = QMF_INACTIVE;
	s_startserver.framel.generic.x	   = 0;
	s_startserver.framel.generic.y	   = 78;
	s_startserver.framel.width  	   = 256;
	s_startserver.framel.height  	   = 329;

	s_startserver.framer.generic.type  = MTYPE_BITMAP;
	s_startserver.framer.generic.name  = GAMESERVER_FRAMER;
	s_startserver.framer.generic.flags = QMF_INACTIVE;
	s_startserver.framer.generic.x	   = 376;
	s_startserver.framer.generic.y	   = 76;
	s_startserver.framer.width  	   = 256;
	s_startserver.framer.height  	   = 334;

	s_startserver.gametype.generic.type		= MTYPE_SPINCONTROL;
	s_startserver.gametype.generic.name		= "Game Type:";
	s_startserver.gametype.generic.flags	= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_startserver.gametype.generic.callback	= StartServer_GametypeEvent;
	s_startserver.gametype.generic.id		= ID_GAMETYPE;
	s_startserver.gametype.generic.x		= 320 - 24;
	s_startserver.gametype.generic.y		= 368;
	s_startserver.gametype.itemnames		= gametype_items;

	for (i=0; i<MAX_MAPSPERPAGE; i++)
	{
		x =	(i % MAX_MAPCOLS) * (128+8) + 188;
		y = (i / MAX_MAPROWS) * (128+8) + 96;

		s_startserver.mappics[i].generic.type   = MTYPE_BITMAP;
		s_startserver.mappics[i].generic.flags  = QMF_LEFT_JUSTIFY|QMF_INACTIVE;
		s_startserver.mappics[i].generic.x	    = x;
		s_startserver.mappics[i].generic.y	    = y;
		s_startserver.mappics[i].generic.id		= ID_PICTURES+i;
		s_startserver.mappics[i].width  		= 128;
		s_startserver.mappics[i].height  	    = 96;
		s_startserver.mappics[i].focuspic       = GAMESERVER_SELECTED;
		s_startserver.mappics[i].errorpic       = GAMESERVER_UNKNOWNMAP;
		s_startserver.mappics[i].generic.ownerdraw = StartServer_LevelshotDraw;

		s_startserver.mapbuttons[i].generic.type     = MTYPE_BITMAP;
		s_startserver.mapbuttons[i].generic.flags    = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_NODEFAULTINIT;
		s_startserver.mapbuttons[i].generic.id       = ID_PICTURES+i;
		s_startserver.mapbuttons[i].generic.callback = StartServer_MapEvent;
		s_startserver.mapbuttons[i].generic.x	     = x - 30;
		s_startserver.mapbuttons[i].generic.y	     = y - 32;
		s_startserver.mapbuttons[i].width  		     = 256;
		s_startserver.mapbuttons[i].height  	     = 248;
		s_startserver.mapbuttons[i].generic.left     = x;
		s_startserver.mapbuttons[i].generic.top  	 = y;
		s_startserver.mapbuttons[i].generic.right    = x + 128;
		s_startserver.mapbuttons[i].generic.bottom   = y + 128;
		s_startserver.mapbuttons[i].focuspic         = GAMESERVER_SELECT;
	}

	s_startserver.arrows.generic.type  = MTYPE_BITMAP;
	s_startserver.arrows.generic.name  = GAMESERVER_ARROWS;
	s_startserver.arrows.generic.flags = QMF_INACTIVE;
	s_startserver.arrows.generic.x	   = 260;
	s_startserver.arrows.generic.y	   = 400;
	s_startserver.arrows.width  	   = 128;
	s_startserver.arrows.height  	   = 32;

	s_startserver.prevpage.generic.type	    = MTYPE_BITMAP;
	s_startserver.prevpage.generic.flags    = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_startserver.prevpage.generic.callback = StartServer_MenuEvent;
	s_startserver.prevpage.generic.id	    = ID_PREVPAGE;
	s_startserver.prevpage.generic.x		= 260;
	s_startserver.prevpage.generic.y		= 400;
	s_startserver.prevpage.width  		    = 64;
	s_startserver.prevpage.height  		    = 32;
	s_startserver.prevpage.focuspic         = GAMESERVER_ARROWSL;

	s_startserver.nextpage.generic.type	    = MTYPE_BITMAP;
	s_startserver.nextpage.generic.flags    = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_startserver.nextpage.generic.callback = StartServer_MenuEvent;
	s_startserver.nextpage.generic.id	    = ID_NEXTPAGE;
	s_startserver.nextpage.generic.x		= 321;
	s_startserver.nextpage.generic.y		= 400;
	s_startserver.nextpage.width  		    = 64;
	s_startserver.nextpage.height  		    = 32;
	s_startserver.nextpage.focuspic         = GAMESERVER_ARROWSR;

	s_startserver.mapname.generic.type  = MTYPE_PTEXT;
	s_startserver.mapname.generic.flags = QMF_CENTER_JUSTIFY|QMF_INACTIVE;
	s_startserver.mapname.generic.x	    = 320;
	s_startserver.mapname.generic.y	    = 440;
	s_startserver.mapname.string        = mapnamebuffer;
	s_startserver.mapname.style         = UI_CENTER|UI_BIGFONT|UI_DROPSHADOW;
	s_startserver.mapname.color         = text_color_normal;

	s_startserver.next.generic.type			= MTYPE_PTEXT;
	s_startserver.next.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_startserver.next.generic.id			= ID_STARTSERVERNEXT;
	s_startserver.next.generic.callback		= StartServer_MenuEvent;
	s_startserver.next.generic.x			= 28;
	s_startserver.next.generic.y			= 119;
	s_startserver.next.string				= "NEXT";
	s_startserver.next.style				= UI_LEFT | UI_DROPSHADOW | UI_TINYFONT;
	s_startserver.next.color				= color_white;
	y+=38;
	s_startserver.back.generic.type			= MTYPE_PTEXT;
	s_startserver.back.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_startserver.back.generic.id			= ID_STARTSERVERBACK;
	s_startserver.back.generic.callback		= StartServer_MenuEvent;
	s_startserver.back.generic.x			= 28;
	s_startserver.back.generic.y			= 157;
	s_startserver.back.string				= "BACK";
	s_startserver.back.style				= UI_LEFT | UI_DROPSHADOW | UI_TINYFONT;
	s_startserver.back.color				= color_white;
	y+=38;


	s_startserver.item_null.generic.type	= MTYPE_BITMAP;
	s_startserver.item_null.generic.flags	= QMF_LEFT_JUSTIFY|QMF_MOUSEONLY|QMF_SILENT;
	s_startserver.item_null.generic.x		= 0;
	s_startserver.item_null.generic.y		= 0;
	s_startserver.item_null.width			= 640;
	s_startserver.item_null.height			= 480;

	Menu_AddItem( &s_startserver.menu, &s_startserver.gametype );
	for (i=0; i<MAX_MAPSPERPAGE; i++)
	{
		Menu_AddItem( &s_startserver.menu, &s_startserver.mappics[i] );
		Menu_AddItem( &s_startserver.menu, &s_startserver.mapbuttons[i] );
	}

	Menu_AddItem( &s_startserver.menu, &s_startserver.arrows );
	Menu_AddItem( &s_startserver.menu, &s_startserver.prevpage );
	Menu_AddItem( &s_startserver.menu, &s_startserver.nextpage );
	Menu_AddItem( &s_startserver.menu, &s_startserver.next );
	Menu_AddItem( &s_startserver.menu, &s_startserver.back );
	Menu_AddItem( &s_startserver.menu, &s_startserver.mapname );
	Menu_AddItem( &s_startserver.menu, &s_startserver.item_null );

	StartServer_GametypeEvent( NULL, QM_ACTIVATED );
}


/*
=================
StartServer_Cache
=================
*/
void StartServer_Cache( void )
{
	int				i;
	const char		*info;
	qboolean		precache;
	char			picname[64];
	char			mapname[ MAX_NAMELENGTH ];

	trap_R_RegisterShaderNoMip( GAMESERVER_BACK0 );
	trap_R_RegisterShaderNoMip( GAMESERVER_BACK1 );
	trap_R_RegisterShaderNoMip( GAMESERVER_NEXT0 );
	trap_R_RegisterShaderNoMip( GAMESERVER_NEXT1 );
	trap_R_RegisterShaderNoMip( GAMESERVER_FRAMEL );
	trap_R_RegisterShaderNoMip( GAMESERVER_FRAMER );
	trap_R_RegisterShaderNoMip( GAMESERVER_SELECT );
	trap_R_RegisterShaderNoMip( GAMESERVER_SELECTED );
	trap_R_RegisterShaderNoMip( GAMESERVER_UNKNOWNMAP );
	trap_R_RegisterShaderNoMip( GAMESERVER_ARROWS );
	trap_R_RegisterShaderNoMip( GAMESERVER_ARROWSL );
	trap_R_RegisterShaderNoMip( GAMESERVER_ARROWSR );

	precache = trap_Cvar_VariableValue("com_buildscript");

	if( precache ) {
		for( i = 0; i < UI_GetNumArenas(); i++ ) {
			info = UI_GetArenaInfoByNumber( i );
			Q_strncpyz( mapname, Info_ValueForKey( info, "map"), MAX_NAMELENGTH );
			Q_strupr( mapname );

			Com_sprintf( picname, sizeof(picname), "maps/%s", mapname );
			trap_R_RegisterShaderNoMip(picname);
		}
	}
}


/*
=================
UI_StartServerMenu
=================
*/
void UI_StartServerMenu( qboolean multiplayer ) {
	// the map list lives in StartServer, the screen is ServerOptions
	StartServer_MenuInit();
	s_startserver.multiplayer = multiplayer;
	trap_Cvar_SetValue( "g_gameType", gametype_remap[s_startserver.gametype.curvalue] );
	UI_ServerOptionsMenu( multiplayer );
}

static void UI_ServerOptionsMenu( qboolean multiplayer );

// TEMP testing aid for the menu redesign, remove before release
void UI_TestMenu( const char *which ) {
	if ( !Q_stricmp( which, "play" ) ) {
		UI_ArenaServersMenu();
		return;
	}
	if ( !Q_stricmp( which, "fighter" ) ) {
		UI_PlayerModelMenu();
		return;
	}
	if ( !Q_stricmp( which, "options" ) ) {
		UI_SystemSettingsMenu();
		return;
	}
	if ( !Q_stricmp( which, "controls" ) ) {
		UI_ControlsMenu();
		return;
	}
	if ( !Q_stricmp( which, "quit" ) ) {
		UI_ConfirmMenu( "Quit?", 0, 0 );
		return;
	}
	if ( !Q_stricmp( which, "camera" ) ) {
		UI_CameraMenu();
		return;
	}
	if ( !Q_stricmp( which, "game" ) ) {
		UI_PreferencesMenu();
		return;
	}
	if ( !Q_stricmp( which, "maps" ) ) {
		UI_StartServerMenu( qtrue );
		return;
	}
	UI_StartServerMenu( qtrue );
	if ( !Q_stricmp( which, "roster" ) || !Q_stricmp( which, "edit" ) ) {
		UI_BotRosterMenu();
	}
	if ( !Q_stricmp( which, "edit" ) ) {
		UI_BotEditMenu( -1 );
	}
}



/*
=============================================================================

SERVER OPTIONS MENU *****

=============================================================================
*/

#define ID_PLAYER_TYPE			20
#define ID_MAXCLIENTS			21
#define ID_DEDICATED			22
#define ID_GO					23
#define ID_BACK					24
#define ID_FIGHTERS				25
#define ID_FIGHTERPANEL			26
#define ID_PREVIEW				27
#define ID_THUMB0				28		// + thumbnail

#define SO_MAP_X				160
#define SO_RULES_X				400
#define SO_PANEL_Y				36
#define SO_FIGHTERS_Y			262

#define PLAYER_SLOTS			12


typedef struct {
	menuframework_s		menu;

	menutext_s			banner;

	menubitmap_s		mappic;
	menubitmap_s		picframe;

	menulist_s			dedicated;
	menufield_s			powerlevel;
	menufield_s			powerlevelMaximum;
	menufield_s			breakLimitRate;
	menufield_s			timelimit;
	menufield_s			fraglimit;
	menufield_s			flaglimit;
	menuradiobutton_s	friendlyfire;
	menufield_s			hostname;
	menuradiobutton_s	pure;
	menulist_s			botSkill;

	menutext_s			player0;
	menulist_s			playerType[PLAYER_SLOTS];
	menutext_s			playerName[PLAYER_SLOTS];
	menulist_s			playerTeam[PLAYER_SLOTS];

	menutext_s			go;
	menutext_s			next;
	menutext_s			back;
	menutext_s			fighters;
	menubitmap_s		fighterPanel;
	menubitmap_s		preview;
	menubitmap_s		thumbs[4];
	menubitmap_s		fightBtn;
	int					rulesBottom;

	qboolean			multiplayer;
	int					gametype;
	char				mapnamebuffer[32];
	char				playerNameBuffers[PLAYER_SLOTS][16];

	qboolean			newBot;
	int					newBotIndex;
	char				newBotName[16];
} serveroptions_t;

static serveroptions_t s_serveroptions;

static const char *dedicated_list[] = {
	"No",
	"LAN",
	"Internet",
	NULL
};

static const char *playerType_list[] = {
	"Open",
	"Bot",
	"----",
	NULL
};

static const char *playerTeam_list[] = {
	"Good",
	"Evil",
	NULL
};

static const char *botSkill_list[] = {
	"I Can Win",
	"Bring It On",
	"Hurt Me Plenty",
	"Hardcore",
	"Nightmare!",
	NULL
};


/*
=================
BotAlreadySelected
=================
*/
static qboolean BotAlreadySelected( const char *checkName ) {
	int		n;

	for( n = 1; n < PLAYER_SLOTS; n++ ) {
		if( s_serveroptions.playerType[n].curvalue != 1 ) {
			continue;
		}
		if( (s_serveroptions.gametype >= GT_TEAM) &&
			(s_serveroptions.playerTeam[n].curvalue != s_serveroptions.playerTeam[s_serveroptions.newBotIndex].curvalue ) ) {
			continue;
		}
		if( Q_stricmp( checkName, s_serveroptions.playerNameBuffers[n] ) == 0 ) {
			return qtrue;
		}
	}

	return qfalse;
}


/*
=================
ServerOptions_Start
=================
*/
static void ServerOptions_Start( void ) {
	int		timelimit;
	int		fraglimit;
	int		powerlevel;
	int 	powerlevelMaximum;
	int		breakLimitRate;
	int		maxclients;
	int		dedicated;
	int		friendlyfire;
	int		flaglimit;
	int		pure;
	int		skill;
	int		n;
	char	buf[64];
	const char *info;

	timelimit	 = atoi( s_serveroptions.timelimit.field.buffer );
	fraglimit	 = atoi( s_serveroptions.fraglimit.field.buffer );
	powerlevel	 = atoi( s_serveroptions.powerlevel.field.buffer );
	powerlevelMaximum = atoi( s_serveroptions.powerlevelMaximum.field.buffer );
	breakLimitRate	 = atoi( s_serveroptions.breakLimitRate.field.buffer );
	flaglimit	 = atoi( s_serveroptions.flaglimit.field.buffer );
	dedicated	 = s_serveroptions.dedicated.curvalue;
	friendlyfire = s_serveroptions.friendlyfire.curvalue;
	pure		 = s_serveroptions.pure.curvalue;
	skill		 = s_serveroptions.botSkill.curvalue + 1;

	//set maxclients
	for( n = 0, maxclients = 0; n < PLAYER_SLOTS; n++ ) {
		if( s_serveroptions.playerType[n].curvalue == 2 ) {
			continue;
		}
		if( (s_serveroptions.playerType[n].curvalue == 1) && (s_serveroptions.playerNameBuffers[n][0] == 0) ) {
			continue;
		}
		maxclients++;
	}

	switch( s_serveroptions.gametype ) {
	case GT_FFA:
	case GT_STRUGGLE:
	default:
		trap_Cvar_SetValue( "ui_ffa_fraglimit", fraglimit );
		trap_Cvar_SetValue( "ui_ffa_timelimit", timelimit );
		trap_Cvar_SetValue( "ui_ffa_powerlevel", powerlevel );
		trap_Cvar_SetValue( "ui_ffa_powerlevelMaximum", powerlevelMaximum );
		trap_Cvar_SetValue( "ui_ffa_breakLimitRate", breakLimitRate );
		break;
	}

#if MAPLENSFLARES	// JUHOX: reset edit mode
	trap_Cvar_SetValue("g_editmode", 0);
#endif
	// room for the roster and for bots added later from the pause menu
	maxclients = 12;
	trap_Cvar_SetValue( "sv_maxclients", Com_Clamp( 0, 12, maxclients ) );
	trap_Cvar_SetValue( "dedicated", Com_Clamp( 0, 2, dedicated ) );
	trap_Cvar_SetValue ("timelimit", Com_Clamp( 0, timelimit, timelimit ) );
	trap_Cvar_SetValue ("fraglimit", Com_Clamp( 0, fraglimit, fraglimit ) );
	trap_Cvar_SetValue ("g_powerlevel", Com_Clamp( 1, 32767, powerlevel ) );
	trap_Cvar_SetValue ("g_powerlevelMaximum", Com_Clamp( 1, 32767, powerlevelMaximum ) );
	trap_Cvar_SetValue ("g_breakLimitRate", Com_Clamp( 1, 100, breakLimitRate ) );
	trap_Cvar_SetValue ("capturelimit", Com_Clamp( 0, flaglimit, flaglimit ) );
	trap_Cvar_SetValue( "g_friendlyfire", friendlyfire );
	trap_Cvar_SetValue( "sv_pure", pure );
	trap_Cvar_Set("sv_hostname", s_serveroptions.hostname.field.buffer );

	// the roster travels in g_queuedBot cvars, the game adds the bots
	// itself once the level runs
	UI_Bots_ExecAddCommands();

	// the wait commands will allow the dedicated to take effect
	info = UI_GetArenaInfoByNumber( s_startserver.maplist[ s_startserver.currentmap ]);
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "wait ; wait ; map %s\n", Info_ValueForKey( info, "map" )));

	// set player's team
	if( dedicated == 0 && s_serveroptions.gametype >= GT_TEAM ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "wait 5; team %s\n", playerTeam_list[s_serveroptions.playerTeam[0].curvalue] ) );
	}
}


/*
=================
ServerOptions_InitPlayerItems
=================
*/
static void ServerOptions_InitPlayerItems( void ) {
	int		n;
	int		v;

	// init types
	if( s_serveroptions.multiplayer ) {
		v = 0;	// open
	}
	else {
		v = 1;	// bot
	}

	for( n = 0; n < PLAYER_SLOTS; n++ ) {
		s_serveroptions.playerType[n].curvalue = v;
	}

	if( s_serveroptions.multiplayer && (s_serveroptions.gametype < GT_TEAM) ) {
		for( n = 8; n < PLAYER_SLOTS; n++ ) {
			s_serveroptions.playerType[n].curvalue = 2;
		}
	}

	// if not a dedicated server, first slot is reserved for the human on the server
	if( s_serveroptions.dedicated.curvalue == 0 ) {
		// human
		s_serveroptions.playerType[0].generic.flags |= QMF_INACTIVE;
		s_serveroptions.playerType[0].curvalue = 0;
		trap_Cvar_VariableStringBuffer( "name", s_serveroptions.playerNameBuffers[0], sizeof(s_serveroptions.playerNameBuffers[0]) );
		Q_CleanStr( s_serveroptions.playerNameBuffers[0] );
	}

	// init teams
	if( s_serveroptions.gametype >= GT_TEAM ) {
		for( n = 0; n < (PLAYER_SLOTS / 2); n++ ) {
			s_serveroptions.playerTeam[n].curvalue = 0;
		}
		for( ; n < PLAYER_SLOTS; n++ ) {
			s_serveroptions.playerTeam[n].curvalue = 1;
		}
	}
	else {
		for( n = 0; n < PLAYER_SLOTS; n++ ) {
			s_serveroptions.playerTeam[n].generic.flags |= (QMF_INACTIVE|QMF_HIDDEN);
		}
	}
}


/*
=================
ServerOptions_SetPlayerItems
=================
*/
static void ServerOptions_SetPlayerItems( void ) {
	int		start;
	int		n;

	// types
//	for( n = 0; n < PLAYER_SLOTS; n++ ) {
//		if( (!s_serveroptions.multiplayer) && (n > 0) && (s_serveroptions.playerType[n].curvalue == 0) ) {
//			s_serveroptions.playerType[n].curvalue = 1;
//		}
//	}

	// names
	if( s_serveroptions.dedicated.curvalue == 0 ) {
		s_serveroptions.player0.string = "Human";
		s_serveroptions.playerName[0].generic.flags &= ~QMF_HIDDEN;

		start = 1;
	}
	else {
		s_serveroptions.player0.string = "Open";
		start = 0;
	}
	for( n = start; n < PLAYER_SLOTS; n++ ) {
		if( s_serveroptions.playerType[n].curvalue == 1 ) {
			s_serveroptions.playerName[n].generic.flags &= ~(QMF_INACTIVE|QMF_HIDDEN);
		}
		else {
			s_serveroptions.playerName[n].generic.flags |= (QMF_INACTIVE|QMF_HIDDEN);
		}
	}

	// teams
	if( s_serveroptions.gametype < GT_TEAM ) {
		return;
	}
	for( n = start; n < PLAYER_SLOTS; n++ ) {
		if( s_serveroptions.playerType[n].curvalue == 2 ) {
			s_serveroptions.playerTeam[n].generic.flags |= (QMF_INACTIVE|QMF_HIDDEN);
		}
		else {
			s_serveroptions.playerTeam[n].generic.flags &= ~(QMF_INACTIVE|QMF_HIDDEN);
		}
	}
}


/*
=================
ServerOptions_Event
=================
*/
static void ServerOptions_SetMap( int index );
static int ServerOptions_FirstThumb( void );

static void ServerOptions_Event( void* ptr, int event ) {
	int id;

	id = ((menucommon_s*)ptr)->id;
	if ( event == QM_ACTIVATED && id >= ID_THUMB0 && id < ID_THUMB0 + 4 ) {
		ServerOptions_SetMap( ServerOptions_FirstThumb() + id - ID_THUMB0 );
		return;
	}
	if ( event == QM_ACTIVATED && id == ID_PREVIEW ) {
		ServerOptions_SetMap( s_startserver.currentmap + 1 );
		return;
	}

	switch( id ) {

	//if( event != QM_ACTIVATED && event != QM_LOSTFOCUS) {
	//	return;
	//}
	case ID_PLAYER_TYPE:
		if( event != QM_ACTIVATED ) {
			break;
		}
		ServerOptions_SetPlayerItems();
		break;

	case ID_MAXCLIENTS:
	case ID_DEDICATED:
		ServerOptions_SetPlayerItems();
		break;
	case ID_GO:
		if( event != QM_ACTIVATED ) {
			break;
		}
		ServerOptions_Start();
		break;

	case ID_FIGHTERS:
	case ID_FIGHTERPANEL:
		if( event != QM_ACTIVATED ) {
			break;
		}
		UI_BotRosterMenu();
		break;

	case ID_STARTSERVERNEXT:
		if( event != QM_ACTIVATED ) {
			break;
		}
		break;
	case ID_BACK:
		if( event != QM_ACTIVATED ) {
			break;
		}
		UI_PopMenu();
		break;
	}
}


static void ServerOptions_PlayerNameEvent( void* ptr, int event ) {
	int		n;

	if( event != QM_ACTIVATED ) {
		return;
	}
	n = ((menutext_s*)ptr)->generic.id;
	s_serveroptions.newBotIndex = n;
	UI_BotSelectMenu( s_serveroptions.playerNameBuffers[n] );
}


/*
=================
ServerOptions_StatusBar
=================
*/
static void ServerOptions_StatusBar( void* ptr ) {
	// the map panel already says 0 = no limit

}


/*
===============
ServerOptions_LevelshotDraw
===============
*/
static void ServerOptions_LevelshotDraw( void *self ) {
	menubitmap_s	*b;
//	int				x;
//	int				y;

	// strange place for this, but it works
	if( s_serveroptions.newBot ) {
		Q_strncpyz( s_serveroptions.playerNameBuffers[s_serveroptions.newBotIndex], s_serveroptions.newBotName, 16 );
		s_serveroptions.newBot = qfalse;
	}

	b = (menubitmap_s *)self;

	trap_R_SetColor( NULL );
	Bitmap_Draw( b );
	UI_DrawRect( b->generic.x - 1, b->generic.y - 1, b->width + 2, b->height + 2, zeq_ki );
/*
	x = b->generic.x;
	y = b->generic.y + b->height;
	UI_FillRect( x, y, b->width, 40, colorBlack );

	x += b->width / 2;
	y += 4;
	UI_DrawString( x, y, s_serveroptions.mapnamebuffer, UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, color_white );

	y += SMALLCHAR_HEIGHT;
	UI_DrawString( x, y, gametype_items[gametype_remap2[s_serveroptions.gametype]], UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, color_white );
*/
}


static void ServerOptions_InitBotNames( void ) {
	int			count;
	int			n;
	const char	*arenaInfo;
	const char	*botInfo;
	char		*p;
	char		*bot;
	char		bots[MAX_INFO_STRING];

	if( s_serveroptions.gametype >= GT_TEAM ) {
		Q_strncpyz( s_serveroptions.playerNameBuffers[1], "goku", 16 );
		Q_strncpyz( s_serveroptions.playerNameBuffers[2], "vegeta", 16 );
		if( s_serveroptions.gametype == GT_TEAM ) {
			Q_strncpyz( s_serveroptions.playerNameBuffers[3], "goku", 16 );
		}
		else {
			s_serveroptions.playerType[3].curvalue = 2;
		}
		s_serveroptions.playerType[4].curvalue = 2;
		s_serveroptions.playerType[5].curvalue = 2;

		Q_strncpyz( s_serveroptions.playerNameBuffers[6], "goku", 16 );
		Q_strncpyz( s_serveroptions.playerNameBuffers[7], "vegeta", 16 );
		Q_strncpyz( s_serveroptions.playerNameBuffers[8], "goku", 16 );
		if( s_serveroptions.gametype == GT_TEAM ) {
			Q_strncpyz( s_serveroptions.playerNameBuffers[9], "vegeta", 16 );
		}
		else {
			s_serveroptions.playerType[9].curvalue = 2;
		}
		s_serveroptions.playerType[10].curvalue = 2;
		s_serveroptions.playerType[11].curvalue = 2;

		return;
	}

	count = 1;	// skip the first slot, reserved for a human

	// get info for this map
	arenaInfo = UI_GetArenaInfoByMap( s_serveroptions.mapnamebuffer );

	// get the bot info - we'll seed with them if any are listed
	Q_strncpyz( bots, Info_ValueForKey( arenaInfo, "bots" ), sizeof(bots) );
	p = &bots[0];
	while( *p && count < PLAYER_SLOTS ) {
		//skip spaces
		while( *p && *p == ' ' ) {
			p++;
		}
		if( !p ) {
			break;
		}

		// mark start of bot name
		bot = p;

		// skip until space of null
		while( *p && *p != ' ' ) {
			p++;
		}
		if( *p ) {
			*p++ = 0;
		}

		botInfo = UI_GetBotInfoByName( bot );
		if( !botInfo )
		{
			botInfo = UI_GetBotInfoByNumber( count );
		}
		bot = Info_ValueForKey( botInfo, "name" );

		Q_strncpyz( s_serveroptions.playerNameBuffers[count], bot, sizeof(s_serveroptions.playerNameBuffers[count]) );
		count++;
	}

	// set the rest of the bot slots to "---"
	for( n = count; n < PLAYER_SLOTS; n++ ) {
		strcpy( s_serveroptions.playerNameBuffers[n], "--------" );
	}

	// pad up to #8 as open slots
	for( ;count < 8; count++ ) {
		s_serveroptions.playerType[count].curvalue = 0;
	}

	// close off the rest by default
	for( ;count < PLAYER_SLOTS; count++ ) {
		if( s_serveroptions.playerType[count].curvalue == 1 ) {
			s_serveroptions.playerType[count].curvalue = 2;
		}
	}
}


/*
=================
ServerOptions_SetMenuItems
=================
*/
static void ServerOptions_SetMenuItems( void ) {
	static char picname[64];
	char		mapname[MAX_NAMELENGTH];
	const char	*info;

	switch( s_serveroptions.gametype ) {
	case GT_FFA:
	case GT_STRUGGLE:
	default:
		Com_sprintf( s_serveroptions.fraglimit.field.buffer, 4, "%i", (int)Com_Clamp( 0, 999, trap_Cvar_VariableValue( "ui_ffa_fraglimit" ) ) );
		Com_sprintf( s_serveroptions.timelimit.field.buffer, 4, "%i", (int)Com_Clamp( 0, 999, trap_Cvar_VariableValue( "ui_ffa_timelimit" ) ) );
		Com_sprintf( s_serveroptions.powerlevel.field.buffer, 6, "%i", (int)Com_Clamp( 1, 32767, trap_Cvar_VariableValue( "ui_ffa_powerlevel" ) ) );
		Com_sprintf( s_serveroptions.powerlevelMaximum.field.buffer, 6, "%i", (int)Com_Clamp( 1, 32767, trap_Cvar_VariableValue( "ui_ffa_powerlevelMaximum" ) ) );
		Com_sprintf( s_serveroptions.breakLimitRate.field.buffer, 4, "%i", (int)Com_Clamp( 1, 100, trap_Cvar_VariableValue( "ui_ffa_breakLimitRate" ) ) );
		break;
	}

	Q_strncpyz( s_serveroptions.hostname.field.buffer, UI_Cvar_VariableString( "sv_hostname" ), sizeof( s_serveroptions.hostname.field.buffer ) );
	s_serveroptions.pure.curvalue = Com_Clamp( 0, 1, trap_Cvar_VariableValue( "sv_pure" ) );

	// set the map pic
	info = UI_GetArenaInfoByNumber( s_startserver.maplist[ s_startserver.currentmap ]);
	Q_strncpyz( mapname, Info_ValueForKey( info, "map"), MAX_NAMELENGTH );
	Q_strupr( mapname );
	Com_sprintf( picname, 64, "maps/%s", mapname );
	s_serveroptions.mappic.generic.name = picname;

	// set the map name
	strcpy( s_serveroptions.mapnamebuffer, s_startserver.mapname.string );
	Q_strupr( s_serveroptions.mapnamebuffer );

	// get the player selections initialized
	ServerOptions_InitPlayerItems();
	ServerOptions_SetPlayerItems();

	// bots come from the botsys roster now (ui_bots.c)
	ServerOptions_SetPlayerItems();
}

/*
=================
PlayerName_Draw
=================
*/
static void PlayerName_Draw( void *item ) {
	menutext_s	*s;
	float		*color;
	int			x, y;
	int			style;
	qboolean	focus;

	s = (menutext_s *)item;

	x = s->generic.x;
	y =	s->generic.y;

	style = UI_SMALLFONT|UI_DROPSHADOW;
	focus = (s->generic.parent->cursor == s->generic.menuPosition);

	if ( s->generic.flags & QMF_GRAYED )
		color = text_color_disabled;
	else if ( focus )
	{
		color = text_color_highlight;
		style |= UI_PULSE;
	}
	else if ( s->generic.flags & QMF_BLINK )
	{
		color = text_color_highlight;
		style |= UI_BLINK;
	}
	else
		color = text_color_normal;

	if ( focus )
	{
		// draw cursor
		UI_FillRect( s->generic.left, s->generic.top, s->generic.right-s->generic.left+1, s->generic.bottom-s->generic.top+1, listbar_color );
		UI_DrawChar( x, y, 13, UI_CENTER|UI_BLINK|UI_SMALLFONT|UI_DROPSHADOW, color);
	}

	UI_DrawString( x - SMALLCHAR_WIDTH, y, s->generic.name, style|UI_RIGHT|UI_DROPSHADOW, color );
	UI_DrawString( x + SMALLCHAR_WIDTH, y, s->string, style|UI_LEFT|UI_DROPSHADOW, color );
}


/*
=======================================================================

CREATE MATCH (menu redesign)

One screen on the design canvas: map preview and strip, rules,
fighters and FIGHT!. The map list still comes from StartServer.

=======================================================================
*/

#define SO_ROW_X			736
#define SO_ROW_Y			166
#define SO_ROW_H			30
#define SO_MAX_ROWS			10
#define SO_THUMBS			4

static menucommon_s	*soRows[SO_MAX_ROWS];
static const char	*soRowLabels[SO_MAX_ROWS];
static int			soNumRows;
static int			soMapTime;

static void ServerOptions_AddRow( void *item, const char *label ) {
	float vx, vy, vw, vh;

	if ( soNumRows >= SO_MAX_ROWS ) {
		return;
	}
	vx = SO_ROW_X - 10; vy = SO_ROW_Y + soNumRows * SO_ROW_H - 4; vw = 492; vh = SO_ROW_H;
	UI_DToV( &vx, &vy, &vw, &vh );
	((menucommon_s *)item)->left = vx;
	((menucommon_s *)item)->top = vy;
	((menucommon_s *)item)->right = vx + vw;
	((menucommon_s *)item)->bottom = vy + vh;
	((menucommon_s *)item)->x = vx + vw * 0.5f;
	((menucommon_s *)item)->y = vy;
	soRows[soNumRows] = (menucommon_s *)item;
	soRowLabels[soNumRows] = label;
	soNumRows++;
}

static int ServerOptions_RowOf( void *item ) {
	int i;

	for ( i = 0; i < soNumRows; i++ ) {
		if ( soRows[i] == item ) {
			return i;
		}
	}
	return 0;
}

// label on the left, value on the right, ki highlight when focused
static float ServerOptions_RowFrame( void *item ) {
	int		row;
	float	y;
	float	focus;

	row = ServerOptions_RowOf( item );
	y = SO_ROW_Y + row * SO_ROW_H;
	focus = UI_DFocus( &s_serveroptions.menu, item );
	UI_DFocusRow( SO_ROW_X - 10, y - 4, 492, SO_ROW_H - 2, focus );
	UI_DText( SO_ROW_X, y, soRowLabels[row], 19, UI_LEFT, focus > 0.0f ? zeq_gold : zeq_text2 );
	return focus;
}

static void ServerOptions_DrawFieldRow( void *self ) {
	menufield_s	*f;
	float		focus;
	float		y;
	float		x;

	f = (menufield_s *)self;
	focus = ServerOptions_RowFrame( self );
	y = SO_ROW_Y + ServerOptions_RowOf( self ) * SO_ROW_H;
	x = SO_ROW_X + 220;
	UI_DText( x, y, f->field.buffer[0] ? f->field.buffer : "0", 19, UI_LEFT, zeq_white );
	if ( focus > 0.0f && ( ( uis.realtime / 400 ) & 1 ) ) {
		UI_DFill( x + UI_DTextWidth( f->field.buffer, 19 ) + 2, y, 2, 20, zeq_white );
	}
}

static void ServerOptions_DrawRadioRow( void *self ) {
	float y;

	ServerOptions_RowFrame( self );
	y = SO_ROW_Y + ServerOptions_RowOf( self ) * SO_ROW_H;
	UI_DToggle( SO_ROW_X + 220, y - 2, ((menuradiobutton_s *)self)->curvalue != 0 );
}

static void ServerOptions_DrawSpinRow( void *self ) {
	menulist_s	*l;
	float		focus;
	float		y;

	l = (menulist_s *)self;
	focus = ServerOptions_RowFrame( self );
	y = SO_ROW_Y + ServerOptions_RowOf( self ) * SO_ROW_H;
	UI_DSelector( SO_ROW_X + 210, y - 2, 170, l->itemnames[l->curvalue], focus );
}

static void ServerOptions_SetMap( int index ) {
	const char	*info;
	static char	picname[64];

	if ( !s_startserver.nummaps ) {
		return;
	}
	index = ( index + s_startserver.nummaps ) % s_startserver.nummaps;
	s_startserver.currentmap = index;
	info = UI_GetArenaInfoByNumber( s_startserver.maplist[index] );
	Q_strncpyz( s_serveroptions.mapnamebuffer, Info_ValueForKey( info, "map" ), sizeof( s_serveroptions.mapnamebuffer ) );
	Q_strncpyz( s_startserver.mapname.string, s_serveroptions.mapnamebuffer, MAX_NAMELENGTH );
	Com_sprintf( picname, sizeof( picname ), "maps/%s.jpg", s_serveroptions.mapnamebuffer );
	s_serveroptions.mappic.generic.name = picname;
	s_serveroptions.mappic.shader = 0;
	soMapTime = uis.realtime;
}

static int ServerOptions_FirstThumb( void ) {
	int first;

	first = s_startserver.currentmap - SO_THUMBS + 1;
	if ( first < 0 ) {
		first = 0;
	}
	if ( first > s_startserver.nummaps - SO_THUMBS ) {
		first = s_startserver.nummaps - SO_THUMBS;
	}
	if ( first < 0 ) {
		first = 0;
	}
	return first;
}

// "lastStand" -> "LAST STAND"
static void ServerOptions_MapTitle( const char *map, char *out, int size ) {
	int n, i;

	n = 0;
	for ( i = 0; map[i] && n < size - 2; i++ ) {
		if ( i > 0 && map[i] >= 'A' && map[i] <= 'Z' && map[i - 1] >= 'a' && map[i - 1] <= 'z' ) {
			out[n++] = ' ';
		}
		out[n++] = ( map[i] >= 'a' && map[i] <= 'z' ) ? map[i] - 'a' + 'A' : map[i];
	}
	out[n] = 0;
}

static void ServerOptions_DrawPreview( void *self ) {
	menubitmap_s	*b;
	const char		*info;
	char			title[64];
	float			focus;
	float			pop;
	vec4_t			edge;
	int				i;

	b = (menubitmap_s *)self;
	focus = UI_DFocus( &s_serveroptions.menu, self );
	pop = UI_AnimFrac( soMapTime, 0, 300 );

	trap_R_SetColor( NULL );
	UI_DPic( 48, 128, 640, 360, trap_R_RegisterShaderNoMip( va( "maps/%s.jpg", s_serveroptions.mapnamebuffer ) ) );
	for ( i = 0; i < 12; i++ ) {
		vec4_t shade;

		shade[0] = 0.02f; shade[1] = 0.024f; shade[2] = 0.10f;
		shade[3] = 0.08f * i;
		UI_DFill( 48, 368 + i * 10, 640, 10, shade );
	}
	edge[0] = 0.47f; edge[1] = 0.63f; edge[2] = 1.0f; edge[3] = 0.55f + 0.45f * focus;
	UI_DFrame( 48, 128, 640, 360, edge );

	ServerOptions_MapTitle( s_serveroptions.mapnamebuffer, title, sizeof( title ) );
	UI_DProp( 72 + 14.0f * ( 1.0f - pop ), 420, title, 34, UI_LEFT|UI_DROPSHADOW, zeq_white );
	info = UI_GetArenaInfoByNumber( s_startserver.maplist[s_startserver.currentmap] );
	UI_DText( 72, 458, va( "%s  -  map %i of %i", Info_ValueForKey( info, "longname" ), s_startserver.currentmap + 1, s_startserver.nummaps ),
		19, UI_LEFT, zeq_text2 );

	// arrows of the game, they pulse when the preview has the focus
	trap_R_SetColor( NULL );
	UI_DPic( 58 - 3.0f * focus, 282, 52, 52, trap_R_RegisterShaderNoMip( "interface/art/arrows_horz_left.png" ) );
	UI_DPic( 626 + 3.0f * focus, 282, 52, 52, trap_R_RegisterShaderNoMip( "interface/art/arrows_horz_right.png" ) );
}

static void ServerOptions_DrawThumb( void *self ) {
	menubitmap_s	*b;
	int				index;
	const char		*info;
	float			focus;
	qboolean		selected;
	vec4_t			dim;

	b = (menubitmap_s *)self;
	index = ServerOptions_FirstThumb() + b->generic.id - ID_THUMB0;
	if ( index >= s_startserver.nummaps ) {
		return;
	}
	info = UI_GetArenaInfoByNumber( s_startserver.maplist[index] );
	focus = UI_DFocus( &s_serveroptions.menu, self );
	selected = ( index == s_startserver.currentmap );
	if ( selected ) {
		UI_DFrame( b->generic.x - 4, b->generic.y - 4, b->width + 8, 84 + 8, zeq_gold );
		UI_DFrame( b->generic.x - 3, b->generic.y - 3, b->width + 6, 84 + 6, zeq_gold );
	}
	dim[0] = dim[1] = dim[2] = ( selected || focus > 0.0f ) ? 1.0f : 0.7f;
	dim[3] = 1.0f;
	trap_R_SetColor( dim );
	UI_DPic( b->generic.x, b->generic.y, b->width, 84, trap_R_RegisterShaderNoMip( va( "maps/%s.jpg", Info_ValueForKey( info, "map" ) ) ) );
	trap_R_SetColor( NULL );
	UI_DText( b->generic.x, b->generic.y + 92, Info_ValueForKey( info, "map" ), 16, UI_LEFT, ( selected || focus > 0.0f ) ? zeq_white : zeq_text2 );
}

static void ServerOptions_DrawFight( void *self ) {
	menubitmap_s *b;

	b = (menubitmap_s *)self;
	UI_DCta( b->generic.x, b->generic.y, b->width, b->height, "FIGHT!", UI_DFocus( &s_serveroptions.menu, self ) );
}

/*
=================
ServerOptions_DrawFighters

the human, then each bot with its level; the portraits rise in turn
=================
*/
static void ServerOptions_DrawFighters( void *self ) {
	menubitmap_s	*b;
	float			focus;
	float			px, rise;
	int				i;
	int				count;
	int				shown;
	char			model[64];
	char			*slash;
	uiRosterSlot_t	*slot;
	uiBotDef_t		*def;
	vec4_t			edge;

	b = (menubitmap_s *)self;
	focus = UI_DFocus( &s_serveroptions.menu, self );

	UI_DGlass( 712, 440, 520, 202 );
	if ( focus > 0.0f ) {
		edge[0] = 0.47f; edge[1] = 0.63f; edge[2] = 1.0f;
		edge[3] = focus * ( 0.6f + 0.3f * sin( uis.realtime / 220.0f ) );
		UI_DFrame( 720, 448, 504, 186, edge );
	}

	count = UI_Bots_RosterCount();
	UI_DSectionTitle( 736, 464, va( "FIGHTERS  %i OF 12", count + 1 ) );
	UI_DProp( 1208, 466, "MANAGE BOTS  >", 13, UI_RIGHT, focus > 0.0f ? zeq_white : zeq_ki );

	trap_Cvar_VariableStringBuffer( "model", model, sizeof( model ) );
	slash = strchr( model, '/' );
	if ( slash ) {
		*slash = 0;
	}
	px = 736;
	rise = 10.0f * ( 1.0f - UI_AnimFrac( uis.transitionTime, 80, 300 ) );
	UI_DPortrait( px, 500 + rise, 72, UI_Bots_Portrait( model ), qfalse, 0.0f );
	UI_DFrame( px - 3, 497 + rise, 78, 78, zeq_ki );
	UI_DProp( px + 36, 584 + rise, "YOU", 12, UI_CENTER, zeq_ki );

	shown = count < 5 ? count : 5;
	for ( i = 0; i < shown; i++ ) {
		slot = UI_Bots_RosterSlot( i );
		def = UI_Bots_Def( slot->def );
		px = 736 + ( i + 1 ) * 84;
		rise = 10.0f * ( 1.0f - UI_AnimFrac( uis.transitionTime, 130 + i * 50, 300 ) );
		UI_DPortrait( px, 500 + rise, 72, UI_Bots_Portrait( def->player ), qfalse, 0.0f );
		UI_DPips( px + 14, 584 + rise, slot->skill, 3, UI_AnimFrac( uis.transitionTime, 260 + i * 50, 300 ) );
		UI_DText( px + 36, 596 + rise, def->player, 14, UI_CENTER, zeq_text2 );
	}
	px = 736 + ( shown + 1 ) * 84;
	if ( count > shown ) {
		UI_DProp( px + 10, 520, va( "+%i", count - shown ), 26, UI_LEFT, zeq_white );
	}
	else if ( count < UI_MAX_ROSTER && px < 1160 ) {
		edge[0] = 0.29f; edge[1] = 0.39f; edge[2] = 0.74f; edge[3] = 0.8f;
		UI_DFrame( px, 500, 72, 72, edge );
		UI_DProp( px + 36, 520, "+", 30, UI_CENTER, zeq_ki );
		UI_DText( px + 36, 588, "Add bot", 14, UI_CENTER, zeq_text3 );
	}
}

/*
=================
ServerOptions_MenuDraw
=================
*/
static void ServerOptions_MenuDraw( void ) {
	static const char *hints[] = { "ARROWS", "Map", "B", "Bots", "ENTER", "Select", "ESC", "Back" };

	// the side strip is shared, set it again after coming back from the roster
	uis.menuamount = -1;
	uis.hideEarth = qtrue;

	UI_DHeader( "MAIN MENU  >  PLAY  >", "CREATE MATCH" );
	UI_DGlass( 712, 128, 520, s_serveroptions.rulesBottom - 128 );
	UI_DSectionTitle( 736, 140, "RULES" );

	Menu_Draw( &s_serveroptions.menu );
	UI_DHints( hints, 4 );
}

static sfxHandle_t ServerOptions_MenuKey( int key ) {
	void *item;

	item = Menu_ItemAtCursor( &s_serveroptions.menu );

	// typing in a rule field
	if ( item && ((menucommon_s *)item)->type == MTYPE_FIELD && key != K_ESCAPE && key != K_MOUSE2 &&
		key != K_UPARROW && key != K_DOWNARROW && key != K_TAB ) {
		return Menu_DefaultKey( &s_serveroptions.menu, key );
	}

	if ( ( key == K_LEFTARROW || key == K_RIGHTARROW ) &&
		( item == &s_serveroptions.preview || ( ((menucommon_s *)item)->id >= ID_THUMB0 && ((menucommon_s *)item)->id < ID_THUMB0 + SO_THUMBS ) ) ) {
		ServerOptions_SetMap( s_startserver.currentmap + ( key == K_LEFTARROW ? -1 : 1 ) );
		return menu_move_sound;
	}
	if ( key == 'b' || key == 'B' ) {
		UI_BotRosterMenu();
		return menu_move_sound;
	}
	return Menu_DefaultKey( &s_serveroptions.menu, key );
}

static void ServerOptions_DItem( menubitmap_s *b, int id, void (*draw)( void * ), int x, int y, int w, int h ) {
	b->generic.type			= MTYPE_BITMAP;
	b->generic.flags		= QMF_NODEFAULTINIT;
	b->generic.id			= id;
	b->generic.callback		= ServerOptions_Event;
	b->generic.ownerdraw	= draw;
	UI_DItem( b, x, y, w, h );
}

static void ServerOptions_FieldRow( menufield_s *f, const char *label, int chars ) {
	f->generic.type				= MTYPE_FIELD;
	f->generic.flags			= QMF_NODEFAULTINIT|QMF_NUMBERSONLY;
	f->generic.ownerdraw		= ServerOptions_DrawFieldRow;
	f->field.widthInChars		= chars;
	f->field.maxchars			= chars;
	ServerOptions_AddRow( f, label );
}

/*
=================
ServerOptions_MenuInit
=================
*/
static void ServerOptions_MenuInit( qboolean multiplayer ) {
	int i;

	memset( &s_serveroptions, 0 ,sizeof(serveroptions_t) );
	s_serveroptions.multiplayer = multiplayer;
	s_serveroptions.gametype = (int)Com_Clamp( 0, GT_MAX_GAME_TYPE - 1, trap_Cvar_VariableValue( "g_gameType" ) );
	soNumRows = 0;

	ServerOptions_Cache();

	s_serveroptions.menu.wrapAround = qtrue;
	s_serveroptions.menu.fullscreen = qtrue;
	s_serveroptions.menu.draw = ServerOptions_MenuDraw;
	s_serveroptions.menu.key = ServerOptions_MenuKey;

	// map preview and strip
	ServerOptions_DItem( &s_serveroptions.preview, ID_PREVIEW, ServerOptions_DrawPreview, 48, 128, 640, 360 );
	for ( i = 0; i < SO_THUMBS; i++ ) {
		ServerOptions_DItem( &s_serveroptions.thumbs[i], ID_THUMB0 + i, ServerOptions_DrawThumb, 48 + i * 164, 506, 148, 110 );
	}

	// rules
	ServerOptions_FieldRow( &s_serveroptions.fraglimit, "KO limit", 3 );
	ServerOptions_FieldRow( &s_serveroptions.timelimit, "Time limit (min)", 3 );
	ServerOptions_FieldRow( &s_serveroptions.powerlevel, "Power level", 5 );
	ServerOptions_FieldRow( &s_serveroptions.powerlevelMaximum, "Maximum power level", 5 );
	ServerOptions_FieldRow( &s_serveroptions.breakLimitRate, "Charge speed", 3 );

	s_serveroptions.pure.generic.type		= MTYPE_RADIOBUTTON;
	s_serveroptions.pure.generic.flags		= QMF_NODEFAULTINIT;
	s_serveroptions.pure.generic.ownerdraw	= ServerOptions_DrawRadioRow;
	ServerOptions_AddRow( &s_serveroptions.pure, "Pure server" );

	if( s_serveroptions.multiplayer ) {
		s_serveroptions.dedicated.generic.type		= MTYPE_SPINCONTROL;
		s_serveroptions.dedicated.generic.flags		= QMF_NODEFAULTINIT;
		s_serveroptions.dedicated.generic.id		= ID_DEDICATED;
		s_serveroptions.dedicated.generic.callback	= ServerOptions_Event;
		s_serveroptions.dedicated.generic.ownerdraw	= ServerOptions_DrawSpinRow;
		s_serveroptions.dedicated.itemnames			= dedicated_list;
		s_serveroptions.dedicated.numitems			= 3;
		ServerOptions_AddRow( &s_serveroptions.dedicated, "Dedicated" );

		s_serveroptions.hostname.generic.type		= MTYPE_FIELD;
		s_serveroptions.hostname.generic.flags		= QMF_NODEFAULTINIT;
		s_serveroptions.hostname.generic.ownerdraw	= ServerOptions_DrawFieldRow;
		s_serveroptions.hostname.field.widthInChars	= 24;
		s_serveroptions.hostname.field.maxchars		= 40;
		ServerOptions_AddRow( &s_serveroptions.hostname, "Server name" );
	}
	s_serveroptions.rulesBottom = SO_ROW_Y + soNumRows * SO_ROW_H + 12;

	// fighters and FIGHT!
	ServerOptions_DItem( &s_serveroptions.fighterPanel, ID_FIGHTERPANEL, ServerOptions_DrawFighters, 712, 440, 520, 202 );
	ServerOptions_DItem( &s_serveroptions.fightBtn, ID_GO, ServerOptions_DrawFight, 1060 + UI_DExtra(), 652, 172, 50 );

	// map picture, only its name is used now
	s_serveroptions.mappic.generic.type		= MTYPE_BITMAP;
	s_serveroptions.mappic.generic.flags	= QMF_INACTIVE|QMF_HIDDEN;

	UI_Bots_Load();

	Menu_AddItem( &s_serveroptions.menu, &s_serveroptions.preview );
	for ( i = 0; i < SO_THUMBS; i++ ) {
		Menu_AddItem( &s_serveroptions.menu, &s_serveroptions.thumbs[i] );
	}
	for ( i = 0; i < soNumRows; i++ ) {
		Menu_AddItem( &s_serveroptions.menu, soRows[i] );
	}
	Menu_AddItem( &s_serveroptions.menu, &s_serveroptions.fighterPanel );
	Menu_AddItem( &s_serveroptions.menu, &s_serveroptions.fightBtn );

	ServerOptions_SetMenuItems();
	ServerOptions_SetMap( s_startserver.currentmap );

	// fewer maps than thumbnails
	for ( i = 0; i < SO_THUMBS; i++ ) {
		if ( i >= s_startserver.nummaps ) {
			s_serveroptions.thumbs[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		}
	}
}

/*
=================
ServerOptions_Cache
=================
*/
void ServerOptions_Cache( void ) {
	trap_R_RegisterShaderNoMip( GAMESERVER_BACK0 );
	trap_R_RegisterShaderNoMip( GAMESERVER_BACK1 );
	trap_R_RegisterShaderNoMip( GAMESERVER_FIGHT0 );
	trap_R_RegisterShaderNoMip( GAMESERVER_FIGHT1 );
	trap_R_RegisterShaderNoMip( GAMESERVER_SELECT );
	trap_R_RegisterShaderNoMip( GAMESERVER_UNKNOWNMAP );
}


/*
=================
UI_ServerOptionsMenu
=================
*/
static void UI_ServerOptionsMenu( qboolean multiplayer ) {
	ServerOptions_MenuInit( multiplayer );
	UI_PushMenu( &s_serveroptions.menu );
	uis.menuamount = -1;
	// ENTER starts the match right away
	Menu_SetCursorToItem( &s_serveroptions.menu, &s_serveroptions.fightBtn );
	uis.hideEarth = qtrue;
	uis.showFrame = qfalse;
}



/*
=============================================================================

BOT SELECT MENU *****

=============================================================================
*/


#define BOTSELECT_BACK0			"interface/art/back_0"
#define BOTSELECT_BACK1			"interface/art/back_1"
#define BOTSELECT_ACCEPT0		"interface/art/accept_0"
#define BOTSELECT_ACCEPT1		"interface/art/accept_1"
#define BOTSELECT_SELECT		"interface/art/opponents_select"
#define BOTSELECT_SELECTED		"interface/art/opponents_selected"
#define BOTSELECT_ARROWS		"interface/art/gs_arrows_0"
#define BOTSELECT_ARROWSL		"interface/art/gs_arrows_l"
#define BOTSELECT_ARROWSR		"interface/art/gs_arrows_r"

#define PLAYERGRID_COLS			4
#define PLAYERGRID_ROWS			4
#define MAX_MODELSPERPAGE		(PLAYERGRID_ROWS * PLAYERGRID_COLS)


typedef struct {
	menuframework_s	menu;

	menutext_s		banner;

	menubitmap_s	pics[MAX_MODELSPERPAGE];
	menubitmap_s	picbuttons[MAX_MODELSPERPAGE];
	menutext_s		picnames[MAX_MODELSPERPAGE];

	menubitmap_s	arrows;
	menubitmap_s	left;
	menubitmap_s	right;

	menubitmap_s	go;
	menubitmap_s	back;

	int				numBots;
	int				modelpage;
	int				numpages;
	int				selectedmodel;
	int				sortedBotNums[MAX_BOTS];
	char			boticons[MAX_MODELSPERPAGE][MAX_QPATH];
	char			botnames[MAX_MODELSPERPAGE][16];
} botSelectInfo_t;

static botSelectInfo_t	botSelectInfo;


/*
=================
UI_BotSelectMenu_SortCompare
=================
*/
static int QDECL UI_BotSelectMenu_SortCompare( const void *arg1, const void *arg2 ) {
	int			num1, num2;
	const char	*info1, *info2;
	const char	*name1, *name2;

	num1 = *(int *)arg1;
	num2 = *(int *)arg2;

	info1 = UI_GetBotInfoByNumber( num1 );
	info2 = UI_GetBotInfoByNumber( num2 );

	name1 = Info_ValueForKey( info1, "name" );
	name2 = Info_ValueForKey( info2, "name" );

	return Q_stricmp( name1, name2 );
}


/*
=================
UI_BotSelectMenu_BuildList
=================
*/
static void UI_BotSelectMenu_BuildList( void ) {
	int		n;

	botSelectInfo.modelpage = 0;
	botSelectInfo.numBots = UI_GetNumBots();
	botSelectInfo.numpages = botSelectInfo.numBots / MAX_MODELSPERPAGE;
	if( botSelectInfo.numBots % MAX_MODELSPERPAGE ) {
		botSelectInfo.numpages++;
	}

	// initialize the array
	for( n = 0; n < botSelectInfo.numBots; n++ ) {
		botSelectInfo.sortedBotNums[n] = n;
	}

	// now sort it
	qsort( botSelectInfo.sortedBotNums, botSelectInfo.numBots, sizeof(botSelectInfo.sortedBotNums[0]), UI_BotSelectMenu_SortCompare );
}


/*
=================
ServerPlayerIcon
=================
*/
static void ServerPlayerIcon( const char *modelAndSkin, char *iconName, int iconNameMaxSize ) {
	char	*skin;
	char	model[MAX_QPATH];

	Q_strncpyz( model, modelAndSkin, sizeof(model));
	skin = Q_strrchr( model, '/' );
	if ( skin ) {
		*skin++ = '\0';
	}
	else {
		skin = "default";
	}

	Com_sprintf(iconName, iconNameMaxSize, "players//%s/icon_%s.png", model, skin );

	if( !trap_R_RegisterShaderNoMip( iconName ) && Q_stricmp( skin, "default" ) != 0 ) {
		Com_sprintf(iconName, iconNameMaxSize, "players//%s/icon_default.png", model );
	}
}


/*
=================
UI_BotSelectMenu_UpdateGrid
=================
*/
static void UI_BotSelectMenu_UpdateGrid( void ) {
	const char	*info;
	int			i;
    int			j;

	j = botSelectInfo.modelpage * MAX_MODELSPERPAGE;
	for( i = 0; i < (PLAYERGRID_ROWS * PLAYERGRID_COLS); i++, j++) {
		if( j < botSelectInfo.numBots ) {
			info = UI_GetBotInfoByNumber( botSelectInfo.sortedBotNums[j] );
			ServerPlayerIcon( Info_ValueForKey( info, "model" ), botSelectInfo.boticons[i], MAX_QPATH );
			Q_strncpyz( botSelectInfo.botnames[i], Info_ValueForKey( info, "name" ), 16 );
			Q_CleanStr( botSelectInfo.botnames[i] );
 			botSelectInfo.pics[i].generic.name = botSelectInfo.boticons[i];
			if( BotAlreadySelected( botSelectInfo.botnames[i] ) ) {
				botSelectInfo.picnames[i].color = color_red;
			}
			else {
				botSelectInfo.picnames[i].color = color_white;
			}
			botSelectInfo.picbuttons[i].generic.flags &= ~QMF_INACTIVE;
		}
		else {
			// dead slot
 			botSelectInfo.pics[i].generic.name         = NULL;
			botSelectInfo.picbuttons[i].generic.flags |= QMF_INACTIVE;
			botSelectInfo.botnames[i][0] = 0;
		}

 		botSelectInfo.pics[i].generic.flags       &= ~QMF_HIGHLIGHT;
 		botSelectInfo.pics[i].shader               = 0;
 		botSelectInfo.picbuttons[i].generic.flags |= QMF_PULSEIFFOCUS;
	}

	// set selected model
	i = botSelectInfo.selectedmodel % MAX_MODELSPERPAGE;
	botSelectInfo.pics[i].generic.flags |= QMF_HIGHLIGHT;
	botSelectInfo.picbuttons[i].generic.flags &= ~QMF_PULSEIFFOCUS;

	if( botSelectInfo.numpages > 1 ) {
		if( botSelectInfo.modelpage > 0 ) {
			botSelectInfo.left.generic.flags &= ~QMF_INACTIVE;
		}
		else {
			botSelectInfo.left.generic.flags |= QMF_INACTIVE;
		}

		if( botSelectInfo.modelpage < (botSelectInfo.numpages - 1) ) {
			botSelectInfo.right.generic.flags &= ~QMF_INACTIVE;
		}
		else {
			botSelectInfo.right.generic.flags |= QMF_INACTIVE;
		}
	}
	else {
		// hide left/right markers
		botSelectInfo.left.generic.flags |= QMF_INACTIVE;
		botSelectInfo.right.generic.flags |= QMF_INACTIVE;
	}
}


/*
=================
UI_BotSelectMenu_Default
=================
*/
static void UI_BotSelectMenu_Default( char *bot ) {
	const char	*botInfo;
	const char	*test;
	int			n;
	int			i;

	for( n = 0; n < botSelectInfo.numBots; n++ ) {
		botInfo = UI_GetBotInfoByNumber( n );
		test = Info_ValueForKey( botInfo, "name" );
		if( Q_stricmp( bot, test ) == 0 ) {
			break;
		}
	}
	if( n == botSelectInfo.numBots ) {
		botSelectInfo.selectedmodel = 0;
		return;
	}

	for( i = 0; i < botSelectInfo.numBots; i++ ) {
		if( botSelectInfo.sortedBotNums[i] == n ) {
			break;
		}
	}
	if( i == botSelectInfo.numBots ) {
		botSelectInfo.selectedmodel = 0;
		return;
	}

	botSelectInfo.selectedmodel = i;
}


/*
=================
UI_BotSelectMenu_LeftEvent
=================
*/
static void UI_BotSelectMenu_LeftEvent( void* ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}
	if( botSelectInfo.modelpage > 0 ) {
		botSelectInfo.modelpage--;
		botSelectInfo.selectedmodel = botSelectInfo.modelpage * MAX_MODELSPERPAGE;
		UI_BotSelectMenu_UpdateGrid();
	}
}


/*
=================
UI_BotSelectMenu_RightEvent
=================
*/
static void UI_BotSelectMenu_RightEvent( void* ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}
	if( botSelectInfo.modelpage < botSelectInfo.numpages - 1 ) {
		botSelectInfo.modelpage++;
		botSelectInfo.selectedmodel = botSelectInfo.modelpage * MAX_MODELSPERPAGE;
		UI_BotSelectMenu_UpdateGrid();
	}
}


/*
=================
UI_BotSelectMenu_BotEvent
=================
*/
static void UI_BotSelectMenu_BotEvent( void* ptr, int event ) {
	int		i;

	if( event != QM_ACTIVATED ) {
		return;
	}

	for( i = 0; i < (PLAYERGRID_ROWS * PLAYERGRID_COLS); i++ ) {
 		botSelectInfo.pics[i].generic.flags &= ~QMF_HIGHLIGHT;
 		botSelectInfo.picbuttons[i].generic.flags |= QMF_PULSEIFFOCUS;
	}

	// set selected
	i = ((menucommon_s*)ptr)->id;
	botSelectInfo.pics[i].generic.flags |= QMF_HIGHLIGHT;
	botSelectInfo.picbuttons[i].generic.flags &= ~QMF_PULSEIFFOCUS;
	botSelectInfo.selectedmodel = botSelectInfo.modelpage * MAX_MODELSPERPAGE + i;
}


/*
=================
UI_BotSelectMenu_BackEvent
=================
*/
static void UI_BotSelectMenu_BackEvent( void* ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}
	UI_PopMenu();
}


/*
=================
UI_BotSelectMenu_SelectEvent
=================
*/
static void UI_BotSelectMenu_SelectEvent( void* ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}
	UI_PopMenu();

	s_serveroptions.newBot = qtrue;
	Q_strncpyz( s_serveroptions.newBotName, botSelectInfo.botnames[botSelectInfo.selectedmodel % MAX_MODELSPERPAGE], 16 );
}


/*
=================
UI_BotSelectMenu_Cache
=================
*/
void UI_BotSelectMenu_Cache( void ) {
	trap_R_RegisterShaderNoMip( BOTSELECT_BACK0 );
	trap_R_RegisterShaderNoMip( BOTSELECT_BACK1 );
	trap_R_RegisterShaderNoMip( BOTSELECT_ACCEPT0 );
	trap_R_RegisterShaderNoMip( BOTSELECT_ACCEPT1 );
	trap_R_RegisterShaderNoMip( BOTSELECT_SELECT );
	trap_R_RegisterShaderNoMip( BOTSELECT_SELECTED );
	trap_R_RegisterShaderNoMip( BOTSELECT_ARROWS );
	trap_R_RegisterShaderNoMip( BOTSELECT_ARROWSL );
	trap_R_RegisterShaderNoMip( BOTSELECT_ARROWSR );
}


static void UI_BotSelectMenu_Init( char *bot ) {
	int		i, j, k;
	int		x, y;

	memset( &botSelectInfo, 0 ,sizeof(botSelectInfo) );
	botSelectInfo.menu.wrapAround = qtrue;
	botSelectInfo.menu.fullscreen = qtrue;

	UI_BotSelectMenu_Cache();

	botSelectInfo.banner.generic.type	= MTYPE_BTEXT;
	botSelectInfo.banner.generic.x		= 320;
	botSelectInfo.banner.generic.y		= 16;
	botSelectInfo.banner.string			= "SELECT BOT";
	botSelectInfo.banner.color			= color_white;
	botSelectInfo.banner.style			= UI_CENTER|UI_DROPSHADOW;

	y =	80;
	for( i = 0, k = 0; i < PLAYERGRID_ROWS; i++) {
		x =	180;
		for( j = 0; j < PLAYERGRID_COLS; j++, k++ ) {
			botSelectInfo.pics[k].generic.type				= MTYPE_BITMAP;
			botSelectInfo.pics[k].generic.flags				= QMF_LEFT_JUSTIFY|QMF_INACTIVE;
			botSelectInfo.pics[k].generic.x					= x;
			botSelectInfo.pics[k].generic.y					= y;
 			botSelectInfo.pics[k].generic.name				= botSelectInfo.boticons[k];
			botSelectInfo.pics[k].width						= 64;
			botSelectInfo.pics[k].height					= 64;
			botSelectInfo.pics[k].focuspic					= BOTSELECT_SELECTED;
			botSelectInfo.pics[k].focuscolor				= colorRed;

			botSelectInfo.picbuttons[k].generic.type		= MTYPE_BITMAP;
			botSelectInfo.picbuttons[k].generic.flags		= QMF_LEFT_JUSTIFY|QMF_NODEFAULTINIT|QMF_PULSEIFFOCUS;
			botSelectInfo.picbuttons[k].generic.callback	= UI_BotSelectMenu_BotEvent;
			botSelectInfo.picbuttons[k].generic.id			= k;
			botSelectInfo.picbuttons[k].generic.x			= x - 16;
			botSelectInfo.picbuttons[k].generic.y			= y - 16;
			botSelectInfo.picbuttons[k].generic.left		= x;
			botSelectInfo.picbuttons[k].generic.top			= y;
			botSelectInfo.picbuttons[k].generic.right		= x + 64;
			botSelectInfo.picbuttons[k].generic.bottom		= y + 64;
			botSelectInfo.picbuttons[k].width				= 128;
			botSelectInfo.picbuttons[k].height				= 128;
			botSelectInfo.picbuttons[k].focuspic			= BOTSELECT_SELECT;
			botSelectInfo.picbuttons[k].focuscolor			= colorRed;

			botSelectInfo.picnames[k].generic.type			= MTYPE_TEXT;
			botSelectInfo.picnames[k].generic.flags			= QMF_SMALLFONT;
			botSelectInfo.picnames[k].generic.x				= x + 32;
			botSelectInfo.picnames[k].generic.y				= y + 64;
			botSelectInfo.picnames[k].string				= botSelectInfo.botnames[k];
			botSelectInfo.picnames[k].color					= color_white;
			botSelectInfo.picnames[k].style					= UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW;

			x += (64 + 6);
		}
		y += (64 + SMALLCHAR_HEIGHT + 6);
	}

	botSelectInfo.arrows.generic.type		= MTYPE_BITMAP;
	botSelectInfo.arrows.generic.name		= BOTSELECT_ARROWS;
	botSelectInfo.arrows.generic.flags		= QMF_INACTIVE;
	botSelectInfo.arrows.generic.x			= 260;
	botSelectInfo.arrows.generic.y			= 440;
	botSelectInfo.arrows.width				= 128;
	botSelectInfo.arrows.height				= 32;

	botSelectInfo.left.generic.type			= MTYPE_BITMAP;
	botSelectInfo.left.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	botSelectInfo.left.generic.callback		= UI_BotSelectMenu_LeftEvent;
	botSelectInfo.left.generic.x			= 260;
	botSelectInfo.left.generic.y			= 440;
	botSelectInfo.left.width  				= 64;
	botSelectInfo.left.height  				= 32;
	botSelectInfo.left.focuspic				= BOTSELECT_ARROWSL;

	botSelectInfo.right.generic.type	    = MTYPE_BITMAP;
	botSelectInfo.right.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	botSelectInfo.right.generic.callback	= UI_BotSelectMenu_RightEvent;
	botSelectInfo.right.generic.x			= 321;
	botSelectInfo.right.generic.y			= 440;
	botSelectInfo.right.width  				= 64;
	botSelectInfo.right.height  		    = 32;
	botSelectInfo.right.focuspic			= BOTSELECT_ARROWSR;

	botSelectInfo.back.generic.type		= MTYPE_BITMAP;
	botSelectInfo.back.generic.name		= BOTSELECT_BACK0;
	botSelectInfo.back.generic.flags	= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	botSelectInfo.back.generic.callback	= UI_BotSelectMenu_BackEvent;
	botSelectInfo.back.generic.x		= 0;
	botSelectInfo.back.generic.y		= 480-64;
	botSelectInfo.back.width			= 128;
	botSelectInfo.back.height			= 64;
	botSelectInfo.back.focuspic			= BOTSELECT_BACK1;

	botSelectInfo.go.generic.type		= MTYPE_BITMAP;
	botSelectInfo.go.generic.name		= BOTSELECT_ACCEPT0;
	botSelectInfo.go.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	botSelectInfo.go.generic.callback	= UI_BotSelectMenu_SelectEvent;
	botSelectInfo.go.generic.x			= 640;
	botSelectInfo.go.generic.y			= 480-64;
	botSelectInfo.go.width				= 128;
	botSelectInfo.go.height				= 64;
	botSelectInfo.go.focuspic			= BOTSELECT_ACCEPT1;

	for( i = 0; i < MAX_MODELSPERPAGE; i++ ) {
		Menu_AddItem( &botSelectInfo.menu,	&botSelectInfo.pics[i] );
		Menu_AddItem( &botSelectInfo.menu,	&botSelectInfo.picbuttons[i] );
		Menu_AddItem( &botSelectInfo.menu,	&botSelectInfo.picnames[i] );
	}
	Menu_AddItem( &botSelectInfo.menu, &botSelectInfo.arrows );
	Menu_AddItem( &botSelectInfo.menu, &botSelectInfo.left );
	Menu_AddItem( &botSelectInfo.menu, &botSelectInfo.right );
	Menu_AddItem( &botSelectInfo.menu, &botSelectInfo.back );
	Menu_AddItem( &botSelectInfo.menu, &botSelectInfo.go );

	UI_BotSelectMenu_BuildList();
	UI_BotSelectMenu_Default( bot );
	botSelectInfo.modelpage = botSelectInfo.selectedmodel / MAX_MODELSPERPAGE;
	UI_BotSelectMenu_UpdateGrid();
}


/*
=================
UI_BotSelectMenu
=================
*/
void UI_BotSelectMenu( char *bot ) {
	UI_BotSelectMenu_Init( bot );
	UI_PushMenu( &botSelectInfo.menu );
}
