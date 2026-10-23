#include "ui_local.h"

/*
=======================================================================

MENU REDESIGN: shared colors, easing, screen transitions and widgets

All coordinates are 640x480 virtual units. Every draw call goes through
UI_FadeSetColor (see the end of this file), so a whole screen fades in
by changing uis.fadeAlpha while it is drawn.

=======================================================================
*/

vec4_t zeq_white	= { 1.00f, 1.00f, 1.00f, 1.00f };
vec4_t zeq_text2	= { 0.68f, 0.71f, 0.85f, 1.00f };
vec4_t zeq_text3	= { 0.56f, 0.59f, 0.74f, 1.00f };
vec4_t zeq_gold		= { 0.95f, 0.70f, 0.24f, 1.00f };
vec4_t zeq_ki		= { 0.33f, 0.59f, 1.00f, 1.00f };

#define SIDE_FIRST_Y		112
#define SIDE_STEP			38
#define SIDE_HEIGHT			34
#define SIDE_LABEL_Y		119

#define TRANSITION_MS		320
#define TRANSITION_SLIDE	28.0f

static const int sideButtonWidths[6] = { 124, 109, 102, 102, 109, 124 };
static qhandle_t sideButtonShaders[6];
static qhandle_t sideButtonGlows[6];

/*
=================
UI_EaseOut

cubic ease out, t in 0..1
=================
*/
float UI_EaseOut( float t ) {
	float inv;

	if ( t <= 0.0f ) {
		return 0.0f;
	}
	if ( t >= 1.0f ) {
		return 1.0f;
	}
	inv = 1.0f - t;
	return 1.0f - inv * inv * inv;
}

/*
=================
UI_AnimFrac

eased progress of an animation that starts delay ms after startTime
=================
*/
float UI_AnimFrac( int startTime, int delay, int duration ) {
	float t;

	if ( duration <= 0 ) {
		return 1.0f;
	}
	t = (float)( uis.realtime - startTime - delay ) / (float)duration;
	return UI_EaseOut( t );
}

static float UI_LeftBias( void ) {
	return ( uis.scale > 0.0f ) ? ( uis.bias / uis.scale ) : 0.0f;
}

/*
=================
UI_BeginContentFade / UI_EndContentFade

the active screen fades in and slides from the side it came from
=================
*/
void UI_BeginContentFade( void ) {
	float t;

	t = UI_AnimFrac( uis.transitionTime, 0, TRANSITION_MS );
	if ( t >= 1.0f ) {
		uis.fadeAlpha = 1.0f;
		uis.drawOffsetX = 0.0f;
	}
	else {
		uis.fadeAlpha = 0.01f + 0.99f * t;
		uis.drawOffsetX = uis.transitionDir * TRANSITION_SLIDE * ( 1.0f - t );
	}
	trap_R_SetColor( NULL );
}

void UI_EndContentFade( void ) {
	uis.fadeAlpha = 1.0f;
	uis.drawOffsetX = 0.0f;
	trap_R_SetColor( NULL );
}

/*
=================
Side buttons

The PTEXT labels at x 28 with the tiny font sit on the slanted buttons
drawn by Menu_Common. Both use the same offset so they move together.
=================
*/
qboolean UI_IsSideButton( menucommon_s *item ) {
	if ( !item || item->type != MTYPE_PTEXT ) {
		return qfalse;
	}
	if ( !( ((menutext_s *)item)->style & UI_TINYFONT ) ) {
		return qfalse;
	}
	return ( item->x <= 40 && item->y >= SIDE_LABEL_Y && item->y < SIDE_LABEL_Y + SIDE_STEP * 6 );
}

static int UI_SideSlot( menucommon_s *item ) {
	return ( item->y - SIDE_LABEL_Y + SIDE_STEP / 2 ) / SIDE_STEP;
}

static int UI_FocusedSideSlot( void ) {
	menucommon_s *item;

	if ( !uis.activemenu ) {
		return -1;
	}
	item = (menucommon_s *)Menu_ItemAtCursor( uis.activemenu );
	if ( !UI_IsSideButton( item ) ) {
		return -1;
	}
	return UI_SideSlot( item );
}

static float UI_SlotOffset( int slot ) {
	float enter;
	float offset;

	// staggered entrance, 45 ms apart
	enter = UI_AnimFrac( uis.transitionTime, slot * 45, 280 );
	offset = -36.0f * ( 1.0f - enter );
	if ( slot == UI_FocusedSideSlot() ) {
		offset += 7.0f * UI_AnimFrac( uis.focusTime, 0, 160 );
	}
	return offset;
}

float UI_SideButtonOffset( menucommon_s *item ) {
	return UI_SlotOffset( UI_SideSlot( item ) );
}

/*
=================
Menu_Common

logo plus the animated side buttons
=================
*/
void Menu_Common( int amount ) {
	int		i;
	float	x, y;
	float	baseOffset;
	float	enter;
	float	pulse;
	int		focusSlot;
	float	targetY;
	float	follow;
	vec4_t	color;

	baseOffset = uis.drawOffsetX;

	// the logo stays put, it is the same on every screen
	uis.drawOffsetX = 0.0f;
	UI_MenuLogo();
	uis.drawOffsetX = baseOffset;

	if ( amount > 6 ) {
		amount = 6;
	}

	for ( i = 0; i < 6; i++ ) {
		if ( !sideButtonShaders[i] ) {
			sideButtonShaders[i] = trap_R_RegisterShaderNoMip( va( "interface/menu/mainButton%i.png", i + 1 ) );
			sideButtonGlows[i] = trap_R_RegisterShaderNoMip( va( "menuKiGlow%i", i + 1 ) );
		}
	}

	focusSlot = UI_FocusedSideSlot();
	x = 16.0f - UI_LeftBias();

	for ( i = 0; i < amount; i++ ) {
		y = SIDE_FIRST_Y + i * SIDE_STEP;
		enter = UI_AnimFrac( uis.transitionTime, i * 45, 280 );

		uis.drawOffsetX = baseOffset + UI_SlotOffset( i );
		color[0] = color[1] = color[2] = 1.0f;
		color[3] = enter;
		trap_R_SetColor( color );
		UI_DrawHandlePic( x, y, sideButtonWidths[i], SIDE_HEIGHT, sideButtonShaders[i] );

		if ( i == focusSlot ) {
			pulse = 0.75f + 0.25f * sin( uis.realtime / 220.0f );
			color[0] = zeq_ki[0];
			color[1] = zeq_ki[1];
			color[2] = zeq_ki[2];
			color[3] = enter * pulse * UI_AnimFrac( uis.focusTime, 0, 180 );
			trap_R_SetColor( color );
			UI_DrawHandlePic( x, y, sideButtonWidths[i], SIDE_HEIGHT, sideButtonGlows[i] );
			UI_DrawHandlePic( x, y, sideButtonWidths[i], SIDE_HEIGHT, sideButtonGlows[i] );
		}
		trap_R_SetColor( NULL );
	}
	uis.drawOffsetX = baseOffset;

	// the dragon ball marker glides to the focused button
	if ( focusSlot >= 0 && focusSlot < amount ) {
		targetY = SIDE_FIRST_Y + focusSlot * SIDE_STEP + SIDE_HEIGHT * 0.5f;
		if ( !uis.markerValid ) {
			uis.markerY = targetY;
			uis.markerValid = qtrue;
		}
		follow = uis.frametime / 70.0f;
		if ( follow > 1.0f ) {
			follow = 1.0f;
		}
		uis.markerY += ( targetY - uis.markerY ) * follow;

		uis.drawOffsetX = baseOffset + UI_SlotOffset( focusSlot );
		UI_DrawDragonBall( x + sideButtonWidths[focusSlot] + 4.0f, uis.markerY - 7.0f, 14.0f );
		uis.drawOffsetX = baseOffset;
	}
	else {
		uis.markerValid = qfalse;
	}
}

void Menu_Frame( void ) {
	UI_DrawHandlePic( 147, 113, 367, 310, uis.menuFrame );
}

void Menu_Side( void ) {}

/*
=================
UI_DrawGlassPanel

menuFrame.png as a nine slice: the corners keep their shape at any size
=================
*/
void UI_DrawGlassPanel( float x, float y, float w, float h ) {
	float	c;
	float	cs, ct;
	float	ax, ay, aw, ah, ac;
	float	xs[4], ys[4];
	float	ss[4], ts[4];
	int		i, j;

	c = 12.0f;
	cs = 40.0f / 587.0f;
	ct = 40.0f / 496.0f;

	ax = x; ay = y; aw = w; ah = h; ac = c;
	UI_AdjustFrom640( &ax, &ay, &aw, &ah );
	ac = c * uis.scale;

	xs[0] = ax; xs[1] = ax + ac; xs[2] = ax + aw - ac; xs[3] = ax + aw;
	ys[0] = ay; ys[1] = ay + ac; ys[2] = ay + ah - ac; ys[3] = ay + ah;
	ss[0] = 0; ss[1] = cs; ss[2] = 1.0f - cs; ss[3] = 1.0f;
	ts[0] = 0; ts[1] = ct; ts[2] = 1.0f - ct; ts[3] = 1.0f;

	trap_R_SetColor( NULL );
	for ( j = 0; j < 3; j++ ) {
		for ( i = 0; i < 3; i++ ) {
			trap_R_DrawStretchPic( xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j],
				ss[i], ts[j], ss[i + 1], ts[j + 1], uis.glassPanel );
		}
	}
}

/*
=================
UI_DrawKiBar

wide slanted button used inside panels (main button art stretched)
=================
*/
void UI_DrawKiBar( float x, float y, float w, float h, qboolean focus, float focusFrac ) {
	vec4_t color;

	trap_R_SetColor( NULL );
	UI_DrawHandlePic( x, y, w, h, uis.kiButton );
	if ( focus ) {
		color[0] = zeq_ki[0];
		color[1] = zeq_ki[1];
		color[2] = zeq_ki[2];
		color[3] = focusFrac * ( 0.75f + 0.25f * sin( uis.realtime / 220.0f ) );
		trap_R_SetColor( color );
		UI_DrawHandlePic( x, y, w, h, uis.kiButtonGlow );
		UI_DrawHandlePic( x, y, w, h, uis.kiButtonGlow );
		trap_R_SetColor( NULL );
	}
}

/*
=================
UI_DrawDragonBall

the four star ball from the confirm frame, with a soft bob
=================
*/
void UI_DrawDragonBall( float x, float y, float size ) {
	float bob;

	bob = 1.2f * sin( uis.realtime / 300.0f );
	trap_R_SetColor( NULL );
	UI_DrawHandlePic( x, y + bob, size, size * 1.2f, uis.dragonBall );
}

/*
=================
UI_DrawPips

skill level, the pips light up one after the other while anim goes 0..1
=================
*/
void UI_DrawPips( float x, float y, int filled, int total, float anim ) {
	int		i;
	float	lit;
	vec4_t	off;
	vec4_t	on;

	off[0] = 0.56f; off[1] = 0.59f; off[2] = 0.74f; off[3] = 0.35f;
	for ( i = 0; i < total; i++ ) {
		UI_FillRect( x + i * 9, y, 7, 4, off );
		if ( i < filled ) {
			lit = anim * total - i;
			if ( lit > 1.0f ) {
				lit = 1.0f;
			}
			if ( lit > 0.0f ) {
				on[0] = zeq_gold[0];
				on[1] = zeq_gold[1];
				on[2] = zeq_gold[2];
				on[3] = lit;
				UI_FillRect( x + i * 9, y, 7, 4, on );
			}
		}
	}
}

/*
=================
UI_DrawStatBar

style value 0..1 as a blue bar that grows with anim
=================
*/
void UI_DrawStatBar( float x, float y, float w, const char *label, float value, float anim ) {
	float	trackX;
	float	trackW;
	vec4_t	track;
	vec4_t	fill;
	vec4_t	shine;

	track[0] = 0.02f; track[1] = 0.03f; track[2] = 0.12f; track[3] = 0.9f;
	fill[0] = 0.18f; fill[1] = 0.37f; fill[2] = 0.82f; fill[3] = 1.0f;
	shine[0] = 0.47f; shine[1] = 0.70f; shine[2] = 1.0f; shine[3] = 1.0f;

	trackX = x + 52;
	trackW = w - 52 - 16;

	UI_DrawStringSized( x, y - 2, label, UI_LEFT, zeq_text2, 6, 10 );
	UI_FillRect( trackX, y, trackW, 5, track );
	if ( value * anim > 0.0f ) {
		UI_FillRect( trackX, y, trackW * value * anim, 5, fill );
		UI_FillRect( trackX, y, trackW * value * anim, 2, shine );
	}
	UI_DrawStringSized( x + w, y - 2, va( "%i", (int)( value * 100.0f + 0.5f ) ), UI_RIGHT, zeq_white, 6, 10 );
}

/*
=================
UI_DrawKeyHint

a key cap followed by its action, returns the width used
=================
*/
float UI_DrawKeyHint( float x, float y, const char *key, const char *label ) {
	float	capW;
	float	labelW;
	vec4_t	capFill;
	vec4_t	capEdge;

	capFill[0] = 0.07f; capFill[1] = 0.09f; capFill[2] = 0.26f; capFill[3] = 0.85f;
	capEdge[0] = 0.29f; capEdge[1] = 0.39f; capEdge[2] = 0.74f; capEdge[3] = 1.0f;

	capW = UI_StringSizedWidth( key, 6 ) + 8;
	if ( capW < 16 ) {
		capW = 16;
	}
	UI_FillRect( x, y, capW, 13, capFill );
	UI_DrawRect( x, y, capW, 13, capEdge );
	UI_DrawStringSized( x + capW * 0.5f, y + 1, key, UI_CENTER, zeq_white, 6, 11 );

	labelW = UI_StringSizedWidth( label, 7 );
	UI_DrawStringSized( x + capW + 4, y, label, UI_LEFT|UI_DROPSHADOW, zeq_text2, 7, 13 );
	return capW + 4 + labelW + 14;
}

/*
=================
UI_DrawKeyHints

footer at the bottom left: pairs = key, label, key, label...
=================
*/
void UI_DrawKeyHints( const char **pairs, int count ) {
	int		i;
	float	x;

	x = 22.0f - UI_LeftBias();
	for ( i = 0; i < count; i++ ) {
		x += UI_DrawKeyHint( x, 458, pairs[i * 2], pairs[i * 2 + 1] );
	}
}

void UI_DrawSectionTitle( float x, float y, const char *text ) {
	UI_DrawPropScaled( x, y, text, UI_LEFT, zeq_gold, 0.42f );
}

/*
=================
UI_DrawPortrait

character icon; the selected one gets the gold ring, focus grows it a little
=================
*/
void UI_DrawPortrait( float x, float y, float size, qhandle_t shader, qboolean selected, float focusFrac ) {
	float	grow;
	vec4_t	ring;
	vec4_t	dim;

	grow = 3.0f * focusFrac;
	x -= grow * 0.5f;
	y -= grow * 0.5f;
	size += grow;

	if ( selected ) {
		ring[0] = zeq_gold[0];
		ring[1] = zeq_gold[1];
		ring[2] = zeq_gold[2];
		ring[3] = 0.35f + 0.15f * sin( uis.realtime / 250.0f );
		UI_FillRect( x - 3, y - 3, size + 6, size + 6, ring );
		ring[3] = 1.0f;
		UI_DrawRect( x - 2, y - 2, size + 4, size + 4, ring );
		UI_DrawRect( x - 1, y - 1, size + 2, size + 2, ring );
	}

	if ( selected || focusFrac > 0.0f ) {
		trap_R_SetColor( NULL );
	}
	else {
		dim[0] = dim[1] = dim[2] = 0.85f;
		dim[3] = 1.0f;
		trap_R_SetColor( dim );
	}
	UI_DrawHandlePic( x, y, size, size, shader );
	trap_R_SetColor( NULL );
}

/*
=================
UI_DrawFocusRow

ki tinted highlight behind a focused list row
=================
*/
void UI_DrawFocusRow( float x, float y, float w, float h, float focusFrac ) {
	vec4_t	fill;
	vec4_t	edge;

	if ( focusFrac <= 0.0f ) {
		return;
	}
	fill[0] = zeq_ki[0];
	fill[1] = zeq_ki[1];
	fill[2] = zeq_ki[2];
	fill[3] = 0.22f * focusFrac;
	UI_FillRect( x, y, w * ( 0.6f + 0.4f * focusFrac ), h, fill );
	edge[0] = 0.47f; edge[1] = 0.63f; edge[2] = 1.0f; edge[3] = 0.55f * focusFrac;
	UI_DrawRect( x, y, w, h, edge );
	UI_FillRect( x, y, 2, h, zeq_gold );
}

/*
=================
UI_FadeSetColor

The wrapper every trap_R_SetColor call in the UI goes through.
It must stay at the end of the file: below this point the name is the
real syscall again.
=================
*/
#undef trap_R_SetColor

void UI_FadeSetColor( const float *rgba ) {
	vec4_t c;

	if ( uis.fadeAlpha <= 0.0f || uis.fadeAlpha >= 1.0f ) {
		trap_R_SetColor( rgba );
		return;
	}
	if ( rgba ) {
		c[0] = rgba[0];
		c[1] = rgba[1];
		c[2] = rgba[2];
		c[3] = rgba[3] * uis.fadeAlpha;
	}
	else {
		c[0] = c[1] = c[2] = 1.0f;
		c[3] = uis.fadeAlpha;
	}
	trap_R_SetColor( c );
}
