/*
	File:    	CFPrefUtils.h
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
	
	Copyright (C) 2010-2015 Apple Inc. All Rights Reserved.
*/
/*!
	@header			CFPrefs API
	@discussion		APIs for getting and setting CFPreferences with type-specific convenience functions.
*/

#ifndef	__CFPrefUtils_h__
#define	__CFPrefUtils_h__

#include "CommonServices.h"

#include CF_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#define NSPrefsCurrentApp		( (__bridge NSString *) kCFPreferencesCurrentApplication )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefs_CopyKeys
	@abstract	Creates an array all keys currently set.
*/
CF_RETURNS_RETAINED
CFArrayRef	CFPrefs_CopyKeys( CFStringRef inAppID, OSStatus *outErr );
#define		NSPrefs_GetKeys( APP_ID, OUT_ERR ) \
			( (NSArray *) CFBridgingRelease( CFPrefs_CopyKeys( (__bridge CFStringRef)(APP_ID), (OUT_ERR) ) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefs_CopyTypedValue
	@abstract	Copies of a value from a CF pref of a specified type.
*/
CF_RETURNS_RETAINED
CFTypeRef	CFPrefs_CopyTypedValue( CFStringRef inAppID, CFStringRef inKey, CFTypeID inType, OSStatus *outErr );
#define		CFPrefs_CopyValue( APP_ID, KEY, OUT_ERR )	CFPrefs_CopyTypedValue( (APP_ID), (KEY), 0, (OUT_ERR) )
#define		NSPrefs_GetValue( APP_ID, KEY, OUT_ERR ) \
			CFBridgingRelease( CFPrefs_CopyValue( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (OUT_ERR) ) )

// Type-specific macros for getting a CF object of the correct type or NULL.

#define		CFPrefs_CopyCFArray(      APP_ID, KEY, OUT_ERR )	( (CFArrayRef)      CFPrefs_CopyTypedValue( (APP_ID), (KEY), CFArrayGetTypeID(),      OUT_ERR ) )
#define		CFPrefs_CopyCFBoolean(    APP_ID, KEY, OUT_ERR )	( (CFBooleanRef)    CFPrefs_CopyTypedValue( (APP_ID), (KEY), CFBooleanGetTypeID(),    OUT_ERR ) )
#define		CFPrefs_CopyCFData(       APP_ID, KEY, OUT_ERR )	( (CFDataRef)       CFPrefs_CopyTypedValue( (APP_ID), (KEY), CFDataGetTypeID(),       OUT_ERR ) )
#define		CFPrefs_CopyCFDate(       APP_ID, KEY, OUT_ERR )	( (CFDateRef)       CFPrefs_CopyTypedValue( (APP_ID), (KEY), CFDateGetTypeID(),       OUT_ERR ) )
#define		CFPrefs_CopyCFDictionary( APP_ID, KEY, OUT_ERR )	( (CFDictionaryRef) CFPrefs_CopyTypedValue( (APP_ID), (KEY), CFDictionaryGetTypeID(), OUT_ERR ) )
#define		CFPrefs_CopyCFNumber(     APP_ID, KEY, OUT_ERR )	( (CFNumberRef)     CFPrefs_CopyTypedValue( (APP_ID), (KEY), CFNumberGetTypeID(),     OUT_ERR ) )
#define		CFPrefs_CopyCFString(     APP_ID, KEY, OUT_ERR )	( (CFStringRef)     CFPrefs_CopyTypedValue( (APP_ID), (KEY), CFStringGetTypeID(),     OUT_ERR ) )

#define		NSPrefs_GetNSArray( APP_ID, KEY, OUT_ERR ) \
			( (NSArray *) CFBridgingRelease( CFPrefs_CopyCFArray( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (OUT_ERR) ) ) )
#define		NSPrefs_GetNSBoolean( APP_ID, KEY, OUT_ERR ) \
			( (NSNumber *) CFBridgingRelease( CFPrefs_CopyCFBoolean( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (OUT_ERR) ) ) )
#define		NSPrefs_GetNSData( APP_ID, KEY, OUT_ERR ) \
			( (NSData *) CFBridgingRelease( CFPrefs_CopyCFData( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (OUT_ERR) ) ) )
#define		NSPrefs_GetNSDate( APP_ID, KEY, OUT_ERR ) \
			( (NSDate *) CFBridgingRelease( CFPrefs_CopyCFDate( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (OUT_ERR) ) ) )
#define		NSPrefs_GetNSDictionary( APP_ID, KEY, OUT_ERR ) \
			( (NSDictionary *) CFBridgingRelease( CFPrefs_CopyCFDictionary( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (OUT_ERR) ) ) )
#define		NSPrefs_GetNSNumber( APP_ID, KEY, OUT_ERR ) \
			( (NSNumber *) CFBridgingRelease( CFPrefs_CopyCFNumber( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (OUT_ERR) ) ) )
#define		NSPrefs_GetNSString( APP_ID, KEY, OUT_ERR ) \
			( (NSString *) CFBridgingRelease( CFPrefs_CopyCFString( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (OUT_ERR) ) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefs_SetValue
	@abstract	Sets a pref by value object.
*/
OSStatus	CFPrefs_SetValue( CFStringRef inAppID, CFStringRef inKey, CFTypeRef inValue );
#define		NSPrefs_SetValue( APP_ID, KEY, VALUE ) \
			CFPrefs_SetValue( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (__bridge CFTypeRef)(VALUE) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefs_RemoveValue
	@abstract	Removes the specified pref.
*/
OSStatus	CFPrefs_RemoveValue( CFStringRef inAppID, CFStringRef inKey );
#define		NSPrefs_RemoveValue( APP_ID, KEY ) \
			CFPrefs_RemoveValue( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefs_GetBoolean
	@abstract	Gets the pref as a boolean.
	@discussion	See conversion details from CFGetBoolean.
*/
#define CFPrefs_GetBoolean( APP_ID, KEY, OUT_ERR )	( ( CFPrefs_GetInt64( (APP_ID), (KEY), (OUT_ERR) ) != 0 ) ? true : false )
#define NSPrefs_GetBoolean( APP_ID, KEY, OUT_ERR ) \
		CFPrefs_GetBoolean( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (OUT_ERR) )
#define CFPrefs_SetBoolean( APP_ID, KEY, X )		CFPrefs_SetValue( (APP_ID), (KEY), (X) ? kCFBooleanTrue : kCFBooleanFalse )
#define NSPrefs_SetBoolean( APP_ID, KEY, X ) \
		CFPrefs_SetBoolean( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (X) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefs_GetCString
	@abstract	Gets a C string from a pref.
	@discussion	See conversion details from CFGetCString.
*/
char *	CFPrefs_GetCString( CFStringRef inAppID, CFStringRef inKey, char *inBuf, size_t inMaxLen, OSStatus *outErr );
#define	NSPrefs_GetCString( APP_ID, KEY, BUF, MAX_LEN, OUT_ERR ) \
		CFPrefs_GetCString( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (BUF), (MAX_LEN), (OUT_ERR) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefs_SetCString
	@abstract	Sets a string pref using a C-style string. May be non-NUL-terminted if inLen is kSizeCString.
*/
OSStatus	CFPrefs_SetCString( CFStringRef inAppID, CFStringRef inKey, const char *inStr, size_t inLen );
#define		NSPrefs_SetCString( APP_ID, KEY, STR, LEN ) \
			CFPrefs_SetCString( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (STR), (LEN) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefs_GetData / CFPrefs_SetData
	@abstract	Gets a binary data representation from a string pref.
	@discussion	See conversion details from CFGetData.
*/
uint8_t *	CFPrefs_GetData( CFStringRef inAppID, CFStringRef inKey, void *inBuf, size_t inMaxLen, size_t *outLen, OSStatus *outErr );
#define		NSPrefs_GetData( APP_ID, KEY, BUF, MAX_LEN, OUT_LEN, OUT_ERR ) \
			CFPrefs_GetData( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (BUF), (MAX_LEN), (OUT_LEN), (OUT_ERR) )

#define		CFPrefs_SetData( APP_ID, KEY, PTR, LEN )	CFObjectSetBytes( (APP_ID), _CFPrefs_SetValue, (KEY), (PTR), (LEN) )
#define		NSPrefs_SetData( APP_ID, KEY, PTR, LEN ) \
			CFPrefs_SetData( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (PTR), (LEN) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefs_GetDouble / CFPrefs_SetDouble
	@abstract	Gets a double from a CF pref, converting as necessary.
	@discussion	See conversion details from CFGetDouble.
*/
double		CFPrefs_GetDouble( CFStringRef inAppID, CFStringRef inKey, OSStatus *outErr );
#define		NSPrefs_GetDouble( APP_ID, KEY, OUT_ERR ) \
			CFPrefs_GetDouble( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (OUT_ERR) )

OSStatus	CFPrefs_SetDouble( CFStringRef inAppID, CFStringRef inKey, double inValue );
#define		NSPrefs_SetDouble( APP_ID, KEY, VALUE ) \
			CFPrefs_SetDouble( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (VALUE) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefs_GetInt64 / CFPrefs_SetInt64
	@abstract	Gets an int64_t from a CF pref, converting as necessary.
	@discussion	See conversion details from CFGetInt64.
*/
int64_t		CFPrefs_GetInt64( CFStringRef inAppID, CFStringRef inKey, OSStatus *outErr );
#define		NSPrefs_GetInt64( APP_ID, KEY, OUT_ERR ) \
			CFPrefs_GetInt64( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (OUT_ERR) )

OSStatus	CFPrefs_SetInt64( CFStringRef inAppID, CFStringRef inKey, int64_t inValue );
#define		NSPrefs_SetInt64( APP_ID, KEY, VALUE ) \
			CFPrefs_SetInt64( (__bridge CFStringRef)(APP_ID), (__bridge CFStringRef)(KEY), (VALUE) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefs_Synchronize
	@abstract	Makes sure in-memory preferences are written to persistent storage and vice-versa.
*/
#define CFPrefs_Synchronize( APP_ID )	CFPreferencesAppSynchronize( (APP_ID) );
#define NSPrefs_Synchronize( APP_ID )	CFPrefs_Synchronize( (__bridge CFStringRef)(APP_ID) );

// Internals

CFTypeRef	_CFPrefs_CopyValue( CFTypeRef inAppID, CFStringRef inKey, OSStatus *outErr );
OSStatus	_CFPrefs_SetValue( CFTypeRef inAppID, CFStringRef inKey, CFTypeRef inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPrefUtils_Test
	@abstract	Unit test.
*/
OSStatus	CFPrefUtils_Test( void );

#ifdef __cplusplus
}
#endif

#endif // __CFPrefUtils_h__
