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

FIGHTER SELECT (player model)

Menu redesign: laid out on the 1280x720 design canvas (see ui_shared.c).
Characters are the folders under players/, skins their icon_<skin>.png,
transformations their tier<N>/icon.png and tier.cfg.

=======================================================================
*/

#include "ui_local.h"

#define LOW_MEMORY			(5 * 1024 * 1024)

#define MAX_FIGHTERS		32
#define MAX_SKINS			8
#define MAX_TIERS			8
#define FIGHTERS_PER_PAGE	8

#define ID_FIGHTER0			0		// + slot on the page
#define ID_SKIN0			20		// + skin
#define ID_PREVPAGE			40
#define ID_NEXTPAGE			41
#define ID_CONFIRM			42
#define ID_TIER0			50		// + tier

typedef struct {
	char		model[MAX_QPATH];
	char		skins[MAX_SKINS][32];
	int			numSkins;
} fighterDef_t;

typedef struct
{
	menuframework_s	menu;
	menubitmap_s	fighters[FIGHTERS_PER_PAGE];
	menubitmap_s	skins[MAX_SKINS];
	menubitmap_s	tiers[MAX_TIERS];
	menubitmap_s	prevPage;
	menubitmap_s	nextPage;
	menufield_s		name;
	menubitmap_s	player;
	menubitmap_s	confirm;
	playerInfo_t	playerinfo;

	fighterDef_t	defs[MAX_FIGHTERS];
	int				numDefs;
	int				page;
	int				numPages;
	int				selected;		// index in defs
	int				skin;			// index in its skins
	int				selectTime;

	char			tierNames[MAX_TIERS][32];
	int				numTiers;
	int				tier;			// transformation shown, 0 = base form
	int				tierTime;
} playermodel_t;

static playermodel_t s_playermodel;

/*
=================
PlayerModel_ModelSkin
=================
*/
static void PlayerModel_ModelSkin( char *out, int size ) {
	fighterDef_t *def;

	def = &s_playermodel.defs[s_playermodel.selected];
	if ( !Q_stricmp( def->skins[s_playermodel.skin], "default" ) ) {
		Q_strncpyz( out, def->model, size );
	}
	else {
		Com_sprintf( out, size, "%s/%s", def->model, def->skins[s_playermodel.skin] );
	}
}

/*
=================
PlayerModel_LoadTiers

tier names come from tier<N>/tier.cfg ("tierName X"), the first one is the base form
=================
*/
static void PlayerModel_LoadTiers( void ) {
	fileHandle_t	f;
	char			text[1024];
	char			*p;
	char			*token;
	int				len;
	int				i;
	fighterDef_t	*def;

	def = &s_playermodel.defs[s_playermodel.selected];
	s_playermodel.numTiers = 0;
	for ( i = 1; i <= MAX_TIERS; i++ ) {
		if ( !trap_R_RegisterShaderNoMip( va( "players/%s/tier%i/icon.png", def->model, i ) ) ) {
			break;
		}
		Q_strncpyz( s_playermodel.tierNames[s_playermodel.numTiers], i == 1 ? "Base" : va( "Tier %i", i ), 32 );
		len = trap_FS_FOpenFile( va( "players/%s/tier%i/tier.cfg", def->model, i ), &f, FS_READ );
		if ( len > 0 ) {
			if ( len >= (int)sizeof( text ) ) {
				len = sizeof( text ) - 1;
			}
			trap_FS_Read( text, len, f );
			text[len] = 0;
			p = text;
			while ( 1 ) {
				token = COM_Parse( &p );
				if ( !token[0] ) {
					break;
				}
				if ( !Q_stricmp( token, "tierName" ) ) {
					Q_strncpyz( s_playermodel.tierNames[s_playermodel.numTiers], COM_ParseExt( &p, qfalse ), 32 );
					break;
				}
			}
			trap_FS_FCloseFile( f );
		}
		s_playermodel.numTiers++;
	}
}

/*
=================
PlayerModel_UpdateModel
=================
*/
static void PlayerModel_LoadModel( void )
{
	vec3_t	viewangles;
	vec3_t	moveangles;
	char	modelskin[MAX_QPATH];
	int		i;

	memset( &s_playermodel.playerinfo, 0, sizeof(playerInfo_t) );

	viewangles[YAW]   = 180 - 30;
	viewangles[PITCH] = 0;
	viewangles[ROLL]  = 0;
	VectorClear( moveangles );

	PlayerModel_ModelSkin( modelskin, sizeof( modelskin ) );
	if( trap_MemoryRemaining() > LOW_MEMORY ) {
		UI_PlayerInfo_SetModelTier( &s_playermodel.playerinfo, modelskin, s_playermodel.tier );
		UI_PlayerInfo_SetInfo( &s_playermodel.playerinfo, ANIM_IDLE, ANIM_IDLE, viewangles, moveangles, WP_NONE, qfalse );
	}

	for ( i = 0; i < MAX_TIERS; i++ ) {
		if ( i < s_playermodel.numTiers ) {
			s_playermodel.tiers[i].generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		}
		else {
			s_playermodel.tiers[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		}
	}
	s_playermodel.tierTime = uis.realtime;
}

static void PlayerModel_UpdateModel( void )
{
	if ( !s_playermodel.numDefs ) {
		return;
	}
	PlayerModel_LoadTiers();
	if ( s_playermodel.tier >= s_playermodel.numTiers ) {
		s_playermodel.tier = 0;
	}
	PlayerModel_LoadModel();
	s_playermodel.selectTime = uis.realtime;
}

// shows a transformation in the preview, the selection is not saved
static void PlayerModel_SetTier( int tier ) {
	if ( tier < 0 || tier >= s_playermodel.numTiers || tier == s_playermodel.tier ) {
		return;
	}
	s_playermodel.tier = tier;
	PlayerModel_LoadModel();
}

/*
=================
PlayerModel_SaveChanges
=================
*/
static void PlayerModel_SaveChanges( void )
{
	char modelskin[MAX_QPATH];

	if ( s_playermodel.numDefs ) {
		PlayerModel_ModelSkin( modelskin, sizeof( modelskin ) );
		trap_Cvar_Set( "model", modelskin );
		trap_Cvar_Set( "headmodel", modelskin );
		trap_Cvar_Set( "team_model", modelskin );
		trap_Cvar_Set( "team_headmodel", modelskin );
	}
	if ( s_playermodel.name.field.buffer[0] ) {
		trap_Cvar_Set( "name", s_playermodel.name.field.buffer );
	}
}

/*
=================
PlayerModel_Refresh

shows the fighters of the current page and the skins of the selected one
=================
*/
static void PlayerModel_Refresh( void ) {
	int i;
	int index;

	for ( i = 0; i < FIGHTERS_PER_PAGE; i++ ) {
		index = s_playermodel.page * FIGHTERS_PER_PAGE + i;
		if ( index < s_playermodel.numDefs ) {
			s_playermodel.fighters[i].generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		}
		else {
			s_playermodel.fighters[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		}
	}
	for ( i = 0; i < MAX_SKINS; i++ ) {
		if ( s_playermodel.numDefs && i < s_playermodel.defs[s_playermodel.selected].numSkins ) {
			s_playermodel.skins[i].generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		}
		else {
			s_playermodel.skins[i].generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		}
	}
	if ( s_playermodel.numPages > 1 ) {
		s_playermodel.prevPage.generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
		s_playermodel.nextPage.generic.flags &= ~( QMF_HIDDEN|QMF_INACTIVE );
	}
	else {
		s_playermodel.prevPage.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
		s_playermodel.nextPage.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
	}
}

static void PlayerModel_Select( int def ) {
	if ( def < 0 || def >= s_playermodel.numDefs ) {
		return;
	}
	if ( def != s_playermodel.selected ) {
		s_playermodel.selected = def;
		s_playermodel.skin = 0;
		s_playermodel.tier = 0;
		PlayerModel_UpdateModel();
	}
	PlayerModel_Refresh();
}

/*
=================
PlayerModel_MenuEvent
=================
*/
static void PlayerModel_MenuEvent( void* ptr, int event )
{
	int id;

	if (event != QM_ACTIVATED)
		return;

	id = ((menucommon_s*)ptr)->id;
	if ( id >= ID_FIGHTER0 && id < ID_FIGHTER0 + FIGHTERS_PER_PAGE ) {
		PlayerModel_Select( s_playermodel.page * FIGHTERS_PER_PAGE + id - ID_FIGHTER0 );
		return;
	}
	if ( id >= ID_SKIN0 && id < ID_SKIN0 + MAX_SKINS ) {
		s_playermodel.skin = id - ID_SKIN0;
		PlayerModel_UpdateModel();
		return;
	}
	if ( id >= ID_TIER0 && id < ID_TIER0 + MAX_TIERS ) {
		PlayerModel_SetTier( id - ID_TIER0 );
		return;
	}

	switch ( id ) {
	case ID_PREVPAGE:
		// the page indicator flips through the pages
		s_playermodel.page = ( s_playermodel.page + 1 ) % s_playermodel.numPages;
		PlayerModel_Refresh();
		break;
	case ID_NEXTPAGE:
		if ( s_playermodel.page < s_playermodel.numPages - 1 ) {
			s_playermodel.page++;
			PlayerModel_Refresh();
		}
		break;
	case ID_CONFIRM:
		PlayerModel_SaveChanges();
		UI_PopMenu();
		break;
	}
}

/*
=================
PlayerModel_MenuKey

left and right walk the roster and flip pages at its ends
=================
*/
static sfxHandle_t PlayerModel_MenuKey( int key )
{
	menucommon_s	*m;
	int				slot;
	int				target;

	m = (menucommon_s *)Menu_ItemAtCursor( &s_playermodel.menu );

	if ( m == (menucommon_s *)&s_playermodel.name && key != K_ESCAPE && key != K_MOUSE2 &&
		key != K_UPARROW && key != K_DOWNARROW && key != K_TAB ) {
		return Menu_DefaultKey( &s_playermodel.menu, key );
	}

	switch ( key ) {
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		if ( m && m->id >= ID_FIGHTER0 && m->id < ID_FIGHTER0 + FIGHTERS_PER_PAGE ) {
			slot = m->id - ID_FIGHTER0;
			target = s_playermodel.page * FIGHTERS_PER_PAGE + slot + ( ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) ? -1 : 1 );
			if ( target < 0 || target >= s_playermodel.numDefs ) {
				return menu_buzz_sound;
			}
			s_playermodel.page = target / FIGHTERS_PER_PAGE;
			PlayerModel_Refresh();
			Menu_SetCursorToItem( &s_playermodel.menu, &s_playermodel.fighters[target % FIGHTERS_PER_PAGE] );
			PlayerModel_Select( target );
			return menu_move_sound;
		}
		if ( m && m->id >= ID_SKIN0 && m->id < ID_SKIN0 + MAX_SKINS ) {
			slot = m->id - ID_SKIN0 + ( ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) ? -1 : 1 );
			if ( slot < 0 || slot >= s_playermodel.defs[s_playermodel.selected].numSkins ) {
				return menu_buzz_sound;
			}
			Menu_SetCursorToItem( &s_playermodel.menu, &s_playermodel.skins[slot] );
			s_playermodel.skin = slot;
			PlayerModel_UpdateModel();
			return menu_move_sound;
		}
		if ( m && m->id >= ID_TIER0 && m->id < ID_TIER0 + MAX_TIERS ) {
			slot = m->id - ID_TIER0 + ( ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) ? -1 : 1 );
			if ( slot < 0 || slot >= s_playermodel.numTiers ) {
				return menu_buzz_sound;
			}
			Menu_SetCursorToItem( &s_playermodel.menu, &s_playermodel.tiers[slot] );
			PlayerModel_SetTier( slot );
			return menu_move_sound;
		}
		break;

	case K_MOUSE2:
	case K_ESCAPE:
		PlayerModel_SaveChanges();
		break;
	}

	return Menu_DefaultKey( &s_playermodel.menu, key );
}

/*
=================
Drawing
=================
*/
static void PlayerModel_DrawFighter( void *self ) {
	menubitmap_s	*b;
	int				index;
	float			focus;
	float			enter;
	fighterDef_t	*def;

	b = (menubitmap_s *)self;
	index = s_playermodel.page * FIGHTERS_PER_PAGE + b->generic.id - ID_FIGHTER0;
	if ( index >= s_playermodel.numDefs ) {
		return;
	}
	def = &s_playermodel.defs[index];
	focus = UI_DFocus( &s_playermodel.menu, self );
	enter = UI_AnimFrac( uis.transitionTime, 60 + ( b->generic.id - ID_FIGHTER0 ) * 35, 260 );
	UI_DPortrait( b->generic.x + 4, b->generic.y + 10.0f * ( 1.0f - enter ), 96, trap_R_RegisterShaderNoMip( va( "players/%s/icon_default.png", def->model ) ),
		index == s_playermodel.selected, focus );
	UI_DText( b->generic.x + 52, b->generic.y + 106, def->model, 15, UI_CENTER, ( index == s_playermodel.selected || focus > 0.0f ) ? zeq_white : zeq_text2 );
}

static void PlayerModel_DrawSkin( void *self ) {
	menubitmap_s	*b;
	int				skin;
	fighterDef_t	*def;

	b = (menubitmap_s *)self;
	skin = b->generic.id - ID_SKIN0;
	def = &s_playermodel.defs[s_playermodel.selected];
	if ( skin >= def->numSkins ) {
		return;
	}
	UI_DPortrait( b->generic.x, b->generic.y, 64, trap_R_RegisterShaderNoMip( va( "players/%s/icon_%s.png", def->model, def->skins[skin] ) ),
		skin == s_playermodel.skin, UI_DFocus( &s_playermodel.menu, self ) );
	UI_DText( b->generic.x + 32, b->generic.y + 72, def->skins[skin], 15, UI_CENTER, skin == s_playermodel.skin ? zeq_white : zeq_text2 );
}

static void PlayerModel_DrawTier( void *self ) {
	menubitmap_s	*b;
	int				tier;
	float			focus;
	fighterDef_t	*def;

	b = (menubitmap_s *)self;
	tier = b->generic.id - ID_TIER0;
	if ( tier >= s_playermodel.numTiers ) {
		return;
	}
	def = &s_playermodel.defs[s_playermodel.selected];
	focus = UI_DFocus( &s_playermodel.menu, self );
	UI_DPortrait( b->generic.x, b->generic.y + 8.0f * ( 1.0f - UI_AnimFrac( s_playermodel.selectTime, tier * 60, 260 ) ), 58,
		trap_R_RegisterShaderNoMip( va( "players/%s/tier%i/icon.png", def->model, tier + 1 ) ), tier == s_playermodel.tier, focus );
	UI_DText( b->generic.x + 29, b->generic.y + 66, s_playermodel.tierNames[tier], 15, UI_CENTER,
		( tier == s_playermodel.tier || focus > 0.0f ) ? zeq_white : zeq_text2 );
}

static void PlayerModel_DrawPage( void *self ) {
	menubitmap_s *b;

	b = (menubitmap_s *)self;
	UI_DSelector( b->generic.x, b->generic.y, b->width, va( "%i / %i", s_playermodel.page + 1, s_playermodel.numPages ),
		UI_DFocus( &s_playermodel.menu, self ) );
}

static void PlayerModel_DrawName( void *self ) {
	menufield_s	*f;
	qboolean	focus;
	char		clean[64];

	f = (menufield_s *)self;
	focus = ( Menu_ItemAtCursor( &s_playermodel.menu ) == self );
	UI_DField( 256, 536, 268, 42, focus ? 1.0f : 0.0f );
	Q_strncpyz( clean, f->field.buffer, sizeof( clean ) );
	if ( focus ) {
		Q_CleanStr( clean );
	}
	UI_DText( 270, 547, focus ? clean : f->field.buffer, 20, UI_LEFT, zeq_white );
	if ( focus && ( ( uis.realtime / 400 ) & 1 ) ) {
		UI_DFill( 272 + UI_DTextWidth( clean, 20 ), 546, 2, 22, zeq_white );
	}
}

static void PlayerModel_DrawPlayer( void *self )
{
	menubitmap_s	*b;

	b = (menubitmap_s *)self;
	if( trap_MemoryRemaining() <= LOW_MEMORY ) {
		UI_DProp( 760, 360, "LOW MEMORY", 20, UI_CENTER, zeq_white );
		return;
	}
	UI_DrawPlayer( b->generic.left, b->generic.top, b->generic.right - b->generic.left, b->generic.bottom - b->generic.top,
		&s_playermodel.playerinfo, uis.realtime / PLAYER_MODEL_SPEED );
}

static void PlayerModel_DrawConfirm( void *self ) {
	menubitmap_s *b;

	b = (menubitmap_s *)self;
	UI_DCta( b->generic.x, b->generic.y, b->width, b->height, "CONFIRM", UI_DFocus( &s_playermodel.menu, self ) );
}

static void PlayerModel_MenuDraw( void ) {
	static const char *hints[] = { "LR", "Fighter", "ENTER", "Select", "ESC", "Back" };
	fighterDef_t	*def;
	char			title[MAX_QPATH];
	float			pop;
	int				i;
	float			x, y;

	uis.menuamount = -1;
	uis.hideEarth = qtrue;

	UI_DHeader( "MAIN MENU  >", "FIGHTER" );

	UI_DGlass( 48, 128, 500, 330 );
	UI_DSectionTitle( 72, 152, "ROSTER" );
	UI_DGlass( 48, 472, 500, 184 );
	UI_DSectionTitle( 72, 496, "SKIN" );
	UI_DSectionTitle( 256, 496, "PLAYER NAME" );
	UI_DText( 256, 590, "Up to 14 characters, ^ color codes allowed", 15, UI_LEFT, zeq_text3 );

	// the fighter on a blob shadow
	trap_R_SetColor( NULL );
	UI_DPic( 600, 600, 300, 90, uis.blobShadow );

	Menu_Draw( &s_playermodel.menu );

	if ( !s_playermodel.numDefs ) {
		UI_DHints( hints, 3 );
		return;
	}

	// name of the fighter and its transformations
	def = &s_playermodel.defs[s_playermodel.selected];
	pop = UI_AnimFrac( s_playermodel.selectTime, 0, 280 );
	Q_strncpyz( title, def->model, sizeof( title ) );
	Q_strupr( title );
	UI_DProp( 928, 136, "SELECTED", 14, UI_LEFT, zeq_text3 );
	UI_DProp( 928 + 16.0f * ( 1.0f - pop ), 158, title, 46, UI_LEFT|UI_DROPSHADOW, zeq_white );
	pop = UI_AnimFrac( s_playermodel.tierTime, 0, 240 );
	UI_DText( 928 + 10.0f * ( 1.0f - pop ), 214, va( "%s skin  -  %s", def->skins[s_playermodel.skin], s_playermodel.tierNames[s_playermodel.tier] ),
		19, UI_LEFT, zeq_text2 );

	UI_DSectionTitle( 928, 262, "TRANSFORMATIONS" );
	if ( s_playermodel.numTiers > 1 ) {
		y = 296 + ( ( s_playermodel.numTiers + 2 ) / 3 ) * 96 + 6;
		x = 928 + UI_DKeyHint( 928, y, "F", "+" ) - 26;
		x += UI_DKeyHint( x, y, "W", "hold in game to power up" );
	}

	UI_DHints( hints, 3 );
}

/*
=================
PlayerModel_BuildList

one entry per character folder with all its icon_<skin> files
=================
*/
static void PlayerModel_BuildList( void )
{
	int				numdirs;
	int				numfiles;
	char			dirlist[2048];
	char			filelist[2048];
	char			skinname[64];
	char			*dirptr;
	char			*fileptr;
	int				i;
	int				j;
	int				dirlen;
	int				filelen;
	fighterDef_t	*def;

	s_playermodel.numDefs = 0;

	numdirs = trap_FS_GetFileList( "players/", "/", dirlist, sizeof( dirlist ) );
	dirptr  = dirlist;
	for ( i = 0; i < numdirs && s_playermodel.numDefs < MAX_FIGHTERS; i++, dirptr += dirlen + 1 ) {
		dirlen = strlen( dirptr );
		if ( dirlen && dirptr[dirlen - 1] == '/' ) {
			dirptr[dirlen - 1] = '\0';
		}
		if ( !dirptr[0] || !strcmp( dirptr, "." ) || !strcmp( dirptr, ".." ) ) {
			continue;
		}

		def = &s_playermodel.defs[s_playermodel.numDefs];
		memset( def, 0, sizeof( *def ) );
		Q_strncpyz( def->model, dirptr, sizeof( def->model ) );

		numfiles = trap_FS_GetFileList( va( "players//%s", dirptr ), "png", filelist, sizeof( filelist ) );
		fileptr  = filelist;
		for ( j = 0; j < numfiles; j++, fileptr += filelen + 1 ) {
			filelen = strlen( fileptr );
			COM_StripExtension( fileptr, skinname, sizeof( skinname ) );
			if ( Q_stricmpn( skinname, "icon_", 5 ) || def->numSkins >= MAX_SKINS ) {
				continue;
			}
			// default first
			if ( !Q_stricmp( skinname + 5, "default" ) && def->numSkins ) {
				Q_strncpyz( def->skins[def->numSkins], def->skins[0], 32 );
				Q_strncpyz( def->skins[0], "default", 32 );
			}
			else {
				Q_strncpyz( def->skins[def->numSkins], skinname + 5, 32 );
			}
			def->numSkins++;
		}
		if ( def->numSkins ) {
			s_playermodel.numDefs++;
		}
	}

	s_playermodel.numPages = ( s_playermodel.numDefs + FIGHTERS_PER_PAGE - 1 ) / FIGHTERS_PER_PAGE;
	if ( s_playermodel.numPages < 1 ) {
		s_playermodel.numPages = 1;
	}
}

/*
=================
PlayerModel_SetMenuItems

start on the fighter and skin of the model cvar
=================
*/
static void PlayerModel_SetMenuItems( void )
{
	char	modelskin[MAX_QPATH];
	char	*skin;
	int		i;
	int		j;

	trap_Cvar_VariableStringBuffer( "model", modelskin, sizeof( modelskin ) );
	skin = strchr( modelskin, '/' );
	if ( skin ) {
		*skin++ = 0;
	}
	else {
		skin = "default";
	}
	for ( i = 0; i < s_playermodel.numDefs; i++ ) {
		if ( Q_stricmp( s_playermodel.defs[i].model, modelskin ) ) {
			continue;
		}
		s_playermodel.selected = i;
		for ( j = 0; j < s_playermodel.defs[i].numSkins; j++ ) {
			if ( !Q_stricmp( s_playermodel.defs[i].skins[j], skin ) ) {
				s_playermodel.skin = j;
			}
		}
		break;
	}
	s_playermodel.page = s_playermodel.selected / FIGHTERS_PER_PAGE;
	Q_strncpyz( s_playermodel.name.field.buffer, UI_Cvar_VariableString( "name" ), sizeof( s_playermodel.name.field.buffer ) );
}

/*
=================
PlayerModel_MenuInit
=================
*/
static void PlayerModel_DItem( menubitmap_s *b, int id, void (*draw)( void * ), int x, int y, int w, int h ) {
	b->generic.type			= MTYPE_BITMAP;
	b->generic.flags		= QMF_NODEFAULTINIT;
	b->generic.id			= id;
	b->generic.callback		= PlayerModel_MenuEvent;
	b->generic.ownerdraw	= draw;
	UI_DItem( b, x, y, w, h );
}

static void PlayerModel_MenuInit( void )
{
	int		i;
	float	vx, vy, vw, vh;

	memset( &s_playermodel, 0 ,sizeof(playermodel_t) );

	PlayerModel_Cache();

	s_playermodel.menu.key        = PlayerModel_MenuKey;
	s_playermodel.menu.draw       = PlayerModel_MenuDraw;
	s_playermodel.menu.wrapAround = qtrue;
	s_playermodel.menu.fullscreen = qtrue;

	for ( i = 0; i < FIGHTERS_PER_PAGE; i++ ) {
		PlayerModel_DItem( &s_playermodel.fighters[i], ID_FIGHTER0 + i, PlayerModel_DrawFighter,
			72 + ( i % 4 ) * 114, 184 + ( i / 4 ) * 132, 104, 124 );
	}
	PlayerModel_DItem( &s_playermodel.prevPage, ID_PREVPAGE, PlayerModel_DrawPage, 404, 146, 120, 26 );
	for ( i = 0; i < MAX_SKINS; i++ ) {
		PlayerModel_DItem( &s_playermodel.skins[i], ID_SKIN0 + i, PlayerModel_DrawSkin, 72 + i * 82, 528, 64, 90 );
	}

	s_playermodel.name.generic.type				= MTYPE_FIELD;
	s_playermodel.name.generic.flags			= QMF_NODEFAULTINIT;
	s_playermodel.name.generic.ownerdraw		= PlayerModel_DrawName;
	s_playermodel.name.field.widthInChars		= 14;
	s_playermodel.name.field.maxchars			= 14;
	vx = 256; vy = 536; vw = 268; vh = 42;
	UI_DToV( &vx, &vy, &vw, &vh );
	s_playermodel.name.generic.x				= vx;
	s_playermodel.name.generic.y				= vy;
	s_playermodel.name.generic.left				= vx;
	s_playermodel.name.generic.top				= vy;
	s_playermodel.name.generic.right			= vx + vw;
	s_playermodel.name.generic.bottom			= vy + vh;

	// the 3D fighter, its rect in virtual units for UI_DrawPlayer
	s_playermodel.player.generic.type			= MTYPE_BITMAP;
	s_playermodel.player.generic.flags			= QMF_INACTIVE|QMF_NODEFAULTINIT;
	s_playermodel.player.generic.ownerdraw		= PlayerModel_DrawPlayer;
	vx = 530; vy = -60; vw = 440; vh = 800;
	UI_DToV( &vx, &vy, &vw, &vh );
	s_playermodel.player.generic.left			= vx;
	s_playermodel.player.generic.top			= vy;
	s_playermodel.player.generic.right			= vx + vw;
	s_playermodel.player.generic.bottom			= vy + vh;

	for ( i = 0; i < MAX_TIERS; i++ ) {
		PlayerModel_DItem( &s_playermodel.tiers[i], ID_TIER0 + i, PlayerModel_DrawTier,
			928 + ( i % 3 ) * 104, 296 + ( i / 3 ) * 96, 64, 84 );
	}
	PlayerModel_DItem( &s_playermodel.confirm, ID_CONFIRM, PlayerModel_DrawConfirm, 1060 + UI_DExtra(), 652, 172, 48 );

	for ( i = 0; i < FIGHTERS_PER_PAGE; i++ ) {
		Menu_AddItem( &s_playermodel.menu, &s_playermodel.fighters[i] );
	}
	Menu_AddItem( &s_playermodel.menu, &s_playermodel.prevPage );
	for ( i = 0; i < MAX_SKINS; i++ ) {
		Menu_AddItem( &s_playermodel.menu, &s_playermodel.skins[i] );
	}
	Menu_AddItem( &s_playermodel.menu, &s_playermodel.name );
	Menu_AddItem( &s_playermodel.menu, &s_playermodel.player );
	for ( i = 0; i < MAX_TIERS; i++ ) {
		Menu_AddItem( &s_playermodel.menu, &s_playermodel.tiers[i] );
	}
	Menu_AddItem( &s_playermodel.menu, &s_playermodel.confirm );

	PlayerModel_SetMenuItems();
	PlayerModel_Refresh();
	PlayerModel_UpdateModel();
}

/*
=================
PlayerModel_Cache
=================
*/
void PlayerModel_Cache( void )
{
	int i;

	PlayerModel_BuildList();
	for ( i = 0; i < s_playermodel.numDefs; i++ ) {
		trap_R_RegisterShaderNoMip( va( "players/%s/icon_default.png", s_playermodel.defs[i].model ) );
	}
}

void UI_PlayerModelMenu(void)
{
	PlayerModel_MenuInit();
	UI_PushMenu( &s_playermodel.menu );
	uis.menuamount = -1;
	uis.showFrame = qfalse;
	uis.hideEarth = qtrue;

	if ( s_playermodel.numDefs ) {
		Menu_SetCursorToItem( &s_playermodel.menu, &s_playermodel.fighters[s_playermodel.selected % FIGHTERS_PER_PAGE] );
	}
}
