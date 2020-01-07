/*
	File:    	SystemUtils.c
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
	
	Copyright (C) 2010-2014 Apple Inc. All Rights Reserved.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "SystemUtils.h"

#include <string.h>

#include "CFPrefUtils.h"
#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "PrintFUtils.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "UUIDUtils.h"

#if( TARGET_OS_DARWIN )
	#include <dispatch/dispatch.h>
	
	#include <CoreFoundation/CFPriv.h>
	#include <SystemConfiguration/SystemConfiguration.h>
#endif


#if( TARGET_OS_MACOSX )
	#include <IOKit/IOKitLib.h>
	#include <IOKit/network/IOEthernetController.h>
	#include <IOKit/network/IOEthernetInterface.h>
	#include <IOKit/network/IONetworkInterface.h>
	
	#include "MiscUtils.h"
#endif

#if( TARGET_OS_POSIX )
	#include <sys/stat.h>
	#include <sys/sysctl.h>
#endif

#if( TARGET_OS_DARWIN )
	static CFPropertyListRef	_CreatePlistFromBundleFile( CFStringRef inBundleID, CFStringRef inFilename, CFStringRef inSubDirName );
#endif

//===========================================================================================================================
//	Gestalt
//===========================================================================================================================

MinimalMutexDefine( gGestaltLock );

static CFStringRef			gGestaltDomain = NULL;
static GestaltHook_f		gGestaltHook_f   = NULL;
static void *				gGestaltHook_ctx = NULL;

ulog_define( Gestalt, kLogLevelNotice, kLogFlags_Default, "Gestalt", NULL );
#define gestalt_ucat()					&log_category_from_name( Gestalt )
#define gestalt_dlog( LEVEL, ... )		dlogc( gestalt_ucat(), (LEVEL), __VA_ARGS__ )
#define gestalt_ulog( LEVEL, ... )		ulog( gestalt_ucat(), (LEVEL), __VA_ARGS__ )

#if( TARGET_OS_DARWIN )
//===========================================================================================================================
//	CopyDeviceSettingsPlist
//===========================================================================================================================

CFDictionaryRef	CopyDeviceSettingsPlist( CFStringRef inBundleID, CFStringRef inFilename, CFStringRef inModel )
{
	CFPropertyListRef		plist;
	CFStringRef				model;
	char					tempStr[ 64 ];
	
	plist = NULL;
	model = NULL;
	
	// If no model is passed in, use the model of the current device.
	
	if( !inModel )
	{
		*tempStr = '\0';
		GetDeviceInternalModelString( tempStr, sizeof( tempStr ) );
		model = CFStringCreateWithCString( NULL, tempStr, kCFStringEncodingUTF8 );
		require( model, exit );
		inModel = model;
	}
	
	// Look for bundle/<model>/file.
	// If missing, look for bundle/<model>.plist.useModel/file.
	// If missing, look for bundle/DefaultModel.plist.useModel/file.
	// If missing, look for bundle/file.
	
	plist = (CFDictionaryRef) _CreatePlistFromBundleFile( inBundleID, inFilename, inModel );
	if( !plist )
	{
		CFStringRef			modelPlistFilename;
		CFDictionaryRef		useModelDict;
		CFStringRef			alternateModel;
		
		// Look for <model>.plist to get the model we should use.
		
		modelPlistFilename = CFStringCreateWithFormat( NULL, NULL, CFSTR( "%@.plist" ), inModel );
		require( modelPlistFilename, exit );
		
		useModelDict = (CFDictionaryRef) _CreatePlistFromBundleFile( inBundleID, modelPlistFilename, NULL );
		CFRelease( modelPlistFilename );
		if( !useModelDict )
		{
			// No <model>.plist so fall back to DefaultModel.plist for the model we should use.
			
			useModelDict = (CFDictionaryRef) _CreatePlistFromBundleFile( inBundleID, CFSTR( "DefaultModel.plist" ), NULL );
		}
		if( useModelDict && ( CFGetTypeID( useModelDict ) != CFDictionaryGetTypeID() ) )
		{
			dlogassert( "Default model plist must be a dictionary" );
			CFRelease( useModelDict );
			useModelDict = NULL;
		}
		if( useModelDict )
		{
			alternateModel = (CFStringRef) CFDictionaryGetValue( useModelDict, CFSTR( "UseModel" ) );
			if( alternateModel && ( CFGetTypeID( alternateModel ) != CFStringGetTypeID() ) )
			{
				dlogassert( "Alternate model must be a string" );
				alternateModel = NULL;
			}
			if( alternateModel )
			{
				plist = (CFDictionaryRef) _CreatePlistFromBundleFile( inBundleID, inFilename, alternateModel );
			}
			CFRelease( useModelDict );
		}
	}
	if( !plist )
	{
		// Nothing more specific found so look for bundle/file.
		
		plist = (CFDictionaryRef) _CreatePlistFromBundleFile( inBundleID, inFilename, NULL );
	}
	if( plist && ( CFGetTypeID( plist ) != CFDictionaryGetTypeID() ) )
	{
		dlogassert( "Settings plist must be a dictionary" );
		CFRelease( plist );
		plist = NULL;
	}
	
exit:
	if( model ) CFRelease( model );
	return( plist );
}
#endif // TARGET_OS_DARWIN

#if( TARGET_OS_DARWIN )
//===========================================================================================================================
//	_CreatePlistFromBundleFile
//===========================================================================================================================

static CFPropertyListRef	_CreatePlistFromBundleFile( CFStringRef inBundleID, CFStringRef inFilename, CFStringRef inSubDirName )
{
	CFPropertyListRef		plist;
	CFBundleRef				bundle;
	CFURLRef				url;
	CFReadStreamRef			readStream;
	Boolean					good;
	
	plist		= NULL;
	readStream	= NULL;
	
	bundle = CFBundleGetBundleWithIdentifier( inBundleID );
	require_quiet( bundle, exit );
	
	url = CFBundleCopyResourceURL( bundle, inFilename, NULL, inSubDirName );
	require_quiet( url, exit ); // Quiet to allow testing if subdirs exist.
	
	readStream = CFReadStreamCreateWithFile( NULL, url );
	CFRelease( url );
	require( readStream, exit );
	
	good = CFReadStreamOpen( readStream );
	require( good, exit );
	
	plist = CFPropertyListCreateWithStream( NULL, readStream, 0, kCFPropertyListImmutable, NULL, NULL );
	require( plist, exit );
	
exit:
	if( readStream ) CFRelease( readStream );
	return( plist );
}
#endif

#if( TARGET_OS_DARWIN )
//===========================================================================================================================
//	CopySystemVersionPlist
//===========================================================================================================================

CFDictionaryRef	CopySystemVersionPlist( void )
{
	CFDictionaryRef		plist;
	CFURLRef			url;
	CFReadStreamRef		readStream;
	Boolean				good;
	
	plist		= NULL;
	readStream	= NULL;
	
	url = CFURLCreateWithFileSystemPath( NULL, CFSTR( "/System/Library/CoreServices/SystemVersion.plist" ), 
		kCFURLPOSIXPathStyle, false );
	require_quiet( url, exit ); // Quiet to allow testing if subdirs exist.
	
	readStream = CFReadStreamCreateWithFile( NULL, url );
	CFRelease( url );
	require( readStream, exit );
	
	good = CFReadStreamOpen( readStream );
	require( good, exit );
	
	plist = (CFDictionaryRef) CFPropertyListCreateWithStream( NULL, readStream, 0, kCFPropertyListImmutable, NULL, NULL );
	require( plist, exit );
	if( !CFIsType( plist, CFDictionary ) )
	{
		dlogassert( "Bad system version plist type: %@", plist );
		CFRelease( plist );
		plist = NULL;
	}
	
exit:
	if( readStream ) CFRelease( readStream );
	return( plist );
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	GestaltSetDomain
//===========================================================================================================================

void	GestaltSetDomain( CFStringRef inDomain )
{
	MinimalMutexEnsureInitialized( gGestaltLock );
	MinimalMutexLock( gGestaltLock );
	ReplaceCF( &gGestaltDomain, inDomain );
	MinimalMutexUnlock( gGestaltLock );
}

//===========================================================================================================================
//	GestaltSetHook
//===========================================================================================================================

void	GestaltSetHook( GestaltHook_f inHook, void *inContext )
{
	gGestaltHook_f   = inHook;
	gGestaltHook_ctx = inContext;
}

//===========================================================================================================================
//	GestaltCopyAnswer
//===========================================================================================================================

CFTypeRef	GestaltCopyAnswer( CFStringRef inQuestion, CFDictionaryRef inOptions, OSStatus *outErr )
{
	CFTypeRef		answer;
	OSStatus		err;
	
	if( gGestaltHook_f )
	{
		answer = gGestaltHook_f( inQuestion, inOptions, outErr, gGestaltHook_ctx );
		if( answer ) return( answer );
	}
	
	if( CFEqual( inQuestion, kGestaltSystemUUID ) )
	{
		uint8_t		uuid[ 16 ];
		
		GetSystemUUID( -1, uuid );
		answer = CFDataCreate( NULL, uuid, 16 );
		require_action( answer, exit, err = kNoMemoryErr );
	}
	else
	{
		answer = NULL;
		err = kNotFoundErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( answer );
}

//===========================================================================================================================
//	GestaltGetBoolean
//===========================================================================================================================

Boolean	GestaltGetBoolean( CFStringRef inQuestion, CFDictionaryRef inOptions, OSStatus *outErr )
{
	Boolean			b;
	CFTypeRef		obj;
	
	obj = GestaltCopyAnswer( inQuestion, inOptions, outErr );
	if( obj )
	{
		b = CFGetBoolean( obj, outErr );
		CFRelease( obj );
	}
	else
	{
		b = false;
	}
	return( b );
}

//===========================================================================================================================
//	GestaltGetCString
//===========================================================================================================================

char *	GestaltGetCString( CFStringRef inQuestion, CFDictionaryRef inOptions, char *inBuf, size_t inMaxLen, OSStatus *outErr )
{
	CFTypeRef		obj;
	char *			ptr;
	
	obj = GestaltCopyAnswer( inQuestion, inOptions, outErr );
	if( obj )
	{
		ptr = CFGetCString( obj, inBuf, inMaxLen );
		CFRelease( obj );
		if( outErr ) *outErr = kNoErr;
	}
	else
	{
		ptr = inBuf;
	}
	return( ptr );
}

//===========================================================================================================================
//	GestaltGetData
//===========================================================================================================================

uint8_t *
	GestaltGetData( 
		CFStringRef		inQuestion, 
		CFDictionaryRef	inOptions, 
		void *			inBuf, 
		size_t			inMaxLen, 
		size_t *		outLen, 
		OSStatus *		outErr )
{
	uint8_t *		ptr;
	CFTypeRef		obj;
	
	obj = GestaltCopyAnswer( inQuestion, inOptions, outErr );
	if( obj )
	{
		ptr = CFGetData( obj, inBuf, inMaxLen, outLen, outErr );
		CFRelease( obj );
	}
	else
	{
		ptr = NULL;
		if( outLen ) *outLen = 0;
	}
	return( ptr );
}

#if 0
#pragma mark -
#endif

#if( TARGET_OS_DARWIN )
//===========================================================================================================================
//	GetDeviceModelString
//===========================================================================================================================

char *	GetDeviceModelString( char *inBuf, size_t inMaxLen )
{
	OSStatus		err;
	char			tempStr[ 64 ];
	size_t			len;
	
	// Get the model property (e.g. iPhone1,1).
	// Prototype model names don't contain commas so if there's no comma, don't use it.
	// Prototypes may use "iProd" so exclude those too.
	
	*tempStr = '\0';
	len = sizeof( tempStr ) - 1;
	err = sysctlbyname( "hw.model", tempStr, &len, NULL, 0 );
	if( err || !memchr( tempStr, ',', len ) || strnstr( tempStr, "iProd", len ) )
	{
		len = 0;
	}
	tempStr[ len ] = '\0';
	strlcpy( inBuf, tempStr, inMaxLen );
	return( inBuf );
}
#endif

//===========================================================================================================================
//	GetDeviceInternalModelString
//===========================================================================================================================

#if( TARGET_IPHONE_SIMULATOR )
char *	GetDeviceInternalModelString( char *inBuf, size_t inMaxLen )
{
	strlcpy( inBuf, "Sim", inMaxLen );
	return( inBuf );
}
#endif


#if( TARGET_OS_MACOSX )
char *	GetDeviceInternalModelString( char *inBuf, size_t inMaxLen )
{
	// Internal models not available on Macs so use the public model.
	return( GetDeviceModelString( inBuf, inMaxLen ) );
}
#endif

//===========================================================================================================================
//	GetDeviceName
//===========================================================================================================================


#if( TARGET_OS_MACOSX )
char *	GetDeviceName( char *inBuf, size_t inMaxLen )
{
	CFStringRef		name;
	Boolean			good;
	
	name = SCDynamicStoreCopyComputerName( NULL, NULL );
	if( !name ) { name = CFSTR( "" ); CFRetain( name ); }
	
	if( inMaxLen > 0 ) *inBuf = '\0';
	good = CFStringGetCString( name, inBuf, (CFIndex) inMaxLen, kCFStringEncodingUTF8 );
	check( good );
	CFRelease( name );
	return( ( inMaxLen > 0 ) ? inBuf : "" );
}
#endif

#if( TARGET_OS_POSIX && !TARGET_OS_DARWIN )
char *	GetDeviceName( char *inBuf, size_t inMaxLen )
{
	if( inMaxLen > 0 )
	{
		inBuf[ 0 ] = '\0';
		gethostname( inBuf, inMaxLen );
		inBuf[ inMaxLen - 1 ] = '\0';
		return( inBuf );
	}
	return( "" );
}
#endif

//===========================================================================================================================
//	GetDeviceName
//===========================================================================================================================

#if( TARGET_OS_WINDOWS )
char *	GetDeviceName( char *inBuf, size_t inMaxLen )
{
	OSStatus		err;
	char			nameUTF8[ 256 ];
	HKEY			regKey;
	WCHAR			utf16[ 256 ];
	DWORD			utf16Size;
	BOOL			good;
	int				len;
	
	if( inMaxLen < 1 ) return( "" );
	nameUTF8[ 0 ] = '\0';
	
	// First try to get the computer description from the registry.
	
	err = RegOpenKeyEx( HKEY_LOCAL_MACHINE, TEXT( "SYSTEM\\CurrentControlSet\\Services\\lanmanserver\\parameters" ), 0, 
		KEY_READ, &regKey );
	if( !err )
	{
		utf16Size = sizeof( utf16 );
		err = RegQueryValueExW( regKey, L"srvcomment", 0, NULL, (LPBYTE) utf16, &utf16Size );
		RegCloseKey( regKey );
		if( !err )
		{
			len = WideCharToMultiByte( CP_UTF8, 0, utf16, -1, nameUTF8, (int) sizeof( nameUTF8 ), NULL, NULL );
			err = map_global_value_errno( len > 0, len );
			check_noerr( err );
		}
	}
	
	// Next, try GetComputerNameEx. Try this before gethostname for <radar:4249284>.
	
	if( nameUTF8[ 0 ] == '\0' )
	{
		utf16Size = countof( utf16 );
		good = GetComputerNameExW( ComputerNamePhysicalDnsHostname, utf16, &utf16Size );
		err = map_global_value_errno( good, good );
		if( !err )
		{
			len = WideCharToMultiByte( CP_UTF8, 0, utf16, -1, nameUTF8, (int) sizeof( nameUTF8 ), NULL, NULL );
			err = map_global_value_errno( len > 0, len );
			check_noerr( err );
		}
	}
	
	// Finally, try gethostname. If that fails, use a default.
	
	if( nameUTF8[ 0 ] == '\0' )
	{
		err = gethostname( nameUTF8, sizeof( nameUTF8 ) - 1 );
		err = map_noerr_errno( err );
		check_noerr( err );
	}
	
	// If we couldn't get a name then use a placeholder so we always return something.
	
	if( nameUTF8[ 0 ] == '\0' )
	{
		strlcpy( nameUTF8, "Device", sizeof( nameUTF8 ) );
	}
	
	strlcpy( inBuf, nameUTF8, inMaxLen );
	return( inBuf );
}
#endif

//===========================================================================================================================
//	GetDeviceUniqueID
//===========================================================================================================================


#if( TARGET_OS_MACOSX )
char *	GetDeviceUniqueID( char *inBuf, size_t inMaxLen )
{
	OSStatus					err;
	CFMutableDictionaryRef		matchDict;
	CFMutableDictionaryRef		propertyMatchDict;
	io_object_t					service, controllerService = IO_OBJECT_NULL;
	CFDataRef					macData = NULL;
	uint8_t						a[ 6 ];
	
	matchDict = IOServiceMatching( kIOEthernetInterfaceClass );
	require_action( matchDict, exit, err = kIOReturnNoMemory );
	
	propertyMatchDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    require_action( propertyMatchDict, exit, err = kIOReturnNoMemory );
	CFDictionarySetValue( propertyMatchDict, CFSTR( kIOPrimaryInterface ), kCFBooleanTrue );
	CFDictionarySetValue( matchDict, CFSTR( kIOPropertyMatchKey ), propertyMatchDict );
	CFRelease( propertyMatchDict );
	
	service = IOServiceGetMatchingService( kIOMasterPortDefault, matchDict );
	matchDict = NULL; // Consumed by the previous function.
	require_action( service, exit, err = kIOReturnNotFound );
	
	err = IORegistryEntryGetParentEntry( service, kIOServicePlane, &controllerService );
	IOObjectRelease( service );
	require_noerr( err, exit );
	
	macData = (CFDataRef) IORegistryEntryCreateCFProperty( controllerService, CFSTR( kIOMACAddress ), NULL, 0 );
	require_action( macData, exit, err = kIOReturnNotFound );
	require_action( CFIsType( macData, CFData ), exit, err = kIOReturnInvalid );
	require_action( CFDataGetLength( macData ) == kIOEthernetAddressSize, exit, err = kIOReturnUnderrun );
	CFDataGetBytes( macData, CFRangeMake( 0, kIOEthernetAddressSize ), a );
	
	snprintf( inBuf, inMaxLen, "0000000000000000000000000000%02x%02x%02x%02x%02x%02x", 
		a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], a[ 5 ] );
	
exit:
	if( macData )			CFRelease( macData );
	if( controllerService )	IOObjectRelease( controllerService );
	if( matchDict )			CFRelease( matchDict );
	return( inBuf );
}
#endif

//===========================================================================================================================
//	GetSystemBuildVersionString
//===========================================================================================================================


#if( TARGET_OS_MACOSX )
char *	GetSystemBuildVersionString( char *inBuf, size_t inMaxLen )
{
	CFDictionaryRef		plist;
	CFStringRef			tempCFStr;
	
	plist = CopySystemVersionPlist();
	require( plist, exit );
	
	tempCFStr = (CFStringRef) CFDictionaryGetValue( plist, _kCFSystemVersionBuildVersionKey );
	require( tempCFStr, exit );
	require( CFIsType( tempCFStr, CFString ), exit );
	
	CFStringGetCString( tempCFStr, inBuf, (CFIndex) inMaxLen, kCFStringEncodingUTF8 );
	
exit:
	if( plist ) CFRelease( plist );
	return( inBuf );
}
#endif

//===========================================================================================================================
//	GetSystemUUID
//
//	Per-host, semi-persistent UUID. Save <UUID:CFAbsoluteTime> (e.g. "7d9ede2a-d023-4b88-8602-eeb6e0307d80:12345678").
//===========================================================================================================================

void	GetSystemUUID( int inMaxAgeSecs, uint8_t outUUID[ 16 ] )
{
	CFStringRef			appID;
	char				cstr[ 128 ];
	char *				ptr;
	Boolean				valid;
	CFAbsoluteTime		t = 0;
	
	MinimalMutexEnsureInitialized( gGestaltLock );
	MinimalMutexLock( gGestaltLock );
	
	appID = gGestaltDomain ? gGestaltDomain : kCFPreferencesCurrentApplication;
	*cstr = '\0';
	CFPrefs_GetCString( appID, CFSTR( "systemUUID" ), cstr, sizeof( cstr ), NULL );
	ptr = strchr( cstr, ':' );
	valid = ptr ? ( StringToUUID( cstr, (size_t)( ptr - cstr ), false, outUUID ) == kNoErr ) : false;
	if( valid ) valid = ( sscanf( ptr + 1, "%lf", &t ) == 1 );
	if( valid ) valid = ( ( inMaxAgeSecs < 0 ) || ( ( ( t = CFAbsoluteTimeGetCurrent() - t ) >= 0 ) && ( t < inMaxAgeSecs ) ) );
	if( valid ) gestalt_ulog( kLogLevelVerbose, "Reused system UUID %s\n", cstr );
	else
	{
		UUIDGet( outUUID );
		SNPrintF( cstr, sizeof( cstr ), "%#U:%f", outUUID, CFAbsoluteTimeGetCurrent() );
		CFPrefs_SetCString( appID, CFSTR( "systemUUID" ), cstr, kSizeCString );
		CFPrefs_Synchronize( appID );
		gestalt_ulog( kLogLevelInfo, "Generated system UUID %s\n", cstr );
	}
	
	MinimalMutexUnlock( gGestaltLock );
}

#if( TARGET_OS_MACOSX )
//===========================================================================================================================
//	HasIvyBridge
//===========================================================================================================================

Boolean	HasIvyBridge( void )
{
	static dispatch_once_t		sOnce	= 0;
	static Boolean				sResult	= false;
	
	dispatch_once( &sOnce, 
	^{
		int    val = 0;
		size_t len = sizeof( val );
		int    err = sysctlbyname( "hw.optional.f16c", &val, &len, NULL, 0 );
		if( !err && val )
		{
			sResult = true;
		}
	} );
	return( sResult );
}
#endif


