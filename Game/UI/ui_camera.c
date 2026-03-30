// Copyright (C) 2009 MDave.
//
/*
=======================================================================

SETUP MENU

=======================================================================
*/


#include "ui_local.h"

#define ART_BACK0				"interface/art/back_0"
#define ART_BACK1				"interface/art/back_1"

#define ID_RANGE		10
#define ID_HEIGHT		11
#define ID_ANGLE		12
#define ID_BACK			13
#define ID_SLIDE		14
#define ID_LOCKED_RANGE	15
#define ID_LOCKED_HEIGHT	16
#define ID_LOCKED_SLIDE	17
#define ID_LOCKED_ANGLE	18

typedef struct
{
	char*	name;
	float	defaultvalue;
	float	value;	
} cameraconfigcvar_t;

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;

	menutext_s		range;
	menutext_s		height;
	menutext_s		angle;
	menutext_s		lockedBanner;

	menuslider_s	rangeSlider;
	menuslider_s	heightSlider;
	menuslider_s	slideSlider;
	menuslider_s	angleSlider;
	menuslider_s	lockedRangeSlider;
	menuslider_s	lockedHeightSlider;
	menuslider_s	lockedSlideSlider;
	menuslider_s	lockedAngleSlider;

	qboolean		changesMade;
	qboolean		previewLocked;

	float			originalThirdPersonRange;
	float			originalThirdPersonHeight;
	float			originalThirdPersonSlide;
	float			originalThirdPersonAngle;

	menubitmap_s	back;
} camera_t;

static camera_t	s_camera;

static cameraconfigcvar_t g_cameraconfigcvars[] =
{
	{"cg_thirdPersonRange",	0,	0},
	{"cg_thirdPersonHeight",0,	0},
	{"cg_thirdPersonSlide",0,	0},
	{"cg_thirdPersonAngle",	0,	0},
	{"cg_lockedRange",		0,	0},
	{"cg_lockedHeight",	0,	0},
	{"cg_lockedSlide",	0,	0},
	{"cg_lockedAngle",		0,	0},
	{NULL,					0,	0}
};

/*
=================
Camera_InitCvars
=================
*/
static void Camera_InitCvars( void )
{
	int				i;
	cameraconfigcvar_t*	cvarptr;

	cvarptr = g_cameraconfigcvars;
	for (i=0; ;i++,cvarptr++)
	{
		if (!cvarptr->name)
			break;

		// get current value
		cvarptr->value = trap_Cvar_VariableValue( cvarptr->name );

		// get default value
		trap_Cvar_Reset( cvarptr->name );
		cvarptr->defaultvalue = trap_Cvar_VariableValue( cvarptr->name );

		// restore current value
		trap_Cvar_SetValue( cvarptr->name, cvarptr->value );
	}
}

/*
=================
Camera_GetCvarDefault
=================
*/
static float Camera_GetCvarDefault( char* name )
{
	cameraconfigcvar_t*	cvarptr;
	int				i;

	cvarptr = g_cameraconfigcvars;
	for (i=0; ;i++,cvarptr++)
	{
		if (!cvarptr->name)
			return (0);

		if (!strcmp(cvarptr->name,name))
			break;
	}

	return (cvarptr->defaultvalue);
}

/*
=================
Camera_GetCvarValue
=================
*/
static float Camera_GetCvarValue( char* name )
{
	cameraconfigcvar_t*	cvarptr;
	int				i;

	cvarptr = g_cameraconfigcvars;
	for (i=0; ;i++,cvarptr++)
	{
		if (!cvarptr->name)
			return (0);

		if (!strcmp(cvarptr->name,name))
			break;
	}

	return (cvarptr->value);
}

/*
=================
Camera_GetConfig
=================
*/
static void Camera_GetConfig( void )
{
	s_camera.rangeSlider.curvalue        = UI_ClampCvar( 15, 180, Camera_GetCvarValue( "cg_thirdPersonRange" ) );
	s_camera.heightSlider.curvalue       = UI_ClampCvar( -80, 80, Camera_GetCvarValue( "cg_thirdPersonHeight" ) );
	s_camera.slideSlider.curvalue        = UI_ClampCvar( -150, 150, Camera_GetCvarValue( "cg_thirdPersonSlide" ) );
	s_camera.angleSlider.curvalue        = UI_ClampCvar( 0, 359, Camera_GetCvarValue( "cg_thirdPersonAngle" ) );
	s_camera.lockedRangeSlider.curvalue  = UI_ClampCvar( 15, 180, Camera_GetCvarValue( "cg_lockedRange" ) );
	s_camera.lockedHeightSlider.curvalue = UI_ClampCvar( -80, 80, Camera_GetCvarValue( "cg_lockedHeight" ) );
	s_camera.lockedSlideSlider.curvalue  = UI_ClampCvar( -150, 150, Camera_GetCvarValue( "cg_lockedSlide" ) );
	s_camera.lockedAngleSlider.curvalue  = UI_ClampCvar( 0, 359, Camera_GetCvarValue( "cg_lockedAngle" ) );
}

static void Camera_StoreOriginalPreviewState( void )
{
	s_camera.originalThirdPersonRange = trap_Cvar_VariableValue( "cg_thirdPersonRange" );
	s_camera.originalThirdPersonHeight = trap_Cvar_VariableValue( "cg_thirdPersonHeight" );
	s_camera.originalThirdPersonSlide = trap_Cvar_VariableValue( "cg_thirdPersonSlide" );
	s_camera.originalThirdPersonAngle = trap_Cvar_VariableValue( "cg_thirdPersonAngle" );
}

static void Camera_ApplyNormalPreview( void )
{
	s_camera.previewLocked = qfalse;
	trap_Cvar_SetValue( "cg_cameraMenuPreviewLocked", 0 );
	trap_Cvar_SetValue( "cg_thirdPersonRange", s_camera.rangeSlider.curvalue );
	trap_Cvar_SetValue( "cg_thirdPersonHeight", s_camera.heightSlider.curvalue );
	trap_Cvar_SetValue( "cg_thirdPersonSlide", s_camera.slideSlider.curvalue );
	trap_Cvar_SetValue( "cg_thirdPersonAngle", s_camera.angleSlider.curvalue );
}

static void Camera_ApplyLockedPreview( void )
{
	s_camera.previewLocked = qtrue;
	trap_Cvar_SetValue( "cg_lockedRange", s_camera.lockedRangeSlider.curvalue );
	trap_Cvar_SetValue( "cg_lockedHeight", s_camera.lockedHeightSlider.curvalue );
	trap_Cvar_SetValue( "cg_lockedSlide", s_camera.lockedSlideSlider.curvalue );
	trap_Cvar_SetValue( "cg_lockedAngle", s_camera.lockedAngleSlider.curvalue );
	trap_Cvar_SetValue( "cg_cameraMenuPreviewLocked", 1 );
}

static void Camera_SaveConfig( void )
{
	trap_Cvar_SetValue( "cg_thirdPersonRange", s_camera.rangeSlider.curvalue );
	trap_Cvar_SetValue( "cg_thirdPersonHeight", s_camera.heightSlider.curvalue );
	trap_Cvar_SetValue( "cg_thirdPersonSlide", s_camera.slideSlider.curvalue );
	trap_Cvar_SetValue( "cg_thirdPersonAngle", s_camera.angleSlider.curvalue );
	trap_Cvar_SetValue( "cg_lockedRange", s_camera.lockedRangeSlider.curvalue );
	trap_Cvar_SetValue( "cg_lockedHeight", s_camera.lockedHeightSlider.curvalue );
	trap_Cvar_SetValue( "cg_lockedSlide", s_camera.lockedSlideSlider.curvalue );
	trap_Cvar_SetValue( "cg_lockedAngle", s_camera.lockedAngleSlider.curvalue );
}

static void Camera_RestoreOriginalPreviewState( void )
{
	trap_Cvar_SetValue( "cg_thirdPersonRange", s_camera.originalThirdPersonRange );
	trap_Cvar_SetValue( "cg_thirdPersonHeight", s_camera.originalThirdPersonHeight );
	trap_Cvar_SetValue( "cg_thirdPersonSlide", s_camera.originalThirdPersonSlide );
	trap_Cvar_SetValue( "cg_thirdPersonAngle", s_camera.originalThirdPersonAngle );
}

static void Camera_HandleExit( void )
{
	if (s_camera.changesMade)
		Camera_SaveConfig();
	trap_Cvar_SetValue( "cg_cameraMenuPreviewLocked", 0 );
	Camera_ApplyNormalPreview();
	UI_PopMenu();
}

/*
=================
Camera_MenuEvent
=================
*/
static void Camera_MenuEvent( void* ptr, int event ) {

	switch( ((menucommon_s*)ptr)->id ) {
	{
		case ID_BACK:
			if (event == QM_ACTIVATED)
			{
				Camera_HandleExit();
			}
			break;
		case ID_RANGE:
		case ID_HEIGHT:
		case ID_SLIDE:
		case ID_ANGLE:
			if (event == QM_ACTIVATED)
			{
				s_camera.changesMade = qtrue;
				Camera_ApplyNormalPreview();
			}
			break;
		case ID_LOCKED_RANGE:
		case ID_LOCKED_HEIGHT:
		case ID_LOCKED_SLIDE:
		case ID_LOCKED_ANGLE:
			if (event == QM_ACTIVATED)
			{
				s_camera.changesMade = qtrue;
				Camera_ApplyLockedPreview();
			}
			break;
		}
	}
}

/*
=================
Camera_MenuKey
=================
*/
static sfxHandle_t Camera_MenuKey( int key )
{
	if ( key == K_ESCAPE ) {
		Camera_HandleExit();
		return menu_out_sound;
	}

	return Menu_DefaultKey( &s_camera.menu, key );
}

/*
=================
Camera_MenuInit
=================
*/
static void Camera_MenuInit( void )
{
	// zero set all our globals
	memset( &s_camera, 0 ,sizeof(camera_t) );

	Camera_Cache();

	s_camera.menu.wrapAround		= qtrue;
	s_camera.menu.fullscreen		= qfalse;
	s_camera.menu.key			= Camera_MenuKey;

	s_camera.banner.generic.type	= MTYPE_BTEXT;
	s_camera.banner.generic.flags	= QMF_CENTER_JUSTIFY;
	s_camera.banner.generic.x		= 320;
	s_camera.banner.generic.y		= 16;
	s_camera.banner.string			= "CAMERA";
	s_camera.banner.color			= color_white;
	s_camera.banner.style			= UI_CENTER|UI_DROPSHADOW;

	s_camera.lockedBanner.generic.type	= MTYPE_TEXT;
	s_camera.lockedBanner.generic.flags	= QMF_SMALLFONT;
	s_camera.lockedBanner.generic.x		= 70;
	s_camera.lockedBanner.generic.y		= 125;
	s_camera.lockedBanner.string			= "Locked camera";
	s_camera.lockedBanner.color			= color_white;
	s_camera.lockedBanner.style			= UI_SMALLFONT|UI_DROPSHADOW;

	s_camera.rangeSlider.generic.type		= MTYPE_SLIDER;
	s_camera.rangeSlider.generic.name		= "Range:";
	s_camera.rangeSlider.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_camera.rangeSlider.generic.callback	= Camera_MenuEvent;
	s_camera.rangeSlider.generic.id			= ID_RANGE;
	s_camera.rangeSlider.generic.x			= 70;
	s_camera.rangeSlider.generic.y			= 40;
	s_camera.rangeSlider.minvalue			= 15;
    s_camera.rangeSlider.maxvalue			= 180;

	s_camera.heightSlider.generic.type		= MTYPE_SLIDER;
	s_camera.heightSlider.generic.name		= "Height:";
	s_camera.heightSlider.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_camera.heightSlider.generic.callback	= Camera_MenuEvent;
	s_camera.heightSlider.generic.id		= ID_HEIGHT;
	s_camera.heightSlider.generic.x			= 70;
	s_camera.heightSlider.generic.y			= 60;
	s_camera.heightSlider.minvalue			= -80;
    s_camera.heightSlider.maxvalue			= 80;

	s_camera.slideSlider.generic.type		= MTYPE_SLIDER;
	s_camera.slideSlider.generic.name		= "Slide:";
	s_camera.slideSlider.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_camera.slideSlider.generic.callback	= Camera_MenuEvent;
	s_camera.slideSlider.generic.id			= ID_SLIDE;
	s_camera.slideSlider.generic.x			= 70;
	s_camera.slideSlider.generic.y			= 80;
	s_camera.slideSlider.minvalue			= -150;
    s_camera.slideSlider.maxvalue			= 150;

	s_camera.angleSlider.generic.type		= MTYPE_SLIDER;
	s_camera.angleSlider.generic.name		= "Angle:";
	s_camera.angleSlider.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_camera.angleSlider.generic.callback	= Camera_MenuEvent;
	s_camera.angleSlider.generic.id			= ID_ANGLE;
	s_camera.angleSlider.generic.x			= 70;
	s_camera.angleSlider.generic.y			= 100;
	s_camera.angleSlider.minvalue			= 0;
    s_camera.angleSlider.maxvalue			= 359;


	s_camera.lockedRangeSlider.generic.type		= MTYPE_SLIDER;
	s_camera.lockedRangeSlider.generic.name		= "Locked range:";
	s_camera.lockedRangeSlider.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_camera.lockedRangeSlider.generic.callback	= Camera_MenuEvent;
	s_camera.lockedRangeSlider.generic.id		= ID_LOCKED_RANGE;
	s_camera.lockedRangeSlider.generic.x			= 70;
	s_camera.lockedRangeSlider.generic.y			= 145;
	s_camera.lockedRangeSlider.minvalue			= 15;
	s_camera.lockedRangeSlider.maxvalue			= 180;

	s_camera.lockedHeightSlider.generic.type		= MTYPE_SLIDER;
	s_camera.lockedHeightSlider.generic.name		= "Locked height:";
	s_camera.lockedHeightSlider.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_camera.lockedHeightSlider.generic.callback	= Camera_MenuEvent;
	s_camera.lockedHeightSlider.generic.id		= ID_LOCKED_HEIGHT;
	s_camera.lockedHeightSlider.generic.x			= 70;
	s_camera.lockedHeightSlider.generic.y			= 165;
	s_camera.lockedHeightSlider.minvalue			= -80;
	s_camera.lockedHeightSlider.maxvalue			= 80;

	s_camera.lockedSlideSlider.generic.type		= MTYPE_SLIDER;
	s_camera.lockedSlideSlider.generic.name		= "Locked slide:";
	s_camera.lockedSlideSlider.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_camera.lockedSlideSlider.generic.callback	= Camera_MenuEvent;
	s_camera.lockedSlideSlider.generic.id		= ID_LOCKED_SLIDE;
	s_camera.lockedSlideSlider.generic.x			= 70;
	s_camera.lockedSlideSlider.generic.y			= 185;
	s_camera.lockedSlideSlider.minvalue			= -150;
	s_camera.lockedSlideSlider.maxvalue			= 150;

	s_camera.lockedAngleSlider.generic.type		= MTYPE_SLIDER;
	s_camera.lockedAngleSlider.generic.name		= "Locked angle:";
	s_camera.lockedAngleSlider.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_camera.lockedAngleSlider.generic.callback	= Camera_MenuEvent;
	s_camera.lockedAngleSlider.generic.id		= ID_LOCKED_ANGLE;
	s_camera.lockedAngleSlider.generic.x			= 70;
	s_camera.lockedAngleSlider.generic.y			= 205;
	s_camera.lockedAngleSlider.minvalue			= 0;
	s_camera.lockedAngleSlider.maxvalue			= 359;

	s_camera.back.generic.type			= MTYPE_BITMAP;
	s_camera.back.generic.name			= ART_BACK0;
	s_camera.back.generic.flags			= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_camera.back.generic.x				= 640-128;
	s_camera.back.generic.y				= 480-64;
	s_camera.back.generic.id			= ID_BACK;
	s_camera.back.generic.callback		= Camera_MenuEvent;
	s_camera.back.width					= 128;
	s_camera.back.height				= 64;
	s_camera.back.focuspic				= ART_BACK1;

	Menu_AddItem( &s_camera.menu, &s_camera.banner );
	Menu_AddItem( &s_camera.menu, &s_camera.lockedBanner );

	Menu_AddItem( &s_camera.menu, &s_camera.rangeSlider );
	Menu_AddItem( &s_camera.menu, &s_camera.heightSlider );
	Menu_AddItem( &s_camera.menu, &s_camera.slideSlider );
	Menu_AddItem( &s_camera.menu, &s_camera.angleSlider );
	Menu_AddItem( &s_camera.menu, &s_camera.lockedRangeSlider );
	Menu_AddItem( &s_camera.menu, &s_camera.lockedHeightSlider );
	Menu_AddItem( &s_camera.menu, &s_camera.lockedSlideSlider );
	Menu_AddItem( &s_camera.menu, &s_camera.lockedAngleSlider );

	Menu_AddItem( &s_camera.menu, &s_camera.back );

	// initialize the configurable cvars
	Camera_InitCvars();

	// initialize the current config
	Camera_GetConfig();
	Camera_StoreOriginalPreviewState();
	trap_Cvar_SetValue( "cg_cameraMenuPreviewLocked", 0 );
}

/*
=================
Camera_Cache
=================
*/
void Camera_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_BACK0 );
	trap_R_RegisterShaderNoMip( ART_BACK1 );
}


/*
=================
UI_CameraMenu
=================
*/
void UI_CameraMenu( void ) {
	Camera_MenuInit();
	UI_PushMenu( &s_camera.menu );
}
