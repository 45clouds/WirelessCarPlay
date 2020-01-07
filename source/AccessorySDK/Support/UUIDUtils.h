/*
	File:    	UUIDUtils.h
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
	
	Copyright (C) 2001-2013 Apple Inc. All Rights Reserved.
*/

#ifndef	__UUIDUtils_h__
#define	__UUIDUtils_h__

#include "CommonServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
//	Constants and Types
//===========================================================================================================================

#define kUUIDSize					16

#define kNullUUID					( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" )
#define kNullUUIDStr				"00000000-0000-0000-0000-000000000000"

#define kBluetoothBaseUUID			( (const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB" )
#define kBluetoothBaseUUIDStr		"00000000-0000-1000-8000-00805f9b34fb"

typedef struct
{
	uint8_t		bytes[ kUUIDSize ];
	
}	UUIDData;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	UUIDGet
	@abstract	Generates a 16 byte/128 bit, big-endian UUID. Input buffer must be at least 16 bytes.
*/
void	UUIDGet( void *outUUID );

//---------------------------------------------------------------------------------------------------------------------------
/*!	group		UUIDMacros
	@abstract	Macros to test and manipulate UUIDs.
*/
#define UUIDClear( X )			memset( ( X ), 0, kUUIDSize );
#define UUIDCompare( X, Y )		memcmp( ( X ), ( Y ), kUUIDSize )
#define UUIDCopy( DST, SRC )	memcpy( ( DST ), ( SRC ), kUUIDSize )
#define UUIDIsNull( X )			( UUIDCompare( X, kNullUUID ) == 0 )

#define UUIDGetVersion( U )			( ( (const uint8_t *)( U ) )[ 6 ] >> 4 )
#define UUIDSetVersion( U, V )		( (uint8_t *)(U) )[ 6 ] = (uint8_t)( ( ( (const uint8_t *)(U) )[ 6 ] & 0x0F ) | ( (V) << 4 ) )

#define kUUIDVersion_Invalid				0
#define kUUIDVersion_1_TimeBased			1
#define kUUIDVersion_2_DCE_POSIX_UID		2
#define kUUIDVersion_3_MD5					3
#define kUUIDVersion_4_Random				4
#define kUUIDVersion_5_SHA1					5

#define UUIDIsRFC4122( U )		( ( ( (const uint8_t *)(U) )[ 8 ] & 0xC0 ) == 0x80 )
#define UUIDMakeRFC4122( U )	( (uint8_t *)(U) )[ 8 ] = (uint8_t)( ( ( (const uint8_t *)(U) )[ 8 ] & 0x3F ) | 0x80 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	group		UUID_SHA1
	@abstract	Makes SHA-1, name-based UUID's (Version 5 in the UUID spec).
	@discussion
	
	Example of making a UUID out of a person's name:
	
	#define kMyUUIDNameSpace		"\x1f\x98\x13\x30\x59\x24\x4b\x88\xa7\xc3\x39\x20\xd5\x2e\xa3\x30"
	
	UUID_SHA1_Context		context;
	UUIDData				uuid;
	
	UUID_SHA1_Init( &context, kMyUUIDNameSpace );
	UUID_SHA1_Update( &context, "Ace", 3 );
	UUID_SHA1_Final( &context, &uuid );
*/
#define UUID_SHA1_Context		SHA_CTX

#define UUID_SHA1_Init( CONTEXT_PTR, NAMESPACE_UUID_PTR ) \
	do \
	{ \
		SHA1_Init( (CONTEXT_PTR) ); \
		SHA1_Update( (CONTEXT_PTR), (NAMESPACE_UUID_PTR), kUUIDSize ); \
		\
	}	while( 0 )

#define UUID_SHA1_Update( CONTEXT_PTR, PTR, LEN )	SHA1_Update( (CONTEXT_PTR), (PTR), (LEN) );

#define UUID_SHA1_Final( CONTEXT_PTR, OUT_UUID ) \
	do \
	{ \
		uint8_t		UUID_SHA1_Final_digest__[ 20 ]; \
		\
		SHA1_Final( UUID_SHA1_Final_digest__, (CONTEXT_PTR) ); \
		memcpy( (OUT_UUID), UUID_SHA1_Final_digest__, kUUIDSize ); \
		UUIDSetVersion( (OUT_UUID), kUUIDVersion_5_SHA1 ); \
		UUIDMakeRFC4122( (OUT_UUID) ); \
		\
	}	while( 0 )

#define UUID_SHA1( NAMESPACE_UUID_PTR, PTR, LEN, OUT_UUID ) \
	do \
	{ \
		UUID_SHA1_Context		UUID_SHA1_ctx__; \
		\
		UUID_SHA1_Init( &UUID_SHA1_ctx__, (NAMESPACE_UUID_PTR) ); \
		UUID_SHA1_Update( &UUID_SHA1_ctx__, (PTR), (LEN) ); \
		UUID_SHA1_Final( &UUID_SHA1_ctx__, (OUT_UUID) ); \
		\
	}	while( 0 )

#if( !EXCLUDE_UNIT_TESTS )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	UUIDUtils_Test
	@abstract	Unit test.
*/
OSStatus	UUIDUtils_Test( void );
#endif

#ifdef __cplusplus
}
#endif

#endif // __UUIDUtils_h__
