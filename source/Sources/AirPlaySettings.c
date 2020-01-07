/*
	File:    	AirPlaySettings.c
	Package: 	CarPlay Communications Plug-in.
	Abstract: 	n/a 
	Version: 	280.33.8
	
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
	
	Copyright (C) 2010-2015 Apple Inc. All Rights Reserved.
*/

#include "AirPlaySettings.h"

#include <CoreUtils/CommonServices.h>
#include <CoreUtils/CFUtils.h>
#include <CoreUtils/SystemUtils.h>

#include "AirPlayCommon.h"

#include CF_HEADER

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#define kAirPlaySettingsBundleID		CFSTR( "com.apple.MediaToolbox" )
#define kAirPlaySettingsFilename		CFSTR( "AirPlaySettings.plist" )

//===========================================================================================================================
//	AirPlaySettings_CopyKeys
//===========================================================================================================================

#if( TARGET_OS_POSIX )
EXPORT_GLOBAL
CFArrayRef	AirPlaySettings_CopyKeys( OSStatus *outErr )
{
	CFMutableArrayRef		allKeys;
	OSStatus				err;
	CFArrayRef				keys;
	
	allKeys = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( allKeys, exit, err = kNoMemoryErr );
	
	keys = CFPreferencesCopyKeyList( CFSTR( kAirPlayPrefAppID ), kCFPreferencesCurrentUser, kCFPreferencesAnyHost );
	if( keys )
	{
		CFArrayAppendArray( allKeys, keys, CFRangeMake( 0, CFArrayGetCount( keys ) ) );
		CFRelease( keys );
	}
	
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( allKeys );
}
#endif // TARGET_OS_POSIX

//===========================================================================================================================
//	AirPlaySettings_CopyValue
//===========================================================================================================================

EXPORT_GLOBAL
CFTypeRef	AirPlaySettings_CopyValue( CFDictionaryRef *ioSettings, CFStringRef inKey, OSStatus *outErr )
{
	return( AirPlaySettings_CopyValueEx( ioSettings, inKey, 0, outErr ) );
}

EXPORT_GLOBAL
CFTypeRef	AirPlaySettings_CopyValueEx( CFDictionaryRef *ioSettings, CFStringRef inKey, CFTypeID inType, OSStatus *outErr )
{
	CFDictionaryRef		settings = (CFDictionaryRef)( ioSettings ? *ioSettings : NULL );
	CFTypeRef			value;
	OSStatus			err;
	
	value = CFPreferencesCopyAppValue( inKey, CFSTR( kAirPlayPrefAppID ) );
	if( value && ( inType != 0 ) && ( CFGetTypeID( value ) != inType ) )
	{
		dlog( kLogLevelNotice, "### Wrote type for AirPlay setting '%@': '%@'\n", inKey, value );
		CFRelease( value );
		value = NULL;
		err = kTypeErr;
		goto exit;
	}
	err = value ? kNoErr : kNotFoundErr;
	
exit:
	if( ioSettings )	*ioSettings = settings;
	else if( settings )	CFRelease( settings );
	if( outErr )		*outErr = err;
	return( value );
}

//===========================================================================================================================
//	AirPlaySettings_GetCString
//===========================================================================================================================

EXPORT_GLOBAL
char *
	AirPlaySettings_GetCString( 
		CFDictionaryRef *	ioSettings, 
		CFStringRef			inKey, 
		char *				inBuf, 
		size_t				inMaxLen, 
		OSStatus *			outErr )
{
	char *			value;
	CFTypeRef		cfValue;
	
	cfValue = AirPlaySettings_CopyValue( ioSettings, inKey, outErr );
	if( cfValue )
	{	
		value = CFGetCString( cfValue, inBuf, inMaxLen );
		CFRelease( cfValue );
		return( value );
	}
	return( NULL );
}

//===========================================================================================================================
//	AirPlaySettings_GetDouble
//===========================================================================================================================

EXPORT_GLOBAL
double	AirPlaySettings_GetDouble( CFDictionaryRef *ioSettings, CFStringRef inKey, OSStatus *outErr )
{
	double			value;
	CFTypeRef		cfValue;
	
	cfValue = AirPlaySettings_CopyValue( ioSettings, inKey, outErr );
	if( cfValue )
	{
		value = CFGetDouble( cfValue, outErr );
		CFRelease( cfValue );
		return( value );
	}
	return( 0 );
}

//===========================================================================================================================
//	AirPlaySettings_GetInt64
//===========================================================================================================================

EXPORT_GLOBAL
int64_t	AirPlaySettings_GetInt64( CFDictionaryRef *ioSettings, CFStringRef inKey, OSStatus *outErr )
{
	int64_t			value;
	CFTypeRef		cfValue;
	
	cfValue = AirPlaySettings_CopyValue( ioSettings, inKey, outErr );
	if( cfValue )
	{
		value = CFGetInt64( cfValue, outErr );
		CFRelease( cfValue );
		return( value );
	}
	return( 0 );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AirPlaySettings_SetCString
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus	AirPlaySettings_SetCString( CFStringRef inKey, const char *inStr, size_t inLen )
{
	OSStatus		err;
	CFStringRef		cfStr;
	
	if( inLen == kSizeCString )
	{
		cfStr = CFStringCreateWithCString( NULL, inStr, kCFStringEncodingUTF8 );
	}
	else
	{
		cfStr = CFStringCreateWithBytes( NULL, (const uint8_t *) inStr, (CFIndex) inLen, kCFStringEncodingUTF8, false );
	}
	require_action( cfStr, exit, err = kFormatErr );
	
	err = AirPlaySettings_SetValue( inKey, cfStr );
	CFRelease( cfStr );
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlaySettings_SetDouble
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus	AirPlaySettings_SetDouble( CFStringRef inKey, double inValue )
{
	return( AirPlaySettings_SetNumber( inKey, kCFNumberDoubleType, &inValue ) );
}

//===========================================================================================================================
//	AirPlaySettings_SetInt64
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus	AirPlaySettings_SetInt64( CFStringRef inKey, int64_t inValue )
{
	OSStatus		err;
	CFNumberRef		num;
	
	num = CFNumberCreateInt64( inValue );
	require_action( num, exit, err = kUnknownErr );
	
	err = AirPlaySettings_SetValue( inKey, num );
	CFRelease( num );
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlaySettings_SetNumber
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus	AirPlaySettings_SetNumber( CFStringRef inKey, CFNumberType inType, const void *inValue )
{
	OSStatus		err;
	CFNumberRef		num;
	
	num = CFNumberCreate( NULL, inType, inValue );
	require_action( num, exit, err = kUnknownErr );
	
	err = AirPlaySettings_SetValue( inKey, num );
	CFRelease( num );
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlaySettings_SetValue
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus	AirPlaySettings_SetValue( CFStringRef inKey, CFTypeRef inValue )
{
	CFPreferencesSetAppValue( inKey, inValue, CFSTR( kAirPlayPrefAppID ) );
	return( kNoErr );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AirPlaySettings_RemoveValue
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus	AirPlaySettings_RemoveValue( CFStringRef inKey )
{
	CFPreferencesSetAppValue( inKey, NULL, CFSTR( kAirPlayPrefAppID ) );
	return( kNoErr );
}

//===========================================================================================================================
//	AirPlaySettings_Synchronize
//===========================================================================================================================

EXPORT_GLOBAL
void	AirPlaySettings_Synchronize( void )
{
	CFPreferencesAppSynchronize( CFSTR( kAirPlayPrefAppID ) );
}

#if 0
#pragma mark -
#endif

