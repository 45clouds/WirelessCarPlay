/*
	File:    	CFUtils.c
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
	
	Copyright (C) 2000-2016 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "CFUtils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "MiscUtils.h"
#include "PrintFUtils.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "TimeUtils.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER

#if( CFL_BINARY_PLISTS )
	#include "CFLiteBinaryPlist.h"
#endif

#if 0
#pragma mark == Formatted Building ==
#endif

//===========================================================================================================================
//	Formatted Building
//===========================================================================================================================

// CFPropertyListStack

typedef struct	CFPropertyListStack	CFPropertyListStack;
struct	CFPropertyListStack
{
	CFPropertyListStack *		next;
	CFMutableArrayRef			array;
	CFMutableDictionaryRef		dict;
};

// Prototypes

static OSStatus
	_CFPropertyListAssociateObject( 
		CFMutableArrayRef 		inArray, 
		CFMutableDictionaryRef 	inDict, 
		CFStringRef * 			ioKey, 
		CFTypeRef 				inObj, 
		CFTypeRef *				ioTopObject );

static OSStatus
	_CFPropertyListStackPush( 
		CFPropertyListStack **	ioStack, 
		CFMutableArrayRef 		inArray, 
		CFMutableDictionaryRef 	inDict );

static OSStatus
	_CFPropertyListStackPop( 
		CFPropertyListStack **		ioStack, 
		CFMutableArrayRef *			outArray, 
		CFMutableDictionaryRef *	outDict );

static void	_CFPropertyListStackFree( CFPropertyListStack *inStack );

//===========================================================================================================================
//	CFCreateF
//===========================================================================================================================

CFTypeRef	CFCreateF( OSStatus *outErr, const char *inFormat, ... )
{
	CFTypeRef		obj;
	va_list			args;
	
	va_start( args, inFormat );
	obj = CFCreateV( outErr, inFormat, args );
	va_end( args );
	return( obj );
}

//===========================================================================================================================
//	CFCreateV
//===========================================================================================================================

CFTypeRef	CFCreateV( OSStatus *outErr, const char *inFormat, va_list inArgs )
{
	CFMutableDictionaryRef		obj = NULL;
	OSStatus					err;
	
	err = CFPropertyListBuildFormatted( NULL, NULL, &obj, inFormat, inArgs );
	require_noerr( err, exit );
	
exit:
	if( outErr ) *outErr = err;
	return( obj );
}

//===========================================================================================================================
//	CFPropertyListCreateFormatted
//===========================================================================================================================

OSStatus	CFPropertyListCreateFormatted( CFAllocatorRef inAllocator, void *outObj, const char *inFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = CFPropertyListCreateFormattedVAList( inAllocator, outObj, inFormat, args );
	va_end( args );
	return( err );
}

//===========================================================================================================================
//	CFPropertyListCreateFormattedVAList
//===========================================================================================================================

OSStatus	CFPropertyListCreateFormattedVAList( CFAllocatorRef inAllocator, void *outObj, const char *inFormat, va_list inArgs )
{
	return( CFPropertyListBuildFormatted( inAllocator, NULL, outObj, inFormat, inArgs ) );
}

//===========================================================================================================================
//	CFPropertyListBuildFormatted
//
//	Spec		Description									Parameters
//	-----------	-------------------------------------------	----------
//	'['			Array begin									none
//	']'			Array end									none
//	'{'			Dictionary begin							none
//	'}'			Dictionary end								none
//	
//	<key>=		Inline key string							none
//	%ks=		Key string (null terminated)				const char *key
//	%.*ks=		Key string (variable size)					int size, const char *key
//	%.nks=		Key string (n characters)					const char *key
//	%kC=		Key string (FourCharCode)					uint32_t code (e.g. 'APPL')
//	%kO=		Key string (CF object)						CFStringRef key
//	%kU=		Key string (UUID)							uint8_t uuid[ 16 ] (big endian)
//
//	<value>;	Inline value string							none
//	%s			String (null terminated)					const char *string
//	%.*s		String (variable size)						int size, const char *string
//	%.ns		String (n characters)						const char *string
//	%C			String (FourCharCode)						uint32_t code (e.g. 'APPL')
//	%i			Integer										int x
//	%lli		64-bit Integer								int64_t x
//	%f			Floating-point value						double x
//	%D			Data										const void *data, int size
//	%#D			Data from binary plist						CFPropertyListRef inObj
//	%b			Boolean										int x (true/false)
//	%O			Object										CFPropertyListRef or NULL
//	%#O			Deep copy of object							CFPropertyListRef or NULL
//	%##O		Merge dictionary into dictionary			CFDictionaryRef
//	%.4a		String (IPv4: 1.2.3.4)						uint32_t *ipv4 (network byte order)
//	%.6a		String (MAC: 00:11:22:33:44:55)				uint8_t mac[ 6 ]
//	%.8a		String (Fibre: 00:11:22:33:44:55:66:77)		uint8_t addr[ 8 ]
//	%.16a		String (IPv6: fe80::217:f2ff:fec8:d6e7)		uint8_t ipv6[ 16 ]
//	%##a		String (IPv4, IPv6, etc. network address)	sockaddr *addr
//	%T			Date/Time									int year, int month, int day, int hour, int minute, int second
//	%U			UUID string									uint8_t uuid[ 16 ] (big endian)
//	%@			Receive current parent						CFTypeRef *outParent
//===========================================================================================================================

OSStatus
	CFPropertyListBuildFormatted( 
		CFAllocatorRef	inAllocator, 
		CFTypeRef 		inParent, 
		void *			outObj, 
		const char *	inFormat, 
		va_list 		inArgs )
{
	OSStatus					err;
	CFMutableArrayRef			array		= NULL; // Only 1 of array or dict will be used to track the container.
	CFMutableDictionaryRef		dict		= NULL; // Separate variables avoid the need for more costly type ID checks.
	CFPropertyListStack *		parentStack	= NULL;
	CFStringRef					key			= NULL;
	CFTypeRef					topObject	= NULL;
	CFTypeRef					obj			= NULL;
	CFTypeID					typeID;
	const unsigned char *		fmt;
	unsigned char				c;
	int							precision;
	int							altForm;
	int							longFlag;
	int							suppress;
	const unsigned char *		p;
	uint32_t					u32;
	int64_t						s64;
	double						d;
	unsigned char				buf[ 64 ];
	int							n;
	CFTypeRef *					objArg;
	
	require_action( inFormat, exit, err = kParamErr );
	
	// Set up and verify the parent array or dictionary (if one was passed in).
	
	if( inParent )
	{
		typeID = CFGetTypeID( inParent );
		if(      typeID == CFArrayGetTypeID() )			array = (CFMutableArrayRef) inParent;
		else if( typeID == CFDictionaryGetTypeID() )	dict  = (CFMutableDictionaryRef) inParent;
		else
		{
			dlogassert( "Parent must be an array, dictionary, or null (typeID=%d)", (int) typeID );
			err = kTypeErr;
			goto exit;
		}
	}
	
	// Parse the format string.
	
	fmt = (const unsigned char *) inFormat;
	for( c = *fmt; c != '\0'; c = *( ++fmt ) )
	{
		// Parse container specifiers.
		
		switch( c )
		{
			case '[':	// Array begin
				
				obj = CFArrayCreateMutable( inAllocator, 0, &kCFTypeArrayCallBacks );
				require_action( obj, exit, err = kNoMemoryErr );
				
				err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
				CFRelease( obj );
				require_noerr( err, exit );
				
				err = _CFPropertyListStackPush( &parentStack, array, dict );
				require_noerr( err, exit );
				
				array = (CFMutableArrayRef) obj;
				dict = NULL;
				continue;
			
			case ']':	// Array end
				
				err = _CFPropertyListStackPop( &parentStack, &array, &dict );
				require_noerr( err, exit );
				continue;
			
			case '{':	// Dictionary begin
				
				obj = CFDictionaryCreateMutable( inAllocator, 0, &kCFTypeDictionaryKeyCallBacks, 
					&kCFTypeDictionaryValueCallBacks );
				require_action( obj, exit, err = kNoMemoryErr );
				
				err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
				CFRelease( obj );
				require_noerr( err, exit );
				
				err = _CFPropertyListStackPush( &parentStack, array, dict );
				require_noerr( err, exit );
				
				array = NULL;
				dict = (CFMutableDictionaryRef) obj;
				continue;
			
			case '}':	// Dictionary end
				
				err = _CFPropertyListStackPop( &parentStack, &array, &dict );
				require_noerr( err, exit );
				continue;
			
			default:	// Non-container specifier
				break;
		}
				
		// Parse inline key/value strings.
		
		if( c != '%' )
		{
			p = fmt;
			for( ; ( c != '\0' ) && ( c != '=' ) && ( c != ';' ); c = *( ++fmt ) ) {}
			if( c == '=' )
			{
				require_action( !key, exit, err = kMalformedErr );
				require_action( dict, exit, err = kMalformedErr );
				
				key = CFStringCreateWithBytes( inAllocator, p, (CFIndex)( fmt - p ), kCFStringEncodingUTF8, false );
				require_action( key, exit, err = kNoMemoryErr );
			}
			else
			{
				obj = CFStringCreateWithBytes( inAllocator, p, (CFIndex)( fmt - p ), kCFStringEncodingUTF8, false );
				require_action( obj, exit, err = kNoMemoryErr );
				
				err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
				CFRelease( obj );
				require_noerr( err, exit );
				
				if( c == '\0' ) break;
			}
			continue;
		}
		
		// Parse flags.
		
		altForm  = 0;
		longFlag = 0;
		suppress = 0;
		for( ;; )
		{
			c = *( ++fmt );
			if(       c == '#' ) ++altForm;
			else if ( c == 'l' ) ++longFlag;
			else if ( c == '?' ) suppress = !va_arg( inArgs, int );
			else break;
		}
		
		// Parse the precision.
		
		precision = -1;
		if( c == '.' )
		{
			c = *( ++fmt );
			if( c == '*' )
			{
				precision = va_arg( inArgs, int );
				require_action( precision >= 0, exit, err = kSizeErr );
				c = *( ++fmt );
			}
			else
			{
				precision = 0;
				for( ; isdigit( c ); c = *( ++fmt ) ) precision = ( precision * 10 ) + ( c - '0' );
				require_action( precision >= 0, exit, err = kSizeErr );
			}
		}
		
		// Parse key specifiers.
		
		if( c == 'k' )
		{
			require_action( !key, exit, err = kMalformedErr );
			require_action( dict, exit, err = kMalformedErr );
			require_action( !suppress, exit, err = kUnsupportedErr );
			
			c = *( ++fmt );
			switch( c )
			{
				case 's':	// %ks: Key String (e.g. "<key>my-key</key>")
				
					p = va_arg( inArgs, const unsigned char * );
					require_action( p, exit, err = kParamErr );
					
					if( precision >= 0 )
					{
						precision = (int) strnlen( (const char *) p, (size_t) precision );
						key = CFStringCreateWithBytes( inAllocator, p, precision, kCFStringEncodingUTF8, false );
						require_action( key, exit, err = kNoMemoryErr );
					}
					else
					{
						key = CFStringCreateWithCString( inAllocator, (const char *) p, kCFStringEncodingUTF8 );
						require_action( key, exit, err = kNoMemoryErr );
					}
					break;
				
				case 'C':	// %kC: Key FourCharCode (e.g. "<key>AAPL</key>")
					
					require_action( precision == -1, exit, err = kFormatErr );
					
					u32 = va_arg( inArgs, uint32_t );
					buf[ 0 ] = (unsigned char)( ( u32 >> 24 ) & 0xFF );
					buf[ 1 ] = (unsigned char)( ( u32 >> 16 ) & 0xFF );
					buf[ 2 ] = (unsigned char)( ( u32 >>  8 ) & 0xFF );
					buf[ 3 ] = (unsigned char)(   u32         & 0xFF );
					key = CFStringCreateWithBytes( inAllocator, buf, 4, kCFStringEncodingMacRoman, false );
					require_action( key, exit, err = kNoMemoryErr );
					break;
				
				case 'o':	// %kO: Key Object (e.g. "<key>my-key</key>")
				case 'O':
					
					require_action( precision == -1, exit, err = kFormatErr );
					
					key = va_arg( inArgs, CFStringRef );
					require_action( key, exit, err = kParamErr );
					
					CFRetain( key );
					break;
				
				case 'U':	// %kU: UUID Key UUID string (e.g. "<key>8129b4b2-86dd-4f40-951f-6be834da5b8e</key>")
					
					require_action( precision == -1, exit, err = kFormatErr );
					
					p = va_arg( inArgs, const unsigned char * );
					require_action( p, exit, err = kParamErr );
					
					key = CFStringCreateWithCString( NULL, UUIDtoCString( p, 0, buf ), kCFStringEncodingUTF8 );
					require_action( key, exit, err = kNoMemoryErr );
					break;
				
				default:
					dlogassert( "Unknown key specifier: %s", fmt );
					err = kFormatErr;
					goto exit;
				
			}
			c = *( ++fmt );
			require_action( c == '=', exit, err = kFormatErr );
			continue;
		}
		
		// Parse value specifiers.
		
		switch( c )
		{
			case 's':	// %s: String (e.g. "<string>my-string</string>")
								
				p = va_arg( inArgs, const unsigned char * );
				if( !p || suppress ) { ForgetCF( &key ); break; }
				
				if( precision >= 0 )
				{
					precision = (int) strnlen( (const char *) p, (size_t) precision );
					obj = CFStringCreateWithBytes( inAllocator, p, precision, kCFStringEncodingUTF8, false );
					require_action( obj, exit, err = kNoMemoryErr );
				}
				else
				{
					obj = CFStringCreateWithCString( inAllocator, (const char *) p, kCFStringEncodingUTF8 );
					require_action( obj, exit, err = kNoMemoryErr );
				}
				err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
				CFRelease( obj );
				require_noerr( err, exit );
				break;

			case 'C':	// %C: FourCharCode (e.g. "<string>AAPL</string>")
				
				require_action( precision == -1, exit, err = kFormatErr );
				
				u32 = va_arg( inArgs, uint32_t );
				if( suppress ) { ForgetCF( &key ); break; }
				buf[ 0 ] = (unsigned char)( ( u32 >> 24 ) & 0xFF );
				buf[ 1 ] = (unsigned char)( ( u32 >> 16 ) & 0xFF );
				buf[ 2 ] = (unsigned char)( ( u32 >>  8 ) & 0xFF );
				buf[ 3 ] = (unsigned char)(   u32         & 0xFF );
				obj = CFStringCreateWithBytes( inAllocator, buf, 4, kCFStringEncodingMacRoman, false );
				require_action( obj, exit, err = kNoMemoryErr );
								
				err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
				CFRelease( obj );
				require_noerr( err, exit );
				break;

			case 'i':	// %i: Integer (e.g. "<integer>-3</integer>")
				require_action( precision == -1, exit, err = kFormatErr );
				
				check( ( longFlag == 0 ) || ( longFlag == 2 ) );
				if( longFlag == 2 )	s64 = va_arg( inArgs, int64_t );
				else				s64 = va_arg( inArgs, int );
				if( suppress ) { ForgetCF( &key ); break; }
				obj = CFNumberCreateInt64( s64 );
				require_action( obj, exit, err = kNoMemoryErr );
				
				err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
				CFRelease( obj );
				require_noerr( err, exit );
				break;
			
			case 'f':	// %f: Floating-point value (e.g. "<real>3.14</real>")
				require_action( precision == -1, exit, err = kFormatErr );
				
				d = va_arg( inArgs, double );
				if( suppress ) { ForgetCF( &key ); break; }
				obj = CFNumberCreate( inAllocator, kCFNumberDoubleType, &d );
				require_action( obj, exit, err = kNoMemoryErr );
				
				err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
				CFRelease( obj );
				require_noerr( err, exit );
				break;
			
			case 'd':	// %D:  Data (e.g. "<data>ABEiMw==</data>")
			case 'D':	// %#D: Data from binary plist.
				
				require_action( precision == -1, exit, err = kFormatErr );
				
				if( altForm == 0 )
				{
					p = va_arg( inArgs, const unsigned char * );
					n = va_arg( inArgs, int );
					if( suppress ) { ForgetCF( &key ); break; }
					require_action( p || ( n == 0 ), exit, err = kParamErr );
					obj = CFDataCreate( inAllocator, p, n );
					require_action( obj, exit, err = kNoMemoryErr );
				}
				else
				{
					obj = va_arg( inArgs, CFTypeRef );
					if( !obj || suppress ) { ForgetCF( &key ); break; }
					
					obj = CFPropertyListCreateData( NULL, obj, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
					require_action( obj, exit, err = kUnsupportedDataErr );
				}
				
				err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
				CFRelease( obj );
				require_noerr( err, exit );
				break;

			case 'b':	// %b: Boolean (e.g. "<true/>")
				
				require_action( precision == -1, exit, err = kFormatErr );
				
				n = va_arg( inArgs, int );
				if( suppress ) { ForgetCF( &key ); break; }
				err = _CFPropertyListAssociateObject( array, dict, &key, n ? kCFBooleanTrue : kCFBooleanFalse, &topObject );
				require_noerr( err, exit );
				break;

			case 'o':	// %O: Object
			case 'O':
				
				require_action( precision == -1, exit, err = kFormatErr );
				
				obj = va_arg( inArgs, CFTypeRef );
				if( !obj || suppress ) { ForgetCF( &key ); break; }
				
				if( altForm == 1 )
				{
					obj = CFPropertyListCreateDeepCopy( inAllocator, obj, kCFPropertyListMutableContainersAndLeaves );
					require_action( obj, exit, err = kNoMemoryErr );
					
					err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
					CFRelease( obj );
					require_noerr( err, exit );
				}
				else if( altForm == 2 )
				{
					require_action( !key, exit, err = kParamErr );
					require_action( dict, exit, err = kParamErr );
					require_action( CFGetTypeID( obj ) == CFDictionaryGetTypeID(), exit, err = kTypeErr );
					
					err = CFDictionaryMergeDictionary( dict, (CFDictionaryRef) obj );
					require_noerr( err, exit );
				}
				else
				{
					err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
					require_noerr( err, exit );
				}
				break;
						
			case 'a':	// %a: Address String (%.4a=IPv4, %.6a=Ethernet, %.8a=Fibre Channel)
				
				p = va_arg( inArgs, const unsigned char * );
				if( !p || suppress ) { ForgetCF( &key ); break; }
				
				if( altForm == 2 )
				{
					n = SNPrintF( (char *) buf, sizeof( buf ), "%##a", p );
					require_action( n > 0, exit, err = kUnknownErr );
				}
				else
				{
					n = SNPrintF( (char *) buf, sizeof( buf ), "%.*a", precision, p );
					require_action( n > 0, exit, err = kUnknownErr );
				}
				
				obj = CFStringCreateWithBytes( inAllocator, buf, n, kCFStringEncodingUTF8, false );
				require_action( obj, exit, err = kNoMemoryErr );
								
				err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
				CFRelease( obj );
				require_noerr( err, exit );
				break;
			
			case 'T':	// %T: Date/Time (e.g. "<date>2006-05-26T02:37:16Z</date>")
			{
				int		year, month, day, hour, minute, second;
				
				require_action( precision == -1, exit, err = kFormatErr );
				
				year	= va_arg( inArgs, int );
				month	= va_arg( inArgs, int );
				day		= va_arg( inArgs, int );
				hour	= va_arg( inArgs, int );
				minute	= va_arg( inArgs, int );
				second	= va_arg( inArgs, int );
				if( suppress ) { ForgetCF( &key ); break; }
				obj = CFDateCreateWithComponents( inAllocator, year, month, day, hour, minute, second );
				require_action( obj, exit, err = kNoMemoryErr );
				
				err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
				CFRelease( obj );
				require_noerr( err, exit );
				break;
			}
			
			case 'U':	// %U: UUID string (e.g. "<string>8129b4b2-86dd-4f40-951f-6be834da5b8e</string>")
				
				p = va_arg( inArgs, const unsigned char * );
				if( !p || suppress ) { ForgetCF( &key ); break; }
				require_action( p, exit, err = kParamErr );
				
				obj = CFStringCreateWithCString( NULL, UUIDtoCString( p, 0, buf ), kCFStringEncodingUTF8 );
				require_action( obj, exit, err = kNoMemoryErr );
				
				err = _CFPropertyListAssociateObject( array, dict, &key, obj, &topObject );
				CFRelease( obj );
				require_noerr( err, exit );
				break;
			
			case '@':	// %@: Receive current parent
				
				require_action( precision == -1, exit, err = kFormatErr );
				
				objArg = va_arg( inArgs, CFTypeRef * );
				require_action( objArg, exit, err = kParamErr );
				
				if( array )		obj = array;
				else if( dict )	obj = dict;
				else			obj = NULL;
				*objArg = obj;
				break;
			
			default:
				dlogassert( "Unknown format char: %s", fmt );
				err = kFormatErr;
				goto exit;
		}
	}
	
	// Success!
	
	if( outObj )
	{
		*( (CFTypeRef *) outObj ) = topObject;
		topObject = NULL;
	}
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( key );
	
	// We only need to release a non-contained object (i.e. a top-level object). If a parent was passed in, any object(s)
	// created by this routine would either be released above or attached to the parent so they don't need to be released.
	
	if( !inParent && topObject ) CFRelease( topObject );
	_CFPropertyListStackFree( parentStack );
	return( err );
}

//===========================================================================================================================
//	_CFPropertyListAssociateObject
//===========================================================================================================================

static OSStatus
	_CFPropertyListAssociateObject( 
		CFMutableArrayRef 		inArray, 
		CFMutableDictionaryRef 	inDict, 
		CFStringRef * 			ioKey, 
		CFTypeRef 				inObj, 
		CFTypeRef *				ioTopObject )
{
	OSStatus		err;
	
	check( ( !inArray && !inDict ) || ( !inArray != !inDict ) );
	check( ioKey );
	check( ( *ioKey || !inDict ) && ( !( *ioKey ) || inDict ) );
	check( inObj );
	check( ioTopObject );
	check( inArray || inDict || ( ioTopObject && !( *ioTopObject ) ) );
	
	if( inArray )
	{
		CFArrayAppendValue( inArray, inObj );
	}
	else if( inDict )
	{
		require_action( *ioKey, exit, err = kMalformedErr );
		
		CFDictionarySetValue( inDict, *ioKey, inObj );
		CFRelease( *ioKey );
		*ioKey = NULL;
	}
	else
	{
		// No parent so retain to keep the object around.
		
		CFRetain( inObj );
	}
	
	// Only set the top object if it has not been set yet.
	
	if( !( *ioTopObject ) ) *ioTopObject = inObj;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CFPropertyListStackPush
//===========================================================================================================================

static OSStatus
	_CFPropertyListStackPush( 
		CFPropertyListStack **	ioStack, 
		CFMutableArrayRef 		inArray, 
		CFMutableDictionaryRef 	inDict )
{
	OSStatus					err;
	CFPropertyListStack *		node;
	
	check( ioStack );
	
	node = (CFPropertyListStack *) calloc( 1U, sizeof( *node ) );
	require_action( node, exit, err = kNoMemoryErr );
	
	node->next 		= *ioStack;
	node->array		= inArray;
	node->dict		= inDict;
	*ioStack		= node;
	err 			= kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CFPropertyListStackPop
//===========================================================================================================================

static OSStatus
	_CFPropertyListStackPop( 
		CFPropertyListStack **		ioStack, 
		CFMutableArrayRef *			outArray, 
		CFMutableDictionaryRef *	outDict )
{
	OSStatus					err;
	CFPropertyListStack *		node;
	
	check( ioStack );
	check( outArray );
	check( outDict );
	
	node = *ioStack;
	require_action( node, exit, err = kMalformedErr );
	
	*ioStack 	= node->next;
	*outArray 	= node->array;
	*outDict 	= node->dict;
	free( node );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CFPropertyListStackFree
//===========================================================================================================================

static void	_CFPropertyListStackFree( CFPropertyListStack *inStack )
{
	while( inStack )
	{
		CFPropertyListStack *		nextNode;
		
		nextNode = inStack->next;
		free( inStack );
		inStack = nextNode;
	}
}

#if 0
#pragma mark -
#pragma mark == Serialization ==
#endif

//===========================================================================================================================
//	CFDictionaryCreateWithINIBytes
//===========================================================================================================================

CFMutableDictionaryRef
	CFDictionaryCreateWithINIBytes( 
		const void *	inPtr, 
		size_t			inLen, 
		uint32_t		inFlags, 
		CFStringRef		inSectionNameKey, 
		OSStatus *		outErr )
{
	const char *				src				= (const char *) inPtr;
	const char * const			end				= src + inLen;
	CFMutableDictionaryRef		result			= NULL;
	CFMutableDictionaryRef		rootDict		= NULL;
	CFMutableDictionaryRef		sectionDict		= NULL;
	CFStringRef					cfstr			= NULL;
	OSStatus					err;
	uint32_t					flags;
	const char *				namePtr;
	size_t						nameLen;
	const char *				valuePtr;
	size_t						valueLen;
	CFMutableArrayRef			sectionsArray;
	CFMutableDictionaryRef		sectionsDict;
	
	rootDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( rootDict, exit, err = kNoMemoryErr );
	
	while( INIGetNext( src, end, &flags, &namePtr, &nameLen, &valuePtr, &valueLen, &src ) )
	{
		if( flags & kINIFlag_Section )
		{
			// Set up a new section dictionary. Optionally use the value (e.g. value in [name "value"]) as the name.
			
			CFReleaseNullSafe( sectionDict );
			sectionDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, 
				&kCFTypeDictionaryValueCallBacks );
			require_action( sectionDict, exit, err = kNoMemoryErr );
			if( valuePtr && inSectionNameKey && ( inSectionNameKey != kINISectionDotted ) && 
				( inSectionNameKey != kINISectionNested ) )
			{
				CFDictionarySetCString( sectionDict, inSectionNameKey, valuePtr, valueLen );
			}
			if( valuePtr && ( inSectionNameKey == kINISectionDotted ) )
			{
				cfstr = CFStringCreateWithFormat( NULL, NULL, CFSTR( "%.*s.%.*s" ), (int) nameLen, namePtr, 
					(int) valueLen, valuePtr );
				require_action( cfstr, exit, err = kUnknownErr );
			}
			else
			{
				cfstr = CFStringCreateWithBytes( NULL, (const uint8_t *) namePtr, (CFIndex) nameLen, 
					kCFStringEncodingUTF8, false );
				require_action( cfstr, exit, err = kUnknownErr );
			}
			
			// Sections dictionaries are grouped into either arrays or dictionaries based on options.
			
			if( inSectionNameKey == kINISectionNested )
			{
				sectionsDict = (CFMutableDictionaryRef) CFDictionaryGetValue( rootDict, cfstr );
				if( !sectionsDict )
				{
					sectionsDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, 
						&kCFTypeDictionaryValueCallBacks );
					require_action( sectionsDict, exit, err = kNoMemoryErr );
					CFDictionarySetValue( rootDict, cfstr, sectionsDict );
					CFRelease( sectionsDict );
				}
				
				CFRelease( cfstr );
				cfstr = CFStringCreateWithBytes( NULL, (const uint8_t *) valuePtr, (CFIndex) valueLen, 
					kCFStringEncodingUTF8, false );
				require_action( cfstr, exit, err = kUnknownErr );
				CFDictionarySetValue( sectionsDict, cfstr, sectionDict );
				CFRelease( cfstr );
				cfstr = NULL;
			}
			else if( inSectionNameKey == kINISectionDotted )
			{
				CFDictionarySetValue( rootDict, cfstr, sectionDict );
				CFRelease( cfstr );
				cfstr = NULL;
			}
			else
			{
				sectionsArray = (CFMutableArrayRef) CFDictionaryGetValue( rootDict, cfstr );
				if( sectionsArray )
				{
					CFArrayAppendValue( sectionsArray, sectionDict );
				}
				else
				{
					sectionsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
					require_action( sectionsArray, exit, err = kNoMemoryErr );
					CFArrayAppendValue( sectionsArray, sectionDict );
					CFDictionarySetValue( rootDict, cfstr, sectionsArray );
					CFRelease( sectionsArray );
				}
				CFRelease( cfstr );
				cfstr = NULL;
			}
		}
		else
		{
			// If there's no section dictionary yet, we're processing global properties so set up a virtual section for them.
			
			if( !sectionDict && !( inFlags & kINIFlag_MergeGlobals ) )
			{
				sectionDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
				require_action( sectionDict, exit, err = kNoMemoryErr );
				CFDictionarySetValue( rootDict, CFSTR( kINISectionType_Global ), sectionDict );
			}
			
			cfstr = CFStringCreateWithBytes( NULL, (const uint8_t *) namePtr, (CFIndex) nameLen, kCFStringEncodingUTF8, false );
			require_action( cfstr, exit, err = kUnknownErr );
			CFDictionarySetCString( sectionDict ? sectionDict : rootDict, cfstr, valuePtr, valueLen );
			CFRelease( cfstr );
			cfstr = NULL;
		}
	}
	
	result = rootDict;
	rootDict = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( cfstr );
	CFReleaseNullSafe( sectionDict );
	CFReleaseNullSafe( rootDict );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	CFPropertyListCreateBytes
//===========================================================================================================================

OSStatus	CFPropertyListCreateBytes( CFPropertyListRef inPlist, CFPropertyListFormat inFormat, uint8_t **outPtr, size_t *outLen )
{
	OSStatus		err;
	CFDataRef		data;
	size_t			len;
	uint8_t *		ptr;
	
	data = CFPropertyListCreateData( NULL, inPlist, inFormat, 0, NULL );
	require_action( data, exit, err = kUnknownErr );
	
	len = (size_t) CFDataGetLength( data );
	ptr = (uint8_t *) malloc( len );
	require_action( ptr, exit, err = kUnknownErr );
	memcpy( ptr, CFDataGetBytePtr( data ), len );
	
	*outPtr = ptr;
	*outLen = len;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( data );
	return( err );
}

//===========================================================================================================================
//	CFPropertyListCreateFromANSIFile
//===========================================================================================================================

OSStatus	CFPropertyListCreateFromANSIFile( FILE *inFile, CFOptionFlags inOptions, CFPropertyListRef *outPlist )
{
	OSStatus				err;
	CFPropertyListRef		plist;
	CFDataRef				data;
	
	data = CFDataCreateWithANSIFile( inFile, &err );
	require_noerr_quiet( err, exit );
	
	plist = CFPropertyListCreateWithData( NULL, data, inOptions, NULL, NULL );
	CFRelease( data );
	require_action_quiet( plist, exit, err = kFormatErr );
	
	*outPlist = plist;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFPropertyListCreateFromFilePath
//===========================================================================================================================

CFTypeRef	CFPropertyListCreateFromFilePath( const char *inPath, CFOptionFlags inOptions, OSStatus *outErr )
{
	CFTypeRef		plist = NULL;
	CFDataRef		data;
	OSStatus		err;
	
	data = CFDataCreateWithFilePath( inPath, &err );
	require_noerr_quiet( err, exit );
	
	plist = CFPropertyListCreateWithData( NULL, data, inOptions, NULL, NULL );
	CFRelease( data );
	require_action_quiet( plist, exit, err = kFormatErr );
	
exit:
	if( outErr ) *outErr = err;
	return( plist );
}

//===========================================================================================================================
//	CFPropertyListWriteToFilePath / CFPropertyListWriteToANSIFile
//===========================================================================================================================

static OSStatus
	CFPropertyListWriteToFilePathEx( 
		CFPropertyListRef	inPlist, 
		const char *		inFormat, 
		const char *		inPath, 
		FILE *				inFile );

OSStatus	CFPropertyListWriteToFilePath( CFPropertyListRef inPlist, const char *inFormat, const char *inPath )
{
	return( CFPropertyListWriteToFilePathEx( inPlist, inFormat, inPath, NULL ) );
}

OSStatus	CFPropertyListWriteToANSIFile( CFPropertyListRef inPlist, const char *inFormat, FILE *inFile )
{
	return( CFPropertyListWriteToFilePathEx( inPlist, inFormat, NULL, inFile ) );
}

static OSStatus
	CFPropertyListWriteToFilePathEx( 
		CFPropertyListRef	inPlist, 
		const char *		inFormat, 
		const char *		inPath, 
		FILE *				inFile )
{
	OSStatus			err;
	CFDataRef			data;
	FILE *				file;
	CFTypeID			typeID;
	const char *		utf8;
	char *				utf8Storage;
	size_t				stringLen;
	CFIndex				dataLen;
	size_t				nWrote;
	
	file		= NULL;
	data		= NULL;
	utf8Storage = NULL;
	
	// If the format is prefixed with "raw-" and it's a type that supports it then write it out raw.
	
	if( strncmp_prefix( inFormat, SIZE_MAX, "raw-" ) == 0 )
	{
		typeID = CFGetTypeID( inPlist );
		if( typeID == CFStringGetTypeID() )
		{
			err = CFStringGetOrCopyCStringUTF8( (CFStringRef) inPlist, &utf8, &utf8Storage, &stringLen );
			require_noerr( err, exit );
			
			if( !inFile )
			{
				require_action( inPath, exit, err = kPathErr );
				file = fopen( inPath, "wb" );
				err = map_global_value_errno( file, file  );
				require_noerr( err, exit );
				inFile = file;
			}
			
			nWrote = fwrite( utf8, 1, stringLen, inFile );
			err = map_global_value_errno( nWrote == stringLen, inFile );
			require_noerr( err, exit );
			goto exit;
		}
		else if( typeID == CFDataGetTypeID() )
		{
			if( !inFile )
			{
				require_action( inPath, exit, err = kPathErr );
				file = fopen( inPath, "wb" );
				err = map_global_value_errno( file, file );
				require_noerr( err, exit );
				inFile = file;
			}
			
			dataLen = CFDataGetLength( (CFDataRef) inPlist );
			nWrote = fwrite( CFDataGetBytePtr( (CFDataRef) inPlist ), 1, (size_t) dataLen, inFile );
			err = map_global_value_errno( nWrote == (size_t) dataLen, inFile );
			require_noerr( err, exit );
			goto exit;
		}
		else
		{
			inFormat += sizeof_string( "raw-" ); // Skip "raw-" to point to the alternate format.
		}
	}
	if( 0 ) {} // Empty if to simplify conditionalize code below.
#if( TARGET_OS_DARWIN || CFL_BINARY_PLISTS )
	else if( strcmp( inFormat, "binary1" ) == 0 )
	{
		data = CFPropertyListCreateData( NULL, inPlist, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
		require_action( data, exit, err = kUnknownErr );
	}
#endif
	else
	{
		dlogassert( "unknown format: '%s'\n", inFormat );
		err = kUnsupportedErr;
		goto exit;
	}
	
	if( !inFile )
	{
		require_action( inPath, exit, err = kPathErr );
		file = fopen( inPath, "wb" );
		err = map_global_value_errno( file, file );
		require_noerr( err, exit );
		inFile = file;
	}
	
	dataLen = CFDataGetLength( data );
	nWrote = fwrite( CFDataGetBytePtr( data ), 1, (size_t) dataLen, inFile );
	err = map_global_value_errno( nWrote == (size_t) dataLen, inFile );
	require_noerr( err, exit );
	
exit:
	if( utf8Storage )	free( utf8Storage );
	if( data )			CFRelease( data );
	if( file )			fclose( file );
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Object Accessors ==
#endif

//===========================================================================================================================
//	CFObjectControlAsync
//===========================================================================================================================

typedef struct
{
	CFTypeRef						object;
	CFObjectControlFunc				func;
	CFObjectFlags					flags;
	CFStringRef						command;
	CFTypeRef						qualifier;
	CFDictionaryRef					params;
	OSStatus						error;
	CFDictionaryRef					response;
	dispatch_queue_t				responseQueue;
	CFObjectControlResponseFunc		responseFunc;
	void *							responseContext;
	
}	CFObjectControlAsyncParams;

static void _CFObjectControlAsync( void *inArg );
static void _CFObjectControlResponse( void *inArg );

OSStatus
	CFObjectControlAsync( 
		CFTypeRef					inObject,
		dispatch_queue_t			inQueue, 
		CFObjectControlFunc			inFunc, 
		CFObjectFlags				inFlags, 
		CFStringRef					inCommand, 
		CFTypeRef					inQualifier, 
		CFDictionaryRef				inParams, 
		dispatch_queue_t			inResponseQueue, 
		CFObjectControlResponseFunc	inResponseFunc, 
		void *						inResponseContext )
{
	OSStatus							err;
	CFObjectControlAsyncParams *		params;
	
	params = (CFObjectControlAsyncParams *) malloc( sizeof( *params ) );
	require_action( params, exit, err = kNoMemoryErr );
	
	CFRetain( inObject );
	params->object	= inObject;
	params->func	= inFunc;
	params->flags	= inFlags;
	
	CFRetain( inCommand );
	params->command = inCommand;
	
	CFRetainNullSafe( inQualifier );
	params->qualifier = inQualifier;
	
	CFRetainNullSafe( inParams );
	params->params   = inParams;
	
	params->response = NULL;
	if( inResponseQueue ) arc_safe_dispatch_retain( inResponseQueue );
	params->responseQueue	= inResponseQueue;
	params->responseFunc    = inResponseFunc;
	params->responseContext = inResponseContext;
	
	dispatch_async_f( inQueue, params, _CFObjectControlAsync );
	err = kNoErr;
	
exit:
	return( err );
}

static void _CFObjectControlAsync( void *inArg )
{
	CFObjectControlAsyncParams *		params = (CFObjectControlAsyncParams *) inArg;
	
	params->error = params->func( params->object, params->flags, params->command, params->qualifier, params->params, 
		&params->response );
	CFRelease( params->object );
	CFRelease( params->command );
	CFReleaseNullSafe( params->qualifier );
	CFReleaseNullSafe( params->params );
	
	if( params->responseFunc )
	{
		if( params->responseQueue )
		{
			dispatch_async_f( params->responseQueue, params, _CFObjectControlResponse );
			params = NULL;
		}
		else
		{
			params->responseFunc( params->error, params->response, params->responseContext );
		}
	}
	else if( params->response )
	{
		dlog( kLogLevelNotice, "### Async control with no completion ignored response %@\n", params->response );
	}
	if( params )
	{
		CFReleaseNullSafe( params->response );
		if( params->responseQueue ) arc_safe_dispatch_release( params->responseQueue );
		free( params );
	}
}

static void _CFObjectControlResponse( void *inArg )
{
	CFObjectControlAsyncParams * const		params = (CFObjectControlAsyncParams *) inArg;
	
	params->responseFunc( params->error, params->response, params->responseContext );
	CFReleaseNullSafe( params->response );
	arc_safe_dispatch_release( params->responseQueue );
	free( params );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CFObjectCopyProperty
//===========================================================================================================================

typedef struct
{
	CFTypeRef					object;
	CFObjectCopyPropertyFunc	func;
	CFObjectFlags				flags;
	CFStringRef					property;
	CFTypeRef					qualifier;
	CFTypeRef					value;
	OSStatus *					errorPtr;
	
}	CFObjectCopyPropertyParams;

static void	_CFObjectCopyProperty( void *inContext );

CFTypeRef
	CFObjectCopyProperty( 
		CFTypeRef					inObject,
		dispatch_queue_t			inQueue, 
		CFObjectCopyPropertyFunc	inFunc, 
		CFObjectFlags				inFlags, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr )
{
	if( !( inFlags & kCFObjectFlagDirect ) )
	{
		CFObjectCopyPropertyParams		params = { inObject, inFunc, inFlags, inProperty, inQualifier, NULL, outErr };
		
		dispatch_sync_f( inQueue, &params, _CFObjectCopyProperty );
		return( params.value );
	}
	return( inFunc( inObject, inFlags, inProperty, inQualifier, outErr ) );
}

static void	_CFObjectCopyProperty( void *inContext )
{
	CFObjectCopyPropertyParams * const		params = (CFObjectCopyPropertyParams *) inContext;
	
	params->value = params->func( params->object, params->flags, params->property, params->qualifier, params->errorPtr );
}

//===========================================================================================================================
//	CFObjectGetPropertyCStringSync
//===========================================================================================================================

char *
	CFObjectGetPropertyCStringSync( 
		CFTypeRef					inObject, 
		dispatch_queue_t			inQueue, 
		CFObjectCopyPropertyFunc	inFunc, 
		CFObjectFlags				inFlags, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		char *						inBuf, 
		size_t						inMaxLen, 
		OSStatus *					outErr )
{
	char *			value;
	CFTypeRef		cfValue;
	
	cfValue = CFObjectCopyProperty( inObject, inQueue, inFunc, inFlags, inProperty, inQualifier, outErr );
	if( cfValue )
	{	
		value = CFGetCString( cfValue, inBuf, inMaxLen );
		CFRelease( cfValue );
		return( value );
	}
	return( NULL );
}

//===========================================================================================================================
//	CFObjectGetPropertyDoubleSync
//===========================================================================================================================

double
	CFObjectGetPropertyDoubleSync( 
		CFTypeRef					inObject, 
		dispatch_queue_t			inQueue, 
		CFObjectCopyPropertyFunc	inFunc, 
		CFObjectFlags				inFlags, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr )
{
	double			value;
	CFTypeRef		cfValue;
	
	cfValue = CFObjectCopyProperty( inObject, inQueue, inFunc, inFlags, inProperty, inQualifier, outErr );
	if( cfValue )
	{
		value = CFGetDouble( cfValue, outErr );
		CFRelease( cfValue );
		return( value );
	}
	return( 0 );
}

//===========================================================================================================================
//	CFObjectGetPropertyInt64Sync
//===========================================================================================================================

int64_t
	CFObjectGetPropertyInt64Sync( 
		CFTypeRef					inObject, 
		dispatch_queue_t			inQueue, 
		CFObjectCopyPropertyFunc	inFunc, 
		CFObjectFlags				inFlags, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr )
{
	int64_t			value;
	CFTypeRef		cfValue;
	
	cfValue = CFObjectCopyProperty( inObject, inQueue, inFunc, inFlags, inProperty, inQualifier, outErr );
	if( cfValue )
	{
		value = CFGetInt64( cfValue, outErr );
		CFRelease( cfValue );
		return( value );
	}
	return( 0 );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CFObjectSetProperty
//===========================================================================================================================

typedef struct
{
	CFTypeRef					object;
	CFObjectSetPropertyFunc		func;
	CFObjectFlags				flags;
	CFStringRef					property;
	CFTypeRef					qualifier;
	CFTypeRef					value;
	OSStatus					error;
	
}	CFObjectSetPropertyParams;

static void	_CFObjectSetProperty( void *inContext );

OSStatus
	CFObjectSetProperty( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		CFTypeRef				inValue )
{
	OSStatus		err;
	
	if( inFlags & kCFObjectFlagDirect )
	{
		err = inFunc( inObject, inFlags, inProperty, inQualifier, inValue );
	}
	else if( inFlags & kCFObjectFlagAsync )
	{
		CFObjectSetPropertyParams *		params;
		
		params = (CFObjectSetPropertyParams *) malloc( sizeof( *params ) );
		require_action( params, exit, err = kNoMemoryErr );
		
		CFRetain( inObject );
		params->object = inObject;
		
		params->func  = inFunc;
		params->flags = inFlags;
		
		CFRetain( inProperty );
		params->property = inProperty;
		
		CFRetainNullSafe( inQualifier );
		params->qualifier = inQualifier;
		
		CFRetainNullSafe( inValue );
		params->value = inValue;
		
		dispatch_async_f( inQueue, params, _CFObjectSetProperty );
		err = kNoErr;
	}
	else
	{
		CFObjectSetPropertyParams		localParams = { inObject, inFunc, inFlags, inProperty, inQualifier, inValue, kUnknownErr };
		
		dispatch_sync_f( inQueue, &localParams, _CFObjectSetProperty );
		err = localParams.error;
	}
	
exit:
	return( err );
}

static void	_CFObjectSetProperty( void *inContext )
{
	CFObjectSetPropertyParams * const		params = (CFObjectSetPropertyParams *) inContext;
	
	params->error = params->func( params->object, params->flags, params->property, params->qualifier, params->value );
	if( params->flags & kCFObjectFlagAsync )
	{
		CFRelease( params->object );
		CFRelease( params->property );
		CFReleaseNullSafe( params->qualifier );
		CFReleaseNullSafe( params->value );
		free( params );
	}
}

//===========================================================================================================================
//	CFObjectSetPropertyF
//===========================================================================================================================

OSStatus
	CFObjectSetPropertyF( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		const char *			inFormat, 
		... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = CFObjectSetPropertyV( inObject, inQueue, inFunc, inFlags, inProperty, inQualifier, inFormat, args );
	va_end( args );
	return( err );
}

//===========================================================================================================================
//	CFObjectSetPropertyV
//===========================================================================================================================

OSStatus
	CFObjectSetPropertyV( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		const char *			inFormat, 
		va_list					inArgs )
{
	OSStatus					err;
	CFMutableDictionaryRef		value = NULL;
	
	err = CFPropertyListCreateFormattedVAList( NULL, &value, inFormat, inArgs );
	require_noerr( err, exit );
	
	err = CFObjectSetProperty( inObject, inQueue, inFunc, inFlags, inProperty, inQualifier, value );
	CFReleaseNullSafe( value );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFObjectSetPropertyCString
//===========================================================================================================================

OSStatus
	CFObjectSetPropertyCString( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		const void *			inStr, 
		size_t					inLen )
{
	OSStatus		err;
	CFStringRef		value;
	
	if( inLen == kSizeCString )
	{
		value = CFStringCreateWithCString( NULL, (const char *) inStr, kCFStringEncodingUTF8 );
		require_action( value, exit, err = kUnknownErr );
	}
	else
	{
		value = CFStringCreateWithBytes( NULL, (const uint8_t *) inStr, (CFIndex) inLen, kCFStringEncodingUTF8, false );
		require_action( value, exit, err = kUnknownErr );
	}
	
	err = CFObjectSetProperty( inObject, inQueue, inFunc, inFlags, inProperty, inQualifier, value );
	CFRelease( value );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFObjectSetPropertyData
//===========================================================================================================================

OSStatus
	CFObjectSetPropertyData( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		const void *			inData, 
		size_t					inLen )
{
	OSStatus		err;
	CFDataRef		value;
	
	value = CFDataCreate( NULL, (const uint8_t *) inData, (CFIndex) inLen );
	require_action( value, exit, err = kUnknownErr );
	
	err = CFObjectSetProperty( inObject, inQueue, inFunc, inFlags, inProperty, inQualifier, value );
	CFRelease( value );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFObjectSetPropertyDouble
//===========================================================================================================================

OSStatus
	CFObjectSetPropertyDouble( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		double					inValue )
{
	OSStatus		err;
	CFNumberRef		value;
	
	value = CFNumberCreate( NULL, kCFNumberDoubleType, &inValue );
	require_action( value, exit, err = kNoMemoryErr );
	
	err = CFObjectSetProperty( inObject, inQueue, inFunc, inFlags, inProperty, inQualifier, value );
	CFRelease( value );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFObjectSetPropertyInt64
//===========================================================================================================================

OSStatus
	CFObjectSetPropertyInt64( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		int64_t					inValue )
{
	OSStatus		err;
	CFNumberRef		value;
	
	value = CFNumberCreateInt64( inValue );
	require_action( value, exit, err = kNoMemoryErr );
	
	err = CFObjectSetProperty( inObject, inQueue, inFunc, inFlags, inProperty, inQualifier, value );
	CFRelease( value );
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CFObjectCopyTypedValue
//===========================================================================================================================

CFTypeRef
	CFObjectCopyTypedValue( 
		CFTypeRef				inObject, 
		CFObjectCopyValue_f		inCallback, 
		CFStringRef				inKey, 
		CFTypeID				inTypeID, 
		OSStatus *				outErr )
{
	CFTypeRef		value;
	OSStatus		err;
	
	value = inCallback( inObject, inKey, &err );
	require_noerr_quiet( err, exit );
	if( ( inTypeID != 0 ) && ( CFGetTypeID( value ) != inTypeID ) )
	{
		CFRelease( value );
		value = NULL;
		err = kTypeErr;
	}
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	CFObjectSetValue
//===========================================================================================================================

OSStatus	CFObjectSetValue( CFTypeRef inObject, CFObjectSetValue_f inCallback, CFStringRef inKey, CFTypeRef inValue )
{
	return( inCallback( inObject, inKey, inValue ) );
}

//===========================================================================================================================
//	CFObjectGetBytes
//===========================================================================================================================

uint8_t *
	CFObjectCopyBytes( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		size_t *			outLen, 
		OSStatus *			outErr )
{
	OSStatus		err;
	CFTypeRef		obj;
	uint8_t *		ptr;
	
	obj = CFObjectCopyTypedValue( inObject, inCallback, inKey, 0, &err );
	if( obj )
	{
		ptr = CFCopyData( obj, outLen, &err );
		CFRelease( obj );
	}
	else
	{
		ptr = NULL;
		if( outLen ) *outLen = 0;
		err = kNotFoundErr;
	}
	if( outErr ) *outErr = err;
	return( ptr );
}

//===========================================================================================================================
//	CFObjectGetBytes
//===========================================================================================================================

uint8_t *
	CFObjectGetBytes( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		void *				inBuf, 
		size_t				inMaxLen, 
		size_t *			outLen, 
		OSStatus *			outErr )
{
	uint8_t *		ptr;
	CFTypeRef		obj;
	
	obj = CFObjectCopyTypedValue( inObject, inCallback, inKey, 0, outErr );
	if( obj )
	{
		ptr = CFGetData( obj, inBuf, inMaxLen, outLen, outErr );
		CFRelease( obj );
	}
	else
	{
		ptr = (uint8_t *) inBuf;
		if( outLen ) *outLen = 0;
	}
	return( ptr );
}

//===========================================================================================================================
//	CFObjectSetBytes
//===========================================================================================================================

OSStatus
	CFObjectSetBytes( 
		CFTypeRef			inObject, 
		CFObjectSetValue_f	inCallback, 
		CFStringRef			inKey, 
		const void *		inPtr, 
		size_t				inLen )
{
	OSStatus		err;
	CFDataRef		obj;
	
	obj = CFDataCreate( NULL, inPtr, (CFIndex) inLen );
	require_action( obj, exit, err = kUnknownErr );
	
	err = CFObjectSetValue( inObject, inCallback, inKey, obj );
	CFRelease( obj );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFObjectCopyCString
//===========================================================================================================================

char *
	CFObjectCopyCString( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		OSStatus *			outErr )
{
	OSStatus		err;
	CFTypeRef		obj;
	char *			ptr;
	
	obj = CFObjectCopyTypedValue( inObject, inCallback, inKey, 0, &err );
	if( obj )
	{
		ptr = CFCopyCString( obj, &err );
		CFRelease( obj );
	}
	else
	{
		ptr = NULL;
		err = kNotFoundErr;
	}
	if( outErr ) *outErr = err;
	return( ptr );
}

//===========================================================================================================================
//	CFObjectGetCString
//===========================================================================================================================

char *
	CFObjectGetCString( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		char *				inBuf, 
		size_t				inMaxLen, 
		OSStatus *			outErr )
{
	char *			ptr;
	CFTypeRef		obj;
	
	obj = CFObjectCopyTypedValue( inObject, inCallback, inKey, 0, outErr );
	if( obj )
	{
		ptr = CFGetCString( obj, inBuf, inMaxLen );
		CFRelease( obj );
	}
	else if( inMaxLen > 0 )
	{
		*inBuf = '\0';
		ptr = inBuf;
	}
	else
	{
		ptr = "";
	}
	return( ptr );
}

//===========================================================================================================================
//	CFObjectSetCString
//===========================================================================================================================

OSStatus
	CFObjectSetCString( 
		CFTypeRef			inObject, 
		CFObjectSetValue_f	inCallback, 
		CFStringRef			inKey, 
		const void *		inStr, 
		size_t				inLen )
{
	OSStatus		err;
	CFStringRef		obj;
	
	if( inLen == kSizeCString )
	{
		obj = CFStringCreateWithCString( NULL, (const char *) inStr, kCFStringEncodingUTF8 );
	}
	else
	{
		obj = CFStringCreateWithBytes( NULL, (const uint8_t *) inStr, (CFIndex) inLen, kCFStringEncodingUTF8, false );
	}
	require_action( obj, exit, err = kFormatErr );
	
	err = CFObjectSetValue( inObject, inCallback, inKey, obj );
	CFRelease( obj );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFObjectGetDouble
//===========================================================================================================================

double
	CFObjectGetDouble( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey,
		OSStatus *			outErr )
{
	double			value;
	CFTypeRef		obj;
	
	obj = CFObjectCopyTypedValue( inObject, inCallback, inKey, 0, outErr );
	if( obj )
	{
		value = CFGetDouble( obj, outErr );
		CFRelease( obj );
		return( value );
	}
	return( 0 );
}

//===========================================================================================================================
//	CFObjectSetDouble
//===========================================================================================================================

OSStatus	CFObjectSetDouble( CFTypeRef inObject, CFObjectSetValue_f inCallback, CFStringRef inKey, double inValue )
{
	OSStatus		err;
	CFNumberRef		obj;
	
	obj = CFNumberCreate( NULL, kCFNumberDoubleType, &inValue );
	require_action( obj, exit, err = kUnknownErr );
	
	err = CFObjectSetValue( inObject, inCallback, inKey, obj );
	CFRelease( obj );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFObjectGetHardwareAddress
//===========================================================================================================================

uint64_t
	CFObjectGetHardwareAddress( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		uint8_t *			inBuf, 
		size_t				inLen, 
		OSStatus *			outErr )
{
	uint64_t		x;
	CFTypeRef		obj;
	
	obj = CFObjectCopyTypedValue( inObject, inCallback, inKey, 0, outErr );
	if( obj )
	{
		x = CFGetHardwareAddress( obj, inBuf, inLen, outErr );
		CFRelease( obj );
	}
	else
	{
		x = 0;
		if( inBuf )  memset( inBuf, 0, inLen );
		if( outErr ) *outErr = kNotFoundErr;
	}
	return( x );
}

//===========================================================================================================================
//	CFObjectSetHardwareAddress
//===========================================================================================================================

OSStatus
	CFObjectSetHardwareAddress( 
		CFTypeRef			inObject, 
		CFObjectSetValue_f	inCallback, 
		CFStringRef			inKey, 
		const void *		inAddr, 
		size_t				inLen )
{
	OSStatus		err;
	char			cstr[ 64 ];
	
	require_action( ( inLen == 6 ) || ( inLen == 8 ), exit, err = kSizeErr );
	HardwareAddressToCString( inAddr, inLen, cstr );
	err = CFObjectSetCString( inObject, inCallback, inKey, cstr, kSizeCString );
	require_noerr( err, exit );
	
exit:
	return( err );
}
//===========================================================================================================================
//	CFObjectGetInt64
//===========================================================================================================================

int64_t
	CFObjectGetInt64( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		OSStatus *			outErr )
{
	int64_t			value;
	CFTypeRef		obj;
	
	obj = CFObjectCopyTypedValue( inObject, inCallback, inKey, 0, outErr );
	if( obj )
	{
		value = CFGetInt64( obj, outErr );
		CFRelease( obj );
		return( value );
	}
	return( 0 );
}

//===========================================================================================================================
//	CFObjectGetInt64Ranged
//===========================================================================================================================

int64_t
	CFObjectGetInt64Ranged( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		int64_t				inMin, 
		int64_t				inMax, 
		OSStatus *			outErr )
{
	int64_t			value;
	CFTypeRef		obj;
	
	obj = CFObjectCopyTypedValue( inObject, inCallback, inKey, 0, outErr );
	if( obj )
	{
		value = CFGetInt64Ranged( obj, inMin, inMax, outErr );
		CFRelease( obj );
		return( value );
	}
	return( 0 );
}

//===========================================================================================================================
//	CFObjectSetInt64
//===========================================================================================================================

OSStatus	CFObjectSetInt64( CFTypeRef inObject, CFObjectSetValue_f inCallback, CFStringRef inKey, int64_t inValue )
{
	OSStatus		err;
	CFNumberRef		obj;
	
	obj = CFNumberCreateInt64( inValue );
	require_action( obj, exit, err = kUnknownErr );
	
	err = CFObjectSetValue( inObject, inCallback, inKey, obj );
	CFRelease( obj );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFObjectGetUUID
//===========================================================================================================================

OSStatus
	CFObjectGetUUID( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		const uint8_t *		inBaseUUID, 
		uint8_t				outUUID[ 16 ] )
{
	OSStatus		err;
	CFTypeRef		obj;
	
	obj = CFObjectCopyTypedValue( inObject, inCallback, inKey, 0, &err );
	require_noerr_quiet( err, exit );
	
	err = CFGetUUIDEx( obj, inBaseUUID, outUUID );
	CFRelease( obj );
	require_noerr_quiet( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFObjectSetUUIDString
//===========================================================================================================================

OSStatus
	CFObjectSetUUIDString( 
		CFTypeRef				inObject, 
		CFObjectSetValue_f		inCallback, 
		CFStringRef				inKey, 
		const void *			inUUID, 
		size_t					inSize, 
		const void *			inBaseUUID, 
		uint32_t				inFlags )
{
	OSStatus		err;
	CFStringRef		str;
	
	str = CFCreateUUIDString( inUUID, inSize, inBaseUUID, inFlags, &err );
	require_noerr( err, exit );
	
	err = CFObjectSetValue( inObject, inCallback, inKey, str );
	CFRelease( str );
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Boxing ==
#endif

//===========================================================================================================================
//	CFCopyCString
//===========================================================================================================================

char *	CFCopyCString( CFTypeRef inObj, OSStatus *outErr )
{
	char *			result = NULL;
	OSStatus		err;
	CFTypeID		typeID;
	size_t			len;
	char			tempCStr[ 64 ];
	
	require_action_quiet( inObj, exit, err = kParamErr );
	
	typeID = CFGetTypeID( inObj );
	if( typeID == CFStringGetTypeID() )
	{
		err = CFStringCopyUTF8CString( (CFStringRef) inObj, &result );
		require_noerr( err, exit );
	}
	else if( typeID == CFDataGetTypeID() )
	{
		len = (size_t)( ( CFDataGetLength( (CFDataRef) inObj ) * 2 ) + 1 );
		result = (char *) malloc( len );
		require_action( result, exit, err = kNoMemoryErr );
		CFGetCString( inObj, result, len );
	}
	else
	{
		CFGetCString( inObj, tempCStr, sizeof( tempCStr ) );
		result = strdup( tempCStr );
		require_action( result, exit, err = kNoMemoryErr );
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	CFGetCString
//===========================================================================================================================

char *	CFGetCString( CFTypeRef inObj, char *inBuf, size_t inMaxLen )
{
	CFTypeID		typeID;
	
	if( inMaxLen <= 0 ) return( "" );
	if( !inObj ) { *inBuf = '\0'; return( inBuf ); }
	
	typeID = CFGetTypeID( inObj );
	if( typeID == CFStringGetTypeID() )
	{
		*inBuf = '\0';
		CFStringGetCString( (CFStringRef) inObj, inBuf, (CFIndex) inMaxLen, kCFStringEncodingUTF8 );
	}
	else if( typeID == CFNumberGetTypeID() )
	{
		double		d;
		int64_t		s64;
		
		if( CFNumberIsFloatType( (CFNumberRef) inObj ) )
		{
			d = 0;
			CFNumberGetValue( (CFNumberRef) inObj, kCFNumberDoubleType, &d );
			snprintf( inBuf, inMaxLen, "%f", d );
		}
		else
		{
			s64 = 0;
			CFNumberGetValue( (CFNumberRef) inObj, kCFNumberSInt64Type, &s64 );
			SNPrintF( inBuf, inMaxLen, "%lld", s64 );
		}
	}
	else if( inObj == kCFBooleanTrue )  strlcpy( inBuf, "true", inMaxLen );
	else if( inObj == kCFBooleanFalse ) strlcpy( inBuf, "false", inMaxLen );
	else if( typeID == CFDataGetTypeID() )
	{
		const uint8_t *		src;
		const uint8_t *		end;
		char *				dst;
		char *				lim;
		uint8_t				b;
		
		src = CFDataGetBytePtr( (CFDataRef) inObj );
		end = src + CFDataGetLength( (CFDataRef) inObj );
		dst = inBuf;
		lim = dst + ( inMaxLen - 1 );
		while( ( src < end ) && ( ( lim - dst ) >= 2 ) )
		{
			b = *src++;
			*dst++ = kHexDigitsLowercase[ b >> 4 ];
			*dst++ = kHexDigitsLowercase[ b & 0xF ];
		}
		*dst = '\0';
	}
	else if( typeID == CFDictionaryGetTypeID() )	snprintf( inBuf, inMaxLen, "{}" );
	else if( typeID == CFArrayGetTypeID() )			snprintf( inBuf, inMaxLen, "[]" );
	else											*inBuf = '\0';
	return( inBuf );
}

//===========================================================================================================================
//	CFCopyCFData
//===========================================================================================================================

CFDataRef	CFCopyCFData( CFTypeRef inObj, size_t *outLen, OSStatus *outErr )
{
	CFDataRef		data = NULL;
	OSStatus		err;
	uint8_t *		ptr;
	size_t			len = 0;
	
	require_action_quiet( inObj, exit, err = kParamErr );
	
	ptr = CFCopyData( inObj, &len, &err );
	require_noerr_quiet( err, exit );
	
	data = CFDataCreate( NULL, ptr, (CFIndex) len );
	free( ptr );
	require_action( data, exit, err = kNoMemoryErr );
	
exit:
	if( outLen ) *outLen = len;
	if( outErr ) *outErr = err;
	return( data );
}

//===========================================================================================================================
//	CFCopyData
//===========================================================================================================================

uint8_t *	CFCopyData( CFTypeRef inObj, size_t *outLen, OSStatus *outErr )
{
	uint8_t *			result = NULL;
	OSStatus			err;
	CFTypeID			typeID;
	const uint8_t *		src;
	size_t				len = 0, len2;
	
	require_action_quiet( inObj, exit, err = kParamErr );
	
	typeID = CFGetTypeID( inObj );
	if( typeID == CFDataGetTypeID() )
	{
		src = CFDataGetBytePtr( (CFDataRef) inObj );
		len = (size_t) CFDataGetLength( (CFDataRef) inObj );
		result = (uint8_t *) malloc( ( len > 0 ) ? len : 1 ); // Use 1 if 0 since malloc( 0 ) is undefined.
		require_action( result, exit, err = kNoMemoryErr );
		if( len > 0 ) memcpy( result, src, len );
	}
	else if( typeID == CFStringGetTypeID() )
	{
		const char *		utf8Ptr;
		char *				utf8Buf;
		
		err = CFStringGetOrCopyCStringUTF8( (CFStringRef) inObj, &utf8Ptr, &utf8Buf, &len2 );
		require_noerr( err, exit );
		
		err = HexToDataCopy( utf8Ptr, len2, 
			kHexToData_IgnoreDelimiters | kHexToData_IgnorePrefixes | kHexToData_IgnoreWhitespace, 
			&result, &len, NULL );
		if( utf8Buf ) free( utf8Buf );
		require_noerr( err, exit );
	}
	else if( typeID == CFNullGetTypeID() )
	{
		result = (uint8_t *) malloc( 1 ); // Use 1 since malloc( 0 ) is undefined.
		require_action( result, exit, err = kNoMemoryErr );
		len = 0;
	}
	else
	{
		err = kUnsupportedErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outLen ) *outLen = len;
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	CFGetData
//===========================================================================================================================

uint8_t *	CFGetData( CFTypeRef inObj, void *inBuf, size_t inMaxLen, size_t *outLen, OSStatus *outErr )
{
	OSStatus			err;
	CFTypeID			typeID;
	const uint8_t *		src;
	size_t				len = 0, len2;
	
	require_action_quiet( inObj, exit, err = kParamErr );
	
	typeID = CFGetTypeID( inObj );
	if( typeID == CFDataGetTypeID() )
	{
		src = CFDataGetBytePtr( (CFDataRef) inObj );
		len = (size_t) CFDataGetLength( (CFDataRef) inObj );
		if( inBuf )
		{
			if( len > inMaxLen ) len = inMaxLen;
			if( len > 0 ) memcpy( inBuf, src, len );
		}
		else
		{
			inBuf = (void *) src;
		}
	}
	else if( typeID == CFStringGetTypeID() )
	{
		const char *		utf8Ptr;
		char *				utf8Buf;
		
		err = CFStringGetOrCopyCStringUTF8( (CFStringRef) inObj, &utf8Ptr, &utf8Buf, &len2 );
		require_noerr( err, exit );
		
		HexToData( utf8Ptr, len2, 
			kHexToData_IgnoreDelimiters | kHexToData_IgnorePrefixes | kHexToData_IgnoreWhitespace, 
			inBuf, inMaxLen, &len, NULL, NULL );
		FreeNullSafe( utf8Buf );
	}
	else if( typeID == CFNullGetTypeID() )
	{
		inBuf = (void *) "";
		len   = 0;
	}
	else
	{
		err = kUnsupportedErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outLen ) *outLen = len;
	if( outErr ) *outErr = err;
	return( (uint8_t *) inBuf );
}

//===========================================================================================================================
//	CFGetDouble
//===========================================================================================================================

double	CFGetDouble( CFTypeRef inObj, OSStatus *outErr )
{
	double			value = 0;
	CFTypeID		typeID;
	OSStatus		err;
	
	require_action_quiet( inObj, exit, err = kParamErr );
	
	typeID = CFGetTypeID( inObj );
	if( typeID == CFNumberGetTypeID() )
	{
		CFNumberGetValue( (CFNumberRef) inObj, kCFNumberDoubleType, &value );
		err = kNoErr;
	}
	else if( typeID == CFStringGetTypeID() )
	{
		char		tempStr[ 128 ];
		Boolean		good;
		int64_t		s64;
		
		good = CFStringGetCString( (CFStringRef) inObj, tempStr, (CFIndex) sizeof( tempStr ), kCFStringEncodingASCII );
		require_action_quiet( good, exit, err = kSizeErr );
		
		if(      IsTrueString( tempStr, kSizeCString ) )				value = 1;
		else if( IsFalseString( tempStr, kSizeCString ) )				value = 0;
		else if( sscanf( tempStr, "%lf", &value )				== 1 )	{}
		else if( sscanf( tempStr, "%lli", &s64 ) == 1 )	value = (double) s64;
		else { err = kFormatErr; goto exit; }
		err = kNoErr;
	}
	else
	{
		value = (double) CFGetInt64( inObj, &err );
	}
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	CFGetHardwareAddress
//===========================================================================================================================

uint64_t	CFGetHardwareAddress( CFTypeRef inObj, uint8_t *inBuf, size_t inLen, OSStatus *outErr )
{
	uint64_t			scalar = 0;
	OSStatus			err;
	CFTypeID			typeID;
	char				tempStr[ 64 ];
	uint8_t				tempAddr[ 8 ];
	Boolean				good;
	const uint8_t *		ptr;
	
	require_action_quiet( inObj, exit, err = kParamErr );
	
	typeID = CFGetTypeID( inObj );
	if( typeID == CFStringGetTypeID() )
	{
		good = CFStringGetCString( (CFStringRef) inObj, tempStr, (CFIndex) sizeof( tempStr ), kCFStringEncodingASCII );
		require_action_quiet( good, exit, err = kSizeErr );
		
		if( !inBuf )
		{
			require_action( inLen <= sizeof( tempAddr ), exit, err = kSizeErr );
			inBuf = tempAddr;
		}
		err = TextToHardwareAddress( tempStr, kSizeCString, inLen, inBuf );
		require_noerr_quiet( err, exit );
		if(      inLen == 6 ) scalar = ReadBig48( inBuf );
		else if( inLen == 8 ) scalar = ReadBig64( inBuf );
	}
	else if( typeID == CFNumberGetTypeID() )
	{
		CFNumberGetValue( (CFNumberRef) inObj, kCFNumberSInt64Type, &scalar );
		if( inBuf )
		{
			if(      inLen == 6 ) WriteBig48( inBuf, scalar );
			else if( inLen == 8 ) WriteBig64( inBuf, scalar );
		}
	}
	else if( typeID == CFDataGetTypeID() )
	{
		require_action_quiet( CFDataGetLength( (CFDataRef) inObj ) == (CFIndex) inLen, exit, err = kSizeErr );
		ptr = CFDataGetBytePtr( (CFDataRef) inObj );
		if( inBuf ) memcpy( inBuf, ptr, inLen );
		if(      inLen == 6 ) scalar = ReadBig48( ptr );
		else if( inLen == 8 ) scalar = ReadBig64( ptr );
	}
	else
	{
		err = kTypeErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( err && inBuf )	memset( inBuf, 0, inLen );
	if( outErr )		*outErr = err;
	return( scalar );
}

//===========================================================================================================================
//	CFGetInt64
//===========================================================================================================================

int64_t	CFGetInt64( CFTypeRef inObj, OSStatus *outErr )
{
	int64_t			value = 0;
	OSStatus		err;
	CFTypeID		typeID;
	Boolean			good;
	
	require_action_quiet( inObj, exit, err = kParamErr );
	
	typeID = CFGetTypeID( inObj );
	if( typeID == CFNumberGetTypeID() )
	{
		if( CFNumberIsFloatType( (CFNumberRef) inObj ) )
		{
			double		tempDouble;
			
			tempDouble = 0;
			CFNumberGetValue( (CFNumberRef) inObj, kCFNumberDoubleType, &tempDouble );
			if(      tempDouble < INT64_MIN ) { value = INT64_MIN; err = kRangeErr; goto exit; }
			else if( tempDouble > INT64_MAX ) { value = INT64_MAX; err = kRangeErr; goto exit; }
			else								value = (int64_t) tempDouble;
		}
		else
		{
			CFNumberGetValue( (CFNumberRef) inObj, kCFNumberSInt64Type, &value );
		}
	}
	else if( ( (CFBooleanRef) inObj ) == kCFBooleanTrue )  value = 1;
	else if( ( (CFBooleanRef) inObj ) == kCFBooleanFalse ) value = 0;
	else if( typeID == CFStringGetTypeID() )
	{
		char		tempStr[ 128 ];
		
		good = CFStringGetCString( (CFStringRef) inObj, tempStr, (CFIndex) sizeof( tempStr ), kCFStringEncodingASCII );
		require_action_quiet( good, exit, err = kSizeErr );
		
		if(      IsTrueString( tempStr, kSizeCString ) )					value = 1;
		else if( IsFalseString( tempStr, kSizeCString ) )					value = 0;
		else if( sscanf( tempStr, "%lli", &value ) != 1 )	{ err = kFormatErr; goto exit; }
	}
	else if( typeID == CFDataGetTypeID() )
	{
		const uint8_t *		ptr;
		const uint8_t *		end;
		
		ptr = CFDataGetBytePtr( (CFDataRef) inObj );
		end = ptr + CFDataGetLength( (CFDataRef) inObj );
		require_action_quiet( ( end - ptr ) <= ( (ptrdiff_t) sizeof( int64_t ) ), exit, err = kSizeErr );
		
		for( ; ptr < end; ++ptr )
		{
			value = ( value << 8 ) + *ptr;
		}
	}
	else if( typeID == CFNullGetTypeID() ) {} // Leave value at 0.
	else
	{
		err = kTypeErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	CFGetInt64Ranged
//===========================================================================================================================

int64_t	CFGetInt64Ranged( CFTypeRef inObj, int64_t inMin, int64_t inMax, OSStatus *outErr )
{
	OSStatus		err;
	int64_t			x;
	
	x = CFGetInt64( inObj, &err );
	require_noerr_quiet( err, exit );
	require_action_quiet( ( x >= inMin ) && ( x <= inMax ), exit, err = kRangeErr );
	
exit:
	if( outErr ) *outErr = err;
	return( x );
}

//===========================================================================================================================
//	CFCreateUUIDString
//===========================================================================================================================

CFStringRef	CFCreateUUIDString( const void *inUUID, size_t inSize, const void *inBaseUUID, uint32_t inFlags, OSStatus *outErr )
{
	CFStringRef		result = NULL;
	OSStatus		err;
	char			buf[ 38 ];
	
	UUIDtoCStringFlags( inUUID, inSize, inBaseUUID, inFlags, buf, &err );
	require_noerr( err, exit );
	
	result = CFStringCreateWithCString( NULL, buf, kCFStringEncodingUTF8 );
	require_action( result, exit, err = kUnknownErr );
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	CFGetUUIDEx
//===========================================================================================================================

OSStatus	CFGetUUIDEx( CFTypeRef inObj, const uint8_t *inBaseUUID, uint8_t outUUID[ 16 ] )
{
	uint8_t * const		uuidPtr = (uint8_t *) outUUID;
	OSStatus			err;
	CFTypeID			typeID;
	char				cstr[ 64 ];
	Boolean				good;
	CFIndex				n;
	const uint8_t *		ptr;
	int64_t				s64;
	
	require_action_quiet( inObj, exit, err = kParamErr );
	
	typeID = CFGetTypeID( inObj );
	if( typeID == CFStringGetTypeID() )
	{
		good = CFStringGetCString( (CFStringRef) inObj, cstr, (CFIndex) sizeof( cstr ), kCFStringEncodingASCII );
		require_action_quiet( good, exit, err = kSizeErr );
		
		err = StringToUUIDEx( cstr, kSizeCString, false, inBaseUUID, uuidPtr );
		require_noerr_quiet( err, exit );
	}
	else if( typeID == CFDataGetTypeID() )
	{
		n = CFDataGetLength( (CFDataRef) inObj );
		if( n == 16 )
		{
			if( uuidPtr ) memcpy( uuidPtr, CFDataGetBytePtr( (CFDataRef) inObj ), 16 );
		}
		else if( ( n == 1 ) && inBaseUUID )
		{
			if( uuidPtr )
			{
				memcpy( uuidPtr, inBaseUUID, 16 );
				CFDataGetBytes( (CFDataRef) inObj, CFRangeMake( 0, 1 ), &uuidPtr[ 3 ] );
			}
		}
		else if( ( n == 2 ) && inBaseUUID )
		{
			if( uuidPtr )
			{
				ptr = CFDataGetBytePtr( (CFDataRef) inObj );
				memcpy( uuidPtr, inBaseUUID, 16 );
				uuidPtr[ 2 ] = ptr[ 0 ];
				uuidPtr[ 3 ] = ptr[ 1 ];
			}
		}
		else if( ( n == 4 ) && inBaseUUID )
		{
			if( uuidPtr )
			{
				ptr = CFDataGetBytePtr( (CFDataRef) inObj );
				memcpy( uuidPtr, inBaseUUID, 16 );
				uuidPtr[ 0 ] = ptr[ 0 ];
				uuidPtr[ 1 ] = ptr[ 1 ];
				uuidPtr[ 2 ] = ptr[ 2 ];
				uuidPtr[ 3 ] = ptr[ 3 ];
			}
		}
		else
		{
			err = kSizeErr;
			goto exit;
		}
	}
	else if( inBaseUUID && ( typeID == CFNumberGetTypeID() ) )
	{
		CFNumberGetValue( (CFNumberRef) inObj, kCFNumberSInt64Type, &s64 );
		require_action_quiet( ( s64 >= 0 ) && ( s64 <= UINT32_MAX ), exit, err = kRangeErr );
		if( uuidPtr )
		{
			memcpy( uuidPtr, inBaseUUID, 16 );
			WriteBig32( uuidPtr, (uint32_t) s64 );
		}
	}
	else
	{
		err = kTypeErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Type-specific Utilities ==
#endif

//===========================================================================================================================
//	CFArrayAppendInt64
//===========================================================================================================================

OSStatus	CFArrayAppendInt64( CFMutableArrayRef inArray, int64_t inValue )
{
	OSStatus		err;
	CFNumberRef		num;
	
	num = CFNumberCreateInt64( inValue );
	require_action( num, exit, err = kNoMemoryErr );
	
	CFArrayAppendValue( inArray, num );
	CFRelease( num );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFArrayAppendCString
//===========================================================================================================================

OSStatus	CFArrayAppendCString( CFMutableArrayRef inArray, const char *inStr, size_t inLen )
{
	OSStatus		err;
	CFStringRef		cfstr;
	
	if( inLen == kSizeCString )
	{
		cfstr = CFStringCreateWithCString( NULL, inStr, kCFStringEncodingUTF8 );
		require_action( cfstr, exit, err = kUnknownErr );
	}
	else
	{
		cfstr = CFStringCreateWithBytes( NULL, (const uint8_t *) inStr, (CFIndex) inLen, kCFStringEncodingUTF8, false );
		require_action( cfstr, exit, err = kUnknownErr );
	}
	
	CFArrayAppendValue( inArray, cfstr );
	CFRelease( cfstr );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFArrayEnsureCreatedAndAppend
//===========================================================================================================================

OSStatus	CFArrayEnsureCreatedAndAppend( CFMutableArrayRef *ioArray, CFTypeRef inValue )
{
	CFMutableArrayRef		array = *ioArray;
	OSStatus				err;
	
	if( !array )
	{
		array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( array, exit, err = kNoMemoryErr );
		*ioArray = array;
	}
	CFArrayAppendValue( array, inValue );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFArrayEnsureCreatedAndAppendCString
//===========================================================================================================================

OSStatus	CFArrayEnsureCreatedAndAppendCString( CFMutableArrayRef *ioArray, const void *inStr, size_t inLen )
{
	OSStatus		err;
	CFStringRef		cfstr;
	
	if( inLen == kSizeCString )
	{
		cfstr = CFStringCreateWithCString( NULL, (const char *) inStr, kCFStringEncodingUTF8 );
		require_action( cfstr, exit, err = kUnknownErr );
	}
	else
	{
		cfstr = CFStringCreateWithBytes( NULL, (const uint8_t *) inStr, (CFIndex) inLen, kCFStringEncodingUTF8, false );
		require_action( cfstr, exit, err = kUnknownErr );
	}
	
	err = CFArrayEnsureCreatedAndAppend( ioArray, cfstr );
	CFRelease( cfstr );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFArrayGetTypedValueAtIndex
//===========================================================================================================================

CFTypeRef	CFArrayGetTypedValueAtIndex( CFArrayRef inArray, CFIndex inIndex, CFTypeID inType, OSStatus *outErr )
{
	CFTypeRef		result = NULL;
	CFTypeRef		value;
	OSStatus		err;
	
	value = CFArrayGetValueAtIndex( inArray, inIndex );
	require_action_quiet( value, exit, err = kNotFoundErr );
	require_action_quiet( CFGetTypeID( value ) == inType, exit, err = kTypeErr );
	result = value;
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	CFDataCreateWithFilePath
//===========================================================================================================================

CFDataRef	CFDataCreateWithFilePath( const char *inPath, OSStatus *outErr )
{
	CFDataRef		result = NULL;
	OSStatus		err;
	FILE *			file;
	
	file = fopen( inPath, "rb" );
	err = map_global_value_errno( file, file );
	require_noerr_quiet( err, exit );
	
	result = CFDataCreateWithANSIFile( file, &err );
	require_noerr( err, exit );
	
exit:
	if( file ) fclose( file );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	CFDataCreateWithANSIFile
//===========================================================================================================================

CFDataRef	CFDataCreateWithANSIFile( FILE *inFile, OSStatus *outErr )
{
	CFDataRef				result = NULL;
	CFMutableDataRef		data;
	OSStatus				err;
	uint8_t *				tempBuf = NULL;
	size_t					tempLen;
	size_t					readSize;
	
	data = CFDataCreateMutable( NULL, 0 );
	require_action( data, exit, err = kNoMemoryErr );
	
	tempLen = 128 * 1024;
	tempBuf = (uint8_t *) malloc( tempLen );
	require_action( tempBuf, exit, err = kNoMemoryErr );
	
	for( ;; )
	{
		readSize = fread( tempBuf, 1, tempLen, inFile );
		if( readSize == 0 ) break;
		
		CFDataAppendBytes( data, tempBuf, (CFIndex) readSize );
	}
	
	result = data;
	data = NULL;
	err = kNoErr;
	
exit:
	if( tempBuf ) free( tempBuf );
	CFReleaseNullSafe( data );
	if( outErr ) *outErr = err;
	return( result );
}

#if( !CFLITE_ENABLED )
//===========================================================================================================================
//	CFDateCreateWithComponents
//===========================================================================================================================

CFDateRef
	CFDateCreateWithComponents( 
		CFAllocatorRef	inAllocator, 
		int				inYear, 
		int				inMonth, 
		int				inDay, 
		int				inHour, 
		int				inMinute, 
		int				inSecond )
{
	CFDateRef			date	 = NULL;
	CFCalendarRef		calendar = NULL;
	CFTimeZoneRef		tz;
	Boolean				good;
	CFAbsoluteTime		t;
	
	(void) inAllocator;
	
	calendar = CFCalendarCopyCurrent();
	require( calendar, exit );
	
	tz = CFTimeZoneCreateWithName( NULL, CFSTR( "GMT" ), false );
	require( tz, exit );
	CFCalendarSetTimeZone( calendar, tz );
	CFRelease( tz );
	
	good = CFCalendarComposeAbsoluteTime( calendar, &t, "yMdHms", inYear, inMonth, inDay, inHour, inMinute, inSecond );
	require( good, exit );
	
	date = CFDateCreate( NULL, t );
	require( date, exit );
	
exit:
	CFReleaseNullSafe( calendar );
	return( date );
}

//===========================================================================================================================
//	CFDateGetComponents
//===========================================================================================================================

OSStatus
	CFDateGetComponents( 
		CFDateRef	inDate, 
		int *		outYear, 
		int *		outMonth, 
		int *		outDay, 
		int *		outHour, 
		int *		outMinute, 
		int *		outSecond, 
		int *		outMicros )
{
	OSStatus			err;
	CFCalendarRef		calendar = NULL;
	CFTimeZoneRef		tz;
	CFAbsoluteTime		t;
	Boolean				good;
	double				d;
	
	calendar = CFCalendarCopyCurrent();
	require_action( calendar, exit, err = kUnknownErr );
	
	tz = CFTimeZoneCreateWithName( NULL, CFSTR( "GMT" ), false );
	require_action( tz, exit, err = kUnknownErr );
	CFCalendarSetTimeZone( calendar, tz );
	CFRelease( tz );
	
	t = CFDateGetAbsoluteTime( inDate );
	good = CFCalendarDecomposeAbsoluteTime( calendar, t, "yMdHms", outYear, outMonth, outDay, outHour, outMinute, outSecond );
	require_action( good, exit, err = kUnknownErr );
	
	if( outMicros ) *outMicros = (int)( modf( t, &d ) * 1000000 );
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( calendar );
	if( err )
	{
		*outYear	= 0;
		*outMonth	= 0;
		*outDay		= 0;
		*outHour	= 0;
		*outMinute	= 0;
		*outSecond	= 0;
		if( outMicros ) *outMicros	= 0;
	}
	return( err );
}
#endif // !CFLITE_ENABLED

//===========================================================================================================================
//	CFDictionaryCopyKeys
//===========================================================================================================================

CFArrayRef	CFDictionaryCopyKeys( CFDictionaryRef inDict, OSStatus *outErr )
{
	CFArrayRef			result = NULL;
	OSStatus			err;
	CFIndex				n;
	const void **		keys = NULL;
	
	n = CFDictionaryGetCount( inDict );
	if( n > 0 )
	{
		keys = (const void **) malloc( ( (size_t) n ) * sizeof( *keys ) );
		require_action( keys, exit, err = kNoMemoryErr );
		CFDictionaryGetKeysAndValues( inDict, keys, NULL );
	}
	
	result = CFArrayCreate( NULL, keys, n, &kCFTypeArrayCallBacks );
	if( keys ) free( (void *) keys );
	require_action( result, exit, err = kNoMemoryErr );
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	CFDictionaryMergeDictionary
//===========================================================================================================================

static void	_CFDictionaryMergeDictionaryApplier( const void *inKey, const void *inValue, void *inContext );

OSStatus	CFDictionaryMergeDictionary( CFMutableDictionaryRef inDestinationDict, CFDictionaryRef inSourceDict )
{
	CFDictionaryApplyFunction( inSourceDict, _CFDictionaryMergeDictionaryApplier, inDestinationDict );
	return( kNoErr );
}

static void	_CFDictionaryMergeDictionaryApplier( const void *inKey, const void *inValue, void *inContext )
{
	CFDictionarySetValue( (CFMutableDictionaryRef) inContext, inKey, inValue );
}

//===========================================================================================================================
//	CFDictionaryCopyCString
//===========================================================================================================================

char *	CFDictionaryCopyCString( CFDictionaryRef inDict, const void *inKey, OSStatus *outErr )
{
	OSStatus		err;
	CFTypeRef		tempObj;
	char *			ptr;
	
	tempObj = inDict ? CFDictionaryGetValue( inDict, inKey ) : NULL;
	if( tempObj )
	{
		ptr = CFCopyCString( tempObj, &err );
	}
	else
	{
		ptr = NULL;
		err = kNotFoundErr;
	}
	if( outErr ) *outErr = err;
	return( ptr );
}

//===========================================================================================================================
//	CFDictionaryGetCString
//===========================================================================================================================

char *	CFDictionaryGetCString( CFDictionaryRef inDict, const void *inKey, char *inBuf, size_t inMaxLen, OSStatus *outErr )
{
	OSStatus		err;
	CFTypeRef		tempObj;
	char *			ptr;
	
	tempObj = inDict ? CFDictionaryGetValue( inDict, inKey ) : NULL;
	if( tempObj )
	{
		ptr = CFGetCString( tempObj, inBuf, inMaxLen );
		err = kNoErr;
	}
	else
	{
		if( inMaxLen > 0 )
		{
			*inBuf = '\0';
			ptr = inBuf;
		}
		else
		{
			ptr = "";
		}
		err = kNotFoundErr;
	}
	if( outErr ) *outErr = err;
	return( ptr );
}

//===========================================================================================================================
//	CFDictionarySetCString
//===========================================================================================================================

OSStatus	CFDictionarySetCString( CFMutableDictionaryRef inDict, const void *inKey, const void *inStr, size_t inLen )
{
	OSStatus		err;
	CFStringRef		cfstr;
	
	if( !inStr ) inStr = "";
	if( inLen == kSizeCString )
	{
		cfstr = CFStringCreateWithCString( NULL, (const char *) inStr, kCFStringEncodingUTF8 );
		require_action( cfstr, exit, err = kUnknownErr );
	}
	else
	{
		cfstr = CFStringCreateWithBytes( NULL, (const uint8_t *) inStr, (CFIndex) inLen, kCFStringEncodingUTF8, false );
		require_action( cfstr, exit, err = kUnknownErr );
	}
	CFDictionarySetValue( inDict, inKey, cfstr );
	CFRelease( cfstr );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFDictionaryCopyCFData
//===========================================================================================================================

CFDataRef
	CFDictionaryCopyCFData( 
		CFDictionaryRef	inDict, 
		const void *	inKey, 
		size_t *		outLen, 
		OSStatus *		outErr )
{
	CFDataRef		data;
	CFTypeRef		obj;
	
	obj = inDict ? CFDictionaryGetValue( inDict, inKey ) : NULL;
	if( obj )
	{
		data = CFCopyCFData( obj, outLen, outErr );
	}
	else
	{
		data = NULL;
		if( outLen ) *outLen = 0;
		if( outErr ) *outErr = kNotFoundErr;
	}
	return( data );
}

//===========================================================================================================================
//	CFDictionaryCopyData
//===========================================================================================================================

uint8_t *
	CFDictionaryCopyData( 
		CFDictionaryRef	inDict, 
		const void *	inKey, 
		size_t *		outLen, 
		OSStatus *		outErr )
{
	uint8_t *		ptr;
	CFTypeRef		obj;
	
	obj = inDict ? CFDictionaryGetValue( inDict, inKey ) : NULL;
	if( obj )
	{
		ptr = CFCopyData( obj, outLen, outErr );
	}
	else
	{
		ptr = NULL;
		if( outLen ) *outLen = 0;
		if( outErr ) *outErr = kNotFoundErr;
	}
	return( ptr );
}

//===========================================================================================================================
//	CFDictionaryGetData
//===========================================================================================================================

uint8_t *
	CFDictionaryGetData( 
		CFDictionaryRef	inDict, 
		const void *	inKey, 
		void *			inBuf, 
		size_t			inMaxLen, 
		size_t *		outLen, 
		OSStatus *		outErr )
{
	uint8_t *		ptr;
	CFTypeRef		obj;
	
	obj = inDict ? CFDictionaryGetValue( inDict, inKey ) : NULL;
	if( obj )
	{
		ptr = CFGetData( obj, inBuf, inMaxLen, outLen, outErr );
	}
	else
	{
		ptr = (uint8_t *) inBuf;
		if( outLen ) *outLen = 0;
		if( outErr ) *outErr = kNotFoundErr;
	}
	return( ptr );
}

//===========================================================================================================================
//	CFDictionarySetData
//===========================================================================================================================

OSStatus	CFDictionarySetData( CFMutableDictionaryRef inDict, const void *inKey, const void *inData, size_t inLen )
{
	OSStatus		err;
	CFDataRef		data;
	
	data = CFDataCreate( NULL, (const uint8_t *) inData, (CFIndex) inLen );
	require_action( data, exit, err = kUnknownErr );
	
	CFDictionarySetValue( inDict, inKey, data );
	CFRelease( data );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFDictionaryGetDouble
//===========================================================================================================================

double	CFDictionaryGetDouble( CFDictionaryRef inDict, const void *inKey, OSStatus *outErr )
{
	double			x;
	CFTypeRef		value;
	
	value = inDict ? CFDictionaryGetValue( inDict, inKey ) : NULL;
	if( value )
	{
		x = CFGetDouble( value, outErr );
	}
	else
	{
		x = 0;
		if( outErr ) *outErr = kNotFoundErr;
	}
	return( x );
}

//===========================================================================================================================
//	CFDictionarySetDouble
//===========================================================================================================================

OSStatus	CFDictionarySetDouble( CFMutableDictionaryRef inDict, const void *inKey, double x )
{
	OSStatus		err;
	CFNumberRef		tempNum;
	
	tempNum = CFNumberCreate( NULL, kCFNumberDoubleType, &x );
	require_action( tempNum, exit, err = kNoMemoryErr );
	
	CFDictionarySetValue( inDict, inKey, tempNum );
	CFRelease( tempNum );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFDictionaryGetHardwareAddress
//===========================================================================================================================

uint64_t
	CFDictionaryGetHardwareAddress( 
		CFDictionaryRef inDict, 
		const void *	inKey, 
		uint8_t *		inBuf, 
		size_t			inLen, 
		OSStatus *		outErr )
{
	uint64_t		x;
	CFTypeRef		value;
	
	value = inDict ? CFDictionaryGetValue( inDict, inKey ) : NULL;
	if( value )
	{
		x = CFGetHardwareAddress( value, inBuf, inLen, outErr );
	}
	else
	{
		x = 0;
		if( inBuf )  memset( inBuf, 0, inLen );
		if( outErr ) *outErr = kNotFoundErr;
	}
	return( x );
}

//===========================================================================================================================
//	CFDictionarySetHardwareAddress
//===========================================================================================================================

OSStatus	CFDictionarySetHardwareAddress( CFMutableDictionaryRef inDict, const void *inKey, const void *inAddr, size_t inLen )
{
	OSStatus		err;
	char			cstr[ 64 ];
	
	require_action( ( inLen == 6 ) || ( inLen == 8 ), exit, err = kSizeErr );
	HardwareAddressToCString( inAddr, inLen, cstr );
	err = CFDictionarySetCString( inDict, inKey, cstr, kSizeCString );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFDictionaryGetInt64
//===========================================================================================================================

int64_t	CFDictionaryGetInt64( CFDictionaryRef inDict, const void *inKey, OSStatus *outErr )
{
	int64_t			x;
	CFTypeRef		value;
	
	value = inDict ? CFDictionaryGetValue( inDict, inKey ) : NULL;
	if( value )
	{
		x = CFGetInt64( value, outErr );
	}
	else
	{
		x = 0;
		if( outErr ) *outErr = kNotFoundErr;
	}
	return( x );
}

//===========================================================================================================================
//	CFDictionaryGetInt64Ranged
//===========================================================================================================================

int64_t	CFDictionaryGetInt64Ranged( CFDictionaryRef inDict, const void *inKey, int64_t inMin, int64_t inMax, OSStatus *outErr )
{
	int64_t			x;
	CFTypeRef		value;
	
	value = inDict ? CFDictionaryGetValue( inDict, inKey ) : NULL;
	if( value )
	{
		x = CFGetInt64Ranged( value, inMin, inMax, outErr );
	}
	else
	{
		x = 0;
		if( outErr ) *outErr = kNotFoundErr;
	}
	return( x );
}

//===========================================================================================================================
//	CFDictionarySetInt64
//===========================================================================================================================

OSStatus	CFDictionarySetInt64( CFMutableDictionaryRef inDict, const void *inKey, int64_t x )
{
	OSStatus		err;
	CFNumberRef		tempNum;
	
	tempNum = CFNumberCreateInt64( x );
	require_action( tempNum, exit, err = kNoMemoryErr );
	
	CFDictionarySetValue( inDict, inKey, tempNum );
	CFRelease( tempNum );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFDictionaryGetNestedValue
//===========================================================================================================================

const void *	CFDictionaryGetNestedValue( CFDictionaryRef inDict, CFStringRef inKey, ... )
{
	const void *		value;
	va_list				args;
	
	va_start( args, inKey );
	for( ;; )
	{
		value = CFDictionaryGetValue( inDict, inKey );
		if( !value ) break;
		
		inKey = va_arg( args, CFStringRef );
		if( !inKey ) break;
		
		inDict = (CFDictionaryRef) value;
		require_action( CFGetTypeID( inDict ) == CFDictionaryGetTypeID(), exit, value = NULL );
	}
	
exit:
	va_end( args );
	return( value );
}

//===========================================================================================================================
//	CFDictionarySetNumber
//===========================================================================================================================

OSStatus	CFDictionarySetNumber( CFMutableDictionaryRef inDict, const void *inKey, CFNumberType inType, void *inValue )
{
	OSStatus		err;
	CFNumberRef		tempNum;
	
	tempNum = CFNumberCreate( NULL, inType, inValue );
	require_action( tempNum, exit, err = kUnknownErr );
	
	CFDictionarySetValue( inDict, inKey, tempNum );
	CFRelease( tempNum );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFDictionaryGetTypedValue
//===========================================================================================================================

CFTypeRef	CFDictionaryGetTypedValue( CFDictionaryRef inDict, const void *inKey, CFTypeID inType, OSStatus *outErr )
{
	CFTypeRef		obj = NULL;
	CFTypeRef		tempObj;
	OSStatus		err;
	
	require_action_quiet( inDict, exit, err = kParamErr );
	
	tempObj = CFDictionaryGetValue( inDict, inKey );
	require_action_quiet( tempObj, exit, err = kNotFoundErr );
	require_action_quiet( CFGetTypeID( tempObj ) == inType, exit, err = kTypeErr );
	
	obj = tempObj;
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( obj );
}

//===========================================================================================================================
//	CFDictionaryGetUUIDEx
//===========================================================================================================================

OSStatus	CFDictionaryGetUUIDEx( CFDictionaryRef inDict, const void *inKey, const uint8_t *inBaseUUID, uint8_t outUUID[ 16 ] )
{
	OSStatus		err;
	CFTypeRef		value;
	
	require_action_quiet( inDict, exit, err = kParamErr );
	
	value = CFDictionaryGetValue( inDict, inKey );
	require_action_quiet( value, exit, err = kNotFoundErr );
	
	err = CFGetUUIDEx( value, inBaseUUID, outUUID );
	require_noerr_quiet( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFDictionarySetUUIDString
//===========================================================================================================================

OSStatus
	CFDictionarySetUUIDString( 
		CFMutableDictionaryRef	inDict, 
		const void *			inKey, 
		const void *			inUUID, 
		size_t					inSize, 
		const void *			inBaseUUID, 
		uint32_t				inFlags )
{
	OSStatus		err;
	CFStringRef		str;
	
	str = CFCreateUUIDString( inUUID, inSize, inBaseUUID, inFlags, &err );
	require_noerr( err, exit );
	
	CFDictionarySetValue( inDict, inKey, str );
	CFRelease( str );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFNumberCreateInt64
//===========================================================================================================================

CFNumberRef	CFNumberCreateInt64( int64_t x )
{
	int8_t			s8;
	int16_t			s16;
	int32_t			s32;
	CFNumberType	type;
	void *			ptr;
	
	if(      ( x >= INT8_MIN )  && ( x <= INT8_MAX ) )  { s8  = (int8_t)  x; type = kCFNumberSInt8Type;  ptr = &s8; }
	else if( ( x >= INT16_MIN ) && ( x <= INT16_MAX ) ) { s16 = (int16_t) x; type = kCFNumberSInt16Type; ptr = &s16; }
	else if( ( x >= INT32_MIN ) && ( x <= INT32_MAX ) ) { s32 = (int32_t) x; type = kCFNumberSInt32Type; ptr = &s32; }
	else												{					 type = kCFNumberSInt64Type; ptr = &x; }
	
	return( CFNumberCreate( NULL, type, ptr ) );
}

//===========================================================================================================================
//	CFNumberGetObject
//===========================================================================================================================

static pthread_mutex_t		gCFNumberGetObjectLock = PTHREAD_MUTEX_INITIALIZER;
static CFNumberRef			gCFNumberArray[ kCFNumberGetObjectMaxValue + 1 ];

CFNumberRef	CFNumberGetObject( uint32_t inValue )
{
	CFNumberRef		obj;
	
	require_action( inValue <= kCFNumberGetObjectMaxValue, exit, obj = NULL );
	
	pthread_mutex_lock( &gCFNumberGetObjectLock );
	obj = gCFNumberArray[ inValue ];
	if( !obj )
	{
		obj = CFNumberCreateUInt64( inValue );
		check( obj );
		if( obj ) gCFNumberArray[ inValue ] = obj;
	}
	pthread_mutex_unlock( &gCFNumberGetObjectLock );
	
exit:
	return( obj );
}

//===========================================================================================================================
//	CFStringCreateComponentsSeparatedByString
//===========================================================================================================================

CFArrayRef	CFStringCreateComponentsSeparatedByString( CFStringRef inString, CFStringRef inSeparator )
{
	CFMutableArrayRef		array;
	OSStatus				err;
	const char *			sourceStr;
	char *					sourceStorage;
	const char *			separatorStr;
	char *					separatorStorage;
	size_t					separatorLen;
	const char *			ptr;
	CFStringRef				componentStr;
	
	sourceStorage    = NULL;
	separatorStorage = NULL;
	
	array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require( array, exit );
	
	err = CFStringGetOrCopyCStringUTF8( inString, &sourceStr, &sourceStorage, NULL );
	require_noerr( err, exit );
	
	err = CFStringGetOrCopyCStringUTF8( inSeparator, &separatorStr, &separatorStorage, &separatorLen );
	require_noerr( err, exit );
	
	for( ;; )
	{
		ptr = strstr( sourceStr, separatorStr );
		if( ptr )
		{
			componentStr = CFStringCreateWithBytes( NULL, (const uint8_t *) sourceStr, (CFIndex)( ptr - sourceStr ), 
				kCFStringEncodingUTF8, false );
			require( componentStr, exit );
			
			sourceStr = ptr + separatorLen;
		}
		else
		{
			componentStr = CFStringCreateWithCString( NULL, sourceStr, kCFStringEncodingUTF8 );
			require( componentStr, exit );
		}
		
		CFArrayAppendValue( array, componentStr );
		CFRelease( componentStr );
		if( !ptr ) break;
	}
	
exit:
	if( sourceStorage )		free( sourceStorage );
	if( separatorStorage )	free( separatorStorage );
	return( array );
}

//===========================================================================================================================
//	CFStringCreateF
//===========================================================================================================================

CFStringRef	CFStringCreateF( OSStatus *outErr, const char *inFormat, ... )
{
	CFStringRef		result;
	va_list			args;
	
	va_start( args, inFormat );
	result = CFStringCreateV( outErr, inFormat, args );
	va_end( args );
	return( result );
}

//===========================================================================================================================
//	CFStringCreateV
//===========================================================================================================================

CFStringRef	CFStringCreateV( OSStatus *outErr, const char *inFormat, va_list inArgs )
{
	CFStringRef		result	= NULL;
	char *			cbuf	= NULL;
	OSStatus		err;
	int				n;
	
	n = VASPrintF( &cbuf, inFormat, inArgs );
	require_action( n >= 0, exit, err = kUnknownErr );
	
	result = CFStringCreateWithCString( NULL, cbuf, kCFStringEncodingUTF8 );
	free( cbuf );
	require_action( result, exit, err = kNoMemoryErr );
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	CFStringAppendF
//===========================================================================================================================

OSStatus	CFStringAppendF( CFMutableStringRef inStr, const char *inFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = CFStringAppendV( inStr, inFormat, args );
	va_end( args );
	return( err );
}

//===========================================================================================================================
//	CFStringAppendV
//===========================================================================================================================

OSStatus	CFStringAppendV( CFMutableStringRef inStr, const char *inFormat, va_list inArgs )
{
	OSStatus		err;
	char *			cbuf = NULL;
	int				n;
	
	n = VASPrintF( &cbuf, inFormat, inArgs );
	require_action( n >= 0, exit, err = kUnknownErr );
	
	CFStringAppendCString( inStr, cbuf, kCFStringEncodingUTF8 );
	free( cbuf );
	err = kNoErr;
	
exit:
	return( err );
}
//===========================================================================================================================
//	CFStringCopyUTF8CString
//===========================================================================================================================

OSStatus	CFStringCopyUTF8CString( CFStringRef inString, char **outUTF8 )
{
	OSStatus			err;
	const char *		src;
	CFRange				range;
	CFIndex				size;
	uint8_t *			utf8;
	
	utf8 = NULL;
	
	src = CFStringGetCStringPtr( inString, kCFStringEncodingUTF8 );
	if( src )
	{
		utf8 = (uint8_t *) strdup( src );
		require_action( utf8, exit, err = kNoMemoryErr );
	}
	else
	{
		range = CFRangeMake( 0, CFStringGetLength( inString ) );
		size = CFStringGetMaximumSizeForEncoding( range.length, kCFStringEncodingUTF8 );
		
		utf8 = (uint8_t *) malloc( (size_t)( size + 1 ) );
		require_action( utf8, exit, err = kNoMemoryErr );
		
		range.location = CFStringGetBytes( inString, range, kCFStringEncodingUTF8, 0, false, utf8, size, &size );
		require_action( range.location == range.length, exit, err = kUnknownErr );
		
		utf8[ size ] = '\0';
	}
	
	*outUTF8 = (char *) utf8; utf8 = NULL;
	err = kNoErr;
	
exit:
	if( utf8 ) free( utf8 );
	return( err );
}

//===========================================================================================================================
//	CFStringGetOrCopyCStringUTF8
//===========================================================================================================================

OSStatus	CFStringGetOrCopyCStringUTF8( CFStringRef inString, const char **outUTF8, char **outStorage, size_t *outLen )
{
	OSStatus			err;
	const char *		ptr;
	uint8_t *			storage = NULL;
	CFRange				range;
	CFIndex				size;
	
	ptr = CFStringGetCStringPtr( inString, kCFStringEncodingUTF8 );
	if( !ptr )
	{
		range = CFRangeMake( 0, CFStringGetLength( inString ) );
		size = CFStringGetMaximumSizeForEncoding( range.length, kCFStringEncodingUTF8 );
		
		storage = (uint8_t *) malloc( (size_t)( size + 1 ) );
		require_action( storage, exit, err = kNoMemoryErr );
		
		range.location = CFStringGetBytes( inString, range, kCFStringEncodingUTF8, 0, false, storage, size, &size );
		require_action( range.location == range.length, exit, err = kUnknownErr );
		
		storage[ size ] = '\0';
		ptr = (const char *) storage;
		if( outLen ) *outLen = (size_t) size;
	}
	else if( outLen )
	{
		*outLen = strlen( ptr );
	}
	
	*outUTF8	= ptr;
	*outStorage = (char *) storage;
	storage		= NULL;
	err			= kNoErr;
	
exit:
	FreeNullSafe( storage );
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Misc ==
#endif

//===========================================================================================================================
//	MapCFStringToValue
//===========================================================================================================================

int	MapCFStringToValue( CFStringRef inString, int inDefaultValue, ... )
{
	va_list			args;
	CFStringRef		str;
	int				val;
	int				x;
	
	check( inString );
	
	val = inDefaultValue;
	va_start( args, inDefaultValue );
	for( ;; )
	{
		str = va_arg( args, CFStringRef );
		if( !str ) break;
		
		x = va_arg( args, int );
		if( CFEqual( inString, str ) )
		{
			val = x;
			break;
		}
	}
	va_end( args );
	return( val );
}

//===========================================================================================================================
//	MapValueToCFString
//===========================================================================================================================

CFStringRef	MapValueToCFString( int inValue, CFStringRef inDefaultStr, ... )
{
	va_list			args;
	CFStringRef		mappedStr;
	CFStringRef		str;
	int				val;
	
	mappedStr = inDefaultStr;
	va_start( args, inDefaultStr );
	for( ;; )
	{
		str = va_arg( args, CFStringRef );
		if( !str ) break;
		
		val = va_arg( args, int );
		if( inValue == val )
		{
			mappedStr = str;
			break;
		}
	}
	va_end( args );
	return( mappedStr );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )

#pragma GCC diagnostic ignored "-Wfloat-equal"

OSStatus	CFUtilsTestCFObjectAccessors( void );

//===========================================================================================================================
//	CFUtilsTest
//===========================================================================================================================

OSStatus	CFUtilsTest( int inPrint )
{
	OSStatus					err;
	uint8_t *					p;
	uint32_t					ip;
	CFMutableDictionaryRef		plist;
	CFMutableDictionaryRef		plist2;
	CFArrayRef					array;
	CFBooleanRef				boolObj;
	CFDataRef					data, data2;
	CFDictionaryRef				dict;
	CFNumberRef					num;
	CFStringRef					str;
	CFStringRef					value;
	CFMutableStringRef			mstr;
	int							x;
	double						d;
	const char *				utf8Ptr;
	char *						utf8;
	CFStringRef					value28;
	uint8_t						buf[ 128 ];
	int8_t						s8;
	uint8_t						u8;
	int16_t						s16;
	uint16_t					u16;
	int32_t						s32;
	uint32_t					u32;
	int64_t						s64;
	uint64_t					u64;
	char						tempStr[ 64 ];
	const uint8_t *				cu8Ptr;
	size_t						len;
	
	//
	// Formatted Building
	//

	p = (uint8_t *) &ip;
	p[ 0 ] = 10;
	p[ 1 ] = 0;
	p[ 2 ] = 1;
	p[ 3 ] = 255;
	
	value28 = CFSTR( "value28" );
	
	err = CFPropertyListCreateFormatted( kCFAllocatorDefault, &plist, 
		"{"
			"key 1=value 1;"							//  Inline Key/Inline Value
			"key 2=value 2;"							//  Inline Key/Inline Value
			"%ks=%s"									//  3 Key String/String
			"%.*ks=%.*s"								//  4 Key String/String
			"%.5ks=%.7s"								//  5 Key String/String
			"%kC=%C"									//  6 Key FourCharCode/FourCharCode
			"%kO=%i"									//  7 Key CFString/Integer
			"key 8=%D"									//  8 Inline Key/Data
			"key 9=%b"									//  9 Inline Key/Boolean
			"key 10=%O"									// 10 Inline Key/CFString
			"key 11=%.4a" 								// 11 Inline Key/IPv4 Address
			"key 12=%.6a" 								// 12 Inline Key/MAC Address
			"key 13=%.8a" 								// 13 Inline Key/Fibre Channel Address
			"key 14=value 14;"							// Inline Key/Inline Value
			"key 15="
			"["
				"%s"									// 16 String
				"%.*s"									// 17 String
				"%O"									// 18 CFString
			"]"
			"key 19="
			"{"
				"%ks="
				"{"										// 20 Key String
					"key 21="
					"{"
						"key 22="
						"["
							"%i"						// 22
							"%i"						// 23
							"%i"						// 24
						"]"
					"}"
					"key 25=value 25;"
					"key 26=%T"							// 26
					"key 27=%D"							// 27
				"}"
			"}"
			"key 28=%#O"								// 28
			"key 29=%D"									// 29
			"key 30=1.2.3b4;"							// 30
			"key 31=%lli"								// 31
			"key 32=%s"									// 32
			"key 33=%s"									// 33
			"key 34=%D"									// 34
			"key 35=%i"									// 35
			"key 36=%U"									// 36
			"key 37=%i"									// 37
			"key 38=%i"									// 38
			"key 39=%s"									// 39
			"key 40=%O"									// 40
			"key 41=%D"									// 41
			"key 42=%i"									// 42
			"key 43=%i"									// 43
			"key 44=%?s"								// 44
			"key 45=%?s"								// 45
			"key 46=%?i"								// 46
			"key 47=%?i"								// 47
			"key 48=%#D"								// 48 CFData from CFPropertyListRef
		"}", 
		
		"key 3", "value 3", 							//  3
		5, "key 4xx", 7, "value 4**",					//  4
		"key 5xx", "value 5**",							//  5
		(uint32_t) 0x6B657936, (uint32_t) 0x76616C36,	//  6 'key6'/'val6'
		CFSTR( "key 7" ), 7777777,						//  7
		"\x11\x22\x33\x44\x55\x66\x77\x88"				//  8
		"\x99\xAA\xBB\xCC\xDD\xEE\xFF\x00" 
		"abcd1234", 24,			
		true,											//  9
		CFSTR( "value 10" ), 							// 10
		"\xC0\xA8\x00\x01", 							// 11 192.168.0.1
		"\x22\x33\x44\x55\x66\x77", 					// 12 22:33:44:55:66:77
		"\x11\x22\x33\x44\x55\x66\x77\x88", 			// 13 11:22:33:44:55:66:77:88
		"value 16",										// 16
		8, "value 17--",								// 17
		CFSTR( "value 18" ), 							// 18
		"key 20", 										// 20
		-123, 											// 22
		0x7FFFFFFF, 									// 23
		123456789,										// 24
		2006, 5, 25, 7, 55, 37, 						// 26
		"abcd12345", 9,									// 27
		value28,										// 28
		"\x00\x11\x22\x33\x44\x55", 6,					// 29
		INT64_C( 1234567890123 ),						// 31
		"11:22:33:44:55:66",							// 32
		"6ba7b810-9dad-11d1-80b4-00c04fd430c8",			// 33
		"\x6b\xa7\xb8\x10\x9d\xad\x11\xd1\x80\xb4\x00\xc0\x4f\xd4\x30\xc8", 16, // 34
		12345, 											// 35
		"\x81\x29\xb4\xb2\x86\xdd\x4f\x40\x95\x1f\x6b\xe8\x34\xda\x5b\x8e",	// 36
		123, 											// 37
		(uint32_t) 0x76613338, 							// 38 'va38'
		"va39",				 							// 39
		CFSTR( "00:11:22:33:44:55" ),					// 40
		"\x00\x11\x22\x33\x44\x55", 6,					// 41
		123,											// 42
		12345,											// 43
		0, "value 44",									// 44
		1, "value 45",									// 45
		0, 46,											// 46
		1, 47,											// 47
		CFSTR( "xy" )									// 48
	);
	require_noerr( err, exit );
	
	if( inPrint ) FPrintF( stderr, "%@\n", plist );

	// CFNumberCreateInt64
	
	num = CFNumberCreateInt64( 0 );
	require_action( num, exit, err = kNoMemoryErr );
	CFNumberGetValue( num, kCFNumberSInt64Type, &s64 );
	CFRelease( num );
	require_action( s64 == 0, exit, err = kMismatchErr );
	
	num = CFNumberCreateInt64( -123 );
	require_action( num, exit, err = kNoMemoryErr );
	CFNumberGetValue( num, kCFNumberSInt64Type, &s64 );
	CFRelease( num );
	require_action( s64 == -123, exit, err = kMismatchErr );
	
	num = CFNumberCreateInt64( 100000 );
	require_action( num, exit, err = kNoMemoryErr );
	CFNumberGetValue( num, kCFNumberSInt64Type, &s64 );
	CFRelease( num );
	require_action( s64 == 100000, exit, err = kMismatchErr );
	
	num = CFNumberCreateInt64( INT64_MIN );
	require_action( num, exit, err = kNoMemoryErr );
	CFNumberGetValue( num, kCFNumberSInt64Type, &s64 );
	CFRelease( num );
	require_action( s64 == INT64_MIN, exit, err = kMismatchErr );
	
	num = CFNumberCreateInt64( INT64_MAX );
	require_action( num, exit, err = kNoMemoryErr );
	CFNumberGetValue( num, kCFNumberSInt64Type, &s64 );
	CFRelease( num );
	require_action( s64 == INT64_MAX, exit, err = kMismatchErr );
	
	// CFNumberCreateUInt64
	
	num = CFNumberCreateUInt64( 0 );
	require_action( num, exit, err = kNoMemoryErr );
	CFNumberGetValue( num, kCFNumberSInt64Type, &u64 );
	CFRelease( num );
	require_action( u64 == 0, exit, err = kMismatchErr );
	
	num = CFNumberCreateUInt64( 123 );
	require_action( num, exit, err = kNoMemoryErr );
	CFNumberGetValue( num, kCFNumberSInt64Type, &u64 );
	CFRelease( num );
	require_action( u64 == 123, exit, err = kMismatchErr );
	
	num = CFNumberCreateUInt64( 100000 );
	require_action( num, exit, err = kNoMemoryErr );
	CFNumberGetValue( num, kCFNumberSInt64Type, &u64 );
	CFRelease( num );
	require_action( u64 == 100000, exit, err = kMismatchErr );
	
	num = CFNumberCreateUInt64( UINT32_MAX );
	require_action( num, exit, err = kNoMemoryErr );
	CFNumberGetValue( num, kCFNumberSInt64Type, &u64 );
	CFRelease( num );
	require_action( u64 == UINT32_MAX, exit, err = kMismatchErr );
	
	num = CFNumberCreateUInt64( UINT64_MAX );
	require_action( num, exit, err = kNoMemoryErr );
	CFNumberGetValue( num, kCFNumberSInt64Type, &u64 );
	CFRelease( num );
	require_action( u64 == UINT64_MAX, exit, err = kMismatchErr );
	
	// CFNumberGetObject
	
	for( u16 = kCFNumberGetObjectMinValue; u16 <= kCFNumberGetObjectMaxValue; ++u16 )
	{
		num = CFNumberGetObject( u16 );
		require_action( num, exit, err = kNoMemoryErr );
		u64 = 100;
		CFNumberGetValue( num, kCFNumberSInt64Type, &u64 );
		require_action( u64 == u16, exit, err = kMismatchErr );
	}
	
	// Repeat to test the already-allocated path and make sure there are no leaks, etc.
	
	for( u16 = kCFNumberGetObjectMinValue; u16 <= kCFNumberGetObjectMaxValue; ++u16 )
	{
		num = CFNumberGetObject( u16 );
		require_action( num, exit, err = kNoMemoryErr );
		u64 = 100;
		CFNumberGetValue( num, kCFNumberSInt64Type, &u64 );
		require_action( u64 == u16, exit, err = kMismatchErr );
	}
	
	// CFStringCreateF
	
	str = CFStringCreateF( &err, "Test" );
	require_noerr( err, exit );
	require_action( str, exit, err = kResponseErr );
	require_action( CFEqual( str, CFSTR( "Test" ) ), exit, err = kMismatchErr );
	CFRelease( str );
	
	str = CFStringCreateF( &err, "Test %d", 123 );
	require_noerr( err, exit );
	require_action( str, exit, err = kResponseErr );
	require_action( CFEqual( str, CFSTR( "Test 123" ) ), exit, err = kMismatchErr );
	CFRelease( str );
	
	// CFStringAppendF
	
	mstr = CFStringCreateMutable( NULL, 0 );
	require_action( mstr, exit, err = kNoMemoryErr );
	CFStringAppendCString( mstr, "Prefix-", kCFStringEncodingUTF8 );
	err = CFStringAppendF( mstr, "Test" );
	require_noerr( err, exit );
	require_action( mstr, exit, err = kResponseErr );
	require_action( CFEqual( mstr, CFSTR( "Prefix-Test" ) ), exit, err = kMismatchErr );
	CFRelease( mstr );
	
	mstr = CFStringCreateMutable( NULL, 0 );
	require_action( mstr, exit, err = kNoMemoryErr );
	CFStringAppendCString( mstr, "Prefix-", kCFStringEncodingUTF8 );
	err = CFStringAppendF( mstr, "Test %@", CFSTR( "abc" ) );
	require_noerr( err, exit );
	require_action( mstr, exit, err = kResponseErr );
	require_action( CFEqual( mstr, CFSTR( "Prefix-Test abc" ) ), exit, err = kMismatchErr );
	CFRelease( mstr );
	
	// CFStringCopyUTF8CString
	
	value = CFStringCreateWithCString( kCFAllocatorDefault, "testing", kCFStringEncodingUTF8 );
	require_action( value, exit, err = kNoMemoryErr );
	
	err = CFStringCopyUTF8CString( value, &utf8 );
	CFRelease( value );
	require_noerr( err, exit );
	require_action( strcmp( utf8, "testing" ) == 0, exit, err = kResponseErr );
	free( utf8 );
	
	value = CFStringCreateWithCString( kCFAllocatorDefault, "\xE8\xA1\x8C\xE3\x81\x8D\xE3\x81\xAA\xE3\x81\x95\xE3\x81\x84", 
		kCFStringEncodingUTF8 );
	require_action( value, exit, err = kNoMemoryErr );
	
	utf8 = NULL;
	err = CFStringCopyUTF8CString( value, &utf8 );
	CFRelease( value );
	require_noerr( err, exit );
	require_action( strcmp( utf8, "\xE8\xA1\x8C\xE3\x81\x8D\xE3\x81\xAA\xE3\x81\x95\xE3\x81\x84" ) == 0, exit, err = kResponseErr );
	free( utf8 );
	
	// CFStringGetOrCopyCStringUTF8
	
	utf8Ptr = NULL;
	utf8 = NULL;
	err = CFStringGetOrCopyCStringUTF8( CFSTR( "testing" ), &utf8Ptr, &utf8, NULL );
	require_noerr( err, exit );
	require_action( utf8Ptr, exit, err = kResponseErr );
	require_action( strcmp( utf8Ptr, "testing" ) == 0, exit, err = kResponseErr );
	if( utf8 ) free( utf8 );
	
	// MapCFStringToValue
	
	x = MapCFStringToValue( CFSTR( "red" ), -1, 
		CFSTR( "red" ),   1, 
		CFSTR( "green" ), 2, 
		CFSTR( "blue" ),  3, 
		NULL );
	require_action( x == 1, exit, err = kResponseErr );
	
	x = MapCFStringToValue( CFSTR( "green" ), -1, 
		CFSTR( "red" ),   1, 
		CFSTR( "green" ), 2, 
		CFSTR( "blue" ),  3, 
		NULL );
	require_action( x == 2, exit, err = kResponseErr );
	
	x = MapCFStringToValue( CFSTR( "blue" ), -1, 
		CFSTR( "red" ),   1, 
		CFSTR( "green" ), 2, 
		CFSTR( "blue" ),  3, 
		NULL );
	require_action( x == 3, exit, err = kResponseErr );
	
	x = MapCFStringToValue( CFSTR( "orange" ), -1, 
		CFSTR( "red" ),   1, 
		CFSTR( "green" ), 2, 
		CFSTR( "blue" ),  3, 
		NULL );
	require_action( x == -1, exit, err = kResponseErr );
	
	x = MapCFStringToValue( CFSTR( "" ), -1, 
		CFSTR( "red" ),   1, 
		CFSTR( "green" ), 2, 
		CFSTR( "blue" ),  3, 
		NULL );
	require_action( x == -1, exit, err = kResponseErr );
	
	x = MapCFStringToValue( CFSTR( "test" ), -1, 
		NULL );
	require_action( x == -1, exit, err = kResponseErr );
	
	x = MapCFStringToValue( CFSTR( "test" ), -1, 
		CFSTR( "test" ), 123, 
		NULL );
	require_action( x == 123, exit, err = kResponseErr );
	
	// MapValueToCFString
	
	str = MapValueToCFString( 1, NULL, 
		CFSTR( "red" ),		1, 
		CFSTR( "green" ),	2, 
		CFSTR( "blue" ),	3, 
		NULL );
	require_action( str && ( CFStringCompare( str, CFSTR( "red" ), 0 ) == kCFCompareEqualTo ), exit, err = kResponseErr );
	
	str = MapValueToCFString( 2, NULL, 
		CFSTR( "red" ),		1, 
		CFSTR( "green" ),	2, 
		CFSTR( "blue" ),	3, 
		NULL );
	require_action( str && ( CFStringCompare( str, CFSTR( "green" ), 0 ) == kCFCompareEqualTo ), exit, err = kResponseErr );
	
	str = MapValueToCFString( 3, NULL, 
		CFSTR( "red" ),		1, 
		CFSTR( "green" ),	2, 
		CFSTR( "blue" ),	3, 
		NULL );
	require_action( str && ( CFStringCompare( str, CFSTR( "blue" ), 0 ) == kCFCompareEqualTo ), exit, err = kResponseErr );
	
	str = MapValueToCFString( 4, NULL, 
		CFSTR( "red" ),		1, 
		CFSTR( "green" ),	2, 
		CFSTR( "blue" ),	3, 
		NULL );
	require_action( str == NULL, exit, err = kResponseErr );
	
	// CFGetBoolean
	
	err = -1;
	require_action( CFGetBoolean( kCFBooleanTrue, &err ) == true, exit, err = -1 );
	require_noerr( err, exit );
	err = -1;
	require_action( CFGetBoolean( CFSTR( "true" ), &err ) == true, exit, err = -1 );
	require_noerr( err, exit );
	err = -1;
	require_action( CFGetBoolean( CFSTR( "Yes" ), &err ) == true, exit, err = -1 );
	require_noerr( err, exit );
	err = -1;
	require_action( CFGetBoolean( CFSTR( "y" ), &err ) == true, exit, err = -1 );
	require_noerr( err, exit );
	require_action( CFGetBoolean( CFSTR( "1" ), NULL ) == true, exit, err = -1 );
	
	err = -1;
	require_action( CFGetBoolean( kCFBooleanFalse, &err ) == false, exit, err = -1 );
	require_noerr( err, exit );
	err = -1;
	require_action( CFGetBoolean( CFSTR( "FALSE" ), &err ) == false, exit, err = -1 );
	require_noerr( err, exit );
	err = -1;
	require_action( CFGetBoolean( CFSTR( "no" ), &err ) == false, exit, err = -1 );
	require_noerr( err, exit );
	err = -1;
	require_action( CFGetBoolean( CFSTR( "n" ), &err ) == false, exit, err = -1 );
	require_noerr( err, exit );
	require_action( CFGetBoolean( CFSTR( "0" ), NULL ) == false, exit, err = -1 );
	
	// CFGetCString
	
	*tempStr = '\0';
	str = CFStringCreateWithCString( NULL, "test", kCFStringEncodingUTF8 );
	require_action( str, exit, err = -1 );
	CFGetCString( str, tempStr, sizeof( tempStr ) );
	CFRelease( str );
	require_action( strcmp( tempStr, "test" ) == 0, exit, err = -1 );
	
	*tempStr = '\0';
	x = 12345678;
	num = CFNumberCreate( NULL, kCFNumberIntType, &x );
	require_action( str, exit, err = -1 );
	CFGetCString( num, tempStr, sizeof( tempStr ) );
	CFRelease( num );
	require_action( strcmp( tempStr, "12345678" ) == 0, exit, err = -1 );
	
	*tempStr = '\0';
	d = 12345.6789;
	num = CFNumberCreate( NULL, kCFNumberDoubleType, &d );
	require_action( num, exit, err = -1 );
	CFGetCString( num, tempStr, sizeof( tempStr ) );
	CFRelease( num );
	require_action( strcmp( tempStr, "12345.678900" ) == 0, exit, err = -1 );
	
	*tempStr = '\0';
	CFGetCString( kCFBooleanTrue, tempStr, sizeof( tempStr ) );
	require_action( strcmp( tempStr, "true" ) == 0, exit, err = -1 );
	
	*tempStr = '\0';
	CFGetCString( kCFBooleanFalse, tempStr, sizeof( tempStr ) );
	require_action( strcmp( tempStr, "false" ) == 0, exit, err = -1 );
	
	*tempStr = '\0';
	CFGetCString( NULL, tempStr, sizeof( tempStr ) );
	require_action( strcmp( tempStr, "" ) == 0, exit, err = -1 );
	
	data = CFDataCreate( NULL, (const uint8_t *) "\x11\x22\x33\x44\x55\x66\xaa\xbb\xcc\xdd", 10 );
	require_action( data, exit, err = -1 );
	CFGetCString( data, tempStr, sizeof( tempStr ) );
	CFRelease( data );
	require_action( strcmp( tempStr, "112233445566aabbccdd" ) == 0, exit, err = -1 );
	
	// CFCopyCFData
	
	data = CFDataCreate( NULL, (const uint8_t *) "\x11\x22\x33\x44\x55\x66\xaa\xbb\xcc\xdd", 10 );
	require_action( data, exit, err = -1 );
	data2 = CFCopyCFData( data, &len, &err );
	CFRelease( data );
	require_noerr( err, exit );
	require_action( data2, exit, err = -1 );
	require_action( len == 10, exit, err = -1 );
	cu8Ptr = CFDataGetBytePtr( data2 );
	len = (size_t) CFDataGetLength( data2 );
	require_action( len == 10, exit, err = -1 );
	require_action( memcmp( cu8Ptr, "\x11\x22\x33\x44\x55\x66\xaa\xbb\xcc\xdd", len ) == 0, exit, err = -1 );
	CFRelease( data2 );
	
	data2 = CFCopyCFData( CFSTR( "112233445566aabbccdd" ), &len, &err );
	require_noerr( err, exit );
	require_action( data2, exit, err = -1 );
	require_action( len == 10, exit, err = -1 );
	cu8Ptr = CFDataGetBytePtr( data2 );
	len = (size_t) CFDataGetLength( data2 );
	require_action( len == 10, exit, err = -1 );
	require_action( memcmp( cu8Ptr, "\x11\x22\x33\x44\x55\x66\xaa\xbb\xcc\xdd", len ) == 0, exit, err = -1 );
	CFRelease( data2 );
	
	data2 = CFCopyCFData( CFSTR( "0x11 0x22 0x33 0x44 0x55 0x66 0xaa 0xbb 0xcc 0xdd" ), &len, &err );
	require_noerr( err, exit );
	require_action( data2, exit, err = -1 );
	require_action( len == 10, exit, err = -1 );
	cu8Ptr = CFDataGetBytePtr( data2 );
	len = (size_t) CFDataGetLength( data2 );
	require_action( len == 10, exit, err = -1 );
	require_action( memcmp( cu8Ptr, "\x11\x22\x33\x44\x55\x66\xaa\xbb\xcc\xdd", len ) == 0, exit, err = -1 );
	CFRelease( data2 );
	
	data2 = CFCopyCFData( CFSTR( "" ), &len, &err );
	require_noerr( err, exit );
	require_action( data2, exit, err = -1 );
	require_action( len == 0, exit, err = -1 );
	require_action( CFDataGetLength( data2 ) == 0, exit, err = -1 );
	CFRelease( data2 );
	
	// CFGetData
	
	data = CFDataCreate( NULL, (const uint8_t *) "\x11\x22\x33\x44\x55\x66\xaa\xbb\xcc\xdd", 10 );
	require_action( data, exit, err = -1 );
	memset( buf, 'z', sizeof( buf ) );
	len = 0;
	CFGetData( data, buf, sizeof( buf ), &len, NULL );
	CFRelease( data );
	require_action( len == 10, exit, err = -1 );
	require_action( memcmp( buf, "\x11\x22\x33\x44\x55\x66\xaa\xbb\xcc\xdd", len ) == 0, exit, err = -1 );
	require_action( buf[ len ] == 'z', exit, err = -1 );
	
	data = CFDataCreate( NULL, (const uint8_t *) "\x11\x22\x33\x44\x55\x66\xaa\xbb\xcc\xdd", 10 );
	require_action( data, exit, err = -1 );
	memset( buf, 'z', sizeof( buf ) );
	len = 0;
	CFGetData( data, buf, 10, &len, NULL );
	CFRelease( data );
	require_action( len == 10, exit, err = -1 );
	require_action( memcmp( buf, "\x11\x22\x33\x44\x55\x66\xaa\xbb\xcc\xdd", len ) == 0, exit, err = -1 );
	require_action( buf[ len ] == 'z', exit, err = -1 );
	
	data = CFDataCreate( NULL, (const uint8_t *) "\x11\x22\x33\x44\x55\x66\xaa\xbb\xcc\xdd", 10 );
	require_action( data, exit, err = -1 );
	len = 0;
	memset( buf, 'z', sizeof( buf ) );
	CFGetData( data, buf, 8, &len, NULL );
	CFRelease( data );
	require_action( len == 8, exit, err = -1 );
	require_action( memcmp( buf, "\x11\x22\x33\x44\x55\x66\xaa\xbb", len ) == 0, exit, err = -1 );
	require_action( buf[ len ] == 'z', exit, err = -1 );
	
	data = CFDataCreate( NULL, (const uint8_t *) "\x11\x22\x33\x44\x55\x66\xaa\xbb\xcc\xdd", 10 );
	require_action( data, exit, err = -1 );
	memset( buf, 'z', sizeof( buf ) );
	len = 0;
	CFGetData( data, buf, 0, &len, NULL );
	CFRelease( data );
	require_action( len == 0, exit, err = -1 );
	require_action( buf[ len ] == 'z', exit, err = -1 );
	
	memset( buf, 'z', sizeof( buf ) );
	len = 0;
	CFGetData( CFSTR( "00112233" ), buf, sizeof( buf ), &len, NULL );
	require_action( len == 4, exit, err = -1 );
	require_action( memcmp( buf, "\x00\x11\x22\x33", len ) == 0, exit, err = -1 );
	require_action( buf[ len ] == 'z', exit, err = -1 );
	
	memset( buf, 'z', sizeof( buf ) );
	len = 0;
	CFGetData( CFSTR( "" ), buf, sizeof( buf ), &len, NULL );
	require_action( len == 0, exit, err = -1 );
	require_action( buf[ len ] == 'z', exit, err = -1 );
	
	memset( buf, 'z', sizeof( buf ) );
	len = 0;
	CFGetData( CFSTR( "00:11:22:33" ), buf, sizeof( buf ), &len, NULL );
	require_action( len == 4, exit, err = -1 );
	require_action( memcmp( buf, "\x00\x11\x22\x33", len ) == 0, exit, err = -1 );
	require_action( buf[ len ] == 'z', exit, err = -1 );
	
	// CFGetDouble
	
	err = -1;
	require_action( CFGetDouble( CFSTR( "true" ), &err ) == 1, exit, err = -1 );
	require_noerr( err, exit );
	err = -1;
	require_action( CFGetDouble( CFSTR( "123" ), &err ) == 123, exit, err = -1 );
	require_noerr( err, exit );
	err = -1;
	require_action( CFGetDouble( CFSTR( "123.45" ), &err ) == 123.45, exit, err = -1 );
	require_noerr( err, exit );
	
	// CFGetHardwareAddress
	
	memset( buf, 'z', sizeof( buf ) );
	err = -1;
	u64 = CFGetMACAddress( CFSTR( "00:11:22:aa:bb:cc" ), buf, &err );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 0x001122aabbcc ), exit, err = -1 );
	require_action( memcmp( buf, "\x00\x11\x22\xaa\xbb\xcc", 6 ) == 0, exit, err = -1 );
	
	memset( buf, 'z', sizeof( buf ) );
	err = -1;
	u64 = CFGetMACAddress( CFSTR( "0:11:22:a:bB:c" ), buf, &err );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 0x0011220abb0c ), exit, err = -1 );
	require_action( memcmp( buf, "\x00\x11\x22\x0a\xbb\x0c", 6 ) == 0, exit, err = -1 );
	
	memset( buf, 'z', sizeof( buf ) );
	err = kNoErr;
	u64 = CFGetMACAddress( NULL, buf, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( u64 == 0, exit, err = -1 );
	require_action( memcmp( buf, "\x00\x00\x00\x00\x00\x00", 6 ) == 0, exit, err = -1 );
	
	err = -1;
	u64 = CFGetMACAddress( CFSTR( "testing" ), buf, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( u64 == 0, exit, err = -1 );
	
	u64 = UINT64_C( 0x001122aabbcc );
	num = CFNumberCreate( NULL, kCFNumberSInt64Type, &u64 );
	require_action( num, exit, err = -1 );
	memset( buf, 'z', sizeof( buf ) );
	err = -1;
	u64 = CFGetMACAddress( num, buf, &err );
	CFRelease( num );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 0x001122aabbcc ), exit, err = -1 );
	require_action( memcmp( buf, "\x00\x11\x22\xaa\xbb\xcc", 6 ) == 0, exit, err = -1 );
	
	err = -1;
	u64 = CFGetMACAddress( kCFBooleanTrue, buf, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( u64 == 0, exit, err = -1 );
	
	data = CFDataCreate( NULL, (const uint8_t *) "\x00\x11\x22\xaa\xbb\xcc", 6 );
	require_action( data, exit, err = -1 );
	memset( buf, 'z', sizeof( buf ) );
	err = -1;
	u64 = CFGetMACAddress( data, buf, &err );
	CFRelease( data );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 0x001122aabbcc ), exit, err = -1 );
	require_action( memcmp( buf, "\x00\x11\x22\xaa\xbb\xcc", 6 ) == 0, exit, err = -1 );
	
	// CFGetInt64
	
	err = -1;
	require_action( CFGetInt64( CFSTR( "true" ), &err ) == 1, exit, err = -1 );
	require_noerr( err, exit );
	err = -1;
	require_action( CFGetInt64( CFSTR( "123" ), &err ) == 123, exit, err = -1 );
	require_noerr( err, exit );
	require_action( CFGetInt64( CFSTR( "123.45" ), &err ) == 123, exit, err = -1 );
	require_noerr( err, exit );
	
	// CFGetInt64Ranged
	
	err = -1;
	s64 = CFGetInt64Ranged( CFSTR( "0" ), 0, 0, &err );
	require_noerr( err, exit );
	require_action( s64 == 0, exit, err = -1 );
	
	err = -1;
	s64 = CFGetInt64Ranged( CFSTR( "-10000" ), -65000, 65000, &err );
	require_noerr( err, exit );
	require_action( s64 == -10000, exit, err = -1 );
	
	err = -1;
	s64 = CFGetInt64Ranged( CFSTR( "65000" ), 0, 65500, &err );
	require_noerr( err, exit );
	require_action( s64 == 65000, exit, err = -1 );
	
	err = -1;
	s64 = CFGetInt64Ranged( CFSTR( "256" ), 0, 255, &err );
	require_action( err == kRangeErr, exit, err = -1 );
	require_action( s64 == 256, exit, err = -1 );
	
	err = -1;
	s64 = CFGetInt64Ranged( CFSTR( "-1000" ), 0, 256, &err );
	require_action( err == kRangeErr, exit, err = -1 );
	require_action( s64 == -1000, exit, err = -1 );
	
	// CFGetSInt8 / CFGetUInt8
	
	s8 = CFGetSInt8( CFSTR( "-100" ), &err );
	require_noerr( err, exit );
	require_action( s8 == -100, exit, err = -1 );
	s8 = CFGetSInt8( CFSTR( "127" ), &err );
	require_noerr( err, exit );
	require_action( s8 == 127, exit, err = -1 );
	(void) CFGetSInt8( CFSTR( "-1000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFGetSInt8( CFSTR( "1000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	u8 = CFGetUInt8( CFSTR( "200" ), &err );
	require_noerr( err, exit );
	require_action( u8 == 200, exit, err = -1 );
	(void) CFGetUInt8( CFSTR( "-100" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFGetUInt8( CFSTR( "300" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	// CFGetSInt16 / CFGetUInt16
	
	s16 = CFGetSInt16( CFSTR( "-1000" ), &err );
	require_noerr( err, exit );
	require_action( s16 == -1000, exit, err = -1 );
	s16 = CFGetSInt16( CFSTR( "32000" ), &err );
	require_noerr( err, exit );
	require_action( s16 == 32000, exit, err = -1 );
	(void) CFGetSInt16( CFSTR( "-100000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFGetSInt16( CFSTR( "100000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	u16 = CFGetUInt16( CFSTR( "12345" ), &err );
	require_noerr( err, exit );
	require_action( u16 == 12345, exit, err = -1 );
	(void) CFGetUInt16( CFSTR( "-1" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFGetUInt16( CFSTR( "70000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	// CFGetSInt32 / CFGetUInt32
	
	s32 = CFGetSInt32( CFSTR( "-100000" ), &err );
	require_noerr( err, exit );
	require_action( s32 == -100000, exit, err = -1 );
	s32 = CFGetSInt32( CFSTR( "200000" ), &err );
	require_noerr( err, exit );
	require_action( s32 == 200000, exit, err = -1 );
	(void) CFGetSInt32( CFSTR( "-3000000000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFGetSInt32( CFSTR( "3000000000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	u32 = CFGetUInt32( CFSTR( "3000000000" ), &err );
	require_noerr( err, exit );
	require_action( u32 == 3000000000U, exit, err = -1 );
	(void) CFGetUInt32( CFSTR( "-128" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFGetUInt32( CFSTR( "8000000000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	// CFGetInt64 / CFGetUInt64
	
	s64 = CFGetInt64( CFSTR( "-100000" ), &err );
	require_noerr( err, exit );
	require_action( s64 == -100000, exit, err = -1 );
	s64 = CFGetInt64( CFSTR( "200000" ), &err );
	require_noerr( err, exit );
	require_action( s64 == 200000, exit, err = -1 );
	s64 = CFGetInt64( CFSTR( "-3000000000" ), &err );
	require_noerr( err, exit );
	require_action( s64 == -INT64_C( 3000000000 ), exit, err = -1 );
	
	u64 = CFGetUInt64( CFSTR( "3000000000" ), &err );
	require_noerr( err, exit );
	require_action( u64 == 3000000000U, exit, err = -1 );
	u64 = CFGetUInt64( CFSTR( "8000000000" ), &err );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 8000000000 ), exit, err = -1 );
	u64 = CFGetUInt64( CFSTR( "10000000000000000000" ), &err );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 10000000000000000000 ), exit, err = -1 );
	
	// CFDictionaryGetInt64Ranged variants
	
	plist = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( plist, exit, err = -1 );
	CFDictionarySetInt64( plist, CFSTR( "-1" ), -1 );
	CFDictionarySetInt64( plist, CFSTR( "-100" ), -100 );
	CFDictionarySetInt64( plist, CFSTR( "-1000" ), -1000 );
	CFDictionarySetInt64( plist, CFSTR( "-10000" ), -10000 );
	CFDictionarySetInt64( plist, CFSTR( "-100000" ), -100000 );
	CFDictionarySetInt64( plist, CFSTR( "-128" ), -128 );
	CFDictionarySetInt64( plist, CFSTR( "-3000000000" ), -INT64_C( 3000000000 ) );
	CFDictionarySetInt64( plist, CFSTR( "0" ), 0 );
	CFDictionarySetInt64( plist, CFSTR( "1000" ), 1000 );
	CFDictionarySetInt64( plist, CFSTR( "100000" ), 100000 );
	CFDictionarySetInt64( plist, CFSTR( "12345" ), 12345 );
	CFDictionarySetInt64( plist, CFSTR( "127" ), 127 );
	CFDictionarySetInt64( plist, CFSTR( "200" ), 200 );
	CFDictionarySetInt64( plist, CFSTR( "200000" ), 200000 );
	CFDictionarySetInt64( plist, CFSTR( "256" ), 256 );
	CFDictionarySetInt64( plist, CFSTR( "300" ), 300 );
	CFDictionarySetInt64( plist, CFSTR( "3000000000" ), INT64_C( 3000000000 ) );
	CFDictionarySetInt64( plist, CFSTR( "32000" ), 32000 );
	CFDictionarySetInt64( plist, CFSTR( "65000" ), 65000 );
	CFDictionarySetInt64( plist, CFSTR( "70000" ), 70000 );
	CFDictionarySetInt64( plist, CFSTR( "8000000000" ), INT64_C( 8000000000 ) );
	
	// CFDictionaryGetInt64Ranged
	
	err = -1;
	s64 = CFDictionaryGetInt64Ranged( plist, CFSTR( "0" ), 0, 0, &err );
	require_noerr( err, exit );
	require_action( s64 == 0, exit, err = -1 );
	
	err = -1;
	s64 = CFDictionaryGetInt64Ranged( plist, CFSTR( "-10000" ), -65000, 65000, &err );
	require_noerr( err, exit );
	require_action( s64 == -10000, exit, err = -1 );
	
	err = -1;
	s64 = CFDictionaryGetInt64Ranged( plist, CFSTR( "65000" ), 0, 65500, &err );
	require_noerr( err, exit );
	require_action( s64 == 65000, exit, err = -1 );
	
	err = -1;
	s64 = CFDictionaryGetInt64Ranged( plist, CFSTR( "256" ), 0, 255, &err );
	require_action( err == kRangeErr, exit, err = -1 );
	require_action( s64 == 256, exit, err = -1 );
	
	err = -1;
	s64 = CFDictionaryGetInt64Ranged( plist, CFSTR( "-1000" ), 0, 256, &err );
	require_action( err == kRangeErr, exit, err = -1 );
	require_action( s64 == -1000, exit, err = -1 );
	
	// CFDictionaryGetSInt8 / CFDictionaryGetUInt8
	
	s8 = CFDictionaryGetSInt8( plist, CFSTR( "-100" ), &err );
	require_noerr( err, exit );
	require_action( s8 == -100, exit, err = -1 );
	s8 = CFDictionaryGetSInt8( plist, CFSTR( "127" ), &err );
	require_noerr( err, exit );
	require_action( s8 == 127, exit, err = -1 );
	(void) CFDictionaryGetSInt8( plist, CFSTR( "-1000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFDictionaryGetSInt8( plist, CFSTR( "1000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	u8 = CFDictionaryGetUInt8( plist, CFSTR( "200" ), &err );
	require_noerr( err, exit );
	require_action( u8 == 200, exit, err = -1 );
	(void) CFDictionaryGetUInt8( plist, CFSTR( "-100" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFDictionaryGetUInt8( plist, CFSTR( "300" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	// CFDictionaryGetSInt16 / CFDictionaryGetUInt16
	
	s16 = CFDictionaryGetSInt16( plist, CFSTR( "-1000" ), &err );
	require_noerr( err, exit );
	require_action( s16 == -1000, exit, err = -1 );
	s16 = CFDictionaryGetSInt16( plist, CFSTR( "32000" ), &err );
	require_noerr( err, exit );
	require_action( s16 == 32000, exit, err = -1 );
	(void) CFDictionaryGetSInt16( plist, CFSTR( "-100000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFDictionaryGetSInt16( plist, CFSTR( "100000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	u16 = CFDictionaryGetUInt16( plist, CFSTR( "12345" ), &err );
	require_noerr( err, exit );
	require_action( u16 == 12345, exit, err = -1 );
	(void) CFDictionaryGetUInt16( plist, CFSTR( "-1" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFDictionaryGetUInt16( plist, CFSTR( "70000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	// CFDictionaryGetSInt32 / CFDictionaryGetUInt32
	
	s32 = CFDictionaryGetSInt32( plist, CFSTR( "-100000" ), &err );
	require_noerr( err, exit );
	require_action( s32 == -100000, exit, err = -1 );
	s32 = CFDictionaryGetSInt32( plist, CFSTR( "200000" ), &err );
	require_noerr( err, exit );
	require_action( s32 == 200000, exit, err = -1 );
	(void) CFDictionaryGetSInt32( plist, CFSTR( "-3000000000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFDictionaryGetSInt32( plist, CFSTR( "3000000000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	u32 = CFDictionaryGetUInt32( plist, CFSTR( "3000000000" ), &err );
	require_noerr( err, exit );
	require_action( u32 == 3000000000U, exit, err = -1 );
	(void) CFDictionaryGetUInt32( plist, CFSTR( "-128" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFDictionaryGetUInt32( plist, CFSTR( "8000000000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	CFRelease( plist );
	
	// CFDictionarySetInt64
	
	plist = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( plist, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), -123 );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == -123, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), 255 );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == 255, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), 32000 );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == 32000, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), 1000000 );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == 1000000, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), INT64_C( 10000000000 ) );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == INT64_C( 10000000000 ), exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), INT64_C( -5000000000 ) );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == INT64_C( -5000000000 ), exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), INT8_MIN );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == INT8_MIN, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), INT8_MAX );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == INT8_MAX, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), INT16_MIN );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == INT16_MIN, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), INT16_MAX );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == INT16_MAX, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), INT32_MIN );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == INT32_MIN, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), INT32_MAX );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == INT32_MAX, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), INT64_MIN );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == INT64_MIN, exit, err = -1 );
	
	err = CFDictionarySetInt64( plist, CFSTR( "test" ), INT64_MAX );
	require_noerr( err, exit );
	require_action( CFDictionaryGetInt64( plist, CFSTR( "test" ), NULL ) == INT64_MAX, exit, err = -1 );
	
	// CFDictionarySetUInt64
	
	err = CFDictionarySetUInt64( plist, CFSTR( "test" ), UINT64_C( 10000000000000000123 ) );
	require_noerr( err, exit );
	require_action( CFDictionaryGetUInt64( plist, CFSTR( "test" ), NULL ) == UINT64_C( 10000000000000000123 ), exit, err = -1 );
	
	err = CFDictionarySetUInt64( plist, CFSTR( "test" ), 0 );
	require_noerr( err, exit );
	require_action( CFDictionaryGetUInt64( plist, CFSTR( "test" ), NULL ) == 0, exit, err = -1 );
	
	err = CFDictionarySetUInt64( plist, CFSTR( "test" ), UINT64_MAX );
	require_noerr( err, exit );
	require_action( CFDictionaryGetUInt64( plist, CFSTR( "test" ), NULL ) == UINT64_MAX, exit, err = -1 );
	
	CFRelease( plist );
	
	// CFDictionaryGetUUID
	
	plist = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( plist, exit, err = -1 );
	CFDictionarySetValue( plist, CFSTR( "str128" ), CFSTR( "40a092a0-e7a8-4284-aa3a-20c4daa00169" ) );
	CFDictionarySetValue( plist, CFSTR( "str32" ),  CFSTR( "11223344" ) );
	CFDictionarySetValue( plist, CFSTR( "str16" ),  CFSTR( "AABB" ) );
	CFDictionarySetValue( plist, CFSTR( "str8" ),   CFSTR( "88" ) );
	CFDictionarySetInt64( plist, CFSTR( "num32" ), 0x22334455 );
	CFDictionarySetInt64( plist, CFSTR( "num16" ), 0x6677 );
	CFDictionarySetInt64( plist, CFSTR( "num8" ), 0x88 );
	CFDictionarySetData( plist, CFSTR( "data4" ), "\x66\x77\x88\x99", 4 );
	CFDictionarySetData( plist, CFSTR( "data2" ), "\xAA\xBB", 2 );
	CFDictionarySetData( plist, CFSTR( "data1" ), "\xCC", 1 );
	
	memset( buf, 0, 16 );
	err = CFDictionaryGetUUID( plist, CFSTR( "str128" ), buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x40\xa0\x92\xa0\xe7\xa8\x42\x84\xaa\x3a\x20\xc4\xda\xa0\x01\x69", 16 ) == 0, exit, err = -1 );
	
	memset( buf, 0, 16 );
	err = CFDictionaryGetUUIDEx( plist, CFSTR( "str32" ), 
		(const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x11\x22\x33\x44\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = -1 );
	
	memset( buf, 0, 16 );
	err = CFDictionaryGetUUIDEx( plist, CFSTR( "str16" ), 
		(const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x00\xAA\xBB\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = -1 );
	
	memset( buf, 0, 16 );
	err = CFDictionaryGetUUIDEx( plist, CFSTR( "str8" ), 
		(const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x00\x00\x88\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = -1 );
	
	memset( buf, 0, 16 );
	err = CFDictionaryGetUUIDEx( plist, CFSTR( "num32" ), 
		(const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x22\x33\x44\x55\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = -1 );
	
	memset( buf, 0, 16 );
	err = CFDictionaryGetUUIDEx( plist, CFSTR( "num16" ), 
		(const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x00\x66\x77\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = -1 );
	
	memset( buf, 0, 16 );
	err = CFDictionaryGetUUIDEx( plist, CFSTR( "num8" ), 
		(const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x00\x00\x88\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = -1 );
	
	memset( buf, 0, 16 );
	err = CFDictionaryGetUUIDEx( plist, CFSTR( "data4" ), 
		(const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x66\x77\x88\x99\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = -1 );
	
	memset( buf, 0, 16 );
	err = CFDictionaryGetUUIDEx( plist, CFSTR( "data2" ), 
		(const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x00\xAA\xBB\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = -1 );
	
	memset( buf, 0, 16 );
	err = CFDictionaryGetUUIDEx( plist, CFSTR( "data1" ), 
		(const uint8_t *) "\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", buf );
	require_noerr( err, exit );
	require_action( memcmp( buf, "\x00\x00\x00\xCC\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16 ) == 0, exit, err = -1 );
	
	err = CFDictionarySetUUIDString( plist, CFSTR( "x1" ), 
		"\xAB", 1, 
		"\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 
		kUUIDFlag_ShortForm );
	require_noerr( err, exit );	
	str = (CFStringRef) CFDictionaryGetValue( plist, CFSTR( "x1" ) );
	require_action( str, exit, err = -1 );
	require_action( CFEqual( str, CFSTR( "ab" ) ), exit, err = -1 );
	
	err = CFDictionarySetUUIDString( plist, CFSTR( "x1" ), 
		"\x00\x00\x12\x34\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 16, 
		"\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 
		kUUIDFlag_ShortForm );
	require_noerr( err, exit );	
	str = (CFStringRef) CFDictionaryGetValue( plist, CFSTR( "x1" ) );
	require_action( str, exit, err = -1 );
	require_action( CFEqual( str, CFSTR( "1234" ) ), exit, err = -1 );
	
	err = CFDictionarySetUUIDString( plist, CFSTR( "x1" ), 
		"\xFF\xFF\xFF\xFF\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFC", 16, 
		"\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 
		kUUIDFlag_ShortForm );
	require_noerr( err, exit );	
	str = (CFStringRef) CFDictionaryGetValue( plist, CFSTR( "x1" ) );
	require_action( str, exit, err = -1 );
	require_action( CFEqual( str, CFSTR( "ffffffff-0000-1000-8000-00805f9b34fc" ) ), exit, err = -1 );
	
	err = CFDictionarySetUUIDString( plist, CFSTR( "x1" ), 
		"\xFF\xFF\xFF\xFF\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFC", 16, 
		"\x00\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\x80\x5F\x9B\x34\xFB", 
		kUUIDFlag_ShortForm );
	require_noerr( err, exit );	
	str = (CFStringRef) CFDictionaryGetValue( plist, CFSTR( "x1" ) );
	require_action( str, exit, err = -1 );
	require_action( CFEqual( str, CFSTR( "ffffffff-0000-1000-8000-00805f9b34fc" ) ), exit, err = -1 );
	
	CFRelease( plist );
	
	// ----------------------------------------------------------------------------------------------------------------------
	// CFDictionaryGetTypedValue
	// ----------------------------------------------------------------------------------------------------------------------
	
	err = CFPropertyListCreateFormatted( NULL, &plist, 
		"{"
			"array=[]"
			"bool=%O"
			"data=%D"
			"date=%T"
			"dict={}"
			"num=%i"
			"str=%O"
		"}", 
		kCFBooleanTrue, 
		"\x00\x01", 2, 
		2012, 8, 28, 8, 51, 40, 
		123, 
		CFSTR( "string" ) );
	require_noerr( err, exit );
	
	// Positive tests.
	
	array = CFDictionaryGetCFArray( plist, CFSTR( "array" ), &err );
	require_action( CFIsType( array, CFArray ), exit, err = -1 );
	require_noerr( err, exit );
	
	boolObj = CFDictionaryGetCFBoolean( plist, CFSTR( "bool" ), &err );
	require_action( CFIsType( boolObj, CFBoolean ), exit, err = -1 );
	require_noerr( err, exit );
	
	data = CFDictionaryGetCFData( plist, CFSTR( "data" ), &err );
	require_action( CFIsType( data, CFData ), exit, err = -1 );
	require_noerr( err, exit );
	
	dict = CFDictionaryGetCFDictionary( plist, CFSTR( "dict" ), &err );
	require_action( CFIsType( dict, CFDictionary ), exit, err = -1 );
	require_noerr( err, exit );
	
	num = CFDictionaryGetCFNumber( plist, CFSTR( "num" ), &err );
	require_action( CFIsType( num, CFNumber ), exit, err = -1 );
	require_noerr( err, exit );
	
	str = CFDictionaryGetCFString( plist, CFSTR( "str" ), &err );
	require_action( CFIsType( str, CFString ), exit, err = -1 );
	require_noerr( err, exit );
	
	// Missing tests.
	
	array = CFDictionaryGetCFArray( plist, CFSTR( "x" ), &err );
	require_action( array == NULL, exit, err = -1 );
	require_action( err == kNotFoundErr, exit, err = -1 );
	
	boolObj = CFDictionaryGetCFBoolean( plist, CFSTR( "x" ), &err );
	require_action( boolObj == NULL, exit, err = -1 );
	require_action( err == kNotFoundErr, exit, err = -1 );
	
	data = CFDictionaryGetCFData( plist, CFSTR( "x" ), &err );
	require_action( data == NULL, exit, err = -1 );
	require_action( err == kNotFoundErr, exit, err = -1 );
	
	dict = CFDictionaryGetCFDictionary( plist, CFSTR( "x" ), &err );
	require_action( dict == NULL, exit, err = -1 );
	require_action( err == kNotFoundErr, exit, err = -1 );
	
	num = CFDictionaryGetCFNumber( plist, CFSTR( "x" ), &err );
	require_action( num == NULL, exit, err = -1 );
	require_action( err == kNotFoundErr, exit, err = -1 );
	
	str = CFDictionaryGetCFString( plist, CFSTR( "x" ), &err );
	require_action( str == NULL, exit, err = -1 );
	require_action( err == kNotFoundErr, exit, err = -1 );
	
	// Mistyped tests.
	
	array = CFDictionaryGetCFArray( plist, CFSTR( "bool" ), &err );
	require_action( array == NULL, exit, err = -1 );
	require_action( err == kTypeErr, exit, err = -1 );
	
	boolObj = CFDictionaryGetCFBoolean( plist, CFSTR( "array" ), &err );
	require_action( boolObj == NULL, exit, err = -1 );
	require_action( err == kTypeErr, exit, err = -1 );
	
	data = CFDictionaryGetCFData( plist, CFSTR( "array" ), &err );
	require_action( data == NULL, exit, err = -1 );
	require_action( err == kTypeErr, exit, err = -1 );
	
	dict = CFDictionaryGetCFDictionary( plist, CFSTR( "array" ), &err );
	require_action( dict == NULL, exit, err = -1 );
	require_action( err == kTypeErr, exit, err = -1 );
	
	num = CFDictionaryGetCFNumber( plist, CFSTR( "array" ), &err );
	require_action( num == NULL, exit, err = -1 );
	require_action( err == kTypeErr, exit, err = -1 );
	
	str = CFDictionaryGetCFString( plist, CFSTR( "array" ), &err );
	require_action( str == NULL, exit, err = -1 );
	require_action( err == kTypeErr, exit, err = -1 );
	
	CFRelease( plist );

	// CFDictionaryCreateWithINIBytes	
{
	static const char * const		kCFUtilsINITest1 = 
		"name  = MyName\n"
		"model = MyModel\n"
		"\n"
		"[SectionType1 \"Section 1 Name 1\"]\n"
		"propertyName1 = section1Value1\n"
		"propertyName2 = 05 0d 09 04 a1 01 05 09 19 01 29 08 15 00\n"
		"propertyName3 = e5f7a68d-7b0f-4305-984b-974f677a150b\n"
		"\n"
		"[SectionType1]\n"
		"propertyName1 = section1Value2\n"
		"propertyName2 = 15 00 25 01 95 08 75 01 81 02 01 29 08 45\n"
		"propertyName3 = 4a0913fc-9676-4cb2-9219-f2cf7074af82\n"
		"\n"
		"[SectionType2 \"Section 2 Name 1\"]\n"
		"propertyName4 = 0xE\n"
		"propertyName5 = 960\n"
		"propertyName6 = 685073b1-71c5-404b-abb8-aa4ea73404f3\n"
		"\n"
		"[SectionType3]\n";
	
	plist = CFDictionaryCreateWithINIBytes( kCFUtilsINITest1, strlen( kCFUtilsINITest1 ), 0, CFSTR( "name" ), &err );
	require_noerr( err, exitINI );
	require_action( plist, exitINI, err = -1 );
	
	// Global section
	
	dict = CFDictionaryGetCFDictionary( plist, CFSTR( kINISectionType_Global ), &err );
	require_noerr( err, exitINI );
	require_action( CFDictionaryGetCount( dict ) == 2, exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "name" ), NULL ), 
		CFSTR( "MyName" ) ), exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "model" ), NULL ), 
		CFSTR( "MyModel" ) ), exitINI, err = -1 );
	
	// SectionType1
	
	array = CFDictionaryGetCFArray( plist, CFSTR( "SectionType1" ), &err );
	require_noerr( err, exitINI );
	require_action( CFArrayGetCount( array ) == 2, exitINI, err = -1 );
	
	dict = CFArrayGetCFDictionaryAtIndex( array, 0, &err );
	require_noerr( err, exitINI );
	require_action( CFDictionaryGetCount( dict ) == 4, exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "name" ), NULL ), 
		CFSTR( "Section 1 Name 1" ) ), exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "propertyName1" ), NULL ), 
		CFSTR( "section1Value1" ) ), exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "propertyName2" ), NULL ), 
		CFSTR( "05 0d 09 04 a1 01 05 09 19 01 29 08 15 00" ) ), exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "propertyName3" ), NULL ), 
		CFSTR( "e5f7a68d-7b0f-4305-984b-974f677a150b" ) ), exitINI, err = -1 );
	
	dict = CFArrayGetCFDictionaryAtIndex( array, 1, &err );
	require_noerr( err, exitINI );
	require_action( CFDictionaryGetCount( dict ) == 3, exitINI, err = -1 );
	require_action( CFDictionaryGetValue( dict, CFSTR( "name" ) ) == NULL, exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "propertyName1" ), NULL ), 
		CFSTR( "section1Value2" ) ), exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "propertyName2" ), NULL ), 
		CFSTR( "15 00 25 01 95 08 75 01 81 02 01 29 08 45" ) ), exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "propertyName3" ), NULL ), 
		CFSTR( "4a0913fc-9676-4cb2-9219-f2cf7074af82" ) ), exitINI, err = -1 );
	
	// SectionType2
	
	array = CFDictionaryGetCFArray( plist, CFSTR( "SectionType2" ), &err );
	require_noerr( err, exitINI );
	require_action( CFArrayGetCount( array ) == 1, exitINI, err = -1 );
	
	dict = CFArrayGetCFDictionaryAtIndex( array, 0, &err );
	require_noerr( err, exitINI );
	require_action( CFDictionaryGetCount( dict ) == 4, exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "name" ), NULL ), 
		CFSTR( "Section 2 Name 1" ) ), exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "propertyName4" ), NULL ), 
		CFSTR( "0xE" ) ), exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "propertyName5" ), NULL ), 
		CFSTR( "960" ) ), exitINI, err = -1 );
	require_action( CFEqualNullSafe( CFDictionaryGetCFString( dict, CFSTR( "propertyName6" ), NULL ), 
		CFSTR( "685073b1-71c5-404b-abb8-aa4ea73404f3" ) ), exitINI, err = -1 );
	
	// SectionType3
	
	array = CFDictionaryGetCFArray( plist, CFSTR( "SectionType3" ), &err );
	require_noerr( err, exitINI );
	require_action( CFArrayGetCount( array ) == 1, exitINI, err = -1 );
	dict = CFArrayGetCFDictionaryAtIndex( array, 0, &err );
	require_noerr( err, exitINI );
	require_action( CFDictionaryGetCount( dict ) == 0, exitINI, err = -1 );
	
exitINI:
	CFRelease( plist );
}
	
	// CFDictionaryMergeDictionary
	
	err = CFPropertyListCreateFormatted( kCFAllocatorDefault, &plist, 
		"{"
			"key1=value1;"
			"key2=value2;"
		"}" );
	require_noerr( err, exit );
	
	err = CFPropertyListCreateFormatted( kCFAllocatorDefault, &plist2, 
		"{"
			"key2=value22;"
			"key4=value4;"
		"}" );
	require_noerr( err, exit );
	
	err = CFDictionaryMergeDictionary( plist, plist2 );
	require_noerr( err, exit );
	
	str = (CFStringRef) CFDictionaryGetValue( plist, CFSTR( "key1" ) );
	require_action( str && ( CFStringCompare( str, CFSTR( "value1" ), 0 ) == kCFCompareEqualTo ), exit, err = kResponseErr );
	
	str = (CFStringRef) CFDictionaryGetValue( plist, CFSTR( "key2" ) );
	require_action( str && ( CFStringCompare( str, CFSTR( "value22" ), 0 ) == kCFCompareEqualTo ), exit, err = kResponseErr );
	
	str = (CFStringRef) CFDictionaryGetValue( plist, CFSTR( "key4" ) );
	require_action( str && ( CFStringCompare( str, CFSTR( "value4" ), 0 ) == kCFCompareEqualTo ), exit, err = kResponseErr );
	
	CFRelease( plist );
	CFRelease( plist2 );
	
	// %##O
	
	err = CFPropertyListCreateFormatted( kCFAllocatorDefault, &plist2, 
		"{"
			"key2=value22;"
			"key4=value4;"
		"}" );
	require_noerr( err, exit );
	
	err = CFPropertyListCreateFormatted( kCFAllocatorDefault, &plist, 
		"{"
			"key1=value1;"
			"key2=value2;"
			"%##O"
		"}", plist2 );
	require_noerr( err, exit );
	
	str = (CFStringRef) CFDictionaryGetValue( plist, CFSTR( "key1" ) );
	require_action( str && ( CFStringCompare( str, CFSTR( "value1" ), 0 ) == kCFCompareEqualTo ), exit, err = kResponseErr );
	
	str = (CFStringRef) CFDictionaryGetValue( plist, CFSTR( "key2" ) );
	require_action( str && ( CFStringCompare( str, CFSTR( "value22" ), 0 ) == kCFCompareEqualTo ), exit, err = kResponseErr );
	
	str = (CFStringRef) CFDictionaryGetValue( plist, CFSTR( "key4" ) );
	require_action( str && ( CFStringCompare( str, CFSTR( "value4" ), 0 ) == kCFCompareEqualTo ), exit, err = kResponseErr );
	
	CFRelease( plist );
	CFRelease( plist2 );
	
	// CFStringCreateComponentsSeparatedByString
	
	str   = CFSTR( "Norman, Stanley, Fletcher" );
	array = CFStringCreateComponentsSeparatedByString( str, CFSTR( ", " ) );
	require_action( array, exit, err = -1 );
	require_action( CFArrayGetCount( array ) == 3, exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 0 ), CFSTR( "Norman" ) ), exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 1 ), CFSTR( "Stanley" ) ), exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 2 ), CFSTR( "Fletcher" ) ), exit, err = -1 );
	CFRelease( array );
	
	str   = CFSTR( ", Norman, Stanley, Fletcher" );
	array = CFStringCreateComponentsSeparatedByString( str, CFSTR( ", " ) );
	require_action( array, exit, err = -1 );
	require_action( CFArrayGetCount( array ) == 4, exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 0 ), CFSTR( "" ) ), exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 1 ), CFSTR( "Norman" ) ), exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 2 ), CFSTR( "Stanley" ) ), exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 3 ), CFSTR( "Fletcher" ) ), exit, err = -1 );
	CFRelease( array );
	
	str   = CFSTR( "Norman, Stanley, Fletcher, " );
	array = CFStringCreateComponentsSeparatedByString( str, CFSTR( ", " ) );
	require_action( array, exit, err = -1 );
	require_action( CFArrayGetCount( array ) == 4, exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 0 ), CFSTR( "Norman" ) ), exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 1 ), CFSTR( "Stanley" ) ), exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 2 ), CFSTR( "Fletcher" ) ), exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 3 ), CFSTR( "" ) ), exit, err = -1 );
	CFRelease( array );
	
	str   = CFSTR( "" );
	array = CFStringCreateComponentsSeparatedByString( str, CFSTR( ", " ) );
	require_action( array, exit, err = -1 );
	require_action( CFArrayGetCount( array ) == 1, exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 0 ), CFSTR( "" ) ), exit, err = -1 );
	CFRelease( array );
	array = NULL;
	
	str   = CFSTR( ", " );
	array = CFStringCreateComponentsSeparatedByString( str, CFSTR( ", " ) );
	require_action( array, exit, err = -1 );
	require_action( CFArrayGetCount( array ) == 2, exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 0 ), CFSTR( "" ) ), exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 1 ), CFSTR( "" ) ), exit, err = -1 );
	CFRelease( array );
	array = NULL;
	
	str   = CFSTR( "Norman" );
	array = CFStringCreateComponentsSeparatedByString( str, CFSTR( ", " ) );
	require_action( array, exit, err = -1 );
	require_action( CFArrayGetCount( array ) == 1, exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 0 ), CFSTR( "Norman" ) ), exit, err = -1 );
	CFRelease( array );
	
	str   = CFSTR( "abcd,1234,+" );
	array = CFStringCreateComponentsSeparatedByString( str, CFSTR( "," ) );
	require_action( array, exit, err = -1 );
	require_action( CFArrayGetCount( array ) == 3, exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 0 ), CFSTR( "abcd" ) ), exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 1 ), CFSTR( "1234" ) ), exit, err = -1 );
	require_action( CFEqual( CFArrayGetValueAtIndex( array, 2 ), CFSTR( "+" ) ), exit, err = -1 );
	CFRelease( array );
	
	err = CFUtilsTestCFObjectAccessors();
	require_noerr( err, exit );
	
exit:
	printf( "CFUtilsTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	CFUtilsTestCFObjectAccessors
//===========================================================================================================================

static CFTypeRef	_CFUtilsTestDictionaryCopyValue( CFTypeRef inAppID, CFStringRef inKey, OSStatus *outErr );
static OSStatus		_CFUtilsTestDictionarySetValue( CFTypeRef inAppID, CFStringRef inKey, CFTypeRef inValue );

CFObjectDefineStandardAccessors( 
	CFDictionaryRef, 
	CFUtilsTestDictionary, 
	_CFUtilsTestDictionaryCopyValue, 
	_CFUtilsTestDictionarySetValue )

OSStatus	CFUtilsTestCFObjectAccessors( void )
{
	OSStatus					err;
	CFMutableDictionaryRef		plist;
	CFArrayRef					cfArray			= NULL;
	CFBooleanRef				cfBoolean		= NULL;
	CFDataRef					cfData			= NULL;
	CFDictionaryRef				cfDictionary	= NULL;
	CFNumberRef					cfNumber		= NULL;
	CFStringRef					cfString		= NULL;
	uint8_t *					ptr				= NULL;
	char *						cptr			= NULL;
	CFTypeRef					obj				= NULL;
	uint8_t *					ptr2;
	char *						cptr2;
	uint8_t						buf[ 64 ];
	char						cstr[ 64 ];
	size_t						len;
	int8_t						s8;
	uint8_t						u8;
	int16_t						s16;
	uint16_t					u16;
	int32_t						s32;
	uint32_t					u32;
	int64_t						s64;
	uint64_t					u64;
	Boolean						b;
	double						d;
	
	plist = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( plist, exit, err = -1 );
	CFDictionarySetBoolean( plist, CFSTR( "false" ), false );
	CFDictionarySetBoolean( plist, CFSTR( "true" ), true );
	CFDictionarySetDouble( plist, CFSTR( "double" ), 123.45 );
	CFDictionarySetValue( plist, CFSTR( "hardwareAddress" ), CFSTR( "00:11:22:33:44:55:66:77" ) );
	CFDictionarySetValue( plist, CFSTR( "macAddress" ), CFSTR( "AA:BB:CC:DD:EE:FF" ) );
	CFDictionarySetValue( plist, CFSTR( "string" ), CFSTR( "string" ) );
	CFDictionarySetInt64( plist, CFSTR( "-1" ), -1 );
	CFDictionarySetInt64( plist, CFSTR( "-100" ), -100 );
	CFDictionarySetInt64( plist, CFSTR( "-1000" ), -1000 );
	CFDictionarySetInt64( plist, CFSTR( "-10000" ), -10000 );
	CFDictionarySetInt64( plist, CFSTR( "-100000" ), -100000 );
	CFDictionarySetInt64( plist, CFSTR( "-128" ), -128 );
	CFDictionarySetInt64( plist, CFSTR( "-3000000000" ), -INT64_C( 3000000000 ) );
	CFDictionarySetInt64( plist, CFSTR( "0" ), 0 );
	CFDictionarySetInt64( plist, CFSTR( "1000" ), 1000 );
	CFDictionarySetInt64( plist, CFSTR( "100000" ), 100000 );
	CFDictionarySetInt64( plist, CFSTR( "12345" ), 12345 );
	CFDictionarySetInt64( plist, CFSTR( "127" ), 127 );
	CFDictionarySetInt64( plist, CFSTR( "200" ), 200 );
	CFDictionarySetInt64( plist, CFSTR( "200000" ), 200000 );
	CFDictionarySetInt64( plist, CFSTR( "256" ), 256 );
	CFDictionarySetInt64( plist, CFSTR( "300" ), 300 );
	CFDictionarySetInt64( plist, CFSTR( "3000000000" ), INT64_C( 3000000000 ) );
	CFDictionarySetInt64( plist, CFSTR( "32000" ), 32000 );
	CFDictionarySetInt64( plist, CFSTR( "65000" ), 65000 );
	CFDictionarySetInt64( plist, CFSTR( "70000" ), 70000 );
	CFDictionarySetInt64( plist, CFSTR( "8000000000" ), INT64_C( 8000000000 ) );
	CFDictionarySetInt64( plist, CFSTR( "10000000000000000000" ), (int64_t) UINT64_C( 10000000000000000000 ) );
	
	// Typed-accessors.
	
	err = -1;
	cfArray = CFUtilsTestDictionaryCopyCFArray( plist, CFSTR( "array" ), &err );
	require_noerr( err, exit );
	require_action( CFIsType( cfArray, CFArray ), exit, err = -1 );
	ForgetCF( &cfArray );
	cfArray = CFUtilsTestDictionaryCopyCFArray( plist, CFSTR( "false" ), &err );
	require_action( err == kTypeErr, exit, err = -1 );
	require_action( !cfArray, exit, err = -1 );
	cfArray = CFUtilsTestDictionaryCopyCFArray( plist, CFSTR( "missing" ), &err );
	require_action( err == kNotFoundErr, exit, err = -1 );
	require_action( !cfArray, exit, err = -1 );
	
	err = -1;
	cfBoolean = CFUtilsTestDictionaryCopyCFBoolean( plist, CFSTR( "false" ), &err );
	require_noerr( err, exit );
	require_action( CFIsType( cfBoolean, CFBoolean ), exit, err = -1 );
	require_action( CFEqual( cfBoolean, kCFBooleanFalse ), exit, err = -1 );
	ForgetCF( &cfBoolean );
	cfBoolean = CFUtilsTestDictionaryCopyCFBoolean( plist, CFSTR( "array" ), &err );
	require_action( err == kTypeErr, exit, err = -1 );
	require_action( !cfBoolean, exit, err = -1 );
	cfBoolean = CFUtilsTestDictionaryCopyCFBoolean( plist, CFSTR( "missing" ), &err );
	require_action( err == kNotFoundErr, exit, err = -1 );
	require_action( !cfBoolean, exit, err = -1 );
	
	err = -1;
	cfData = CFUtilsTestDictionaryCopyCFData( plist, CFSTR( "data" ), &err );
	require_noerr( err, exit );
	require_action( CFIsType( cfData, CFData ), exit, err = -1 );
	ForgetCF( &cfData );
	cfData = CFUtilsTestDictionaryCopyCFData( plist, CFSTR( "string" ), &err );
	require_action( err == kTypeErr, exit, err = -1 );
	require_action( !cfData, exit, err = -1 );
	cfData = CFUtilsTestDictionaryCopyCFData( plist, CFSTR( "missing" ), &err );
	require_action( err == kNotFoundErr, exit, err = -1 );
	require_action( !cfData, exit, err = -1 );
	
	err = -1;
	cfDictionary = CFUtilsTestDictionaryCopyCFDictionary( plist, CFSTR( "dict" ), &err );
	require_noerr( err, exit );
	require_action( CFIsType( cfDictionary, CFDictionary ), exit, err = -1 );
	ForgetCF( &cfDictionary );
	cfDictionary = CFUtilsTestDictionaryCopyCFDictionary( plist, CFSTR( "date" ), &err );
	require_action( err == kTypeErr, exit, err = -1 );
	require_action( !cfDictionary, exit, err = -1 );
	cfDictionary = CFUtilsTestDictionaryCopyCFDictionary( plist, CFSTR( "missing" ), &err );
	require_action( err == kNotFoundErr, exit, err = -1 );
	require_action( !cfDictionary, exit, err = -1 );
	
	err = -1;
	cfNumber = CFUtilsTestDictionaryCopyCFNumber( plist, CFSTR( "num" ), &err );
	require_noerr( err, exit );
	require_action( CFIsType( cfNumber, CFNumber ), exit, err = -1 );
	require_action( CFGetInt64( cfNumber, NULL ) == 12345, exit, err = -1 );
	ForgetCF( &cfNumber );
	cfNumber = CFUtilsTestDictionaryCopyCFNumber( plist, CFSTR( "dict" ), &err );
	require_action( err == kTypeErr, exit, err = -1 );
	require_action( !cfNumber, exit, err = -1 );
	cfNumber = CFUtilsTestDictionaryCopyCFNumber( plist, CFSTR( "missing" ), &err );
	require_action( err == kNotFoundErr, exit, err = -1 );
	require_action( !cfNumber, exit, err = -1 );
	
	err = -1;
	cfString = CFUtilsTestDictionaryCopyCFString( plist, CFSTR( "string" ), &err );
	require_noerr( err, exit );
	require_action( CFIsType( cfString, CFString ), exit, err = -1 );
	require_action( CFEqual( cfString, CFSTR( "string" ) ), exit, err = -1 );
	ForgetCF( &cfString );
	cfString = CFUtilsTestDictionaryCopyCFString( plist, CFSTR( "false" ), &err );
	require_action( err == kTypeErr, exit, err = -1 );
	require_action( !cfString, exit, err = -1 );
	cfString = CFUtilsTestDictionaryCopyCFString( plist, CFSTR( "missing" ), &err );
	require_action( err == kNotFoundErr, exit, err = -1 );
	require_action( !cfString, exit, err = -1 );
	
	// GetBoolean / SetBoolean
	
	err = -1;
	b = CFUtilsTestDictionaryGetBoolean( plist, CFSTR( "true" ), &err );
	require_noerr( err, exit );
	require_action( b, exit, err = -1 );
	b = CFUtilsTestDictionaryGetBoolean( plist, CFSTR( "false" ), &err );
	require_noerr( err, exit );
	require_action( !b, exit, err = -1 );
	
	err = CFUtilsTestDictionarySetBoolean( plist, CFSTR( "true-new" ), true );
	require_noerr( err, exit );
	b = CFUtilsTestDictionaryGetBoolean( plist, CFSTR( "true-new" ), &err );
	require_noerr( err, exit );
	require_action( b, exit, err = -1 );
	err = CFUtilsTestDictionarySetBoolean( plist, CFSTR( "false-new" ), false );
	require_noerr( err, exit );
	b = CFUtilsTestDictionaryGetBoolean( plist, CFSTR( "false-new" ), &err );
	require_noerr( err, exit );
	require_action( !b, exit, err = -1 );
	
	// CopyBytes, GetBytes, SetBytes
	
	err = -1;
	len = 0;
	ptr = CFUtilsTestDictionaryCopyBytes( plist, CFSTR( "data" ), &len, &err );
	require_noerr( err, exit );
	require_action( ptr, exit, err = -1 );
	require_action( MemEqual( ptr, len, "\x11\xAA\x22", 3 ), exit, err = -1 );
	ForgetMem( &ptr );
	ptr = CFUtilsTestDictionaryCopyBytes( plist, CFSTR( "date" ), &len, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( !ptr, exit, err = -1 );
	ptr = CFUtilsTestDictionaryCopyBytes( plist, CFSTR( "missing" ), &len, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( !ptr, exit, err = -1 );
	
	err = -1;
	memset( buf, 'z', sizeof( buf ) );
	len = 0;
	ptr2 = CFUtilsTestDictionaryGetBytes( plist, CFSTR( "data" ), buf, sizeof( buf ), &len, &err );
	require_noerr( err, exit );
	require_action( ptr2, exit, err = -1 );
	require_action( ptr2 == buf, exit, err = -1 );
	require_action( MemEqual( ptr2, len, "\x11\xAA\x22", 3 ), exit, err = -1 );
	ptr2 = CFUtilsTestDictionaryGetBytes( plist, CFSTR( "date" ), buf, sizeof( buf ), &len, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( ptr2 == buf, exit, err = -1 );
	ptr2 = CFUtilsTestDictionaryGetBytes( plist, CFSTR( "missing" ), buf, sizeof( buf ), &len, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( ptr2 == buf, exit, err = -1 );
	
	err = CFUtilsTestDictionarySetBytes( plist, CFSTR( "data-new" ), "\xAA\xBB\xCC\xDD", 4 );
	require_noerr( err, exit );
	ptr2 = CFUtilsTestDictionaryGetBytes( plist, CFSTR( "data-new" ), buf, sizeof( buf ), &len, &err );
	require_noerr( err, exit );
	require_action( ptr2, exit, err = -1 );
	require_action( MemEqual( ptr2, len, "\xAA\xBB\xCC\xDD", 4 ), exit, err = -1 );
	
	// CopyCString, GetCString, SetCString
	
	err = -1;
	len = 0;
	cptr = CFUtilsTestDictionaryCopyCString( plist, CFSTR( "string" ), &err );
	require_noerr( err, exit );
	require_action( cptr, exit, err = -1 );
	require_action( strcmp( cptr, "string" ) == 0, exit, err = -1 );
	ForgetMem( &cptr );
	cptr = CFUtilsTestDictionaryCopyCString( plist, CFSTR( "missing" ), &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( !cptr, exit, err = -1 );
	
	err = -1;
	len = 0;
	cptr2 = CFUtilsTestDictionaryGetCString( plist, CFSTR( "string" ), cstr, sizeof( cstr ), &err );
	require_noerr( err, exit );
	require_action( cptr2, exit, err = -1 );
	require_action( strcmp( cptr2, "string" ) == 0, exit, err = -1 );
	cptr2 = CFUtilsTestDictionaryGetCString( plist, CFSTR( "num" ), cstr, sizeof( cstr ), &err );
	require_noerr( err, exit );
	require_action( cptr2, exit, err = -1 );
	require_action( strcmp( cptr2, "12345" ) == 0, exit, err = -1 );
	cptr2 = CFUtilsTestDictionaryGetCString( plist, CFSTR( "missing" ), cstr, sizeof( cstr ), &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( cptr2 == cstr, exit, err = -1 );
	
	err = CFUtilsTestDictionarySetCString( plist, CFSTR( "string-new" ), "string2", kSizeCString );
	require_noerr( err, exit );
	cptr2 = CFUtilsTestDictionaryGetCString( plist, CFSTR( "string-new" ), cstr, sizeof( cstr ), &err );
	require_noerr( err, exit );
	require_action( cptr2, exit, err = -1 );
	require_action( strcmp( cptr2, "string2" ) == 0, exit, err = -1 );
	
	// GetDouble / SetDouble
	
	err = -1;
	d = CFUtilsTestDictionaryGetDouble( plist, CFSTR( "double" ), &err );
	require_noerr( err, exit );
	require_action( d == 123.45, exit, err = -1 );
	d = CFUtilsTestDictionaryGetDouble( plist, CFSTR( "string" ), &err );
	require_action( d == 0, exit, err = -1 );
	d = CFUtilsTestDictionaryGetDouble( plist, CFSTR( "missing" ), &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( d == 0, exit, err = -1 );
	
	err = CFUtilsTestDictionarySetDouble( plist, CFSTR( "double-new" ), 234.56 );
	require_noerr( err, exit );
	d = CFUtilsTestDictionaryGetDouble( plist, CFSTR( "double-new" ), &err );
	require_noerr( err, exit );
	require_action( d == 234.56, exit, err = -1 );
	
	// GetHardwareAddress / SetHardwareAddress
	
	err = -1;
	memset( buf, 'z', sizeof( buf ) );
	u64 = CFUtilsTestDictionaryGetHardwareAddress( plist, CFSTR( "hardwareAddress" ), buf, 8, &err );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 0x0011223344556677 ), exit, err = -1 );
	require_action( memcmp( buf, "\x00\x11\x22\x33\x44\x55\x66\x77", 8 ) == 0, exit, err = -1 );
	u64 = CFUtilsTestDictionaryGetHardwareAddress( plist, CFSTR( "string" ), buf, 8, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( u64 == 0, exit, err = -1 );
	u64 = CFUtilsTestDictionaryGetHardwareAddress( plist, CFSTR( "missing" ), buf, 8, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( u64 == 0, exit, err = -1 );
	
	err = CFUtilsTestDictionarySetHardwareAddress( plist, CFSTR( "hardwareAddress-new" ), "\x55\x66\x77\x88\xAA\xBB\xCC\xDD", 8 );
	require_noerr( err, exit );
	u64 = CFUtilsTestDictionaryGetHardwareAddress( plist, CFSTR( "hardwareAddress-new" ), buf, 8, &err );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 0x55667788AABBCCDD ), exit, err = -1 );
	require_action( memcmp( buf, "\x55\x66\x77\x88\xAA\xBB\xCC\xDD", 8 ) == 0, exit, err = -1 );
	
	// GetMACAddress / SetMACAddress
	
	err = -1;
	memset( buf, 'z', sizeof( buf ) );
	u64 = CFUtilsTestDictionaryGetMACAddress( plist, CFSTR( "macAddress" ), buf, &err );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 0xAABBCCDDEEFF ), exit, err = -1 );
	require_action( memcmp( buf, "\xAA\xBB\xCC\xDD\xEE\xFF", 6 ) == 0, exit, err = -1 );
	u64 = CFUtilsTestDictionaryGetMACAddress( plist, CFSTR( "string" ), buf, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( u64 == 0, exit, err = -1 );
	u64 = CFUtilsTestDictionaryGetMACAddress( plist, CFSTR( "missing" ), buf, &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( u64 == 0, exit, err = -1 );
	
	err = CFUtilsTestDictionarySetMACAddress( plist, CFSTR( "macAddress-new" ), "\x11\xAA\x22\xBB\x33\xCC" );
	require_noerr( err, exit );
	u64 = CFUtilsTestDictionaryGetMACAddress( plist, CFSTR( "macAddress-new" ), buf, &err );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 0x11AA22BB33CC ), exit, err = -1 );
	require_action( memcmp( buf, "\x11\xAA\x22\xBB\x33\xCC", 6 ) == 0, exit, err = -1 );
	
	// GetInt64 / SetInt64
	
	err = -1;
	s64 = CFUtilsTestDictionaryGetInt64( plist, CFSTR( "num" ), &err );
	require_noerr( err, exit );
	require_action( s64 == 12345, exit, err = -1 );
	s64 = CFUtilsTestDictionaryGetInt64( plist, CFSTR( "string" ), &err );
	require_action( s64 == 0, exit, err = -1 );
	s64 = CFUtilsTestDictionaryGetInt64( plist, CFSTR( "missing" ), &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( s64 == 0, exit, err = -1 );
	
	err = CFUtilsTestDictionarySetInt64( plist, CFSTR( "num-new" ), 23456 );
	require_noerr( err, exit );
	s64 = CFUtilsTestDictionaryGetInt64( plist, CFSTR( "num-new" ), &err );
	require_noerr( err, exit );
	require_action( s64 == 23456, exit, err = -1 );
	
	// GetInt64Ranged
	
	err = -1;
	s64 = CFUtilsTestDictionaryGetInt64Ranged( plist, CFSTR( "0" ), 0, 0, &err );
	require_noerr( err, exit );
	require_action( s64 == 0, exit, err = -1 );
	
	err = -1;
	s64 = CFUtilsTestDictionaryGetInt64Ranged( plist, CFSTR( "-10000" ), -65000, 65000, &err );
	require_noerr( err, exit );
	require_action( s64 == -10000, exit, err = -1 );
	
	err = -1;
	s64 = CFUtilsTestDictionaryGetInt64Ranged( plist, CFSTR( "65000" ), 0, 65500, &err );
	require_noerr( err, exit );
	require_action( s64 == 65000, exit, err = -1 );
	
	err = -1;
	s64 = CFUtilsTestDictionaryGetInt64Ranged( plist, CFSTR( "256" ), 0, 255, &err );
	require_action( err == kRangeErr, exit, err = -1 );
	require_action( s64 == 256, exit, err = -1 );
	
	err = -1;
	s64 = CFUtilsTestDictionaryGetInt64Ranged( plist, CFSTR( "-1000" ), 0, 256, &err );
	require_action( err == kRangeErr, exit, err = -1 );
	require_action( s64 == -1000, exit, err = -1 );
	
	// GetSInt8 / GetUInt8
	
	s8 = CFUtilsTestDictionaryGetSInt8( plist, CFSTR( "-100" ), &err );
	require_noerr( err, exit );
	require_action( s8 == -100, exit, err = -1 );
	s8 = CFUtilsTestDictionaryGetSInt8( plist, CFSTR( "127" ), &err );
	require_noerr( err, exit );
	require_action( s8 == 127, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetSInt8( plist, CFSTR( "-1000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetSInt8( plist, CFSTR( "1000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	u8 = CFUtilsTestDictionaryGetUInt8( plist, CFSTR( "200" ), &err );
	require_noerr( err, exit );
	require_action( u8 == 200, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetUInt8( plist, CFSTR( "-100" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetUInt8( plist, CFSTR( "300" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	// GetSInt16 / GetUInt16
	
	s16 = CFUtilsTestDictionaryGetSInt16( plist, CFSTR( "-1000" ), &err );
	require_noerr( err, exit );
	require_action( s16 == -1000, exit, err = -1 );
	s16 = CFUtilsTestDictionaryGetSInt16( plist, CFSTR( "32000" ), &err );
	require_noerr( err, exit );
	require_action( s16 == 32000, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetSInt16( plist, CFSTR( "-100000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetSInt16( plist, CFSTR( "100000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	u16 = CFUtilsTestDictionaryGetUInt16( plist, CFSTR( "12345" ), &err );
	require_noerr( err, exit );
	require_action( u16 == 12345, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetUInt16( plist, CFSTR( "-1" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetUInt16( plist, CFSTR( "70000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	// GetSInt32 / GetUInt32
	
	s32 = CFUtilsTestDictionaryGetSInt32( plist, CFSTR( "-100000" ), &err );
	require_noerr( err, exit );
	require_action( s32 == -100000, exit, err = -1 );
	s32 = CFUtilsTestDictionaryGetSInt32( plist, CFSTR( "200000" ), &err );
	require_noerr( err, exit );
	require_action( s32 == 200000, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetSInt32( plist, CFSTR( "-3000000000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetSInt32( plist, CFSTR( "3000000000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	u32 = CFUtilsTestDictionaryGetUInt32( plist, CFSTR( "3000000000" ), &err );
	require_noerr( err, exit );
	require_action( u32 == 3000000000U, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetUInt32( plist, CFSTR( "-128" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	(void) CFUtilsTestDictionaryGetUInt32( plist, CFSTR( "8000000000" ), &err );
	require_action( err == kRangeErr, exit, err = -1 );
	
	// GetUInt64 / SetUInt64
	
	u64 = CFUtilsTestDictionaryGetUInt64( plist, CFSTR( "200000" ), &err );
	require_noerr( err, exit );
	require_action( u64 == 200000, exit, err = -1 );
	u64 = CFUtilsTestDictionaryGetUInt64( plist, CFSTR( "10000000000000000000" ), &err );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 10000000000000000000 ), exit, err = -1 );
	
	err = CFUtilsTestDictionarySetUInt64( plist, CFSTR( "10000000000000000001-new" ), UINT64_C( 10000000000000000001 ) );
	require_noerr( err, exit );
	u64 = CFUtilsTestDictionaryGetUInt64( plist, CFSTR( "10000000000000000001-new" ), &err );
	require_noerr( err, exit );
	require_action( u64 == UINT64_C( 10000000000000000001 ), exit, err = -1 );
	
	// CopyValue / CopyTypedValue / SetValue
	
	obj = CFUtilsTestDictionaryCopyValue( plist, CFSTR( "string" ), &err );
	require_noerr( err, exit );
	require_action( CFIsType( obj, CFString ) && CFEqual( obj, CFSTR( "string" ) ), exit, err = -1 );
	ForgetCF( &obj );
	obj = CFUtilsTestDictionaryCopyValue( plist, CFSTR( "num" ), &err );
	require_noerr( err, exit );
	require_action( CFIsType( obj, CFNumber ) && ( CFGetInt64( obj, NULL ) == 12345 ), exit, err = -1 );
	ForgetCF( &obj );
	obj = CFUtilsTestDictionaryCopyValue( plist, CFSTR( "missing" ), &err );
	require_action( err == kNotFoundErr, exit, err = -1 );
	require_action( !obj, exit, err = -1 );
	
	obj = CFUtilsTestDictionaryCopyTypedValue( plist, CFSTR( "string" ), CFStringGetTypeID(), &err );
	require_noerr( err, exit );
	require_action( CFIsType( obj, CFString ) && CFEqual( obj, CFSTR( "string" ) ), exit, err = -1 );
	ForgetCF( &obj );
	obj = CFUtilsTestDictionaryCopyTypedValue( plist, CFSTR( "string" ), CFNumberGetTypeID(), &err );
	require_action( err == kTypeErr, exit, err = -1 );
	require_action( !obj, exit, err = -1 );
	obj = CFUtilsTestDictionaryCopyTypedValue( plist, CFSTR( "missing" ), CFNumberGetTypeID(), &err );
	require_action( err == kNotFoundErr, exit, err = -1 );
	require_action( !obj, exit, err = -1 );
	
	err = CFUtilsTestDictionarySetValue( plist, CFSTR( "value-new" ), CFSTR( "value-1" ) );
	require_noerr( err, exit );
	obj = CFUtilsTestDictionaryCopyTypedValue( plist, CFSTR( "value-new" ), CFStringGetTypeID(), &err );
	require_noerr( err, exit );
	require_action( CFIsType( obj, CFString ) && CFEqual( obj, CFSTR( "value-1" ) ), exit, err = -1 );
	ForgetCF( &obj );
	
exit:
	CFReleaseNullSafe( plist );
	CFReleaseNullSafe( cfArray );
	CFReleaseNullSafe( cfBoolean );
	CFReleaseNullSafe( cfData );
	CFReleaseNullSafe( cfDictionary );
	CFReleaseNullSafe( cfNumber );
	CFReleaseNullSafe( cfString );
	CFReleaseNullSafe( obj );
	FreeNullSafe( ptr );
	FreeNullSafe( cptr );
	return( kNoErr );
}

//===========================================================================================================================
//	_CFUtilsTestDictionaryCopyValue
//===========================================================================================================================

static CFTypeRef	_CFUtilsTestDictionaryCopyValue( CFTypeRef inObj, CFStringRef inKey, OSStatus *outErr )
{
	CFDictionaryRef const		dict  = (CFDictionaryRef) inObj;
	CFTypeRef					value = NULL;
	OSStatus					err;
	
	require_action( CFIsType( inObj, CFDictionary ), exit, err = kParamErr );
	
	value = CFDictionaryGetValue( dict, inKey );
	require_action_quiet( value, exit, err = kNotFoundErr );
	
	CFRetain( value );
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	_CFUtilsTestDictionarySetValue
//===========================================================================================================================

static OSStatus	_CFUtilsTestDictionarySetValue( CFTypeRef inObj, CFStringRef inKey, CFTypeRef inValue )
{
	CFMutableDictionaryRef const		dict = (CFMutableDictionaryRef) inObj;
	OSStatus							err;
	
	require_action( CFIsType( inObj, CFDictionary ), exit, err = kParamErr );
	
	CFDictionarySetValue( dict, inKey, inValue );
	err = kNoErr;
	
exit:
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
