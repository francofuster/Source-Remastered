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

#include "../../Shared/q_shared.h"
#include "../../Shared/qcommon.h"
#include "sys_local.h"

#include <stdio.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

/*
==================
CON_Shutdown
==================
*/
void CON_Shutdown( void )
{
}

/*
==================
CON_Init
==================
*/
void CON_Init( void )
{
}

/*
==================
CON_Input
==================
*/
char *CON_Input( void )
{
	return NULL;
}

/*
==================
CON_Print
==================
*/
void CON_Print( const char *msg )
{
#ifdef __ANDROID__
	// logcat wants whole lines and has no use for Q3 colour codes
	static char line[ 1024 ];
	static int length = 0;

	while( *msg )
	{
		if( Q_IsColorString( msg ) )
		{
			msg += 2;
			continue;
		}

		if( *msg != '\n' )
			line[ length++ ] = *msg;

		if( *msg == '\n' || length == sizeof( line ) - 1 )
		{
			line[ length ] = '\0';
			__android_log_write( ANDROID_LOG_INFO, "ZEQ2", line );
			length = 0;
		}

		msg++;
	}
#else
	if( com_ansiColor && com_ansiColor->integer )
		Sys_AnsiColorPrint( msg );
	else
		fputs( msg, stderr );
#endif
}
