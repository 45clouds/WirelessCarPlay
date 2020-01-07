/*
	File:    	EasyConfigUtils.c
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
	
	Copyright (C) 2013 Apple Inc. All Rights Reserved.
*/

#include "EasyConfigUtils.h"

#include "CommonServices.h"
#include "DataBufferUtils.h"
#include "DebugServices.h"
#include "IEEE80211Utils.h"
#include "TLVUtils.h"

#include CF_HEADER

//===========================================================================================================================
//	EasyConfigCreateDictionaryFromTLV
//===========================================================================================================================

CFDictionaryRef	EasyConfigCreateDictionaryFromTLV( const void *inTLVPtr, size_t inTLVLen, OSStatus *outErr )
{
	CFDictionaryRef				result = NULL;
	CFMutableDictionaryRef		dict   = NULL;
	OSStatus					err;
	const uint8_t *				src = (const uint8_t *) inTLVPtr;
	const uint8_t * const		end = src + inTLVLen;
	uint8_t						eid;
	const uint8_t *				ptr;
	size_t						len;
	CFStringRef					key, cfstr;
	CFMutableArrayRef			mfiProtocols = NULL;
	CFDataRef					data;
	
	dict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( dict, exit, err = kNoMemoryErr );
	
	while( TLV8GetNext( src, end, &eid, &ptr, &len, &src ) == kNoErr )
	{
		switch( eid )
		{
			case kEasyConfigTLV_AdminPassword:		key = CFSTR( kEasyConfigKey_AdminPassword );	goto anyString;
			case kEasyConfigTLV_BundleSeedID:		key = CFSTR( kEasyConfigKey_BundleSeedID );		goto anyString;
			case kEasyConfigTLV_FirmwareRevision:	key = CFSTR( kEasyConfigKey_FirmwareRevision );	goto anyString;
			case kEasyConfigTLV_HardwareRevision:	key = CFSTR( kEasyConfigKey_HardwareRevision );	goto anyString;
			case kEasyConfigTLV_Language:			key = CFSTR( kEasyConfigKey_Language );			goto anyString;
			case kEasyConfigTLV_Manufacturer:		key = CFSTR( kEasyConfigKey_Manufacturer );		goto anyString;
			case kEasyConfigTLV_Model:				key = CFSTR( kEasyConfigKey_Model );			goto anyString;
			case kEasyConfigTLV_Name:				key = CFSTR( kEasyConfigKey_Name );				goto anyString;
			case kEasyConfigTLV_PlayPassword:		key = CFSTR( kEasyConfigKey_PlayPassword );		goto anyString;
			case kEasyConfigTLV_SerialNumber:		key = CFSTR( kEasyConfigKey_SerialNumber );		goto anyString;
			case kEasyConfigTLV_WiFiSSID:			key = CFSTR( kEasyConfigKey_WiFiSSID );			goto anyString;
			anyString:
				cfstr = CFStringCreateWithBytes( NULL, ptr, (CFIndex) len, kCFStringEncodingUTF8, false );
				require_action( cfstr, exit, err = kMalformedErr );
				CFDictionarySetValue( dict, key, cfstr );
				CFRelease( cfstr );
				break;
			
			case kEasyConfigTLV_MFiProtocol:
				if( !mfiProtocols )
				{
					mfiProtocols = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
					require_action( mfiProtocols, exit, err = kNoMemoryErr );
				}
				cfstr = CFStringCreateWithBytes( NULL, ptr, (CFIndex) len, kCFStringEncodingUTF8, false );
				require_action( cfstr, exit, err = kMalformedErr );
				CFArrayAppendValue( mfiProtocols, cfstr );
				CFRelease( cfstr );
				break;
			
			case kEasyConfigTLV_WiFiPSK:
				data = CFDataCreate( NULL, ptr, (CFIndex) len );
				require_action( data, exit, err = kMalformedErr );
				CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_WiFiPSK ), data );
				CFRelease( data );
				break;
			
			default:
				lu_ulog( kLogLevelNotice, "### Ignoring unsupported EasyConfig EID 0x%02X\n", eid );
				break;
		}
	}
	if( mfiProtocols ) CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_MFiProtocols ), mfiProtocols );
	
	result = dict;
	dict = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( mfiProtocols );
	CFReleaseNullSafe( dict );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	EasyConfigCreateTLVfromDictionary
//===========================================================================================================================

typedef struct
{
	DataBuffer		dataBuf;
	uint8_t			fixedBuf[ 512 ];
	OSStatus		err;
	
}	TLVApplierContext;

static void	_EasyConfigCreateTLVfromDictionaryApplier( const void *inKey, const void *inValue, void *inContext );

uint8_t *	EasyConfigCreateTLVfromDictionary( CFDictionaryRef inDict, size_t *outLen, OSStatus *outErr )
{
	uint8_t *				tlv = NULL;
	TLVApplierContext		ctx;
	
	DataBuffer_Init( &ctx.dataBuf, ctx.fixedBuf, sizeof( ctx.fixedBuf ), 128 * kBytesPerKiloByte );
	ctx.err = kNoErr;
	CFDictionaryApplyFunction( inDict, _EasyConfigCreateTLVfromDictionaryApplier, &ctx );
	require_noerr( ctx.err, exit );
	
	ctx.err = DataBuffer_Detach( &ctx.dataBuf, &tlv, outLen );
	require_noerr( ctx.err, exit );
	
exit:
	DataBuffer_Free( &ctx.dataBuf );
	if( outErr ) *outErr = ctx.err;
	return( tlv );
}

static void	_EasyConfigCreateTLVfromDictionaryApplier( const void *inKey, const void *inValue, void *inContext )
{
	TLVApplierContext * const		ctx = (TLVApplierContext *) inContext;
	OSStatus						err;
	char							cstr[ 256 ];
	CFTypeID						typeID;
	uint8_t							eid;
	CFIndex							i, n;
	CFStringRef						cfstr;
	Boolean							good;
	
	if( ctx->err ) return;
	
	typeID = CFGetTypeID( inValue );
	if( 0 ) {}
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_AdminPassword ) ) )		eid = kEasyConfigTLV_AdminPassword;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_BundleSeedID ) ) )		eid = kEasyConfigTLV_BundleSeedID;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_FirmwareRevision ) ) )	eid = kEasyConfigTLV_FirmwareRevision;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_HardwareRevision ) ) )	eid = kEasyConfigTLV_HardwareRevision;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_Language ) ) )			eid = kEasyConfigTLV_Language;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_Manufacturer ) ) )		eid = kEasyConfigTLV_Manufacturer;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_Model ) ) )				eid = kEasyConfigTLV_Model;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_Name ) ) )				eid = kEasyConfigTLV_Name;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_PlayPassword ) ) )		eid = kEasyConfigTLV_PlayPassword;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_SerialNumber ) ) )		eid = kEasyConfigTLV_SerialNumber;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_WiFiSSID ) ) )			eid = kEasyConfigTLV_WiFiSSID;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_WiFiPSK ) ) )			eid = kEasyConfigTLV_WiFiPSK;
	else if( CFEqual( inKey, CFSTR( kEasyConfigKey_MFiProtocols ) ) )
	{
		require_action( typeID == CFArrayGetTypeID(), exit, err = kTypeErr );
		n = CFArrayGetCount( (CFArrayRef) inValue );
		for( i = 0; i < n; ++i )
		{
			cfstr = (CFStringRef) CFArrayGetValueAtIndex( (CFArrayRef) inValue, i );
			require_action( CFGetTypeID( cfstr ) == CFStringGetTypeID(), exit, err = kTypeErr );
			
			*cstr = '\0';
			good = CFStringGetCString( (CFStringRef) cfstr, cstr, (CFIndex) sizeof( cstr ), kCFStringEncodingUTF8 );
			require_action( good, exit, err = kValueErr );
			
			err = DataBuffer_AppendIE( &ctx->dataBuf, kEasyConfigTLV_MFiProtocol, cstr, kSizeCString );
			require_noerr( err, exit );
		}
		err = kNoErr;
		goto exit;
	}
	else
	{
		lu_ulog( kLogLevelNotice, "### Ignoring unsupported EasyConfig key '%@'\n", inKey );
		err = kNoErr;
		goto exit;
	}
	
	if( typeID == CFStringGetTypeID() )
	{
		*cstr = '\0';
		good = CFStringGetCString( (CFStringRef) inValue, cstr, (CFIndex) sizeof( cstr ), kCFStringEncodingUTF8 );
		require_action( good, exit, err = kValueErr );
		err = DataBuffer_AppendIE( &ctx->dataBuf, eid, cstr, kSizeCString );
		require_noerr( err, exit );
	}
	else if( typeID == CFDataGetTypeID() )
	{
		err = DataBuffer_AppendIE( &ctx->dataBuf, eid, CFDataGetBytePtr( (CFDataRef) inValue ), 
			(size_t) CFDataGetLength( (CFDataRef) inValue ) );
		require_noerr( err, exit );
	}
	else
	{
		lu_ulog( kLogLevelNotice, "### Bad EasyConfig type for key '%@': %@\n", inKey, inValue );
		err = kTypeErr;
		goto exit;
	}
	
exit:
	ctx->err = err;
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	EasyConfigUtilsTest
//===========================================================================================================================

OSStatus	EasyConfigUtilsTest( void )
{
	OSStatus					err;
	CFMutableDictionaryRef		dict;
	CFDictionaryRef				dict2;
	CFMutableArrayRef			array;
	CFDataRef					data;
	uint8_t *					tlv = NULL;
	const uint8_t *				tlvPtr;
	const uint8_t *				tlvEnd;
	size_t						tlvLen;
	size_t						len;
	const uint8_t *				ptr;
	Boolean						b;
	
	dict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( dict, exit, err = kNoMemoryErr );
	
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_AdminPassword ),		CFSTR( kEasyConfigKey_AdminPassword ) );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_BundleSeedID ),		CFSTR( kEasyConfigKey_BundleSeedID ) );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_FirmwareRevision ),	CFSTR( kEasyConfigKey_FirmwareRevision ) );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_HardwareRevision ),	CFSTR( kEasyConfigKey_HardwareRevision ) );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_Language ),			CFSTR( kEasyConfigKey_Language ) );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_Manufacturer ),		CFSTR( kEasyConfigKey_Manufacturer ) );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_Model ),				CFSTR( kEasyConfigKey_Model ) );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_Name ),				CFSTR( kEasyConfigKey_Name ) );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_PlayPassword ),		CFSTR( kEasyConfigKey_PlayPassword ) );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_SerialNumber ),		CFSTR( kEasyConfigKey_SerialNumber ) );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_WiFiSSID ),			CFSTR( kEasyConfigKey_WiFiSSID ) );
	
	array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( array, exit, err = kNoMemoryErr );
	CFArrayAppendValue( array, CFSTR( "protocol1" ) );
	CFArrayAppendValue( array, CFSTR( "protocol2" ) );
	CFArrayAppendValue( array, CFSTR( "protocol3" ) );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_MFiProtocols ), array );
	CFRelease( array );
	
	data = CFDataCreate( NULL, (const uint8_t *) "password", 8 );
	require_action( data, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, CFSTR( kEasyConfigKey_WiFiPSK ), data );
	CFRelease( data );
	
	tlv = EasyConfigCreateTLVfromDictionary( dict, &tlvLen, &err );
	require_noerr( err, exit );
	tlvPtr = tlv;
	tlvEnd = tlv + tlvLen;
	dlog( kLogLevelNotice, "%{tlv8}\n%.1H\n", kEasyConfigTLVDescriptors, tlv, (int) tlvLen, tlv, (int) tlvLen, (int) tlvLen );
	
	// Verify IE.
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_AdminPassword, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, kEasyConfigKey_AdminPassword, strlen( kEasyConfigKey_AdminPassword ) ), exit, err = -1 );
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_BundleSeedID, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, kEasyConfigKey_BundleSeedID, strlen( kEasyConfigKey_BundleSeedID ) ), exit, err = -1 );
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_FirmwareRevision, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, kEasyConfigKey_FirmwareRevision, strlen( kEasyConfigKey_FirmwareRevision ) ), exit, err = -1 );
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_Language, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, kEasyConfigKey_Language, strlen( kEasyConfigKey_Language ) ), exit, err = -1 );
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_Manufacturer, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, kEasyConfigKey_Manufacturer, strlen( kEasyConfigKey_Manufacturer ) ), exit, err = -1 );
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_Model, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, kEasyConfigKey_Model, strlen( kEasyConfigKey_Model ) ), exit, err = -1 );
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_Name, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, kEasyConfigKey_Name, strlen( kEasyConfigKey_Name ) ), exit, err = -1 );
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_PlayPassword, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, kEasyConfigKey_PlayPassword, strlen( kEasyConfigKey_PlayPassword ) ), exit, err = -1 );
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_SerialNumber, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, kEasyConfigKey_SerialNumber, strlen( kEasyConfigKey_SerialNumber ) ), exit, err = -1 );
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_WiFiSSID, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, kEasyConfigKey_WiFiSSID, strlen( kEasyConfigKey_WiFiSSID ) ), exit, err = -1 );
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_MFiProtocol, &ptr, &len, &tlvPtr );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, "protocol1", strlen( "protocol1" ) ), exit, err = -1 );
	
	err = TLV8Get( tlvPtr, tlvEnd, kEasyConfigTLV_MFiProtocol, &ptr, &len, &tlvPtr );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, "protocol2", strlen( "protocol2" ) ), exit, err = -1 );
	
	err = TLV8Get( tlvPtr, tlvEnd, kEasyConfigTLV_MFiProtocol, &ptr, &len, &tlvPtr );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, "protocol3", strlen( "protocol3" ) ), exit, err = -1 );
	
	err = TLV8Get( tlvPtr, tlvEnd, kEasyConfigTLV_MFiProtocol, &ptr, &len, &tlvPtr );
	require_action( err != kNoErr, exit, err = -1 );
	
	err = TLV8Get( tlv, tlvEnd, kEasyConfigTLV_WiFiPSK, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( MemEqual( ptr, len, "password", strlen( "password" ) ), exit, err = -1 );
	
	// Convert back and verify.
	
	dict2 = EasyConfigCreateDictionaryFromTLV( tlv, tlvLen, &err );
	require_noerr( err, exit );
	b = CFEqual( dict, dict2 );
	CFRelease( dict2 );
	require_action( b, exit, err = -1 );
	
exit:
	if( tlv ) free( tlv );
	CFReleaseNullSafe( dict );
	printf( "EasyConfigUtilsTest: %s\n", !err ? "passed" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
