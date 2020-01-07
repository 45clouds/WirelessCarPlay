/*
	File:    	CFUtils.h
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
	
	Copyright (C) 2000-2014 Apple Inc. All Rights Reserved.
*/

#ifndef __CFUtils_h__
#define	__CFUtils_h__

#include <stdio.h>

#include "CommonServices.h"
#include "DebugServices.h"

#include CF_HEADER

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	#include <IOKit/IOKitLib.h>
#endif

#if( GCD_ENABLED )
	#include LIBDISPATCH_HEADER
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == Formatted Building ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPropertyListCreateFormatted
	@abstract	Creates a property list using a format string and a variable number of arguments.
	
	@param		inAllocator		Allocator to use when creating objects.
	param		inParent		Parent object to use. May be NULL if an output object is used.
	@param		outObj			Ptr to receive root object. Caller must CFRelease it. May be NULL if a parent is specified.
	@param		inFormat		Format string made up of the format specified described below.
	param		ARGS			Variable number of arguments based on the format string.
	
	Spec		Description									Parameters
	-----------	-------------------------------------------	----------
	'['			Array begin									none
	']'			Array end									none
	'{'			Dictionary begin							none
	'}'			Dictionary end								none
	
	<key>=		Inline key string							none
	%ks=		Key string (null terminated)				const char *key
	%.*ks=		Key string (variable size)					int size, const char *key
	%.nks=		Key string (n characters)					const char *key
	%kC=		Key string (FourCharCode)					uint32_t code (e.g. 'APPL').
	%kO=		Key string (CF object)						CFStringRef key
	%kU=		Key string (UUID)							uint8_t uuid[ 16 ] (big endian)
	
	%?<spec>	Use %? to conditionally suppress a value	int suppress (non-zero = suppress, 0 = don't suppress).
	<value>;	Inline value string							none
	%s			String (null terminated)					const char *string or NULL
	%.*s		String (variable size)						int size, const char *string or NULL
	%.ns		String (n characters)						const char *string or NULL
	%C			String (FourCharCode)						uint32_t code (e.g. 'APPL')
	%i			Integer										int x
	%lli		64-bit Integer								int64_t x
	%f			Floating-pointer value						double x
	%D			Data										const void *inData, int inSize
	%#D			Data from binary plist						CFPropertyListRef inObj
	%b			Boolean										int x (true/false)
	%O			Object										CFPropertyListRef or NULL
	%#O			Deep copy of object							CFPropertyListRef or NULL
	%##O		Merge dictionary into dictionary			CFDictionaryRef or NULL
	%T			Date/Time									int year, int month, int day, int hour, int minute, int second
	%.4a		String (IPv4: 1.2.3.4)						uint32_t *ipv4 (network byte order) or NULL
	%.6a		String (MAC: 00:11:22:33:44:55)				uint8_t mac[ 6 ] or NULL
	%.8a		String (Fibre: 00:11:22:33:44:55:66:77)		uint8_t addr[ 8 ] or NULL
	%.16a		String (IPv6: fe80::217:f2ff:fec8:d6e7)		uint8_t ipv6[ 16 ] or NULL
	%##a		String (IPv4, IPv6, etc. network address)	sockaddr *addr or NULL
	%T			Date/Time									int year, int month, int day, int hour, int minute, int second
	%U			UUID string									uint8_t uuid[ 16 ] (big endian) or NULL
	%@			Receive current parent						CFTypeRef *outParent
*/
CF_RETURNS_RETAINED
CFTypeRef	CFCreateF( OSStatus *outErr, const char *inFormat, ... );

CF_RETURNS_RETAINED
CFTypeRef	CFCreateV( OSStatus *outErr, const char *inFormat, va_list inArgs );

OSStatus	CFPropertyListCreateFormatted( CFAllocatorRef inAllocator, void *outObj, const char *inFormat, ... );
OSStatus	CFPropertyListCreateFormattedVAList( CFAllocatorRef inAllocator, void *outObj, const char *inFormat, va_list inArgs );
OSStatus	CFPropertyListAppendFormatted( CFAllocatorRef inAllocator, CFTypeRef inParent, const char *inFormat, ... );
OSStatus
	CFPropertyListAppendFormattedVAList( 
		CFAllocatorRef	inAllocator, 
		CFTypeRef 		inParent, 
		const char *	inFormat, 
		va_list 		inArgs );
OSStatus
	CFPropertyListBuildFormatted( 
		CFAllocatorRef	inAllocator, 
		CFTypeRef 		inParent, 
		void *			outObj, 
		const char *	inFormat, 
		va_list 		inArgs );

#if 0
#pragma mark == Formatted Extracting and Validation ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPropertyListExtractFormatted
	@abstract	Extracts an object using a format string path (e.g. "key 1.%ks.[1]").
	
	@param		inObj		Object to extract from.
	@param		outResult	Receives found object.
	@param		inFormat	Format string made up of the format specifiers specified below.
	
		<pre><code>
		
		Spec	Purpose									Parameters
		-------	---------------------------------------	----------
		<key>	Lookup inline key in dictionary			none
		%kC		Lookup FourCharCode key in dictionary	uint32_t code (e.g. 'APPL'/0x4150504C).
		%ki		Lookup string-based integer key			uint64_t integerKey
		%ks		Lookup c-string key in dictionary		const char *key
		%kt		Lookup text key in dictionary			const char *key, int keySize
		%kO		Lookup object key in dictionary			CFStringRef key
		[n]		Lookup inline index n in array			none
		[*]		Lookup variable index n in array		int index
		
		Conversion Types	Purpose								Parameter				Comment
		-------------------	-----------------------------------	-----------------------	-------
		bool				CFBoolean to Boolean				Boolean *outBool
		code				CFString to OSType					uint32_t *outCode
		data*				CFData to const void *				void **outPtr			Last va_arg is size_t expected size.
		err					CFNumber or CFString to OSStatus	OSStatus *outErr
		int					CFNumber or CFString to int			int *outValue
		int8				CFNumber or CFString to int8		int8_t *outValue
		int16				CFNumber or CFString to int16_t		int16_t *outValue
		int64				CFNumber or CFString to int64_t		int64_t *outValue
		int*				CFNumber or CFString to integer		void *outValue			Last va_arg is size_t sizeof( integer ).
		ipv4				CFString to IPv4 address			uint32_t *outIPv4		Returned in host byte order.
		mac					CFData or CFString MAC address		uint8_t outMAC[ 6 ]		outMAC must be at least 6 bytes.
		macStr				CFData or CFString MAC address		char outMACStr[ 18 ]	outMACStr must be at least 18 bytes.
		obj					Retained CF object					CFTypeRef *outObj		Caller must CFRelease on success.
		utf8				CFString to malloc'd UTF-8			char **outString		Caller must free on success.
		*utf8				CFString to fixed-size UTF-8		char *inBuffer			Last va_arg is size_t inBufferSize.
		vers				CFString to 32-bit NumVersion		uint32_t *outVersion
		svers				CFString to 32-bit Source Version	uint32_t *outVersion
		uuid				CFData or CFString to 16-bytes		uint8_t outUUID[ 16 ]	Returns a big endian UUID.
		<n>					CFData to n-bytes of data			uint8_t outData[ n ]	outData must be at least n bytes.
		
		</code></pre>
		
	param		ARGS		Variable number of arguments based on the format string.
	
	@result		An error code indicating failure reason or kNoErr (0) if successful.
*/
OSStatus	CFPropertyListExtractFormatted( CFPropertyListRef inObj, void *outResult, const char *inFormat, ... );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPropertyListExtractFormattedVAList
	@abstract	va_list version of CFPropertyListExtractFormatted. See CFPropertyListExtractFormatted for details.
*/
OSStatus	CFPropertyListExtractFormattedVAList( CFPropertyListRef inObj, void *outResult, const char *inFormat, va_list inArgs );

#if 0
#pragma mark == Serialization ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFCreateWithPlistBytes
	@abstract	Creates a CF object from the bytes of a binary/XML plist and optionally checks its type.
*/
CF_RETURNS_RETAINED
CFTypeRef	CFCreateWithPlistBytes( const void *inPtr, size_t inLen, uint32_t inFlags, CFTypeID inType, OSStatus *outErr );

#define CFDictionaryCreateWithBytes( PTR, LEN, OUT_ERR ) \
	( (CFDictionaryRef) CFCreateWithPlistBytes( (PTR), (LEN), kCFPropertyListImmutable, CFDictionaryGetTypeID(), (OUT_ERR ) ) )

#define CFDictionaryCreateMutableWithBytes( PTR, LEN, OUT_ERR ) \
	( (CFMutableDictionaryRef) CFCreateWithPlistBytes( (PTR), (LEN), kCFPropertyListMutableContainers, CFDictionaryGetTypeID(), (OUT_ERR ) ) )

#define CFArrayCreateWithBytes( PTR, LEN, OUT_ERR ) \
	( (CFArrayRef) CFCreateWithPlistBytes( (PTR), (LEN), kCFPropertyListImmutable, CFArrayGetTypeID(), (OUT_ERR ) ) )

#define CFArrayCreateMutableWithBytes( PTR, LEN, OUT_ERR ) \
	( (CFMutableArrayRef) CFCreateWithPlistBytes( (PTR), (LEN), kCFPropertyListMutableContainers, CFArrayGetTypeID(), (OUT_ERR ) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFCreateObjectFromString
	@abstract	Creates a CF object by parsing the string and trying to figure out what type of object to create.
	@discussion
	
	Here's the guidelines it follows to determine the object type:
	
		Case insensitive "true"  / "yes" / "y" / "on"  are turned into kCFBooleanTrue.
		Case insensitive "false" / "no"	 / "n" / "off" are turned into kCFBooleanFalse.
		Binary (e.g. 0b01101000), decimal (e.g. 10), hex (e.g. 0x10), or octal (e.g. 0177) strings are turned into CFNumber.
		"[]" is turned into an empty array.
		"{}" is turned into an empty dictionary.
		Anything else is treated as a simple string.
		
		Type can be --bool for booleans.
		Type can be --integer for integers.
		Type can be --hex for hex data to convert to CFData.
		Type can be --txt to convert space separated name=value pairs into a CFData DNS TXT record.
		Type can be --array for arrays.
		Type can be --dict for dictionaries.
*/
OSStatus	CFCreateObjectFromString( const char *inStr, CFTypeRef *outObj );
OSStatus	CFCreateObjectFromStringEx( const char *inStr, const char *inType, CFTypeRef *outObj );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDictionaryCreateWithINIBytes
	@abstract	Creates a dictionary with bytes of an INI-formatted file.
	
	@param		inPtr				Ptr to INI-formatted bytes.
	@param		inLen				Number of INI-formatted bytes.
	@param		inSectionNameKey	Key to store section names within a section dictionary. May be NULL to not store names.
	@param		outErr				Receives error code or noErr if successful. May be NULL.
	
	@result		Root dictionary for the INI file. Caller must be release.
*/
#define kINIFlags_None				0			// No flags.
#define kINIFlag_MergeGlobals		( 1 << 0 )	// Don't create a separate dictionary for the global section. Use the root.

// Use name.value as the section dictionary key (e.g. [name "value"] would use "name.value" as the section key).
#define kINISectionDotted		( (CFStringRef)(intptr_t) -1 )

// Use the name of the section as the key for the outer dictionary and the value as the key for the inner dictionary.
// For example, [name "value"] would have be like "name" : { "value" : {...} }.
#define kINISectionNested		( (CFStringRef)(intptr_t) -2 )

CF_RETURNS_RETAINED
CFMutableDictionaryRef
	CFDictionaryCreateWithINIBytes( 
		const void *	inPtr, 
		size_t			inLen, 
		uint32_t		inFlags, 
		CFStringRef		inSectionNameKey, 
		OSStatus *		outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPropertyListCreateBytes	
	@abstract	Converts a plist to serialized data (e.g. binary plist or XML) and returns a malloc'd pointer to it.
*/
OSStatus	CFPropertyListCreateBytes( CFPropertyListRef inPlist, CFPropertyListFormat inFormat, uint8_t **outPtr, size_t *outLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFPropertyListCreateFromFilePath	
	@abstract	Reads the specified plist file and converts it to a CF object. Must be an XML or binary plist file.
*/
OSStatus	CFPropertyListCreateFromANSIFile( FILE *inFile, CFOptionFlags inOptions, CFPropertyListRef *outPlist );

CF_RETURNS_RETAINED
CFTypeRef	CFPropertyListCreateFromFilePath( const char *inPath, CFOptionFlags inOptions, OSStatus *outErr );

OSStatus	CFPropertyListWriteToFilePath( CFPropertyListRef inPlist, const char *inFormat, const char *inPath );
OSStatus	CFPropertyListWriteToANSIFile( CFPropertyListRef inPlist, const char *inFormat, FILE *inFile );

#if 0
#pragma mark == IOKit ==
#endif

//===========================================================================================================================
//	CFIOKitObject -- CF wrapper around io_object_t to allow tracking in CF collections.
//===========================================================================================================================

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )

typedef struct CFIOKitObjectPrivate *	CFIOKitObjectRef;

CFTypeID	CFIOKitObjectGetTypeID( void );
OSStatus	CFIOKitObjectCreate( CFIOKitObjectRef *outObj, io_object_t inIOKitObject );
io_object_t	CFIOKitObjectGetIOKitObject( CFIOKitObjectRef inObj );

#endif

#if( GCD_ENABLED )
//===========================================================================================================================
//	Object Accessors
//===========================================================================================================================

#if 0
#pragma mark -
#pragma mark == Object Accessors ==
#endif

typedef uint32_t		CFObjectFlags;
#define kCFObjectStandardFlagsMask		0xFFFF		// Public flags only use these bits. Private flags can use the others.
#define kCFObjectFlagDirect				( 1 << 0 )	// Call directly when on the same thread. Don't use GCD.
#define kCFObjectFlagDelegateOnly		( 1 << 1 )	// Skip top-level processing and call the delegate directly.
#define kCFObjectFlagPlatformOnly		( 1 << 2 )	// Skip top-level and delegate processing and call the platform directly.
#define kCFObjectFlagAsync				( 1 << 3 )	// Don't wait until the function completes before returning.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		CFObjectControlAsync
	@abstract	Asynchronously performs commands on an object.
*/
typedef OSStatus
	( *CFObjectControlFunc )( 
		CFTypeRef			inObject, 
		CFObjectFlags		inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outResponse );

typedef void ( *CFObjectControlResponseFunc )( OSStatus inError, CFDictionaryRef inResponse, void *inContext );

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
		void *						inResponseContext );

OSStatus
	CFObjectControlAsyncF( 
		CFTypeRef					inObject,
		dispatch_queue_t			inQueue, 
		CFObjectControlFunc			inFunc, 
		CFObjectFlags				inFlags, 
		CFStringRef					inCommand, 
		CFTypeRef					inQualifier, 
		dispatch_queue_t			inResponseQueue, 
		CFObjectControlResponseFunc	inResponseFunc, 
		void *						inResponseContext, 
		const char *				inFormat, 
		... );

OSStatus
	CFObjectControlAsyncV( 
		CFTypeRef					inObject,
		dispatch_queue_t			inQueue, 
		CFObjectControlFunc			inFunc, 
		CFObjectFlags				inFlags, 
		CFStringRef					inCommand, 
		CFTypeRef					inQualifier, 
		dispatch_queue_t			inResponseQueue, 
		CFObjectControlResponseFunc	inResponseFunc, 
		void *						inResponseContext, 
		const char *				inFormat, 
		va_list						inArgs );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		CFObjectControlSync
	@abstract	Synchronously performs commands on an object.
*/
OSStatus
	CFObjectControlSync( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectControlFunc		inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inCommand, 
		CFTypeRef				inQualifier, 
		CFDictionaryRef			inParams, 
		CFDictionaryRef *		outResponse );

OSStatus
	CFObjectControlSyncF( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectControlFunc		inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inCommand, 
		CFTypeRef				inQualifier, 
		CFDictionaryRef *		outResponse, 
		const char *			inFormat, 
		... );

OSStatus
	CFObjectControlSyncV( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectControlFunc		inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inCommand, 
		CFTypeRef				inQualifier, 
		CFDictionaryRef *		outResponse, 
		const char *			inFormat, 
		va_list					inArgs );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		CFObjectCopyProperty
	@abstract	Copies a property synchronously from the object.
*/
typedef CFTypeRef
	( *CFObjectCopyPropertyFunc )( 
		CFTypeRef		inObject, 
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		OSStatus *		outErr );

CF_RETURNS_RETAINED
CFTypeRef
	CFObjectCopyProperty( 
		CFTypeRef					inObject,
		dispatch_queue_t			inQueue, 
		CFObjectCopyPropertyFunc	inFunc, 
		CFObjectFlags				inFlags, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr );

// Convenience accessors

#define CFObjectGetPropertyBooleanSync( inObject, inQueue, inFunc, inFlags, inProperty, inQualifier, outErr ) \
	( CFObjectGetPropertyInt64Sync( (inObject), (inQueue), (inFunc), (inFlags), (inProperty), (inQualifier), (outErr) ) ? true : false )

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
		OSStatus *					outErr );

double
	CFObjectGetPropertyDoubleSync( 
		CFTypeRef					inObject, 
		dispatch_queue_t			inQueue, 
		CFObjectCopyPropertyFunc	inFunc, 
		CFObjectFlags				inFlags, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr );

int64_t
	CFObjectGetPropertyInt64Sync( 
		CFTypeRef					inObject, 
		dispatch_queue_t			inQueue, 
		CFObjectCopyPropertyFunc	inFunc, 
		CFObjectFlags				inFlags, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		CFObjectSetProperty
	@abstract	Sets property on an object.
*/
typedef OSStatus
	( *CFObjectSetPropertyFunc )( 
		CFTypeRef		inObject, 
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		CFTypeRef		inValue );

OSStatus
	CFObjectSetProperty( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		CFTypeRef				inValue );

OSStatus
	CFObjectSetPropertyF( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		const char *			inFormat, 
		... );

OSStatus
	CFObjectSetPropertyV( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		const char *			inFormat, 
		va_list					inArgs );

// Convenience accessors

#define CFObjectSetPropertyBoolean( OBJ, QUEUE, FUNC, FLAGS, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetProperty( (OBJ), (QUEUE), (FUNC), (FLAGS), (PROPERTY), (QUALIFIER), (VALUE) ? kCFBooleanTrue : kCFBooleanFalse )

OSStatus
	CFObjectSetPropertyCString( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		const void *			inStr, 
		size_t					inLen ); // May be kSizeCString if it's a null-terminated string.

OSStatus
	CFObjectSetPropertyData( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		const void *			inData, 
		size_t					inLen );

OSStatus
	CFObjectSetPropertyDouble( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		double					inValue );

OSStatus
	CFObjectSetPropertyInt64( 
		CFTypeRef				inObject, 
		dispatch_queue_t		inQueue, 
		CFObjectSetPropertyFunc	inFunc, 
		CFObjectFlags			inFlags, 
		CFStringRef				inProperty, 
		CFTypeRef				inQualifier, 
		int64_t					inValue );

#endif // GCD_ENABLED

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		CFObject Key'd Value Accessors
	@abstract	Copies a value from the object by key and checks its type.
*/
typedef CFTypeRef ( *CFObjectCopyValue_f )( CFTypeRef inObject, CFStringRef inKey, OSStatus *outErr );
typedef OSStatus  ( *CFObjectSetValue_f )( CFTypeRef inObject, CFStringRef inKey, CFTypeRef inValue );

CF_RETURNS_RETAINED
CFTypeRef
	CFObjectCopyTypedValue( 
		CFTypeRef				inObject, 
		CFObjectCopyValue_f		inCallback, 
		CFStringRef				inKey, 
		CFTypeID				inTypeID, 
		OSStatus *				outErr );
OSStatus	CFObjectSetValue( CFTypeRef inObject, CFObjectSetValue_f inCallback, CFStringRef inKey, CFTypeRef inValue );

#define CFObjectCopyCFArray(      OBJ, CALLBACK, KEY, OUT_ERR )	( (CFArrayRef)		CFObjectCopyTypedValue( (OBJ), (CALLBACK), (KEY), CFArrayGetTypeID(),      (OUT_ERR) ) )
#define CFObjectCopyCFBoolean(    OBJ, CALLBACK, KEY, OUT_ERR )	( (CFBooleanRef)	CFObjectCopyTypedValue( (OBJ), (CALLBACK), (KEY), CFBooleanGetTypeID(),    (OUT_ERR) ) )
#define CFObjectCopyCFData(       OBJ, CALLBACK, KEY, OUT_ERR )	( (CFDataRef)		CFObjectCopyTypedValue( (OBJ), (CALLBACK), (KEY), CFDataGetTypeID(),       (OUT_ERR) ) )
#define CFObjectCopyCFDate(       OBJ, CALLBACK, KEY, OUT_ERR )	( (CFDateRef)		CFObjectCopyTypedValue( (OBJ), (CALLBACK), (KEY), CFDateGetTypeID(),       (OUT_ERR) ) )
#define CFObjectCopyCFDictionary( OBJ, CALLBACK, KEY, OUT_ERR )	( (CFDictionaryRef)	CFObjectCopyTypedValue( (OBJ), (CALLBACK), (KEY), CFDictionaryGetTypeID(), (OUT_ERR) ) )
#define CFObjectCopyCFNumber(     OBJ, CALLBACK, KEY, OUT_ERR )	( (CFNumberRef)		CFObjectCopyTypedValue( (OBJ), (CALLBACK), (KEY), CFNumberGetTypeID(),     (OUT_ERR) ) )
#define CFObjectCopyCFString(     OBJ, CALLBACK, KEY, OUT_ERR )	( (CFStringRef)		CFObjectCopyTypedValue( (OBJ), (CALLBACK), (KEY), CFStringGetTypeID(),     (OUT_ERR) ) )

#define CFObjectGetBoolean( OBJECT, CALLBACK, KEY, OUT_ERR ) \
	( CFObjectGetInt64( (OBJECT), (CALLBACK), (KEY), (OUT_ERR) ) ? true : false )
#define CFObjectSetBoolean( OBJECT, CALLBACK, KEY, VALUE ) \
		CFObjectSetValue( (OBJECT), (CALLBACK), (KEY), (VALUE) ? kCFBooleanTrue : kCFBooleanFalse )
uint8_t *
	CFObjectCopyBytes( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		size_t *			outLen, 
		OSStatus *			outErr );
uint8_t *
	CFObjectGetBytes( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		void *				inBuf, 
		size_t				inMaxLen, 
		size_t *			outLen, 
		OSStatus *			outErr );
OSStatus
	CFObjectSetBytes( 
		CFTypeRef			inObject, 
		CFObjectSetValue_f	inCallback, 
		CFStringRef			inKey, 
		const void *		inPtr, 
		size_t				inLen );
char *
	CFObjectCopyCString( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		OSStatus *			outErr );
char *
	CFObjectGetCString( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		char *				inBuf, 
		size_t				inMaxLen, 
		OSStatus *			outErr );
OSStatus
	CFObjectSetCString( 
		CFTypeRef			inObject, 
		CFObjectSetValue_f	inCallback, 
		CFStringRef			inKey, 
		const void *		inStr, 
		size_t				inLen );
double
	CFObjectGetDouble( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey,
		OSStatus *			outErr );
OSStatus	CFObjectSetDouble( CFTypeRef inObject, CFObjectSetValue_f inCallback, CFStringRef inKey, double inValue );
uint64_t
	CFObjectGetHardwareAddress( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		uint8_t *			inBuf, 
		size_t				inLen, 
		OSStatus *			outErr );
OSStatus
	CFObjectSetHardwareAddress( 
		CFTypeRef			inObject, 
		CFObjectSetValue_f	inCallback, 
		CFStringRef			inKey, 
		const void *		inAddr, 
		size_t				inLen );
int64_t
	CFObjectGetInt64( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		OSStatus *			outErr );
int64_t
	CFObjectGetInt64Ranged( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		int64_t				inMin, 
		int64_t				inMax, 
		OSStatus *			outErr );

OSStatus	CFObjectSetInt64( CFTypeRef inObject, CFObjectSetValue_f inCallback, CFStringRef inKey, int64_t inValue );
OSStatus
	CFObjectGetUUID( 
		CFTypeRef			inObject, 
		CFObjectCopyValue_f	inCallback, 
		CFStringRef			inKey, 
		const uint8_t *		inBaseUUID, 
		uint8_t				outUUID[ 16 ] );
OSStatus
	CFObjectSetUUIDString( 
		CFTypeRef				inObject, 
		CFObjectSetValue_f		inCallback, 
		CFStringRef				inKey, 
		const void *			inUUID, 
		size_t					inSize, 
		const void *			inBaseUUID, 
		uint32_t				inFlags );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Standard object accessors.
	@abstract	Simplifies adding accessors to custom objects.
	@discussion
	
	For example, if your object is named MyServer then you would add the following to your MyServer.h file:
	
	CFObjectDefineStandardAccessors( MyServerRef, MyServer, _MyServerCopyProperty, _MyServerSetPropery )
*/
#define CFObjectDefineStandardAccessors( TYPE_NAME, PREFIX, GETTER, SETTER ) \
\
CF_RETURNS_RETAINED STATIC_INLINE CFTypeRef	PREFIX##CopyValue( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectCopyTypedValue( inObj, (GETTER), inKey, 0, outErr ) ); } \
\
CF_RETURNS_RETAINED STATIC_INLINE CFTypeRef	PREFIX##CopyTypedValue( TYPE_NAME inObj, CFStringRef inKey, CFTypeID inTypeID, OSStatus *outErr ) \
{ return( CFObjectCopyTypedValue( inObj, (GETTER), inKey, inTypeID, outErr ) ); } \
\
STATIC_INLINE OSStatus	PREFIX##SetValue( TYPE_NAME inObj, CFStringRef inKey, CFTypeRef inValue ) \
{ return( CFObjectSetValue( inObj, (SETTER), inKey, inValue ) ); } \
\
CF_RETURNS_RETAINED STATIC_INLINE CFArrayRef	PREFIX##CopyCFArray( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectCopyCFArray( inObj, (GETTER), inKey, outErr ) ); } \
\
CF_RETURNS_RETAINED STATIC_INLINE CFBooleanRef	PREFIX##CopyCFBoolean( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectCopyCFBoolean( inObj, (GETTER), inKey, outErr ) ); } \
\
CF_RETURNS_RETAINED STATIC_INLINE CFDataRef	PREFIX##CopyCFData( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectCopyCFData( inObj, (GETTER), inKey, outErr ) ); } \
\
CF_RETURNS_RETAINED STATIC_INLINE CFDateRef	PREFIX##CopyCFDate( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectCopyCFDate( inObj, (GETTER), inKey, outErr ) ); } \
\
CF_RETURNS_RETAINED STATIC_INLINE CFDictionaryRef	PREFIX##CopyCFDictionary( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectCopyCFDictionary( inObj, (GETTER), inKey, outErr ) ); } \
\
CF_RETURNS_RETAINED STATIC_INLINE CFNumberRef	PREFIX##CopyCFNumber( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectCopyCFNumber( inObj, (GETTER), inKey, outErr ) ); } \
\
CF_RETURNS_RETAINED STATIC_INLINE CFStringRef	PREFIX##CopyCFString( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectCopyCFString( inObj, (GETTER), inKey, outErr ) ); } \
\
STATIC_INLINE Boolean	PREFIX##GetBoolean( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectGetBoolean( inObj, (GETTER), inKey, outErr ) ); } \
\
STATIC_INLINE OSStatus	PREFIX##SetBoolean( TYPE_NAME inObj, CFStringRef inKey, Boolean inValue ) \
{ return( CFObjectSetBoolean( inObj, (SETTER), inKey, inValue ) ); } \
\
STATIC_INLINE uint8_t *	PREFIX##CopyBytes( TYPE_NAME inObj, CFStringRef inKey, size_t *outLen, OSStatus *outErr ) \
{ return( CFObjectCopyBytes( inObj, (GETTER), inKey, outLen, outErr ) ); } \
\
STATIC_INLINE uint8_t *	PREFIX##GetBytes( TYPE_NAME inObj, CFStringRef inKey, void *inBuf, size_t inMaxLen, size_t *outLen, OSStatus *outErr ) \
{ return( CFObjectGetBytes( inObj, (GETTER), inKey, inBuf, inMaxLen, outLen, outErr ) ); } \
\
STATIC_INLINE OSStatus	PREFIX##SetBytes( TYPE_NAME inObj, CFStringRef inKey, const void *inPtr, size_t inLen ) \
{ return( CFObjectSetBytes( inObj, (SETTER), inKey, inPtr, inLen ) ); } \
\
STATIC_INLINE char *	PREFIX##CopyCString( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectCopyCString( inObj, (GETTER), inKey, outErr ) ); } \
\
STATIC_INLINE char *	PREFIX##GetCString( TYPE_NAME inObj, CFStringRef inKey, char *inBuf, size_t inMaxLen, OSStatus *outErr ) \
{ return( CFObjectGetCString( inObj, (GETTER), inKey, inBuf, inMaxLen, outErr ) ); } \
\
STATIC_INLINE OSStatus	PREFIX##SetCString( TYPE_NAME inObj, CFStringRef inKey, const void *inStr, size_t inLen ) \
{ return( CFObjectSetCString( inObj, (SETTER), inKey, inStr, inLen ) ); } \
\
STATIC_INLINE double	PREFIX##GetDouble( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectGetDouble( inObj, (GETTER), inKey, outErr ) ); } \
\
STATIC_INLINE OSStatus	PREFIX##SetDouble( TYPE_NAME inObj, CFStringRef inKey, double inValue ) \
{ return( CFObjectSetDouble( inObj, (SETTER), inKey, inValue ) ); } \
\
STATIC_INLINE uint64_t	PREFIX##GetHardwareAddress( TYPE_NAME inObj, CFStringRef inKey, uint8_t *inBuf, size_t inLen, OSStatus *outErr ) \
{ return( CFObjectGetHardwareAddress( inObj, (GETTER), inKey, inBuf, inLen, outErr ) ); } \
\
STATIC_INLINE OSStatus	PREFIX##SetHardwareAddress( TYPE_NAME inObj, CFStringRef inKey, const void *inAddr, size_t inLen ) \
{ return( CFObjectSetHardwareAddress( inObj, (SETTER), inKey, inAddr, inLen ) ); } \
STATIC_INLINE int64_t	PREFIX##GetInt64( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( CFObjectGetInt64( inObj, (GETTER), inKey, outErr ) ); } \
\
STATIC_INLINE int64_t	PREFIX##GetInt64Ranged( TYPE_NAME inObj, CFStringRef inKey, int64_t inMin, int64_t inMax, OSStatus *outErr ) \
{ return( CFObjectGetInt64Ranged( inObj, (GETTER), inKey, inMin, inMax, outErr ) ); } \
\
STATIC_INLINE int8_t	PREFIX##GetSInt8( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( (int8_t) CFObjectGetInt64Ranged( inObj, (GETTER), inKey, INT8_MIN, INT8_MAX, outErr ) ); } \
\
STATIC_INLINE uint8_t	PREFIX##GetUInt8( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( (uint8_t) CFObjectGetInt64Ranged( inObj, (GETTER), inKey, 0, UINT8_MAX, outErr ) ); } \
\
STATIC_INLINE int16_t	PREFIX##GetSInt16( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( (int16_t) CFObjectGetInt64Ranged( inObj, (GETTER), inKey, INT16_MIN, INT16_MAX, outErr ) ); } \
\
STATIC_INLINE uint16_t	PREFIX##GetUInt16( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( (uint16_t) CFObjectGetInt64Ranged( inObj, (GETTER), inKey, 0, UINT16_MAX, outErr ) ); } \
\
STATIC_INLINE int32_t	PREFIX##GetSInt32( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( (int32_t) CFObjectGetInt64Ranged( inObj, (GETTER), inKey, INT32_MIN, INT32_MAX, outErr ) ); } \
\
STATIC_INLINE uint32_t	PREFIX##GetUInt32( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( (uint32_t) CFObjectGetInt64Ranged( inObj, (GETTER), inKey, 0, UINT32_MAX, outErr ) ); } \
\
STATIC_INLINE OSStatus	PREFIX##SetInt64( TYPE_NAME inObj, CFStringRef inKey, int64_t inValue ) \
{ return( CFObjectSetInt64( inObj, (SETTER), inKey, inValue ) ); } \
\
STATIC_INLINE uint64_t	PREFIX##GetUInt64( TYPE_NAME inObj, CFStringRef inKey, OSStatus *outErr ) \
{ return( (uint64_t) CFObjectGetInt64( inObj, (GETTER), inKey, outErr ) ); } \
\
STATIC_INLINE OSStatus	PREFIX##SetUInt64( TYPE_NAME inObj, CFStringRef inKey, uint64_t inValue ) \
{ return( CFObjectSetInt64( inObj, (SETTER), inKey, (int64_t) inValue ) ); } \
\
STATIC_INLINE uint64_t	PREFIX##GetMACAddress( TYPE_NAME inObj, CFStringRef inKey, uint8_t *inBuf, OSStatus *outErr ) \
{ return( CFObjectGetHardwareAddress( inObj, (GETTER), inKey, inBuf, 6, outErr ) ); } \
\
STATIC_INLINE OSStatus	PREFIX##SetMACAddress( TYPE_NAME inObj, CFStringRef inKey, const void *inAddr ) \
{ return( CFObjectSetHardwareAddress( inObj, (SETTER), inKey, inAddr, 6 ) ); }

#if 0
#pragma mark -
#pragma mark == Boxing ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFGetBoolean
	@abstract	Gets a Boolean from a CF object, converting as necessary.
	@discussion	See conversion rules for CFGetInt64. Any non-zero value is mapped to true. 0 is mapped to false.
*/
#define CFGetBoolean( OBJ, OUT_ERR )	( ( CFGetInt64( (OBJ), (OUT_ERR ) ) != 0 ) ? true : false )
#define NSGetBoolean( OBJ, OUT_ERR )	( ( NSGetInt64( (OBJ), (OUT_ERR ) ) != 0 ) ? true : false )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFCopyCString
	@abstract	Copies a C string representation of a CF object.
*/
char *	CFCopyCString( CFTypeRef inObj, OSStatus *outErr );
#define NSCopyCString( OBJ, OUT_ERR )	CFCopyCString( (__bridge CFTypeRef)(OBJ), (OUT_ERR) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFGetCString
	@abstract	Gets a C string representation from a CF object.
*/
char *	CFGetCString( CFTypeRef inObj, char *inBuf, size_t inMaxLen );
#define NSGetCString( OBJ, BUF, MAX_LEN )	CFGetCString( (__bridge CFTypeRef)(OBJ), (BUF), (MAX_LEN) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFCopyData
	@abstract	Copies a binary data representation from a CF object. Caller must free returned bytes.
	@discussion
	
	CFData:		Returns as is.
	CFString:	Parses as hex.
	Other:		Unsupported.
*/
CF_RETURNS_RETAINED
CFDataRef	CFCopyCFData( CFTypeRef inObj, size_t *outLen, OSStatus *outErr );

uint8_t *	CFCopyData( CFTypeRef inObj, size_t *outLen, OSStatus *outErr );
#define		NSCopyBytes( OBJ, OUT_LEN, OUT_ERR )	CFCopyData( (__bridge CFTypeRef)(OBJ), (OUT_LEN), (OUT_ERR) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFGetData
	@abstract	Gets a binary data representation from a CF object.
	@discussion
	
	CFData:		Returns as is.
	CFString:	Parses as hex.
	Other:		Unsupported.
*/
uint8_t *	CFGetData( CFTypeRef inObj, void *inBuf, size_t inMaxLen, size_t *outLen, OSStatus *outErr );
#define		NSGetBytes( OBJ, BUF, MAX_LEN, OUT_LEN, OUT_ERR ) \
	CFGetData( (__bridge CFTypeRef)(OBJ), (BUF), (MAX_LEN), (OUT_LEN), (OUT_ERR) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFGetDouble
	@abstract	Gets a double from from a CF object, converting as necessary.
	@discussion
	
	This tries to guess at a reasonable integer given a CF type:
	
	CFBoolean:			true = 1, false = 0.
	CFNumber (float):	Returned as-is.
	CFNumber (integer):	CF conversion to double.
	CFString:			Case insensitive "true"  / "yes" / "y" / "on"  = 1.
						Case insensitive "false" / "no"  / "n" / "off" = 0.
						Decimal integer text, converted to integer.
						0x-prefixed, converted from hex to integer.
						0-prefixed, converted from octal to integer.
	CFData:				Converts big endian integer value to integer. Error if > 8 bytes.
*/
double	CFGetDouble( CFTypeRef inObj, OSStatus *outErr );
#define NSGetDouble( OBJ, OUT_ERR )		CFGetDouble( (__bridge CFTypeRef)(OBJ), (OUT_ERR) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFGetHardwareAddress
	@abstract	Gets a 48-bit or 64-bit hardware addrress from a CF object.
	@discussion
	
	CFNumber:	Returns 64-bit scalar as-is. Writes as big endian bytes if an output buffer is provided.
	CFString:	Parses EUI-48/MAC address (e.g. "00:11:22:33:44:55") or EUI-64 address (e.g. "00:11:22:33:44:55:66:77").
	CFData:		Converts big endian bytes to 48-bit or 64-bit scalar. Returns raw bytes if output buffer is provided.
*/
uint64_t	CFGetHardwareAddress( CFTypeRef inObj, uint8_t *inBuf, size_t inLen, OSStatus *outErr );
#define		NSGetHardwareAddress( OBJ, BUF, LEN, OUT_ERR ) \
	CFGetHardwareAddress( (__bridge CFTypeRef)(OBJ), (BUF), (LEN), (OUT_ERR) )

#define		CFGetMACAddress( OBJ, BUF, OUT_ERR )	CFGetHardwareAddress( (OBJ), (BUF), 6, (OUT_ERR) )
#define		NSGetMACAddress( OBJ, BUF, OUT_ERR )	NSGetHardwareAddress( (OBJ), (BUF), 6, (OUT_ERR) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFGetInt64
	@abstract	Gets an int64_t from a CF object, converting as necessary.
	@discussion
	
	This tries to guess at a reasonable integer given a CF type:
	
	CFBoolean:			true = 1, false = 0.
	CFNumber (float):	Saturates to 64-bit integer min/max or truncates to integer.
	CFNumber (integer):	64-bit integer.
	CFString:			Case insensitive "true"  / "yes" / "y" / "on"  = 1.
						Case insensitive "false" / "no"  / "n" / "off" = 0.
						Decimal integer text, converted to integer.
						0x-prefixed, converted from hex to integer.
						0-prefixed, converted from octal to integer.
	CFData:				Converts big endian integer value to integer. Error if > 8 bytes.
*/
int64_t	CFGetInt64( CFTypeRef inObj, OSStatus *outErr );
#define CFGetUInt64( OBJ, OUT_ERR )		( (uint64_t) CFGetInt64( (OBJ), (OUT_ERR) ) )
#define NSGetInt64( OBJ, OUT_ERR )		CFGetInt64( (__bridge CFTypeRef)(OBJ), (OUT_ERR) )
#define NSGetUInt64( OBJ, OUT_ERR )		( (uint64_t) NSGetInt64( (OBJ), (OUT_ERR) ) )

int64_t	CFGetInt64Ranged( CFTypeRef inObj, int64_t inMin, int64_t inMax, OSStatus *outErr );
#define CFGetSInt8( OBJ, OUT_ERR )		( (int8_t)   CFGetInt64Ranged( (OBJ), INT8_MIN, INT8_MAX, OUT_ERR ) )
#define CFGetUInt8( OBJ, OUT_ERR )		( (uint8_t)  CFGetInt64Ranged( (OBJ), 0, UINT8_MAX, OUT_ERR ) )
#define CFGetSInt16( OBJ, OUT_ERR )		( (int16_t)  CFGetInt64Ranged( (OBJ), INT16_MIN, INT16_MAX, OUT_ERR ) )
#define CFGetUInt16( OBJ, OUT_ERR )		( (uint16_t) CFGetInt64Ranged( (OBJ), 0, UINT16_MAX, OUT_ERR ) )
#define CFGetSInt32( OBJ, OUT_ERR )		( (int32_t)  CFGetInt64Ranged( (OBJ), INT32_MIN, INT32_MAX, OUT_ERR ) )
#define CFGetUInt32( OBJ, OUT_ERR )		( (uint32_t) CFGetInt64Ranged( (OBJ), 0, UINT32_MAX, OUT_ERR ) )

#define NSGetInt64Ranged( OBJ, MIN, MAX, OUT_ERR ) \
		CFGetInt64Ranged( (__bridge CFTypeRef)(OBJ), (MIN), (MAX), (OUT_ERR) )
#define NSGetSInt8( OBJ, OUT_ERR )		( (int8_t)   NSGetInt64Ranged( (OBJ), INT8_MIN, INT8_MAX, OUT_ERR ) )
#define NSGetUInt8( OBJ, OUT_ERR )		( (uint8_t)  NSGetInt64Ranged( (OBJ), 0, UINT8_MAX, OUT_ERR ) )
#define NSGetSInt16( OBJ, OUT_ERR )		( (int16_t)  NSGetInt64Ranged( (OBJ), INT16_MIN, INT16_MAX, OUT_ERR ) )
#define NSGetUInt16( OBJ, OUT_ERR )		( (uint16_t) NSGetInt64Ranged( (OBJ), 0, UINT16_MAX, OUT_ERR ) )
#define NSGetSInt32( OBJ, OUT_ERR )		( (int32_t)  NSGetInt64Ranged( (OBJ), INT32_MIN, INT32_MAX, OUT_ERR ) )
#define NSGetUInt32( OBJ, OUT_ERR )		( (uint32_t) NSGetInt64Ranged( (OBJ), 0, UINT32_MAX, OUT_ERR ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFCreateUUIDString
	@abstract	Creates a CFString from a UUID.
	
	@param		inUUID			Ptr to 8, 16, 32, or 128-bit UUID data.
	@param		inSize			Number of bytes in "inUUID". Must be 1, 2, 4, or 16.
	@param		inBaseUUID		Optional base UUID to use for expanding short for UUIDs to full UUIDs. May be NULL.
	@param		inFlags			Flags controlling how the UUID is processed. See kUUIDFlags_*.
	@param		outErr			Optionally receives an error code. May be NULL.
*/
CF_RETURNS_RETAINED
CFStringRef	CFCreateUUIDString( const void *inUUID, size_t inSize, const void *inBaseUUID, uint32_t inFlags, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFGetUUID
	@abstract	Gets 16-byte UUID from a CF object.
	@discussion
	
	CFData must be 16 bytes.
	CFString must be formatted as UUID string (e.g. "069e4f7b-06e9-4bde-8bb5-c902f0d1ff8e"). Case is insignificant.
*/
OSStatus	CFGetUUID( CFTypeRef inObj, uint8_t outUUID[ 16 ] );
#define		CFGetUUID( OBJ, OUT_UUID )			CFGetUUIDEx( (OBJ), NULL, (OUT_UUID) )
#define		NSGetUUID( OBJ, BASE, OUT_UUID )	CFGetUUIDEx( (__bridge CFTypeRef)(OBJ), (BASE), (OUT_UUID) )

OSStatus	CFGetUUIDEx( CFTypeRef inObj, const uint8_t *inBaseUUID, uint8_t outUUID[ 16 ] );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFSetObjectAtPath
	@abstract	Sets a CF object a path inside of a plist.
	@discussion
	
	This lets you dig into a plist using a dot-separated path of object keys or array indexes and set a value.
	
	CFSetObjectAtPath( plist, "key1.key2.[2].key4", CFSTR( "newValue" ) );
*/
OSStatus	CFSetObjectAtPath( CFTypeRef inPlist, const char *inPath, CFTypeRef inObj );

#if 0
#pragma mark -
#pragma mark == Type-specific Utilities ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFArrayApplyBlock
	@abstract	Block-based versions of the CF collection iteration functions.
*/
#if( COMPILER_HAS_BLOCKS && !COMMON_SERVICES_NO_CORE_SERVICES )
	typedef void ( ^CFArrayApplierBlock )( const void *inValue );
	void	CFArrayApplyBlock( CFArrayRef inArray, CFRange inRange, CFArrayApplierBlock inBlock );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFArrayAppendInt64
	@abstract	Appends a CFNumber from an int64_t to an array.
*/
OSStatus	CFArrayAppendInt64( CFMutableArrayRef inArray, int64_t inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFArrayAppendCString
	@abstract	Appends a CFString from a C string to an array.
	
	@param		inStr	C string to append. Doesn't need to be NUL terminated if inLen is not kSizeCString.
	@param		inLen	Number of bytes in string. May be kSizeCString if the string is NUL terminated.
*/
OSStatus	CFArrayAppendCString( CFMutableArrayRef inArray, const char *inStr, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFArrayCreateSortedByKeyPath	
	@abstract	Sorts an array into a new array using the key path to sort.
*/
CF_RETURNS_RETAINED
CFArrayRef			CFArrayCreateSortedByKeyPath( CFArrayRef inArray, const char *inKeyPath );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFArrayEnsureCreatedAndAppend
	@abstract	Appends a value to an array. If the array doesn't already exist, it is created.
*/
OSStatus	CFArrayEnsureCreatedAndAppend( CFMutableArrayRef *ioArray, CFTypeRef inValue );
OSStatus	CFArrayEnsureCreatedAndAppendCString( CFMutableArrayRef *ioArray, const void *inStr, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFArrayGetTypedValueAtIndex
	@abstract	Gets a value from an array at the specified index and makes sure it's the correct type.
*/
CF_RETURNS_NOT_RETAINED
CFTypeRef	CFArrayGetTypedValueAtIndex( CFArrayRef inArray, CFIndex inIndex, CFTypeID inType, OSStatus *outErr );

#define CFArrayGetCFArrayAtIndex( ARRAY, INDEX, OUT_ERR ) \
	( (CFArrayRef) CFArrayGetTypedValueAtIndex( (ARRAY), (INDEX), CFArrayGetTypeID(), (OUT_ERR) ) )

#define CFArrayGetCFDataAtIndex( ARRAY, INDEX, OUT_ERR ) \
	( (CFDataRef) CFArrayGetTypedValueAtIndex( (ARRAY), (INDEX), CFDataGetTypeID(), (OUT_ERR) ) )

#define CFArrayGetCFDateAtIndex( ARRAY, INDEX, OUT_ERR ) \
	( (CFDateRef) CFArrayGetTypedValueAtIndex( (ARRAY), (INDEX), CFDateGetTypeID(), (OUT_ERR) ) )

#define CFArrayGetCFDictionaryAtIndex( ARRAY, INDEX, OUT_ERR ) \
	( (CFDictionaryRef) CFArrayGetTypedValueAtIndex( (ARRAY), (INDEX), CFDictionaryGetTypeID(), (OUT_ERR) ) )

#define CFArrayGetCFNumberAtIndex( ARRAY, INDEX, OUT_ERR ) \
	( (CFNumberRef) CFArrayGetTypedValueAtIndex( (ARRAY), (INDEX), CFNumberGetTypeID(), (OUT_ERR) ) )

#define CFArrayGetCFStringAtIndex( ARRAY, INDEX, OUT_ERR ) \
	( (CFStringRef) CFArrayGetTypedValueAtIndex( (ARRAY), (INDEX), CFStringGetTypeID(), (OUT_ERR) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDataCreateSubdataWithRangeNoCopy
	@abstract	Creates a CFData from a range of bytes from another CFData.
	@discussion	Caller must ensure the original data is not modified (if it's mutable).
*/
CF_RETURNS_RETAINED
CFDataRef	CFDataCreateSubdataWithRangeNoCopy( CFDataRef inData, CFRange inRange, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDataCreateWithFilePath/CFDataCreateWithANSIFile	
	@abstract	Creates a CFData with the contents of a file. Note: file may refer to a pipe (e.g. stdin or /dev/fd/0).
*/
CF_RETURNS_RETAINED
CFDataRef	CFDataCreateWithFilePath( const char *inPath, OSStatus *outErr );

CF_RETURNS_RETAINED
CFDataRef	CFDataCreateWithANSIFile( FILE *inFile, OSStatus *outErr );

#if( !CFLITE_ENABLED )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDateCreateWithComponents	
	@abstract	Creates a CFDate from a GMT broken down date (year, month, day, hour, minute, second).
*/
CF_RETURNS_RETAINED
CFDateRef
	CFDateCreateWithComponents( 
		CFAllocatorRef	inAllocator, 
		int				inYear, 
		int				inMonth, 
		int				inDay, 
		int				inHour, 
		int				inMinute, 
		int				inSecond );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDateGetComponents	
	@abstract	Gets the GMT broken down date (year, month, day, hour, minute, second) from a CFDate.
*/
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
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDictionaryApplyBlock
	@abstract	Block-based versions of the CF collection iteration functions.
*/
#if( COMPILER_HAS_BLOCKS && !COMMON_SERVICES_NO_CORE_SERVICES )
	typedef void ( ^CFDictionaryApplierBlock )( const void *inKey, const void *inValue );
	void	CFDictionaryApplyBlock( CFDictionaryRef inSet, CFDictionaryApplierBlock inBlock );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDictionaryCopyKeys
	@abstract	Copies all the keys of a dictionary into an array.
*/
CF_RETURNS_RETAINED
CFArrayRef	CFDictionaryCopyKeys( CFDictionaryRef inDict, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDictionaryCreateFromNameTypeValueArgList
	@abstract	Creates a dictionary key/value pairs from an arg list of strings of the form "name:type:value" where type 
				must be one of the following:
				
				b (Boolean):		Value must be case insensitive "true", "false", "yes", "no", "y", "n", "1", or "0".
				i (Integer):		Value must be a decimal, hex (0x), or octal (0) integer string.
				m (MAC address):	Value must be a MAC address string (e.g. "00:11:22:33:44:55").
				s (String):			Value is any UTF-8 compatible string.
				u (UUID):			Value must be a big-endian UUID string (e.g. "069E4F7B-06E9-4BDE-8BB5-C902F0D1FF8E").
				h (Hex):			Value must be a hex-encoding string (e.g. "00112233AABB").
				{} (dictionary):	Value must be empty or a comma-separated (backslash-escaped) list of name=value pairs.
				[] (array):			Value must be empty.
				s[] (string array):	Value must be empty or a comma-separated (backslash-escaped) list of strings.
*/
OSStatus	CFDictionaryCreateFromNameTypeValueArgList( CFDictionaryRef *outDict, int inArgI, int inArgC, const char *inArgV[] );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDictionaryCreateWithCFStringArray
	@abstract	Creates a dictionary of key/boolean pairs with the key's coming from a CFArray of CFString's.
*/
OSStatus	CFDictionaryCreateWithCFStringArray( CFArrayRef inArray, CFMutableDictionaryRef *outDict );
OSStatus	CFDictionaryCreateWithFourCharCodeArray( const uint32_t inArray[], size_t inCount, CFMutableDictionaryRef *outDict );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDictionaryMergeDictionary
	@abstract	Sets all the key/value pairs of one dictionary into another dictionary (add if missing, replace if present).
*/
OSStatus	CFDictionaryMergeDictionary( CFMutableDictionaryRef inDestinationDict, CFDictionaryRef inSourceDict );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDictionaryCopyCString / CFDictionaryGetCString / CFDictionarySetCString
	@abstract	Copies, gets, or sets a CFDictionary key from/to a CFString value to/from a C string.
	@discussion	Note: inLen may be kSizeCString if it's a null-terminated string.
*/
char *		CFDictionaryCopyCString( CFDictionaryRef inDict, const void *inKey, OSStatus *outErr );
#define		NSDictionaryCopyCString( DICT, KEY, OUT_ERR ) \
			CFDictionaryCopyCString( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (OUT_ERR) )

char *		CFDictionaryGetCString( CFDictionaryRef inDict, const void *inKey, char *inBuf, size_t inMaxLen, OSStatus *outErr );
#define		NSDictionaryGetCString( DICT, KEY, BUF, MAX_LEN, OUT_ERR ) \
			CFDictionaryGetCString( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (BUF), (MAX_LEN), (OUT_ERR) )
	
OSStatus	CFDictionarySetCString( CFMutableDictionaryRef inDict, const void *inKey, const void *inStr, size_t inLen );
#define		NSDictionarySetCString( DICT, KEY, STR, LEN ) \
			CFDictionarySetCString( (__bridge CFMutableDictionaryRef)(DICT), (__bridge const void *)(KEY), (STR), (LEN) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDictionaryGetData
	@abstract	Gets binary data from a value in a dictionary. See CFGetData for conversion details.
*/
CF_RETURNS_RETAINED
CFDataRef
	CFDictionaryCopyCFData( 
		CFDictionaryRef	inDict, 
		const void *	inKey, 
		size_t *		outLen, 
		OSStatus *		outErr );
uint8_t *
	CFDictionaryCopyData( 
		CFDictionaryRef	inDict, 
		const void *	inKey, 
		size_t *		outLen, 
		OSStatus *		outErr );
#define NSDictionaryCopyBytes( DICT, KEY, LEN, OUT_ERR ) \
		CFDictionaryCopyData( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (LEN), (OUT_ERR) )

uint8_t *
	CFDictionaryGetData( 
		CFDictionaryRef	inDict, 
		const void *	inKey, 
		void *			inBuf, 
		size_t			inMaxLen, 
		size_t *		outLen, 
		OSStatus *		outErr );
#define NSDictionaryGetBytes( DICT, KEY, BUF, MAX_LEN, OUT_LEN, OUT_ERR ) \
		CFDictionaryGetData( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (BUF), (MAX_LEN), (OUT_LEN), (OUT_ERR) )

OSStatus	CFDictionarySetData( CFMutableDictionaryRef inDict, const void *inKey, const void *inData, size_t inLen );
#define		NSDictionarySetBytes( DICT, KEY, PTR, LEN ) \
			CFDictionarySetData( (__bridge CFMutableDictionaryRef)(DICT), (__bridge const void *)(KEY), (PTR), (LEN) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDictionaryGetBoolean / CFDictionaryGetDouble / CFDictionaryGetInt64 / CFDictionarySetNumber
	@abstract	Gets or sets a CFDictionary key to/from a CFNumber value to/from a boolean/double/int/float/etc value.
*/
#define		CFDictionaryGetBoolean( DICT, KEY, OUT_ERR )	( ( CFDictionaryGetInt64( (DICT), (KEY), (OUT_ERR ) ) != 0 ) ? true : false )
#define		NSDictionaryGetBoolean( DICT, KEY, OUT_ERR ) \
			CFDictionaryGetBoolean( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (OUT_ERR) )

#define		CFDictionarySetBoolean( DICT, KEY, X ) \
			CFDictionarySetValue( (DICT), (KEY), (X) ? kCFBooleanTrue : kCFBooleanFalse )
#define		NSDictionarySetBoolean( DICT, KEY, X ) \
			CFDictionarySetBoolean( (__bridge CFMutableDictionaryRef)(DICT), (__bridge const void *)(KEY), (X) )

double		CFDictionaryGetDouble( CFDictionaryRef inDict, const void *inKey, OSStatus *outErr );
#define		NSDictionaryGetDouble( DICT, KEY, OUT_ERR ) \
			CFDictionaryGetDouble( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (OUT_ERR) )

OSStatus	CFDictionarySetDouble( CFMutableDictionaryRef inDict, const void *inKey, double x );
#define		NSDictionarySetDouble( DICT, KEY, X ) \
			CFDictionarySetDouble( (__bridge CFMutableDictionaryRef)(DICT), (__bridge const void *)(KEY), (X) )

uint64_t
	CFDictionaryGetHardwareAddress( 
		CFDictionaryRef inDict, 
		const void *	inKey, 
		uint8_t *		inBuf, 
		size_t			inLen, 
		OSStatus *		outErr );
#define		NSDictionaryGetHardwareAddress( DICT, KEY, BUF, LEN, OUT_ERR ) \
			CFDictionaryGetHardwareAddress( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (BUF), (LEN), (OUT_ERR) )

#define		CFDictionaryGetMACAddress( DICT, KEY, BUF, OUT_ERR ) \
			CFDictionaryGetHardwareAddress( (DICT), (KEY), (BUF), 6, (OUT_ERR) )
#define		NSDictionaryGetMACAddress( DICT, KEY, BUF, OUT_ERR ) \
			CFDictionaryGetMACAddress( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (BUF), (OUT_ERR) )

OSStatus	CFDictionarySetHardwareAddress( CFMutableDictionaryRef inDict, const void *inKey, const void *inAddr, size_t inLen );
#define		NSDictionarySetHardwareAddress( DICT, KEY, ADDR, LEN ) \
			CFDictionarySetHardwareAddress( (__bridge CFMutableDictionaryRef)(DICT), (__bridge const void *)(KEY), (ADDR), (LEN) )

#define		CFDictionarySetMACAddress( DICT, KEY, ADDR )	CFDictionarySetHardwareAddress( (DICT), (KEY), (ADDR), 6 )
#define		NSDictionarySetMACAddress( DICT, KEY, ADDR ) \
			CFDictionarySetMACAddress( (__bridge CFMutableDictionaryRef)(DICT), (__bridge const void *)(KEY), (ADDR) )

int64_t		CFDictionaryGetInt64( CFDictionaryRef inDict, const void *inKey, OSStatus *outErr );
#define		CFDictionaryGetUInt64( DICT, KEY, OUT_ERR )		( (uint64_t) CFDictionaryGetInt64( (DICT), (KEY), (OUT_ERR) ) )

#define		NSDictionaryGetInt64( DICT, KEY, OUT_ERR ) \
			CFDictionaryGetInt64( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (OUT_ERR) )
#define		NSDictionaryGetUInt64( DICT, KEY, OUT_ERR )		( (uint64_t) NSDictionaryGetInt64( (DICT), (KEY), (OUT_ERR) ) )

int64_t		CFDictionaryGetInt64Ranged( CFDictionaryRef inDict, const void *inKey, int64_t inMin, int64_t inMax, OSStatus *outErr );
#define		CFDictionaryGetSInt8( DICT, KEY, OUT_ERR )		( (int8_t)   CFDictionaryGetInt64Ranged( (DICT), (KEY), INT8_MIN, INT8_MAX, OUT_ERR ) )
#define		CFDictionaryGetUInt8( DICT, KEY, OUT_ERR )		( (uint8_t)  CFDictionaryGetInt64Ranged( (DICT), (KEY), 0, UINT8_MAX, OUT_ERR ) )
#define		CFDictionaryGetSInt16( DICT, KEY, OUT_ERR )		( (int16_t)  CFDictionaryGetInt64Ranged( (DICT), (KEY), INT16_MIN, INT16_MAX, OUT_ERR ) )
#define		CFDictionaryGetUInt16( DICT, KEY, OUT_ERR )		( (uint16_t) CFDictionaryGetInt64Ranged( (DICT), (KEY), 0, UINT16_MAX, OUT_ERR ) )
#define		CFDictionaryGetSInt32( DICT, KEY, OUT_ERR )		( (int32_t)  CFDictionaryGetInt64Ranged( (DICT), (KEY), INT32_MIN, INT32_MAX, OUT_ERR ) )
#define		CFDictionaryGetUInt32( DICT, KEY, OUT_ERR )		( (uint32_t) CFDictionaryGetInt64Ranged( (DICT), (KEY), 0, UINT32_MAX, OUT_ERR ) )

#define		NSDictionaryGetInt64Ranged( DICT, KEY, MIN, MAX, OUT_ERR) \
			CFDictionaryGetInt64Ranged( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (MIN), (MAX), (OUT_ERR) )
#define		NSDictionaryGetSInt8( DICT, KEY, OUT_ERR )		( (int8_t)   NSDictionaryGetInt64Ranged( (DICT), (KEY), INT8_MIN, INT8_MAX, OUT_ERR ) )
#define		NSDictionaryGetUInt8( DICT, KEY, OUT_ERR )		( (uint8_t)  NSDictionaryGetInt64Ranged( (DICT), (KEY), 0, UINT8_MAX, OUT_ERR ) )
#define		NSDictionaryGetSInt16( DICT, KEY, OUT_ERR )		( (int16_t)  NSDictionaryGetInt64Ranged( (DICT), (KEY), INT16_MIN, INT16_MAX, OUT_ERR ) )
#define		NSDictionaryGetUInt16( DICT, KEY, OUT_ERR )		( (uint16_t) NSDictionaryGetInt64Ranged( (DICT), (KEY), 0, UINT16_MAX, OUT_ERR ) )
#define		NSDictionaryGetSInt32( DICT, KEY, OUT_ERR )		( (int32_t)  NSDictionaryGetInt64Ranged( (DICT), (KEY), INT32_MIN, INT32_MAX, OUT_ERR ) )
#define		NSDictionaryGetUInt32( DICT, KEY, OUT_ERR )		( (uint32_t) NSDictionaryGetInt64Ranged( (DICT), (KEY), 0, UINT32_MAX, OUT_ERR ) )

OSStatus	CFDictionarySetInt64( CFMutableDictionaryRef inDict, const void *inKey, int64_t x );
#define		CFDictionarySetUInt64( DICT, KEY, VALUE )		CFDictionarySetInt64( (DICT), (KEY), (int64_t)(VALUE) )
#define		NSDictionarySetInt64( DICT, KEY, X ) \
			CFDictionarySetInt64( (__bridge CFMutableDictionaryRef)(DICT), (__bridge const void *)(KEY), (X) )
#define		NSDictionarySetUInt64( DICT, KEY, X ) \
			CFDictionarySetUInt64( (__bridge CFMutableDictionaryRef)(DICT), (__bridge const void *)(KEY), (X) )

OSStatus	CFDictionarySetNumber( CFMutableDictionaryRef inDict, const void *inKey, CFNumberType inType, void *inValue );

#define		CFDictionaryGetUUID( DICT, KEY, OUT_UUID )	CFDictionaryGetUUIDEx( (DICT), (KEY), NULL, (OUT_UUID) )
OSStatus	CFDictionaryGetUUIDEx( CFDictionaryRef inDict, const void *inKey, const uint8_t *inBaseUUID, uint8_t outUUID[ 16 ] );
#define		NSDictionaryGetUUID( DICT, KEY, BASE, OUT_UUID ) \
			CFDictionaryGetUUIDEx( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (BASE), (OUT_UUID) )

OSStatus
	CFDictionarySetUUIDString( 
		CFMutableDictionaryRef	inDict, 
		const void *			inKey, 
		const void *			inUUID, 
		size_t					inSize, 
		const void *			inBaseUUID, 
		uint32_t				inFlags );
#define NSDictionarySetUUIDString( DICT, KEY, UUID, SIZE, BASE, FLAGS ) \
	CFDictionarySetUUIDString( (__bridge CFMutableDictionaryRef)(DICT), (__bridge const void *)(KEY), (UUID), (SIZE), (BASE), (FLAGS) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDictionaryGetNestedValue
	@abstract	Gets a value from a nested set of CFDictionary objects.
*/
CF_RETURNS_NOT_RETAINED
const void *	CFDictionaryGetNestedValue( CFDictionaryRef inDict, CFStringRef inKey, ... );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFDictionaryGetTypedValue (and variants)
	@abstract	Gets a dictionary value of a specific type. Returns NULL and optionally an error if it's missing or mistyped.
*/
CF_RETURNS_NOT_RETAINED
CFTypeRef	CFDictionaryGetTypedValue( CFDictionaryRef inDict, const void *inKey, CFTypeID inType, OSStatus *outErr );
#define		CFDictionaryGetCFArray( DICT, KEY, OUT_ERR )		( (CFArrayRef)		CFDictionaryGetTypedValue( (DICT), (KEY), CFArrayGetTypeID(),		(OUT_ERR ) ) )
#define		CFDictionaryGetCFBoolean( DICT, KEY, OUT_ERR )		( (CFBooleanRef)	CFDictionaryGetTypedValue( (DICT), (KEY), CFBooleanGetTypeID(),		(OUT_ERR ) ) )
#define		CFDictionaryGetCFData( DICT, KEY, OUT_ERR )			( (CFDataRef)		CFDictionaryGetTypedValue( (DICT), (KEY), CFDataGetTypeID(),		(OUT_ERR ) ) )
#define		CFDictionaryGetCFDate( DICT, KEY, OUT_ERR )			( (CFDateRef)		CFDictionaryGetTypedValue( (DICT), (KEY), CFDateGetTypeID(),		(OUT_ERR ) ) )
#define		CFDictionaryGetCFDictionary( DICT, KEY, OUT_ERR )	( (CFDictionaryRef)	CFDictionaryGetTypedValue( (DICT), (KEY), CFDictionaryGetTypeID(),	(OUT_ERR ) ) )
#define		CFDictionaryGetCFNumber( DICT, KEY, OUT_ERR )		( (CFNumberRef)		CFDictionaryGetTypedValue( (DICT), (KEY), CFNumberGetTypeID(),		(OUT_ERR ) ) )
#define		CFDictionaryGetCFString( DICT, KEY, OUT_ERR )		( (CFStringRef)		CFDictionaryGetTypedValue( (DICT), (KEY), CFStringGetTypeID(),		(OUT_ERR ) ) )

#define		NSDictionaryGetNSArray( DICT, KEY, OUT_ERR ) \
			( (__bridge NSArray *) CFDictionaryGetCFArray( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (OUT_ERR) ) )
#define		NSDictionaryGetNSData( DICT, KEY, OUT_ERR ) \
			( (__bridge NSData *) CFDictionaryGetCFData( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (OUT_ERR) ) )
#define		NSDictionaryGetNSDate( DICT, KEY, OUT_ERR ) \
			( (__bridge NSDate *) CFDictionaryGetCFDate( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (OUT_ERR) ) )
#define		NSDictionaryGetNSDictionary( DICT, KEY, OUT_ERR ) \
			( (__bridge NSDictionary *) CFDictionaryGetCFDictionary( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (OUT_ERR) ) )
#define		NSDictionaryGetNSNumber( DICT, KEY, OUT_ERR ) \
			( (__bridge NSNumber *) CFDictionaryGetCFNumber( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (OUT_ERR) ) )
#define		NSDictionaryGetNSString( DICT, KEY, OUT_ERR ) \
			( (__bridge NSString *) CFDictionaryGetCFString( (__bridge CFDictionaryRef)(DICT), (__bridge const void *)(KEY), (OUT_ERR) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFNumberCreateInt64
	@abstract	Creates a CFNumber from an int64_t that tries to use the smallest type possible.
*/
CF_RETURNS_RETAINED
CFNumberRef	CFNumberCreateInt64( int64_t x );

CF_RETURNS_RETAINED
STATIC_INLINE CFNumberRef	CFNumberCreateUInt64( uint64_t x )
{
	return( CFNumberCreateInt64( (int64_t) x ) );
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFNumberGetObject
	@abstract	Gets a CFNumber object for the specified value.
*/
#define kCFNumberGetObjectMinValue		0  //! Smallest value supported by CFNumberGetObject.
#define kCFNumberGetObjectMaxValue		31 //! Largest value supported by CFNumberGetObject.

CFNumberRef	CFNumberGetObject( uint32_t inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFSetApplyBlock
	@abstract	Block-based versions of the CF collection iteration functions.
*/	
#if( COMPILER_HAS_BLOCKS && !COMMON_SERVICES_NO_CORE_SERVICES )
	typedef void ( ^CFSetApplierBlock )( const void *inValue );
	void	CFSetApplyBlock( CFSetRef inSet, CFSetApplierBlock inBlock );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFStringCreateComponentsSeparatedByString
	@abstract	Returns an array containing substrings from the receiver that have been divided by a given separator.
	
	@param		inString		String to separate.
	@param		inSeparator		Separator string.
	
	@discussion
	
	This is a CFString equivalent of NSString's componentsSeparatedByString.
*/
CF_RETURNS_RETAINED
CFArrayRef	CFStringCreateComponentsSeparatedByString( CFStringRef inString, CFStringRef inSeparator );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFStringCreateF / CFStringCreateV
	@abstract	Creates a CFString using a format string and variable argument list.
	@discussion	This supports the format specifiers from PrintFUtils.h.
*/
CFStringRef	CFStringCreateF( OSStatus *outErr, const char *inFormat, ... );
#define		NSStringCreateF( OUT_ERR, ... ) \
			( (NSString *) CFBridgingTransfer( CFStringCreateF( (OUT_ERR), __VA_ARGS__ ) ) )

CFStringRef	CFStringCreateV( OSStatus *outErr, const char *inFormat, va_list inArgs );
#define		NSStringCreateV( OUT_ERR, FORMAT, ARGS ) \
			( (NSString *) CFBridgingTransfer( CFStringCreateV( (OUT_ERR), (FORMAT), (ARGS) ) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFStringAppendF / CFStringAppendV
	@abstract	Appends to CFString using a format string and variable argument list.
	@discussion	This supports the format specifiers from PrintFUtils.h.
*/
OSStatus	CFStringAppendF( CFMutableStringRef inStr, const char *inFormat, ... );
#define		NSStringAppendF( STR, ... ) \
			CFStringAppendF( (__bridge CFMutableStringRef)(STR), __VA_ARGS__ )

OSStatus	CFStringAppendV( CFMutableStringRef inStr, const char *inFormat, va_list inArgs );
#define		NSStringAppendV( STR, FORMAT, ARGS ) \
			CFStringAppendF( (__bridge CFMutableStringRef)(STR), (FORMAT), (ARGS) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFStringCopyUTF8CString
	@abstract	Converts a CFString to a malloc'd UTF-8 string. Caller must free UTF-8 string on success.
*/
OSStatus	CFStringCopyUTF8CString( CFStringRef inString, char **outUTF8 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFStringGetOrCopyCStringUTF8
	@abstract	Gets a pointer to a UTF-8 C-string directly if it can and if it can't, it copies into a malloc'd C-string.
	
	@param		inString	String to get a C-string from.
	@param		outUTF8		Receives a ptr to a C-string. WARNING: This should not be free'd.
	@param		outStorage	Receives a ptr to malloc'd buffer, if needed. Caller must free this ptr on success if non-NULL.
	
	@discussion
	
	WARNING: The resulting C-string is only valid as long as the input CFString is valid. If you release the CFString, 
	the C-string may go away with it. If you need to keep the string around longer, use CFStringCopyUTF8CString.
	
	This function tries to be efficient as possible by directly returning a pointer to the underlying storage if the 
	string is stored underneath as UTF-8 (CFLite). If not, it has to malloc a buffer, convert to UTF-8 into a malloc'd 
	buffer, and return that buffer, which the caller must free. Here's an example of using it:
	
		OSStatus			err;
		const char *		utf8;
		char *				utf8Storage;
		
		utf8Storage = NULL;
		err = CFStringGetOrCopyCStringUTF8( CFSTR( "test string" ), &utf8, &utf8Storage );
		require_noerr( err, exit );
		
		... use utf8
	
	exit:
		if( utf8Storage ) free( utf8Storage );
*/
OSStatus	CFStringGetOrCopyCStringUTF8( CFStringRef inString, const char **outUTF8, char **outStorage, size_t *outLen );

#if 0
#pragma mark -
#pragma mark == Misc ==
#endif

CFComparisonResult	CFSortCompareKeyPath( const void *inLeft, const void *inRight, void *inContext );
CFComparisonResult	CFSortLocalizedStandardCompare( const void *inLeft, const void *inRight, void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MapCFStringToValue
	@abstract	Maps CFString to a value using a variable argument list of string/integer pairs, terminated by a single NULL.
	@discussion
	
	Example usage:
	int	i;
	
	i = MapCFStringToValue( CFSTR( "blue" ), -1, 
		CFSTR( "red" ),   1, 
		CFSTR( "green" ), 2, 
		CFSTR( "blue" ),  3, 
		NULL );
	Result: i == 3
*/
int	MapCFStringToValue( CFStringRef inString, int inDefaultValue, ... );
CF_RETURNS_NOT_RETAINED
CFStringRef	MapValueToCFString( int inValue, CFStringRef inDefaultStr, ... );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	StringToRangeArray/RangeArrayToString
	@abstract	Convert number list string (e.g. "1-5" or "1,2,3-7" or "3,2,1") <-> CFArray of begin/end number CFDictionary's.
	
	@discussion
	
	Commas or spaces may separate each number or number range. Ranges are specified using x-y. The result is a CFArray
	of CFDictionary's with each CFDictionary containing "begin" and "end" CFNumber values. For example, the string:
	
		"1, 2, 3-7"
	
	would turn into the following object (using the CFUtil plist format syntax):
	
		[
			{
				begin=1
				end=1
			}
			{
				begin=2
				end=2
			}
			{
				begin=3
				end=7
			}
		]
	
	Validating two range array strings checks that the two have the same general structure and begin/end deltas, but may 
	have different actual values. For example, if you wanted to map TCP ports "1000-2000, 3003" to "5000-6000, 7003", 
	those strings would be compatible, but if you tried to map "1000-2000, 3003" to "5000, 7003", they wouldn't be.
	
	Conflicting range arrays check for any overlapping values between the two. For example, you wouldn't want to have two 
	port mappings that overlapped. The first of the two functions compares strings and the second one compares a string to 
	an existing range array which might occurn if someone is entering new data and wants to compare against existing.
*/
OSStatus	StringToRangeArray( const char *inStr, CFArrayRef *outArray );
OSStatus	RangeArrayToString( CFArrayRef inArray, CFStringRef *outString );
OSStatus	ValidateRangeArrayStrings( const char *inStr1, const char *inStr2 );
OSStatus	ConflictingRangeArrayStrings( const char *inStr1, const char *inStr2 );
OSStatus	ConflictingRangeArrayStringAndRangeArray( const char *inStr, CFArrayRef inArray );

#if 0
#pragma mark == Debugging ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFUtilsTest
	@abstract	Unit test.
*/
OSStatus	CFUtilsTest( int inPrint );

#ifdef __cplusplus
}
#endif

#endif // __CFUtils_h__
