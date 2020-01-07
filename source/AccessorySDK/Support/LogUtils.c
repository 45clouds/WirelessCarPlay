/*
	File:    	LogUtils.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ”Public 
	Software”, and is further subject to your agreement to the following additional terms, and your 
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
	you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive 
	license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use, 
	reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and 
	redistribute the Apple Software, with or without modifications, in binary form. While you may not 
	redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
	form, you must retain this notice and the following text and disclaimers in all such redistributions
	of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
	used to endorse or promote products derived from the Apple Software without specific prior written
	permission from Apple. Except as expressly stated in this notice, no other rights or licenses, 
	express or implied, are granted by Apple herein, including but not limited to any patent rights that
	may be infringed by your derivative works or by other works in which the Apple Software may be 
	incorporated.  
	
	Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug 
	fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
	Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use, 
	reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
	distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products 
	and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you 
	acknowledge and agree that Apple may exercise the license granted above without the payment of 
	royalties or further consideration to Participant.
	
	The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR 
	IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY 
	AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
	IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES 
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION 
	AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
	(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE 
	POSSIBILITY OF SUCH DAMAGE.
	
	Copyright (C) 2007-2015 Apple Inc. All Rights Reserved.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "LogUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "MiscUtils.h"
#include "PrintFUtils.h"
#include "StringUtils.h"
#include "TickUtils.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <ctype.h>
#endif

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	#include <asl.h>
	#include <dlfcn.h>
	#include <notify.h>
#endif

#if( TARGET_OS_POSIX )
	#include <fcntl.h>
	#include <pthread.h>
	#include <sys/stat.h>
	#include <syslog.h>
#endif

#if( TARGET_OS_WINDOWS && !TARGET_OS_WINDOWS_CE )
	#include <direct.h>
	#include <fcntl.h>
	#include <io.h>
#endif

#if( LOGUTILS_OSLOG_ENABLED )
	#include <os/log.h>
#endif

//===========================================================================================================================
//	Types
//===========================================================================================================================

typedef uint32_t		LogControlFlags;
#define kLogControlFlags_None			0			// No flags.
#define kLogControlFlags_Internal		( 1U << 0 )	// Called internally. Callback output allowed.
#define kLogControlFlags_FromPrefs		( 1U << 1 )	// Called for applying from prefs.
#define kLogControlFlags_Defaults		( 1U << 2 )	// Called for applying default settings.

typedef struct LogAction		LogAction;
struct LogAction
{
	LogAction *		next;
	char *			name;
	char *			variable;
	char *			value;
};

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void	_LogUtils_FreeAction( LogAction *inAction );

#if( LOGUTILS_CF_DISTRIBUTED_NOTIFICATIONS )
	static void	_LogUtils_EnsureCFNotificationsInitialized( void );
	static void
		_LogUtils_HandleCFNotification( 
			CFNotificationCenterRef	inCenter, 
			void *					inObserver, 
			CFStringRef				inName, 
			const void *			inObject, 
			CFDictionaryRef			inUserInfo );
#endif
#if( LOGUTILS_CF_PREFERENCES )
	static void		_LogUtils_ReadCFPreferences( LogControlFlags inFlags );
	static OSStatus	_LogUtils_WriteCFPreferences( void );
#endif

static OSStatus		_LogControlLocked( const char *inCmd, LogControlFlags inFlags );
#if( LOGUTILS_CF_ENABLED )
	static OSStatus	_LogControlLockedCF( CFStringRef inStr, LogControlFlags inFlags );
#endif

static OSStatus		_LogCategory_ApplyActions( LogCategory *inCategory );
static OSStatus		_LogCategory_ApplyAction_Output( LogCategory *inCategory, int inOutputID, LogAction *inAction );
static char *		_LULevelToString( LogLevel inLevel, char *inBuf, size_t inLen );

static OSStatus		_LogOutputCreate( const char *inConfigStr, LogOutput **outOutput );
static void			_LogOutputDelete( LogOutput *inOutput );
static void			_LogOutputDeleteUnused( void );

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	static OSStatus	_LogOutputASL_Setup( LogOutput *inOutput, const char *inParams );
	static void		_LogOutputASL_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
#endif

static OSStatus		_LogOutputCallBack_Setup( LogOutput *inOutput, const char *inParams );
static void			_LogOutputCallBack_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );



#if( DEBUG_FPRINTF_ENABLED )
	static OSStatus	_LogOutputFile_Setup( LogOutput *inOutput, const char *inParams );
	static void		_LogOutputFile_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
	static OSStatus	_LogOutputFile_BackupLogFiles( LogOutput *inOutput );
	static OSStatus	_LogOutputFile_CopyLogFile( const char *inSrcPath, const char *inDstPath );
#endif

#if( DEBUG_IDEBUG_ENABLED )
	static OSStatus	_LogOutputiDebug_Setup( LogOutput *inOutput, const char *inParams );
	static void		_LogOutputiDebug_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
#endif

#if( DEBUG_MAC_OS_X_IOLOG_ENABLED )
	static void		_LogOutputIOLog_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
#endif

#if( DEBUG_KPRINTF_ENABLED )
	static void		_LogOutputKPrintF_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
#endif

#if( LOGUTILS_OSLOG_ENABLED )
	static OSStatus	_LogOutputOSLog_Setup( LogOutput *inOutput, const char *inParams );
	static void		_LogOutputOSLog_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
#endif

#if( TARGET_OS_THREADX )
	static void		_LogOutputThreadX_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
#endif

#if( TARGET_OS_POSIX )
	static OSStatus	_LogOutputSysLog_Setup( LogOutput *inOutput, const char *inParams );
	static void		_LogOutputSysLog_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
#endif

#if( TARGET_OS_WINDOWS )
	static void		_LogOutputWindowsDebugger_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
#endif

#if( DEBUG_WINDOWS_EVENT_LOG_ENABLED )
	static OSStatus	_LogOutputWindowsEventLog_Setup( LogOutput *inOutput, const char *inParams );
	static void		_LogOutputWindowsEventLog_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
#endif

#if( TARGET_OS_WINDOWS_KERNEL )
	static void		_LogOutputWindowsKernel_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
#endif

#if( TARGET_OS_POSIX )
	static Boolean	_LogOutput_IsStdErrMappedToDevNull( void );
#else
	#define _LogOutput_IsStdErrMappedToDevNull()		0
#endif

#if( TARGET_OS_WINDOWS )
	static TCHAR *
		_LogOutput_CharToTCharString( 
			const char *	inCharString, 
			size_t 			inCharCount, 
			TCHAR *			outTCharString, 
			size_t 			inTCharCountMax, 
			size_t *		outTCharCount );
	static void	_LogOutput_EnableWindowsConsole( void );
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

MinimalMutexDefine( gLogUtilsLock );

Boolean							gLogUtilsInitializing = false;
#if( LOGUTILS_CF_ENABLED )
	static Boolean				gLogCFInitialized	= false;
#endif
#if( LOGUTILS_CF_DISTRIBUTED_NOTIFICATIONS )
	static Boolean				gLogCFNotificationInitialized	= false;
	static CFStringRef			gLogCFNotificationObserver		= CFSTR( "LogUtilsCFObserver" );
	static CFStringRef			gLogCFNotificationPoster		= CFSTR( "LogUtilsCFPoster" );
#endif
#if( LOGUTILS_CF_PREFERENCES )
	static CFStringRef			gLogCFPrefsAppID		= NULL;
	static CFStringRef			gLogCFLastControlPref	= NULL;
#endif
#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	static int					gLogUtilsMCDefaultsChangedToken = -1;
#endif

static LogCategory *			gLogCategoryList	= NULL;
static LogAction *				gLogActionList		= NULL;
static LogOutput *				gLogOutputList		= NULL;

ulog_define( LogUtils, kLogLevelAll, kLogFlags_PrintTime, "LogUtils", NULL );

//===========================================================================================================================
//	LogUtils_EnsureInitialized
//===========================================================================================================================

OSStatus	LogUtils_EnsureInitialized( void )
{
	MinimalMutexEnsureInitialized( gLogUtilsLock );
#if( LOGUTILS_CF_ENABLED )
	if( gLogCFInitialized || gLogUtilsInitializing ) return( kNoErr ); // Avoid recursion.
	MinimalMutexLock( gLogUtilsLock );
	gLogUtilsInitializing = true;
	if( !gLogCFInitialized )
	{
		gLogCFInitialized = true; // Mark initialized first to handle recursive invocation.
		
		#if( TARGET_OS_DARWIN && LOGUTILS_CF_PREFERENCES && !COMMON_SERVICES_NO_CORE_SERVICES )
			// Re-read if ManagedConfiguration changed a pref.
			
			notify_register_dispatch( "com.apple.managedconfiguration.defaultsdidchange", &gLogUtilsMCDefaultsChangedToken, 
				dispatch_get_main_queue(), 
			^( int inToken )
			{
				(void) inToken;
				
				MinimalMutexLock( gLogUtilsLock );
				_LogUtils_ReadCFPreferences( kLogControlFlags_None );
				MinimalMutexUnlock( gLogUtilsLock );
			} );
		#endif
		
		#if( LOGUTILS_CF_DISTRIBUTED_NOTIFICATIONS )
			_LogUtils_EnsureCFNotificationsInitialized();
		#endif
		
		#if( LOGUTILS_CF_PREFERENCES )
			_LogUtils_ReadCFPreferences( kLogControlFlags_None );
		#endif
	}
	gLogUtilsInitializing = false;
	MinimalMutexUnlock( gLogUtilsLock );
#endif
	
	return( kNoErr );
}

//===========================================================================================================================
//	LogUtils_Finalize
//===========================================================================================================================

void	LogUtils_Finalize( void )
{
	LogCategory *		category;
	LogAction *			action;
	LogOutput *			output;
	
#if( LOGUTILS_CF_DISTRIBUTED_NOTIFICATIONS )
	CFNotificationCenterRef		dnc;
	
	dnc = CFNotificationCenterGetDistributedCenter();
	if( dnc ) CFNotificationCenterRemoveEveryObserver( dnc, gLogCFNotificationObserver );
#endif
	
	for( category = gLogCategoryList; category; category = category->next )
	{
		category->level   = kLogLevelUninitialized;
		category->output1 = NULL;
		category->output2 = NULL;
	}
	while( ( action = gLogActionList ) != NULL )
	{
		gLogActionList = action->next;
		_LogUtils_FreeAction( action );
	}
	while( ( output = gLogOutputList ) != NULL )
	{
		gLogOutputList = output->next;
		_LogOutputDelete( output );
	}
	
#if( LOGUTILS_CF_PREFERENCES )
	ForgetCF( &gLogCFPrefsAppID );
	ForgetCF( &gLogCFLastControlPref );
#endif
#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	notify_forget( &gLogUtilsMCDefaultsChangedToken );
#endif
	MinimalMutexEnsureFinalized( gLogUtilsLock );
}

//===========================================================================================================================
//	_LogUtils_FreeAction
//===========================================================================================================================

static void	_LogUtils_FreeAction( LogAction *inAction )
{
	ForgetMem( &inAction->name );
	ForgetMem( &inAction->variable );
	ForgetMem( &inAction->value );
	free( inAction );
}

//===========================================================================================================================
//	_LogUtils_EnsureCFNotificationsInitialized
//
//	Note: LogUtils lock must be held.
//===========================================================================================================================

#if( LOGUTILS_CF_DISTRIBUTED_NOTIFICATIONS )
static void	_LogUtils_EnsureCFNotificationsInitialized( void )
{
	CFNotificationCenterRef		dnc;
	
	if( gLogCFNotificationInitialized ) return;
	dnc = CFNotificationCenterGetDistributedCenter();
	if( !dnc ) return;
	
	CFNotificationCenterAddObserver( dnc, gLogCFNotificationObserver, _LogUtils_HandleCFNotification, 
		CFSTR( kLogUtilsRequestNotification ), NULL, CFNotificationSuspensionBehaviorDeliverImmediately );
	gLogCFNotificationInitialized = true;
}
#endif

//===========================================================================================================================
//	_LogUtils_HandleCFNotification
//===========================================================================================================================

#if( LOGUTILS_CF_DISTRIBUTED_NOTIFICATIONS )
static void
	_LogUtils_HandleCFNotification( 
		CFNotificationCenterRef	inCenter, 
		void *					inObserver, 
		CFStringRef				inName, 
		const void *			inObject, 
		CFDictionaryRef			inUserInfo )
{
	CFStringRef					controlStr;
	CFMutableDictionaryRef		userInfo = NULL;
	OSStatus					err;
	char *						showCStr;
	CFStringRef					showCFStr;
	
	(void) inObserver;
	(void) inName;
	(void) inObject;
	
	// Change the config if a control string is specified.
	
	if( inUserInfo )
	{
		controlStr = (CFStringRef) CFDictionaryGetValue( inUserInfo, CFSTR( kLogUtilsKey_LogConfig ) );
		if( controlStr )
		{
			require( CFIsType( controlStr, CFString ), exit );
			LogControlCF( controlStr );
		}
	}
	
	// Post a notification with the latest config.
	
	userInfo = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require( userInfo, exit );
	
	showCStr = NULL;
	err = LogShow( &showCStr );
	require_noerr( err, exit );
	showCFStr = CFStringCreateWithCString( NULL, showCStr, kCFStringEncodingUTF8 );
	free( showCStr );
	require( showCFStr, exit );
	CFDictionarySetValue( userInfo, CFSTR( kLogUtilsKey_LogConfig ), showCFStr );
	CFRelease( showCFStr );
	
	CFNotificationCenterPostNotificationWithOptions( inCenter, CFSTR( kLogUtilsAckNotification ), 
		gLogCFNotificationPoster, userInfo, kCFNotificationDeliverImmediately | kCFNotificationPostToAllSessions );
	
exit:
	CFReleaseNullSafe( userInfo );
}
#endif

//===========================================================================================================================
//	_LogUtils_ReadCFPreferences
//
//	Note: LogUtils lock must be held.
//===========================================================================================================================

#if( LOGUTILS_CF_PREFERENCES )
static void	_LogUtils_ReadCFPreferences( LogControlFlags inFlags )
{
	CFStringRef		appID;
	CFStringRef		cfStr;
	
	appID = gLogCFPrefsAppID ? gLogCFPrefsAppID : kCFPreferencesCurrentApplication;
	CFPreferencesAppSynchronize( appID );
	cfStr = (CFStringRef) CFPreferencesCopyAppValue( CFSTR( kLogUtilsKey_LogConfig ), appID );
	if( cfStr )
	{
		if( ( CFGetTypeID( cfStr ) == CFStringGetTypeID() ) && 
			( ( inFlags & kLogControlFlags_Defaults ) || !CFEqualNullSafe( cfStr, gLogCFLastControlPref ) ) )
		{
			_LogControlLockedCF( cfStr, inFlags | kLogControlFlags_FromPrefs );
			ReplaceCF( &gLogCFLastControlPref, cfStr );
		}
		CFRelease( cfStr );
	}
}
#endif

//===========================================================================================================================
//	_LogUtils_WriteCFPreferences
//
//	Note: LogUtils lock must be held.
//===========================================================================================================================

#if( LOGUTILS_CF_PREFERENCES )
static OSStatus	_LogUtils_WriteCFPreferences( void )
{
	OSStatus		err;
	char *			configStr = NULL;
	LogAction *		action;
	CFStringRef		configCFStr;
	CFStringRef		appID;
	
	for( action = gLogActionList; action; action = action->next )
	{
		// Don't allow a callback function pointer to be written out.
		
		if( ( ( stricmp( action->variable, "output" )  == 0 ) || 
			  ( stricmp( action->variable, "output2" ) == 0 ) ) && 
			( stricmp_prefix( action->value, "callback" ) == 0 ) )
		{
			continue;
		}
		
		AppendPrintF( &configStr, "%s%s:%s=%s", ( action == gLogActionList ) ? "" : ",", 
			action->name, action->variable, action->value );
	}
	require_action_quiet( configStr, exit, err = kNoErr );
	
	configCFStr = CFStringCreateWithCString( NULL, configStr, kCFStringEncodingUTF8 );
	require_action_quiet( configCFStr, exit, err = kNoMemoryErr );
	
	appID = gLogCFPrefsAppID ? gLogCFPrefsAppID : kCFPreferencesCurrentApplication;
	CFPreferencesSetAppValue( CFSTR( kLogUtilsKey_LogConfig ), configCFStr, appID );
	CFPreferencesAppSynchronize( appID );
	ReplaceCF( &gLogCFLastControlPref, configCFStr );
	CFRelease( configCFStr );
	err = kNoErr;
	
exit:
	FreeNullSafe( configStr );
	return( err );
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	LogControl
//===========================================================================================================================

OSStatus	LogControl( const char *inCmd )
{
	OSStatus		err;
	
	LogUtils_EnsureInitialized();
	MinimalMutexLock( gLogUtilsLock );
	err = _LogControlLocked( inCmd, kLogControlFlags_None );
	MinimalMutexUnlock( gLogUtilsLock );
	return( err );
}

//===========================================================================================================================
//	_LogControlLocked
//
//	Note: assumes the lock is held.
//===========================================================================================================================

static OSStatus	_LogControlLocked( const char *inCmd, LogControlFlags inFlags )
{
	OSStatus			err;
	int					isDefault = false;
	int					persist   = false;
	char				c;
	const char *		namePtr;
	size_t				nameLen;
	const char *		variablePtr;
	size_t				variableLen;
	const char *		valuePtr;
	size_t				valueLen;
	LogAction **		actionNext;
	LogAction *			action;
	LogAction *			actionNew = NULL;
	
	if( inCmd )
	{
		isDefault = ( *inCmd == '?' );
		if( isDefault ) ++inCmd;
		
		persist = ( *inCmd == '+' );
		if( persist ) ++inCmd;
		
		// Ignore default/persist prefixes if they come from prefs (probably accidental).
		
		if( inFlags & kLogControlFlags_FromPrefs )
		{
			isDefault = false;
			persist   = false;
		}
		
		// Both default and persist are set, clear the prefs.
		
		if( isDefault && persist )
		{
			#if( LOGUTILS_CF_PREFERENCES )
				CFStringRef		appID;
				
				appID = gLogCFPrefsAppID ? gLogCFPrefsAppID : kCFPreferencesCurrentApplication;
				CFPreferencesSetAppValue( CFSTR( kLogUtilsKey_LogConfig ), NULL, appID );
			#endif
			err = kNoErr;
			goto exit;
		}
	}
	
	// If the command is NULL or empty, it means to delete all actions.
	
	if( !inCmd || ( *inCmd == '\0' ) )
	{
		while( ( action = gLogActionList ) != NULL )
		{
			gLogActionList = action->next;
			_LogUtils_FreeAction( action );
		}
		#if( LOGUTILS_CF_PREFERENCES )
			if( !isDefault && persist )
			{
				CFStringRef		appID;
				
				appID = gLogCFPrefsAppID ? gLogCFPrefsAppID : kCFPreferencesCurrentApplication;
				CFPreferencesSetAppValue( CFSTR( kLogUtilsKey_LogConfig ), NULL, appID );
				CFPreferencesAppSynchronize( appID );
			}
		#endif
		err = kNoErr;
		goto exit;
	}
	
	// Parse actions from the control string. This appends each unique action, replacing duplicate actions.
	
	while( *inCmd != '\0' )
	{
		// Parse a name:variable=value segment.
		
		namePtr = inCmd;
		while( ( ( c = *inCmd ) != '\0' ) && ( c != ':' ) ) ++inCmd;
		require_action_quiet( c != '\0', exit, err = kMalformedErr );
		nameLen = (size_t)( inCmd - namePtr );
		++inCmd;
		
		variablePtr = inCmd;
		while( ( ( c = *inCmd ) != '\0' ) && ( c != '=' ) ) ++inCmd;
		require_action_quiet( c != '\0', exit, err = kMalformedErr );
		variableLen = (size_t)( inCmd - variablePtr );
		++inCmd;
		
		valuePtr = inCmd;
		while( ( ( c = *inCmd ) != '\0' ) && ( c != ',' ) ) ++inCmd;
		valueLen = (size_t)( inCmd - valuePtr );
		if( c == ',' ) ++inCmd;
		
		// Don't allow a callback function pointer to be specified in a control string.
		
		if( !( inFlags & kLogControlFlags_Internal ) && 
			( ( strnicmpx( variablePtr, variableLen, "output" )  == 0 ) || 
			  ( strnicmpx( variablePtr, variableLen, "output2" ) == 0 ) ) &&
			( strnicmp_prefix( valuePtr, valueLen, "callback" ) == 0 ) )
		{
			continue;
		}
		
		// Search for an action with the same name/variable. If found, replace. If not found, add.
		
		for( actionNext = &gLogActionList; ( action = *actionNext ) != NULL; actionNext = &action->next )
		{
			if( ( strnicmpx( namePtr,		nameLen,		action->name )		== 0 ) && 
				( strnicmpx( variablePtr,	variableLen,	action->variable )	== 0 ) )
			{
				break;
			}
		}
		if( !action )
		{
			actionNew = (LogAction *) calloc( 1, sizeof( *actionNew ) );
			require_action_quiet( actionNew, exit, err = kNoMemoryErr );
			action = actionNew;
		}
		
		// Replace all the strings to handle case differences.
		
		err = ReplaceString( &action->name, NULL, namePtr, nameLen );
		require_noerr_quiet( err, exit );
		
		err = ReplaceString( &action->variable, NULL, variablePtr, variableLen );
		require_noerr_quiet( err, exit );
		
		err = ReplaceString( &action->value, NULL, valuePtr, valueLen );
		require_noerr_quiet( err, exit );
		
		if( actionNew )
		{
			*actionNext = actionNew;
			actionNew = NULL;
		}
	}
	
	// Re-apply all actions to account for the new action(s).
	
	err = _LogCategory_ApplyActions( NULL );
	require_noerr_quiet( err, exit );
	
#if( LOGUTILS_CF_PREFERENCES )
	if( !( inFlags & kLogControlFlags_FromPrefs ) )
	{
		if( isDefault )		_LogUtils_ReadCFPreferences( inFlags | kLogControlFlags_Defaults );
		else if( persist )	_LogUtils_WriteCFPreferences();
	}
#endif
	
exit:
	if( actionNew ) _LogUtils_FreeAction( actionNew );
	return( err );
}

//===========================================================================================================================
//	LogControlCF
//===========================================================================================================================

#if( LOGUTILS_CF_ENABLED )
OSStatus	LogControlCF( CFStringRef inCmd )
{
	OSStatus		err;
	
	LogUtils_EnsureInitialized();
	MinimalMutexLock( gLogUtilsLock );
	err = _LogControlLockedCF( inCmd, kLogControlFlags_None );
	MinimalMutexUnlock( gLogUtilsLock );
	return( err );
}
#endif

//===========================================================================================================================
//	_LogControlLockedCF
//===========================================================================================================================

#if( LOGUTILS_CF_ENABLED )
static OSStatus	_LogControlLockedCF( CFStringRef inStr, LogControlFlags inFlags )
{
	OSStatus		err;
	CFRange			range;
	CFIndex			len;
	char *			configStr;
	
	range = CFRangeMake( 0, CFStringGetLength( inStr ) );
	len = CFStringGetMaximumSizeForEncoding( range.length, kCFStringEncodingUTF8 );
	configStr = (char *) malloc( (size_t)( len + 1 ) );
	require_action_quiet( configStr, exit, err = kNoMemoryErr );
	
	range.location = CFStringGetBytes( inStr, range, kCFStringEncodingUTF8, 0, false, (uint8_t *) configStr, len, &len );
	require_action_quiet( range.location == range.length, exit, err = kUnknownErr );
	require_action_quiet( len > 0, exit, err = kNoErr );
	configStr[ len ] = '\0';
	
	err = _LogControlLocked( configStr, inFlags );
	
exit:
	FreeNullSafe( configStr );
	return( err );
}
#endif

//===========================================================================================================================
//	LogSetAppID
//===========================================================================================================================

#if( LOGUTILS_CF_PREFERENCES )
void	LogSetAppID( CFStringRef inAppID )
{
	MinimalMutexEnsureInitialized( gLogUtilsLock );
	MinimalMutexLock( gLogUtilsLock );
		ReplaceCF( &gLogCFPrefsAppID, inAppID );
	MinimalMutexUnlock( gLogUtilsLock );
}
#endif

//===========================================================================================================================
//	LogSetOutputCallback
//===========================================================================================================================

OSStatus	LogSetOutputCallback( const char *inCategoryRegex, int inOutputNum, LogOutputCallBack inCallback, void *inContext )
{
	OSStatus		err;
	char *			cmd;
	
	cmd = NULL;
	ASPrintF( &cmd, "%s:output%?d=callback;func=%p;arg=%p", inCategoryRegex ? inCategoryRegex : ".*", 
		inOutputNum > 1, inOutputNum, inCallback, inContext );
	require_action( cmd, exit, err = kNoMemoryErr );
	
	LogUtils_EnsureInitialized();
	MinimalMutexLock( gLogUtilsLock );
	err = _LogControlLocked( cmd, kLogControlFlags_Internal );
	MinimalMutexUnlock( gLogUtilsLock );
	free( cmd );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	LogShow
//===========================================================================================================================

OSStatus	LogShow( char **outOutput )
{
	OSStatus			err;
	char *				outputStr;
	LogCategory *		category;
	LogAction *			action;
	int					n;
	char				buf[ 64 ];
	const char *		output1;
	const char *		output2;
	
	MinimalMutexLock( gLogUtilsLock );
	
	outputStr = NULL;
	n = ASPrintF( &outputStr, "=== LogUtils (%s, PID %llu) ===\n", getprogname(), (unsigned long long) getpid() );
	err = ( n > 0 ) ? kNoErr : kNoMemoryErr;
	
	// Categories
	
	if( !err )
	{
		size_t		widestName, widestLevel, width;
		
		widestName  = 0;
		widestLevel = 0;
		for( category = gLogCategoryList; category; category = category->next )
		{
			width = strlen( category->name );
			if( width > widestName ) widestName = width;
			
			width = strlen( _LULevelToString( category->level, buf, sizeof( buf ) ) );
			if( width > widestLevel ) widestLevel = width;
		}
		for( category = gLogCategoryList; category; category = category->next )
		{
			output1 = category->output1 ? category->output1->configStr : NULL;
			if( !output1 ) output1 = "";
			if( stricmp_prefix( output1, "callback" ) == 0 )
			{
				output1 = "callback";
			}
			
			output2 = category->output2 ? category->output2->configStr : NULL;
			if( !output2 ) output2 = "";
			if( stricmp_prefix( output2, "callback" ) == 0 )
			{
				output2 = "callback";
			}
			
			n = AppendPrintF( &outputStr, "  %-*s  L=%-*s  R=%u/%-5llu  O1=%s  O2=%s\n", 
				(int) widestName, category->name, 
				(int) widestLevel, _LULevelToString( category->level, buf, sizeof( buf ) ), 
				category->rateMaxCount, UpTicksToMilliseconds( category->rateInterval ), output1, output2 );
			if( n <= 0 ) { err = kNoMemoryErr; break; }
		}
	}
	
	// Actions
	
	if( !err )
	{
		if( gLogCategoryList && gLogActionList )
		{
			n = AppendPrintF( &outputStr, "\n" );
			if( n <= 0 ) err = kNoMemoryErr;
		}
		for( action = gLogActionList; action; action = action->next )
		{
			// Don't print a callback function pointer.
			
			if( ( ( stricmp( action->variable, "output" )  == 0 ) || 
				  ( stricmp( action->variable, "output2" ) == 0 ) ) && 
				( stricmp_prefix( action->value, "callback" ) == 0 ) )
			{
				continue;
			}
			
			n = AppendPrintF( &outputStr, "  Action: %s:%s=%s\n", action->name, action->variable, action->value );
			if( n <= 0 ) { err = kNoMemoryErr; break; }
		}
	}
	
	MinimalMutexUnlock( gLogUtilsLock );
	
	// Return or print the final string.
	
	if( outputStr )
	{
		if( outOutput ) *outOutput = outputStr;
		else
		{
			lu_ulog( kLogLevelMax, "%s", outputStr );
			free( outputStr );
		}
	}
	else
	{
		if( outOutput == NULL )
		{
			lu_ulog( kLogLevelError, "### ERROR: %#m\n", err );
		}
	}
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_LogCategory_Initialize
//===========================================================================================================================

// Just here for older CoreUtils clients that link to the old name. When all clients are gone, this can be removed.
Boolean	__LogCategory_Initialize( LogCategory *inCategory, LogLevel inLevel );
Boolean	__LogCategory_Initialize( LogCategory *inCategory, LogLevel inLevel )
{
	return( _LogCategory_Initialize( inCategory, inLevel ) );
}

Boolean	_LogCategory_Initialize( LogCategory *inCategory, LogLevel inLevel )
{
	LogLevel		level;
	
	if( gLogUtilsInitializing ) return( false );
	LogUtils_EnsureInitialized();
	MinimalMutexLock( gLogUtilsLock );
	
	if( inCategory->level == kLogLevelUninitialized )
	{
		LogCategory **		next;
		LogCategory *		curr;
		
		inCategory->level = inCategory->initLevel;
		
		for( next = &gLogCategoryList; ( curr = *next ) != NULL; next = &curr->next )
		{
			if( strnicmpx( curr->name, SIZE_MAX, inCategory->name ) > 0 )
			{
				break;
			}
		}
		inCategory->next = *next;
		*next = inCategory;
		
		if( inCategory->initConfig )
		{
			_LogControlLocked( inCategory->initConfig, kLogControlFlags_None );
		}
		_LogCategory_ApplyActions( inCategory );
		if( !inCategory->output1 )
		{
			_LogOutputCreate( "console", &inCategory->output1 );
			if( inCategory->output1 ) ++inCategory->output1->refCount;
		}
	}
	level = inCategory->level;
	
	MinimalMutexUnlock( gLogUtilsLock );
	return( (Boolean)( ( inLevel & kLogLevelMask ) >= level ) );
}

//===========================================================================================================================
//	LogCategory_Remove
//===========================================================================================================================

void	LogCategory_Remove( LogCategory *inCategory )
{
	LogCategory **		next;
	LogCategory *		curr;
	
	if( gLogUtilsInitializing ) return;
	LogUtils_EnsureInitialized();
	MinimalMutexLock( gLogUtilsLock );
	
	for( next = &gLogCategoryList; ( curr = *next ) != NULL; next = &curr->next )
	{
		if( curr == inCategory )
		{
			if( curr->output1 ) --curr->output1->refCount;
			if( curr->output2 ) --curr->output2->refCount;
			*next = curr->next;
			curr->level		= kLogLevelUninitialized;
			curr->output1	= NULL;
			curr->output2	= NULL;
			_LogOutputDeleteUnused();
			break;
		}
	}
	
	MinimalMutexUnlock( gLogUtilsLock );
}

//===========================================================================================================================
//	_LogCategory_ApplyActions
//
//	Note: assumes the lock is held.
//===========================================================================================================================

static OSStatus	_LogCategory_ApplyActions( LogCategory *inCategory )
{
	OSStatus			err;
	LogAction *			action;
	LogCategory *		category;
	LogLevel			level;
	const char *		valueSrc;
	const char *		valueEnd;
	const char *		valueTok;
	size_t				valueLen;
	LogFlags			flags;
	int					outputID;
	
	// Apply level actions.
	
	for( action = gLogActionList; action; action = action->next )
	{
		if( strnicmpx( action->variable, SIZE_MAX, "level" ) != 0 )
			continue;
		
		level = LUStringToLevel( action->value );
		if( level == kLogLevelUninitialized ) continue;
		
		for( category = gLogCategoryList; category; category = category->next )
		{
			if( inCategory && ( inCategory != category ) ) 		continue;
			if( !RegexMatch( action->name, category->name ) )	continue;
			
			category->level = level;
		}
	}
	
	// Apply flag actions.
	
	for( action = gLogActionList; action; action = action->next )
	{
		if( strnicmpx( action->variable, SIZE_MAX, "flags" ) != 0 )
			continue;
		
		flags = 0;
		valueSrc = action->value;
		valueEnd = valueSrc + strlen( valueSrc );
		while( valueSrc < valueEnd )
		{
			for( valueTok = valueSrc; ( valueSrc < valueEnd ) && ( *valueSrc != ';' ); ++valueSrc ) {}
			valueLen = (size_t)( valueSrc - valueTok );
			if( valueSrc < valueEnd ) ++valueSrc;
			
			if(      strnicmpx( valueTok, valueLen, "none" )		== 0 ) flags  = kLogFlags_None;
			else if( strnicmpx( valueTok, valueLen, "time" )		== 0 ) flags |= kLogFlags_PrintTime;
			else if( strnicmpx( valueTok, valueLen, "pid" )			== 0 ) flags |= kLogFlags_PrintPID;
			else if( strnicmpx( valueTok, valueLen, "program" )		== 0 ) flags |= kLogFlags_PrintProgram;
			else if( strnicmpx( valueTok, valueLen, "category" )	== 0 ) flags |= kLogFlags_PrintCategory;
			else if( strnicmpx( valueTok, valueLen, "level" )		== 0 ) flags |= kLogFlags_PrintLevel;
			else if( strnicmpx( valueTok, valueLen, "prefix" )		== 0 ) flags |= kLogFlags_PrintPrefix;
			else if( strnicmpx( valueTok, valueLen, "function" )	== 0 ) flags |= kLogFlags_PrintFunction;
			else continue;
		}
		
		for( category = gLogCategoryList; category; category = category->next )
		{
			if( inCategory && ( inCategory != category ) ) 		continue;
			if( !RegexMatch( action->name, category->name ) )	continue;
			
			if( flags & kLogFlags_PrintPrefix )
			{
				valueEnd = strchr( category->name, '_' );
				if( !valueEnd ) valueEnd = category->name + strlen( category->name );
				category->prefixPtr = category->name;
				category->prefixLen = (int)( valueEnd - category->prefixPtr );
			}
			category->flags = flags;
		}
	}
	
	// Apply rate limiter actions.
	
	for( action = gLogActionList; action; action = action->next )
	{
		uint64_t		intervalTicks;
		uint32_t		maxCount;
		
		if( strnicmpx( action->variable, SIZE_MAX, "rate" ) != 0 )
			continue;
		
		valueSrc = action->value;
		valueEnd = valueSrc + strlen( valueSrc );
		
		maxCount = 0;
		for( ; ( valueSrc < valueEnd ) && isdigit_safe( *valueSrc ); ++valueSrc )
			maxCount = ( maxCount * 10 ) + ( *valueSrc - '0' );
		if( valueSrc < valueEnd ) ++valueSrc;
		
		intervalTicks = 0;
		for( ; ( valueSrc < valueEnd ) && isdigit_safe( *valueSrc ); ++valueSrc )
			intervalTicks = ( intervalTicks * 10 ) + ( *valueSrc - '0' );
		intervalTicks = ( UpTicksPerSecond() * intervalTicks ) / 1000;
		
		for( category = gLogCategoryList; category; category = category->next )
		{
			if( inCategory && ( inCategory != category ) ) 		continue;
			if( !RegexMatch( action->name, category->name ) )	continue;
			
			category->rateInterval = intervalTicks;
			category->rateMaxCount = maxCount;
		}
	}
	
	// Apply output actions.
	
	for( action = gLogActionList; action; action = action->next )
	{
		if(      strnicmpx( action->variable, SIZE_MAX, "output" )  == 0 ) outputID = 1;
		else if( strnicmpx( action->variable, SIZE_MAX, "output2" ) == 0 ) outputID = 2;
		else continue;
		
		for( category = gLogCategoryList; category; category = category->next )
		{
			if( inCategory && ( inCategory != category ) ) 		continue;
			if( !RegexMatch( action->name, category->name ) )	continue;
			
			err = _LogCategory_ApplyAction_Output( category, outputID, action );
			require_noerr_quiet( err, exit );
		}
	}
	_LogOutputDeleteUnused();
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_LogCategory_ApplyAction_Output
//
//	Note: assumes the lock is held.
//===========================================================================================================================

static OSStatus	_LogCategory_ApplyAction_Output( LogCategory *inCategory, int inOutputID, LogAction *inAction )
{
	OSStatus			err;
	LogOutput *			newOutput;
	LogOutput *			oldOutput;
	LogOutput **		outputAddr;
	
	if( *inAction->value != '\0' )
	{
		err = _LogOutputCreate( inAction->value, &newOutput );
		require_noerr_quiet( err, exit );
	}
	else
	{
		newOutput = NULL;
		err = kNoErr;
	}
	if(      inOutputID == 1 ) outputAddr = &inCategory->output1;
	else if( inOutputID == 2 ) outputAddr = &inCategory->output2;
	else { err = kParamErr; goto exit; }
	
	oldOutput = *outputAddr;
	if( oldOutput != newOutput )
	{
		if( oldOutput ) --oldOutput->refCount;
		if( newOutput ) ++newOutput->refCount;
		*outputAddr = newOutput;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	_LULevelToString
//===========================================================================================================================

static const struct
{
	LogLevel		level;
	const char *	name;
	
}	kLogLevelToStringTable[] = 
{
	{ kLogLevelAll,			"all" }, 
	{ kLogLevelMin,			"min" }, 
	{ kLogLevelChatty,		"chatty" }, 
	{ kLogLevelVerbose,		"verbose" }, 
	{ kLogLevelTrace,		"trace" }, 
	{ kLogLevelInfo,		"info" }, 
	{ kLogLevelNotice,		"notice" }, 
	{ kLogLevelWarning,		"warning" }, 
	{ kLogLevelAssert,		"assert" }, 
	{ kLogLevelRequire,		"require" }, 
	{ kLogLevelError,		"error" }, 
	{ kLogLevelCritical,	"critical" }, 
	{ kLogLevelAlert,		"alert" }, 
	{ kLogLevelEmergency,	"emergency" }, 
	{ kLogLevelTragic,		"tragic" }, 
	{ kLogLevelMax,			"max" }, 
	{ kLogLevelOff,			"off" }, 
	{ kLogLevelMax + 1, 	NULL }
};

static char *	_LULevelToString( LogLevel inLevel, char *inBuf, size_t inLen )
{
	int					i;
	int					diff;
	int					smallestDiff;
	int					closestIndex;
	const char *		name;
	
	inLevel &= kLogLevelMask;
	
	smallestDiff = INT_MAX;
	closestIndex = 0;
	for( i = 0; kLogLevelToStringTable[ i ].name; ++i )
	{
		diff = inLevel - kLogLevelToStringTable[ i ].level;
		if( diff < 0 ) diff = -diff;
		if( diff < smallestDiff )
		{
			smallestDiff = diff;
			closestIndex = i;
		}
	}
	
	name = kLogLevelToStringTable[ closestIndex ].name;
	diff = inLevel - kLogLevelToStringTable[ closestIndex ].level;
	if(      diff > 0 ) SNPrintF( inBuf, inLen, "%s+%u", name, diff );
	else if( diff < 0 )	SNPrintF( inBuf, inLen, "%s-%u", name, -diff );
	else				SNPrintF( inBuf, inLen, "%s",    name );
	return( inBuf );
}

//===========================================================================================================================
//	LUStringToLevel
//===========================================================================================================================

// Workaround until <radar:11684218> is fixed in the clang analyzer.

#ifdef __clang_analyzer__
	#undef  isalpha_safe
	#define isalpha_safe( X )	( ( ( (X) >= 'a' ) && ( (X) <= 'z' ) ) || ( ( (X) >= 'A' ) && ( (X) <= 'Z' ) ) )

	#undef  isdigit_safe
	#define isdigit_safe( X )	( ( (X) >= '0' ) && ( (X) <= '9' ) )
#endif

LogLevel	LUStringToLevel( const char *inStr )
{
	LogLevel			level;
	const char *		ptr;
	char				c;
	int					i;
	size_t				len;
	char				adjust;
	int					x;
	
	for( ptr = inStr; isalpha_safe( c = *ptr ); ++ptr ) {}
	if( ptr != inStr )
	{
		len = (size_t)( ptr - inStr );
		level = kLogLevelUninitialized;
		for( i = 0; kLogLevelToStringTable[ i ].name; ++i )
		{
			if( strncmp( inStr, kLogLevelToStringTable[ i ].name, len ) == 0 )
			{
				level = kLogLevelToStringTable[ i ].level;
				break;
			}
		}
		require_quiet( level != kLogLevelUninitialized, exit );
		if( c == '\0' ) goto exit;
		adjust = c;
		
		x = 0;
		for( ++ptr; isdigit_safe( c = *ptr ); ++ptr ) x = ( x * 10 ) + ( c - '0' );
		require_action_quiet( c == '\0', exit, level = kLogLevelUninitialized );
		
		if(      adjust == '+' ) level += x;
		else if( adjust == '-' ) level -= x;
		else { level = kLogLevelUninitialized; goto exit; }
	}
	else
	{
		level = 0;
		for( ; isdigit_safe( c = *ptr ); ++ptr ) level = ( level * 10 ) + ( c - '0' );
		require_action_quiet( c == '\0', exit, level = kLogLevelUninitialized );
	}
	
exit:
	return( level );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	LogPrintF
//===========================================================================================================================

static int	_LogPrintFCallBack( const char *inStr, size_t inLen, void *inContext );
static void	_LogPrintFWrite( LogPrintFContext *inContext, const char *inStr, size_t inLen );

#if( !TARGET_HAS_VA_ARG_MACROS )
int	LogPrintF_C89( LogCategory *inCategory, const char *inFunction, LogLevel inLevel, const char *inFormat, ... )
{
	if( log_category_ptr_enabled( inCategory, inLevel ) )
	{
		int				n;
		va_list			args;
		
		va_start( args, inFormat );
		n = LogPrintV( inCategory, inFunction, inLevel, inFormat, args );
		va_end( args );
		return( n );
	}
	return( 0 );
}
#endif

int	LogPrintF( LogCategory *inCategory, const char *inFunction, LogLevel inLevel, const char *inFormat, ... )
{
	int				n;
	va_list			args;
	
	va_start( args, inFormat );
	n = LogPrintV( inCategory, inFunction, inLevel, inFormat, args );
	va_end( args );
	return( n );
}

int	LogPrintV( LogCategory *inCategory, const char *inFunction, LogLevel inLevel, const char *inFormat, va_list inArgs )
{
	LogPrintFContext		context;
	int						total, n, last;
	va_list					args;
	char *					reason = NULL;
	
	if( inLevel & kLogLevelFlagCrashReport )
	{
		// Note: this is done here to work around what looks like a clang static analyzer issue.
		// <rdar://problem/12292635> False positive for uninitialized value?
		
		va_copy( args, inArgs );
		VASPrintF( &reason, inFormat, args );
		va_end( args );
	}
	
	LogUtils_EnsureInitialized();
	MinimalMutexLock( gLogUtilsLock );
	
	context.category	= inCategory;
	context.level		= inLevel;
	context.buf[ 0 ]	= '\0';
	context.len			= 0;
	context.flushOnEnd	= false;
	
	// Print the header.
	
	total = 0;
	if( !( inLevel & kLogLevelFlagContinuation ) )
	{
		LogFlags		flags;
		
		flags = inCategory->flags;
		if( inLevel & kLogLevelFlagFunction ) flags |= kLogFlags_PrintFunction;
		
		// Skip if we're logging too frequently.
		
		if( ( inCategory->rateMaxCount > 0 ) && !( inLevel & kLogLevelFlagDontRateLimit ) )
		{
			if( inCategory->rateEnd == 0 )
			{
				inCategory->rateEnd = UpTicks() + inCategory->rateInterval;
			}
			if( UpTicks() >= inCategory->rateEnd )
			{
				inCategory->rateEnd		= 0;
				inCategory->rateCounter = 0;
			}
			if( inCategory->rateCounter >= inCategory->rateMaxCount )
			{
				MinimalMutexUnlock( gLogUtilsLock );
				goto exit;
			}
			++inCategory->rateCounter;
		}
		
		// Time
		
		if( flags & kLogFlags_PrintTime )
		{
			n = CPrintF( _LogPrintFCallBack, &context, "%N " );
			if( n > 0 ) total += n;
		}
		
		if( flags & ( kLogFlags_PrintProgram | kLogFlags_PrintPID | kLogFlags_PrintCategory | 
			kLogFlags_PrintLevel | kLogFlags_PrintPrefix | kLogFlags_PrintFunction ) )
		{
			n = CPrintF( _LogPrintFCallBack, &context, "[" );
			if( n > 0 ) total += n;
			last = total;
			
			// Program
			
			if( flags & kLogFlags_PrintProgram )
			{
				n = CPrintF( _LogPrintFCallBack, &context, "%s", __PROGRAM__ );
				if( n > 0 ) total += n;
			}
			
			// PID
			
			#if( TARGET_OS_POSIX || TARGET_OS_WINDOWS )
			if( flags & kLogFlags_PrintPID )
			{
				n = CPrintF( _LogPrintFCallBack, &context, "%s%llu", ( last == total ) ? "" : ":", (uint64_t) getpid() );
				if( n > 0 ) total += n;
			}
			#endif
			
			// Category
			
			if( flags & kLogFlags_PrintCategory )
			{
				n = CPrintF( _LogPrintFCallBack, &context, "%s%s", ( last == total ) ? "" : ",", inCategory->name );
				if( n > 0 ) total += n;
			}
			
			// Prefix
			
			else if( flags & kLogFlags_PrintPrefix )
			{
				n = CPrintF( _LogPrintFCallBack, &context, "%s%.*s", ( last == total ) ? "" : ",", 
					inCategory->prefixLen, inCategory->prefixPtr );
				if( n > 0 ) total += n;
			}
			
			// Function
			
			if( flags & kLogFlags_PrintFunction )
			{
				n = CPrintF( _LogPrintFCallBack, &context, "%s%s", ( last == total ) ? "" : ",", inFunction );
				if( n > 0 ) total += n;
			}
			
			// Level
			
			if( flags & kLogFlags_PrintLevel )
			{
				char		levelStr[ 64 ];
				
				n = CPrintF( _LogPrintFCallBack, &context, "%s%s", ( last == total ) ? "" : "@", 
					_LULevelToString( inLevel, levelStr, sizeof( levelStr ) ) );
				if( n > 0 ) total += n;
			}
			
			n = CPrintF( _LogPrintFCallBack, &context, "] " );
			if( n > 0 ) total += n;
		}
	}
	
	// Print the body.
	
	n = VCPrintF( _LogPrintFCallBack, &context, inFormat, inArgs );
	if( n > 0 ) total += n;
	
	context.flushOnEnd = true;
	n = _LogPrintFCallBack( "", 0, &context );
	if( n > 0 ) total += n;
	
	MinimalMutexUnlock( gLogUtilsLock );
	
	// Print out a stack trace if requested.
	
	if( inLevel & kLogLevelFlagStackTrace )
	{
		DebugStackTrace( kLogLevelMax );
	}
	
	// Break into the debugger if requested.
	
	if( ( inLevel & kLogLevelFlagDebugBreak ) && DebugIsDebuggerPresent() )
	{
		DebugEnterDebugger( true );
	}
	
	// Force a crash report if requested.
	
	if( inLevel & kLogLevelFlagCrashReport )
	{
		if( reason )
		{
			char *		end;
			
			for( end = reason + strlen( reason ); ( end > reason ) && ( end[ -1 ] == '\n' ); --end ) {}
			*end = '\0';
			ReportCriticalError( reason, 0, true );
		}
	}
	
exit:
	if( reason ) free( reason );
	return( total );
}

static int	_LogPrintFCallBack( const char *inStr, size_t inLen, void *inContext )
{
	LogPrintFContext * const		context = (LogPrintFContext *) inContext;
	
	// Flush buffered data if we got an explicit flush (inLen == 0) or we'd go over our max size.
	
	if( ( ( inLen == 0 ) && context->flushOnEnd ) || ( ( context->len + inLen ) > sizeof( context->buf ) ) )
	{
		if( context->len > 0 )
		{
			_LogPrintFWrite( context, context->buf, context->len );
			context->len = 0;
		}
	}
	
	// Flush immediately if the new data is too big for our buffer. Otherwise, buffer it.
	
	if( inLen > sizeof( context->buf ) )
	{
		_LogPrintFWrite( context, inStr, inLen );
	}
	else if( inLen > 0 )
	{
		memcpy( &context->buf[ context->len ], inStr, inLen );
		context->len += inLen;
	}
	return( (int) inLen );
}

static void	_LogPrintFWrite( LogPrintFContext *inContext, const char *inStr, size_t inLen )
{
	if( inContext->category->output1 )
	{
		#if( LOGUTILS_OSLOG_ENABLED )
		if( !( inContext->level & kLogLevelFlagSensitive ) || ( inContext->category->output1->type == kLogOutputType_OSLog ) )
		#else
		if( !( inContext->level & kLogLevelFlagSensitive ) )
		#endif
		{
			inContext->category->output1->writer( inContext, inContext->category->output1, inStr, inLen );
		}
	}
	if( inContext->category->output2 )
	{
		#if( LOGUTILS_OSLOG_ENABLED )
		if( !( inContext->level & kLogLevelFlagSensitive ) || ( inContext->category->output2->type == kLogOutputType_OSLog ) )
		#else
		if( !( inContext->level & kLogLevelFlagSensitive ) )
		#endif
		{
			inContext->category->output2->writer( inContext, inContext->category->output2, inStr, inLen );
		}
	}
	
#if( DEBUG_FPRINTF_ENABLED )
	if( ( inContext->level & kLogLevelFlagForceConsole ) && !( inContext->level & kLogLevelFlagSensitive ) )
	{
		#if( TARGET_OS_POSIX )
			int			fd;
			ssize_t		n;
			
			fd = open( "/dev/console", O_WRONLY, 0 );
			if( fd >= 0 )
			{
				n = write( fd, inStr, inLen );
				(void) n;
				close( fd );
				usleep( 200 );
			}
		#else		
			FILE *		f;
			
			f = fopen( "/dev/console", "w" );
			if( f )
			{
				fwrite( inStr, 1, inLen, f );
				fflush( f );
				fclose( f );
			}
		#endif
	}
#endif
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_LogOutputCreate
//
//	Note: assumes the lock is held.
//===========================================================================================================================

static OSStatus	_LogOutputCreate( const char *inConfigStr, LogOutput **outOutput )
{
	OSStatus			err;
	LogOutput *			output;
	const char *		typeSrc;
	size_t				typeLen;
	char				c;
	
	// If there's an existing output with the same config string, use that one instead.
	
	for( output = gLogOutputList; output; output = output->next )
	{
		if( strnicmpx( output->configStr, SIZE_MAX, inConfigStr ) == 0 )
		{
			*outOutput = output;
			output = NULL;
			err = kNoErr;
			goto exit;
		}
	}
	
	output = (LogOutput *) calloc( 1, sizeof( *output ) );
	require_action_quiet( output, exit, err = kNoMemoryErr );
	
	output->refCount = 0; // Only referenced when associated with a category.
	output->configStr = strdup( inConfigStr );
	require_action_quiet( output->configStr, exit, err = kNoMemoryErr );
	
	// Parse the output type and set it up based on the type.
	
	typeSrc = inConfigStr;
	while( ( ( c = *inConfigStr ) != '\0' ) && ( c != ';' ) ) ++inConfigStr;
	typeLen = (size_t)( inConfigStr - typeSrc );
	require_action_quiet( typeLen > 0, exit, err = kTypeErr );
	if( c != '\0' ) ++inConfigStr;
	
	if( 0 ) {}
#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	else if( strnicmpx( typeSrc, typeLen, "asl" ) == 0 )
	{
		err = _LogOutputASL_Setup( output, inConfigStr );
		require_noerr_quiet( err, exit );
	}
#endif
	else if( strnicmpx( typeSrc, typeLen, "callback" ) == 0 )
	{
		err = _LogOutputCallBack_Setup( output, inConfigStr );
		require_noerr_quiet( err, exit );
	}
	else if( strnicmpx( typeSrc, typeLen, "console" ) == 0 )
	{
		#if  ( TARGET_OS_POSIX )
			if( _LogOutput_IsStdErrMappedToDevNull() )	err = _LogOutputSysLog_Setup( output, inConfigStr );
			else										err = _LogOutputFile_Setup( output, inConfigStr );
			require_noerr_quiet( err, exit );
		#elif( DEBUG_FPRINTF_ENABLED )
			err = _LogOutputFile_Setup( output, inConfigStr );
			require_noerr_quiet( err, exit );
		#elif( TARGET_OS_THREADX )
			output->writer	= _LogOutputThreadX_Writer;
			output->type	= kLogOutputType_ThreadX;
		#elif( TARGET_OS_WINDOWS_CE )
			output->writer	= _LogOutputWindowsDebugger_Writer;
			output->type	= kLogOutputType_WindowsDebugger;
		#elif( TARGET_OS_WINDOWS_KERNEL )
			output->writer	= _LogOutputWindowsKernel_Writer;
			output->type	= kLogOutputType_WindowsKernel;
		#endif
	}
#if( DEBUG_FPRINTF_ENABLED )
	else if( strnicmpx( typeSrc, typeLen, "file" ) == 0 )
	{
		err = _LogOutputFile_Setup( output, inConfigStr );
		require_noerr_quiet( err, exit );
	}
#endif
#if( DEBUG_IDEBUG_ENABLED )
	else if( strnicmpx( typeSrc, typeLen, "iDebug" ) == 0 )
	{
		err = _LogOutputiDebug_Setup( output, inConfigStr );
		require_noerr_quiet( err, exit );
	}
#endif
#if( DEBUG_MAC_OS_X_IOLOG_ENABLED )
	else if( strnicmpx( typeSrc, typeLen, "IOLog" ) == 0 )
	{
		output->writer	= _LogOutputIOLog_Writer;
		output->type	= kLogOutputType_IOLog;
	}
#endif
#if( DEBUG_KPRINTF_ENABLED )
	else if( strnicmpx( typeSrc, typeLen, "kprintf" ) == 0 )
	{
		output->writer	= _LogOutputKPrintF_Writer;
		output->type	= kLogOutputType_kprintf;
	}
#endif
#if( LOGUTILS_OSLOG_ENABLED )
	else if( strnicmpx( typeSrc, typeLen, "oslog" ) == 0 )
	{
		err = _LogOutputOSLog_Setup( output, inConfigStr );
		require_noerr_quiet( err, exit );
	}
#endif
#if( TARGET_OS_POSIX )
	else if( strnicmpx( typeSrc, typeLen, "syslog" ) == 0 )
	{
		err = _LogOutputSysLog_Setup( output, inConfigStr );
		require_noerr_quiet( err, exit );
	}
#endif
#if( TARGET_OS_WINDOWS )
	else if( strnicmpx( typeSrc, typeLen, "WindowsDebugger" ) == 0 )
	{
		output->writer	= _LogOutputWindowsDebugger_Writer;
		output->type	= kLogOutputType_WindowsDebugger;
	}
#endif
#if( DEBUG_WINDOWS_EVENT_LOG_ENABLED )
	else if( strnicmpx( typeSrc, typeLen, "WindowsEventLog" ) == 0 )
	{
		err = _LogOutputWindowsEventLog_Setup( output, inConfigStr );
		require_noerr_quiet( err, exit );
	}
#endif
#if( TARGET_OS_WINDOWS_KERNEL )
	else if( strnicmpx( typeSrc, typeLen, "WindowsKernel" ) == 0 )
	{
		output->writer	= _LogOutputWindowsKernel_Writer;
		output->type	= kLogOutputType_WindowsKernel;
	}
#endif
	else
	{
		err = kParamErr;
		goto exit;
	}
	
	output->next	= gLogOutputList;
	gLogOutputList	= output;
	*outOutput		= output;
	output			= NULL;
	err				= kNoErr;
	
exit:
	if( output ) _LogOutputDelete( output );
	return( err );
}

//===========================================================================================================================
//	_LogOutputDelete
//===========================================================================================================================

static void	_LogOutputDelete( LogOutput *inOutput )
{
#if( TARGET_OS_DARWIN )
	if( inOutput->type == kLogOutputType_ASL )
	{
		ForgetMem( &inOutput->config.asl.facility );
		ForgetMem( &inOutput->config.asl.sender );
	}
#endif
#if( DEBUG_FPRINTF_ENABLED )
	if( inOutput->type == kLogOutputType_File )
	{
		ForgetMem( &inOutput->config.file.logFileName );
		if( inOutput->config.file.logFilePtr )
		{
			if( ( inOutput->config.file.logFilePtr != stderr ) && 
				( inOutput->config.file.logFilePtr != stdout ) )
			{
				fclose( inOutput->config.file.logFilePtr );
			}
			inOutput->config.file.logFilePtr = NULL;
		}
		ForgetMem( &inOutput->config.file.logBackupFileName );
	}
#endif
#if( LOGUTILS_OSLOG_ENABLED )
	if( inOutput->type == kLogOutputType_OSLog )
	{
		os_log_t * const		logObjectPtr = (os_log_t *) &inOutput->config.oslog.logObject;
		
		ForgetMem( &inOutput->config.oslog.subsystem );
		ForgetMem( &inOutput->config.oslog.category );
		os_forget( logObjectPtr );
	}
#endif
	
	ForgetMem( &inOutput->configStr );
	free( inOutput );
}

//===========================================================================================================================
//	_LogOutputDeleteUnused
//
//	Note: assumes the lock is held.
//===========================================================================================================================

static void	_LogOutputDeleteUnused( void )
{
	LogOutput **	next;
	LogOutput *		curr;
	
	next = &gLogOutputList;
	while( ( curr = *next ) != NULL )
	{
		if( curr->refCount == 0 )
		{
			*next = curr->next;
			_LogOutputDelete( curr );
			continue;
		}
		next = &curr->next;
	}
}

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
//===========================================================================================================================
//	_LogOutputASL_Setup
//===========================================================================================================================

static OSStatus	_LogOutputASL_Setup( LogOutput *inOutput, const char *inParams )
{
	OSStatus			err;
	const char *		namePtr;
	size_t				nameLen;
	const char *		valuePtr;
	size_t				valueLen;
	char				c;
	char				buf[ 32 ];
	char *				cptr;
	int					level;
	int					priority;
	
	inOutput->config.asl.priority = ASL_LEVEL_NOTICE;
	
	while( *inParams != '\0' )
	{
		namePtr = inParams;
		while( ( ( c = *inParams ) != '\0' ) && ( c != '=' ) ) ++inParams;
		require_action_quiet( c != '\0', exit, err = kMalformedErr );
		nameLen = (size_t)( inParams - namePtr );
		++inParams;
		
		valuePtr = inParams;
		while( ( ( c = *inParams ) != '\0' ) && ( c != ';' ) ) ++inParams;
		valueLen = (size_t)( inParams - valuePtr );
		if( c != '\0' ) ++inParams;
		
		if( strnicmpx( namePtr, nameLen, "facility" ) == 0 )
		{
			// Format: facility=<ASL_KEY_FACILITY name>.
			
			cptr = (char *) malloc( valueLen + 1 );
			require_action_quiet( cptr, exit, err = kNoMemoryErr );
			memcpy( cptr, valuePtr, valueLen );
			cptr[ valueLen ] = '\0';
			
			ForgetMem( &inOutput->config.asl.facility );
			inOutput->config.asl.facility = cptr;
		}
		else if( strnicmpx( namePtr, nameLen, "level" ) == 0 )
		{
			// Format: level=<log level to use>.
			
			valueLen = Min( valueLen, sizeof( buf ) - 1 );
			memcpy( buf, valuePtr, valueLen );
			buf[ valueLen ] = '\0';
			
			level = LUStringToLevel( buf );
			if(      level == kLogLevelUninitialized )	continue;
			else if( level >= kLogLevelEmergency )		priority = ASL_LEVEL_EMERG;
			else if( level >= kLogLevelAlert )			priority = ASL_LEVEL_ALERT;
			else if( level >= kLogLevelCritical )		priority = ASL_LEVEL_CRIT;
			else if( level >= kLogLevelError )			priority = ASL_LEVEL_ERR;
			else if( level >= kLogLevelWarning )		priority = ASL_LEVEL_WARNING;
			else if( level >= kLogLevelNotice )			priority = ASL_LEVEL_NOTICE;
			else if( level >= kLogLevelInfo )			priority = ASL_LEVEL_INFO;
			else										priority = ASL_LEVEL_DEBUG;
			inOutput->config.asl.priority = priority;
		}
		else if( strnicmpx( namePtr, nameLen, "sender" ) == 0 )
		{
			// Format: sender=<ASL_KEY_SENDER name>.
			
			cptr = (char *) malloc( valueLen + 1 );
			require_action_quiet( cptr, exit, err = kNoMemoryErr );
			memcpy( cptr, valuePtr, valueLen );
			cptr[ valueLen ] = '\0';
			
			ForgetMem( &inOutput->config.asl.sender );
			inOutput->config.asl.sender = cptr;
		}
		else
		{
			err = kUnsupportedErr;
			goto exit;
		}
	}
	
	inOutput->writer = _LogOutputASL_Writer;
	inOutput->type   = kLogOutputType_ASL;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_LogOutputASL_Writer
//===========================================================================================================================

static void	_LogOutputASL_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	aslmsg		msg;
	
	(void) inContext;
	
	msg = asl_new( ASL_TYPE_MSG );
	require_quiet( msg, exit );
	if( inOutput->config.asl.facility )	asl_set( msg, ASL_KEY_FACILITY, inOutput->config.asl.facility );
	if( inOutput->config.asl.sender )	asl_set( msg, ASL_KEY_SENDER,   inOutput->config.asl.sender );
	
	if( ( inLen > 0 ) && ( inStr[ inLen - 1 ] == '\n' ) ) --inLen; // Strip trailing newline.
	asl_log( NULL, msg, inOutput->config.asl.priority, "%.*s", (int) inLen, inStr );
	asl_free( msg );
	
exit:
	return;
}
#endif // TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES

//===========================================================================================================================
//	_LogOutputCallBack_Setup
//===========================================================================================================================

static OSStatus	_LogOutputCallBack_Setup( LogOutput *inOutput, const char *inParams )
{
	OSStatus			err;
	const char *		namePtr;
	size_t				nameLen;
	const char *		valuePtr;
	size_t				valueLen;
	char				c;
	char				tempStr[ 64 ];
	int					n;
	void *				tempPtr;
	
	inOutput->config.callback.func = NULL;
	inOutput->config.callback.arg  = NULL;
	while( *inParams != '\0' )
	{
		namePtr = inParams;
		while( ( ( c = *inParams ) != '\0' ) && ( c != '=' ) ) ++inParams;
		require_action_quiet( c != '\0', exit, err = kMalformedErr );
		nameLen = (size_t)( inParams - namePtr );
		++inParams;
		
		valuePtr = inParams;
		while( ( ( c = *inParams ) != '\0' ) && ( c != ';' ) ) ++inParams;
		valueLen = (size_t)( inParams - valuePtr );
		if( c != '\0' ) ++inParams;
		
		if( strnicmpx( namePtr, nameLen, "func" ) == 0 )
		{
			// Format: func=<function pointer>.
			
			require_action_quiet( valueLen < sizeof( tempStr ), exit, err = kSizeErr );
			memcpy( tempStr, valuePtr, valueLen );
			tempStr[ valueLen ] = '\0';
			
			n = sscanf( tempStr, "%p", &tempPtr );
			require_action_quiet( n == 1, exit, err = kMalformedErr );
			inOutput->config.callback.func = (LogOutputCallBack)(uintptr_t) tempPtr;
		}
		else if( strnicmpx( namePtr, nameLen, "arg" ) == 0 )
		{
			// Format: arg=<pointer arg>.
			
			require_action_quiet( valueLen < sizeof( tempStr ), exit, err = kSizeErr );
			memcpy( tempStr, valuePtr, valueLen );
			tempStr[ valueLen ] = '\0';
			
			n = sscanf( tempStr, "%p", &inOutput->config.callback.arg );
			require_action_quiet( n == 1, exit, err = kMalformedErr );
		}
		else
		{
			err = kUnsupportedErr;
			goto exit;
		}
	}
	
	inOutput->writer = _LogOutputCallBack_Writer;
	inOutput->type   = kLogOutputType_CallBack;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_LogOutputCallBack_Writer
//===========================================================================================================================

static void	_LogOutputCallBack_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	if( inOutput->config.callback.func ) inOutput->config.callback.func( inContext, inStr, inLen, inOutput->config.callback.arg );
}



#if( DEBUG_FPRINTF_ENABLED )
//===========================================================================================================================
//	_LogOutputFile_Setup
//===========================================================================================================================

static OSStatus	_LogOutputFile_Setup( LogOutput *inOutput, const char *inParams )
{
	OSStatus			err;
	const char *		namePtr;
	size_t				nameLen;
	const char *		valuePtr;
	const char *		valueEnd;
	size_t				valueLen;
	const char *		ptr;
	char				c;
	char *				str;
	size_t				len;
	int64_t				x;
	
	inOutput->writer = _LogOutputFile_Writer;
	inOutput->type   = kLogOutputType_File;
	
	if( inOutput->config.file.logFilePtr && 
		( inOutput->config.file.logFilePtr != stderr ) && 
		( inOutput->config.file.logFilePtr != stdout ) )
	{
		fclose( inOutput->config.file.logFilePtr );
	}
	inOutput->config.file.logFilePtr = NULL;
	
	if( ( *inParams == '\0' ) || ( strnicmpx( inParams, SIZE_MAX, "stderr" ) == 0 ) )
	{
		#if( DEBUG && TARGET_OS_WINDOWS )
			_LogOutput_EnableWindowsConsole();
		#endif
		
		inOutput->config.file.logFilePtr = stderr;
	}
	else if( strnicmpx( inParams, SIZE_MAX, "stdout" ) == 0 )
	{
		#if( DEBUG && TARGET_OS_WINDOWS )
			_LogOutput_EnableWindowsConsole();
		#endif
		
		inOutput->config.file.logFilePtr = stdout;
	}
	else
	{
		while( *inParams != '\0' )
		{
			namePtr = inParams;
			while( ( ( c = *inParams ) != '\0' ) && ( c != '=' ) ) ++inParams;
			require_action_quiet( c != '\0', exit, err = kMalformedErr );
			nameLen = (size_t)( inParams - namePtr );
			++inParams;
			
			valuePtr = inParams;
			while( ( ( c = *inParams ) != '\0' ) && ( c != ';' ) ) ++inParams;
			valueEnd = inParams;
			valueLen = (size_t)( inParams - valuePtr );
			if( c != '\0' ) ++inParams;
			
			if( strnicmpx( namePtr, nameLen, "path" ) == 0 )
			{
				// Format: path=<path to log file>.
				
				require_action_quiet( valueLen > 0, exit, err = kPathErr );
				str = (char *) malloc( valueLen + 1 );
				require_action_quiet( str, exit, err = kNoMemoryErr );
				memcpy( str, valuePtr, valueLen );
				str[ valueLen ] = '\0';
				ForgetMem( &inOutput->config.file.logFileName );
				inOutput->config.file.logFileName = str;
				
				#if( TARGET_OS_POSIX )
				{
					const char *		dirEnd;
					size_t				dirLen;
					char				dirPath[ PATH_MAX + 1 ];
					
					dirEnd = strrchr( inOutput->config.file.logFileName, '/' );
					if( dirEnd )
					{
						dirLen = (size_t)( dirEnd - inOutput->config.file.logFileName );
						require_action_quiet( dirLen < sizeof( dirPath ), exit, err = kPathErr );
						memcpy( dirPath, inOutput->config.file.logFileName, dirLen );
						dirPath[ dirLen ] = '\0';
						
						mkpath( dirPath, S_IRWXU | S_IRWXG, S_IRWXU | S_IRWXG );
					}
				}
				#endif
				inOutput->config.file.logFilePtr = fopen( inOutput->config.file.logFileName, "a" );
				require_action_quiet( inOutput->config.file.logFilePtr, exit, err = kOpenErr );
				
				fseeko( inOutput->config.file.logFilePtr, 0, SEEK_END );
				inOutput->config.file.logFileSize = ftello( inOutput->config.file.logFilePtr );
			}
			else if( strnicmpx( namePtr, nameLen, "roll" ) == 0 )
			{
				// Format: roll=<maxSize>#<maxCount>.
				
				x = 0;
				for( ; ( valuePtr < valueEnd ) && isdigit_safe( *valuePtr ); ++valuePtr )
					x = ( x * 10 ) + ( *valuePtr - '0' );
				if( valuePtr < valueEnd )
				{
					if(      *valuePtr == 'B' ) ++valuePtr;
					else if( *valuePtr == 'K' ) { x *=          1024;   ++valuePtr; }
					else if( *valuePtr == 'M' ) { x *= ( 1024 * 1024 ); ++valuePtr; }
				}
				require_action_quiet( ( valuePtr == valueEnd ) || ( *valuePtr == '#' ), exit, err = kParamErr );
				inOutput->config.file.logFileMaxSize = x;
				if( valuePtr < valueEnd ) ++valuePtr;
				
				x = 0;
				for( ; ( valuePtr < valueEnd ) && isdigit_safe( *valuePtr ); ++valuePtr )
					x = ( x * 10 ) + ( *valuePtr - '0' );
				require_action_quiet( valuePtr == valueEnd, exit, err = kParamErr );
				inOutput->config.file.logFileMaxCount = (int) x;
			}
			else if( strnicmpx( namePtr, nameLen, "backup" ) == 0 )
			{
				// Format: backup=<base path to backup files>#<maxCount>.
				
				ptr = valuePtr;
				for( ; ( valuePtr < valueEnd ) && ( *valuePtr != '#' ); ++valuePtr ) {}
				require_action_quiet( ( valuePtr == valueEnd ) || ( *valuePtr == '#' ), exit, err = kParamErr );
				
				str = NULL;
				len = (size_t)( valuePtr - ptr );
				if( len > 0 )
				{
					str = (char *) malloc( len + 1 );
					require_action_quiet( str, exit, err = kNoMemoryErr );
					memcpy( str, ptr, len );
					str[ len ] = '\0';
				}
				ForgetMem( &inOutput->config.file.logBackupFileName );
				inOutput->config.file.logBackupFileName = str;
				if( valuePtr < valueEnd ) ++valuePtr;
				
				x = 0;
				for( ; ( valuePtr < valueEnd ) && isdigit_safe( *valuePtr ); ++valuePtr )
					x = ( x * 10 ) + ( *valuePtr - '0' );
				require_action_quiet( valuePtr == valueEnd, exit, err = kParamErr );
				inOutput->config.file.logBackupFileMaxCount = (int) x;
			}
			else
			{
				err = kUnsupportedErr;
				goto exit;
			}
		}
		require_action_quiet( inOutput->config.file.logFilePtr, exit, err = kParamErr );
	}
	
	// Force output to use non-buffered I/O.
	
	setvbuf( inOutput->config.file.logFilePtr, NULL, _IONBF, 0 );
#if( TARGET_OS_VXWORKS )
	setbuf( stderr, NULL );
	setbuf( stdout, NULL );
#endif
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_LogOutputFile_Writer
//
//	Note: assumes the lock is held.
//===========================================================================================================================

static void	_LogOutputFile_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	(void) inContext;
	
	// Roll the log files if the current file goes above the max size.
	
	if( ( inOutput->config.file.logFilePtr != stderr ) && ( inOutput->config.file.logFilePtr != stdout ) )
	{
		inOutput->config.file.logFileSize += inLen;
		if( inOutput->config.file.logFileMaxSize > 0 )
		{
			if( inOutput->config.file.logFileSize > inOutput->config.file.logFileMaxSize )
			{
				_LogOutputFile_BackupLogFiles( inOutput );
				RollLogFiles( &inOutput->config.file.logFilePtr, 
					"\nLOG ENDED, CONTINUES IN NEXT LOG FILE\n", 
					inOutput->config.file.logFileName, inOutput->config.file.logFileMaxCount );
				inOutput->config.file.logFileSize = (int64_t) inLen;
			}
		}
	}
	if( inOutput->config.file.logFilePtr )
	{
		fwrite( inStr, 1, inLen, inOutput->config.file.logFilePtr );
		fflush( inOutput->config.file.logFilePtr );
	}
}

//===========================================================================================================================
//	_LogOutputFile_BackupLogFiles
//
//	Note: assumes the lock is held.
//===========================================================================================================================

static OSStatus	_LogOutputFile_BackupLogFiles( LogOutput *inOutput )
{
	OSStatus		err;
	char			oldPath[ PATH_MAX + 1 ];
	char 			newPath[ PATH_MAX + 1 ];
	int				i;
	
	require_action_quiet( inOutput->config.file.logBackupFileName, exit, err = kNoErr );
	require_action_quiet( inOutput->config.file.logBackupFileMaxCount > 0, exit, err = kNoErr );
	
	// Delete the oldest file.
	
	SNPrintF( oldPath, sizeof( oldPath ), "%s.%d", inOutput->config.file.logBackupFileName, 
		inOutput->config.file.logBackupFileMaxCount - 1 );
	remove( oldPath );
	
	// Shift all the files down by 1.
	
	for( i = inOutput->config.file.logBackupFileMaxCount - 2; i > 0; --i )
	{
		SNPrintF( oldPath, sizeof( oldPath ), "%s.%d", inOutput->config.file.logBackupFileName, i );
		SNPrintF( newPath, sizeof( newPath ), "%s.%d", inOutput->config.file.logBackupFileName, i + 1 );
		rename( oldPath, newPath );
	}
	SNPrintF( newPath, sizeof( newPath ), "%s.1", inOutput->config.file.logBackupFileName );
	rename( inOutput->config.file.logBackupFileName, newPath );
	
	// Copy the latest file.
	
	SNPrintF( newPath, sizeof( newPath ), "%s", inOutput->config.file.logBackupFileName );
	_LogOutputFile_CopyLogFile( inOutput->config.file.logFileName, newPath );
	err = kNoErr;

exit:
	return( err );
}

//===========================================================================================================================
//	_LogOutputFile_CopyLogFile
//===========================================================================================================================

static OSStatus	_LogOutputFile_CopyLogFile( const char *inSrcPath, const char *inDstPath )
{
	OSStatus		err;
	char *			buffer;
	size_t			bufLen;
	FILE *			srcFile;
	FILE *			dstFile;
	size_t			nRead;
	size_t			nWrote;
	
	srcFile = NULL;
	dstFile = NULL;
	
	bufLen = 4 * 1024;
	buffer = (char *) malloc( bufLen );
	require_action_quiet( buffer, exit, err = kNoMemoryErr );
	
	srcFile = fopen( inSrcPath, "r" );
	err = map_global_value_errno( srcFile, srcFile );
	require_noerr_quiet( err, exit );
	
	dstFile = fopen( inDstPath, "w" );
	err = map_global_value_errno( dstFile, dstFile );
	require_noerr_quiet( err, exit );
	
	for( ;; )
	{
		nRead = fread( buffer, 1, bufLen, srcFile );
		if( nRead == 0 ) break;
		
		nWrote = fwrite( buffer, 1, nRead, dstFile );
		err = map_global_value_errno( nWrote == nRead, nWrote );
		require_noerr_quiet( err, exit );
	}
	
exit:
	if( srcFile )	fclose( srcFile );
	if( dstFile )	fclose( dstFile );
	if( buffer )	free( buffer );
	return( err );
}

#endif // DEBUG_FPRINTF_ENABLED

#if( DEBUG_IDEBUG_ENABLED )
//===========================================================================================================================
//	_LogOutputiDebug_Setup
//===========================================================================================================================

static OSStatus	_LogOutputiDebug_Setup( LogOutput *inOutput, const char *inParams )
{
	OSStatus		err;
	
	(void) inParams;
	
#if( TARGET_OS_DARWIN_KERNEL )	
	extern uint32_t *		_giDebugReserved1;
	
	// Emulate the iDebugSetOutputType macro in iDebugServices.h.
	// Note: This is not thread safe, but neither is iDebugServices.h nor iDebugKext.
	
	if( !_giDebugReserved1 )
	{
		_giDebugReserved1 = (uint32_t *) IOMalloc( sizeof( uint32_t ) );
		require_action_quiet( _giDebugReserved1, exit, err = kNoMemoryErr );
	}
	*_giDebugReserved1 = 0x00010000U;
	
	inOutput->writer = _LogOutputiDebug_Writer;
	inOutput->type   = kLogOutputType_iDebug;
	err = kNoErr;
	
exit:
#else
	__private_extern__ void	iDebugSetOutputTypeInternal( uint32_t inType );
	
	iDebugSetOutputTypeInternal( 0x00010000U );
	err = kNoErr;
#endif
	
	return( err );
}

//===========================================================================================================================
//	_LogOutputiDebug_Write
//===========================================================================================================================

static void	_LogOutputiDebug_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	(void) inContext;
	(void) inOutput;
	
#if( TARGET_OS_DARWIN_KERNEL )
	// Locally declared here so we do not need to include iDebugKext.h. Note: IOKit uses a global namespace and only 
	// a partial link occurs at build time. When the KEXT is loaded, the runtime linker will link in this extern'd
	// symbol. _giDebugLogInternal is actually part of IOKit proper so this should link even if iDebug is not present.
	
	typedef void ( *iDebugLogFunctionPtr )( uint32_t inLevel, uint32_t inTag, const char *inFormat, ... );
	
	extern iDebugLogFunctionPtr		_giDebugLogInternal;
	
	if( _giDebugLogInternal ) _giDebugLogInternal( 0, 0, "%.*s", (int) inLen, inStr );
#else
	__private_extern__ void	iDebugLogInternal( uint32_t inLevel, uint32_t inTag, const char *inFormat, ... );
	
	iDebugLogInternal( 0, 0, "%.*s", (int) inLen, inStr );
#endif
}
#endif // DEBUG_IDEBUG_ENABLED

//===========================================================================================================================
//	_LogOutputIOLog_Writer
//===========================================================================================================================

#if( DEBUG_MAC_OS_X_IOLOG_ENABLED )
static void	_LogOutputIOLog_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	(void) inContext;
	(void) inOutput;
	
	extern void	IOLog( const char *inFormat, ... );
	
	IOLog( "%.*s", (int) inLen, inStr );
}
#endif

//===========================================================================================================================
//	_LogOutputKPrintF_Writer
//===========================================================================================================================

#if( DEBUG_KPRINTF_ENABLED )
static void	_LogOutputKPrintF_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	(void) inContext;
	(void) inOutput;
	
	extern void	kprintf( const char *inFormat, ... );
	
	kprintf( "%.*s", (int) inLen, inStr );
}
#endif

#if( LOGUTILS_OSLOG_ENABLED )
//===========================================================================================================================
//	_LogOutputOSLog_Setup
//===========================================================================================================================

static OSStatus	_LogOutputOSLog_Setup( LogOutput *inOutput, const char *inParams )
{
	os_log_t * const	logObjectPtr = (os_log_t *) &inOutput->config.oslog.logObject;
	OSStatus			err;
	const char *		namePtr;
	size_t				nameLen;
	const char *		valuePtr;
	size_t				valueLen;
	char				c;
	char *				cptr;
	
	inOutput->writer = _LogOutputOSLog_Writer;
	inOutput->type   = kLogOutputType_OSLog;
	
	ForgetMem( &inOutput->config.oslog.subsystem );
	ForgetMem( &inOutput->config.oslog.category );
	os_forget( logObjectPtr );
	inOutput->config.oslog.sensitive = false;
	
	while( *inParams != '\0' )
	{
		namePtr = inParams;
		while( ( ( c = *inParams ) != '\0' ) && ( c != '=' ) ) ++inParams;
		require_action_quiet( c != '\0', exit, err = kMalformedErr );
		nameLen = (size_t)( inParams - namePtr );
		++inParams;
		
		valuePtr = inParams;
		while( ( ( c = *inParams ) != '\0' ) && ( c != ';' ) ) ++inParams;
		valueLen = (size_t)( inParams - valuePtr );
		if( c != '\0' ) ++inParams;
		
		if( strnicmpx( namePtr, nameLen, "subsystem" ) == 0 )
		{
			// Format: subsystem=<os_log_create subsystem name>.
			
			cptr = (char *) malloc( valueLen + 1 );
			require_action_quiet( cptr, exit, err = kNoMemoryErr );
			memcpy( cptr, valuePtr, valueLen );
			cptr[ valueLen ] = '\0';
			
			ForgetMem( &inOutput->config.oslog.subsystem );
			inOutput->config.oslog.subsystem = cptr;
		}
		else if( strnicmpx( namePtr, nameLen, "category" ) == 0 )
		{
			// Format: category=<os_log_create category name>.
			
			cptr = (char *) malloc( valueLen + 1 );
			require_action_quiet( cptr, exit, err = kNoMemoryErr );
			memcpy( cptr, valuePtr, valueLen );
			cptr[ valueLen ] = '\0';
			
			ForgetMem( &inOutput->config.oslog.category );
			inOutput->config.oslog.category = cptr;
		}
		else if( strnicmpx( namePtr, nameLen, "sensitive" ) == 0 )
		{
			inOutput->config.oslog.sensitive = IsTrueString( valuePtr, valueLen );
		}
		else
		{
			err = kUnsupportedErr;
			goto exit;
		}
	}
	
#if( 0 ) // Conditionalized out log object creation until issue with upcoming changes to os_log_create are worked out.
	if( inOutput->config.oslog.subsystem && inOutput->config.oslog.category )
	{
		*logObjectPtr = os_log_create( inOutput->config.oslog.subsystem, inOutput->config.oslog.category );
		require_action_quiet( *logObjectPtr, exit, err = kNoMemoryErr );
	}
#endif
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_LogOutputOSLog_Writer
//===========================================================================================================================

static void	_LogOutputOSLog_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	if( ( inLen > 0 ) && ( inStr[ inLen - 1 ] == '\n' ) ) --inLen; // Strip trailing newline.
	if( inOutput->config.oslog.sensitive || ( inContext->level & kLogLevelFlagSensitive ) )
	{
		os_log_sensitive( inOutput->config.oslog.logObject ? inOutput->config.oslog.logObject : OS_LOG_DEFAULT, 
			"%.*s", (int) inLen, inStr );
	}
	else
	{
		os_log( inOutput->config.oslog.logObject ? inOutput->config.oslog.logObject : OS_LOG_DEFAULT, 
			"%.*s", (int) inLen, inStr );
	}
}
#endif // LOGUTILS_OSLOG_ENABLED

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	_LogOutputSysLog_Setup
//===========================================================================================================================

static OSStatus	_LogOutputSysLog_Setup( LogOutput *inOutput, const char *inParams )
{
	OSStatus			err;
	const char *		namePtr;
	size_t				nameLen;
	const char *		valuePtr;
	size_t				valueLen;
	char				c;
	char				buf[ 32 ];
	LogLevel			level;
	int					priority;
	
	inOutput->config.syslog.priority = LOG_NOTICE;
	
	while( *inParams != '\0' )
	{
		namePtr = inParams;
		while( ( ( c = *inParams ) != '\0' ) && ( c != '=' ) ) ++inParams;
		require_action_quiet( c != '\0', exit, err = kMalformedErr );
		nameLen = (size_t)( inParams - namePtr );
		++inParams;
		
		valuePtr = inParams;
		while( ( ( c = *inParams ) != '\0' ) && ( c != ';' ) ) ++inParams;
		valueLen = (size_t)( inParams - valuePtr );
		if( c != '\0' ) ++inParams;
		
		if( strnicmpx( namePtr, nameLen, "level" ) == 0 )
		{
			// Format: level=<fixed level to use>.
			
			valueLen = Min( valueLen, sizeof( buf ) - 1 );
			memcpy( buf, valuePtr, valueLen );
			buf[ valueLen ] = '\0';
			
			level = LUStringToLevel( buf );
			if(      level == kLogLevelUninitialized )	continue;
			else if( level >= kLogLevelEmergency )		priority = LOG_EMERG;
			else if( level >= kLogLevelAlert )			priority = LOG_ALERT;
			else if( level >= kLogLevelCritical )		priority = LOG_CRIT;
			else if( level >= kLogLevelError )			priority = LOG_ERR;
			else if( level >= kLogLevelWarning )		priority = LOG_WARNING;
			else if( level >= kLogLevelNotice )			priority = LOG_NOTICE;
			else if( level >= kLogLevelInfo )			priority = LOG_INFO;
			else										priority = LOG_DEBUG;
			inOutput->config.syslog.priority = priority;
		}
		else
		{
			err = kUnsupportedErr;
			goto exit;
		}
	}
	
	inOutput->writer = _LogOutputSysLog_Writer;
	inOutput->type   = kLogOutputType_syslog;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_LogOutputSysLog_Writer
//===========================================================================================================================

static void	_LogOutputSysLog_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	(void) inContext;
	
	if( ( inLen > 0 ) && ( inStr[ inLen - 1 ] == '\n' ) ) --inLen; // Strip trailing newline.
	syslog( inOutput->config.syslog.priority, "%.*s", (int) inLen, inStr );
}
#endif // TARGET_OS_POSIX

#if( TARGET_OS_THREADX )
//===========================================================================================================================
//	_LogOutputThreadX_Writer
//===========================================================================================================================

static void	_LogOutputThreadX_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	(void) inContext;
	(void) inOutput;
	
	printf( "%.*s", (int) inLen, inStr );
}
#endif

#if( TARGET_OS_WINDOWS )
//===========================================================================================================================
//	_LogOutputWindowsDebugger_Writer
//===========================================================================================================================

static void	_LogOutputWindowsDebugger_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	TCHAR				buf[ 512 ];
	const char *		src;
	const char *		end;
	TCHAR *				dst;
	char				c;
	
	(void) inContext;
	(void) inOutput;
	
	// Copy locally and null terminate the string. This also converts from char to TCHAR in case we are 
	// building with UNICODE enabled since the input is always char. Also convert \r to \n in the process.

	src = inStr;
	if( inLen >= countof( buf ) )
	{
		inLen = countof( buf ) - 1;
	}
	end = src + inLen;
	dst = buf;
	while( src < end )
	{
		c = *src++;
		if( c == '\r' ) c = '\n';
		*dst++ = (TCHAR) c;
	}
	*dst = 0;
	
	// Print out the string to the debugger.
	
	OutputDebugString( buf );
}
#endif

#if( DEBUG_WINDOWS_EVENT_LOG_ENABLED )
//===========================================================================================================================
//	_LogOutputWindowsEventLog_Setup
//===========================================================================================================================

static OSStatus	_LogOutputWindowsEventLog_Setup( LogOutput *inOutput, const char *inParams )
{
	OSStatus			err;
	HKEY				key;
	const char *		programName;
	TCHAR				name[ 128 ];
	const char *		src;
	TCHAR				path[ MAX_PATH ];
	size_t				size;
	DWORD				typesSupported;
	DWORD 				n;
	BOOL				good;
	
	(void) inParams; // Unused

	key = NULL;
	
	// Build the path string using the fixed registry path and app name.
	
	src = "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\";
	programName = getprogname();
	_LogOutput_CharToTCharString( programName, kSizeCString, name, sizeof( name ), NULL );
	_LogOutput_CharToTCharString( src, kSizeCString, path, countof( path ), &size );
	_LogOutput_CharToTCharString( programName, kSizeCString, path + size, countof( path ) - size, NULL );
	
	// Add/Open the source name as a sub-key under the Application key in the EventLog registry key.
	
	err = RegCreateKeyEx( HKEY_LOCAL_MACHINE, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL );
	require_noerr_quiet( err, exit );
	
	// Set the path in the EventMessageFile subkey. Add 1 to the TCHAR count to include the null terminator.
	
	n = GetModuleFileName( NULL, path, countof( path ) );
	err = map_global_value_errno( n > 0, n );
	require_noerr_quiet( err, exit );
	n += 1;
	n *= sizeof( TCHAR );
	
	err = RegSetValueEx( key, TEXT( "EventMessageFile" ), 0, REG_EXPAND_SZ, (const LPBYTE) path, n );
	require_noerr_quiet( err, exit );
	
	// Set the supported event types in the TypesSupported subkey.
	
	typesSupported = EVENTLOG_SUCCESS | EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE |
					 EVENTLOG_AUDIT_SUCCESS | EVENTLOG_AUDIT_FAILURE;
	err = RegSetValueEx( key, TEXT( "TypesSupported" ), 0, REG_DWORD, (const LPBYTE) &typesSupported, sizeof( DWORD ) );
	require_noerr_quiet( err, exit );
	
	// Set up the event source.
	
	if( inOutput->config.windowsEventLog.source )
	{
		good = DeregisterEventSource( inOutput->config.windowsEventLog.source );
		err = map_global_value_errno( good, good );
		check_noerr( err );
	}
	inOutput->config.windowsEventLog.source = RegisterEventSource( NULL, name );
	err = map_global_value_errno( inOutput->config.windowsEventLog.source, inOutput->config.windowsEventLog.source );
	require_noerr_quiet( err, exit );
	
	inOutput->writer	= _LogOutputWindowsEventLog_Writer;
	inOutput->type		= kLogOutputType_WindowsEventLog;
	
exit:
	if( key ) RegCloseKey( key );
	return( err );
}

//===========================================================================================================================
//	_LogOutputWindowsEventLog_Writer
//===========================================================================================================================

static void	_LogOutputWindowsEventLog_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	WORD				type;
	TCHAR				buf[ 512 ];
	const char *		src;
	const char *		end;
	TCHAR *				dst;
	char				c;
	const TCHAR *		array[ 1 ];
	
	// Map the debug level to a Windows EventLog type.
	
	if(      inContext->level <= kLogLevelNotice )	type = EVENTLOG_INFORMATION_TYPE;
	else if( inContext->level <= kLogLevelWarning )	type = EVENTLOG_WARNING_TYPE;
	else											type = EVENTLOG_ERROR_TYPE;
	
	// Copy locally and null terminate the string. This also converts from char to TCHAR in case we are 
	// building with UNICODE enabled since the input is always char. Also convert \r to \n in the process.
	
	src = inStr;
	if( inLen >= countof( buf ) ) inLen = countof( buf ) - 1;
	end = src + inLen;
	dst = buf;
	while( src < end )
	{
		c = *src++;
		if( c == '\r' ) c = '\n';
		*dst++ = (TCHAR) c;
	}
	*dst = 0;
	
	// Add the the string to the event log.
	
	if( inOutput->config.windowsEventLog.source )
	{
		array[ 0 ] = buf;
		ReportEvent( inOutput->config.windowsEventLog.source, type, 0, 0x20000001L, NULL, 1, 0, array, NULL );
	}
}
#endif // DEBUG_WINDOWS_EVENT_LOG_ENABLED

#if( TARGET_OS_WINDOWS_KERNEL )
//===========================================================================================================================
//	_LogOutputWindowsKernel_Writer
//===========================================================================================================================

static void	_LogOutputWindowsKernel_Writer( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen )
{
	(void) inContext;
	(void) inOutput;
	
	DbgPrint( "%.*s", (int) inLen, inStr );
}
#endif

#if( TARGET_OS_WINDOWS )
//===========================================================================================================================
//	_LogOutput_CharToTCharString
//===========================================================================================================================

static TCHAR *
	_LogOutput_CharToTCharString( 
		const char *	inCharString, 
		size_t 			inCharCount, 
		TCHAR *			outTCharString, 
		size_t 			inTCharCountMax, 
		size_t *		outTCharCount )
{
	const char *		src;
	TCHAR *				dst;
	TCHAR *				end;
	
	if( inCharCount == kSizeCString ) inCharCount = strlen( inCharString );
	src = inCharString;
	dst = outTCharString;
	if( inTCharCountMax > 0 )
	{
		inTCharCountMax -= 1;
		if( inTCharCountMax > inCharCount ) inTCharCountMax = inCharCount;
		
		end = dst + inTCharCountMax;
		while( dst < end ) *dst++ = (TCHAR) *src++;
		*dst = 0;
	}
	if( outTCharCount ) *outTCharCount = (size_t)( dst - outTCharString );
	return( outTCharString );
}
#endif

#if( TARGET_OS_WINDOWS && !TARGET_OS_WINDOWS_CE )
//===========================================================================================================================
//	_LogOutput_EnableWindowsConsole
//===========================================================================================================================

#pragma warning( disable:4311 )

static void	_LogOutput_EnableWindowsConsole( void )
{
	static int		sConsoleEnabled = false;
	BOOL			result;
	int				fileHandle;
	FILE *			file;
	int				err;
	
	if( sConsoleEnabled ) goto exit;
	
	// Create console window.
	
	result = AllocConsole();
	require_quiet( result, exit );

	// Redirect stdin to the console stdin.
	
	fileHandle = _open_osfhandle( (intptr_t) GetStdHandle( STD_INPUT_HANDLE ), _O_TEXT );
	
	#if( defined( __MWERKS__ ) )
		file = __handle_reopen( (unsigned long) fileHandle, "r", stdin );
		require_quiet( file, exit );
	#else
		file = _fdopen( fileHandle, "r" );
		require_quiet( file, exit );
	
		*stdin = *file;
	#endif
	
	err = setvbuf( stdin, NULL, _IONBF, 0 );
	require_noerr_quiet( err, exit );
	
	// Redirect stdout to the console stdout.
	
	fileHandle = _open_osfhandle( (intptr_t) GetStdHandle( STD_OUTPUT_HANDLE ), _O_TEXT );
	
	#if( defined( __MWERKS__ ) )
		file = __handle_reopen( (unsigned long) fileHandle, "w", stdout );
		require_quiet( file, exit );
	#else
		file = _fdopen( fileHandle, "w" );
		require_quiet( file, exit );
		
		*stdout = *file;
	#endif
	
	err = setvbuf( stdout, NULL, _IONBF, 0 );
	require_noerr_quiet( err, exit );
	
	// Redirect stderr to the console stdout.
	
	fileHandle = _open_osfhandle( (intptr_t) GetStdHandle( STD_OUTPUT_HANDLE ), _O_TEXT );
	
	#if( defined( __MWERKS__ ) )
		file = __handle_reopen( (unsigned long) fileHandle, "w", stderr );
		require_quiet( file, exit );
	#else
		file = _fdopen( fileHandle, "w" );
		require_quiet( file, exit );
	
		*stderr = *file;
	#endif
	
	err = setvbuf( stderr, NULL, _IONBF, 0 );
	require_noerr_quiet( err, exit );
	
	sConsoleEnabled = true;
	
exit:
	return;
}

#pragma warning( default:4311 )

#endif // TARGET_OS_WINDOWS && !TARGET_OS_WINDOWS_CE

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	_LogOutput_IsStdErrMappedToDevNull
//===========================================================================================================================

static Boolean	_LogOutput_IsStdErrMappedToDevNull( void )
{
	Boolean			mapped = false;
	int				err;
	int				fd;
	struct stat		sb, sb2;
	
	fd = fileno( stderr );
	require_quiet( fd >= 0, exit );
	
	err = fstat( fd, &sb );
	require_noerr_quiet( err, exit );
	
	fd = open( "/dev/null", O_RDONLY );
	require_quiet( fd >= 0, exit );
	
	err = fstat( fd, &sb2 );
	close( fd );
	require_noerr_quiet( err, exit );
	
	if( ( sb.st_dev == sb2.st_dev ) && ( sb.st_ino == sb2.st_ino ) )
	{
		mapped = true;
	}
	
exit:
	return( mapped );
}
#endif

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	LogUtils_Test
//===========================================================================================================================

OSStatus	LogUtils_Test( void )
{
	OSStatus		err;
	
	err = kNoErr;
	require_noerr( err, exit );
	
exit:
	printf( "LogUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif
