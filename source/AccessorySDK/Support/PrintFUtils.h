/*
	File:    	PrintFUtils.h
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

#ifndef	__PrintFUtils_h__
#define	__PrintFUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#if( COMPILER_OBJC )
	#import <Foundation/Foundation.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
/*!	@group		PrintF
	@abstract	printf implementation with many additional features over the standard printf.
	@discussion
	
	Extra features over the standard C snprintf:
	
	64-bit support for %d (%lld), %i (%lli), %u (%llu), %o (%llo), %x (%llx), and %b (%llb).
	Support for hh (char), j (intmax_t), z (size_t), and t (ptrdiff_t) length modifiers. These are part of C99.
	Support for the thousands separator "'" flag character (e.g. "%'d", 12345 -> "12,345").
	Support for the "?" flag for optional suppression. Arg=Non-zero int to include, zero int to suppress.
	%@		- Cocoa/CoreFoundation object. Param is the object. For containers, field width is the indent level.
			  Note: Non-Mac builds must defined DEBUG_CF_OBJECTS_ENABLED to 1 for this feature to be enabled.
	%#@		- Same as %@, but writes the object in XML plist format.
	%.2a	- CEC/HDMI address (2-byte big endian converted to a.b.c.d). Arg=ptr to network address.
	%.4a	- IPv4 address (big endian). Arg=ptr to network address.
	%.6a	- MAC address (6 bytes, AA:BB:CC:DD:EE:FF). Arg=ptr to network address.
	%.8a	- Fibre Channel address (AA:BB:CC:DD:EE:FF:00:11). Arg=ptr to network address.
	%.16a	- IPv6 address (16 bytes). Arg=ptr to network address.
	%#.4a	- IPv4 address in host byte order: Arg=uint32_t * (points to host byte order uint32_t).
	%#.6a	- MAC address from host order uint64_t: Arg=uint64_t * (points to host byte order uint64_t).
	%#a		- IPv4 or IPv6 mDNSAddr. Arg=ptr to mDNSAddr.
	%##a	- IPv4 (if AF_INET defined) or IPv6 (if AF_INET6 defined) sockaddr. Arg=ptr to sockaddr.
	%b		- Binary representation of integer (e.g. 01101011). Modifiers and arg=the same as %d, %x, etc.
	%##b	- Bit numbers of integer (0xA -> "4 2"). 0 bit at right. Modifiers and arg=mostly the same as %d, %x, etc.
	%###b	- Bit numbers of integer (0xA -> "4 2"). 0 bit at left. Modifiers and arg=mostly the same as %d, %x, etc.
			  1 h defaults to 16 bits (%##hb), 2 l defaults to 64 bits (%##llb), otherwise defaults to 32 bits.
			  Precision overrides the number of bits (e.g. %###.8llb means IBM bit 0 is normal bit 7).
	%'c		- Same as %c, but wrapped in single quotes (e.g. %'c, 'a' -> "'a'").
	%C		- Mac-style FourCharCode (e.g. 'APPL'). Arg=32-bit value to print as a Mac-style FourCharCode.
	%'C		- Same as %C, but wrapped in single quotes (e.g. %'C, 'abcd' -> "'abcd'").
	%H		- Hex Dump (e.g. "\x6b\xa7" -> "6B A7"). 1st arg=ptr, 2nd arg=size, 3rd arg=max size.
	%#H		- Hex Dump & ASCII (e.g. "\x41\x62" -> "6B A7 |Ab|"). 1st arg=ptr, 2nd arg=size, 3rd arg=max size.
	%##H	- Raw ASCII dump (e.g. "\x41\x62" -> "|Ab|"). 1st arg=ptr, 2nd arg=size, 3rd arg=max size.
	%.1H	- Multi-line hex dump with ASCII. 1st arg=ptr, 2nd arg=size, 3rd arg=max size. Field width is indent count.
	%.2H	- Hex dump: 1 line <= 16, multi-line > 16. 1st arg=ptr, 2nd arg=size, 3rd arg=max size. Field width is indent count.
	%.3H	- Lowercase hex byte stream (e.g. "00aa11bb"). 1st arg=ptr, 2nd arg=size, 3rd arg=max size.
	%.4H	- Uppercase hex byte stream (e.g. "00AA11BB"). 1st arg=ptr, 2nd arg=size, 3rd arg=max size.
	%m		- Error Message (e.g. 0 -> "kNoErr"). Arg=error code. Size modifiers are the same as %d, %x, etc.
	%#m		- Error and Error Message (e.g. 0 -> "0 kNoErr"). Arg=error code. Size modifiers are the same as %d, %x, etc.
	%##m	- Same as %#m except output is in columns for nicer display of multiple error codes.
	%N		- Now (current Date/Time string) as YYYY-MM-DD HH:MM:SS.ssssss AM/PM. No arg.
	%#N		- Now (current Date/Time string) as YYYY-MM-DD_HH-MM-SS.ssssss-AM/PM. No arg. Suitable for filenames.
	%#s		- Pascal-style length-prefixed string. Arg=ptr to string.
	%##s	- DNS label-sequence name. Arg=ptr to name.
	%###s	- Cleansed function name (i.e. isolate just the [<class>::]<function> part). Arg=ptr to C-string function name.
	%'s		- Same as %s, but wrapped in single quotes (e.g. "%'s, "test" -> "'test'".
	%''s	- Same as %s, but wrapped in double quotes (e.g. "%''s, "test" -> ""test"".
	%S		- UTF-16 string, 0x0000 terminated. Host order if no BOM. Precision is UTF-16 count. Precision includes BOM.
	%#S		- Big Endian UTF-16 string (unless BOM overrides). Otherwise, the same as %S.
	%##S	- Little Endian UTF-16 string (unless BOM overrides). Otherwise, the same as %S.
	%T		- Windows TCHAR string. Arg=wchar_t * if UNICODE/_UNICODE defined, otherwise char *. Otherwise same as %s/%S.
	%U		- Universally Unique Identifier (UUID) in Microsoft little endian format. Arg=ptr to 16-byte UUID.
			  Little endian: 10 b8 a7 6b ad 9d d1 11 80 b4 00 c0 4f d4 30 c8 -> "6ba7b810-9dad-11d1-80b4-00c04fd430c8".
	%#U		- Universally Unique Identifier (UUID) in big endian format. Arg=ptr to 16-byte UUID.
			  Big endian: 6b a7 b8 10 9d ad 11 d1 80 b4 00 c0 4f d4 30 c8 -> "6ba7b810-9dad-11d1-80b4-00c04fd430c8".
	%v		- NumVersion-style version (e.g. 1.2.3b4). Arg is uint32_t.
	%V		- Nested PrintF format string and va_list. 1st arg=const char *format, 2nd arg=va_list *args. Note: 2nd is a ptr.
	
	%{<extension>} extensions:
	
	%{asbd}		CoreAudio AudioStreamBasicDescription. Arg=AudioStreamBasicDescription *.
	%{cec}		HDMI CEC message. Args: const void *ptr, int len.
	%{dur}		Time Duration (e.g. 930232 seconds prints "10d 18h 23m 52s"). Arg=Same as %u.
	%#{dur}		Time Duration (e.g. 930232 seconds prints "10d 18:23:52"). Arg=Same as %u.
	%{end}		End printing. Used with ?, such as "%?{end}", for conditional suppression. No args.
	%{fill}		Repeat a single character N times. Args:int inCharacter, size_t inLen.
	%{flags}	Bit flags. Args=int value, const char *descriptors. Use size modifiers for non-int value.
				Descriptors is a list of <1:bit number> <label string\0> pairs. A pair with an empty label ends the list.
	%#{flags}	Same as %{flags}, but prepends the value in hex (e.g. "0x12 < BUSY LINK >").
	%{pid}		Process name. Arg=pid_t.
	%#{pid}		Process name with numeric PID. Arg=pid_t.
	%{ptr}		Obfuscated pointer as a 16-bit hex value. Arg=const void *.
	%{sline}	Single line string. \r and \n are replaced with ⏎. Arg=ptr to string.
	%{text}		Multi-line text. const void *ptr, size_t len. Field width is indent count of each line.
	%{tlv8}		8-bit Type-Length-Value (TLV) data. const char *descriptors, const void *ptr, int len. Field width is indent count.
				Descriptors is a list of <1:type> <label string\0> pairs. A pair with an empty label ends the list.
	%{tpl}		EFI only: Current TPL. const void *ptr, size_t len.
	%#{tpl}		EFI only: Specified TPL. Arg=EFI_TPL.
	%{txt}		DNS TXT record: print one name=value pair per line. const void *ptr, size_t len. Field width is indent count.
	%#{txt}		DNS TXT record: print everything one line. const void *ptr, size_t len.
	%{xml}		XML-escaped text. Args:const char *inText, size_t inLen.
	%{xpc}		XPC object description. xpc_object_t. Field width is indent count.
===========================================================================================================================*/

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		PrintFRegisterExtension / PrintFDeregisterExtension
	@abstract	Registers or deregisters a PrintF extension handler.
	@discussion	Extensions are specified as %{xyz} where xyz is the extension name.
*/
typedef struct PrintFContext		PrintFContext;
typedef struct
{
	unsigned		leftJustify:1;
	unsigned		forceSign:1;
	unsigned		zeroPad:1;
	unsigned		havePrecision:1;
	unsigned		suppress:1;
	char			hSize;
	char			lSize;
	char			altForm;
	char			sign; // +, -, or space
	unsigned int	fieldWidth;
	size_t			precision;
	char			group;
	char			prefix;
	char			suffix;
	
}	PrintFFormat;
typedef struct { va_list args; } PrintFVAList;
typedef int	( *PrintFExtensionHandler_f )( PrintFContext *ctx, PrintFFormat *inFormat, PrintFVAList *inArgs, void *inUserContext );

OSStatus	PrintFRegisterExtension( const char *inName, PrintFExtensionHandler_f inHandler, void *inContext );
OSStatus	PrintFDeregisterExtension( const char *inName );

// Core APIs for use by extensions only.

int	PrintFCore( PrintFContext *inContext, const char *inFormat, ... );
int	PrintFCoreVAList( PrintFContext *inContext, const char *inFormat, va_list inArgs );

#if( COMPILER_OBJC )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NSPrintF/NSPrintV
	@abstract	PrintF and return as an NSString.
*/
NSString *	NSPrintF( const char *inFormat, ... );
NSString *	NSPrintV( const char *inFormat, va_list inArgs );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SNPrintF/VSNPrintF
	@abstract	PrintF to a fixed-size C string buffer.
*/
int			SNPrintF( void *inBuf, size_t inMaxLen, const char *inFormat, ... ) PRINTF_STYLE_FUNCTION( 3, 4 );
int			VSNPrintF( void *inBuf, size_t inMaxLen, const char *inFormat, va_list inArgs ) PRINTF_STYLE_FUNCTION( 3, 0 );
OSStatus	SNPrintF_Add( char **ioPtr, char *inEnd, const char *inFormat, ... ) PRINTF_STYLE_FUNCTION( 3, 4 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ASPrintF/VASPrintF
	@abstract	PrintF to a malloc'd C string.
*/
int	AppendPrintF( char **ioStr, const char *inFormat, ... ) PRINTF_STYLE_FUNCTION( 2, 3 );
int	ASPrintF( char **outStr, const char *inFormat, ... ) PRINTF_STYLE_FUNCTION( 2, 3 );
int	VASPrintF( char **outStr, const char *inFormat, va_list inArgs ) PRINTF_STYLE_FUNCTION( 2, 0 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CPrintF/VCPrintF
	@abstract	PrintF to a callback function.
*/
typedef int	( *PrintFUserCallBack )( const char *inStr, size_t inSize, void *inContext );

int	CPrintF( PrintFUserCallBack inCallBack, void *inContext, const char *inFormat, ... ) PRINTF_STYLE_FUNCTION( 3, 4 );
int	VCPrintF( PrintFUserCallBack inCallBack, void *inContext, const char *inFormat, va_list inArgs ) PRINTF_STYLE_FUNCTION( 3, 0 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	FPrintF/VFPrintF
	@abstract	Like fprintf, but supports the PrintFCore extensions and platforms that don't have fprintf.
*/
#if( TARGET_HAS_C_LIB_IO )
	int	FPrintF( FILE *inFile, const char *inFormat, ... ) PRINTF_STYLE_FUNCTION( 2, 3 );
	int	VFPrintF( FILE *inFile, const char *inFormat, va_list inArgs ) PRINTF_STYLE_FUNCTION( 2, 0 );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MemPrintF/VMemPrintF
	@abstract	Like SNPrintF, but it doesn't null terminate. Useful for inserting into existing strings.
*/
int	MemPrintF( void *inBuf, size_t inMaxLen, const char *inFormat, ... ) PRINTF_STYLE_FUNCTION( 3, 4 );
int	VMemPrintF( void *inBuf, size_t inMaxLen, const char *inFormat, va_list inArgs ) PRINTF_STYLE_FUNCTION( 3, 0 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PrintFTest
	@abstract	Unit test.
*/
#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	PrintFUtils_Test( void );
#endif

#ifdef __cplusplus
}
#endif

#endif // __PrintFUtils_h__
