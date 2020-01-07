/*
	File:    	CFCompat.h
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
	
	Copyright (C) 2004-2015 Apple Inc. All Rights Reserved.
*/

#ifndef __CFCompat_h__
#define	__CFCompat_h__

#include "CommonServices.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <stdarg.h>
	#include <stddef.h>
#endif

#if( TARGET_OS_BSD )
	#include <sys/event.h>
#endif

#if 0
#pragma mark == Configuration ==
#endif

//===========================================================================================================================
//	Configuration
//===========================================================================================================================

// CFL_SORT_DICTIONARY_XML: 1=Sort Dictionaries by key when outputting XML. 0=Don't sort.
// When building for non-Embedded platforms, sort the XML output of dictionaries by key (for Mac CF parity).

#if( !defined( CFL_SORT_DICTIONARY_XML ) )
	#if( TARGET_OS_VXWORKS )
		#define	CFL_SORT_DICTIONARY_XML		0
	#else
		#define	CFL_SORT_DICTIONARY_XML		1
	#endif
#endif

// CFL_XML_PARSING: 1=Support XML property lists, 0=Don't support XML. Requires expat library and headers.

#if( !defined( CFL_XML_PARSING ) )
	#if( TARGET_OS_DARWIN )
		#define	CFL_XML_PARSING		1
	#else
		#define	CFL_XML_PARSING		0
	#endif
#endif

#if( CFLITE_ENABLED )
	#include "CFLite.h"
#endif

// CFCOMPAT_NOTIFICATIONS_ENABLED: 1=Include CFLite notification stuff.

#if( !defined( CFCOMPAT_NOTIFICATIONS_ENABLED ) )
	#if( CFLITE_ENABLED )
		#define	CFCOMPAT_NOTIFICATIONS_ENABLED		1
	#else
		#define	CFCOMPAT_NOTIFICATIONS_ENABLED		0
	#endif
#endif

// CFCOMPAT_PREFERENCES_ENABLED: 1=Include CFPreferences stuff.

#if( !defined( CFCOMPAT_PREFERENCES_ENABLED ) )
	#if( CFLITE_ENABLED )
		#define	CFCOMPAT_PREFERENCES_ENABLED		1
	#else
		#define	CFCOMPAT_PREFERENCES_ENABLED		0
	#endif
#endif

// CFCOMPAT_RUNTIME_CLASSES_ENABLED: 1=Enable runtime classes (requires private CF headers on the Mac). 0=Don't compile in.

#if( !defined( CFCOMPAT_RUNTIME_CLASSES_ENABLED ) )
	#if( CFLITE_ENABLED )
		#define CFCOMPAT_RUNTIME_CLASSES_ENABLED		1
	#else
		#define CFCOMPAT_RUNTIME_CLASSES_ENABLED		0
	#endif
#endif

// CFCOMPAT_HAS_UNICODE_SUPPORT: 1=Use utfconv for Unicode conversions. 0=No Unicode support (everything must be UTF-8).

#if( !defined( CFCOMPAT_HAS_UNICODE_SUPPORT ) )
	#define CFCOMPAT_HAS_UNICODE_SUPPORT		1
#endif

#if 0
#pragma mark == General ==
#endif

//===========================================================================================================================
//	General
//===========================================================================================================================

#ifdef __cplusplus
extern "C" {
#endif

#if( CFLITE_ENABLED )

#define CF_ENUM( _type, _name )		_type _name; enum

// General Types

#define UInt8		uint8_t

// Base

#define CFAllocatorRef			CFLAllocatorRef
#define	kCFAllocatorDefault		kCFLAllocatorDefault
#define	kCFAllocatorMalloc		kCFLAllocatorMalloc
#define	kCFAllocatorNull		kCFLAllocatorNull

#define	CFTypeID				CFLTypeID

#define	_kCFRuntimeNotATypeID	0

typedef const void *			CFTypeRef;
#define CFIndex					CFLIndex
#define	CFHashCode				CFLHashCode
#define	CFOptionFlags			uint32_t
#define	CFPropertyListRef		CFTypeRef

typedef int		CFComparisonResult;
#define kCFCompareLessThan			-1
#define kCFCompareEqualTo			0
#define kCFCompareGreaterThan		1

#define CFComparatorFunction	CFLComparatorFunction

#define kCFNotFound					-1

#define CFRange		CFLRange

STATIC_INLINE CFRange	CFRangeMake( CFIndex inLocation, CFIndex inLength )
{
	CFRange		range;
	
	range.location	= inLocation;
	range.length	= inLength;
	return( range );
}

typedef double		CFAbsoluteTime;
typedef double		CFTimeInterval;

CFTypeID	CFGetTypeID( CFTypeRef inObj );
CFIndex		CFGetRetainCount( CFTypeRef inObj );
CFTypeRef	CFRetain( CFTypeRef inObject );
void		CFRelease( CFTypeRef inObject );
Boolean		CFEqual( CFTypeRef inLeft, CFTypeRef inRight );
CFHashCode	CFHash( CFTypeRef inObject );

enum
{
	kCFPropertyListImmutable					= 0,
	kCFPropertyListMutableContainers			= 1, 
	kCFPropertyListMutableContainersAndLeaves	= 2
};

CFPropertyListRef
	CFPropertyListCreateDeepCopy( 
		CFAllocatorRef 		inAllocator, 
		CFPropertyListRef	inPropertyList, 
		CFOptionFlags		inMutabilityOption );

// Array

#define	CFArrayRef					CFLArrayRef
#define	CFMutableArrayRef			CFLArrayRef
#define	CFArrayCallBacks			CFLArrayCallBacks
#define kCFTypeArrayCallBacks		kCFLArrayCallBacksCFLTypes

CFTypeID	CFArrayGetTypeID( void );
CFArrayRef
	CFArrayCreate( 
		CFAllocatorRef				inAllocator, 
		const void **				inValues, 
		CFIndex						inCount, 
		const CFArrayCallBacks *	inCallBacks );
CFArrayRef			CFArrayCreateCopy( CFAllocatorRef inAllocator, CFArrayRef inArray );
CFMutableArrayRef	CFArrayCreateMutable( CFAllocatorRef inAllocator, CFIndex inCapacity, const CFArrayCallBacks *inCallBacks );
CFMutableArrayRef	CFArrayCreateMutableCopy( CFAllocatorRef inAllocator, CFIndex inCapacity, CFArrayRef inArray );
CFIndex		CFArrayGetCount( CFArrayRef inObject );
CFIndex		CFArrayGetFirstIndexOfValue( CFArrayRef inArray, CFRange inRange, const void *inValue );
void		CFArrayGetValues( CFArrayRef inArray, CFRange inRange, const void **inValues );
void *		CFArrayGetValueAtIndex( CFArrayRef inObject, CFIndex inIndex );
void		CFArraySetValueAtIndex( CFMutableArrayRef inObject, CFIndex inIndex, const void *inValue );
void		CFArrayInsertValueAtIndex( CFMutableArrayRef inObject, CFIndex inIndex, const void *inValue );
void		CFArrayAppendValue( CFMutableArrayRef inObject, const void *inValue );
void		CFArrayRemoveValueAtIndex( CFMutableArrayRef inObject, CFIndex inIndex );
void		CFArrayRemoveAllValues( CFMutableArrayRef inObject );
Boolean		CFArrayContainsValue( CFArrayRef inArray, CFRange inRange, const void *inValue );
void		CFArrayAppendArray( CFMutableArrayRef inDstArray, CFArrayRef inSrcArray, CFRange inSrcRange );

#define		CFArrayApplierFunction		CFLArrayApplierFunction
#define		CFArrayApplyFunction		CFLArrayApplyFunction
#define		CFArraySortValues			CFLArraySortValues

// Boolean

#define	CFBooleanRef		CFLBooleanRef
#define kCFBooleanTrue		kCFLBooleanTrue
#define kCFBooleanFalse		kCFLBooleanFalse

CFTypeID	CFBooleanGetTypeID( void );
Boolean		CFBooleanGetValue( CFBooleanRef inBoolean );

// Data

#define	CFDataRef			CFLDataRef
#define	CFMutableDataRef	CFLDataRef

CFTypeID			CFDataGetTypeID( void );
CFDataRef			CFDataCreate( CFAllocatorRef inAllocator, const uint8_t *inData, CFIndex inSize );
CFMutableDataRef	CFDataCreateMutable( CFAllocatorRef inAllocator, CFIndex inCapacity );
CFMutableDataRef	CFDataCreateMutableCopy( CFAllocatorRef inAllocator, CFIndex inCapacity, CFDataRef inData );
CFDataRef
	CFDataCreateWithBytesNoCopy( 
		CFAllocatorRef 	inAllocator, 
		const uint8_t *	inBytes,
		CFIndex			inLen, 
		CFAllocatorRef	inBytesDeallocator );
CFDataRef			CFDataCreateSubdataWithRangeNoCopy( CFDataRef inData, CFRange inRange, OSStatus *outErr );
CFIndex				CFDataGetLength( CFDataRef inObject );
#define				CFDataSetLength( OBJ, LEN )		CFLDataSetData( ( OBJ ), NULL, (size_t)( LEN ) )
const uint8_t *		CFDataGetBytePtr( CFDataRef inObject );
void				CFDataGetBytes( CFDataRef inObject, CFRange inRange, uint8_t *inBuffer );
uint8_t *			CFDataGetMutableBytePtr( CFDataRef inObject );
void				CFDataAppendBytes( CFMutableDataRef inObject, const uint8_t *inData, CFIndex inSize );

// Date

#define kCFAbsoluteTimeIntervalSince1970		978307200.0

#define	CFDateRef		CFLDateRef

CFTypeID	CFDateGetTypeID( void );
CFDateRef	CFDateCreate( CFAllocatorRef inAllocator, CFAbsoluteTime inTime );
CFDateRef
	CFDateCreateWithComponents( 
		CFAllocatorRef	inAllocator, 
		int				inYear, 
		int				inMonth, 
		int				inDay, 
		int				inHour, 
		int				inMinute, 
		int				inSecond );
CFAbsoluteTime		CFDateGetAbsoluteTime( CFDateRef inDate );
CFComparisonResult	CFDateCompare( CFDateRef inA, CFDateRef inB, void *inContext );

// Dictionary

#define	CFDictionaryRef						CFLDictionaryRef
#define	CFMutableDictionaryRef				CFLDictionaryRef
#define	CFDictionaryKeyCallBacks			CFLDictionaryKeyCallBacks
#define	CFDictionaryValueCallBacks			CFLDictionaryValueCallBacks
#define	kCFTypeDictionaryKeyCallBacks		kCFLDictionaryKeyCallBacksCFLTypes
#define	kCFTypeDictionaryValueCallBacks		kCFLDictionaryValueCallBacksCFLTypes

CFTypeID	CFDictionaryGetTypeID( void );

CFDictionaryRef
	CFDictionaryCreate(
		CFAllocatorRef 						inAllocator,
		const void **						inKeys,
		const void **						inValues,
		CFIndex								inCount,
		const CFDictionaryKeyCallBacks *	inKeyCallBacks,
		const CFDictionaryValueCallBacks *	inValueCallBacks );

CFMutableDictionaryRef
	CFDictionaryCreateMutable( 
		CFAllocatorRef 						inAllocator, 
		CFIndex								inCapacity, 
		const CFDictionaryKeyCallBacks *	inKeyCallBacks, 
		const CFDictionaryValueCallBacks *	inValueCallBacks );
CFMutableDictionaryRef	CFDictionaryCreateMutableCopy( CFAllocatorRef inAllocator, CFIndex inCapacity, CFDictionaryRef inDict );

CFIndex			CFDictionaryGetCount( CFDictionaryRef inObject );
const void *	CFDictionaryGetValue( CFDictionaryRef inObject, const void *inKey );
void			CFDictionarySetValue( CFMutableDictionaryRef inObject, const void *inKey, const void *inValue );
void			CFDictionaryAddValue( CFMutableDictionaryRef inObject, const void *inKey, const void *inValue );
void			CFDictionaryRemoveValue( CFMutableDictionaryRef inObject, const void *inKey );
void			CFDictionaryRemoveAllValues( CFMutableDictionaryRef inObject );

#define CFDictionaryContainsKey		CFLDictionaryContainsKey

void	CFDictionaryGetKeysAndValues( CFDictionaryRef inObject, const void **ioKeys, const void **ioValues );

typedef void	( *CFDictionaryApplierFunction )( const void *inKey, const void *inValue, void *inContext );

void	CFDictionaryApplyFunction( CFDictionaryRef inDict, CFLDictionaryApplierFunction inApplier, void *inContext );

// Null

#define	CFNullRef				CFLNullRef
#define kCFNull					kCFLNull
#define CFNullGetTypeID()		CFLNullGetTypeID()

// Number

#define	CFNumberRef					CFLNumberRef

#define	CFNumberType				CFLNumberType
#define	kCFNumberSInt8Type			kCFLNumberSInt8Type
#define	kCFNumberSInt16Type			kCFLNumberSInt16Type
#define	kCFNumberSInt32Type			kCFLNumberSInt32Type
#define	kCFNumberSInt64Type			kCFLNumberSInt64Type
#define	kCFNumberSInt128Type		kCFLNumberSInt128Type
#define	kCFNumberCharType			kCFLNumberCharType
#define	kCFNumberShortType			kCFLNumberShortType
#define	kCFNumberIntType			kCFLNumberIntType
#define	kCFNumberLongType			kCFLNumberLongType
#define	kCFNumberLongLongType		kCFLNumberLongLongType
#define	kCFNumberCFIndexType		kCFLNumberCFIndexType
#define	kCFNumberMaxType			kCFLNumberMaxType

#if( CFL_FLOATING_POINT_NUMBERS )
	#define	kCFNumberFloat32Type	kCFLNumberFloat32Type
	#define	kCFNumberFloat64Type	kCFLNumberFloat64Type
	#define	kCFNumberFloatType		kCFLNumberFloatType
	#define	kCFNumberDoubleType		kCFLNumberDoubleType
#endif

CFTypeID	CFNumberGetTypeID( void );
CFNumberRef	CFNumberCreate( CFAllocatorRef inAllocator, CFNumberType inType, const void *inValuePtr );
#define		CFNumberGetByteSize( NUM )	CFLNumberGetByteSize( NUM )
#define		CFNumberGetType( NUM )		CFLNumberGetType( NUM )
Boolean		CFNumberGetValue( CFNumberRef inNumber, CFNumberType inType, void *outValue );

CFComparisonResult	CFNumberCompare( CFNumberRef inLeft, CFNumberRef inRight, void *inContext );

#if( CFL_FLOATING_POINT_NUMBERS )
	#define CFNumberIsFloatType( NUM )		CFLNumberIsFloatType( NUM )
#endif

// String

#define	CFSTR						CFLSTR

#define	CFStringRef					CFLStringRef
#define	CFMutableStringRef			CFLStringRef

typedef uint32_t	CFStringEncoding;
enum
{
	kCFStringEncodingMacRoman		= 0,
	kCFStringEncodingWindowsLatin1	= 0x0500,		// ANSI codepage 1252
	kCFStringEncodingISOLatin1		= 0x0201,		// ISO 8859-1
	kCFStringEncodingNextStepLatin	= 0x0B01,		// NextStep encoding
	kCFStringEncodingASCII			= 0x0600,		// 0..127 (in creating CFString, values greater than 0x7F are treated as corresponding Unicode value)
	kCFStringEncodingUnicode		= 0x0100,		// kTextEncodingUnicodeDefault  + kTextEncodingDefaultFormat (aka kUnicode16BitFormat)
	kCFStringEncodingUTF8			= 0x08000100,	// kTextEncodingUnicodeDefault + kUnicodeUTF8Format
	kCFStringEncodingNonLossyASCII	= 0x0BFF,		// 7bit Unicode variants used by Cocoa & Java
	kCFStringEncodingUTF16			= 0x0100,		// kTextEncodingUnicodeDefault + kUnicodeUTF16Format (alias of kCFStringEncodingUnicode)
	kCFStringEncodingUTF16BE		= 0x10000100,	// kTextEncodingUnicodeDefault + kUnicodeUTF16BEFormat
	kCFStringEncodingUTF16LE		= 0x14000100,	// kTextEncodingUnicodeDefault + kUnicodeUTF16LEFormat
	kCFStringEncodingUTF32			= 0x0c000100,	// kTextEncodingUnicodeDefault + kUnicodeUTF32Format
	kCFStringEncodingUTF32BE		= 0x18000100,	// kTextEncodingUnicodeDefault + kUnicodeUTF32BEFormat
	kCFStringEncodingUTF32LE		= 0x1c000100	// kTextEncodingUnicodeDefault + kUnicodeUTF32LEFormat	
};

typedef enum
{
	kCFCompareCaseInsensitive	= 1, 
	kCFCompareNumerically		= 64

}	CFStringCompareFlags;

CFTypeID	CFStringGetTypeID( void );

CFStringRef			CFStringCreateCopy( CFAllocatorRef inAllocator, CFStringRef inString );
CFMutableStringRef	CFStringCreateMutable( CFAllocatorRef inAllocator, CFIndex inMaxLength );
CFMutableStringRef	CFStringCreateMutableCopy( CFAllocatorRef inAllocator, CFIndex inMaxLength, CFStringRef inString );
CFStringRef
	CFStringCreateWithBytes( 
		CFAllocatorRef 		inAllocator, 
		const uint8_t *		inBytes, 
		CFIndex				inSize, 
		CFStringEncoding	inEncoding, 
		Boolean				inIsExternal );
CFStringRef	CFStringCreateWithCString( CFAllocatorRef inAllocator, const char *inString, CFStringEncoding inEncoding );
CFStringRef
	CFStringCreateWithFormat( 
		CFAllocatorRef	inAllocator, 
		CFDictionaryRef	inFormatOptions, 
		CFStringRef		inFormat, 
		... );

CFIndex	CFStringGetLength( CFStringRef inObject );
CFIndex	CFStringGetMaximumSizeForEncoding( CFIndex inSize, CFStringEncoding inEncoding );
const char *	CFStringGetCStringPtr( CFStringRef inString, CFStringEncoding inEncoding );
Boolean	CFStringGetCString( CFStringRef inString, char *outBuffer, CFIndex inMaxSize, CFStringEncoding inEncoding );
Boolean	CFStringGetPascalString( CFStringRef inString, unsigned char *inBuf, CFIndex inBufSize, CFStringEncoding inEncoding );
CFIndex
	CFStringGetBytes(
		CFStringRef			inString,
		CFRange				inRange,
		CFStringEncoding	inEncoding,
		UInt8				inLossByte,
		Boolean				inIsExternalRepresentation,
		UInt8 *				inBuffer,
		CFIndex				inMaxBufLen,
		CFIndex *			outUsedBufLen );

CFComparisonResult	CFStringCompare( CFStringRef inS1, CFStringRef inS2, CFOptionFlags inOptions );
int32_t	CFStringGetIntValue( CFStringRef inObject );
CFIndex
	CFStringFindAndReplace( 
		CFMutableStringRef	inString, 
		CFStringRef			inStringToFind, 
		CFStringRef			inReplacementString, 
		CFRange				inRangeToSearch, 
		CFOptionFlags		inCompareOptions );

Boolean
	CFStringFindWithOptions(
		CFStringRef		inStringToSearch, 
		CFStringRef		inStringToFind, 
		CFRange			inRangeToSearch, 
		CFOptionFlags	inSearchOptions,
		CFRange *		outRange );

void	CFStringAppendCString( CFMutableStringRef inCFStr, const char *inCStr, CFStringEncoding inEncoding );

typedef struct
{
    UInt8 byte0;
    UInt8 byte1;
    UInt8 byte2;
    UInt8 byte3;
    UInt8 byte4;
    UInt8 byte5;
    UInt8 byte6;
    UInt8 byte7;
    UInt8 byte8;
    UInt8 byte9;
    UInt8 byte10;
    UInt8 byte11;
    UInt8 byte12;
    UInt8 byte13;
    UInt8 byte14;
    UInt8 byte15;
	
}	CFUUIDBytes;

#endif // CFLITE_ENABLED

#if 0
#pragma mark == Runtime Classes ==
#endif

//===========================================================================================================================
//	Runtime Classes
//===========================================================================================================================

#if( CFCOMPAT_RUNTIME_CLASSES_ENABLED && !defined( __COREFOUNDATION_CFRUNTIME__ ) )

#define CFRuntimeBase	CFLObject

// Warning: this stuff is copied from the private CFRuntime.h so it may change.

typedef void		( *CFRuntimeInitFunc )( CFTypeRef inObj );
typedef CFTypeRef	( *CFRuntimeCopyFunc )( CFAllocatorRef inAllocator, CFTypeRef inObj );
typedef void		( *CFRuntimeFinalizeFunc )( CFTypeRef inObj );
typedef Boolean		( *CFRuntimeEqualFunc )( CFTypeRef inObj1, CFTypeRef inObj2 );
typedef CFHashCode	( *CFRuntimeHashFunc )( CFTypeRef inObj);
typedef CFStringRef	( *CFRuntimeCopyFormattingDescFunc )( CFTypeRef inObj, CFDictionaryRef inFomatOptions );
typedef CFStringRef	( *CFRuntimeCopyDebugDescFunc )( CFTypeRef inObj );
typedef CFStringRef	( *CFRuntimeReclaimFunc )( CFTypeRef inObj );
typedef uint32_t	( *CFRuntimeRefCountFunc )( intptr_t inOp, CFTypeRef inObj );

typedef struct
{
	CFIndex								version;
	const char *						className;
	CFRuntimeInitFunc					init;
	CFRuntimeCopyFunc					copy;
	CFRuntimeFinalizeFunc				finalize;
	CFRuntimeEqualFunc					equal;
	CFRuntimeHashFunc					hash;
	CFRuntimeCopyFormattingDescFunc		copyFormattingDesc;
	CFRuntimeCopyDebugDescFunc			copyDebugDesc;
	CFRuntimeReclaimFunc				reclaim;
	CFRuntimeRefCountFunc				refcount;

}	CFRuntimeClass;

CFTypeID	_CFRuntimeRegisterClass( const CFRuntimeClass * const inClass );
CFTypeRef
	_CFRuntimeCreateInstance( 
		CFAllocatorRef	inAllocator, 
		CFTypeID		inTypeID, 
		CFIndex 		inExtraBytes, 
		unsigned char *	inCategory );

#endif // CFCOMPAT_RUNTIME_CLASSES_ENABLED && !defined( __COREFOUNDATION_CFRUNTIME__ )

#if 0
#pragma mark == Plists/XML ==
#endif

//===========================================================================================================================
//	Plists/XML
//===========================================================================================================================

#if( CFLITE_ENABLED )
	typedef CFIndex		CFPropertyListFormat;
	#define kCFPropertyListOpenStepFormat			1
	#define kCFPropertyListXMLFormat_v1_0			100
	#define kCFPropertyListBinaryFormat_v1_0		200
	#define kCFPropertyListBinaryFormat_Streamed	1000
	
	typedef struct CFErrorPrivate *		CFErrorRef;
	
	CFDataRef
		CFPropertyListCreateData( 
			CFAllocatorRef			inAllocator, 
			CFPropertyListRef		inPropertyList, 
			CFPropertyListFormat	inFormat, 
			CFOptionFlags			inOptions, 
			CFErrorRef *			outError );
	
	CFPropertyListRef
		CFPropertyListCreateWithData( 
			CFAllocatorRef			inAllocator, 
			CFDataRef				inData, 
			CFOptionFlags			inOptions, 
			CFPropertyListFormat *	outFormat, 
			CFErrorRef *			outError );
	
	CFPropertyListRef
		CFPropertyListCreateFromXMLData( 
			CFAllocatorRef	inAllocator, 
			CFDataRef 		inXML, 
			CFOptionFlags	inOptions, 
			CFStringRef *	outErrorString );
	
	CFDataRef	CFPropertyListCreateXMLData( CFAllocatorRef inAllocator, CFPropertyListRef inPlist );
#endif

#if( DEBUG )
	OSStatus	CFLogXML( CFPropertyListRef inObj );
	
	#define cflogxml_debug( OBJ )		CFLogXML( OBJ )
#else
	#define cflogxml_debug( OBJ )
#endif

#define cfl_debug_dump_object( OBJ )	cflogxml_debug( OBJ ) // DEPRECATED -- Use cflogxml_debug instead.

OSStatus	CFLiteXMLTest( Boolean inPrint );

#if 0
#pragma mark == Preferences ==
#endif

//===========================================================================================================================
//	Preferences
//===========================================================================================================================

CFArrayRef			CFPreferencesCopyKeyList_compat( CFStringRef inAppID, CFStringRef inUser, CFStringRef inHost );
CFPropertyListRef	CFPreferencesCopyAppValue_compat( CFStringRef inKey, CFStringRef inAppID );
void				CFPreferencesSetAppValue_compat( CFStringRef inKey, CFPropertyListRef inValue, CFStringRef inAppID );
Boolean				CFPreferencesAppSynchronize_compat( CFStringRef inAppID );

OSStatus			CFLitePreferencesFileTest( void );

#if( CFLITE_ENABLED )
	#define kCFPreferencesCurrentApplication		CFSTR( "kCFPreferencesCurrentApplication" )
	#define kCFPreferencesCurrentUser				CFSTR( "kCFPreferencesCurrentUser" )
	#define kCFPreferencesAnyHost					CFSTR( "kCFPreferencesAnyHost" )
	
	#define CFPreferencesCopyKeyList( APP_ID, USER, HOST )		CFPreferencesCopyKeyList_compat( (APP_ID), (USER), (HOST) )
	#define CFPreferencesCopyAppValue( KEY, APP_ID )			CFPreferencesCopyAppValue_compat( (KEY), (APP_ID) )
	#define CFPreferencesSetAppValue( KEY, VALUE, APP_ID )		CFPreferencesSetAppValue_compat( (KEY), (VALUE), (APP_ID) )
	#define CFPreferencesAppSynchronize( APP_ID )				CFPreferencesAppSynchronize_compat( (APP_ID) )
#endif

#if( !CFLITE_ENABLED )
	Boolean
		CFPreferencesGetBooleanValue( 
			CFStringRef inKey, 
			CFStringRef inAppID, 
			CFStringRef inUser, 
			CFStringRef inHost, 
			Boolean *	outValid );
	
	CFIndex
		CFPreferencesGetIntegerValue( 
			CFStringRef inKey, 
			CFStringRef inAppID, 
			CFStringRef inUser, 
			CFStringRef inHost, 
			Boolean *	outValid );
	
	OSStatus
		CFPreferencesSetIntegerValue( 
			CFStringRef			inKey, 
			CFIndex				inValue, 
			CFStringRef			inAppID, 
			CFStringRef			inUser, 
			CFStringRef			inHost );
#endif

#if 0
#pragma mark == RunLoop ==
#endif

//===========================================================================================================================
//	RunLoop
//===========================================================================================================================

#if( CFLITE_ENABLED )

typedef struct CFRunLoop *				CFRunLoopRef;
typedef struct CFRunLoopSource *		CFRunLoopSourceRef;
typedef struct CFRunLoopTimer *			CFRunLoopTimerRef;

extern CFStringRef		kCFRunLoopDefaultMode;
extern CFStringRef		kCFRunLoopCommonModes;

enum
{
	kCFRunLoopRunFinished		= 1,
	kCFRunLoopRunStopped		= 2,
	kCFRunLoopRunTimedOut		= 3,
	kCFRunLoopRunHandledSource	= 4
};

OSStatus	CFRunLoopEnsureInitialized( void );
OSStatus	CFRunLoopFinalize( void );

#if( TARGET_OS_WINDOWS )
	typedef Boolean
		( *CFRunLoopWindowHookPtr )( 
			HWND		inWindow, 
			UINT		inMsg, 
			WPARAM		inWParam, 
			LPARAM		inLParam, 
			LRESULT *	outResult, 
			void *		inContext );
	
	HWND	CFRunLoopGetWindow( void );
	void	CFRunLoopSetWindowHook( CFRunLoopWindowHookPtr inHook, void *inContext );
#endif

CFTypeID		CFRunLoopGetTypeID( void );
CFRunLoopRef	CFRunLoopGetCurrent( void );
CFRunLoopRef	CFRunLoopGetMain( void );
int				CFRunLoopGetFD( CFRunLoopRef inRL );

#if( TARGET_OS_BSD )
	typedef void ( *CFRunLoopEventHookPtr )( const struct kevent *inEvent, void *inContext );
	
	void	CFRunLoopSetEventHook( CFRunLoopRef inRL, CFRunLoopEventHookPtr inHook, void *inContext );
#endif

void	CFRunLoopRun( void );
int32_t	CFRunLoopRunInMode( CFStringRef inMode, CFTimeInterval inSeconds, Boolean inReturnAfterSourceHandled );
void	CFRunLoopStop( CFRunLoopRef inRL );
void	CFRunLoopWakeUp( CFRunLoopRef inRL );
void	CFRunLoopAddSource( CFRunLoopRef inRL, CFRunLoopSourceRef inSource, CFStringRef inMode );
void	CFRunLoopRemoveSource( CFRunLoopRef inRL, CFRunLoopSourceRef inSource, CFStringRef inMode );
void	CFRunLoopAddTimer( CFRunLoopRef inRL, CFRunLoopTimerRef inTimer, CFStringRef inMode );
void	CFRunLoopRemoveTimer( CFRunLoopRef inRL, CFRunLoopTimerRef inTimer, CFStringRef inMode );

#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	CFLiteRunLoopTest( void );
#endif

#endif // CFLITE_ENABLED

//===========================================================================================================================
//	CFRunLoopSource
//===========================================================================================================================

#if( CFLITE_ENABLED )

typedef struct
{
	CFIndex			version;
	void *			info;
	const void *	( *retain )( const void *inInfo );
	void			( *release )( const void *inInfo );
	CFStringRef		( *copyDescription )( const void *inInfo );
	Boolean			( *equal )( const void *inInfo1, const void *inInfo2 );
	CFHashCode		( *hash )( const void *inInfo );
	void			( *schedule )(void *inInfo, CFRunLoopRef inRL, CFStringRef inMode );
	void			( *cancel )(void *inInfo, CFRunLoopRef inRL, CFStringRef inMode );
	void			( *perform )( void *inInfo );
	
}	CFRunLoopSourceContext;

CFTypeID			CFRunLoopSourceGetTypeID( void );
CFRunLoopSourceRef	CFRunLoopSourceCreate( CFAllocatorRef inAllocator, CFIndex inOrder, CFRunLoopSourceContext *inContext );
void				CFRunLoopSourceInvalidate( CFRunLoopSourceRef inSource );
void				CFRunLoopSourceSignal( CFRunLoopSourceRef inSource );

#endif // CFLITE_ENABLED

//===========================================================================================================================
//	CFRunLoopTimer
//===========================================================================================================================

#if( CFLITE_ENABLED )

typedef struct
{
	CFIndex		version;
	void *		info;
	void *		retain;				// UNSUPPORTED...must be NULL.
	void *		release;			// UNSUPPORTED...must be NULL.
	void *		copyDescription;	// UNSUPPORTED...must be NULL.

}	CFRunLoopTimerContext;

typedef void ( *CFRunLoopTimerCallBack )( CFRunLoopTimerRef inTimer, void *inContext );

CFAbsoluteTime	CFAbsoluteTimeGetCurrent( void );

CFTypeID	CFRunLoopTimerGetTypeID( void );

CFRunLoopTimerRef
	CFRunLoopTimerCreate( 
		CFAllocatorRef			inAllocator,
		CFAbsoluteTime			inFireDate,
		CFTimeInterval			inInterval,
		CFOptionFlags			inFlags,
		CFIndex					inOrder,
		CFRunLoopTimerCallBack	inCallBack,
		CFRunLoopTimerContext *	inContext );

void	CFRunLoopTimerSetNextFireDate( CFRunLoopTimerRef inTimer, CFAbsoluteTime inFireDate );
void	CFRunLoopTimerInvalidate( CFRunLoopTimerRef inTimer );

#endif // CFLITE_ENABLED

//===========================================================================================================================
//	CFSocket
//===========================================================================================================================

#if( CFLITE_ENABLED )

typedef struct CFSocket *		CFSocketRef;
typedef SocketRef				CFSocketNativeHandle;

typedef enum
{
	kCFSocketSuccess	= 0,
	kCFSocketError		= -1,
	kCFSocketTimeout	= -2
	
}	CFSocketError;

typedef uint32_t	CFSocketCallBackType;
enum
{
	kCFSocketNoCallBack			= 0,
	kCFSocketReadCallBack		= 0x1,
//	kCFSocketAcceptCallBack		= 0x2, NOT SUPPORTED
//	kCFSocketDataCallBack		= 0x3, NOT SUPPORTED
	kCFSocketConnectCallBack	= 0x4,
	kCFSocketWriteCallBack		= 0x8
	
};

enum
{
	kCFSocketAutomaticallyReenableReadCallBack		= 0x01,
	kCFSocketAutomaticallyReenableAcceptCallBack	= 0x02,
	kCFSocketAutomaticallyReenableDataCallBack		= 0x03,
	kCFSocketAutomaticallyReenableWriteCallBack		= 0x08,
	kCFSocketCloseOnInvalidate						= 0x80
};

typedef void
	( *CFSocketCallBack )( 
		CFSocketRef				inSock, 
		CFSocketCallBackType	inType, 
		CFDataRef				inAddr,
		const void *			inData,
		void *					inContext );

typedef struct
{
	CFIndex			version;
	void *			info;
	const void *	( *retain )( const void *inInfo );
	void			( *release )( const void *inInfo );
	CFStringRef		( *copyDescription )( const void *inInfo );
	
}	CFSocketContext;

CFTypeID	CFSocketGetTypeID( void );

CFSocketRef
	CFSocketCreateWithNative( 
		CFAllocatorRef			inAllocator, 
		CFSocketNativeHandle	inSock, 
		CFOptionFlags			inCallBackTypes, 
		CFSocketCallBack		inCallBack, 
		const CFSocketContext *	inContext );

void	CFSocketInvalidate( CFSocketRef inSock );

CFSocketError	CFSocketConnectToAddress( CFSocketRef inSock, CFDataRef inAddr, CFTimeInterval inTimeout );

CFSocketNativeHandle	CFSocketGetNative( CFSocketRef inSock );

CFOptionFlags	CFSocketGetSocketFlags( CFSocketRef inSock );
void			CFSocketSetSocketFlags( CFSocketRef inSock, CFOptionFlags inFlags );

void	CFSocketEnableCallBacks( CFSocketRef inSock, CFOptionFlags inCallBackTypes );
void	CFSocketDisableCallBacks( CFSocketRef inSock, CFOptionFlags inCallBackTypes );

CFRunLoopSourceRef	CFSocketCreateRunLoopSource( CFAllocatorRef inAllocator, CFSocketRef inSock, CFIndex inOrder );

#endif // CFLITE_ENABLED

//===========================================================================================================================
//	CFFileDescriptor
//===========================================================================================================================

#if( CFLITE_ENABLED )

typedef struct CFFileDescriptor *		CFFileDescriptorRef;
typedef int								CFFileDescriptorNativeDescriptor;

#define kCFFileDescriptorReadCallBack		( 1 << 0 )
#define kCFFileDescriptorWriteCallBack		( 1 << 1 )

typedef void ( *CFFileDescriptorCallBack )( CFFileDescriptorRef inDesc, CFOptionFlags inCallBackTypes, void *inContext );

typedef struct
{
	CFIndex		version;
	void *		info;
	void *		( *retain )( void *inContext );
	void		( *release )( void *inContext );
	CFStringRef	( *copyDescription )( void *inContext );
	
}	CFFileDescriptorContext;

CFTypeID	CFFileDescriptorGetTypeID( void );

CFFileDescriptorRef
	CFFileDescriptorCreate( 
		CFAllocatorRef						inAllocator, 
		CFFileDescriptorNativeDescriptor	inFD, 
		Boolean								inCloseOnInvalidate, 
		CFFileDescriptorCallBack			inCallBack, 
		const CFFileDescriptorContext *		inContext );

CFFileDescriptorNativeDescriptor	CFFileDescriptorGetNativeDescriptor( CFFileDescriptorRef inDesc );

void	CFFileDescriptorGetContext( CFFileDescriptorRef inDesc, CFFileDescriptorContext *inContext );

void	CFFileDescriptorEnableCallBacks( CFFileDescriptorRef inDesc, CFOptionFlags inCallBackTypes );
void	CFFileDescriptorDisableCallBacks( CFFileDescriptorRef inDesc, CFOptionFlags inCallBackTypes );

void	CFFileDescriptorInvalidate( CFFileDescriptorRef inDesc );
Boolean	CFFileDescriptorIsValid( CFFileDescriptorRef inDesc );

CFRunLoopSourceRef	CFFileDescriptorCreateRunLoopSource( CFAllocatorRef inAllocator, CFFileDescriptorRef inDesc, CFIndex inOrder );

#endif // CFLITE_ENABLED

#if 0
#pragma mark == Utilities ==
#endif

//===========================================================================================================================
//	Utilities
//===========================================================================================================================

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFUnionRef
	@abstract	Union of CF object references to avoid needing to cast.
*/

typedef union
{
	void *						voidPtr;
	CFTypeRef					typeRef;
	CFPropertyListRef			plistRef;
	
	CFArrayRef					arrayRef;
	CFBooleanRef				booleanRef;
	CFDataRef					dataRef;
	CFDateRef					dateRef;
	CFDictionaryRef				dictionaryRef;
	CFNumberRef					numberRef;
	CFStringRef					stringRef;
	
	CFMutableArrayRef			mutableArrayRef;
	CFMutableDataRef			mutableDataRef;
	CFMutableDictionaryRef		mutableDictionaryRef;
	CFMutableStringRef			mutableStringRef;
	
}	CFUnionRef;

#if( CFLITE_ENABLED )
	OSStatus
		CFDateGetComponents( 
			CFDateRef	inDate, 
			int *		outYear, 
			int *		outMonth, 
			int *		outDay, 
			int *		outHour, 
			int *		outMinute, 
			int *		outSecond, 
			int *		outMicros );

	CFStringRef	CFCopyLocalizedString( CFStringRef inKey, const char *inComment );
#endif

// Debugging

OSStatus	CFCompatTest( void );

#ifdef __cplusplus
}
#endif

#if 0
#pragma mark == Post Includes ==
#endif

//===========================================================================================================================
//	Post Includes
//
//	Includes that rely on stuff earlier in this file must come after everything else.
//===========================================================================================================================

#if( CFCOMPAT_NOTIFICATIONS_ENABLED )
	#include "CFLiteNotifications.h"
#endif

#if( CFCOMPAT_PREFERENCES_ENABLED )
	#if( TARGET_OS_WINDOWS )
		#include "CFLitePreferencesWindows.h"
	#endif
#endif

#endif // __CFCompat_h__
