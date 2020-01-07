/*
	File:    	KeychainUtils.c
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
	
	Copyright (C) 2011-2015 Apple Inc. All Rights Reserved.
*/

#include "KeychainUtils.h"

#include "CFUtils.h"
#include "PrintFUtils.h"

//===========================================================================================================================
//	KeychainAddFormatted
//===========================================================================================================================

OSStatus	KeychainAddFormatted( CFTypeRef *outResult, const char *inAttributesFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inAttributesFormat );
	err = KeychainAddFormattedVAList( outResult, inAttributesFormat, args );
	va_end( args );
	return( err );
}

//===========================================================================================================================
//	KeychainAddFormattedVAList
//===========================================================================================================================

OSStatus	KeychainAddFormattedVAList( CFTypeRef *outResult, const char *inAttributesFormat, va_list inArgs )
{
	OSStatus					err;
	CFMutableDictionaryRef		attributesDict;
	
	err = CFPropertyListCreateFormattedVAList( NULL, &attributesDict, inAttributesFormat, inArgs );
	require_noerr( err, exit );
	
	err = SecItemAdd( attributesDict, outResult );
	CFRelease( attributesDict );
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	KeychainCopyMatchingFormatted
//===========================================================================================================================

CFTypeRef	KeychainCopyMatchingFormatted( OSStatus *outErr, const char *inFormat, ... )
{
	CFTypeRef		result;
	va_list			args;
	
	va_start( args, inFormat );
	result = KeychainCopyMatchingFormattedVAList( outErr, inFormat, args );
	va_end( args );
	return( result );
}

//===========================================================================================================================
//	KeychainCopyMatchingFormattedVAList
//===========================================================================================================================

CFTypeRef	KeychainCopyMatchingFormattedVAList( OSStatus *outErr, const char *inFormat, va_list inArgs )
{
	CFTypeRef					result;
	OSStatus					err;
	CFMutableDictionaryRef		query;
	
	result = NULL;
	
	err = CFPropertyListCreateFormattedVAList( NULL, &query, inFormat, inArgs );
	require_noerr( err, exit );
	
	err = SecItemCopyMatching( query, &result );
	CFRelease( query );
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	KeychainDeleteFormatted
//===========================================================================================================================

OSStatus	KeychainDeleteFormatted( const char *inFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = KeychainDeleteFormattedVAList( inFormat, args );
	va_end( args );
	return( err );
}

//===========================================================================================================================
//	KeychainDeleteFormattedVAList
//===========================================================================================================================

OSStatus	KeychainDeleteFormattedVAList( const char *inFormat, va_list inArgs )
{
	OSStatus					err;
	CFMutableDictionaryRef		query;
	
	err = CFPropertyListCreateFormattedVAList( NULL, &query, inFormat, inArgs );
	require_noerr( err, exit );
	
	err = SecItemDelete( query );
	CFRelease( query );
	
exit:
	return( err );
}

#if( !KEYCHAIN_LITE_ENABLED )
//===========================================================================================================================
//	KeychainDeleteItemByPersistentRef
//===========================================================================================================================

OSStatus	KeychainDeleteItemByPersistentRef( CFDataRef inRef, CFDictionaryRef inAttrs )
{
	OSStatus		err;
	
#if( TARGET_OS_MACOSX && !COMMON_SERVICES_NO_CORE_SERVICES )	
	// Work around <radar:17355300> by checking if the item is kSecAttrSynchronizable or not.
	// If it's kSecAttrSynchronizable then a use persistent ref with SecItemDelete/KeychainDeleteFormatted.
	// If it's not kSecAttrSynchronizable, work around <radar:12559935> by using SecKeychainItemDelete.
	
	if( CFDictionaryGetBoolean( inAttrs, kSecAttrSynchronizable, NULL ) )
	{
		err = KeychainDeleteFormatted( "{%kO=%O}", kSecValuePersistentRef, inRef );
		require_noerr( err, exit );
	}
	else
	{
		SecKeychainItemRef		itemRef;
		
		err = SecKeychainItemCopyFromPersistentReference( inRef, &itemRef );
		require_noerr( err, exit );
		
		err = SecKeychainItemDelete( itemRef );
		CFRelease( itemRef );
		require_noerr( err, exit );
	}
#else
	(void) inAttrs;
	
	err = KeychainDeleteFormatted( "{%kO=%O}", kSecValuePersistentRef, inRef );
	require_noerr( err, exit );
#endif

exit:
	return( err );
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	KeychainUpdateFormatted
//===========================================================================================================================

OSStatus	KeychainUpdateFormatted( CFTypeRef inRefOrQuery, const char *inAttributesToUpdateFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inAttributesToUpdateFormat );
	err = KeychainUpdateFormattedVAList( inRefOrQuery, inAttributesToUpdateFormat, args );
	va_end( args );
	return( err );
}

//===========================================================================================================================
//	KeychainUpdateFormattedVAList
//===========================================================================================================================

OSStatus	KeychainUpdateFormattedVAList( CFTypeRef inRefOrQuery, const char *inAttributesToUpdateFormat, va_list inArgs )
{
	OSStatus					err;
	CFMutableDictionaryRef		attributesToUpdateDict;
	
	err = CFPropertyListCreateFormattedVAList( NULL, &attributesToUpdateDict, inAttributesToUpdateFormat, inArgs );
	require_noerr( err, exit );
	
	err = SecItemUpdate( (CFDictionaryRef) inRefOrQuery, attributesToUpdateDict );
	CFRelease( attributesToUpdateDict );
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	KeychainUtils_Test
//===========================================================================================================================

OSStatus	KeychainUtils_Test( int inPrint )
{
	OSStatus		err;
	CFTypeRef		results;
	
	KeychainDeleteFormatted( 
		"{"
			"%kO=%O"
			"%kO=%O"
			"%kO=%O"
		"}", 
		kSecClass,			kSecClassGenericPassword, 
		kSecAttrAccount,	CFSTR( "00:11:22:aa:bb:cc" ), 
		kSecAttrService,	CFSTR( "Keychain Test Device" ) );
	
	err = KeychainAddFormatted( 
		NULL, 
		"{"
			"%kO=%O" // 1
			"%kO=%O" // 2
			"%kO=%O" // 3
			"%kO=%O" // 4
			"%kO=%D" // 5
			"%kO=%O" // 5
		"}", 
		kSecClass,				kSecClassGenericPassword,					// 1
		kSecAttrAccount,		CFSTR( "00:11:22:aa:bb:cc" ),				// 2
		kSecAttrDescription,	CFSTR( "Keychain Test Device password" ), 	// 3
		kSecAttrService,		CFSTR( "Keychain Test Device" ),			// 4
		kSecValueData,			"password", 8,								// 5
		kSecAttrLabel,			CFSTR( "Keychain Test" ) );					// 6
	require_noerr( err, exit );
	
	results = KeychainCopyMatchingFormatted( &err, 
		"{"
			"%kO=%O" // 1
			"%kO=%O" // 2
			"%kO=%O" // 3
		"}", 
		kSecClass,				kSecClassGenericPassword, 		// 1
		kSecAttrAccount,		CFSTR( "00:11:22:aa:bb:cc" ), 	// 2
		kSecReturnAttributes,	kCFBooleanTrue ); 				// 4
	require_noerr( err, exit );
	if( inPrint ) FPrintF( stderr, "%@\n", results );
	CFRelease( results );
	
	results = KeychainCopyMatchingFormatted( &err, 
		"{"
			"%kO=%O" // 1
			"%kO=%O" // 2
			"%kO=%O" // 3
			"%kO=%O" // 4
			"%kO=%O" // 5
		"}", 
		kSecClass,				kSecClassGenericPassword, 		// 1
		kSecAttrAccount,		CFSTR( "00:11:22:aa:bb:cc" ), 	// 2
		kSecReturnData,			kCFBooleanTrue, 				// 3
		kSecReturnAttributes,	kCFBooleanTrue,					// 4
		kSecReturnRef,			kCFBooleanTrue ); 				// 5
	require_noerr( err, exit );
	if( inPrint ) FPrintF( stderr, "%@\n", results );
	
	err = KeychainUpdateFormatted( results, "{%kO=%D}", kSecValueData, "test", 4 );
	CFRelease( results );
	require_noerr( err, exit );
	
	results = KeychainCopyMatchingFormatted( &err, 
		"{"
			"%kO=%O" // 1
			"%kO=%O" // 2
			"%kO=%O" // 3
			"%kO=%O" // 4
			"%kO=%O" // 5
		"}", 
		kSecClass,				kSecClassGenericPassword, 					// 1
		kSecAttrDescription,	CFSTR( "Keychain Test Device password" ), 	// 2
		kSecReturnData,			kCFBooleanTrue, 							// 3
		kSecReturnAttributes,	kCFBooleanTrue,								// 4
		kSecMatchLimitAll,		kCFBooleanTrue );							// 5
	require_noerr( err, exit );
	if( inPrint ) FPrintF( stderr, "%@\n", results );
	CFRelease( results );
	
	KeychainDeleteFormatted( 
		"{"
			"%kO=%O"
			"%kO=%O"
			"%kO=%O"
		"}", 
		kSecClass,			kSecClassGenericPassword, 
		kSecAttrAccount,	CFSTR( "00:11:22:aa:bb:cc" ), 
		kSecAttrService,	CFSTR( "Keychain Test Device" ) );
	
exit:
	printf( "KeychainUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
