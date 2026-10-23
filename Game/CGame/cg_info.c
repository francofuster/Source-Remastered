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
// cg_info.c -- display information while data is being loading
//
// Menu redesign: the loading screen is laid out on a 1280x720 design
// canvas that is fitted to the real screen (letterboxed on other aspects).

#include "cg_local.h"

#define LOAD_DESIGN_W		1280.0f
#define LOAD_DESIGN_H		720.0f
#define LOAD_MAX_FIGHTERS	6

static float	loadingProgress;		// 0..1, raised by each loading stage
static char		loadingStage[64];

static float	ld_scale;
static float	ld_offX;
static float	ld_offY;

static vec4_t	ld_white	= { 1.00f, 1.00f, 1.00f, 1.00f };
static vec4_t	ld_text2	= { 0.77f, 0.80f, 0.92f, 1.00f };
static vec4_t	ld_text3	= { 0.68f, 0.71f, 0.85f, 1.00f };
static vec4_t	ld_gold		= { 0.95f, 0.70f, 0.24f, 1.00f };
static vec4_t	ld_ki		= { 0.33f, 0.59f, 1.00f, 1.00f };

static const char *loadingTips[] = {
	"Press G to lock on to the nearest opponent and keep them in view.",
	"Hold F and W to power up to your next transformation.",
	"Use the number keys to pick a skill, then attack with the mouse.",
	"Press R to fly up and C to fly down.",
	"Hold F1 to see the scores of the match."
};

/*
=======================================================================

design canvas helpers, all in 1280x720 design pixels

=======================================================================
*/

static void LD_Setup( void ) {
	float sx, sy;

	sx = cgs.glconfig.vidWidth / LOAD_DESIGN_W;
	sy = cgs.glconfig.vidHeight / LOAD_DESIGN_H;
	ld_scale = ( sx < sy ) ? sx : sy;
	ld_offX = ( cgs.glconfig.vidWidth - LOAD_DESIGN_W * ld_scale ) * 0.5f;
	ld_offY = ( cgs.glconfig.vidHeight - LOAD_DESIGN_H * ld_scale ) * 0.5f;
}

static void LD_PicST( float x, float y, float w, float h, float s0, float t0, float s1, float t1, qhandle_t shader ) {
	trap_R_DrawStretchPic( ld_offX + x * ld_scale, ld_offY + y * ld_scale, w * ld_scale, h * ld_scale, s0, t0, s1, t1, shader );
}

static void LD_Pic( float x, float y, float w, float h, qhandle_t shader ) {
	LD_PicST( x, y, w, h, 0, 0, 1, 1, shader );
}

static void LD_Fill( float x, float y, float w, float h, const float *color ) {
	trap_R_SetColor( color );
	LD_Pic( x, y, w, h, cgs.media.whiteShader );
	trap_R_SetColor( NULL );
}

static void LD_Frame( float x, float y, float w, float h, const float *color ) {
	LD_Fill( x, y, w, 1, color );
	LD_Fill( x, y + h - 1, w, 1, color );
	LD_Fill( x, y, 1, h, color );
	LD_Fill( x + w - 1, y, 1, h, color );
}

// the menu glass (menuFrame.png) as a nine slice
static void LD_Glass( float x, float y, float w, float h ) {
	qhandle_t	shader;
	float		c, cs, ct;
	float		xs[4], ys[4], ss[4], ts[4];
	int			i, j;

	shader = trap_R_RegisterShaderNoMip( "interface/menu/menuFrame.png" );
	c = 22.0f;
	cs = 40.0f / 587.0f;
	ct = 40.0f / 496.0f;
	xs[0] = x; xs[1] = x + c; xs[2] = x + w - c; xs[3] = x + w;
	ys[0] = y; ys[1] = y + c; ys[2] = y + h - c; ys[3] = y + h;
	ss[0] = 0; ss[1] = cs; ss[2] = 1.0f - cs; ss[3] = 1.0f;
	ts[0] = 0; ts[1] = ct; ts[2] = 1.0f - ct; ts[3] = 1.0f;
	trap_R_SetColor( NULL );
	for ( j = 0; j < 3; j++ ) {
		for ( i = 0; i < 3; i++ ) {
			LD_PicST( xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j], ss[i], ts[j], ss[i + 1], ts[j + 1], shader );
		}
	}
}

// console font (font0); size is the line height in design pixels
static float LD_TextWidth( const char *str, float size ) {
	return CG_DrawStrlen( str ) * size * 0.40f;
}

static void LD_Text( float x, float y, const char *str, float size, int style, const float *color ) {
	const char	*s;
	int			ch;
	float		cw, adv;
	float		ax;
	float		frow, fcol;
	vec4_t		shadow;
	int			pass;

	cw = size * 0.58f;
	adv = size * 0.40f;
	switch ( style & UI_FORMATMASK ) {
	case UI_CENTER:
		x -= LD_TextWidth( str, size ) * 0.5f;
		break;
	case UI_RIGHT:
		x -= LD_TextWidth( str, size );
		break;
	default:
		break;
	}
	shadow[0] = shadow[1] = shadow[2] = 0.0f;
	shadow[3] = 0.7f * color[3];
	for ( pass = ( style & UI_DROPSHADOW ) ? 0 : 1; pass < 2; pass++ ) {
		trap_R_SetColor( pass ? color : shadow );
		ax = x + ( pass ? 0.0f : 1.5f );
		for ( s = str; *s; s++ ) {
			if ( Q_IsColorString( s ) ) {
				s++;
				continue;
			}
			ch = *s & 255;
			if ( ch != ' ' ) {
				frow = ( ch >> 4 ) * 0.0625f;
				fcol = ( ch & 15 ) * 0.0625f;
				LD_PicST( ax, y + ( pass ? 0.0f : 1.5f ), cw, size, fcol, frow, fcol + 0.0625f, frow + 0.0625f, cgs.media.charsetShader );
			}
			ax += adv;
		}
	}
	trap_R_SetColor( NULL );
}

// proportional font (font1); size is the cell height in design pixels
static void LD_Prop( float x, float y, const char *str, float size, int style, const float *color ) {
	float scale;

	scale = size / (float)PROP_HEIGHT * ld_scale;
	switch ( style & UI_FORMATMASK ) {
	case UI_CENTER:
		x -= CG_PropStringWidthPx( str, scale ) / ld_scale * 0.5f;
		break;
	case UI_RIGHT:
		x -= CG_PropStringWidthPx( str, scale ) / ld_scale;
		break;
	default:
		break;
	}
	CG_DrawPropStringPx( ld_offX + x * ld_scale, ld_offY + y * ld_scale, str, scale, style & ~UI_FORMATMASK, color );
}

// level shot cropped to fill the whole screen, not just the design canvas
static void LD_Background( qhandle_t shader ) {
	float	w, h, x, y;

	if ( !shader ) {
		return;
	}
	w = cgs.glconfig.vidWidth;
	h = w * 0.75f;
	if ( h < cgs.glconfig.vidHeight ) {
		h = cgs.glconfig.vidHeight;
		w = h / 0.75f;
	}
	x = ( cgs.glconfig.vidWidth - w ) * 0.5f;
	y = ( cgs.glconfig.vidHeight - h ) * 0.5f;
	trap_R_SetColor( NULL );
	trap_R_DrawStretchPic( x, y, w, h, 0, 0, 1, 1, shader );
}

// dark band at the top, clear middle, deep navy at the bottom
static void LD_Gradient( void ) {
	int		i;
	float	t, a;
	float	bandH;
	vec4_t	c;

	c[0] = 0.02f; c[1] = 0.024f; c[2] = 0.10f;
	// whole pixel bands that touch but never overlap, or the seams show
	for ( i = 0; i < 96; i++ ) {
		float y0, y1;

		y0 = (int)( cgs.glconfig.vidHeight * i / 96.0f );
		y1 = (int)( cgs.glconfig.vidHeight * ( i + 1 ) / 96.0f );
		bandH = y1 - y0;
		t = ( i + 0.5f ) / 96.0f;
		if ( t < 0.22f ) {
			a = 0.55f * ( 1.0f - t / 0.22f );
		}
		else if ( t < 0.42f ) {
			a = 0.0f;
		}
		else if ( t < 0.78f ) {
			a = 0.92f * ( t - 0.42f ) / 0.36f;
		}
		else {
			a = 0.92f + 0.08f * ( t - 0.78f ) / 0.22f;
		}
		c[3] = a;
		trap_R_SetColor( c );
		trap_R_DrawStretchPic( 0, y0, cgs.glconfig.vidWidth, bandH, 0, 0, 0, 0, cgs.media.whiteShader );
	}
	trap_R_SetColor( NULL );
}

// "lastStand" -> "LAST STAND"
static void LD_MapTitle( const char *mapname, char *out, int size ) {
	int n;
	int i;

	n = 0;
	for ( i = 0; mapname[i] && n < size - 2; i++ ) {
		if ( i > 0 && mapname[i] >= 'A' && mapname[i] <= 'Z' && mapname[i - 1] >= 'a' && mapname[i - 1] <= 'z' ) {
			out[n++] = ' ';
		}
		out[n++] = ( mapname[i] >= 'a' && mapname[i] <= 'z' ) ? mapname[i] - 'a' + 'A' : mapname[i];
	}
	out[n] = 0;
}

static const char *LD_GametypeName( void ) {
	switch ( cgs.gametype ) {
	case GT_FFA:			return "Free For All";
	case GT_SINGLE_PLAYER:	return "Single Player";
	case GT_TOURNAMENT:		return "Tournament";
	case GT_STRUGGLE:		return "Struggle";
	case GT_TEAM:			return "Team Battle";
	case GT_CTF:			return "Capture The Flag";
	default:				return "Battle";
	}
}

// word wraps text into the tip panel, at most three lines
static void LD_WrappedText( float x, float y, const char *text, float size, float maxW, const float *color ) {
	char		line[128];
	char		word[64];
	const char	*p;
	int			n;
	int			lines;
	float		lineH;

	line[0] = 0;
	lines = 0;
	lineH = size * 1.2f;
	p = text;
	while ( *p && lines < 3 ) {
		n = 0;
		while ( *p == ' ' ) {
			p++;
		}
		while ( *p && *p != ' ' && n < (int)sizeof( word ) - 1 ) {
			word[n++] = *p++;
		}
		word[n] = 0;
		if ( !n ) {
			break;
		}
		if ( line[0] && LD_TextWidth( va( "%s %s", line, word ), size ) > maxW ) {
			LD_Text( x, y + lines * lineH, line, size, UI_LEFT, color );
			lines++;
			Q_strncpyz( line, word, sizeof( line ) );
		}
		else {
			Q_strcat( line, sizeof( line ), line[0] ? va( " %s", word ) : word );
		}
	}
	if ( line[0] && lines < 3 ) {
		LD_Text( x, y + lines * lineH, line, size, UI_LEFT, color );
	}
}

/*
======================
CG_LoadingString

Each stage of CG_Init raises the progress bar. Names that are not stages
(lens flare files, map name) keep the current value.
======================
*/
static float LD_StageProgress( const char *s ) {
	if ( !s[0] ) {
		return 1.0f;
	}
	if ( !Q_stricmp( s, "collision map" ) ) {
		return 0.08f;
	}
	if ( !Q_stricmp( s, "sounds" ) ) {
		return 0.22f;
	}
	if ( !Q_stricmp( s, "graphics" ) ) {
		return 0.38f;
	}
	if ( !Q_stricmp( s, "lens flares" ) ) {
		return 0.55f;
	}
	if ( !Q_stricmp( s, "game media" ) ) {
		return 0.66f;
	}
	if ( !Q_stricmp( s, "clients" ) ) {
		return 0.82f;
	}
	return 0.0f;
}

void CG_LoadingString( const char *s ) {
	float stage;

	Q_strncpyz( cg.infoScreenText, s, sizeof( cg.infoScreenText ) );

	stage = LD_StageProgress( s );
	if ( stage > loadingProgress ) {
		loadingProgress = stage;
		Q_strncpyz( loadingStage, s, sizeof( loadingStage ) );
	}
	else if ( loadingProgress >= 0.82f && loadingProgress < 0.97f && s[0] ) {
		// fighters arrive one by one during the clients stage
		loadingProgress += 0.02f;
	}

	trap_UpdateScreen();
}

/*
===================
CG_LoadingItem
===================
*/
void CG_LoadingItem( int itemNum ) {}

/*
===================
CG_LoadingClient
===================
*/
void CG_LoadingClient( int clientNum ) {
	const char		*info;
	char			personality[MAX_QPATH];

	info = CG_ConfigString( CS_PLAYERS + clientNum );
	Q_strncpyz( personality, Info_ValueForKey( info, "n" ), sizeof(personality) );
	Q_CleanStr( personality );

	if( cgs.gametype == GT_SINGLE_PLAYER ) {
		trap_S_RegisterSound( va( "sound/player/announce/%s.ogg", personality ), qtrue );
	}

	CG_LoadingString( personality );
}

/*
===================
LD_DrawFighters

portraits of everyone in the match, from the player configstrings
===================
*/
static void LD_DrawFighters( float x, float y ) {
	const char	*info;
	char		model[MAX_QPATH];
	char		name[MAX_QPATH];
	char		*slash;
	int			i;
	int			shown;
	qboolean	bot;

	shown = 0;
	for ( i = 0; i < MAX_CLIENTS && shown < LOAD_MAX_FIGHTERS; i++ ) {
		info = CG_ConfigString( CS_PLAYERS + i );
		if ( !info[0] ) {
			continue;
		}
		Q_strncpyz( model, Info_ValueForKey( info, "model" ), sizeof( model ) );
		slash = strchr( model, '/' );
		if ( slash ) {
			*slash = 0;
		}
		Q_strncpyz( name, Info_ValueForKey( info, "n" ), sizeof( name ) );
		Q_CleanStr( name );
		bot = Info_ValueForKey( info, "skill" )[0] != 0;

		trap_R_SetColor( NULL );
		LD_Pic( x, y, 44, 44, trap_R_RegisterShaderNoMip( va( "players/%s/icon_default.png", model ) ) );
		if ( i == cg.clientNum ) {
			LD_Frame( x - 2, y - 2, 48, 48, ld_ki );
		}
		LD_Text( x + 54, y + 2, name, 18, UI_LEFT|UI_DROPSHADOW, ld_white );
		LD_Text( x + 54, y + 23, va( "%s  -  %s", model, ( i == cg.clientNum ) ? "you" : ( bot ? "bot" : "fighter" ) ), 15, UI_LEFT, ld_text3 );
		x += 74 + LD_TextWidth( name, 18 ) + 30;
		shown++;
	}
}

/*
====================
CG_DrawInformation

Draw all the status / pacifier stuff during level loading
====================
*/
void CG_DrawInformation( void ) {
	const char	*s;
	const char	*info;
	const char	*sysInfo;
	char		title[64];
	char		subtitle[256];
	char		stage[96];
	char		buf[1024];
	int			value;
	int			tip;
	float		fill;
	float		bob;
	vec4_t		track;
	vec4_t		edge;
	vec4_t		fillDark;
	vec4_t		fillLight;
	vec4_t		glow;
	qhandle_t	logo;

	LD_Setup();
	info = CG_ConfigString( CS_SERVERINFO );
	sysInfo = CG_ConfigString( CS_SYSTEMINFO );

	// full bleed level shot under a navy gradient
	s = Info_ValueForKey( info, "mapname" );
	LD_Background( trap_R_RegisterShaderNoMip( va( "maps/%s.jpg", s ) ) );
	LD_Gradient();
	LD_MapTitle( s, title, sizeof( title ) );

	// logo, cropped out of the 1024x768 logo sheet
	logo = trap_R_RegisterShaderNoMip( "interface/menu/zeq2litelogo.png" );
	trap_R_SetColor( NULL );
	LD_PicST( 40, 24, 170, 86, 155.0f / 1024.0f, 194.0f / 768.0f, 874.0f / 1024.0f, 558.0f / 768.0f, logo );

	// tip of the day
	tip = ( atoi( Info_ValueForKey( CG_ConfigString( CS_SYSTEMINFO ), "sv_serverid" ) ) & 0x7fffffff ) % ( sizeof( loadingTips ) / sizeof( loadingTips[0] ) );
	LD_Glass( 860, 30, 380, 118 );
	trap_R_SetColor( NULL );
	LD_Pic( 884, 54, 30, 36, trap_R_RegisterShaderNoMip( "interface/menu/dragonBall.png" ) );
	LD_Prop( 928, 52, "TIP", 15, UI_LEFT, ld_gold );
	LD_WrappedText( 928, 72, loadingTips[tip], 19, 290, ld_white );

	// map name and rules
	LD_Prop( 60, 420, "LOADING", 15, UI_LEFT, ld_gold );
	LD_Prop( 60, 440, title, 62, UI_LEFT|UI_DROPSHADOW, ld_white );

	s = CG_ConfigString( CS_MESSAGE );
	if ( !Q_stricmpn( s, "ZEQ2 - ", 7 ) ) {
		s += 7;
	}
	Com_sprintf( subtitle, sizeof( subtitle ), "%s%s%s", s, s[0] ? "  -  " : "", LD_GametypeName() );
	value = atoi( Info_ValueForKey( info, "fraglimit" ) );
	if ( value && cgs.gametype < GT_CTF ) {
		Q_strcat( subtitle, sizeof( subtitle ), va( "  -  KO limit %i", value ) );
	}
	value = atoi( Info_ValueForKey( info, "timelimit" ) );
	if ( value ) {
		Q_strcat( subtitle, sizeof( subtitle ), va( "  -  %i min", value ) );
	}
	if ( Info_ValueForKey( sysInfo, "sv_cheats" )[0] == '1' ) {
		Q_strcat( subtitle, sizeof( subtitle ), "  -  cheats on" );
	}
	LD_Text( 60, 506, subtitle, 22, UI_LEFT|UI_DROPSHADOW, ld_text2 );

	// progress, a ki bar like the health bar of the HUD
	if ( loadingStage[0] ) {
		Com_sprintf( stage, sizeof( stage ), "Loading %s", loadingStage );
	}
	else {
		Q_strncpyz( stage, cg.infoScreenText[0] ? "Loading" : "Awaiting snapshot", sizeof( stage ) );
	}
	if ( cg.infoScreenText[0] && Q_stricmp( cg.infoScreenText, loadingStage ) && Q_stricmpn( cg.infoScreenText, "LF:", 3 ) ) {
		Q_strcat( stage, sizeof( stage ), va( "  -  %s", cg.infoScreenText ) );
	}
	LD_Text( 60, 556, stage, 18, UI_LEFT, ld_text3 );
	LD_Prop( 820, 558, va( "%i%%", (int)( loadingProgress * 100.0f ) ), 15, UI_RIGHT, ld_white );

	track[0] = 0.02f; track[1] = 0.03f; track[2] = 0.12f; track[3] = 0.9f;
	edge[0] = 0.24f; edge[1] = 0.33f; edge[2] = 0.66f; edge[3] = 1.0f;
	fillDark[0] = 0.16f; fillDark[1] = 0.35f; fillDark[2] = 0.78f; fillDark[3] = 1.0f;
	fillLight[0] = 0.66f; fillLight[1] = 0.82f; fillLight[2] = 1.0f; fillLight[3] = 1.0f;
	glow[0] = ld_ki[0]; glow[1] = ld_ki[1]; glow[2] = ld_ki[2]; glow[3] = 0.25f;
	LD_Fill( 60, 586, 760, 16, track );
	LD_Frame( 60, 586, 760, 16, edge );
	fill = 756.0f * loadingProgress;
	if ( fill > 0.0f ) {
		LD_Fill( 62, 582, fill, 24, glow );
		LD_Fill( 62, 588, fill, 12, fillDark );
		LD_Fill( 62, 588, fill, 4, fillLight );
	}

	// the fighters of the match
	LD_DrawFighters( 60, 628 );

	// bottom right: server and the dragon ball, bobbing on every redraw
	trap_Cvar_VariableStringBuffer( "sv_running", buf, sizeof( buf ) );
	if ( atoi( buf ) ) {
		Q_strncpyz( buf, "Local match", sizeof( buf ) );
	}
	else {
		Q_strncpyz( buf, Info_ValueForKey( info, "sv_hostname" ), sizeof( buf ) );
		Q_CleanStr( buf );
	}
	LD_Prop( 1158, 630, "CONNECTED", 15, UI_RIGHT, ld_white );
	LD_Text( 1158, 650, buf, 17, UI_RIGHT, ld_text3 );
	bob = 3.0f * sin( trap_Milliseconds() / 160.0f );
	trap_R_SetColor( NULL );
	LD_Pic( 1176, 620 + bob, 56, 67, trap_R_RegisterShaderNoMip( "interface/menu/dragonBall.png" ) );
}
