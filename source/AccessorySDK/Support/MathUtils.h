/*
	File:    	MathUtils.h
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

#ifndef	__MathUtils_h__
#define	__MathUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	iceil2
	@abstract	Rounds up to an integral power of 2 <= x. 0 and 1 are considered powers of 2.
	@discussion
	
	iceil2    is valid for 0 >= x <= 2^31.
*/
uint32_t	iceil2( uint32_t x );

#if 0
#pragma mark == Statistics ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Exponentially Weighted Moving Average (EWMA) for floating point.
	@abstract	See <http://en.wikipedia.org/wiki/Moving_average>.
*/
#define kEWMAFlags_StartWithFirstValue		0x00
#define kEWMAFlags_StartWithZero			0x01

typedef struct
{
	double			alpha;
	double			average;
	unsigned char	flags;	
	
}	EWMA_FP_Data;

#define EWMA_FP_Init( DATA, ALPHA, FLAGS ) \
	do \
	{ \
		(DATA)->alpha   = (ALPHA); \
		(DATA)->average = 0; \
		(DATA)->flags   = (FLAGS); \
		\
	}	while( 0 )

#define EWMA_FP_Update( DATA, X ) \
	do \
	{ \
		if( (DATA)->flags ) \
		{ \
			(DATA)->average += ( (DATA)->alpha * ( (X) - (DATA)->average ) ); \
		} \
		else \
		{ \
			(DATA)->average = (X); \
			(DATA)->flags = 1; \
		} \
		\
	}	while( 0 )

#define EWMA_FP_Get( DATA )		(DATA)->average

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MovingAverageF
	@abstract	Floating point exponentially weighted moving average (EWMA).
	@discussion	See <http://en.wikipedia.org/wiki/Moving_average>.
	
	@param		AVG		Current average. This is often initialized to the first value.
	@param		X		New value to add to the average.
	@param		ALPHA	Smoothing factor between 0 and 1. To express in time periods: ALPHA = 2/(N+1), such as N=19: 2/(19/1) = 0.1.
	
	@result		New average.
*/
#define MovingAverageF( AVG, X, ALPHA )		( ( (AVG) * ( 1 - (ALPHA) ) ) + ( (X) * (ALPHA) ) )

#if 0
#pragma mark == Debugging ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MathUtils_Test
	@abstract	Unit test.
*/
#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	MathUtils_Test( int inPrint );
#endif

#ifdef __cplusplus
}
#endif

#endif // __MathUtils_h__
