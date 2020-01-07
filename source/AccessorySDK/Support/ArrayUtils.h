/*
	File:    	ArrayUtils.h
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

#ifndef	__ArrayUtils_h__
#define	__ArrayUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		SimpleArray
	@abstract	Functions for managing a simple, dynamic C array.
*/
#define		SimpleArrayInit( OUT_ARRAY, OUT_COUNT )		\
	do													\
	{													\
		*( OUT_ARRAY ) = NULL;							\
		*( OUT_COUNT ) = 0;								\
														\
	}	while( 0 )


#define		SimpleArrayFree( OUT_ARRAY, OUT_COUNT )		\
	do													\
	{													\
		ForgetMem( ( OUT_ARRAY ) );						\
		*( OUT_COUNT ) = 0;								\
														\
	}	while( 0 )

OSStatus	SimpleArrayAppendItem( void *ioArray, size_t *ioCount, size_t inItemLen, const void *inItemPtr );
OSStatus	SimpleArrayRemoveItemAtIndex( void *ioArray, size_t *ioCount, size_t inItemLen, size_t inIndex, Boolean inShrink );

#define PtrArrayInit( OUT_ARRAY, OUT_COUNT )	SimpleArrayInit( OUT_ARRAY, OUT_COUNT )
#define PtrArrayFree( OUT_ARRAY, OUT_COUNT )	SimpleArrayFree( OUT_ARRAY, OUT_COUNT )

STATIC_INLINE OSStatus	PtrArrayAppendItem( void *ioArray, size_t *ioCount, const void *inPtr )
{
	return( SimpleArrayAppendItem( ioArray, ioCount, sizeof( inPtr ), &inPtr ) );
}

STATIC_INLINE OSStatus	PtrArrayRemoveItemAtIndex( void *ioArray, size_t *ioCount, size_t inIndex, Boolean inShrink )
{
	return( SimpleArrayRemoveItemAtIndex( ioArray, ioCount, sizeof( const void * ), inIndex, inShrink ) );
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ArrayUtils_Test
	@abstract	Unit test.
*/
#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	ArrayUtils_Test( void );
#endif

#ifdef __cplusplus
}
#endif

#endif // __ArrayUtils_h__
