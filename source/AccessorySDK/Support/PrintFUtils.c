/*
	File:    	PrintFUtils.c
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

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "PrintFUtils.h"

#include "ChecksumUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "RandomNumberUtils.h"
#include "StringUtils.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <time.h>
#endif

#if( TARGET_OS_DARWIN )
	#if( !TARGET_KERNEL && !COMMON_SERVICES_NO_CORE_SERVICES )
		#include <CoreAudio/CoreAudio.h>
	#endif
#endif
#if( TARGET_OS_POSIX )
	#include <net/if.h>
	#include <pthread.h>
	#include <sys/time.h>
	#include <time.h>
	
	#include "MiscUtils.h"
	#include "TimeUtils.h"
#endif

#if( DEBUG_CF_OBJECTS_ENABLED )
	#include "CFUtils.h"
#endif

#include LIBDISPATCH_HEADER

#if( XPC_ENABLED )
	#include XPC_HEADER
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

// Enables limited %f floating-point output.

#if( !defined( PRINTF_ENABLE_FLOATING_POINT ) )
	#if( !TARGET_OS_WINDOWS_KERNEL )
		#define	PRINTF_ENABLE_FLOATING_POINT		1
	#else
		#define	PRINTF_ENABLE_FLOATING_POINT		0
	#endif
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================

static const PrintFFormat		kPrintFFormatDefault = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

typedef int	( *PrintFCallBack )( const char *inStr, size_t inSize, PrintFContext *inContext );

struct PrintFContext
{
	PrintFCallBack			callback;
	char *					str;
	size_t					usedSize;
	size_t					reservedSize;
	
	PrintFUserCallBack		userCallBack;
	void *					userContext;
};

typedef struct PrintFExtension	PrintFExtension;
struct PrintFExtension
{
	PrintFExtension *				next;
	PrintFExtensionHandler_f		handler_f;
	void *							context;
	char							name[ 1 ]; // Variable length.
};

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static int		PrintFWriteAddr( const uint8_t *inAddr, PrintFFormat *inFormat, char *outStr );
static int		PrintFWriteAudioStreamBasicDescription( PrintFContext *inContext, const AudioStreamBasicDescription *inASBD );
static int		PrintFWriteCEC( PrintFContext *ctx, const uint8_t *inData, size_t inLen );
static int		PrintFWriteBits( uint64_t inX, PrintFFormat *inFormat, char *outStr );
#if( DEBUG_CF_OBJECTS_ENABLED )
	static int	PrintFWriteCFObject( PrintFContext *inContext, PrintFFormat *inFormat, CFTypeRef inObj, char *inBuffer );
#endif
#if( DEBUG_CF_OBJECTS_ENABLED && ( !CFLITE_ENABLED || CFL_XML ) )
	static int	PrintFWriteCFXMLObject( PrintFContext *inContext, PrintFFormat *inFormat, CFTypeRef inObj );
#endif
static int		PrintFWriteFill( PrintFContext *inContext, int inC, size_t inCount, char *inBuf );
static int		PrintFWriteFlags( PrintFContext *inContext, PrintFFormat *inFormat, const char *inDescriptors, uint64_t inX );
static int
	PrintFWriteHex( 
		PrintFContext *	inContext, 
		PrintFFormat *	inFormat, 
		int				inIndent, 
		const void *	inData, 
		size_t			inSize, 
		size_t			inMaxSize );
static int		PrintFWriteHexOneLine( PrintFContext *inContext, PrintFFormat *inFormat, const uint8_t *inData, size_t inSize );
static int		PrintFWriteHexByteStream( PrintFContext *inContext, Boolean inUppercase, const uint8_t *inData, size_t inSize );
static int		PrintFWriteMultiLineText( PrintFContext *inContext, PrintFFormat *inFormat, const char *inStr, size_t inLen );
static int		PrintFWriteNumVersion( uint32_t inVersion, char *outStr );
static int		PrintFWriteSingleLineText( PrintFContext *inContext, const char *inStr, size_t inLen );
static int		PrintFWriteObfuscatedPtr( PrintFContext *inContext, const void *inPtr );
static void		_PrintFWriteObfuscatedPtrInit( void *inArg );
static int		PrintFWriteString( const char *inStr, PrintFFormat *inFormat, char *inBuf, const char **outStrPtr );
static int		PrintFWriteText( PrintFContext *inContext, PrintFFormat *inFormat, const char *inText, size_t inSize );
static int		PrintFWriteTimeDuration( uint64_t inSeconds, int inAltMode, char *inBuf );
static int
	PrintFWriteTLV8( 
		PrintFContext *	inContext, 
		PrintFFormat *	inFormat, 
		const char *	inDescriptors, 
		const uint8_t *	inPtr, 
		size_t			inLen );
static int		PrintFWriteTXTRecord( PrintFContext *inContext, PrintFFormat *inFormat, const void *inPtr, size_t inLen );
static int		PrintFWriteUnicodeString( const uint8_t *inStr, PrintFFormat *inFormat, char *inBuf );
static int		PrintFWriteXMLEscaped( PrintFContext *inContext, const char *inPtr, size_t inLen );

#define	print_indent( CONTEXT, N )	PrintFCore( ( CONTEXT ), "%*s", (int)( ( N ) * 4 ), "" )

static int		PrintFCallBackFixedString( const char *inStr, size_t inSize, PrintFContext *inContext );
static int		PrintFCallBackAllocatedString( const char *inStr, size_t inSize, PrintFContext *inContext );
static int		PrintFCallBackUserCallBack( const char *inStr, size_t inSize, PrintFContext *inContext );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

MinimalMutexDefine( gPrintFUtilsLock );

static PrintFExtension *		gExtensionList = NULL;

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	PrintFRegisterExtension
//===========================================================================================================================

OSStatus	PrintFRegisterExtension( const char *inName, PrintFExtensionHandler_f inHandler, void *inContext )
{
	OSStatus				err;
	size_t					len;
	PrintFExtension *		extension;
	
	MinimalMutexEnsureInitialized( gPrintFUtilsLock );
	MinimalMutexLock( gPrintFUtilsLock );
	
	for( extension = gExtensionList; extension; extension = extension->next )
	{
		if( stricmp( extension->name, inName ) == 0 )
		{
			break;
		}
	}
	require_action( !extension, exit, err = kDuplicateErr );
	
	len = strlen( inName ) + 1;
	extension = (PrintFExtension *) malloc( offsetof( PrintFExtension, name ) + len );
	require_action( extension, exit, err = kNoMemoryErr );
	extension->handler_f	= inHandler;
	extension->context		= inContext;
	memcpy( extension->name, inName, len );
	
	extension->next = gExtensionList;
	gExtensionList = extension;
	err = kNoErr;
	
exit:
	MinimalMutexUnlock( gPrintFUtilsLock );
	return( err );
}

//===========================================================================================================================
//	PrintFDeregisterExtension
//===========================================================================================================================

OSStatus	PrintFDeregisterExtension( const char *inName )
{
	PrintFExtension *		curr;
	PrintFExtension **		next;
	
	MinimalMutexEnsureInitialized( gPrintFUtilsLock );
	MinimalMutexLock( gPrintFUtilsLock );
	for( next = &gExtensionList; ( curr = *next ) != NULL; next = &curr->next )
	{
		if( stricmp( curr->name, inName ) == 0 )
		{
			*next = curr->next;
			free( curr );
			MinimalMutexUnlock( gPrintFUtilsLock );
			return( kNoErr );
		}
	}
	MinimalMutexUnlock( gPrintFUtilsLock );
	return( kNotFoundErr );
}

#if 0
#pragma mark -
#endif

#if( COMPILER_OBJC )
//===========================================================================================================================
//	NSPrintF
//===========================================================================================================================

NSString *	NSPrintF( const char *inFormat, ... )
{
	NSString *		result;
	va_list			args;
	
	va_start( args, inFormat );
	result = NSPrintV( inFormat, args );
	va_end( args );
	return( result );
}

//===========================================================================================================================
//	NSPrintV
//===========================================================================================================================

NSString *	NSPrintV( const char *inFormat, va_list inArgs )
{
	NSString *		result;
	char *			cptr = NULL;
	
	VASPrintF( &cptr, inFormat, inArgs );
	result = cptr ? @(cptr) : @"";
	FreeNullSafe( cptr );
	return( result );
}
#endif // COMPILER_OBJC

//===========================================================================================================================
//	SNPrintF
//===========================================================================================================================

int	SNPrintF( void *inBuf, size_t inMaxLen, const char *inFormat, ... )
{
	int			n;
	va_list		args;
	
	va_start( args, inFormat );
	n = VSNPrintF( inBuf, inMaxLen, inFormat, args );
	va_end( args );
	return( n );
}

//===========================================================================================================================
//	VSNPrintF
//===========================================================================================================================

int	VSNPrintF( void *inBuf, size_t inMaxLen, const char *inFormat, va_list inArgs )
{
	int					n;
	PrintFContext		context;
	
	context.callback		= PrintFCallBackFixedString;
	context.str		 		= (char *) inBuf;
	context.usedSize		= 0;
	context.reservedSize	= ( inMaxLen > 0 ) ? inMaxLen - 1 : 0;
	
	n = PrintFCoreVAList( &context, inFormat, inArgs );
	if( inMaxLen > 0 ) context.str[ context.usedSize ] = '\0';
	return( n );
}

//===========================================================================================================================
//	SNPrintF_Add
//===========================================================================================================================

#if( !defined( SNPRINTF_USE_ASSERTS ) )
	#define SNPRINTF_USE_ASSERTS		1
#endif

OSStatus	SNPrintF_Add( char **ioPtr, char *inEnd, const char *inFormat, ... )
{
	char * const		ptr = *ioPtr;
	size_t				len;
	int					n;
	va_list				args;
	
	len = (size_t)( inEnd - ptr );
#if( SNPRINTF_USE_ASSERTS )
	require_action( len > 0, exit, n = kNoSpaceErr );
#else
	require_action_quiet( len > 0, exit, n = kNoSpaceErr );
#endif
	
	va_start( args, inFormat );
	n = VSNPrintF( ptr, len, inFormat, args );
	va_end( args );
#if( SNPRINTF_USE_ASSERTS )
	require( n >= 0, exit );
#else
	require_quiet( n >= 0, exit );
#endif
	if( n >= ( (int) len ) )
	{
		#if( SNPRINTF_USE_ASSERTS )
			dlogassert( "Add '%s' format failed due to lack of space (%d vs %zu)", inFormat, n, len );
		#endif
		*ioPtr = inEnd;
		n = kOverrunErr;
		goto exit;
	}
	*ioPtr = ptr + n;
	n = kNoErr;
	
exit:
	return( n );
}

//===========================================================================================================================
//	AppendPrintF
//===========================================================================================================================

int	AppendPrintF( char **ioStr, const char *inFormat, ... )
{
	int			n;
	va_list		args;
	char *		tempStr;
	
	va_start( args, inFormat );
	n = ASPrintF( &tempStr, "%s%V", *ioStr ? *ioStr : "", inFormat, &args );
	va_end( args );
	require_quiet( n >= 0, exit );
	
	if( *ioStr ) free( *ioStr );
	*ioStr = tempStr;
	
exit:
	return( n );
}

//===========================================================================================================================
//	ASPrintF
//===========================================================================================================================

int	ASPrintF( char **outStr, const char *inFormat, ... )
{
	int			n;
	va_list		args;
	
	va_start( args, inFormat );
	n = VASPrintF( outStr, inFormat, args );
	va_end( args );
	return( n );
}

//===========================================================================================================================
//	VASPrintF
//===========================================================================================================================

int	VASPrintF( char **outStr, const char *inFormat, va_list inArgs )
{
	int					n;
	PrintFContext		context;
	int					tmp;
	
	context.callback		= PrintFCallBackAllocatedString;
	context.str		 		= NULL;
	context.usedSize		= 0;
	context.reservedSize	= 0;
	
	n = PrintFCoreVAList( &context, inFormat, inArgs );
	if( n >= 0 )
	{
		tmp = context.callback( "", 1, &context );
		if( tmp < 0 ) n = tmp;
	}
	if( n >= 0 ) *outStr = context.str;
	else if( context.str ) free( context.str );
	return( n );
}

//===========================================================================================================================
//	CPrintF
//===========================================================================================================================

int	CPrintF( PrintFUserCallBack inCallBack, void *inContext, const char *inFormat, ... )
{
	int			n;
	va_list		args;
	
	va_start( args, inFormat );
	n = VCPrintF( inCallBack, inContext, inFormat, args );
	va_end( args );
	return( n );
}

//===========================================================================================================================
//	VCPrintF
//===========================================================================================================================

int	VCPrintF( PrintFUserCallBack inCallBack, void *inContext, const char *inFormat, va_list inArgs )
{
	int					n;
	PrintFContext		context;
	int					tmp;
	
	context.callback		= PrintFCallBackUserCallBack;
	context.str		 		= NULL;
	context.usedSize		= 0;
	context.reservedSize	= 0;
	context.userCallBack	= inCallBack;
	context.userContext		= inContext;
	
	n = PrintFCoreVAList( &context, inFormat, inArgs );
	if( n >= 0 )
	{
		tmp = context.callback( "", 0, &context );
		if( tmp < 0 ) n = tmp;
	}
	return( n );
}

#if( TARGET_HAS_C_LIB_IO )
//===========================================================================================================================
//	FPrintF
//===========================================================================================================================

int	FPrintF( FILE *inFile, const char *inFormat, ... )
{
	int			n;
	va_list		args;
	
	va_start( args, inFormat );
	n = VFPrintF( inFile, inFormat, args );
	va_end( args );
	
	return( n );
}

//===========================================================================================================================
//	VFPrintF
//===========================================================================================================================

static int	FPrintFCallBack( const char *inStr, size_t inSize, void *inContext );

int	VFPrintF( FILE *inFile, const char *inFormat, va_list inArgs )
{
	return( VCPrintF( FPrintFCallBack, inFile, inFormat, inArgs ) );
}

//===========================================================================================================================
//	FPrintFCallBack
//===========================================================================================================================

static int	FPrintFCallBack( const char *inStr, size_t inSize, void *inContext )
{
	FILE * const		file = (FILE *) inContext;
	
	if( file ) fwrite( inStr, 1, inSize, file );
	return( (int) inSize );
}
#endif // TARGET_HAS_C_LIB_IO

//===========================================================================================================================
//	MemPrintF
//===========================================================================================================================

int	MemPrintF( void *inBuf, size_t inMaxLen, const char *inFormat, ... )
{
	int			n;
	va_list		args;
	
	va_start( args, inFormat );
	n = VMemPrintF( inBuf, inMaxLen, inFormat, args );
	va_end( args );
	return( n );
}

//===========================================================================================================================
//	VMemPrintF
//===========================================================================================================================

int	VMemPrintF( void *inBuf, size_t inMaxLen, const char *inFormat, va_list inArgs )
{
	int					n;
	PrintFContext		context;
	
	context.callback		= PrintFCallBackFixedString;
	context.str		 		= (char *) inBuf;
	context.usedSize		= 0;
	context.reservedSize	= inMaxLen;
	
	n = PrintFCoreVAList( &context, inFormat, inArgs );
	return( n );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	PrintFCore
//===========================================================================================================================

int	PrintFCore( PrintFContext *inContext, const char *inFormat, ... )
{
	int			n;
	va_list		args;
	
	va_start( args, inFormat );
	n = PrintFCoreVAList( inContext, inFormat, args );
	va_end( args );
	return( n );
}

//===========================================================================================================================
//	PrintFCoreVAList
//===========================================================================================================================

#define	kPrintFBufSize				300 // Enough space for a 256-byte domain name and some error text.

#define PrintFIsPrintable( C )		( ( (C) >= 0x20 ) && ( (C) < 0x7F ) )
#define PrintFMakePrintable( C )	( (char)( PrintFIsPrintable( (C) ) ? (C) : (C) ? '^' : '.' ) )

int	PrintFCoreVAList( PrintFContext *inContext, const char *inFormat, va_list inArgs )
{
	int					total = 0;
	const char *		fmt    = inFormat;
	PrintFVAList		vaArgs;
	const char *		src;
	int					err;
	char				buf[ kPrintFBufSize ];
	char *				p;
	int					c;
	PrintFFormat		F;
	int					i;
	const char *		digits;
	const char *		s;
	const uint8_t *		a;
	int					n;
	size_t				size;
	size_t				sizeMax;
	uint64_t			x;
	unsigned int		base;
	uint32_t			remain;
	unsigned int		extra;
	
#if( defined( va_copy ) )
	va_copy( vaArgs.args, inArgs );
#else
	vaArgs.args = inArgs; // Not portable and only works on va_list's that are pointers and not arrays.
#endif
	
	for( c = *fmt; ; c = *++fmt )
	{
		// Non-conversion characters are copied directly to the output.
		
		src = fmt;
		while( ( c != '\0' ) && ( c != '%' ) ) c = *++fmt;
		if( fmt != src )
		{
			i = (int)( fmt - src );
			err = inContext->callback( src, (size_t) i, inContext );
			if( err < 0 ) goto error;
			total += i;
		}
		if( c == '\0' ) break;
		
		F = kPrintFFormatDefault;
		
		// Flags
		
		for( ;; )
		{
			c = *++fmt;
			if(      c == '-' )  F.leftJustify	= 1;
			else if( c == '+' )  F.forceSign	= 1;
			else if( c == ' ' )  F.sign			= ' ';
			else if( c == '#' )  F.altForm 	   += 1;
			else if( c == '0' )  F.zeroPad		= 1;
			else if( c == '\'' ) F.group	   += 1;
			else if( c == '?' )  F.suppress		= !va_arg( vaArgs.args, int );
			else break;
		}
		
		// Field Width
		
		if( c == '*' )
		{
			i = va_arg( vaArgs.args, int );
			if( i < 0 )
			{
				i = -i;
				F.leftJustify = 1;
			}
			F.fieldWidth = (unsigned int) i;
			c = *++fmt;
		}
		else
		{
			for( ; ( c >= '0' ) && ( c <= '9' ); c = *++fmt )
			{
				F.fieldWidth = ( 10 * F.fieldWidth ) + ( c - '0' );
			}
		}
		
		// Precision
		
		if( c == '.' )
		{
			c = *++fmt;
			if( c == '*' )
			{
				F.precision = va_arg( vaArgs.args, unsigned int );
				c = *++fmt;
			}
			else
			{
				for( ; ( c >= '0' ) && ( c <= '9' ); c = *++fmt )
				{
					F.precision = ( 10 * F.precision ) + ( c - '0' );
				}
			}
			F.havePrecision = 1;
		}
		if( F.leftJustify ) F.zeroPad = 0;
		
		// Length modifiers
		
		for( ;; )
		{
			if(      c == 'h' ) { ++F.hSize; c = *++fmt; }
			else if( c == 'l' ) { ++F.lSize; c = *++fmt; }
			else if( c == 'j' )
			{
				if( F.hSize || F.lSize ) { err = -1; goto error; }
				// Disable unreachable code warnings because unreachable paths are reachable on some architectures.
				begin_unreachable_code_paths()
				if(      sizeof( intmax_t ) == sizeof( long ) )		F.lSize = 1;
				else if( sizeof( intmax_t ) == sizeof( int64_t ) )	F.lSize = 2;
				else												F.lSize = 0;
				end_unreachable_code_paths()
				c = *++fmt;
				break;
			}
			else if( c == 'z' )
			{
				if( F.hSize || F.lSize ) { err = -1; goto error; };
				// Disable unreachable code warnings because unreachable paths are reachable on some architectures.
				begin_unreachable_code_paths()
				if(      sizeof( size_t ) == sizeof( long ) )		F.lSize = 1;
				else if( sizeof( size_t ) == sizeof( int64_t ) )	F.lSize = 2;
				else												F.lSize = 0;
				end_unreachable_code_paths()
				c = *++fmt;
				break;
			}
			else if( c == 't' )
			{
				if( F.hSize || F.lSize ) { err = -1; goto error; };
				// Disable unreachable code warnings because unreachable paths are reachable on some architectures.
				begin_unreachable_code_paths()
				if(      sizeof( ptrdiff_t ) == sizeof( long ) )	F.lSize = 1;
				else if( sizeof( ptrdiff_t ) == sizeof( int64_t ) )	F.lSize = 2;
				else												F.lSize = 0;
				end_unreachable_code_paths()
				c = *++fmt;
				break;
			}
			else break;
		}
		if( F.hSize > 2 )		 { err = -1; goto error; };
		if( F.lSize > 2 )		 { err = -1; goto error; };
		if( F.hSize && F.lSize ) { err = -1; goto error; };
		
		// Conversions
		
		digits = kHexDigitsUppercase;
		switch( c )
		{
			// %d, %i, %u, %o, %b, %x, %X, %p: Number
			
			case 'd':
			case 'i': base = 10; goto canBeSigned;
			case 'u': base = 10; goto notSigned;
			case 'o': base =  8; goto notSigned;
			case 'b': base =  2; goto notSigned;
			case 'x': digits = kHexDigitsLowercase;
			case 'X': base = 16; goto notSigned;
			case 'p':
				x = (uintptr_t) va_arg( vaArgs.args, void * );
				F.precision		= sizeof( void * ) * 2;
				F.havePrecision = 1;
				F.altForm		= 1;
				F.sign			= 0;
				base			= 16;
				c				= 'x';
				goto number;
			
			canBeSigned:
				if(      F.lSize == 1 )	x = (uint64_t) va_arg( vaArgs.args, long );
				else if( F.lSize == 2 )	x = (uint64_t) va_arg( vaArgs.args, int64_t );
				else					x = (uint64_t) va_arg( vaArgs.args, int );
				if(      F.hSize == 1 )	x = (uint64_t)(short)( x & 0xFFFF );
				else if( F.hSize == 2 )	x = (uint64_t)(signed char)( x & 0xFF );
				if( (int64_t) x < 0 ) { x = (uint64_t)( -(int64_t) x ); F.sign = '-'; }
				else if( F.forceSign ) F.sign = '+';
				goto number;
			
			notSigned:
				if(      F.lSize == 1 )	x = va_arg( vaArgs.args, unsigned long );
				else if( F.lSize == 2 )	x = va_arg( vaArgs.args, uint64_t );
				else					x = va_arg( vaArgs.args, unsigned int );
				if(      F.hSize == 1 )	x = (unsigned short)( x & 0xFFFF );
				else if( F.hSize == 2 )	x = (unsigned char)( x & 0xFF );
				F.sign = 0;
				goto number;
			
			number:
				if( F.suppress ) continue;
				if( ( base == 2 ) && ( F.altForm > 1 ) )
				{
					i = PrintFWriteBits( x, &F, buf );
					s = buf;
				}
				else
				{
					if( !F.havePrecision )
					{
						if( F.zeroPad )
						{
							extra = 0;
							if( F.altForm )
							{
								if(      base ==  8 ) extra += 1; // Make room for the leading "0".
								else if( base != 10 ) extra += 2; // Make room for the leading "0x", "0b", etc.
							}
							if( F.sign ) extra += 1; // Make room for the leading "+" or "-".
							F.precision = ( F.fieldWidth > extra ) ? ( F.fieldWidth - extra ) : 0;
						}
						if( F.precision < 1 ) F.precision = 1;
					}
					if( F.precision > ( sizeof( buf ) - 1 ) ) F.precision = sizeof( buf ) - 1;
					
					p = buf + sizeof( buf );
					i = 0;
					if( F.group )
					{
						n = 0;
						for( ;; )
						{
							Divide64x32( x, base, remain );
							*--p = digits[ remain ]; ++i; ++n;
							if( !x ) break;
							if( ( n % 3 ) == 0 ) { *--p = ','; ++i; }
						}
					}
					else
					{
						while( x )
						{
							Divide64x32( x, base, remain );
							*--p = digits[ remain ]; ++i;
						}
					}
					for( ; i < (int) F.precision; ++i )	*--p = '0';
					if( F.altForm )
					{
						if(      base ==  8 ) {                  *--p = '0'; i += 1; }
						else if( base != 10 ) { *--p = (char) c; *--p = '0'; i += 2; }
					}
					if( F.sign ) { *--p = F.sign; ++i; }
					s = p;
				}
				break;
		
		#if( PRINTF_ENABLE_FLOATING_POINT )
			case 'f':	// %f: Floating point
			{
				char		fpFormat[ 9 ];
				double		dx;
				
				i = 0;
				fpFormat[ i++ ]						= '%';
				if( F.forceSign ) fpFormat[ i++ ]	= '+';
				if( F.altForm )   fpFormat[ i++ ]	= '#';
				if( F.zeroPad )   fpFormat[ i++ ]	= '0';
				fpFormat[ i++ ]						= '*';
				if( F.havePrecision )
				{
					fpFormat[ i++ ]					= '.';
					fpFormat[ i++ ]					= '*';
				}
				fpFormat[ i++ ]						= 'f';
				fpFormat[ i ]						= '\0';
				
				i = (int) F.fieldWidth;
				if( F.leftJustify ) i = -i;
				dx = va_arg( vaArgs.args, double );
				if( F.suppress ) continue;
				if( F.havePrecision ) i = snprintf( buf, sizeof( buf ), fpFormat, i, (int) F.precision, dx );
				else				  i = snprintf( buf, sizeof( buf ), fpFormat, i, dx );
				if( i < 0 ) { err = i; goto error; }
				s = buf;
				break;
			}
		#endif
		
		#if( TARGET_OS_WINDOWS && !defined( UNICODE ) && !defined( _UNICODE ) )
			case 'T':	// %T: TCHAR string (Windows only)
		#endif
			case 's':	// %s: String
				src = va_arg( vaArgs.args, const char * );
				if( F.suppress ) continue;
				if( !src && ( !F.havePrecision || ( F.precision != 0 ) ) ) { s = "<<NULL>>"; i = 8; break; }
				if( F.group && F.havePrecision )
				{
					if( F.precision >= 2 ) F.precision -= 2;
					else				 { F.precision  = 0; F.group = '\0'; }
				}
				i = PrintFWriteString( src, &F, buf, &s );
				if(      F.group == 1 ) { F.prefix = '\''; F.suffix = '\''; }
				else if( F.group == 2 ) { F.prefix = '"';  F.suffix = '"'; }
				break;
		
		#if( TARGET_OS_WINDOWS && ( defined( UNICODE ) || defined( _UNICODE ) ) )
			case 'T':	// %T: TCHAR string (Windows only)
		#endif
			case 'S':	// %S: Unicode String
				a = va_arg( vaArgs.args, uint8_t * );
				if( F.suppress ) continue;
				if( !a && ( !F.havePrecision || ( F.precision != 0 ) ) ) { s = "<<NULL>>"; i = 8; break; }
				if( F.group && F.havePrecision )
				{
					if( F.precision >= 2 ) F.precision -= 2;
					else				 { F.precision  = 0; F.group = '\0'; }
				}
				i = PrintFWriteUnicodeString( a, &F, buf );
				s = buf;
				if(      F.group == 1 ) { F.prefix = '\''; F.suffix = '\''; }
				else if( F.group == 2 ) { F.prefix = '"';  F.suffix = '"'; }
				break;
			
			case '@':	// %@: Cocoa/CoreFoundation Object
				a = va_arg( vaArgs.args, uint8_t * );
				if( F.suppress ) continue;
				
				#if( DEBUG_CF_OBJECTS_ENABLED )
				{
					CFTypeRef		cfObj;
					
					cfObj = (CFTypeRef) a;
					if( !cfObj ) cfObj = CFSTR( "<<NULL>>" );
					
					if( F.group && F.havePrecision )
					{
						if( F.precision >= 2 ) F.precision -= 2;
						else				 { F.precision  = 0; F.group = '\0'; }
					}
					if(      F.group == 1 ) { F.prefix = '\''; F.suffix = '\''; }
					else if( F.group == 2 ) { F.prefix = '"';  F.suffix = '"'; }
					
					#if( !CFLITE_ENABLED || CFL_XML )
						if( F.altForm ) err = PrintFWriteCFXMLObject( inContext, &F, cfObj );
						else
					#endif
					err = PrintFWriteCFObject( inContext, &F, cfObj, buf );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				#else
					i = SNPrintF( buf, sizeof( buf ), "<<%%@=%p WITH CF OBJECTS DISABLED>>", a );
					s = buf;
					break;
				#endif
			
			case 'm':	// %m: Error Message
			{
				OSStatus		errCode;
				
				errCode = va_arg( vaArgs.args, OSStatus );
				if( F.suppress ) continue;
				if( F.altForm )
				{
					if( PrintFIsPrintable( ( errCode >> 24 ) & 0xFF ) && 
						PrintFIsPrintable( ( errCode >> 16 ) & 0xFF ) &&
						PrintFIsPrintable( ( errCode >>  8 ) & 0xFF ) &&
						PrintFIsPrintable(   errCode         & 0xFF ) )
					{
						if( F.altForm == 2 )
						{
							i = SNPrintF( buf, sizeof( buf ), "%-11d    0x%08X    '%C'    ", 
								(int) errCode, (unsigned int) errCode, (uint32_t) errCode );
						}
						else
						{
							i = SNPrintF( buf, sizeof( buf ), "%d/0x%X/'%C' ", 
								(int) errCode, (unsigned int) errCode, (uint32_t) errCode );
						}
					}
					else
					{
						if( F.altForm == 2 )
						{
							i = SNPrintF( buf, sizeof( buf ), "%-11d    0x%08X    '^^^^'    ", 
								(int) errCode, (unsigned int) errCode, (uint32_t) errCode );
						}
						else
						{
							i = SNPrintF( buf, sizeof( buf ), "%d/0x%X ", 
								(int) errCode, (unsigned int) errCode, (uint32_t) errCode );
						}
					}
				}
				else
				{
					#if( DEBUG || DEBUG_EXPORT_ERROR_STRINGS )
						i = 0;
					#else
						i = SNPrintF( buf, sizeof( buf ), "%d/0x%X ", (int) errCode, (unsigned int) errCode );
					#endif
				}
				#if( DEBUG || DEBUG_EXPORT_ERROR_STRINGS )
					DebugGetErrorString( errCode, &buf[ i ], sizeof( buf ) - ( (size_t) i ) );
				#endif
				s = buf;
				for( i = 0; s[ i ]; ++i ) {}
				break;
			}
			
			case 'H':	// %H: Hex Dump
				a		= va_arg( vaArgs.args, uint8_t * );
				size	= (size_t) va_arg( vaArgs.args, int );
				sizeMax	= (size_t) va_arg( vaArgs.args, int );
				if( F.suppress ) continue;
				if( a || ( size == 0 ) )
				{
					if( size == kSizeCString ) size = strlen( (const char *) a );
					if(      F.precision == 0 ) err = PrintFWriteHexOneLine( inContext, &F, a, Min( size, sizeMax ) );
					else if( F.precision == 1 ) err = PrintFWriteHex( inContext, &F, (int) F.fieldWidth, a, size, sizeMax );
					else if( F.precision == 2 )
					{
						if(       size <= 0 )	err = PrintFCore( inContext, "(0 bytes)\n" );
						else if ( size <= 16 )	err = PrintFWriteHex( inContext, &F, 0, a, size, sizeMax );
						else
						{
							err = PrintFCore( inContext, "\n" );
							if( err < 0 ) goto error;
							
							err = PrintFWriteHex( inContext, &F, (int) F.fieldWidth, a, size, sizeMax );
						}
					}
					else if( F.precision == 3 ) err = PrintFWriteHexByteStream( inContext, false, a, Min( size, sizeMax ) );
					else if( F.precision == 4 ) err = PrintFWriteHexByteStream( inContext, true, a, Min( size, sizeMax ) );
					else						err = PrintFCore( inContext, "<< BAD %%H PRECISION >>" );
					if( err < 0 ) goto error;
					total += err;
				}
				else
				{
					err = PrintFCore( inContext, "<<NULL %zu/%zu>>", size, sizeMax );
					if( err < 0 ) goto error;
					total += err;
				}
				continue;
			
			case 'c':	// %c: Character
				c = va_arg( vaArgs.args, int );
				if( F.suppress ) continue;
				if( F.group )
				{
					buf[ 0 ] = '\'';
					buf[ 1 ] = PrintFMakePrintable( c );
					buf[ 2 ] = '\'';
					i = 3;
				}
				else
				{
					buf[ 0 ] = (char) c;
					i = 1;
				}
				s = buf;
				break;
			
			case 'C':	// %C: FourCharCode
				x = va_arg( vaArgs.args, uint32_t );
				if( F.suppress ) continue;
				i = 0;
				if( F.group ) buf[ i++ ] = '\'';
				buf[ i ] = (char)( ( x >> 24 ) & 0xFF ); buf[ i ] = PrintFMakePrintable( buf[ i ] ); ++i;
				buf[ i ] = (char)( ( x >> 16 ) & 0xFF ); buf[ i ] = PrintFMakePrintable( buf[ i ] ); ++i;
				buf[ i ] = (char)( ( x >>  8 ) & 0xFF ); buf[ i ] = PrintFMakePrintable( buf[ i ] ); ++i;
				buf[ i ] = (char)(   x         & 0xFF ); buf[ i ] = PrintFMakePrintable( buf[ i ] ); ++i;
				if( F.group ) buf[ i++ ] = '\'';
				s = buf;
				break;
			
			case 'a':	// %a: Address
				a = va_arg( vaArgs.args, const uint8_t * );
				if( F.suppress ) continue;
				if( !a ) { s = "<<NULL>>"; i = 8; break; }
				i = PrintFWriteAddr( a, &F, buf );
				s = buf;
				break;
			
			case 'N':	// %N Now (date/time string).
				if( F.suppress ) continue;
				#if( TARGET_OS_POSIX )
				{
					struct timeval		now;
					time_t				nowTT;
					struct tm *			nowTM;
					char				dateTimeStr[ 24 ];
					char				amPMStr[ 8 ];
					
					gettimeofday( &now, NULL );
					nowTT = now.tv_sec;
					nowTM = localtime( &nowTT );
					if( F.altForm )	strftime( dateTimeStr, sizeof( dateTimeStr ), "%Y-%m-%d_%I-%M-%S", nowTM );
					else			strftime( dateTimeStr, sizeof( dateTimeStr ), "%Y-%m-%d %I:%M:%S", nowTM );
					strftime( amPMStr, sizeof( amPMStr ), "%p", nowTM );
					i = SNPrintF( buf, sizeof( buf ), "%s.%06u%c%s", dateTimeStr, now.tv_usec, F.altForm ? '-' : ' ', amPMStr );
					s = buf;
				}
				#elif( TARGET_HAS_STD_C_LIB )
				{
					time_t			now;
					struct tm *		nowTM;
					
					buf[ 0 ] = '\0';
					now = time( NULL );
					nowTM = localtime( &now );
					if( F.altForm )	i = (int) strftime( buf, sizeof( buf ), "%Y-%m-%d_%I-%M-%S-%p", nowTM );
					else			i = (int) strftime( buf, sizeof( buf ), "%Y-%m-%d %I:%M:%S %p", nowTM );
					s = buf;
				}
				#elif( TARGET_OS_DARWIN_KERNEL || ( TARGET_OS_NETBSD && TARGET_KERNEL ) )
				{
					struct timeval		now;
					int64_t				secs;
					int					year, month, day, hour, minute, second;
					
					microtime( &now );
					secs = now.tv_sec + ( INT64_C( 719163 ) * kSecondsPerDay ); // Seconds to 1970-01-01 00:00:00.
					SecondsToYMD_HMS( secs, &year, &month, &day, &hour, &minute, &second );
					if( F.altForm )
					{
						i = SNPrintF( buf, sizeof( buf ), "%04d-%02d-%02d_%02d-%02d-%02d.%06u-%s", 
							year, month, day, Hour24ToHour12( hour ), minute, second, now.tv_usec, Hour24ToAMPM( hour ) );
					}
					else
					{
						i = SNPrintF( buf, sizeof( buf ), "%04d-%02d-%02d %02d:%02d:%02d.%06u %s", 
							year, month, day, Hour24ToHour12( hour ), minute, second, now.tv_usec, Hour24ToAMPM( hour ) );
					}
					s = buf;
				}
				#else
					s = "<<NO TIME>>";
					i = 11;
				#endif
				break;
			
			case 'U':	// %U: UUID
				a = va_arg( vaArgs.args, const uint8_t * );
				if( F.suppress ) continue;
				if( !a ) { s = "<<NULL>>"; i = 8; break; }
				
				// Note: Windows and EFI treat some sections as 32-bit and 16-bit little endian values and those are the
				// most common UUID's so default to that, but allow %#U to print big-endian UUIDs.
				
				if( F.altForm == 0 )
				{
					i = SNPrintF( buf, sizeof( buf ), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", 
						a[ 3 ], a[ 2 ], a[  1 ], a[  0 ], a[  5 ], a[  4 ], a[  7 ], a[  6 ], 
						a[ 8 ], a[ 9 ], a[ 10 ], a[ 11 ], a[ 12 ], a[ 13 ], a[ 14 ], a[ 15 ] );
				}
				else
				{
					i = SNPrintF( buf, sizeof( buf ), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", 
						a[ 0 ], a[ 1 ], a[  2 ], a[  3 ], a[  4 ], a[  5 ], a[  6 ], a[  7 ], 
						a[ 8 ], a[ 9 ], a[ 10 ], a[ 11 ], a[ 12 ], a[ 13 ], a[ 14 ], a[ 15 ] );
				}
				s = buf;
				break;
			
			case 'v' :	// %v: NumVersion
				x = va_arg( vaArgs.args, unsigned int );
				if( F.suppress ) continue;
				i = PrintFWriteNumVersion( (uint32_t) x, buf );
				s = buf;
				break;
			
			case 'V':	// %V: Nested PrintF format string and va_list.
			{
				const char *		nestedFormat;
				va_list *			nestedArgs;
				
				nestedFormat = va_arg( vaArgs.args, const char * );
				nestedArgs   = va_arg( vaArgs.args, va_list * );
				if( F.suppress ) continue;
				if( !nestedFormat || !nestedArgs ) { s = "<<NULL>>"; i = 8; break; }
				
				err = PrintFCoreVAList( inContext, nestedFormat, *nestedArgs );
				if( err < 0 ) goto error;
				total += err;
				continue;
			}
			
			case 'n' :	// %n: Receive the number of characters written so far.
				p = va_arg( vaArgs.args, char * );
				if(      F.hSize == 1 ) *( (short   *) p ) = (short) total;
				else if( F.hSize == 2 ) *( (char    *) p ) = (char)  total;
				else if( F.lSize == 1 )	*( (long    *) p ) = (long)  total;
				else if( F.lSize == 2 )	*( (int64_t *) p ) = (long)  total;
				else					*( (int     *) p ) = total;
				continue;
			
			case '%':	// %%: Literal %
				if( F.suppress ) continue;
				buf[ 0 ] = '%';
				i = 1;
				s = buf;
				break;
			
			case '{':	// %{<extension>}
			{
				const char *			extensionPtr;
				size_t					extensionLen;
				PrintFExtension *		extension;
				
				extensionPtr = ++fmt;
				while( ( c != '\0' ) && ( c != '}' ) ) c = *++fmt;
				extensionLen = (size_t)( fmt - extensionPtr );
				
				// %{asbd}: AudioStreamBasicDescription
				
				if( strnicmpx( extensionPtr, extensionLen, "asbd" ) == 0 )
				{
					const AudioStreamBasicDescription * const		absd = va_arg( vaArgs.args, const AudioStreamBasicDescription * );
					
					if( F.suppress ) continue;
					err = PrintFWriteAudioStreamBasicDescription( inContext, absd );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				
				// %{cec}: HDMI CEC message.
				
				if( strnicmpx( extensionPtr, extensionLen, "cec" ) == 0 )
				{
					a    = va_arg( vaArgs.args, uint8_t * );
					size = (size_t) va_arg( vaArgs.args, int );
					if( F.suppress ) continue;
					err = PrintFWriteCEC( inContext, a, size );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				
				// %{dur}:  Time Duration (e.g. 930232 seconds prints "10d 18h 23m 52s").
				// %#{dur}: Time Duration (e.g. 930232 seconds prints "10d 18:23:52").
				
				if( strnicmpx( extensionPtr, extensionLen, "dur" ) == 0 )
				{
					if(      F.lSize == 1 )	x = va_arg( vaArgs.args, unsigned long );
					else if( F.lSize == 2 )	x = va_arg( vaArgs.args, uint64_t );
					else					x = va_arg( vaArgs.args, unsigned int );
					if(      F.hSize == 1 )	x = (unsigned short)( x & 0xFFFF );
					else if( F.hSize == 2 )	x = (unsigned char)( x & 0xFF );
					if( F.suppress ) continue;
					i = PrintFWriteTimeDuration( x, F.altForm, buf );
					s = buf;
					break;
				}
				
				// %{end}: End printing (used with ? for conditional suppression).
				
				if( strnicmpx( extensionPtr, extensionLen, "end" ) == 0 )
				{
					if( F.suppress ) continue;
					goto exit;
				}
				
				// %{fill}: Repeat a single character N times.
				
				if( strnicmpx( extensionPtr, extensionLen, "fill" ) == 0 )
				{
					c    = va_arg( vaArgs.args, int );
					size = (size_t) va_arg( vaArgs.args, int );
					if( F.suppress ) continue;
					
					err = PrintFWriteFill( inContext, c, size, buf );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				
				// %{flags}: Bit flags (e.g. "0x43 <POWER LINK ERROR>").
				
				if( strnicmpx( extensionPtr, extensionLen, "flags" ) == 0 )
				{
					if(      F.lSize == 1 )	x = va_arg( vaArgs.args, unsigned long );
					else if( F.lSize == 2 )	x = va_arg( vaArgs.args, uint64_t );
					else					x = va_arg( vaArgs.args, unsigned int );
					if(      F.hSize == 1 )	x = (unsigned short)( x & 0xFFFF );
					else if( F.hSize == 2 )	x = (unsigned char)( x & 0xFF );
					s    = va_arg( vaArgs.args, const char * ); 
					if( F.suppress ) continue;
					
					err = PrintFWriteFlags( inContext, &F, s ? s : "\x00", x );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				
				// %{pid}: Process name (with optional numeric PID).
				
				if( strnicmpx( extensionPtr, extensionLen, "pid" ) == 0 )
				{
					#if( TARGET_OS_POSIX )
						pid_t		pid;
						
						if(  sizeof( pid_t ) > sizeof( int ) )	pid = (pid_t) va_arg( vaArgs.args, int64_t );
						else									pid = (pid_t) va_arg( vaArgs.args, int );
						if( F.suppress ) continue;
						*buf = '\0';
						GetProcessNameByPID( pid, buf, sizeof( buf ) );
						if( F.altForm ) err = PrintFCore( inContext, "%s:%lld", buf, (int64_t) pid );
						else			err = PrintFCore( inContext, "%s", buf );
					#else
						err = PrintFCore( inContext, "<< ERROR: %%{pid} not supported on this platform >>" );
					#endif
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				
				// %{ptr}: Obfuscated pointer.
				
				if( strnicmpx( extensionPtr, extensionLen, "ptr" ) == 0 )
				{
					a = va_arg( vaArgs.args, uint8_t * );
					if( F.suppress ) continue;
					err = PrintFWriteObfuscatedPtr( inContext, a );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				
				// %{sline}: Single line string. \r and \n are replaced with ⏎. Arg=ptr to string.
				
				if( strnicmpx( extensionPtr, extensionLen, "sline" ) == 0 )
				{
					s    = va_arg( vaArgs.args, const char * );
					size = va_arg( vaArgs.args, size_t );
					if( F.suppress ) continue;
					if( size == kSizeCString ) size = strlen( s );
					
					err = PrintFWriteSingleLineText( inContext, s, size );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				
				// %{text}: Multi-line text (with optional indenting).
				
				if( strnicmpx( extensionPtr, extensionLen, "text" ) == 0 )
				{
					s    = va_arg( vaArgs.args, const char * );
					size = va_arg( vaArgs.args, size_t );
					if( F.suppress ) continue;
					if( size == kSizeCString ) size = strlen( s );
					
					err = PrintFWriteMultiLineText( inContext, &F, s, size );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				
				// %{tlv8}: 8-bit Type-Length-Value (TLV) data.
				
				if( strnicmpx( extensionPtr, extensionLen, "tlv8" ) == 0 )
				{
					s    = va_arg( vaArgs.args, const char * ); 
					a    = va_arg( vaArgs.args, uint8_t * );
					size = (size_t) va_arg( vaArgs.args, int );
					if( F.suppress ) continue;
					err = PrintFWriteTLV8( inContext, &F, s ? s : "\x00", a, size );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				
				// %{tpl}: EFI Task Priority Level (TPL).
				
				
				// %{txt}: DNS TXT record name=value pairs.
				
				if( strnicmpx( extensionPtr, extensionLen, "txt" ) == 0 )
				{
					a    = va_arg( vaArgs.args, uint8_t * );
					size = va_arg( vaArgs.args, size_t );
					if( F.suppress ) continue;
					err = PrintFWriteTXTRecord( inContext, &F, a, size );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				
				// %{xml}: XML-escaped text.
				
				if( strnicmpx( extensionPtr, extensionLen, "xml" ) == 0 )
				{
					s    = va_arg( vaArgs.args, const char * );
					size = va_arg( vaArgs.args, size_t );
					if( F.suppress ) continue;
					err = PrintFWriteXMLEscaped( inContext, s, size );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				
				// %{xpc}: XPC Object.
				
				if( strnicmpx( extensionPtr, extensionLen, "xpc" ) == 0 )
				{
					a = va_arg( vaArgs.args, uint8_t * );
					if( F.suppress ) continue;
					if( !a ) { s = "<<NULL>>"; i = 8; break; }
					
					#if( XPC_ENABLED )
						p = xpc_copy_description( (xpc_object_t) a );
						if( !p ) { s = "<<NULL XPC DESC>>"; i = 17; break; }
						
						err = PrintFWriteMultiLineText( inContext, &F, p, strlen( p ) );
						free( p );
						if( err < 0 ) goto error;
						total += err;
						continue;
					#else
						s = "<<NO XPC SUPPORT>>";
						i = 18;
						break;
					#endif
				}
				
				// Search extensions.
				
				MinimalMutexEnsureInitialized( gPrintFUtilsLock );
				MinimalMutexLock( gPrintFUtilsLock );
				for( extension = gExtensionList; extension; extension = extension->next )
				{
					if( strnicmpx( extensionPtr, extensionLen, extension->name ) == 0 )
					{
						break;
					}
				}
				if( extension )
				{
					err = extension->handler_f( inContext, &F, &vaArgs, extension->context );
					MinimalMutexUnlock( gPrintFUtilsLock );
					if( err < 0 ) goto error;
					total += err;
					continue;
				}
				MinimalMutexUnlock( gPrintFUtilsLock );
				
				// Unknown extension.
				
				i = SNPrintF( buf, sizeof( buf ), "<<UNKNOWN PRINTF EXTENSION '%.*s'>>", (int) extensionLen, extensionPtr );
				s = buf;
				break;
			}
			
			default:
				i = SNPrintF( buf, sizeof( buf ), "<<UNKNOWN FORMAT CONVERSION CODE %%%c>>", c );
				s = buf;
				break;
		}
		
		// Print the text with the correct padding, etc.
		
		err = PrintFWriteText( inContext, &F, s, (size_t) i );
		if( err < 0 ) goto error;
		total += err;
	}
	
exit:
	return( total );
	
error:
	return( err );
}

//===========================================================================================================================
//	PrintFWriteAddr
//===========================================================================================================================

typedef struct
{
	int32_t		type;
	union
	{
		uint8_t		v4[ 4 ];
		uint8_t		v6[ 16 ];
		
	}	ip;
	
}	mDNSAddrCompat;

static int	PrintFWriteAddr( const uint8_t *inAddr, PrintFFormat *inFormat, char *outStr )
{
	int					n;
	const uint8_t *		a;
	PrintFFormat *		F;
	
	a = inAddr;
	F = inFormat;
	if( ( F->altForm == 1 ) && ( F->precision == 4 ) ) // %#.4a - IPv4 address in host byte order
	{
		#if( TARGET_RT_BIG_ENDIAN )
			n = SNPrintF( outStr, kPrintFBufSize, "%u.%u.%u.%u", a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ] );
		#else
			n = SNPrintF( outStr, kPrintFBufSize, "%u.%u.%u.%u", a[ 3 ], a[ 2 ], a[ 1 ], a[ 0 ] );
		#endif
	}
	else if( ( F->altForm == 1 ) && ( F->precision == 6 ) ) // %#.6a - MAC address from host order uint64_t *.
	{
		#if( TARGET_RT_BIG_ENDIAN )
			n = SNPrintF( outStr, kPrintFBufSize, "%02X:%02X:%02X:%02X:%02X:%02X", 
				a[ 2 ], a[ 3 ], a[ 4 ], a[ 5 ], a[ 6 ], a[ 7 ] );
		#else
			n = SNPrintF( outStr, kPrintFBufSize, "%02X:%02X:%02X:%02X:%02X:%02X", 
				a[ 5 ], a[ 4 ], a[ 3 ], a[ 2 ], a[ 1 ], a[ 0 ] );
		#endif
	}
	else if( F->altForm == 1 ) // %#a: mDNSAddr
	{
		mDNSAddrCompat *		ip;
		
		ip = (mDNSAddrCompat *) inAddr;
		if( ip->type == 4 )
		{
			a = ip->ip.v4;
			n = SNPrintF( outStr, kPrintFBufSize, "%u.%u.%u.%u", a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ] );
		}
		else if( ip->type == 6 )
		{
			IPv6AddressToCString( ip->ip.v6, 0, 0, -1, outStr, 0 );
			n = (int) strlen( outStr );
		}
		else
		{
			n = SNPrintF( outStr, kPrintFBufSize, "%s", "<< ERROR: %#a used with unsupported type: %d >>", ip->type );
		}
	}
	else if( F->altForm == 2 ) // %##a: sockaddr
	{
		#if( defined( AF_INET ) )
			int		family;
			
			family = ( (const struct sockaddr *) inAddr )->sa_family;
			if( family == AF_INET )
			{
				const struct sockaddr_in * const		sa4 = (const struct sockaddr_in *) a;
				
				IPv4AddressToCString( ntoh32( sa4->sin_addr.s_addr ), (int) ntoh16( sa4->sin_port ), outStr );
				n = (int) strlen( outStr );
			}
			#if( defined( AF_INET6 ) )
			else if( family == AF_INET6 )
			{
				const struct sockaddr_in6 * const		sa6 = (const struct sockaddr_in6 *) a;
				
				IPv6AddressToCString( sa6->sin6_addr.s6_addr, sa6->sin6_scope_id, (int) ntoh16( sa6->sin6_port ), -1, outStr, 0 );
				n = (int) strlen( outStr );
			}
			#endif
			#if( defined( AF_LINK ) && defined( LLADDR ) )
			else if( family == AF_LINK )
			{
				const struct sockaddr_dl * const		sdl = (const struct sockaddr_dl *) a;
				
				a = (const uint8_t *) LLADDR( sdl );
				if( sdl->sdl_alen == 6 )
				{
					n = SNPrintF( outStr, kPrintFBufSize, "%02X:%02X:%02X:%02X:%02X:%02X", 
						a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], a[ 5 ] );
				}
				else
				{
					n = SNPrintF( outStr, kPrintFBufSize, "<< AF_LINK %H >>", a, sdl->sdl_alen, sdl->sdl_alen );
				}
			}
			#endif
			else if( family == AF_UNSPEC )
			{
				n = SNPrintF( outStr, kPrintFBufSize, "<< AF_UNSPEC >>" );
			}
			else
			{
				n = SNPrintF( outStr, kPrintFBufSize, "<< ERROR: %%##a used with unknown family: %d >>", family );
			}
		#else
			n = SNPrintF( outStr, kPrintFBufSize, "%s", "<< ERROR: %##a used without socket support >>" );
		#endif
	}
	else
	{
		switch( F->precision )
		{
			case 2:
				n = SNPrintF( outStr, kPrintFBufSize, "%u.%u.%u.%u", a[ 0 ] >> 4, a[ 0 ] & 0xF, a[ 1 ] >> 4, a[ 1 ] & 0xF );
				break;
			
			case 4:
				n = SNPrintF( outStr, kPrintFBufSize, "%u.%u.%u.%u", a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ] );
				break;
			
			case 6:
				n = SNPrintF( outStr, kPrintFBufSize, "%02X:%02X:%02X:%02X:%02X:%02X", 
					a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], a[ 5 ] );
				break;
			
			case 8:
				n = SNPrintF( outStr, kPrintFBufSize, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", 
					a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], a[ 5 ], a[ 6 ], a[ 7 ] );
				break;
			
			case 16:
				IPv6AddressToCString( a, 0, 0, -1, outStr, 0 );
				n = (int) strlen( outStr );
				break;
			
			default:
				n = SNPrintF( outStr, kPrintFBufSize, "%s", 
					"<< ERROR: Must specify address size (i.e. %.4a=IPv4, %.6a=Enet, %.8a=Fibre, %.16a=IPv6) >>" );
				break;
		}
	}
	return( n );
}

//===========================================================================================================================
//	PrintFWriteAudioStreamBasicDescription
//===========================================================================================================================

static int	PrintFWriteAudioStreamBasicDescription( PrintFContext *inContext, const AudioStreamBasicDescription *inASBD )
{
	int					total = 0;
	int					n;
	char				buf[ 32 ];
	const char *		str;
	uint32_t			u32;
	
	if(      inASBD->mFormatID == kAudioFormatMPEG4AAC_ELD )	str = "ELD,";
	else if( inASBD->mFormatID == kAudioFormatMPEG4AAC )		str = "AAC,";
	else if( inASBD->mFormatID == kAudioFormatAppleLossless )	str = "ALAC,";
	else if( inASBD->mFormatID == kAudioFormatLinearPCM )		str = "PCM,";
	else { SNPrintF( buf, sizeof( buf ), "%C,", inASBD->mFormatID ); str = buf; }
	
	n = PrintFCore( inContext, "%-5s %5u Hz", str, (uint32_t) inASBD->mSampleRate );
	require_action_quiet( n >= 0, exit, total = n );
	total += n;
	
	if( inASBD->mBitsPerChannel > 0 )
	{
		n = PrintFCore( inContext, ", %2u-bit", inASBD->mBitsPerChannel );
		require_action_quiet( n >= 0, exit, total = n );
		total += n;
	}
	else if( inASBD->mFormatID == kAudioFormatAppleLossless )
	{
		if(      inASBD->mFormatFlags == kAppleLosslessFormatFlag_16BitSourceData ) str = "16-bit";
		else if( inASBD->mFormatFlags == kAppleLosslessFormatFlag_20BitSourceData ) str = "20-bit";
		else if( inASBD->mFormatFlags == kAppleLosslessFormatFlag_24BitSourceData ) str = "24-bit";
		else if( inASBD->mFormatFlags == kAppleLosslessFormatFlag_32BitSourceData ) str = "32-bit";
		else																		str = "\?\?-bit";
		n = PrintFCore( inContext, ", %s", str );
		require_action_quiet( n >= 0, exit, total = n );
		total += n;
	}
	
	if(      inASBD->mChannelsPerFrame == 1 ) str = "Mono";
	else if( inASBD->mChannelsPerFrame == 2 ) str = "Stereo";
	else { SNPrintF( buf, sizeof( buf ), "%u ch", inASBD->mChannelsPerFrame ); str = buf; }
	n = PrintFCore( inContext, ", %s", str );
	require_action_quiet( n >= 0, exit, total = n );
	total += n;
	
	if( inASBD->mFormatFlags & kAudioFormatFlagIsNonInterleaved )
	{
		n = PrintFCore( inContext, ", Non-interleaved" );
		require_action_quiet( n >= 0, exit, total = n );
		total += n;
	}
	
	if( inASBD->mFormatID == kAudioFormatLinearPCM )
	{
		#if( TARGET_RT_LITTLE_ENDIAN )
		if( inASBD->mFormatFlags & kLinearPCMFormatFlagIsBigEndian )
		#else
		if( !( inASBD->mFormatFlags & kLinearPCMFormatFlagIsBigEndian ) )
		#endif
		{
			n = PrintFCore( inContext, ", Swapped" );
			require_action_quiet( n >= 0, exit, total = n );
			total += n;
		}
		
		if( inASBD->mFormatFlags & kAudioFormatFlagIsFloat )
		{
			n = PrintFCore( inContext, ", Float" );
			require_action_quiet( n >= 0, exit, total = n );
			total += n;
		}
		else if( ( inASBD->mBitsPerChannel > 0 ) && ( inASBD->mFormatFlags & kLinearPCMFormatFlagIsSignedInteger ) && 
				 ( inASBD->mFormatFlags & kLinearPCMFormatFlagsSampleFractionMask ) )
		{
			u32 = ( inASBD->mFormatFlags & kLinearPCMFormatFlagsSampleFractionMask ) >> kLinearPCMFormatFlagsSampleFractionShift;
		
			n = PrintFCore( inContext, ", %u.%u", inASBD->mBitsPerChannel - u32, u32 );
			require_action_quiet( n >= 0, exit, total = n );
			total += n;
		}
	}
	
	if( inASBD->mFramesPerPacket > 1 )
	{
		n = PrintFCore( inContext, ", %u samples/packet", inASBD->mFramesPerPacket );
		require_action_quiet( n >= 0, exit, total = n );
		total += n;
	}
	
exit:
	return( total );	
}

//===========================================================================================================================
//	PrintFWriteBits
//===========================================================================================================================

static int	PrintFWriteBits( uint64_t inX, PrintFFormat *inFormat, char *outStr )
{
#if( TYPE_LONGLONG_NATIVE )
	static const uint64_t			kBit0 = 1;
	uint64_t						x = inX;
#else
	static const unsigned long		kBit0 = 1;
	unsigned long					x = (unsigned long) inX;
#endif
	int								maxBit;
	int								bit;
	char *							dst;
	char *							lim;
	
	dst = outStr;
	lim = dst + kPrintFBufSize;
	if( !inFormat->havePrecision )
	{
		if(      inFormat->hSize == 1 )	inFormat->precision = 8 * sizeof( short );
		else if( inFormat->hSize == 2 )	inFormat->precision = 8 * sizeof( char );
		else if( inFormat->lSize == 1 )	inFormat->precision = 8 * sizeof( long );
		else if( inFormat->lSize == 2 )	inFormat->precision = 8 * sizeof( int64_t );
		else							inFormat->precision = 8 * sizeof( int );
	}
	if( inFormat->precision > ( sizeof( kBit0 ) * 8 ) )
	{
		SNPrintF_Add( &dst, lim, "ERROR: << precision must be 0-%d >>", ( sizeof( kBit0 ) * 8 ) );
	}
	else
	{
		if( inFormat->precision < 1 ) inFormat->precision = 1;
		maxBit = (int)( inFormat->precision - 1 );
		if( inFormat->altForm == 2 )
		{
			for( bit = maxBit; bit >= 0; --bit )
			{
				if( x & ( kBit0 << bit ) )
				{
					SNPrintF_Add( &dst, lim, "%s%d", ( dst != outStr ) ? " " : "", bit );
				}
			}
		}
		else
		{
			for( bit = 0; bit <= maxBit; ++bit )
			{
				if( x & ( kBit0 << ( maxBit - bit ) ) )
				{
					SNPrintF_Add( &dst, lim, "%s%d", ( dst != outStr ) ? " " : "", bit );
				}
			}
		}
	}
	return( (int)( dst - outStr ) );
}

//===========================================================================================================================
//	PrintFWriteCEC
//===========================================================================================================================

#define CECAddressToString( X )	( \
	( (X) == 0 )	? "TV"			: \
	( (X) == 1 )	? "Record 1" 	: \
	( (X) == 2 )	? "Record 2" 	: \
	( (X) == 3 )	? "Tuner 1"		: \
	( (X) == 4 )	? "Player1" 	: \
	( (X) == 5 )	? "Audio"		: \
	( (X) == 6 )	? "Tuner 2"		: \
	( (X) == 7 )	? "Tuner 3"		: \
	( (X) == 8 )	? "Player 2"	: \
	( (X) == 9 )	? "Record 3"	: \
	( (X) == 10 )	? "Tuner 4"		: \
	( (X) == 11 )	? "Player 3"	: \
	( (X) == 12 )	? "Backup 1"	: \
	( (X) == 13 )	? "Backup 2"	: \
	( (X) == 14 )	? "Extra"		: \
	( (X) == 15 )	? "Broadcast"	: \
					  "?" )

static int	PrintFWriteCEC( PrintFContext *ctx, const uint8_t *inData, size_t inLen )
{
	int							total	= 0;
	const uint8_t *				src		= inData;
	const uint8_t * const		end		= src + inLen;
	OSStatus					err;
	uint8_t						srcAddr, dstAddr;
	uint8_t						opcode;
	const char *				label;
	const char *				params = NULL;
	char						labelBuf[ 32 ];
	char						paramsBuf[ 32 ];
	size_t						len;
	
	require_action_quiet( ( end - src ) >= 1, exit, err = kUnderrunErr );
	srcAddr = *src >> 4;
	dstAddr = *src & 0xF;
	src += 1;
	
	if( ( end - src ) >= 1 )
	{
		opcode = *src++;
		switch( opcode )
		{
			case 0x04: // ImageViewOn
				label = "<Image View On>";
				break;
			
			case 0x0D: // TextViewOn
				label = "<Text View On>";
				break;
			
			case 0x82: // ActiveSource
				require_action_quiet( ( end - src ) >= 2, exit, err = kMalformedErr );
				label = "<Active Source>";
				SNPrintF( paramsBuf, sizeof( paramsBuf ), "%.2a", src );
				params = paramsBuf;
				break;
			
			case 0x9D: // InactiveSource
				label = "<Inactive Source>";
				break;
			
			case 0x85: // RequestActiveSource
				label = "<Request Active Source>";
				break;
			
			case 0x80: // RoutingChange
				require_action_quiet( ( end - src ) >= 4, exit, err = kMalformedErr );
				label = "<Routing Change>";
				SNPrintF( paramsBuf, sizeof( paramsBuf ), "%.2a -> %.2a", src, src + 2 );
				params = paramsBuf;
				break;
			
			case 0x8F: // GivePowerStatus
				label = "<Give Power Status>";
				break;
			
			case 0x90: // ReportPowerStatus
				label = "<Report Power Status>";
				break;
			
			case 0x36: // Standby
				label = "<Standby>";
				break;
			
			case 0x9F: // GetCECVersion
				label = "<Get CEC Version>";
				break;
			
			case 0x9E: // CECVersion
				label = "<CEC Version>";
				require_action_quiet( ( end - src ) >= 1, exit, err = kMalformedErr );
				if(      src[ 0 ] == 4 ) params = "1.3a";
				else if( src[ 0 ] == 5 ) params = "1.4";
				else if( src[ 0 ] == 6 ) params = "2.0";
				else
				{
					len = (size_t)( end - src );
					SNPrintF( paramsBuf, sizeof( paramsBuf ), "Other %H", src, (int) len, (int) len );
					params = paramsBuf;
				}
				break;
			
			case 0x83: // GivePhysicalAddress
				label = "<Give Physical Address>";
				break;
			
			case 0x84: // ReportPhysicalAddress
				label = "<Report Physical Address>";
				break;
			
			case 0x8C: // GiveDeviceVendorID
				label = "<Give Device Vendor ID>";
				break;
			
			case 0x87: // DeviceVendorID
				label = "<Device Vendor ID>";
				require_action_quiet( ( end - src ) >= 3, exit, err = kMalformedErr );
				SNPrintF( paramsBuf, sizeof( paramsBuf ), "%02X-%02X-%02X", src[ 0 ], src[ 1 ], src[ 2 ] );
				params = paramsBuf;
				break;
			
			case 0x86: // SetStreamPath
				label = "<Set Stream Path>";
				require_action_quiet( ( end - src ) >= 2, exit, err = kMalformedErr );
				SNPrintF( paramsBuf, sizeof( paramsBuf ), "%.2a", src );
				params = paramsBuf;
				break;
			
			case 0x46: // GetOSDName
				label = "<Get OSD Name>";
				break;
			
			case 0x47: // SetOSDName
				label = "<Set OSD Name>";
				SNPrintF( paramsBuf, sizeof( paramsBuf ), "'%.*s'", (int)( end - src ), (const char *) src );
				params = paramsBuf;
				break;
			
			case 0x32: // SetMenuLanguage
				label = "<Set Menu Language>";
				SNPrintF( paramsBuf, sizeof( paramsBuf ), "'%.*s'", (int)( end - src ), (const char *) src );
				params = paramsBuf;
				break;
			
			case 0x8D: // MenuRequest
				label = "<Menu Request>";
				break;
			
			case 0x8E: // MenuStatus
				label = "<Menu Status>";
				break;
			
			case 0x44: // UserControlPressed
				label = "<User Control Pressed>";
				break;
			
			case 0x45: // UserControlReleased
				label = "<User Control Released>";
				break;
			
			case 0x00: // FeatureAbort
				label = "<Feature Abort>";
				break;
			
			case 0x1A: // GiveDeckStatus
				label = "<Give Deck Status>";
				break;
			
			case 0xA0: // VendorCommandWithID
				label = "<Vendor Command with ID>";
				break;
			
			default:
				SNPrintF( labelBuf, sizeof( labelBuf ), "<<? 0x%02X>>", opcode );
				label = labelBuf;
				break;
		}
	}
	else
	{
		label  = "<Poll>";
		params = "";
	}
	if( !params )
	{
		len = (size_t)( end - src );
		SNPrintF( paramsBuf, sizeof( paramsBuf ), "%H", src, (int) len, (int) len );
		params = paramsBuf;
	}
	err = PrintFCore( ctx, "%-9s -> %9s: %s %s", CECAddressToString( srcAddr ), CECAddressToString( dstAddr ), label, params );
	require_quiet( err >= 0, exit );
	total += err;
	err = kNoErr;
	
exit:
	if( err ) total = PrintFCore( ctx, "<< MALFORMED CEC: %H >>", inData, (int) inLen, 64 );
	return( total );
}

#if( DEBUG_CF_OBJECTS_ENABLED )

//===========================================================================================================================
//	PrintFWriteCFObject
//===========================================================================================================================

typedef struct
{
	PrintFContext *		context;
	PrintFFormat *		format;
	int					indent;
	int					total; // Note: temporary total for a recursive operation.
	OSStatus			error;
	
}	PrintFWriteCFObjectContext;

static int	PrintFWriteCFObjectLevel( PrintFWriteCFObjectContext *inContext, CFTypeRef inObj, Boolean inPrintingArray );
static void	PrintFWriteCFObjectApplier( const void *inKey, const void *inValue, void *inContext );

static int	PrintFWriteCFObject( PrintFContext *inContext, PrintFFormat *inFormat, CFTypeRef inObj, char *inBuffer )
{
	int					total;
	CFTypeID			typeID;
	const char *		s;
	int					n;
	
	typeID = CFGetTypeID( inObj );
	
	// Boolean
	
	if( typeID == CFBooleanGetTypeID() )
	{
		if(      ( (CFBooleanRef) inObj ) == kCFBooleanTrue )	{ s = "true";   n = 4; }
		else													{ s = "false";  n = 5; }
		total = PrintFWriteText( inContext, inFormat, s, (size_t) n );
	}
	
	// Number
	
	else if( typeID == CFNumberGetTypeID() )
	{
		#if( !CFLITE_ENABLED || CFL_FLOATING_POINT_NUMBERS )
			if( CFNumberIsFloatType( (CFNumberRef) inObj ) )
			{
				double		dval = 0;
				
				CFNumberGetValue( (CFNumberRef) inObj, kCFNumberDoubleType, &dval );
				n = SNPrintF( inBuffer, kPrintFBufSize, "%f", dval );
			}
			else
		#endif
			{
				int64_t		s64 = 0;
				
				CFNumberGetValue( (CFNumberRef) inObj, kCFNumberSInt64Type, &s64 );
				n = SNPrintF( inBuffer, kPrintFBufSize, "%lld", s64 );
			}
		total = PrintFWriteText( inContext, inFormat, inBuffer, (size_t) n );
	}
	
	// String
	
	else if( typeID == CFStringGetTypeID() )
	{
		CFStringRef const		cfStr = (CFStringRef) inObj;
		CFIndex					cfLen;
		size_t					size;
		
		cfLen = CFStringGetLength( cfStr );
		size = (size_t) CFStringGetMaximumSizeForEncoding( cfLen, kCFStringEncodingUTF8 );
		if( size > 0 )
		{
			char *			cStr;
			CFRange			range;
			CFIndex			i;
			
			cStr = (char *) malloc( size );
			require_action_quiet( cStr, exit, total = kNoMemoryErr );
			
			i = 0;
			range = CFRangeMake( 0, cfLen );
			CFStringGetBytes( cfStr, range, kCFStringEncodingUTF8, '^', false, (UInt8 *) cStr, (CFIndex) size, &i );
			
			// Restrict the string length to the precision, but don't truncate in the middle of a UTF-8 character.
			
			if( inFormat->havePrecision && ( i > (CFIndex) inFormat->precision ) )
			{
				for( i = (int) inFormat->precision; ( i > 0 ) && ( ( cStr[ i ] & 0xC0 ) == 0x80 ); --i ) {}
			}
			
			total = PrintFWriteText( inContext, inFormat, cStr, (size_t) i );
			free( cStr );
		}
		else
		{
			// Note: this is needed because there may be field widths, etc. to fill.
			
			total = PrintFWriteText( inContext, inFormat, "", 0 );
		}
	}
	
	// Null
	
	else if( typeID == CFNullGetTypeID() )
	{
		total = PrintFWriteText( inContext, inFormat, "Null", 4 );
	}
	
#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	// URL
	
	else if( typeID == CFURLGetTypeID() )
	{
		CFStringRef		cfStr;
		
		cfStr = CFURLGetString( (CFURLRef) inObj );
		require_action_quiet( cfStr, exit, total = kUnknownErr );
		total = PrintFWriteCFObject( inContext, inFormat, cfStr, inBuffer );
	}
	
	// UUID
	
	else if( typeID == CFUUIDGetTypeID() )
	{
		CFUUIDBytes		bytes;
		
		bytes = CFUUIDGetUUIDBytes( (CFUUIDRef) inObj );
		n = SNPrintF( inBuffer, kPrintFBufSize, "%#U", &bytes );
		total = PrintFWriteText( inContext, inFormat, inBuffer, (size_t) n );
	}
#endif
	
	// Other
	
	else
	{
		PrintFWriteCFObjectContext		cfContext;
		
		cfContext.context	= inContext;
		cfContext.format	= inFormat;
		cfContext.indent	= (int) inFormat->fieldWidth;
		cfContext.error		= kNoErr;
		
		total = PrintFWriteCFObjectLevel( &cfContext, inObj, false );
		require_quiet( total >= 0, exit );
		
		if( ( typeID == CFArrayGetTypeID() ) || ( typeID == CFDictionaryGetTypeID() ) )
		{
			n = inContext->callback( "\n", 1, inContext );
			require_action_quiet( n > 0, exit, total = n );
			total += n;
		}
	}
	
exit:
	return( total );
}

//===========================================================================================================================
//	PrintFWriteCFObjectLevel
//===========================================================================================================================

static int	PrintFWriteCFObjectLevel( PrintFWriteCFObjectContext *inContext, CFTypeRef inObj, Boolean inPrintingArray )
{
	int				total = 0;
	OSStatus		err;
	CFTypeID		typeID;
	CFIndex			i, n;
	CFTypeRef		obj;
	size_t			size;
	char			buf[ 4 ];
	
	typeID = CFGetTypeID( inObj );
	
	// Array
	
	if( typeID == CFArrayGetTypeID() )
	{
		err = print_indent( inContext->context, inContext->indent );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		n = CFArrayGetCount( (CFArrayRef) inObj );
		if( n > 0 )
		{
			err = inContext->context->callback( "[\n", 2, inContext->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 2;
			
			for( i = 0; i < n; )
			{
				obj = CFArrayGetValueAtIndex( (CFArrayRef) inObj, i );
				
				++inContext->indent;
				err = PrintFWriteCFObjectLevel( inContext, obj, true );
				--inContext->indent;
				require_action_quiet( err >= 0, exit, total = err );
				total += err;
				
				++i;
				size = 0;
				if( i < n ) buf[ size++ ] = ',';
				buf[ size++ ] = '\n';
				err = inContext->context->callback( buf, size, inContext->context );
				require_action_quiet( err >= 0, exit, total = err );
				total += 1;
			}
			
			err = print_indent( inContext->context, inContext->indent );
			require_action_quiet( err >= 0, exit, total = err );
			total += err;
			
			err = inContext->context->callback( "]", 1, inContext->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 1;
		}
		else
		{
			err = inContext->context->callback( "[]", 2, inContext->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 2;
		}
	}
	
	// Boolean
	
	else if( typeID == CFBooleanGetTypeID() )
	{
		const char *		boolStr;
		
		err = print_indent( inContext->context, inContext->indent );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		if( ( (CFBooleanRef) inObj ) == kCFBooleanTrue )	{ boolStr = "true";  size = 4; }
		else												{ boolStr = "false"; size = 5; }
		
		err = inContext->context->callback( boolStr, size, inContext->context );
		require_action_quiet( err >= 0, exit, total = err );
		total += (int) size;
	}
	
	// Data
	
	else if( typeID == CFDataGetTypeID() )
	{
		int		oldIndent;
		
		oldIndent = inContext->indent;
		size = (size_t) CFDataGetLength( (CFDataRef) inObj );
		if( ( size <= 16 ) && !inPrintingArray )
		{
			inContext->indent = 0;
		}
		else
		{
			err = inContext->context->callback( "\n", 1, inContext->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 1;
			
			inContext->indent = oldIndent + 1;
		}
		
		err = PrintFWriteHex( inContext->context, inContext->format, inContext->indent, 
			CFDataGetBytePtr( (CFDataRef) inObj ), size, 
			inContext->format->havePrecision ? inContext->format->precision : size );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		inContext->indent = oldIndent;
	}
	
	// Date
	
	else if( typeID == CFDateGetTypeID() )
	{
		int		year, month, day, hour, minute, second, micros;
		
		err = print_indent( inContext->context, inContext->indent );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		CFDateGetComponents( (CFDateRef) inObj, &year, &month, &day, &hour, &minute, &second, &micros );
		err = PrintFCore( inContext->context, "%04d-%02d-%02d %02d:%02d:%02d.%06d", 
			year, month, day, hour, minute, second, micros );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
	}
	
	// Dictionary
	
	else if( typeID == CFDictionaryGetTypeID() )
	{
		err = print_indent( inContext->context, inContext->indent );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		if( CFDictionaryGetCount( (CFDictionaryRef) inObj ) > 0 )
		{
			err = inContext->context->callback( "{\n", 2, inContext->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 2;
			
			++inContext->indent;
			
			inContext->total = total;
			CFDictionaryApplyFunction( (CFDictionaryRef) inObj, PrintFWriteCFObjectApplier, inContext );
			require_action_quiet( inContext->error >= 0, exit, total = inContext->error );
			total = inContext->total;
			
			--inContext->indent;
			
			err = print_indent( inContext->context, inContext->indent );
			require_action_quiet( err >= 0, exit, total = err );
			total += err;
			
			err = inContext->context->callback( "}", 1, inContext->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 1;
		}
		else
		{
			err = inContext->context->callback( "{}", 2, inContext->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 2;
		}
	}
	
	// Number
	
	else if( typeID == CFNumberGetTypeID() )
	{
		err = print_indent( inContext->context, inContext->indent );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		#if( !CFLITE_ENABLED || CFL_FLOATING_POINT_NUMBERS )
			if( CFNumberIsFloatType( (CFNumberRef) inObj ) )
			{
				double		dval = 0;
				
				CFNumberGetValue( (CFNumberRef) inObj, kCFNumberDoubleType, &dval );
				err = PrintFCore( inContext->context, "%f", dval );
			}
			else
		#endif
			{
				int64_t		s64 = 0;
				
				CFNumberGetValue( (CFNumberRef) inObj, kCFNumberSInt64Type, &s64 );
				err = PrintFCore( inContext->context, "%lld", s64 );
			}
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
	}
	
	// String
	
	else if( typeID == CFStringGetTypeID() )
	{
		CFStringRef const		cfStr = (CFStringRef) inObj;
		
		err = print_indent( inContext->context, inContext->indent );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		err = inContext->context->callback( "\"", 1, inContext->context );
		require_action_quiet( err >= 0, exit, total = err );
		total += 1;
		
		n = CFStringGetLength( cfStr );
		size = (size_t) CFStringGetMaximumSizeForEncoding( n, kCFStringEncodingUTF8 );
		if( size > 0 )
		{
			char *			cStr;
			CFRange			range;
			CFIndex			converted;
			
			cStr = (char *) malloc( size );
			require_action_quiet( cStr, exit, total = kNoMemoryErr );
			
			converted = 0;
			range = CFRangeMake( 0, n );
			CFStringGetBytes( cfStr, range, kCFStringEncodingUTF8, '^', false, (UInt8 *) cStr, (CFIndex) size, &converted );
			
			err = inContext->context->callback( cStr, (size_t) converted, inContext->context );
			free( cStr );
			require_action_quiet( err >= 0, exit, total = err );
			total += (int) converted;
		}
		
		err = inContext->context->callback( "\"", 1, inContext->context );
		require_action_quiet( err >= 0, exit, total = err );
		total += 1;
	}
	
	// Null
	
	else if( typeID == CFNullGetTypeID() )
	{
		err = print_indent( inContext->context, inContext->indent );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		err = inContext->context->callback( "Null", 4, inContext->context );
		require_action_quiet( err >= 0, exit, total = err );
		total += 4;
	}
	
#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	// URL
	
	else if( typeID == CFURLGetTypeID() )
	{
		CFStringRef		cfStr;
		
		cfStr = CFURLGetString( (CFURLRef) inObj );
		require_action_quiet( cfStr, exit, total = kUnknownErr );
		err = PrintFWriteCFObjectLevel( inContext, cfStr, inPrintingArray );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
	}
	
	// UUID
	
	else if( typeID == CFUUIDGetTypeID() )
	{
		CFUUIDBytes		bytes;
		
		err = print_indent( inContext->context, inContext->indent );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		bytes = CFUUIDGetUUIDBytes( (CFUUIDRef) inObj );
		err = PrintFCore( inContext->context, "%#U", &bytes );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
	}
#endif
	
	// Unknown
	
	else
	{
		err = print_indent( inContext->context, inContext->indent );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
			
		#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
		{
			CFStringRef		desc;
			
			desc = CFCopyDescription( inObj );
			if( desc )
			{
				err = PrintFCore( inContext->context, "%@", desc );
				CFRelease( desc );
				require_action_quiet( err >= 0, exit, total = err );
				total += err;
				goto exit;
			}
		}
		#endif
		
		err = PrintFCore( inContext->context, "<<UNKNOWN CF OBJECT TYPE: %d>>", (int) typeID );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
	}
	
exit:
	return( total );
}

//===========================================================================================================================
//	PrintFWriteCFObjectApplier
//===========================================================================================================================

static void	PrintFWriteCFObjectApplier( const void *inKey, const void *inValue, void *inContext )
{
	int										total;
	PrintFWriteCFObjectContext * const		context = (PrintFWriteCFObjectContext *) inContext;
	CFTypeRef const							value	= (CFTypeRef) inValue;
	OSStatus								err;
	CFTypeID								typeID;
	 
	if( context->error ) return;
	
	// Print the key.
	
	err = PrintFWriteCFObjectLevel( context, (CFTypeRef) inKey, false );
	require_action_quiet( err >= 0, exit, total = err );
	total = err;
	
	err = context->context->callback( " : ", 3, context->context );
	require_action_quiet( err >= 0, exit, total = err );
	total += 3;
	
	// Print the value based on its type.
	
	typeID = CFGetTypeID( value );
	if( typeID == CFArrayGetTypeID() )
	{
		if( CFArrayGetCount( (CFArrayRef) inValue ) > 0 )
		{
			err = context->context->callback( "\n", 1, context->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 1;
			
			err = PrintFWriteCFObjectLevel( context, value, true );
			require_action_quiet( err >= 0, exit, total = err );
			total += err;
			
			err = context->context->callback( "\n", 1, context->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 1;
		}
		else
		{
			err = context->context->callback( "[]\n", 3, context->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 3;
		}
	}
	else if( typeID == CFDictionaryGetTypeID() )
	{
		if( CFDictionaryGetCount( (CFDictionaryRef) inValue ) > 0 )
		{
			err = context->context->callback( "\n", 1, context->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 1;
			
			err = PrintFWriteCFObjectLevel( context, value, false );
			require_action_quiet( err >= 0, exit, total = err );
			total += err;
			
			err = context->context->callback( "\n", 1, context->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 1;
		}
		else
		{
			err = context->context->callback( "{}\n", 3, context->context );
			require_action_quiet( err >= 0, exit, total = err );
			total += 3;
		}
	}
	else if( ( typeID == CFDataGetTypeID() ) && ( context->format->altForm != 2 ) )
	{
		err = PrintFWriteCFObjectLevel( context, value, false );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
	}
	else
	{
		int		oldIndent;
		
		oldIndent = context->indent;
		context->indent = 0;
		
		err = PrintFWriteCFObjectLevel( context, value, false );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		context->indent = oldIndent;
		
		err = context->context->callback( "\n", 1, context->context );
		require_action_quiet( err >= 0, exit, total = err );
		total += 1;
	}
	
exit:
	context->total += total;
	if( err < 0 ) context->error = err;
}

//===========================================================================================================================
//	PrintFWriteCFXMLObject
//===========================================================================================================================

#if( !CFLITE_ENABLED || CFL_XML )
static int	PrintFWriteCFXMLObject( PrintFContext *inContext, PrintFFormat *inFormat, CFTypeRef inObj )
{
	int				total = 0, err;
	CFDataRef		xmlData;
	const char *	xmlPtr;
	size_t			xmlLen;
	
	xmlData = CFPropertyListCreateData( NULL, inObj, kCFPropertyListXMLFormat_v1_0, 0, NULL );
	if( !xmlData )
	{
		err = PrintFCore( inContext, "<<PLIST NOT XML-ABLE>>" );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		goto exit;
	}
	xmlPtr = (const char *) CFDataGetBytePtr( xmlData );
	xmlLen = (size_t) CFDataGetLength( xmlData );
	
	err = PrintFWriteMultiLineText( inContext, inFormat, xmlPtr, xmlLen );
	CFRelease( xmlData );
	require_action_quiet( err >= 0, exit, total = err );
	total += err;
	
exit:
	return( total );
}
#endif // !CFLITE_ENABLED || CFL_XML
#endif // DEBUG_CF_OBJECTS_ENABLED

//===========================================================================================================================
//	PrintFWriteFill
//===========================================================================================================================

static int	PrintFWriteFill( PrintFContext *inContext, int inC, size_t inCount, char *inBuf )
{
	int			total = 0, n;
	size_t		len;
	
	while( inCount > 0 )
	{
		len = Min( inCount, kPrintFBufSize );
		memset( inBuf, inC, len );
		inCount -= len;
		
		n = inContext->callback( inBuf, len, inContext );
		require_action_quiet( n >= 0, exit, total = n );
		total += n;
	}
	
exit:
	return( total );
}

//===========================================================================================================================
//	PrintFWriteFlags
//===========================================================================================================================

static int	PrintFWriteFlags( PrintFContext *inContext, PrintFFormat *inFormat, const char *inDescriptors, uint64_t inX )
{
	int					total = 0, n;
	uint64_t			mask;
	uint8_t				i, bit;
	const char *		descPtr;
	size_t				len;
	
	if( inFormat->altForm )
	{
		n = PrintFCore( inContext, "0x%llX ", inX );
		require_action_quiet( n >= 0, exit, total = n );
		total += n;
	}
	
	n = PrintFCore( inContext, "<" );
	require_action_quiet( n >= 0, exit, total = n );
	total += n;
	
	for( i = 0; ( i < 64 ) && inX; ++i )
	{
		mask = UINT64_C( 1 ) << i;
		if( !( inX & mask ) ) continue;
		inX &= ~mask;
		
		for( descPtr = inDescriptors; ; descPtr += ( len + 1 ) )
		{
			bit = (uint8_t)( *descPtr++ );
			len = strlen( descPtr );
			if( len == 0 ) break;
			if( bit != i ) continue;
			
			n = PrintFCore( inContext, " %s", descPtr );
			require_action_quiet( n >= 0, exit, total = n );
			total += n;
			break;
		}
	}
	
	n = PrintFCore( inContext, " >" );
	require_action_quiet( n >= 0, exit, total = n );
	total += n;
	
exit:
	return( total );
}

//===========================================================================================================================
//	PrintFWriteHex
//===========================================================================================================================

static int
	PrintFWriteHex( 
		PrintFContext *	inContext, 
		PrintFFormat *	inFormat, 
		int				inIndent, 
		const void *	inData, 
		size_t			inSize, 
		size_t			inMaxSize )
{
	int					total = 0;
	int					err;
	const uint8_t *		start;
	const uint8_t *		ptr;
	size_t				size;
	uint8_t				hex1[ 64 ];
	uint8_t				hex2[ 64 ];
	uint8_t *			currHexPtr;
	uint8_t *			prevHexPtr;
	uint8_t *			tempHexPtr;
	int					dupCount;
	size_t				dupSize;
	
	currHexPtr	= hex1;
	prevHexPtr	= hex2;
	dupCount	= 0;
	dupSize		= 0;
	start		= (const uint8_t *) inData;
	ptr			= start;
	size		= ( inSize > inMaxSize ) ? inMaxSize : inSize;
	
	for( ;; )
	{
		size_t			chunkSize;
		uint8_t			ascii[ 64 ];
		uint8_t *		s;
		uint8_t			c;
		size_t			i;
		
		// Build a hex string (space every 4 bytes) and pad with space to fill the full 16-byte range.
		
		chunkSize = Min( size, 16 );
		s = currHexPtr;
		for( i = 0; i < 16; ++i )
		{
			if( ( i > 0 ) && ( ( i % 4 ) == 0 ) ) *s++ = ' ';
			if( i < chunkSize )
			{
				*s++ = (uint8_t) kHexDigitsLowercase[ ptr[ i ] >> 4   ];
				*s++ = (uint8_t) kHexDigitsLowercase[ ptr[ i ] &  0xF ];
			}
			else
			{
				*s++ = ' ';
				*s++ = ' ';
			}
		}
		*s++ = '\0';
		check( ( (size_t)( s - currHexPtr ) ) < sizeof( hex1 ) );
		
		// Build a string with the ASCII version of the data (replaces non-printable characters with '^').
		// Pads the string with spaces to fill the full 16 byte range (so it lines up).
		
		s = ascii;
		for( i = 0; i < 16; ++i )
		{
			if( i < chunkSize )
			{
				c = ptr[ i ];
				if( !PrintFIsPrintable( c ) )
				{
					c = '^';
				}
			}
			else
			{
				c = ' ';
			}
			*s++ = c;
		}
		*s++ = '\0';
		check( ( (size_t)( s - ascii ) ) < sizeof( ascii ) );
		
		// Print the data.
		
		if( inSize <= 16 )
		{
			err = print_indent( inContext, inIndent );
			require_action_quiet( err >= 0, exit, total = err );
			total += err;
		
			err = PrintFCore( inContext, "%s |%s| (%zu bytes)\n", currHexPtr, ascii, inSize );
			require_action_quiet( err >= 0, exit, total = err );
			total += err;
		}
		else if( ptr == start )
		{
			err = print_indent( inContext, inIndent );
			require_action_quiet( err >= 0, exit, total = err );
			total += err;
		
			err = PrintFCore( inContext, "+%04X: %s |%s| (%zu bytes)\n", (int)( ptr - start ), currHexPtr, ascii, inSize );
			require_action_quiet( err >= 0, exit, total = err );
			total += err;
		}
		else if( ( inFormat->group > 0 ) && ( memcmp( currHexPtr, prevHexPtr, 32 ) == 0 ) )
		{
			dupCount += 1;
			dupSize  += chunkSize;
		}
		else
		{
			if( dupCount > 0 )
			{
				err = print_indent( inContext, inIndent );
				require_action_quiet( err >= 0, exit, total = err );
				total += err;
				
				err = PrintFCore( inContext, "* (%zu more identical bytes, %zu total)\n", dupSize, dupSize + 16 );
				require_action_quiet( err >= 0, exit, total = err );
				total += err;
				
				dupCount = 0;
				dupSize  = 0;
			}
			
			err = print_indent( inContext, inIndent );
			require_action_quiet( err >= 0, exit, total = err );
			total += err;
				
			err = PrintFCore( inContext, "+%04X: %s |%s|\n", (int)( ptr - start ), currHexPtr, ascii );
			require_action_quiet( err >= 0, exit, total = err );
			total += err;
		}
		
		tempHexPtr = prevHexPtr;
		prevHexPtr = currHexPtr;
		currHexPtr = tempHexPtr;
		
		ptr  += chunkSize;
		size -= chunkSize;
		if( size <= 0 ) break;
	}
	
	if( dupCount > 0 )
	{
		err = print_indent( inContext, inIndent );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		err = PrintFCore( inContext, "* (%zu more identical bytes, %zu total)\n", dupSize, dupSize + 16 );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
	}
	if( inSize > inMaxSize )
	{
		err = print_indent( inContext, inIndent );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
		
		err = PrintFCore( inContext, "... %zu more bytes ...\n", inSize - inMaxSize );
		require_action_quiet( err >= 0, exit, total = err );
		total += err;
	}
	
exit:
	return( total );
}

//===========================================================================================================================
//	PrintFWriteHexOneLine
//===========================================================================================================================

static int	PrintFWriteHexOneLine( PrintFContext *inContext, PrintFFormat *inFormat, const uint8_t *inData, size_t inSize )
{
	int			total = 0;
	int			err;
	size_t		i;
	size_t		j;
	uint8_t		b;
	char		hex[ 3 ];
	char		c;
	
	require_quiet( inSize > 0, exit );
	
	// Print each byte as hex.
	
	if( inFormat->altForm != 2 )
	{
		for( i = 0; i < inSize; ++i )
		{
			j = 0;
			if( i != 0 ) hex[ j++ ] = ' ';
			b = inData[ i ];
			hex[ j++ ] = kHexDigitsLowercase[ ( b >> 4 ) & 0x0F ];
			hex[ j++ ] = kHexDigitsLowercase[   b        & 0x0F ];
			err = inContext->callback( hex, j, inContext );
			require_action_quiet( err >= 0, exit, total = err );
			total += (int) j;
		}
	}
	
	// Print each byte as ASCII if requested.
	
	if( inFormat->altForm > 0 )
	{
		if( total > 0 )
		{
			err = inContext->callback( " |", 2, inContext );
			require_action_quiet( err >= 0, exit, total = err );
			total += 2;
		}
		else
		{
			err = inContext->callback(  "|", 1, inContext );
			require_action_quiet( err >= 0, exit, total = err );
			total += 1;
		}
		for( i = 0; i < inSize; ++i )
		{
			c = (char) inData[ i ];
			if( ( c < 0x20 ) || ( c >= 0x7F ) ) c = '^';
			
			err = inContext->callback( &c, 1, inContext );
			require_action_quiet( err >= 0, exit, total = err );
			total += 1;
		}
		
		err = inContext->callback( "|", 1, inContext );
		require_action_quiet( err >= 0, exit, total = err );
		total += 1;
	}
	
exit:
	return( total );
}

//===========================================================================================================================
//	PrintFWriteHexByteStream
//===========================================================================================================================

static int	PrintFWriteHexByteStream( PrintFContext *inContext, Boolean inUppercase, const uint8_t *inData, size_t inSize )
{
	const char * const		digits = inUppercase ? kHexDigitsUppercase : kHexDigitsLowercase;
	int						total = 0;
	int						err;
	const uint8_t *			src;
	const uint8_t *			end;
	char					buf[ 64 ];
	char *					dst;
	char *					lim;
	size_t					len;
	
	src = inData;
	end = src + inSize;
	dst = buf;
	lim = dst + sizeof( buf );
	
	while( src < end )
	{
		uint8_t		b;
		
		if( dst == lim )
		{
			len = (size_t)( dst - buf );
			err = inContext->callback( buf, len, inContext );
			require_action_quiet( err >= 0, exit, total = err );
			total += (int) len;
			
			dst = buf;
		}
		
		b = *src++;
		*dst++ = digits[ ( b >> 4 ) & 0x0F ];
		*dst++ = digits[   b        & 0x0F ];
	}
	if( dst != buf )
	{
		len = (size_t)( dst - buf );
		err = inContext->callback( buf, len, inContext );
		require_action_quiet( err >= 0, exit, total = err );
		total += (int) len;
	}
	
exit:
	return( total );
}

//===========================================================================================================================
//	PrintFWriteMultiLineText
//===========================================================================================================================

static int	PrintFWriteMultiLineText( PrintFContext *inContext, PrintFFormat *inFormat, const char *inStr, size_t inLen )
{
	int					total = 0, err;
	const char *		line;
	const char *		end;
	const char *		eol;
	const char *		next;
	unsigned int		i, n;
	size_t				len;
	
	for( line = inStr, end = line + inLen; line < end; line = next )
	{
		for( eol = line; ( eol < end ) && ( *eol != '\r' ) && ( *eol != '\n' ); ++eol ) {}
		if( eol < end )
		{
			if( ( eol[ 0 ] == '\r' ) && ( ( ( eol + 1 ) < end ) && ( eol[ 1 ] == '\n' ) ) )
			{
				next = eol + 2;
			}
			else
			{
				next = eol + 1;
			}
		}
		else
		{
			next = eol;
		}
		if( ( line < eol ) && ( *line != '\r' ) && ( *line != '\n' ) )
		{
			n = inFormat->fieldWidth;
			for( i = 0; i < n; ++i )
			{
				err = inContext->callback( "    ", 4, inContext );
				require_action_quiet( err >= 0, exit, total = err );
				total += 4;
			}
		}
		if( line < eol )
		{
			len = (size_t)( eol - line );
			err = inContext->callback( line, len, inContext );
			require_action_quiet( err >= 0, exit, total = err );
			total += (int) len;
		}
		if( eol < end )
		{
			err = inContext->callback( "\n", 1, inContext );
			require_action_quiet( err >= 0, exit, total = err );
			total += 1;
		}
	}
	
exit:
	return( total );
}

//===========================================================================================================================
//	PrintFWriteNumVersion
//===========================================================================================================================

static int	PrintFWriteNumVersion( uint32_t inVersion, char *outStr )
{
	char *		dst;
	char *		lim;
	uint8_t		majorRev;
	uint8_t		minor;
	uint8_t		bugFix;
	uint8_t		stage;
	uint8_t		revision;
		
	majorRev 	= (uint8_t)( ( inVersion >> 24 ) & 0xFF );
	minor		= (uint8_t)( ( inVersion >> 20 ) & 0x0F );
	bugFix		= (uint8_t)( ( inVersion >> 16 ) & 0x0F );
	stage 		= (uint8_t)( ( inVersion >>  8 ) & 0xFF );
	revision 	= (uint8_t)(   inVersion         & 0xFF );
	
	// Convert the major, minor, and bugfix numbers. Bugfix only added if it is non-zero (i.e. print 6.2 and not 6.2.0).
	
	dst  = outStr;
	lim  = dst + kPrintFBufSize;
	SNPrintF_Add( &dst, lim, "%u", majorRev );
	SNPrintF_Add( &dst, lim, ".%u", minor );
	if( bugFix != 0 ) SNPrintF_Add( &dst, lim, ".%u", bugFix );
	
	// Convert the version stage and non-release revision number.
	
	switch( stage )
	{
		case kVersionStageDevelopment:	SNPrintF_Add( &dst, lim, "d%u", revision ); break;
		case kVersionStageAlpha:		SNPrintF_Add( &dst, lim, "a%u", revision ); break;
		case kVersionStageBeta:			SNPrintF_Add( &dst, lim, "b%u", revision ); break;
		case kVersionStageFinal:
			
			// A non-release revision of zero is a special case indicating the software is GM (at the golden master 
			// stage) and therefore, the non-release revision should not be added to the string.
			
			if( revision != 0 ) SNPrintF_Add( &dst, lim, "f%u", revision );
			break;
		
		default:
			SNPrintF_Add( &dst, lim, "<< ERROR: invalid NumVersion stage: 0x%02X >>", revision );
			break;
	}
	return( (int)( dst - outStr ) );
}

//===========================================================================================================================
//	PrintFWriteObfuscatedPtr
//===========================================================================================================================

static int	PrintFWriteObfuscatedPtr( PrintFContext *inContext, const void *inPtr )
{
	static dispatch_once_t		sOnce = 0;
	static uint8_t				sKey[ 16 ];
	int							n;
	uint64_t					hash;
	
	dispatch_once_f( &sOnce, sKey, _PrintFWriteObfuscatedPtrInit );
	hash = inPtr ? SipHash( sKey, &inPtr, sizeof( inPtr ) ) : 0;
	n = PrintFCore( inContext, "0x%04X", (uint16_t)( hash & 0xFFFF ) );
	return( ( n >= 0 ) ? n : 0 );
}

//===========================================================================================================================
//	_PrintFWriteObfuscatedPtrInit
//===========================================================================================================================

static void	_PrintFWriteObfuscatedPtrInit( void *inArg )
{
	uint8_t * const		key = (uint8_t *) inArg;
	OSStatus			err;
	
	DEBUG_USE_ONLY( err );
	
	err = RandomBytes( key, 16 );
	check_noerr( err );
}

//===========================================================================================================================
//	PrintFWriteSingleLineText
//===========================================================================================================================

static int	PrintFWriteSingleLineText( PrintFContext *inContext, const char *inStr, size_t inLen )
{
	int					total = 0, err;
	const char *		src;
	const char *		end;
	const char *		ptr;
	size_t				len;
	
	src = inStr;
	end = inStr + inLen;
	while( src < end )
	{
		for( ptr = src; ( src < end ) && ( *src != '\r' ) && ( *src != '\n' ); ++src ) {}
		if( ptr < src )
		{
			len = (size_t)( src - ptr );
			err = inContext->callback( ptr, len, inContext );
			require_action_quiet( err >= 0, exit, total = err );
			total += (int) len;
		}
		
		for( ptr = src; ( src < end ) && ( ( *src == '\r' ) || ( *src == '\n' ) ); ++src ) {}
		if( ( ptr < src ) && ( src < end ) )
		{
			len = sizeof_string( " ⏎ " );
			err = inContext->callback( " ⏎ ", len, inContext );
			require_action_quiet( err >= 0, exit, total = err );
			total += (int) len;
		}
	}
	
exit:
	return( total );
}

//===========================================================================================================================
//	PrintFWriteString
//===========================================================================================================================

static int	PrintFWriteString( const char *inStr, PrintFFormat *inFormat, char *inBuf, const char **outStrPtr )
{
	int					i;
	const char *		s;
	PrintFFormat *		F;
	int					c;
	
	s = inStr;
	F = inFormat;
	if( F->altForm == 0 )		// %s: C-string
	{
		i = 0;
		if( F->havePrecision )
		{
			int		j;
			
			while( ( i < (int) F->precision ) && ( s[ i ] != '\0' ) ) ++i;
			
			// Make sure we don't truncate in the middle of a UTF-8 character.
			// If the last character is part of a multi-byte UTF-8 character, back up to the start of it.
			// If the actual count of UTF-8 characters matches the encoded UTF-8 count, add it back.
			
			c = 0;
			j = 0;
			while( ( i > 0 ) && ( ( c = s[ i - 1 ] ) & 0x80 ) ) { ++j; --i; if( ( c & 0xC0 ) != 0x80 ) break; }
			if( ( j > 1 ) && ( j <= 6 ) )
			{
				int		test;
				int		mask;
				
				test = ( 0xFF << ( 8 - j ) ) & 0xFF;
				mask = test | ( 1 << ( ( 8 - j ) - 1 ) );
				if( ( c & mask ) == test ) i += j;
			}
		}
		else
		{
			while( s[ i ] != '\0' ) ++i;
		}
	}
	else if( F->altForm == 1 )	// %#s: Pascal-string
	{
		i = *s++;
	}
	else if( F->altForm == 2 )	// %##s: DNS label-sequence name
	{
		const uint8_t *		a;
		char *				dst;
		char *				lim;
		
		a = (const uint8_t *) s;
		dst = inBuf;
		lim = dst + kPrintFBufSize;
		if( *a == 0 ) *dst++ = '.';	// Special case for root DNS name.
		while( *a )
		{
			if( *a > 63 )
			{
				SNPrintF_Add( &dst, lim, "<<INVALID DNS LABEL LENGTH %u>>", *a );
				break;
			}
			if( ( dst + *a ) >= &inBuf[ 254 ] )
			{
				SNPrintF_Add( &dst, lim, "<<DNS NAME TOO LONG>>" );
				break;
			}
			SNPrintF_Add( &dst, lim, "%#s.", a );
			a += ( 1 + *a );
		}
		i = (int)( dst - inBuf );
		s = inBuf;
	}
	else if( F->altForm == 3 )	// %###s: Cleansed function name (i.e. isolate just the [<class>::]<function> part).
	{
		const char *		functionStart;
		
		// This needs to handle function names with the following forms:
		//
		// main
		// main(int, const char **)
		// int main(int, const char **)
		// MyClass::operator()
		// MyClass::operator'()'
		// const char * MyClass::MyFunction(const char *) const
		// void *MyClass::MyFunction(const char *) const
		// +[MyClass MyMethod]
		// -[MyClass MyMethod:andParam2:]
		
		functionStart = inStr;
		if( ( *functionStart == '+' ) || ( *functionStart == '-' ) ) // Objective-C class or instance method.
		{
			s = functionStart + strlen( functionStart );
		}
		else
		{
			for( s = inStr; ( ( c = *s ) != '\0' ) && ( c != ':' ); ++s )
			{
				if( c == ' ' ) functionStart = s + 1;
			}
			if( c == ':' ) c = *( ++s );
			if( c == ':' ) ++s;
			else
			{
				// Non-C++ function so re-do the search for a C function name.
				
				functionStart = inStr;
				for( s = inStr; ( ( c = *s ) != '\0' ) && ( c != '(' ); ++s )
				{
					if( c == ' ' ) functionStart = s + 1;
				}
			}
			for( ; ( ( c = *s ) != '\0' ) && ( c != ' ' ) && ( c != '(' ); ++s ) {}
			if( (      s[ 0 ] == '(' ) && ( s[ 1 ] == ')' ) && ( s[ 2 ] == '('   ) ) s += 2;
			else if( ( s[ 0 ] == '(' ) && ( s[ 1 ] == ')' ) && ( s[ 2 ] == '\''  ) ) s += 3;
			if( ( functionStart < s ) && ( *functionStart == '*' ) ) ++functionStart;
		}
		i = (int)( s - functionStart );
		s = functionStart;
	}
	else
	{
		i = SNPrintF( inBuf, kPrintFBufSize, "<< ERROR: %%s with too many #'s (%d) >>", F->altForm );
		s = inBuf;
	}
	
	// Make sure we don't truncate in the middle of a UTF-8 character.
	
	if( F->havePrecision && ( i > (int) F->precision ) )
	{
		for( i = (int) F->precision; ( i > 0 ) && ( ( s[ i ] & 0xC0 ) == 0x80 ); --i ) {}
	}
	*outStrPtr = s;
	return( i );
}

//===========================================================================================================================
//	PrintFWriteText
//===========================================================================================================================

static int	PrintFWriteText( PrintFContext *inContext, PrintFFormat *inFormat, const char *inText, size_t inSize )
{
	int		total = 0;
	int		err;
	int		n;
	
	n = (int) inSize;
	if( inFormat->prefix != '\0' ) n += 1;
	if( inFormat->suffix != '\0' ) n += 1;
	
	// Pad on the left.
	
	if( !inFormat->leftJustify && ( n < (int) inFormat->fieldWidth ) )
	{
		do
		{
			err = inContext->callback( " ", 1, inContext );
			if( err < 0 ) goto error;
			total += 1;
			
		}	while( n < (int) --inFormat->fieldWidth );
	}
	
	// Write the prefix (if any).
	
	if( inFormat->prefix != '\0' )
	{
		err = inContext->callback( &inFormat->prefix, 1, inContext );
		if( err < 0 ) goto error;
		total += 1;
	}
	
	// Write the actual text.
	
	err = inContext->callback( inText, inSize, inContext );
	if( err < 0 ) goto error;
	total += (int) inSize;
	
	// Write the suffix (if any).
	
	if( inFormat->suffix != '\0' )
	{
		err = inContext->callback( &inFormat->suffix, 1, inContext );
		if( err < 0 ) goto error;
		total += 1;
	}
	
	// Pad on the right.
	
	for( ; n < (int) inFormat->fieldWidth; ++n )
	{
		err = inContext->callback( " ", 1, inContext );
		if( err < 0 ) goto error;
		total += 1;
	}
	
	return( total );

error:
	return( err );
}

//===========================================================================================================================
//	PrintFWriteTimeDuration
//
//	Converts seconds into a days, hours, minutes, and seconds string. For example: 930232 -> "10d 18h 23m 52s".
//===========================================================================================================================

static int	PrintFWriteTimeDuration( uint64_t inSeconds, int inAltForm, char *inBuf )
{
	unsigned int		years;
	unsigned int		remain;
	unsigned int		days;
	unsigned int		hours;
	unsigned int		minutes;
	unsigned int		seconds;
	unsigned int		x;
	char *				dst;
	
	years	= (unsigned int)( inSeconds / kSecondsPerYear );
	remain	= (unsigned int)( inSeconds % kSecondsPerYear );
	days    = remain / kSecondsPerDay;
	remain	= remain % kSecondsPerDay;
	hours	= remain / kSecondsPerHour;
	remain	= remain % kSecondsPerHour;
	minutes	= remain / kSecondsPerMinute;
	seconds	= remain % kSecondsPerMinute;
	
	dst = inBuf;
	if( years != 0 )
	{
		append_decimal_string( years, dst );
		*dst++ = 'y';
	}
	if( days != 0 )
	{
		if( dst != inBuf ) *dst++ = ' ';
		append_decimal_string( days, dst );
		*dst++ = 'd';
	}
	x = hours;
	if( x != 0 )
	{
		if( dst != inBuf ) *dst++ = ' ';
		if( inAltForm && ( x < 10 ) ) *dst++ = '0';
		append_decimal_string( x, dst );
		*dst++ = inAltForm ? ':' : 'h';
	}
	x = minutes;
	if( ( x != 0 ) || inAltForm )
	{
		if( !inAltForm && ( dst != inBuf ) )				*dst++ = ' ';
		if( inAltForm  && ( x < 10 ) && ( hours != 0 ) )	*dst++ = '0';
		append_decimal_string( x, dst );
		*dst++ = inAltForm ? ':' : 'm';
	}
	x = seconds;
	if( ( x != 0 ) || ( dst == inBuf ) || inAltForm )
	{
		if( !inAltForm && ( dst != inBuf ) ) *dst++ = ' ';
		if( inAltForm  && ( x < 10 ) )		 *dst++ = '0';
		append_decimal_string( x, dst );
		if( !inAltForm ) *dst++ = 's';
	}
	*dst = '\0';
	return( (int)( dst - inBuf ) );
}

//===========================================================================================================================
//	PrintFWriteTLV8
//===========================================================================================================================

static int
	PrintFWriteTLV8( 
		PrintFContext *	inContext, 
		PrintFFormat *	inFormat, 
		const char *	inDescriptors, 
		const uint8_t *	inPtr, 
		size_t			inLen )
{
	int							total	= 0, n;
	const uint8_t * const		end		= inPtr + inLen;
	const uint8_t *				src;
	const uint8_t *				ptr;
	const uint8_t *				ptr2;
	const uint8_t *				next;
	unsigned int				len, len2;
	uint8_t						type, type2;
	unsigned int				widestDesc, widestLen;
	const char *				descPtr;
	const char *				label;
	Boolean						isText;
	
	// Determine the widest pieces.
	
	widestDesc = 0;
	widestLen  = 0;
	for( src = inPtr; ( end - src ) >= 2; src = next )
	{
		type = src[ 0 ];
		len  = src[ 1 ];
		ptr  = &src[ 2 ];
		next = ptr + len;
		if( ( next < src ) || ( next > end ) ) break;
		if( len > widestLen ) widestLen = len;
		
		for( descPtr = inDescriptors; ; descPtr += ( len2 + 1 ) )
		{
			type2 = (uint8_t)( *descPtr++ );
			len2 = (unsigned int) strlen( descPtr );
			if( len2 == 0 ) break;
			if( type2 != type ) continue;
			if( len2 > widestDesc ) widestDesc = len2;
		}
	}
	widestLen = ( widestLen < 10 ) ? 1 : ( widestLen < 100 ) ? 2 : 3;
	
	// Print each item.
	
	for( src = inPtr; ( end - src ) >= 2; src = next )
	{
		type = src[ 0 ];
		len  = src[ 1 ];
		ptr  = &src[ 2 ];
		next = ptr + len;
		if( ( next < src ) || ( next > end ) ) break;
		
		// Search for a label for this type.
		
		label = NULL;
		for( descPtr = inDescriptors; ; descPtr += ( len2 + 1 ) )
		{
			type2 = (uint8_t)( *descPtr++ );
			len2 = (unsigned int) strlen( descPtr );
			if( len2 == 0 ) break;
			if( type2 != type ) continue;
			label = descPtr;
			break;
		}
		
		// Print the item.
		
		for( ptr2 = ptr; ( ptr2 != next ) && PrintFIsPrintable( *ptr2 ); ++ptr2 ) {}
		isText = ( ptr2 == next ) ? true : false;
		
		n = PrintFCore( inContext, "%*s0x%02X", inFormat->fieldWidth * 4, "", type );
		require_action_quiet( n >= 0, exit, total = n );
		total += n;
		
		if( label )
		{
			n = PrintFCore( inContext, " (%s)", label );
			require_action_quiet( n >= 0, exit, total = n );
			total += n;
		}
		
		len2 = label ? ( widestDesc - ( (unsigned int) strlen( label ) ) ) : ( 3 + widestDesc );
		if( len == 0 )			n = PrintFCore( inContext, ", %*s%*u bytes\n", len2, "", widestLen, 0 );
		else if( isText )		n = PrintFCore( inContext, ", %*s%*u bytes, \"%.*s\"\n", len2, "", widestLen, len, len, ptr );
		else if( len <= 16 )	n = PrintFCore( inContext, ", %*s%*u bytes, %#H\n", len2, "", widestLen, len, ptr, len, len );
		else					n = PrintFCore( inContext, "\n%*.1H", inFormat->fieldWidth + 1, ptr, len, len );
		require_action_quiet( n >= 0, exit, total = n );
		total += n;
	}
	
exit:
	return( total );
}


//===========================================================================================================================
//	PrintFWriteTXTRecord
//===========================================================================================================================

static int	PrintFWriteTXTRecord( PrintFContext *inContext, PrintFFormat *inFormat, const void *inPtr, size_t inLen )
{
	const unsigned int		kIndent = inFormat->fieldWidth * 4;
	int						total   = 0;
	int						n;
	const uint8_t *			buf;
	const uint8_t *			src;
	const uint8_t *			end;
	size_t					len;
	uint8_t					c;
	const char *			seperator;
	
	buf = (const uint8_t *) inPtr;
	src = buf;
	end = src + inLen;
	
	// AltForm prints the TXT record on a single line.
	
	if( inFormat->altForm )
	{
		seperator = "";
		for( ; src < end; src += len )
		{
			len = *src++;
			if( ( (size_t)( end - src ) ) < len ) break;
			
			n = PrintFCore( inContext, "%s%.*s", seperator, (int) len, src );
			require_action_quiet( n >= 0, exit, total = n );
			total += n;
			seperator = " | ";
		}
	}
	else
	{
		// Handle AirPort TXT records that are one big entry with comma-separated name=value pairs.
		
		if( ( inLen >= 6 ) && ( memcmp( &src[ 1 ], "waMA=", 5 ) == 0 ) )
		{
			uint8_t			tempBuf[ 256 ];
			uint8_t *		tempPtr;
			
			len = *src++;
			if( ( src + len ) != end )
			{
				n = PrintFCore( inContext, "%*s### bad TXT record length byte (%zu, %zu expected)\n", 
					kIndent, "", len, (size_t)( end - src ) );
				require_action_quiet( n >= 0, exit, total = n );
				total += n;
				goto exit;
			}
			while( src < end )
			{
				tempPtr = tempBuf;
				while( src < end )
				{
					c = *src++;
					if( c == ',' ) break;
					if( c == '\\' )
					{
						if( src >= end )
						{
							n = PrintFCore( inContext, "%*s### bad TXT escape: %.*s\n", kIndent, "", 
								(int)( inLen - 1 ), buf + 1 );
							require_action_quiet( n >= 0, exit, total = n );
							total += n;
							goto exit;
						}
						c = *src++;
					}
					*tempPtr++ = c;
				}
				
				n = PrintFCore( inContext, "%*s%.*s\n", kIndent, "", (int)( tempPtr - tempBuf ), tempBuf );
				require_action_quiet( n >= 0, exit, total = n );
				total += n;
			}
		}
		else
		{
			for( ; src < end; src += len )
			{
				len = *src++;
				if( ( src + len ) > end )
				{
					n = PrintFCore( inContext, "%*s### TXT record length byte too big (%zu, %zu max)\n", 
						kIndent, "", len, (size_t)( end - src ) );
					require_action_quiet( n >= 0, exit, total = n );
					total += n;
					goto exit;
				}
				n = PrintFCore( inContext, "%*s%.*s\n", kIndent, "", (int) len, src );
				require_action_quiet( n >= 0, exit, total = n );
				total += n;
			}
		}
		if( ( inLen == 0 ) || ( buf[ 0 ] == 0 ) )
		{
			n = PrintFCore( inContext, "\n" );
			require_action_quiet( n >= 0, exit, total = n );
			total += n;
		}
	}
	
exit:
	return( total );
}

//===========================================================================================================================
//	PrintFWriteUnicodeString
//===========================================================================================================================

static int	PrintFWriteUnicodeString( const uint8_t *inStr, PrintFFormat *inFormat, char *inBuf )
{
	int						i;
	const uint8_t *			a;
	const uint16_t *		u;
	PrintFFormat *			F;
	char *					p;
	char *					q;
	int						endianIndex;
	
	a = inStr;
	F = inFormat;
	if( !F->havePrecision || ( F->precision > 0 ) )
	{
		if(      ( a[ 0 ] == 0xFE ) && ( a[ 1 ] == 0xFF ) ) { F->altForm = 1; a += 2; --F->precision; } // Big Endian
		else if( ( a[ 0 ] == 0xFF ) && ( a[ 1 ] == 0xFE ) ) { F->altForm = 2; a += 2; --F->precision; } // Little Endian
	}
	u = (const uint16_t *) a;
	p = inBuf;
	q = p + kPrintFBufSize;
	switch( F->altForm )
	{
		case 0:	// Host Endian
			for( i = 0; ( !F->havePrecision || ( i < (int) F->precision ) ) && u[ i ] && ( ( q - p ) > 0 ); ++i )
			{
				*p++ = PrintFMakePrintable( u[ i ] );
			}
			break;
		
		case 1:	// Big Endian
		case 2:	// Little Endian
			endianIndex = 1 - ( F->altForm - 1 );
			for( i = 0; ( !F->havePrecision || ( i < (int) F->precision ) ) && u[ i ] && ( ( q - p ) > 0 ); ++i )
			{
				*p++ = PrintFMakePrintable( a[ endianIndex ] );
				a += 2;
			}
			break;
		
		default:
			i = SNPrintF( inBuf, kPrintFBufSize, "<< ERROR: %%S with too many #'s (%d) >>", F->altForm );
			break;
	}
	return( i );
}

//===========================================================================================================================
//	PrintFWriteXMLEscaped
//===========================================================================================================================

static int	PrintFWriteXMLEscaped( PrintFContext *inContext, const char *inPtr, size_t inLen )
{
	const char * const		end		= inPtr + ( ( inLen == kSizeCString ) ? strlen( inPtr ) : inLen );
	int						total	= 0;
	int						n;
	char					c;
	const char *			run;
	size_t					len;
	const char *			replace;
	
	run = inPtr;
	while( inPtr < end )
	{
		c = *inPtr;
		switch( c )
		{
			case '&':	replace = "&amp;";	break;
			case '"':	replace = "&quot;";	break;
			case '\'':	replace = "&#39;";	break; // No &apos; in HTML 4, but &#39; works in HTML, XHTML, and XML.
			case '<':	replace = "&lt;";	break;
			case '>':	replace = "&gt;";	break;
			default:	replace = NULL;		break;
		}
		if( replace )
		{
			len = (size_t)( inPtr - run );
			++inPtr;
			if( len > 0 )
			{
				n = inContext->callback( run, len, inContext );
				require_action_quiet( n >= 0, exit, total = n );
				total += n;
			}
			run = inPtr;
			
			n = PrintFCore( inContext, "%s", replace );
			require_action_quiet( n >= 0, exit, total = n );
			total += n;
		}
		else
		{
			++inPtr;
		}
	}
	
	len = (size_t)( inPtr - run );
	if( len > 0 )
	{
		n = inContext->callback( run, len, inContext );
		require_action_quiet( n >= 0, exit, total = n );
		total += n;
	}
	
exit:
	return( total );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	PrintFCallBackFixedString
//===========================================================================================================================

static int	PrintFCallBackFixedString( const char *inStr, size_t inSize, PrintFContext *inContext )
{
	size_t		n;
	
	// If the string is too long, truncate it, but don't truncate in the middle of a UTF-8 character.
	
	n = inContext->reservedSize - inContext->usedSize;
	if( inSize > n )
	{
		while( ( n > 0 ) && ( ( inStr[ n ] & 0xC0 ) == 0x80 ) ) --n;
		inSize = n;
	}
	
	// Copy the string (excluding any null terminator).
	
	if( inSize > 0 ) memcpy( inContext->str + inContext->usedSize, inStr, inSize );
	inContext->usedSize += inSize;
	return( (int) inSize );
}

//===========================================================================================================================
//	PrintFCallBackAllocatedString
//===========================================================================================================================

static int	PrintFCallBackAllocatedString( const char *inStr, size_t inSize, PrintFContext *inContext )
{
	int			result;
	size_t		n;
	
	// If there's not enough room in the buffer, resize it. Amortize allocations by rounding the size up.
	
	n = inContext->usedSize + inSize;
	if( n > inContext->reservedSize )
	{
		char *		tmp;
		
		if( n < 256 ) n = 256;
		else		  n = ( n + 1023 ) & ~1023U;
		
		#if( TARGET_NO_REALLOC )
			tmp = (char *) malloc( n );
			require_action( tmp, exit, result = kNoMemoryErr );
			memcpy( tmp, inContext->str, inContext->usedSize );
			
			free( inContext->str );
			inContext->str = tmp;
			inContext->reservedSize = n;
		#else
			tmp = (char *) realloc( inContext->str, n );
			require_action( tmp, exit, result = kNoMemoryErr );
			inContext->str = tmp;
			inContext->reservedSize = n;
		#endif
	}
	
	// Copy the string (excluding any null terminator).
	
	memcpy( inContext->str + inContext->usedSize, inStr, inSize );
	inContext->usedSize += inSize;
	result = (int) inSize;
	
exit:
	return( result );
}

//===========================================================================================================================
//	PrintFCallBackUserCallBack
//===========================================================================================================================

static int	PrintFCallBackUserCallBack( const char *inStr, size_t inSize, PrintFContext *inContext )
{
	return( inContext->userCallBack( inStr, inSize, inContext->userContext ) );
}

#if 0
#pragma mark -
#endif

#if( !defined( PRINTF_UTILS_PRINT_TEST ) )
	#if( DEBUG_FPRINTF_ENABLED )
		#define	PRINTF_UTILS_PRINT_TEST		0
	#endif
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	PrintFUtils_Test
//===========================================================================================================================

static int		_PrintFTestExtension1( PrintFContext *ctx, PrintFFormat *inFormat, PrintFVAList *inArgs, void *inUserContext );
static int		_PrintFTestExtension2( PrintFContext *ctx, PrintFFormat *inFormat, PrintFVAList *inArgs, void *inUserContext );
static OSStatus	_PrintFTestString( const char *inMatch, const char *inFormat, ... );
static OSStatus	_PrintFTestVAList( const char *inMatch, const char *inFormat, ... );

#define	PFTEST1( MATCH, FORMAT, PARAM ) \
	do \
	{ \
		err = _PrintFTestString( ( MATCH ), ( FORMAT ), ( PARAM ) ); \
		require_noerr( err, exit ); \
		\
	}	while( 0 )

#define	PFTEST2( MATCH, FORMAT, PARAM1, PARAM2 ) \
	do \
	{ \
		err = _PrintFTestString( ( MATCH ), ( FORMAT ), ( PARAM1 ), ( PARAM2 ) ); \
		require_noerr( err, exit ); \
		\
	}	while( 0 )

OSStatus	PrintFUtils_Test( void )
{
	OSStatus			err;
	int					n;
	char				buf[ 512 ];
	const char *		src;
	const char *		end;
	char *				dst;
	char *				lim;
	
	// Field Width and Precision Tests.
		
	PFTEST1( ":hello, world:",		":%s:",			"hello, world" );
	PFTEST1( ":hello, world:",		":%10s:",		"hello, world" );
	PFTEST1( ":hello, wor:", 		":%.10s:",		"hello, world" );
	PFTEST1( ":hello, world:",		":%-10s:",		"hello, world" );
	PFTEST1( ":hello, world:",		":%.15s:",		"hello, world" );
	PFTEST1( ":hello, world   :",	":%-15s:",		"hello, world" );
	PFTEST1( ":   hello, world:",	":%15s:",		"hello, world" );
	PFTEST1( ":     hello, wor:",	":%15.10s:",	"hello, world" );
	PFTEST1( ":hello, wor     :",	":%-15.10s:",	"hello, world" );
	
	PFTEST1( ":'hello, world':", ":%'s:", "hello, world" );
	PFTEST1( ":\"hello, world\":", ":%''s:", "hello, world" );
	PFTEST1( ":'hello, wor':", ":%'.12s:", "hello, world" );
	PFTEST1( ":   'hello, world':", ":%'17s:", "hello, world" );
	PFTEST1( ":'h':", ":%'.3s:", "hello, world" );
	PFTEST1( ":'':", ":%'.2s:", "hello, world" );
	PFTEST1( "::", ":%'.1s:", "hello, world" );
	
	// Number Tests.
	
	PFTEST1( "1234", "%d", 1234 );
	PFTEST1( "1234", "%#d", 1234 );
	PFTEST1( "0", "%'d", 0 );
	PFTEST1( "1", "%'d", 1 );
	PFTEST1( "12", "%'d", 12 );
	PFTEST1( "123", "%'d", 123 );
	PFTEST1( "1,234", "%'d", 1234 );
	PFTEST1( "12,345", "%'d", 12345 );
	PFTEST1( "123,456", "%'d", 123456 );
	PFTEST1( "1,234,567", "%'d", 1234567 );
	PFTEST1( "-1", "%'d", -1 );
	PFTEST1( "-12", "%'d", -12 );
	PFTEST1( "-123", "%'d", -123 );
	PFTEST1( "-1,234", "%'d", -1234 );
	PFTEST1( "-12,345", "%'d", -12345 );
	PFTEST1( "-123,456", "%'d", -123456 );
	PFTEST1( "-1,234,567", "%'d", -1234567 );
	PFTEST1( "1234", "%i", 1234 );
	PFTEST1( "1234", "%u", 1234 );
	PFTEST1( "2322", "%o", 1234 );
	PFTEST1( "02322", "%#o", 1234 );
	PFTEST1( "0777", "%#2o", 0777 );
	PFTEST1( "12AB", "%X", 0x12AB );
	PFTEST1( "12ab", "%x", 0x12AB );
	PFTEST1( "0x12ab", "%#x", 0x12AB );
	PFTEST1( "0x1", "%#01x", 0x1 );
	PFTEST1( "0x01", "%#04x", 0x1 );
	PFTEST1( "1001010101011", "%b", 0x12AB );
	PFTEST1( " 1234", "%5d", 1234 );
	PFTEST1( "1234 ", "%-5d", 1234 );
	PFTEST1( "2147483647", "%ld", 2147483647L );
	PFTEST1( "4294967295", "%lu", (unsigned long) UINT32_C( 4294967295 ) );
	PFTEST1( "9223372036854775807", "%lld", INT64_C( 9223372036854775807 ) );
	PFTEST1( "-9223372036854775807", "%lld", INT64_C( -9223372036854775807 ) );
	PFTEST1( "18446744073709551615", "%llu", UINT64_C( 18446744073709551615 ) );
	PFTEST1( "-46", "%hhd", 1234 );
	PFTEST1( "210", "%hhu", 1234 );
	PFTEST1( "12345678", "%jX", (intmax_t) 0x12345678 );
	PFTEST1( "12345678", "%zX", (size_t) 0x12345678 );
	PFTEST1( "305419896", "%zd", (size_t) 0x12345678 );
	PFTEST1( "12345678", "%tX", (ptrdiff_t) 0x12345678 );
	PFTEST1( "1111011011110110111101101111011", "%lb", (unsigned long) 0x7B7B7B7B );

#if( PRINTF_ENABLE_FLOATING_POINT )
	PFTEST1( "123.234000", "%f", 123.234 );
	PFTEST1( "123.23", "%.2f", 123.234 );
	PFTEST1( " 123.2340000000", "%15.10f", 123.234 );
	PFTEST1( "123.2340000000 ", "%-15.10f", 123.234 );
#endif
	
	// Bit Number Tests.
	
	PFTEST1( "12 9 5 4 2", "%##b", 0x1234 );
	PFTEST1( "19 22 26 27 29", "%###b", 0x1234 );
	PFTEST1( "3 6 10 11 13", "%###hb", 0x1234 );
	PFTEST1( "4 1", "%##.8lb", UINT32_C( 0x12 ) );
	PFTEST1( "59 62", "%###llb", UINT64_C( 0x12 ) );
	PFTEST1( "4 1", "%##.8llb", UINT64_C( 0x12 ) );
	PFTEST1( "3 6", "%###.8llb", UINT64_C( 0x12 ) );
	PFTEST1( "", "%###.0llb", UINT64_C( 0x12 ) );
	PFTEST1( "ERROR: << precision must be 0-64 >>", "%###.128llb", UINT64_C( 0x12 ) );
	PFTEST1( "31 0", "%##b", 0x80000001U );
	PFTEST1( "0 31", "%###b", 0x80000001U );
	PFTEST1( "31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0", "%##b", 0xFFFFFFFFU );
	PFTEST1( "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31", "%###b", 0xFFFFFFFFU );
	PFTEST1( "4 1 0     ", "%-##10b", 0x13 );
	PFTEST1( "     4 1 0", "%##10b", 0x13 );
	
	// String Tests.
	
	PFTEST1( "test", "%#s", "\04test" );
	PFTEST1( "www.apple.com.", "%##s", "\03www\05apple\03com" );
	
	// Address Tests.
	
	PFTEST1( "1.2.3.4", "%.2a", "\x12\x34" );
	PFTEST1( "17.205.123.5", "%.4a", "\x11\xCD\x7B\x05" );
	PFTEST1( "00:11:22:AA:BB:CC", "%.6a", "\x00\x11\x22\xAA\xBB\xCC" );
	PFTEST1( "00:11:22:AA:BB:CC:56:78", "%.8a", "\x00\x11\x22\xAA\xBB\xCC\x56\x78" );
	PFTEST1( "102:304:506:708:90a:b0c:d0e:f10", "%.16a", "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10" );
	
	{
		mDNSAddrCompat		maddr;
		
		memset( &maddr, 0, sizeof( maddr ) );
		maddr.type = 4;
		maddr.ip.v4[ 0 ] = 127;
		maddr.ip.v4[ 1 ] = 0;
		maddr.ip.v4[ 2 ] = 0;
		maddr.ip.v4[ 3 ] = 1;
		PFTEST1( "127.0.0.1", "%#a", &maddr );
		
		memset( &maddr, 0, sizeof( maddr ) );
		maddr.type = 6;
		maddr.ip.v6[  0 ] = 0xFE;
		maddr.ip.v6[  1 ] = 0x80;
		maddr.ip.v6[ 15 ] = 0x01;
		PFTEST1( "fe80::1", "%#a", &maddr );
	}
	
	#if( defined( AF_INET ) )
	{
		struct sockaddr_in		sa4;
		
		memset( &sa4, 0, sizeof( sa4 ) );
		SIN_LEN_SET( &sa4 );
		sa4.sin_family 		= AF_INET;
		sa4.sin_port		= hton16( 80 );
		sa4.sin_addr.s_addr	= hton32( UINT32_C( 0x7f000001 ) );
		
		PFTEST1( "127.0.0.1:80", "%##a", &sa4 );
	}
	#endif
	
	#if( defined( AF_INET6 ) )
	{
		struct sockaddr_in6		sa6;
		
		memset( &sa6, 0, sizeof( sa6 ) );
		SIN6_LEN_SET( &sa6 );
		sa6.sin6_family 			= AF_INET6;
		sa6.sin6_port				= hton16( 80 );
		sa6.sin6_addr.s6_addr[  0 ]	= 0xFE;
		sa6.sin6_addr.s6_addr[  1 ]	= 0x80;
		sa6.sin6_addr.s6_addr[ 15 ]	= 0x01;
		sa6.sin6_scope_id			= 2;
		
		#if( TARGET_OS_POSIX )
		{
			char		ifname[ IF_NAMESIZE ];
			
			SNPrintF( buf, sizeof( buf ), "[fe80::1%%%s]:80", if_indextoname( sa6.sin6_scope_id, ifname ) );
			PFTEST1( buf, "%##a", &sa6 );
		}
		#else
			PFTEST1( "[fe80::1%2]:80", "%##a", &sa6 );
		#endif
		
		memset( &sa6, 0, sizeof( sa6 ) );
		SIN6_LEN_SET( &sa6 );
		sa6.sin6_family 			= AF_INET6;
		sa6.sin6_port				= hton16( 80 );
		sa6.sin6_addr.s6_addr[ 10 ] = 0xFF; // ::ffff:<32-bit IPv4 address>
		sa6.sin6_addr.s6_addr[ 11 ] = 0xFF;
		memcpy( &sa6.sin6_addr.s6_addr[ 12 ], "\xE8\x05\x0F\x49", 4 ); // IPv4 address is in the low 32 bits of the IPv6 address.
		
		PFTEST1( "[::ffff:232.5.15.73]:80", "%##a", &sa6 );
	}
	#endif
	
	// Unicode Tests.
	
	PFTEST2( "tes", "%.*s", 4, "tes" );
	PFTEST2( "test", "%.*s", 4, "test" );
	PFTEST2( "test", "%.*s", 4, "testing" );
	PFTEST2( "te\xC3\xA9", "%.*s", 4, "te\xC3\xA9" );
	PFTEST2( "te\xC3\xA9", "%.*s", 4, "te\xC3\xA9ing" );
	PFTEST2( "tes", "%.*s", 4, "tes\xC3\xA9ing" );
	PFTEST2( "t\xE3\x82\xBA", "%.*s", 4, "t\xE3\x82\xBA" );
	PFTEST2( "t\xE3\x82\xBA", "%.*s", 4, "t\xE3\x82\xBAing" );
	PFTEST2( "te", "%.*s", 4, "te\xE3\x82\xBA" );
	PFTEST2( "te", "%.*s", 4, "te\xE3\x82\xBAing" );
	PFTEST2( "te\xC3\xA9\xE3\x82\xBA", "%.*s", 7, "te\xC3\xA9\xE3\x82\xBAing" );
	PFTEST2( "te\xC3\xA9", "%.*s", 6, "te\xC3\xA9\xE3\x82\xBAing" );
	PFTEST2( "te\xC3\xA9", "%.*s", 5, "te\xC3\xA9\xE3\x82\xBAing" );
	#if( TARGET_RT_BIG_ENDIAN )
		PFTEST1( "abcd", "%S", "\x00" "a" "\x00" "b" "\x00" "c" "\x00" "d" "\x00" "\x00" );
	#else
		PFTEST1( "abcd", "%S", "a" "\x00" "b" "\x00" "c" "\x00" "d" "\x00" "\x00" "\x00" );
	#endif
	PFTEST1( "abcd", "%S", 
		"\xFE\xFF" "\x00" "a" "\x00" "b" "\x00" "c" "\x00" "d" "\x00" "\x00" );	// Big Endian BOM
	PFTEST1( "abcd", "%S", 
		"\xFF\xFE" "a" "\x00" "b" "\x00" "c" "\x00" "d" "\x00" "\x00" "\x00" );	// Little Endian BOM
	PFTEST1( "abcd", "%#S", "\x00" "a" "\x00" "b" "\x00" "c" "\x00" "d" "\x00" "\x00" );	// Big Endian
	PFTEST1( "abcd", "%##S", "a" "\x00" "b" "\x00" "c" "\x00" "d" "\x00" "\x00" "\x00" );	// Little Endian
	PFTEST2( "abc", "%.*S", 
		4, "\xFE\xFF" "\x00" "a" "\x00" "b" "\x00" "c" "\x00" "d" );	// Big Endian BOM
	PFTEST2( "abc", "%.*S", 
		4, "\xFF\xFE" "a" "\x00" "b" "\x00" "c" "\x00" "d" "\x00" );	// Little Endian BOM
	#if( TARGET_RT_BIG_ENDIAN )
		PFTEST2( "abc", "%.*S", 3, "\x00" "a" "\x00" "b" "\x00" "c" "\x00" "d" );
	#else
		PFTEST2( "abc", "%.*S", 3, "a" "\x00" "b" "\x00" "c" "\x00" "d" "\x00" );
	#endif
	PFTEST2( "abc", "%#.*S", 3, "\x00" "a" "\x00" "b" "\x00" "c" "\x00" "d" );	// Big Endian
	PFTEST2( "abc", "%##.*S", 3, "a" "\x00" "b" "\x00" "c" "\x00" "d" "\x00" );	// Little Endian

#if( TARGET_OS_WINDOWS )
	PFTEST1( "Testing", "%T", TEXT( "Testing" ) );
#endif

	// Other Tests.
	
	PFTEST1( "a", "%c", 'a' );
	PFTEST1( "'a'", "%'c", 'a' );
	PFTEST1( "AbCd", "%C", 0x41624364 /* 'AbCd' */ );
	PFTEST1( "'AbCd'", "%'C", 0x41624364 /* 'AbCd' */ );
	PFTEST1( "6ba7b810-9dad-11d1-80b4-00c04fd430c8", "%U",  "\x10\xb8\xa7\x6b" "\xad\x9d" "\xd1\x11" "\x80\xb4" "\x00\xc0\x4f\xd4\x30\xc8" );
	PFTEST1( "10b8a76b-ad9d-d111-80b4-00c04fd430c8", "%#U", "\x10\xb8\xa7\x6b" "\xad\x9d" "\xd1\x11" "\x80\xb4" "\x00\xc0\x4f\xd4\x30\xc8" );
	
	PFTEST2( "Player1   -> Broadcast: <Poll> ", "%{cec}", "\x4F", 1 );
	
#if( DEBUG || DEBUG_EXPORT_ERROR_STRINGS )
	PFTEST1( "noErr", "%m", 0 );
	PFTEST1( "kUnknownErr", "%m", kUnknownErr );
	PFTEST1( "-6700/0xFFFFE5D4 kUnknownErr", "%#m", kUnknownErr );
#endif

	err = _PrintFTestString( 
		"6b a7 b8 10 9d ad 11 d1 80 b4 00 c0 4f d4 30 c8", 
		"%H", 
		"\x6b\xa7\xb8\x10\x9d\xad\x11\xd1\x80\xb4\x00\xc0\x4f\xd4\x30\xc8", 16, 16 );
	require_noerr( err, exit );
	
	err = _PrintFTestString( 
		"6b a7 b8 10 9d ad 11 d1 80 b4 00 c0 4f d4 30 c8 "
		"6b a7 b8 10 9d ad 11 d1 80 b4 00 c0 4f d4 30 c8", 
		"%H", 
		"\x6b\xa7\xb8\x10\x9d\xad\x11\xd1\x80\xb4\x00\xc0\x4f\xd4\x30\xc8"
		"\x6b\xa7\xb8\x10\x9d\xad\x11\xd1\x80\xb4\x00\xc0\x4f\xd4\x30\xc8", 
		32, 32 );
	require_noerr( err, exit );
	
	err = _PrintFTestString( "6b a7", "%H", "\x6b\xa7", 2, 2 );
	require_noerr( err, exit );
	
	err = _PrintFTestString( "0123456789abcdef", "%.3H", "\x01\x23\x45\x67\x89\xab\xcd\xef", 8, 8 );
	require_noerr( err, exit );
	
	err = _PrintFTestString( "0123456789ABCDEF", "%.4H", "\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, 8 );
	require_noerr( err, exit );
	
#if( DEBUG_CF_OBJECTS_ENABLED )
{
	CFNumberRef		num;
	int64_t			s64;
	
	PFTEST1( ":hello, world:",		":%@:",			CFSTR( "hello, world" ) );
	PFTEST1( ":hello, wor     :",	":%-15.10@:",	CFSTR( "hello, world" ) );
	PFTEST1( ":   hello, world:",	":%15@:",		CFSTR( "hello, world" ) );
	
	s64 = INT64_C( 9223372036854775807 );
	num = CFNumberCreate( NULL, kCFNumberSInt64Type, &s64 );
	require_action( num, exit, err = kNoMemoryErr );
	
	err = _PrintFTestString( "9223372036854775807", "%@", num );
	CFRelease( num );
	require_noerr( err, exit );
}
#endif
	
	PFTEST1( "1.2.3b4", "%v", (uint32_t) 0x01236004 );
	PFTEST1( "%5", "%%%d", 5 );
	
	err = _PrintFTestString( "Test 123 hello, world", "Test %d %s%n", 123, "hello, world", &n );
	require_noerr( err, exit );
	require_action( n == 21, exit, err = kResponseErr );
	
	err = _PrintFTestVAList( "begin 123 test 456 end", "%d test %s%n", 123, "456", &n );
	require_noerr( err, exit );
	require_action( n == 12, exit, err = kResponseErr );
	
	PFTEST1( "0s", "%{dur}", 0 );
	PFTEST1( "10d 18h 23m 52s", "%{dur}", 930232 );
	
	PFTEST1( "a", "%c", 'a' );
	
	err = _PrintFTestString( 
		"Line 1"
		, "%{text}",
		"Line 1"
	, kSizeCString );
	require_noerr( err, exit );
	
	err = _PrintFTestString( 
		"Line 1\n"
		"Line 2\n"
		"Line 3"
		, "%{text}",
		"Line 1\n"
		"Line 2\n"
		"Line 3"
	, kSizeCString );
	require_noerr( err, exit );
	
	err = _PrintFTestString( 
		"Line 1\n"
		"Line 2\n"
		"Line 3\n"
		"\n"
		"\n"
		"\n"
		"Line 7\n"
		, "%{text}",
		"Line 1\r"
		"Line 2\r\n"
		"Line 3\n"
		"\n"
		"\r"
		"\r\n"
		"Line 7\n"
		, kSizeCString );
	require_noerr( err, exit );
	
	err = _PrintFTestString( 
		"    Line 1\n"
		"    Line 2\n"
		"    Line 3\n"
		"\n"
		"\n"
		"\n"
		"    Line 7\n"
		, "%1{text}",
		"Line 1\r"
		"Line 2\r\n"
		"Line 3\n"
		"\n"
		"\r"
		"\r\n"
		"Line 7\n"
		, kSizeCString );
	require_noerr( err, exit );
	
	err = _PrintFTestString( ":abc123:", ":%?s%?d%?d%?s:", true, "abc", true, 123, false, 456, false, "xyz" );
	require_noerr( err, exit );
	
	// %{fill}
	
	PFTEST2( "", "%{fill}", '*', 0 );
	PFTEST2( "*", "%{fill}", '*', 1 );
	PFTEST2( "*******", "%{fill}", '*', 7 );
	PFTEST2( "\t\t\t", "%{fill}", '\t', 3 );
	memset( buf, 'z', 400 );
	buf[ 400 ] = '\0';
	PFTEST2( buf, "%{fill}", 'z', 400 );
	
	// %{flags}
	
	PFTEST2( "< >", "%{flags}", 0, 
		"\x02" "FLAG2\0"
		"\x08" "FLAG8\0"
		"\x00" );
	PFTEST2( "< FLAG2 FLAG8 >", "%{flags}", 0x1F4, 
		"\x02" "FLAG2\0"
		"\x08" "FLAG8\0"
		"\x00" );
	PFTEST2( "0xFF < FLAG0 FLAG1 FLAG2 FLAG3 FLAG4 FLAG5 FLAG6 FLAG7 >", "%#{flags}", 0xFF, 
		"\x00" "FLAG0\0"
		"\x01" "FLAG1\0"
		"\x02" "FLAG2\0"
		"\x03" "FLAG3\0"
		"\x04" "FLAG4\0"
		"\x05" "FLAG5\0"
		"\x06" "FLAG6\0"
		"\x07" "FLAG7\0"
		"\x00" );
	PFTEST2( "0x800000000022 < FLAG1 FLAG5 FLAG47 >", "%#ll{flags}", UINT64_C( 0x800000000022 ), 
		"\x00" "FLAG0\0"
		"\x01" "FLAG1\0"
		"\x05" "FLAG5\0"
		"\x2F" "FLAG47\0"
		"\x30" "FLAG48\0"
		"\x00" );
	
	// %{ptr}
	
	n = SNPrintF( buf, sizeof( buf ), "%{ptr}", buf );
	require_action( n == 6, exit, err = kSizeErr );
	require_action( buf[ 0 ] == '0', exit, err = kMismatchErr );
	require_action( buf[ 1 ] == 'x', exit, err = kMismatchErr );
	
	n = SNPrintF( buf, sizeof( buf ), "%{ptr}", NULL );
	require_action( n == 6, exit, err = kSizeErr );
	require_action( strcmp( buf, "0x0000" ) == 0, exit, err = kMismatchErr );
	
	// %#{txt}
	
	n = SNPrintF( buf, sizeof( buf ), "%#{txt}", NULL, (size_t) 0 );
	require_action( n == 0, exit, err = kSizeErr );
	require_action( strcmp( buf, "" ) == 0, exit, err = kMismatchErr );
	
	n = SNPrintF( buf, sizeof( buf ), "%#{txt}", "\x03" "a=b", (size_t) 4 );
	require_action( n == 3, exit, err = kSizeErr );
	require_action( strcmp( buf, "a=b" ) == 0, exit, err = kMismatchErr );
	
	n = SNPrintF( buf, sizeof( buf ), "%#{txt}", "\x06" "a=1234" "\x05" "abc=x", (size_t) 13 );
	require_action( n == 14, exit, err = kSizeErr );
	require_action( strcmp( buf, "a=1234 | abc=x" ) == 0, exit, err = kMismatchErr );
	
	// %{xml}
	
	PFTEST2( "",										"%{xml}", "", 0 );
	PFTEST2( "test",									"%{xml}", "test", 4 );
	PFTEST2( "&lt;test&gt;",							"%{xml}", "<test>", kSizeCString );
	PFTEST2( "&amp;&quot;&#39;&lt;&gt;",				"%{xml}", "&\"'<>", kSizeCString );
	PFTEST2( " &amp; &quot; &#39; &lt; &gt; ",			"%{xml}", " & \" ' < > ", kSizeCString );
	PFTEST2( "&lt;test",								"%{xml}", "<test", kSizeCString );
	PFTEST2( "test&gt;",								"%{xml}", "test>", kSizeCString );
	PFTEST2( "Test of a &quot;string&quot; of text",	"%{xml}", "Test of a \"string\" of text", kSizeCString );
	
	// Extension tests.
	
	PFTEST1( "< <<UNKNOWN PRINTF EXTENSION 'test1'>> >", "< %{test1} >", 123 );
	err = PrintFRegisterExtension( "test1", _PrintFTestExtension1, NULL );
	require_noerr( err, exit );
	PFTEST2( "< Alt=0, a=123, b='xyz' >", "< %{test1} >", 123, "xyz" );
	PrintFDeregisterExtension( "test1" );
	PFTEST1( "< <<UNKNOWN PRINTF EXTENSION 'test1'>> >", "< %{test1} >", 123 );
	
	err = PrintFRegisterExtension( "test1", _PrintFTestExtension1, NULL );
	require_noerr( err, exit );
	err = PrintFRegisterExtension( "test2", _PrintFTestExtension2, NULL );
	require_noerr( err, exit );
	PFTEST2( "< Alt=1, a=456, b='ABC' >", "< %#{test1} >", 456, "ABC" );
	PFTEST1( "< NoArgsExtension >", "< %{test2} >", 123 );
	PrintFDeregisterExtension( "test1" );
	PFTEST1( "< <<UNKNOWN PRINTF EXTENSION 'test1'>> >", "< %{test1} >", 123 );
	PFTEST1( "< NoArgsExtension >", "< %{test2} >", 123 );
	PrintFDeregisterExtension( "test2" );
	PFTEST1( "< <<UNKNOWN PRINTF EXTENSION 'test1'>> >", "< %{test1} >", 123 );
	PFTEST1( "< <<UNKNOWN PRINTF EXTENSION 'test2'>> >", "< %{test2} >", 123 );
	
	err = PrintFRegisterExtension( "test1", _PrintFTestExtension1, NULL );
	require_noerr( err, exit );
	err = PrintFRegisterExtension( "test2", _PrintFTestExtension2, NULL );
	require_noerr( err, exit );
	PFTEST2( "< Alt=1, a=456, b='ABC' >", "< %#{test1} >", 456, "ABC" );
	PFTEST1( "< NoArgsExtension >", "< %{test2} >", 123 );
	PrintFDeregisterExtension( "test2" );
	PFTEST2( "< Alt=1, a=456, b='ABC' >", "< %#{test1} >", 456, "ABC" );
	PFTEST1( "< <<UNKNOWN PRINTF EXTENSION 'test2'>> >", "< %{test2} >", 123 );
	PrintFDeregisterExtension( "test1" );
	PFTEST1( "< <<UNKNOWN PRINTF EXTENSION 'test1'>> >", "< %{test1} >", 123 );
	PFTEST1( "< <<UNKNOWN PRINTF EXTENSION 'test2'>> >", "< %{test2} >", 123 );
	
	// Bounds Tests.
	
	memset( buf, 'Z', sizeof( buf ) );
	n = SNPrintF( buf, 0, "%s", "test" );
	require_action( n == 4, exit, err = kResponseErr );
	src = buf;
	end = buf + sizeof( buf );
	while( ( src < end ) && ( *src == 'Z' ) ) ++src;
	require_action( src == end, exit, err = kImmutableErr );
	
	memset( buf, 'Z', sizeof( buf ) );
	n = SNPrintF( buf, 3, "%s", "test" );
	require_action( n == 4, exit, err = kResponseErr );
	require_action( buf[ 2 ] == '\0', exit, err = kResponseErr );
	require_action( strcmp( buf, "te" ) == 0, exit, err = kResponseErr );
	src = buf + 3;
	end = buf + sizeof( buf );
	while( ( src < end ) && ( *src == 'Z' ) ) ++src;
	require_action( src == end, exit, err = kImmutableErr );
	
	memset( buf, 'Z', sizeof( buf ) );
	n = SNPrintF( buf, 4, "%s", "te\xC3\xA9" );
	require_action( n == 4, exit, err = kResponseErr );
	require_action( buf[ 2 ] == '\0', exit, err = kResponseErr );
	require_action( strcmp( buf, "te" ) == 0, exit, err = kResponseErr );
	src = buf + 3;
	end = buf + sizeof( buf );
	while( ( src < end ) && ( *src == 'Z' ) ) ++src;
	require_action( src == end, exit, err = kImmutableErr );
	
	// SNPrintF_Add
	
	dst = buf;
	lim = dst + 10;
	err = SNPrintF_Add( &dst, lim, "12345" );
	require_noerr( err, exit );
	require_action( strcmp( buf, "12345" ) == 0, exit, err = -1 );
	
	err = SNPrintF_Add( &dst, lim, "67890" );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( strcmp( buf, "123456789" ) == 0, exit, err = -1 );
	
	err = SNPrintF_Add( &dst, lim, "ABCDE" );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( strcmp( buf, "123456789" ) == 0, exit, err = -1 );
	
	
	dst = buf;
	lim = dst + 10;
	err = SNPrintF_Add( &dst, lim, "12345" );
	require_noerr( err, exit );
	require_action( strcmp( buf, "12345" ) == 0, exit, err = -1 );
	
	err = SNPrintF_Add( &dst, lim, "6789" );
	require_noerr( err, exit );
	require_action( strcmp( buf, "123456789" ) == 0, exit, err = -1 );
	
	err = SNPrintF_Add( &dst, lim, "ABCDE" );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( strcmp( buf, "123456789" ) == 0, exit, err = -1 );
	
	
	memcpy( buf, "abc", 4 );
	dst = buf;
	lim = dst;
	err = SNPrintF_Add( &dst, lim, "12345" );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( strcmp( lim, "abc" ) == 0, exit, err = -1 );
	
#if( COMPILER_OBJC )
@autoreleasepool
{
	require_action( [NSPrintF( "Test %d", 123 ) isEqual:@"Test 123"], exit, err = -1 );
	require_action( [NSPrintF( "abc" ) isEqual:@"abc"], exit, err = -1 );
	require_action( [NSPrintF( "%m", kNoErr ) isEqual:@"noErr"], exit, err = -1 );
}
#endif
	
	err = kNoErr;
	
exit:
	PrintFDeregisterExtension( "test1" );
	PrintFDeregisterExtension( "test2" );
	printf( "PrintFUtils: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	_PrintFTestExtension1
//===========================================================================================================================

static int	_PrintFTestExtension1( PrintFContext *ctx, PrintFFormat *inFormat, PrintFVAList *inArgs, void *inUserContext )
{
	int					a;
	const char *		b;
	
	(void) inUserContext;
	
	a = va_arg( inArgs->args, int );
	b = va_arg( inArgs->args, const char * );
	return( PrintFCore( ctx, "Alt=%d, a=%d, b='%s'", inFormat->altForm, a, b ) );
}

//===========================================================================================================================
//	_PrintFTestExtension2
//===========================================================================================================================

static int	_PrintFTestExtension2( PrintFContext *ctx, PrintFFormat *inFormat, PrintFVAList *inArgs, void *inUserContext )
{
	(void) inFormat;
	(void) inArgs;
	(void) inUserContext;
	
	return( PrintFCore( ctx, "NoArgsExtension" ) );
}

//===========================================================================================================================
//	_PrintFTestString
//===========================================================================================================================

static OSStatus	_PrintFTestString( const char *inMatch, const char *inFormat, ... )
{
	OSStatus			err;
	size_t				size;
	int					n;
	va_list				args;
	char				buf[ 512 ];
	const char *		src;
	const char *		end;
	
	memset( buf, 'Z', sizeof( buf ) );
	va_start( args, inFormat );
	n = VSNPrintF( buf, sizeof( buf ), inFormat, args );
	va_end( args );
	
#if( PRINTF_UTILS_PRINT_TEST )
	printf( "\"%.*s\"\n", (int) sizeof( buf ), buf );
#endif
	
	size = strlen( inMatch );
	require_action_quiet( n == (int) size, exit, err = kSizeErr );
	require_action_quiet( buf[ n ] == '\0', exit, err = kOverrunErr );
	require_action_quiet( strcmp( buf, inMatch ) == 0, exit, err = kMismatchErr );
	
	src = &buf[ n + 1 ];
	end = buf + sizeof( buf );
	while( ( src < end ) && ( *src == 'Z' ) ) ++src;
	require_action_quiet( src == end, exit, err = kImmutableErr );
	err = kNoErr;
	
exit:
	if( err ) printf( "### Bad PrintF output:\n'%s'\nExpected:\n'%s'", buf, inMatch );
	return( err );
}

//===========================================================================================================================
//	_PrintFTestVAList
//===========================================================================================================================

static OSStatus	_PrintFTestVAList( const char *inMatch, const char *inFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = _PrintFTestString( inMatch, "begin %V end", inFormat, &args );
	va_end( args );
	return( err );
}

#endif // !EXCLUDE_UNIT_TESTS
