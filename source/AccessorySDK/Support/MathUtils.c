/*
	File:    	MathUtils.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.12
	
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
	
	Copyright (C) 2001-2014 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "MathUtils.h"

#include <math.h>

//===========================================================================================================================
//	iceil2
//
//	Rounds x up to the closest integral power of 2. Based on code from the book Hacker's Delight.
//===========================================================================================================================

uint32_t	iceil2( uint32_t x )
{
	check( x <= UINT32_C( 0x80000000 ) );
	
	x = x - 1;
	x |= ( x >> 1 );
	x |= ( x >> 2 );
	x |= ( x >> 4 );
	x |= ( x >> 8 );
	x |= ( x >> 16 );
	return( x + 1 );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	MathUtils_Test
//===========================================================================================================================

OSStatus	MathUtils_Test( int inPrint )
{
	OSStatus			err;
	EWMA_FP_Data		ewma;
	
	EWMA_FP_Init( &ewma, 0.1, kEWMAFlags_StartWithFirstValue );
	EWMA_FP_Update( &ewma, 71 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 70 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 69 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 68 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 64 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 65 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 72 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 78 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 75 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 75 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 75 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 70 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	require_action( RoundTo( EWMA_FP_Get( &ewma ), .01 ) == 71.50, exit, err = -1 );
	
	require_action( iceil2( 0 ) == 0, exit, err = kResponseErr );
	require_action( iceil2( 1 ) == 1, exit, err = kResponseErr );
	require_action( iceil2( 2 ) == 2, exit, err = kResponseErr );
	require_action( iceil2( 32 ) == 32, exit, err = kResponseErr );
	require_action( iceil2( 4096 ) == 4096, exit, err = kResponseErr );
	require_action( iceil2( UINT32_C( 0x80000000 ) ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	require_action( iceil2( 3 ) == 4, exit, err = kResponseErr );
	require_action( iceil2( 17 ) == 32, exit, err = kResponseErr );
	require_action( iceil2( 2289 ) == 4096, exit, err = kResponseErr );
	require_action( iceil2( 26625 ) == 32768, exit, err = kResponseErr );
	require_action( iceil2( 2146483648 ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	require_action( iceil2( 2147483647 ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	
	err = kNoErr;
	
exit:
	printf( "MathUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
