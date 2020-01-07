/*
	File:    	CFPrefUtils.c
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

#include "CFPrefUtils.h"

#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"

//===========================================================================================================================
//	CFPrefs_CopyKeys
//===========================================================================================================================

CFArrayRef	CFPrefs_CopyKeys( CFStringRef inAppID, OSStatus *outErr )
{
	CFArrayRef		keys;
	OSStatus		err;
	
	if( !inAppID ) inAppID = kCFPreferencesCurrentApplication;
	keys = CFPreferencesCopyKeyList( inAppID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost );
	if( !keys ) keys = CFArrayCreate( NULL, NULL, 0, &kCFTypeArrayCallBacks );
	require_action( keys, exit, err = kUnknownErr );
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( keys );
}

//===========================================================================================================================
//	_CFPrefs_CopyValue
//===========================================================================================================================

CFTypeRef	_CFPrefs_CopyValue( CFTypeRef inAppID, CFStringRef inKey, OSStatus *outErr )
{
	CFTypeRef		value = NULL;
	OSStatus		err;
	
	if( !inAppID ) inAppID = kCFPreferencesCurrentApplication;
	require_action( CFIsType( inAppID, CFString ), exit, err = kParamErr );
	
	value = CFPreferencesCopyAppValue( inKey, (CFStringRef) inAppID );
	err = value ? kNoErr : kNotFoundErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	_CFPrefs_SetValue
//===========================================================================================================================

OSStatus	_CFPrefs_SetValue( CFTypeRef inAppID, CFStringRef inKey, CFTypeRef inValue )
{
	OSStatus		err;
	
	if( !inAppID ) inAppID = kCFPreferencesCurrentApplication;
	require_action( CFIsType( inAppID, CFString ), exit, err = kParamErr );
	
	CFPreferencesSetAppValue( inKey, inValue, (CFStringRef) inAppID );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFPrefs_CopyTypedValue
//===========================================================================================================================

CFTypeRef	CFPrefs_CopyTypedValue( CFStringRef inAppID, CFStringRef inKey, CFTypeID inType, OSStatus *outErr )
{
	CFTypeRef		obj;
	OSStatus		err;
	
	if( !inAppID ) inAppID = kCFPreferencesCurrentApplication;
	obj = CFPreferencesCopyAppValue( inKey, inAppID );
	require_action_quiet( obj, exit, err = kNotFoundErr );
	if( ( inType != 0 ) && ( CFGetTypeID( obj ) != inType ) )
	{
		dlog( kLogLevelNotice, "### Wrong type for pref domain '%@', key '%@': value '%@'\n", inAppID, inKey, obj );
		CFRelease( obj );
		obj = NULL;
		err = kTypeErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( obj );
}

//===========================================================================================================================
//	CFPrefs_RemoveValue
//===========================================================================================================================

OSStatus	CFPrefs_RemoveValue( CFStringRef inAppID, CFStringRef inKey )
{
	if( !inAppID ) inAppID = kCFPreferencesCurrentApplication;
	CFPreferencesSetAppValue( inKey, NULL, inAppID );
	return( kNoErr );
}

//===========================================================================================================================
//	CFPrefs_SetValue
//===========================================================================================================================

OSStatus	CFPrefs_SetValue( CFStringRef inAppID, CFStringRef inKey, CFTypeRef inValue )
{
	if( !inAppID ) inAppID = kCFPreferencesCurrentApplication;
	CFPreferencesSetAppValue( inKey, inValue, inAppID );
	return( kNoErr );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CFPrefs_GetCString
//===========================================================================================================================

char *	CFPrefs_GetCString( CFStringRef inAppID, CFStringRef inKey, char *inBuf, size_t inMaxLen, OSStatus *outErr )
{
	char *			ptr;
	CFTypeRef		obj;
	
	obj = CFPrefs_CopyValue( inAppID, inKey, outErr );
	if( obj )
	{
		ptr = CFGetCString( obj, inBuf, inMaxLen );
		CFRelease( obj );
	}
	else if( inMaxLen > 0 )
	{
		*inBuf = '\0';
		ptr = inBuf;
	}
	else
	{
		ptr = "";
	}
	return( ptr );
}

//===========================================================================================================================
//	CFPrefs_SetCString
//===========================================================================================================================

OSStatus	CFPrefs_SetCString( CFStringRef inAppID, CFStringRef inKey, const char *inStr, size_t inLen )
{
	OSStatus		err;
	CFStringRef		obj;
	
	if( inLen == kSizeCString )
	{
		obj = CFStringCreateWithCString( NULL, inStr, kCFStringEncodingUTF8 );
	}
	else
	{
		obj = CFStringCreateWithBytes( NULL, (const uint8_t *) inStr, (CFIndex) inLen, kCFStringEncodingUTF8, false );
	}
	require_action( obj, exit, err = kFormatErr );
	
	err = CFPrefs_SetValue( inAppID, inKey, obj );
	CFRelease( obj );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFPrefs_GetData
//===========================================================================================================================

uint8_t *	CFPrefs_GetData( CFStringRef inAppID, CFStringRef inKey, void *inBuf, size_t inMaxLen, size_t *outLen, OSStatus *outErr )
{
	uint8_t *		ptr;
	CFTypeRef		obj;
	
	obj = CFPrefs_CopyValue( inAppID, inKey, outErr );
	if( obj )
	{
		ptr = CFGetData( obj, inBuf, inMaxLen, outLen, outErr );
		CFRelease( obj );
	}
	else
	{
		ptr = (uint8_t *) inBuf;
		if( outLen ) *outLen = 0;
	}
	return( ptr );
}

//===========================================================================================================================
//	CFPrefs_GetDouble
//===========================================================================================================================

double	CFPrefs_GetDouble( CFStringRef inAppID, CFStringRef inKey, OSStatus *outErr )
{
	double			value;
	CFTypeRef		obj;
	
	obj = CFPrefs_CopyValue( inAppID, inKey, outErr );
	if( obj )
	{
		value = CFGetDouble( obj, outErr );
		CFRelease( obj );
		return( value );
	}
	return( 0 );
}

//===========================================================================================================================
//	CFPrefs_SetDouble
//===========================================================================================================================

OSStatus	CFPrefs_SetDouble( CFStringRef inAppID, CFStringRef inKey, double inValue )
{
	OSStatus		err;
	CFNumberRef		obj;
	
	obj = CFNumberCreate( NULL, kCFNumberDoubleType, &inValue );
	require_action( obj, exit, err = kUnknownErr );
	
	err = CFPrefs_SetValue( inAppID, inKey, obj );
	CFRelease( obj );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFPrefs_GetInt64
//===========================================================================================================================

int64_t	CFPrefs_GetInt64( CFStringRef inAppID, CFStringRef inKey, OSStatus *outErr )
{
	int64_t			value;
	CFTypeRef		obj;
	
	obj = CFPrefs_CopyValue( inAppID, inKey, outErr );
	if( obj )
	{
		value = CFGetInt64( obj, outErr );
		CFRelease( obj );
		return( value );
	}
	return( 0 );
}

//===========================================================================================================================
//	CFPrefs_SetInt64
//===========================================================================================================================

OSStatus	CFPrefs_SetInt64( CFStringRef inAppID, CFStringRef inKey, int64_t inValue )
{
	OSStatus		err;
	CFNumberRef		obj;
	
	obj = CFNumberCreateInt64( inValue );
	require_action( obj, exit, err = kUnknownErr );
	
	err = CFPrefs_SetValue( inAppID, inKey, obj );
	CFRelease( obj );
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	CFPrefUtils_Test
//===========================================================================================================================

#define kTestDomain		CFSTR( "com.apple.CFPrefUtils_Test" )

OSStatus	CFPrefUtils_Test( void )
{
	OSStatus		err;
	CFArrayRef		keys = NULL;
	CFIndex			i, n;
	CFStringRef		cfstr;
	char			cstr[ 128 ];
	char *			cptr;
	CFTypeRef		obj;
	Boolean			b, b1, b2, b3, b4, b5;
	uint8_t *		ptr;
	uint8_t			buf[ 64 ];
	size_t			len;
	
	// Test NULL domain (current app).
	
	keys = CFPrefs_CopyKeys( NULL, NULL );
	n = keys ? CFArrayGetCount( keys ) : 0;
	for( i = 0; i < n; ++i )
	{
		cfstr = (CFStringRef) CFArrayGetValueAtIndex( keys, i );
		CFPrefs_SetValue( NULL, cfstr, NULL );
	}
	ForgetCF( &keys );
	
	err = CFPrefs_SetBoolean( NULL, CFSTR( "key1" ), true );
	require_noerr( err, exit );
	b = CFPrefs_GetBoolean( NULL, CFSTR( "key1" ), &err );
	require_noerr( err, exit );
	require_action( b, exit, err = kResponseErr );
	
	keys = CFPrefs_CopyKeys( NULL, &err );
	require_noerr( err, exit );
	require_action( keys, exit, err = kResponseErr );
	require_action( CFArrayGetCount( keys ) == 1, exit, err = kResponseErr );
	require_action( CFEqual( CFArrayGetValueAtIndex( keys, 0 ), CFSTR( "key1" ) ), exit, err = kResponseErr );
	CFPrefs_SetValue( NULL, CFSTR( "key1" ), NULL );
	ForgetCF( &keys );
	
	// Remove any existing prefs from a previous failed test to start from a clean slate.
	
	keys = CFPreferencesCopyKeyList( kTestDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost );
	n = keys ? CFArrayGetCount( keys ) : 0;
	for( i = 0; i < n; ++i )
	{
		cfstr = (CFStringRef) CFArrayGetValueAtIndex( keys, i );
		CFPreferencesSetAppValue( cfstr, NULL, kTestDomain );
	}
	ForgetCF( &keys );
	
	// Boolean
	
	require_action( !CFPrefs_GetBoolean( kTestDomain, CFSTR( "key1" ), NULL ), exit, err = kResponseErr );
	require_action( !CFPrefs_GetBoolean( kTestDomain, CFSTR( "key1" ), &err ), exit, err = kResponseErr );
	require_action( err, exit, err = kResponseErr );
	
	err = CFPrefs_SetBoolean( kTestDomain, CFSTR( "key1" ), false );
	require_noerr( err, exit );
	obj = CFPrefs_CopyValue( kTestDomain, CFSTR( "key1" ), NULL );
	b = CFIsType( obj, CFBoolean );
	CFReleaseNullSafe( obj );
	require_action( b, exit, err = kResponseErr );
	*cstr = '\0';
	CFPrefs_GetCString( kTestDomain, CFSTR( "key1" ), cstr, sizeof( cstr ), &err );
	require_noerr( err, exit );
	require_action( strcmp( cstr, "false" ) == 0, exit, err = kResponseErr );
	
	require_action( !CFPrefs_GetBoolean( kTestDomain, CFSTR( "key1" ), NULL ), exit, err = kResponseErr );
	require_action( !CFPrefs_GetBoolean( kTestDomain, CFSTR( "key1" ), &err ), exit, err = kResponseErr );
	require_noerr( err, exit );
	
	err = CFPrefs_SetValue( kTestDomain, CFSTR( "key1" ), CFSTR( "true" ) );
	require_noerr( err, exit );
	require_action( CFPrefs_GetBoolean( kTestDomain, CFSTR( "key1" ), NULL ), exit, err = kResponseErr );
	require_action( CFPrefs_GetBoolean( kTestDomain, CFSTR( "key1" ), &err ), exit, err = kResponseErr );
	require_noerr( err, exit );
	
	// Data
	
	err = kNoErr;
	ptr = CFPrefs_GetData( kTestDomain, CFSTR( "key5" ), buf, sizeof( buf ), &len, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( ptr == buf, exit, err = -1 );
	
	err = CFPrefs_SetData( kTestDomain, CFSTR( "key5" ), "\x11\xAA\x22", 3 );
	require_noerr( err, exit );
	ptr = CFPrefs_GetData( kTestDomain, CFSTR( "key5" ), buf, sizeof( buf ), &len, &err );
	require_noerr( err, exit );
	require_action( ptr && ( ptr == buf ) && MemEqual( ptr, len, "\x11\xAA\x22", 3 ), exit, err = -1 );
	
	// Double
	
	require_action( CFPrefs_GetDouble( kTestDomain, CFSTR( "key2" ), NULL ) == 0, exit, err = kResponseErr );
	require_action( CFPrefs_GetDouble( kTestDomain, CFSTR( "key2" ), &err ) == 0, exit, err = kResponseErr );
	require_action( err, exit, err = kResponseErr );
	
	err = CFPrefs_SetDouble( kTestDomain, CFSTR( "key2" ), 123.45 );
	require_noerr( err, exit );
	obj = CFPrefs_CopyValue( kTestDomain, CFSTR( "key2" ), NULL );
	b = CFIsType( obj, CFNumber );
	CFReleaseNullSafe( obj );
	require_action( b, exit, err = kResponseErr );
	*cstr = '\0';
	CFPrefs_GetCString( kTestDomain, CFSTR( "key2" ), cstr, sizeof( cstr ), &err );
	require_noerr( err, exit );
	require_action( strcmp( cstr, "123.450000" ) == 0, exit, err = kResponseErr );
	
	require_action( CFPrefs_GetDouble( kTestDomain, CFSTR( "key2" ), NULL ) == 123.45, exit, err = kResponseErr );
	require_action( CFPrefs_GetDouble( kTestDomain, CFSTR( "key2" ), &err ) == 123.45, exit, err = kResponseErr );
	require_noerr( err, exit );
	
	err = CFPrefs_SetValue( kTestDomain, CFSTR( "key2" ), CFSTR( "-123.45" ) );
	require_noerr( err, exit );
	require_action( CFPrefs_GetDouble( kTestDomain, CFSTR( "key2" ), NULL ) == -123.45, exit, err = kResponseErr );
	require_action( CFPrefs_GetDouble( kTestDomain, CFSTR( "key2" ), &err ) == -123.45, exit, err = kResponseErr );
	require_noerr( err, exit );
	
	// Integer
	
	require_action( CFPrefs_GetInt64( kTestDomain, CFSTR( "key3" ), NULL ) == 0, exit, err = kResponseErr );
	require_action( CFPrefs_GetInt64( kTestDomain, CFSTR( "key3" ), &err ) == 0, exit, err = kResponseErr );
	require_action( err, exit, err = kResponseErr );
	
	err = CFPrefs_SetInt64( kTestDomain, CFSTR( "key3" ), 123 );
	require_noerr( err, exit );
	obj = CFPrefs_CopyValue( kTestDomain, CFSTR( "key3" ), NULL );
	b = CFIsType( obj, CFNumber );
	CFReleaseNullSafe( obj );
	require_action( b, exit, err = kResponseErr );
	*cstr = '\0';
	CFPrefs_GetCString( kTestDomain, CFSTR( "key3" ), cstr, sizeof( cstr ), &err );
	require_noerr( err, exit );
	require_action( strcmp( cstr, "123" ) == 0, exit, err = kResponseErr );
	
	require_action( CFPrefs_GetInt64( kTestDomain, CFSTR( "key3" ), NULL ) == 123, exit, err = kResponseErr );
	require_action( CFPrefs_GetInt64( kTestDomain, CFSTR( "key3" ), &err ) == 123, exit, err = kResponseErr );
	require_noerr( err, exit );
	
	err = CFPrefs_SetValue( kTestDomain, CFSTR( "key3" ), CFSTR( "-123" ) );
	require_noerr( err, exit );
	require_action( CFPrefs_GetInt64( kTestDomain, CFSTR( "key3" ), NULL ) == -123, exit, err = kResponseErr );
	require_action( CFPrefs_GetInt64( kTestDomain, CFSTR( "key3" ), &err ) == -123, exit, err = kResponseErr );
	require_noerr( err, exit );
	
	// Strings
	
	cstr[ 0 ] = 'a';
	cstr[ 1 ] = '\0';
	cptr = CFPrefs_GetCString( kTestDomain, CFSTR( "key4" ), cstr, sizeof( cstr ), &err );
	require_action( err, exit, err = kResponseErr );
	require_action( cptr == cstr, exit, err = kResponseErr );
	require_action( !*cptr, exit, err = kResponseErr );
	require_action( !*cstr, exit, err = kResponseErr );
	
	err = CFPrefs_SetCString( kTestDomain, CFSTR( "key4" ), "string", kSizeCString );
	require_noerr( err, exit );
	obj = CFPrefs_CopyValue( kTestDomain, CFSTR( "key4" ), NULL );
	b = CFIsType( obj, CFString );
	CFReleaseNullSafe( obj );
	require_action( b, exit, err = kResponseErr );
	
	cptr = CFPrefs_GetCString( kTestDomain, CFSTR( "key4" ), cstr, sizeof( cstr ), &err );
	require_noerr( err, exit );
	require_action( cptr == cstr, exit, err = kResponseErr );
	require_action( strcmp( cstr, "string" ) == 0, exit, err = kResponseErr );
	
	// Keys
	
	keys = CFPrefs_CopyKeys( kTestDomain, &err );
	require_action( keys, exit, err = kResponseErr );
	require_noerr( err, exit );
	n = CFArrayGetCount( keys );
	require_action( n == 5, exit, err = kResponseErr );
	b1 = b2 = b3 = b4 = b5 = 0;
	for( i = 0; i < n; ++i ) 
	{
		cfstr = (CFStringRef) CFArrayGetValueAtIndex( keys, i );
		if(      CFEqual( cfstr, CFSTR( "key1" ) ) ) b1 = true;
		else if( CFEqual( cfstr, CFSTR( "key2" ) ) ) b2 = true;
		else if( CFEqual( cfstr, CFSTR( "key3" ) ) ) b3 = true;
		else if( CFEqual( cfstr, CFSTR( "key4" ) ) ) b4 = true;
		else if( CFEqual( cfstr, CFSTR( "key5" ) ) ) b5 = true;
		err = CFPrefs_RemoveValue( kTestDomain, cfstr );
		require_noerr( err, exit );
	}
	require_action( b1, exit, err = kResponseErr );
	require_action( b2, exit, err = kResponseErr );
	require_action( b3, exit, err = kResponseErr );
	require_action( b4, exit, err = kResponseErr );
	require_action( b5, exit, err = kResponseErr );
	ForgetCF( &keys );
	
	keys = CFPrefs_CopyKeys( kTestDomain, &err );
	require_action( keys, exit, err = kResponseErr );
	require_action( CFArrayGetCount( keys ) == 0, exit, err = kResponseErr );
	ForgetCF( &keys );
	
exit:
	ForgetCF( &keys );
	printf( "CFPrefUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
