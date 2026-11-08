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

CONFIRMATION MENU

=======================================================================
*/


#include "ui_local.h"


#define ART_CONFIRM_FRAME	"interface/art/cut_frame"

#define ID_CONFIRM_NO		10
#define ID_CONFIRM_YES		11


typedef struct {
	menuframework_s menu;

	menutext_s		no;
	menutext_s		yes;

	int				slashX;
	const char *	question;
	void			(*draw)( void );
	void			(*action)( qboolean result );

	int style;
	const char **lines;
} confirmMenu_t;


static confirmMenu_t	s_confirm;

// PC on the 1280x720 canvas, phone on the 844x390 one (dp)
typedef struct {
	int		glassX, glassY, glassW;
	int		ballX, ballY, ballW, ballH;
	int		textX, questionY, questionSize;
	int		lineY, lineStep, lineSize;
	int		buttonW, buttonH;
	int		yesX, noX, buttonY, okX, okY;
	int		confirmH, messageH;
} confirmLayout_t;

static const confirmLayout_t confirmLayouts[2] = {
	{ 420, 240, 440,	806, 196, 74, 89,	640, 286, 32,	276, 26, 20,	160, 46,	472, 648, 368, 560, 400,	220, 240 },
	{ 212, 96, 420,		596, 72, 44, 53,	422, 126, 24,	120, 22, 15,	170, 52,	246, 440, 214, 337, 236,	198, 212 }
};

#define CL	( confirmLayouts[s_confirm.menu.mobile ? 1 : 0] )

// a tap outside the dialog answers no (phone layout)
static qboolean ConfirmMenu_TapOutside( void ) {
	float x, y, w, h;

	if ( !s_confirm.menu.mobile || !UI_TouchMode() ) {
		return qfalse;
	}
	UI_DCanvas( qtrue );
	x = CL.glassX; y = CL.glassY; w = CL.glassW; h = s_confirm.lines ? CL.messageH : CL.confirmH;
	UI_DToV( &x, &y, &w, &h );
	return ( uis.cursorx < x || uis.cursorx > x + w || uis.cursory < y || uis.cursory > y + h );
}


/*
=================
ConfirmMenu_Event
=================
*/
static void ConfirmMenu_Event( void* ptr, int event ) {
	qboolean	result;

	if( event != QM_ACTIVATED ) {
		return;
	}

	UI_PopMenu();

	if( ((menucommon_s*)ptr)->id == ID_CONFIRM_NO ) {
		result = qfalse;
	}
	else {
		result = qtrue;
	}

	if( s_confirm.action ) {
		s_confirm.action( result );
	}
}


/*
=================
ConfirmMenu_Key
=================
*/
static sfxHandle_t ConfirmMenu_Key( int key ) {
	switch ( key ) {
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		key = K_TAB;
		break;

	case 'n':
	case 'N':
		ConfirmMenu_Event( &s_confirm.no, QM_ACTIVATED );
		break;

	case 'y':
	case 'Y':
		ConfirmMenu_Event( &s_confirm.yes, QM_ACTIVATED );
		break;
	case K_MOUSE1:
		if ( ConfirmMenu_TapOutside() ) {
			ConfirmMenu_Event( s_confirm.lines ? &s_confirm.yes : &s_confirm.no, QM_ACTIVATED );
			return menu_out_sound;
		}
		break;
	}

	return Menu_DefaultKey( &s_confirm.menu, key );
}


/*
=================
MessaheMenu_Draw
=================
*/
/*
=================
Menu redesign: a glass dialog over the dimmed screen, the dragon ball of
the old frame on its corner and the answers as ki buttons
=================
*/
static void ConfirmMenu_DrawButton( void *self ) {
	menutext_s *t;

	t = (menutext_s *)self;
	UI_DKiButton( t->generic.x, t->generic.y + ( UI_TouchPressed( self ) ? 2.0f : 0.0f ), CL.buttonW, CL.buttonH, t->string, NULL,
		UI_DFocus( &s_confirm.menu, self ), qfalse, qfalse );
}

static void ConfirmMenu_PlaceButton( menutext_s *t, int x, int y ) {
	float vx, vy, vw, vh;

	t->generic.x = x;
	t->generic.y = y;
	t->generic.ownerdraw = ConfirmMenu_DrawButton;
	vx = x; vy = y; vw = CL.buttonW; vh = CL.buttonH;
	UI_DToV( &vx, &vy, &vw, &vh );
	t->generic.left = vx;
	t->generic.top = vy;
	t->generic.right = vx + vw;
	t->generic.bottom = vy + vh;
}

static void ConfirmMenu_DrawDialog( float h ) {
	float	bias;
	vec4_t	shade;

	uis.menuamount = -1;
	bias = ( uis.scale > 0.0f ) ? ( uis.bias / uis.scale ) : 0.0f;
	shade[0] = 0.02f; shade[1] = 0.024f; shade[2] = 0.10f; shade[3] = 0.6f;
	UI_FillRect( -bias - uis.drawOffsetX, 0, 640 + 2 * bias, 480, shade );
	UI_DGlass( CL.glassX, CL.glassY, CL.glassW, h );
	trap_R_SetColor( NULL );
	UI_DPic( CL.ballX, CL.ballY + 2.0f * sin( uis.realtime / 300.0f ), CL.ballW, CL.ballH, uis.dragonBall );
}

static void MessageMenu_Draw( void ) {
	int i;

	ConfirmMenu_DrawDialog( CL.messageH );
	for( i = 0; s_confirm.lines[i]; i++ ) {
		UI_DText( CL.textX, CL.lineY + i * CL.lineStep, s_confirm.lines[i], CL.lineSize, UI_CENTER, i ? zeq_text2 : zeq_white );
	}

	Menu_Draw( &s_confirm.menu );

	if( s_confirm.draw ) {
		s_confirm.draw();
	}
}

/*
=================
ConfirmMenu_Draw
=================
*/
static void ConfirmMenu_Draw( void ) {
	static const char *hints[] = { "Y", "Yes", "N", "No", "ESC", "Cancel" };

	ConfirmMenu_DrawDialog( CL.confirmH );
	UI_DProp( CL.textX, CL.questionY, s_confirm.question, CL.questionSize, UI_CENTER|UI_DROPSHADOW, zeq_white );
	// over a match the HUD is in the bottom left corner: center them there
	if ( s_confirm.menu.fullscreen ) {
		UI_DHints( hints, 3 );
	}
	else {
		UI_DHintsCentered( hints, 3 );
	}

	Menu_Draw( &s_confirm.menu );

	if( s_confirm.draw ) {
		s_confirm.draw();
	}
}


/*
=================
ConfirmMenu_Cache
=================
*/
void ConfirmMenu_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_CONFIRM_FRAME );
}


/*
=================
UI_ConfirmMenu_Style
=================
*/
void UI_ConfirmMenu_Style( const char *question, int style, void (*draw)( void ), void (*action)( qboolean result ) ) {
	uiClientState_t	cstate;
	int	n1, n2, n3;
	int	l1, l2, l3;

	uis.menuamount = 0;
	// zero set all our globals
	memset( &s_confirm, 0, sizeof(s_confirm) );

	ConfirmMenu_Cache();

	n1 = UI_ProportionalStringWidth( "YES/NO" );
	n2 = UI_ProportionalStringWidth( "YES" ) + PROP_GAP_WIDTH;
	n3 = UI_ProportionalStringWidth( "/" )  + PROP_GAP_WIDTH;
	l1 = 320 - ( n1 / 2 );
	l2 = l1 + n2;
	l3 = l2 + n3;
	s_confirm.slashX = l2;

	s_confirm.question = question;
	s_confirm.draw = draw;
	s_confirm.action = action;
	s_confirm.style = style;

	s_confirm.menu.draw       = ConfirmMenu_Draw;
	s_confirm.menu.key        = ConfirmMenu_Key;
	s_confirm.menu.wrapAround = qtrue;
	s_confirm.menu.mobile     = UI_MobileLayout();
	UI_DCanvas( s_confirm.menu.mobile );

	trap_GetClientState( &cstate );
	if ( cstate.connState >= CA_CONNECTED ) {
		s_confirm.menu.fullscreen = qfalse;
	}
	else {
		s_confirm.menu.fullscreen = qtrue;
	}

	s_confirm.yes.generic.type		= MTYPE_PTEXT;
	s_confirm.yes.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_confirm.yes.generic.callback	= ConfirmMenu_Event;
	s_confirm.yes.generic.id		= ID_CONFIRM_YES;
	s_confirm.yes.generic.x			= l1;
	s_confirm.yes.generic.y			= 264;
	s_confirm.yes.string			= "YES";
	s_confirm.yes.color				= color_white;
	s_confirm.yes.style				= UI_LEFT|UI_DROPSHADOW;

	s_confirm.no.generic.type		= MTYPE_PTEXT;
	s_confirm.no.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_confirm.no.generic.callback	= ConfirmMenu_Event;
	s_confirm.no.generic.id			= ID_CONFIRM_NO;
	s_confirm.no.generic.x		    = l3;
	s_confirm.no.generic.y		    = 264;
	s_confirm.no.string				= "NO";
	s_confirm.no.color			    = color_white;
	s_confirm.no.style			    = UI_LEFT|UI_DROPSHADOW;

	Menu_AddItem( &s_confirm.menu,	&s_confirm.yes );
	Menu_AddItem( &s_confirm.menu,	&s_confirm.no );
	ConfirmMenu_PlaceButton( &s_confirm.yes, CL.yesX, CL.buttonY );
	ConfirmMenu_PlaceButton( &s_confirm.no, CL.noX, CL.buttonY );

	UI_PushMenu( &s_confirm.menu );

	Menu_SetCursorToItem( &s_confirm.menu, &s_confirm.no );
}

/*
=================
UI_ConfirmMenu
=================
*/
void UI_ConfirmMenu( const char *question, void (*draw)( void ), void (*action)( qboolean result ) ) {
	UI_ConfirmMenu_Style(question, UI_CENTER|UI_INVERSE, draw, action);
}

/*
=================
UI_Message
hacked over from Confirm stuff
=================
*/
void UI_Message( const char **lines ) {
	uiClientState_t	cstate;
	int n1, l1;

	// zero set all our globals
	memset( &s_confirm, 0, sizeof(s_confirm) );

	ConfirmMenu_Cache();

	n1 = UI_ProportionalStringWidth( "OK" );
	l1 = 320 - ( n1 / 2 );

	s_confirm.lines = lines;
	s_confirm.style = UI_CENTER|UI_INVERSE|UI_SMALLFONT;

	s_confirm.menu.draw       = MessageMenu_Draw;
	s_confirm.menu.key        = ConfirmMenu_Key;
	s_confirm.menu.wrapAround = qtrue;
	s_confirm.menu.mobile     = UI_MobileLayout();
	UI_DCanvas( s_confirm.menu.mobile );

	trap_GetClientState( &cstate );
	if ( cstate.connState >= CA_CONNECTED ) {
		s_confirm.menu.fullscreen = qfalse;
	}
	else {
		s_confirm.menu.fullscreen = qtrue;
	}

	s_confirm.yes.generic.type		= MTYPE_PTEXT;
	s_confirm.yes.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_confirm.yes.generic.callback	= ConfirmMenu_Event;
	s_confirm.yes.generic.id		= ID_CONFIRM_YES;
	s_confirm.yes.generic.x			= l1;
	s_confirm.yes.generic.y			= 280;
	s_confirm.yes.string			= "OK";
	s_confirm.yes.color				= color_red;
	s_confirm.yes.style				= UI_LEFT;

	Menu_AddItem( &s_confirm.menu,	&s_confirm.yes );
	ConfirmMenu_PlaceButton( &s_confirm.yes, CL.okX, CL.okY );

	UI_PushMenu( &s_confirm.menu );

	Menu_SetCursorToItem( &s_confirm.menu, &s_confirm.yes );
}
