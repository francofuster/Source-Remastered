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
#ifndef __UI_LOCAL_H__
#define __UI_LOCAL_H__

#include "../../Shared/q_shared.h"
#include "../../Engine/renderer/tr_types.h"
//NOTE: include the ui_public.h from the new UI
#include "ui_public.h"
//redefine to old API version
#undef UI_API_VERSION
#define UI_API_VERSION	4
#include "../../Engine/client/keycodes.h"
#include "../Game/bg_public.h"

typedef void (*voidfunc_f)(void);

extern vmCvar_t	ui_ffa_fraglimit;
extern vmCvar_t	ui_ffa_timelimit;
extern vmCvar_t	ui_ffa_powerlevel;
extern vmCvar_t ui_ffa_powerlevelMaximum;
extern vmCvar_t	ui_ffa_breakLimitRate;

extern vmCvar_t	ui_tourney_fraglimit;
extern vmCvar_t	ui_tourney_timelimit;
extern vmCvar_t	ui_tourney_powerlevel;
extern vmCvar_t	ui_tourney_breakLimitRate;

extern vmCvar_t	ui_team_fraglimit;
extern vmCvar_t	ui_team_timelimit;
extern vmCvar_t	ui_team_powerlevel;
extern vmCvar_t	ui_team_breakLimitRate;
extern vmCvar_t	ui_team_friendly;

extern vmCvar_t	ui_ctf_capturelimit;
extern vmCvar_t	ui_ctf_timelimit;
extern vmCvar_t	ui_ctf_powerlevel;
extern vmCvar_t	ui_ctf_breakLimitRate;
extern vmCvar_t	ui_ctf_friendly;

extern vmCvar_t	ui_arenasFile;
extern vmCvar_t	ui_botsFile;
extern vmCvar_t	ui_spScores1;
extern vmCvar_t	ui_spScores2;
extern vmCvar_t	ui_spScores3;
extern vmCvar_t	ui_spScores4;
extern vmCvar_t	ui_spScores5;
extern vmCvar_t	ui_spAwards;
extern vmCvar_t	ui_spVideos;
extern vmCvar_t	ui_spSkill;

extern vmCvar_t	ui_spSelection;

extern vmCvar_t	ui_browserMaster;
extern vmCvar_t	ui_browserGameType;
extern vmCvar_t	ui_browserSortKey;
extern vmCvar_t	ui_browserShowFull;
extern vmCvar_t	ui_browserShowEmpty;

extern vmCvar_t	ui_brassTime;
extern vmCvar_t	ui_drawCrosshair;
extern vmCvar_t	ui_drawCrosshairNames;
extern vmCvar_t	ui_marks;

extern vmCvar_t	ui_server1;
extern vmCvar_t	ui_server2;
extern vmCvar_t	ui_server3;
extern vmCvar_t	ui_server4;
extern vmCvar_t	ui_server5;
extern vmCvar_t	ui_server6;
extern vmCvar_t	ui_server7;
extern vmCvar_t	ui_server8;
extern vmCvar_t	ui_server9;
extern vmCvar_t	ui_server10;
extern vmCvar_t	ui_server11;
extern vmCvar_t	ui_server12;
extern vmCvar_t	ui_server13;
extern vmCvar_t	ui_server14;
extern vmCvar_t	ui_server15;
extern vmCvar_t	ui_server16;

extern vmCvar_t	ui_cdkey;
extern vmCvar_t	ui_cdkeychecked;
extern vmCvar_t s_mastermusicvolume;

// Speed scale for UI character animations.
#define PLAYER_MODEL_SPEED	1.25

//
// ui_qmenu.c
//

#define RCOLUMN_OFFSET			( BIGCHAR_WIDTH )
#define LCOLUMN_OFFSET			(-BIGCHAR_WIDTH )

#define SLIDER_RANGE			10
#define	MAX_EDIT_LINE			256

#define MAX_MENUDEPTH			8
#define MAX_MENUITEMS			64

#define MTYPE_NULL				0
#define MTYPE_SLIDER			1
#define MTYPE_ACTION			2
#define MTYPE_SPINCONTROL		3
#define MTYPE_FIELD				4
#define MTYPE_RADIOBUTTON		5
#define MTYPE_BITMAP			6
#define MTYPE_TEXT				7
#define MTYPE_SCROLLLIST		8
#define MTYPE_PTEXT				9
#define MTYPE_BTEXT				10

#define QMF_BLINK				((unsigned int) 0x00000001)
#define QMF_SMALLFONT			((unsigned int) 0x00000002)
#define QMF_LEFT_JUSTIFY		((unsigned int) 0x00000004)
#define QMF_CENTER_JUSTIFY		((unsigned int) 0x00000008)
#define QMF_RIGHT_JUSTIFY		((unsigned int) 0x00000010)
#define QMF_NUMBERSONLY			((unsigned int) 0x00000020)	// edit field is only numbers
#define QMF_HIGHLIGHT			((unsigned int) 0x00000040)
#define QMF_HIGHLIGHT_IF_FOCUS	((unsigned int) 0x00000080)	// steady focus
#define QMF_PULSEIFFOCUS		((unsigned int) 0x00000100)	// pulse if focus
#define QMF_HASMOUSEFOCUS		((unsigned int) 0x00000200)
#define QMF_NOONOFFTEXT			((unsigned int) 0x00000400)
#define QMF_MOUSEONLY			((unsigned int) 0x00000800)	// only mouse input allowed
#define QMF_HIDDEN				((unsigned int) 0x00001000)	// skips drawing
#define QMF_GRAYED				((unsigned int) 0x00002000)	// grays and disables
#define QMF_INACTIVE			((unsigned int) 0x00004000)	// disables any input
#define QMF_NODEFAULTINIT		((unsigned int) 0x00008000)	// skip default initialization
#define QMF_OWNERDRAW			((unsigned int) 0x00010000)
#define QMF_PULSE				((unsigned int) 0x00020000)
#define QMF_LOWERCASE			((unsigned int) 0x00040000)	// edit field is all lower case
#define QMF_UPPERCASE			((unsigned int) 0x00080000)	// edit field is all upper case
#define QMF_SILENT				((unsigned int) 0x00100000)
#define QMF_TINYFONT			((unsigned int) 0x00200000)

// callback notifications
#define QM_GOTFOCUS				1
#define QM_LOSTFOCUS			2
#define QM_ACTIVATED			3

typedef struct _tag_menuframework
{
	int	cursor;
	int cursor_prev;

	int	nitems;
	void *items[MAX_MENUITEMS];

	void (*draw) (void);
	sfxHandle_t (*key) (int key);

	qboolean	wrapAround;
	qboolean	fullscreen;
	qboolean	showlogo;
} menuframework_s;

typedef struct
{
	int type;
	const char *name;
	int	id;
	int x, y;
	int left;
	int	top;
	int	right;
	int	bottom;
	menuframework_s *parent;
	int menuPosition;
	unsigned int flags;

	void (*callback)( void *self, int event );
	void (*statusbar)( void *self );
	void (*ownerdraw)( void *self );
} menucommon_s;

typedef struct {
	int		cursor;
	int		scroll;
	int		widthInChars;
	char	buffer[MAX_EDIT_LINE];
	int		maxchars;
} mfield_t;

typedef struct
{
	menucommon_s	generic;
	mfield_t		field;
} menufield_s;

typedef struct
{
	menucommon_s generic;

	float minvalue;
	float maxvalue;
	float curvalue;

	float range;
} menuslider_s;

typedef struct
{
	menucommon_s generic;

	int	oldvalue;
	int curvalue;
	int	numitems;
	int	top;

	const char **itemnames;

	int width;
	int height;
	int	columns;
	int	seperation;
} menulist_s;

typedef struct
{
	menucommon_s generic;
} menuaction_s;

typedef struct
{
	menucommon_s generic;
	int curvalue;
} menuradiobutton_s;

typedef struct
{
	menucommon_s	generic;
	char*			focuspic;
	char*			errorpic;
	qhandle_t		shader;
	int				width;
	int				height;
	float*			focuscolor;
} menubitmap_s;

typedef struct
{
	menucommon_s	generic;
	char*			string;
	int				style;
	float*			color;
} menutext_s;

extern void			Menu_Cache( void );
extern void			Menu_Focus( menucommon_s *m );
extern void			Menu_AddItem( menuframework_s *menu, void *item );
extern void			Menu_AdjustCursor( menuframework_s *menu, int dir );
extern void			Menu_Common(int amount);
extern void 		Menu_Frame(void);
extern void 		Menu_Side(void);

// ui_shared.c: redesign drawing helpers and animation
extern vec4_t		zeq_white;
extern vec4_t		zeq_text2;
extern vec4_t		zeq_text3;
extern vec4_t		zeq_gold;
extern vec4_t		zeq_ki;
extern float		UI_EaseOut( float t );
extern float		UI_AnimFrac( int startTime, int delay, int duration );
extern void			UI_BeginContentFade( void );
extern void			UI_EndContentFade( void );
extern qboolean		UI_IsSideButton( menucommon_s *item );
extern float		UI_SideButtonOffset( menucommon_s *item );
extern void			UI_DrawGlassPanel( float x, float y, float w, float h );
extern void			UI_DrawKiBar( float x, float y, float w, float h, qboolean focus, float focusFrac );
extern void			UI_DrawDragonBall( float x, float y, float size );
extern void			UI_DrawPips( float x, float y, int filled, int total, float anim );
extern void			UI_DrawStatBar( float x, float y, float w, const char *label, float value, float anim );
extern float		UI_DrawKeyHint( float x, float y, const char *key, const char *label );
extern void			UI_DrawKeyHints( const char **pairs, int count );
extern void			UI_DrawSectionTitle( float x, float y, const char *text );
extern void			UI_DrawPortrait( float x, float y, float size, qhandle_t shader, qboolean selected, float focusFrac );
extern void			UI_DrawFocusRow( float x, float y, float w, float h, float focusFrac );
extern void			UI_FadeSetColor( const float *rgba );
// design layer: 1280x720 design pixels
extern vec4_t		zeq_shade;
extern float		UI_DScale( void );
extern float		UI_DExtra( void );
extern void			UI_DToV( float *x, float *y, float *w, float *h );
extern void			UI_DItem( menubitmap_s *b, int x, int y, int w, int h );
extern void			UI_DPicST( float x, float y, float w, float h, float s0, float t0, float s1, float t1, qhandle_t shader );
extern void			UI_DPic( float x, float y, float w, float h, qhandle_t shader );
extern void			UI_DPicSliced( float x, float y, float w, float h, qhandle_t shader, float texW, float texH, float capL, float capR );
extern void			UI_DFill( float x, float y, float w, float h, const float *color );
extern void			UI_DFrame( float x, float y, float w, float h, const float *color );
extern void			UI_DGlass( float x, float y, float w, float h );
extern float		UI_DTextWidth( const char *str, float size );
extern void			UI_DText( float x, float y, const char *str, float size, int style, vec4_t color );
extern float		UI_DPropWidth( const char *str, float size );
extern void			UI_DProp( float x, float y, const char *str, float size, int style, vec4_t color );
extern void			UI_DLogo( float x, float y, float w );
extern void			UI_DHeader( const char *crumb, const char *title );
extern void			UI_DSectionTitle( float x, float y, const char *text );
extern float		UI_DKeyHint( float x, float y, const char *key, const char *label );
extern void			UI_DHints( const char **pairs, int count );
extern void			UI_DKiButton( float x, float y, float w, float h, const char *label, const char *caption, float focus, qboolean disabled, qboolean marker );
extern void			UI_DCta( float x, float y, float w, float h, const char *label, float focus );
extern void			UI_DTab( float x, float y, float w, float h, const char *label, qboolean active, float focus );
extern void			UI_DSelector( float x, float y, float w, const char *value, float focus );
extern void			UI_DToggle( float x, float y, qboolean on );
extern void			UI_DSlider( float x, float y, float w, float frac, float focus );
extern void			UI_DPips( float x, float y, int filled, int total, float anim );
extern void			UI_DStatBar( float x, float y, float w, const char *label, float value, float anim );
extern void			UI_DPortrait( float x, float y, float size, qhandle_t shader, qboolean selected, float focus );
extern vec4_t		zeq_portraitBack;

// line icons of the design (white, tinted when drawn)
typedef enum {
	UI_ICON_EDIT,
	UI_ICON_CLOSE,
	UI_ICON_PLUS,
	UI_ICON_CHEVRIGHT,
	UI_ICON_CHEVLEFT,
	UI_ICON_CHEVDOWN,
	UI_ICON_ARROWRIGHT,
	UI_ICON_ARROWLEFT,
	UI_ICON_ARROWUP,
	UI_ICON_ARROWDOWN,
	UI_ICON_WARN
} uiIcon_t;

extern void			UI_DIcon( float x, float y, float size, int icon, const float *color );
extern void			UI_DField( float x, float y, float w, float h, float focus );
extern void			UI_DIconButton( float x, float y, float size, int icon, const float *iconColor, float hot );
extern void			UI_DStatBarW( float x, float y, float w, float labelW, const char *label, float value, float anim );
extern void			UI_DWarnBox( float x, float y, float w, const char *text );
extern void			UI_DRoundFill( float x, float y, float w, float h, float r, const float *color );
extern void			UI_DRoundEdge( float x, float y, float w, float h, float r, const float *color );
extern void			UI_DRoundGlow( float x, float y, float w, float h, float spread, const float *color );
extern void			UI_DRoundCorners( float x, float y, float w, float h, float r, const float *color );
extern void			UI_DRoundPanel( float x, float y, float w, float h, float r, const float *top, const float *mid, float midStop, const float *bottom );
extern void			UI_DRoundRowGlow( float x, float y, float w, float h, float r, const float *left, const float *right );
extern void			UI_DHintsCentered( const char **pairs, int count );
extern void			InGame_DrawTopStrip( qboolean bots );
extern void			UI_DFocusRow( float x, float y, float w, float h, float focus );
extern float		UI_DFocus( menuframework_s *menu, void *item );
extern void			UI_DNamePlate( float x, float y, float w, float h );
extern int			UI_DTextWrapped( float x, float y, float w, const char *text, float size, vec4_t color );
extern void			UI_DOptionsBegin( menuframework_s *menu, float x, float y, float w, float rowH );
extern void			UI_DOptionsSection( const char *title );
extern void			UI_DOptionsSkip( float h );
extern void			UI_DOptionsRow( void *item, const char *label, const char *help, qboolean restart );
extern void			UI_DOptionsRowValue( void *item, const char *label, const char *help, void (*value)( void *item, char *buf, int size ) );
extern void			UI_DOptionsDrawSections( void );
extern void			UI_DOptionsDrawHelp( float x, float y, float w, float h );
extern void			UI_DTabItem( menutext_s *t, int index, const char *label, qboolean active );
extern void			Menu_Draw( menuframework_s *menu );
extern void			*Menu_ItemAtCursor( menuframework_s *m );
extern sfxHandle_t	Menu_ActivateItem( menuframework_s *s, menucommon_s* item );
extern void			Menu_SetCursor( menuframework_s *s, int cursor );
extern void			Menu_SetCursorToItem( menuframework_s *m, void* ptr );
extern sfxHandle_t	Menu_DefaultKey( menuframework_s *s, int key );
extern void			Bitmap_Init( menubitmap_s *b );
extern void			Bitmap_Draw( menubitmap_s *b );
extern void			ScrollList_Draw( menulist_s *l );
extern sfxHandle_t	ScrollList_Key( menulist_s *l, int key );
extern sfxHandle_t	menu_in_sound;
extern sfxHandle_t	menu_move_sound;
extern sfxHandle_t	menu_out_sound;
extern sfxHandle_t	menu_buzz_sound;
extern sfxHandle_t	menu_null_sound;
extern sfxHandle_t	menu_exit_sound;
extern vec4_t		menu_text_color;
extern vec4_t		menu_grayed_color;
extern vec4_t		menu_dark_color;
extern vec4_t		menu_highlight_color;
extern vec4_t		menu_red_color;
extern vec4_t		menu_black_color;
extern vec4_t		menu_dim_color;
extern vec4_t		color_black;
extern vec4_t		color_yellow;
extern vec4_t		color_blue;
extern vec4_t		color_white;
extern vec4_t		color_orange;
extern vec4_t		color_red;
extern vec4_t		color_dim;
extern vec4_t		name_color;
extern vec4_t		list_color;
extern vec4_t		listbar_color;
extern vec4_t		text_color_disabled;
extern vec4_t		text_color_normal;
extern vec4_t		text_color_highlight;

extern char	*ui_medalNames[];
extern char	*ui_medalPicNames[];
extern char	*ui_medalSounds[];
//
// ui_mfield.c
//
extern void			MField_Clear( mfield_t *edit );
extern void			MField_KeyDownEvent( mfield_t *edit, int key );
extern void			MField_CharEvent( mfield_t *edit, int ch );
extern void			MField_Draw( mfield_t *edit, int x, int y, int style, vec4_t color );
extern void			MenuField_Init( menufield_s* m );
extern void			MenuField_Draw( menufield_s *f );
extern sfxHandle_t	MenuField_Key( menufield_s* m, int* key );

//
// ui_menu.c
//
extern void MainMenu_Cache( void );
extern void UI_MainMenu(void);
extern void UI_RegisterCvars( void );
extern void UI_UpdateCvars( void );

//
// ui_credits.c
//
extern void UI_CreditMenu( void );

//
// ui_music.c
//
extern void UI_MusicMenu( void );
extern void UI_MusicMenu_Cache( void );


//
// ui_ingame.c
//
extern void InGame_Cache( void );
extern void UI_InGameMenu(void);

//
// ui_confirm.c
//
extern void ConfirmMenu_Cache( void );
extern void UI_ConfirmMenu( const char *question, void (*draw)( void ), void (*action)( qboolean result ) );
extern void UI_ConfirmMenu_Style( const char *question, int style, void (*draw)( void ), void (*action)( qboolean result ) );
extern void UI_Message( const char **lines );

//
// ui_setup.c
//
extern void UI_SetupMenu_Cache( void );
extern void UI_SetupMenu(void);

//
// ui_team.c
//
extern void UI_TeamMainMenu( void );
extern void TeamMain_Cache( void );

//
// ui_connect.c
//
extern void UI_DrawConnectScreen( qboolean overlay );

//
// ui_camera.c
//
extern void UI_CameraMenu( void );
extern void Camera_Cache( void );

//
// ui_controls2.c
//
extern void UI_ControlsMenu( void );
extern void Controls_Cache( void );

//
// ui_playermodel.c
//
extern void UI_PlayerModelMenu( void );
extern void PlayerModel_Cache( void );

//
// ui_playersettings.c
//
extern void UI_PlayerSettingsMenu( void );
extern void PlayerSettings_Cache( void );

//
// ui_preferences.c
//
extern void UI_PreferencesMenu( void );
extern void Preferences_Cache( void );

//
// ui_specifyleague.c
//
extern void UI_SpecifyLeagueMenu( void );
extern void SpecifyLeague_Cache( void );

//
// ui_specifyserver.c
//
extern void UI_SpecifyServerMenu( void );
extern void SpecifyServer_Cache( void );

//
// ui_servers2.c
//
#define MAX_FAVORITESERVERS 16

extern void UI_ArenaServersMenu( void );
extern void ArenaServers_Cache( void );

//
// ui_startserver.c
//
extern void UI_StartServerMenu( qboolean multiplayer );
extern void UI_TestMenu( const char *which );
extern void StartServer_Cache( void );
extern void ServerOptions_Cache( void );
extern void UI_BotSelectMenu( char *bot );
extern void UI_BotSelectMenu_Cache( void );

//
// ui_bots.c
//
#define UI_MAX_BOTDEFS			32
#define UI_MAX_STYLES			16
#define UI_MAX_ROSTER			11		// 12 fighters, slot 0 is the human

typedef struct {
	char	id[32];
	char	name[32];
	char	player[32];
	char	archetype[32];
	int		skill;
} uiBotDef_t;

typedef struct {
	char	file[32];		// aggressive.cfg
	char	label[32];		// Aggressive
	float	values[6];		// aggression, rush, combos, specials, block, ranged
} uiBotStyle_t;

typedef struct {
	int		def;			// index in the bot definitions
	int		skill;			// 1 to 3
	int		style;			// index in the styles
	char	name[32];		// empty = the name in bots.txt
} uiRosterSlot_t;

extern void			UI_Bots_Load( void );
extern int			UI_Bots_NumDefs( void );
extern uiBotDef_t	*UI_Bots_Def( int index );
extern int			UI_Bots_NumStyles( void );
extern uiBotStyle_t	*UI_Bots_Style( int index );
extern int			UI_Bots_RosterCount( void );
extern uiRosterSlot_t *UI_Bots_RosterSlot( int index );
extern const char	*UI_Bots_SkillName( int skill );
extern qhandle_t	UI_Bots_Portrait( const char *player );
extern void			UI_Bots_ExecAddCommands( void );
extern void			UI_BotRosterMenu( void );
extern void			UI_BotEditMenu( int slot );
extern void			UI_PauseBotsMenu( void );

//
// ui_serverinfo.c
//
extern void UI_ServerInfoMenu( void );
extern void ServerInfo_Cache( void );

//
// ui_system.c
//
extern void UI_SystemSettingsMenu( void );
extern void SystemSettings_Cache( void );

//
// ui_players.c
//

//FIXME ripped from cg_local.h
typedef struct {
	int			oldFrame;
	int			oldFrameTime;		// time when ->oldFrame was exactly on

	int			frame;
	int			frameTime;			// time when ->frame will be exactly on

	float		backlerp;

	float		yawAngle;
	qboolean	yawing;
	float		pitchAngle;
	qboolean	pitching;

	int			animationNumber;	// may include ANIM_TOGGLEBIT
	animation_t	*animation;
	int			animationTime;		// time when the first frame of the animation will be exact
} lerpFrame_t;

#define UI_AURA_BOLTS		4

// aura and lightning of a transformation, for the menu preview
typedef struct {
	qboolean	show;			// aura around the fighter
	qboolean	lightning;		// lightning bolts (showLightning in tier.cfg)
	vec3_t		color;
	vec3_t		lightColor;
	float		scale;
	float		length;
	int			numTags[3];		// tag_aura count on head, torso, legs
	qhandle_t	shader;
	qhandle_t	lightningShader;
	int			startTime;
	int			nextBolt;
	int			boltTime[UI_AURA_BOLTS];
	vec3_t		boltOffset[UI_AURA_BOLTS];
} uiAura_t;

typedef struct {
	// model info
	qboolean		fixedlegs;		// true if legs yaw is always the same as torso yaw
	qboolean		fixedtorso;		// true if torso never changes yaw
	qboolean		overrideHead;

	qhandle_t		legsModel;
	qhandle_t		legsSkin;
	lerpFrame_t		legs;

	qhandle_t		torsoModel;
	qhandle_t		torsoSkin;
	lerpFrame_t		torso;

	qhandle_t		headModel;
	qhandle_t		headSkin;
	lerpFrame_t		head;

	animation_t		animations[MAX_ANIMATIONS];

	qhandle_t		weaponModel;
	qhandle_t		barrelModel;
	qhandle_t		flashModel;
	vec3_t			flashDlightColor;
	int				muzzleFlashTime;

	// currently in use drawing parms
	vec3_t			viewAngles;
	vec3_t			moveAngles;
	weapon_t		currentWeapon;
	int				legsAnim;
	int				torsoAnim;

	// animation vars
	weapon_t		weapon;
	weapon_t		lastWeapon;
	weapon_t		pendingWeapon;
	int				weaponTimer;
	int				pendingLegsAnim;
	int				torsoAnimationTimer;

	int				pendingTorsoAnim;
	int				legsAnimationTimer;

	int				pendingHeadAnim;
	int				headAnimationTimer;

	qboolean		chat;
	qboolean		newModel;

	qboolean		barrelSpinning;
	float			barrelAngle;
	int				barrelTime;

	int				realWeapon;

	uiAura_t		aura;
} playerInfo_t;

void UI_DrawPlayer( float x, float y, float w, float h, playerInfo_t *pi, int time );
void UI_DrawPlayer_zMesh( float x, float y, float w, float h, playerInfo_t *pi, int time );
void UI_PlayerInfo_SetModel( playerInfo_t *pi, const char *model );
void UI_PlayerInfo_SetModelTier( playerInfo_t *pi, const char *model, int tier );
void UI_PlayerInfo_SetInfo( playerInfo_t *pi, int legsAnim, int torsoAnim, vec3_t viewAngles, vec3_t moveAngles, weapon_t weaponNum, qboolean chat );
qboolean UI_RegisterClientModelname( playerInfo_t *pi, const char *modelSkinName );

//
// ui_atoms.c
//
typedef struct {
	int					frametime;
	int					realtime;
	int					cursorx;
	int					cursory;
	int					menusp;
	int					menuamount;
	qboolean			hideEarth,hideBack,showFrame,showSide;
	menuframework_s*	activemenu;
	menuframework_s*	stack[MAX_MENUDEPTH];
	glconfig_t			glconfig;
	qboolean			debug;
	qhandle_t			whiteShader;
	qhandle_t			menuBackShader;
	qhandle_t			logoShader;
	qhandle_t			nameArea;
	qhandle_t			blobShadow;
	qhandle_t			applyButton;
	qhandle_t			menuSide;
	qhandle_t			menuFrame;
	qhandle_t			menuButton1;
	qhandle_t			menuButton2;
	qhandle_t			menuButton3;
	qhandle_t			menuButton4;
	qhandle_t			menuButton5;
	qhandle_t			menuButton6;
	qhandle_t			sceneModel;
	qhandle_t			logoModel;
	qhandle_t			charset;
	qhandle_t			charsetProp;
	qhandle_t			charsetPropGlow;
	qhandle_t			charsetPropB;
	qhandle_t			cursor;
	qhandle_t			rb_on;
	qhandle_t			rb_off;
	// Virtual 640x480 UI scaling
	float				scale;
	float				bias;
	// Menu redesign: screen transitions and focus animation
	int					transitionTime;		// realtime of the last push/pop
	int					transitionDir;		// +1 push (slide from the right), -1 pop
	int					focusTime;			// realtime of the last cursor change
	float				fadeAlpha;			// global alpha applied by UI_FadeSetColor
	float				drawOffsetX;		// global x offset in virtual units
	float				markerY;			// animated dragon ball marker
	qboolean			markerValid;
	qhandle_t			kiButton;			// path registered copies that honour vertex color
	qhandle_t			kiButtonGlow;
	qhandle_t			glassPanel;
	qhandle_t			dragonBall;
	qboolean			demoversion;
	qboolean			firstdraw;
} uiStatic_t;

extern void			UI_Init( void );
extern void			UI_Shutdown( void );
extern void			UI_KeyEvent( int key, int down );
extern void			UI_MouseEvent( int dx, int dy );
extern void			UI_Refresh( int realtime );
extern qboolean		UI_ConsoleCommand( int realTime );
extern float		UI_ClampCvar( float min, float max, float value );
extern void			UI_DrawNamedPic( float x, float y, float width, float height, const char *picname );
extern void			UI_DrawHandlePic( float x, float y, float w, float h, qhandle_t hShader );
extern void			UI_DrawHandlePicAspectFill( qhandle_t hShader );
extern void			UI_FillRect( float x, float y, float width, float height, const float *color );
extern void			UI_DrawRect( float x, float y, float width, float height, const float *color );
extern void			UI_UpdateScreen( void );
extern void			UI_SetColor( const float *rgba );
extern void			UI_LerpColor(vec4_t a, vec4_t b, vec4_t c, float t);
extern void			UI_DrawBannerString( int x, int y, const char* str, int style, vec4_t color );
extern float		UI_ProportionalSizeScale( int style );
extern void			UI_DrawProportionalString( int x, int y, const char* str, int style, vec4_t color );
extern void			UI_DrawProportionalString_AutoWrapped( int x, int ystart, int xmax, int ystep, const char* str, int style, vec4_t color );
extern int			UI_ProportionalStringWidth( const char* str );
extern int			UI_DrawStrlen(const char *str);	// JUHOX
extern void			UI_DrawString( int x, int y, const char* str, int style, vec4_t color );
extern void			UI_DrawStringSized( float x, float y, const char* str, int style, vec4_t color, float charw, float charh );
extern float		UI_StringSizedWidth( const char *str, float charw );
extern void			UI_DrawPropScaled( float x, float y, const char* str, int style, vec4_t color, float sizeScale );
extern void			UI_DrawChar( int x, int y, int ch, int style, vec4_t color );
extern qboolean 	UI_CursorInRect (int x, int y, int width, int height);
extern void			UI_AdjustFrom640( float *x, float *y, float *w, float *h );
extern void			UI_DrawTextBox (int x, int y, int width, int lines);
extern qboolean		UI_IsFullscreen( void );
extern void			UI_SetActiveMenu( uiMenuCommand_t menu );
extern void			UI_PushMenu ( menuframework_s *menu );
extern void			UI_PopMenu (void);
extern void			UI_ForceMenuOff (void);
extern char			*UI_Argv( int arg );
extern char			*UI_Cvar_VariableString( const char *var_name );
extern void			UI_MenuScene( void );
extern void			UI_MenuLogo( void );
extern void			UI_DrawBackPic(qboolean drawPic);	// JUHOX
extern void			UI_Refresh( int time );
extern void			UI_StartDemoLoop( void );
extern qboolean		m_entersound;
extern uiStatic_t	uis;

//
// ui_syscalls.c
//

void			trap_Print( const char *string );
void			trap_Error( const char *string );
int				trap_Milliseconds( void );
void			trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
void			trap_Cvar_Update( vmCvar_t *vmCvar );
void			trap_Cvar_Set( const char *var_name, const char *value );
float			trap_Cvar_VariableValue( const char *var_name );
void			trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
void			trap_Cvar_SetValue( const char *var_name, float value );
void			trap_Cvar_Reset( const char *name );
void			trap_Cvar_Create( const char *var_name, const char *var_value, int flags );
void			trap_Cvar_InfoStringBuffer( int bit, char *buffer, int bufsize );
int				trap_Argc( void );
void			trap_Argv( int n, char *buffer, int bufferLength );
void			trap_Cmd_ExecuteText( int exec_when, const char *text );	// don't use EXEC_NOW!
int				trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void			trap_FS_Read( void *buffer, int len, fileHandle_t f );
void			trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void			trap_FS_FCloseFile( fileHandle_t f );
int				trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize );
int				trap_FS_Seek( fileHandle_t f, long offset, int origin ); // fsOrigin_t
qhandle_t		trap_R_RegisterModel( const char *name );
qhandle_t		trap_R_RegisterSkin( const char *name );
qhandle_t		trap_R_RegisterShaderNoMip( const char *name );
void			trap_R_ClearScene( void );
void			trap_R_AddRefEntityToScene( const refEntity_t *re );
void			trap_R_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts );
void			trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void			trap_R_RenderScene( const refdef_t *fd );
void			trap_R_SetColor( const float *rgba );
// Every UI draw goes through the fade wrapper so whole screens can fade in.
// ui_syscalls.c and the wrapper itself undefine this.
#define trap_R_SetColor UI_FadeSetColor
void			trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );
void			trap_UpdateScreen( void );
int				trap_CM_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName );
void			trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
sfxHandle_t	trap_S_RegisterSound( const char *sample, qboolean compressed );
void			trap_Key_KeynumToStringBuf( int keynum, char *buf, int buflen );
void			trap_Key_GetBindingBuf( int keynum, char *buf, int buflen );
void			trap_Key_SetBinding( int keynum, const char *binding );
qboolean		trap_Key_IsDown( int keynum );
qboolean		trap_Key_GetOverstrikeMode( void );
void			trap_Key_SetOverstrikeMode( qboolean state );
void			trap_Key_ClearStates( void );
int				trap_Key_GetCatcher( void );
void			trap_Key_SetCatcher( int catcher );
void			trap_GetClipboardData( char *buf, int bufsize );
void			trap_GetClientState( uiClientState_t *state );
void			trap_GetGlconfig( glconfig_t *glconfig );
int				trap_GetConfigString( int index, char* buff, int buffsize );
int				trap_LAN_GetServerCount( int source );
void			trap_LAN_GetServerAddressString( int source, int n, char *buf, int buflen );
void			trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen );
int				trap_LAN_GetPingQueueCount( void );
int				trap_LAN_ServerStatus( const char *serverAddress, char *serverStatus, int maxLen );
void			trap_LAN_ClearPing( int n );
void			trap_LAN_GetPing( int n, char *buf, int buflen, int *pingtime );
void			trap_LAN_GetPingInfo( int n, char *buf, int buflen );
int				trap_MemoryRemaining( void );

//
// ui_teamorders.c
//
extern void UI_TeamOrdersMenu( void );
extern void UI_TeamOrdersMenu_f( void );
extern void UI_TeamOrdersMenu_Cache( void );

//
// ui_loadconfig.c
//
void UI_LoadConfig_Cache( void );
void UI_LoadConfigMenu( void );

//
// ui_saveconfig.c
//
void UI_SaveConfigMenu_Cache( void );
void UI_SaveConfigMenu( void );

//
// ui_gameinfo.c
//
typedef enum {
	AWARD_ACCURACY,
	AWARD_IMPRESSIVE,
	AWARD_EXCELLENT,
	AWARD_GAUNTLET,
	AWARD_FRAGS,
	AWARD_PERFECT
} awardType_t;

const char *UI_GetArenaInfoByNumber( int num );
const char *UI_GetArenaInfoByMap( const char *map );
const char *UI_GetSpecialArenaInfo( const char *tag );
int UI_GetNumArenas( void );
int UI_GetNumSPArenas( void );
int UI_GetNumSPTiers( void );

char *UI_GetBotInfoByNumber( int num );
char *UI_GetBotInfoByName( const char *name );
int UI_GetNumBots( void );

void UI_GetBestScore( int level, int *score, int *skill );
void UI_SetBestScore( int level, int score );
int UI_TierCompleted( int levelWon );
qboolean UI_ShowTierVideo( int tier );
qboolean UI_CanShowTierVideo( int tier );
int  UI_GetCurrentGame( void );
void UI_NewGame( void );
void UI_LogAwardData( int award, int data );
int UI_GetAwardLevel( int award );

void UI_SPUnlock_f( void );
void UI_SPUnlockMedals_f( void );

void UI_InitGameinfo( void );

#endif
