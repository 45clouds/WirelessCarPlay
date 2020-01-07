/*
	File:    	MD5Utils.h
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
	
	Copyright (C) 2007-2013 Apple Inc. All Rights Reserved.
*/
	 
#ifndef __MD5Utils_h__
#define __MD5Utils_h__

#include "CommonServices.h"

#ifdef __cplusplus
extern "C" {
#endif

#if( TARGET_OS_FREEBSD )
	#include <openssl/md5.h>
#elif( TARGET_HAS_MOCANA_SSL )
	#include "mtypes.h"
	#include "merrors.h"
	#include "hw_accel.h"
	#include "md5.h"
#elif( !TARGET_HAS_COMMON_CRYPTO )

	// OpenSSL-compatible mappings.

	#define MD5_CTX			MD5Context
	#define MD5_Init		MD5Init
	#define MD5_Update		MD5Update
	#define MD5_Final		MD5Final
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		MD5Context
	@abstract	Structure used for context between MD5 calls.
*/

typedef struct
{
	uint32_t		buf[ 4 ];
	uint32_t		bytes[ 2 ];
	uint32_t		in[ 16 ];
	
}	MD5Context;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MD5
	@abstract	Convenience routine to generate an MD5 from a single buffer of data.
*/

typedef void ( *MD5Func )( const void *inSourcePtr, size_t inSourceSize, uint8_t outKey[ 16 ] );

void	MD5OneShot( const void *inSourcePtr, size_t inSourceSize, uint8_t outKey[ 16 ] );
void	MD5OneShot_V1( const void *inSourcePtr, size_t inSourceSize, uint8_t outKey[ 16 ] );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MD5Init
	@abstract	Initializes the MD5 message digest.
*/

void	MD5Init( MD5Context *context );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MD5Update
	@abstract	Updates the MD5 message digest with the specified data.
*/

typedef void ( *MD5UpdateFunc )( MD5Context *context, void const *inBuf, size_t len );

void	MD5Update( MD5Context *context, void const *inBuf, size_t len );
void	MD5Update_V1( MD5Context *context, void const *inBuf, size_t len );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MD5Final
	@abstract	Finalizes and generates the resulting message digest.
*/

typedef void ( *MD5FinalFunc )( uint8_t digest[ 16 ], MD5Context *context );

void	MD5Final( uint8_t digest[ 16 ], MD5Context *context );
void	MD5Final_V1( uint8_t digest[ 16 ], MD5Context *context );

#if 0
#pragma mark == Debugging ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MD5UtilsTest
	@abstract	Unit test.
*/

#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	MD5UtilsTest( void );
#endif

#ifdef __cplusplus
}
#endif

#endif // __MD5Utils_h__
