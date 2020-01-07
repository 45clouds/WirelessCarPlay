/*
	File:    	AtomicUtils.c
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
	
	Copyright (C) 2009-2014 Apple Inc. All Rights Reserved.
*/

#include "AtomicUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"

#if( TARGET_OS_NETBSD )
	#include <sys/ras.h>
#endif

#if( AtomicUtils_USE_PTHREADS )
	#include <pthread.h>
#endif

//===========================================================================================================================
//	AtomicOpsInitialize
//===========================================================================================================================

#if( !AtomicUtils_USE_BUILTINS && TARGET_OS_NETBSD && TARGET_CPU_ARM )

static void		__AtomicUtils_Initialize( void );
static int32_t	atomic_fetch_and_or_32_slow(  int32_t *inPtr, int32_t inVal );
static int32_t	atomic_fetch_and_or_32_fast(  int32_t *inPtr, int32_t inVal );
static int32_t	atomic_fetch_and_and_32_slow( int32_t *inPtr, int32_t inVal );
static int32_t	atomic_fetch_and_and_32_fast( int32_t *inPtr, int32_t inVal );
static int32_t	atomic_fetch_and_xor_32_slow( int32_t *inPtr, int32_t inVal );
static int32_t	atomic_fetch_and_xor_32_fast( int32_t *inPtr, int32_t inVal );
static int32_t	atomic_add_and_fetch_32_slow( int32_t *inPtr, int32_t inVal );
static int32_t	atomic_add_and_fetch_32_fast( int32_t *inPtr, int32_t inVal );
static int32_t	atomic_val_compare_and_swap_32_slow( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue );
static int32_t	atomic_val_compare_and_swap_32_fast( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue );

static Boolean							gAtomicUtils_Initialized				= false;
static int32_t							gAtomicUtils_SpinLock					= 0;

atomic_fetch_and_or_32_func				g_atomic_fetch_and_or_32_func			= atomic_fetch_and_or_32_slow;
atomic_fetch_and_and_32_func			g_atomic_fetch_and_and_32_func			= atomic_fetch_and_and_32_slow;
atomic_fetch_and_xor_32_func			g_atomic_fetch_and_xor_32_func			= atomic_fetch_and_xor_32_slow;
atomic_add_and_fetch_32_func			g_atomic_add_and_fetch_32_func			= atomic_add_and_fetch_32_slow;
atomic_val_compare_and_swap_32_func		g_atomic_val_compare_and_swap_32_func	= atomic_val_compare_and_swap_32_slow;

RAS_DECL( gRAS_atomic_add_and_fetch_32 );
RAS_DECL( gRAS_atomic_fetch_and_or_32 );
RAS_DECL( gRAS_atomic_fetch_and_and_32 );
RAS_DECL( gRAS_atomic_fetch_and_xor_32 );
RAS_DECL( gRAS_atomic_val_compare_and_swap_32 );

static void	__AtomicUtils_Initialize( void )
{
	OSStatus		err;
	
	while( atomic_fetch_and_store_32( &gAtomicUtils_SpinLock, 1 ) != 0 )
	{
		atomic_yield();
	}
	if( !gAtomicUtils_Initialized )
	{
		err = rasctl( RAS_ADDR( gRAS_atomic_add_and_fetch_32 ), RAS_SIZE( gRAS_atomic_add_and_fetch_32 ), RAS_INSTALL );
		err = map_global_noerr_errno( err );
		check_noerr( err );
		
		err = rasctl( RAS_ADDR( gRAS_atomic_fetch_and_or_32 ),  RAS_SIZE( gRAS_atomic_fetch_and_or_32 ),  RAS_INSTALL );
		err = map_global_noerr_errno( err );
		check_noerr( err );
		
		err = rasctl( RAS_ADDR( gRAS_atomic_fetch_and_and_32 ), RAS_SIZE( gRAS_atomic_fetch_and_and_32 ), RAS_INSTALL );
		err = map_global_noerr_errno( err );
		check_noerr( err );
		
		err = rasctl( RAS_ADDR( gRAS_atomic_fetch_and_xor_32 ), RAS_SIZE( gRAS_atomic_fetch_and_xor_32 ), RAS_INSTALL );
		err = map_global_noerr_errno( err );
		check_noerr( err );
		
		err = rasctl( RAS_ADDR( gRAS_atomic_val_compare_and_swap_32 ), RAS_SIZE( gRAS_atomic_val_compare_and_swap_32 ), RAS_INSTALL );
		err = map_global_noerr_errno( err );
		check_noerr( err );
		
		g_atomic_fetch_and_or_32_func			= atomic_fetch_and_or_32_fast;
		g_atomic_fetch_and_and_32_func			= atomic_fetch_and_and_32_fast;
		g_atomic_fetch_and_xor_32_func			= atomic_fetch_and_xor_32_fast;
		g_atomic_add_and_fetch_32_func			= atomic_add_and_fetch_32_fast;
		g_atomic_val_compare_and_swap_32_func	= atomic_val_compare_and_swap_32_fast;
		
		atomic_read_write_barrier();
		gAtomicUtils_Initialized = true;
	}
	atomic_read_write_barrier();
	gAtomicUtils_SpinLock = 0;
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	atomic_fetch_and_add_32
//===========================================================================================================================

#if( !AtomicUtils_USE_BUILTINS && TARGET_CPU_MIPS )
int32_t	atomic_fetch_and_add_32( int32_t *inPtr, int32_t inVal )
{
	int32_t		result;
	int32_t		temp;
	
	__asm__ __volatile__(
	".set push				\n" // Save off assembler state.
	".set mips2				\n" // Tell assembler to let us use ll/sc instructions
	"1:	ll		%0, 0(%2)	\n" // result = *inPtr; (linked)
	"	addu	%1, %0, %3	\n" // temp = result + inVal;
	"	sc		%1, 0(%2)	\n" // *inPtr = temp (if still linked)
	"	beqzl	%1, 1b		\n" // If failed, retry
	".set pop				\n" // Restore assembler state.
	: "=&r" (result), 			// Outputs: %0 = result
	  "=&r" (temp) 				//			%1 = temp
	: "r"   (inPtr),			// Inputs:	%2 = inPtr
	  "r"   (inVal)				//			%3 = inVal
	: "memory" );				// Trashes: memory
	
	return( result );
}
#elif( !AtomicUtils_HAS_SYNC_BUILTINS && TARGET_OS_THREADX )
int32_t	atomic_fetch_and_add_32( int32_t *inPtr, int32_t inVal )
{
	UINT		oldInt;
	int32_t		oldValue;
	
	oldInt = tx_interrupt_control( TX_INT_DISABLE );
		oldValue = *inPtr;
		*inPtr = oldValue + inVal;
	tx_interrupt_control( oldInt );
	return( oldValue );
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	atomic_fetch_and_or_32
//===========================================================================================================================

#if( !AtomicUtils_USE_BUILTINS && TARGET_OS_NETBSD && TARGET_CPU_ARM )
static int32_t	atomic_fetch_and_or_32_slow( int32_t *inPtr, int32_t inVal )
{
	__AtomicUtils_Initialize();
	return( g_atomic_fetch_and_or_32_func( inPtr, inVal ) );
}

static int32_t	atomic_fetch_and_or_32_fast( int32_t *inPtr, int32_t inVal )
{
	int32_t		oldValue;
	
	RAS_START( gRAS_atomic_fetch_and_or_32 );
		__asm__ __volatile__( 
		"ldr r3, [%0]		\n"				// Read old value:		y = *inPtr.
		"orr r2, r3, %1		\n"				// OR inVal into value:	x = y | inVal
		"str r2, [%0]		\n"				// Write new value:		*inPtr = x;
		:									// Outputs:	none
		: "r" (inPtr), 						// Inputs:	%0 = inPtr
		  "r" (inVal)						// 			%1 = inVal
		: "r2", "r3", "memory" );			// Trashed:	r2, r3, and memory.
	RAS_END( gRAS_atomic_fetch_and_or_32 );
	__asm__ __volatile__( "mov %0, r3\n" : "=r" (oldValue) : : "memory" ); // Return old value.
	
	return( oldValue );
}
#elif( !AtomicUtils_USE_BUILTINS && TARGET_CPU_MIPS )
int32_t	atomic_fetch_and_or_32( int32_t *inPtr, int32_t inVal )
{
	uint32_t		oldValue;
	
	__asm__ __volatile__(
	".set push				\n" // Save off assembler state.
	".set mips2				\n"	// Tell assembler to let us use ll/sc instructions
	"1:	ll		%0, 0(%1)	\n"	// oldValue = *inPtr; (linked)
	"	or		$8, %0, %2	\n"	// newValue = oldValue | inVal;
	"	sc		$8, 0(%1)	\n"	// *inPtr   = oldValue (if still linked)
	"	beq		$8, 0, 1b	\n"	// If failed, retry
	".set pop				\n" // Save off assembler state.
	: "=r" (oldValue) 			// Outputs: %0 = newValue
	: "r" (inPtr), "r" (inVal)	// Inputs:	%1 = inPtr, %2 = inVal
	: "$8", "memory" );			// Trashes: t0/$8, memory
	return( oldValue );
}
#elif( !AtomicUtils_HAS_SYNC_BUILTINS && TARGET_OS_THREADX )
int32_t	atomic_fetch_and_or_32( int32_t *inPtr, int32_t inVal )
{
	UINT		oldInt;
	int32_t		oldValue;
	
	oldInt = tx_interrupt_control( TX_INT_DISABLE );
		oldValue = *inPtr;
		*inPtr = oldValue | inVal;
	tx_interrupt_control( oldInt );
	return( oldValue );
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	atomic_fetch_and_and_32
//===========================================================================================================================

#if( !AtomicUtils_USE_BUILTINS && TARGET_OS_NETBSD && TARGET_CPU_ARM )
static int32_t	atomic_fetch_and_and_32_slow( int32_t *inPtr, int32_t inVal )
{
	__AtomicUtils_Initialize();
	return( g_atomic_fetch_and_and_32_func( inPtr, inVal ) );
}

static int32_t	atomic_fetch_and_and_32_fast( int32_t *inPtr, int32_t inVal )
{
	int32_t		oldValue;
	
	RAS_START( gRAS_atomic_fetch_and_and_32 );
		__asm__ __volatile__( 
		"ldr r3, [%0]		\n"				// Read old value:			y = *inPtr.
		"and r2, r3, %1		\n"				// AND value with inVal:	x = y & inVal
		"str r2, [%0]		\n"				// Write new value:			*inPtr = x;
		:									// Outputs:	none
		: "r" (inPtr), "r" (inVal)			// Inputs:	%0 = inPtr, %1 = inVal.
		: "r2", "r3", "memory" );			// Trashed:	r2, r3, and memory.
	RAS_END( gRAS_atomic_fetch_and_and_32 );
	__asm__ __volatile__( "mov %0, r3\n" : "=r" (oldValue) : : "memory" ); // Return old value.
	
	return( oldValue );
}
#elif( !AtomicUtils_USE_BUILTINS && TARGET_CPU_MIPS )
int32_t	atomic_fetch_and_and_32( int32_t *inPtr, int32_t inVal )
{
	uint32_t		oldValue;
	
	__asm__ __volatile__(
	".set push				\n" // Save off assembler state.
	".set mips2				\n"	// Tell assembler to let us use ll/sc instructions
	"1:	ll		%0, 0(%1)	\n"	// oldValue = *inPtr; (linked)
	"	and		$8, %0, %2	\n"	// newValue = oldValue & inVal;
	"	sc		$8, 0(%1)	\n"	// *inPtr   = oldValue (if still linked)
	"	beq		$8, 0, 1b	\n"	// If failed, retry
	".set pop				\n" // Restore assembler state.
	: "=r" (oldValue) 			// Outputs: %0 = newValue
	: "r" (inPtr), "r" (inVal)	// Inputs:	%1 = inPtr, %2 = inVal
	: "$8", "memory" );			// Trashes: t0/$8, memory
	return( oldValue );
}
#elif( !AtomicUtils_HAS_SYNC_BUILTINS && TARGET_OS_THREADX )
int32_t	atomic_fetch_and_and_32( int32_t *inPtr, int32_t inVal )
{
	UINT		oldInt;
	int32_t		oldValue;
	
	oldInt = tx_interrupt_control( TX_INT_DISABLE );
		oldValue = *inPtr;
		*inPtr = oldValue & inVal;
	tx_interrupt_control( oldInt );
	return( oldValue );
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	atomic_fetch_and_xor_32
//===========================================================================================================================

#if( !AtomicUtils_USE_BUILTINS && TARGET_OS_NETBSD && TARGET_CPU_ARM )
static int32_t	atomic_fetch_and_xor_32_slow( int32_t *inPtr, int32_t inVal )
{
	__AtomicUtils_Initialize();
	return( g_atomic_fetch_and_xor_32_func( inPtr, inVal ) );
}

static int32_t	atomic_fetch_and_xor_32_fast( int32_t *inPtr, int32_t inVal )
{
	int32_t		oldValue;
	
	RAS_START( gRAS_atomic_fetch_and_xor_32 );
		__asm__ __volatile__( 
		"ldr r3, [%0]		\n"				// Read old value:			y = *inPtr.
		"eor r2, r3, %1		\n"				// XOR value with inVal:	x = y ^ inVal
		"str r2, [%0]		\n"				// Write new value:			*inPtr = x;
		:									// Outputs:	none
		: "r" (inPtr), "r" (inVal)			// Inputs:	%0 = inPtr, %1 = inVal.
		: "r2", "r3", "memory" );			// Trashed:	r2, r3, and memory.
	RAS_END( gRAS_atomic_fetch_and_xor_32 );
	__asm__ __volatile__( "mov %0, r3\n" : "=r" (oldValue) : : "memory" ); // Return old value.
	
	return( oldValue );
}
#elif( !AtomicUtils_USE_BUILTINS && TARGET_CPU_MIPS )
int32_t	atomic_fetch_and_xor_32( int32_t *inPtr, int32_t inVal )
{
	uint32_t		oldValue;
	
	__asm__ __volatile__(
	".set push				\n" // Save off assembler state.
	".set mips2				\n"	// Tell assembler to let us use ll/sc instructions
	"1:	ll		%0, 0(%1)	\n"	// oldValue = *inPtr; (linked)
	"	xor		$8, %0, %2	\n"	// newValue = oldValue ^ inVal;
	"	sc		$8, 0(%1)	\n"	// *inPtr   = oldValue (if still linked)
	"	beq		$8, 0, 1b	\n"	// If failed, retry
	".set pop				\n" // Restore assembler state.
	: "=r" (oldValue) 			// Outputs: %0 = newValue
	: "r" (inPtr), "r" (inVal)	// Inputs:	%1 = inPtr, %2 = inVal
	: "$8", "memory" );			// Trashes: t0/$8, memory
	return( oldValue );
}
#elif( !AtomicUtils_HAS_SYNC_BUILTINS && TARGET_OS_THREADX )
int32_t	atomic_fetch_and_xor_32( int32_t *inPtr, int32_t inVal )
{
	UINT		oldInt;
	int32_t		oldValue;
	
	oldInt = tx_interrupt_control( TX_INT_DISABLE );
		oldValue = *inPtr;
		*inPtr = oldValue ^ inVal;
	tx_interrupt_control( oldInt );
	return( oldValue );
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	atomic_add_and_fetch_32
//===========================================================================================================================

#if( !AtomicUtils_USE_BUILTINS && TARGET_OS_NETBSD && TARGET_CPU_ARM )
static int32_t	atomic_add_and_fetch_32_slow( int32_t *inPtr, int32_t inVal )
{
	__AtomicUtils_Initialize();
	return( g_atomic_add_and_fetch_32_func( inPtr, inVal ) );
}

static int32_t	atomic_add_and_fetch_32_fast( int32_t *inPtr, int32_t inVal )
{	
	int32_t		newValue;
	
	RAS_START( gRAS_atomic_add_and_fetch_32 );
		__asm__ __volatile__( 
		"ldr r3, [%0]		\n"			// Read old value:		x = *inPtr.
		"add r3, r3, %1		\n"			// Add inVal to value:	x += inVal
		"str r3, [%0]		\n"			// Write new value:		*inPtr = x;
		:								// Outputs:	none
		: "r" (inPtr), 					// Inputs:	%0 = inPtr
		  "r" (inVal)					//			%1 = inVal
		: "r3", "memory" );				// Trashed:	r3 and memory.
	RAS_END( gRAS_atomic_add_and_fetch_32 );
	__asm__ __volatile__( "mov %0, r3\n" : "=r" (newValue) : : "memory" ); // Return new value.
	
	return( newValue );
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	atomic_fetch_and_store_32
//===========================================================================================================================

#if( !AtomicUtils_USE_BUILTINS && TARGET_CPU_ARM )
int32_t	atomic_fetch_and_store_32( int32_t *inPtr, int32_t inVal )
{
	int32_t		result;
	
	__asm__ __volatile__(
	"	swp	%0, %1, [%2]"
	: "=&r" (result)	// Outputs:	%0 = result
	: "r"   (inVal), 	// Inputs:	%1 = inVal
	  "r"   (inPtr)		//			%2 = inPtr
	: "memory", "cc" );	// Trashes:	memory and cc
	return( result );
}
#elif( !AtomicUtils_USE_BUILTINS && TARGET_CPU_MIPS )
int32_t	atomic_fetch_and_store_32( int32_t *inPtr, int32_t inVal )
{
	uint32_t		oldValue;
	
	__asm__ __volatile__(
	".set push				\n" // Save off assembler state.
	".set mips2				\n"	// Tell assembler to let us use ll/sc instructions
	"1:	ll		%0, 0(%1)	\n"	// oldValue = *inPtr; (linked)
	"	move	$8, %2		\n"	// newValue = inVal;
	"	sc		$8, 0(%1)	\n"	// *inPtr   = newValue (if still linked)
	"	beqzl	$8, 1b		\n"	// If failed, retry
	".set pop				\n" // Restore assembler state.
	: "=r" (oldValue) 			// Outputs: %0 = newValue
	: "r" (inPtr), 				// Inputs:	%1 = inPtr
	  "r" (inVal)				//			%2 = inVal
	: "$8", "memory" );			// Trashes: t0/$8, memory
	return( oldValue );
}
#elif( !AtomicUtils_HAS_SYNC_BUILTINS && TARGET_OS_THREADX )
int32_t	atomic_fetch_and_store_32( int32_t *inPtr, int32_t inVal )
{
	UINT		oldInt;
	int32_t		oldValue;
	
	oldInt = tx_interrupt_control( TX_INT_DISABLE );
		oldValue = *inPtr;
		*inPtr = inVal;
	tx_interrupt_control( oldInt );
	return( oldValue );
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	atomic_fetch_and_store_32
//===========================================================================================================================

#if( !AtomicUtils_USE_BUILTINS && TARGET_OS_NETBSD && TARGET_CPU_ARM )
static int32_t	atomic_val_compare_and_swap_32_slow( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	__AtomicUtils_Initialize();
	return( g_atomic_val_compare_and_swap_32_func( inPtr, inOldValue, inNewValue ) );
}

static int32_t	atomic_val_compare_and_swap_32_fast( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	int32_t		oldValue;
	
	RAS_START( gRAS_atomic_val_compare_and_swap_32 );
	__asm__ __volatile__( 
		"ldr   r3, [%0]		\n"		// oldValue = *inPtr.
		"cmp   r3, %1		\n"		// good = ( oldValue == inOldValue ).
		"streq %2, [%0]		\n"		// if( good ) *inPtr = inNewValue.
		: 							// Outputs:	none
		: "r"   (inPtr), 			// Inputs:	%0 = inPtr
		  "r"   (inOldValue), 		//			%1 = inOldValue
		  "r"   (inNewValue)		// 			%2 = inNewValue
		: "r3", "cc", "memory" );	// Trashed:	r3, condition codes, and memory.
	RAS_END( gRAS_atomic_val_compare_and_swap_32 );
	__asm__ __volatile__( "mov %0, r3\n" : "=r" (oldValue) : : "memory" ); // Return old value.
	return( oldValue );
}
#elif( !AtomicUtils_USE_BUILTINS && TARGET_CPU_MIPS )
int32_t	atomic_val_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	int32_t		oldValue;
	int32_t		temp;
	
	__asm__ __volatile__(
	".set push				\n" // Save off assembler state.
	".set mips2				\n"	// Tell assembler to let us use ll/sc instructions
	"1:	ll		%0, 0(%2)	\n"	// oldValue = *inPtr; (linked)
	"	bne		%0, %3, 2f	\n"	// If oldValue != inOldValue, goto exit
	"	move	%1, %4		\n"	// temp = inNewValue (sc trashes temp)
	"	sc		%1, 0(%2)	\n"	// *inPtr = temp (if still linked)
	"	beqzl	%1, 1b		\n"	// If failed, retry
	"2:						\n"
	".set pop				\n" // Restore assembler state.
	: "=%r" (oldValue), 		// Outputs: %0 = oldValue
	  "=%r" (temp)				//			%1 = temp
	: "r"   (inPtr), 			// Inputs:	%2 = inPtr
	  "r"   (inOldValue), 		//			%3 = inOldValue
	  "r"   (inNewValue)		//			%4 = inNewValue
	: "memory" );				// Trashes: memory
	return( oldValue );
}
#elif( !AtomicUtils_HAS_SYNC_BUILTINS && TARGET_OS_THREADX )
int32_t	atomic_val_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	UINT		oldInt;
	int32_t		oldValue;
	
	oldInt = tx_interrupt_control( TX_INT_DISABLE );
		oldValue = *inPtr;
		if( oldValue == inOldValue )
		{
			*inPtr = inNewValue;
		}
	tx_interrupt_control( oldInt );
	return( oldValue );
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	atomic_once_slow
//===========================================================================================================================

#if( !AtomicUtils_USE_PTHREADS )
void	atomic_once_slow( atomic_once_t *inOnce, void *inContext, atomic_once_function_t inFunction )
{
	atomic_once_t		prev;
	
	prev = atomic_val_compare_and_swap_32( inOnce, 0, 1 );
	if( __builtin_expect( prev, 2 ) == 2 )
	{
		// Already initialized. Nothing to do.
	}
	else if( prev == 0 )
	{
		// Got lock. Initialize it and release the lock
		
		inFunction( inContext );
		atomic_read_write_barrier();
		*inOnce = 2;
	}
	else
	{
		// Another thread is initializing it. Yield while it completes.
		
		volatile atomic_once_t * const		volatileOnce = inOnce;
		
		do
		{
			atomic_yield();
			
		}	while( *volatileOnce != 2 );
		
		atomic_read_write_barrier();
	}
}
#endif

#if 0
#pragma mark -
#pragma mark == pthreads ==
#endif

#if( AtomicUtils_USE_PTHREADS )

static pthread_mutex_t		gAtomicMutex = PTHREAD_MUTEX_INITIALIZER;

//===========================================================================================================================
//	atomic_add_and_fetch_32
//===========================================================================================================================

int32_t	atomic_add_and_fetch_32( int32_t *inPtr, int32_t inVal )
{
	int32_t		tmp;
	
	pthread_mutex_lock( &gAtomicMutex );
	*inPtr += inVal;
	tmp = *inPtr;
	pthread_mutex_unlock( &gAtomicMutex );
	return( tmp );
}

//===========================================================================================================================
//	atomic_fetch_and_add_32
//===========================================================================================================================

int32_t	atomic_fetch_and_add_32( int32_t *inPtr, int32_t inVal )
{
	int32_t		tmp;
	
	pthread_mutex_lock( &gAtomicMutex );
	tmp = *inPtr;
	*inPtr += inVal;
	pthread_mutex_unlock( &gAtomicMutex );
	return( tmp );
}

#endif

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	AtomicUtils_Test
//===========================================================================================================================

void	AtomicUtils_OnceTest( void *inContext );

OSStatus	AtomicUtils_Test( void )
{
	OSStatus			err;
	int32_t				s32;
	atomic_once_t		once;
	
	s32 = 123456790;
	require_action( ( atomic_fetch_and_add_32( &s32, 1 )  == 123456790 ) && ( s32 ==  123456791 ), exit, err = -1 );
	require_action( ( atomic_fetch_and_add_32( &s32, -1 ) == 123456791 ) && ( s32 ==  123456790 ), exit, err = -1 );
	require_action( ( atomic_fetch_and_sub_32( &s32, 1 )  == 123456790 ) && ( s32 ==  123456789 ), exit, err = -1 );
	require_action( ( atomic_fetch_and_sub_32( &s32, -1 ) == 123456789 ) && ( s32 ==  123456790 ), exit, err = -1 );
	
	require_action( ( atomic_fetch_and_add_32( &s32,  234567901 ) ==  123456790 ) && ( s32 ==  358024691 ), exit, err = -1 );
	require_action( ( atomic_fetch_and_sub_32( &s32,  234567901 ) ==  358024691 ) && ( s32 ==  123456790 ), exit, err = -1 );
	require_action( ( atomic_fetch_and_or_32(  &s32, 1142982721 ) ==  123456790 ) && ( s32 == 1199295831 ), exit, err = -1 );
	require_action( ( atomic_fetch_and_and_32( &s32, (int32_t) UINT32_C( 2216824865 ) ) == 1199295831 ) && ( s32 == 69337089 ), exit, err = -1 );
	require_action( ( atomic_fetch_and_xor_32( &s32, 1281434631 ) ==   69337089 ) && ( s32 == 1212359686 ), exit, err = -1 );
	
	require_action( ( atomic_add_and_fetch_32( &s32,  234567901 ) ==  1446927587 ) && ( s32 == 1446927587 ), exit, err = -1 );
	require_action( ( atomic_sub_and_fetch_32( &s32,  234567901 ) ==  1212359686 ) && ( s32 == 1212359686 ), exit, err = -1 );
	require_action( ( atomic_or_and_fetch_32(  &s32, 1142982721 ) ==  1281600583 ) && ( s32 == 1281600583 ), exit, err = -1 );
	require_action( ( atomic_and_and_fetch_32( &s32, (int32_t) UINT32_C( 2216824865 ) ) == 69337089 ) && ( s32 == 69337089 ), exit, err = -1 );
	require_action( ( atomic_xor_and_fetch_32( &s32, 1281434631 ) ==  1212359686 ) && ( s32 == 1212359686 ), exit, err = -1 );
	
	require_action( ( atomic_fetch_and_store_32( &s32, 123456790 ) == 1212359686 ) && ( s32 == 123456790 ), exit, err = -1 );
	require_action(  atomic_bool_compare_and_swap_32( &s32, 123456790, 234567901 ) && ( s32 == 234567901 ), exit, err = -1 );
	require_action( !atomic_bool_compare_and_swap_32( &s32, 224567901, 123456790 ) && ( s32 == 234567901 ), exit, err = -1 );
	require_action( ( atomic_val_compare_and_swap_32( &s32, 234567901, 123456790 ) == 234567901 ) && ( s32 == 123456790 ), exit, err = -1 );
	require_action( ( atomic_val_compare_and_swap_32( &s32, 234567901, 113456790 ) == 123456790 ) && ( s32 == 123456790 ), exit, err = -1 );
	
	atomic_read_barrier();
	atomic_write_barrier();
	atomic_read_write_barrier();
	
	atomic_yield();
	atomic_hardware_yield();
	
	s32 = 0;
	once = 0;
	atomic_once( &once, &s32, AtomicUtils_OnceTest );
	require_action( s32 == 1, exit, err = -1 );
	atomic_once( &once, &s32, AtomicUtils_OnceTest );
	require_action( s32 == 1, exit, err = -1 );
	atomic_once( &once, &s32, AtomicUtils_OnceTest );
	require_action( s32 == 1, exit, err = -1 );
	
	s32 = 0;
	atomic_spinlock_lock( &s32 );
	require_action( s32 == 1, exit, err = -1 );
	atomic_spinlock_unlock( &s32 );
	require_action( s32 == 0, exit, err = -1 );
	
	err = kNoErr;
		
exit:
	printf( "AtomicUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

void	AtomicUtils_OnceTest( void *inContext )
{
	*( (int32_t *) inContext ) += 1;
}
#endif // !EXCLUDE_UNIT_TESTS
