/*
	File:    	CommonServices.h
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
	
	Copyright (C) 2002-2015 Apple Inc. All Rights Reserved.
	
	See <http://sourceforge.net/p/predef/wiki/Home/> for a decent list of predefined macros for various environments.
*/

#ifndef	__CommonServices_h__
#define	__CommonServices_h__

// CommonServices_PLATFORM_HEADER can be defined to include a platform-specific file before any other files are included.

#if( defined( CommonServices_PLATFORM_HEADER ) )
	#include  CommonServices_PLATFORM_HEADER
#endif

#if 0
#pragma mark == Compiler ==
#endif

//===========================================================================================================================
//	Compiler
//===========================================================================================================================

// Compatibility for clang's extension macros.

#if( !defined( __has_attribute ) )
	#define __has_attribute( x )	0
#endif

#if( !defined( __has_feature ) )
	#define __has_feature( x )		0
#endif

#if( !defined( __has_include ) )
	#define __has_include( x )		0
#endif

#if( __has_feature( objc_arc ) )
	#define COMPILER_ARC			1
#else
	#define COMPILER_ARC			0
#endif

#if( defined( __ARMCC_VERSION ) && __ARMCC_VERSION )
	#define COMPILER_ARM_REALVIEW	__ARMCC_VERSION
#else
	#define COMPILER_ARM_REALVIEW	0
#endif

#if( defined( __clang__ ) && __clang__ )
	#define COMPILER_CLANG			( ( __clang_major__ * 10000 ) + ( __clang_minor__ * 100 ) + __clang_patchlevel__ )
#else
	#define COMPILER_CLANG			0
#endif

#if( defined( __GNUC__ ) && __GNUC__ )
	#define COMPILER_GCC			( ( __GNUC__ * 10000 ) + ( __GNUC_MINOR__ * 100 ) + __GNUC_PATCHLEVEL__ )
#else
	#define COMPILER_GCC			0
#endif

#if( defined( __BLOCKS__ ) && __BLOCKS__ )
	#define COMPILER_HAS_BLOCKS		1
#else
	#define COMPILER_HAS_BLOCKS		0
#endif

#if( defined( __OBJC__ ) && __OBJC__ )
	#if( defined( __OBJC2__ ) && __OBJC2__ )
		#define COMPILER_OBJC		2
	#else
		#define COMPILER_OBJC		1
	#endif
#else
	#define COMPILER_OBJC			0
#endif

#if( defined( _MSC_VER ) )
	#define COMPILER_VISUAL_CPP		_MSC_VER
#else
	#define COMPILER_VISUAL_CPP		0
#endif

// STATIC_INLINE -- Portable way to marking an inline function for use in a header file.

#if( !defined( STATIC_INLINE ) )
	#if( defined( __GNUC__ ) && ( __GNUC__ >= 4 ) )
		#define STATIC_INLINE		static __inline__ __attribute__( ( always_inline ) )
	#elif( defined( __GNUC__ ) )
		#define STATIC_INLINE		static __inline__
	#elif( defined( __MWERKS__ ) || defined( __cplusplus ) )
		#define STATIC_INLINE		static inline
	#elif( COMPILER_VISUAL_CPP )
		#define STATIC_INLINE		static __inline
	#elif( defined( __WIN32__ ) )
		#define STATIC_INLINE		static __inline__
	#else
		#define STATIC_INLINE		static inline
	#endif
#endif

// ARC-safe macros for building code with ARC enabled or disabled.

#if( COMPILER_ARC )
	STATIC_INLINE id	arc_safe_autorelease( id x )	{ return( x ); }
	#define				arc_safe_release( x )			do {} while( 0 )
	STATIC_INLINE id	arc_safe_retain( id x )			{ return( x ); }
	#define				arc_safe_super_dealloc()		do {} while( 0 )
	
	#define	arc_safe_Block_copy( X )					[(X) copy]
	#define	arc_safe_Block_release( X )					do {} while( 0 )
	
	#define arc_safe_dispatch_release( x )				do {} while( 0 )
	#define arc_safe_dispatch_retain( x )				do {} while( 0 )
	
	#define arc_safe_os_release( x )					do {} while( 0 )
	#define arc_safe_os_retain( x )						do {} while( 0 )
	
	#define arc_safe_xpc_release( x )					do {} while( 0 )
	#define arc_safe_xpc_retain( x )					do {} while( 0 )
	
	// Transfer ownership of CF object to ARC.
	#define CFBridgingTransfer( X )						CFBridgingRelease( X )
#else
	#define arc_safe_autorelease( x )					[x autorelease]
	#define arc_safe_release( x )						[x release]
	#define arc_safe_retain( x )						[x retain]
	#define arc_safe_super_dealloc()					[super dealloc]
	
	#define	arc_safe_Block_copy( X )					Block_copy( (X) )
	#define	arc_safe_Block_release( X )					Block_release( (X) )
	
	#define arc_safe_dispatch_release( x )				dispatch_release( x )
	#define arc_safe_dispatch_retain( x )				dispatch_retain( x )
	
	#define arc_safe_os_release( x )					os_release( x )
	#define arc_safe_os_retain( x )						os_retain( x )
	
	#define arc_safe_xpc_release( x )					xpc_release( x )
	#define arc_safe_xpc_retain( x )					xpc_retain( x )
	
	// Transfer ownership of CF object to Objective-C code that will release it.
	#define CFBridgingTransfer( X )						( (id)(X) )
#endif

// ARC-safe macros for using an Objective-C object with C callbacks that void * contexts.
// Call CFBridgingRetainContext to get void * to pass to the C callback registration function.
// Call CFBridgingReleaseContext with that void * when the C callback won't be invoked anymore. NULL safe.
// Call CFBridgingReleaseObject to release the object if you don't have the void * (e.g. no stopped callback). nil safe.
// Note: Make sure to call CFBridgingReleaseContext if registering the C callback fails.
//
// The release functions don't use autorelease pools so don't release at the top of the callback function and continue 
// to use it (like you would with a normal CFBridgingRelease). Call the release function at the end when you're done.
// Autorelease pools are avoided for use with high frequency callbacks where objects may pile up on an autorelease pool.

#define CFBridgingRetainContext( OBJ )			( (void *) CFBridgingRetain( (OBJ) ) )
#if( COMPILER_ARC )
	#define CFBridgingReleaseContext( CTX )		CFBridgingRelease( (CFTypeRef)(CTX) )
	#define CFBridgingReleaseObject( OBJ )		CFBridgingRelease( (__bridge CFTypeRef)(OBJ) )
#else
	#define CFBridgingReleaseContext( CTX )		CFReleaseNullSafe( (CFTypeRef)(CTX) )
	#define CFBridgingReleaseObject( OBJ )		CFReleaseNullSafe( (__bridge CFTypeRef)(OBJ) )
#endif

// ATTRIBUTE_NORETURN -- Marks a function as never returning.

#if( !defined( ATTRIBUTE_NORETURN ) )
	#if( defined( __GNUC__ ) )
		#define ATTRIBUTE_NORETURN		__attribute__( ( __noreturn__ ) )
	#else
		#define ATTRIBUTE_NORETURN
	#endif
#endif

// __builtin_expect -- See <http://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html>.

#if( !defined( __builtin_expect ) && ( !defined( __GNUC__ ) || ( __GNUC__ < 3 ) || COMPILER_ARM_REALVIEW ) )
	#define __builtin_expect( EXPRESSSION, EXPECTED_RESULT )		( EXPRESSSION )
#endif

// C99's _Pragma operator is not yet supported with Visual Studio, but Visual Studio does support __pragma so use that.

#if( COMPILER_VISUAL_CPP )
	#define _Pragma		__pragma
#endif

// STATIC_ANALYZER_NORETURN -- Tells the static analyzer assume a function doesn't return (e.g. assertion handlers).

#if( !defined( STATIC_ANALYZER_NORETURN ) )
	#if( COMPILER_CLANG )
		#define STATIC_ANALYZER_NORETURN		__attribute__( ( analyzer_noreturn ) )
	#else
		#define STATIC_ANALYZER_NORETURN
	#endif
#endif

// Unreachable code -- Tells the compiler that unreachable code is expected between the begin/end.

#if( COMPILER_CLANG )
	#define begin_unreachable_code_paths() \
		_Pragma( "clang diagnostic push" ) \
		_Pragma( "clang diagnostic ignored \"-Wunreachable-code\"" )
#else
	#define begin_unreachable_code_paths()
#endif

#if( COMPILER_CLANG )
	#define end_unreachable_code_paths() \
		_Pragma( "clang diagnostic pop" )
#else
	#define end_unreachable_code_paths()
#endif

#if 0
#pragma mark == Target ==
#endif

//===========================================================================================================================
//	Target
//===========================================================================================================================

#if( !defined( TARGET_OS_OVERRIDE ) )
	#define TARGET_OS_OVERRIDE		0
#endif

#if( !defined( TARGET_OS_ANDROID ) )
// Android
	#if( !TARGET_OS_OVERRIDE )
		#if( defined( __ANDROID__ ) )
			#define	TARGET_OS_ANDROID	1
		#else
			#define	TARGET_OS_ANDROID	0
		#endif
	#else
		#define	TARGET_OS_ANDROID		0
	#endif
#endif

#if( !defined( TARGET_OS_MAC ) )
// Macintosh
	#if( !TARGET_OS_OVERRIDE )
		#if( defined( __MACH__ ) && __MACH__ )
			// TargetConditionals.h will define this flag.
		#else
			#define	TARGET_OS_MAC		0
		#endif
	#else
		#define TARGET_OS_MAC			0
	#endif
#endif


#if( !defined( TARGET_OS_FREEBSD ) )
// FreeBSD
	#if( !TARGET_OS_OVERRIDE )
		#if( defined( __FreeBSD__ ) )
			#define TARGET_OS_FREEBSD		1
		#else
			#define TARGET_OS_FREEBSD		0
		#endif
	#else
		#define TARGET_OS_FREEBSD			0
	#endif
#endif

#if( !defined( TARGET_OS_LINUX ) )
// Linux
	#if( !TARGET_OS_OVERRIDE )
		#if( defined( __linux__ ) )
			#define	TARGET_OS_LINUX			1
		#else
			#define	TARGET_OS_LINUX			0
		#endif
	#else
		#define	TARGET_OS_LINUX				0
	#endif
#endif

#if( !defined( TARGET_OS_NETBSD ) )
// NetBSD
	#if( !TARGET_OS_OVERRIDE )
		#if( defined( __NetBSD__ ) )
			#define TARGET_OS_NETBSD		1
		#else
			#define TARGET_OS_NETBSD		0
		#endif
	#else
		#define TARGET_OS_NETBSD			0
	#endif
#endif

#if( !defined( TARGET_OS_PALM ) )
// Palm
	#if( !TARGET_OS_OVERRIDE )
		#if( defined( __PALMOS_TRAPS__ ) || defined( __PALMOS_ARMLET__ ) )
			#define	TARGET_OS_PALM			1
		#else
			#define	TARGET_OS_PALM			0
		#endif
	#else
		#define	TARGET_OS_PALM				0
	#endif
#endif

#if( !defined( TARGET_OS_QNX ) )
// QNX
	#if( defined( __QNX__ ) )
		#define TARGET_OS_QNX				1
	#else
		#define TARGET_OS_QNX				0
	#endif
#endif


#if( !defined( TARGET_OS_THREADX ) )
// ThreadX
	#if( !TARGET_OS_OVERRIDE )
		#if( defined( __THREADX ) || defined( __RTOS_THREADX__ ) || defined( RTOS_ThreadX ) )
			#define	TARGET_OS_THREADX		1
		#else
			#define	TARGET_OS_THREADX		0
		#endif
	#else
		#define	TARGET_OS_THREADX			0
	#endif
#endif

#if( !defined( TARGET_PLATFORM_WICED ) )
// WICED
	#if( defined( WICED_VERSION ) )
		#define TARGET_PLATFORM_WICED		1
	#else
		#define TARGET_PLATFORM_WICED		0
	#endif
#endif

#if( !defined( TARGET_OS_WINDOWS_KERNEL ) )
// Windows
	#if( !TARGET_OS_OVERRIDE )
		#if( defined( NT_INST ) ) // NT_INST was the best thing I could see defined when building Windows Drivers.
			#define	TARGET_OS_WINDOWS_KERNEL	1
			
			#undef	TARGET_OS_WIN32
			#define	TARGET_OS_WIN32				0 // Force off because EFI uses Windows compilers and looks like Windows otherwise.
		#else
			#define	TARGET_OS_WINDOWS_KERNEL	0
		#endif
	#else
		#define	TARGET_OS_WINDOWS_KERNEL		0
	#endif
#endif
#if( !defined( TARGET_OS_WIN32 ) )
	#if( !TARGET_OS_OVERRIDE )
		#if( ( defined( macintosh ) && macintosh ) || ( defined( __MACH__ ) && __MACH__ ) )
			// ConditionalMacros.h in CoreServices will define this TARGET_* flag.
		#else
			#if( defined( _WIN32 ) && !TARGET_OS_WINDOWS_KERNEL )
				#define	TARGET_OS_WIN32		1
			#else
				#define	TARGET_OS_WIN32		0
			#endif
		#endif
	#else
		#define	TARGET_OS_WIN32				0
	#endif
#endif
#if( !defined( TARGET_OS_WIN64 ) )
	#if( !TARGET_OS_OVERRIDE )
		#if( defined( _WIN64 ) )
			#define	TARGET_OS_WIN64		1
		#else
			#define	TARGET_OS_WIN64		0
		#endif
	#else
		#define	TARGET_OS_WIN64			0
	#endif
#endif
#if( !defined( TARGET_OS_WINDOWS ) )
	#if( ( defined( TARGET_OS_WIN32 ) && TARGET_OS_WIN32 ) || ( defined( TARGET_OS_WIN64 ) && TARGET_OS_WIN64 ) )
		#define	TARGET_OS_WINDOWS	1
	#else
		#define	TARGET_OS_WINDOWS	0
	#endif
#endif

#if( !defined( TARGET_OS_WINDOWS_CE ) )
// Windows CE
	#if( !TARGET_OS_OVERRIDE )
		#if( defined( _WIN32_WCE ) )
			#define	TARGET_OS_WINDOWS_CE	1
		#else
			#define	TARGET_OS_WINDOWS_CE	0
		#endif
	#else
		#define	TARGET_OS_WINDOWS_CE		0
	#endif
#endif

#if( !defined( TARGET_OS_VXWORKS ) )
// VxWorks
	#if( !TARGET_OS_OVERRIDE )
		#if( defined( __VXWORKS ) || defined( __RTOS_VXWORKS__ ) )
			#define	TARGET_OS_VXWORKS		1
		#else
			#define	TARGET_OS_VXWORKS		0
		#endif
	#else
		#define	TARGET_OS_VXWORKS			0
	#endif
#endif

#if 0
#pragma mark == Target High Level ==
#endif

//===========================================================================================================================
//	Target High Level -- TARGET_* flags based on the above TARGET_OS_* flags.
//===========================================================================================================================

#if( !defined( COMMON_SERVICES_NO_CORE_SERVICES ) )
	#define COMMON_SERVICES_NO_CORE_SERVICES			0
#endif

#if( !defined( COMMON_SERVICES_NO_SYSTEM_CONFIGURATION ) )
	#define COMMON_SERVICES_NO_SYSTEM_CONFIGURATION		0
#endif

#if( !defined( EXCLUDE_UNIT_TESTS ) )
	#if( TARGET_PLATFORM_WICED )
		#define EXCLUDE_UNIT_TESTS		1
	#endif
#endif

// TARGET_KERNEL -- Any kernel-level code (i.e. not user level).

#if( defined( KERNEL ) || defined( _KERNEL ) || defined( __KERNEL__ ) || defined( INKERNEL ) || TARGET_OS_WINDOWS_KERNEL )
	#define	TARGET_KERNEL		1
#else
	#define	TARGET_KERNEL		0
#endif

// TARGET_MACH -- Any Mach-based OS.

#if( !defined( TARGET_MACH ) )
	#if( defined( __MACH__ ) && __MACH__ ) // EFI can build with Mach-o compilers so exclude EFI.
		#define TARGET_MACH		1
	#else
		#define TARGET_MACH		0
	#endif
#endif

// TARGET_OS_DARWIN -- Any variant of Darwin.

#if( !defined( TARGET_OS_DARWIN ) )
	#if( !TARGET_OS_OVERRIDE )	
		#if( TARGET_MACH )
			#define TARGET_OS_DARWIN	1
		#else
			#define TARGET_OS_DARWIN	0
		#endif
	#else
		#define TARGET_OS_DARWIN		0
	#endif
#endif

// TARGET_OS_DARWIN_KERNEL -- Any variant of the Darwin kernel.

#if( !defined( TARGET_OS_DARWIN_KERNEL ) )
	#if( TARGET_MACH && TARGET_KERNEL )
		#define TARGET_OS_DARWIN_KERNEL		1
	#else
		#define TARGET_OS_DARWIN_KERNEL		0
	#endif
#endif

// TARGET_OS_POSIX -- Any OS that's mostly POSIX compatible.

#if( TARGET_KERNEL || TARGET_OS_PALM || TARGET_OS_THREADX || TARGET_OS_WINDOWS || TARGET_OS_VXWORKS )
	#define	TARGET_OS_POSIX		0
#else
	#define	TARGET_OS_POSIX		1
#endif

// TARGET_HAS_AVX

#if( defined( __AVX2__ ) && __AVX2__ )
	#define TARGET_HAS_AVX		2
#elif( defined( __AVX__ ) && __AVX__ )
	#define TARGET_HAS_AVX		1
#else
	#define TARGET_HAS_AVX		0
#endif
#define AVX_VERSION( X )		( (X) )

// TARGET_HAS_NEON

#if( defined( __ARM_NEON__ ) && __ARM_NEON__ )
	#define TARGET_HAS_NEON		1
#else
	#define TARGET_HAS_NEON		0
#endif

// TARGET_HAS_SSE

#if( defined( __SSE4_2__ ) && __SSE4_2__ )
	#define TARGET_HAS_SSE		42
#elif( defined( __SSE4_1__ ) && __SSE4_1__ )
	#define TARGET_HAS_SSE		41
#elif( defined( __SSE3__ ) && __SSE3__ )
	#define TARGET_HAS_SSE		30
#elif( defined( __SSE2__ ) && __SSE2__ )
	#define TARGET_HAS_SSE		20
#elif( defined( __SSE__ ) && __SSE__ )
	#define TARGET_HAS_SSE		10
#else
	#define TARGET_HAS_SSE		0
#endif
#define SSE_VERSION( MAJOR, MINOR )		( ( (MAJOR) * 10 ) + (MINOR) )

// TARGET_HAS_SSSE (Supplemental SSE)

#if( defined( __SSSE3__ ) && __SSSE3__ )
	#define TARGET_HAS_SSSE		3
#else
	#define TARGET_HAS_SSSE		0
#endif
#define SSSE_VERSION( X )		( (X) )

// TARGET_HAS_STD_C_LIB -- Anything that has a standard C library (strcpy, memcpy, malloc, etc.).

#if( TARGET_KERNEL )
	#define	TARGET_HAS_STD_C_LIB		0
#else
	#define	TARGET_HAS_STD_C_LIB		1
#endif

// TARGET_HAS_C_LIB_IO -- Has C library I/O support (fopen, fprintf, etc.).

#if( !defined( TARGET_HAS_C_LIB_IO ) )
	#if( TARGET_HAS_STD_C_LIB && !TARGET_OS_THREADX && !TARGET_OS_WINDOWS_CE )
		#define	TARGET_HAS_C_LIB_IO			1
	#else
		#define	TARGET_HAS_C_LIB_IO			0
	#endif
#endif

// TARGET_HAS_FLOATING_POINT_SUPPORT -- Has either floating point emulation libraries or hardware floating point.

	#define TARGET_HAS_FLOATING_POINT_SUPPORT		1

// TARGET_HAS_SOCKETS -- Has BSD-style socket API support.

#if( !defined( TARGET_HAS_SOCKETS ) )
	#if( !TARGET_PLATFORM_WICED )
		#define	TARGET_HAS_SOCKETS			1
	#else
		#define	TARGET_HAS_SOCKETS			0
	#endif
#endif

// TARGET_LANGUAGE_C_LIKE -- indicates if we're building for code that is C-like (vs assembly).

#if( COMPILER_GCC || !TARGET_OS_VXWORKS )
	#define	TARGET_LANGUAGE_C_LIKE		1
#else
	#define	TARGET_LANGUAGE_C_LIKE		0
#endif

// TARGET_NETWORK_NETX_DUO -- indicates if we're using Express Logic's NetX Duo networking stack.

#if( defined( NETWORK_NetX_Duo ) && NETWORK_NetX_Duo )
	#define	TARGET_NETWORK_NETX_DUO		1
#else
	#define	TARGET_NETWORK_NETX_DUO		0
#endif

// TARGET_VISUAL_STUDIO flags for determining Visual Studio versions.
// Note: EFI's compiler reports a Visual C++ version of 1400, but it doesn't support VS 2005 or later features.

#if( COMPILER_VISUAL_CPP >= 1800 )
	#define	TARGET_VISUAL_STUDIO_2013_OR_LATER		1
#else
	#define	TARGET_VISUAL_STUDIO_2013_OR_LATER		0
#endif
#if( COMPILER_VISUAL_CPP >= 1600 )
	#define	TARGET_VISUAL_STUDIO_2010_OR_LATER		1
#else
	#define	TARGET_VISUAL_STUDIO_2010_OR_LATER		0
#endif
#if( COMPILER_VISUAL_CPP >= 1500 )
	#define	TARGET_VISUAL_STUDIO_2008_OR_LATER		1
#else
	#define	TARGET_VISUAL_STUDIO_2008_OR_LATER		0
#endif
#if(  COMPILER_VISUAL_CPP >= 1400  )
	#define	TARGET_VISUAL_STUDIO_2005_OR_LATER		1
#else
	#define	TARGET_VISUAL_STUDIO_2005_OR_LATER		0
#endif
#if( COMPILER_VISUAL_CPP )
	#define	TARGET_VISUAL_STUDIO_2003_OR_LATER		1
	#define	TARGET_VISUAL_STUDIO					1
#else
	#define	TARGET_VISUAL_STUDIO_2003_OR_LATER		0
	#define	TARGET_VISUAL_STUDIO					0
#endif

#if 0
#pragma mark == Includes ==
#endif

//===========================================================================================================================
//	Includes
//===========================================================================================================================

#if( !TARGET_KERNEL )
	#include <stddef.h>
#endif

#if( TARGET_OS_DARWIN && !TARGET_OS_DARWIN_KERNEL )
// Darwin non-kernel
	#include <stdbool.h>
	#include <stdint.h>
	
	#include <sys/types.h>
	#include <sys/socket.h>
	
	#include <arpa/inet.h>
	#include <fcntl.h>
	#include <net/if.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <pthread.h>
	#include <sys/ioctl.h>
	#include <sys/param.h>
	#include <sys/uio.h>
	#include <unistd.h>
	
	#if( COMMON_SERVICES_NO_CORE_SERVICES )
		#include <AvailabilityMacros.h>
		#include <MacTypes.h>
		#include <TargetConditionals.h>
	#else
		#include <CoreFoundation/CoreFoundation.h>
		
			#include <CoreServices/CoreServices.h>
	#endif
	
	#include <CommonCrypto/CommonCrypto.h>
	#include <CommonCrypto/CommonRandom.h> // Remove when <radar:22390036> is fixed.
	#if( !COMMON_SERVICES_NO_CORE_SERVICES && __has_include( <network/tcp_connection.h> ) )
		#include <network/tcp_connection.h>
	#endif

#elif( TARGET_OS_DARWIN_KERNEL )
// Darwin Kernel
	#include <stdint.h>
	
	#include <sys/types.h>
	
	#include <machine/limits.h>
	#include <netinet/in.h>
	#include <sys/kpi_socket.h>
	#include <sys/malloc.h>
	#include <sys/socket.h>
	
	#include <IOKit/IOTypes.h>
	#include <libkern/libkern.h>
	#include <libkern/OSTypes.h>

#elif( TARGET_OS_FREEBSD )
// FreeBSD
	#if( TARGET_KERNEL )
		#include <sys/types.h>
		#include <netinet/in.h>
		#include <sys/malloc.h>
		#include <sys/socket.h>
	#else
		#include <stdint.h>
		
		#include <sys/types.h>
		#include <netinet/in.h>
		#include <sys/socket.h>
		#include <unistd.h>
	#endif

#elif( TARGET_OS_LINUX )
// Linux
	#include <stdint.h>

	#include <sys/types.h>
	
	#include <net/if.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <unistd.h>

#elif( TARGET_OS_NETBSD )
// NetBSD
	#if( TARGET_KERNEL )
		#include <sys/types.h>
		#include <netinet/in.h>
		#include <sys/malloc.h>
		#include <sys/socket.h>
	#else
		#include <stdint.h>
		
		#include <sys/types.h>
		#include <netinet/in.h>
		#include <sys/socket.h>
		#include <unistd.h>
	#endif

#elif( TARGET_OS_PALM )
// Palm (no special includes yet).

#elif( TARGET_OS_QNX )
// QNX
	#include <stdbool.h>
	#include <stdint.h>
	#include <string.h>
	
	#include <sys/types.h>
	#include <sys/socket.h>
	
	#include <arpa/inet.h>
	#include <fcntl.h>
	#include <net/if.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <pthread.h>
	#include <sys/ioctl.h>
	#include <sys/param.h>
	#include <sys/uio.h>
	#include <unistd.h>
	
	#include <sys/neutrino.h>

#elif( TARGET_OS_THREADX )
// ThreadX
	#include <stdint.h>
	
	#include "tx_api.h"
	#if( TARGET_NETWORK_NETX_DUO )
		#include "nx_api.h"
	#endif

#elif( TARGET_OS_VXWORKS )
// VxWorks
	#include "vxWorks.h"
	
	#if( TARGET_LANGUAGE_C_LIKE )
		#include <sys/types.h>
		#include <netinet/in.h>
	#endif

#elif( TARGET_PLATFORM_WICED )
// WICED
	#include "wiced.h"

#elif( TARGET_OS_WINDOWS )
// Windows
	#if( !defined( WIN32_LEAN_AND_MEAN ) )
		#define	WIN32_LEAN_AND_MEAN		// Needed to avoid redefinitions by Windows interfaces.
	#endif
	
	#if( !defined( WINVER ) )
		#define WINVER			0x0501	// Default to Windows XP.
	#endif
	
	#if( !defined( _WIN32_WINNT ) )
		#define _WIN32_WINNT	WINVER
	#endif
	
	#if( defined( __MWERKS__ ) )
		#if( __option( c9x ) )
			#include <stdbool.h>
		#endif
		#include <io.h>
	#elif( COMPILER_VISUAL_CPP )
		#pragma warning( disable:4127 )	// Disable "conditional expression is constant" warning for debug macros.
		#pragma warning( disable:4201 )	// Disable "nonstandard extension used : nameless struct/union" warning for Microsoft headers.
		#pragma warning( disable:4706 )	// Disable "assignment within conditional expression" for Microsoft headers.
	#endif
	
	#if( TARGET_VISUAL_STUDIO_2013_OR_LATER )
		#include <stdbool.h>
	#endif
	#include <stdint.h>
	#include <windows.h>
	
	#include <process.h>
	#include <winioctl.h>
	#include <winsock2.h>
	#include <Ws2tcpip.h>
	#include <tchar.h>
	#include <time.h>
	
	#if( _WIN32_WINNT < 0x0502 )
		#include <Wspiapi.h> // For getaddrinfo, etc. emulation inlines Windows 2000 and earlier systems.
	#endif
	
	#if( COMPILER_VISUAL_CPP )
		#pragma warning( default:4201 )	// Re-enable "nonstandard extension used : nameless struct/union" after Microsoft headers.
		#pragma warning( default:4706 )	// Re-enable "assignment within conditional expression" after Microsoft headers.
	#endif

#elif( TARGET_OS_WINDOWS_KERNEL )
// Windows Kernel
	#include <stdarg.h>
	#include <ntddk.h>

#else
// Unknown
	#error unknown OS - update this file to support your OS
#endif

#if( TARGET_HAS_STD_C_LIB )
	#include <errno.h>
#endif

#if( ( TARGET_OS_FREEBSD || TARGET_OS_NETBSD || defined( __unix__ ) || defined( unix ) ) && !defined( USG ) )
	// Include sys/param.h on systems that have it to pick up things like the "BSD" preprocessor symbol.
	
	#include <sys/param.h>
#endif

#if( !defined( TARGET_IPHONE_SIMULATOR ) )
	#define TARGET_IPHONE_SIMULATOR		0
#endif
#if( defined( BSD ) )
	#define	TARGET_OS_BSD				1
#else
	#define	TARGET_OS_BSD				0
#endif

// TARGET_OS_MACOSX -- Mac OS X (but not iOS or the iOS Simulator).

#if( !defined( TARGET_OS_MACOSX ) )
	#if( TARGET_OS_MAC )
		#define TARGET_OS_MACOSX		1
	#else
		#define TARGET_OS_MACOSX		0
	#endif
#endif

#if 0
#pragma mark == Post-Include Defines ==
#endif

//===========================================================================================================================
//	Defines that rely on the base set of includes
//===========================================================================================================================

// CF_BRIDGED_TYPE -- Marks a typedef as a CF-compatible object for automatic bridging with Objective-C/Swift.
// Use it like this: typedef struct CF_BRIDGED_TYPE( id ) MyTypePrivate *		MyTypeRef;

#if( !defined( CF_BRIDGED_TYPE ) )
	#define CF_BRIDGED_TYPE( T )
#endif
	#define CF_BRIDGED_TYPE_compat( T )

// CF_NONNULL -- Marks a pointer as not being NULL as part of the API contract.

#if( !defined( CF_NONNULL ) )
	#if( __has_feature( nullability ) )
		#define CF_NONNULL		__nonnull
	#else
		#define CF_NONNULL
	#endif
#endif

// CF_NULLABLE -- Marks a pointer as potentially being NULL.

#if( !defined( CF_NULLABLE ) )
	#if( __has_feature( nullability ) )
		#define CF_NULLABLE		__nullable
	#else
		#define CF_NULLABLE
	#endif
#endif

// CF_RETURNS_RETAINED -- Marks a function as returning a CFRetain'd object.

#if( !defined( CF_RETURNS_RETAINED ) )
	#if( __has_feature( attribute_cf_returns_retained ) )
		#define CF_RETURNS_RETAINED		__attribute__( ( cf_returns_retained ) )
	#else
		#define CF_RETURNS_RETAINED
	#endif
#endif

// CF_RETURNS_RETAINED_PARAMETER -- Marks a function parameter as returning a CFRetain'd object.

#if( !defined( CF_RETURNS_RETAINED_PARAMETER ) )
	#if( __has_feature( attribute_cf_returns_on_parameters ) )
		#define CF_RETURNS_RETAINED_PARAMETER		CF_RETURNS_RETAINED
	#else
		#define CF_RETURNS_RETAINED_PARAMETER
	#endif
#endif

// CF_RETURNS_NOT_RETAINED -- Marks a function as not returning a CFRetain'd object.

#if( !defined( CF_RETURNS_NOT_RETAINED ) )
	#if( __has_feature( attribute_cf_returns_not_retained ) )
		#define CF_RETURNS_NOT_RETAINED		__attribute__( ( cf_returns_not_retained ) )
	#else
		#define CF_RETURNS_NOT_RETAINED
	#endif
#endif

// CFL_BINARY_PLISTS: 1=Support binary property lists, 0=Don't.

#if( !defined( CFL_BINARY_PLISTS ) )
	#define	CFL_BINARY_PLISTS		1
#endif

// CFL_BINARY_PLISTS_STREAMED: 1=Support binary property lists using the streamed format.

#if( !defined( CFL_BINARY_PLISTS_STREAMED ) )
	#define	CFL_BINARY_PLISTS_STREAMED		1
#endif

// CFLITE_ENABLED: 1=Use CFLite. 0=Strip out CFLite code.

#if( !defined( CFLITE_ENABLED ) )
	#if( TARGET_OS_DARWIN || TARGET_PLATFORM_WICED )
		#define	CFLITE_ENABLED		0
	#else
		#define	CFLITE_ENABLED		1
	#endif
#endif

// CF_ENABLED: 1=CoreFoundation APIs can be used (either the real CF or CFLite). 0=CoreFoundation cannot be used.

#if( !defined( CF_ENABLED ) )
	#if( ( CFLITE_ENABLED || TARGET_OS_DARWIN ) && !TARGET_KERNEL )
		#define	CF_ENABLED		1
	#else
		#define	CF_ENABLED		0
	#endif
#endif

// CFL_XML: 1=Support XML property lists, 0=Don't support XML.

#if( !defined( CFL_XML ) )
	#define	CFL_XML		0
#endif

// DEBUG/NDEBUG

#if( !defined( DEBUG ) )
	#define DEBUG			0
#endif
#if( !defined( NDEBUG ) )
	#if( DEBUG )
		#define NDEBUG		0
	#else
		#define NDEBUG		1
	#endif
#endif
#if( DEBUG && NDEBUG )
	#error NDEBUG and DEBUG both enabled...they are mutually exclusive.
#endif

// DISPATCH_LITE_ENABLED -- Controls whether to emulate libdispatch or not.

#if( !defined( DISPATCH_LITE_ENABLED ) )
	#if( TARGET_OS_DARWIN || TARGET_PLATFORM_WICED )
		#define DISPATCH_LITE_ENABLED		0
	#else
		#define DISPATCH_LITE_ENABLED		1
	#endif
#endif
#if( !defined( GCD_ENABLED ) )
	#if( DISPATCH_LITE_ENABLED || TARGET_OS_DARWIN )
		#define	GCD_ENABLED		1
	#else
		#define	GCD_ENABLED		0
	#endif
#endif

#if( defined( OS_OBJECT_USE_OBJC ) && OS_OBJECT_USE_OBJC )
	#define DISPATCH_UNSAFE_UNRETAINED		__unsafe_unretained
#else
	#define DISPATCH_UNSAFE_UNRETAINED
#endif

// EXCLUDE_UNIT_TESTS -- Controls whether unit tests are stripped out.

#if( !defined( EXCLUDE_UNIT_TESTS ) )
	#define EXCLUDE_UNIT_TESTS		0
#endif

// KEYCHAIN_LITE_ENABLED -- Controls whether to emulate Keychain emulation APIs or not.

#if( !defined( KEYCHAIN_LITE_ENABLED ) )
	#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
		#define KEYCHAIN_LITE_ENABLED		0
	#else
		#define KEYCHAIN_LITE_ENABLED		1
	#endif
#endif
#if( !defined( KEYCHAIN_ENABLED ) )
	#if( KEYCHAIN_LITE_ENABLED || ( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES ) )
		#define	KEYCHAIN_ENABLED		1
	#else
		#define	KEYCHAIN_ENABLED		0
	#endif
#endif

// QNX_VERSION -- Version of QNX used to build the code. Format is version * 100 so 6.6 = 660.

#if( !defined( QNX_VERSION ) )
	#if( defined( _NTO_VERSION ) )
		#define QNX_VERSION		_NTO_VERSION
	#else
		#define QNX_VERSION		0
	#endif
#endif

// XPC_LITE_ENABLED -- Controls whether to emulate XPC or not.

#if( !defined( XPC_LITE_ENABLED ) )
	#if( ( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES ) || TARGET_PLATFORM_WICED )
		#define XPC_LITE_ENABLED		0
	#else
		#define XPC_LITE_ENABLED		1
	#endif
#endif
#if( !defined( XPC_ENABLED ) )
	#if( XPC_LITE_ENABLED || ( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES && !TARGET_KERNEL ) )
		#define	XPC_ENABLED		1
	#else
		#define	XPC_ENABLED		0
	#endif
#endif

// NS_DESIGNATED_INITIALIZER -- Marks a method as the designated initializer for an object.

#if( !defined( NS_DESIGNATED_INITIALIZER ) )
	#if( __has_attribute( ns_designated_initializer ) )
		#define NS_DESIGNATED_INITIALIZER	__attribute__( ( ns_designated_initializer ) )
	#else
		#define NS_DESIGNATED_INITIALIZER
	#endif
#endif

// __ROUTINE__ -- Routine name. Uses ISO __func__ where possible. Otherwise, uses the best thing available (if anything).

#if( COMPILER_ARM_REALVIEW || COMPILER_GCC )
	#define	__ROUTINE__		__PRETTY_FUNCTION__
#elif( ( COMPILER_VISUAL_CPP && !defined( _WIN32_WCE ) ) || defined( __ghs__ ) )
	#define	__ROUTINE__		__FUNCTION__
#elif( defined( __MWERKS__ ) )
	#define	__ROUTINE__		__func__
#else
	#define	__ROUTINE__		""
#endif

#if( defined( __MWERKS__ ) || defined( __GNUC__ ) || defined( __ghs__ ) )
	#define	__ROUTINE_FULL__		__PRETTY_FUNCTION__
#elif( COMPILER_VISUAL_CPP && !defined( _WIN32_WCE ) )
	#define	__ROUTINE_FULL__		__FUNCTION__
#else
	#define	__ROUTINE_FULL__		""
#endif

// __PROGRAM__ -- Program name.

#if( TARGET_OS_DARWIN_KERNEL )
	extern kmod_info_t		KMOD_INFO_NAME;
	#define	__PROGRAM__		( (const char *)( KMOD_INFO_NAME.name ) )
#elif( TARGET_KERNEL )
	#define	__PROGRAM__		"KERNEL"
#elif( TARGET_OS_BSD || TARGET_OS_THREADX || TARGET_OS_WINDOWS )
	#define	__PROGRAM__		getprogname()
#else
	#define	__PROGRAM__		""
#endif

// TARGET_HAS_AUDIO_SESSION: 1=Platform supports CoreAudio's AudioSession APIs (and AVFoundation's AVAudioSession).

#if( !defined( TARGET_HAS_AUDIO_SESSION ) )
		#define TARGET_HAS_AUDIO_SESSION		0
#endif

// TARGET_HAS_COMMON_CRYPTO: 1=Has Apple's CommonCrypto.

#if( !defined( TARGET_HAS_COMMON_CRYPTO ) )
	#if( TARGET_OS_DARWIN && !TARGET_KERNEL )
		#define TARGET_HAS_COMMON_CRYPTO		1
	#else
		#define TARGET_HAS_COMMON_CRYPTO		0
	#endif
#endif

// TARGET_HAS_MD5_UTILS: 1=Use MD5Utils.c instead of an OS-specific library.

#if( !defined( TARGET_HAS_MD5_UTILS ) )
	#if( TARGET_OS_WINDOWS )
		#define TARGET_HAS_MD5_UTILS		1
	#else
		#define TARGET_HAS_MD5_UTILS		0
	#endif
#endif

#if( !defined( TARGET_HAS_MOCANA_SSL ) )
	#define TARGET_HAS_MOCANA_SSL			0
#endif

// TARGET_HAS_SHA_UTILS: 1=Use SHAUtils.c instead of an OS-specific library.

#if( !defined( TARGET_HAS_SHA_UTILS ) )
	#if( TARGET_OS_WINDOWS )
		#define TARGET_HAS_SHA_UTILS		1
	#else
		#define TARGET_HAS_SHA_UTILS		0
	#endif
#endif

// TARGET_NO_OPENSSL: 1=Prevent OpenSSL from being used.

#if( !defined( TARGET_NO_OPENSSL ) )
	#if( TARGET_HAS_COMMON_CRYPTO || TARGET_HAS_MOCANA_SSL || TARGET_PLATFORM_WICED )
		#define TARGET_NO_OPENSSL		1
	#else
		#define TARGET_NO_OPENSSL		0
	#endif
#endif

#if( TARGET_HAS_COMMON_CRYPTO )
	#if( !defined( COMMON_DIGEST_FOR_OPENSSL ) )
		#define MD5_DIGEST_LENGTH				CC_MD5_DIGEST_LENGTH
		#define MD5_CTX							CC_MD5_CTX
		#define MD5_Init						CC_MD5_Init
		#define MD5_Update( CTX, PTR, LEN ) 	CC_MD5_Update( (CTX), (PTR), (CC_LONG)(LEN) )
		#define MD5_Final						CC_MD5_Final
		
		#define SHA_DIGEST_LENGTH				CC_SHA1_DIGEST_LENGTH
		#define SHA_CTX							CC_SHA1_CTX
		#define SHA1_Init						CC_SHA1_Init
		#define SHA1_Update( CTX, PTR, LEN )	CC_SHA1_Update( (CTX), (PTR), (CC_LONG)(LEN) )
		#define SHA1_Final						CC_SHA1_Final
		#define SHA1( PTR, LEN, DIGEST )		CC_SHA1( (PTR), (CC_LONG)(LEN), DIGEST )
		
		#define SHA256_DIGEST_LENGTH			CC_SHA256_DIGEST_LENGTH
		#define SHA256_CTX						CC_SHA256_CTX
		#define SHA256_Init						CC_SHA256_Init
		#define SHA256_Update( CTX, PTR, LEN )	CC_SHA256_Update( (CTX), (PTR), (CC_LONG)(LEN) )
		#define SHA256_Final					CC_SHA256_Final
		#define SHA256( PTR, LEN, DIGEST )		CC_SHA256( (PTR), (CC_LONG)(LEN), DIGEST )
		
		#define SHA512_DIGEST_LENGTH			CC_SHA512_DIGEST_LENGTH
		#define SHA512_CTX						CC_SHA512_CTX
		#define SHA512_Init						CC_SHA512_Init
		#define SHA512_Update( CTX, PTR, LEN )	CC_SHA512_Update( (CTX), (PTR), (CC_LONG)(LEN) )
		#define SHA512_Final					CC_SHA512_Final
		#define SHA512( PTR, LEN, DIGEST )		CC_SHA512( (PTR), (CC_LONG)(LEN), DIGEST )
	#endif
	
	#define HEADER_MD5_H	1 // Trick openssl/md5.h into doing nothing.
	#define HEADER_SHA_H	1 // Trick openssl/sha.h into doing nothing.
#elif( TARGET_HAS_MOCANA_SSL )
	#define MD5_DIGEST_LENGTH					MD5_DIGESTSIZE
	#define MD5_CTX								MD5_CTX
	#define MD5_Init							MD5init_HandShake
	#define MD5_Update( CTX, PTR, LEN ) 		MD5update_HandShake( (CTX), (const ubyte *)(PTR), (LEN) )
	#define MD5_Final( DIGEST, CTX )			MD5final_HandShake( (CTX), (ubyte *)(DIGEST) )
	
	#define SHA_DIGEST_LENGTH					20
	#define SHA_CTX								SHA1_CTX
	#define SHA1_Init							SHA1_initDigestHandShake
	#define SHA1_Update( CTX, PTR, LEN )		SHA1_updateDigestHandShake( (CTX), (const ubyte *)(PTR), (LEN) )
	#define SHA1_Final( DIGEST, CTX )			SHA1_finalDigestHandShake( (CTX), (DIGEST) )
	#define SHA1( PTR, LEN, DIGEST )			SHA1_completeDigest( (ubyte *)(PTR), (LEN), (DIGEST) )
	
	#define SHA512_DIGEST_LENGTH				64
	#define SHA512_CTX							SHA512_CTX_compat
	#define SHA512_Init( CTX )					SHA512_Init_compat( (CTX) )
	#define SHA512_Update( CTX, PTR, LEN )		SHA512_Update_compat( (CTX), (PTR), (LEN) )
	#define SHA512_Final( DIGEST, CTX )			SHA512_Final_compat( (DIGEST), (CTX) )
	#define SHA512( PTR, LEN, DIGEST )			SHA512_compat( (PTR), (LEN), DIGEST )
#elif( TARGET_PLATFORM_WICED )
	#define MD5_DIGEST_LENGTH					16
	#define MD5_CTX								md5_context
	#define MD5_Init							md5_starts
	#define MD5_Update( CTX, PTR, LEN ) 		md5_update( (CTX), (unsigned char *)(PTR), (int)(LEN) )
	#define MD5_Final( DIGEST, CTX )			md5_finish( (CTX), (DIGEST) )
	
	#define SHA_DIGEST_LENGTH					20
	#define SHA_CTX								sha1_context
	#define SHA1_Init							sha1_starts
	#define SHA1_Update( CTX, PTR, LEN )		sha1_update( (CTX), (unsigned char *)(PTR), (int)(LEN) )
	#define SHA1_Final( DIGEST, CTX )			sha1_finish( (CTX), (DIGEST) )
	#define SHA1( PTR, LEN, DIGEST )			sha1( (unsigned char *)(PTR), (int)(LEN), (DIGEST) )
	
	#if( 0 ) // WICED has a SHA256_CTX typedef that conflicts so exclude SHA-256 until there's a better solution.
	#define SHA256_DIGEST_LENGTH				32
	#define SHA256_CTX							sha2_context
	#define SHA256_Init( CTX )					sha2_starts( (CTX), 0 )
	#define SHA256_Update( CTX, PTR, LEN )		sha2_update( (CTX), (unsigned char *)(PTR), (int)(LEN) )
	#define SHA256_Final( DIGEST, CTX )			sha2_finish( (CTX), (DIGEST) )
	#define SHA256( PTR, LEN, DIGEST )			sha2( (unsigned char *)(PTR), (int)(LEN), (DIGEST), 0 )
	#endif
	
	#define SHA512_DIGEST_LENGTH				64
	#define SHA512_CTX							SHA512_CTX_compat
	#define SHA512_Init( CTX )					SHA512_Init_compat( (CTX) )
	#define SHA512_Update( CTX, PTR, LEN )		SHA512_Update_compat( (CTX), (PTR), (LEN) )
	#define SHA512_Final( DIGEST, CTX )			SHA512_Final_compat( (DIGEST), (CTX) )
	#define SHA512( PTR, LEN, DIGEST )			SHA512_compat( (PTR), (LEN), DIGEST )
#elif( TARGET_HAS_SHA_UTILS )
	#define SHA_DIGEST_LENGTH					20
	#define SHA_CTX								SHA_CTX_compat
	#define SHA1_Init( CTX )					SHA1_Init_compat( (CTX) )
	#define SHA1_Update( CTX, PTR, LEN )		SHA1_Update_compat( (CTX), (PTR), (LEN) )
	#define SHA1_Final( DIGEST, CTX )			SHA1_Final_compat( (DIGEST), (CTX) )
	#define SHA1( PTR, LEN, DIGEST )			SHA1_compat( (PTR), (LEN), DIGEST )
	
	#define SHA512_DIGEST_LENGTH				64
	#define SHA512_CTX							SHA512_CTX_compat
	#define SHA512_Init( CTX )					SHA512_Init_compat( (CTX) )
	#define SHA512_Update( CTX, PTR, LEN )		SHA512_Update_compat( (CTX), (PTR), (LEN) )
	#define SHA512_Final( DIGEST, CTX )			SHA512_Final_compat( (DIGEST), (CTX) )
	#define SHA512( PTR, LEN, DIGEST )			SHA512_compat( (PTR), (LEN), DIGEST )
#endif

#define SHA3_DIGEST_LENGTH						64
#define SHA3_CTX								SHA3_CTX_compat
#define SHA3_Init( CTX )						SHA3_Init_compat( (CTX) )
#define SHA3_Update( CTX, PTR, LEN )			SHA3_Update_compat( (CTX), (PTR), (LEN) )
#define SHA3_Final( DIGEST, CTX )				SHA3_Final_compat( (DIGEST), (CTX) )
#define SHA3( PTR, LEN, DIGEST )				SHA3_compat( (PTR), (LEN), DIGEST )

// Curve25519

	#define curve25519( RESULT, SK, BASE )		curve25519_donna( (RESULT), (SK), (BASE) )

// Ed25519

	#define Ed25519_make_key_pair( PK, SK )				ed25519_make_key_pair_ref( (PK), (SK) )
	#define Ed25519_sign( SIG, MSG, LEN, PK, SK )		ed25519_sign_ref( (SIG), (MSG), (LEN), (PK), (SK) )
	#define Ed25519_verify( MSG, LEN, SIG, PK )			ed25519_verify_ref( (MSG), (LEN), (SIG), (PK) )

#define kEd25519_PublicKeyBytes			32
#define kEd25519_SecretKeyBytes			32
#define kEd25519_SignatureBytes			64

// HKDF

	#define HKDF_SHA512( INPUT_PTR, INPUT_LEN, SALT_PTR, SALT_LEN, INFO_PTR, INFO_LEN, OUTPUT_LEN, OUTPUT_PTR ) \
		HKDF_SHA512_compat( (INPUT_PTR), (INPUT_LEN), (SALT_PTR), (SALT_LEN), (INFO_PTR), (INFO_LEN), \
			(OUTPUT_LEN), (OUTPUT_PTR) )

// Variable argument macro support. Use ANSI C99 __VA_ARGS__ where possible. Otherwise, use the next best thing.

#if( defined( __GNUC__ ) )
	#if( ( __GNUC__ > 3 ) || ( ( __GNUC__ == 3 ) && ( __GNUC_MINOR__ >= 2 ) ) )
		#define	TARGET_HAS_C99_VA_ARGS		1
		#define	TARGET_HAS_GNU_VA_ARGS		0
	#else
		#define	TARGET_HAS_C99_VA_ARGS		0
		#define	TARGET_HAS_GNU_VA_ARGS		1
	#endif
	#define	TARGET_HAS_VA_ARG_MACROS		1
#elif( TARGET_VISUAL_STUDIO_2005_OR_LATER )
	#define	TARGET_HAS_C99_VA_ARGS			1
	#define	TARGET_HAS_GNU_VA_ARGS			0
	#define	TARGET_HAS_VA_ARG_MACROS		1
#elif( defined( __MWERKS__ ) )
	#define	TARGET_HAS_C99_VA_ARGS			1
	#define	TARGET_HAS_GNU_VA_ARGS			0
	#define	TARGET_HAS_VA_ARG_MACROS		1
#else
	#define	TARGET_HAS_C99_VA_ARGS			0
	#define	TARGET_HAS_GNU_VA_ARGS			0
	#define	TARGET_HAS_VA_ARG_MACROS		0
#endif

#if 0
#pragma mark == CPU ==
#endif

//===========================================================================================================================
//	CPU
//===========================================================================================================================

#if( !defined( TARGET_CPU_ARM ) )
// ARM
	#if( defined( __arm__ ) || defined( __arm ) || defined( __ARM ) || defined( __THUMB ) )
		#define	TARGET_CPU_ARM				1
	#else
		#define	TARGET_CPU_ARM				0
	#endif
#endif

#if( !defined( TARGET_CPU_ARM64 ) )
	#if( defined( __arm64__ ) || defined( __arm64 ) )
		#define	TARGET_CPU_ARM64			1
	#else
		#define	TARGET_CPU_ARM64			0
	#endif
#endif

#if( !defined( TARGET_CPU_MIPS ) )
// MIPS
	#if( defined( __mips__ ) || defined( __MIPS__ ) || defined( MIPS32 ) || defined( __MIPSEB__ ) || defined( __MIPSEL__ ) )
		#define	TARGET_CPU_MIPS				1
	#elif( defined( R3000 ) || defined( R4000 ) || defined( R4650 ) || defined( _M_MRX000 ) )
		#define	TARGET_CPU_MIPS				1
	#else
		#define	TARGET_CPU_MIPS				0
	#endif
#endif

#if( !defined( TARGET_CPU_PPC ) )
// PowerPC
	#if( defined( __ppc__ ) || defined( __POWERPC__ ) || defined( __PPC__ ) || defined( powerpc ) || defined( ppc ) || defined( _M_MPPC ) )
		#define	TARGET_CPU_PPC				1
	#else
		#define	TARGET_CPU_PPC				0
	#endif
#endif
#if( !defined( TARGET_CPU_PPC64 ) )
	#if( defined( __ppc64__ ) )
		#define	TARGET_CPU_PPC64			1
	#else
		#define	TARGET_CPU_PPC64			0
	#endif
#endif

#if( !defined( TARGET_CPU_X86 ) )
// x86
	#if( defined( __i386__ ) || defined( __INTEL__ ) || defined( i386 ) || defined( intel ) || defined( _M_IX86 ) )
		#define	TARGET_CPU_X86				1
	#else
		#define	TARGET_CPU_X86				0
	#endif
#endif
#if( !defined( TARGET_CPU_X86_64 ) )
	#if( defined( __x86_64__ ) || defined( _M_X64 ) || defined( _M_AMD64 ) || defined( _M_IA64 ) )
		#define	TARGET_CPU_X86_64			1
	#else
		#define	TARGET_CPU_X86_64			0
	#endif
#endif

// TARGET_NEEDS_NATURAL_ALIGNMENT - CPU requires naturally aligned accesses or an exception occurs.

#if( !defined( TARGET_NEEDS_NATURAL_ALIGNMENT ) )
	#if( TARGET_CPU_PPC || TARGET_CPU_PPC64 || TARGET_CPU_X86 || TARGET_CPU_X86_64 )
		#define	TARGET_NEEDS_NATURAL_ALIGNMENT		0
	#else
		#define	TARGET_NEEDS_NATURAL_ALIGNMENT		1
	#endif
#endif

// 32-bit and 64-bit support to avoid relying on platform-specific conditionals in code outside of this file.
// See also <http://msdn.microsoft.com/msdnmag/issues/06/05/x64/default.aspx> for Windows 64 bit stuff.

#if( !defined( TARGET_RT_64_BIT ) )
	#if( ( defined( __LP64__ ) && __LP64__ ) || defined( _WIN64 ) || defined( EFI64 ) || defined( EFIX64 ) || \
		TARGET_CPU_X86_64 || TARGET_CPU_PPC64 )
		#define	TARGET_RT_64_BIT		1
	#else
		#define	TARGET_RT_64_BIT		0
	#endif
#endif

#if( !defined( TARGET_RT_32_BIT ) )
	#if( TARGET_RT_64_BIT )
		#define	TARGET_RT_32_BIT		0
	#else
		#define	TARGET_RT_32_BIT		1
	#endif
#endif

#if 0
#pragma mark == Byte Order ==
#endif

//===========================================================================================================================
//	Byte Order
//===========================================================================================================================

// TARGET_RT_LITTLE_ENDIAN

#if( !defined( TARGET_RT_LITTLE_ENDIAN ) )
	#if( ( defined( TARGET_RT_BIG_ENDIAN ) && !TARGET_RT_BIG_ENDIAN )										|| \
		 ( defined(   BYTE_ORDER ) && defined(   LITTLE_ENDIAN ) && (   BYTE_ORDER ==   LITTLE_ENDIAN ) )	|| \
		 ( defined(  _BYTE_ORDER ) && defined(  _LITTLE_ENDIAN ) && (  _BYTE_ORDER ==  _LITTLE_ENDIAN ) )	|| \
		 ( defined( __BYTE_ORDER ) && defined( __LITTLE_ENDIAN ) && ( __BYTE_ORDER == __LITTLE_ENDIAN ) )	|| \
		 ( COMPILER_ARM_REALVIEW && !defined( __BIG_ENDIAN ) )												|| \
		 ( defined( IL_LITTLE_ENDIAN ) && IL_LITTLE_ENDIAN )												|| \
		 defined( __LITTLE_ENDIAN__ ) || defined( EFI_NET_LITTLE_ENDIAN ) || defined( __LITTLEENDIAN__ )	|| \
		 defined( __LittleEndian ) || defined( IP_LITTLE_ENDIAN ) 											|| \
		 TARGET_CPU_X86 || TARGET_CPU_X86_64 || defined( __MIPSEL__ ) || defined( MIPSEL )					|| \
		 defined( vax ) || defined( __ARMEL__ ) )
		#define	TARGET_RT_LITTLE_ENDIAN		1
	#else
		#define	TARGET_RT_LITTLE_ENDIAN		0
	#endif
#endif

// TARGET_RT_BIG_ENDIAN

#if( !defined( TARGET_RT_BIG_ENDIAN ) )
	#if( ( defined( TARGET_RT_LITTLE_ENDIAN ) && !TARGET_RT_LITTLE_ENDIAN )										|| \
		 ( defined(   BYTE_ORDER ) && defined(   BIG_ENDIAN ) && (   BYTE_ORDER ==   BIG_ENDIAN ) )				|| \
		 ( defined(  _BYTE_ORDER ) && defined(  _BIG_ENDIAN ) && (  _BYTE_ORDER ==  _BIG_ENDIAN ) )				|| \
		 ( defined( __BYTE_ORDER ) && defined( __BIG_ENDIAN ) && ( __BYTE_ORDER == __BIG_ENDIAN ) )				|| \
		 ( COMPILER_ARM_REALVIEW && defined( __BIG_ENDIAN ) )													|| \
		 ( defined( IL_BIG_ENDIAN ) && IL_BIG_ENDIAN )															|| \
		 defined( __BIG_ENDIAN__ ) || defined( EFI_NET_BIG_ENDIAN ) || defined( __BIGENDIAN__ )					|| \
		 defined( __BigEndian ) || defined( IP_BIG_ENDIAN ) 													|| \
		 TARGET_CPU_PPC || TARGET_CPU_PPC64 || defined( __MIPSEB__ ) || defined( MIPSEB ) || defined( sparc ) 	|| \
		 defined( mc68000 ) || defined( sel ) || defined( __ARMEB__ ) )
		#define	TARGET_RT_BIG_ENDIAN		1
	#else
		#define	TARGET_RT_BIG_ENDIAN		0
	#endif
#endif

#if( !defined( TARGET_RT_LITTLE_ENDIAN ) || !defined( TARGET_RT_BIG_ENDIAN ) )
	#error unknown byte order - update this file to support your byte order
#endif

// TARGET_RT_BYTE_ORDER

#if( !defined( TARGET_RT_BYTE_ORDER_BIG_ENDIAN ) )
	#define	TARGET_RT_BYTE_ORDER_BIG_ENDIAN			1234
#endif

#if( !defined( TARGET_RT_BYTE_ORDER_LITTLE_ENDIAN ) )
	#define	TARGET_RT_BYTE_ORDER_LITTLE_ENDIAN		4321
#endif

#if( !defined( TARGET_RT_BYTE_ORDER ) )
	#if( TARGET_RT_LITTLE_ENDIAN )
		#define	TARGET_RT_BYTE_ORDER				TARGET_RT_BYTE_ORDER_LITTLE_ENDIAN
	#else
		#define	TARGET_RT_BYTE_ORDER				TARGET_RT_BYTE_ORDER_BIG_ENDIAN
	#endif
#endif

#if 0
#pragma mark == Compile Time Asserts ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	check_compile_time
	@abstract	Performs a compile-time check of something such as the size of an int.
	@discussion
	
	This declares an array with a size that is determined by a compile-time expression. If the expression evaluates 
	to 0, the array has a size of -1, which is illegal and generates a compile-time error.
	
	For example:
	
	check_compile_time( sizeof( int ) == 4 );
	
	Note: This only works with compile-time expressions.
	Note: This only works in places where extern declarations are allowed (e.g. global scope).
	
	References:
	
	<http://www.jaggersoft.com/pubs/CVu11_3.html>
	<http://www.jaggersoft.com/pubs/CVu11_5.html>
	
	Note: The following macros differ from the macros on the www.jaggersoft.com web site because those versions do not
	work with GCC due to GCC allowing a zero-length array. Using a -1 condition turned out to be more portable.
*/
#undef check_compile_time
#if( !defined( check_compile_time ) )
	#if( defined( __cplusplus ) )
		#define	check_compile_time( X )		extern "C" int compile_time_assert_failed[ (X) ? 1 : -1 ]
	#else
		#define	check_compile_time( X )		extern int compile_time_assert_failed[ (X) ? 1 : -1 ]
	#endif
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	check_compile_time_code
	@abstract	Perform a compile-time check, suitable for placement in code, of something such as the size of an int.
	@discussion
	
	This creates a switch statement with an existing case for 0 and an additional case using the result of a 
	compile-time expression. A switch statement cannot have two case labels with the same constant so if the
	compile-time expression evaluates to 0, it is illegal and generates a compile-time error. If the compile-time
	expression does not evaluate to 0, the resulting value is used as the case label and it compiles without error.

	For example:
	
	check_compile_time_code( sizeof( int ) == 4 );
	
	Note: This only works with compile-time expressions.
	Note: This does not work in a global scope so it must be inside a function.
	
	References:
	
	<http://www.jaggersoft.com/pubs/CVu11_3.html>
	<http://www.jaggersoft.com/pubs/CVu11_5.html>
*/
#undef check_compile_time_code
#if( !defined( check_compile_time_code ) )
	#if( TARGET_VISUAL_STUDIO_2005_OR_LATER )
		#define	check_compile_time_code( X ) \
			do \
			{ \
				_Pragma( warning( push ) ) \
				_Pragma( warning( disable:6326 ) ) \
				switch( 0 ) { case 0: case X:; } \
				_Pragma( warning( pop ) ) \
			}	while( 0 )
	#else
		#define	check_compile_time_code( X )	switch( 0 ) { case 0: case X:; }
	#endif
#endif

#if 0
#pragma mark == Types ==
#endif

//===========================================================================================================================
//	Standard Types
//===========================================================================================================================

#if( TARGET_LANGUAGE_C_LIKE )

// Primitive types

#if( !defined( INT8_MIN ) && !defined( _INT8_T_DECLARED ) && !defined( __MWERKS__ ) && !defined( __intptr_t_defined ) )
	
	#define INT8_MIN					SCHAR_MIN
	
	#if( COMPILER_VISUAL_CPP )

		// C99 stdint.h not supported in VC++/VS.NET yet.

		typedef INT8					int8_t;
		typedef UINT8					uint8_t;
		typedef INT16					int16_t;
		typedef UINT16					uint16_t;
		typedef INT32					int32_t;
		typedef UINT32					uint32_t;
		typedef __int64					int64_t;
		typedef unsigned __int64		uint64_t;
		
		typedef int64_t					intmax_t;
		typedef uint64_t				uintmax_t;
	
	#elif( TARGET_OS_VXWORKS && ( TORNADO_VERSION < 220 ) )
		typedef long long				int64_t;
		typedef unsigned long long		uint64_t;
		
		typedef int64_t					intmax_t;
		typedef uint64_t				uintmax_t;
	#endif
	
	typedef int8_t						int_least8_t;
	typedef int16_t						int_least16_t;
	typedef int32_t						int_least32_t;
	typedef int64_t						int_least64_t;

	typedef uint8_t						uint_least8_t;
	typedef uint16_t					uint_least16_t;
	typedef uint32_t					uint_least32_t;
	typedef uint64_t					uint_least64_t;
	
	#if( !TARGET_OS_NETBSD && !TARGET_OS_THREADX )
		typedef int8_t					int_fast8_t;
		typedef int16_t					int_fast16_t;
		typedef int32_t					int_fast32_t;
		typedef int64_t					int_fast64_t;
		
		typedef uint8_t					uint_fast8_t;
		typedef uint16_t				uint_fast16_t;
		typedef uint32_t				uint_fast32_t;
		typedef uint64_t				uint_fast64_t;
	#endif
	
	#if( ( !COMPILER_VISUAL_CPP && !TARGET_OS_THREADX ) || TARGET_OS_WINDOWS_CE )
		#if( !defined( intptr_t ) )
			typedef long				intptr_t;
		#endif
		#if( !defined( uintptr_t ) )
			typedef unsigned long		uintptr_t;
		#endif
	#endif

#endif


#if( TARGET_OS_WINDOWS_KERNEL )
	#define CHAR_BIT				8
	
	typedef int						ptrdiff_t;
#endif

#if( TARGET_KERNEL )
	typedef __PTRDIFF_TYPE__		ptrdiff_t;
#endif

#if( TARGET_OS_NETBSD && TARGET_KERNEL )
	typedef _BSD_PTRDIFF_T_			ptrdiff_t;
#endif

#if( TARGET_LANGUAGE_C_LIKE )
	#if( ( TARGET_OS_WINDOWS || ( !defined( _BSD_SSIZE_T_DEFINED_ ) && !defined( _SSIZE_T ) ) ) && !TARGET_OS_VXWORKS && !TARGET_OS_LINUX && !TARGET_OS_NETBSD )
		typedef int		ssize_t;
	#endif
#endif

// int128 support

typedef struct
{
	int64_t		hi;
	uint64_t	lo;
	
}	int128_compat;

typedef struct
{
	uint64_t	hi;
	uint64_t	lo;

}	uint128_compat;

#if( !defined( TARGET_HAS_NATIVE_INT128 ) )
	#if( TARGET_RT_64_BIT && ( COMPILER_CLANG >= 30100 ) )
		#define TARGET_HAS_NATIVE_INT128		1
		typedef __int128						int128_t;
		typedef unsigned __int128				uint128_t;
	#elif( TARGET_RT_64_BIT && __GNUC__ )
		#if( __SIZEOF_INT128__ )
			#define TARGET_HAS_NATIVE_INT128	1
			typedef __int128					int128_t;
			typedef unsigned __int128			uint128_t;
		#elif( COMPILER_GCC >= 40400 )
			#define TARGET_HAS_NATIVE_INT128	1
			typedef signed						int128_t  __attribute__( ( mode( TI ) ) );
			typedef unsigned					uint128_t __attribute__( ( mode( TI ) ) );
		#endif
	#endif
#endif

// Vector types

#if( defined( __SSE2__ ) && __SSE2__ )
	typedef uint32_t	uint32x4_t __attribute__( ( vector_size( 16 ) ) );
#endif

// Limits -- primitive types.

#if( !defined( SCHAR_MIN ) )
	#define SCHAR_MIN		( -128 )
#endif

#if( !defined( UCHAR_MAX ) )
	#define UCHAR_MAX		255
#endif

#if( !defined( SHRT_MIN ) )
	#define SHRT_MIN		( -32768 )
#endif

#if( !defined( USHRT_MAX ) )
	#define USHRT_MAX		65535
#endif

#if( !defined( UINT_MAX ) )
	#define UINT_MAX		0xFFFFFFFFU
#endif

#if( !defined( INT_MIN ) )
	#define INT_MIN		( -2147483647 - 1 )
#endif

#if( !defined( INT_MAX ) )
	#define INT_MAX		2147483647
#endif

#if( !defined( SIZE_MAX ) )
	#define SIZE_MAX	2147483647
#endif

// Limits -- sized types.

#if( !defined( INT8_MAX ) )
	#define INT8_MAX		127
#endif
#if( !defined( INT16_MAX ) )
	#define INT16_MAX		32767
#endif
#if( !defined( INT32_MAX ) )
	#define INT32_MAX		2147483647
#endif
#if( !defined( INT64_MAX ) )
	#define INT64_MAX		 INT64_C( 9223372036854775807 )
#endif

#if( !defined( INT8_MIN ) )
	#define INT8_MIN		( -128 )
#endif
#if( !defined( INT16_MIN ) )
	#define INT16_MIN		( -32768 )
#endif
#if( !defined( INT32_MIN ) )
	#define INT32_MIN		( -INT32_MAX - 1 )
#endif
#if( !defined( INT64_MIN ) )
	#define INT64_MIN        (-INT64_MAX - 1 )
#endif

#if( !defined( UINT8_MAX ) )
	#define UINT8_MAX		255
#endif
#if( !defined( UINT16_MAX ) )
	#define UINT16_MAX		65535
#endif
#if( !defined( UINT32_MAX ) )
	#define UINT32_MAX		UINT32_C( 4294967295 )
#endif
#if( !defined( UINT64_MAX ) )
	#define UINT64_MAX		UINT64_C( 18446744073709551615 )
#endif

#if( !defined( __MACTYPES__ ) && !defined( __MACTYPES_H__ ) && !defined( __COREFOUNDATION_CFBASE__ ) )
	typedef float		Float32; // 32 bit IEEE float: 1 sign bit, 8 exponent bits, 23 fraction bits.
	typedef double		Float64; // 64 bit IEEE float: 1 sign bit, 11 exponent bits, 52 fraction bits.
	
	check_compile_time( sizeof( Float32 ) == 4 );
	check_compile_time( sizeof( Float64 ) == 8 );
#endif

// Macros for minimum-width integer constants

#if( !defined( INT8_C ) )
	#define INT8_C( value )			value
#endif

#if( !defined( INT16_C ) )
	#define INT16_C( value )		value
#endif

#if( !defined( INT32_C ) )
	#define INT32_C( value )		value
#endif

#define INT64_C_safe( value )		INT64_C( value )
#if( !defined( INT64_C ) )
	#if( COMPILER_VISUAL_CPP )
		#define INT64_C( value )	value ## i64
	#else
		#define INT64_C( value )	value ## LL
	#endif
#endif

#define UINT8_C_safe( value )		UINT8_C( value )
#if( !defined( UINT8_C ) )
	#define UINT8_C( value )		value ## U
#endif

#define UINT16_C_safe( value )		UINT16_C( value )
#if( !defined( UINT16_C ) )
	#define UINT16_C( value )		value ## U
#endif

#define UINT32_C_safe( value )		UINT32_C( value )
#if( !defined( UINT32_C ) )
	#define UINT32_C( value )		value ## U
#endif

#define UINT64_C_safe( value )		UINT64_C( value )
#if( !defined( UINT64_C ) )
	#if( COMPILER_VISUAL_CPP )
		#define UINT64_C( value )	value ## UI64
	#else
		#define UINT64_C( value )	value ## ULL
	#endif
#endif

// Value16 -- 16-bit union of a bunch of types.

typedef union
{
	char		c[ 2 ];
	uint8_t		u8[ 2 ];
	int8_t		s8[ 2 ];
	uint16_t	u16;
	int16_t		s16;
	
}	Value16;

check_compile_time( sizeof( Value16 ) == 2 );

// Value32 -- 32-bit union of a bunch of types.

typedef union
{
	char		c[ 4 ];
	uint8_t		u8[ 4 ];
	int8_t		s8[ 4 ];
	uint16_t	u16[ 2 ];
	int16_t		s16[ 2 ];
	uint32_t	u32;
	int32_t		s32;
	Float32		f32;
	
}	Value32;

check_compile_time( sizeof( Value32 ) == 4 );

// Value64 -- 64-bit union of a bunch of types.

typedef union
{
	char		c[ 8 ];
	uint8_t		u8[ 8 ];
	int8_t		s8[ 8 ];
	uint16_t	u16[ 4 ];
	int16_t		s16[ 4 ];
	uint32_t	u32[ 2 ];
	int32_t		s32[ 2 ];
	uint64_t	u64;
	int64_t		s64;
	Float32		f32[ 2 ];
	Float64		f64;
	
}	Value64;

check_compile_time( sizeof( Value64 ) == 8 );

// timespec

#if( TARGET_OS_WINDOWS )
	struct timespec
	{
		int32_t		tv_sec;
        int32_t		tv_nsec;
	};
#endif

// timeval

#if(   ( TARGET_PLATFORM_WICED && ( !defined( _TIMEVAL_DEFINED ) || !_TIMEVAL_DEFINED ) ) )
	struct timeval
	{
		int32_t		tv_sec;
		int32_t		tv_usec;
	};
#endif

#endif // TARGET_LANGUAGE_C_LIKE

#if 0
#pragma mark == Endian Swapping ==
#endif

//===========================================================================================================================
//	Alignment safe read/write/swap macros
//===========================================================================================================================

// Big endian reading

#define	ReadBig16( PTR ) \
	( (uint16_t)( \
		( ( (uint16_t)( (uint8_t *)(PTR) )[ 0 ] ) << 8 ) | \
		  ( (uint16_t)( (uint8_t *)(PTR) )[ 1 ] ) ) )

#define	ReadBig24( PTR ) \
	( (uint32_t)( \
		( ( (uint32_t)( (uint8_t *)(PTR) )[ 0 ] ) << 16 ) | \
		( ( (uint32_t)( (uint8_t *)(PTR) )[ 1 ] ) <<  8 ) | \
		  ( (uint32_t)( (uint8_t *)(PTR) )[ 2 ] ) ) )

#define	ReadBig32( PTR ) \
	( (uint32_t)( \
		( ( (uint32_t)( (uint8_t *)(PTR) )[ 0 ] ) << 24 ) | \
		( ( (uint32_t)( (uint8_t *)(PTR) )[ 1 ] ) << 16 ) | \
		( ( (uint32_t)( (uint8_t *)(PTR) )[ 2 ] ) <<  8 ) | \
		  ( (uint32_t)( (uint8_t *)(PTR) )[ 3 ] ) ) )

#define	ReadBig48( PTR ) \
	( (uint64_t)( \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 0 ] ) << 40 ) | \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 1 ] ) << 32 ) | \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 2 ] ) << 24 ) | \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 3 ] ) << 16 ) | \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 4 ] ) <<  8 ) | \
		  ( (uint64_t)( (uint8_t *)(PTR) )[ 5 ] ) ) )

#define	ReadBig64( PTR ) \
	( (uint64_t)( \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 0 ] ) << 56 ) | \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 1 ] ) << 48 ) | \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 2 ] ) << 40 ) | \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 3 ] ) << 32 ) | \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 4 ] ) << 24 ) | \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 5 ] ) << 16 ) | \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 6 ] ) <<  8 ) | \
		  ( (uint64_t)( (uint8_t *)(PTR) )[ 7 ] ) ) )

// Big endian writing

#define	WriteBig16( PTR, X ) \
	do \
	{ \
		( (uint8_t *)(PTR) )[ 0 ] = (uint8_t)( ( (X) >>  8 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 1 ] = (uint8_t)(   (X)         & 0xFF ); \
	\
	}	while( 0 )

#define	WriteBig24( PTR, X ) \
	do \
	{ \
		( (uint8_t *)(PTR) )[ 0 ] = (uint8_t)( ( (X) >> 16 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 1 ] = (uint8_t)( ( (X) >>  8 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 2 ] = (uint8_t)(   (X)         & 0xFF ); \
	\
	}	while( 0 )

#define	WriteBig32( PTR, X ) \
	do \
	{ \
		( (uint8_t *)(PTR) )[ 0 ] = (uint8_t)( ( (X) >> 24 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 1 ] = (uint8_t)( ( (X) >> 16 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 2 ] = (uint8_t)( ( (X) >>  8 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 3 ] = (uint8_t)(   (X)         & 0xFF ); \
	\
	}	while( 0 )

#define	WriteBig48( PTR, X ) \
	do \
	{ \
		( (uint8_t *)(PTR) )[ 0 ] = (uint8_t)( ( (X) >> 40 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 1 ] = (uint8_t)( ( (X) >> 32 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 2 ] = (uint8_t)( ( (X) >> 24 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 3 ] = (uint8_t)( ( (X) >> 16 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 4 ] = (uint8_t)( ( (X) >>  8 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 5 ] = (uint8_t)(   (X)         & 0xFF ); \
	\
	}	while( 0 )

#define	WriteBig64( PTR, X ) \
	do \
	{ \
		( (uint8_t *)(PTR) )[ 0 ] = (uint8_t)( ( (X) >> 56 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 1 ] = (uint8_t)( ( (X) >> 48 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 2 ] = (uint8_t)( ( (X) >> 40 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 3 ] = (uint8_t)( ( (X) >> 32 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 4 ] = (uint8_t)( ( (X) >> 24 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 5 ] = (uint8_t)( ( (X) >> 16 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 6 ] = (uint8_t)( ( (X) >>  8 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 7 ] = (uint8_t)(   (X)         & 0xFF ); \
	\
	}	while( 0 )

// Little endian reading

#if( TARGET_RT_LITTLE_ENDIAN && !TARGET_NEEDS_NATURAL_ALIGNMENT )
	#define	ReadLittle16( PTR )		( *( (uint16_t *)(PTR) ) )
#else
	#define	ReadLittle16( PTR ) \
		( (uint16_t)( \
			  ( (uint16_t)( (uint8_t *)(PTR) )[ 0 ] ) | \
			( ( (uint16_t)( (uint8_t *)(PTR) )[ 1 ] ) <<  8 ) ) )
#endif

#define	ReadLittle24( PTR ) \
	( (uint32_t)( \
		  ( (uint32_t)( (uint8_t *)(PTR) )[ 0 ] ) | \
		( ( (uint32_t)( (uint8_t *)(PTR) )[ 1 ] ) <<  8 ) | \
		( ( (uint32_t)( (uint8_t *)(PTR) )[ 2 ] ) << 16 ) ) )

#if( TARGET_RT_LITTLE_ENDIAN && !TARGET_NEEDS_NATURAL_ALIGNMENT )
	#define	ReadLittle32( PTR )		( *( (uint32_t *)(PTR) ) )
#else
	#define	ReadLittle32( PTR ) \
		( (uint32_t)( \
			  ( (uint32_t)( (uint8_t *)(PTR) )[ 0 ] ) | \
			( ( (uint32_t)( (uint8_t *)(PTR) )[ 1 ] ) <<  8 ) | \
			( ( (uint32_t)( (uint8_t *)(PTR) )[ 2 ] ) << 16 ) | \
			( ( (uint32_t)( (uint8_t *)(PTR) )[ 3 ] ) << 24 ) ) )
#endif

#define	ReadLittle48( PTR ) \
	( (uint64_t)( \
		  ( (uint64_t)( (uint8_t *)(PTR) )[ 0 ] )			| \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 1 ] ) <<  8 )	| \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 2 ] ) << 16 )	| \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 3 ] ) << 24 )	| \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 4 ] ) << 32 )	| \
		( ( (uint64_t)( (uint8_t *)(PTR) )[ 5 ] ) << 40 ) ) )

#if( TARGET_RT_LITTLE_ENDIAN && !TARGET_NEEDS_NATURAL_ALIGNMENT )
	#define	ReadLittle64( PTR )		( *( (uint64_t *)(PTR) ) )
#else
	#define	ReadLittle64( PTR ) \
		( (uint64_t)( \
			  ( (uint64_t)( (uint8_t *)(PTR) )[ 0 ] )			| \
			( ( (uint64_t)( (uint8_t *)(PTR) )[ 1 ] ) <<  8 )	| \
			( ( (uint64_t)( (uint8_t *)(PTR) )[ 2 ] ) << 16 )	| \
			( ( (uint64_t)( (uint8_t *)(PTR) )[ 3 ] ) << 24 )	| \
			( ( (uint64_t)( (uint8_t *)(PTR) )[ 4 ] ) << 32 )	| \
			( ( (uint64_t)( (uint8_t *)(PTR) )[ 5 ] ) << 40 )	| \
			( ( (uint64_t)( (uint8_t *)(PTR) )[ 6 ] ) << 48 )	| \
			( ( (uint64_t)( (uint8_t *)(PTR) )[ 7 ] ) << 56 ) ) )
#endif

// Little endian writing

#if( TARGET_RT_LITTLE_ENDIAN && !TARGET_NEEDS_NATURAL_ALIGNMENT )
	#define	WriteLittle16( PTR, X )		do { *( (uint16_t *)(PTR) ) = (uint16_t)(X); } while( 0 )
#else
	#define	WriteLittle16( PTR, X ) \
		do \
		{ \
			( (uint8_t *)(PTR) )[ 0 ] = (uint8_t)(   (X)         & 0xFF ); \
			( (uint8_t *)(PTR) )[ 1 ] = (uint8_t)( ( (X) >>  8 ) & 0xFF ); \
		\
		}	while( 0 )
#endif

#define	WriteLittle24( PTR, X ) \
	do \
	{ \
		( (uint8_t *)(PTR) )[ 0 ] = (uint8_t)(   (X)         & 0xFF ); \
		( (uint8_t *)(PTR) )[ 1 ] = (uint8_t)( ( (X) >>  8 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 2 ] = (uint8_t)( ( (X) >> 16 ) & 0xFF ); \
	\
	}	while( 0 )

#if( TARGET_RT_LITTLE_ENDIAN && !TARGET_NEEDS_NATURAL_ALIGNMENT )
	#define	WriteLittle32( PTR, X )		do { *( (uint32_t *)(PTR) ) = (uint32_t)(X); } while( 0 )
#else
	#define	WriteLittle32( PTR, X ) \
		do \
		{ \
			( (uint8_t *)(PTR) )[ 0 ] = (uint8_t)(   (X)         & 0xFF ); \
			( (uint8_t *)(PTR) )[ 1 ] = (uint8_t)( ( (X) >>  8 ) & 0xFF ); \
			( (uint8_t *)(PTR) )[ 2 ] = (uint8_t)( ( (X) >> 16 ) & 0xFF ); \
			( (uint8_t *)(PTR) )[ 3 ] = (uint8_t)( ( (X) >> 24 ) & 0xFF ); \
		\
		}	while( 0 )
#endif

#define	WriteLittle48( PTR, X ) \
	do \
	{ \
		( (uint8_t *)(PTR) )[ 0 ] = (uint8_t)(   (X)         & 0xFF ); \
		( (uint8_t *)(PTR) )[ 1 ] = (uint8_t)( ( (X) >>  8 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 2 ] = (uint8_t)( ( (X) >> 16 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 3 ] = (uint8_t)( ( (X) >> 24 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 4 ] = (uint8_t)( ( (X) >> 32 ) & 0xFF ); \
		( (uint8_t *)(PTR) )[ 5 ] = (uint8_t)( ( (X) >> 40 ) & 0xFF ); \
	\
	}	while( 0 )

#if( TARGET_RT_LITTLE_ENDIAN && !TARGET_NEEDS_NATURAL_ALIGNMENT )
	#define	WriteLittle64( PTR, X )		do { *( (uint64_t *)(PTR) ) = (uint64_t)(X); } while( 0 )
#else
	#define	WriteLittle64( PTR, X ) \
		do \
		{ \
			( (uint8_t *)(PTR) )[ 0 ] = (uint8_t)(   (X)         & 0xFF ); \
			( (uint8_t *)(PTR) )[ 1 ] = (uint8_t)( ( (X) >>  8 ) & 0xFF ); \
			( (uint8_t *)(PTR) )[ 2 ] = (uint8_t)( ( (X) >> 16 ) & 0xFF ); \
			( (uint8_t *)(PTR) )[ 3 ] = (uint8_t)( ( (X) >> 24 ) & 0xFF ); \
			( (uint8_t *)(PTR) )[ 4 ] = (uint8_t)( ( (X) >> 32 ) & 0xFF ); \
			( (uint8_t *)(PTR) )[ 5 ] = (uint8_t)( ( (X) >> 40 ) & 0xFF ); \
			( (uint8_t *)(PTR) )[ 6 ] = (uint8_t)( ( (X) >> 48 ) & 0xFF ); \
			( (uint8_t *)(PTR) )[ 7 ] = (uint8_t)( ( (X) >> 56 ) & 0xFF ); \
		\
		}	while( 0 )
#endif

// Floating point

STATIC_INLINE Float32	ReadBigFloat32( const void *p )				{ Value32 v32; v32.u32 = ReadBig32( p ); return( v32.f32 ); }
STATIC_INLINE Float64	ReadBigFloat64( const void *p )				{ Value64 v64; v64.u64 = ReadBig64( p ); return( v64.f64 ); }
STATIC_INLINE Float32	ReadLittleFloat32( const void *p )			{ Value32 v32; v32.u32 = ReadLittle32( p ); return( v32.f32 ); }
STATIC_INLINE Float64	ReadLittleFloat64( const void *p )			{ Value64 v64; v64.u64 = ReadLittle64( p ); return( v64.f64 ); }

STATIC_INLINE void		WriteBigFloat32( void *p, Float32 x )		{ Value32 v32; v32.f32 = x; WriteBig32( p, v32.u32 ); }
STATIC_INLINE void		WriteBigFloat64( void *p, Float64 x )		{ Value64 v64; v64.f64 = x; WriteBig64( p, v64.u64 ); }
STATIC_INLINE void		WriteLittleFloat32( void *p, Float32 x )	{ Value32 v32; v32.f32 = x; WriteLittle32( p, v32.u32 ); }
STATIC_INLINE void		WriteLittleFloat64( void *p, Float64 x )	{ Value64 v64; v64.f64 = x; WriteLittle64( p, v64.u64 ); }

// Host order read/write

#define Read8( PTR )					( ( (const uint8_t *)(PTR) )[ 0 ] )
#define Write8( PTR, X )				( ( (uint8_t *)(PTR) )[ 0 ] = (X) )

#if( TARGET_RT_BIG_ENDIAN )
	#define ReadHost16( PTR )			ReadBig16( (PTR) )
	#define ReadHost24( PTR )			ReadBig24( (PTR) )
	#define ReadHost32( PTR )			ReadBig32( (PTR) )
	#define ReadHost48( PTR )			ReadBig48( (PTR) )
	#define ReadHost64( PTR )			ReadBig64( (PTR) )
	
	#define WriteHost16( PTR, X )		WriteBig16( (PTR), (X) )
	#define WriteHost24( PTR, X )		WriteBig24( (PTR), (X) )
	#define WriteHost32( PTR, X )		WriteBig32( (PTR), (X) )
	#define WriteHost48( PTR, X )		WriteBig48( (PTR), (X) )
	#define WriteHost64( PTR, X )		WriteBig64( (PTR), (X) )
	
	#define ReadHostFloat32( PTR )		ReadBigFloat32( (PTR) )
	#define ReadHostFloat64( PTR )		ReadBigFloat64( (PTR) )
	
	#define WriteHostFloat32( PTR, X )	WriteBigFloat32( (PTR), (X) )
	#define WriteHostFloat64( PTR, X )	WriteBigFloat64( (PTR), (X) )
#else
	#define ReadHost16( PTR )			ReadLittle16( (PTR) )
	#define ReadHost24( PTR )			ReadLittle24( (PTR) )
	#define ReadHost32( PTR )			ReadLittle32( (PTR) )
	#define ReadHost48( PTR )			ReadLittle48( (PTR) )
	#define ReadHost64( PTR )			ReadLittle64( (PTR) )
	
	#define WriteHost16( PTR, X )		WriteLittle16( (PTR), (X) )
	#define WriteHost24( PTR, X )		WriteLittle24( (PTR), (X) )
	#define WriteHost32( PTR, X )		WriteLittle32( (PTR), (X) )
	#define WriteHost48( PTR, X )		WriteLittle48( (PTR), (X) )
	#define WriteHost64( PTR, X )		WriteLittle64( (PTR), (X) )
	
	#define ReadHostFloat32( PTR )		ReadLittleFloat32( (PTR) )
	#define ReadHostFloat64( PTR )		ReadLittleFloat64( (PTR) )
	
	#define WriteHostFloat32( PTR, X )	WriteLittleFloat32( (PTR), (X) )
	#define WriteHostFloat64( PTR, X )	WriteLittleFloat64( (PTR), (X) )
#endif

// Unconditional swap read/write.

#if( TARGET_RT_BIG_ENDIAN )
	#define ReadSwap16( PTR )			ReadLittle16( (PTR) )
	#define ReadSwap24( PTR )			ReadLittle24( (PTR) )
	#define ReadSwap32( PTR )			ReadLittle32( (PTR) )
	#define ReadSwap48( PTR )			ReadLittle48( (PTR) )
	#define ReadSwap64( PTR )			ReadLittle64( (PTR) )
	
	#define WriteSwap16( PTR, X )		WriteLittle16( (PTR), (X) )
	#define WriteSwap24( PTR, X )		WriteLittle24( (PTR), (X) )
	#define WriteSwap32( PTR, X )		WriteLittle32( (PTR), (X) )
	#define WriteSwap48( PTR, X )		WriteLittle48( (PTR), (X) )
	#define WriteSwap64( PTR, X )		WriteLittle64( (PTR), (X) )
	
	#define ReadSwapFloat32( PTR )		ReadLittleFloat32( (PTR) )
	#define ReadSwapFloat64( PTR )		ReadLittleFloat64( (PTR) )
	
	#define WriteSwapFloat32( PTR, X )	WriteLittleFloat32( (PTR), (X) )
	#define WriteSwapFloat64( PTR, X )	WriteLittleFloat64( (PTR), (X) )
#else
	#define ReadSwap16( PTR )			ReadBig16( (PTR) )
	#define ReadSwap24( PTR )			ReadBig24( (PTR) )
	#define ReadSwap32( PTR )			ReadBig32( (PTR) )
	#define ReadSwap48( PTR )			ReadBig48( (PTR) )
	#define ReadSwap64( PTR )			ReadBig64( (PTR) )
	
	#define WriteSwap16( PTR, X )		WriteBig16( (PTR), (X) )
	#define WriteSwap24( PTR, X )		WriteBig24( (PTR), (X) )
	#define WriteSwap32( PTR, X )		WriteBig32( (PTR), (X) )
	#define WriteSwap48( PTR, X )		WriteBig48( (PTR), (X) )
	#define WriteSwap64( PTR, X )		WriteBig64( (PTR), (X) )
	
	#define ReadSwapFloat32( PTR )		ReadBigFloat32( (PTR) )
	#define ReadSwapFloat64( PTR )		ReadBigFloat64( (PTR) )
	
	#define WriteSwapFloat32( PTR, X )	WriteBigFloat32( (PTR), (X) )
	#define WriteSwapFloat64( PTR, X )	WriteBigFloat64( (PTR), (X) )
#endif

// Conditional swaps

#if( TARGET_RT_BIG_ENDIAN )
	#define HostToLittle16( X )						Swap16( (X) )
	#define LittleToHost16( X )						Swap16( (X) )
	#define HostToLittle32( X )						Swap32( (X) )
	#define LittleToHost32( X )						Swap32( (X) )
	#define HostToLittle64( X )						Swap64( (X) )
	#define LittleToHost64( X )						Swap64( (X) )
	
	#define HostToBig16( X )						(X)
	#define BigToHost16( X )						(X)
	#define HostToBig32( X )						(X)
	#define BigToHost32( X )						(X)
	#define HostToBig64( X )						(X)
	#define BigToHost64( X )						(X)
	
	#define HostToBig16Mem( SRC, LEN, DST )			memmove( (DST), (SRC), (LEN) )
	#define BigToHost16Mem( SRC, LEN, DST )			memmove( (DST), (SRC), (LEN) )
	
	#define HostToLittle16Mem( SRC, LEN, DST )		Swap16Mem( (SRC), (LEN), (DST) )
	#define LittleToHost16Mem( SRC, LEN, DST )		Swap16Mem( (SRC), (LEN), (DST) )
	
	#define HostToBigMem( SRC, LEN, DST )			memmove( (DST), (SRC), (LEN) )
	#define BigToHostMem( SRC, LEN, DST )			memmove( (DST), (SRC), (LEN) )
	
	#define HostToLittleMem( SRC, LEN, DST )		MemReverse( (SRC), (LEN), (DST) )
	#define LittleToHostMem( SRC, LEN, DST )		MemReverse( (SRC), (LEN), (DST) )
#else
	#define HostToLittle16( X )						(X)
	#define LittleToHost16( X )						(X)
	#define HostToLittle32( X )						(X)
	#define LittleToHost32( X )						(X)
	#define HostToLittle64( X )						(X)
	#define LittleToHost64( X )						(X)
	
	#define HostToBig16( X )						Swap16( (X) )
	#define BigToHost16( X )						Swap16( (X) )
	#define HostToBig32( X )						Swap32( (X) )
	#define BigToHost32( X )						Swap32( (X) )
	#define HostToBig64( X )						Swap64( (X) )
	#define BigToHost64( X )						Swap64( (X) )
	
	#define HostToBig16Mem( SRC, LEN, DST )			Swap16Mem( (SRC), (LEN), (DST) )
	#define BigToHost16Mem( SRC, LEN, DST )			Swap16Mem( (SRC), (LEN), (DST) )
	
	#define HostToLittle16Mem( SRC, LEN, DST )		memmove( (DST), (SRC), (LEN) )
	#define LittleToHost16Mem( SRC, LEN, DST )		memmove( (DST), (SRC), (LEN) )
	
	#define HostToBigMem( SRC, LEN, DST )			MemReverse( (SRC), (LEN), (DST) )
	#define BigToHostMem( SRC, LEN, DST )			MemReverse( (SRC), (LEN), (DST) )
	
	#define HostToLittleMem( SRC, LEN, DST )		memmove( (DST), (SRC), (LEN) )
	#define LittleToHostMem( SRC, LEN, DST )		memmove( (DST), (SRC), (LEN) )
#endif

// Unconditional endian swaps

#define	Swap16( X ) \
	( (uint16_t)( \
		( ( ( (uint16_t)(X) ) << 8 ) & UINT16_C( 0xFF00 ) ) | \
		( ( ( (uint16_t)(X) ) >> 8 ) & UINT16_C( 0x00FF ) ) ) )

#define	Swap32( X ) \
	( (uint32_t)( \
		( ( ( (uint32_t)(X) ) << 24 ) & UINT32_C( 0xFF000000 ) ) | \
		( ( ( (uint32_t)(X) ) <<  8 ) & UINT32_C( 0x00FF0000 ) ) | \
		( ( ( (uint32_t)(X) ) >>  8 ) & UINT32_C( 0x0000FF00 ) ) | \
		( ( ( (uint32_t)(X) ) >> 24 ) & UINT32_C( 0x000000FF ) ) ) )

#define Swap64( X ) \
	( (uint64_t)( \
		( ( ( (uint64_t)(X) ) << 56 ) & UINT64_C( 0xFF00000000000000 ) ) | \
		( ( ( (uint64_t)(X) ) << 40 ) & UINT64_C( 0x00FF000000000000 ) ) | \
		( ( ( (uint64_t)(X) ) << 24 ) & UINT64_C( 0x0000FF0000000000 ) ) | \
		( ( ( (uint64_t)(X) ) <<  8 ) & UINT64_C( 0x000000FF00000000 ) ) | \
		( ( ( (uint64_t)(X) ) >>  8 ) & UINT64_C( 0x00000000FF000000 ) ) | \
		( ( ( (uint64_t)(X) ) >> 24 ) & UINT64_C( 0x0000000000FF0000 ) ) | \
		( ( ( (uint64_t)(X) ) >> 40 ) & UINT64_C( 0x000000000000FF00 ) ) | \
		( ( ( (uint64_t)(X) ) >> 56 ) & UINT64_C( 0x00000000000000FF ) ) ) )

// Host<->Network/Big endian swaps

#if( TARGET_RT_BIG_ENDIAN )
	#define hton16( X )		(X)
	#define ntoh16( X )		(X)
	
	#define hton32( X )		(X)
	#define ntoh32( X )		(X)
	
	#define hton64( X )		(X)
	#define ntoh64( X )		(X)
#else
	#define hton16( X )		Swap16( X )
	#define ntoh16( X )		Swap16( X )
	
	#define hton32( X )		Swap32( X )
	#define ntoh32( X )		Swap32( X )
	
	#define hton64( X )		Swap64( X )
	#define ntoh64( X )		Swap64( X )
#endif


#if 0
#pragma mark == Compatibility ==
#endif

//===========================================================================================================================
//	Compatibility
//===========================================================================================================================

#if( !defined( __BEGIN_DECLS ) )
	#if( defined( __cplusplus ) )
		#define __BEGIN_DECLS	extern "C" {
		#define __END_DECLS		}
	#else
		#define __BEGIN_DECLS
		#define __END_DECLS
	#endif
#endif

// Macros to allow the same code to work on Windows and other sockets API-compatible platforms.

#if( TARGET_OS_WINDOWS )
	#if( TARGET_LANGUAGE_C_LIKE )
		typedef SOCKET		SocketRef;
	#endif
	#define	IsValidSocket( X )					( (X) != INVALID_SOCKET )
	#define	kInvalidSocketRef					INVALID_SOCKET
	#define	close_compat( X )					closesocket( X )
	#define	read_compat( SOCK, BUF, SIZE )		recv( SOCK, BUF, SIZE, 0 )
	#define	write_compat( SOCK, BUF, SIZE )		send( SOCK, BUF, SIZE, 0 )
	
	#define	errno_compat()						(int) GetLastError()
	#define	set_errno_compat( X )				SetLastError( X )
	
	#if( TARGET_VISUAL_STUDIO_2010_OR_LATER )
		// Visual Studio 2010 defines these in errno.h, but we want the WinSock versions.
		
		#undef ECANCELED
		#undef ECONNRESET
		#undef ENOTCONN
		#undef ETIMEDOUT
		#undef EWOULDBLOCK
	#endif
	#define	ECONNRESET				WSAECONNRESET
	#define	ENOTCONN				WSAENOTCONN
	#define	ETIMEDOUT				WSAETIMEDOUT
	#define	EWOULDBLOCK				WSAEWOULDBLOCK
	#define	SHUT_WR_COMPAT			SD_SEND
	#define ECANCELED				WSAECANCELLED
	#define EHOSTDOWN				WSAEHOSTDOWN
	#define ETHER_ADDR_LEN			6
	#define IF_NAMESIZE				64
	#if( !defined( IFF_POINTOPOINT ) )
		#define IFF_POINTOPOINT		IFF_POINTTOPOINT
	#endif
	#define NAME_MAX				MAX_PATH
	#define PATH_MAX				MAX_PATH
	#define S_ISDIR( MODE )			( ( MODE ) & _S_IFDIR )
	#define S_ISREG( MODE )			( ( MODE ) & _S_IFREG )
	#define SO_REUSEPORT			SO_REUSEADDR
	
	STATIC_INLINE struct tm *	gmtime_r( const time_t *inTime, struct tm *inTM )
	{
		return( gmtime_s( inTM, inTime ) ? NULL : inTM );
	}
	#define getpid()				_getpid()
	
	const char *	getprogname( void );
	
	#define fseeko( F, OFF, MODE )	_fseeki64( ( F ), ( OFF ), ( MODE ) )
	#define ftello( F )				_ftelli64( ( F ) )
	#define	sleep( SECS )			Sleep( (DWORD)( ( SECS ) * 1000 ) )
	#define strtoll					_strtoi64
	#define	usleep( USECS )			Sleep( (DWORD)( ( ( USECS ) / 1000 ) + ( ( ( USECS ) % 1000 ) ? 1 : 0 ) ) )
	
#elif( TARGET_OS_DARWIN_KERNEL )
	#define	SocketRef				socket_t
	#define	IsValidSocket( X )		( (X) != kInvalidSocketRef )
	#define	kInvalidSocketRef		( (SocketRef) NULL )
	#define	close_compat( X )		sock_close( X )
	
	#define getpid()				0
	#define getprogname()			"kernel"
	
#else
	#if( TARGET_OS_BSD )
		#if( !defined( lseek64 ) )
			#define lseek64						lseek // BSD's off_t is always 64 bit.
		#endif
	#endif
	
	
	#if( TARGET_LANGUAGE_C_LIKE )
		typedef int		SocketRef;
	#endif
	
	#if( TARGET_OS_LINUX )
		#ifdef __cplusplus
			extern "C" char *		program_invocation_short_name;
		#else
			extern char *			program_invocation_short_name;
		#endif
		#define getprogname()		program_invocation_short_name
	#endif
	#if( TARGET_OS_QNX )
		#ifdef __cplusplus
			extern "C" char *		__progname;
		#else
			extern char *			__progname;
		#endif
		STATIC_INLINE const char *	getprogname( void ) { return( __progname ); }
	#endif
	
	#if( TARGET_OS_THREADX )
		#define getprogname()		( tx_thread_identify()->tx_thread_name )
		#define	usleep( USECS )		tx_thread_sleep( (USECS) / 1000 )
	#else
		#define	IsValidSocket( X )					( (X) >= 0 )
		#define	kInvalidSocketRef					-1
		#define	close_compat( X )					close( X )
		#define	read_compat( SOCK, BUF, LEN )		read(  (SOCK), (BUF), (LEN) )
		#define	write_compat( SOCK, BUF, LEN )		write( (SOCK), (BUF), (LEN) )
	#endif
	
	#if( !TARGET_OS_THREADX )
		#define	errno_compat()				errno
		#define	set_errno_compat( X )		do { errno = (X); } while( 0 )
	#endif
	#if( defined( SHUT_WR ) )
		#define	SHUT_WR_COMPAT		SHUT_WR
	#else
		#define	SHUT_WR_COMPAT		1
	#endif
#endif

#if( !TARGET_OS_THREADX )
	#define errno_safe()	( errno_compat() ? errno_compat() : kUnknownErr )
#endif

// iovec

#if( TARGET_OS_WINDOWS )
	#define iovec_t			WSABUF
	#define iov_len			len
	#define iov_base		buf
#elif( TARGET_OS_POSIX || TARGET_KERNEL )
	#if( TARGET_LANGUAGE_C_LIKE )
		typedef struct iovec		iovec_t;
	#endif
#else
	typedef struct iovec	iovec_t;
	struct iovec
	{
		void *		iov_base;
		size_t		iov_len;
	};
#endif

#if( !defined( SETIOV ) )
	#define SETIOV( IOV, PTR, LEN ) \
		do \
		{ \
			(IOV)->iov_base = (void *)(PTR); \
			(IOV)->iov_len  = (LEN); \
		\
		}	while( 0 )
#endif

// Path Delimiters

#define kHFSPathDelimiterChar				':'
#define kHFSPathDelimiterString				":"

#define	kPOSIXPathDelimiterChar				'/'
#define	kPOSIXPathDelimiterString			"/"

#define	kWindowsPathDelimiterChar			'\\'
#define	kWindowsPathDelimiterString			"\\"

#if( TARGET_OS_WINDOWS )
	#define kNativePathDelimiterChar		kWindowsPathDelimiterChar
	#define kNativePathDelimiterString		kWindowsPathDelimiterString
#else
	#define kNativePathDelimiterChar		kPOSIXPathDelimiterChar
	#define kNativePathDelimiterString		kPOSIXPathDelimiterString
#endif

// FDRef for File Handles/Descriptors

#if( TARGET_OS_WINDOWS )
	#define TARGET_HAVE_FDREF		1
	
	typedef HANDLE		FDRef;
	
	#define IsValidFD( X )		( (X) != INVALID_HANDLE_VALUE )
	#define kInvalidFD			INVALID_HANDLE_VALUE
	#define CloseFD( X )		CloseHandle( X )
	
	STATIC_INLINE ssize_t	ReadFD( FDRef inFD, void *inBuf, size_t inLen )
	{
		BOOL		good;
		DWORD		nActual;
		
		good = ReadFile( inFD, inBuf, (DWORD) inLen, &nActual, NULL );
		if( good ) return( (ssize_t) nActual );
		return( -1 );
	}
	
	STATIC_INLINE ssize_t	WriteFD( FDRef inFD, const void *inBuf, size_t inLen )
	{
		BOOL		good;
		DWORD		nActual;
		
		good = WriteFile( inFD, inBuf, (DWORD) inLen, &nActual, NULL );
		if( good ) return( (ssize_t) nActual );
		return( -1 );
	}
	
#elif( TARGET_OS_POSIX )
	#define TARGET_HAVE_FDREF		1
	
	typedef int			FDRef;
	
	#define IsValidFD( X )				( (X) >= 0 )
	#define kInvalidFD					-1
	#define CloseFD( X )				close( X )
	#define ReadFD( FD, PTR, LEN )		read( FD, PTR, LEN )
	#define WriteFD( FD, PTR, LEN )		write( FD, PTR, LEN )
#endif

#if( TARGET_OS_WINDOWS )
	// Initialize/delete a critical section in a thread-safe manner. STATE must be a LONG.
	#define InitializeCriticalSectionOnce( CS_PTR, STATE_PTR ) \
		do \
		{ \
			for( ;; ) \
			{ \
				LONG		prev; \
				\
				prev = InterlockedCompareExchange( (STATE_PTR), 1, 0 ); \
				if( prev == 2 )			/* Already initialized. */ \
				{ \
					break; \
				} \
				else if( prev == 0 )	/* Got lock. Initialize it and release the lock. */ \
				{ \
					InitializeCriticalSection( (CS_PTR) ); \
					InterlockedExchange( (STATE_PTR), 2 ); \
					break; \
				} \
				else	/* Another thread is initializing it. Sleep while it completes. */ \
				{ \
					Sleep( 1 ); \
				} \
			} \
		\
		}	while( 0 )
	
	#define DeleteCriticalSectionOnce( CS_PTR, STATE_PTR, REUSE ) \
		do \
		{ \
			if( InterlockedCompareExchange( (STATE_PTR), (REUSE) ? 0 : 3, 2 ) == 2 ) \
			{ \
				DeleteCriticalSection( CS_PTR ); \
			} \
		\
		}	while( 0 )
#endif

// socklen_t is not defined on the following platforms so emulate it if not defined:
//
// - Pre-Panther Mac OS X. Panther defines SO_NOADDRERR so trigger off that.
// - Windows SDK prior to 2003. 2003+ SDK's define EAI_AGAIN so trigger off that.
// - VxWorks prior to PNE 2.2.1/IPv6.
// - EFI when not building with GCC.

#if( TARGET_LANGUAGE_C_LIKE )
	#if( ( TARGET_OS_DARWIN && !defined( SO_NOADDRERR ) ) ||   ( TARGET_OS_WINDOWS && !defined( EAI_AGAIN ) ) ||   ( TARGET_OS_VXWORKS &&  TORNADO_VERSION < 221  ) )
		typedef int		socklen_t;
	#endif
#endif

// EFI doesn't have stdarg.h or string.h, but it does have some equivalents so map them to the standard names.


// Darwin Kernel mappings.

#if( TARGET_OS_DARWIN_KERNEL )
	#define isalpha( X ) 				    ( ( ( (X) >= 'a' ) && ( (X) <= 'z' ) ) || \
											  ( ( (X) >= 'A' ) && ( (X) <= 'Z' ) ) )
	#define isdigit( X ) 				    ( ( (X) >= '0' ) && ( (X) <= '9' ) )
	#define isupper( X )					( ( (X) >= 'A' ) && ( (X) <= 'Z' ) )
	#define tolower( X )					( isupper( X ) ? ( (X) + 32 ) : (X) )
	
	#define	rand()							( (int) random() )
	
	extern void	*	_MALLOC( size_t inSize, int inType, int inFlags );
	extern void		_FREE( void *inPtr, int inType );
	
	#define TARGET_NO_REALLOC				1
	#define calloc( COUNT, SIZE )			_MALLOC( (COUNT) * (SIZE), M_TEMP, M_WAITOK | M_ZERO )
	#define malloc( SIZE )					_MALLOC( (SIZE), M_TEMP, M_WAITOK )
	STATIC_INLINE void	free( void *inPtr ) { _FREE( inPtr, M_TEMP ); }
	
	#define malloc_compat( SIZE )			malloc( (SIZE) )
	#define free_compat( PTR )				free( (PTR) )
	
	#define MALLOC_COMPAT_DEFINED			1
	
	char *	strrchr( const char *inStr, int inC );
	#define strspn( STR, SET )				strnspn( (STR), kSizeCString, (SET) )
#endif

// NetBSD Kernel mappings.

#if( TARGET_OS_NETBSD && TARGET_KERNEL )
	#define malloc_compat( SIZE )			malloc( (SIZE), M_TEMP, M_WAITOK )
	#define free_compat( PTR )				free( (PTR), M_TEMP )	
	
	#define MALLOC_COMPAT_DEFINED			1
	#define TARGET_NO_REALLOC				1
#endif

// Windows CE doesn't have strdup and Visual Studio 2005 marks strdup, stricmp, and strnicmp as a deprecated so map 
// them to their underscore variants on Windows so code can use the standard names on all platforms.

#if( TARGET_OS_WINDOWS )
	#define	strdup( X )					_strdup( X )
	#define	stricmp( A, B )				_stricmp( A, B )
	#define	strnicmp( A, B, SIZE )		_strnicmp( A, B, SIZE )
	
	#if( COMPILER_VISUAL_CPP < 1800 )
		#define va_copy( DST, SRC )		(DST) = (SRC)
	#endif
#endif

// Windows doesn't have snprintf/vsnprintf, but it does have _snprintf/_vsnprintf.
// Additionally, Visual Studio 2005 and later have deprecated these functions and replaced them with
// versions with an _s suffix (supposedly more secure). So just map the standard functions to those.

#if( TARGET_OS_WINDOWS && !defined( __MWERKS__ ) )
	#if( TARGET_VISUAL_STUDIO_2005_OR_LATER )
		#define	snprintf( BUF, COUNT, ... )		_snprintf_s( (BUF), (COUNT), (COUNT), __VA_ARGS__ )
	#else
		#define	snprintf		_snprintf
		#define	vsnprintf		_vsnprintf
	#endif
#endif

// Generic mappings.

#if( !defined( MALLOC_COMPAT_DEFINED ) )
	#define MALLOC_COMPAT_DEFINED		0
#endif
#if( !MALLOC_COMPAT_DEFINED )
	#define malloc_compat( SIZE )		malloc( (SIZE ) )
	#define free_compat( PTR )			free( (PTR) )
#endif
#if( !defined( TARGET_NO_REALLOC ) )
	#define TARGET_NO_REALLOC		0
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	likely/unlikely
	@abstract	Macro for Linux-style branch prediction.
	@discussion
	
	Use these macros like this:
	
	if( likely( ptr != NULL ) )
	{
		// This code path happens frequently (ptr is usually non-NULL).
	}
	else
	{
		// This code path rarely happens (ptr is rarely NULL).
	}
	
	if( unlikely( err != kNoErr ) )
	{
		// This code path rarely happens (errors rarely occur).
	}
	else
	{
		// This code path happens frequently (normally kNoErr).
	}
*/
#if( !defined( likely ) )
	#define likely( EXPRESSSION )		__builtin_expect( !!(EXPRESSSION), 1 )
#endif

#if( !defined( unlikely ) )
	#define unlikely( EXPRESSSION )		__builtin_expect( !!(EXPRESSSION), 0 )
#endif

// PRAGMA_PACKPUSH	- Compiler supports: #pragma pack(push, n)/pack(pop)
// PRAGMA_PACK		- Compiler supports: #pragma pack(n)
//
// Here's the common way to use these before declaring structures:
/*
	#if( PRAGMA_PACKPUSH )
		#pragma pack( push, 2 )
	#elif( PRAGMA_PACK )
		#pragma pack( 2 )
	#else
		#warning "FIX ME: packing not supported by this compiler"
	#endif
	
	Then after the declaring structures:
	
	#if( PRAGMA_PACKPUSH )
		#pragma pack( pop )
	#elif( PRAGMA_PACK )
		#pragma pack()
	#else
		#warning "FIX ME: packing not supported by this compiler"
	#endif
*/

#if( COMPILER_VISUAL_CPP || ( defined( __GNUC__ ) && TARGET_OS_DARWIN ) || defined( __ghs__ ) )
	#define PRAGMA_PACKPUSH		1
#else
	#define PRAGMA_PACKPUSH		0
#endif

#if( COMPILER_VISUAL_CPP || defined( __GNUC__ ) || defined( __MWERKS__ ) || defined( __ghs__ ) )
	#define PRAGMA_PACK			1
#else
	#define PRAGMA_PACK			0
#endif

#if( !defined( PRAGMA_STRUCT_PACKPUSH ) )
	#define PRAGMA_STRUCT_PACKPUSH		PRAGMA_PACKPUSH
#endif

#if( !defined( PRAGMA_STRUCT_PACK ) )
	#define PRAGMA_STRUCT_PACK			PRAGMA_PACK
#endif

// TARGET_HAS_BUILTIN_CLZ - Compiler supports __builtin_clz to count the number of leading zeros in an integer.

#if( TARGET_CPU_ARM && TARGET_OS_NETBSD )
	
	// The version of GCC we use doesn't map the __builtin_clz to the ARM clz instruction so re-map it.
	
	STATIC_INLINE unsigned int __builtin_clz_armv5( unsigned int x )
	{
		unsigned int		n;
		
		__asm__( "clz %0, %1" : "=r" (n) : "r" (x) );
		return( n );
	}
	
	#define __builtin_clz				__builtin_clz_armv5
	#define TARGET_HAS_BUILTIN_CLZ		1
	
#elif( COMPILER_CLANG || ( __GNUC__ >= 4 ) )
	#define TARGET_HAS_BUILTIN_CLZ		1
#elif( TARGET_VISUAL_STUDIO_2005_OR_LATER )
	STATIC_INLINE unsigned int	__builtin_clz( unsigned int x )
	{
		unsigned long		i;
		
		_BitScanReverse( &i, x );
		return( (unsigned int)( 31 - i ) );
	}
	
	#define TARGET_HAS_BUILTIN_CLZ		1
#else
	#define TARGET_HAS_BUILTIN_CLZ		0
#endif

// static_analyzer_cfretained -- Tells the static analyzer that a CF object was retained (e.g. by a called function).
// static_analyzer_cfreleased -- Tells the static analyzer that a CF object will be released (e.g. passed to a releasing function).
// Remove this when clang does inter-procedural analysis <rdar://problem/8178274>.

#ifdef __clang_analyzer__
	#define static_analyzer_cfretained( X )		CFRetain( (X) )
	#define static_analyzer_cfreleased( X )		CFRelease( (X) )
#else
	#define static_analyzer_cfretained( X )
	#define static_analyzer_cfreleased( X )
#endif

// static_analyzer_malloc_freed -- Tells the static analyzer that malloc'd memory will be freed (e.g. by a called function).
// Remove this when clang does inter-procedural analysis <rdar://problem/10925331>.

#ifdef __clang_analyzer__
	#define static_analyzer_malloc_freed( X )	free( (X) )
#else
	#define static_analyzer_malloc_freed( X )
#endif

// static_analyzer_mem_zeroed -- Tells the static analyzer that the memory has been zeroed.
// Remove this when clang fixes <radar:15309659>.

#ifdef __clang_analyzer__
	#define static_analyzer_mem_zeroed( PTR, LEN )	memset( (PTR), 0, (LEN) )
#else
	#define static_analyzer_mem_zeroed( PTR, LEN )
#endif

// static_analyzer_nsretained -- Tells the static analyzer that an NS object was retained (e.g. by a called function).
// static_analyzer_nsreleased -- Tells the static analyzer that an NS object will be released (e.g. passed to a releasing function).
// Remove this when clang does inter-procedural analysis <rdar://problem/8178274>.

#ifdef __clang_analyzer__
	#define static_analyzer_nsretained( X )		[(X) retain]
	#define static_analyzer_nsreleased( X )		[(X) release]
#else
	#define static_analyzer_nsretained( X )
	#define static_analyzer_nsreleased( X )
#endif

#if 0
#pragma mark -
#pragma mark == Compatibility - Includes ==
#endif

//===========================================================================================================================
//	Compatibility - Includes
//===========================================================================================================================

#if( !defined( FRAMEWORK_STYLE_INCLUDES ) )
	#define FRAMEWORK_STYLE_INCLUDES		1
#endif

// AUDIO_CONVERTER_HEADER -- Header file to include for AudioConverter support.

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	#define AUDIO_CONVERTER_HEADER		<AudioToolbox/AudioToolbox.h>
#else
	#define AUDIO_CONVERTER_HEADER		"AudioConverterLite.h"
	
	#if( !defined( AUDIO_CONVERTER_LITE_ENABLED ) )
		#define AUDIO_CONVERTER_LITE_ENABLED		1
	#endif
#endif

// COREAUDIO_HEADER -- Header file to include for CoreAudio types.

#if( TARGET_OS_DARWIN )
	#define COREAUDIO_HEADER		<CoreAudio/CoreAudio.h>
#elif( FRAMEWORK_STYLE_INCLUDES )
	#define COREAUDIO_HEADER		<CoreUtils/CommonServices.h>
#else
	#define COREAUDIO_HEADER		"CommonServices.h"
#endif

// CF_HEADER -- Header file to include for CoreFoundation support.

#if( !CFLITE_ENABLED )
	#define CF_HEADER		<CoreFoundation/CoreFoundation.h>
#elif( FRAMEWORK_STYLE_INCLUDES )
	#define CF_HEADER		<CoreUtils/CFCompat.h>
#else
	#define CF_HEADER		"CFCompat.h"
#endif

// CF_RUNTIME_HEADER -- Header file to include for CoreFoundation's runtime support.

#if( !CFLITE_ENABLED )
	#define CF_RUNTIME_HEADER		<CoreFoundation/CFRuntime.h>
#elif( FRAMEWORK_STYLE_INCLUDES )
	#define CF_RUNTIME_HEADER		<CoreUtils/CFCompat.h>
#else
	#define CF_RUNTIME_HEADER		"CFCompat.h"
#endif

// CURVE25519_HEADER -- Header file to include for Curve25519.

#if( TARGET_PLATFORM_WICED )
	#if( FRAMEWORK_STYLE_INCLUDES )
		#define CURVE25519_HEADER	<CoreUtils/Small25519.h>
	#else
		#define CURVE25519_HEADER	"Small25519.h"
	#endif
#elif( FRAMEWORK_STYLE_INCLUDES )
	#define CURVE25519_HEADER		<CoreUtils/curve25519-donna.h>
#else
	#define CURVE25519_HEADER		"curve25519-donna.h"
#endif

// ED25519_HEADER -- Header file to include for Ed25519.

#if( TARGET_PLATFORM_WICED )
	#if( FRAMEWORK_STYLE_INCLUDES )
		#define ED25519_HEADER		<CoreUtils/Small25519.h>
	#else
		#define ED25519_HEADER		"Small25519.h"
	#endif
#elif( FRAMEWORK_STYLE_INCLUDES )
	#define ED25519_HEADER			<CoreUtils/ed25519.h>
#else
	#define ED25519_HEADER			"ed25519.h"
#endif

// KEYCHAIN_HEADER -- Header file to include for Keychain support.

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	#define KEYCHAIN_HEADER			<Security/Security.h>
#elif( FRAMEWORK_STYLE_INCLUDES )
	#define KEYCHAIN_HEADER			<CoreUtils/KeychainLite.h>
#else
	#define KEYCHAIN_HEADER			"KeychainLite.h"
#endif

// LIBDISPATCH_HEADER -- Header file to include for libdispatch/GCD support.

#if( !DISPATCH_LITE_ENABLED )
	#define LIBDISPATCH_HEADER		<dispatch/dispatch.h>
#elif( FRAMEWORK_STYLE_INCLUDES )
	#define LIBDISPATCH_HEADER		<CoreUtils/DispatchLite.h>
#else
	#define LIBDISPATCH_HEADER		"DispatchLite.h"
#endif

// MD5_HEADER -- Header file to include for MD5 support.

#if( TARGET_HAS_COMMON_CRYPTO )
	#define MD5_HEADER		<CommonCrypto/CommonCrypto.h>
#elif( TARGET_HAS_MOCANA_SSL )
	#if( FRAMEWORK_STYLE_INCLUDES )
		#define MD5_HEADER	<CoreUtils/MD5Utils.h>
	#else
		#define MD5_HEADER	"MD5Utils.h"
	#endif
#elif( TARGET_PLATFORM_WICED )
	#define MD5_HEADER		"wiced_security.h"
#elif( TARGET_HAS_MD5_UTILS )
	#if( FRAMEWORK_STYLE_INCLUDES )
		#define MD5_HEADER	<CoreUtils/MD5Utils.h>
	#else
		#define MD5_HEADER	"MD5Utils.h"
	#endif
#elif( !TARGET_NO_OPENSSL )
	#define MD5_HEADER		<openssl/md5.h>
#else
	// Can't think of a better way without messy #if's at the call site so just include ourself.
	 
	#if( FRAMEWORK_STYLE_INCLUDES )
		#define MD5_HEADER	<CoreUtils/CommonServices.h>
	#else
		#define MD5_HEADER	"CommonServices.h"
	#endif
#endif

// SHA_HEADER -- Header file to include for SHA support.

#if( TARGET_HAS_COMMON_CRYPTO )
	#define SHA_HEADER		<CommonCrypto/CommonCrypto.h>
#elif( TARGET_HAS_MOCANA_SSL )
	#if( FRAMEWORK_STYLE_INCLUDES )
		#define SHA_HEADER	<CoreUtils/SHAUtils.h>
	#else
		#define SHA_HEADER	"SHAUtils.h"
	#endif
#elif( TARGET_PLATFORM_WICED )
	#define SHA_HEADER		"wiced_security.h"
#elif( TARGET_HAS_SHA_UTILS )
	#if( FRAMEWORK_STYLE_INCLUDES )
		#define SHA_HEADER	<CoreUtils/SHAUtils.h>
	#else
		#define SHA_HEADER	"SHAUtils.h"
	#endif
#elif( !TARGET_NO_OPENSSL )
	#define SHA_HEADER		<openssl/sha.h>
#else
	// Can't think of a better way without messy #if's at the call site so just include ourself.
	 
	#if( FRAMEWORK_STYLE_INCLUDES )
		#define SHA_HEADER	<CoreUtils/CommonServices.h>
	#else
		#define SHA_HEADER	"CommonServices.h"
	#endif
#endif

// XPC_HEADER -- Header file to include for XPC support.

#if( !XPC_LITE_ENABLED && TARGET_OS_DARWIN )
	#define XPC_HEADER		<xpc/xpc.h>
#elif( FRAMEWORK_STYLE_INCLUDES )
	#define XPC_HEADER		<CoreUtils/XPCLite.h>
#else
	#define XPC_HEADER		"XPCLite.h"
#endif

#if 0
#pragma mark -
#pragma mark == Compatibility - Networking ==
#endif

//===========================================================================================================================
//	Compatibility - Networking
//===========================================================================================================================

#if( !defined( AF_UNSPEC ) && !TARGET_NETWORK_NETX_DUO )
	#define AF_UNSPEC		0
	#define AF_INET			4
	#define AF_INET6		6
	#define AF_LINK			7
	
	#define LLADDR( X )		( (X)->sdl_data + (X)->sdl_nlen )
#endif

#if( !defined( SO_REUSEPORT ) )
	#define SO_REUSEPORT		SO_REUSEADDR
#endif

// sockaddr

	
// sockaddr_in


// sockaddr_in6


// sockaddr_dl


// sockaddr_ip -- like sockaddr_storage, but only for IPv4 and IPv6, doesn't require casts, and saves space.

#if( TARGET_LANGUAGE_C_LIKE && !TARGET_NETWORK_NETX_DUO && !defined( __TCP_CONNECTION_H__ ) )
	typedef union
	{
		struct sockaddr				sa;
		struct sockaddr_in			v4;
		#if( defined( AF_INET6 ) )
			struct sockaddr_in6		v6;
		#endif
		
	}	sockaddr_ip;
#endif

// Macros to workaround sin_len, etc. not being present on some platforms. SIN6_LEN seems like a reasonable indictor.

#if( !defined( SUN_LEN_SET ) )
	#if( defined( SIN6_LEN ) )
		#define	SUN_LEN_SET( X )	(X)->sun_len = (unsigned char) SUN_LEN( (X) )
	#else
		#define	SUN_LEN_SET( X )	do {} while( 0 )
	#endif
#endif

#if( !defined( SIN_LEN_SET ) )
	#if( defined( SIN6_LEN ) )
		#define	SIN_LEN_SET( X )	(X)->sin_len = (unsigned char) sizeof( struct sockaddr_in )
	#else
		#define	SIN_LEN_SET( X )	do {} while( 0 )
	#endif
#endif

#if( !defined( SIN6_LEN_SET ) )
	#if( defined( SIN6_LEN ) )
		#define	SIN6_LEN_SET( X )	(X)->sin6_len = (unsigned char) sizeof( struct sockaddr_in6 )
	#else
		#define	SIN6_LEN_SET( X )	do {} while( 0 )
	#endif
#endif

// Determines if a sockaddr is a link-local address whether IPv6 is supported or not.

#if( defined( AF_INET6 ) )
	#define	SockAddrIsLinkLocal( X )	( SockAddrIsIPv4LinkLocal( (X) ) || SockAddrIsIPv6LinkLocal( (X) ) )
#else
	#define	SockAddrIsLinkLocal( X )	SockAddrIsIPv4LinkLocal( (X) )
#endif

#define	SockAddrIsIPv4LinkLocal( X ) \
		( ( ( (const struct sockaddr *)(X) )->sa_family == AF_INET ) \
		  ? ( ( ( (uint8_t *)( &( (const struct sockaddr_in *)(X) )->sin_addr ) )[ 0 ] == 169 ) && \
			  ( ( (uint8_t *)( &( (const struct sockaddr_in *)(X) )->sin_addr ) )[ 1 ] == 254 ) ) \
		  : 0 )

#define	SockAddrIsIPv6LinkLocal( X ) \
		( ( ( (const struct sockaddr *)(X) )->sa_family == AF_INET6 ) \
		  ? IN6_IS_ADDR_LINKLOCAL( &( (const struct sockaddr_in6 *)(X) )->sin6_addr ) : 0 )

#if( !defined( IN6_IS_ADDR_LINKLOCAL ) )
	#define IN6_IS_ADDR_LINKLOCAL( a )	( ( (a)->s6_addr[ 0 ] == 0xfe ) && ( ( (a)->s6_addr[ 1 ] & 0xc0 ) == 0x80 ) )
#endif

// Determines if a sockaddr is a loopback address whether IPv6 is supported or not.

#if( defined( AF_INET6 ) )
	#define	SockAddrIsLoopBack( X ) \
		( ( ( (const struct sockaddr *)(X) )->sa_family == AF_INET ) \
		  ? ( ( (const struct sockaddr_in *)(X) )->sin_addr.s_addr == htonl( INADDR_LOOPBACK ) ) \
		  : ( ( (const struct sockaddr *)(X) )->sa_family == AF_INET6 ) \
			? IN6_IS_ADDR_LOOPBACK( &( (const struct sockaddr_in6 *)(X) )->sin6_addr ) \
			: 0 )
#else
	#define	SockAddrIsLoopBack( X ) \
		( ( ( (const struct sockaddr *)(X) )->sa_family == AF_INET ) \
		  ? ( ( (const struct sockaddr_in *)(X) )->sin_addr.s_addr == htonl( INADDR_LOOPBACK ) ) \
		  : 0 )
#endif

// Determines if a sockaddr is a multicast address whether IPv6 is supported or not.

#if( defined( AF_INET6 ) )
	#define	SockAddrIsMulticast( X ) \
		( ( ( (const struct sockaddr *)(X) )->sa_family == AF_INET ) \
		  ? ( ( ( (uint8_t *)( &( (const struct sockaddr_in *)(X) )->sin_addr ) )[ 0 ] & 0xF0 ) == 0xE0 ) \
		  : ( ( (const struct sockaddr *)(X) )->sa_family == AF_INET6 ) \
			? IN6_IS_ADDR_MULTICAST( &( (const struct sockaddr_in6 *)(X) )->sin6_addr ) \
			: 0 )
#else
	#define	SockAddrIsMulticast( X ) \
		( ( ( (const struct sockaddr *)(X) )->sa_family == AF_INET ) \
		  ? ( ( ( (uint8_t *)( &( (const struct sockaddr_in *)(X) )->sin_addr ) )[ 0 ] & 0xF0 ) == 0xE0 ) \
		  : 0 )
#endif

// Maps a sockaddr family to a string.

#if( defined( AF_INET6 ) && defined( AF_LINK ) )
	#define	SockAddrFamilyToString( X ) \
		( ( (X) == AF_INET )	? "AF_INET"  : \
		( ( (X) == AF_INET6 )	? "AF_INET6" : \
		( ( (X) == AF_LINK )	? "AF_LINK"  : \
								  "UNKNOWN" ) ) )
#elif( defined( AF_INET6 ) )
	#define	SockAddrFamilyToString( X ) \
		( ( (X) == AF_INET )	? "AF_INET"  : \
		( ( (X) == AF_INET6 )	? "AF_INET6" : \
								  "UNKNOWN" ) )
#elif( defined( AF_LINK ) )
	#define	SockAddrFamilyToString( X ) \
		( ( (X) == AF_INET )	? "AF_INET"  : \
		( ( (X) == AF_LINK )	? "AF_LINK"  : \
								  "UNKNOWN" ) )
#else
	#define	SockAddrFamilyToString( X ) \
		( ( (X) == AF_INET )	? "AF_INET"  : \
								  "UNKNOWN" )
#endif

// Determines if a 16-byte IPv6 address is an IPv4-mapped IPv6 address.

#define	IsIPv4MappedIPv6Address( A ) \
	(	( ( (const uint8_t *)(A) )[  0 ] == 0 )		&& \
		( ( (const uint8_t *)(A) )[  1 ] == 0 )		&& \
		( ( (const uint8_t *)(A) )[  2 ] == 0 )		&& \
		( ( (const uint8_t *)(A) )[  3 ] == 0 )		&& \
		( ( (const uint8_t *)(A) )[  4 ] == 0 )		&& \
		( ( (const uint8_t *)(A) )[  5 ] == 0 )		&& \
		( ( (const uint8_t *)(A) )[  6 ] == 0 )		&& \
		( ( (const uint8_t *)(A) )[  7 ] == 0 )		&& \
		( ( (const uint8_t *)(A) )[  8 ] == 0 )		&& \
		( ( (const uint8_t *)(A) )[  9 ] == 0 )		&& \
		( ( (const uint8_t *)(A) )[ 10 ] == 0xFF )	&& \
		( ( (const uint8_t *)(A) )[ 11 ] == 0xFF ) )

// Determines if a 16-byte IPv6 address is an IPv4-compatible IPv6 address.

#define	IsIPv4CompatibleIPv6Address( A ) \
	(	(	( ( (const uint8_t *)(A) )[  0 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[  1 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[  2 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[  3 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[  4 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[  5 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[  6 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[  7 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[  8 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[  9 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[ 10 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[ 11 ] == 0 ) )	&& \
		!(	( ( (const uint8_t *)(A) )[ 12 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[ 13 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[ 14 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[ 15 ] == 0 ) )	&& \
		!(	( ( (const uint8_t *)(A) )[ 12 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[ 13 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[ 14 ] == 0 )		&& \
			( ( (const uint8_t *)(A) )[ 15 ] == 1 ) ) )

#define kDNSServiceFlagsSuppressUnusable_compat		0x8000U		// Added in version 203 of dns_sd.h.
#define kDNSServiceFlagsUnicastResponse_compat		0x400000U	// Added in version 393 of dns_sd.h.
#define kDNSServiceFlagsThresholdOne_compat			0x2000000U	// Added in version 504 of dns_sd.h
#define kDNSServiceFlagsThresholdReached_compat		0x2000000U	// Added in version 504 of dns_sd.h

#if( !defined( kSecAttrAccessibleAlways_compat ) )
		#define kSecAttrAccessibleAlways_compat		kSecAttrAccessibleAlways
#endif

#if 0
#pragma mark -
#pragma mark == Compatibility - CoreAudio ==
#endif

//===========================================================================================================================
//	Compatibility - CoreAudio
//===========================================================================================================================

#if( !TARGET_OS_DARWIN || COMMON_SERVICES_NO_CORE_SERVICES )

#if( TARGET_RT_BIG_ENDIAN )
	#define kAudioFormatFlagsNativeEndian			kAudioFormatFlagIsBigEndian
#else
	#define kAudioFormatFlagsNativeEndian			0
#endif
#define kAudioFormatFlagIsFloat						( 1 << 0 )
#define kAudioFormatFlagIsBigEndian					( 1 << 1 )
#define kAudioFormatFlagIsPacked					( 1 << 3 )
#define kAudioFormatFlagIsSignedInteger				( 1 << 2 )
#define kAudioFormatFlagIsAlignedHigh				( 1 << 4 )
#define kAudioFormatFlagIsNonInterleaved			( 1 << 5 )
#define kAudioFormatFlagIsNonMixable				( 1 << 6 )

#define kAudioConverterDecompressionMagicCookie		0x646D6763 // 'dmgc'

#define kAudioFormatAppleLossless					0x616C6163 // 'alac'
	#define kAppleLosslessFormatFlag_16BitSourceData	1
	#define kAppleLosslessFormatFlag_20BitSourceData	2
	#define kAppleLosslessFormatFlag_24BitSourceData	3
	#define kAppleLosslessFormatFlag_32BitSourceData	4
#define kAudioFormatLinearPCM						0x6C70636D // 'lpcm'
	#define kLinearPCMFormatFlagIsFloat					kAudioFormatFlagIsFloat
	#define kLinearPCMFormatFlagIsBigEndian				kAudioFormatFlagIsBigEndian
	#define kLinearPCMFormatFlagIsSignedInteger			kAudioFormatFlagIsSignedInteger
	#define kLinearPCMFormatFlagIsPacked				kAudioFormatFlagIsPacked
	#define kLinearPCMFormatFlagIsAlignedHigh			kAudioFormatFlagIsAlignedHigh
	#define kLinearPCMFormatFlagIsNonInterleaved		kAudioFormatFlagIsNonInterleaved
	#define kLinearPCMFormatFlagIsNonMixable			kAudioFormatFlagIsNonMixable
	#define kLinearPCMFormatFlagsSampleFractionShift	7
	#define kLinearPCMFormatFlagsSampleFractionMask		( 0x3F << kLinearPCMFormatFlagsSampleFractionShift )

#define kAudioFormatMPEG4AAC						0x61616320 // 'aac '
#define kAudioFormatMPEG4AAC_ELD					0x61616365 // 'aace'
#define kAudioFormatOpus							0x6F707573 // 'opus'

typedef struct
{
	uint32_t		mSampleRate;
	uint32_t		mFormatID;
	uint32_t		mFormatFlags;
	uint32_t		mBytesPerPacket;
	uint32_t		mFramesPerPacket;
	uint32_t		mBytesPerFrame;
	uint32_t		mChannelsPerFrame;
	uint32_t		mBitsPerChannel;
	uint32_t		mReserved;
	
}	AudioStreamBasicDescription;

#endif // !TARGET_OS_DARWIN

#define kAudioMillisecondsPerPacket_Opus			20
#define kAudioSamplesPerPacket_AAC_ELD				480
#define kAudioSamplesPerPacket_AAC_LC				1024
#define kAudioSamplesPerPacket_ALAC_Small			352  // Sized for sending one frame per UDP packet.
#define kAudioSamplesPerPacket_ALAC_Default			4096

#define ASBD_FillAAC_ELD( FMT, RATE, CHANNELS ) \
	do \
	{ \
		(FMT)->mSampleRate			= (RATE); \
		(FMT)->mFormatID			= kAudioFormatMPEG4AAC_ELD; \
		(FMT)->mFormatFlags			= 0; \
		(FMT)->mBytesPerPacket		= 0; \
		(FMT)->mFramesPerPacket		= kAudioSamplesPerPacket_AAC_ELD; \
		(FMT)->mBytesPerFrame		= 0; \
		(FMT)->mChannelsPerFrame	= (CHANNELS); \
		(FMT)->mBitsPerChannel		= 0; \
		(FMT)->mReserved			= 0; \
		\
	}	while( 0 )

#define ASBD_FillAAC_LC( FMT, RATE, CHANNELS ) \
	do \
	{ \
		(FMT)->mSampleRate			= (RATE); \
		(FMT)->mFormatID			= kAudioFormatMPEG4AAC; \
		(FMT)->mFormatFlags			= 0; \
		(FMT)->mBytesPerPacket		= 0; \
		(FMT)->mFramesPerPacket		= kAudioSamplesPerPacket_AAC_LC; \
		(FMT)->mBytesPerFrame		= 0; \
		(FMT)->mChannelsPerFrame	= (CHANNELS); \
		(FMT)->mBitsPerChannel		= 0; \
		(FMT)->mReserved			= 0; \
		\
	}	while( 0 )

#define ASBD_FillALAC( FMT, RATE, BITS, CHANNELS ) \
	do \
	{ \
		(FMT)->mSampleRate			= (RATE); \
		(FMT)->mFormatID			= kAudioFormatAppleLossless; \
		(FMT)->mFormatFlags			= ( (BITS) == 16 ) ? kAppleLosslessFormatFlag_16BitSourceData : \
									  ( (BITS) == 20 ) ? kAppleLosslessFormatFlag_20BitSourceData : \
									  ( (BITS) == 24 ) ? kAppleLosslessFormatFlag_24BitSourceData : 0; \
		(FMT)->mBytesPerPacket		= 0; \
		(FMT)->mFramesPerPacket		= kAudioSamplesPerPacket_ALAC_Small; \
		(FMT)->mBytesPerFrame		= 0; \
		(FMT)->mChannelsPerFrame	= (CHANNELS); \
		(FMT)->mBitsPerChannel		= 0; \
		(FMT)->mReserved			= 0; \
		\
	}	while( 0 )

#define ASBD_FillAUCanonical( FMT, CHANNELS ) \
	do \
	{ \
		(FMT)->mSampleRate			= 48000; \
		(FMT)->mFormatID			= kAudioFormatLinearPCM; \
		(FMT)->mFormatFlags			= kAudioFormatFlagIsFloat | \
									  kAudioFormatFlagsNativeEndian | \
									  kAudioFormatFlagIsPacked | \
									  kAudioFormatFlagIsNonInterleaved; \
		(FMT)->mBytesPerPacket		= 4; \
		(FMT)->mFramesPerPacket		= 1; \
		(FMT)->mBytesPerFrame		= 4; \
		(FMT)->mChannelsPerFrame	= (CHANNELS); \
		(FMT)->mBitsPerChannel		= 32; \
		(FMT)->mReserved			= 0; \
		\
	}	while( 0 )

#define ASBD_FillOpus( FMT, RATE, CHANNELS ) \
	do \
	{ \
		(FMT)->mSampleRate			= (RATE); \
		(FMT)->mFormatID			= kAudioFormatOpus; \
		(FMT)->mFormatFlags			= 0; \
		(FMT)->mBytesPerPacket		= 0; \
		(FMT)->mFramesPerPacket		= (uint32_t)( ( (RATE) * kAudioMillisecondsPerPacket_Opus ) / 1000 ); \
		(FMT)->mBytesPerFrame		= 0; \
		(FMT)->mChannelsPerFrame	= (CHANNELS); \
		(FMT)->mBitsPerChannel		= 0; \
		(FMT)->mReserved			= 0; \
		\
	}	while( 0 )

#define ASBD_FillPCM( FMT, RATE, VALID_BITS, TOTAL_BITS, CHANNELS ) \
	do \
	{ \
		(FMT)->mSampleRate			= (RATE); \
		(FMT)->mFormatID			= kAudioFormatLinearPCM; \
		(FMT)->mFormatFlags			= kAudioFormatFlagsNativeEndian   | \
									  kAudioFormatFlagIsSignedInteger | \
									  ( ( (VALID_BITS) == (TOTAL_BITS) ) ? \
									  	kAudioFormatFlagIsPacked : \
									  	kAudioFormatFlagIsAlignedHigh ); \
		(FMT)->mBytesPerPacket		= (CHANNELS) * ( (TOTAL_BITS) / 8 ); \
		(FMT)->mFramesPerPacket		= 1; \
		(FMT)->mBytesPerFrame		= (CHANNELS) * ( (TOTAL_BITS) / 8 ); \
		(FMT)->mChannelsPerFrame	= (CHANNELS); \
		(FMT)->mBitsPerChannel		= (VALID_BITS); \
		(FMT)->mReserved			= 0; \
		\
	}	while( 0 )

#define ASBD_MakePCM( RATE, VALID_BITS, TOTAL_BITS, CHANNELS ) \
	{ \
		/* mSampleRate			*/	(RATE), \
		/* mFormatID			*/	kAudioFormatLinearPCM, \
		/* mFormatFlags 		*/	kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | \
									( ( (VALID_BITS) == (TOTAL_BITS) ) ? \
										kAudioFormatFlagIsPacked : \
										kAudioFormatFlagIsAlignedHigh ), \
		/* mBytesPerPacket		*/	(CHANNELS) * ( (TOTAL_BITS) / 8 ), \
		/* mFramesPerPacket		*/	1, \
		/* mBytesPerFrame		*/	(CHANNELS) * ( (TOTAL_BITS) / 8 ), \
		/* mChannelsPerFrame	*/	(CHANNELS), \
		/* mBitsPerChannel		*/	(VALID_BITS), \
		/* mReserved			*/	0 \
	}

typedef struct // From ACAppleLosslessCodec.h.
{
	uint32_t		frameLength;		// Note: AudioConverter expects this in big endian byte order.
	uint8_t			compatibleVersion;
	uint8_t			bitDepth;			// max 32
	uint8_t			pb;					// 0 <= pb <= 255
	uint8_t			mb;
	uint8_t			kb;
	uint8_t			numChannels;
	uint16_t		maxRun;				// Note: AudioConverter expects this in big endian byte order.
	uint32_t		maxFrameBytes;		// Note: AudioConverter expects this in big endian byte order.
	uint32_t		avgBitRate;			// Note: AudioConverter expects this in big endian byte order.
	uint32_t		sampleRate;			// Note: AudioConverter expects this in big endian byte order.
	
}	ALACParams;

#if 0
#pragma mark -
#pragma mark == Compatibility - Other ==
#endif

//===========================================================================================================================
//	Compatibility - Other
//===========================================================================================================================

// _beginthreadex and _endthreadex are not supported on Windows CE 2.1 or later (the C runtime issues with leaking 
// resources have apparently been resolved and they seem to have just ripped out support for the API) so map it to 
// CreateThread on Windows CE.

#if( TARGET_OS_WINDOWS_CE )
	#define	_beginthreadex_compat( SECURITY_PTR, STACK_SIZE, START_ADDRESS, ARG_LIST, FLAGS, THREAD_ID_PTR ) \
		(uintptr_t) CreateThread( SECURITY_PTR, STACK_SIZE, (LPTHREAD_START_ROUTINE) START_ADDRESS, ARG_LIST, FLAGS, \
					  (LPDWORD) THREAD_ID_PTR )
	
	#define	_endthreadex_compat( RESULT )		ExitThread( (DWORD)(RESULT) )
#elif( TARGET_OS_WINDOWS )
	#define	_beginthreadex_compat				_beginthreadex
	#define	_endthreadex_compat					_endthreadex
#endif

// GetProcAddress takes a char string on normal Windows, but a TCHAR string on Windows CE so hide this in a macro.

#if( TARGET_OS_WINDOWS_CE )
	#define	GetProcAddressCompat( LIB, STR )		GetProcAddress( LIB, TEXT( STR ) )
#elif( TARGET_OS_WINDOWS )
	#define	GetProcAddressCompat( LIB, STR )		GetProcAddress( LIB, STR )
#endif

// Calling conventions

#if( !defined( CALLBACK_COMPAT ) )
	#if( TARGET_OS_WINDOWS )
		#define	CALLBACK_COMPAT		CALLBACK
	#else
		#define	CALLBACK_COMPAT
	#endif
#endif

// CFEqual that's safe to call with NULL parameters.
#define CFEqualNullSafe( A, B )		( ( (A) == (B) ) || ( (A) && (B) && CFEqual( (A), (B) ) ) )

// CFEqual that's safe to call with NULL parameters and treats NULL and kCFNull as equal.
#define CFEqualNullSafeEx( A, B )	( ( (A) == (B) ) || CFEqual( (A) ? (A) : kCFNull, (B) ? (B) : kCFNull ) )

// Null-safe macro to make CF type checking easier.
#define CFIsType( OBJ, TYPE )		( (OBJ) && ( CFGetTypeID( (OBJ) ) == TYPE##GetTypeID() ) )

// CFRelease that's safe to call with NULL.
#define CFReleaseNullSafe( X )		do { if( (X) ) CFRelease( (X) ); } while( 0 )

// CFRetain that's safe to call with NULL.
#define CFRetainNullSafe( X )		do { if( (X) ) CFRetain( (X) ); } while( 0 )

#if( CFLITE_ENABLED )
	#define	kCFNumberSInt128Type_compat		kCFNumberSInt128Type
#else
	#define	kCFNumberSInt128Type_compat		17 // Keep in sync with Darwin CoreFoundation.
#endif

// CFString comparison with all the right options for sorting the way humans expect it.
#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	#define CFStringLocalizedStandardCompare( A, B ) \
		CFStringCompare( (A), (B), (CFStringCompareFlags)( \
			kCFCompareCaseInsensitive | \
			kCFCompareLocalized | \
			kCFCompareNumerically | \
			kCFCompareWidthInsensitive | \
			kCFCompareForcedOrdering ) )
#else
	#define CFStringLocalizedStandardCompare( A, B ) \
		CFStringCompare( (A), (B), kCFCompareCaseInsensitive | kCFCompareNumerically )
#endif

// HAS_CF_DISTRIBUTED_NOTIFICATIONS

#if( !defined( HAS_CF_DISTRIBUTED_NOTIFICATIONS ) )
	#if(  TARGET_OS_DARWIN  || TARGET_OS_WINDOWS )
		#define HAS_CF_DISTRIBUTED_NOTIFICATIONS		1
	#else
		#define HAS_CF_DISTRIBUTED_NOTIFICATIONS		0
	#endif
#endif


// NetBSD uses a uintptr_t for the udata field of the kevent structure, but other platforms use a void * so map it on NetBSD.

#if( TARGET_OS_NETBSD )
	#define	EV_SET_compat( kevp, a, b, c, d, e, f )	EV_SET( kevp, a, b, c, d, e, (uintptr_t)( f ) )
#else
	#define	EV_SET_compat( kevp, a, b, c, d, e, f )	EV_SET( kevp, a, b, c, d, e, f )
#endif

// MAP_ANONYMOUS is preferred on Linux and some other platforms so map MAP_ANON to that.

#if( TARGET_OS_DARWIN || TARGET_OS_QNX )
	#define MAP_ANONYMOUS		MAP_ANON
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	RandomRange
	@abstract	Returns a random number that's >= MIN and <= MAX (i.e. it's inclusive). MIN must be <= MAX.
*/
#define RandomRange( MIN, MAX )		( (MIN) + ( Random32() % ( ( (MAX) - (MIN) ) + 1 ) ) )
#define RandomRangeF( MIN, MAX )	( ( ( Random32() / 4294967295.0 ) * ( (MAX) - (MIN) ) ) + (MIN) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Random32
	@abstract	Returns a random number that usually has 30-32 bits of randomness and is reasonably fast.
*/
#if( TARGET_OS_DARWIN )
	#define Random32()		arc4random()
#elif( TARGET_OS_POSIX )
	#define Random32()		( (uint32_t) random() )
#else
	#define Random32()		( (uint32_t)( ( rand() * RAND_MAX ) + rand() ) )
#endif

#if 0
#pragma mark -
#pragma mark == Misc ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	PRINTF_STYLE_FUNCTION
	@abstract	Some compilers allow you to mark printf-style functions so their format string and argument list 
				is checked by the compiler. Adding PRINTF_STYLE_FUNCTION to a function enables this functionality.
	
	@param		FORMAT_INDEX		1's-based index of the format string (e.g. if the 2nd param is the format string, use 2).
	@param		ARGS_INDEX			1's-based index of the first arg. The function takes a va_list, use 0.
	
	@discussion	
	
	Here's an example of using it:
	
	void MyPrintF( const char *inFormat, ... ) PRINTF_STYLE_FUNCTION( 1, 2 );
	
	Many of the printf-style function provide more format string features than supported by GCC's printf checking
	(e.g. dlog supports %.4a for printing IPv4 addresses). GCC will flag these as errors so by default, printf-style
	function checking is disabled. To enable it, #define CHECK_PRINTF_STYLE_FUNCTIONS to 1.
*/
#if( !defined( CHECK_PRINTF_STYLE_FUNCTIONS ) )
	#define CHECK_PRINTF_STYLE_FUNCTIONS		0
#endif
#if( CHECK_PRINTF_STYLE_FUNCTIONS && defined( __GNUC__ ) )
	#define PRINTF_STYLE_FUNCTION( FORMAT_INDEX, ARGS_INDEX )	__attribute__( ( format( printf, FORMAT_INDEX, ARGS_INDEX ) ) )
#else
	#define PRINTF_STYLE_FUNCTION( FORMAT_INDEX, ARGS_INDEX )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	EXPORT_PACKAGE
	@abstract	Macro to mark a function as exported to other code within the same package.
*/
#if( TARGET_OS_DARWIN_KERNEL )
	#define	EXPORT_PACKAGE		__private_extern__
#else
	#define	EXPORT_PACKAGE
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	EXPORT_GLOBAL
	@abstract	Macro to mark a function as exported outside of the package.
	@discussion	
	
	GCC 4.0 and later have improved support for marking all symbols __private_extern__ automatically so you can
	explicitly mark only the functions you actually want exported. To make this work portably, EXPORT_GLOBAL lets 
	you mark a symbol globally when it is supported by the compiler. Only really needed for IOKit drivers and DLL.
	
	To export a class as global, declare it like this:
	
	class EXPORT_GLOBAL MyClass
	{
		... normal class declaration stuff
	};
	
	To export a function as global:
	
	EXPORT_GLOBAL void	MyFunction( void );
*/
#if( ( __GNUC__ > 4 ) || ( ( __GNUC__ == 4 ) && ( __GNUC_MINOR__ >= 0 ) ) )
	#define EXPORT_GLOBAL		__attribute__( ( visibility( "default" ) ) )
#else
	#define EXPORT_GLOBAL
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	IMPORT_GLOBAL
	@abstract	Macro to mark a data symbol as an DLL import.
	@discussion	
	
	To import, declare it like this
	
	IMPORT_GLOBAL int		gMyGlobal;
*/
#if( TARGET_OS_WINDOWS && !defined( _USRDLL ) )
	#define IMPORT_GLOBAL		__declspec( dllimport ) extern
#else
	#define IMPORT_GLOBAL		extern
#endif

#if 0
#pragma mark == ctype safe macros ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		ctype safe macros
	@abstract	Wrappers for the ctype.h macros make them safe when used with signed characters.
	@discussion
	
	Some implementations of the ctype.h macros use the character value to directly index into a table.
	This can lead to crashes and other problems when used with signed characters if the character value
	is greater than 127 because the values 128-255 will appear to be negative if viewed as a signed char.
	A negative subscript to an array causes it to index before the beginning and access invalid memory.
	
	To work around this, these *_safe wrappers mask the value and cast it to an unsigned char.
*/
#define isalnum_safe( X )		isalnum(  ( (unsigned char)( (X) & 0xFF ) ) )
#define isalpha_safe( X )		isalpha(  ( (unsigned char)( (X) & 0xFF ) ) )
#define iscntrl_safe( X )		iscntrl(  ( (unsigned char)( (X) & 0xFF ) ) )
#define isdigit_safe( X )		isdigit(  ( (unsigned char)( (X) & 0xFF ) ) )
#define isgraph_safe( X )		isgraph(  ( (unsigned char)( (X) & 0xFF ) ) )
#define islower_safe( X )		islower(  ( (unsigned char)( (X) & 0xFF ) ) )
#define isoctal_safe( X )		isoctal(  ( (unsigned char)( (X) & 0xFF ) ) )
#define isprint_safe( X )		isprint(  ( (unsigned char)( (X) & 0xFF ) ) )
#define ispunct_safe( X )		ispunct(  ( (unsigned char)( (X) & 0xFF ) ) )
#define isspace_safe( X )		isspace(  ( (unsigned char)( (X) & 0xFF ) ) )
#define isupper_safe( X )		isupper(  ( (unsigned char)( (X) & 0xFF ) ) )
#define isxdigit_safe( X )		isxdigit( ( (unsigned char)( (X) & 0xFF ) ) )
#define tolower_safe( X )		tolower(  ( (unsigned char)( (X) & 0xFF ) ) )
#define toupper_safe( X )		toupper(  ( (unsigned char)( (X) & 0xFF ) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		CoreFoundation object creation/subclassing.
	@abstract	Macros to make it easier to create CFType subclasses.
	@example
	
	struct MyClass
	{
		CFRuntimeBase		base; // CF type info. Must be first.
		
		... put any other fields you need here.
	};
	
	CF_CLASS_DEFINE( MyClass )
	
	OSStatus	MyClassCreate( MyClassRef *outObj )
	{
		OSStatus		err;
		MyClassRef		me;
		
		CF_OBJECT_CREATE( MyClass, me, err, exit );
		
		... object will be zero'd, but do any non-zero init you need here.
		
		*outObj = me;
		err = kNoErr;
		
	exit:
		return( err );
	}
	
	static void	_MyClassFinalize( CFTypeRef inCF )
	{
		MyClassRef const		me = (MyClassRef) inCF;
		
		... do any finalization you need here. Don't free the object itself (that's handled by CF after this returns).
	}
*/
#define CF_CLASS_DEFINE( NAME ) \
	static void	_ ## NAME ## Finalize( CFTypeRef inCF ); \
	\
	static dispatch_once_t			g ## NAME ## InitOnce = 0; \
	static CFTypeID					g ## NAME ## TypeID = _kCFRuntimeNotATypeID; \
	static const CFRuntimeClass		k ## NAME ## Class = \
	{ \
		0,						/* version */ \
		# NAME,					/* className */ \
		NULL,					/* init */ \
		NULL,					/* copy */ \
		_ ## NAME ## Finalize,	/* finalize */ \
		NULL,					/* equal -- NULL means pointer equality. */ \
		NULL,					/* hash  -- NULL means pointer hash. */ \
		NULL,					/* copyFormattingDesc */ \
		NULL,					/* copyDebugDesc */ \
		NULL,					/* reclaim */ \
		NULL					/* refcount */ \
	}; \
	\
	static void _ ## NAME ## GetTypeID( void *inContext ) \
	{ \
		(void) inContext; \
		\
		g ## NAME ## TypeID = _CFRuntimeRegisterClass( &k ## NAME ## Class ); \
		check( g ## NAME ## TypeID != _kCFRuntimeNotATypeID ); \
	} \
	\
	CFTypeID	NAME ## GetTypeID( void ) \
	{ \
		dispatch_once_f( &g ## NAME ## InitOnce, NULL, _ ## NAME ## GetTypeID ); \
		return( g ## NAME ## TypeID ); \
	} \
	\
	check_compile_time( sizeof_field( struct NAME ## Private, base ) == sizeof( CFRuntimeBase ) ); \
	check_compile_time( offsetof( struct NAME ## Private, base ) == 0 )

#define CF_OBJECT_CREATE( NAME, OBJ, ERR, EXIT_LABEL ) \
	do \
	{ \
		size_t		extraLen; \
		\
		extraLen = sizeof( *OBJ ) - sizeof( OBJ->base ); \
		OBJ = (NAME ## Ref) _CFRuntimeCreateInstance( NULL, NAME ## GetTypeID(), (CFIndex) extraLen, NULL ); \
		require_action( OBJ, EXIT_LABEL, ERR = kNoMemoryErr ); \
		memset( ( (uint8_t *) OBJ ) + sizeof( OBJ->base ), 0, extraLen ); \
		\
	}	while( 0 )

#if 0
#pragma mark == Macros ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	kSizeCString
	@abstract	A meta-value to pass to supported routines to indicate the size should be calculated with strlen.
*/
#define	kSizeCString		( (size_t) -1 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	countof
	@abstract	Determines the number of elements in an array.
*/
#define	countof( X )						( sizeof( X ) / sizeof( X[ 0 ] ) )
#define	countof_field( TYPE, FIELD )		countof( ( (TYPE *) 0 )->FIELD )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	offsetof
	@abstract	Number of bytes from the beginning of the type to the specified field.
*/
#if( !defined( offsetof ) )
	#define offsetof( TYPE, FIELD )		( (size_t)(uintptr_t)( &( (TYPE *) 0 )->FIELD ) )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	sizeof_element
	@abstract	Determines the size of an array element.
*/
#define	sizeof_element( X )		sizeof( X[ 0 ] )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	sizeof_field
	@abstract	Determines the size of a field of a type.
*/
#define	sizeof_field( TYPE, FIELD )		sizeof( ( ( (TYPE *) 0 )->FIELD ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	sizeof_string
	@abstract	Determines the size of a constant C string, excluding the null terminator.
*/
#define	sizeof_string( X )		( sizeof( (X) ) - 1 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AbsoluteDiff
	@abstract	Returns the absolute value of the difference between two values.
*/
#define	AbsoluteDiff( X, Y )		( ( (X) < (Y) ) ? ( (Y) - (X) ) : ( (X) - (Y) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AbsoluteValue
	@abstract	Returns the absolute value of a value.
*/
#define	AbsoluteValue( X )		( ( (X) < 0 ) ? -(X) : (X) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	RoundDown
	@abstract	Rounds VALUE down to the nearest multiple of MULTIPLE.
*/
#define	RoundDown( VALUE, MULTIPLE )		( ( (VALUE) / (MULTIPLE) ) * (MULTIPLE) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	RoundUp
	@abstract	Rounds VALUE up to the nearest multiple of MULTIPLE.
*/
#define	RoundUp( VALUE, MULTIPLE )		( ( ( (VALUE) + ( (MULTIPLE) - 1 ) ) / (MULTIPLE) ) * (MULTIPLE) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	RoundTo
	@abstract	Rounds a value to a specific precision.
	@discussion
	
	This can round to any arbitrary precision. To round to a specific number of decimal digits, use a precision that is 
	pow( 10, -digits ). For example, for 2 decimal places, pow( 10, -2 ) -> .01 and RoundTo( 1.234, .01 ) -> 1.23. This
	can also be used to round to other precisions, such as 1/8: RoundTo( 1.3, 1.0 / 8.0 ) -> 1.25.
*/
#define RoundTo( VALUE, PRECISION )		( floor( ( (VALUE) / (PRECISION) ) + 0.5 ) * (PRECISION) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsAligned
	@abstract	Returns non-zero if X is aligned to a Y byte boundary and 0 if not. Y must be a power of 2.
*/
#define	IsAligned( X, Y )		( ( (X) & ( (Y) - 1 ) ) == 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsFieldAligned
	@abstract	Returns non-zero if FIELD of type TYPE is aligned to a Y byte boundary and 0 if not. Y must be a power of 2.
*/
#define	IsFieldAligned( X, TYPE, FIELD, Y )		IsAligned( ( (uintptr_t)(X) ) + offsetof( TYPE, FIELD ), (Y) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PtrsOverlap
	@abstract	Returns true if the two ptr/len pairs overlap each other.
*/
#define	PtrsOverlap( PTR1, LEN1, PTR2, LEN2 ) \
	( !( ( ( ( (uintptr_t)(PTR1) ) + (LEN1) )	<=   ( (uintptr_t)(PTR2) ) ) || \
	       ( ( (uintptr_t)(PTR1) )				>= ( ( (uintptr_t)(PTR2) ) + (LEN2) ) ) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsPtrAligned
	@abstract	Returns non-zero if PTR is aligned to a Y byte boundary and 0 if not. Y must be a power of 2.
*/
#define	IsPtrAligned( PTR, Y )		( ( ( (uintptr_t)(PTR) ) & ( (Y) - 1 ) ) == 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AlignDown
	@abstract	Aligns X down to a Y byte boundary. Y must be a power of 2.
*/
#define	AlignDown( X, Y )		( (X) & ~( (Y) - 1 ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AlignUp
	@abstract	Aligns X up to a Y byte boundary. Y must be a power of 2.
*/
#define	AlignUp( X, Y )		( ( (X) + ( (Y) - 1 ) ) & ~( (Y) - 1 ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AlignedBuffer
	@abstract	Specifies a buffer of a specific number of bytes that is aligned to the strictest C alignment.
	@discussion
	
	This is useful for things like defining a buffer on the stack that can be cast to structures that may need alignment.
	For example, the following allocates a 128 byte buffer on the stack that is safe to cast as a uint64_t:
	
	AlignedBuffer( 128 )		buf;
	uint64_t *					ptr;
	
	ptr = (uint64_t *) &buf; // This is safe because the buffer is guaranteed to be aligned for the largest type.
*/
#define	AlignedBuffer( SIZE ) \
	union \
	{ \
		uintmax_t		align; \
		uint8_t			buf[ SIZE ]; \
	}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Logical and arithmetic shifts on signed values.
	@abstract	These work around undefined behavior in C for shifts of signed values.
	
	ASR = Arithmetic Shift Right. The sign bit is replicated to fill in vacant positions (e.g. 0x80 >> 1 = 0xC0).
	LSR = Logical Shift Right. Zero bits fill in vacant positions (e.g. 0x80 >> 1 = 0x40).
	X is the value and N is the number of bits to shift. The return value contains the result.
	
	Warning: shifting a signed value to the right is not the same as dividing because shifts round down instead of 
	toward zero so -1 >> 1 = -1 instead of 0. If you care about the low bit then you'll need something better.
*/
#define HighOnes32( N )		( ~( (uint32_t) 0 ) << ( 32 - (N) ) )
#define HighOnes64( N )		( ~( (uint64_t) 0 ) << ( 64 - (N) ) )

#define LSR32( X, N )		( (int32_t)(   ( (uint32_t)(X) ) >> (N) ) )
#define ASR32( X, N )		( (int32_t)( ( ( (uint32_t)(X) ) >> (N) ) ^ ( ( ( (int32_t)(X) ) < 0 ) ? HighOnes32( (N) ) : 0 ) ) )

#define LSR64( X, N )		( (int64_t)( (   (uint64_t)(X) ) >> (N) ) )
#define ASR64( X, N )		( (int64_t)( ( ( (uint64_t)(X) ) >> (N) ) ^ ( ( ( (int64_t)(X) ) < 0 ) ? HighOnes64( (N) ) : 0 ) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		BitRotates
	@abstract	Rotates X COUNT bits to the left or right.
*/
#define ROTL( X, N, SIZE )			( ( (X) << (N) ) | ( (X) >> ( (SIZE) - N ) ) )
#define ROTR( X, N, SIZE )			( ( (X) >> (N) ) | ( (X) << ( (SIZE) - N ) ) )

#define ROTL32( X, N )				ROTL( (X), (N), 32 )
#define ROTR32( X, N )				ROTR( (X), (N), 32 )

#define ROTL64( X, N )				ROTL( (X), (N), 64 )
#define ROTR64( X, N )				ROTR( (X), (N), 64 )

#define	RotateBitsLeft( X, N )		ROTL( (X), (N), sizeof( (X) ) * 8 )
#define	RotateBitsRight( X, N )		ROTR( (X), (N), sizeof( (X) ) * 8 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Divide64x32
	@abstract	Divides a 64-bit value by a 32-bit divisor and fills in the remainder.
*/
#define	Divide64x32( VALUE, DIVISOR, REMAIN ) \
	do \
	{ \
		if( (VALUE) <= UINT64_C( 0xFFFFFFFF ) ) \
		{ \
			REMAIN = ( (uint32_t)(VALUE) ) % (DIVISOR); \
			VALUE  = ( (uint32_t)(VALUE) ) / (DIVISOR); \
		} \
		else \
		{ \
			REMAIN = (uint32_t)( (VALUE) % (DIVISOR) ); \
			VALUE /= (DIVISOR); \
		} \
		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsOdd
	@abstract	Returns non-zero if the value is odd and 0 if it is even.
*/
#define	IsOdd( X )			( ( (X) & 1 ) != 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsEven
	@abstract	Returns non-zero if the value is even and 0 if it is odd.
*/
#define	IsEven( X )			( ( (X) & 1 ) == 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsPowerOf2
	@abstract	Returns non-zero if the value is a power of 2 and 0 if it is not. 0 and 1 are not considered powers of 2.
*/
#define	IsPowerOf2( X )		( ( (X) > 1 ) && ( ( (X) & ( (X) - 1 ) ) == 0 ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MinPowerOf2BytesForValue
	@abstract	Returns the minimum number of power-of-2 bytes needed to hold a specific value.
*/
#define MinPowerOf2BytesForValue( X )	( \
	( (X) & UINT64_C( 0xFFFFFFFF00000000 ) ) ? 8 : \
	( (X) & UINT64_C( 0x00000000FFFF0000 ) ) ? 4 : \
	( (X) & UINT64_C( 0x000000000000FF00 ) ) ? 2 : \
											   1 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsMultipleOf
	@abstract	Returns non-zero if X is a multiple of Y.
*/
#define IsMultipleOf( X, Y )		( ( ( (X) / (Y) ) * (Y) ) == (X) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Min
	@abstract	Returns the lesser of X and Y.
*/
#if( !defined( Min ) )
	#define	Min( X, Y )		( ( (X) < (Y) ) ? (X) : (Y) )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Max
	@abstract	Returns the greater of X and Y.
*/
#if( !defined( Max ) )
	#define	Max( X, Y )		( ( (X) > (Y) ) ? (X) : (Y) )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Clamp
	@abstract	Clamps a value to no less than "a" and no greater than "b".
*/
#define Clamp( x, a, b )		Max( (a), Min( (b), (x) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	increment_wrap
	@abstract	Increments VAR and if it wraps to 0, set VAR to WRAP.
*/
#define	increment_wrap( VAR, WRAP )		do { ++(VAR); if( (VAR) == 0 ) { (VAR) = (WRAP); } } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	increment_saturate
	@abstract	Increments VAR unless doing so would cause it to exceed MAX.
*/
#define	increment_saturate( VAR, MAX )		do { if( (VAR) < (MAX) ) { ++(VAR); } } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	add_saturate
	@abstract	Adds VALUE to VAR. If the result would go over MAX, VAR is capped to MAX.
*/
#define	add_saturate( VAR, VALUE, MAX ) \
	do \
	{ \
		if( (VAR) < ( (MAX) - (VALUE) ) ) \
		{ \
			(VAR) += (VALUE); \
		} \
		else \
		{ \
			(VAR) = (MAX); \
		} \
		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	median_of_3
	@abstract	Returns the median (middle) value given 3 values.
*/
#define median_of_3( x0, x1, x2 ) \
	( ( (x0) > (x1) ) ? \
		( (x1) > (x2) ) ? (x1) : ( (x2) > (x0) ) ? (x0) : (x2) : \
		( (x1) < (x2) ) ? (x1) : ( (x2) < (x0) ) ? (x0) : (x2) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	median_of_5
	@abstract	Returns the median (middle) value given 5 values.
*/
#define median_of_5( a, b, c, d, e ) \
		( (b) < (a) ? (d) < (c) ? (b) < (d) ? (a) < (e) ? (a) < (d) ? (e) < (d) ? (e) \
		: (d) \
		: (c) < (a) ? (c) : (a) \
		: (e) < (d) ? (a) < (d) ? (a) : (d) \
		: (c) < (e) ? (c) : (e) \
		: (c) < (e) ? (b) < (c) ? (a) < (c) ? (a) : (c) \
		: (e) < (b) ? (e) : (b) \
		: (b) < (e) ? (a) < (e) ? (a) : (e) \
		: (c) < (b) ? (c) : (b) \
		: (b) < (c) ? (a) < (e) ? (a) < (c) ? (e) < (c) ? (e) : (c) \
		: (d) < (a) ? (d) : (a) \
		: (e) < (c) ? (a) < (c) ? (a) : (c) \
		: (d) < (e) ? (d) : (e) \
		: (d) < (e) ? (b) < (d) ? (a) < (d) ? (a) : (d) \
		: (e) < (b) ? (e) : (b) \
		: (b) < (e) ? (a) < (e) ? (a) : (e) \
		: (d) < (b) ? (d) : (b) \
		: (d) < (c) ? (a) < (d) ? (b) < (e) ? (b) < (d) ? (e) < (d) ? (e) : (d) \
		: (c) < (b) ? (c) : (b) \
		: (e) < (d) ? (b) < (d) ? (b) : (d) \
		: (c) < (e) ? (c) : (e) \
		: (c) < (e) ? (a) < (c) ? (b) < (c) ? (b) : (c) \
		: (e) < (a) ? (e) : (a) \
		: (a) < (e) ? (b) < (e) ? (b) : (e) \
		: (c) < (a) ? (c) : (a) \
		: (a) < (c) ? (b) < (e) ? (b) < (c) ? (e) < (c) ? (e) : (c) \
		: (d) < (b) ? (d) : (b) \
		: (e) < (c) ? (b) < (c) ? (b) : (c) \
		: (d) < (e) ? (d) : (e) \
		: (d) < (e) ? (a) < (d) ? (b) < (d) ? (b) : (d) \
		: (e) < (a) ? (e) : (a) \
		: (a) < (e) ? (b) < (e) ? (b) : (e) \
		: (d) < (a) ? (d) : (a) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	append_decimal_string
	@abstract	Appends a decimal string to a buffer.
	
	@param		X		Unsigned decimal number to convert to a string. It will be modified by this macro.
	@param		DST		Pointer to write string to. Will point to end of string on return.
	
	@discussion
	
	Example usage:
	
	char		str[ 32 ];
	char *		dst;
	int			x;
	
	strcpy( str, "test" );
	dst = str + 4;
	
	x = 1234;
	append_decimal_string( x, dst );
	strcpy( dst, "end" );
	
	... str is "test1234end".
*/
#define	append_decimal_string( X, DST ) \
	do \
	{ \
		char		_adsBuf[ 32 ]; \
		char *		_adsPtr; \
		\
		_adsPtr = _adsBuf; \
		do \
		{ \
			*_adsPtr++ = (char)( '0' + ( (X) % 10 ) ); \
			(X) /= 10; \
		\
		}	while( (X) > 0 ); \
 		\
		while( _adsPtr > _adsBuf ) \
		{ \
			*(DST)++ = *( --_adsPtr ); \
		} \
		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	BitArray
	@abstract	Macros for working with bit arrays.
	@discussion
	
	Bit numbers start from the left so bit 0 is 0x80 in byte 0, bit 1 is 0x40 in byte 0, bit 8 is 0x80 in byte 1, etc. 
	This minimizes the number of bytes, allows new bits to be added later, and supports an unlimited number of bits.
	The following ASCII art shows how the bits are arranged:
	
			                     1 1 1 1 1 1 1 1 1 1 2 2 2 2 
	Bit		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 
			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			|    a          |b              |  c           d| = Bytes 0x20 0x80 0x41 (bits a=2, b=8, c=17, and d=23).
			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	Byte	0               1               2
*/
#define BitArray_MinBytes( ARRAY, N_BYTES )			memrlen( (ARRAY), (N_BYTES) )
#define BitArray_MaxBytes( BITS )					( ( (BITS) + 7 ) / 8 )
#define BitArray_MaxBits( ARRAY_BYTES )				( (ARRAY_BYTES) * 8 )
#define BitArray_Clear( ARRAY_PTR, ARRAY_BYTES )	memset( (ARRAY_PTR), 0, (ARRAY_BYTES) );
#define BitArray_GetBit( PTR, LEN, BIT ) \
	( ( (BIT) < BitArray_MaxBits( (LEN) ) ) && ( (PTR)[ (BIT) / 8 ] & ( 1 << ( 7 - ( (BIT) & 7 ) ) ) ) )
#define BitArray_SetBit( ARRAY, BIT )				( (ARRAY)[ (BIT) / 8 ] |=  ( 1 << ( 7 - ( (BIT) & 7 ) ) ) )
#define BitArray_ClearBit( ARRAY, BIT )				( (ARRAY)[ (BIT) / 8 ] &= ~( 1 << ( 7 - ( (BIT) & 7 ) ) ) )

// BSD-style bit array macros. These store bits within a byte as MSB first (e.g. bit 0 is rightmost bit in byte).

#if( !defined( setbit ) )
	#define	setbit( ARRAY, INDEX )		( ( (uint8_t *)(ARRAY) )[ (INDEX) / 8 ] |= ( 1 << ( (INDEX) % 8 ) ) )
#endif
#if( !defined( clrbit ) )
	#define	clrbit( ARRAY, INDEX )		( ( (uint8_t *)(ARRAY) )[ (INDEX) / 8 ] &= ( ~( 1 << ( (INDEX) % 8 ) ) ) )
#endif
#if( !defined( isset ) )
	#define	isset( ARRAY, INDEX )		( ( (uint8_t *)(ARRAY) )[ (INDEX) / 8 ] & ( 1 << ( (INDEX) % 8 ) ) )
#endif
#if( !defined( isclr ) )
	#define	isclr( ARRAY, INDEX )		( ( ( (uint8_t *)(ARRAY) )[ (INDEX) / 8 ] & ( 1 << ( (INDEX) % 8 ) ) ) == 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	InsertBits
	@abstract	Inserts BITS (both 0 and 1 bits) into X, controlled by MASK and SHIFT, and returns the result.
	@discussion
	
	MASK is the bitmask of the bits in the final position.
	SHIFT is the number of bits to shift left for 1 to reach the first bit position of MASK.
	
	For example, if you wanted to insert 0x3 into the leftmost 4 bits of a 32-bit value:
	
	InsertBits( 0, 0x3, 0xF0000000U, 28 ) == 0x30000000
*/
#define	InsertBits( X, BITS, MASK, SHIFT )		( ( (X) & ~(MASK) ) | ( ( (BITS) << (SHIFT) ) & (MASK) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ExtractBits
	@abstract	Extracts bits from X, controlled by MASK and SHIFT, and returns the result.
	@discussion
	
	MASK is the bitmask of the bits in the final position.
	SHIFT is the number of bits to shift right to right justify MASK.
	
	For example, if you had a 32-bit value (e.g. 0x30000000) wanted the left-most 4 bits (e.g. 3 in this example):
	
	ExtractBits( 0x30000000U, 0xF0000000U, 28 ) == 0x3
*/
#define	ExtractBits( X, MASK, SHIFT )			( ( (X) >> (SHIFT) ) & ( (MASK) >> (SHIFT) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SetOrClearBits
	@abstract	Sets bits if the test is non-zero or clears bits if the test is zero.
	@discussion
	
	int		x;
	
	SetOrClearBits( &x, 0x7, true  ); // Sets   bits 0, 1, 2
	SetOrClearBits( &x, 0x7, false ); // Clears bits 0, 1, 2
*/
#define SetOrClearBits( VALUE_PTR, BITS, TEST ) \
	do { *(VALUE_PTR) = (TEST) ? ( *(VALUE_PTR) | (BITS) ) : ( *(VALUE_PTR) & ~(BITS) ); } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Stringify
	@abstract	Stringify's an expression.
	@discussion
	
	Stringify macros to process raw text passed via -D options to C string constants. The double-wrapping is necessary 
	because the C preprocessor doesn't perform its normal argument expansion pre-scan with stringified macros so the 
	-D macro needs to be expanded once via the wrapper macro then stringified so the raw text is stringified. Otherwise, 
	the replacement value would be used instead of the symbolic name (only for preprocessor symbols like #defines).
	
	For example:
	
		#define	kMyConstant		1
		
		printf( "%s", Stringify( kMyConstant ) );			// Prints "kMyConstant"
		printf( "%s", StringifyExpansion( kMyConstant ) );	// Prints "1"
		
	Non-preprocessor symbols do not have this issue. For example:
	
		enum
		{
			kMyConstant = 1
		};
		
		printf( "%s", Stringify( kMyConstant ) );			// Prints "kMyConstant"
		printf( "%s", StringifyExpansion( kMyConstant ) );	// Prints "kMyConstant"
	
	See <http://gcc.gnu.org/onlinedocs/cpp/Argument-Prescan.html> for more info on C preprocessor pre-scanning.
*/
#define	Stringify( X )				# X
#define	StringifyExpansion( X )		Stringify( X )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Forget macros
	@abstract	These take a pointer and if what it points to is valid, it gets rid of it and marks it invalid.
*/
#define	ForgetCustom( X, DELETER )				do { if( *(X) ) { DELETER( *(X) ); *(X) = NULL; } } while( 0 )
#define ForgetCustomEx( X, STOPPER, DELETER )	do { if( *(X) ) { STOPPER( *(X) ); DELETER( *(X) ); *(X) = NULL; } } while( 0 )
#define	ObjCForgetCustom( X, STOPPER )			do { [*(X) STOPPER]; arc_safe_release( *(X) ); *(X) = nil; } while( 0 )

#define AudioConverterForget( X )		ForgetCustom( X, AudioConverterDispose )
#if( COMPILER_ARC )
	#define	BlockForget( X )			do { *(X) = nil; } while( 0 )
#else
	#define	BlockForget( X )			ForgetCustom( X, Block_release )
#endif
#define	CFForget( X )					ForgetCustom( X, CFRelease )
#define	COMObjectForget( X )			do { if( *(X) ) { ( *(X) )->Release(); *(X) = NULL; } } while( 0 )
#define	dispatch_forget( X )			ForgetCustom( X, arc_safe_dispatch_release )
#define dispatch_release_null_safe( X )	do { if( (X) ) arc_safe_dispatch_release( (X) ); } while( 0 )
#define	DNSServiceForget( X )			ForgetCustom( X, DNSServiceRefDeallocate )
#define	ForgetBlock( X )				BlockForget( X )
#define	ForgetCF( X )					ForgetCustom( X, CFRelease )
#define	ForgetIOKitKernelObject( X )	do { if( *(X) ) { ( *(X) )->release(); *(X) = NULL; } } while( 0 )
#define	ForgetMem( X )					ForgetCustom( X, free_compat )
#define	ForgetObjectiveCObject( X )		do { arc_safe_release( *(X) ); *(X) = nil; } while( 0 )
#define	ForgetPtrLen( PTR, LEN )		do { ForgetMem( (PTR) ); *(LEN) = 0; } while( 0 )
#define	ForgetPtrLenSecure( PTR, LEN )	do { if( *(PTR) ) { MemZeroSecure( *(PTR), *(LEN) ); } ForgetPtrLen( (PTR), (LEN) ); } while( 0 )
#define	ForgetVxSem( X )				do { if( *(X) ) { semDelete( *(X) ); *(X) = 0; } } while( 0 )
#define	ForgetWinHandle( X )			do { if( *(X) ) { CloseHandle( *(X) ); *(X) = 0; } } while( 0 )
#define	ForgetWinRegKey( X )			ForgetCustom( X, RegCloseKey )
#define FreeNullSafe( X )				do { if( (X) ) free( (X) ); } while( 0 )
#define	IONotificationPortForget( X )	ForgetCustom( X, IONotificationPortDestroy )
#define	IOPlugInInterfaceForget( X )	ForgetCustom( X, IODestroyPlugInInterface )
#define os_forget( X )					ForgetCustom( X, arc_safe_os_release )
#define SRP_forget( X )					ForgetCustom( X, SRP_free )
#define SRP_cstr_forget( X )			ForgetCustom( X, cstr_free )
#define SRP_cstr_clear_forget( X )		ForgetCustom( X, cstr_clear_free )
#define	xpc_forget( X )					ForgetCustom( X, arc_safe_xpc_release )
#define xpc_release_null_safe( X )		do { if( (X) ) arc_safe_xpc_release( (X) ); } while( 0 )

#define dispatch_socket_forget( SOURCE, SOCK_PTR, SUSPENDED ) \
	do \
	{ \
		if( (SOURCE) ) \
		{ \
			dispatch_source_cancel( (SOURCE) ); \
			if( (SUSPENDED) ) dispatch_resume( (SOURCE) ); \
			arc_safe_dispatch_release( (SOURCE) ); \
		} \
		else \
		{ \
			ForgetSocket( (SOCK_PTR) ); \
		} \
		\
	}	while( 0 )

#define	dispatch_source_forget( X ) \
	do \
	{ \
		if( *(X) ) \
		{ \
			dispatch_source_cancel( *(X) ); \
			arc_safe_dispatch_release( *(X) ); \
			*(X) = NULL; \
		} \
		\
	}	while( 0 )

#define	dispatch_source_forget_ex( SOURCE_PTR, SUSPENDED_PTR ) \
	do \
	{ \
		if( *(SOURCE_PTR) ) \
		{ \
			dispatch_source_cancel( *(SOURCE_PTR) ); \
			if( (SUSPENDED_PTR) && *(SUSPENDED_PTR) ) \
			{ \
				dispatch_resume( *(SOURCE_PTR) ); \
				*(SUSPENDED_PTR) = false; \
			} \
			arc_safe_dispatch_release( *(SOURCE_PTR) ); \
			*(SOURCE_PTR) = NULL; \
		} \
		\
	}	while( 0 )

#define dispatch_resume_if_suspended( SOURCE, SUSPENDED_PTR ) \
	do \
	{ \
		if( *(SUSPENDED_PTR) ) \
		{ \
			*(SUSPENDED_PTR) = false; \
			dispatch_resume( (SOURCE) ); \
		} \
		\
	}	while( 0 )

#define dispatch_suspend_if_resumed( SOURCE, SUSPENDED_PTR ) \
	do \
	{ \
		if( !*(SUSPENDED_PTR) ) \
		{ \
			*(SUSPENDED_PTR) = true; \
			dispatch_suspend( (SOURCE) ); \
		} \
		\
	}	while( 0 )

#define	ForgetANSIFile( X ) \
	do \
	{ \
		if( *(X) ) \
		{ \
			OSStatus		ForgetANSIFileErr; \
			\
			ForgetANSIFileErr = fclose( *(X) ); \
			ForgetANSIFileErr = map_noerr_errno( ForgetANSIFileErr ); \
			check_noerr( ForgetANSIFileErr ); \
			*(X) = NULL; \
		} \
 		\
	}	while( 0 )

#define ForgetFD( X ) \
	do \
	{ \
		if( IsValidFD( *(X) ) ) \
		{ \
			OSStatus		ForgetFDErr; \
			\
			ForgetFDErr = CloseFD( *(X) ); \
			ForgetFDErr = map_global_noerr_errno( ForgetFDErr ); \
			check_noerr( ForgetFDErr ); \
			*(X) = kInvalidFD; \
		} \
		\
	}	while( 0 )

#define IOKitUSBDeviceForget( USBIF, OPENED ) \
	do \
	{ \
		if( *(OPENED) ) \
		{ \
			( **(USBIF) )->USBDeviceClose( *(USBIF) ); \
			*(OPENED) = false; \
		} \
		if( *(USBIF) ) \
		{ \
			( **(USBIF) )->Release( *(USBIF) ); \
			*(USBIF) = NULL; \
		} \
		\
	}	while( 0 )

#define IOKitUSBInterfaceForget( USBIF, OPENED ) \
	do \
	{ \
		if( *(OPENED) ) \
		{ \
			( **(USBIF) )->USBInterfaceClose( *(USBIF) ); \
			*(OPENED) = false; \
		} \
		if( *(USBIF) ) \
		{ \
			( **(USBIF) )->Release( *(USBIF) ); \
			*(USBIF) = NULL; \
		} \
		\
	}	while( 0 )

#define	notify_forget( X ) \
	do \
	{ \
		if( *(X) != -1 ) \
		{ \
			OSStatus		notify_forget_err_; \
			\
			notify_forget_err_ = (OSStatus) notify_cancel( *(X) ); \
			check_noerr( notify_forget_err_ ); \
			*(X) = -1; \
		} \
		\
	}	while( 0 )

#define pthread_cond_forget( X ) \
	do \
	{ \
		if( *(X) ) \
		{ \
			int		pthread_cond_forget_err_; \
			\
			DEBUG_USE_ONLY( pthread_cond_forget_err_ ); \
			\
			pthread_cond_forget_err_ = pthread_cond_destroy( *(X) ); \
			check_noerr( pthread_cond_forget_err_ ); \
			*(X) = NULL; \
		} \
		\
	}	while( 0 )

#define pthread_mutex_forget( X ) \
	do \
	{ \
		if( *(X) ) \
		{ \
			int		pthread_mutex_forget_err_; \
			\
			DEBUG_USE_ONLY( pthread_mutex_forget_err_ ); \
			\
			pthread_mutex_forget_err_ = pthread_mutex_destroy( *(X) ); \
			check_noerr( pthread_mutex_forget_err_ ); \
			*(X) = NULL; \
		} \
		\
	}	while( 0 )

#define ForgetSocket( X ) \
	do \
	{ \
		if( IsValidSocket( *(X) ) ) \
		{ \
			OSStatus		ForgetSocketErr; \
			\
			ForgetSocketErr = close_compat( *(X) ); \
			ForgetSocketErr = map_socket_noerr_errno( *(X), ForgetSocketErr ); \
			check_noerr( ForgetSocketErr ); \
			*(X) = kInvalidSocketRef; \
		} \
		\
	}	while( 0 )

#define IOObjectForget( X ) \
	do \
	{ \
		if( *(X) != IO_OBJECT_NULL ) \
		{ \
			IOReturn		IOObjectForgetErr; \
			\
			IOObjectForgetErr = IOObjectRelease( *(X) ); \
			check_noerr( IOObjectForgetErr ); \
			*(X) = IO_OBJECT_NULL; \
		} \
		\
	}	while( 0 )

#define	xpc_connection_forget( X ) \
	do \
	{ \
		if( *(X) ) \
		{ \
			xpc_connection_cancel( *(X) ); \
			arc_safe_xpc_release( *(X) ); \
			*(X) = NULL; \
		} \
		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Replace macros
	@abstract	These retain/copy/etc the new thing and release/free/etc the old thing (if the old thing is valid).
*/
#define ReplaceBlock( BLOCK_PTR, NEW_BLOCK ) \
	do \
	{ \
		__typeof( (NEW_BLOCK) )		ReplaceBlock_TempBlock = (NEW_BLOCK); \
		\
		if( ReplaceBlock_TempBlock ) \
		{ \
			ReplaceBlock_TempBlock = arc_safe_Block_copy( ReplaceBlock_TempBlock ); \
			check( ReplaceBlock_TempBlock ); \
		} \
		if( *(BLOCK_PTR) ) arc_safe_Block_release( *(BLOCK_PTR) ); \
		*(BLOCK_PTR) = ReplaceBlock_TempBlock; \
		\
	}	while( 0 )

#define ReplaceCF( OBJECT_PTR, NEW_OBJECT ) \
	do \
	{ \
		CFTypeRef *		ReplaceCF_objectPtr = (CFTypeRef *)(OBJECT_PTR); \
		CFTypeRef		ReplaceCF_oldObject = *ReplaceCF_objectPtr; \
		CFTypeRef		ReplaceCF_newObject = (NEW_OBJECT); \
		\
		if( ReplaceCF_newObject ) CFRetain( ReplaceCF_newObject ); \
		*ReplaceCF_objectPtr = ReplaceCF_newObject; \
		if( ReplaceCF_oldObject ) CFRelease( ReplaceCF_oldObject ); \
		\
	}	while( 0 )

#define ReplaceDispatchQueue( QUEUE_PTR, NEW_QUEUE ) \
	do \
	{ \
		dispatch_queue_t		ReplaceDispatchQueue_TempQueue = (NEW_QUEUE); \
		\
		if( !ReplaceDispatchQueue_TempQueue ) ReplaceDispatchQueue_TempQueue = dispatch_get_main_queue(); \
		arc_safe_dispatch_retain( ReplaceDispatchQueue_TempQueue ); \
		if( *(QUEUE_PTR) ) arc_safe_dispatch_release( *(QUEUE_PTR) ); \
		*(QUEUE_PTR) = ReplaceDispatchQueue_TempQueue; \
		\
	}	while( 0 )

#define ReplaceObjectiveCObject( OBJECT_PTR, NEW_OBJECT ) \
	do \
	{ \
		id *		ReplaceObjectiveCObject_objectPtr = (id *)(OBJECT_PTR); \
		id			ReplaceObjectiveCObject_oldObject = *ReplaceObjectiveCObject_objectPtr; \
		id			ReplaceObjectiveCObject_newObject = (NEW_OBJECT); \
		\
		arc_safe_retain( ReplaceObjectiveCObject_newObject ); \
		*ReplaceObjectiveCObject_objectPtr = ReplaceObjectiveCObject_newObject; \
		arc_safe_release( ReplaceObjectiveCObject_oldObject ); \
		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MemEqual
	@abstract	Returns non-zero if two ptr/len pairs are equal and 0 otherwise.
*/

#define MemEqual( PTR1, LEN1, PTR2, LEN2 ) \
	( ( ( LEN1 ) == ( LEN2 ) ) && ( memcmp( ( PTR1 ), ( PTR2 ), ( LEN1 ) ) == 0 ) )

#define MemIEqual( PTR1, LEN1, PTR2, LEN2 ) \
	( ( ( LEN1 ) == ( LEN2 ) ) && ( memicmp( ( PTR1 ), ( PTR2 ), ( LEN1 ) ) == 0 ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DECLARE_QSORT_FUNC / DEFINE_QSORT_FUNC
	@abstract	Declares/defines a qsort-compatible sort function for numeric types.
	@abstract
	
	Use it like this in your header file:
	
		DECLARE_QSORT_NUMERIC_COMPARATOR( cmp_double );
	
	Then in your source file:
	
		DEFINE_QSORT_NUMERIC_COMPARATOR( double, cmd_double );
	
	Then to use it in code:
	
		qsort( array, count, elementSize, cmd_double );
*/

#define DECLARE_QSORT_NUMERIC_COMPARATOR( NAME )	int	NAME( const void *a, const void *b )

#define DEFINE_QSORT_NUMERIC_COMPARATOR( TYPE, NAME ) \
	int	NAME( const void *a, const void *b ) \
	{ \
		TYPE const		aa = *( (const TYPE *) a ); \
		TYPE const		bb = *( (const TYPE *) b ); \
		\
		return( ( aa > bb ) - ( aa < bb ) ); \
	}

DECLARE_QSORT_NUMERIC_COMPARATOR( qsort_cmp_int8 );
DECLARE_QSORT_NUMERIC_COMPARATOR( qsort_cmp_uint8 );
DECLARE_QSORT_NUMERIC_COMPARATOR( qsort_cmp_int16 );
DECLARE_QSORT_NUMERIC_COMPARATOR( qsort_cmp_uint16 );
DECLARE_QSORT_NUMERIC_COMPARATOR( qsort_cmp_int32 );
DECLARE_QSORT_NUMERIC_COMPARATOR( qsort_cmp_uint32 );
DECLARE_QSORT_NUMERIC_COMPARATOR( qsort_cmp_int64 );
DECLARE_QSORT_NUMERIC_COMPARATOR( qsort_cmp_uint64 );
DECLARE_QSORT_NUMERIC_COMPARATOR( qsort_cmp_float );
DECLARE_QSORT_NUMERIC_COMPARATOR( qsort_cmp_double );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	HAS_FEATURE
	@abstract	Performs a compile-time check for a feature flag and fails to compile if the feature is not defined.
	@discussion
	
	This can be used to detect if a feature is defined to 1 (feature enabled) or defined 0 (feature not enabled) or
	it fails to compile if the feature flag is not defined at all. This can help catch errors when you're testing
	for a feature, but typed it wrong, forgot to include the right header file, or passed the wrong -D compile flags.
	Here's how you use it:
	
	#if( HAS_FEATURE( COOL_FEATURE ) )
		... code to relate to COOL_FEATURE.
	#endif
*/
#define HAS_FEATURE_CAT( a, b )		a ## b 
#define HAS_FEATURE_CAT2( a, b )	HAS_FEATURE_CAT( a, b ) 
#define HAS_FEATURE_CHECK_0			1 
#define HAS_FEATURE_CHECK_1			1 
#define HAS_FEATURE( X )			( X / HAS_FEATURE_CAT2( HAS_FEATURE_CHECK_, X ) ) 

#if 0
#pragma mark == Fixed-Point Math ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Fixed-point math
	@abstract	Macros to work with Q32.32 numbers. <http://en.wikipedia.org/wiki/Q_(number_format)>.
	@discussion
	
	- Addition and subtraction with other Q32.32 numbers can use the normal + and - operators, respectively.
	- Multiplication by other Q32.32 numbers needs to use the normal * then right shift the result by 32.
	  Warning: Due to the lack of a standard int128_t, multiplication will overflow if either value is >= 1 (and other cases).
	- Negation can use the normal unary - operator.
	- Negative numbers can be detected using a normal < 0 check.
	- Setting to 0 can be by simply assigning 0.
*/
#if( TARGET_LANGUAGE_C_LIKE )
	typedef int32_t		Q16x16;
	typedef int64_t		Q32x32;
#endif

#define kQ16_1pt0		0x00010000 // 1.0 in 16.16 fixed format.
#define kQ16_0pt5		0x00008000 // 0.5 in 16.16 fixed format.

#define FloatToQ32x32( X )					( (int64_t)( (X) * ( (double) UINT32_C( 0xFFFFFFFF ) ) ) )
#define Q32x32ToFloat( X )					( ( (double)(X) ) / ( (double)( UINT32_C( 0xFFFFFFFF ) ) ) )

#define Q32x32_Integer( a )					( ( (Q32x32)(a) ) << 32 )

#define Q32x32_GetInteger( x )				( (int32_t)( ( (x) < 0 ) ? ( -( -(x) >> 32 ) ) : ( (x) >> 32 ) ) )
#define Q32x32_SetInteger( x, a )			( (x) = ( (Q32x32)(a) ) << 32 )
#define Q32x32_GetFraction( x )				( (uint32_t)( (x) & UINT32_C( 0xFFFFFFFF ) ) )

#define Q32x32_AddInteger( x, a )			( (x) += ( ( (Q32x32)(a) ) << 32 ) )
#define Q32x32_MultiplyByInteger( x, a )	( (x) *= (a) )
#define Q32x32_RightShift( x, a ) \
	do \
	{ \
		if( (x) < 0 )	(x) = -( -(x) >> (a) ); \
		else			(x) =     (x) >> (a); \
	\
	}	while ( 0 )

#if 0
#pragma mark -
#pragma mark == Modular Math ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Modular math
	@abstract	AKA Serial Number Arithmetic per RFC 1982). See <http://en.wikipedia.org/wiki/Serial_number_arithmetic>.
	@discussion
	
	EQ:   Returns non-zero if A == B.
	LT:   Returns non-zero if A <  B.
	LE:   Returns non-zero if A <= B.
	GT:   Returns non-zero if A >  B.
	GE:   Returns non-zero if A >= B.
	Cmp:  Returns 0 if A == B, < 0 if A < B, and > 0 if A > B. Do not compare against -1 or 1...use < 0 or > 0, respectively.
	Diff: Returns the absolute value of the difference between A and B (e.g. Mod8_Diff( 5, 10 ) == 5 and Mod8_Diff( 10, 5 ) == 5).
*/

// 8-bit modular math. Note: these only work if the difference is less than 2^7.

#if( !defined( Mod8_EQ ) )
	#define	Mod8_EQ( A, B )			( ( (uint8_t)(A) ) == ( (uint8_t)(B) ) )
	#define	Mod8_LT( A, B )			( (int8_t)( ( (uint8_t)(A) ) - ( (uint8_t)(B) ) ) <   0 )
	#define	Mod8_LE( A, B )			( (int8_t)( ( (uint8_t)(A) ) - ( (uint8_t)(B) ) ) <=  0 )
	#define	Mod8_GT( A, B )			( (int8_t)( ( (uint8_t)(A) ) - ( (uint8_t)(B) ) ) >   0 )
	#define	Mod8_GE( A, B )			( (int8_t)( ( (uint8_t)(A) ) - ( (uint8_t)(B) ) ) >=  0 )
	#define	Mod8_Cmp( A, B )		( (int8_t)( ( (uint8_t)(A) ) - ( (uint8_t)(B) ) ) )
	#define	Mod8_Diff( A, B )		( Mod8_LT( (A), (B) ) ? ( (B) - (A) ) : ( (A) - (B) ) )
#endif

// 16-bit modular math. Note: these only work if the difference is less than 2^15.

#if( !defined( Mod16_LT ) )
	#define	Mod16_EQ( A, B )		( ( (uint16_t)(A) ) == ( (uint16_t)(B) ) )
	#define	Mod16_LT( A, B )		( (int16_t)( ( (uint16_t)(A) ) - ( (uint16_t)(B) ) ) <   0 )
	#define	Mod16_LE( A, B )		( (int16_t)( ( (uint16_t)(A) ) - ( (uint16_t)(B) ) ) <=  0 )
	#define	Mod16_GT( A, B )		( (int16_t)( ( (uint16_t)(A) ) - ( (uint16_t)(B) ) ) >   0 )
	#define	Mod16_GE( A, B )		( (int16_t)( ( (uint16_t)(A) ) - ( (uint16_t)(B) ) ) >=  0 )
	#define	Mod16_Cmp( A, B )		( (int16_t)( ( (uint16_t)(A) ) - ( (uint16_t)(B) ) ) )
	#define	Mod16_Diff( A, B )		( Mod16_LT( (A), (B) ) ? ( (B) - (A) ) : ( (A) - (B) ) )
#endif

// 32-bit modular math. Note: these only work if the difference is less than 2^31.

#if( !defined( Mod32_LT ) )
	#define	Mod32_EQ( A, B )		( ( (uint32_t)(A) ) == ( (uint32_t)(B) ) )
	#define	Mod32_LT( A, B )		( (int32_t)( ( (uint32_t)(A) ) - ( (uint32_t)(B) ) ) <   0 )
	#define	Mod32_LE( A, B )		( (int32_t)( ( (uint32_t)(A) ) - ( (uint32_t)(B) ) ) <=  0 )
	#define	Mod32_GT( A, B )		( (int32_t)( ( (uint32_t)(A) ) - ( (uint32_t)(B) ) ) >   0 )
	#define	Mod32_GE( A, B )		( (int32_t)( ( (uint32_t)(A) ) - ( (uint32_t)(B) ) ) >=  0 )
	#define	Mod32_Cmp( A, B )		( (int32_t)( ( (uint32_t)(A) ) - ( (uint32_t)(B) ) ) )
	#define	Mod32_Diff( A, B )		( Mod32_LT( (A), (B) ) ? ( (B) - (A) ) : ( (A) - (B) ) )
#endif

#if 0
#pragma mark -
#pragma mark == Booleans ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	bool
	@abstract	Standardized boolean type. Built-in with C99 and C++, but emulated elsewhere.
	@discussion
	
	C++ defines bool, true, and false. Metrowerks allows this to be controlled by the "bool" option though.
	C99 defines __bool_true_false_are_defined when bool, true, and false are defined.
	MacTypes.h defines Boolean, true, and false.
	VxWorks rwos.h defines _RWOS_H_ and defines bool, true, and false if bool is not already defined.
	
	Note: The Metrowerks has to be in its own block because Microsoft Visual Studio .NET does not completely 
	short-circuit and gets confused by the option( bool ) portion of the conditional.
	
	The moral the story is just don't use "bool" unless you know you're using C++ and never want compatibility with C 
	code. Otherwise, it's just too much of a pain. There are also binary compatibility issues because bool may be a
	different size in different environments. Use Boolean instead (provided in this file if needed and always 1 byte).
*/
#if( defined( __MWERKS__ ) )
	
	// Note: The following test is done on separate lines because CodeWarrior doesn't like it all on one line.
	
	#if( !__bool_true_false_are_defined && ( !defined( __cplusplus ) || !__option( bool ) ) )
		#define	COMMON_SERVICES_NEEDS_BOOL		1
	#else
		#define	COMMON_SERVICES_NEEDS_BOOL		0
	#endif
	
	// Workaround when building with CodeWarrior, but using the Apple stdbool.h header, which uses _Bool.
	
	#if( __bool_true_false_are_defined && !defined( __cplusplus ) && !__option( c9x ) )
		#define _Bool	int
	#endif
	
	// Workaround when building with CodeWarrior for C++ with bool disabled and using the Apple stdbool.h header, 
	// which defines true and false to map to C++ true and false (which are not enabled). Serenity Now!
	
	#if( __bool_true_false_are_defined && defined( __cplusplus ) && !__option( bool ) )
		#define	true	1
		#define	false	0
	#endif
#else
	#if( !defined( __cplusplus ) && ( !defined( __bool_true_false_are_defined ) || !__bool_true_false_are_defined ) && \
		 !defined( bool ) && !defined( _RWOS_H_ ) && !defined( __IOKIT_IOTYPES_H ) )
		#define COMMON_SERVICES_NEEDS_BOOL		1
	#else
		#define COMMON_SERVICES_NEEDS_BOOL		0
	#endif
#endif

#if( COMMON_SERVICES_NEEDS_BOOL )
	
	#if( TARGET_LANGUAGE_C_LIKE )
		typedef int		bool;
	#endif
	
	#define	bool	bool
	
	#if( !defined( true ) )
		#define	true	1
	#endif
	
	#if( !defined( false ) )
		#define	false	0
	#endif
	
	#define __bool_true_false_are_defined		1
#endif

// IOKit IOTypes.h typedef's bool if TYPE_BOOL is not defined so define it here to prevent redefinition by IOTypes.h.

#if( TARGET_OS_DARWIN_KERNEL )
	#define TYPE_BOOL		1
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	Boolean
	@abstract	Mac-style Boolean type. Emulated on non-Mac platforms.
*/

// MacTypes.h (Carbon) and OSTypes.h (IOKit) typedef Boolean so only typedef if those haven't been included.
// Others use __BOOLEAN_DEFINED__ when they typedef Boolean so check for that and define it if we typedef it.

#if( !defined( __MACTYPES__ ) && !defined( _OS_OSTYPES_H ) && !defined( __BOOLEAN_DEFINED__ ) )
	#if( TARGET_LANGUAGE_C_LIKE )
		typedef uint8_t		Boolean;
	#endif
	
	#define	__BOOLEAN_DEFINED__		1
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	TYPE_LONGLONG_NATIVE
	@abstract	Defines whether long long (or its equivalent) is natively supported or requires special libraries.
*/
#if( !defined( TYPE_LONGLONG_NATIVE ) )
	#if( !defined( __GNUC__ ) || ( ( __GNUC__ > 2 ) || ( ( __GNUC__ == 2 ) && ( __GNUC_MINOR__ >= 9 ) ) ) || defined( __ghs__ ) )
		#define	TYPE_LONGLONG_NATIVE			1
	#else
		#define	TYPE_LONGLONG_NATIVE			0
	#endif
#endif

#if 0
#pragma mark -
#pragma mark == Errors ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		OSStatus
	@abstract	Status Code
*/
#if( TARGET_LANGUAGE_C_LIKE )
	#if  ( !TARGET_OS_DARWIN )
		typedef int32_t			OSStatus;
		
		#define OSSTATUS_DEFINED		1
	#endif
#endif

#define kNoErr						0		//! No error occurred.
#define kInProgressErr				1		//! Operation in progress.

// Generic error codes are in the range -6700 to -6779.

#define kGenericErrorBase			-6700	//! Starting error code for all generic errors.

#define kUnknownErr					-6700	//! Unknown error occurred.
#define kOptionErr					-6701	//! Option was not acceptable.
#define kSelectorErr				-6702	//! Selector passed in is invalid or unknown.
#define kExecutionStateErr			-6703	//! Call made in the wrong execution state (e.g. called at interrupt time).
#define kPathErr					-6704	//! Path is invalid, too long, or otherwise not usable.
#define kParamErr					-6705	//! Parameter is incorrect, missing, or not appropriate.
#define kUserRequiredErr			-6706	//! User interaction is required.
#define kCommandErr					-6707	//! Command invalid or not supported.
#define kIDErr						-6708	//! Unknown, invalid, or inappropriate identifier.
#define kStateErr					-6709	//! Not in appropriate state to perform operation.
#define kRangeErr					-6710	//! Index is out of range or not valid.
#define kRequestErr					-6711	//! Request was improperly formed or not appropriate.
#define kResponseErr				-6712	//! Response was incorrect or out of sequence.
#define kChecksumErr				-6713	//! Checksum does not match the actual data.
#define kNotHandledErr				-6714	//! Operation was not handled (or not handled completely).
#define kVersionErr					-6715	//! Version is not correct or not compatible.
#define kSignatureErr				-6716	//! Signature did not match what was expected.
#define kFormatErr					-6717	//! Unknown, invalid, or inappropriate file/data format.
#define kNotInitializedErr			-6718	//! Action request before needed services were initialized.
#define kAlreadyInitializedErr		-6719	//! Attempt made to initialize when already initialized.
#define kNotInUseErr				-6720	//! Object not in use (e.g. cannot abort if not already in use).
#define kAlreadyInUseErr			-6721	//! Object is in use (e.g. cannot reuse active param blocks).
#define kTimeoutErr					-6722	//! Timeout occurred.
#define kCanceledErr				-6723	//! Operation canceled (successful cancel).
#define kAlreadyCanceledErr			-6724	//! Operation has already been canceled.
#define kCannotCancelErr			-6725	//! Operation could not be canceled (maybe already done or invalid).
#define kDeletedErr					-6726	//! Object has already been deleted.
#define kNotFoundErr				-6727	//! Something was not found.
#define kNoMemoryErr				-6728	//! Not enough memory was available to perform the operation.
#define kNoResourcesErr				-6729	//! Resources unavailable to perform the operation.
#define kDuplicateErr				-6730	//! Duplicate found or something is a duplicate.
#define kImmutableErr				-6731	//! Entity is not changeable.
#define kUnsupportedDataErr			-6732	//! Data is unknown or not supported.
#define kIntegrityErr				-6733	//! Data is corrupt.
#define kIncompatibleErr			-6734	//! Data is not compatible or it is in an incompatible format.
#define kUnsupportedErr				-6735	//! Feature or option is not supported.
#define kUnexpectedErr				-6736	//! Error occurred that was not expected.
#define kValueErr					-6737	//! Value is not appropriate.
#define kNotReadableErr				-6738	//! Could not read or reading is not allowed.
#define kNotWritableErr				-6739	//! Could not write or writing is not allowed.
#define	kBadReferenceErr			-6740	//! An invalid or inappropriate reference was specified.
#define	kFlagErr					-6741	//! An invalid, inappropriate, or unsupported flag was specified.
#define	kMalformedErr				-6742	//! Something was not formed correctly.
#define	kSizeErr					-6743	//! Size was too big, too small, or not appropriate.
#define	kNameErr					-6744	//! Name was not correct, allowed, or appropriate.
#define	kNotPreparedErr				-6745	//! Device or service is not ready.
#define	kReadErr					-6746	//! Could not read.
#define	kWriteErr					-6747	//! Could not write.
#define	kMismatchErr				-6748	//! Something does not match.
#define	kDateErr					-6749	//! Date is invalid or out-of-range.
#define	kUnderrunErr				-6750	//! Less data than expected.
#define	kOverrunErr					-6751	//! More data than expected.
#define	kEndingErr					-6752	//! Connection, session, or something is ending.
#define	kConnectionErr				-6753	//! Connection failed or could not be established.
#define	kAuthenticationErr			-6754	//! Authentication failed or is not supported.
#define	kOpenErr					-6755	//! Could not open file, pipe, device, etc.
#define	kTypeErr					-6756	//! Incorrect or incompatible type (e.g. file, data, etc.).
#define	kSkipErr					-6757	//! Items should be or was skipped.
#define	kNoAckErr					-6758	//! No acknowledge.
#define	kCollisionErr				-6759	//! Collision occurred (e.g. two on bus at same time).
#define	kBackoffErr					-6760	//! Backoff in progress and operation intentionally failed.
#define	kAddressErr					-6761	//! Bad address or no acknowledge of address.
#define	kInternalErr				-6762	//! An error internal to the implementation occurred.
#define	kNoSpaceErr					-6763	//! Not enough space to perform operation.
#define	kCountErr					-6764	//! Count is incorrect.
#define	kEndOfDataErr				-6765	//! Reached the end of the data (e.g. recv returned 0).
#define	kWouldBlockErr				-6766	//! Would need to block to continue (e.g. non-blocking read/write).
#define	kLookErr					-6767	//! Special case that needs to be looked at (e.g. interleaved data).
#define	kSecurityRequiredErr		-6768	//! Security is required for the operation (e.g. must use encryption).
#define	kOrderErr					-6769	//! Order is incorrect.
#define	kUpgradeErr					-6770	//! Must upgrade.
#define kAsyncNoErr					-6771	//! Async operation successfully started and is now in progress.
#define kDeprecatedErr				-6772	//! Operation or data is deprecated.
#define kPermissionErr				-6773	//! Permission denied.
#define kReadWouldBlockErr			-6774	//! Needs more incoming data.
#define kWriteWouldBlockErr			-6775	//! Needs more buffer space to write data.

#define kGenericErrorEnd			-6779	//! Last generic error code (inclusive)

// OSStatus namespace for mapping low-numbered errors to OSStatus. Allocated range 300000-300999.

// HTTP 2.0 errors

#define kHTTP2ErrorBase					300000 // 300000-300099
#define HTTP2ErrorToOSStatus( X )		( (OSStatus)( kHTTP2ErrorBase + (X) ) )
#define kHTTP2Error_NoError				0x0	// No error.
#define kHTTP2Error_ProtocolError		0x1	// Endpoint detected an unspecific protocol error.
#define kHTTP2Error_InternalError		0x2	// Endpoint encountered an unexpected internal error.
#define kHTTP2Error_FlowControlError	0x3	// Endpoint detected that its peer violated the flow control protocol.
#define kHTTP2Error_SettingsTimeout		0x4	// Endpoint sent a SETTINGS frame, but did not receive a response in a timely manner.
#define kHTTP2Error_StreamClosed		0x5	// Endpoint received a frame after a stream was half closed.
#define kHTTP2Error_FrameSizeError		0x6	// Endpoint received a frame that was larger than the maximum size that it supports.
#define kHTTP2Error_RefusedStream		0x7	// Endpoint refuses the stream prior to performing any application processing.
#define kHTTP2Error_Cancel				0x8	// Endpoint indicated the stream is no longer needed.
#define kHTTP2Error_CompressionError	0x9	// Endpoint is unable to maintain the compression context for the connection.
#define kHTTP2Error_ConnectError		0xA	// Connection established in response to a CONNECT request was reset or abnormally closed.
#define kHTTP2Error_EnhanceYourCalm		0xB	// Endpoint detected peer is generating excessive load.
#define kHTTP2Error_InadequateSecurity	0xC	// Underlying transport has properties that do not meet the minimum requirements.
#define kHTTP2Error_HTTP1Required		0xD	// Endpoint requires that HTTP/1.1 be used instead of HTTP/2.

// CoreBluetooth errors.

#define kCBErrorBase					300100 // 300100-30119
#define CBErrorToOSStatus( X )			( (OSStatus)( kCBErrorBase + (X) ) )

#define kCBATTErrorBase					300120 // 300120-30139
#define CBATTErrorToOSStatus( X )		( (OSStatus)( kCBATTErrorBase + (X) ) )

// CFNetwork

#define kCFNetworkErrorBase				300140 // 300140-30169
#define CFNetworkToOSStatus( X )		( (OSStatus)( kCFNetworkErrorBase + (X) ) )

// HomeKit

#define kHomeKitErrorBase				300200 // 300200-300299
#define HomeKitErrorToOSStatus( X )		( (OSStatus)( kHomeKitErrorBase + (X) ) )

// POSIX Signals

#define kPOSIXSignalErrorBase			300300 // 300300-300399
#define POSIXSignalToOSStatus( X )		( (OSStatus)( kPOSIXSignalErrorBase + (X) ) )

// NSErrorCreateWithOSStatus -- Creates an NSError object from an OSStatus, following the convention of nil == noErr.
#define NSErrorCreateWithOSStatus( ERR ) \
	( (ERR) ? [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:(ERR) userInfo:nil] : nil )

// NSErrorCreateWithOSStatusEx -- Creates an NSError object from an OSStatus with description, following the convention of nil == noErr.
#define NSErrorCreateWithOSStatusEx( ERR ) \
	( (ERR) ? [[NSError alloc] initWithDomain:NSOSStatusErrorDomain code:(ERR) \
		userInfo:@{ NSLocalizedDescriptionKey : @(DebugGetErrorString( (ERR), NULL, 0 )) }] : nil )

// NSErrorToOSStatusSimple -- Map an NSError to OSStatus, but doesn't try to check the domain.
#define NSErrorToOSStatusSimple( ERR )	( (OSStatus)( (ERR) ? ( (ERR).code ? (ERR).code : kUnknownErr ) : kNoErr ) )

// NSErrorWithOSStatus -- Makes an NSError object from an OSStatus, following the convention of nil == noErr.
#define NSErrorWithOSStatus( ERR ) \
	( (ERR) ? [NSError errorWithDomain:NSOSStatusErrorDomain code:(ERR) userInfo:nil] : nil )

// NSErrorWithOSStatusEx -- Makes an NSError object from an OSStatus with description, following the convention of nil == noErr.
#define NSErrorWithOSStatusEx( ERR ) \
	( (ERR) ? [NSError errorWithDomain:NSOSStatusErrorDomain code:(ERR) \
		userInfo:@{ NSLocalizedDescriptionKey : @(DebugGetErrorString( (ERR), NULL, 0 )) }] : nil )

// Throws an exception to report that a subclass must implement the method.
#define	NSExceptionSubClassMustImplement() \
	[NSException raise:NSGenericException format:@"### %c[%@ %@] must be implemented by subclass.", \
		( [self class] == self ) ? '+' : '-', NSStringFromClass( [self class] ), NSStringFromSelector( _cmd )]

#if 0
#pragma mark == LogLevel ==
#endif

// LogLevel

#if( TARGET_LANGUAGE_C_LIKE )
	typedef int32_t		LogLevel;
#endif

#define	kLogLevelUninitialized		-1			//! PRIVATE: only used internally for tracking LogCategory state.
#define kLogLevelMask				0x000000FF
#define kLogLevelAll				0			//! Intended for setting as a level to enable all logging. Not for passing to ulog.
#define kLogLevelMin				1			//! Intended for passing to ulog as the lowest level. Not for setting as a level.
#define kLogLevelChatty				10
#define kLogLevelVerbose			20			//! Similar to LOG_DEBUG
#define kLogLevelTrace 				30
#define kLogLevelInfo 				40			//! Similar to LOG_INFO
#define kLogLevelNotice				50			//! Similar to LOG_NOTICE
#define kLogLevelWarning			60			//! Similar to LOG_WARNING
#define kLogLevelAssert 			70
#define kLogLevelRequire			80
#define kLogLevelError				90			//! Similar to LOG_ERR
#define kLogLevelCritical			100			//! Similar to LOG_CRIT
#define kLogLevelAlert				110			//! Similar to LOG_ALERT
#define kLogLevelEmergency			120			//! Similar to LOG_EMERG
#define kLogLevelTragic				130
#define kLogLevelMax				0x000000FE	//! Intended for passing to ulog. Not for setting as a level.
#define kLogLevelOff				0x000000FF	//! Only for use when setting a log level. Not for passing to ulog.

#define kLogLevelFlagMask			0xFFFF0000
#define kLogLevelFlagStackTrace		0x00010000	//! Print a stack trace.
#define kLogLevelFlagDebugBreak		0x00020000	//! Break into a debugger (if a debugger is present, ignored otherwise).
#define kLogLevelFlagForceConsole	0x00040000	//! Open /dev/console, print, and close each time to force output.
#define kLogLevelFlagContinuation	0x00080000	//! Use with ulog. Indicates it's part of a previous ulog call.
#define kLogLevelFlagFunction		0x00100000	//! Use with ulog. Prints the name of the function calling ulog.
#define kLogLevelFlagCrashReport	0x00200000	//! Use with ulog. Forces a crash report with stackshot, etc.
#define kLogLevelFlagDontRateLimit	0x00400000	//! Use with ulog. Bypasses normal rate limiting.
#define kLogLevelFlagSensitive		0x00800000	//! Use with ulog. Only logs if safe (i.e. os_log_sensitive).

#if 0
#pragma mark -
#pragma mark == Misc ==
#endif

//===========================================================================================================================
//	Misc
//===========================================================================================================================

// Seconds <-> Minutes <-> Hours <-> Days <-> Weeks <-> Months <-> Years conversions

#define kAttosecondsPerSecond			1000000000000000000		// 1e-18 seconds.
#define kFemtosecondsPerSecond			1000000000000000		// 1e-15 seconds.
#define kPicosecondsPerSecond			1000000000000			// 1e-12 seconds.
#define kNanosecondsPerMicrosecond		1000
#define kNanosecondsPerMillisecond		1000000
#define kNanosecondsPerSecond			1000000000				// 1e-9 seconds.
#define kMicrosecondsPerSecond			1000000					// 1e-6 seconds.
#define kMicrosecondsPerMillisecond		1000
#define kMillisecondsPerSecond			1000
#define kSecondsPerMinute				60
#define kSecondsPerHour					( 60 * 60 )				// 3600
#define kSecondsPerDay					( 60 * 60 * 24 )		// 86400
#define kSecondsPerWeek					( 60 * 60 * 24 * 7 )	// 604800
#define kSecondsPerMonth				( 60 * 60 * 24 * 30 )	// 2592000
#define kSecondsPerYear					( 60 * 60 * 24 * 365 )	// 31536000
#define kMinutesPerHour					60
#define kMinutesPerDay					( 60 * 24 )				// 1440
#define kHoursPerDay					24
#define kDaysPerWeek					7
#define kWeeksPerYear					52
#define kMonthsPerYear					12

#define	IsLeapYear( YEAR )		( !( ( YEAR ) % 4 ) && ( ( ( YEAR ) % 100 ) || !( ( YEAR ) % 400 ) ) )
#define YearToDays( YEAR )		( ( (YEAR) * 365 ) + ( (YEAR) / 4 ) - ( (YEAR) / 100 ) + ( (YEAR) / 400 ) )
#define MonthToDays( MONTH ) 	( ( ( (MONTH) * 3057 ) - 3007 ) / 100 )

#define dispatch_time_milliseconds( MS )	dispatch_time( DISPATCH_TIME_NOW, ( ( (int64_t)(MS) )   * INT64_C_safe( kNanosecondsPerMillisecond ) ) )
#define dispatch_time_seconds( SECS )		dispatch_time( DISPATCH_TIME_NOW, ( ( (int64_t)(SECS) ) * INT64_C_safe( kNanosecondsPerSecond ) ) )

// Bytes

#define kBytesPerTeraByte		1099511627776
#define kBytesPerGigaByte		1073741824
#define kBytesPerMegaByte		1048576
#define kBytesPerKiloByte		1024

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		NumVersion
	@abstract	Mac-style version numbers represented by 32-bit numbers (e.g. 1.2.3b4 -> 0x01236004).
*/
#define	kVersionStageDevelopment		0x20	//! Development version.
#define	kVersionStageAlpha				0x40	//! Alpha version (feature complete, possibly crashing bugs).
#define	kVersionStageBeta				0x60	//! Beta version (feature complete, no crashing bugs).
#define	kVersionStageFinal				0x80	//! Final version (f0 means GM).

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	NumVersionBuild
	@abstract	Builds a 32-bit Mac-style NumVersion value (e.g. NumVersionBuild( 1, 2, 3, kVersionStageBeta, 4 ) -> 1.2.3b4).
*/
#define	NumVersionBuild( MAJOR, MINOR, BUGFIX, STAGE, REV ) \
	( ( ( ( MAJOR )  & 0xFF ) << 24 ) | \
	  ( ( ( MINOR )  & 0x0F ) << 20 ) | \
	  ( ( ( BUGFIX ) & 0x0F ) << 16 ) | \
	  ( ( ( STAGE )  & 0xFF ) <<  8 ) | \
	  ( ( ( REV )    & 0xFF )       ) )

#define	NumVersionExtractMajor( VERSION )				( (uint8_t)( ( ( VERSION ) >> 24 ) & 0xFF ) )
#define	NumVersionExtractMinorAndBugFix( VERSION )		( (uint8_t)( ( ( VERSION ) >> 16 ) & 0xFF ) )
#define	NumVersionExtractMinor( VERSION )				( (uint8_t)( ( ( VERSION ) >> 20 ) & 0x0F ) )
#define	NumVersionExtractBugFix( VERSION )				( (uint8_t)( ( ( VERSION ) >> 16 ) & 0x0F ) )
#define	NumVersionExtractStage( VERSION )				( (uint8_t)( ( ( VERSION ) >>  8 ) & 0xFF ) )
#define	NumVersionExtractRevision( VERSION )			( (uint8_t)(   ( VERSION )         & 0xFF ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	NumVersionCompare
	@abstract	Compares two NumVersion values and returns < 0 if A < B, 0 if A == B, or > 0 if A > B.
*/
#define	NumVersionCompare( A, B ) \
	( ( ( (A) & 0xFFFFFF00U )  < ( (B) & 0xFFFFFF00U ) )	? -1 : \
	  ( ( (A) & 0xFFFFFF00U )  > ( (B) & 0xFFFFFF00U ) )	?  1 : \
	  ( ( ( (A) - 1 ) & 0xFF ) < ( ( (B) - 1 ) & 0xFF ) )	? -1 : \
	  ( ( ( (A) - 1 ) & 0xFF ) > ( ( (B) - 1 ) & 0xFF ) )	?  1 : 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defined	SourceVersionToInteger
	@abstract	Converts source version components to an integer.
	@discussion	For example, source version 110.35 would be SourceVersionToInteger( 110, 35, 0 ) which is 1103500.
*/
#define SourceVersionToInteger( X, Y, Z )	( ( 10000 * (X) ) + ( 100 * (Y) ) + (Z) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		CharSets
	@abstract	Useful character sets.
*/
#define kBinaryDigits				"01"
#define kDecimalDigits				"0123456789"
#define kHexDigitsUppercase			"0123456789ABCDEF"
#define kHexDigitsLowercase			"0123456789abcdef"
#define kOctalDigits				"01234567"

#define kAlphaCharSet				"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define kAlphaNumericCharSet		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
#define kUnmistakableCharSet		"ACDEFGHJKMNPQRTUVWXY34679" // Avoids easily mistaken characters: 0/o, 1/l/i, 2/Z, 5/S, 8/B

// AFP Volume names -- (0x20-0x7E, except ':').

#define kAFPVolumeNameCharSet \
	" !\"#$%&'()*+,-./" \
	"0123456789" \
	";<=>?@" \
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
	"[\\]^_`" \
	"abcdefghijklmnopqrstuvwxyz" \
	"{|}~"	

// ASCII

#define kASCII7BitCharSet \
	"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F" \
	"\x00\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F" \
	" !\"#$%&'()*+,-./" \
	"0123456789" \
	":;<=>?@" \
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
	"[\\]^_`" \
	"abcdefghijklmnopqrstuvwxyz" \
	"{|}~\x7F"

#define kASCIIPrintableCharSet \
	"\t\n\x0B\x0C\r" \
	" !\"#$%&'()*+,-./" \
	"0123456789" \
	":;<=>?@" \
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
	"[\\]^_`" \
	"abcdefghijklmnopqrstuvwxyz" \
	"{|}~"

// Bonjour SubTypes -- restrict to lowercase letters, digits, '_', and '-'.

#define kBonjourSubTypeCharSet		"abcdefghijklmnopqrstuvwxyz0123456789_-"

// DNS names -- RFC 1034 says DNS names must consist of only letters, digits, dots, and hyphens.

#define kDNSCharSet		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-"

// TXT record keys -- Printable ASCII (0x20-0x7E, except 0x3D '=').

#define kTXTKeyCharSet \
	" !\"#$%&'()*+,-./" \
	"0123456789" \
	":;<>?@" \
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
	"[\\]^_`" \
	"abcdefghijklmnopqrstuvwxyz" \
	"{|}~"	

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Hex Char Testing and Conversions
	@abstract	Macros for testing for hex chars and converting them to values and/or bytes.
*/

// Hex Char Testing and Conversions.

#define HexCharToValue( X ) \
	( ( ( (X) >= '0' ) && ( (X) <= '9' ) ) ? (        (X) - '0'   ) : \
	  ( ( (X) >= 'A' ) && ( (X) <= 'F' ) ) ? ( 10 + ( (X) - 'A' ) ) : \
	  ( ( (X) >= 'a' ) && ( (X) <= 'f' ) ) ? ( 10 + ( (X) - 'a' ) ) : 0 )

#define IsHexPair( PTR ) \
	( isxdigit_safe( ( (const unsigned char *)(PTR) )[ 0 ] ) && \
	  isxdigit_safe( ( (const unsigned char *)(PTR) )[ 1 ] ) )

#define HexPairToByte( PTR )	( (uint8_t)( \
	( HexCharToValue( ( (const unsigned char *)(PTR) )[ 0 ] ) << 4 ) | \
	  HexCharToValue( ( (const unsigned char *)(PTR) )[ 1 ] ) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Octal Char Testing and Conversions
	@abstract	Macros for testing for octal chars and converting them to values and/or bytes.
*/
#define isoctal( X )				( ( (X) >= '0' ) && ( (X) <= '7' ) )
#define OctalCharToValue( X )		( ( ( (X) >= '0' ) && ( (X) <= '7' ) ) ? ( (X) - '0' ) : 0 )

#define IsOctalTriple( PTR ) \
	( ( ( ( (const unsigned char *)(PTR) )[ 0 ] >= '0' ) && \
	    ( ( (const unsigned char *)(PTR) )[ 0 ] <= '3' ) ) && \
	  isoctal_safe( ( (const unsigned char *)(PTR) )[ 1 ] ) && \
	  isoctal_safe( ( (const unsigned char *)(PTR) )[ 2 ] ) )

#define OctalTripleToByte( PTR )	( (uint8_t)( \
	( OctalCharToValue( ( (const unsigned char *)(PTR) )[ 0 ] ) * 64 ) | \
	( OctalCharToValue( ( (const unsigned char *)(PTR) )[ 1 ] ) *  8 ) | \
	  OctalCharToValue( ( (const unsigned char *)(PTR) )[ 2 ] ) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	BCDByteToDecimal/DecimalByteToBCD
	@abstract	Converts a byte to/from BSD (e.g. 0x50 -> 50).
*/		
#define	BCDByteToDecimal( X )	( ( ( ( (X) >> 4 ) & 0x0F ) * 10 ) + ( (X) & 0x0F ) )
#define	DecimalByteToBCD( X )	( ( ( (X) / 10 ) << 4 ) | ( (X) % 10 ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	BigEndianIntegerIncrement
	@abstract	Increments a big endian integer.
*/
STATIC_INLINE void	BigEndianIntegerIncrement( uint8_t *inInteger, size_t inLen )
{
	while( inLen-- > 0 )
	{
		if( ++( inInteger[ inLen ] ) != 0 )
		{
			break;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	LittleEndianIntegerIncrement
	@abstract	Increments a little endian integer.
*/
STATIC_INLINE void	LittleEndianIntegerIncrement( uint8_t *inInteger, size_t inLen )
{
	size_t		i;
	
	for( i = 0; i < inLen; ++i )
	{
		if( ++( inInteger[ i ] ) != 0 )
		{
			break;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Celsius <-> Fahrenheit conversions.
	@discussion	Macros to convert between Celsius and Fahrenheit.
*/
#define CelsiusToFahrenheit( X )	( ( ( (X) * 9 ) / 5 ) + 32 )
#define FahrenheitToCelsius( X )	( ( ( (X) - 32 ) * 5 ) / 9 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		dB <-> linear Conversions
	@discussion	Macros to convert between dB attentuation values and linear volume levels.
	
	dB		= 20log10( linear )
	linear	= pow( 10, dB / 20 )
	
	See <http://en.wikipedia.org/wiki/Decibel> for details on the math behind this.
*/
#define DBtoLinear( DB )		( ( (DB)     <= -144.0f ) ?    0.0f : ( (DB)     >= 0.0f ) ? 1.0f : powf( 10, (DB) / 20 ) )
#define LinearToDB( LINEAR )	( ( (LINEAR) <=    0.0f ) ? -144.0f : ( (LINEAR) >= 1.0f ) ? 0.0f : ( 20 * log10f( (LINEAR) ) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Q-format fixed-point number conversions
	@abstract	Macros to convert between floating point and fixed-point Q-format numbers.
	@discussion	See <http://en.wikipedia.org/wiki/Q_(number_format)> for details.
	
	N is number of fractional bits to use.
*/
#define FloatToQ( X, N )		( (int)( (X) * ( 1 << (N) ) ) )
#define QToFloat( X, N )		( ( (float)(X) ) / ( (float)( 1 << (N) ) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	dispatch_status_block_t
	@abstract	Block type for a commonly used block with a status parameter.
*/
#if( COMPILER_HAS_BLOCKS )
	typedef void ( ^dispatch_status_block_t )( OSStatus inStatus );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	memcmp_constant_time
	@abstract	Compares memory so that the time it takes does not depend on the data being compared.
	@discussion	This is needed to avoid certain timing attacks in cryptographic software.
*/
STATIC_INLINE int	memcmp_constant_time( const void *inA, const void *inB, size_t inLen )
{
	const uint8_t * const		a = (const uint8_t *) inA;
	const uint8_t * const		b = (const uint8_t *) inB;
	int							result = 0;
	size_t						i;
	
	for( i = 0; i < inLen; ++i )
	{
		result |= ( a[ i ] ^ b[ i ] );
	}
	return( result );
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MemZeroSecure
	@abstract	Zeros memory in a way that prevents the compiler from optimizing it out (as it could with memset).
	@discussion	This is needed for cases such as clearing a buffer containing a cryptographic key.
*/
#if( ( defined( __STDC_WANT_LIB_EXT1__ ) && ( __STDC_WANT_LIB_EXT1__ >= 1 ) && !TARGET_KERNEL ) || ( QNX_VERSION >= 660 ) )
STATIC_INLINE void	MemZeroSecure( void *inPtr, size_t inLen )
{
	if( inPtr ) memset_s( inPtr, inLen, 0, inLen );
}
#elif( TARGET_OS_WINDOWS )
#define MemZeroSecure( PTR, LEN )		RtlSecureZeroMemory( (PTR), (LEN) )
#else
STATIC_INLINE void	MemZeroSecure( void *inPtr, size_t inLen )
{
	volatile uint8_t *		ptr = (volatile uint8_t *) inPtr;
	
	while( inLen-- ) *ptr++ = 0;
}
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		MinimalMutex
	@abstract	Macros for using a mutex with minimal requirements.
*/
#if  ( TARGET_OS_DARWIN_KERNEL )
	#define MinimalMutexDefine( NAME )					static OSSpinLock NAME = OS_SPINLOCK_INIT
	#define MinimalMutexEnsureInitialized( NAME )		do {} while( 0 )
	#define MinimalMutexEnsureFinalized( NAME )			do {} while( 0 )
	#define MinimalMutexLock( NAME )					do {} while( !OSSpinLockTry( &NAME ) )
	#define MinimalMutexUnlock( NAME )					OSSpinLockUnlock( &NAME )
#elif( TARGET_OS_POSIX )
	#define MinimalMutexDefine( NAME )					static pthread_mutex_t NAME = PTHREAD_MUTEX_INITIALIZER
	#define MinimalMutexEnsureInitialized( NAME )		do {} while( 0 )
	#define MinimalMutexEnsureFinalized( NAME )			do {} while( 0 )
	#define MinimalMutexLock( NAME )					pthread_mutex_lock( &NAME )
	#define MinimalMutexUnlock( NAME )					pthread_mutex_unlock( &NAME )
#elif( TARGET_OS_THREADX )
	#define MinimalMutexDefine( NAME )					static TX_MUTEX NAME
	#define MinimalMutexEnsureInitialized( NAME )		tx_mutex_create( &NAME, # NAME, TX_INHERIT )
	#define MinimalMutexEnsureFinalized( NAME )			tx_mutex_delete( &NAME )
	#define MinimalMutexLock( NAME )					tx_mutex_get( &NAME, TX_WAIT_FOREVER )
	#define MinimalMutexUnlock( NAME )					tx_mutex_put( &NAME )
#elif( TARGET_OS_WINDOWS )
	#define MinimalMutexDefine( NAME )					static CRITICAL_SECTION NAME; static LONG NAME##State = 0
	#define MinimalMutexEnsureInitialized( NAME )		InitializeCriticalSectionOnce( &NAME, &NAME##State )
	#define MinimalMutexEnsureFinalized( NAME )			DeleteCriticalSectionOnce( &NAME, &NAME##State, true )
	#define MinimalMutexLock( NAME )					EnterCriticalSection( &NAME )
	#define MinimalMutexUnlock( NAME )					LeaveCriticalSection( &NAME )
#endif

#if 0
#pragma mark == Time96 ==
#endif

//===========================================================================================================================
//	Time96
//
//	Support for 96-bit (32.64) binary time.
//===========================================================================================================================

typedef struct
{
	int32_t		secs; //! Number of seconds. Epoch depends on usage. 0, 1970-01-01 00:00:00 (Unix time), etc.
	uint64_t	frac; //! Fraction of a second in units of 1/2^64.
	
}	Time96;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Time96ToDouble / DoubleToTime96
	@abstract	Convert between Time96 and floating-point seconds values.
*/
#define Time96ToDouble( T96 )	( ( (double) (T96)->secs ) + ( ( (double) (T96)->frac ) * ( 1.0 / 18446744073709551615.0 ) ) )
#define DoubleToTime96( D, T96 ) \
	do \
	{ \
		double		_DoubleToTime96_secs; \
		\
		_DoubleToTime96_secs = floor( (D) ); \
		(T96)->secs = (int32_t) _DoubleToTime96_secs; \
		(T96)->frac = (uint64_t)( ( (D) - _DoubleToTime96_secs ) * 18446744073709551615.0 ); \
		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Time96ToNTP / NTPToTime96
	@abstract	Convert between Time96 and NTP 32.32 values.
*/
#define Time96ToNTP( T96 )		( ( ( (uint64_t) (T96)->secs ) << 32 ) | ( (T96)->frac >> 32 ) )
#define NTPToTime96( NTP, T96 ) \
	do \
	{ \
		(T96)->secs = (int32_t)( (NTP) >> 32 ); \
		(T96)->frac =          ( (NTP) << 32 ); \
		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Time96ToNTP / NTPToTime96
	@abstract	Convert between Time96 and NTP 32.32 values.
*/
#define Time96ToNTP( T96 )		( ( ( (uint64_t) (T96)->secs ) << 32 ) | ( (T96)->frac >> 32 ) )
#define NTPToTime96( NTP, T96 ) \
	do \
	{ \
		(T96)->secs = (int32_t)( (NTP) >> 32 ); \
		(T96)->frac =          ( (NTP) << 32 ); \
		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Time96FracToNanoseconds / NanosecondsToTime96Frac
	@abstract	Convert between Time96 fractional seconds and nanoseconds.
*/
#define Time96FracToNanoseconds( FRAC )		( ( UINT64_C( 1000000000 ) * (uint32_t)( (FRAC) >> 32 ) ) >> 32 )
#define NanosecondsToTime96Frac( NS )		( (NS) * UINT64_C( 18446744073 ) ) // 2^64 / 1000000000 = 18446744073

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Time96_AddFrac
	@abstract	Adds a fractional seconds (1/2^64 units) value to a time.
*/
STATIC_INLINE void	Time96_AddFrac( Time96 *inTime, uint64_t inFrac )
{
	uint64_t		frac;
	
	frac = inTime->frac;
	inTime->frac = frac + inFrac;
	if( frac > inTime->frac ) inTime->secs += 1; // Increment seconds on fraction wrap.
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Time96_Add
	@abstract	Adds one time to another time.
*/
STATIC_INLINE void	Time96_Add( Time96 *inTime, const Time96 *inAdd )
{
	uint64_t		frac;
	
	frac = inTime->frac;
	inTime->frac = frac + inAdd->frac;
	if( frac > inTime->frac ) inTime->secs += 1; // Increment seconds on fraction wrap.
	inTime->secs += inAdd->secs;
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Time96_Sub
	@abstract	Subtracts one time from another time.
*/
STATIC_INLINE void	Time96_Sub( Time96 *inTime, const Time96 *inSub )
{
	uint64_t	frac;
	
	frac = inTime->frac;
	inTime->frac = frac - inSub->frac;
	if( frac < inTime->frac ) inTime->secs -= 1; // Decrement seconds on fraction wrap.
	inTime->secs -= inSub->secs;
}

#if 0
#pragma mark -
#pragma mark == timeval macros ==
#endif

//===========================================================================================================================
//	timeval macros
//===========================================================================================================================

#define	TIMEVAL_USECS_PER_SEC		1000000

// A == B
#define	TIMEVAL_EQ( A, B )	( ( (A).tv_sec == (B).tv_sec ) && ( (A).tv_usec == (B).tv_usec ) )

// A < B
#define	TIMEVAL_LT( A, B )	(   ( (A).tv_sec  < (B).tv_sec ) || \
							  ( ( (A).tv_sec == (B).tv_sec ) && ( (A).tv_usec < (B).tv_usec ) ) )

// A <= B
#define	TIMEVAL_LE( A, B )	(   ( (A).tv_sec  < (B).tv_sec ) || \
							  ( ( (A).tv_sec == (B).tv_sec ) && ( (A).tv_usec <= (B).tv_usec ) ) )

// A > B
#define	TIMEVAL_GT( A, B )	(   ( (A).tv_sec  > (B).tv_sec ) || \
							  ( ( (A).tv_sec == (B).tv_sec ) && ( (A).tv_usec > (B).tv_usec ) ) )

// A >= B
#define	TIMEVAL_GE( A, B )	(   ( (A).tv_sec  > (B).tv_sec ) || \
							  ( ( (A).tv_sec == (B).tv_sec ) && ( (A).tv_usec >= (B).tv_usec ) ) )

// A  < B = -1
// A  > B =  1
// A == B =  0
#define	TIMEVAL_CMP( A, B ) \
		( (A).tv_sec  < (B).tv_sec )  ? -1 : \
		( (A).tv_sec  > (B).tv_sec )  ?  1 : \
		( (A).tv_usec < (B).tv_usec ) ? -1 : \
		( (A).tv_usec > (B).tv_usec ) ?  1 : 0

// Non-zero if tv_usec is between 0 and (1000000 - 1).
#define	TIMEVAL_VALID( X )		( ( (X).tv_usec >= 0 ) && ( (X).tv_usec < TIMEVAL_USECS_PER_SEC ) )

// Sets X to 0 seconds and 0 microseconds.
#define	TIMEVAL_ZERO( X )		TIMEVAL_SET( X, 0, 0 )

// Sets X from secs and microseconds.
#define	TIMEVAL_SET( X, SECS, USECS ) \
	do \
	{ \
		(X).tv_sec  = ( SECS ); \
		(X).tv_usec = ( USECS ); \
	\
	}	while( 0 )

// A += B
#define	TIMEVAL_ADD( A, B ) \
	do \
	{ \
		(A).tv_sec  += (B).tv_sec; \
		(A).tv_usec += (B).tv_usec; \
		TIMEVAL_NORMALIZE( A ); \
	\
	}	while( 0 )

// A += X. X is the number of microseconds to add.
#define	TIMEVAL_ADD_USEC( A, X ) \
	do \
	{ \
		(A).tv_usec += (X); \
		TIMEVAL_NORMALIZE( A ); \
	\
	}	while( 0 )

// X = A + B
#define	TIMEVAL_ADD_COPY( X, A, B ) \
	do \
	{ \
		(X) = (A); \
		(X).tv_sec  += (B).tv_sec; \
		(X).tv_usec += (B).tv_usec; \
		TIMEVAL_NORMALIZE( X ); \
	\
	}	while( 0 )


// A -= B
#define	TIMEVAL_SUB( A, B ) \
	do \
	{ \
		if( TIMEVAL_GT( A, B ) ) \
		{ \
			(A).tv_sec  -= (B).tv_sec; \
			(A).tv_usec -= (B).tv_usec; \
			TIMEVAL_NORMALIZE( A ); \
		} \
		else \
		{ \
			(A).tv_sec  = 0; \
			(A).tv_usec = 0; \
		} \
	\
	}	while( 0 )

// X = A - B
#define	TIMEVAL_SUB_COPY( X, A, B ) \
	do \
	{ \
		(X) = (A); \
		TIMEVAL_SUB( (X), (B) ); \
	\
	}	while( 0 )

// A *= X. X must be a positive integer. X must be <= 2147 to avoid overflow.
#define	TIMEVAL_MUL( A, X ) \
	do \
	{ \
		(A).tv_sec  *= (X); \
		(A).tv_usec *= (X); \
		TIMEVAL_NORMALIZE( A ); \
	\
	}	while( 0 )

// X = A * Y. Y must be a positive integer. Y must be <= 2147 to avoid overflow.
#define	TIMEVAL_MUL_COPY( X, A, Y ) \
	do \
	{ \
		(X) = (A); \
		(X).tv_sec  *= (Y); \
		(X).tv_usec *= (Y); \
		TIMEVAL_NORMALIZE( X ); \
	\
	}	while( 0 )

// Adjusts tv_sec and tv_usec so tv_usec is between 0 and (1000000 - 1).
#define	TIMEVAL_NORMALIZE( X ) \
	do \
	{ \
		for( ;; ) \
		{ \
			if( (X).tv_usec >= TIMEVAL_USECS_PER_SEC ) \
			{ \
				(X).tv_sec  += 1; \
				(X).tv_usec -= TIMEVAL_USECS_PER_SEC; \
			} \
			else if( (X).tv_usec < 0 ) \
			{ \
				(X).tv_sec  -= 1; \
				(X).tv_usec += TIMEVAL_USECS_PER_SEC; \
			} \
			else \
			{ \
				break; \
			} \
		} \
		\
	}	while( 0 )

// X as single, unsigned 32-bit microseconds value. X must be <= 4294 to avoid overflow.
#define	TIMEVAL_USEC32( X )				( ( ( (uint32_t)(X).tv_sec ) * TIMEVAL_USECS_PER_SEC ) + (X).tv_usec )

// X as single, signed 64-bit microseconds value.
#define	TIMEVAL_USEC64( X )				( ( ( (int64_t)(X).tv_sec ) * TIMEVAL_USECS_PER_SEC ) + (X).tv_usec )

// A - B as single, signed 64-bit microseconds value (negative if A < B).
#define	TIMEVAL_USEC64_DIFF( A, B )		( TIMEVAL_USEC64( A ) - TIMEVAL_USEC64( B ) )

// X as a single, floating point seconds value.
#define TIMEVAL_FP_SECS( X )			( ( (double)(X).tv_sec ) + ( ( (double)(X).tv_usec ) * ( 1.0 / 1000000.0 ) ) )

#if 0
#pragma mark == ANSI/ASCII Escape Sequences ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		ANSI Escape Sequences
	@abstract	Escape sequences for use with printf/etc to terminal windows.
	@discussion
	
	These are intended to be used either directly in a string or via %s in printf. For example:
	
	// Print "hello" in bold then reset it back to normal.
	printf( kANSIBold "hello" kANSINormal "\n" );
	
	// Print "hello" in bold then reset it back to normal.
	printf( "%s%s%s\n", kANSIBold, "hello", kANSINormal );
*/
#define kANSINormal					"\x1B[0m"  // all attributes off
#define	kANSINormalIntensity		"\x1B[22m" // not bold and not faint.
#define kANSIBold					"\x1B[1m"
#define kANSIFaint					"\x1B[2m"  // not widely suppport.
#define kANSIItalic					"\x1B[3m"  // not widely suppport. Sometimes treated as inverse.
#define	kANSIUnderline				"\x1B[4m"  // not widely suppport.
#define	kANSIUnderlineDouble		"\x1B[21m" // not widely suppport.
#define	kANSIUnderlineOff			"\x1B[24m"
#define	kANSIBlink					"\x1B[5m"  // less than 150 per minute.
#define	kANSIBlinkRapid				"\x1B[6m"  // 150 per minute or more.
#define	kANSIBlinkOff				"\x1B[25m" // not widely suppport.
#define	kANSINegative				"\x1B[7m"  // inverse/reverse; swap foreground and background.
#define	kANSIPositive				"\x1B[27m" // inverse of negative.
#define	kANSIConceal				"\x1B[8m"  // not widely supported.
#define	kANSIReveal					"\x1B[28m" // conceal off.

#define kANSIClearScreen			"\x1B[2J" // Clear entire screen.
#define kANSIResetCursor			"\x1B[H"  // Clear entire screen.

// Foreground colors

#define kANSIBlack					"\x1B[30m"
#define kANSIGray					"\x1B[0;37m"
#define kANSIRed					"\x1B[31m"
#define kANSIGreen					"\x1B[32m"
#define kANSIYellow					"\x1B[33m"
#define kANSIBlue					"\x1B[34m"
#define kANSIMagenta				"\x1B[35m"
#define kANSICyan					"\x1B[36m"
#define kANSIWhite					"\x1B[37m"
#define kANSIForeReset				"\x1B[39m"

// Background colors

#define kANSIBackBlack				"\x1B[40m"
#define kANSIBackRed				"\x1B[41m"
#define kANSIBackGreen				"\x1B[42m"
#define kANSIBackYellow				"\x1B[43m"
#define kANSIBackBlue				"\x1B[44m"
#define kANSIBackMagenta			"\x1B[45m"
#define kANSIBackCyan				"\x1B[46m"
#define kANSIBackWhite				"\x1B[47m"
#define kANSIBackReset				"\x1B[49m"

// High Intensity Foreground colors

#define kANSIHighBlack				"\x1B[90m"
#define kANSIHighRed				"\x1B[91m"
#define kANSIHighGreen				"\x1B[92m"
#define kANSIHighYellow				"\x1B[93m"
#define kANSIHighBlue				"\x1B[94m"
#define kANSIHighMagenta			"\x1B[95m"
#define kANSIHighCyan				"\x1B[96m"
#define kANSIHighWhite				"\x1B[97m"
#define kANSIHighForeReset			"\x1B[99m"

// High Intensity Background colors

#define kANSIHighBackBlack			"\x1B[100m"
#define kANSIHighBackRed			"\x1B[101m"
#define kANSIHighBackGreen			"\x1B[102m"
#define kANSIHighBackYellow			"\x1B[103m"
#define kANSIHighBackBlue			"\x1B[104m"
#define kANSIHighBackMagenta		"\x1B[105m"
#define kANSIHighBackCyan			"\x1B[106m"
#define kANSIHighBackWhite			"\x1B[107m"
#define kANSIHighBackReset			"\x1B[109m"

// Unit Test

#define kANSIEscapeSequenceTest \
	kANSINormal				"kANSINormal"				kANSINormal "\n" \
	kANSIBold				"kANSIBold"					kANSINormal "\n" \
	kANSIFaint				"kANSIFaint"				kANSINormal "\n" \
	kANSIItalic				"kANSIItalic"				kANSINormal "\n" \
	kANSIUnderline			"kANSIUnderline"			kANSINormal "\n" \
	kANSIUnderlineDouble	"kANSIUnderlineDouble"		kANSINormal "\n" \
	kANSIUnderlineOff		"kANSIUnderlineOff"			kANSINormal "\n" \
	kANSIBlink				"kANSIBlink"				kANSINormal "\n" \
	kANSIBlinkRapid			"kANSIBlinkRapid"			kANSINormal "\n" \
	kANSIBlinkOff			"kANSIBlinkOff"				kANSINormal "\n" \
	kANSINegative			"kANSINegative"				kANSINormal "\n" \
	kANSIPositive			"kANSIPositive"				kANSINormal "\n" \
	kANSIConceal			"kANSIConceal"				kANSINormal " (kANSIConceal)\n" \
	kANSIReveal				"kANSIReveal"				kANSINormal "\n" \
	\
	kANSIBlack				"kANSIBlack"				kANSINormal " (kANSIBlack)\n" \
	kANSIRed				"kANSIRed"					kANSINormal "\n" \
	kANSIGreen				"kANSIGreen"				kANSINormal "\n" \
	kANSIYellow				"kANSIYellow"				kANSINormal "\n" \
	kANSIBlue				"kANSIBlue"					kANSINormal "\n" \
	kANSIMagenta			"kANSIMagenta"				kANSINormal "\n" \
	kANSICyan				"kANSICyan"					kANSINormal "\n" \
	kANSIWhite				"kANSIWhite"				kANSINormal " (kANSIWhite)\n" \
	kANSIForeReset			"kANSIForeReset"			kANSINormal "\n" \
	\
	kANSIBackBlack			"kANSIBackBlack"			kANSINormal "\n" \
	kANSIBackRed			"kANSIBackRed"				kANSINormal "\n" \
	kANSIBackGreen			"kANSIBackGreen"			kANSINormal "\n" \
	kANSIBackYellow			"kANSIBackYellow"			kANSINormal "\n" \
	kANSIBackBlue			"kANSIBackBlue"				kANSINormal "\n" \
	kANSIBackMagenta		"kANSIBackMagenta"			kANSINormal "\n" \
	kANSIBackCyan			"kANSIBackCyan"				kANSINormal "\n" \
	kANSIBackWhite			"kANSIBackWhite"			kANSINormal "\n" \
	kANSIBackReset			"kANSIBackReset"			kANSINormal "\n" \
	\
	kANSIHighBlack			"kANSIHighBlack"			kANSINormal "\n" \
	kANSIHighRed			"kANSIHighRed"				kANSINormal "\n" \
	kANSIHighGreen			"kANSIHighGreen"			kANSINormal "\n" \
	kANSIHighYellow			"kANSIHighYellow"			kANSINormal "\n" \
	kANSIHighBlue			"kANSIHighBlue"				kANSINormal "\n" \
	kANSIHighMagenta		"kANSIHighMagenta"			kANSINormal "\n" \
	kANSIHighCyan			"kANSIHighCyan"				kANSINormal "\n" \
	kANSIHighWhite			"kANSIHighWhite"			kANSINormal " (kANSIHighWhite)\n" \
	kANSIHighForeReset		"kANSIHighForeReset"		kANSINormal "\n" \
	\
	kANSIHighBackBlack		"kANSIHighBackBlack"		kANSINormal "\n" \
	kANSIHighBackRed		"kANSIHighBackRed"			kANSINormal "\n" \
	kANSIHighBackGreen		"kANSIHighBackGreen"		kANSINormal "\n" \
	kANSIHighBackYellow		"kANSIHighBackYellow"		kANSINormal "\n" \
	kANSIHighBackBlue		"kANSIHighBackBlue"			kANSINormal "\n" \
	kANSIHighBackMagenta	"kANSIHighBackMagenta"		kANSINormal "\n" \
	kANSIHighBackCyan		"kANSIHighBackCyan"			kANSINormal "\n" \
	kANSIHighBackWhite		"kANSIHighBackWhite"		kANSINormal " (kANSIHighBackWhite)\n" \
	kANSIHighBackReset		"kANSIHighBackReset"		kANSINormal "\n"

// ASCII special characters.

#define kASCII_BellChar					'\x07'
#define kASCII_BellStr					"\x07"

#define kASCII_RecordSeparatorChar		'\x1E'
#define kASCII_RecordSeparatorStr		"\x1E"

#if 0
#pragma mark == GreenThreads ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		GreenThreads
	@abstract	Green thread implementation for lightweight, stackless, cooperatively-scheduled threads.
	@discussion
	
	Based on protothreads: <http://www.sics.se/~adam/pt/>
	
	Examples:
		
		int	main( int argc, const char **argv )
		{
			GreenTopThread		thread;
			
			GREENTHREAD_START( &thread, MyThread, NULL );
			while( MyThread( &thread, NULL ) != kGState_Done ) {} // Bad to spin wait like this...only here to so how it works.
			return( 0 );
		}
		
		GState	MyThread( GreenThread *inThread, void *inArg )
		{
			(void) inArg;
			
			GREENTHREAD_BEGIN( inThread );
			
			printf( "Step 1\n" );
			GREENTHREAD_YIELD();
			
			printf( "Step 2\n" );
			GREENTHREAD_YIELD();
			
			GREENTHREAD_END();
		}
	
	Implementation Details:
	
		* GREENTHREAD_END() begins with "do {} while( 0 );" to allow it to be placed immediately after an exit label.
		  Otherwise, some compilers generate warnings about not having a statement after a label.
		
		* GREENTHREAD_YIELD() has an extra "do {} while( 0 );" to avoid warnings about a label without a statement after it.
*/
typedef int_fast8_t		GState;
	#define kGState_Waiting			0 //! Thread is waiting for a condition to become true.
	#define kGState_Done			1 //! Thread has exited.

typedef struct GreenThread		GreenThread;
typedef struct GreenTopThread	GreenTopThread;

typedef GState ( *GreenThreadFunc )( GreenThread *inThread, void *inArg );
struct GreenThread
{
	int					pc;
	GreenTopThread *	topThread;
};

struct GreenTopThread
{
	GreenThread			gt;
	GreenThreadFunc		func;
};

#define GREENTHREAD_START( GTT, FUNC, ARG )		do { (GTT)->gt.pc = 0; (GTT)->gt.topThread = (GTT); (GTT)->func = (FUNC); (FUNC)( &(GTT)->gt, (ARG) ); } while( 0 )
#define GREENTHREAD_ENTER( GT )					{ GreenThread * const __GreenThread = (GT); switch( (GT)->pc ) { case 0:
#define GREENTHREAD_EXIT()						do {} while( 0 ); } return( kGState_Done ); } do {} while( 0 )
#define GREENTHREAD_YIELD()						do { __GreenThread->pc = __LINE__; return( kGState_Waiting ); case __LINE__: do {} while( 0 ); } while( 0 )
#define GREENTHREAD_CALL( GT, FUNC, ARG )		do { (GT)->pc = 0; (GT)->topThread = __GreenThread->topThread; GREENTHREAD_WAIT_UNTIL( (FUNC)( (GT), (ARG) ) == kGState_Done ); } while( 0 )
#define GREENTHREAD_WAIT_UNTIL( X )				do { __GreenThread->pc = __LINE__; case __LINE__: if( !(X) ) return( kGState_Waiting ); } while( 0 )

#endif // __CommonServices_h__
