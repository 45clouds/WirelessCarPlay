/*
	File:    	AtomicUtils.h
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
	
	Copyright (C) 2001-2014 Apple Inc. All Rights Reserved.
*/
/*!
    @header		Atomic Operations 
    @discussion	Provides lock-free atomic operations like such as incrementing an integer, compare-and-swap, etc.
*/

#ifndef	__AtomicUtils_h__
#define	__AtomicUtils_h__

#if( defined( AtomicUtils_PLATFORM_HEADER ) )
	#include  AtomicUtils_PLATFORM_HEADER
#endif

#include "CommonServices.h"

#if( COMPILER_VISUAL_CPP )
	#include <intrin.h>
#endif

#if( TARGET_OS_FREEBSD )
	#include <machine/atomic.h>
#endif

#if( TARGET_OS_QNX )
	#include <atomic.h>
	#include _NTO_CPU_HDR_(smpxchg.h)
#endif

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Atomic Operations
	@abstract	Macros for atomic operations.
	
	atomic_add()					{ *ptr += value; }
	atomic_sub()					{ *ptr -= value; }
	atomic_or()						{ *ptr |= value; }
	atomic_and()					{ *ptr &= value; }
	atomic_xor()					{ *ptr ^= value; }
	
	atomic_fetch_and_add()			{ tmp = *ptr; *ptr += value; return tmp; }
	atomic_fetch_and_sub()			{ tmp = *ptr; *ptr -= value; return tmp; }
	atomic_fetch_and_or()			{ tmp = *ptr; *ptr |= value; return tmp; }
	atomic_fetch_and_and()			{ tmp = *ptr; *ptr &= value; return tmp; }
	atomic_fetch_and_xor()			{ tmp = *ptr; *ptr ^= value; return tmp; }
	
	atomic_add_and_fetch()			{ *ptr += value; return *ptr; }
	atomic_sub_and_fetch()			{ *ptr -= value; return *ptr; }
	atomic_or_and_fetch()			{ *ptr |= value; return *ptr; }
	atomic_and_and_fetch()			{ *ptr &= value; return *ptr; }
	atomic_xor_and_fetch()			{ *ptr ^= value; return *ptr; }
	
	atomic_fetch_and_store()		{ tmp = *ptr; *ptr = value; return tmp; }
	atomic_bool_compare_and_swap()	{ if( *ptr == old ) { *ptr = new; return 1;   } else return 0; }
	atomic_val_compare_and_swap()	{ if( *ptr == old ) { *ptr = new; return old; } else return *ptr; }
	
	atomic_read_barrier()
	atomic_write_barrier()
	atomic_read_write_barrier()
*/

// clang and newer versions of GCC provide atomic builtins. GCC introduced these in 4.2, but for some targets, 
// such as ARM, they didn't show up until later (4.5 for ARM tested, but it's possible earlier versions also 
// have them). It's also possible that specific targets may not have these builtins even with newer versions
// of GCC. There isn't a good way to detect support for it so this assumes it works and you can override if not.

#if( !defined( AtomicUtils_HAS_SYNC_BUILTINS ) )
	#if( COMPILER_CLANG || ( COMPILER_GCC >= 40500 ) )
		#define AtomicUtils_HAS_SYNC_BUILTINS		1
	#else
		#define AtomicUtils_HAS_SYNC_BUILTINS		0
	#endif
#endif

#if( !defined( AtomicUtils_USE_PTHREADS ) )
	#define AtomicUtils_USE_PTHREADS		0
#endif
#if( AtomicUtils_USE_PTHREADS )

#if 0
#pragma mark == pthreads ==
#endif

//===========================================================================================================================
//	pthreads
//===========================================================================================================================

int32_t	atomic_add_and_fetch_32( int32_t *inPtr, int32_t inVal );
int32_t	atomic_fetch_and_add_32( int32_t *inPtr, int32_t inVal );
int32_t	atomic_fetch_and_store_32( int32_t *inPtr, int32_t inVal );

#elif( AtomicUtils_HAS_SYNC_BUILTINS )

#if 0
#pragma mark -
#pragma mark == __sync builtins ==
#endif

//===========================================================================================================================
//	Sync Builtins
//===========================================================================================================================
	
#define AtomicUtils_USE_BUILTINS		1

#define atomic_fetch_and_add_32( PTR, VAL )							__sync_fetch_and_add( (PTR), (VAL) )
#define atomic_fetch_and_sub_32( PTR, VAL )							__sync_fetch_and_sub( (PTR), (VAL) )
#define atomic_fetch_and_or_32( PTR, VAL )							__sync_fetch_and_or( (PTR), (VAL) )
#define atomic_fetch_and_and_32( PTR, VAL )							__sync_fetch_and_and( (PTR), (VAL) )
#define atomic_fetch_and_xor_32( PTR, VAL )							__sync_fetch_and_xor( (PTR), (VAL) )

#define atomic_add_and_fetch_32( PTR, VAL )							__sync_add_and_fetch( (PTR), (VAL) )
#define atomic_sub_and_fetch_32( PTR, VAL )							__sync_sub_and_fetch( (PTR), (VAL) )
#define atomic_or_and_fetch_32( PTR, VAL )							__sync_or_and_fetch( (PTR), (VAL) )
#define atomic_and_and_fetch_32( PTR, VAL )							__sync_and_and_fetch( (PTR), (VAL) )
#define atomic_xor_and_fetch_32( PTR, VAL )							__sync_xor_and_fetch( (PTR), (VAL) )

#define atomic_fetch_and_store_32( PTR, VAL )						__sync_lock_test_and_set( (PTR), (VAL) )
#define atomic_bool_compare_and_swap_32( PTR, OLD_VAL, NEW_VAL )	__sync_bool_compare_and_swap( (PTR), (OLD_VAL), (NEW_VAL) )
#define atomic_val_compare_and_swap_32( PTR, OLD_VAL, NEW_VAL )		__sync_val_compare_and_swap( (PTR), (OLD_VAL), (NEW_VAL) )

#if( defined( __i386__ ) || defined( __x86_64__ ) ) // GCC emits nothing for __sync_synchronize() on i386/x86_64.
	#define atomic_read_barrier()									__asm__ __volatile__( "mfence" )
	#define atomic_write_barrier()									__asm__ __volatile__( "mfence" )
	#define atomic_read_write_barrier()								__asm__ __volatile__( "mfence" )
#else
	#define atomic_read_barrier()									__sync_synchronize()
	#define atomic_write_barrier()									__sync_synchronize()
	#define atomic_read_write_barrier()								__sync_synchronize()
#endif

#elif( COMPILER_VISUAL_CPP )

#if 0
#pragma mark -
#pragma mark == Visual C++ ==
#endif

//===========================================================================================================================
//	Visual C++
//===========================================================================================================================
	
#define AtomicUtils_USE_BUILTINS		1

#pragma intrinsic( _InterlockedAnd )
#pragma intrinsic( _InterlockedCompareExchange )
#pragma intrinsic( _InterlockedExchangeAdd )
#pragma intrinsic( _InterlockedOr )
#pragma intrinsic( _InterlockedXor )
#pragma intrinsic( _ReadBarrier )
#pragma intrinsic( _WriteBarrier )
#pragma intrinsic( _ReadWriteBarrier )

check_compile_time( sizeof( long ) == sizeof( int32_t ) );
check_compile_time( sizeof( long ) == 4 );

#define					atomic_fetch_and_add_32( PTR, VAL )							_InterlockedExchangeAdd( (long *)(PTR), (VAL) )
STATIC_INLINE int32_t	atomic_fetch_and_sub_32( int32_t *inPtr, int32_t inVal )	{ return( _InterlockedExchangeAdd( (long *) inPtr, -inVal ) ); }
#define					atomic_fetch_and_or_32( PTR, VAL )							_InterlockedOr( (long *)(PTR), (VAL) )
#define					atomic_fetch_and_and_32( PTR, VAL )							_InterlockedAnd( (long *)(PTR), (VAL) )
#define					atomic_fetch_and_xor_32( PTR, VAL )							_InterlockedXor( (long *)(PTR), (VAL) )

STATIC_INLINE int32_t	atomic_add_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( _InterlockedExchangeAdd( (long *) inPtr,  inVal ) + inVal ); }
STATIC_INLINE int32_t	atomic_sub_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( _InterlockedExchangeAdd( (long *) inPtr, -inVal ) - inVal ); }
STATIC_INLINE int32_t	atomic_or_and_fetch_32(  int32_t *inPtr, int32_t inVal )	{ return( _InterlockedOr(          (long *) inPtr,  inVal ) | inVal ); }
STATIC_INLINE int32_t	atomic_and_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( _InterlockedAnd(         (long *) inPtr,  inVal ) & inVal ); }
STATIC_INLINE int32_t	atomic_xor_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( _InterlockedXor(         (long *) inPtr,  inVal ) ^ inVal ); }

#define atomic_fetch_and_store_32( PTR, VAL )						_InterlockedExchange( (long *)(PTR), (VAL) )
STATIC_INLINE Boolean	atomic_bool_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	return( (Boolean)( _InterlockedCompareExchange( (long *) inPtr, inNewValue, inOldValue ) == inOldValue ) );
}
#define atomic_val_compare_and_swap_32( PTR, OLD_VAL, NEW_VAL )		_InterlockedCompareExchange( (long *)(PTR), (NEW_VAL), (OLD_VAL) )

#define atomic_read_barrier()			_ReadBarrier()
#define atomic_write_barrier()			_WriteBarrier()
#define atomic_read_write_barrier()		_ReadWriteBarrier()

#elif( TARGET_OS_FREEBSD )

#if 0
#pragma mark -
#pragma mark == FreeBSD ==
#endif

//===========================================================================================================================
//	FreeBSD
//===========================================================================================================================

#undef  atomic_add_32
#define atomic_add_32( PTR, VAL )		atomic_add_int( (volatile u_int *)(PTR), (u_int)(VAL) )

STATIC_INLINE int32_t	atomic_add_and_fetch_32( int32_t *inPtr, int32_t inVal ) \
{
	return( ( inVal >= 0 ) ?
		( ( (int32_t) atomic_fetchadd_32( (volatile u_int *) inPtr,  (u_int)  inVal ) ) + inVal ) :
		( ( (int32_t) atomic_fetchadd_32( (volatile u_int *) inPtr,  (u_int) -inVal ) ) + inVal ) );
}

#define atomic_read_barrier()			rmb()
#define atomic_write_barrier()			wmb()
#define atomic_read_write_barrier()		mb()

#elif( TARGET_OS_NETBSD && TARGET_CPU_ARM )

#if 0
#pragma mark -
#pragma mark == NetBSD (ARM) ==
#endif

//===========================================================================================================================
//	NetBSD (ARM)
//===========================================================================================================================

typedef int32_t	( *atomic_fetch_and_or_32_func )(  int32_t *inPtr, int32_t inVal );
typedef int32_t	( *atomic_fetch_and_and_32_func )( int32_t *inPtr, int32_t inVal );
typedef int32_t	( *atomic_fetch_and_xor_32_func )( int32_t *inPtr, int32_t inVal );
typedef int32_t	( *atomic_add_and_fetch_32_func )( int32_t *inPtr, int32_t inVal );
typedef int32_t	( *atomic_val_compare_and_swap_32_func )( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue );

extern atomic_fetch_and_or_32_func				g_atomic_fetch_and_or_32_func;
extern atomic_fetch_and_and_32_func				g_atomic_fetch_and_and_32_func;
extern atomic_fetch_and_xor_32_func				g_atomic_fetch_and_xor_32_func;
extern atomic_add_and_fetch_32_func				g_atomic_add_and_fetch_32_func;
extern atomic_val_compare_and_swap_32_func		g_atomic_val_compare_and_swap_32_func;

#define					atomic_add_and_fetch_32( PTR, VAL )							g_atomic_add_and_fetch_32_func( (PTR), (VAL) )

STATIC_INLINE int32_t	atomic_fetch_and_add_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_add_and_fetch_32( inPtr,  inVal ) - inVal ); }
STATIC_INLINE int32_t	atomic_fetch_and_sub_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_add_and_fetch_32( inPtr, -inVal ) + inVal ); }
#define					atomic_fetch_and_or_32(  PTR, VAL )							g_atomic_fetch_and_or_32_func(  (PTR), (VAL) )
#define					atomic_fetch_and_and_32( PTR, VAL )							g_atomic_fetch_and_and_32_func( (PTR), (VAL) )
#define					atomic_fetch_and_xor_32( PTR, VAL )							g_atomic_fetch_and_xor_32_func( (PTR), (VAL) )

STATIC_INLINE int32_t	atomic_sub_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_add_and_fetch_32( inPtr, -inVal ) ); }
STATIC_INLINE int32_t	atomic_or_and_fetch_32(  int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_or_32(  inPtr,  inVal ) | inVal ); }
STATIC_INLINE int32_t	atomic_and_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_and_32( inPtr,  inVal ) & inVal ); }
STATIC_INLINE int32_t	atomic_xor_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_xor_32( inPtr,  inVal ) ^ inVal ); }

int32_t					atomic_fetch_and_store_32( int32_t *inPtr, int32_t inVal );
#define					atomic_val_compare_and_swap_32( PTR, OLD_VAL, NEW_VAL )		g_atomic_val_compare_and_swap_32_func( (PTR), (OLD_VAL), (NEW_VAL) )
STATIC_INLINE Boolean	atomic_bool_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	return( (Boolean)( atomic_val_compare_and_swap_32( inPtr, inOldValue, inNewValue ) == inOldValue ) );
}

#define atomic_read_barrier()			__asm__ __volatile__( "" : : : "memory" )
#define atomic_write_barrier()			__asm__ __volatile__( "" : : : "memory" )
#define atomic_read_write_barrier()		__asm__ __volatile__( "" : : : "memory" )

#elif( TARGET_CPU_MIPS )

#if 0
#pragma mark -
#pragma mark == MIPS ==
#endif

//===========================================================================================================================
//	MIPS
//===========================================================================================================================

int32_t					atomic_fetch_and_add_32( int32_t *inPtr, int32_t inVal );
STATIC_INLINE int32_t	atomic_fetch_and_sub_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_add_32( inPtr, -inVal ) ); }
int32_t					atomic_fetch_and_or_32(  int32_t *inPtr, int32_t inVal );
int32_t					atomic_fetch_and_and_32( int32_t *inPtr, int32_t inVal );
int32_t					atomic_fetch_and_xor_32( int32_t *inPtr, int32_t inVal );

STATIC_INLINE int32_t	atomic_add_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_add_32( inPtr,  inVal ) + inVal ); }
STATIC_INLINE int32_t	atomic_sub_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_add_32( inPtr, -inVal ) - inVal ); }
STATIC_INLINE int32_t	atomic_or_and_fetch_32(  int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_or_32(  inPtr,  inVal ) | inVal ); }
STATIC_INLINE int32_t	atomic_and_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_and_32( inPtr,  inVal ) & inVal ); }
STATIC_INLINE int32_t	atomic_xor_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_xor_32( inPtr,  inVal ) ^ inVal ); }

int32_t					atomic_fetch_and_store_32( int32_t *inPtr, int32_t inVal );
int32_t					atomic_val_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue );
STATIC_INLINE Boolean	atomic_bool_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	return( (Boolean)( atomic_val_compare_and_swap_32( inPtr, inOldValue, inNewValue ) == inOldValue ) );
}

#define atomic_read_barrier()			__asm__ __volatile__( "" : : : "memory" )
#define atomic_write_barrier()			__asm__ __volatile__( "" : : : "memory" )
#define atomic_read_write_barrier()		__asm__ __volatile__( "" : : : "memory" )

#elif( TARGET_OS_QNX )

#if 0
#pragma mark -
#pragma mark == QNX ==
#endif

//===========================================================================================================================
//	QNX
//===========================================================================================================================

STATIC_INLINE int32_t	atomic_fetch_and_add_32( int32_t *inPtr, int32_t inVal )
{
	if( inVal >= 0 )	return( (int32_t) atomic_add_value( (unsigned *) inPtr, (unsigned)  inVal ) );
	else				return( (int32_t) atomic_sub_value( (unsigned *) inPtr, (unsigned) -inVal ) );
}

STATIC_INLINE int32_t	atomic_fetch_and_sub_32( int32_t *inPtr, int32_t inVal )
{
	if( inVal >= 0 )	return( (int32_t) atomic_sub_value( (unsigned *) inPtr, (unsigned)  inVal ) );
	else				return( (int32_t) atomic_add_value( (unsigned *) inPtr, (unsigned) -inVal ) );
}

STATIC_INLINE int32_t	atomic_fetch_and_or_32(  int32_t *inPtr, int32_t inVal )
{
	return( (int32_t) atomic_set_value( (unsigned *) inPtr, (unsigned) inVal ) );
}

STATIC_INLINE int32_t	atomic_fetch_and_and_32( int32_t *inPtr, int32_t inVal )
{
	return( (int32_t) atomic_clr_value( (unsigned *) inPtr, ~( (unsigned) inVal ) ) );
}

STATIC_INLINE int32_t	atomic_fetch_and_xor_32( int32_t *inPtr, int32_t inVal )
{
	return( (int32_t) atomic_toggle_value( (unsigned *) inPtr, (unsigned) inVal ) );
}

STATIC_INLINE int32_t	atomic_add_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_add_32( inPtr,  inVal ) + inVal ); }
STATIC_INLINE int32_t	atomic_sub_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_add_32( inPtr, -inVal ) - inVal ); }
STATIC_INLINE int32_t	atomic_or_and_fetch_32(  int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_or_32(  inPtr,  inVal ) | inVal ); }
STATIC_INLINE int32_t	atomic_and_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_and_32( inPtr,  inVal ) & inVal ); }
STATIC_INLINE int32_t	atomic_xor_and_fetch_32( int32_t *inPtr, int32_t inVal )	{ return( atomic_fetch_and_xor_32( inPtr,  inVal ) ^ inVal ); }

STATIC_INLINE int32_t	atomic_fetch_and_store_32( int32_t *inPtr, int32_t inVal )
{
	return( (int32_t) _smp_xchg( (unsigned *) inPtr, (unsigned) inVal ) );
}

STATIC_INLINE int32_t	atomic_val_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	return( (int32_t) _smp_cmpxchg( (unsigned *) inPtr, (unsigned) inOldValue, (unsigned) inNewValue ) );
}

STATIC_INLINE Boolean	atomic_bool_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	return( (Boolean)( atomic_val_compare_and_swap_32( inPtr, inOldValue, inNewValue ) == inOldValue ) );
}

#define atomic_read_barrier()			__asm__ __volatile__( "" : : : "memory" )
#define atomic_write_barrier()			__asm__ __volatile__( "" : : : "memory" )
#define atomic_read_write_barrier()		__asm__ __volatile__( "" : : : "memory" )

#elif( TARGET_OS_THREADX )

#if 0
#pragma mark -
#pragma mark == ThreadX ==
#endif

//===========================================================================================================================
//	ThreadX
//===========================================================================================================================
	
check_compile_time( sizeof( long ) == sizeof( int32_t ) );

int32_t					atomic_fetch_and_add_32( int32_t *inPtr, int32_t inVal );
STATIC_INLINE int32_t	atomic_fetch_and_sub_32( int32_t *inPtr, int32_t inVal ) { return( atomic_fetch_and_add_32( inPtr, -inVal ) ); }
int32_t					atomic_fetch_and_or_32(  int32_t *inPtr, int32_t inVal );
int32_t					atomic_fetch_and_and_32( int32_t *inPtr, int32_t inVal );
int32_t					atomic_fetch_and_xor_32( int32_t *inPtr, int32_t inVal );

STATIC_INLINE int32_t	atomic_add_and_fetch_32( int32_t *inPtr, int32_t inVal ) { return( atomic_fetch_and_add_32( inPtr,  inVal ) + inVal ); }
STATIC_INLINE int32_t	atomic_sub_and_fetch_32( int32_t *inPtr, int32_t inVal ) { return( atomic_fetch_and_add_32( inPtr, -inVal ) - inVal ); }
STATIC_INLINE int32_t	atomic_or_and_fetch_32(  int32_t *inPtr, int32_t inVal ) { return( atomic_fetch_and_or_32(  inPtr,  inVal ) | inVal ); }
STATIC_INLINE int32_t	atomic_and_and_fetch_32( int32_t *inPtr, int32_t inVal ) { return( atomic_fetch_and_and_32( inPtr,  inVal ) & inVal ); }
STATIC_INLINE int32_t	atomic_xor_and_fetch_32( int32_t *inPtr, int32_t inVal ) { return( atomic_fetch_and_xor_32( inPtr,  inVal ) ^ inVal ); }

int32_t					atomic_fetch_and_store_32( int32_t *inPtr, int32_t inVal );
int32_t					atomic_val_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue );
STATIC_INLINE Boolean	atomic_bool_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	return( (Boolean)( atomic_val_compare_and_swap_32( inPtr, inOldValue, inNewValue ) == inOldValue ) );
}

#define atomic_read_barrier()			__asm__ __volatile__( "" : : : "memory" )
#define atomic_write_barrier()			__asm__ __volatile__( "" : : : "memory" )
#define atomic_read_write_barrier()		__asm__ __volatile__( "" : : : "memory" )
	
#endif

#if 0
#pragma mark -
#pragma mark == Generic ==
#endif

//===========================================================================================================================
//	Generic
//===========================================================================================================================

// Atomic APIs that don't return the result.

#if( !defined( atomic_add_32 ) )
	#define atomic_add_32( PTR, VAL )		( (void) atomic_add_and_fetch_32( (PTR), (VAL) ) )
#endif

#if( !defined( atomic_sub_32 ) )
	#define atomic_sub_32( PTR, VAL )		( (void) atomic_sub_and_fetch_32( (PTR), (VAL) ) )
#endif

#if( !defined( atomic_or_32 ) )
	#define atomic_or_32( PTR, VAL )		( (void) atomic_fetch_and_or_32(  (PTR), (VAL) ) )
#endif

#if( !defined( atomic_and_32 ) )
	#define atomic_and_32( PTR, VAL )		( (void) atomic_fetch_and_and_32( (PTR), (VAL) ) )
#endif

#if( !defined( atomic_xor_32 ) )
	#define atomic_xor_32( PTR, VAL )		( (void) atomic_fetch_and_xor_32( (PTR), (VAL) ) )
#endif

// atomic_yield -- Yield via software/thread.

#if( TARGET_OS_WINDOWS )
	#define atomic_yield()		Sleep( 0 )
#elif( TARGET_OS_DARWIN )
	#define atomic_yield()		pthread_yield_np()
#elif( TARGET_OS_POSIX )
	#define atomic_yield()		usleep( 1 )
#elif( TARGET_OS_THREADX )
	#define atomic_yield()		tx_thread_relinquish()
#endif

// atomic_hardware_yield -- Yield via hardware. WARNING: may not yield if on a higher priority thread.

#if( TARGET_CPU_X86 || TARGET_CPU_X86_64 )
	#if( COMPILER_GCC )
		#define atomic_hardware_yield()		__asm__ __volatile__( "pause" )
	#elif( COMPILER_VISUAL_CPP )
		#define atomic_hardware_yield()		YieldProcessor()
	#endif
#elif( COMPILER_ARM_REALVIEW ) // __yield() doesn't work correctly with RealView so use nop.
	#define atomic_hardware_yield()			__nop()
#elif( COMPILER_GCC )
	#define atomic_hardware_yield()			__asm__ __volatile__( "" )
#endif

// atomic_once

typedef int32_t	atomic_once_t;
typedef void ( *atomic_once_function_t )( void *inContext );

void	atomic_once_slow( atomic_once_t *inOnce, void *inContext, atomic_once_function_t inFunction );

#define atomic_once( ONCE, CONTEXT, FUNCTION ) \
	do \
	{ \
		if( __builtin_expect( *(ONCE), 2 ) != 2 ) \
		{ \
			atomic_once_slow( (ONCE), (CONTEXT), (FUNCTION) ); \
		} \
		\
	}	while( 0 )

// atomic_spinlock

typedef int32_t		atomic_spinlock_t;

#define atomic_spinlock_lock( SPIN_LOCK_PTR ) \
	do { while( !atomic_bool_compare_and_swap_32( (SPIN_LOCK_PTR), 0, 1 ) ) atomic_yield(); } while( 0 )

#define atomic_spinlock_unlock( SPIN_LOCK_PTR ) \
	do { atomic_read_write_barrier(); *(SPIN_LOCK_PTR) = 0; } while( 0 )

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AtomicUtils_Test
	@internal
	@abstract	Unit test.
*/

OSStatus	AtomicUtils_Test( void );

#ifdef __cplusplus
}
#endif

#endif // __AtomicUtils_h__
