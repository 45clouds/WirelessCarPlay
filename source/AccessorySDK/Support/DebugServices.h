/*
	File:    	DebugServices.h
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
	
	Copyright (C) 1997-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__DebugServices_h__
#define	__DebugServices_h__

#include "CommonServices.h"

#if( TARGET_OS_NETBSD && TARGET_KERNEL )
	#include <machine/stdarg.h>
#else
	#include <stdarg.h>
#endif

#if( TARGET_HAS_STD_C_LIB )
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
#endif

#if( COMPILER_OBJC )
	#import <Foundation/Foundation.h>
#endif

#if( GCD_ENABLED )
	#include LIBDISPATCH_HEADER
#endif

#if 0
#pragma mark == Debugging ==
#endif

#if 0
#pragma mark ==    Settings ==
#endif

// DEBUG_ENABLE_ASSERTS -- Override to enable asserts even in non-debug builds.

#if( !defined( DEBUG_ENABLE_ASSERTS ) )
	#define DEBUG_ENABLE_ASSERTS		0
#endif

// DEBUG_EXPORT_ERROR_STRINGS -- controls whether DebugGetErrorString is exported for non-debug builds.

#if( !defined( DEBUG_EXPORT_ERROR_STRINGS ) )
	#define	DEBUG_EXPORT_ERROR_STRINGS		0
#endif

// DEBUG_OVERRIDE_APPLE_MACROS -- AssertMacros.h/Debugging.h overrides.

#if( !defined( DEBUG_OVERRIDE_APPLE_MACROS ) )
	#define	DEBUG_OVERRIDE_APPLE_MACROS		1
#endif

// DEBUG_SERVICES_LITE -- If defined non-zero, build a minimal version of DebugServices for small devices.

#if( !defined( DEBUG_SERVICES_LITE ) )
	#if( TARGET_PLATFORM_WICED )
		#define DEBUG_SERVICES_LITE		1
	#else
		#define DEBUG_SERVICES_LITE		0
	#endif
#endif

#if 0
#pragma mark ==    Output ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_FPRINTF_ENABLED
	@abstract	Enables ANSI C fprintf output.
*/
#if( !defined( DEBUG_FPRINTF_ENABLED ) )
	#if( TARGET_HAS_C_LIB_IO )
		#define	DEBUG_FPRINTF_ENABLED			1
	#else
		#define	DEBUG_FPRINTF_ENABLED			0
	#endif
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_MAC_OS_X_IOLOG_ENABLED
	@abstract	Enables IOLog (Mac OS X Kernel) output.
*/
#if( !defined( DEBUG_MAC_OS_X_IOLOG_ENABLED ) )
	#define	DEBUG_MAC_OS_X_IOLOG_ENABLED		TARGET_OS_DARWIN_KERNEL
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_KPRINTF_ENABLED
	@abstract	Enables kprintf (Mac OS X Kernel) output.
*/
#if( !defined( DEBUG_KPRINTF_ENABLED ) )
	#define	DEBUG_KPRINTF_ENABLED				TARGET_OS_DARWIN_KERNEL
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_IDEBUG_ENABLED
	@abstract	Enables iDebug (Mac OS X user and Kernel) output.
	@discussion
	
	For Mac OS X kernel development, iDebug is enabled by default because we can dynamically check for the presence 
	of iDebug via some exported IOKit symbols. Mac OS X app usage doesn't allow dynamic detection because it relies
	on statically linking to the iDebugServices.cp file so for Mac OS X app usage, you have to manually enable iDebug.
*/
#if( !defined( DEBUG_IDEBUG_ENABLED ) )
	#define	DEBUG_IDEBUG_ENABLED				TARGET_OS_DARWIN_KERNEL
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_WINDOWS_EVENT_LOG_ENABLED
	@abstract	Enables Windows Event Log output.
	@discussion
	
	Windows Event Log support requires the advapi32 library, which may not be desirable, so this allows Event Log support
	to be removed entirely by disabling this.
*/
#if( !defined( DEBUG_WINDOWS_EVENT_LOG_ENABLED ) )
	#if( TARGET_OS_WINDOWS && !TARGET_OS_WINDOWS_CE )
		#define	DEBUG_WINDOWS_EVENT_LOG_ENABLED		1
	#else
		#define	DEBUG_WINDOWS_EVENT_LOG_ENABLED		0
	#endif
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_CF_OBJECTS_ENABLED
	@abstract	Controls Core Foundation objects are supported. Enabling requires Core Foundation framework/DLL.
*/
#if( !defined( DEBUG_CF_OBJECTS_ENABLED ) )
	#if( !TARGET_KERNEL && !TARGET_PLATFORM_WICED )
		#define	DEBUG_CF_OBJECTS_ENABLED		1
	#else
		#define	DEBUG_CF_OBJECTS_ENABLED		0
	#endif
#endif

#if( CFLITE_ENABLED || ( TARGET_OS_NETBSD && !TARGET_KERNEL ) || TARGET_OS_WINDOWS )
	#include CF_HEADER
#endif

#if 0
#pragma mark ==    Macros - General ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_UNUSED
	@abstract	Macro to mark a paramter as unused to avoid unused parameter warnings.
	@discussion
	
	There is no universally supported pragma/attribute for indicating a variable is unused. DEBUG_UNUSED lets us
	indicate a variable is unused in a manner that is supported by most compilers.
*/
#define	DEBUG_UNUSED( X )			(void)(X)

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_USE_ONLY
	@abstract	Macro to mark a variable as used only when debugging is enabled.
	@discussion
	
	Variables are sometimes needed only for debugging. When debugging is turned off, these debug-only variables generate 
	compiler warnings about unused variables. To eliminate these warnings, use these macros to indicate variables that 
	are only used for debugging.
*/
#if( DEBUG )
	#define	DEBUG_USE_ONLY( X )
#else
	#define	DEBUG_USE_ONLY( X )		(void)(X)
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_STATIC
	@abstract	Macro to make variables and functions static when debugging is off, but extern when debugging is on.
	@discussion
	
	Rather than using "static" directly, using this macro allows you to access these variables externally while 
	debugging without being penalized for production builds. It also allows you to get symbols in debug builds.
*/
#if( DEBUG )
	#define	DEBUG_STATIC
#else
	#define	DEBUG_STATIC	static
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_VIRTUAL
	@abstract	Macro to make a C++ member function virtual only when debugging is enabled.
	@discussion
	
	Example usage inside a class declaration:
	
		DEBUG_VIRTUAL void MyMethod( void );
*/
#if( defined( __cplusplus ) )
	#if( DEBUG )
		#define	DEBUG_VIRTUAL	virtual
	#else
		#define	DEBUG_VIRTUAL
	#endif
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_EXPORT_GLOBAL
	@abstract	Maps to EXPORT_GLOBAL when debugging is enabled, but maps to nothing when debugging is not enabled.
*/
#if( DEBUG )
	#define	DEBUG_EXPORT_GLOBAL		EXPORT_GLOBAL
#else
	#define	DEBUG_EXPORT_GLOBAL
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DEBUG_EXPORT
	@abstract	Macros to export symbols only in debug builds.
*/
#if( TARGET_OS_DARWIN_KERNEL )
	#define	DEBUG_EXPORT		__private_extern__
#else
	#define	DEBUG_EXPORT		extern
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	debug_add
	@abstract	Macro to add (or subtract if negative) a value when debugging is on. Does nothing if debugging is off.
*/
#if( DEBUG )
	#define	debug_add( A, B )		(A) += (B)
	#define	debug_sub( A, B )		(A) -= (B)
#else
	#define	debug_add( A, B )		do {} while( 0 )
	#define	debug_sub( A, B )		do {} while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	debug_perform
	@abstract	Macro to perform something in debug-only builds.
*/
#if( DEBUG )
	#define	debug_perform( X )		do { X; } while( 0 )
#else
	#define	debug_perform( X )		do {} while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	debug_track_min/debug_track_max
	@abstract	Tracks the minimum or maximum value seen.
*/
#if( DEBUG )
	#define debug_track_min( VAR, VALUE )	do { if( (VALUE) < (VAR) ) { VAR = (VALUE); } } while( 0 )
	#define debug_track_max( VAR, VALUE )	do { if( (VALUE) > (VAR) ) { VAR = (VALUE); } } while( 0 )
#else
	#define debug_track_min( VAR, VALUE )	do {} while( 0 )
	#define debug_track_max( VAR, VALUE )	do {} while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	debug_increment_wrap
	@abstract	Variant of increment_wrap for debug only.
*/
#if( DEBUG )
	#define debug_increment_wrap( VAR, WRAP )	increment_wrap( (VAR), (WRAP) )
#else
	#define debug_increment_wrap( VAR, WRAP )	do {} while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	debug_increment_saturate
	@abstract	Variant of increment_saturate for debug only.
*/
#if( DEBUG )
	#define debug_increment_saturate( VAR, MAX )	increment_saturate( (VAR), (MAX) )
#else
	#define debug_increment_saturate( VAR, MAX )	do {} while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	debug_add_saturate
	@abstract	Variant of add_saturate for debug only.
*/
#if( DEBUG )
	#define	debug_add_saturate( VAR, VALUE, MAX )	add_saturate( (VAR), (VALUE), (MAX) )
#else
	#define	debug_add_saturate( VAR, VALUE, MAX )	do {} while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	debug_true_conditional
	@abstract	Conditionalizes code using a variable for debug builds, but unconditionally true otherwise.
	@discussion
	
	Example:
	
		// Do something using a runtime conditional for debug builds, but unconditionally true with non-debug builds.
		
		if( debug_true_conditional( gMyVariableToTest ) )
		{
			// Do something
		}
*/
#if( DEBUG )
	#define debug_true_conditional( VAR )		(VAR)
#else
	#define debug_true_conditional( VAR )		1
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	debug_false_conditional
	@abstract	Conditionalizes code using a variable for debug builds, but unconditionally false otherwise.
	@discussion
	
	Example:
	
		// Do something using a runtime conditional for debug builds, but unconditionally false with non-debug builds.
		
		if( debug_false_conditional( gMyVariableToTest ) )
		{
			// Do something
		}
*/
#if( DEBUG )
	#define debug_false_conditional( VAR )		(VAR)
#else
	#define debug_false_conditional( VAR )		0
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	debug_tweakable_constant
	@abstract	Defines a "constant" that can be changed at runtime for debug builds, but is truly constant for ship builds.
	@discussion
	
	Example:
	
		debug_tweakable_constant( unsigned int, kMyConstant, 1234 );
		
		For debug builds, this expands to:
		
		unsigned int				kMyConstant = 1234;
		
		For ship builds, this expands to:
		
		static const unsigned int	kMyConstant = 1234;
*/
#if( DEBUG )
	#define	debug_tweakable_constant( TYPE, NAME, VALUE )		TYPE	NAME = (VALUE)
#else
	#define	debug_tweakable_constant( TYPE, NAME, VALUE )		static const TYPE	NAME = (VALUE)
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	debug_trace_*
	@abstract	Tracing macros to track the time spent between two points.
	@discussion
	
	debug_trace_init:		Initializes a trace. Must be in a place where a variable can be defined.
	debug_trace_init_begin: Initializes a trace and begins a trace. Must be in a place where a variable can be defined.
	debug_trace_begin:		Begins a trace that has already been initialized.
	debug_trace_end:		Ends a trace and records stats.
	
	Example:
		
		#if( DEBUG )
			uint64_t		gMyFunctionMinNanos;
			uint64_t		gMyFunctionMaxNanos;
		#endif
		
		void	MyFunction( void )
		{
			debug_trace_init_begin( uint64_t, nanos, ClockGetNanoseconds() );
			
			int		x;
			int		y;
			
			... do some time consuming stuff.
			
			debug_trace_end( nanos, ClockGetNanoseconds(), gMyFunctionMinNanos, gMyFunctionMaxNanos );
		}
*/
#if( DEBUG )
	#define debug_trace_init( TYPE, NAME )							TYPE	NAME
	#define debug_trace_init_begin( TYPE, NAME, FUNCTION )			TYPE	NAME = FUNCTION
	#define debug_trace_begin( NAME, FUNCTION )						NAME = FUNCTION
	#define debug_trace_end( NAME, FUNCTION, MIN_VAR, MAX_VAR )		\
		do															\
		{															\
			NAME = FUNCTION - NAME;									\
			debug_track_min( MIN_VAR, NAME );						\
			debug_track_max( MAX_VAR, NAME );						\
																	\
		}	while( 0 )
#else
	#define debug_trace_init( TYPE, NAME )
	#define debug_trace_init_begin( TYPE, NAME, FUNCTION )
	#define debug_trace_begin( NAME, FUNCTION )
	#define debug_trace_end( NAME, FUNCTION, MIN_VAR, MAX_VAR )
#endif

#if 0
#pragma mark == Macros - errno ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		GlobalErrnoMacros
	@abstract	Macros to help deal with errno.
	@discussion
	
	EXAMPLE (map_global_noerr_errno):
	
		OSStatus		err;
		
		err = gethostname( name, sizeof( name ) );
		err = map_global_noerr_errno( err );
		require_noerr( err, exit );
	
	EXAMPLE (map_global_value_errno):
	
		OSStatus		err;
		int				n;
		
		n = select( maxFD + 1, &readSet, &writeSet, NULL, timeoutPtr );
		err = map_global_value_errno( n >= 0, n );
		require_noerr( err, exit );
*/
#define map_fd_creation_errno( FD )					( IsValidFD( FD ) ? 0 : global_value_errno( FD ) )
#define map_global_noerr_errno( ERR )				( !(ERR) ? 0 : global_value_errno(ERR) )
#define map_global_value_errno( TEST, VALUE )		( (TEST) ? 0 : global_value_errno(VALUE) )

#if( TARGET_HAS_STD_C_LIB && !TARGET_OS_THREADX )
	#define map_noerr_errno( ERR )					map_global_noerr_errno( (ERR) )
	
	#define global_value_errno( VALUE )				( errno_compat() ? errno_compat() : kUnknownErr )
#else
	#define global_value_errno( VALUE )				( (VALUE) ? ( (OSStatus)(VALUE) ) : kUnknownErr )
#endif

#define kqueue_errno( N )		( ( (N) > 0 ) ? kNoErr : ( (N) == 0 ) ? kTimeoutErr : global_value_errno( (N) ) )
#define poll_errno( N )			( ( (N) > 0 ) ? kNoErr : ( (N) == 0 ) ? kTimeoutErr : global_value_errno( (N) ) )
#define select_errno( N )		( ( (N) > 0 ) ? kNoErr : ( (N) == 0 ) ? kTimeoutErr : global_value_errno( (N) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		SocketErrnoMacros
	@abstract	Macros to help deal with errno for sockets.
	@discussion
	
	EXAMPLE (map_socket_creation_errno):
	
		OSStatus		err;
		SocketRef		sock;
		
		sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
		err = map_socket_creation_errno( sock );
		require_noerr( err, exit );
	
	EXAMPLE (map_socket_noerr_errno):
		
		OSStatus		err;
		
		err = shutdown( sock, SHUT_WR );
		err = map_socket_noerr_errno( sock, err );
		require_noerr_quiet( err, exit );
	
	EXAMPLE (map_socket_value_errno):
	
		OSStatus		err;
		ssize_t			n;
		
		n = send( sock, "q", 1, 0 );
		err = map_socket_value_errno( sock, n == 1, n );
		require_noerr( err, exit );
*/
#define map_socket_creation_errno( SOCK )				( IsValidSocket( SOCK ) ? 0 : global_value_errno( SOCK ) )
#define map_socket_noerr_errno( SOCK, ERR )				( !(ERR) ? 0 : socket_errno( (SOCK) ) )
#define map_socket_value_errno( SOCK, TEST, VALUE )		( (TEST) ? 0 : socket_value_errno( (SOCK), (VALUE) ) )

#define socket_value_errno( SOCK, VALUE )				socket_errno( SOCK )

#if( TARGET_OS_THREADX )
	#define socket_errno( SOCK )		( t_errno( (SOCK) ) ? t_errno( (SOCK) ) : kUnknownErr )
#else	
	#define socket_errno( SOCK )		( errno_compat() ? errno_compat() : kUnknownErr )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	map_efi_error
	@abstract	Maps an EFI error code to an OSStatus.
	@discussion
	
	EFI uses different values for error codes on 32-bit vs 64-bit because an EFI error code is made up of the high 
	bit OR'd into an error number. This means on 32 bit, EFI_UNSUPPORTED would be 0x80000003, but on 64 bit, it 
	would be 0x8000000000000003. To make this a little more sane, this macro normalizes the error codes into the 
	32-bit range so EFI_UNSUPPORTED maps to 0x80000003 on both 32 bit and 64 bit.
*/
#if( TARGET_RT_64_BIT )
	#define map_efi_error( X )	\
		( (int32_t)( ( ( ( (uint64_t)(X) ) >> 32 ) & UINT32_C( 0x80000000 ) ) | ( (X) & UINT32_C( 0xFFFFFFFF ) ) ) )
#else
	#define map_efi_error( X )		( (int32_t)(X) )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	map_hresult
	@abstract	Returns 0 if SUCCEEDED. If FAILED, returns the HRESULT if non-zero and othewise returns kUnknownErr.
*/
#define map_hresult( HR )		( SUCCEEDED( (HR) ) ? 0 : (HR) ? (int)(HR) : kUnknownErr )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	map_scerror
	@abstract	If TEST is true, returns 0.
				If TEST is false and SystemConfiguration framework's SCError() is non-zero, returns SCError().
				If TEST is false and SystemConfiguration framework's SCError() is zero, returns kUnknownErr.
*/
#define map_scerror( TEST )		( (TEST) ? 0 : SCError() ? SCError() : kUnknownErr )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NEW
	@abstract	Allocates an object or returns NULL if it could not be allocated.
*/
#if( !defined( NEW ) && defined( __cplusplus ) )
	#define NEW		new( std::nothrow )
#endif

#if 0
#pragma mark ==    Macros - Checks ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	check
	@abstract	Check that an expression is true (non-zero).
	@discussion
	
	If expression evalulates to false, this prints debugging information (actual expression string, file, line number, 
	function name, etc.) using the default debugging output method.
	
	Code inside check() statements is not compiled into production builds.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef check
#endif
#if( !defined( check ) )
	#if( DEBUG )
		#define	check( X )																					\
			do 																								\
			{																								\
				if( unlikely( !(X) ) ) 																		\
				{																							\
					debug_print_assert( 0, #X, NULL, __FILE__, __LINE__, __ROUTINE__ );						\
				}																							\
																											\
			}	while( 0 )
	#else
		#define	check( X )
	#endif
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	check_string
	@abstract	Check that an expression is true (non-zero) with an explanation.
	@discussion
	
	If expression evalulates to false, this prints debugging information (actual expression string, file, line number, 
	function name, etc.) and a custom explanation string using the default debugging output method.
	
	Code inside check_string() statements is not compiled into production builds.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef check_string
#endif
#if( !defined( check_string ) )
	#if( DEBUG )
		#define	check_string( X, STR )																		\
			do 																								\
			{																								\
				if( unlikely( !(X) ) ) 																		\
				{																							\
					debug_print_assert( 0, #X, STR, __FILE__, __LINE__, __ROUTINE__ );						\
				}																							\
																											\
			}	while( 0 )
	#else
		#define	check_string( X, STR )
	#endif
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	check_noerr
	@abstract	Check that an error code is noErr (0).
	@discussion
	
	If the error code is non-0, this prints debugging information (actual expression string, file, line number, 
	function name, etc.) using the default debugging output method.
	
	Code inside check_noerr() statements is not compiled into production builds.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef check_noerr
#endif
#if( !defined( check_noerr ) )
	#if( DEBUG )
		#define	check_noerr( ERR )																			\
			do 																								\
			{																								\
				OSStatus		localErr;																	\
																											\
				localErr = (OSStatus)(ERR);																	\
				if( unlikely( localErr != 0 ) ) 															\
				{																							\
					debug_print_assert( localErr, NULL, NULL, __FILE__, __LINE__, __ROUTINE__ );			\
				}																							\
																											\
			}	while( 0 )
	#else
		#define	check_noerr( ERR )
	#endif
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	check_noerr_string
	@abstract	Check that an error code is noErr (0) with an explanation.
	@discussion
	
	If the error code is non-0, this prints debugging information (actual expression string, file, line number, 
	function name, etc.) and a custom explanation string using the default debugging output method.
	
	Code inside check_noerr_string() statements is not compiled into production builds.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef check_noerr_string
#endif
#if( !defined( check_noerr_string ) )
	#if( DEBUG )
		#define	check_noerr_string( ERR, STR )																\
			do 																								\
			{																								\
				OSStatus		localErr;																	\
																											\
				localErr = (OSStatus)(ERR);																	\
				if( unlikely( localErr != 0 ) ) 															\
				{																							\
					debug_print_assert( localErr, NULL, STR, __FILE__, __LINE__, __ROUTINE__ );				\
				}																							\
																											\
			}	while( 0 )
	#else
		#define	check_noerr_string( ERR, STR )
	#endif
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	check_ptr_overlap
	@abstract	Checks that two ptrs do not overlap.
*/
#define	check_ptr_overlap( P1, P1_SIZE, P2, P2_SIZE )									\
	do																					\
	{																					\
		check( !( ( (uintptr_t)(P1) >=     (uintptr_t)(P2) ) && 						\
				  ( (uintptr_t)(P1) <  ( ( (uintptr_t)(P2) ) + (P2_SIZE) ) ) ) );		\
		check( !( ( (uintptr_t)(P2) >=     (uintptr_t)(P1) ) && 						\
				  ( (uintptr_t)(P2) <  ( ( (uintptr_t)(P1) ) + (P1_SIZE) ) ) ) );		\
																						\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	check_ptr_bounds
	@abstract	Checks that a ptr/size doesn't go outside the bounds of its enclosing buffer.
	@discussion
	
	This can used to check that a read or write from a ptr won't access outside the buffer bounds. For example:
	
	uint8_t			buf[ 1024 ];
	uint8_t *		ptr;
	size_t			size;
	
	ptr  = ... somewhere hopefully within buf
	size = ... some size
	
	check_ptr_bounds( buf, sizeof( buf ), ptr, size );
*/
#if( DEBUG )
	#define	check_ptr_bounds( BUF_BASE, BUF_SIZE, PTR, SIZE )												\
		do																									\
		{																									\
			check( (   (uintptr_t)(PTR) )				>= (   (uintptr_t)(BUF_BASE) ) );					\
			check( (   (uintptr_t)(PTR) )				<= ( ( (uintptr_t)(BUF_BASE) ) + (BUF_SIZE) ) );	\
			check( ( ( (uintptr_t)(PTR) ) + (SIZE) )	>= (   (uintptr_t)(BUF_BASE) ) );					\
			check( ( ( (uintptr_t)(PTR) ) + (SIZE) )	<= ( ( (uintptr_t)(BUF_BASE) ) + (BUF_SIZE) ) );	\
																											\
		}	while( 0 )
#else
	#define	check_ptr_bounds( BUF_BASE, BUF_SIZE, PTR, SIZE )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	check_panic
	@abstract	Debug-only check if an expression is non-zero and panics if it is not.
*/
#if( !defined( check_panic ) )
	#if( DEBUG )
		#define	check_panic( X, STR )															\
			do 																					\
			{																					\
				if( unlikely( !(X) ) ) 															\
				{																				\
					debug_print_assert_panic( 0, #X, STR, __FILE__, __LINE__, __ROUTINE__ );	\
				}																				\
																								\
			}	while( 0 )
	#else
		#define	check_panic( X, STR )
	#endif
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	check_noerr_panic
	@abstract	Debug-only check if an error is 0 and panics if it is not.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef check_noerr_panic
#endif
#if( !defined( check_noerr_panic ) )
	#if( DEBUG )
		#define	check_noerr_panic( ERR, STR )																\
			do 																								\
			{																								\
				OSStatus		localErr;																	\
																											\
				localErr = (OSStatus)(ERR);																	\
				if( unlikely( localErr != 0 ) ) 															\
				{																							\
					debug_print_assert_panic( localErr, NULL, STR, __FILE__, __LINE__, __ROUTINE__ );		\
				}																							\
																											\
			}	while( 0 )
	#else
		#define	check_noerr_panic( ERR, STR )
	#endif
#endif

#if 0
#pragma mark ==    Macros - Requires ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require
	@abstract	Requires that an expression evaluate to true.
	@discussion
	
	If expression evalulates to false, this prints debugging information (actual expression string, file, line number, 
	function name, etc.) using the default debugging output method then jumps to a label.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require
#endif
#if( !defined( require ) )
	#define	require( X, LABEL )																				\
		do 																									\
		{																									\
			if( unlikely( !(X) ) ) 																			\
			{																								\
				debug_print_assert( 0, #X, NULL, __FILE__, __LINE__, __ROUTINE__ );							\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require_string
	@abstract	Requires that an expression evaluate to true with an explanation.
	@discussion
	
	If expression evalulates to false, this prints debugging information (actual expression string, file, line number, 
	function name, etc.) and a custom explanation string using the default debugging output method then jumps to a label.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require_string
#endif
#if( !defined( require_string ) )
	#define	require_string( X, LABEL, STR )																	\
		do 																									\
		{																									\
			if( unlikely( !(X) ) ) 																			\
			{																								\
				debug_print_assert( 0, #X, STR, __FILE__, __LINE__, __ROUTINE__ );							\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require_quiet
	@abstract	Requires that an expression evaluate to true.
	@discussion
	
	If expression evalulates to false, this jumps to a label. No debugging information is printed.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require_quiet
#endif
#if( !defined( require_quiet ) )
	#define	require_quiet( X, LABEL )																		\
		do 																									\
		{																									\
			if( unlikely( !(X) ) )																			\
			{																								\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require_noerr
	@abstract	Require that an error code is noErr (0).
	@discussion
	
	If the error code is non-0, this prints debugging information (actual expression string, file, line number, 
	function name, etc.) using the default debugging output method then jumps to a label.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require_noerr
#endif
#if( !defined( require_noerr ) )
	#define	require_noerr( ERR, LABEL )																		\
		do 																									\
		{																									\
			OSStatus		localErr;																		\
																											\
			localErr = (OSStatus)(ERR);																		\
			if( unlikely( localErr != 0 ) ) 																\
			{																								\
				debug_print_assert( localErr, NULL, NULL, __FILE__, __LINE__, __ROUTINE__ );				\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require_noerr_string
	@abstract	Require that an error code is noErr (0).
	@discussion
	
	If the error code is non-0, this prints debugging information (actual expression string, file, line number, 
	function name, etc.), and a custom explanation string using the default debugging output method using the 
	default debugging output method then jumps to a label.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require_noerr_string
#endif
#if( !defined( require_noerr_string ) )
	#define	require_noerr_string( ERR, LABEL, STR )															\
		do 																									\
		{																									\
			OSStatus		localErr;																		\
																											\
			localErr = (OSStatus)(ERR);																		\
			if( unlikely( localErr != 0 ) ) 																\
			{																								\
				debug_print_assert( localErr, NULL, STR, __FILE__, __LINE__, __ROUTINE__ );					\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require_noerr_action_string
	@abstract	Require that an error code is noErr (0).
	@discussion
	
	If the error code is non-0, this prints debugging information (actual expression string, file, line number, 
	function name, etc.), and a custom explanation string using the default debugging output method using the 
	default debugging output method then executes an action and jumps to a label.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require_noerr_action_string
#endif
#if( !defined( require_noerr_action_string ) )
	#define	require_noerr_action_string( ERR, LABEL, ACTION, STR )											\
		do 																									\
		{																									\
			OSStatus		localErr;																		\
																											\
			localErr = (OSStatus)(ERR);																		\
			if( unlikely( localErr != 0 ) ) 																\
			{																								\
				debug_print_assert( localErr, NULL, STR, __FILE__, __LINE__, __ROUTINE__ );					\
				{ ACTION; }																					\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require_noerr_quiet
	@abstract	Require that an error code is noErr (0).
	@discussion
	
	If the error code is non-0, this jumps to a label. No debugging information is printed.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require_noerr_quiet
#endif
#if( !defined( require_noerr_quiet ) )
	#define	require_noerr_quiet( ERR, LABEL )																\
		do 																									\
		{																									\
			if( unlikely( (ERR) != 0 ) ) 																	\
			{																								\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require_noerr_action
	@abstract	Require that an error code is noErr (0) with an action to execute otherwise.
	@discussion
	
	If the error code is non-0, this prints debugging information (actual expression string, file, line number, 
	function name, etc.) using the default debugging output method then executes an action and jumps to a label.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require_noerr_action
#endif
#if( !defined( require_noerr_action ) )
	#define	require_noerr_action( ERR, LABEL, ACTION )														\
		do 																									\
		{																									\
			OSStatus		localErr;																		\
																											\
			localErr = (OSStatus)(ERR);																		\
			if( unlikely( localErr != 0 ) ) 																\
			{																								\
				debug_print_assert( localErr, NULL, NULL, __FILE__, __LINE__, __ROUTINE__ );				\
				{ ACTION; }																					\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require_noerr_action_quiet
	@abstract	Require that an error code is noErr (0) with an action to execute otherwise.
	@discussion
	
	If the error code is non-0, this executes an action and jumps to a label. No debugging information is printed.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require_noerr_action_quiet
#endif
#if( !defined( require_noerr_action_quiet ) )
	#define	require_noerr_action_quiet( ERR, LABEL, ACTION )												\
		do 																									\
		{																									\
			if( unlikely( (ERR) != 0 ) ) 																	\
			{																								\
				{ ACTION; }																					\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require_action
	@abstract	Requires that an expression evaluate to true with an action to execute otherwise.
	@discussion
	
	If expression evalulates to false, this prints debugging information (actual expression string, file, line number, 
	function name, etc.) using the default debugging output method then executes an action and jumps to a label.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require_action
#endif
#if( !defined( require_action ) )
	#define	require_action( X, LABEL, ACTION )																\
		do 																									\
		{																									\
			if( unlikely( !(X) ) ) 																			\
			{																								\
				debug_print_assert( 0, #X, NULL, __FILE__, __LINE__, __ROUTINE__ );							\
				{ ACTION; }																					\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require_action_string
	@abstract	Requires that an expression evaluate to true with an explanation and action to execute otherwise.
	@discussion
	
	If expression evalulates to false, this prints debugging information (actual expression string, file, line number, 
	function name, etc.) and a custom explanation string using the default debugging output method then executes an
	action and jumps to a label.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require_action_string
#endif
#if( !defined( require_action_string ) )
	#define	require_action_string( X, LABEL, ACTION, STR )													\
		do 																									\
		{																									\
			if( unlikely( !(X) ) ) 																			\
			{																								\
				debug_print_assert( 0, #X, STR, __FILE__, __LINE__, __ROUTINE__ );							\
				{ ACTION; }																					\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	require_action_quiet
	@abstract	Requires that an expression evaluate to true with an action to execute otherwise.
	@discussion
	
	If expression evalulates to false, this executes an action and jumps to a label. No debugging information is printed.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef require_action_quiet
#endif
#if( !defined( require_action_quiet ) )
	#define	require_action_quiet( X, LABEL, ACTION )														\
		do 																									\
		{																									\
			if( unlikely( !(X) ) ) 																			\
			{																								\
				{ ACTION; }																					\
				goto LABEL;																					\
			}																								\
																											\
		}	while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	debug_string
	@abstract	Prints a debugging C string.
*/
#if( DEBUG_OVERRIDE_APPLE_MACROS )
	#undef debug_string
#endif
#if( !defined( debug_string ) )
	#if( DEBUG )
		#define	debug_string( STR )																		\
			do 																							\
			{																							\
				debug_print_assert( 0, NULL, STR, __FILE__, __LINE__, __ROUTINE__ );					\
																										\
			}	while( 0 )
	#else
		#define	debug_string( STR )
	#endif
#endif

#if 0
#pragma mark ==    Macros - Expect ==
#endif

//===========================================================================================================================
//	Expect macros
//===========================================================================================================================

// Expect macros allow code to include runtime checking of things that should not happen in shipping code (e.g. internal 
// programmer errors, such as a NULL parameter where it is not allowed). Once the code has been verified to work correctly 
// without asserting, the DEBUG_EXPECT_VERIFIED conditional can be set to eliminate the error checking entirely. It can 
// also be useful to measure the cost of error checking code by profiling with it enabled and with it disabled.

#if( !defined( DEBUG_EXPECT_VERIFIED ) )
	#define DEBUG_EXPECT_VERIFIED		0
#endif
#if( DEBUG_EXPECT_VERIFIED )
	#define	require_expect
	#define	require_string_expect
	#define	require_quiet_expect
	#define	require_noerr_expect
	#define	require_noerr_string_expect
	#define	require_noerr_action_string_expect
	#define	require_noerr_quiet_expect
	#define	require_noerr_action_expect
	#define	require_noerr_action_quiet_expect
	#define	require_action_expect
	#define	require_action_quiet_expect
	#define	require_action_string_expect
#else
	#define	require_expect							require
	#define	require_string_expect					require_string
	#define	require_quiet_expect					require_quiet
	#define	require_noerr_expect					require_noerr
	#define	require_noerr_string_expect				require_noerr_string
	#define	require_noerr_action_string_expect		require_noerr_action_string
	#define	require_noerr_quiet_expect				require_noerr_quiet
	#define	require_noerr_action_expect				require_noerr_action
	#define	require_noerr_action_quiet_expect		require_noerr_action_quiet
	#define	require_action_expect					require_action
	#define	require_action_quiet_expect				require_action_quiet
	#define	require_action_string_expect			require_action_string
#endif

#if 0
#pragma mark ==    Macros - Debug Output ==
#endif

// dlog categories

#if( DEBUG )
	#define	dlog_control( NAME )								LogControl( (NAME) )
	#define dlog_define( NAME, LEVEL, FLAGS, PREFIX, CONFIG )	ulog_define( NAME, LEVEL, FLAGS, PREFIX, CONFIG )
	#define	dlog_extern( NAME )									ulog_extern( NAME )
#else
	#define	dlog_control( NAME )								do {} while( 0 )
	#define dlog_define( NAME, LEVEL, FLAGS, PREFIX, CONFIG )	extern int debug_empty_extern
	#define	dlog_extern( NAME )									extern int debug_empty_extern
#endif

// dlog

#if( !TARGET_HAS_VA_ARG_MACROS )
	// No VA_ARG macros so we have to do it from a real function.
	
	int	DebugLog_C89( const char *inFunction, LogLevel inLevel, const char *inFormat, ... );
#endif
#if( DEBUG )
	#if( TARGET_HAS_C99_VA_ARGS )
		#if( DEBUG_SERVICES_LITE )
			extern LogLevel		gDebugServicesLevel;
			
			#define	dlog( LEVEL, ... ) \
				do \
				{ \
					if( unlikely( ( (LEVEL) & kLogLevelMask ) >= gDebugServicesLevel ) ) \
					{ \
						printf( __VA_ARGS__ ); \
					} \
					\
				}	while( 0 )
		#else
			#define	dlog( LEVEL, ... )	ulog( &log_category_from_name( DebugServicesLogging ), (LEVEL), __VA_ARGS__ )
		#endif
	#elif( TARGET_HAS_GNU_VA_ARGS )
		#define	dlog( ARGS... )			ulog( &log_category_from_name( DebugServicesLogging ), ## ARGS )
	#else
		#define dlog					DebugLog_C89
	#endif
	
	#define dlogv( LEVEL, FORMAT, ARGS )	\
		ulogv( &log_category_from_name( DebugServicesLogging ), (LEVEL), (FORMAT), (ARGS) )
#else
	#if( TARGET_HAS_C99_VA_ARGS )
		#define	dlog( LEVEL, ... )			do {} while( 0 )
	#elif( TARGET_HAS_GNU_VA_ARGS )
		#define	dlog( ARGS... )				do {} while( 0 )
	#else
		#define	dlog						while( 0 )
	#endif
	
	#define dlogv( LEVEL, FORMAT, ARGS )	do {} while( 0 )
#endif

// dlogc

#if( DEBUG )
	#define dlogc		ulog
#else
	#if( TARGET_HAS_C99_VA_ARGS )
		#define dlogc( CATEGORY_PTR, LEVEL, ... )		do {} while( 0 )
	#elif( TARGET_HAS_GNU_VA_ARGS )
		#define dlogc( CATEGORY_PTR, LEVEL, ARGS... )	do {} while( 0 )
	#else
		#define	dlogc									while( 0 )
	#endif
#endif

// dlogcv

#if( DEBUG )
	#define dlogcv		ulogv
#else
	#define dlogcv( CATEGORY_PTR, LEVEL, FORMAT, ARGS )		do {} while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	dlogassert
	@abstract	Prints a debug-only assert.
*/
#if( DEBUG )
	#if( TARGET_HAS_C99_VA_ARGS )
		#define	dlogassert( ... )	\
			DebugPrintAssert( kDebugAssertFlagsNone, kNoErr, NULL, __FILE__, __LINE__, __ROUTINE__, __VA_ARGS__ )
	#elif( TARGET_HAS_GNU_VA_ARGS )
		#define	dlogassert( ARGS... )	\
			DebugPrintAssert( kDebugAssertFlagsNone, kNoErr, NULL, __FILE__, __LINE__, __ROUTINE__, ## ARGS )
	#else
		#define	dlogassert				while( 0 )
	#endif
#else
	#if( TARGET_HAS_C99_VA_ARGS )
		#define	dlogassert( ... )		do {} while( 0 )
	#elif( TARGET_HAS_GNU_VA_ARGS )
		#define	dlogassert( ARGS... )	do {} while( 0 )
	#else
		#define	dlogassert				while( 0 )
	#endif
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	DebugNSLog
	@abstract	Debug-only macro for the Cocoa NSLog function.
*/
#if( DEBUG )
	#if( TARGET_HAS_C99_VA_ARGS )
		#define	DebugNSLog( ... )			NSLog( __VA_ARGS__ )
	#elif( TARGET_HAS_GNU_VA_ARGS )
		#define	DebugNSLog( ARGS... )		NSLog( ## ARGS )
	#else
		#define	DebugNSLog					NSLog
	#endif
#else
	#if( TARGET_HAS_C99_VA_ARGS )
		#define	DebugNSLog( ... )			do {} while( 0 )
	#elif( TARGET_HAS_GNU_VA_ARGS )
		#define	DebugNSLog( ARGS... )		do {} while( 0 )
	#else
		#define	DebugNSLog					while( 0 )
	#endif
#endif

#if 0
#pragma mark ==    Routines - General ==
#endif

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DEBUG_HALT
	@abstract	Halts the program.
*/
#if( TARGET_CPU_PPC || TARGET_CPU_PPC64 )
	#define DEBUG_HALT()		__asm__ __volatile__( "trap" )
#elif( TARGET_CPU_X86 || TARGET_CPU_X86_64 )
	#if( defined( __GNUC__ ) )
		#define DEBUG_HALT()	__asm__ __volatile__( "int3" )
	#elif( COMPILER_VISUAL_CPP )
		#define DEBUG_HALT()	__debugbreak()
	#elif( defined( __MWERKS__ ) )
		#define DEBUG_HALT()	DebugBreak()
	#else
		#warning "unknown x86 compiler...using infinite loop for DEBUG_HALT()"
		#define DEBUG_HALT()	do { for( ;; ) {} } while( 0 )
	#endif
#elif( TARGET_CPU_ARM )
	#define DEBUG_HALT()		__asm__ __volatile__( "bkpt 0xCF" )
#elif( TARGET_CPU_MIPS )
	#define DEBUG_HALT()		__asm__ __volatile__( "break" )
#elif( COMPILER_CLANG || ( COMPILER_GCC >= 30300 ) ) // GCC 3.3 or later has __builtin_trap.
	#define DEBUG_HALT()		__builtin_trap()
#else
	#warning "unknown architecture...using infinite loop for DEBUG_HALT()"
	#define DEBUG_HALT()		do { for( ;; ) {} } while( 0 )
#endif

#if 0
#pragma mark ==    Routines - Output ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	DebugAssertFlags
	@abstract	Flags to control an assert.
*/
typedef uint32_t	DebugAssertFlags;

#define	kDebugAssertFlagsNone		0			//! No flags.
#define	kDebugAssertFlagsPanic		( 1 << 0 )	//! Panic Assert: Print the assert, print a stack trace, then stop.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DebugPrintAssert
	@abstract	Prints a message describing the reason the (e.g. an assert failed), an optional error message, 
				an optional source filename, an optional source line number.

	@param		inErrorCode			Error that generated this assert or 0 if not error-code based.
	@param		inAssertString		C string containing assertion text.
	@param		inFilename			C string containing path of file where the error occurred.
	@param		inLineNumber		Line number in source file where the error occurred.
	@param		inFunction			C string containing name of function where assert occurred.
	@param		inMessageFormat		printf-style format string for the message or NULL if there is no message.
	param		VA_ARGS				Variable argument list for the message (if the message is non-NULL).

	@discussion
	
	Example output:	
	
	[ASSERT] "dataPtr != NULL" allocate memory for object failed
	[ASSERT] "MyFile.c", line 123, ("MyFunction")
	
	OR
	
	[ASSERT] -6728 (kNoMemoryErr)
	[ASSERT] "MyFile.c", line 123, ("MyFunction")
*/
void
	DebugPrintAssert( 
		DebugAssertFlags	inFlags, 
		OSStatus			inErrorCode, 
		const char *		inAssertString, 
		const char *		inFilename, 
		long				inLineNumber, 
		const char *		inFunction, 
		const char *		inMessageFormat, 
		... ) PRINTF_STYLE_FUNCTION( 7, 8 ) STATIC_ANALYZER_NORETURN;

#if( DEBUG || DEBUG_ENABLE_ASSERTS )
	#define	debug_print_assert( ERROR_CODE, ASSERT_STRING, MESSAGE, FILENAME, LINE_NUMBER, FUNCTION )			\
		DebugPrintAssert( kDebugAssertFlagsNone, (ERROR_CODE), (ASSERT_STRING), (FILENAME),						\
			(LINE_NUMBER), (FUNCTION), (MESSAGE) )
	
	#define	debug_print_assert_panic( ERROR_CODE, ASSERT_STRING, MESSAGE, FILENAME, LINE_NUMBER, FUNCTION )		\
		DebugPrintAssert( kDebugAssertFlagsPanic, (ERROR_CODE), (ASSERT_STRING), (FILENAME),					\
			(LINE_NUMBER), (FUNCTION), (MESSAGE) )
#else
	#define	debug_print_assert( ERROR_CODE, ASSERT_STRING, MESSAGE, FILENAME, LINE_NUMBER, FUNCTION )		do {} while( 0 )
	#define	debug_print_assert_panic( ERROR_CODE, ASSERT_STRING, MESSAGE, FILENAME, LINE_NUMBER, FUNCTION )	do {} while( 0 )
#endif

#if 0
#pragma mark ==    Routines - Utilities ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DebugGetErrorString
	@abstract	Gets an error string from an error code.

	@param		inStatus		Error code to get the string for.
	@param		inBuffer		Optional buffer to copy the string to for non-static strings. May be null.
	@param		inBufferSize	Size of optional buffer (including space for the null terminator).
	
	@result		C string containing error string for the error code. Guaranteed to be a valid, static string. If a 
				buffer is supplied, the return value will always be a pointer to the supplied buffer, which will 
				contain the best available description of the error code. If a buffer is not supplied, the return
				value will be the best available description of the error code that can be represented as a static 
				string. This allows code that cannot use a temporary buffer to hold the result to still get a useful 
				error string in most cases, but also allows code that can use a temporary buffer to get the best 
				available description.
*/
const char *	DebugGetErrorString( OSStatus inErrorCode, char *inBuffer, size_t inBufferSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DebugGetNextError
	@abstract	Gets the next error code in the list of known error codes.

	@param		inIndex		Index of error code. Start at 0 and increment until this function returns an error.
	@param		outErr		Receives error code.
*/
OSStatus	DebugGetNextError( size_t inIndex, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DebugStackTrace
	@abstract	Prints out a stack trace to the output device.
*/
char *		DebugCopyStackTrace( OSStatus *outErr );
OSStatus	DebugStackTrace( LogLevel inLevel );

#if( DEBUG )
	#define	debug_stack_trace( LEVEL )	DebugStackTrace( (LEVEL) )
#else
	#define	debug_stack_trace( LEVEL )	do {} while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DebugValidPtr
	@abstract	Returns 1 if the pointer/size can be safely accessed. Otherwise, it returns 0.
*/
int	DebugValidPtr( uintptr_t inPtr, size_t inSize, int inRead, int inWrite, int inExecute );

#if( DEBUG )
	#define	debug_valid_ptr_r( PTR, SIZE )		DebugValidPtr( (uintptr_t)(PTR), (SIZE), 1, 0, 0 )
	#define	debug_valid_ptr_w( PTR, SIZE )		DebugValidPtr( (uintptr_t)(PTR), (SIZE), 0, 1, 0 )
	#define	debug_valid_ptr_e( PTR, SIZE )		DebugValidPtr( (uintptr_t)(PTR), (SIZE), 0, 0, 1 )
	#define	debug_valid_ptr_rw( PTR, SIZE )		DebugValidPtr( (uintptr_t)(PTR), (SIZE), 1, 1, 0 )
	#define	debug_valid_ptr_re( PTR, SIZE )		DebugValidPtr( (uintptr_t)(PTR), (SIZE), 1, 0, 1 )
	#define	debug_valid_ptr_we( PTR, SIZE )		DebugValidPtr( (uintptr_t)(PTR), (SIZE), 0, 1, 1 )
	#define	debug_valid_ptr_rwe( PTR, SIZE )	DebugValidPtr( (uintptr_t)(PTR), (SIZE), 1, 1, 1 )
#else
	#define	debug_valid_ptr_r( PTR, SIZE )		1
	#define	debug_valid_ptr_w( PTR, SIZE )		1
	#define	debug_valid_ptr_e( PTR, SIZE )		1
	#define	debug_valid_ptr_rw( PTR, SIZE )		1
	#define	debug_valid_ptr_re( PTR, SIZE )		1
	#define	debug_valid_ptr_we( PTR, SIZE )		1
	#define	debug_valid_ptr_rwe( PTR, SIZE )	1
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DebugIsDebuggerPresent
	@abstract	Returns true if a debugger is detected.
*/
int	DebugIsDebuggerPresent( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DebugEnterDebugger
	@abstract	Tries to enter the debugger.
	
	@param		inForce		If the system has a runtime facility for turning off debugger breaks, this control if that
							facility should be overridden to break anyway (e.g. USERBREAK environment variable with Xcode).
*/
void	DebugEnterDebugger( Boolean inForce );

#if( DEBUG )
	#define debug_enter_debugger()		DebugEnterDebugger( false )
#else
	#define debug_enter_debugger()		do {} while( 0 )
#endif

#if( GCD_ENABLED )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DebugIsCurrentDispatchQueue
	@abstract	Returns true if the current dispatch queue is the same as the one passed in.
	@discussion	The underlying libdispatch function is deprecated so this should only be used by debug code.
*/
Boolean	DebugIsCurrentDispatchQueue( dispatch_queue_t inQueue );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPGetReasonPhrase
	@abstract	Returns the reason phrase associated with an HTTP-style status code or an empty string if not found/valid.
*/
const char *	HTTPGetReasonPhrase( int inStatusCode );

#if( COMPILER_OBJC )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NSErrorToOSStatus
	@abstract	Maps NSError objects to an OSStatus value. A nil NSError maps to kNoErr.
*/
OSStatus	NSErrorToOSStatus( NSError *inError );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ReportCriticalError
	@abstract	Reports a critical error, optionally triggering a crash log.
*/
void	ReportCriticalError( const char *inReason, uint32_t inExceptionCode, Boolean inCrashLog );

#if( COMPILER_OBJC )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ObjectTrackerRegister
	@abstract	Registers a tracker for an object with option to print a stack trace or call a function when it's released.
	@discussion	Object must be a backed by an Objective-C object, such as CFDictionary, xpc_object_t, Objective-C object, etc.
*/
#define kObjectTrackerFlags_None			0
#define kObjectTrackerFlag_PrintStack		( 1 << 0 ) // Print stack trace on release.

typedef void ( *ObjectTrackerRelease_f )( const void *inObj, void *inContext );

OSStatus
	ObjectTrackerRegister( 
		const void *			inObj, 
		uint32_t				inFlags, 
		ObjectTrackerRelease_f	inCallback, 
		void *					inContext );
void	ObjectTrackerDeregister( CFTypeRef inObj );
#endif

#if 0
#pragma mark ==    Routines - Unit Tests ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DebugServicesTest
	@abstract	Unit test.
*/
OSStatus	DebugServicesTest( void );

#ifdef __cplusplus
}
#endif

#endif // __DebugServices_h__

// Post includes for compatibility with code expecting DebugServices.h to provide LogUtils.

#if( !DEBUG_SERVICES_LITE )
	#include "LogUtils.h"
#elif( !defined( LOGUTILS_ENABLED ) )
	#define LOGUTILS_ENABLED		0
#endif
