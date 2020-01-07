/*
	File:    	HIDUtils.c
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
	
	Copyright (C) 2012-2015 Apple Inc. All Rights Reserved.
*/

#include "HIDUtils.h"

#include "CFUtils.h"
#include "CommonServices.h"
#include "MathUtils.h"
#include "ThreadUtils.h"

#if( TARGET_OS_DARWIN )
	#include <IOKit/hid/IOHIDUsageTables.h>
#else
#endif

static pthread_mutex_t				gHIDOverrideLock	= PTHREAD_MUTEX_INITIALIZER;
static CFMutableDictionaryRef		gHIDOverrides		= NULL;


#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	HIDRegisterOverrideDescriptor
//===========================================================================================================================

OSStatus	HIDRegisterOverrideDescriptor( const HIDInfo *inInfo, const void *inPtr, size_t inLen )
{
	OSStatus		err;
	uintptr_t		key;
	CFDataRef		value;
	
	pthread_mutex_lock( &gHIDOverrideLock );
	
	if( !gHIDOverrides )
	{
		gHIDOverrides = CFDictionaryCreateMutable( NULL, 0, NULL, &kCFTypeDictionaryValueCallBacks );
		require_action( gHIDOverrides, exit, err = kNoMemoryErr );
	}
	
	key = (uintptr_t)( ( inInfo->vendorID << 16 ) | inInfo->productID );
	value = CFDataCreate( NULL, (const uint8_t *) inPtr, (CFIndex) inLen );
	require_action( value, exit, err = kNoMemoryErr );
	CFDictionarySetValue( gHIDOverrides, (const void *) key, value );
	CFRelease( value );
	err = kNoErr;
	
exit:
	pthread_mutex_unlock( &gHIDOverrideLock );
	return( err );
}

//===========================================================================================================================
//	HIDDeregisterOverrideDescriptor
//===========================================================================================================================

OSStatus	HIDDeregisterOverrideDescriptor( const HIDInfo *inInfo )
{
	uintptr_t		key;
	
	pthread_mutex_lock( &gHIDOverrideLock );
	if( gHIDOverrides )
	{
		key = (uintptr_t)( ( inInfo->vendorID << 16 ) | inInfo->productID );
		CFDictionaryRemoveValue( gHIDOverrides, (const void *) key );
	}
	pthread_mutex_unlock( &gHIDOverrideLock );
	return( kNoErr );
}

//===========================================================================================================================
//	HIDCopyOverrideDescriptor
//===========================================================================================================================

OSStatus	HIDCopyOverrideDescriptor( const HIDInfo *inInfo, uint8_t **outPtr, size_t *outLen )
{
	OSStatus		err;
	uintptr_t		key;
	uint8_t *		ptr = NULL;
	size_t			len = 0;
	
	pthread_mutex_lock( &gHIDOverrideLock );
	
	require_action_quiet( gHIDOverrides, exit, err = kNotFoundErr );
	key = (uintptr_t)( ( inInfo->vendorID << 16 ) | inInfo->productID );
	ptr = CFDictionaryCopyData( gHIDOverrides, (const void *) key, &len, &err );
	require_noerr_quiet( err, exit );
	
exit:
	pthread_mutex_unlock( &gHIDOverrideLock );
	*outPtr = ptr;
	*outLen = len;
	return( err );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	HIDUtilsTest
//===========================================================================================================================

OSStatus	HIDUtilsTest( void )
{
	OSStatus		err;
	HIDInfo			hidInfo;
	uint8_t *		ptr = NULL;
	size_t			len;
	
	hidInfo.vendorID  = 0x1234;
	hidInfo.productID = 0x5678;
	err = HIDCopyOverrideDescriptor( &hidInfo, &ptr, &len );
	require_action( err != kNoErr, exit, err = -1 );
	
	len = 0;
	err = HIDRegisterOverrideDescriptor( &hidInfo, "\x01\x02\0x03\x04\x05", 5 );
	require_noerr( err, exit );
	hidInfo.vendorID  = 0x1234;
	hidInfo.productID = 0x5678;
	err = HIDCopyOverrideDescriptor( &hidInfo, &ptr, &len );
	require_noerr( err, exit );
	require_action( ptr && MemEqual( ptr, len, "\x01\x02\0x03\x04\x05", 5 ), exit, err = -1 );
	ForgetMem( &ptr );
	
	hidInfo.vendorID  = 0xAABB;
	hidInfo.productID = 0xCCDD;
	err = HIDRegisterOverrideDescriptor( &hidInfo, "\x0A\x0B\0x0C\x0D", 4 );
	require_noerr( err, exit );
	len = 0;
	err = HIDCopyOverrideDescriptor( &hidInfo, &ptr, &len );
	require_noerr( err, exit );
	require_action( ptr && MemEqual( ptr, len, "\x0A\x0B\0x0C\x0D", 4 ), exit, err = -1 );
	ForgetMem( &ptr );
	hidInfo.vendorID  = 0x1234;
	hidInfo.productID = 0x5678;
	len = 0;
	err = HIDCopyOverrideDescriptor( &hidInfo, &ptr, &len );
	require_noerr( err, exit );
	require_action( ptr && MemEqual( ptr, len, "\x01\x02\0x03\x04\x05", 5 ), exit, err = -1 );
	ForgetMem( &ptr );
	
	hidInfo.vendorID  = 0x1234;
	hidInfo.productID = 0x5678;
	err = HIDDeregisterOverrideDescriptor( &hidInfo );
	require_noerr( err, exit );
	hidInfo.vendorID  = 0xAABB;
	hidInfo.productID = 0xCCDD;
	err = HIDDeregisterOverrideDescriptor( &hidInfo );
	require_noerr( err, exit );
	require_action( !gHIDOverrides || ( CFDictionaryGetCount( gHIDOverrides ) == 0 ), exit, err = -1 );
	
exit:
	FreeNullSafe( ptr );
	printf( "HIDUtilsTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
