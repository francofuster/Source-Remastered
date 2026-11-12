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
/*
=======================================================================

TOUCH CONTROLS

Phone layout only: this page takes the place of the key bindings. Its
rows set the cvars of the touch layer of the engine (cl_touch.c and
cl_aimassist.c), and the preview draws the buttons where the engine puts
them, at the size and opacity chosen. The positions are a copy of the
table of cl_touch.c: the UI module cannot see the engine.

=======================================================================
*/

#include "ui_local.h"

#define ID_TC_CONTROLS		10
#define ID_TC_SYSTEM		11
#define ID_TC_GENERAL		12
#define ID_TC_PROFILE		13
#define ID_TC_AIM			14
#define ID_TC_SIZE			15
#define ID_TC_ALPHA			16
#define ID_TC_LOOK			17
#define ID_TC_HEIGHT		18
#define ID_TC_RESET			19

// preview: a phone of 844 x 390 dp drawn in this rect
#define TC_PREVIEW_X		508
#define TC_PREVIEW_Y		118
#define TC_PREVIEW_W		300
#define TC_PREVIEW_H		139

typedef struct {
	menuframework_s		menu;
	menutext_s			controls;
	menutext_s			system;
	menutext_s			general;
	menulist_s			profile;
	menuradiobutton_s	aim;
	menuslider_s		size;
	menuslider_s		alpha;
	menuslider_s		look;
	menulist_s			height;
	menubitmap_s		reset;
} touchControls_t;

static touchControls_t s_touch;

static const char *tcProfiles[] = { "Classic", "Assisted", NULL };
static const char *tcHeights[] = { "Screen", "540", "600", "720", "900", "1080", NULL };
static const int tcHeightValues[] = { 0, 540, 600, 720, 900, 1080 };
static int tcInitialHeight;		// r_androidHeight when the page opened

// the round buttons of cl_touch.c: dp from the right edge and the bottom
// (from the top when top is set; the pause button sits in the middle)
typedef struct {
	float		x, y, r;
	qboolean	top;
	qboolean	middle;
	const char	*label;
	float		tint[3];
} tcButton_t;

// without a lock-on: LOCK sits where GUARD shows up once locked, and the
// second attack is KI slid left
static const tcButton_t tcButtons[] = {
	{  92,  92, 44, qfalse, qfalse, "KI",	{ 1.00f, 0.62f, 0.15f } },
	{ 188,  72, 36, qfalse, qfalse, "Z",	{ 0.30f, 0.86f, 1.00f } },
	{  68, 192, 28, qfalse, qfalse, "",		{ 1.00f, 0.86f, 0.30f } },
	{ 268, 194, 28, qfalse, qfalse, "",		{ 0.75f, 0.85f, 1.00f } },
	{ 162, 164, 27, qfalse, qfalse, "",		{ 0.45f, 0.95f, 0.55f } },
	{   0,  32, 20, qtrue,  qtrue,  "",		{ 1.00f, 1.00f, 1.00f } }
};

static void TouchControls_HeightFromCvar( void ) {
	int i, value;

	value = (int)trap_Cvar_VariableValue( "r_androidHeight" );
	s_touch.height.curvalue = 0;
	for ( i = 0; tcHeights[i]; i++ ) {
		if ( tcHeightValues[i] == value ) {
			s_touch.height.curvalue = i;
		}
	}
}

static void TouchControls_SetItems( void ) {
	s_touch.profile.curvalue = trap_Cvar_VariableValue( "touch_profile" ) != 0;
	s_touch.aim.curvalue = trap_Cvar_VariableValue( "cl_aimAssist" ) != 0;
	s_touch.size.curvalue = trap_Cvar_VariableValue( "touch_scale" ) * 100.0f;
	s_touch.alpha.curvalue = trap_Cvar_VariableValue( "touch_alpha" ) * 100.0f;
	s_touch.look.curvalue = trap_Cvar_VariableValue( "touch_lookSpeed" ) * 100.0f;
	TouchControls_HeightFromCvar();
}

static void TouchControls_Event( void *ptr, int event ) {
	if ( event != QM_ACTIVATED ) {
		return;
	}
	switch ( ((menucommon_s *)ptr)->id ) {
	case ID_TC_SYSTEM:
		UI_PopMenu();
		UI_SystemSettingsMenu();
		break;
	case ID_TC_GENERAL:
		UI_PopMenu();
		UI_PreferencesMenu();
		break;
	case ID_TC_PROFILE:
		trap_Cvar_SetValue( "touch_profile", s_touch.profile.curvalue );
		break;
	case ID_TC_AIM:
		trap_Cvar_SetValue( "cl_aimAssist", s_touch.aim.curvalue );
		break;
	case ID_TC_SIZE:
		trap_Cvar_SetValue( "touch_scale", s_touch.size.curvalue / 100.0f );
		break;
	case ID_TC_ALPHA:
		trap_Cvar_SetValue( "touch_alpha", s_touch.alpha.curvalue / 100.0f );
		break;
	case ID_TC_LOOK:
		trap_Cvar_SetValue( "touch_lookSpeed", s_touch.look.curvalue / 100.0f );
		break;
	case ID_TC_HEIGHT:
		trap_Cvar_SetValue( "r_androidHeight", tcHeightValues[s_touch.height.curvalue] );
		break;
	case ID_TC_RESET:
		trap_Cvar_Set( "touch_profile", "1" );
		trap_Cvar_Set( "cl_aimAssist", "1" );
		trap_Cvar_Set( "touch_scale", "1" );
		trap_Cvar_Set( "touch_alpha", "0.7" );
		trap_Cvar_Set( "touch_lookSpeed", "0.3" );
		trap_Cvar_Set( "r_androidHeight", "720" );
		TouchControls_SetItems();
		break;
	}
}

static void TouchControls_DrawReset( void *self ) {
	menubitmap_s *b;

	b = (menubitmap_s *)self;
	UI_DKiButton( b->generic.x, b->generic.y + ( UI_TouchPressed( self ) ? 2.0f : 0.0f ), b->width, b->height, "RESET", NULL,
		UI_DFocus( &s_touch.menu, self ), qfalse, qfalse );
}

// the screen of a phone with the buttons of the touch layer
static void TouchControls_DrawPreview( void ) {
	float		s, scale, alpha, x, y, r;
	int			i;
	vec4_t		color;
	const tcButton_t *b;

	UI_DGlass( 498, 88, 320, 212 );
	UI_DProp( 516, 98, "PREVIEW", 10, UI_LEFT, zeq_gold );
	trap_R_SetColor( NULL );
	UI_DPic( TC_PREVIEW_X, TC_PREVIEW_Y, TC_PREVIEW_W, TC_PREVIEW_H, trap_R_RegisterShaderNoMip( "maps/lastStand.jpg" ) );
	color[0] = 0.02f; color[1] = 0.024f; color[2] = 0.10f; color[3] = 0.35f;
	UI_DFill( TC_PREVIEW_X, TC_PREVIEW_Y, TC_PREVIEW_W, TC_PREVIEW_H, color );
	color[0] = 0.47f; color[1] = 0.63f; color[2] = 1.0f; color[3] = 0.6f;
	UI_DRoundEdge( TC_PREVIEW_X, TC_PREVIEW_Y, TC_PREVIEW_W, TC_PREVIEW_H, 6, color );

	s = TC_PREVIEW_W / 844.0f;
	scale = Com_Clamp( 0.6f, 1.6f, s_touch.size.curvalue / 100.0f );
	alpha = Com_Clamp( 0.1f, 1.0f, s_touch.alpha.curvalue / 100.0f );

	// the stick where a thumb usually lands
	r = 62 * scale * s;
	color[0] = color[1] = color[2] = 1.0f; color[3] = 0.5f * alpha;
	UI_DRoundEdge( TC_PREVIEW_X + ( 24 + 70 ) * scale * s - r, TC_PREVIEW_Y + TC_PREVIEW_H - ( 24 + 70 ) * scale * s - r, 2 * r, 2 * r, r, color );

	for ( i = 0; i < (int)( sizeof( tcButtons ) / sizeof( tcButtons[0] ) ); i++ ) {
		b = &tcButtons[i];
		r = b->r * scale * s;
		x = b->middle ? TC_PREVIEW_X + TC_PREVIEW_W * 0.5f : TC_PREVIEW_X + TC_PREVIEW_W - b->x * scale * s;
		y = b->top ? TC_PREVIEW_Y + b->y * scale * s : TC_PREVIEW_Y + TC_PREVIEW_H - b->y * scale * s;
		color[0] = b->tint[0] * 0.35f; color[1] = b->tint[1] * 0.35f; color[2] = b->tint[2] * 0.35f; color[3] = 0.55f * alpha;
		UI_DRoundFill( x - r, y - r, 2 * r, 2 * r, r, color );
		color[0] = b->tint[0]; color[1] = b->tint[1]; color[2] = b->tint[2]; color[3] = alpha;
		UI_DRoundEdge( x - r, y - r, 2 * r, 2 * r, r, color );
		if ( b->label[0] ) {
			color[0] = color[1] = color[2] = 1.0f;
			UI_DProp( x, y - r * 0.4f, b->label, r * 0.8f, UI_CENTER, color );
		}
	}
	// VUELO, the two halves
	r = 27 * scale * s;
	x = TC_PREVIEW_X + TC_PREVIEW_W - 268 * scale * s;
	y = TC_PREVIEW_Y + TC_PREVIEW_H - ( 122 + 28 ) * scale * s;
	color[0] = color[1] = color[2] = 1.0f; color[3] = 0.7f * alpha;
	UI_DRoundEdge( x - r, y, 2 * r, ( 122 - 66 + 56 ) * scale * s, r, color );

	UI_DText( 516, 264, "KI slid left: second attack.", 12, UI_LEFT, zeq_text3 );
	UI_DText( 516, 280, "AURA slid left: lower ki.", 12, UI_LEFT, zeq_text3 );
}

static void TouchControls_Draw( void ) {
	uis.menuamount = -1;
	uis.hideEarth = qtrue;
	uis.showFrame = qfalse;

	UI_DMobileHeader( "MAIN MENU  >", "OPTIONS" );
	UI_DOptionsDrawPhone( tcHeightValues[s_touch.height.curvalue] != tcInitialHeight ?
		"The render height applies the next time the game starts." : NULL );
	UI_DOptionsDrawSections();
	TouchControls_DrawPreview();
	Menu_Draw( &s_touch.menu );
}

static void TouchControls_Tab( menutext_s *t, int id ) {
	t->generic.type		= MTYPE_PTEXT;
	t->generic.flags	= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	t->generic.id		= id;
	t->generic.callback	= TouchControls_Event;
	t->string			= "";
	t->color			= color_white;
	t->style			= UI_LEFT;
	Menu_AddItem( &s_touch.menu, t );
}

static void TouchControls_Spin( menulist_s *l, int id, const char **names ) {
	l->generic.type		= MTYPE_SPINCONTROL;
	l->generic.flags	= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	l->generic.id		= id;
	l->generic.callback	= TouchControls_Event;
	l->itemnames		= names;
	Menu_AddItem( &s_touch.menu, l );
}

static void TouchControls_Slider( menuslider_s *s, int id, float min, float max ) {
	s->generic.type		= MTYPE_SLIDER;
	s->generic.flags	= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s->generic.id		= id;
	s->generic.callback	= TouchControls_Event;
	s->minvalue			= min;
	s->maxvalue			= max;
	Menu_AddItem( &s_touch.menu, s );
}

static void TouchControls_MenuInit( void ) {
	memset( &s_touch, 0, sizeof( s_touch ) );
	s_touch.menu.wrapAround = qtrue;
	s_touch.menu.fullscreen = qtrue;
	s_touch.menu.draw = TouchControls_Draw;
	s_touch.menu.mobile = qtrue;
	UI_DCanvas( qtrue );

	TouchControls_Tab( &s_touch.controls, ID_TC_CONTROLS );
	TouchControls_Tab( &s_touch.system, ID_TC_SYSTEM );
	TouchControls_Tab( &s_touch.general, ID_TC_GENERAL );
	TouchControls_Spin( &s_touch.profile, ID_TC_PROFILE, tcProfiles );
	s_touch.aim.generic.type		= MTYPE_RADIOBUTTON;
	s_touch.aim.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_touch.aim.generic.id			= ID_TC_AIM;
	s_touch.aim.generic.callback	= TouchControls_Event;
	Menu_AddItem( &s_touch.menu, &s_touch.aim );
	TouchControls_Slider( &s_touch.size, ID_TC_SIZE, 60, 160 );
	TouchControls_Slider( &s_touch.alpha, ID_TC_ALPHA, 20, 100 );
	TouchControls_Slider( &s_touch.look, ID_TC_LOOK, 10, 80 );
	TouchControls_Spin( &s_touch.height, ID_TC_HEIGHT, tcHeights );

	s_touch.reset.generic.type		= MTYPE_BITMAP;
	s_touch.reset.generic.flags		= QMF_NODEFAULTINIT;
	s_touch.reset.generic.id		= ID_TC_RESET;
	s_touch.reset.generic.callback	= TouchControls_Event;
	s_touch.reset.generic.ownerdraw	= TouchControls_DrawReset;
	UI_DItem( &s_touch.reset, 498, 312, 150, 52 );
	Menu_AddItem( &s_touch.menu, &s_touch.reset );
	UI_DBackItem( &s_touch.menu );

	// tabs, rows: the options layer of the design canvas
	UI_DTabItem( &s_touch.controls, 0, "CONTROLS", qtrue );
	UI_DTabItem( &s_touch.system, 1, "VIDEO & AUDIO", qfalse );
	UI_DTabItem( &s_touch.general, 2, "GAME", qfalse );
	UI_DOptionsBegin( &s_touch.menu, 0, 0, 0, 0 );
	UI_DOptionsPhoneArea( 34, 444, 356 );
	UI_DOptionsSection( "PLAY" );
	UI_DOptionsRow( &s_touch.profile, "Profile", "Assisted: magnet, auto lock; in melee hold KI to launch.", qfalse );
	UI_DOptionsRow( &s_touch.aim, "Aim assist", "Slows the view near a rival and bends shots to it.", qfalse );
	UI_DOptionsRow( &s_touch.look, "Look speed", "How fast a drag on the right half turns the view.", qfalse );
	UI_DOptionsSection( "BUTTONS" );
	UI_DOptionsRow( &s_touch.size, "Button size", "Size of every button, in percent.", qfalse );
	UI_DOptionsRow( &s_touch.alpha, "Opacity", "How solid the buttons are drawn.", qfalse );
	UI_DOptionsSection( "SCREEN" );
	UI_DOptionsRow( &s_touch.height, "Render height", "Lower is faster; the screen scales it up.", qfalse );

	TouchControls_SetItems();
	tcInitialHeight = (int)trap_Cvar_VariableValue( "r_androidHeight" );
}

void UI_TouchControlsMenu( void ) {
	TouchControls_MenuInit();
	UI_PushMenu( &s_touch.menu );
	uis.menuamount = -1;
	uis.hideEarth = qtrue;
	uis.showFrame = qfalse;
}
