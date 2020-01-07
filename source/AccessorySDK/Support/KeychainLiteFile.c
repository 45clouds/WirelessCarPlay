/*
	File-based version of the Keychain for platforms without system-provided Keychain-like functionality.
	
	File:    	KeychainLiteFile.c
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
	
	Copyright (C) 2014-2015 Apple Inc. All Rights Reserved.
*/

#include "KeychainLite.h"

#include <stdio.h>
#include <sys/stat.h>

#include "CFUtils.h"
#include "MiscUtils.h"
#include "ThreadUtils.h"

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#if( defined( KEYCHAINS_PATH ) )
	#define kKeychainParentPath		KEYCHAINS_PATH
#else
	#define kKeychainParentPath		"~/Library/Keychains"
#endif
#if( defined( KEYCHAIN_DEFAULT_FILENAME ) )
	#define kKeychainFilename		KEYCHAIN_DEFAULT_FILENAME
#else
	#define kKeychainFilename		"default.keychain"
#endif

static void		_SecItemCopyMatchingApplier( const void *inKey, const void *inValue, void *inContext );
static OSStatus	_ReadKeychain( void );
static OSStatus	_WriteKeychain( void );

static CFMutableArrayRef		gKeychainItems	= NULL;
static pthread_mutex_t			gKeychainLock	= PTHREAD_MUTEX_INITIALIZER;

//===========================================================================================================================
//	SecItemAdd_compat
//===========================================================================================================================

OSStatus	SecItemAdd_compat( CFDictionaryRef inAttrs, CFTypeRef *outResult )
{
	OSStatus			err;
	CFIndex				i, n;
	CFDictionaryRef		item;
	CFTypeRef			account, account2, service, service2, type, type2;
	
	pthread_mutex_lock( &gKeychainLock );
	
	if( !gKeychainItems )
	{
		_ReadKeychain();
	}
	
	// Search for a duplicate.
	
	account = CFDictionaryGetValue( inAttrs, kSecAttrAccount );
	require_action_quiet( account, exit, err = kParamErr );
	
	service = CFDictionaryGetValue( inAttrs, kSecAttrService );
	require_action_quiet( service, exit, err = kParamErr );
	
	type = CFDictionaryGetValue( inAttrs, kSecAttrType );
	
	n = gKeychainItems ? CFArrayGetCount( gKeychainItems ) : 0;
	for( i = 0; i < n; ++i )
	{
		item = (CFDictionaryRef) CFArrayGetValueAtIndex( gKeychainItems, i );
		account2 = CFDictionaryGetValue( item, kSecAttrAccount );
		check( account2 );
		if( !account2 ) continue;
		if( !CFEqual( account, account2 ) ) continue;
		
		service2 = CFDictionaryGetValue( item, kSecAttrService );
		check( service2 );
		if( !service2 ) continue;
		if( !CFEqual( service, service2 ) ) continue;
		
		if( type )
		{
			type2 = CFDictionaryGetValue( item, kSecAttrType );
			check( type2 );
			if( !type2 ) continue;
			if( !CFEqual( type, type2 ) ) continue;
		}
		
		err = errSecDuplicateItem;
		goto exit;
	}
	
	// Add the item.
	
	if( !gKeychainItems )
	{
		gKeychainItems = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( gKeychainItems, exit, err = kNoMemoryErr );
	}
	CFArrayAppendValue( gKeychainItems, inAttrs );
	
	err = _WriteKeychain();
	require_noerr( err, exit );
	
	if( outResult )
	{
		CFRetain( inAttrs );
		*outResult = inAttrs;
	}
	
exit:
	pthread_mutex_unlock( &gKeychainLock );
	return( err );
}

//===========================================================================================================================
//	SecItemCopyMatching_compat
//===========================================================================================================================

typedef struct
{
	CFDictionaryRef		dict;
	Boolean				found;
	
}	SecItemCopyMatchingApplierContext;

OSStatus	SecItemCopyMatching_compat( CFDictionaryRef inQuery, CFTypeRef *outResult )
{
	OSStatus								err;
	CFIndex									i, n;
	CFDictionaryRef							item;
	CFStringRef								cfstr;
	SecItemCopyMatchingApplierContext		ctx;
	CFMutableArrayRef						results = NULL;
	Boolean									matchAll, returnAttrs, returnData, returnPersistentRef, returnRef;
	CFTypeRef								obj;
	
	pthread_mutex_lock( &gKeychainLock );
	
	if( !gKeychainItems )
	{
		_ReadKeychain();
	}
	
	cfstr = CFDictionaryGetCFString( inQuery, kSecMatchLimit, &err );
	matchAll = ( cfstr && CFEqual( cfstr, kSecMatchLimitAll ) );
	if( !matchAll && CFDictionaryGetBoolean( inQuery, kSecMatchLimitAll, NULL ) )
	{
		matchAll = true;
	}
	
	n = gKeychainItems ? CFArrayGetCount( gKeychainItems ) : 0;
	for( i = 0; i < n; ++i )
	{
		item = (CFDictionaryRef) CFArrayGetValueAtIndex( gKeychainItems, i );
		ctx.dict  = item;
		ctx.found = true;
		CFDictionaryApplyFunction( inQuery, _SecItemCopyMatchingApplier, &ctx );
		if( !ctx.found ) continue;
		if( !matchAll )
		{
			returnAttrs			= CFDictionaryGetBoolean( inQuery, kSecReturnAttributes, NULL );
			returnData			= CFDictionaryGetBoolean( inQuery, kSecReturnData, NULL );
			returnPersistentRef	= CFDictionaryGetBoolean( inQuery, kSecReturnPersistentRef, NULL );
			returnRef			= CFDictionaryGetBoolean( inQuery, kSecReturnRef, NULL );
			
			if( returnData && !returnAttrs && !returnPersistentRef && !returnRef )
			{
				obj = CFDictionaryGetCFData( item, kSecValueData, NULL );
				require_action_quiet( obj, exit, err = kNotFoundErr );
			}
			else if( returnPersistentRef && !returnAttrs && !returnData && !returnRef )
			{
				obj = CFDictionaryGetValue( item, kSecReturnPersistentRef );
				require_action_quiet( obj, exit, err = kNotFoundErr );
			}
			else if( returnRef && !returnAttrs && !returnData && !returnPersistentRef )
			{
				obj = CFDictionaryGetValue( item, kSecReturnRef );
				require_action_quiet( obj, exit, err = kNotFoundErr );
			}
			else
			{
				obj = item;
			}
			
			CFRetain( obj );
			*outResult = obj;
			err = kNoErr;
			goto exit;
		}
		
		err = CFArrayEnsureCreatedAndAppend( &results, item );
		require_noerr( err, exit );
	}
	require_action_quiet( results, exit, err = errSecItemNotFound );
	
	*outResult = results;
	results = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( results );
	pthread_mutex_unlock( &gKeychainLock );
	return( err );
}

static void	_SecItemCopyMatchingApplier( const void *inKey, const void *inValue, void *inContext )
{
	SecItemCopyMatchingApplierContext * const		ctx = (SecItemCopyMatchingApplierContext *) inContext;
	CFTypeRef										value;
	
	if( !ctx->found )								return;
	if( CFEqual( inKey, kSecAttrSynchronizable ) )	return;
	if( CFEqual( inKey, kSecMatchLimit ) )			return;
	if( CFEqual( inKey, kSecMatchLimitAll ) )		return;
	if( CFEqual( inKey, kSecReturnAttributes ) )	return;
	if( CFEqual( inKey, kSecReturnData ) )			return;
	if( CFEqual( inKey, kSecReturnPersistentRef ) )	return;
	if( CFEqual( inKey, kSecReturnRef ) )			return;
	
	value = CFDictionaryGetValue( ctx->dict, inKey );
	if( !value || !CFEqual( value, inValue ) )
	{
		ctx->found = false;
	}
}

//===========================================================================================================================
//	SecItemDelete_compat
//===========================================================================================================================

OSStatus	SecItemDelete_compat( CFDictionaryRef inQuery )
{
	OSStatus			err;
	CFIndex				i, n;
	CFDictionaryRef		item;
	CFTypeRef			account, account2, service, service2, type, type2;
	CFStringRef			cfstr;
	Boolean				matchAll;
	Boolean				found = false;
	
	pthread_mutex_lock( &gKeychainLock );
	
	if( !gKeychainItems )
	{
		_ReadKeychain();
	}
	
	account	= CFDictionaryGetValue( inQuery, kSecAttrAccount );
	service	= CFDictionaryGetValue( inQuery, kSecAttrService );
	type	= CFDictionaryGetValue( inQuery, kSecAttrType );
	
	cfstr = CFDictionaryGetCFString( inQuery, kSecMatchLimit, &err );
	matchAll = ( cfstr && CFEqual( cfstr, kSecMatchLimitAll ) );
	if( !matchAll && CFDictionaryGetBoolean( inQuery, kSecMatchLimitAll, NULL ) )
	{
		matchAll = true;
	}
	
	n = gKeychainItems ? CFArrayGetCount( gKeychainItems ) : 0;
	for( i = 0; i < n; ++i )
	{
		item = (CFDictionaryRef) CFArrayGetValueAtIndex( gKeychainItems, i );
		
		if( account )
		{
			account2 = CFDictionaryGetValue( item, kSecAttrAccount );
			if( !account2 ) continue;
			if( !CFEqual( account, account2 ) ) continue;
		}
		if( service )
		{
			service2 = CFDictionaryGetValue( item, kSecAttrService );
			if( !service2 ) continue;
			if( !CFEqual( service, service2 ) ) continue;
		}
		if( type )
		{
			type2 = CFDictionaryGetValue( item, kSecAttrType );
			if( !type2 ) continue;
			if( !CFEqual( type, type2 ) ) continue;
		}
		
		CFArrayRemoveValueAtIndex( gKeychainItems, i );
		--i;
		--n;
		found = true;
		if( !matchAll ) break;
	}
	require_action_quiet( found, exit, err = errSecItemNotFound );
	
	err = _WriteKeychain();
	require_noerr( err, exit );
	
exit:
	pthread_mutex_unlock( &gKeychainLock );
	return( err );
}

//===========================================================================================================================
//	SecItemUpdate_compat
//===========================================================================================================================

OSStatus	SecItemUpdate_compat( CFDictionaryRef inQuery, CFDictionaryRef inAttrs )
{
	OSStatus			err;
	CFIndex				i, n;
	CFDictionaryRef		item;
	CFTypeRef			account, account2, service, service2;
	Boolean				found = false;
	
	pthread_mutex_lock( &gKeychainLock );
	
	if( !gKeychainItems )
	{
		_ReadKeychain();
	}
	
	account = CFDictionaryGetValue( inQuery, kSecAttrAccount );
	service = CFDictionaryGetValue( inQuery, kSecAttrService );
	
	n = gKeychainItems ? CFArrayGetCount( gKeychainItems ) : 0;
	for( i = 0; i < n; ++i )
	{
		item = (CFDictionaryRef) CFArrayGetValueAtIndex( gKeychainItems, i );
		
		if( account )
		{
			account2 = CFDictionaryGetValue( item, kSecAttrAccount );
			if( !account2 ) continue;
			if( !CFEqual( account, account2 ) ) continue;
		}
		if( service )
		{
			service2 = CFDictionaryGetValue( item, kSecAttrService );
			if( !service2 ) continue;
			if( !CFEqual( service, service2 ) ) continue;
		}
		
		CFDictionaryMergeDictionary( item, inAttrs );
		found = true;
	}
	require_action_quiet( found, exit, err = errSecItemNotFound );
	
	err = _WriteKeychain();
	require_noerr( err, exit );
	
exit:
	pthread_mutex_unlock( &gKeychainLock );
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_ReadKeychain
//
//	gKeychainLock must be held.
//===========================================================================================================================

static OSStatus	_ReadKeychain( void )
{
	OSStatus				err;
	char					path[ PATH_MAX ];
	size_t					len, n, reserved, used;
	FILE *					file = NULL;
	CFMutableDataRef		data = NULL;
	CFMutableArrayRef		items = NULL;
	uint8_t *				ptr;
	
	NormalizePath( kKeychainParentPath, kSizeCString, path, sizeof( path ), 0 );
	mkpath( path, S_IRWXU, S_IRWXU );
	len = strlen( path );
	if( ( len > 0 ) && ( path[ len - 1 ] != '/' ) ) path[ len++ ] = '/';
	snprintf( &path[ len ], sizeof( path ) - len, "%s", kKeychainFilename );
	
	file = fopen( path, "rb" );
	err = map_global_value_errno( file, file );
	require_noerr_quiet( err, exit );
	
	data = CFDataCreateMutable( NULL, 0 );
	require_action( data, exit, err = kNoMemoryErr );
	
	reserved = 0;
	used = 0;
	for( ;; )
	{
		if( used == reserved )
		{
			reserved += 16384;
			CFDataSetLength( data, reserved );
		}
		ptr = CFDataGetMutableBytePtr( data );
		n = fread( ptr + used, 1, reserved - used, file );
		if( n == 0 ) break;
		used += n;
	}
	CFDataSetLength( data, used );
	
	items = (CFMutableArrayRef) CFPropertyListCreateWithData( NULL, data, kCFPropertyListMutableContainers, NULL, NULL );
	require_action( items, exit, err = kUnknownErr );
	require_action( CFIsType( items, CFArray ), exit, err = kTypeErr );
	
	ForgetCF( &gKeychainItems );
	gKeychainItems = items;
	items = NULL;
	
exit:
	CFReleaseNullSafe( items );
	CFReleaseNullSafe( data );
	if( file ) fclose( file );
	return( err );
}

//===========================================================================================================================
//	_WriteKeychain
//
//	gKeychainLock must be held.
//===========================================================================================================================

static OSStatus	_WriteKeychain( void )
{
	OSStatus		err;
	CFDataRef		data = NULL;
	char			path[ PATH_MAX ];
	size_t			len, n;
	FILE *			file = NULL;
	
	require_action( gKeychainItems, exit, err = kNotPreparedErr );
	data = CFPropertyListCreateData( NULL, gKeychainItems, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
	require_action( data, exit, err = kUnknownErr );
	
	NormalizePath( kKeychainParentPath, kSizeCString, path, sizeof( path ), 0 );
	mkpath( path, S_IRWXU, S_IRWXU );
	len = strlen( path );
	if( ( len > 0 ) && ( path[ len - 1 ] != '/' ) ) path[ len++ ] = '/';
	snprintf( &path[ len ], sizeof( path ) - len, "%s", kKeychainFilename );
	
	file = fopen( path, "wb" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	
	len = (size_t) CFDataGetLength( data );
	n = fwrite( CFDataGetBytePtr( data ), 1, len, file );
	err = map_global_value_errno( n == len, file );
	require_noerr( err, exit );
	
exit:
	if( file ) fclose( file );
	CFReleaseNullSafe( data );
	return( err );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	KeychainLiteFileTest
//===========================================================================================================================

OSStatus	KeychainLiteFileTest( void );
OSStatus	KeychainLiteFileTest( void )
{
	OSStatus					err;
	char						path[ PATH_MAX ];
	CFMutableDictionaryRef		mitem, query = NULL;
	CFTypeRef					item = NULL, obj;
	CFDictionaryRef				dict;
	
	NormalizePath( kKeychainParentPath "/" kKeychainFilename, kSizeCString, path, sizeof( path ), 0 );
	remove( path );
	ForgetCF( &gKeychainItems );
	
	mitem = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( mitem, exit, err = kNoMemoryErr );
	CFDictionarySetValue( mitem, kSecAttrAccount, CFSTR( "account1" ) );
	CFDictionarySetValue( mitem, kSecAttrService, CFSTR( "service1" ) );
	CFDictionarySetValue( mitem, kSecAttrLabel, CFSTR( "label1" ) );
	err = SecItemAdd_compat( mitem, NULL );
	require_noerr( err, exit );
	
	err = SecItemDelete_compat( mitem );
	require_noerr( err, exit );
	require_action( gKeychainItems && ( CFArrayGetCount( gKeychainItems ) == 0 ), exit, err = -1 );
	
	err = SecItemDelete_compat( mitem );
	require_action( err != kNoErr, exit, err = -1 );
	
	err = SecItemAdd_compat( mitem, NULL );
	require_noerr( err, exit );
	
	item = NULL;
	query = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( query, exit, err = kNoMemoryErr );
	CFDictionarySetValue( query, kSecAttrAccount, CFSTR( "account1" ) );
	CFDictionarySetValue( query, kSecAttrService, CFSTR( "service1" ) );
	err = SecItemCopyMatching( query, &item );
	require_noerr( err, exit );
	require_action( item, exit, err = -1 );
	require_action( CFIsType( item, CFDictionary ), exit, err = -1 );
	dict = (CFDictionaryRef) item;
	obj = CFDictionaryGetValue( dict, kSecAttrAccount );
	require_action( CFEqualNullSafe( obj, CFSTR( "account1" ) ), exit, err = -1 );
	obj = CFDictionaryGetValue( dict, kSecAttrService );
	require_action( CFEqualNullSafe( obj, CFSTR( "service1" ) ), exit, err = -1 );
	ForgetCF( &query );
	ForgetCF( &item );
	
	item = NULL;
	query = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( query, exit, err = kNoMemoryErr );
	CFDictionarySetValue( query, kSecAttrAccount, CFSTR( "account1" ) );
	CFDictionarySetValue( query, kSecAttrService, CFSTR( "service1" ) );
	CFDictionarySetValue( query, kSecMatchLimit, kSecMatchLimitAll );
	err = SecItemCopyMatching( query, &item );
	require_noerr( err, exit );
	require_action( item, exit, err = -1 );
	require_action( CFIsType( item, CFArray ), exit, err = -1 );
	require_action( CFArrayGetCount( (CFArrayRef) item ) > 0, exit, err = -1 );
	dict = CFArrayGetCFDictionaryAtIndex( (CFArrayRef) item, 0, &err );
	require_noerr( err, exit );
	obj = CFDictionaryGetValue( dict, kSecAttrAccount );
	require_action( CFEqualNullSafe( obj, CFSTR( "account1" ) ), exit, err = -1 );
	obj = CFDictionaryGetValue( dict, kSecAttrService );
	require_action( CFEqualNullSafe( obj, CFSTR( "service1" ) ), exit, err = -1 );
	ForgetCF( &query );
	ForgetCF( &item );
	
exit:
	CFReleaseNullSafe( mitem );
	CFReleaseNullSafe( query );
	CFReleaseNullSafe( item );
	printf( "KeychainLiteFileTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif
