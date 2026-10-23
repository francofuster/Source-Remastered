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

	// screens laid out on the design canvas draw their own header
	if ( amount < 0 ) {
		return;
	}

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

// button art: texture size and the end caps that must keep their shape
#define KI_ART		199, 54, 10, 29
#define TAB_ART		163, 54, 8, 8
#define CTA_ART		400, 108, 12, 42

/*
=================
UI_PicSliced

button art in three slices: the end caps keep the proportions of the art
(scaled by the height) and only the middle stretches, so the slanted edge
keeps its angle at any width or aspect ratio
=================
*/
static void UI_PicSliced( float x, float y, float w, float h, qhandle_t shader, float texW, float texH, float capL, float capR ) {
	float	lw, rw;

	lw = capL * h / texH;
	rw = capR * h / texH;
	if ( lw + rw >= w ) {
		UI_DrawHandlePic( x, y, w, h, shader );
		return;
	}
	UI_AdjustFrom640( &x, &y, &w, &h );
	lw = capL * h / texH;
	rw = capR * h / texH;
	trap_R_DrawStretchPic( x, y, lw, h, 0, 0, capL / texW, 1, shader );
	trap_R_DrawStretchPic( x + lw, y, w - lw - rw, h, capL / texW, 0, 1.0f - capR / texW, 1, shader );
	trap_R_DrawStretchPic( x + w - rw, y, rw, h, 1.0f - capR / texW, 0, 1, 1, shader );
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
	UI_PicSliced( x, y, w, h, uis.kiButton, KI_ART );
	if ( focus ) {
		color[0] = zeq_ki[0];
		color[1] = zeq_ki[1];
		color[2] = zeq_ki[2];
		color[3] = focusFrac * ( 0.75f + 0.25f * sin( uis.realtime / 220.0f ) );
		trap_R_SetColor( color );
		UI_PicSliced( x, y, w, h, uis.kiButtonGlow, KI_ART );
		UI_PicSliced( x, y, w, h, uis.kiButtonGlow, KI_ART );
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

footer centered at the bottom, clear of the HUD in the corners:
pairs = key, label, key, label...
=================
*/
void UI_DrawKeyHints( const char **pairs, int count ) {
	int		i;
	float	x;
	float	capW;
	float	total;

	total = 0.0f;
	for ( i = 0; i < count; i++ ) {
		capW = UI_StringSizedWidth( pairs[i * 2], 6 ) + 8;
		if ( capW < 16 ) {
			capW = 16;
		}
		total += capW + 4 + UI_StringSizedWidth( pairs[i * 2 + 1], 7 ) + 14;
	}
	x = 320.0f - ( total - 14 ) * 0.5f;
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
=======================================================================

DESIGN LAYER

Screens of the redesign are laid out on the same 1280x720 canvas as the
design artboards. The canvas is fitted to the real screen (letterboxed
when the aspect differs) and converted to the 640x480 virtual units the
rest of the UI uses. Sizes of text are line heights in design pixels.

=======================================================================
*/

vec4_t zeq_shade	= { 0.02f, 0.024f, 0.10f, 0.35f };

static qhandle_t	d_logo;
static qhandle_t	d_cta;
static qhandle_t	d_ctaGlow;
static qhandle_t	d_tab;
static qhandle_t	d_tabGlow;
static qhandle_t	d_nameArea;
static qhandle_t	d_switchOn;
static qhandle_t	d_switchOff;
static qhandle_t	d_sliderKnob;
static qhandle_t	d_round;
static qhandle_t	d_roundEdge;
static qhandle_t	d_roundMask;
static qhandle_t	d_roundGlow;

static void UI_DRegister( void ) {
	if ( d_logo ) {
		return;
	}
	d_logo			= trap_R_RegisterShaderNoMip( "interface/menu/zeq2litelogo.png" );
	d_cta			= trap_R_RegisterShaderNoMip( "interface/menu/ctaButton.png" );
	d_round			= trap_R_RegisterShaderNoMip( "interface/menu/round.png" );
	d_roundEdge		= trap_R_RegisterShaderNoMip( "interface/menu/roundEdge.png" );
	d_roundMask		= trap_R_RegisterShaderNoMip( "interface/menu/roundMask.png" );
	d_roundGlow		= trap_R_RegisterShaderNoMip( "interface/menu/roundGlow.png" );
	d_ctaGlow		= trap_R_RegisterShaderNoMip( "menuCtaGlow" );
	d_tab			= trap_R_RegisterShaderNoMip( "interface/menu/mainButton3.png" );
	d_tabGlow		= trap_R_RegisterShaderNoMip( "menuKiGlow3" );
	d_nameArea		= trap_R_RegisterShaderNoMip( "interface/menu/nameArea.png" );
	d_switchOn		= trap_R_RegisterShaderNoMip( "interface/art/switch_on.png" );
	d_switchOff		= trap_R_RegisterShaderNoMip( "interface/art/switch_off.png" );
	d_sliderKnob	= trap_R_RegisterShaderNoMip( "interface/art/sliderbutt_0.png" );
}

/*
=================
UI_DScale / UI_DToV

design pixels to virtual units
=================
*/
static void UI_DFit( float *s, float *offX, float *offY ) {
	float sx, sy;

	sx = uis.glconfig.vidWidth / 1280.0f;
	sy = uis.glconfig.vidHeight / 720.0f;
	*s = ( sx < sy ) ? sx : sy;
	*offX = ( uis.glconfig.vidWidth - 1280.0f * *s ) * 0.5f;
	*offY = ( uis.glconfig.vidHeight - 720.0f * *s ) * 0.5f;
}

/*
=================
UI_DExtra

design pixels between the canvas and each side of a screen wider than
16:9 (21:9 and up). Header, hints and the main buttons move out by it so
they sit on the edges like on 16:9; panels stay centered.
=================
*/
float UI_DExtra( void ) {
	float s, ox, oy;

	UI_DFit( &s, &ox, &oy );
	return ( s > 0.0f ) ? ox / s : 0.0f;
}

float UI_DScale( void ) {
	float s, ox, oy;

	UI_DFit( &s, &ox, &oy );
	return ( uis.scale > 0.0f ) ? s / uis.scale : 0.5f;
}

void UI_DToV( float *x, float *y, float *w, float *h ) {
	float s, ox, oy;

	UI_DFit( &s, &ox, &oy );
	if ( uis.scale <= 0.0f ) {
		return;
	}
	*x = ( ox + *x * s - uis.bias ) / uis.scale;
	*y = ( oy + *y * s ) / uis.scale;
	*w = *w * s / uis.scale;
	*h = *h * s / uis.scale;
}

/*
=================
UI_DItem

places a bitmap item on the design canvas: the design rect is kept in
x, y, width and height for its ownerdraw, the hit box in virtual units
=================
*/
void UI_DItem( menubitmap_s *b, int x, int y, int w, int h ) {
	float vx, vy, vw, vh;

	b->generic.x = x;
	b->generic.y = y;
	b->width = w;
	b->height = h;
	vx = x; vy = y; vw = w; vh = h;
	UI_DToV( &vx, &vy, &vw, &vh );
	b->generic.left = vx;
	b->generic.top = vy;
	b->generic.right = vx + vw;
	b->generic.bottom = vy + vh;
}

void UI_DPicST( float x, float y, float w, float h, float s0, float t0, float s1, float t1, qhandle_t shader ) {
	UI_DToV( &x, &y, &w, &h );
	UI_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, w, h, s0, t0, s1, t1, shader );
}

void UI_DPic( float x, float y, float w, float h, qhandle_t shader ) {
	UI_DPicST( x, y, w, h, 0, 0, 1, 1, shader );
}

// UI_PicSliced on the design canvas
void UI_DPicSliced( float x, float y, float w, float h, qhandle_t shader, float texW, float texH, float capL, float capR ) {
	float	lw, rw;

	lw = capL * h / texH;
	rw = capR * h / texH;
	if ( lw + rw >= w ) {
		UI_DPic( x, y, w, h, shader );
		return;
	}
	UI_DPicST( x, y, lw, h, 0, 0, capL / texW, 1, shader );
	UI_DPicST( x + lw, y, w - lw - rw, h, capL / texW, 0, 1.0f - capR / texW, 1, shader );
	UI_DPicST( x + w - rw, y, rw, h, 1.0f - capR / texW, 0, 1, 1, shader );
}

void UI_DFill( float x, float y, float w, float h, const float *color ) {
	trap_R_SetColor( color );
	UI_DPic( x, y, w, h, uis.whiteShader );
	trap_R_SetColor( NULL );
}

void UI_DFrame( float x, float y, float w, float h, const float *color ) {
	UI_DFill( x, y, w, 1.5f, color );
	UI_DFill( x, y + h - 1.5f, w, 1.5f, color );
	UI_DFill( x, y, 1.5f, h, color );
	UI_DFill( x + w - 1.5f, y, 1.5f, h, color );
}

void UI_DGlass( float x, float y, float w, float h ) {
	float	c, cs, ct;
	float	xs[4], ys[4], ss[4], ts[4];
	int		i, j;

	c = 24.0f;
	cs = 40.0f / 587.0f;
	ct = 40.0f / 496.0f;
	xs[0] = x; xs[1] = x + c; xs[2] = x + w - c; xs[3] = x + w;
	ys[0] = y; ys[1] = y + c; ys[2] = y + h - c; ys[3] = y + h;
	ss[0] = 0; ss[1] = cs; ss[2] = 1.0f - cs; ss[3] = 1.0f;
	ts[0] = 0; ts[1] = ct; ts[2] = 1.0f - ct; ts[3] = 1.0f;
	trap_R_SetColor( NULL );
	for ( j = 0; j < 3; j++ ) {
		for ( i = 0; i < 3; i++ ) {
			UI_DPicST( xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j], ss[i], ts[j], ss[i + 1], ts[j + 1], uis.glassPanel );
		}
	}
}

/*
=================
Text on the design canvas

UI_DText: condensed console font (font0), size = line height
UI_DProp: the title font (font1), size = cell height
=================
*/
float UI_DTextWidth( const char *str, float size ) {
	return UI_DrawStrlen( str ) * size * 0.40f;
}

void UI_DText( float x, float y, const char *str, float size, int style, vec4_t color ) {
	float k;

	if ( !str ) {
		return;
	}
	k = UI_DScale();
	switch ( style & UI_FORMATMASK ) {
	case UI_CENTER:
		x -= UI_DTextWidth( str, size ) * 0.5f;
		break;
	case UI_RIGHT:
		x -= UI_DTextWidth( str, size );
		break;
	default:
		break;
	}
	{
		float vx, vy, vw, vh;
		vx = x; vy = y; vw = 1; vh = 1;
		UI_DToV( &vx, &vy, &vw, &vh );
		UI_DrawStringSized( vx, vy, str, ( style & ~UI_FORMATMASK ), color, size * 0.58f * k, size * k );
	}
}

float UI_DPropWidth( const char *str, float size ) {
	return UI_ProportionalStringWidth( str ) * size / (float)PROP_HEIGHT;
}

void UI_DProp( float x, float y, const char *str, float size, int style, vec4_t color ) {
	float vx, vy, vw, vh;

	switch ( style & UI_FORMATMASK ) {
	case UI_CENTER:
		x -= UI_DPropWidth( str, size ) * 0.5f;
		break;
	case UI_RIGHT:
		x -= UI_DPropWidth( str, size );
		break;
	default:
		break;
	}
	vx = x; vy = y; vw = 1; vh = 1;
	UI_DToV( &vx, &vy, &vw, &vh );
	UI_DrawPropScaled( vx, vy, str, style & ~UI_FORMATMASK, color, size / (float)PROP_HEIGHT * UI_DScale() );
}

/*
=================
Pieces of the redesign
=================
*/
void UI_DLogo( float x, float y, float w ) {
	UI_DRegister();
	trap_R_SetColor( NULL );
	UI_DPicST( x, y, w, w * 364.0f / 719.0f, 155.0f / 1024.0f, 194.0f / 768.0f, 874.0f / 1024.0f, 558.0f / 768.0f, d_logo );
}

// logo, divider, breadcrumb and title of a sub screen
void UI_DHeader( const char *crumb, const char *title ) {
	vec4_t line;

	float	e;

	e = UI_DExtra();
	line[0] = 0.35f; line[1] = 0.51f; line[2] = 0.90f; line[3] = 0.45f;
	UI_DLogo( 40 - e, 22, 170 );
	UI_DFill( 236 - e, 34, 1.5f, 62, line );
	UI_DText( 262 - e, 38, crumb, 18, UI_LEFT, zeq_text3 );
	UI_DProp( 262 - e, 58, title, 34, UI_LEFT|UI_DROPSHADOW, zeq_white );
}

void UI_DSectionTitle( float x, float y, const char *text ) {
	UI_DProp( x, y, text, 17, UI_LEFT, zeq_gold );
}

// a key cap and its action; returns the width used
float UI_DKeyHint( float x, float y, const char *key, const char *label ) {
	float	capW;
	vec4_t	capFill;
	vec4_t	capEdge;

	capFill[0] = 0.07f; capFill[1] = 0.09f; capFill[2] = 0.26f; capFill[3] = 0.85f;
	capEdge[0] = 0.29f; capEdge[1] = 0.39f; capEdge[2] = 0.74f; capEdge[3] = 1.0f;
	capW = UI_DTextWidth( key, 15 ) + 16;
	if ( capW < 30 ) {
		capW = 30;
	}
	UI_DFill( x, y, capW, 26, capFill );
	UI_DFrame( x, y, capW, 26, capEdge );
	UI_DText( x + capW * 0.5f, y + 5, key, 15, UI_CENTER, zeq_white );
	UI_DText( x + capW + 8, y + 3, label, 19, UI_LEFT|UI_DROPSHADOW, zeq_text2 );
	return capW + 8 + UI_DTextWidth( label, 19 ) + 26;
}


/*
=================
Rounded shapes

white rounded rect art (round.png, radius 24 of 64 px) drawn in nine
slices and tinted: fills, 1.5 px outlines, soft glows and corner masks
that round off square pictures (portraits) over a dark panel
=================
*/
#define ROUND_C		( 24.0f / 64.0f )

static void UI_DRound9( float x, float y, float w, float h, float r, float c, qhandle_t shader, const float *color ) {
	float	xs[4], ys[4], ss[4];
	int		i, j;

	if ( r * 2.0f > w ) {
		r = w * 0.5f;
	}
	if ( r * 2.0f > h ) {
		r = h * 0.5f;
	}
	xs[0] = x; xs[1] = x + r; xs[2] = x + w - r; xs[3] = x + w;
	ys[0] = y; ys[1] = y + r; ys[2] = y + h - r; ys[3] = y + h;
	ss[0] = 0; ss[1] = c; ss[2] = 1.0f - c; ss[3] = 1.0f;
	trap_R_SetColor( color );
	for ( j = 0; j < 3; j++ ) {
		for ( i = 0; i < 3; i++ ) {
			UI_DPicST( xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j], ss[i], ss[j], ss[i + 1], ss[j + 1], shader );
		}
	}
	trap_R_SetColor( NULL );
}

void UI_DRoundFill( float x, float y, float w, float h, float r, const float *color ) {
	UI_DRegister();
	UI_DRound9( x, y, w, h, r, ROUND_C, d_round, color );
}

// the stroke is 1.5 px at r 12, it scales with r
void UI_DRoundEdge( float x, float y, float w, float h, float r, const float *color ) {
	UI_DRegister();
	UI_DRound9( x, y, w, h, r, ROUND_C, d_roundEdge, color );
}

// box-shadow like glow spreading about spread px out of the rect
void UI_DRoundGlow( float x, float y, float w, float h, float spread, const float *color ) {
	float k;

	UI_DRegister();
	k = spread / 32.0f;
	UI_DRound9( x - 32.0f * k, y - 32.0f * k, w + 64.0f * k, h + 64.0f * k, 56.0f * k, 56.0f / 128.0f, d_roundGlow, color );
}

// paints the four corners outside a rounded rect, in the color behind it
void UI_DRoundCorners( float x, float y, float w, float h, float r, const float *color ) {
	UI_DRegister();
	trap_R_SetColor( color );
	UI_DPicST( x, y, r, r, 0, 0, ROUND_C, ROUND_C, d_roundMask );
	UI_DPicST( x + w - r, y, r, r, 1.0f - ROUND_C, 0, 1, ROUND_C, d_roundMask );
	UI_DPicST( x, y + h - r, r, r, 0, 1.0f - ROUND_C, ROUND_C, 1, d_roundMask );
	UI_DPicST( x + w - r, y + h - r, r, r, 1.0f - ROUND_C, 1.0f - ROUND_C, 1, 1, d_roundMask );
	trap_R_SetColor( NULL );
}

static void UI_DGradColor( vec4_t out, float t, const float *top, const float *mid, float midStop, const float *bottom ) {
	const float	*a, *b;
	float		f;
	int			i;

	if ( t < midStop ) {
		a = top; b = mid; f = ( midStop > 0.0f ) ? t / midStop : 1.0f;
	}
	else {
		a = mid; b = bottom; f = ( midStop < 1.0f ) ? ( t - midStop ) / ( 1.0f - midStop ) : 1.0f;
	}
	for ( i = 0; i < 4; i++ ) {
		out[i] = a[i] + ( b[i] - a[i] ) * f;
	}
}

/*
=================
UI_DRoundPanel

rounded panel with a vertical gradient (top, mid at midStop, bottom), as
the CSS linear-gradient of the pause panel; the middle is drawn in thin
bands that meet edge to edge so there is no visible step
=================
*/
void UI_DRoundPanel( float x, float y, float w, float h, float r, const float *top, const float *mid, float midStop, const float *bottom ) {
	vec4_t	color;
	float	xs[4], ss[4];
	float	y0, y1;
	int		i, k;

	UI_DRegister();
	xs[0] = x; xs[1] = x + r; xs[2] = x + w - r; xs[3] = x + w;
	ss[0] = 0; ss[1] = ROUND_C; ss[2] = 1.0f - ROUND_C; ss[3] = 1.0f;

	UI_DGradColor( color, r * 0.5f / h, top, mid, midStop, bottom );
	trap_R_SetColor( color );
	for ( i = 0; i < 3; i++ ) {
		UI_DPicST( xs[i], y, xs[i + 1] - xs[i], r, ss[i], 0, ss[i + 1], ROUND_C, d_round );
	}

	for ( k = 0; k < 64; k++ ) {
		y0 = y + r + ( h - 2.0f * r ) * k / 64.0f;
		y1 = y + r + ( h - 2.0f * r ) * ( k + 1 ) / 64.0f;
		UI_DGradColor( color, ( ( y0 + y1 ) * 0.5f - y ) / h, top, mid, midStop, bottom );
		trap_R_SetColor( color );
		UI_DPic( x, y0, w, y1 - y0, uis.whiteShader );
	}

	UI_DGradColor( color, 1.0f - r * 0.5f / h, top, mid, midStop, bottom );
	trap_R_SetColor( color );
	for ( i = 0; i < 3; i++ ) {
		UI_DPicST( xs[i], y + h - r, xs[i + 1] - xs[i], r, ss[i], 1.0f - ROUND_C, ss[i + 1], 1, d_round );
	}
	trap_R_SetColor( NULL );
}

// rounded highlight fading from left to right, for the focused list row
void UI_DRoundRowGlow( float x, float y, float w, float h, float r, const float *left, const float *right ) {
	vec4_t	color;
	float	x0, x1;
	int		k;

	UI_DRegister();
	trap_R_SetColor( left );
	UI_DPicST( x, y, r, r, 0, 0, ROUND_C, ROUND_C, d_round );
	UI_DPicST( x, y + r, r, h - 2.0f * r, 0, ROUND_C, ROUND_C, 1.0f - ROUND_C, d_round );
	UI_DPicST( x, y + h - r, r, r, 0, 1.0f - ROUND_C, ROUND_C, 1, d_round );
	for ( k = 0; k < 32; k++ ) {
		x0 = x + r + ( w - 2.0f * r ) * k / 32.0f;
		x1 = x + r + ( w - 2.0f * r ) * ( k + 1 ) / 32.0f;
		UI_DGradColor( color, ( ( x0 + x1 ) * 0.5f - x ) / w, left, right, 1.0f, right );
		trap_R_SetColor( color );
		UI_DPic( x0, y, x1 - x0, h, uis.whiteShader );
	}
	trap_R_SetColor( right );
	UI_DPicST( x + w - r, y, r, r, 1.0f - ROUND_C, 0, 1, ROUND_C, d_round );
	UI_DPicST( x + w - r, y + r, r, h - 2.0f * r, 1.0f - ROUND_C, ROUND_C, 1, 1.0f - ROUND_C, d_round );
	UI_DPicST( x + w - r, y + h - r, r, r, 1.0f - ROUND_C, 1.0f - ROUND_C, 1, 1, d_round );
	trap_R_SetColor( NULL );
}

// key hints centered under the screen, as the pause screens have them
void UI_DHintsCentered( const char **pairs, int count ) {
	float	total, capW, x;
	int		i;

	total = 0.0f;
	for ( i = 0; i < count; i++ ) {
		capW = UI_DTextWidth( pairs[i * 2], 15 ) + 16;
		if ( capW < 30 ) {
			capW = 30;
		}
		total += capW + 8 + UI_DTextWidth( pairs[i * 2 + 1], 19 ) + 26;
	}
	x = 640.0f - ( total - 26.0f ) * 0.5f;
	for ( i = 0; i < count; i++ ) {
		x += UI_DKeyHint( x, 672, pairs[i * 2], pairs[i * 2 + 1] );
	}
}

// footer at the bottom left: pairs = key, label, key, label...
void UI_DHints( const char **pairs, int count ) {
	int		i;
	float	x;

	x = 60 - UI_DExtra();
	for ( i = 0; i < count; i++ ) {
		x += UI_DKeyHint( x, 672, pairs[i * 2], pairs[i * 2 + 1] );
	}
}

/*
=================
UI_DKiButton

the slanted main button with an optional caption under the label; focus
brightens it with the pulsing ki glow and shows the dragon ball on its left
=================
*/
void UI_DKiButton( float x, float y, float w, float h, const char *label, const char *caption, float focus, qboolean disabled, qboolean marker ) {
	vec4_t	color;
	float	labelY;

	trap_R_SetColor( NULL );
	UI_DPicSliced( x, y, w, h, uis.kiButton, KI_ART );
	if ( focus > 0.0f && !disabled ) {
		color[0] = zeq_ki[0]; color[1] = zeq_ki[1]; color[2] = zeq_ki[2];
		color[3] = focus * ( 0.75f + 0.25f * sin( uis.realtime / 220.0f ) );
		trap_R_SetColor( color );
		UI_DPicSliced( x, y, w, h, uis.kiButtonGlow, KI_ART );
		UI_DPicSliced( x, y, w, h, uis.kiButtonGlow, KI_ART );
		trap_R_SetColor( NULL );
		if ( marker ) {
			UI_DPic( x - 40, y + h * 0.5f - 16 + 1.5f * sin( uis.realtime / 300.0f ), 26, 31, uis.dragonBall );
		}
	}
	labelY = caption ? y + h * 0.5f - 21 : y + h * 0.5f - h * 0.24f;
	UI_DProp( x + 26, labelY, label, caption ? 24 : h * 0.44f, UI_LEFT|UI_DROPSHADOW|( focus > 0.0f && !disabled ? UI_PULSE : 0 ),
		disabled ? zeq_text3 : ( focus > 0.0f ? zeq_white : zeq_text2 ) );
	if ( caption ) {
		UI_DText( x + 26, y + h * 0.5f + 5, caption, 17, UI_LEFT, focus > 0.0f ? zeq_text2 : zeq_text3 );
	}
}

// gold call to action (FIGHT!, CONFIRM, APPLY...)
void UI_DCta( float x, float y, float w, float h, const char *label, float focus ) {
	vec4_t dark;
	vec4_t glow;

	UI_DRegister();
	trap_R_SetColor( NULL );
	UI_DPicSliced( x, y, w, h, d_cta, CTA_ART );
	if ( focus > 0.0f ) {
		glow[0] = 1.0f; glow[1] = 0.85f; glow[2] = 0.45f;
		glow[3] = focus * ( 0.45f + 0.25f * sin( uis.realtime / 200.0f ) );
		trap_R_SetColor( glow );
		UI_DPicSliced( x, y, w, h, d_ctaGlow, CTA_ART );
		trap_R_SetColor( NULL );
	}
	dark[0] = 0.11f; dark[1] = 0.06f; dark[2] = 0.01f; dark[3] = 1.0f;
	UI_DProp( x + w * 0.46f, y + h * 0.5f - h * 0.26f, label, h * 0.52f, UI_CENTER, dark );
}

// tab of a tab strip, active = the shown page
void UI_DTab( float x, float y, float w, float h, const char *label, qboolean active, float focus ) {
	vec4_t color;

	UI_DRegister();
	trap_R_SetColor( NULL );
	UI_DPicSliced( x, y, w, h, d_tab, TAB_ART );
	if ( active || focus > 0.0f ) {
		color[0] = zeq_ki[0]; color[1] = zeq_ki[1]; color[2] = zeq_ki[2];
		color[3] = active ? 0.8f : 0.5f * focus;
		if ( focus > 0.0f ) {
			color[3] += 0.2f * sin( uis.realtime / 220.0f );
		}
		trap_R_SetColor( color );
		UI_DPicSliced( x, y, w, h, d_tabGlow, TAB_ART );
		trap_R_SetColor( NULL );
	}
	UI_DProp( x + w * 0.46f, y + h * 0.5f - 8, label, 16, UI_CENTER|UI_DROPSHADOW, ( active || focus > 0.0f ) ? zeq_white : zeq_text3 );
}

// two chevron boxes around a value
void UI_DSelector( float x, float y, float w, const char *value, float focus ) {
	vec4_t box;
	vec4_t edge;

	box[0] = 0.02f; box[1] = 0.03f; box[2] = 0.12f; box[3] = 0.7f;
	edge[0] = 0.24f; edge[1] = 0.33f; edge[2] = 0.66f; edge[3] = 1.0f;
	UI_DFill( x, y, 22, 22, box );
	UI_DFrame( x, y, 22, 22, edge );
	UI_DText( x + 11, y + 2, "<", 17, UI_CENTER, focus > 0.0f ? zeq_white : zeq_text2 );
	UI_DFill( x + w - 22, y, 22, 22, box );
	UI_DFrame( x + w - 22, y, 22, 22, edge );
	UI_DText( x + w - 11, y + 2, ">", 17, UI_CENTER, focus > 0.0f ? zeq_white : zeq_text2 );
	UI_DText( x + w * 0.5f, y + 1, value, 19, UI_CENTER, zeq_white );
}

void UI_DToggle( float x, float y, qboolean on ) {
	UI_DRegister();
	trap_R_SetColor( NULL );
	UI_DPic( x, y, 22, 22, on ? d_switchOn : d_switchOff );
	UI_DText( x + 30, y + 1, on ? "On" : "Off", 19, UI_LEFT, zeq_white );
}

// track with a ki fill and the slider knob of the game
void UI_DSlider( float x, float y, float w, float frac, float focus ) {
	vec4_t track, edge, fill, shine;

	UI_DRegister();
	if ( frac < 0 ) {
		frac = 0;
	}
	if ( frac > 1 ) {
		frac = 1;
	}
	track[0] = 0.02f; track[1] = 0.03f; track[2] = 0.12f; track[3] = 0.9f;
	edge[0] = 0.24f; edge[1] = 0.33f; edge[2] = 0.66f; edge[3] = 1.0f;
	fill[0] = 0.18f; fill[1] = 0.37f; fill[2] = 0.82f; fill[3] = 1.0f;
	shine[0] = 0.47f; shine[1] = 0.70f; shine[2] = 1.0f; shine[3] = 1.0f;
	UI_DFill( x, y + 7, w, 8, track );
	UI_DFrame( x, y + 7, w, 8, edge );
	UI_DFill( x + 1, y + 8, ( w - 2 ) * frac, 6, fill );
	UI_DFill( x + 1, y + 8, ( w - 2 ) * frac, 2, shine );
	trap_R_SetColor( NULL );
	UI_DPic( x + w * frac - 7, y - 2 - focus * 1.5f, 14, 26 + focus * 3.0f, d_sliderKnob );
}

void UI_DPips( float x, float y, int filled, int total, float anim ) {
	int		i;
	float	lit;
	vec4_t	off;
	vec4_t	on;

	off[0] = 0.56f; off[1] = 0.59f; off[2] = 0.74f; off[3] = 0.35f;
	for ( i = 0; i < total; i++ ) {
		UI_DFill( x + i * 15, y, 12, 7, off );
		if ( i < filled ) {
			lit = anim * total - i;
			if ( lit > 1.0f ) {
				lit = 1.0f;
			}
			if ( lit > 0.0f ) {
				on[0] = zeq_gold[0]; on[1] = zeq_gold[1]; on[2] = zeq_gold[2]; on[3] = lit;
				UI_DFill( x + i * 15, y, 12, 7, on );
			}
		}
	}
}

void UI_DStatBar( float x, float y, float w, const char *label, float value, float anim ) {
	vec4_t	track, fill, shine;
	float	tx, tw;

	track[0] = 0.02f; track[1] = 0.03f; track[2] = 0.12f; track[3] = 0.9f;
	fill[0] = 0.18f; fill[1] = 0.37f; fill[2] = 0.82f; fill[3] = 1.0f;
	shine[0] = 0.47f; shine[1] = 0.70f; shine[2] = 1.0f; shine[3] = 1.0f;
	tx = x + 100;
	tw = w - 100 - 40;
	UI_DText( x, y - 4, label, 16, UI_LEFT, zeq_text2 );
	UI_DFill( tx, y, tw, 7, track );
	if ( value * anim > 0.0f ) {
		UI_DFill( tx, y, tw * value * anim, 7, fill );
		UI_DFill( tx, y, tw * value * anim, 2, shine );
	}
	UI_DText( x + w, y - 4, va( "%i", (int)( value * 100.0f + 0.5f ) ), 16, UI_RIGHT, zeq_white );
}

/*
=================
UI_DPortrait

character icon with rounded corners (radius 14 at 96 px); the selected one
gets the 3 px gold outline and its glow, the focused one a ki outline
=================
*/
vec4_t zeq_portraitBack = { 0.035f, 0.032f, 0.13f, 1.0f };	// the glass under the portraits

void UI_DPortrait( float x, float y, float size, qhandle_t shader, qboolean selected, float focus ) {
	vec4_t	ring;
	vec4_t	dim;
	float	grow;
	float	r;

	grow = 4.0f * focus;
	x -= grow * 0.5f;
	y -= grow * 0.5f;
	size += grow;
	r = size * 14.0f / 96.0f;
	if ( selected ) {
		ring[0] = zeq_gold[0]; ring[1] = zeq_gold[1]; ring[2] = zeq_gold[2];
		ring[3] = 0.45f + 0.12f * sin( uis.realtime / 250.0f );
		UI_DRoundGlow( x - 2, y - 2, size + 4, size + 4, 16, ring );
	}
	if ( selected || focus > 0.0f ) {
		trap_R_SetColor( NULL );
	}
	else {
		dim[0] = dim[1] = dim[2] = 0.82f;
		dim[3] = 1.0f;
		trap_R_SetColor( dim );
	}
	UI_DPic( x, y, size, size, shader );
	trap_R_SetColor( NULL );
	UI_DRoundCorners( x, y, size, size, r, zeq_portraitBack );
	if ( selected ) {
		ring[3] = 1.0f;
		UI_DRoundEdge( x - 5, y - 5, size + 10, size + 10, r + 5, ring );
		UI_DRoundEdge( x - 4, y - 4, size + 8, size + 8, r + 4, ring );
		UI_DRoundEdge( x - 3.5f, y - 3.5f, size + 7, size + 7, r + 3.5f, ring );
	}
	else if ( focus > 0.0f ) {
		ring[0] = 0.33f; ring[1] = 0.59f; ring[2] = 1.0f; ring[3] = 0.9f * focus;
		UI_DRoundEdge( x - 4, y - 4, size + 8, size + 8, r + 4, ring );
		UI_DRoundEdge( x - 3.5f, y - 3.5f, size + 7, size + 7, r + 3.5f, ring );
	}
}

void UI_DFocusRow( float x, float y, float w, float h, float focus ) {
	vec4_t fill;
	vec4_t edge;

	if ( focus <= 0.0f ) {
		return;
	}
	fill[0] = zeq_ki[0]; fill[1] = zeq_ki[1]; fill[2] = zeq_ki[2]; fill[3] = 0.30f * focus;
	UI_DFill( x, y, w * ( 0.55f + 0.45f * focus ), h, fill );
	edge[0] = 0.47f; edge[1] = 0.63f; edge[2] = 1.0f; edge[3] = 0.45f * focus;
	UI_DFrame( x, y, w, h, edge );
}

// a focus value that eases in after the cursor lands on item
float UI_DFocus( menuframework_s *menu, void *item ) {
	if ( Menu_ItemAtCursor( menu ) != item ) {
		return 0.0f;
	}
	return UI_AnimFrac( uis.focusTime, 0, 160 );
}

void UI_DNamePlate( float x, float y, float w, float h ) {
	UI_DRegister();
	trap_R_SetColor( NULL );
	UI_DPic( x, y, w, h, d_nameArea );
}

/*
=======================================================================

OPTION ROWS

Options screens keep their menu items (spin controls, sliders, radio
buttons, fields, actions) and only hand them to these rows: the row
takes the hit box, draws label and value on the design canvas, and the
help panel shows the text of the focused row.

=======================================================================
*/

#define DOPT_MAX_ROWS		48
#define DOPT_MAX_SECTIONS	8
#define DOPT_TAB_W			172
#define DOPT_TAB_H			44

typedef struct {
	void		*item;
	const char	*label;
	const char	*help;
	qboolean	restart;
	float		y;
	void		(*value)( void *item, char *buf, int size );
} dOptRow_t;

typedef struct {
	const char	*title;
	float		y;
} dOptSection_t;

static dOptRow_t		dOptRows[DOPT_MAX_ROWS];
static int				dOptNumRows;
static dOptSection_t	dOptSections[DOPT_MAX_SECTIONS];
static int				dOptNumSections;
static float			dOptX, dOptY, dOptW, dOptRowH, dOptCursor;
static menuframework_s	*dOptMenu;

// wraps text into lines of at most w design pixels, returns the line count
int UI_DTextWrapped( float x, float y, float w, const char *text, float size, vec4_t color ) {
	char		line[256];
	char		word[96];
	const char	*p;
	int			n;
	int			lines;

	line[0] = 0;
	lines = 0;
	p = text;
	while ( *p ) {
		while ( *p == ' ' ) {
			p++;
		}
		n = 0;
		while ( *p && *p != ' ' && n < (int)sizeof( word ) - 1 ) {
			word[n++] = *p++;
		}
		word[n] = 0;
		if ( !n ) {
			break;
		}
		if ( line[0] && UI_DTextWidth( va( "%s %s", line, word ), size ) > w ) {
			UI_DText( x, y + lines * size * 1.3f, line, size, UI_LEFT, color );
			lines++;
			Q_strncpyz( line, word, sizeof( line ) );
		}
		else {
			Q_strcat( line, sizeof( line ), line[0] ? va( " %s", word ) : word );
		}
	}
	if ( line[0] ) {
		UI_DText( x, y + lines * size * 1.3f, line, size, UI_LEFT, color );
		lines++;
	}
	return lines;
}

void UI_DOptionsBegin( menuframework_s *menu, float x, float y, float w, float rowH ) {
	dOptMenu = menu;
	dOptX = x;
	dOptY = y;
	dOptW = w;
	dOptRowH = rowH;
	dOptCursor = y;
	dOptNumRows = 0;
	dOptNumSections = 0;
}

void UI_DOptionsSection( const char *title ) {
	if ( dOptNumSections >= DOPT_MAX_SECTIONS ) {
		return;
	}
	if ( dOptNumRows || dOptNumSections ) {
		dOptCursor += 10;
	}
	dOptSections[dOptNumSections].title = title;
	dOptSections[dOptNumSections].y = dOptCursor;
	dOptNumSections++;
	dOptCursor += 26;
}

void UI_DOptionsSkip( float h ) {
	dOptCursor += h;
}

static dOptRow_t *UI_DOptRowOf( void *item ) {
	int i;

	for ( i = 0; i < dOptNumRows; i++ ) {
		if ( dOptRows[i].item == item ) {
			return &dOptRows[i];
		}
	}
	return NULL;
}

static float UI_DOptSliderW( void ) {
	// the slider key maps the mouse over 10 small chars, keep the same width
	return ( 10 * SMALLCHAR_WIDTH ) / UI_DScale();
}

static void UI_DOptRowDraw( void *self ) {
	dOptRow_t		*row;
	menucommon_s	*item;
	float			focus;
	float			vx, y;
	qboolean		grayed;
	char			buf[64];
	vec4_t			*labelColor;
	menuslider_s	*s;
	menulist_s		*l;

	row = UI_DOptRowOf( self );
	if ( !row ) {
		return;
	}
	item = (menucommon_s *)self;
	grayed = ( item->flags & QMF_GRAYED ) != 0;
	focus = grayed ? 0.0f : UI_DFocus( dOptMenu, self );
	y = row->y;
	vx = dOptX + dOptW * 0.50f;

	UI_DFocusRow( dOptX, y - 4, dOptW, dOptRowH - 2, focus );
	labelColor = grayed ? &zeq_text3 : ( focus > 0.0f ? &zeq_gold : &zeq_text2 );
	// labels of old menus are lower case, show them capitalized
	Q_strncpyz( buf, row->label, sizeof( buf ) );
	if ( buf[0] >= 'a' && buf[0] <= 'z' ) {
		buf[0] += 'A' - 'a';
	}
	UI_DText( dOptX + 12, y, buf, 19, UI_LEFT, *labelColor );

	if ( row->value ) {
		row->value( self, buf, sizeof( buf ) );
		UI_DText( vx, y, buf, 19, UI_LEFT, grayed ? zeq_text3 : zeq_white );
		return;
	}

	switch ( item->type ) {
	case MTYPE_SPINCONTROL:
		l = (menulist_s *)self;
		if ( l->itemnames && l->curvalue >= 0 && l->curvalue < l->numitems ) {
			float sw;

			sw = UI_DTextWidth( l->itemnames[l->curvalue], 19 ) + 70;
			UI_DSelector( vx, y - 2, sw > 190 ? sw : 190, l->itemnames[l->curvalue], focus );
		}
		break;
	case MTYPE_SLIDER:
		s = (menuslider_s *)self;
		UI_DSlider( vx, y - 2, UI_DOptSliderW(), ( s->maxvalue > s->minvalue ) ? ( s->curvalue - s->minvalue ) / ( s->maxvalue - s->minvalue ) : 0.0f, focus );
		UI_DText( vx + UI_DOptSliderW() + 16, y, va( "%i", (int)( s->curvalue + 0.5f ) ), 19, UI_LEFT, zeq_white );
		break;
	case MTYPE_RADIOBUTTON:
		UI_DToggle( vx, y - 2, ((menuradiobutton_s *)self)->curvalue != 0 );
		break;
	case MTYPE_FIELD:
		UI_DText( vx, y, ((menufield_s *)self)->field.buffer, 19, UI_LEFT, zeq_white );
		if ( focus > 0.0f && ( ( uis.realtime / 400 ) & 1 ) ) {
			UI_DFill( vx + UI_DTextWidth( ((menufield_s *)self)->field.buffer, 19 ) + 2, y, 2, 20, zeq_white );
		}
		break;
	default:
		break;
	}
}

static void UI_DOptAdd( void *item, const char *label, const char *help, qboolean restart, void (*value)( void *, char *, int ) ) {
	menucommon_s	*c;
	dOptRow_t		*row;
	float			vx, vy, vw, vh;

	if ( dOptNumRows >= DOPT_MAX_ROWS ) {
		return;
	}
	c = (menucommon_s *)item;
	row = &dOptRows[dOptNumRows++];
	row->item = item;
	row->label = label;
	row->help = help;
	row->restart = restart;
	row->value = value;
	row->y = dOptCursor;
	dOptCursor += dOptRowH;

	// hit box over the whole row
	vx = dOptX; vy = row->y - 4; vw = dOptW; vh = dOptRowH;
	UI_DToV( &vx, &vy, &vw, &vh );
	c->left = vx;
	c->top = vy;
	c->right = vx + vw;
	c->bottom = vy + vh;
	c->ownerdraw = UI_DOptRowDraw;
	c->statusbar = 0;	// the help panel replaces the old status lines

	// sliders read the mouse from their x
	if ( c->type == MTYPE_SLIDER ) {
		vx = dOptX + dOptW * 0.50f; vy = row->y; vw = 1; vh = 1;
		UI_DToV( &vx, &vy, &vw, &vh );
		c->x = vx - 2 * SMALLCHAR_WIDTH;
	}
}

// call after Menu_AddItem so the default init does not undo the layout
void UI_DOptionsRow( void *item, const char *label, const char *help, qboolean restart ) {
	UI_DOptAdd( item, label, help, restart, 0 );
}

void UI_DOptionsRowValue( void *item, const char *label, const char *help, void (*value)( void *item, char *buf, int size ) ) {
	UI_DOptAdd( item, label, help, qfalse, value );
}

void UI_DOptionsDrawSections( void ) {
	int		i;
	vec4_t	line;

	line[0] = 0.35f; line[1] = 0.51f; line[2] = 0.90f; line[3] = 0.25f;
	for ( i = 0; i < dOptNumSections; i++ ) {
		UI_DProp( dOptX + 12, dOptSections[i].y, dOptSections[i].title, 14, UI_LEFT, zeq_gold );
		UI_DFill( dOptX + 12 + UI_DPropWidth( dOptSections[i].title, 14 ) + 12, dOptSections[i].y + 7, dOptW - 36 - UI_DPropWidth( dOptSections[i].title, 14 ), 1, line );
	}
}

// right panel: the focused option, what it does and if it needs a restart
void UI_DOptionsDrawHelp( float x, float y, float w, float h ) {
	dOptRow_t	*row;
	int			lines;
	vec4_t		warn;
	vec4_t		warnFill;
	float		pop;

	UI_DGlass( x, y, w, h );
	row = UI_DOptRowOf( Menu_ItemAtCursor( dOptMenu ) );
	UI_DProp( x + 26, y + 26, "SELECTED OPTION", 13, UI_LEFT, zeq_text3 );
	if ( !row ) {
		UI_DTextWrapped( x + 26, y + 56, w - 52, "Pick an option on the left to read what it does.", 19, zeq_text2 );
		return;
	}
	pop = UI_AnimFrac( uis.focusTime, 0, 240 );
	UI_DProp( x + 26 + 10.0f * ( 1.0f - pop ), y + 48, row->label, 24, UI_LEFT|UI_DROPSHADOW, zeq_white );
	lines = UI_DTextWrapped( x + 26, y + 92, w - 52, row->help ? row->help : "", 19, zeq_text2 );
	if ( row->restart ) {
		warn[0] = zeq_gold[0]; warn[1] = zeq_gold[1]; warn[2] = zeq_gold[2]; warn[3] = 0.55f;
		warnFill[0] = zeq_gold[0]; warnFill[1] = zeq_gold[1]; warnFill[2] = zeq_gold[2]; warnFill[3] = 0.08f;
		UI_DFill( x + 26, y + 110 + lines * 25, w - 52, 40, warnFill );
		UI_DFrame( x + 26, y + 110 + lines * 25, w - 52, 40, warn );
		UI_DText( x + 40, y + 120 + lines * 25, "Applies after a video restart", 18, UI_LEFT, zeq_gold );
	}
}

/*
=================
Tabs of a screen made from text items (CONTROLS / SYSTEM / GENERAL)
=================
*/
static void UI_DTabItemDraw( void *self ) {
	menutext_s *t;

	t = (menutext_s *)self;
	UI_DTab( t->generic.x, t->generic.y, DOPT_TAB_W, DOPT_TAB_H, t->string, ( t->generic.flags & QMF_HIGHLIGHT ) != 0,
		UI_DFocus( (menuframework_s *)t->generic.parent, self ) );
}

// call after Menu_AddItem; label replaces the text of the item
void UI_DTabItem( menutext_s *t, int index, const char *label, qboolean active ) {
	float vx, vy, vw, vh;

	t->generic.x = 48 + index * ( DOPT_TAB_W + 6 );
	t->generic.y = 128;
	t->string = (char *)label;
	t->generic.ownerdraw = UI_DTabItemDraw;
	t->generic.flags &= ~QMF_HIGHLIGHT;
	if ( active ) {
		t->generic.flags |= QMF_HIGHLIGHT;
	}
	vx = t->generic.x; vy = t->generic.y; vw = DOPT_TAB_W; vh = DOPT_TAB_H;
	UI_DToV( &vx, &vy, &vw, &vh );
	t->generic.left = vx;
	t->generic.top = vy;
	t->generic.right = vx + vw;
	t->generic.bottom = vy + vh;
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
