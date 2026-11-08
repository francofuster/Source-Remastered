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

MULTIPLAYER MENU (SERVER BROWSER)

=======================================================================
*/


#include "ui_local.h"


#define MAX_GLOBALSERVERS		128
#define MAX_PINGREQUESTS		32
#define MAX_ADDRESSLENGTH		64
#define MAX_HOSTNAMELENGTH		22
#define MAX_MAPNAMELENGTH		16
#define MAX_LISTBOXITEMS		128
#define MAX_LOCALSERVERS		128
#define MAX_STATUSLENGTH		64
#define MAX_LEAGUELENGTH		28
#define MAX_LISTBOXWIDTH		68

#define ART_BACK0				"interface/art/back_0"
#define ART_BACK1				"interface/art/back_1"
#define ART_CREATE0				"interface/art/create_0"
#define ART_CREATE1				"interface/art/create_1"
#define ART_SPECIFY0			"interface/art/specify_0"
#define ART_SPECIFY1			"interface/art/specify_1"
#define ART_REFRESH0			"interface/art/refresh_0"
#define ART_REFRESH1			"interface/art/refresh_1"
#define ART_CONNECT0			"interface/art/fight_0"
#define ART_CONNECT1			"interface/art/fight_1"
#define ART_ARROWS0				"interface/art/arrows_vert_0"
#define ART_ARROWS_UP			"interface/art/arrows_vert_top"
#define ART_ARROWS_DOWN			"interface/art/arrows_vert_bot"
#define ART_UNKNOWNMAP			"interface/art/unknownmap"
#define ART_REMOVE0				"interface/art/delete_0"
#define ART_REMOVE1				"interface/art/delete_1"

#define ID_MASTER			10
#define ID_GAMETYPE			11
#define ID_SORTKEY			12
#define ID_SHOW_FULL		13
#define ID_SHOW_EMPTY		14
#define ID_LIST				15
#define ID_SCROLL_UP		16
#define ID_SCROLL_DOWN		17
#define ID_BACK				18
#define ID_CONNECT			19
#define ID_CREATE			20
#define ID_REFRESH			21
#define ID_SPECIFY			22
#define ID_REMOVE			23
#define ID_TAB0				30		// + tab
#define ID_SORTSEL			33
#define ID_CONNECTTYPED		34

#define GR_LOGO				30
#define GR_LETTERS			31

#define UIAS_LOCAL			0
#define UIAS_GLOBAL1			1
#define UIAS_GLOBAL2			2
#define UIAS_GLOBAL3			3
#define UIAS_GLOBAL4			4
#define UIAS_GLOBAL5			5
#define UIAS_FAVORITES			6

#define SORT_HOST			0
#define SORT_MAP			1
#define SORT_CLIENTS		2
#define SORT_GAME			3
#define SORT_PING			4

#define GAMES_ALL			0
#define GAMES_STRUGGLE		1

static const char *master_items[] = {
	"Local",
	"Internet",
	"Internet2",
	"Internet3",
	"Internet4",
	"Internet5",
	"Favorites",
	NULL
};

static const char *servertype_items[] = {
	//"All",
	"Struggle",
	NULL
};

static const char *sortkey_items[] = {
	"Server Name",
	"Map Name",
	"Open Player Spots",
	"Game Type",
	"Ping Time",
	NULL
};

static char* gamenames[] = {
/*
	"DM ",	// deathmatch
	"1v1",	// tournament
	"SP ",	// single player
	"Team DM",	// team deathmatch
	"CTF",	// capture the flag
	"One Flag CTF",		// one flag ctf
	"OverLoad",				// Overload
	"Harvester",			// Harvester
	"Rocket Arena 3",	// Rocket Arena 3
	"Q3F",						// Q3F
	"Urban Terror",		// Urban Terror
	"OSP",						// Orange Smoothie Productions
*/
	"ZEQ2-Lite",	// ZEQ2Lite
	"???",			// unknown
	NULL
};

static char* netnames[] = {
	"??? ",
	"UDP ",
	"UDP6",
	NULL
};

static char quake3worldMessage[] = "";

typedef struct {
	char	adrstr[MAX_ADDRESSLENGTH];
	int		start;
} pinglist_t;

typedef struct servernode_s {
	char	adrstr[MAX_ADDRESSLENGTH];
	char	hostname[MAX_HOSTNAMELENGTH+3];
	char	mapname[MAX_MAPNAMELENGTH];
	int		numclients;
	int		maxclients;
	int		pingtime;
	int		gametype;
	char	gamename[16];
	int		nettype;
	int		minPing;
	int		maxPing;

} servernode_t;

typedef struct {
	char			buff[MAX_LISTBOXWIDTH];
	servernode_t*	servernode;
} table_t;

typedef struct {
	menuframework_s		menu;

	menutext_s			banner;

	menulist_s			master;
	menulist_s			gametype;
	menulist_s			sortkey;
	menuradiobutton_s	showfull;
	menuradiobutton_s	showempty;

	menulist_s			list;
	menubitmap_s		mappic;
	menubitmap_s		arrows;
	menubitmap_s		up;
	menubitmap_s		down;
	menutext_s			status;
	menutext_s			statusbar;

	menutext_s			go;
	menutext_s			create;
	menutext_s			refresh;
	menutext_s			specify;
	menutext_s			back;
	menubitmap_s		remove;

	// menu redesign
	menubitmap_s		tabs[4];		// the fourth, ADDRESS, only on a phone
	menubitmap_s		sortsel;
	menubitmap_s		refreshBtn;
	menubitmap_s		joinBtn;
	menubitmap_s		createBtn;
	menubitmap_s		connectBtn;
	menufield_s			address;

	pinglist_t			pinglist[MAX_PINGREQUESTS];
	table_t				table[MAX_LISTBOXITEMS];
	char*				items[MAX_LISTBOXITEMS];
	int					numqueriedservers;
	int					*numservers;
	servernode_t		*serverlist;
	int					currentping;
	qboolean			refreshservers;
	int					nextpingtime;
	int					maxservers;
	int					refreshtime;
	char				favoriteaddresses[MAX_FAVORITESERVERS][MAX_ADDRESSLENGTH];
	int					numfavoriteaddresses;

} arenaservers_t;

static arenaservers_t	g_arenaservers;


static servernode_t		g_globalserverlist[MAX_GLOBALSERVERS];
static int				g_numglobalservers;
static servernode_t		g_localserverlist[MAX_LOCALSERVERS];
static int				g_numlocalservers;
static servernode_t		g_favoriteserverlist[MAX_FAVORITESERVERS];
static int				g_numfavoriteservers;
static int				g_servertype;
static int				g_gametype;
static int				g_sortkey;
static int				g_emptyservers;
static int				g_fullservers;


/*
=================
ArenaServers_MaxPing
=================
*/
static int ArenaServers_MaxPing( void ) {
	int		maxPing;

	maxPing = (int)trap_Cvar_VariableValue( "cl_maxPing" );
	if( maxPing < 100 ) {
		maxPing = 100;
	}
	return maxPing;
}


/*
=================
ArenaServers_Compare
=================
*/
static int QDECL ArenaServers_Compare( const void *arg1, const void *arg2 ) {
	float			f1;
	float			f2;
	servernode_t*	t1;
	servernode_t*	t2;

	t1 = (servernode_t *)arg1;
	t2 = (servernode_t *)arg2;

	switch( g_sortkey ) {
	case SORT_HOST:
		return Q_stricmp( t1->hostname, t2->hostname );

	case SORT_MAP:
		return Q_stricmp( t1->mapname, t2->mapname );

	case SORT_CLIENTS:
		f1 = t1->maxclients - t1->numclients;
		if( f1 < 0 ) {
			f1 = 0;
		}

		f2 = t2->maxclients - t2->numclients;
		if( f2 < 0 ) {
			f2 = 0;
		}

		if( f1 < f2 ) {
			return 1;
		}
		if( f1 == f2 ) {
			return 0;
		}
		return -1;

	case SORT_GAME:
		if( t1->gametype < t2->gametype ) {
			return -1;
		}
		if( t1->gametype == t2->gametype ) {
			return 0;
		}
		return 1;

	case SORT_PING:
		if( t1->pingtime < t2->pingtime ) {
			return -1;
		}
		if( t1->pingtime > t2->pingtime ) {
			return 1;
		}
		return Q_stricmp( t1->hostname, t2->hostname );
	}

	return 0;
}


/*
=================
ArenaServers_Go
=================
*/
static void ArenaServers_Go( void ) {
	servernode_t*	servernode;

	servernode = g_arenaservers.table[g_arenaservers.list.curvalue].servernode;
	if( servernode ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "connect %s\n", servernode->adrstr ) );
	}
}


/*
=================
ArenaServers_UpdatePicture
=================
*/
static void ArenaServers_UpdatePicture( void ) {
	static char		picname[64];
	servernode_t*	servernodeptr;

	if( !g_arenaservers.list.numitems ) {
		g_arenaservers.mappic.generic.name = NULL;
	}
	else {
		servernodeptr = g_arenaservers.table[g_arenaservers.list.curvalue].servernode;
		Com_sprintf( picname, sizeof(picname), "maps/%s.png", servernodeptr->mapname );
		g_arenaservers.mappic.generic.name = picname;

	}

	// force shader update during draw
	g_arenaservers.mappic.shader = 0;
}


/*
=================
ArenaServers_UpdateMenu
=================
*/
static void ArenaServers_UpdateMenu( void ) {
	int				i;
	int				j;
	int				count;
	char*			buff;
	servernode_t*	servernodeptr;
	table_t*		tableptr;
	char			*pingColor;

	if( g_arenaservers.numqueriedservers > 0 ) {
		// servers found
		if( g_arenaservers.refreshservers && ( g_arenaservers.currentping <= g_arenaservers.numqueriedservers ) ) {
			// show progress
			Com_sprintf( g_arenaservers.status.string, MAX_STATUSLENGTH, "%d of %d ZEQ2-Lite Servers.", g_arenaservers.currentping, g_arenaservers.numqueriedservers);
			g_arenaservers.statusbar.string  = "Press SPACE to stop";
			qsort( g_arenaservers.serverlist, *g_arenaservers.numservers, sizeof( servernode_t ), ArenaServers_Compare);
		}
		else {
			// all servers pinged - enable controls
			g_arenaservers.master.generic.flags		&= ~QMF_GRAYED;
			g_arenaservers.gametype.generic.flags	&= ~QMF_GRAYED;
			g_arenaservers.sortkey.generic.flags	&= ~QMF_GRAYED;
			g_arenaservers.showempty.generic.flags	&= ~QMF_GRAYED;
			g_arenaservers.showfull.generic.flags	&= ~QMF_GRAYED;
			g_arenaservers.list.generic.flags		&= ~QMF_GRAYED;
			g_arenaservers.refresh.generic.flags	&= ~QMF_GRAYED;
			g_arenaservers.go.generic.flags			&= ~QMF_GRAYED;

			// update status bar
			if( g_servertype >= UIAS_GLOBAL1 && g_servertype <= UIAS_GLOBAL5 ) {
				g_arenaservers.statusbar.string = quake3worldMessage;
			}
			else {
				g_arenaservers.statusbar.string = "";
			}

		}
	}
	else {
		// no servers found
		if( g_arenaservers.refreshservers ) {
			strcpy( g_arenaservers.status.string,"Scanning For Servers." );
			g_arenaservers.statusbar.string = "Press SPACE to stop";

			// disable controls during refresh
			g_arenaservers.master.generic.flags		|= QMF_GRAYED;
			g_arenaservers.gametype.generic.flags	|= QMF_GRAYED;
			g_arenaservers.sortkey.generic.flags	|= QMF_GRAYED;
			g_arenaservers.showempty.generic.flags	|= QMF_GRAYED;
			g_arenaservers.showfull.generic.flags	|= QMF_GRAYED;
			g_arenaservers.list.generic.flags		|= QMF_GRAYED;
			g_arenaservers.refresh.generic.flags	|= QMF_GRAYED;
			g_arenaservers.go.generic.flags			|= QMF_GRAYED;
		}
		else {
			if( g_arenaservers.numqueriedservers < 0 ) {
				strcpy(g_arenaservers.status.string,"No Response From Master Server." );
			}
			else {
				strcpy(g_arenaservers.status.string,"No Servers Found." );
			}

			// update status bar
			if( g_servertype >= UIAS_GLOBAL1 && g_servertype <= UIAS_GLOBAL5 ) {
				g_arenaservers.statusbar.string = quake3worldMessage;
			}
			else {
				g_arenaservers.statusbar.string = "";
			}

			// end of refresh - set control state
			g_arenaservers.master.generic.flags		&= ~QMF_GRAYED;
			g_arenaservers.gametype.generic.flags	&= ~QMF_GRAYED;
			g_arenaservers.sortkey.generic.flags	&= ~QMF_GRAYED;
			g_arenaservers.showempty.generic.flags	&= ~QMF_GRAYED;
			g_arenaservers.showfull.generic.flags	&= ~QMF_GRAYED;
			g_arenaservers.list.generic.flags		|= QMF_GRAYED;
			g_arenaservers.refresh.generic.flags	&= ~QMF_GRAYED;
			g_arenaservers.go.generic.flags			|= QMF_GRAYED;
		}

		// zero out list box
		g_arenaservers.joinBtn.generic.flags |= QMF_GRAYED;
		g_arenaservers.list.numitems = 0;
		g_arenaservers.list.curvalue = 0;
		g_arenaservers.list.top      = 0;

		// update picture
		ArenaServers_UpdatePicture();
		return;
	}

	// build list box strings - apply culling filters
	servernodeptr = g_arenaservers.serverlist;
	count         = *g_arenaservers.numservers;
	for( i = 0, j = 0; i < count; i++, servernodeptr++ ) {
		tableptr = &g_arenaservers.table[j];
		tableptr->servernode = servernodeptr;
		buff = tableptr->buff;

		// can only cull valid results
		if( !g_emptyservers && !servernodeptr->numclients ) {
			continue;
		}

		if( !g_fullservers && ( servernodeptr->numclients == servernodeptr->maxclients ) ) {
			continue;
		}
/*
		if( Q_strncmp( "zeq2", servernodeptr->gamename, 4) ) {
			continue;
		}
*/
		switch( g_gametype ) {
		case GAMES_ALL:
			break;

		case GAMES_STRUGGLE:
			if( servernodeptr->gametype != GT_STRUGGLE ) {
				continue;
			}
			break;
		}

		if( servernodeptr->pingtime < servernodeptr->minPing ) {
			pingColor = S_COLOR_BLUE;
		}
		else if( servernodeptr->maxPing && servernodeptr->pingtime > servernodeptr->maxPing ) {
			pingColor = S_COLOR_BLUE;
		}
		else if( servernodeptr->pingtime < 200 ) {
			pingColor = S_COLOR_GREEN;
		}
		else if( servernodeptr->pingtime < 400 ) {
			pingColor = S_COLOR_YELLOW;
		}
		else {
			pingColor = S_COLOR_RED;
		}

		Com_sprintf( buff, MAX_LISTBOXWIDTH, "%-20.20s %-12.12s %2d/%2d %-8.8s %3s %s%3d",
			servernodeptr->hostname, servernodeptr->mapname, servernodeptr->numclients,
 			servernodeptr->maxclients, servernodeptr->gamename,
			netnames[servernodeptr->nettype], pingColor, servernodeptr->pingtime );

		j++;
	}

	g_arenaservers.list.numitems = j;
	g_arenaservers.list.curvalue = 0;
	g_arenaservers.list.top      = 0;
	if ( j ) {
		g_arenaservers.joinBtn.generic.flags &= ~QMF_GRAYED;
	}
	else {
		g_arenaservers.joinBtn.generic.flags |= QMF_GRAYED;
	}

	// update picture
	ArenaServers_UpdatePicture();
}


/*
=================
ArenaServers_Remove
=================
*/
static void ArenaServers_Remove( void )
{
	int				i;
	servernode_t*	servernodeptr;
	table_t*		tableptr;

	if (!g_arenaservers.list.numitems)
		return;

	// remove selected item from display list
	// items are in scattered order due to sort and cull
	// perform delete on list box contents, resync all lists

	tableptr      = &g_arenaservers.table[g_arenaservers.list.curvalue];
	servernodeptr = tableptr->servernode;

	// find address in master list
	for (i=0; i<g_arenaservers.numfavoriteaddresses; i++)
	{
		if (!Q_stricmp(g_arenaservers.favoriteaddresses[i],servernodeptr->adrstr))
		{
			// delete address from master list
			if (i < g_arenaservers.numfavoriteaddresses-1)
			{
				// shift items up
				memcpy( &g_arenaservers.favoriteaddresses[i], &g_arenaservers.favoriteaddresses[i+1], (g_arenaservers.numfavoriteaddresses - i - 1)* MAX_ADDRESSLENGTH );
			}
			g_arenaservers.numfavoriteaddresses--;
			memset( &g_arenaservers.favoriteaddresses[g_arenaservers.numfavoriteaddresses], 0, MAX_ADDRESSLENGTH );
			break;
		}
	}

	// find address in server list
	for (i=0; i<g_numfavoriteservers; i++)
	{
		if (&g_favoriteserverlist[i] == servernodeptr)
		{

			// delete address from server list
			if (i < g_numfavoriteservers-1)
			{
				// shift items up
				memcpy( &g_favoriteserverlist[i], &g_favoriteserverlist[i+1], (g_numfavoriteservers - i - 1)*sizeof(servernode_t));
			}
			g_numfavoriteservers--;
			memset( &g_favoriteserverlist[ g_numfavoriteservers ], 0, sizeof(servernode_t));
			break;
		}
	}

	g_arenaservers.numqueriedservers = g_arenaservers.numfavoriteaddresses;
	g_arenaservers.currentping       = g_arenaservers.numfavoriteaddresses;
}


/*
=================
ArenaServers_Insert
=================
*/
static void ArenaServers_Insert( char* adrstr, char* info, int pingtime )
{
	servernode_t*	servernodeptr;
	char*			s;
	int				i;


	if ((pingtime >= ArenaServers_MaxPing()) && (g_servertype != UIAS_FAVORITES))
	{
		// slow global or local servers do not get entered
		return;
	}
	//remove
	//trap_Cmd_ExecuteText( EXEC_NOW, va( "echo infostring: %s\n", info )  );

	if (*g_arenaservers.numservers >= g_arenaservers.maxservers) {
		// list full;
		servernodeptr = g_arenaservers.serverlist+(*g_arenaservers.numservers)-1;
	} else {
		// next slot
		servernodeptr = g_arenaservers.serverlist+(*g_arenaservers.numservers);
		(*g_arenaservers.numservers)++;
	}

	Q_strncpyz( servernodeptr->adrstr, adrstr, MAX_ADDRESSLENGTH );

	Q_strncpyz( servernodeptr->hostname, Info_ValueForKey( info, "hostname"), MAX_HOSTNAMELENGTH );
	Q_CleanStr( servernodeptr->hostname );
	Q_strupr( servernodeptr->hostname );

	Q_strncpyz( servernodeptr->mapname, Info_ValueForKey( info, "mapname"), MAX_MAPNAMELENGTH );
	Q_CleanStr( servernodeptr->mapname );
	Q_strupr( servernodeptr->mapname );

	servernodeptr->numclients = atoi( Info_ValueForKey( info, "clients") );
	servernodeptr->maxclients = atoi( Info_ValueForKey( info, "sv_maxclients") );
	servernodeptr->pingtime   = pingtime;
	servernodeptr->minPing    = atoi( Info_ValueForKey( info, "minPing") );
	servernodeptr->maxPing    = atoi( Info_ValueForKey( info, "maxPing") );


	/*
	s = Info_ValueForKey( info, "nettype" );
	for (i=0; ;i++)
	{
		if (!netnames[i])
		{
			servernodeptr->nettype = 0;
			break;
		}
		else if (!Q_stricmp( netnames[i], s ))
		{
			servernodeptr->nettype = i;
			break;
		}
	}
	*/
	servernodeptr->nettype = atoi(Info_ValueForKey(info, "nettype"));
	if (servernodeptr->nettype < 0 || servernodeptr->nettype >= ARRAY_LEN(netnames) - 1) {
		servernodeptr->nettype = 0;
	}

	s = Info_ValueForKey( info, "game");
	i = atoi( Info_ValueForKey( info, "gametype") );
	if( i < 0 ) {
		i = 0;
	}
	else if( i > 11 ) {
		i = 12;
	}
	if( *s ) {
		servernodeptr->gametype = i;//-1;
		Q_strncpyz( servernodeptr->gamename, s, sizeof(servernodeptr->gamename) );
	}
	else {
		servernodeptr->gametype = i;
		Q_strncpyz( servernodeptr->gamename, gamenames[i], sizeof(servernodeptr->gamename) );
	}
}


/*
=================
ArenaServers_InsertFavorites

Insert nonresponsive address book entries into display lists.
=================
*/
void ArenaServers_InsertFavorites( void )
{
	int		i;
	int		j;
	char	info[MAX_INFO_STRING];

	// resync existing results with new or deleted cvars
	info[0] = '\0';
	Info_SetValueForKey( info, "hostname", "No Response" );
	for (i=0; i<g_arenaservers.numfavoriteaddresses; i++)
	{
		// find favorite address in refresh list
		for (j=0; j<g_numfavoriteservers; j++)
			if (!Q_stricmp(g_arenaservers.favoriteaddresses[i],g_favoriteserverlist[j].adrstr))
				break;

		if ( j >= g_numfavoriteservers)
		{
			// not in list, add it
			ArenaServers_Insert( g_arenaservers.favoriteaddresses[i], info, ArenaServers_MaxPing() );
		}
	}
}


/*
=================
ArenaServers_LoadFavorites

Load cvar address book entries into local lists.
=================
*/
void ArenaServers_LoadFavorites( void )
{
	int				i;
	int				j;
	int				numtempitems;
	char			emptyinfo[MAX_INFO_STRING];
	char			adrstr[MAX_ADDRESSLENGTH];
	servernode_t	templist[MAX_FAVORITESERVERS];
	qboolean		found;

	found        = qfalse;
	emptyinfo[0] = '\0';

	// copy the old
	memcpy( templist, g_favoriteserverlist, sizeof(servernode_t)*MAX_FAVORITESERVERS );
	numtempitems = g_numfavoriteservers;

	// clear the current for sync
	memset( g_favoriteserverlist, 0, sizeof(servernode_t)*MAX_FAVORITESERVERS );
	g_numfavoriteservers = 0;

	// resync existing results with new or deleted cvars
	for (i=0; i<MAX_FAVORITESERVERS; i++)
	{
		trap_Cvar_VariableStringBuffer( va("server%d",i+1), adrstr, MAX_ADDRESSLENGTH );
		if (!adrstr[0])
			continue;

		// quick sanity check to avoid slow domain name resolving
		// first character must be numeric
		if (adrstr[0] < '0' || adrstr[0] > '9')
			continue;

		// favorite server addresses must be maintained outside refresh list
		// this mimics local and global netadr's stored in client
		// these can be fetched to fill ping list
		strcpy( g_arenaservers.favoriteaddresses[g_numfavoriteservers], adrstr );

		// find this server in the old list
		for (j=0; j<numtempitems; j++)
			if (!Q_stricmp( templist[j].adrstr, adrstr ))
				break;

		if (j < numtempitems)
		{
			// found server - add exisiting results
			memcpy( &g_favoriteserverlist[g_numfavoriteservers], &templist[j], sizeof(servernode_t) );
			found = qtrue;
		}
		else
		{
			// add new server
			Q_strncpyz( g_favoriteserverlist[g_numfavoriteservers].adrstr, adrstr, MAX_ADDRESSLENGTH );
			g_favoriteserverlist[g_numfavoriteservers].pingtime = ArenaServers_MaxPing();
		}

		g_numfavoriteservers++;
	}

	g_arenaservers.numfavoriteaddresses = g_numfavoriteservers;

	if (!found)
	{
		// no results were found, reset server list
		// list will be automatically refreshed when selected
		g_numfavoriteservers = 0;
	}
}


/*
=================
ArenaServers_StopRefresh
=================
*/
static void ArenaServers_StopRefresh( void )
{
	if (!g_arenaservers.refreshservers)
		// not currently refreshing
		return;

	g_arenaservers.refreshservers = qfalse;

	if (g_servertype == UIAS_FAVORITES)
	{
		// nonresponsive favorites must be shown
		ArenaServers_InsertFavorites();
	}

	// final tally
	if (g_arenaservers.numqueriedservers >= 0)
	{
		g_arenaservers.currentping       = *g_arenaservers.numservers;
		g_arenaservers.numqueriedservers = *g_arenaservers.numservers;
	}

	// sort
	qsort( g_arenaservers.serverlist, *g_arenaservers.numservers, sizeof( servernode_t ), ArenaServers_Compare);

	ArenaServers_UpdateMenu();
}


/*
=================
ArenaServers_DoRefresh
=================
*/
static void ArenaServers_DoRefresh( void )
{
	int		i;
	int		j;
	int		time;
	int		maxPing;
	char	adrstr[MAX_ADDRESSLENGTH];
	char	info[MAX_INFO_STRING];

	if (uis.realtime < g_arenaservers.refreshtime)
	{
	  if (g_servertype != UIAS_FAVORITES) {
			if (g_servertype == UIAS_LOCAL) {
				if (!trap_LAN_GetServerCount(g_servertype)) {
					return;
				}
			}
			if (trap_LAN_GetServerCount(g_servertype) < 0) {
			  // still waiting for response
			  return;
			}
	  }
	}

	if (uis.realtime < g_arenaservers.nextpingtime)
	{
		// wait for time trigger
		return;
	}

	// trigger at 10Hz intervals
	g_arenaservers.nextpingtime = uis.realtime + 10;

	// process ping results
	maxPing = ArenaServers_MaxPing();
	for (i=0; i<MAX_PINGREQUESTS; i++)
	{
		trap_LAN_GetPing( i, adrstr, MAX_ADDRESSLENGTH, &time );
		if (!adrstr[0])
		{
			// ignore empty or pending pings
			continue;
		}

		// find ping result in our local list
		for (j=0; j<MAX_PINGREQUESTS; j++)
			if (!Q_stricmp( adrstr, g_arenaservers.pinglist[j].adrstr ))
				break;

		if (j < MAX_PINGREQUESTS)
		{
			// found it
			if (!time)
			{
				time = uis.realtime - g_arenaservers.pinglist[j].start;
				if (time < maxPing)
				{
					// still waiting
					continue;
				}
			}

			if (time > maxPing)
			{
				// stale it out
				info[0] = '\0';
				time    = maxPing;
			}
			else
			{
				trap_LAN_GetPingInfo( i, info, MAX_INFO_STRING );
			}

			// insert ping results
			ArenaServers_Insert( adrstr, info, time );

			// clear this query from internal list
			g_arenaservers.pinglist[j].adrstr[0] = '\0';
   		}

		// clear this query from external list
		trap_LAN_ClearPing( i );
	}

	// get results of servers query
	// counts can increase as servers respond
	if (g_servertype == UIAS_FAVORITES) {
	  g_arenaservers.numqueriedservers = g_arenaservers.numfavoriteaddresses;
	} else {
	  g_arenaservers.numqueriedservers = trap_LAN_GetServerCount(g_servertype);
	}

//	if (g_arenaservers.numqueriedservers > g_arenaservers.maxservers)
//		g_arenaservers.numqueriedservers = g_arenaservers.maxservers;

	// send ping requests in reasonable bursts
	// iterate ping through all found servers
	for (i=0; i<MAX_PINGREQUESTS && g_arenaservers.currentping < g_arenaservers.numqueriedservers; i++)
	{
		if (trap_LAN_GetPingQueueCount() >= MAX_PINGREQUESTS)
		{
			// ping queue is full
			break;
		}

		// find empty slot
		for (j=0; j<MAX_PINGREQUESTS; j++)
			if (!g_arenaservers.pinglist[j].adrstr[0])
				break;

		if (j >= MAX_PINGREQUESTS)
			// no empty slots available yet - wait for timeout
			break;

		// get an address to ping

		if (g_servertype == UIAS_FAVORITES) {
		  strcpy( adrstr, g_arenaservers.favoriteaddresses[g_arenaservers.currentping] );
		} else {
		  trap_LAN_GetServerAddressString(g_servertype, g_arenaservers.currentping, adrstr, MAX_ADDRESSLENGTH );
		}

		strcpy( g_arenaservers.pinglist[j].adrstr, adrstr );
		g_arenaservers.pinglist[j].start = uis.realtime;

		trap_Cmd_ExecuteText( EXEC_NOW, va( "ping %s\n", adrstr )  );
/*
		if	(Q_strncmp( "zeq2", g_arenaservers.serverlist->gamename, 4) ) {
			break;
		}
*/
		// advance to next server
		g_arenaservers.currentping++;
	}

	if (!trap_LAN_GetPingQueueCount())
	{
		// all pings completed
		ArenaServers_StopRefresh();
		return;
	}

	// update the user interface with ping status
	ArenaServers_UpdateMenu();
}


/*
=================
ArenaServers_StartRefresh
=================
*/
static void ArenaServers_StartRefresh( void )
{
	int		i;
	char	myargs[32], protocol[32];

	memset( g_arenaservers.serverlist, 0, g_arenaservers.maxservers*sizeof(table_t) );

	for (i=0; i<MAX_PINGREQUESTS; i++)
	{
		g_arenaservers.pinglist[i].adrstr[0] = '\0';
		trap_LAN_ClearPing( i );
	}

	g_arenaservers.refreshservers    = qtrue;
	g_arenaservers.currentping       = 0;
	g_arenaservers.nextpingtime      = 0;
	*g_arenaservers.numservers       = 0;
	g_arenaservers.numqueriedservers = 0;

	// allow max 5 seconds for responses
	g_arenaservers.refreshtime = uis.realtime + 5000;

	// place menu in zeroed state
	ArenaServers_UpdateMenu();

	if( g_servertype == UIAS_LOCAL ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "localservers\n" );
		return;
	}

	if( g_servertype >= UIAS_GLOBAL1 && g_servertype <= UIAS_GLOBAL5 ) {
		switch( g_arenaservers.gametype.curvalue ) {
		default:
		case GAMES_ALL:
			myargs[0] = 0;
			break;

		case GAMES_STRUGGLE:
			strcpy( myargs, " struggle" );
			break;
		}

		if (g_emptyservers) {
			strcat(myargs, " empty");
		}

		if (g_fullservers) {
			strcat(myargs, " full");
		}

		protocol[0] = '\0';
		trap_Cvar_VariableStringBuffer( "debug_protocol", protocol, sizeof(protocol) );
		if (strlen(protocol)) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "globalservers %d %s%s\n", g_servertype - 1, protocol, myargs ));
		}
		else {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "globalservers %d %d%s\n", g_servertype - 1, (int)trap_Cvar_VariableValue( "protocol" ), myargs ) );
		}
	}
}


/*
=================
ArenaServers_SaveChanges
=================
*/
void ArenaServers_SaveChanges( void )
{
	int	i;

	for (i=0; i<g_arenaservers.numfavoriteaddresses; i++)
		trap_Cvar_Set( va("server%d",i+1), g_arenaservers.favoriteaddresses[i] );

	for (; i<MAX_FAVORITESERVERS; i++)
		trap_Cvar_Set( va("server%d",i+1), "" );
}


/*
=================
ArenaServers_Sort
=================
*/
void ArenaServers_Sort( int type ) {
	if( g_sortkey == type ) {
		return;
	}

	g_sortkey = type;
	qsort( g_arenaservers.serverlist, *g_arenaservers.numservers, sizeof( servernode_t ), ArenaServers_Compare);
}


/*
=================
ArenaServers_SetType
=================
*/
int ArenaServers_SetType( int type )
{
	if(type >= UIAS_GLOBAL1 && type <= UIAS_GLOBAL5)
	{
		char masterstr[2], cvarname[sizeof("sv_master1")];

		while(type <= UIAS_GLOBAL5)
		{
			Com_sprintf(cvarname, sizeof(cvarname), "sv_master%d", type);
			trap_Cvar_VariableStringBuffer(cvarname, masterstr, sizeof(masterstr));
			if(*masterstr)
				break;

			type++;
		}
	}

	g_servertype = type;

	switch( type ) {
	default:
	case UIAS_LOCAL:
		g_arenaservers.remove.generic.flags |= (QMF_INACTIVE|QMF_HIDDEN);
		g_arenaservers.serverlist = g_localserverlist;
		g_arenaservers.numservers = &g_numlocalservers;
		g_arenaservers.maxservers = MAX_LOCALSERVERS;
		break;

	case UIAS_GLOBAL1:
	case UIAS_GLOBAL2:
	case UIAS_GLOBAL3:
	case UIAS_GLOBAL4:
	case UIAS_GLOBAL5:
		g_arenaservers.remove.generic.flags |= (QMF_INACTIVE|QMF_HIDDEN);
		g_arenaservers.serverlist = g_globalserverlist;
		g_arenaservers.numservers = &g_numglobalservers;
		g_arenaservers.maxservers = MAX_GLOBALSERVERS;
		break;

	case UIAS_FAVORITES:
		g_arenaservers.remove.generic.flags &= ~(QMF_INACTIVE|QMF_HIDDEN);
		g_arenaservers.serverlist = g_favoriteserverlist;
		g_arenaservers.numservers = &g_numfavoriteservers;
		g_arenaservers.maxservers = MAX_FAVORITESERVERS;
		break;

	}

	if( !*g_arenaservers.numservers ) {
		ArenaServers_StartRefresh();
	}
	else {
		// avoid slow operation, use existing results
		g_arenaservers.currentping       = *g_arenaservers.numservers;
		g_arenaservers.numqueriedservers = *g_arenaservers.numservers;
		ArenaServers_UpdateMenu();
	}
	strcpy(g_arenaservers.status.string,"Press R or REFRESH to update the list.");

	return type;
}


/*
=================
ArenaServers_Event
=================
*/
static void ArenaServers_Event( void* ptr, int event ) {
	int		id;

	id = ((menucommon_s*)ptr)->id;

	if( event != QM_ACTIVATED && id != ID_LIST ) {
		return;
	}

	switch( id ) {
	case ID_MASTER:
		g_arenaservers.master.curvalue = ArenaServers_SetType(g_arenaservers.master.curvalue);
		trap_Cvar_SetValue( "ui_browserMaster", g_arenaservers.master.curvalue);
		break;

	case ID_GAMETYPE:
		trap_Cvar_SetValue( "ui_browserGameType", g_arenaservers.gametype.curvalue );
		g_gametype = g_arenaservers.gametype.curvalue;
		ArenaServers_UpdateMenu();
		break;

	case ID_SORTKEY:
		trap_Cvar_SetValue( "ui_browserSortKey", g_arenaservers.sortkey.curvalue );
		ArenaServers_Sort( g_arenaservers.sortkey.curvalue );
		ArenaServers_UpdateMenu();
		break;

	case ID_SHOW_FULL:
		trap_Cvar_SetValue( "ui_browserShowFull", g_arenaservers.showfull.curvalue );
		g_fullservers = g_arenaservers.showfull.curvalue;
		ArenaServers_UpdateMenu();
		break;

	case ID_SHOW_EMPTY:
		trap_Cvar_SetValue( "ui_browserShowEmpty", g_arenaservers.showempty.curvalue );
		g_emptyservers = g_arenaservers.showempty.curvalue;
		ArenaServers_UpdateMenu();
		break;

	case ID_LIST:
		if( event == QM_GOTFOCUS ) {
			ArenaServers_UpdatePicture();
		}
		break;

	case ID_SCROLL_UP:
		ScrollList_Key( &g_arenaservers.list, K_UPARROW );
		break;

	case ID_SCROLL_DOWN:
		ScrollList_Key( &g_arenaservers.list, K_DOWNARROW );
		break;


	case ID_CONNECT:
		ArenaServers_Go();
		break;

	case ID_CREATE:
		UI_StartServerMenu( qtrue );
		break;

	case ID_SPECIFY:
		UI_SpecifyServerMenu();
		break;

	case ID_REFRESH:
		ArenaServers_StartRefresh();
		break;


	case ID_BACK:
		ArenaServers_StopRefresh();
		ArenaServers_SaveChanges();
		UI_PopMenu();
		break;

	case ID_REMOVE:
		ArenaServers_Remove();
		ArenaServers_UpdateMenu();
		break;
	}
}


/*
=================
Play screen of the menu redesign

The data side (master queries, pings, sorting, favorites) is untouched;
these functions only lay the screen out on the design canvas.
=================
*/

#define PLAY_LIST_X			72
#define PLAY_LIST_Y			246
#define PLAY_LIST_W			762
#define PLAY_LIST_ROWS		12

// phone: the list on the 844x390 canvas, rows of 58 dp
#define PLAYM_LIST_X		34
#define PLAYM_LIST_Y		126
#define PLAYM_LIST_W		532
#define PLAYM_ROW_H			58
#define PLAYM_ROWS			4

static const int playTabTypes[3] = { UIAS_GLOBAL1, UIAS_LOCAL, UIAS_FAVORITES };
static const char *playTabLabels[4] = { "INTERNET", "LAN", "FAVORITES", "ADDRESS" };
static qboolean playAddress;	// phone: the ADDRESS tab is open
static void ArenaServers_PhoneItems( void );
static const char *playSortLabels[5] = { "Server name", "Map name", "Open spots", "Game type", "Ping time" };

static qboolean ArenaServers_TabActive( int tab ) {
	if ( tab == 3 || playAddress ) {
		return tab == 3 && playAddress;
	}
	if ( playTabTypes[tab] == UIAS_GLOBAL1 ) {
		return ( g_servertype >= UIAS_GLOBAL1 && g_servertype <= UIAS_GLOBAL5 );
	}
	return g_servertype == playTabTypes[tab];
}

static void ArenaServers_DrawTab( void *self ) {
	menubitmap_s	*b;
	int				tab;

	b = (menubitmap_s *)self;
	tab = b->generic.id - ID_TAB0;
	UI_DTab( b->generic.x, b->generic.y, b->width, b->height, playTabLabels[tab], ArenaServers_TabActive( tab ),
		UI_DFocus( &g_arenaservers.menu, self ) );
}

static void ArenaServers_DrawSort( void *self ) {
	menubitmap_s	*b;
	float			focus;

	b = (menubitmap_s *)self;
	focus = UI_DFocus( &g_arenaservers.menu, self );
	if ( g_arenaservers.menu.mobile ) {
		UI_DField( b->generic.x, b->generic.y, b->width, b->height, focus );
		UI_DText( b->generic.x + 14, b->generic.y + b->height * 0.5f - 8, "Sort:", 15, UI_LEFT, zeq_text3 );
		UI_DText( b->generic.x + 60, b->generic.y + b->height * 0.5f - 8, playSortLabels[g_sortkey], 15, UI_LEFT, zeq_white );
		UI_DIcon( b->generic.x + b->width - 24, b->generic.y + b->height * 0.5f - 5, 10, UI_ICON_CHEVDOWN, zeq_text2 );
		return;
	}
	UI_DText( b->generic.x - 12, b->generic.y + 5, "SORT BY", 16, UI_RIGHT, zeq_text3 );
	// a dropdown box as the artboard; arrows and clicks cycle the order
	UI_DField( b->generic.x, b->generic.y, b->width, 34, focus );
	UI_DText( b->generic.x + 12, b->generic.y + 7, playSortLabels[g_sortkey], 18, UI_LEFT, zeq_white );
	UI_DIcon( b->generic.x + b->width - 22, b->generic.y + 12, 10, UI_ICON_CHEVDOWN, zeq_text2 );
}

static void ArenaServers_DrawButton( void *self ) {
	menubitmap_s	*b;
	const char		*label;
	qboolean		disabled;

	b = (menubitmap_s *)self;
	disabled = ( b->generic.flags & QMF_GRAYED ) != 0;
	switch ( b->generic.id ) {
	case ID_REFRESH:	label = g_arenaservers.refreshservers ? "STOP" : "REFRESH"; break;
	case ID_CONNECT:	label = "JOIN"; break;
	default:			label = g_arenaservers.menu.mobile ? "CONNECT" : "FIGHT!"; break;
	}
	UI_DKiButton( b->generic.x, b->generic.y, b->width, b->height, label, NULL, UI_DFocus( &g_arenaservers.menu, self ), disabled, qfalse );
}

static void ArenaServers_DrawCreate( void *self ) {
	menubitmap_s *b;

	b = (menubitmap_s *)self;
	UI_DCta( b->generic.x, b->generic.y, b->width, b->height, g_arenaservers.menu.mobile ? "CREATE MATCH" : "CREATE", UI_DFocus( &g_arenaservers.menu, self ) );
}

// the typed address of the connect card
static void ArenaServers_DrawAddress( void *self ) {
	menufield_s	*f;
	qboolean	focus;

	f = (menufield_s *)self;
	focus = ( Menu_ItemAtCursor( &g_arenaservers.menu ) == self );
	if ( g_arenaservers.menu.mobile ) {
		UI_DField( 50, 150, 500, 52, focus ? 1.0f : 0.0f );
		UI_DText( 66, 166, f->field.buffer[0] ? f->field.buffer : "ip:port", 19, UI_LEFT, f->field.buffer[0] ? zeq_white : zeq_text3 );
		if ( focus && f->field.buffer[0] && ( ( uis.realtime / 400 ) & 1 ) ) {
			UI_DFill( 66 + UI_DTextWidth( f->field.buffer, 19 ) + 2, 165, 2, 22, zeq_white );
		}
		return;
	}
	UI_DField( 904, 512, 304, 42, focus ? 1.0f : 0.0f );
	if ( f->field.buffer[0] ) {
		UI_DText( 918, 523, f->field.buffer, 20, UI_LEFT, zeq_white );
	}
	else if ( !focus ) {
		UI_DText( 918, 523, "ip:port", 20, UI_LEFT, zeq_text3 );
	}
	if ( focus && ( ( uis.realtime / 400 ) & 1 ) ) {
		UI_DFill( 918 + UI_DTextWidth( f->field.buffer, 20 ) + 2, 522, 2, 22, zeq_white );
	}
}

static int ArenaServers_GametypeIndex( int gametype ) {
	return gametype;
}

static void ArenaServers_PingColor( int ping, vec4_t c ) {
	if ( ping < 200 ) {
		c[0] = 0.44f; c[1] = 0.86f; c[2] = 0.55f;
	}
	else if ( ping < 400 ) {
		c[0] = 0.95f; c[1] = 0.70f; c[2] = 0.24f;
	}
	else {
		c[0] = 0.94f; c[1] = 0.50f; c[2] = 0.43f;
	}
	c[3] = 1.0f;
}

// phone: 58 dp rows, the map and mode on a second line; a drag scrolls
static void ArenaServers_DrawListPhone( menulist_s *l, qboolean focus ) {
	static const char *gametypeNames[] = { "Free for all", "Tournament", "Single", "Struggle", "Team", "CTF" };
	servernode_t	*node;
	char			host[64];
	vec4_t			line, ping;
	float			y, h;
	int				i, row;

	line[0] = 0.35f; line[1] = 0.51f; line[2] = 0.90f; line[3] = 0.4f;
	UI_DText( 50, 100, "SERVER", 12, UI_LEFT, zeq_text3 );
	UI_DText( 430, 100, "PLAYERS", 12, UI_LEFT, zeq_text3 );
	UI_DText( 552, 100, "PING", 12, UI_RIGHT, zeq_text3 );
	UI_DFill( 42, 120, 516, 1, line );

	if ( !l->numitems ) {
		trap_R_SetColor( NULL );
		UI_DPic( 278, 160 + 2.0f * sin( uis.realtime / 300.0f ), 40, 48, uis.dragonBall );
		UI_DProp( 298, 222, g_arenaservers.refreshservers ? "SCANNING" : "NO SERVERS FOUND", 15, UI_CENTER, zeq_white );
		UI_DText( 298, 246, g_arenaservers.refreshservers ? "Asking the master server for games..." :
			"Refresh the list, or host your own match with bots.", 14, UI_CENTER, zeq_text2 );
		UI_DText( 298, 268, g_arenaservers.status.string, 12, UI_CENTER, zeq_text3 );
		return;
	}

	for ( i = 0; i < PLAYM_ROWS; i++ ) {
		row = l->top + i;
		if ( row >= l->numitems ) {
			break;
		}
		node = g_arenaservers.table[row].servernode;
		if ( !node ) {
			continue;
		}
		y = PLAYM_LIST_Y + i * PLAYM_ROW_H;
		h = PLAYM_ROW_H - 2;
		if ( row == l->curvalue ) {
			UI_DFocusRow( PLAYM_LIST_X, y, PLAYM_LIST_W, h, 0.8f );
		}
		trap_R_SetColor( NULL );
		UI_DPic( PLAYM_LIST_X + 6, y + 7, 60, h - 14, trap_R_RegisterShaderNoMip( va( "maps/%s.jpg", node->mapname ) ) );
		Q_strncpyz( host, node->hostname, sizeof( host ) );
		UI_DText( PLAYM_LIST_X + 76, y + 9, host, 16, UI_LEFT, row == l->curvalue ? zeq_white : zeq_text2 );
		UI_DText( PLAYM_LIST_X + 76, y + 31, va( "%s  -  %s", node->mapname, ( node->gametype >= 0 && node->gametype < 6 ) ? gametypeNames[node->gametype] : "?" ),
			13, UI_LEFT, zeq_text3 );
		UI_DText( 430, y + h * 0.5f - 8, va( "%i / %i", node->numclients, node->maxclients ), 15, UI_LEFT, zeq_text2 );
		ArenaServers_PingColor( node->pingtime, ping );
		UI_DText( 552, y + h * 0.5f - 8, va( "%i", node->pingtime ), 15, UI_RIGHT, ping );
	}
	// scroll bar
	if ( l->numitems > PLAYM_ROWS ) {
		line[3] = 0.2f;
		UI_DFill( 563, PLAYM_LIST_Y, 3, PLAYM_ROWS * PLAYM_ROW_H, line );
		line[3] = 0.8f;
		h = PLAYM_ROWS * PLAYM_ROW_H * PLAYM_ROWS / (float)l->numitems;
		UI_DFill( 563, PLAYM_LIST_Y + ( PLAYM_ROWS * PLAYM_ROW_H - h ) * l->top / (float)( l->numitems - PLAYM_ROWS ), 3, h, line );
	}
}

// one row per server; the scroll list keeps the selection and the keys
static void ArenaServers_DrawList( void *self ) {
	menulist_s		*l;
	int				i;
	int				row;
	float			rowH;
	float			y;
	servernode_t	*node;
	qboolean		focus;
	qboolean		selected;
	vec4_t			pingColor;
	vec4_t			line;
	char			host[64];
	static const char *gametypeNames[] = { "Free for all", "Tournament", "Single", "Struggle", "Team", "CTF" };

	l = (menulist_s *)self;
	focus = ( Menu_ItemAtCursor( &g_arenaservers.menu ) == self );
	rowH = SMALLCHAR_HEIGHT / UI_DScale();
	if ( g_arenaservers.menu.mobile ) {
		ArenaServers_DrawListPhone( l, focus );
		return;
	}

	line[0] = 0.35f; line[1] = 0.51f; line[2] = 0.90f; line[3] = 0.4f;
	UI_DText( 84, 214, "SERVER", 16, UI_LEFT, zeq_text3 );
	UI_DText( 470, 214, "MAP", 16, UI_LEFT, zeq_text3 );
	UI_DText( 612, 214, "MODE", 16, UI_LEFT, zeq_text3 );
	UI_DText( 720, 214, "PLAYERS", 16, UI_LEFT, zeq_text3 );
	UI_DText( 822, 214, "PING", 16, UI_RIGHT, zeq_text3 );
	UI_DFill( 72, 238, 762, 1.5f, line );

	if ( !l->numitems ) {
		trap_R_SetColor( NULL );
		UI_DPic( 426, 330 + 2.0f * sin( uis.realtime / 300.0f ), 54, 65, uis.dragonBall );
		UI_DProp( 453, 412, g_arenaservers.refreshservers ? "SCANNING" : "NO SERVERS FOUND", 20, UI_CENTER, zeq_white );
		UI_DText( 453, 444, g_arenaservers.refreshservers ? "Asking the master server for games..." :
			"Refresh the list, or host your own match with bots.", 19, UI_CENTER, zeq_text2 );
		return;
	}

	for ( i = 0; i < l->height; i++ ) {
		row = l->top + i;
		if ( row >= l->numitems ) {
			break;
		}
		node = g_arenaservers.table[row].servernode;
		if ( !node ) {
			continue;
		}
		y = PLAY_LIST_Y + i * rowH;
		selected = ( row == l->curvalue );
		if ( selected ) {
			UI_DFocusRow( 72, y, 762, rowH - 2, focus ? UI_AnimFrac( uis.focusTime, 0, 160 ) : 0.55f );
		}
		trap_R_SetColor( NULL );
		UI_DPic( 78, y + 2, 36, rowH - 6, trap_R_RegisterShaderNoMip( va( "maps/%s.jpg", node->mapname ) ) );
		Q_strncpyz( host, node->hostname, sizeof( host ) );
		UI_DText( 124, y + 3, host, 18, UI_LEFT, selected ? zeq_white : zeq_text2 );
		UI_DText( 470, y + 3, node->mapname, 18, UI_LEFT, zeq_text2 );
		UI_DText( 612, y + 3, ( node->gametype >= 0 && node->gametype < 6 ) ? gametypeNames[ArenaServers_GametypeIndex( node->gametype )] : "?", 18, UI_LEFT, zeq_text2 );
		UI_DText( 720, y + 3, va( "%i / %i", node->numclients, node->maxclients ), 18, UI_LEFT, zeq_text2 );
		if ( node->pingtime < 200 ) {
			pingColor[0] = 0.44f; pingColor[1] = 0.86f; pingColor[2] = 0.55f;
		}
		else if ( node->pingtime < 400 ) {
			pingColor[0] = 0.95f; pingColor[1] = 0.70f; pingColor[2] = 0.24f;
		}
		else {
			pingColor[0] = 0.94f; pingColor[1] = 0.50f; pingColor[2] = 0.43f;
		}
		pingColor[3] = 1.0f;
		UI_DText( 822, y + 3, va( "%i", node->pingtime ), 18, UI_RIGHT, pingColor );
	}
}

static void ArenaServers_MenuDrawPhone( void ) {
	const char	*arena;
	vec4_t		fade;
	int			i;

	ArenaServers_PhoneItems();
	UI_DMobileHeader( "MAIN MENU  >", "PLAY" );
	UI_DGlass( 26, 88, 548, 276 );
	if ( playAddress ) {
		UI_DProp( 50, 112, "CONNECT BY ADDRESS", 11, UI_LEFT, zeq_gold );
		UI_DText( 50, 214, "Server address and port; the default port if omitted.", 14, UI_LEFT, zeq_text3 );
	}

	// host a match card
	arena = UI_GetArenaInfoByNumber( 0 );
	if ( arena ) {
		trap_R_SetColor( NULL );
		UI_DPic( 586, 88, 232, 132, trap_R_RegisterShaderNoMip( va( "maps/%s.jpg", Info_ValueForKey( arena, "map" ) ) ) );
		for ( i = 0; i < 10; i++ ) {
			fade[0] = 0.02f; fade[1] = 0.024f; fade[2] = 0.10f; fade[3] = 0.6f - 0.06f * i;
			UI_DFill( 586, 88 + i * 5, 232, 5, fade );
		}
		UI_DRoundCorners( 586, 88, 232, 132, 8, zeq_portraitBack );
		fade[0] = 0.47f; fade[1] = 0.63f; fade[2] = 1.0f; fade[3] = 0.45f;
		UI_DRoundEdge( 586, 88, 232, 132, 8, fade );
	}
	UI_DProp( 598, 98, "HOST A MATCH", 10, UI_LEFT, zeq_gold );
	UI_DText( 598, 114, "Local match with bots", 13, UI_LEFT, zeq_white );

	Menu_Draw( &g_arenaservers.menu );
}

static void ArenaServers_MenuDraw( void )
{
	static const char *hints[] = { "TAB", "Source", "R", "Refresh", "ENTER", "Join", "ESC", "Back" };
	const char	*arena;
	char		mapTitle[64];
	vec4_t		line;
	vec4_t		fade;
	int			i;

	if (g_arenaservers.refreshservers)
		ArenaServers_DoRefresh();

	uis.menuamount = -1;
	uis.hideEarth = qtrue;

	if ( g_arenaservers.menu.mobile ) {
		ArenaServers_MenuDrawPhone();
		return;
	}

	UI_DHeader( "MAIN MENU  >", "PLAY" );

	// server browser panel
	UI_DGlass( 48, 128, 810, 528 );
	line[0] = 0.35f; line[1] = 0.51f; line[2] = 0.90f; line[3] = 0.25f;
	UI_DFill( 72, 590, 762, 1.5f, line );
	UI_DText( 72, 606, g_arenaservers.status.string, 17, UI_LEFT, zeq_text3 );

	// host a match card
	UI_DGlass( 880, 128, 352, 318 );
	UI_DSectionTitle( 904, 152, "HOST A MATCH" );
	arena = UI_GetArenaInfoByNumber( 0 );
	if ( arena ) {
		trap_R_SetColor( NULL );
		UI_DPic( 904, 178, 304, 158, trap_R_RegisterShaderNoMip( va( "maps/%s.jpg", Info_ValueForKey( arena, "map" ) ) ) );
		Q_strncpyz( mapTitle, Info_ValueForKey( arena, "map" ), sizeof( mapTitle ) );
		Q_strupr( mapTitle );
		// fade to navy under the title, 6 px corners and a soft blue border
		for ( i = 0; i < 12; i++ ) {
			fade[0] = 0.02f; fade[1] = 0.024f; fade[2] = 0.10f; fade[3] = 0.075f * i;
			UI_DFill( 904, 278 + i * 5, 304, 5, fade );
		}
		UI_DRoundCorners( 904, 178, 304, 158, 6, zeq_portraitBack );
		fade[0] = 0.47f; fade[1] = 0.63f; fade[2] = 1.0f; fade[3] = 0.45f;
		UI_DRoundEdge( 904, 178, 304, 158, 6, fade );
		UI_DProp( 916, 302, mapTitle, 15, UI_LEFT, zeq_white );
		UI_DText( 916, 318, "Local match with bots", 15, UI_LEFT, zeq_text2 );
	}

	// connect by address card
	UI_DGlass( 880, 462, 352, 194 );
	UI_DSectionTitle( 904, 486, "CONNECT BY ADDRESS" );
	UI_DText( 904, 572, "Default port if omitted", 16, UI_LEFT, zeq_text3 );

	Menu_Draw( &g_arenaservers.menu );
	UI_DHints( hints, 4 );
}

static void ArenaServers_ConnectTyped( void ) {
	char addr[MAX_EDIT_LINE];

	Q_strncpyz( addr, g_arenaservers.address.field.buffer, sizeof( addr ) );
	if ( !addr[0] ) {
		return;
	}
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "connect %s\n", addr ) );
}

static void ArenaServers_CycleSort( int dir ) {
	g_sortkey = ( g_sortkey + dir + 5 ) % 5;
	g_arenaservers.sortkey.curvalue = g_sortkey;
	trap_Cvar_SetValue( "ui_browserSortKey", g_sortkey );
	ArenaServers_Sort( g_sortkey );
	ArenaServers_UpdateMenu();
}

static void ArenaServers_PickTab( int tab ) {
	g_arenaservers.master.curvalue = ArenaServers_SetType( playTabTypes[tab] );
	trap_Cvar_SetValue( "ui_browserMaster", g_arenaservers.master.curvalue );
}

static void ArenaServers_NewEvent( void *ptr, int event ) {
	int id;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	id = ((menucommon_s *)ptr)->id;
	if ( id == ID_TAB0 + 3 ) {
		playAddress = qtrue;
		ArenaServers_PhoneItems();
		return;
	}
	if ( id >= ID_TAB0 && id < ID_TAB0 + 3 ) {
		playAddress = qfalse;
		ArenaServers_PhoneItems();
		ArenaServers_PickTab( id - ID_TAB0 );
		return;
	}
	switch ( id ) {
	case ID_SORTSEL:
		ArenaServers_CycleSort( 1 );
		break;
	case ID_REFRESH:
		if ( g_arenaservers.refreshservers ) {
			ArenaServers_StopRefresh();
		}
		else {
			ArenaServers_StartRefresh();
		}
		break;
	case ID_CONNECT:
		ArenaServers_Go();
		break;
	case ID_CREATE:
		UI_StartServerMenu( qtrue );
		break;
	case ID_CONNECTTYPED:
		ArenaServers_ConnectTyped();
		break;
	}
}

static sfxHandle_t ArenaServers_MenuKey( int key ) {
	void	*item;
	int		tab;

	item = Menu_ItemAtCursor( &g_arenaservers.menu );

	// phone: a tap picks a row, a tap on the picked row joins it
	if ( g_arenaservers.menu.mobile && key == K_MOUSE1 && item == &g_arenaservers.list &&
		( g_arenaservers.list.generic.flags & QMF_HASMOUSEFOCUS ) && g_arenaservers.list.numitems ) {
		float	x, y, w, h;
		int		row;

		UI_DCanvas( qtrue );
		x = PLAYM_LIST_X; y = PLAYM_LIST_Y; w = PLAYM_LIST_W; h = PLAYM_ROW_H;
		UI_DToV( &x, &y, &w, &h );
		row = g_arenaservers.list.top + (int)( ( uis.cursory - y ) / h );
		if ( row < 0 || row >= g_arenaservers.list.numitems ) {
			return menu_null_sound;
		}
		if ( row == g_arenaservers.list.curvalue ) {
			ArenaServers_Go();
			return menu_move_sound;
		}
		g_arenaservers.list.curvalue = row;
		ArenaServers_UpdatePicture();
		return menu_move_sound;
	}

	if( key == K_SPACE  && g_arenaservers.refreshservers ) {
		ArenaServers_StopRefresh();
		return menu_move_sound;
	}

	if( ( key == K_DEL || key == K_KP_DEL ) && ( g_servertype == UIAS_FAVORITES ) &&
		( item == &g_arenaservers.list ) ) {
		ArenaServers_Remove();
		ArenaServers_UpdateMenu();
		return menu_move_sound;
	}

	if( key == K_MOUSE2 || key == K_ESCAPE ) {
		ArenaServers_StopRefresh();
		ArenaServers_SaveChanges();
		return Menu_DefaultKey( &g_arenaservers.menu, key );
	}

	// the address field takes every other key while it is being typed in
	if ( item == &g_arenaservers.address ) {
		if ( key == K_ENTER || key == K_KP_ENTER ) {
			ArenaServers_ConnectTyped();
			return menu_move_sound;
		}
		return Menu_DefaultKey( &g_arenaservers.menu, key );
	}

	if ( key == K_TAB ) {
		for ( tab = 0; tab < 3; tab++ ) {
			if ( ArenaServers_TabActive( tab ) ) {
				break;
			}
		}
		ArenaServers_PickTab( ( tab + 1 ) % 3 );
		return menu_move_sound;
	}
	if ( key == 'r' || key == 'R' ) {
		ArenaServers_StartRefresh();
		return menu_move_sound;
	}
	if ( item == &g_arenaservers.sortsel && ( key == K_LEFTARROW || key == K_RIGHTARROW ) ) {
		ArenaServers_CycleSort( key == K_LEFTARROW ? -1 : 1 );
		return menu_move_sound;
	}
	if ( item == &g_arenaservers.list && ( key == K_ENTER || key == K_KP_ENTER ) ) {
		ArenaServers_Go();
		return menu_move_sound;
	}

	return Menu_DefaultKey( &g_arenaservers.menu, key );
}

static void ArenaServers_DItem( menubitmap_s *b, int id, void (*draw)( void * ), int x, int y, int w, int h ) {
	b->generic.type			= MTYPE_BITMAP;
	b->generic.flags		= QMF_NODEFAULTINIT;
	b->generic.id			= id;
	b->generic.callback		= ArenaServers_NewEvent;
	b->generic.ownerdraw	= draw;
	UI_DItem( b, x, y, w, h );
}


static void ArenaServers_InitState( void );

/*
=================
ArenaServers_PhoneItems

phone: the ADDRESS tab swaps the list for the address field
=================
*/
static void ArenaServers_PhoneItems( void ) {
	if ( !g_arenaservers.menu.mobile ) {
		return;
	}
	if ( playAddress ) {
		g_arenaservers.list.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		g_arenaservers.address.generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		g_arenaservers.connectBtn.generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
	}
	else {
		g_arenaservers.list.generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		g_arenaservers.address.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		g_arenaservers.connectBtn.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
	}
}

/*
=================
ArenaServers_MenuInitPhone

the phone layout on the 844x390 canvas, after the arenaservers_t items
of the old screen are set up
=================
*/
static void ArenaServers_MenuInitPhone( void ) {
	static const int tabX[4] = { 196, 318, 404, 530 };
	static const int tabW[4] = { 118, 82, 122, 108 };
	int i;

	for ( i = 0; i < 4; i++ ) {
		ArenaServers_DItem( &g_arenaservers.tabs[i], ID_TAB0 + i, ArenaServers_DrawTab, tabX[i], 26, tabW[i], 48 );
	}
	ArenaServers_DItem( &g_arenaservers.sortsel, ID_SORTSEL, ArenaServers_DrawSort, 586, 296, 232, 52 );
	ArenaServers_DItem( &g_arenaservers.refreshBtn, ID_REFRESH, ArenaServers_DrawButton, 586, 232, 114, 52 );
	ArenaServers_DItem( &g_arenaservers.joinBtn, ID_CONNECT, ArenaServers_DrawButton, 704, 232, 114, 52 );
	ArenaServers_DItem( &g_arenaservers.createBtn, ID_CREATE, ArenaServers_DrawCreate, 598, 156, 206, 52 );
	ArenaServers_DItem( &g_arenaservers.connectBtn, ID_CONNECTTYPED, ArenaServers_DrawButton, 50, 252, 170, 52 );

	g_arenaservers.address.generic.type			= MTYPE_FIELD;
	g_arenaservers.address.generic.flags		= QMF_NODEFAULTINIT;
	g_arenaservers.address.generic.ownerdraw	= ArenaServers_DrawAddress;
	g_arenaservers.address.field.widthInChars	= 32;
	g_arenaservers.address.field.maxchars		= 64;
	UI_DSetRect( &g_arenaservers.address.generic, 50, 150, 500, 52 );
	g_arenaservers.address.generic.x = g_arenaservers.address.generic.left;
	g_arenaservers.address.generic.y = g_arenaservers.address.generic.top;

	UI_DBackItem( &g_arenaservers.menu );
	for ( i = 0; i < 4; i++ ) {
		Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.tabs[i] );
	}
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.list );
	// the scroll list sized itself in chars: give it the phone rows
	g_arenaservers.list.height = PLAYM_ROWS;
	UI_DSetRect( &g_arenaservers.list.generic, PLAYM_LIST_X, PLAYM_LIST_Y, PLAYM_LIST_W, PLAYM_ROWS * PLAYM_ROW_H );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.createBtn );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.refreshBtn );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.joinBtn );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.sortsel );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.address );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.connectBtn );
	ArenaServers_PhoneItems();

	ArenaServers_LoadFavorites();
	ArenaServers_InitState();
}

/*
=================
ArenaServers_MenuInit
=================
*/
static void ArenaServers_MenuInit( void ) {
	int			i;
	int			y;
	int			value;
	static char	statusbuffer[MAX_STATUSLENGTH];
//	static char leaguebuffer[MAX_LEAGUELENGTH]; // TTimo: unused

	// zero set all our globals
	memset( &g_arenaservers, 0 ,sizeof(arenaservers_t) );

	ArenaServers_Cache();
	uis.menuamount = 0;
	g_arenaservers.menu.fullscreen = qtrue;
	g_arenaservers.menu.wrapAround = qtrue;
	g_arenaservers.menu.draw       = ArenaServers_MenuDraw;
	g_arenaservers.menu.key        = ArenaServers_MenuKey;

	g_arenaservers.banner.generic.type  = MTYPE_BTEXT;
	g_arenaservers.banner.generic.flags = QMF_CENTER_JUSTIFY;
	g_arenaservers.banner.generic.x	    = 320;
	g_arenaservers.banner.generic.y	    = 16;
	g_arenaservers.banner.string  		= "ZEQII LITE SERVERS";
	g_arenaservers.banner.style  	    = UI_CENTER|UI_DROPSHADOW;
	g_arenaservers.banner.color  	    = color_white;

	y = 30;
	g_arenaservers.master.generic.type			= MTYPE_SPINCONTROL;
	g_arenaservers.master.generic.name			= "Servers:";
	g_arenaservers.master.generic.flags			= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	g_arenaservers.master.generic.callback		= ArenaServers_Event;
	g_arenaservers.master.generic.id			= ID_MASTER;
	g_arenaservers.master.generic.x				= 320;
	g_arenaservers.master.generic.y				= y;
	g_arenaservers.master.itemnames				= master_items;

	y += SMALLCHAR_HEIGHT;
	g_arenaservers.sortkey.generic.type			= MTYPE_SPINCONTROL;
	g_arenaservers.sortkey.generic.name			= "Sort By:";
	g_arenaservers.sortkey.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	g_arenaservers.sortkey.generic.callback		= ArenaServers_Event;
	g_arenaservers.sortkey.generic.id			= ID_SORTKEY;
	g_arenaservers.sortkey.generic.x			= 320;
	g_arenaservers.sortkey.generic.y			= y;
	g_arenaservers.sortkey.itemnames			= sortkey_items;


	y += 3 * SMALLCHAR_HEIGHT;
	{
		float vx, vy, vw, vh;

		vx = PLAY_LIST_X; vy = PLAY_LIST_Y; vw = PLAY_LIST_W; vh = 1;
		UI_DToV( &vx, &vy, &vw, &vh );
		g_arenaservers.list.generic.x			= vx;
		g_arenaservers.list.generic.y			= vy;
		g_arenaservers.list.width				= vw / SMALLCHAR_WIDTH;
	}
	g_arenaservers.list.generic.type			= MTYPE_SCROLLLIST;
	g_arenaservers.list.generic.flags			= QMF_HIGHLIGHT_IF_FOCUS;
	g_arenaservers.list.generic.id				= ID_LIST;
	g_arenaservers.list.generic.callback		= ArenaServers_Event;
	g_arenaservers.list.generic.ownerdraw		= ArenaServers_DrawList;
	g_arenaservers.list.height					= PLAY_LIST_ROWS;
	g_arenaservers.list.itemnames				= (const char **)g_arenaservers.items;
	for( i = 0; i < MAX_LISTBOXITEMS; i++ ) {
		g_arenaservers.items[i] = g_arenaservers.table[i].buff;
	}

	g_arenaservers.mappic.generic.type			= MTYPE_BITMAP;
	g_arenaservers.mappic.generic.flags			= QMF_LEFT_JUSTIFY|QMF_INACTIVE;
	g_arenaservers.mappic.generic.x				= 450;
	g_arenaservers.mappic.generic.y				= 10;
	g_arenaservers.mappic.width					= 160;
	g_arenaservers.mappic.height				= 120;
	g_arenaservers.mappic.errorpic				= ART_UNKNOWNMAP;

	g_arenaservers.arrows.generic.type			= MTYPE_BITMAP;
	g_arenaservers.arrows.generic.name			= ART_ARROWS0;
	g_arenaservers.arrows.generic.flags			= QMF_LEFT_JUSTIFY|QMF_INACTIVE;
	g_arenaservers.arrows.generic.callback		= ArenaServers_Event;
	g_arenaservers.arrows.generic.x				= 512+48;
	g_arenaservers.arrows.generic.y				= 210-64+16;
	g_arenaservers.arrows.width					= 64;
	g_arenaservers.arrows.height				= 128;

	g_arenaservers.up.generic.type				= MTYPE_BITMAP;
	g_arenaservers.up.generic.flags				= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_MOUSEONLY;
	g_arenaservers.up.generic.callback			= ArenaServers_Event;
	g_arenaservers.up.generic.id				= ID_SCROLL_UP;
	g_arenaservers.up.generic.x					= 512+48;
	g_arenaservers.up.generic.y					= 210-64+16;
	g_arenaservers.up.width						= 64;
	g_arenaservers.up.height					= 64;
	g_arenaservers.up.focuspic					= ART_ARROWS_UP;

	g_arenaservers.down.generic.type			= MTYPE_BITMAP;
	g_arenaservers.down.generic.flags			= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_MOUSEONLY;
	g_arenaservers.down.generic.callback		= ArenaServers_Event;
	g_arenaservers.down.generic.id				= ID_SCROLL_DOWN;
	g_arenaservers.down.generic.x				= 512+48;
	g_arenaservers.down.generic.y				= 210+16;
	g_arenaservers.down.width					= 64;
	g_arenaservers.down.height					= 64;
	g_arenaservers.down.focuspic				= ART_ARROWS_DOWN;

	y = 440;
	g_arenaservers.status.generic.type		= MTYPE_TEXT;
	g_arenaservers.status.generic.x			= 320;
	g_arenaservers.status.generic.y			= y;
	g_arenaservers.status.string			= statusbuffer;
	g_arenaservers.status.style				= UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW;
	g_arenaservers.status.color				= menu_text_color;

	y += SMALLCHAR_HEIGHT;
	g_arenaservers.statusbar.generic.type   = MTYPE_TEXT;
	g_arenaservers.statusbar.generic.x	    = 320;
	g_arenaservers.statusbar.generic.y	    = y;
	g_arenaservers.statusbar.string	        = "";
	g_arenaservers.statusbar.style	        = UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW;
	g_arenaservers.statusbar.color	        = text_color_normal;

	g_arenaservers.go.generic.type		= MTYPE_PTEXT;
	g_arenaservers.go.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	g_arenaservers.go.generic.id		= ID_CONNECT;
	g_arenaservers.go.generic.callback	= ArenaServers_Event;
	g_arenaservers.go.generic.x			= 28;
	g_arenaservers.go.generic.y			= 119;
	g_arenaservers.go.string			= "FIGHT!";
	g_arenaservers.go.style				= UI_LEFT | UI_DROPSHADOW | UI_TINYFONT;
	g_arenaservers.go.color				= color_white;
	y+=38;
	g_arenaservers.create.generic.type		= MTYPE_PTEXT;
	g_arenaservers.create.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	g_arenaservers.create.generic.id		= ID_CREATE;
	g_arenaservers.create.generic.callback	= ArenaServers_Event;
	g_arenaservers.create.generic.x			= 28;
	g_arenaservers.create.generic.y			= 157;
	g_arenaservers.create.string			= "CREATE";
	g_arenaservers.create.style				= UI_LEFT | UI_DROPSHADOW | UI_TINYFONT;
	g_arenaservers.create.color				= color_white;
	y+=38;
	g_arenaservers.refresh.generic.type		= MTYPE_PTEXT;
	g_arenaservers.refresh.generic.flags	= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	g_arenaservers.refresh.generic.id		= ID_REFRESH;
	g_arenaservers.refresh.generic.callback	= ArenaServers_Event;
	g_arenaservers.refresh.generic.x		= 28;
	g_arenaservers.refresh.generic.y		= 195;
	g_arenaservers.refresh.string			= "REFRESH";
	g_arenaservers.refresh.style			= UI_LEFT | UI_DROPSHADOW | UI_TINYFONT;
	g_arenaservers.refresh.color			= color_white;
	y+=38;
	g_arenaservers.specify.generic.type		= MTYPE_PTEXT;
	g_arenaservers.specify.generic.flags	= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	g_arenaservers.specify.generic.id		= ID_SPECIFY;
	g_arenaservers.specify.generic.callback	= ArenaServers_Event;
	g_arenaservers.specify.generic.x		= 28;
	g_arenaservers.specify.generic.y		= 233;
	g_arenaservers.specify.string			= "SPECIFY";
	g_arenaservers.specify.style			= UI_LEFT | UI_DROPSHADOW | UI_TINYFONT;
	g_arenaservers.specify.color			= color_white;
	y+=38;

	g_arenaservers.back.generic.type		= MTYPE_PTEXT;
	g_arenaservers.back.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	g_arenaservers.back.generic.id			= ID_BACK;
	g_arenaservers.back.generic.callback	= ArenaServers_Event;
	g_arenaservers.back.generic.x			= 28;
	g_arenaservers.back.generic.y			= 271;
	g_arenaservers.back.string				= "BACK";
	g_arenaservers.back.style				= UI_LEFT | UI_DROPSHADOW | UI_TINYFONT;
	g_arenaservers.back.color				= color_white;
	y+=38;

	g_arenaservers.remove.generic.type		= MTYPE_BITMAP;
	g_arenaservers.remove.generic.name		= ART_REMOVE0;
	g_arenaservers.remove.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	g_arenaservers.remove.generic.callback	= ArenaServers_Event;
	g_arenaservers.remove.generic.id		= ID_REMOVE;
	g_arenaservers.remove.generic.x			= 512;
	g_arenaservers.remove.generic.y			= 86;
	g_arenaservers.remove.width				= 96;
	g_arenaservers.remove.height			= 48;
	g_arenaservers.remove.focuspic			= ART_REMOVE1;


	// menu redesign: tabs, sort, list, footer buttons and the two cards
	g_arenaservers.menu.mobile = UI_MobileLayout();
	UI_DCanvas( g_arenaservers.menu.mobile );
	playAddress = qfalse;
	if ( g_arenaservers.menu.mobile ) {
		ArenaServers_MenuInitPhone();
		return;
	}
	for ( i = 0; i < 3; i++ ) {
		ArenaServers_DItem( &g_arenaservers.tabs[i], ID_TAB0 + i, ArenaServers_DrawTab, 72 + i * 156, 152, 150, 40 );
	}
	ArenaServers_DItem( &g_arenaservers.sortsel, ID_SORTSEL, ArenaServers_DrawSort, 650, 156, 184, 30 );
	ArenaServers_DItem( &g_arenaservers.refreshBtn, ID_REFRESH, ArenaServers_DrawButton, 520, 600, 150, 42 );
	ArenaServers_DItem( &g_arenaservers.joinBtn, ID_CONNECT, ArenaServers_DrawButton, 684, 600, 150, 42 );
	ArenaServers_DItem( &g_arenaservers.createBtn, ID_CREATE, ArenaServers_DrawCreate, 904, 382, 170, 46 );
	ArenaServers_DItem( &g_arenaservers.connectBtn, ID_CONNECTTYPED, ArenaServers_DrawButton, 1060, 592, 148, 44 );

	g_arenaservers.address.generic.type			= MTYPE_FIELD;
	g_arenaservers.address.generic.flags		= QMF_NODEFAULTINIT;
	g_arenaservers.address.generic.ownerdraw	= ArenaServers_DrawAddress;
	g_arenaservers.address.field.widthInChars	= 32;
	g_arenaservers.address.field.maxchars		= 64;
	{
		float vx, vy, vw, vh;

		vx = 904; vy = 512; vw = 304; vh = 42;
		UI_DToV( &vx, &vy, &vw, &vh );
		g_arenaservers.address.generic.x		= vx;
		g_arenaservers.address.generic.y		= vy;
		g_arenaservers.address.generic.left		= vx;
		g_arenaservers.address.generic.top		= vy;
		g_arenaservers.address.generic.right	= vx + vw;
		g_arenaservers.address.generic.bottom	= vy + vh;
	}

	for ( i = 0; i < 3; i++ ) {
		Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.tabs[i] );
	}
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.sortsel );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.list );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.refreshBtn );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.joinBtn );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.createBtn );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.address );
	Menu_AddItem( &g_arenaservers.menu, (void*) &g_arenaservers.connectBtn );

	ArenaServers_LoadFavorites();

	ArenaServers_InitState();
}

/*
=================
ArenaServers_InitState

browser settings, shared by both layouts
=================
*/
static void ArenaServers_InitState( void ) {
	int value;

	g_servertype = Com_Clamp( 0, 3, ui_browserMaster.integer );
	// hack to get rid of MPlayer stuff
	value = g_servertype;
	if (value >= 1)
		value--;
	g_arenaservers.master.curvalue = value;

	g_gametype = Com_Clamp( 0, 4, ui_browserGameType.integer );
	g_arenaservers.gametype.curvalue = g_gametype;

	g_sortkey = Com_Clamp( 0, 4, ui_browserSortKey.integer );
	g_arenaservers.sortkey.curvalue = g_sortkey;

	g_fullservers = Com_Clamp( 0, 1, ui_browserShowFull.integer );
	g_arenaservers.showfull.curvalue = g_fullservers;

	g_emptyservers = Com_Clamp( 0, 1, ui_browserShowEmpty.integer );
	g_arenaservers.showempty.curvalue = g_emptyservers;

	// force to initial state and refresh
	g_arenaservers.master.curvalue = g_servertype = ArenaServers_SetType(g_servertype);

	trap_Cvar_Register(NULL, "debug_protocol", "", 0 );
}


/*
=================
ArenaServers_Cache
=================
*/
void ArenaServers_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_BACK0 );
	trap_R_RegisterShaderNoMip( ART_BACK1 );
	trap_R_RegisterShaderNoMip( ART_CREATE0 );
	trap_R_RegisterShaderNoMip( ART_CREATE1 );
	trap_R_RegisterShaderNoMip( ART_SPECIFY0 );
	trap_R_RegisterShaderNoMip( ART_SPECIFY1 );
	trap_R_RegisterShaderNoMip( ART_REFRESH0 );
	trap_R_RegisterShaderNoMip( ART_REFRESH1 );
	trap_R_RegisterShaderNoMip( ART_CONNECT0 );
	trap_R_RegisterShaderNoMip( ART_CONNECT1 );
	trap_R_RegisterShaderNoMip( ART_ARROWS0  );
	trap_R_RegisterShaderNoMip( ART_ARROWS_UP );
	trap_R_RegisterShaderNoMip( ART_ARROWS_DOWN );
	trap_R_RegisterShaderNoMip( ART_UNKNOWNMAP );
}


/*
=================
UI_ArenaServersMenu
=================
*/
void UI_ArenaServersMenu( void ) {
	//trap_S_StopBackgroundTrack();
	//trap_S_StartBackgroundTrack("music/yamamoto/menu02.ogg", "music/yamamoto/menu02.ogg");
	ArenaServers_MenuInit();
	uis.menuamount = -1;
	uis.hideEarth = qtrue;
	uis.showFrame = qfalse;
	UI_PushMenu( &g_arenaservers.menu );
}
