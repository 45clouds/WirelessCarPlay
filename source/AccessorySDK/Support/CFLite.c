/*
	File:    	CFLite.c
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
	
	Core Foundation Lite -- A lightweight and portable implementation of the Apple Core Foundation-like APIs.
*/

#if 0
#pragma mark == Includes ==
#endif

#include <CoreUtils/CFLite.h>

#include "AtomicUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <limits.h>
	#include <math.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
#endif

#if( COMPILER_VISUAL_CPP )
	#pragma warning( disable:4311 ) // Disable "type cast' : pointer truncation from ABC to XYZ" for CFLHashCode casts.
#endif

#if 0
#pragma mark == Constants ==
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define	kCFLSignatureValid		'V'
#define	kCFLSignatureFree		'f'

#define	kCFLTypeInvalid			0
#define	kCFLTypeArray			1
#define	kCFLTypeBoolean			2
#define	kCFLTypeData			3
#define	kCFLTypeDate			4
#define	kCFLTypeDictionary		5
#define	kCFLTypeNumber			6
#define	kCFLTypeString			7 // WARNING: This must always be 7 because it is hard-coded into constant objects.
#define	kCFLTypeNull			8

#define	kCFLTypeMin				1

#define kCFLConstantFlag		( 1 << 0 )
#define	kCFLConstantRefCount	0x7FFFFFFF

#if 0
#pragma mark == Structures ==
#endif

//===========================================================================================================================
//	Types
//===========================================================================================================================

// CFLArray

typedef struct CFLArray		CFLArray;
struct CFLArray
{
	CFLObject				base;
	CFLArrayCallBacks		callbacks;
	CFLIndex				count;
	void **					storage;
};

// CFLBoolean

typedef struct CFLBoolean	CFLBoolean;
struct CFLBoolean
{
	CFLObject		base;
};

// CFLData

typedef struct CFLData		CFLData;
struct CFLData
{
	CFLObject			base;
	uint8_t *			data;
	size_t				usedSize;
	size_t				reservedSize;
	CFLAllocatorRef		bytesDeallocator;
	CFLDataRef			parentData;
};

// CFLDate

typedef struct CFLDate		CFLDate;
struct CFLDate
{
	CFLObject				base;
	CFLDateComponents		date;
};

// CFLDictionaryNode

typedef struct CFLDictionaryNode	CFLDictionaryNode;
struct CFLDictionaryNode
{
	CFLDictionaryNode *		next;
	const void *			key;
	const void *			value;
};

// CFLDictionary

typedef struct CFLDictionary		CFLDictionary;
struct CFLDictionary
{
	CFLObject						base;
	CFLDictionaryKeyCallBacks		keyCallBacks;
	CFLDictionaryValueCallBacks		valueCallBacks;
	CFLIndex						count;
	CFLIndex						bucketCount;
	CFLDictionaryNode **			buckets;
};

// CFLNull

typedef struct CFLNull		CFLNull;
struct CFLNull
{
	CFLObject		base;
};

// CFLNumber

typedef struct CFLNumber	CFLNumber;
struct CFLNumber
{
	CFLObject			base;
	CFLNumberType		type;
	union
	{
		int64_t			s64;
		uint64_t		u64;
		int128_compat	s128;
		#if( CFL_FLOATING_POINT_NUMBERS )
		Float64			f64;
		#endif
		
	}	value;
};

// CFLString
//
// WARNING: This cannot change because it is hard-coded into constant objects.

typedef struct CFLString	CFLString;
struct CFLString
{
	CFLObject		base;
	char *			data;
	size_t			size;
};

#if 0
#pragma mark == Macros ==
#endif

//===========================================================================================================================
//	Macros
//===========================================================================================================================

#define CFLObjectGetSignature( OBJ )		( ( (const uint8_t *)( OBJ ) )[ 0 ] )
#define CFLObjectGetType( OBJ )				( ( (const uint8_t *)( OBJ ) )[ 1 ] )
#define CFLObjectGetFlags( OBJ )			( ( (const uint8_t *)( OBJ ) )[ 2 ] )

#define	CFLValidSignature( OBJ )			( CFLObjectGetSignature( OBJ ) == kCFLSignatureValid )
#define	CFLValidType( OBJ )					( ( CFLObjectGetType( OBJ ) >= kCFLTypeMin ) && \
											  ( CFLObjectGetType( OBJ ) < gCFLRuntimeClassTableCount ) )
#define	CFLEqualType( OBJ, TYPE )			( CFLObjectGetType( OBJ ) == ( TYPE ) )
#define	CFLValidObject( OBJ )				( ( OBJ ) && CFLValidSignature( OBJ ) && CFLValidType( OBJ ) )
#define	CFLValidObjectType( OBJ, TYPE )		( ( OBJ ) && CFLValidSignature( OBJ ) && CFLEqualType( OBJ, TYPE ) )
#define	CFLDataRoundUpSize( X )		\
	( ( ( X ) <  256U ) ?  256U :		\
	  ( ( X ) < 4096U ) ? 4096U :		\
	  AlignUp( ( X ),    32768U ) )

#define	CFLIsConstantObject( OBJ )			( CFLObjectGetFlags( OBJ ) & kCFLConstantFlag )
#define	CFLGetConstantStringPtr( OBJ )		( (const char *)( ( (const uint8_t *)( OBJ ) ) + kCFLStringConstantHeaderSize ) )

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

// General

static const void *	_CFLContainerRetain( CFLAllocatorRef inAllocator, const void *inObject );
static void			_CFLContainerRelease( CFLAllocatorRef inAllocator, const void *inObject );
static Boolean		_CFLContainerEqual( const void *inLeft, const void *inRight );
static CFLHashCode	_CFLContainerHash( const void *inObject );

// Array

static Boolean		_CFLArrayEqual( CFLObjectRef inLeft, CFLObjectRef inRight );
static CFLHashCode	_CFLArrayHash( CFLObjectRef inObject );
static void			_CFLArrayFree( CFLObjectRef inObject );
static OSStatus		_CFLArrayCopy( CFLArrayRef inSrc, CFLArrayRef *outDst );

// Data

static Boolean		_CFLDataEqual( CFLObjectRef inLeft, CFLObjectRef inRight );
static CFLHashCode	_CFLDataHash( CFLObjectRef inObject );
static void			_CFLDataFree( CFLObjectRef inObject );
static OSStatus		_CFLDataCopy( CFLDataRef inSrc, CFLDataRef *outDst );

// Date

static Boolean		_CFLDateEqual( CFLObjectRef inLeft, CFLObjectRef inRight );
static CFLHashCode	_CFLDateHash( CFLObjectRef inObject );
static void			_CFLDateFree( CFLObjectRef inObject );
static OSStatus		_CFLDateCopy( CFLDateRef inSrc, CFLDateRef *outDst );

// Dictionary

static Boolean		_CFLDictionaryEqual( CFLObjectRef inLeft, CFLObjectRef inRight );
static CFLHashCode	_CFLDictionaryHash( CFLObjectRef inObject );
static void			_CFLDictionaryFree( CFLObjectRef inObject );
static OSStatus		_CFLDictionaryCopy( CFLDictionaryRef inSrc, CFLDictionaryRef *outDst );
static OSStatus		_CFLDictionaryFindKey( const CFLDictionary *inObject, const void *inKey, CFLDictionaryNode ***outNode );

// Number

static Boolean		_CFLNumberEqual( CFLObjectRef inLeft, CFLObjectRef inRight );
static CFLHashCode	_CFLNumberHash( CFLObjectRef inObject );
static void			_CFLNumberFree( CFLObjectRef inObject );
static OSStatus		_CFLNumberCopy( CFLNumberRef inSrc, CFLNumberRef *outDst );

// String

static Boolean		_CFLStringEqual( CFLObjectRef inLeft, CFLObjectRef inRight );
static CFLHashCode	_CFLStringHash( CFLObjectRef inObject );
static void			_CFLStringFree( CFLObjectRef inObject );
static OSStatus		_CFLStringCopy( CFLStringRef inSrc, CFLStringRef *outDst );

// Utilities

static CFLHashCode	_CFLHashFNV1a( const void *inPtr, size_t inLen );
static CFLIndex		_CFLNextPrime( CFLIndex inValue );
static void			_CFLQSortPtrs( void *inPtrArray, size_t inPtrCount, CFLComparatorFunction inCmp, void *inContext );
#if( TARGET_NO_REALLOC )
	static void *	_CFLrealloc( void *inMem, size_t inOldSize, size_t inNewSize );
#endif
static size_t		_CFLstrnlen( const char *inString, size_t inMax );

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static const CFLRuntimeClass		kCFLRuntimeClassTable[] = 
{
	// Name				Free CallBack			Equal CallBack			Hash CallBack
	// ================	=======================	=======================	=============
	{ "<invalid>", 		NULL, 					NULL,					NULL }, 
	{ "CFLArray", 		_CFLArrayFree, 			_CFLArrayEqual,			_CFLArrayHash }, 
	{ "CFLBoolean", 	NULL, 					NULL,					NULL }, 
	{ "CFLData", 		_CFLDataFree, 			_CFLDataEqual,			_CFLDataHash }, 
	{ "CFLDate", 		_CFLDateFree, 			_CFLDateEqual,			_CFLDateHash }, 
	{ "CFLDictionary", 	_CFLDictionaryFree, 	_CFLDictionaryEqual,	_CFLDictionaryHash }, 
	{ "CFLNumber", 		_CFLNumberFree, 		_CFLNumberEqual, 		_CFLNumberHash }, 
	{ "CFLString", 		_CFLStringFree,			_CFLStringEqual, 		_CFLStringHash }, 
	{ "CFLNull",		NULL, 					NULL,					NULL }, 
};

static atomic_spinlock_t			gCFLRuntimeClassSpinLock		= 0;
static const CFLRuntimeClass *		gCFLRuntimeClassTable			= kCFLRuntimeClassTable;
static CFLRuntimeClass *			gCFLRuntimeClassTableStorage	= NULL;
static size_t						gCFLRuntimeClassTableCount		= countof( kCFLRuntimeClassTable );

//
// Array
//

const CFLArrayCallBacks			kCFLArrayCallBacksCFLTypes =
{
	0, 
	_CFLContainerRetain, 
	_CFLContainerRelease, 
	0, 
	_CFLContainerEqual
};

const CFLArrayCallBacks			kCFLArrayCallBacksNull = { 0, 0, 0, 0, 0 };

//
// Boolean
//

static CFLBoolean		_kCFLBooleanTrue 	= { { kCFLSignatureValid, kCFLTypeBoolean, kCFLConstantFlag, 0, kCFLConstantRefCount } };
static CFLBoolean		_kCFLBooleanFalse 	= { { kCFLSignatureValid, kCFLTypeBoolean, kCFLConstantFlag, 0, kCFLConstantRefCount } };
CFLBooleanRef			kCFLBooleanTrue		= &_kCFLBooleanTrue;
CFLBooleanRef			kCFLBooleanFalse	= &_kCFLBooleanFalse;

//
// Dictionary
//

const CFLDictionaryKeyCallBacks			kCFLDictionaryKeyCallBacksCFLTypes =
{
	0, 
	_CFLContainerRetain, 
	_CFLContainerRelease, 
	0, 
	_CFLContainerEqual, 
	_CFLContainerHash
};

const CFLDictionaryValueCallBacks		kCFLDictionaryValueCallBacksCFLTypes = 
{
	0, 
	_CFLContainerRetain, 
	_CFLContainerRelease, 
	0, 
	_CFLContainerEqual
};

const CFLDictionaryKeyCallBacks			kCFLDictionaryKeyCallBacksNull 		= { 0, 0, 0, 0, 0, 0 };
const CFLDictionaryValueCallBacks		kCFLDictionaryValueCallBacksNull 	= { 0, 0, 0, 0, 0 };

//
// Null
//

static CFLNull		_kCFLNull 	= { { kCFLSignatureValid, kCFLTypeNull, kCFLConstantFlag, 0, kCFLConstantRefCount } };
CFLNullRef			kCFLNull	= &_kCFLNull;

#if 0
#pragma mark -
#pragma mark == General ==
#endif

//===========================================================================================================================
//	CFLGetTypeID
//===========================================================================================================================

OSStatus	CFLGetTypeID( CFLObjectRef inObject, CFLTypeID *outTypeID )
{
	OSStatus		err;
	
	require_action( CFLValidObject( inObject ), exit, err = kBadReferenceErr );
	
	if( outTypeID ) *outTypeID = CFLObjectGetType( inObject );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLGetRetainCount
//===========================================================================================================================

OSStatus	CFLGetRetainCount( CFLObjectRef inObject, CFLIndex *outCount )
{
	OSStatus		err;
	CFLObject *		obj;
	
	require_action( CFLValidObject( inObject ), exit, err = kBadReferenceErr );
	obj = (CFLObject *) inObject;
	
	if( outCount ) *outCount = CFLIsConstantObject( inObject ) ? kCFLConstantRefCount : obj->retainCount;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLRetain
//===========================================================================================================================

CFLObjectRef	CFLRetain( CFLObjectRef inObject )
{
	OSStatus		err;
	CFLObject *		obj;
	
	require_action( CFLValidObject( inObject ), exit, err = kBadReferenceErr );
	obj = (CFLObject *) inObject;
	
	if( !CFLIsConstantObject( obj ) )
	{
		atomic_add_32( &obj->retainCount, 1 );
	}
	err = kNoErr;
	
exit:
	return( ( err == kNoErr ) ? inObject : NULL );
}

//===========================================================================================================================
//	CFLRelease
//===========================================================================================================================

void	CFLRelease( CFLObjectRef inObject )
{
	CFLObject *		obj;
	
	require( CFLValidObject( inObject ), exit );
	obj = (CFLObject *) inObject;
	
	// Decrement the reference count and if it goes to zero, free the memory used by the object.
	
	if( !CFLIsConstantObject( obj ) )
	{
		if( atomic_add_and_fetch_32( &obj->retainCount, -1 ) == 0 )
		{
			// Use the type-specific free callback to free the object.
			
			if( gCFLRuntimeClassTable[ obj->type ].freeObj ) gCFLRuntimeClassTable[ obj->type ].freeObj( obj );
			
			// Mark the object as invalid to help detect accident re-use then free the memory used by the object.
			
			obj->signature	= kCFLSignatureFree;
			obj->type		= kCFLTypeInvalid;
			free( obj );
		}
	}

exit:
	return;
}

//===========================================================================================================================
//	CFLEqual
//===========================================================================================================================

Boolean	CFLEqual( CFLObjectRef inLeft, CFLObjectRef inRight )
{
	OSStatus		err;
	CFLObject *		left;
	CFLObject *		right;
	CFLTypeID		type;
	
	require_action( CFLValidObject( inLeft ), exit, err = kBadReferenceErr );
	require_action( CFLValidObject( inRight ), exit, err = kBadReferenceErr );
	left  = (CFLObject *) inLeft;
	right = (CFLObject *) inRight;
	
	// Equal ptrs means equal objects.
	
	if( left == right )
	{
		err = kNoErr;
		goto exit;
	}
	
	// Different types means different objects.
	
	type = CFLObjectGetType( left );
	if( type != CFLObjectGetType( right ) )
	{
		err = kMismatchErr;
		goto exit;
	}
	
	// Use the type-specific equal callback to determine if they are equal.
	
	if( gCFLRuntimeClassTable[ type ].equal )
	{
		if( !gCFLRuntimeClassTable[ type ].equal( left, right ) )
		{
			err = kMismatchErr;
			goto exit;
		}
	}
	else
	{
		err = kMismatchErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( (Boolean)( err == kNoErr ) );
}

//===========================================================================================================================
//	CFLHash
//===========================================================================================================================

CFLHashCode	CFLHash( CFLObjectRef inObject )
{
	CFLHashCode		hash;
	CFLObject *		obj;
	
	hash = 0;
	require( CFLValidObject( inObject ), exit );
	obj = (CFLObject *) inObject;
	
	// If there's a type-specific hash callback, use that. Otherwise, use the pointer as the hash.
	
	if( gCFLRuntimeClassTable[ CFLObjectGetType( obj ) ].hash )
	{
		hash = gCFLRuntimeClassTable[ CFLObjectGetType( obj ) ].hash( obj );
	}
	else
	{
		hash = (CFLHashCode)(uintptr_t) obj;
	}

exit:
	return( hash );
}

//===========================================================================================================================
//	CFLCopy
//===========================================================================================================================

OSStatus	CFLCopy( CFLObjectRef inSrc, CFLObjectRef *outDst )
{
	OSStatus		err;
	CFLTypeID		typeID;
	
	require_action( outDst, exit, err = kParamErr );
	
	err = CFLGetTypeID( inSrc, &typeID );
	require_noerr( err, exit );
	
	if( typeID == CFLArrayGetTypeID() )				// Array
	{
		err = _CFLArrayCopy( (CFLArrayRef) inSrc, (CFLArrayRef *) outDst );
		require_noerr( err, exit );
	}
	else if( typeID == CFLBooleanGetTypeID() )		// Boolean
	{
		// Boolean's are constant objects and don't need to be copied.
		
		*outDst = inSrc;
	}
	else if( typeID == CFLDataGetTypeID() )			// Data
	{
		err = _CFLDataCopy( (CFLDataRef) inSrc, (CFLDataRef *) outDst );
		require_noerr( err, exit );
	}
	else if( typeID == CFLDateGetTypeID() )			// Date
	{
		err = _CFLDateCopy( (CFLDateRef) inSrc, (CFLDateRef *) outDst );
		require_noerr( err, exit );
	}
	else if( typeID == CFLDictionaryGetTypeID() )	// Dictionary
	{
		err = _CFLDictionaryCopy( (CFLDictionaryRef) inSrc, (CFLDictionaryRef *) outDst );
		require_noerr( err, exit );
	}
	else if( typeID == CFLNullGetTypeID() )			// Null
	{
		// Null's are constant objects and don't need to be copied.
		
		*outDst = inSrc;
	}
	else if( typeID == CFLNumberGetTypeID() )		// Number
	{
		err = _CFLNumberCopy( (CFLNumberRef) inSrc, (CFLNumberRef *) outDst );
		require_noerr( err, exit );
	}
	else if( typeID == CFLStringGetTypeID() )		// String
	{
		err = _CFLStringCopy( (CFLStringRef) inSrc, (CFLStringRef *) outDst );
		require_noerr( err, exit );
	}
	else
	{
		err = kTypeErr;
		dlogassert( "Unknown typeID %u", typeID );
		goto exit;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CFLContainerRetain
//===========================================================================================================================

static const void *	_CFLContainerRetain( CFLAllocatorRef inAllocator, const void *inObject )
{
	(void) inAllocator; // Unused
	
	return( CFLRetain( inObject ) );
}

//===========================================================================================================================
//	_CFLContainerRelease
//===========================================================================================================================

static void	_CFLContainerRelease( CFLAllocatorRef inAllocator, const void *inObject )
{
	(void) inAllocator; // Unused
	
	CFLRelease( inObject );
}

//===========================================================================================================================
//	_CFLContainerEqual
//===========================================================================================================================

static Boolean	_CFLContainerEqual( const void *inLeft, const void *inRight )
{
	return( CFLEqual( inLeft, inRight ) );
}

//===========================================================================================================================
//	_CFLContainerHash
//===========================================================================================================================

static CFLHashCode		_CFLContainerHash( const void *inObject )
{
	return( CFLHash( inObject ) );
}

#if 0
#pragma mark -
#pragma mark == Array ==
#endif

//===========================================================================================================================
//	CFLArrayGetTypeID
//===========================================================================================================================

CFLTypeID	CFLArrayGetTypeID( void )
{
	return( kCFLTypeArray );
}

//===========================================================================================================================
//	CFLArrayCreate
//===========================================================================================================================

OSStatus	CFLArrayCreate( CFLAllocatorRef inAllocator, const CFLArrayCallBacks *inCallBacks, CFLArrayRef *outRef )
{
	OSStatus		err;
	CFLArray *		object;
	
	object = NULL;
	require_action( inAllocator == kCFLAllocatorDefault, exit, err = kParamErr );
	require_action( outRef, exit, err = kParamErr );
	
	// Allocate and initialize the array.
	
	object = (CFLArray *) calloc( 1U, sizeof( CFLArray ) );
	require_action( object, exit, err = kNoMemoryErr );
	
	object->base.signature		= kCFLSignatureValid;
	object->base.type			= kCFLTypeArray;
	object->base.retainCount	= 1;
	object->callbacks	 		= inCallBacks ? *inCallBacks : kCFLArrayCallBacksNull;
	
	// Success!
	
	*outRef = (CFLArrayRef) object;
	object = NULL;
	err = kNoErr;
	
exit:
	if( object ) CFLRelease( (CFLArrayRef) object );
	return( err );
}

//===========================================================================================================================
//	CFLArrayCreateCopy
//===========================================================================================================================

OSStatus	CFLArrayCreateCopy( CFLAllocatorRef inAllocator, CFLArrayRef inArray, CFLArrayRef *outArray )
{
	OSStatus		err;
	CFLArray *		oldArray;
	CFLArrayRef		newArray = NULL;
	CFLIndex		i, n;
	
	require_action( CFLValidObjectType( inArray, kCFLTypeArray ), exit, err = kBadReferenceErr );
	oldArray = (CFLArray *) inArray;
	
	err = CFLArrayCreate( inAllocator, &oldArray->callbacks, &newArray );
	require_noerr( err, exit );
	
	n = oldArray->count;
	for( i = 0; i < n; ++i )
	{
		err = CFLArrayAppendValue( newArray, oldArray->storage[ i ] );
		require_noerr( err, exit );
	}
	
	*outArray = newArray;
	newArray = NULL;
	
exit:
	if( newArray ) CFLRelease( newArray );
	return( err );
}

//===========================================================================================================================
//	CFLArrayGetCount
//===========================================================================================================================

OSStatus	CFLArrayGetCount( CFLArrayRef inObject, CFLIndex *outCount )
{
	OSStatus		err;
	CFLArray *		object;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeArray ), exit, err = kBadReferenceErr );
	object = (CFLArray *) inObject;
	
	if( outCount ) *outCount = object->count;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLArrayGetValues
//===========================================================================================================================

OSStatus	CFLArrayGetValues( CFLArrayRef inObject, CFLIndex inIndex, CFLIndex inCount, const void **inValues )
{
	OSStatus		err;
	CFLArray *		object;
	CFLIndex		i;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeArray ), exit, err = kBadReferenceErr );
	object = (CFLArray *) inObject;
	require_action( ( inIndex >= 0 ) && ( inIndex <= object->count ), exit, err = kRangeErr );
	require_action( ( inCount >= 0 ) && ( inCount <= object->count ), exit, err = kRangeErr );
	i = inIndex + inCount;
	require_action( ( i >= 0 ) && ( i <= object->count ), exit, err = kRangeErr );
	
	for( i = 0; i < inCount; ++i )
	{
		inValues[ i ] = object->storage[ inIndex++ ];
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLArrayGetValueAtIndex
//===========================================================================================================================

OSStatus	CFLArrayGetValueAtIndex( CFLArrayRef inObject, CFLIndex inIndex, void *outValue )
{
	OSStatus		err;
	CFLArray *		object;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeArray ), exit, err = kBadReferenceErr );
	object = (CFLArray *) inObject;
	require_action_quiet( ( inIndex >= 0 ) && ( inIndex < object->count ), exit, err = kRangeErr );
	
	if( outValue ) *( (void **) outValue ) = object->storage[ inIndex ];
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLArraySetValueAtIndex
//===========================================================================================================================

OSStatus	CFLArraySetValueAtIndex( CFLArrayRef inObject, CFLIndex inIndex, const void *inValue )
{
	OSStatus		err;
	CFLArray *		object;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeArray ), exit, err = kBadReferenceErr );
	object = (CFLArray *) inObject;
	require_action( ( inIndex >= 0 ) && ( inIndex < object->count ), exit, err = kRangeErr );
	
	// Retain the new value then release any existing value.
	
	if( object->callbacks.retain )	object->callbacks.retain( kCFLAllocatorDefault, inValue );
	if( object->callbacks.release )	object->callbacks.release( kCFLAllocatorDefault, object->storage[ inIndex ] );
	object->storage[ inIndex ] = (void *) inValue;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLArrayInsertValueAtIndex
//===========================================================================================================================

OSStatus	CFLArrayInsertValueAtIndex( CFLArrayRef inObject, CFLIndex inIndex, const void *inValue )
{
	OSStatus		err;
	CFLArray *		object;
	void **			newStorage;
	void **			oldStorage;
	CFLIndex		i, n;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeArray ), exit, err = kBadReferenceErr );
	object = (CFLArray *) inObject;
	if( inIndex == kCFLIndexEnd ) inIndex = object->count;
	require_action( ( inIndex >= 0 ) && ( inIndex <= object->count ), exit, err = kRangeErr );
	
	// Resize the array to hold the new value.
	
	n = object->count + 1;
	newStorage = (void **) malloc( ( (size_t) n ) * sizeof( void * ) );
	require_action( newStorage, exit, err = kNoMemoryErr );
	
	// Copy over the old values except for the slot where the new value will go then replace the storage.
	
	oldStorage = object->storage;
	for( i = 0; i < inIndex; ++i )
	{
		newStorage[ i ] = oldStorage[ i ];
	}
	for( i = inIndex + 1; i < n; ++i )
	{
		newStorage[ i ] = oldStorage[ i - 1 ];
	}
	if( oldStorage ) free( oldStorage );
	object->storage = newStorage;
	
	// Retain the new value then release any existing value.
	
	if( object->callbacks.retain ) object->callbacks.retain( kCFLAllocatorDefault, inValue );
	object->storage[ inIndex ] = (void *) inValue;
	object->count = n;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLArrayAppendValue
//===========================================================================================================================

OSStatus	CFLArrayAppendValue( CFLArrayRef inObject, const void *inValue )
{
	return( CFLArrayInsertValueAtIndex( inObject, kCFLIndexEnd, inValue ) );
}

//===========================================================================================================================
//	CFLArrayRemoveValueAtIndex
//===========================================================================================================================

OSStatus	CFLArrayRemoveValueAtIndex( CFLArrayRef inObject, CFLIndex inIndex )
{
	OSStatus		err;
	CFLArray *		object;
	void **			newStorage;
	void **			oldStorage;
	CFLIndex		i, n;
	void *			oldValue;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeArray ), exit, err = kBadReferenceErr );
	object = (CFLArray *) inObject;
	require_action( ( inIndex >= 0 ) && ( inIndex < object->count ), exit, err = kRangeErr );
	check( object->storage );
	
	// Resize the array to account for the removed value.
	
	oldStorage = object->storage;
	n = object->count - 1;
	if( n > 0 )
	{
		newStorage = (void **) malloc( ( (size_t) n ) * sizeof( void * ) );
		require_action( newStorage, exit, err = kNoMemoryErr );
		
		// Copy over the old values except for the slot where the new value will go then replace the storage.
		
		for( i = 0; i < inIndex; ++i )
		{
			newStorage[ i ] = oldStorage[ i ];
		}
		for( i = inIndex + 1; i <= n; ++i )
		{
			newStorage[ i - 1 ] = oldStorage[ i ];
		}
	}
	else
	{
		newStorage = NULL;
	}
	oldValue = oldStorage[ inIndex ];
	free( oldStorage );
	object->storage = newStorage;
	object->count = n;
	
	// Release the removed value.
	
	if( object->callbacks.release ) object->callbacks.release( kCFLAllocatorDefault, oldValue );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLArrayRemoveAllValues
//===========================================================================================================================

OSStatus	CFLArrayRemoveAllValues( CFLArrayRef inObject )
{
	OSStatus		err;
	CFLArray *		object;
	CFLIndex		i, n;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeArray ), exit, err = kBadReferenceErr );
	object = (CFLArray *) inObject;
	check( ( object->count == 0 ) || object->storage );
	
	// Release all the values and free the memory used by the pointer storage.
	
	n = object->count;
	for( i = 0; i < n; ++i )
	{
		if( object->callbacks.release ) object->callbacks.release( kCFLAllocatorDefault, object->storage[ i ] );
	}
	if( object->storage )
	{
		free( object->storage );
		object->storage = NULL;
		object->count = 0;
	}
	check( object->count == 0 );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLArrayContainsValue
//===========================================================================================================================

Boolean	CFLArrayContainsValue( CFLArrayRef inObject, CFLIndex inStart, CFLIndex inEnd, const void *inValue )
{
	CFLArray *		object;
	CFLIndex		i;
	
	require( CFLValidObjectType( inObject, kCFLTypeArray ), exit );
	object = (CFLArray *) inObject;
	
	// Note: "inEnd" is 1 past the last index to check (e.g. &array[ inEnd ]) so <= is OK here.
	
	require( ( inStart >= 0 ) && ( inStart <= inEnd ) && ( inEnd <= object->count ), exit );
	
	for( i = inStart; i < inEnd; ++i )
	{
		const void *		x;
		
		// Equal ptrs means equal values.
		
		x = object->storage[ i ];
		if( x == inValue )
		{
			return( true );
		}
		
		// If there's an equal callback check that. If there's no equal callback then we only compare ptrs.
		
		if( object->callbacks.equal && object->callbacks.equal( x, inValue ) )
		{
			return( true );
		}
	}
	
exit:
	return( false );
}

//===========================================================================================================================
//	CFLArrayApplyFunction
//===========================================================================================================================

OSStatus	CFLArrayApplyFunction( CFLArrayRef inArray, CFLRange inRange, CFLArrayApplierFunction inApplier, void *inContext )
{
	OSStatus		err;
	CFLArray *		obj;
	CFLIndex		i, n;
	
	require_action( CFLValidObjectType( inArray, kCFLTypeArray ), exit, err = kBadReferenceErr );
	obj = (CFLArray *) inArray;
	
	n = inRange.location + inRange.length;
	for( i = inRange.location; i < n; ++i )
	{
		inApplier( obj->storage[ i ], inContext );
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLArraySortValues
//===========================================================================================================================

OSStatus	CFLArraySortValues( CFLArrayRef inArray, CFLRange inRange, CFLComparatorFunction inCmp, void *inContext )
{
	OSStatus		err;
	CFLArray *		obj;
	CFLIndex		n;
	
	require_action( CFLValidObjectType( inArray, kCFLTypeArray ), exit, err = kBadReferenceErr );
	obj = (CFLArray *) inArray;
	
	n = inRange.location + inRange.length;
	require_action( ( inRange.location >= 0 ) && ( inRange.location <= n ) && ( n <= obj->count ), exit, err = kRangeErr );
	_CFLQSortPtrs( &obj->storage[ inRange.location ], (size_t) inRange.length, inCmp, inContext );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CFLArrayEqual
//===========================================================================================================================

static Boolean	_CFLArrayEqual( CFLObjectRef inLeft, CFLObjectRef inRight )
{
	OSStatus		err;
	CFLArray *		l;
	CFLArray *		r;
	CFLIndex		i, n;
	
	// The upper-level CFLEqual routine will have already validated the objects and performed pointer equality testing 
	// so it is unnecessary here because this should never get called unless those tests failed. Just assert instead.
	
	check( CFLValidObjectType( inLeft, kCFLTypeArray ) );
	check( CFLValidObjectType( inRight, kCFLTypeArray ) );
	check_string( inLeft != inRight, "object ptrs match...should have passed earlier equality testing" );
	l = (CFLArray *) inLeft;
	r = (CFLArray *) inRight;
	
	// Different counts means different objects.
	
	if( l->count != r->count )
	{
		err = kMismatchErr;
		goto exit;
	}
	
	// Different equal callbacks means different objects.
	
	if( l->callbacks.equal != r->callbacks.equal )
	{
		err = kMismatchErr;
		goto exit;
	}
	
	// Compare each value. Different values means different objects.
	
	n = l->count;
	for( i = 0; i < n; ++i )
	{
		const void *		lValue;
		const void *		rValue;
		
		// Equal ptrs means equal values.
		
		lValue = l->storage[ i ];
		rValue = r->storage[ i ];
		if( lValue == rValue ) continue;
		
		// Different values means different objects.
		
		if( l->callbacks.equal )
		{
			if( !l->callbacks.equal( lValue, rValue ) )
			{
				err = kMismatchErr;
				goto exit;
			}
		}
	}
	
	// All equality tests passed. Objects are equal.
	
	err = kNoErr;
	
exit:
	return( (Boolean)( err == kNoErr ) );
}

//===========================================================================================================================
//	_CFLArrayHash
//===========================================================================================================================

static CFLHashCode	_CFLArrayHash( CFLObjectRef inObject )
{
	CFLArray *		object;
	
	// The upper-level CFLHash routine will have already validated parameters so just assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeArray ) );
	object = (CFLArray *) inObject;
	
	return( (CFLHashCode) object->count );
}

//===========================================================================================================================
//	_CFLArrayFree
//===========================================================================================================================

static void	_CFLArrayFree( CFLObjectRef inObject )
{
	OSStatus		err;
	CFLArray *		object;
	
	DEBUG_USE_ONLY( err );
	
	// The upper-level CFLRelease/etc routine will have already validated parameters so juse assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeArray ) );
	object = (CFLArray *) inObject;
	
	// Remove all the values in the array. This also releases all the keys and values in the Array.
	
	err = CFLArrayRemoveAllValues( object );
	check_noerr( err );
	DEBUG_USE_ONLY( err );
}

//===========================================================================================================================
//	_CFLArrayCopy
//===========================================================================================================================

static OSStatus	_CFLArrayCopy( CFLArrayRef inSrc, CFLArrayRef *outDst )
{
	OSStatus			err;
	CFLArrayRef			newArray;
	CFLIndex			i, n;
	CFLObjectRef		oldValue, newValue;
	
	newArray = NULL;
	check( outDst );
	
	// Create an empty array to copy the elements into.
	
	err = CFLArrayCreate( kCFLAllocatorDefault, &kCFLArrayCallBacksCFLTypes, &newArray );
	require_noerr( err, exit );
	
	// Copy all the elements from the old array to the new array.
	
	err = CFLArrayGetCount( inSrc, &n );
	require_noerr( err, exit );
	
	for( i = 0; i < n; ++i )
	{
		err = CFLArrayGetValueAtIndex( inSrc, i, (void *) &oldValue );
		require_noerr( err, exit );
		
		err = CFLCopy( oldValue, &newValue );
		require_noerr( err, exit );
		
		err = CFLArrayAppendValue( newArray, newValue );
		CFLRelease( newValue );
		require_noerr( err, exit );
	}
	
	*outDst = newArray;
	newArray = NULL;
	
exit:
	if( newArray ) CFLRelease( newArray );
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Boolean ==
#endif

//===========================================================================================================================
//	CFLBooleanGetTypeID
//===========================================================================================================================

CFLTypeID	CFLBooleanGetTypeID( void )
{
	return( kCFLTypeBoolean );
}

#if 0
#pragma mark -
#pragma mark == Data ==
#endif

//===========================================================================================================================
//	CFLDataGetTypeID
//===========================================================================================================================

CFLTypeID	CFLDataGetTypeID( void )
{
	return( kCFLTypeData );
}

//===========================================================================================================================
//	CFLDataCreate
//===========================================================================================================================

OSStatus	CFLDataCreate( CFLAllocatorRef inAllocator, const void *inData, size_t inSize, CFLDataRef *outRef )
{
	OSStatus		err;
	CFLData *		object;
	size_t			n;
	
	object = NULL;
	require_action( inAllocator == kCFLAllocatorDefault, exit, err = kParamErr );
	require_action( outRef, exit, err = kParamErr );
	
	// Allocate and initialize the object.
	
	object = (CFLData *) calloc( 1U, sizeof( CFLData ) );
	require_action( object, exit, err = kNoMemoryErr );
	
	object->base.signature		= kCFLSignatureValid;
	object->base.type			= kCFLTypeData;
	object->base.retainCount	= 1;
	
	// Allocate the data buffer. Round the reserved size up to the next chunk size multiple. Copy any input data.
	
	n = CFLDataRoundUpSize( inSize );
	object->data = (uint8_t *) calloc( 1U, n );
	require_action( object->data, exit, err = kNoMemoryErr );
	
	object->usedSize			= inSize;
	object->reservedSize 		= n;
	object->bytesDeallocator	= kCFLAllocatorDefault;
	if( inData ) memcpy( object->data, inData, inSize );
	
	// Success!
	
	*outRef = (CFLDataRef) object;
	object = NULL;
	err = kNoErr;
	
exit:
	if( object ) CFLRelease( (CFLDataRef) object );
	return( err );
}

//===========================================================================================================================
//	CFLDataCreateNoCopy
//===========================================================================================================================

OSStatus
	CFLDataCreateNoCopy( 
		CFLAllocatorRef		inAllocator, 
		const void *		inData, 
		size_t				inSize, 
		CFLAllocatorRef		inBytesDeallocator, 
		CFLDataRef *		outRef )
{
	OSStatus		err;
	CFLData *		object = NULL;
	
	require_action( inAllocator == kCFLAllocatorDefault, exit, err = kParamErr );
	require_action( ( inBytesDeallocator == kCFLAllocatorMalloc ) || ( inBytesDeallocator == kCFLAllocatorNull ), 
		exit, err = kParamErr );
	require_action( outRef, exit, err = kParamErr );
	
	object = (CFLData *) calloc( 1U, sizeof( CFLData ) );
	require_action( object, exit, err = kNoMemoryErr );
	
	object->base.signature		= kCFLSignatureValid;
	object->base.type			= kCFLTypeData;
	object->base.retainCount	= 1;
	object->data				= (uint8_t *) inData;
	object->usedSize			= inSize;
	object->reservedSize		= inSize;
	object->bytesDeallocator	= inBytesDeallocator;
	
	*outRef = (CFLDataRef) object;
	object = NULL;
	err = kNoErr;
	
exit:
	if( object ) CFLRelease( (CFLDataRef) object );
	return( err );
}

//===========================================================================================================================
//	CFLDataCreateSubdataWithRangeNoCopy
//===========================================================================================================================

OSStatus
	CFLDataCreateSubdataWithRangeNoCopy( 
		CFLAllocatorRef	inAllocator, 
		CFLDataRef		inData, 
		CFLRange		inRange, 
		CFLDataRef *	outRef )
{
	OSStatus		err;
	CFLData *		object = NULL;
	uint8_t *		ptr;
	uint8_t *		end;
	size_t			len;
	
	require_action( inAllocator == kCFLAllocatorDefault, exit, err = kParamErr );
	require_action( inData, exit, err = kParamErr );
	require_action( outRef, exit, err = kParamErr );
	
	err = CFLDataGetDataPtr( inData, &ptr, &len );
	require_noerr( err, exit );
	require_action( ( inRange.location >= 0 ) && ( inRange.location <= ( (CFLIndex) len ) ), exit, err = kRangeErr );
	end = ptr + len;
	ptr += inRange.location;
	require_action( ( end - ptr ) >= ( (ptrdiff_t) inRange.length ), exit, err = kSizeErr );
	
	object = (CFLData *) calloc( 1U, sizeof( CFLData ) );
	require_action( object, exit, err = kNoMemoryErr );
	
	object->base.signature		= kCFLSignatureValid;
	object->base.type			= kCFLTypeData;
	object->base.retainCount	= 1;
	object->data				= ptr;
	object->usedSize			= (size_t) inRange.length;
	object->reservedSize		= object->usedSize;
	object->bytesDeallocator	= kCFLAllocatorNull;
	object->parentData			= inData;
	CFLRetain( inData );
	
	*outRef = (CFLDataRef) object;
	object = NULL;
	err = kNoErr;
	
exit:
	if( object ) CFLRelease( (CFLDataRef) object );
	return( err );
}

//===========================================================================================================================
//	CFLDataGetDataPtr
//===========================================================================================================================

OSStatus	CFLDataGetDataPtr( CFLDataRef inObject, void *outDataPtr, size_t *outSize )
{
	OSStatus		err;
	CFLData *		object;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeData ), exit, err = kBadReferenceErr );
	object = (CFLData *) inObject;
	check( object->data );
	
	if( outDataPtr )	*( (void **) outDataPtr )	= object->data;
	if( outSize )		*outSize					= object->usedSize;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLDataSetData
//===========================================================================================================================

OSStatus	CFLDataSetData( CFLDataRef inObject, const void *inData, size_t inSize )
{
	OSStatus		err;
	CFLData *		object;
	uint8_t *		data;
	size_t			n;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeData ), exit, err = kBadReferenceErr );
	object = (CFLData *) inObject;
	require_action( object->bytesDeallocator == kCFLAllocatorDefault, exit, err = kUnsupportedErr );
	
	n = CFLDataRoundUpSize( inSize );
#if( TARGET_NO_REALLOC )
	data = (uint8_t *) _CFLrealloc( object->data, object->reservedSize, n );
#else
	data = (uint8_t *) realloc( object->data, n );
#endif
	require_action( data, exit, err = kNoMemoryErr );
	
	if( inData && ( inSize > 0 ) )
	{
		memcpy( data, inData, inSize );
	}
	
	object->data			= data;
	object->usedSize		= inSize;
	object->reservedSize	= n;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLDataAppendData
//===========================================================================================================================

OSStatus	CFLDataAppendData( CFLDataRef inObject, const void *inData, size_t inSize )
{
	OSStatus		err;
	CFLData *		object;
	size_t			n;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeData ), exit, err = kBadReferenceErr );
	object = (CFLData *) inObject;
	require_action( object->bytesDeallocator == kCFLAllocatorDefault, exit, err = kUnsupportedErr );
	if( inSize == kSizeCString ) inSize = strlen( (const char *) inData );
	
	// Reallocate the buffer if there is not enough space available in the existing buffer.
	
	n = object->usedSize + inSize;
	if( n >= object->reservedSize )
	{
		uint8_t *		tmp;
		
		n = CFLDataRoundUpSize( n );
		
		#if( TARGET_NO_REALLOC )
			tmp = (uint8_t *) _CFLrealloc( object->data, object->reservedSize, n );
			require_action( tmp, exit, err = kNoMemoryErr );
		#else
			tmp = (uint8_t *) realloc( object->data, n );
			require_action( tmp, exit, err = kNoMemoryErr );
		#endif
		
		object->data 			= tmp;
		object->reservedSize 	= n;
	}
	
	// Copy in the new data. A null ptr means to resize, but not copy any data.
	
	if( inData ) memcpy( object->data + object->usedSize, inData, inSize );
	object->usedSize += inSize;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CFLDataEqual
//===========================================================================================================================

static Boolean	_CFLDataEqual( CFLObjectRef inLeft, CFLObjectRef inRight )
{
	OSStatus		err;
	CFLData *		l;
	CFLData *		r;
	
	// The upper-level CFLEqual routine will have already validated the objects and performed pointer equality testing 
	// so it is unnecessary here because this should never get called unless those tests failed. Just assert instead.
	
	check( CFLValidObjectType( inLeft, kCFLTypeData ) );
	check( CFLValidObjectType( inRight, kCFLTypeData ) );
	check_string( inLeft != inRight, "object ptrs match...should have passed earlier equality testing" );
	l = (CFLData *) inLeft;
	r = (CFLData *) inRight;
	
	// Different sizes means different objects.
	
	if( l->usedSize != r->usedSize )
	{
		err = kMismatchErr;
		goto exit;
	}
	
	// Different content means different objects.
	
	if( l->data && r->data && ( l->usedSize > 0 ) )
	{
		if( memcmp( l->data, r->data, l->usedSize ) != 0 )
		{
			err = kMismatchErr;
			goto exit;
		}
	}
	
	// All equality tests passed. Objects are equal.
	
	err = kNoErr;
	
exit:
	return( (Boolean)( err == kNoErr ) );
}

//===========================================================================================================================
//	_CFLDataHash
//===========================================================================================================================

static CFLHashCode	_CFLDataHash( CFLObjectRef inObject )
{
	CFLData *		object;
	
	// The upper-level CFLHash routine will have already validated parameters so just assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeData ) );
	object = (CFLData *) inObject;
	return( _CFLHashFNV1a( object->data, object->usedSize ) );
}

//===========================================================================================================================
//	_CFLDataFree
//===========================================================================================================================

static void	_CFLDataFree( CFLObjectRef inObject )
{
	CFLData *		object;
	
	// The upper-level CFLRelease/etc routine will have already validated parameters so just assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeData ) );
	object = (CFLData *) inObject;
	
	// Free the memory used for the data.
	
	if( ( object->bytesDeallocator == kCFLAllocatorDefault ) || ( object->bytesDeallocator == kCFLAllocatorMalloc ) )
	{
		ForgetMem( &object->data );
	}
	if( object->parentData )
	{
		CFLRelease( object->parentData );
		object->parentData = NULL;
	}
}

//===========================================================================================================================
//	_CFLDataCopy
//===========================================================================================================================

static OSStatus	_CFLDataCopy( CFLDataRef inSrc, CFLDataRef *outDst )
{
	OSStatus		err;
	void *			p;
	size_t			n;
	
	check( outDst );
	
	err = CFLDataGetDataPtr( inSrc, &p, &n );
	require_noerr( err, exit );
	
	err = CFLDataCreate( kCFLAllocatorDefault, p, n, outDst );
	require_noerr( err, exit );
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Date ==
#endif

//===========================================================================================================================
//	CFLDateGetTypeID
//===========================================================================================================================

CFLTypeID	CFLDateGetTypeID( void )
{
	return( kCFLTypeDate );
}

//===========================================================================================================================
//	CFLDateCreate
//===========================================================================================================================

OSStatus	CFLDateCreate( CFLAllocatorRef inAllocator, const CFLDateComponents *inDate, CFLDateRef *outRef )
{
	OSStatus		err;
	CFLDate *		object;
	
	object = NULL;
	require_action( inAllocator == kCFLAllocatorDefault, exit, err = kParamErr );
	require_action( inDate, exit, err = kParamErr );
	require_action( outRef, exit, err = kParamErr );
	
	// Allocate and initialize the Date.
	
	object = (CFLDate *) calloc( 1U, sizeof( CFLDate ) );
	require_action( object, exit, err = kNoMemoryErr );
	
	object->base.signature		= kCFLSignatureValid;
	object->base.type			= kCFLTypeDate;
	object->base.retainCount	= 1;
	
	err = CFLDateSetDate( (CFLDateRef) object, inDate );
	require_noerr( err, exit );
	
	// Success!
	
	*outRef = (CFLDateRef) object;
	object = NULL;
	err = kNoErr;
	
exit:
	if( object ) CFLRelease( (CFLDateRef) object );
	return( err );
}

//===========================================================================================================================
//	CFLDateGetDate
//===========================================================================================================================

OSStatus	CFLDateGetDate( CFLDateRef inObject, CFLDateComponents *outDate )
{
	OSStatus		err;
	CFLDate *		object;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeDate ), exit, err = kBadReferenceErr );
	object = (CFLDate *) inObject;
	
	if( outDate ) *outDate = object->date;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLDateSetDate
//===========================================================================================================================

OSStatus	CFLDateSetDate( CFLDateRef inObject, const CFLDateComponents *inDate )
{
	OSStatus		err;
	CFLDate *		object;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeDate ), exit, err = kBadReferenceErr );
	require_action( inDate, exit, err = kParamErr );
	object = (CFLDate *) inObject;
	
	object->date = *inDate;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CFLDateEqual
//===========================================================================================================================

static Boolean	_CFLDateEqual( CFLObjectRef inLeft, CFLObjectRef inRight )
{
	CFLDate *		l;
	CFLDate *		r;
	
	// The upper-level CFLEqual routine will have already validated the objects and performed pointer equality testing 
	// so it is unnecessary here because this should never get called unless those tests failed. Just assert instead.
	
	check( CFLValidObjectType( inLeft, kCFLTypeDate ) );
	check( CFLValidObjectType( inRight, kCFLTypeDate ) );
	check_string( inLeft != inRight, "object ptrs match...should have passed earlier equality testing" );
	l = (CFLDate *) inLeft;
	r = (CFLDate *) inRight;
	
	// Equal date components means equal objects.
	
	if( ( l->date.year 		== r->date.year ) 	&& 
		( l->date.month 	== r->date.month ) 	&& 
		( l->date.day 		== r->date.day ) 	&& 
		( l->date.hour 		== r->date.hour ) 	&& 
		( l->date.minute 	== r->date.minute ) && 
		( l->date.second 	== r->date.second ) )
	{
		return( true );
	}
	return( false );
}

//===========================================================================================================================
//	_CFLDateHash
//===========================================================================================================================

static CFLHashCode	_CFLDateHash( CFLObjectRef inObject )
{
	CFLDate *		obj;
	CFLHashCode		hash;
	
	// The upper-level CFLHash routine will have already validated parameters so just assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeDate ) );
	obj = (CFLDate *) inObject;
	
	// Use the total seconds as the hash.
	
	hash  =   (CFLHashCode)( obj->date.year   * kSecondsPerYear );
	hash += ( (CFLHashCode)( obj->date.month  * kSecondsPerMonth ) );
	hash += ( (CFLHashCode)( obj->date.day    * kSecondsPerDay ) );
	hash += ( (CFLHashCode)( obj->date.hour   * kSecondsPerHour ) );
	hash += ( (CFLHashCode)( obj->date.minute * kSecondsPerMinute ) );
	hash += ( (CFLHashCode)  obj->date.second );
	return( hash );
}

//===========================================================================================================================
//	_CFLDateFree
//===========================================================================================================================

static void	_CFLDateFree( CFLObjectRef inObject )
{
	DEBUG_USE_ONLY( inObject );
	
	// The upper-level CFLRelease/etc routine will have already validated parameters so just assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeDate ) );
}

//===========================================================================================================================
//	_CFLDateCopy
//===========================================================================================================================

static OSStatus	_CFLDateCopy( CFLDateRef inSrc, CFLDateRef *outDst )
{
	OSStatus				err;
	CFLDateComponents		dateComponents;
	
	check( outDst );
	
	err = CFLDateGetDate( inSrc, &dateComponents );
	require_noerr( err, exit );
	
	err = CFLDateCreate( kCFLAllocatorDefault, &dateComponents, outDst );
	require_noerr( err, exit );
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Dictionary ==
#endif

//===========================================================================================================================
//	CFLDictionaryGetTypeID
//===========================================================================================================================

CFLTypeID	CFLDictionaryGetTypeID( void )
{
	return( kCFLTypeDictionary );
}

//===========================================================================================================================
//	CFLDictionaryCreate
//===========================================================================================================================

OSStatus
	CFLDictionaryCreate( 
		CFLAllocatorRef 					inAllocator, 
		CFLIndex							inCapacity, 
		const CFLDictionaryKeyCallBacks *	inKeyCallBacks, 
		const CFLDictionaryValueCallBacks *	inValueCallBacks, 
		CFLDictionaryRef *					outRef )
{
	OSStatus			err;
	CFLDictionary *		object;
	
	object = NULL;
	
	require_action( inAllocator == kCFLAllocatorDefault, exit, err = kParamErr );
	require_action( outRef, exit, err = kParamErr );
	if( inCapacity <= 0 )	inCapacity = 101;	// Default to a reasonable prime number so modular hashes work well.
	else					inCapacity = _CFLNextPrime( inCapacity );
	
	// Allocate and initialize the dictionary.
	
	object = (CFLDictionary *) calloc( 1U, sizeof( CFLDictionary ) );
	require_action( object, exit, err = kNoMemoryErr );
	
	object->base.signature		= kCFLSignatureValid;
	object->base.type			= kCFLTypeDictionary;
	object->base.retainCount	= 1;
	object->keyCallBacks 		= inKeyCallBacks   ? *inKeyCallBacks   : kCFLDictionaryKeyCallBacksNull;
	object->valueCallBacks 		= inValueCallBacks ? *inValueCallBacks : kCFLDictionaryValueCallBacksNull;
	
	// Allocate an initial array of buckets.
	
	object->buckets = (CFLDictionaryNode **) calloc( 1U, ( (size_t) inCapacity ) * sizeof( CFLDictionaryNode * ) );
	require_action( object->buckets, exit, err = kNoMemoryErr );
	object->bucketCount = inCapacity;
	
	// Success!
	
	*outRef = (CFLDictionaryRef) object;
	object = NULL;
	err = kNoErr;
	
exit:
	if( object ) CFLRelease( (CFLDictionaryRef) object );
	return( err );
}

//===========================================================================================================================
//	CFLDictionaryCreateCopy
//===========================================================================================================================

OSStatus	CFLDictionaryCreateCopy( CFLAllocatorRef inAllocator, CFLDictionaryRef inDict, CFLDictionaryRef *outDict )
{
	OSStatus				err;
	CFLDictionaryRef		newDict = NULL;
	CFLIndex				i, n;
	CFLDictionaryNode *		node;
	
	require_action( CFLValidObjectType( inDict, kCFLTypeDictionary ), exit, err = kBadReferenceErr );
	
	err = CFLDictionaryCreate( inAllocator, inDict->bucketCount, &inDict->keyCallBacks, &inDict->valueCallBacks, &newDict );
	require_noerr( err, exit );
	
	n = inDict->bucketCount;
	for( i = 0; i < n; ++i )
	{
		for( node = inDict->buckets[ i ]; node; node = node->next )
		{
			err = CFLDictionaryAddValue( newDict, node->key, node->value );
			require_noerr( err, exit );
		}
	}
		
	*outDict = newDict;
	newDict = NULL;
	
exit:
	if( newDict ) CFLRelease( newDict );
	return( err );
}

//===========================================================================================================================
//	CFLDictionaryGetCount
//===========================================================================================================================

OSStatus	CFLDictionaryGetCount( CFLDictionaryRef inObject, CFLIndex *outCount )
{
	OSStatus			err;
	CFLDictionary *		object;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeDictionary ), exit, err = kBadReferenceErr );
	object = (CFLDictionary *) inObject;
	
	if( outCount ) *outCount = object->count;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLDictionaryGetValue
//===========================================================================================================================

OSStatus	CFLDictionaryGetValue( CFLDictionaryRef inObject, const void *inKey, void *outValue )
{
	OSStatus					err;
	CFLDictionary *				object;
	CFLDictionaryNode **		node;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeDictionary ), exit, err = kBadReferenceErr );
	object = (CFLDictionary *) inObject;
	
	// Search for the node by key.
	
	err = _CFLDictionaryFindKey( object, inKey, &node );
	if( err == kNoErr )
	{
		if( outValue ) *( (const void **) outValue ) = ( *node )->value;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLDictionarySetValue
//===========================================================================================================================

OSStatus	CFLDictionarySetValue( CFLDictionaryRef inObject, const void *inKey, const void *inValue )
{
	OSStatus					err;
	CFLDictionary *				object;
	CFLDictionaryNode **		node;
	int							found;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeDictionary ), exit, err = kBadReferenceErr );
	object = (CFLDictionary *) inObject;
	
	// Search for the node by key and get a pointer to the node it should go whether found or not.
	
	err = _CFLDictionaryFindKey( object, inKey, &node );
	require( ( err == kNoErr ) || ( err == kNotFoundErr ), exit );
	require_action( node, exit, err = kUnknownErr );
	found = ( err == kNoErr );
	
	// If the key was not found, add a new entry for it.
	
	if( !found )
	{
		CFLDictionaryNode *		newNode;
		
		newNode = (CFLDictionaryNode *) calloc( 1U, sizeof( CFLDictionaryNode ) );
		require_action( newNode, exit, err = kNoMemoryErr );
		
		newNode->key 	= inKey;
		newNode->next 	= NULL;
		*node 			= newNode;
		object->count  += 1;
		
		// Retain the key.
		
		if( object->keyCallBacks.retain ) object->keyCallBacks.retain( kCFLAllocatorDefault, inKey );
	}
	
	// Retain the new value then release any existing value.
	
	if( object->valueCallBacks.retain ) object->valueCallBacks.retain( kCFLAllocatorDefault, inValue );
	if( found )
	{
		if( object->valueCallBacks.release ) object->valueCallBacks.release( kCFLAllocatorDefault, ( *node )->value );
	}
	( *node )->value = inValue;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLDictionaryAddValue
//===========================================================================================================================

OSStatus	CFLDictionaryAddValue( CFLDictionaryRef inObject, const void *inKey, const void *inValue )
{
	OSStatus					err;
	CFLDictionary *				object;
	CFLDictionaryNode **		node;
	CFLDictionaryNode *			newNode;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeDictionary ), exit, err = kBadReferenceErr );
	object = (CFLDictionary *) inObject;
	
	err = _CFLDictionaryFindKey( object, inKey, &node );
	if( !err ) goto exit;
	require( err == kNotFoundErr, exit );
	require_action( node, exit, err = kUnknownErr );
	
	newNode = (CFLDictionaryNode *) calloc( 1U, sizeof( CFLDictionaryNode ) );
	require_action( newNode, exit, err = kNoMemoryErr );
	
	newNode->key 	= inKey;
	newNode->next 	= NULL;
	*node 			= newNode;
	object->count  += 1;
	
	if( object->keyCallBacks.retain ) object->keyCallBacks.retain( kCFLAllocatorDefault, inKey );
	if( object->valueCallBacks.retain ) object->valueCallBacks.retain( kCFLAllocatorDefault, inValue );
	( *node )->value = inValue;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLDictionaryRemoveValue
//===========================================================================================================================

OSStatus	CFLDictionaryRemoveValue( CFLDictionaryRef inObject, const void *inKey )
{
	OSStatus				err;
	CFLDictionary *			object;
	CFLDictionaryNode **	node;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeDictionary ), exit, err = kBadReferenceErr );
	object = (CFLDictionary *) inObject;
	
	// Search for the node by key and get a pointer to the node it should go whether found or not.
	
	err = _CFLDictionaryFindKey( object, inKey, &node );
	require( ( err == kNoErr ) || ( err == kNotFoundErr ), exit );
	require_action( node, exit, err = kUnknownErr );
	
	// Remove the node if found.
	
	if( err == kNoErr )
	{
		CFLDictionaryNode *		next;
		
		// Release the key and value, free the node, and disconnect from the list.
		
		if( object->keyCallBacks.release )		object->keyCallBacks.release( kCFLAllocatorDefault, ( *node )->key );
		if( object->valueCallBacks.release )	object->valueCallBacks.release( kCFLAllocatorDefault, ( *node )->value );
		
		next = ( *node )->next;
		free( *node );
		*node = next;
		object->count -= 1;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLDictionaryRemoveAllValues
//===========================================================================================================================

OSStatus	CFLDictionaryRemoveAllValues( CFLDictionaryRef inObject )
{
	OSStatus				err;
	CFLIndex				i;
	CFLIndex				n;
	CFLDictionary *			object;
	CFLDictionaryNode *		node;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeDictionary ), exit, err = kBadReferenceErr );
	object = (CFLDictionary *) inObject;
	
	// Remove each node in each bucket.
	
	n = object->bucketCount;
	for( i = 0; i < n; ++i )
	{
		CFLDictionaryNode *		nextNode;
		
		for( node = object->buckets[ i ]; node; node = nextNode )
		{
			// Release the key and value and free the node.
			
			if( object->keyCallBacks.release ) 		object->keyCallBacks.release( kCFLAllocatorDefault, node->key );
			if( object->valueCallBacks.release )	object->valueCallBacks.release( kCFLAllocatorDefault, node->value );
			
			nextNode = node->next;
			free( node );
		}
		object->buckets[ i ] = NULL;
	}
	object->count = 0;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLDictionaryContainsKey
//===========================================================================================================================

Boolean	CFLDictionaryContainsKey( CFLDictionaryRef inObject, const void *inKey )
{
	return( (Boolean)( _CFLDictionaryFindKey( inObject, inKey, NULL ) == kNoErr ) );
}

//===========================================================================================================================
//	CFLDictionaryCopyKeysAndValues
//===========================================================================================================================

OSStatus	CFLDictionaryCopyKeysAndValues( CFLDictionaryRef inObject, void *outKeys, void *outValues, CFLIndex *outCount )
{
	OSStatus			err;
	CFLDictionary *		object;
	void **				keys;
	void **				values;
	CFLIndex			n;
	CFLIndex			i;
	CFLIndex			iTotal;
	
	keys 	= NULL;
	values 	= NULL;
	require_action( CFLValidObjectType( inObject, kCFLTypeDictionary ), exit, err = kBadReferenceErr );
	object = (CFLDictionary *) inObject;
	
	if( object->count > 0 )
	{
		if( outKeys )
		{
			keys = (void **) malloc( ( (size_t) object->count ) * sizeof( void * ) );
			require_action( keys, exit, err = kNoMemoryErr );
		}
		if( outValues )
		{
			values = (void **) malloc( ( (size_t) object->count ) * sizeof( void * ) );
			require_action( values, exit, err = kNoMemoryErr );
		}
		
		// Copy each key/value ptr.
		
		iTotal = 0;
		n = object->bucketCount;
		for( i = 0; i < n; ++i )
		{
			CFLDictionaryNode *		node;
			
			for( node = object->buckets[ i ]; node; node = node->next )
			{
				if( keys )   keys[   iTotal ] = (void *) node->key;
				if( values ) values[ iTotal ] = (void *) node->value;
				++iTotal;
			}
		}
	}
	if( outKeys )
	{
		*( (void ** ) outKeys ) = keys;
		keys = NULL;
	}
	if( outValues )
	{
		*( (void ** ) outValues ) = values;
		values = NULL;
	}
	if( outCount ) *outCount = object->count;
	err = kNoErr;
	
exit:
	if( keys )	 free( keys );
	if( values ) free( values );
	return( err );
}

//===========================================================================================================================
//	CFLDictionaryGetKeysAndValues
//===========================================================================================================================

OSStatus	CFLDictionaryGetKeysAndValues( CFLDictionaryRef inObject, const void **ioKeys, const void **ioValues )
{
	OSStatus			err;
	CFLDictionary *		object;
	CFLIndex			n;
	CFLIndex			i;
	CFLIndex			iTotal;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeDictionary ), exit, err = kBadReferenceErr );
	object = (CFLDictionary *) inObject;
	
	if( object->count > 0 )
	{
		iTotal = 0;
		n = object->bucketCount;
		for( i = 0; i < n; ++i )
		{
			CFLDictionaryNode *		node;
			
			for( node = object->buckets[ i ]; node; node = node->next )
			{
				if( ioKeys )   ioKeys[   iTotal ] = (void *) node->key;
				if( ioValues ) ioValues[ iTotal ] = (void *) node->value;
				++iTotal;
			}
		}
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLDictionaryApplyFunction
//===========================================================================================================================

OSStatus	CFLDictionaryApplyFunction( CFLDictionaryRef inDict, CFLDictionaryApplierFunction inApplier, void *inContext )
{
	OSStatus				err;
	CFLDictionary *			obj;
	CFLIndex				i;
	CFLIndex				n;
	CFLDictionaryNode *		node;
	
	require_action( CFLValidObjectType( inDict, kCFLTypeDictionary ), exit, err = kBadReferenceErr );
	obj = (CFLDictionary *) inDict;
	
	n = obj->bucketCount;
	for( i = 0; i < n; ++i )
	{
		for( node = obj->buckets[ i ]; node; node = node->next )
		{
			inApplier( node->key, node->value, inContext );
		}
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CFLDictionaryEqual
//===========================================================================================================================

static Boolean	_CFLDictionaryEqual( CFLObjectRef inLeft, CFLObjectRef inRight )
{
	OSStatus			err;
	CFLDictionary *		l;
	CFLDictionary *		r;
	CFLIndex			i;
	CFLIndex			n;
	
	// The upper-level CFLEqual routine will have already validated the objects and performed pointer equality testing 
	// so it is unnecessary here because this should never get called unless those tests failed. Just assert instead.
	
	check( CFLValidObjectType( inLeft, kCFLTypeDictionary ) );
	check( CFLValidObjectType( inRight, kCFLTypeDictionary ) );
	check_string( inLeft != inRight, "object ptrs match...should have passed earlier equality testing" );
	l = (CFLDictionary *) inLeft;
	r = (CFLDictionary *) inRight;
	
	// Different counts means different objects.
	
	if( l->count != r->count )
	{
		err = kMismatchErr;
		goto exit;
	}
	
	// Different equal callbacks means different objects.
	
	if( ( l->keyCallBacks.equal 	!= r->keyCallBacks.equal ) || 
		( l->valueCallBacks.equal 	!= r->valueCallBacks.equal ) )
	{
		err = kMismatchErr;
		goto exit;
	}
	
	// Empty dictionaries that match up to this point are considered the equal.
	
	if( l->count == 0 )
	{
		err = kNoErr;
		goto exit;
	}
	
	// Lookup each key and compare the values. Any missing key or differing value means different objects.
	
	n = l->bucketCount;
	for( i = 0; i < n; ++i )
	{
		CFLDictionaryNode *		node;
		
		for( node = l->buckets[ i ]; node; node = node->next )
		{
			const void *		value;
			
			// Missing key means objects are different.
			
			err = CFLDictionaryGetValue( r, node->key, (void *) &value );
			check( ( err == kNoErr ) || ( err == kNotFoundErr ) );
			require_noerr_quiet( err, exit );
			
			// Equal ptrs means equal values.
			
			if( node->value == value ) continue;
			
			// Different values means different objects.
			
			if( l->valueCallBacks.equal )
			{
				if( !l->valueCallBacks.equal( node->value, value ) )
				{
					err = kMismatchErr;
					goto exit;
				}
			}
		}
	}
	
	// All equality tests passed. Objects are equal.
	
	err = kNoErr;
	
exit:
	return( (Boolean)( err == kNoErr ) );
}

//===========================================================================================================================
//	_CFLDictionaryHash
//===========================================================================================================================

static CFLHashCode	_CFLDictionaryHash( CFLObjectRef inObject )
{
	CFLDictionary *		object;
	
	// The upper-level CFLHash routine will have already validated parameters so juse assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeDictionary ) );
	object = (CFLDictionary *) inObject;
	
	return( (CFLHashCode) object->count );
}

//===========================================================================================================================
//	_CFLDictionaryFree
//===========================================================================================================================

static void	_CFLDictionaryFree( CFLObjectRef inObject )
{
	OSStatus			err;
	CFLDictionary *		object;
	
	// The upper-level CFLRelease/etc routine will have already validated parameters so juse assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeDictionary ) );
	object = (CFLDictionary *) inObject;
	
	// Remove all the values in the dictionary. This also releases all the keys and values in the dictionary.
	
	if( object->buckets )
	{
		err = CFLDictionaryRemoveAllValues( object );
		require_noerr( err, exit );
		
		free( object->buckets );
		object->buckets = NULL;
	}
	
exit:
	return;
}

//===========================================================================================================================
//	_CFLDictionaryCopy
//===========================================================================================================================

static OSStatus	_CFLDictionaryCopy( CFLDictionaryRef inSrc, CFLDictionaryRef *outDst )
{
	OSStatus				err;
	CFLDictionary *			object;
	CFLDictionaryRef		newDict;
	CFLIndex				n;
	CFLIndex				i;
	CFLObjectRef			newKey;
	CFLObjectRef			newValue;
	
	newDict		= NULL;
	newKey 		= NULL;
	newValue	= NULL;
	check( CFLValidObjectType( inSrc, kCFLTypeDictionary ) );
	object = (CFLDictionary *) inSrc;
	check( outDst );
	
	// Create an empty array to copy the entries into.
	
	err = CFLDictionaryCreate( kCFLAllocatorDefault, 0, &kCFLDictionaryKeyCallBacksCFLTypes, 
		&kCFLDictionaryValueCallBacksCFLTypes, &newDict );
	require_noerr( err, exit );
	
	// Copy each key/value object.
	
	n = object->bucketCount;
	for( i = 0; i < n; ++i )
	{
		CFLDictionaryNode *		node;
		
		for( node = object->buckets[ i ]; node; node = node->next )
		{
			err = CFLCopy( node->key, &newKey );
			require_noerr( err, exit );
			
			err = CFLCopy( node->value, &newValue );
			require_noerr( err, exit );
			
			err = CFLDictionarySetValue( newDict, newKey, newValue );
			require_noerr( err, exit );
			
			CFLRelease( newKey );
			CFLRelease( newValue );
			newKey = NULL;
			newValue = NULL;
		}
	}
	*outDst = newDict;
	newDict = NULL;
	err = kNoErr;
	
exit:
	if( newKey )	CFLRelease( newKey );
	if( newValue )	CFLRelease( newValue );
	if( newDict )	CFLRelease( newDict );
	return( err );
}

//===========================================================================================================================
//	_CFLDictionaryFindKey
//===========================================================================================================================

static OSStatus	_CFLDictionaryFindKey( const CFLDictionary *inObject, const void *inKey, CFLDictionaryNode ***outNode )
{
	OSStatus				err;
	CFLHashCode				hash;
	CFLDictionaryNode **	node;
	
	check( inObject );
	
	node = NULL;
	
	if( inObject->keyCallBacks.hash )
	{
		hash = inObject->keyCallBacks.hash( inKey );
	}
	else
	{
		hash = (CFLHashCode)(uintptr_t) inKey;
	}
	hash %= (CFLHashCode) inObject->bucketCount;
	
	for( node = &inObject->buckets[ hash ]; *node; node = &( *node )->next )
	{
		if( inObject->keyCallBacks.equal )
		{
			if( inObject->keyCallBacks.equal( inKey, ( *node )->key ) )
			{
				break;
			}
		}
		else
		{
			if( inKey == ( *node )->key )
			{
				break;
			}
		}
	}
	err = ( *node ) ? kNoErr : kNotFoundErr;
	if( outNode ) *outNode = node;
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Null ==
#endif

//===========================================================================================================================
//	CFLNullGetTypeID
//===========================================================================================================================

CFLTypeID	CFLNullGetTypeID( void )
{
	return( kCFLTypeNull );
}

#if 0
#pragma mark -
#pragma mark == Number ==
#endif

//===========================================================================================================================
//	CFLNumberGetTypeID
//===========================================================================================================================

CFLTypeID	CFLNumberGetTypeID( void )
{
	return( kCFLTypeNumber );
}

//===========================================================================================================================
//	CFLNumberCreate
//===========================================================================================================================

OSStatus	CFLNumberCreate( CFLAllocatorRef inAllocator, CFLNumberType inType, const void *inValue, CFLNumberRef *outRef )
{
	OSStatus		err;
	CFLNumber *		object;
	
	object = NULL;
	require_action( inAllocator == kCFLAllocatorDefault, exit, err = kParamErr );
	require_action( outRef, exit, err = kParamErr );
	
	// Allocate and initialize the Number.
	
	object = (CFLNumber *) calloc( 1U, sizeof( CFLNumber ) );
	require_action( object, exit, err = kNoMemoryErr );
	
	object->base.signature		= kCFLSignatureValid;
	object->base.type			= kCFLTypeNumber;
	object->base.retainCount	= 1;
	object->type				= inType;
	
	if(      inType == kCFLNumberSInt8Type )	object->value.s64  = *( (int8_t *)		inValue );
	else if( inType == kCFLNumberSInt16Type )	object->value.s64  = *( (int16_t *)		inValue );
	else if( inType == kCFLNumberSInt32Type )	object->value.s64  = *( (int32_t *)		inValue );
	else if( inType == kCFLNumberSInt64Type )	object->value.s64  = *( (int64_t *)		inValue );
	else if( inType == kCFLNumberSInt128Type )	object->value.s128 = *( (int128_compat *) inValue );
	else if( inType == kCFLNumberCharType )		object->value.s64  = *( (char *)		inValue );
	else if( inType == kCFLNumberShortType )	object->value.s64  = *( (short *)		inValue );
	else if( inType == kCFLNumberIntType )		object->value.s64  = *( (int *)			inValue );
	else if( inType == kCFLNumberLongType )		object->value.s64  = *( (long *)		inValue );
	else if( inType == kCFLNumberLongLongType )	object->value.s64  = *( (long long *)	inValue );
	else if( inType == kCFLNumberCFIndexType )	object->value.s64  = *( (CFLIndex *)	inValue );
#if( CFL_FLOATING_POINT_NUMBERS )
	else if( inType == kCFLNumberFloat32Type )	object->value.f64  = *( (Float32 *)		inValue );
	else if( inType == kCFLNumberFloat64Type )	object->value.f64  = *( (Float64 *)		inValue );
	else if( inType == kCFLNumberFloatType )	object->value.f64  = *( (float *)		inValue );
	else if( inType == kCFLNumberDoubleType )	object->value.f64  = *( (double *)		inValue );
#endif
	else { dlogassert( "bad number type %d", inType ); err = kParamErr; goto exit; }
	
	// Success!
	
	*outRef = (CFLNumberRef) object;
	object = NULL;
	err = kNoErr;
	
exit:
	if( object ) CFLRelease( (CFLNumberRef) object );
	return( err );
}

//===========================================================================================================================
//	CFLNumberGetByteSize
//===========================================================================================================================

CFLIndex	CFLNumberGetByteSize( CFLNumberRef inObject )
{
	require( CFLValidObjectType( inObject, kCFLTypeNumber ), exit );
	switch( ( (CFLNumber *) inObject )->type )
	{
		case kCFLNumberSInt8Type:		return( (CFLIndex) sizeof( int8_t ) );
		case kCFLNumberSInt16Type:		return( (CFLIndex) sizeof( int16_t ) );
		case kCFLNumberSInt32Type:		return( (CFLIndex) sizeof( int32_t ) );
		case kCFLNumberSInt64Type:		return( (CFLIndex) sizeof( int64_t ) );
		case kCFLNumberSInt128Type:		return( (CFLIndex) sizeof( int128_compat ) );
		case kCFLNumberCharType:		return( (CFLIndex) sizeof( char ) );
		case kCFLNumberShortType:		return( (CFLIndex) sizeof( short ) );
		case kCFLNumberIntType:			return( (CFLIndex) sizeof( int ) );
		case kCFLNumberLongType:		return( (CFLIndex) sizeof( long ) );
		case kCFLNumberLongLongType:	return( (CFLIndex) sizeof( long long ) );
		case kCFLNumberCFIndexType:		return( (CFLIndex) sizeof( CFLIndex ) );
		#if( CFL_FLOATING_POINT_NUMBERS )
		case kCFLNumberFloat32Type:		return( (CFLIndex) sizeof( Float32 ) );
		case kCFLNumberFloat64Type:		return( (CFLIndex) sizeof( Float64 ) );
		case kCFLNumberFloatType:		return( (CFLIndex) sizeof( float ) );
		case kCFLNumberDoubleType:		return( (CFLIndex) sizeof( double ) );
		#endif
		
		default:
			dlogassert( "internal bad number type %d", ( (CFLNumber *) inObject )->type );
			goto exit;
	}
	
exit:
	return( 0 );
}

//===========================================================================================================================
//	CFLNumberGetType
//===========================================================================================================================

CFLNumberType	CFLNumberGetType( CFLNumberRef inObject )
{
	require( CFLValidObjectType( inObject, kCFLTypeNumber ), exit );
	return( ( (CFLNumber *) inObject )->type );
	
exit:
	return( kCFLNumberInvalidType );
}

//===========================================================================================================================
//	CFLNumberGetValue
//===========================================================================================================================

OSStatus	CFLNumberGetValue( CFLNumberRef inObject, CFLNumberType inType, void *outValue )
{
	OSStatus		err;
	CFLNumber *		object;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeNumber ), exit, err = kBadReferenceErr );
	object = (CFLNumber *) inObject;
	
	switch( object->type )
	{
		case kCFLNumberSInt8Type:
		case kCFLNumberSInt16Type:
		case kCFLNumberSInt32Type:
		case kCFLNumberSInt64Type:
		case kCFLNumberCharType:
		case kCFLNumberShortType:
		case kCFLNumberIntType:
		case kCFLNumberLongType:
		case kCFLNumberLongLongType:
		case kCFLNumberCFIndexType:
			if(      inType == kCFLNumberSInt8Type )	*( (int8_t *)		outValue ) = (int8_t)	object->value.s64;
			else if( inType == kCFLNumberSInt16Type )	*( (int16_t *)		outValue ) = (int16_t)	object->value.s64;
			else if( inType == kCFLNumberSInt32Type )	*( (int32_t *)		outValue ) = (int32_t)	object->value.s64;
			else if( inType == kCFLNumberSInt64Type )	*( (int64_t *)		outValue ) = (int64_t)	object->value.s64;
			else if( inType == kCFLNumberSInt128Type )
			{
				int128_compat * const		p128 = (int128_compat *) outValue;
				
				p128->hi = ( object->value.s64 < 0 ) ? -1 : 0;
				p128->lo = (uint64_t) object->value.s64;
			}
			else if( inType == kCFLNumberCharType )		*( (char *)			outValue ) = (char)		object->value.s64;
			else if( inType == kCFLNumberShortType )	*( (short *)		outValue ) = (short)	object->value.s64;
			else if( inType == kCFLNumberIntType )		*( (int *)			outValue ) = (int)		object->value.s64;
			else if( inType == kCFLNumberLongType )		*( (long *)			outValue ) = (long)		object->value.s64;
			else if( inType == kCFLNumberLongLongType )	*( (long long *)	outValue ) =			object->value.s64;
			else if( inType == kCFLNumberCFIndexType )	*( (CFLIndex *)		outValue ) = (CFLIndex)	object->value.s64;
		#if( CFL_FLOATING_POINT_NUMBERS )
			else if( inType == kCFLNumberFloat32Type )	*( (Float32 *)		outValue ) = (Float32)	object->value.s64;
			else if( inType == kCFLNumberFloat64Type )	*( (Float64 *)		outValue ) = (Float64)	object->value.s64;
			else if( inType == kCFLNumberFloatType )	*( (float *)		outValue ) = (float)	object->value.s64;
			else if( inType == kCFLNumberDoubleType )	*( (double *)		outValue ) = (double)	object->value.s64;
		#endif
			else { dlogassert( "bad number type %d", inType ); err = kParamErr; goto exit; }
			break;
		
		case kCFLNumberSInt128Type:
			if(      inType == kCFLNumberSInt8Type )	*( (int8_t *)			outValue ) = (int8_t)		object->value.s128.lo;
			else if( inType == kCFLNumberSInt16Type )	*( (int16_t *)			outValue ) = (int16_t)		object->value.s128.lo;
			else if( inType == kCFLNumberSInt32Type )	*( (int32_t *)			outValue ) = (int32_t)		object->value.s128.lo;
			else if( inType == kCFLNumberSInt64Type )	*( (int64_t *)			outValue ) = (int64_t)		object->value.s128.lo;
			else if( inType == kCFLNumberSInt128Type )	*( (int128_compat *)	outValue ) = object->value.s128;
			else if( inType == kCFLNumberCharType )		*( (char *)				outValue ) = (char)			object->value.s128.lo;
			else if( inType == kCFLNumberShortType )	*( (short *)			outValue ) = (short)		object->value.s128.lo;
			else if( inType == kCFLNumberIntType )		*( (int *)				outValue ) = (int)			object->value.s128.lo;
			else if( inType == kCFLNumberLongType )		*( (long *)				outValue ) = (long)			object->value.s128.lo;
			else if( inType == kCFLNumberLongLongType )	*( (long long *)		outValue ) = (long long)	object->value.s128.lo;
			else if( inType == kCFLNumberCFIndexType )	*( (CFLIndex *)			outValue ) = (CFLIndex)		object->value.s128.lo;
			#if( CFL_FLOATING_POINT_NUMBERS )
			else if( CFLNumberTypeIsFloatType( inType ) )
			{
				int128_compat	s128;
				Float64			f64;
				
				if( object->value.s128.hi < 0 )
				{
					s128.lo = ~object->value.s128.lo + 1;
					s128.hi = ~object->value.s128.hi + ( s128.lo == 0 );
					f64 = -( ( ( (Float64) s128.hi ) * 18446744073709551616.0 ) + ( (Float64) s128.lo ) );
				}
				else
				{
					f64 = ( ( (Float64) object->value.s128.hi ) * 18446744073709551616.0 ) + ( (Float64) object->value.s128.lo );
				}
				if(      inType == kCFLNumberFloat32Type )	*( (Float32 *)	outValue ) = (Float32)	f64;
				else if( inType == kCFLNumberFloat64Type )	*( (Float64 *)	outValue ) = (Float64)	f64;
				else if( inType == kCFLNumberFloatType )	*( (float *)	outValue ) = (float)	f64;
				else if( inType == kCFLNumberDoubleType )	*( (double *)	outValue ) = (double)	f64;
				else { dlogassert( "bad number type %d", inType ); err = kParamErr; goto exit; }
			}
			#endif
			else { dlogassert( "bad number type %d", inType ); err = kParamErr; goto exit; }
			break;
		
		#if( CFL_FLOATING_POINT_NUMBERS )
		case kCFLNumberFloat32Type:
		case kCFLNumberFloat64Type:
		case kCFLNumberFloatType:
		case kCFLNumberDoubleType:
			if(      inType == kCFLNumberSInt8Type )	*( (int8_t *)		outValue ) = (int8_t)	object->value.f64;
			else if( inType == kCFLNumberSInt16Type )	*( (int16_t *)		outValue ) = (int16_t)	object->value.f64;
			else if( inType == kCFLNumberSInt32Type )	*( (int32_t *)		outValue ) = (int32_t)	object->value.f64;
			else if( inType == kCFLNumberSInt64Type )	*( (int64_t *)		outValue ) = (int64_t)	object->value.f64;
			else if( inType == kCFLNumberSInt128Type )
			{
				int128_compat * const		p128 = (int128_compat *) outValue;
				Float64						f64;
				
				if( object->value.f64 < -170141183460469231731687303715884105728.0 ) // -1*10^127
				{
					p128->hi = INT64_MIN;
					p128->lo = 0;
				}
				else if( object->value.f64 >= 170141183460469231731687303715884105728.0 ) // 1*10^127
				{
					p128->hi = INT64_C( 0x7FFFFFFFFFFFFFFF );
					p128->lo = UINT64_C( 0xFFFFFFFFFFFFFFFF );
				}
				else
				{
					f64 = floor( object->value.f64 / 18446744073709551616.0 ); // 2^64
					p128->hi = (int64_t) f64;
					p128->lo = (uint64_t)( object->value.f64 - ( f64 * 18446744073709551616.0 ) );
				}
			}
			else if( inType == kCFLNumberCharType )		*( (char *)			outValue ) = (char)		object->value.f64;
			else if( inType == kCFLNumberShortType )	*( (short *)		outValue ) = (short)	object->value.f64;
			else if( inType == kCFLNumberIntType )		*( (int *)			outValue ) = (int)		object->value.f64;
			else if( inType == kCFLNumberLongType )		*( (long *)			outValue ) = (long)		object->value.f64;
			else if( inType == kCFLNumberLongLongType )	*( (long long *)	outValue ) = (long long) object->value.f64;
			else if( inType == kCFLNumberCFIndexType )	*( (CFLIndex *)		outValue ) = (CFLIndex)	object->value.f64;
			else if( inType == kCFLNumberFloat32Type )	*( (Float32 *)		outValue ) = (Float32)	object->value.f64;
			else if( inType == kCFLNumberFloat64Type )	*( (Float64 *)		outValue ) =			object->value.f64;
			else if( inType == kCFLNumberFloatType )	*( (float *)		outValue ) = (float)	object->value.f64;
			else if( inType == kCFLNumberDoubleType )	*( (double *)		outValue ) = (double)	object->value.f64;
			else { dlogassert( "bad number type %d", inType ); err = kParamErr; goto exit; }
			break;
		#endif
		
		default:
			dlogassert( "internal bad number type %d", inType );
			err = kInternalErr;
			goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLNumberIsFloatType
//===========================================================================================================================

Boolean	CFLNumberIsFloatType( CFLNumberRef inNumber )
{
	CFLNumber *		object;
	
	require( CFLValidObjectType( inNumber, kCFLTypeNumber ), exit );
	object = (CFLNumber *) inNumber;
	
	return( CFLNumberTypeIsFloatType( object->type ) );
	
exit:
	return( false );
}

//===========================================================================================================================
//	CFLNumberCompare
//===========================================================================================================================

int	CFLNumberCompare( CFLNumberRef inLeft, CFLNumberRef inRight )
{
	const CFLNumber * const		left  = (const CFLNumber *) inLeft;
	const CFLNumber * const		right = (const CFLNumber *) inRight;
	int							result;
	Value64						leftValue, rightValue;
	
	require_action( CFLValidObjectType( inLeft,  kCFLTypeNumber ), exit, result = -1 );
	require_action( CFLValidObjectType( inRight, kCFLTypeNumber ), exit, result = 1 );
	
	// $$$ TO DO: Handle 128-bit integers (tricky when one is a float and one is an int128_t).
	
	if( CFLNumberTypeIsFloatType( left->type ) || CFLNumberTypeIsFloatType( right->type ) )
	{
		CFLNumberGetValue( inLeft,  kCFLNumberFloat64Type, &leftValue.f64 );
		CFLNumberGetValue( inRight, kCFLNumberFloat64Type, &rightValue.f64 );
		result = ( leftValue.f64 > rightValue.f64 ) - ( leftValue.f64 < rightValue.f64 );
	}
	else
	{
		CFLNumberGetValue( inLeft,  kCFLNumberSInt64Type, &leftValue.s64 );
		CFLNumberGetValue( inRight, kCFLNumberSInt64Type, &rightValue.s64 );
		result = ( leftValue.s64 > rightValue.s64 ) - ( leftValue.s64 < rightValue.s64 );
	}
	
exit:
	return( result );
}

//===========================================================================================================================
//	_CFLNumberEqual
//===========================================================================================================================

static Boolean	_CFLNumberEqual( CFLObjectRef inLeft, CFLObjectRef inRight )
{
	CFLNumber *			l;
	CFLNumber *			r;
	int128_compat		s128;
	
	// The upper-level CFLEqual routine will have already validated the objects and performed pointer equality testing 
	// so it is unnecessary here because this should never get called unless those tests failed. Just assert instead.
	
	check( CFLValidObjectType( inLeft, kCFLTypeNumber ) );
	check( CFLValidObjectType( inRight, kCFLTypeNumber ) );
	check_string( inLeft != inRight, "object ptrs match...should have passed earlier equality testing" );
	l = (CFLNumber *) inLeft;
	r = (CFLNumber *) inRight;
	
	// Equal values means equal objects.
	
#if( CFL_FLOATING_POINT_NUMBERS )
	if( !CFLNumberTypeIsFloatType( l->type ) )
	{
		if( CFLNumberTypeIsFloatType( r->type ) )
		{
			return( false );
		}
		if( ( l->type == kCFLNumberSInt128Type ) == ( r->type == kCFLNumberSInt128Type ) )
		{
			return( ( l->value.s128.hi == r->value.s128.hi ) && ( l->value.s128.lo == r->value.s128.lo ) );
		}
		else if( l->type == kCFLNumberSInt128Type )
		{
			s128.hi = ( r->value.s64 < 0 ) ? -1 : 0;
			s128.lo = (uint64_t) r->value.s64;
			return( ( l->value.s128.hi == s128.hi ) && ( l->value.s128.lo == s128.lo ) );
		}
		else if( r->type == kCFLNumberSInt128Type )
		{
			s128.hi = ( l->value.s64 < 0 ) ? -1 : 0;
			s128.lo = (uint64_t) l->value.s64;
			return( ( s128.hi == r->value.s128.hi ) && ( s128.lo == r->value.s128.lo ) );
		}
		return( (Boolean)( l->value.s64 == r->value.s64 ) );
	}
	else
	{
		if( !CFLNumberTypeIsFloatType( r->type ) )
		{
			return( false );
		}
		return( (Boolean)( l->value.f64 == r->value.f64 ) );
	}
#else
	if( ( l->type == kCFLNumberSInt128Type ) == ( r->type == kCFLNumberSInt128Type ) )
	{
		return( ( l->value.s128.hi == r->value.s128.hi ) && ( l->value.s128.lo == r->value.s128.lo ) );
	}
	else if( l->type == kCFLNumberSInt128Type )
	{
		s128.hi = ( r->value.s64 < 0 ) ? -1 : 0;
		s128.lo = (uint64_t) r->value.s64;
		return( ( l->value.s128.hi == s128.hi ) && ( l->value.s128.lo == s128.lo ) );
	}
	else if( r->type == kCFLNumberSInt128Type )
	{
		s128.hi = ( l->value.s64 < 0 ) ? -1 : 0;
		s128.lo = (uint64_t) l->value.s64;
		return( ( s128.hi == r->value.s128.hi ) && ( s128.lo == r->value.s128.lo ) );
	}
	return( (Boolean)( l->value.s64 == r->value.s64 ) );
#endif
}

//===========================================================================================================================
//	_CFLNumberHash
//===========================================================================================================================

static CFLHashCode	_CFLNumberHash( CFLObjectRef inObject )
{
	CFLNumber *		object;
	uint64_t		hash;
	
	// The upper-level CFLHash routine will have already validated parameters so just assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeNumber ) );
	object = (CFLNumber *) inObject;
	
	if( object->type == kCFLNumberSInt128Type )
	{
		hash = _CFLHashFNV1a( &object->value.s128, sizeof( object->value.s128 ) );
	}
	else
	{
		// 64-bit integer to 32-bit hash based on code from Thomas Wang. See <http://www.cris.com/~Ttwang/tech/inthash.htm>.
	
		hash = object->value.u64; // Also works for floating point types because value is a union.
		hash = ( hash << 18 ) - hash - 1;
		hash =   hash ^ ( hash >> 31 );
		hash = ( hash + ( hash <<  2 ) ) + ( hash << 4 );
		hash =   hash ^ ( hash >> 11 );
		hash =   hash + ( hash <<  6 );
		hash =   hash ^ ( hash >> 22 );
	}
	return( (CFLHashCode)( hash & UINT64_C( 0xFFFFFFFF ) ) );
}

//===========================================================================================================================
//	_CFLNumberFree
//===========================================================================================================================

static void	_CFLNumberFree( CFLObjectRef inObject )
{
	DEBUG_USE_ONLY( inObject );
	
	// The upper-level CFLRelease/etc routine will have already validated parameters so just assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeNumber ) );
}

//===========================================================================================================================
//	_CFLNumberCopy
//===========================================================================================================================

static OSStatus	_CFLNumberCopy( CFLNumberRef inSrc, CFLNumberRef *outDst )
{
	OSStatus		err;
	CFLNumber *		srcObject;
	CFLNumber *		object;
	
	require_action( CFLValidObjectType( inSrc, kCFLTypeNumber ), exit, err = kTypeErr );
	require_action( outDst, exit, err = kParamErr );
	srcObject = (CFLNumber *) inSrc;
	
	object = (CFLNumber *) calloc( 1U, sizeof( CFLNumber ) );
	require_action( object, exit, err = kNoMemoryErr );
	
	object->base.signature		= kCFLSignatureValid;
	object->base.type			= kCFLTypeNumber;
	object->base.retainCount	= 1;
	object->type				= srcObject->type;
	object->value				= srcObject->value;
	
	*outDst = (CFLNumberRef) object;
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == String ==
#endif

//===========================================================================================================================
//	CFLStringGetTypeID
//===========================================================================================================================

CFLTypeID	CFLStringGetTypeID( void )
{
	return( kCFLTypeString );
}

//===========================================================================================================================
//	CFLStringCreateWithText
//===========================================================================================================================

OSStatus
	CFLStringCreateWithText( 
		CFLAllocatorRef 	inAllocator, 
		const void *		inText, 
		size_t 				inTextSize, 
		CFLStringRef *		outRef )
{
	OSStatus			err;
	CFLString *			object;
	
	object = NULL;
	require_action( inAllocator == kCFLAllocatorDefault, exit, err = kParamErr );
	require_action( ( inTextSize == 0 ) || inText, exit, err = kParamErr );
	require_action( outRef, exit, err = kParamErr );
	
	// Allocate and initialize the String.
	
	object = (CFLString *) calloc( 1U, sizeof( CFLString ) );
	require_action( object, exit, err = kNoMemoryErr );
	
	object->base.signature		= kCFLSignatureValid;
	object->base.type			= kCFLTypeString;
	object->base.retainCount	= 1;
	
	// Set the string to the input text (even if null/empty).
	
	err = CFLStringSetText( (CFLStringRef) object, inText, inTextSize );
	require_noerr( err, exit );
	
	// Success!
	
	*outRef = (CFLStringRef) object;
	object = NULL;
	err = kNoErr;
	
exit:
	if( object ) CFLRelease( (CFLStringRef) object );
	return( err );
}

//===========================================================================================================================
//	CFLStringGetLength
//===========================================================================================================================

OSStatus	CFLStringGetLength( CFLStringRef inObject, CFLIndex *outLength )
{
	OSStatus		err;
	CFLString *		object;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeString ), exit, err = kBadReferenceErr );
	object = (CFLString *) inObject;
	
	if( CFLIsConstantObject( inObject ) )
	{
		if( outLength ) *outLength = (CFLIndex) strlen( CFLGetConstantStringPtr( object ) );
	}
	else
	{
		if( outLength ) *outLength = (CFLIndex) object->size;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLStringGetCStringPtr
//===========================================================================================================================

OSStatus	CFLStringGetCStringPtr( CFLStringRef inObject, const char **outCString, size_t *outSize )
{
	OSStatus		err;
	CFLString *		object;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeString ), exit, err = kBadReferenceErr );
	object = (CFLString *) inObject;
	
	if( CFLIsConstantObject( inObject ) )
	{
		const char *		s;
		
		s = CFLGetConstantStringPtr( object );
		if( outCString )	*outCString = s;
		if( outSize )		*outSize	= strlen( s );
	}
	else
	{
		check( object->data );
		if( outCString ) 	*outCString = object->data;
		if( outSize )		*outSize	= object->size;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLStringSetText
//===========================================================================================================================

OSStatus	CFLStringSetText( CFLStringRef inObject, const void *inText, size_t inSize )
{
	OSStatus		err;
	CFLString *		object;
	char *			s;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeString ), exit, err = kBadReferenceErr );
	require_action( !CFLIsConstantObject( inObject ), exit, err = kImmutableErr );
	object = (CFLString *) inObject;
	require_action( ( inSize == 0 ) || inText, exit, err = kParamErr );
	if( inSize == kSizeCString )	inSize = strlen( (const char *) inText );
	else							inSize = _CFLstrnlen( (const char *) inText, inSize );
	
	// Allocate the new string and copy the source string to it.
	
	s = (char *) malloc( inSize + 1 );
	require_action( s, exit, err = kNoMemoryErr );
	
	if( inSize > 0 ) memcpy( s, inText, inSize );
	s[ inSize ] = '\0';
	
	// Replace the old string with the new string.
	
	if( object->data ) free( object->data );
	object->data = s;
	object->size = inSize;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLStringAppendText
//===========================================================================================================================

OSStatus	CFLStringAppendText( CFLStringRef inObject, const void *inText, size_t inSize )
{
	OSStatus		err;
	CFLString *		obj;
	size_t			newSize;
	char *			tmp;
	
	require_action( CFLValidObjectType( inObject, kCFLTypeString ), exit, err = kBadReferenceErr );
	require_action( !CFLIsConstantObject( inObject ), exit, err = kImmutableErr );
	obj = (CFLString *) inObject;
	require_action( inText || ( inSize == 0 ), exit, err = kParamErr );
	if( inSize == kSizeCString )	inSize = strlen( (const char *) inText );
	else							inSize = _CFLstrnlen( (const char *) inText, inSize );
	newSize = obj->size + inSize;
	
#if( TARGET_NO_REALLOC )
	tmp = (char *) _CFLrealloc( obj->data, obj->size, newSize + 1 );
	require_action( tmp, exit, err = kNoMemoryErr );
#else
	tmp = (char *) realloc( obj->data, newSize + 1 );
	require_action( tmp, exit, err = kNoMemoryErr );
#endif
	
	obj->data = tmp;
	if( inText && ( inSize > 0 ) ) memcpy( obj->data + obj->size, inText, inSize );
	obj->data[ newSize ] = '\0';
	obj->size = newSize;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CFLStringEqual
//===========================================================================================================================

static Boolean	_CFLStringEqual( CFLObjectRef inLeft, CFLObjectRef inRight )
{
	OSStatus		err;
	CFLString *		l;
	CFLString *		r;
	int				lConst;
	int				rConst;
	
	// The upper-level CFLEqual routine will have already validated the objects and performed pointer equality testing 
	// so it is unnecessary here because this should never get called unless those tests failed. Just assert instead.
	
	check( CFLValidObjectType( inLeft, kCFLTypeString ) );
	check( CFLValidObjectType( inRight, kCFLTypeString ) );
	check_string( inLeft != inRight, "object ptrs match...should have passed earlier equality testing" );
	l = (CFLString *) inLeft;
	r = (CFLString *) inRight;
	lConst = CFLIsConstantObject( inLeft );
	rConst = CFLIsConstantObject( inRight );
	
	// If one of the objects is constant then its length will not be valid so we have to strcmp to detect equality.
	
	if( lConst && rConst )	// Both Const
	{
		if( strcmp( CFLGetConstantStringPtr( l ), CFLGetConstantStringPtr( r ) ) != 0 )
		{
			err = kMismatchErr;
			goto exit;
		}
	}
	else if( lConst )		// Left Const, Right Non-Const
	{
		check( r->data );
		if( strcmp( CFLGetConstantStringPtr( l ), r->data ) != 0 )
		{
			err = kMismatchErr;
			goto exit;
		}
	}
	else if( rConst )		// Right Const, Left Non-Const
	{
		check( l->data );
		if( strcmp( l->data, CFLGetConstantStringPtr( r ) ) != 0 )
		{
			err = kMismatchErr;
			goto exit;
		}
	}
	else					// Both Non-Const
	{
		// Different length means different objects.
		
		if( l->size != r->size )
		{
			err = kMismatchErr;
			goto exit;
		}
		
		// Different content means different objects.
		
		check( l->data );
		check( r->data );
		if( strcmp( l->data, r->data ) != 0 )
		{
			err = kMismatchErr;
			goto exit;
		}
	}
	
	// All equality tests passed. Objects are equal.
	
	err = kNoErr;
	
exit:
	return( (Boolean)( err == kNoErr ) );
}

//===========================================================================================================================
//	_CFLStringHash
//===========================================================================================================================

static CFLHashCode	_CFLStringHash( CFLObjectRef inObject )
{
	CFLString *		object;
	
	// The upper-level CFLHash routine will have already validated parameters so just assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeString ) );
	object = (CFLString *) inObject;
	
	return( _CFLHashFNV1a( CFLIsConstantObject( inObject ) ? CFLGetConstantStringPtr( inObject ) : object->data, kSizeCString ) );
}

//===========================================================================================================================
//	_CFLStringFree
//===========================================================================================================================

static void	_CFLStringFree( CFLObjectRef inObject )
{
	CFLString *		object;
	
	// The upper-level CFLRelease/etc routine will have already validated parameters so just assert here.
	
	check( CFLValidObjectType( inObject, kCFLTypeString ) );
	check_string( !CFLIsConstantObject( inObject ), "constant objects should never get freed" );
	object = (CFLString *) inObject;
	
	// Free the memory used by the string.
	
	if( object->data ) free( object->data );
}

//===========================================================================================================================
//	_CFLStringCopy
//===========================================================================================================================

static OSStatus	_CFLStringCopy( CFLStringRef inSrc, CFLStringRef *outDst )
{
	OSStatus			err;
	const char *		p;
	size_t				n;
	
	check( outDst );
	
	err = CFLStringGetCStringPtr( inSrc, &p, &n );
	require_noerr( err, exit );
	
	err = CFLStringCreateWithText( kCFLAllocatorDefault, p, n, outDst );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLStringFindAndReplace
//===========================================================================================================================

#if( TARGET_HAS_STD_C_LIB )
CFLIndex
	CFLStringFindAndReplace( 
		CFLStringRef	inString, 
		CFLStringRef	inStringToFind, 
		CFLStringRef	inReplacementString, 
		CFLIndex		inLocation, 
		CFLIndex		inLength, 
		uint32_t		inCompareOptions )
{
	OSStatus			err;
	CFLDataRef			tempBuf = NULL;
	void *				tempBufPtr;
	size_t				tempBufLen;
	CFLIndex			finalReplaceCount = 0;
	CFLIndex			replaceCount;
	const char *		string;
	size_t				stringLen;
	const char *		stringToFind;
	size_t				stringToFindLen;
	const char *		replacementString;
	size_t				replacementStringLen;
	const char *		pos;
	
	require( inLocation == 0, exit );		// No sub-string searching yet.
	require( inCompareOptions == 0, exit );	// We don't support any options yet.
	
	err = CFLDataCreate( kCFLAllocatorDefault, NULL, 0, &tempBuf );
	require_noerr( err, exit );
	
	err = CFLStringGetCStringPtr( inString, &string, &stringLen );
	require_noerr( err, exit );
	require( inLength == (CFLIndex) stringLen, exit ); // No sub-string searching yet.
	
	err = CFLStringGetCStringPtr( inStringToFind, &stringToFind, &stringToFindLen );
	require_noerr( err, exit );
	
	err = CFLStringGetCStringPtr( inReplacementString, &replacementString, &replacementStringLen );
	require_noerr( err, exit );
	
	replaceCount = 0;
	pos = string;
	for( ;; )
	{
		const char *	nextPos;
		
		nextPos = strstr( pos, stringToFind );
		if( nextPos == NULL )
		{
			// Not found so append everything remaining of the original to the new string and we're done.
			
			err = CFLDataAppendData( tempBuf, pos, strlen( pos ) );
			require_noerr( err, exit );
			break;
		}
		else
		{
			size_t		len;
			
			len = (size_t)( nextPos - pos );
			err = CFLDataAppendData( tempBuf, pos, len );
			require_noerr( err, exit );
			pos += len;

			err = CFLDataAppendData( tempBuf, replacementString, replacementStringLen );
			require_noerr( err, exit );
			pos += stringToFindLen;

			++replaceCount;
		}
	}
	
	err = CFLDataGetDataPtr( tempBuf, &tempBufPtr, &tempBufLen );
	require_noerr( err, exit );
	
	err = CFLStringSetText( inString, tempBufPtr, tempBufLen );
	require_noerr( err, exit );
	
	finalReplaceCount = replaceCount;
	
exit:
	if( tempBuf ) CFLRelease( tempBuf );
	return( finalReplaceCount );
}
#endif // TARGET_HAS_STD_C_LIB

#if 0
#pragma mark -
#pragma mark == Runtime ==
#endif

//===========================================================================================================================
//	CFLRuntimeFinalize
//===========================================================================================================================

void	CFLRuntimeFinalize( void )
{
	gCFLRuntimeClassTable = kCFLRuntimeClassTable;
	ForgetMem( &gCFLRuntimeClassTableStorage );
	gCFLRuntimeClassTableCount = countof( kCFLRuntimeClassTable );
}

//===========================================================================================================================
//	CFLRuntimeRegisterClass
//===========================================================================================================================

OSStatus	CFLRuntimeRegisterClass( const CFLRuntimeClass * const inClass, CFLTypeID *outTypeID )
{
	OSStatus				err;
	CFLRuntimeClass *		oldClassArray;
	CFLRuntimeClass *		newClassArray;
	size_t					n;
	
	atomic_spinlock_lock( &gCFLRuntimeClassSpinLock );
	
	n = gCFLRuntimeClassTableCount;
	newClassArray = (CFLRuntimeClass *) malloc( ( n + 1 ) * sizeof( *newClassArray ) );
	require_action( newClassArray, exit, err = kNoMemoryErr );
	memcpy( newClassArray, gCFLRuntimeClassTable, n * sizeof( *gCFLRuntimeClassTable ) );
	newClassArray[ n ] = *inClass;
	
	oldClassArray					= gCFLRuntimeClassTableStorage;
	gCFLRuntimeClassTableStorage	= newClassArray;
	gCFLRuntimeClassTable			= newClassArray;
	atomic_read_write_barrier();
	gCFLRuntimeClassTableCount		= n + 1;
	
	if( oldClassArray ) free( oldClassArray );
	*outTypeID = (CFLTypeID) n;
	err = kNoErr;
	
exit:
	atomic_spinlock_unlock( &gCFLRuntimeClassSpinLock );
	return( err );
}

//===========================================================================================================================
//	CFLRuntimeCreateInstance
//===========================================================================================================================

OSStatus	CFLRuntimeCreateInstance( CFLAllocatorRef inAllocator, CFLTypeID inTypeID, size_t inExtraBytes, void *outObj )
{
	OSStatus		err;
	CFLObject *		obj;
	
	require_action( inAllocator == kCFLAllocatorDefault, exit, err = kParamErr );
	require_action( inTypeID < gCFLRuntimeClassTableCount, exit, err = kParamErr );
	
	obj = (CFLObject *) malloc( sizeof( CFLObject ) + inExtraBytes );
	require_action( obj, exit, err = kNoMemoryErr );
	
	obj->signature		= kCFLSignatureValid;
	obj->type			= (uint8_t) inTypeID;
	obj->flags			= 0;
	obj->pad			= 0;
	obj->retainCount	= 1;
	
	*( (CFLObject **) outObj ) = obj;
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Utilities ==
#endif

//===========================================================================================================================
//	_CFLHashFNV1a
//
//	32-bit Fowler/Noll/Vo (FNV-1a) hash based on code from <http://www.isthe.com/chongo/tech/comp/fnv/>.
//===========================================================================================================================

static CFLHashCode	_CFLHashFNV1a( const void *inPtr, size_t inLen )
{
	const uint8_t * const		ptr = (const uint8_t *) inPtr;
	CFLHashCode					hash;
	size_t						i;
	uint8_t						b;
	
	hash = UINT32_C( 0x811c9dc5 );
	if( inLen == kSizeCString )
	{
		for( i = 0; ( b = ptr[ i ] ) != '\0'; ++i )
		{
			hash ^= b;
			hash *= UINT32_C( 0x01000193 );
		}
	}
	else
	{
		for( i = 0; i < inLen; ++i )
		{
			hash ^= ptr[ i ];
			hash *= UINT32_C( 0x01000193 );
		}
	}
	return( hash );
}

//===========================================================================================================================
//	_CFLNextPrime
//
//	Based on prime number code from snippets.org.
//===========================================================================================================================

static CFLIndex	_CFLNextPrime( CFLIndex inValue )
{
	CFLIndex		i;
	
	if( inValue < 2 )
	{
		inValue = 1;
	}
	else if( inValue < 4 )
	{
		inValue = 3;
	}
	else
	{
		if( ( inValue % 2 ) == 0 )
		{
			inValue += 1;
		}
		for( ;; )
		{
			i = 3;
			for( ;; )
			{
				if( ( inValue % i ) == 0 ) break;
				if( ( inValue / i ) < i )  goto exit;
				i += 2;
			}
			inValue += 2;
		}
	}

exit:
	return( inValue );
}

//===========================================================================================================================
//	_CFLQSortPtrs
//
//	QuickSort code derived from the simple quicksort code from the book "The Practice of Programming".
//===========================================================================================================================

static void	_CFLQSortPtrs( void *inPtrArray, size_t inPtrCount, CFLComparatorFunction inCmp, void *inContext )
{
	void ** const		ptrArray = (void **) inPtrArray;
	void *				t;
	size_t				i, last;
	
	if( inPtrCount <= 1 )
		return;
	
	i = Random32() % inPtrCount;
	t = ptrArray[ 0 ];
	ptrArray[ 0 ] = ptrArray[ i ];
	ptrArray[ i ] = t;
	
	last = 0;
	for( i = 1; i < inPtrCount; ++i )
	{
		if( inCmp( ptrArray[ i ], ptrArray[ 0 ], inContext ) < 0 )
		{
			t = ptrArray[ ++last ];
			ptrArray[ last ] = ptrArray[ i ];
			ptrArray[ i ] = t;
		}
	}
	t = ptrArray[ 0 ];
	ptrArray[ 0 ] = ptrArray[ last ];
	ptrArray[ last ] = t;
	
	_CFLQSortPtrs( ptrArray, last, inCmp, inContext );
	_CFLQSortPtrs( &ptrArray[ last + 1 ], ( inPtrCount - last ) - 1, inCmp, inContext );
}

//===========================================================================================================================
//	_CFLrealloc
//===========================================================================================================================

#if( TARGET_NO_REALLOC )
static void *	_CFLrealloc( void *inMem, size_t inOldSize, size_t inNewSize )
{
	void *		mem;
	
	mem = malloc( inNewSize );
	require( mem, exit );
	
	if( inMem )
	{
		memcpy( mem, inMem, inOldSize );
		free( inMem );
	}
	
exit:
	return( mem );
}
#endif

//===========================================================================================================================
//	_CFLstrnlen
//
//	Like the ANSI C strlen routine, but allows you to specify a maximum size.
//===========================================================================================================================

static size_t	_CFLstrnlen( const char *inString, size_t inMax )
{
	const char *		src;
	const char *		end;
	
	src = inString;
	end = src + inMax;
	while( ( src < end ) && ( *src != '\0' ) ) ++src;
	return( (size_t)( src - inString ) );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	CFLRandomString
//===========================================================================================================================

#define kCFLRandomCharSet		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

static char *	CFLRandomString( size_t inMinChars, size_t inMaxChars, char *outString )
{
	char *		ptr;
	char *		end;
	
	check( inMinChars <= inMaxChars );
	
	ptr = outString;
	end = ptr + ( inMinChars + ( Random32() % ( ( inMaxChars - inMinChars ) + 1 ) ) );
	while( ptr < end ) *ptr++ = kCFLRandomCharSet[ Random32() % sizeof_string( kCFLRandomCharSet ) ];
	*ptr = '\0';
	return( outString );
}

//===========================================================================================================================
//	CFLDictionaryStats
//===========================================================================================================================

static void	CFLDictionaryStats( CFLDictionaryRef inDictionary, int *outMinDepth, int *outMaxDepth );

static void	CFLDictionaryStats( CFLDictionaryRef inDictionary, int *outMinDepth, int *outMaxDepth )
{
	CFLDictionary *			dict;
	CFLIndex				i;
	CFLIndex				n;
	int						minDepth;
	int						maxDepth;
	int						depth;
	CFLDictionaryNode *		node;
	
	check( inDictionary );
	
	dict = (CFLDictionary *) inDictionary;
	minDepth = 0x7FFFFFFF;
	maxDepth = 0;
	n = dict->bucketCount;
	for( i = 0; i < n; ++i )
	{
		depth = 0;
		for( node = dict->buckets[ i ]; node; node = node->next )
		{
			++depth;
		}
		if( depth > maxDepth ) maxDepth = depth;
		if( depth < minDepth ) minDepth = depth;
	}
	if( outMinDepth ) *outMinDepth = minDepth;
	if( outMaxDepth ) *outMaxDepth = maxDepth;
}

//===========================================================================================================================
//	CFLiteRuntimeClassesTest
//===========================================================================================================================

static const CFLRuntimeClass		kCFLiteTestClass = 
{
	"CFLiteTestClass", 
	0, 
	0, 
	0
};

OSStatus	CFLiteRuntimeClassesTest( void );

OSStatus	CFLiteRuntimeClassesTest( void )
{
	OSStatus			err;
	CFLTypeID			typeID;
	CFLObjectRef		obj;
	CFLIndex			n;
	
	typeID = 0;
	err = CFLRuntimeRegisterClass( &kCFLiteTestClass, &typeID );
	require_noerr( err, exit );
	
	obj = NULL;
	err = CFLRuntimeCreateInstance( kCFLAllocatorDefault, typeID, 0, (void *) &obj );
	require_noerr( err, exit );
	
	err = CFLGetRetainCount( obj, &n );
	require_noerr( err, exit );
	require_action( n == 1, exit, err = kResponseErr );
	
#if( !defined( __clang_analyzer__ ) || !__clang_analyzer__ )
	obj = CFLRetain( obj );
	require_action( obj, exit, err = kResponseErr );
	err = CFLGetRetainCount( obj, &n );
	require_noerr( err, exit );
	require_action( n == 2, exit, err = kResponseErr );
	
	CFLRelease( obj );
	err = CFLGetRetainCount( obj, &n );
	require_noerr( err, exit );
	require_action( n == 1, exit, err = kResponseErr );
#endif
	
	CFLRelease( obj );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFLiteTest
//===========================================================================================================================

static int	CFLiteTestCompareCStrings( const void *inLeft, const void *inRight, void *inContext );

OSStatus	CFLiteTest( int inPrint )
{
	OSStatus				err;
	CFLObjectRef			obj;
	CFLArrayRef				array, array2;
	CFLDataRef				data, data2;
	CFLDateRef				date;
	CFLDictionaryRef		dict;
	CFLDictionaryRef		dict2;
	CFLNumberRef			number, number2;
	CFLStringRef			string;
	CFLRange				range;
	const char *			s;
	const char *			s2;
	CFLIndex				i;
	CFLIndex				j;
	CFLTypeID				typeID;
	Boolean					b;
	int8_t					value8;
	int16_t					value16;
	int32_t					value32;
	int64_t					value64;
	int128_compat			value128;	
#if( CFL_FLOATING_POINT_NUMBERS )
	Float32					float32;
	Float64					float64;
#endif
	size_t					n, n2;
	CFLDateComponents		dateComponents;
	int						minDepth;
	int						maxDepth;
	char					tempString[ 256 ];
	const void *			values[ 8 ];
	
	//
	// Array
	//
	
	err = CFLArrayCreate( kCFLAllocatorDefault, NULL, &array );
	require_noerr( err, exit );
	
	err = CFLGetTypeID( array, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLArrayGetTypeID(), exit, err = kMismatchErr );
	
	err = CFLArrayAppendValue( array, "test-append-1" );
	require_noerr( err, exit );
	
	err = CFLArrayAppendValue( array, "test-append-2" );
	require_noerr( err, exit );
	
	err = CFLArrayAppendValue( array, "test-append-3" );
	require_noerr( err, exit );
	
	err = CFLArrayCreateCopy( kCFLAllocatorDefault, array, &array2 );
	require_noerr( err, exit );
	
	for( n = 0; n < countof( values ); ++n ) values[ n ] = NULL;
	err = CFLArrayGetValues( array, 0, 3, values );
	require_noerr( err, exit );
	require_action( strcmp( (const char *) values[ 0 ], "test-append-1" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( (const char *) values[ 1 ], "test-append-2" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( (const char *) values[ 2 ], "test-append-3" ) == 0, exit, err = kResponseErr );
	for( n = 3; n < countof( values ); ++n ) require_action( values[ n ] == NULL, exit, err = kResponseErr );
	
	err = CFLArrayGetCount( array2, &i );
	require_noerr( err, exit );
	require_action( i == 3, exit, err = kResponseErr );
	for( n = 0; n < countof( values ); ++n ) values[ n ] = NULL;
	err = CFLArrayGetValues( array2, 0, 3, values );
	require_noerr( err, exit );
	require_action( strcmp( (const char *) values[ 0 ], "test-append-1" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( (const char *) values[ 1 ], "test-append-2" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( (const char *) values[ 2 ], "test-append-3" ) == 0, exit, err = kResponseErr );
	CFLRelease( array2 );
	
	for( n = 0; n < countof( values ); ++n ) values[ n ] = NULL;
	err = CFLArrayGetValues( array, 2, 1, values );
	require_noerr( err, exit );
	require_action( strcmp( (const char *) values[ 0 ], "test-append-3" ) == 0, exit, err = kResponseErr );
	for( n = 1; n < countof( values ); ++n ) require_action( values[ n ] == NULL, exit, err = kResponseErr );
	
	require_action( CFLArrayContainsValue( array, 0, 3, "test-append-1" ), exit, err = kResponseErr );
	require_action( CFLArrayContainsValue( array, 0, 3, "test-append-2" ), exit, err = kResponseErr );
	require_action( CFLArrayContainsValue( array, 0, 3, "test-append-3" ), exit, err = kResponseErr );
	require_action( !CFLArrayContainsValue( array, 0, 3, "test-append-4" ), exit, err = kResponseErr );
	
	require_action( CFLArrayContainsValue( array, 2, 3, "test-append-3" ), exit, err = kResponseErr );
	require_action( !CFLArrayContainsValue( array, 2, 3, "test-append-2" ), exit, err = kResponseErr );
	
	require_action( !CFLArrayContainsValue( array, 0, 0, "test-append-1" ), exit, err = kResponseErr );
	
	err = CFLArrayInsertValueAtIndex( array, 0, "test-insert-front" );
	require_noerr( err, exit );
	
	err = CFLArrayInsertValueAtIndex( array, 2, "test-insert-at-2" );
	require_noerr( err, exit );
	
	err = CFLArrayRemoveValueAtIndex( array, 0 );
	require_noerr( err, exit );
	
	err = CFLArrayRemoveValueAtIndex( array, 3 );
	require_noerr( err, exit );
	
	err = CFLArrayRemoveValueAtIndex( array, 1 );
	require_noerr( err, exit );
	
	err = CFLArrayRemoveAllValues( array );
	require_noerr( err, exit );
	
	CFLRelease( array );
	array = NULL;
	
	err = CFLArrayCreate( kCFLAllocatorDefault, NULL, &array );
	require_noerr( err, exit );
	err = CFLArrayAppendValue( array, "xyz" );
	require_noerr( err, exit );
	err = CFLArrayAppendValue( array, "abc" );
	require_noerr( err, exit );
	err = CFLArrayAppendValue( array, "efg" );
	require_noerr( err, exit );
	err = CFLArrayAppendValue( array, "bcd" );
	require_noerr( err, exit );
	range.location = 0;
	range.length = 4;
	err = CFLArraySortValues( array, range, CFLiteTestCompareCStrings, NULL );
	require_noerr( err, exit );
	
	err = CFLArrayGetValues( array, 0, 4, values );
	require_noerr( err, exit );
	require_action( strcmp( (const char *) values[ 0 ], "abc" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( (const char *) values[ 1 ], "bcd" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( (const char *) values[ 2 ], "efg" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( (const char *) values[ 3 ], "xyz" ) == 0, exit, err = kResponseErr );
	
	CFLRelease( array );
	array = NULL;
	
	//
	// Boolean
	//
	
	err = CFLGetTypeID( kCFLBooleanTrue, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLBooleanGetTypeID(), exit, err = kMismatchErr );
	
	err = CFLGetTypeID( kCFLBooleanFalse, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLBooleanGetTypeID(), exit, err = kMismatchErr );
	
	obj = CFLRetain( kCFLBooleanTrue );
	require_action( obj, exit, err = kResponseErr );
	
	err = CFLGetRetainCount( kCFLBooleanTrue, &i );
	require_noerr( err, exit );
	
	err = CFLGetRetainCount( kCFLBooleanFalse, &i );
	require_noerr( err, exit );
	
	CFLRelease( kCFLBooleanTrue );
	
	err = CFLGetRetainCount( kCFLBooleanTrue, &i );
	require_noerr( err, exit );
	
	require_action( CFLEqual( kCFLBooleanTrue, kCFLBooleanTrue ), exit, err = kResponseErr );
	require_action( CFLEqual( kCFLBooleanFalse, kCFLBooleanFalse ), exit, err = kResponseErr );
	require_action( !CFLEqual( kCFLBooleanTrue, kCFLBooleanFalse ), exit, err = kResponseErr );
	require_action( !CFLEqual( kCFLBooleanFalse, kCFLBooleanTrue ), exit, err = kResponseErr );
	
	//
	// Data
	//
	
	s2 = "tester";
	err = CFLDataCreate( kCFLAllocatorDefault, s2, sizeof( "tester" ) - 1, &data );
	require_noerr( err, exit );
	
	err = CFLGetTypeID( data, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLDataGetTypeID(), exit, err = kMismatchErr );
	
	err = CFLDataGetDataPtr( data, (void *) &s, &n );
	require_noerr( err, exit );
	require_action( s != s2, exit, err = kMismatchErr );
	require_action( memcmp( s, "tester", strlen( "tester" ) ) == 0, exit, err = kMismatchErr );
	
	err = CFLDataAppendData( data, " & ", strlen( " & " )  );
	require_noerr( err, exit );
	
	err = CFLDataAppendData( data, "sons", strlen( "sons" )  );
	require_noerr( err, exit );
	
	err = CFLDataGetDataPtr( data, (void *) &s, &n );
	require_noerr( err, exit );
	require_action( memcmp( s, "tester & sons", strlen( "tester & sons" ) ) == 0, exit, err = kMismatchErr );
	
	CFLRelease( data );
	data = NULL;
	
	// No copy data.
	
	s2 = "tester";
	n2 = sizeof_string( "tester" );
	err = CFLDataCreateNoCopy( kCFLAllocatorDefault, s2, n2, kCFLAllocatorNull, &data );
	require_noerr( err, exit );
	
	err = CFLDataGetDataPtr( data, (void *) &s, &n );
	require_noerr( err, exit );
	require_action( s == s2, exit, err = kMismatchErr );
	require_action( n == n2, exit, err = kMismatchErr );
	
	CFLRelease( data );
	data = NULL;
	
	// Subdata 1.
	
	s2 = "tester12";
	n2 = sizeof_string( "tester12" );
	err = CFLDataCreate( kCFLAllocatorDefault, s2, n2, &data );
	require_noerr( err, exit );
	err = CFLDataGetDataPtr( data, (void *) &s, &n );
	require_noerr( err, exit );
	
	range.location = 2;
	range.length = 4;
	err = CFLDataCreateSubdataWithRangeNoCopy( kCFLAllocatorDefault, data, range, &data2 );
	require_noerr( err, exit );
	err = CFLDataGetDataPtr( data2, (void *) &s2, &n2 );
	require_noerr( err, exit );
	require_action( s2 == ( s + 2 ), exit, err = kMismatchErr );
	require_action( n2 == 4, exit, err = kMismatchErr );
	require_action( memcmp( s2, "ster", n2 ) == 0, exit, err = kMismatchErr );
	
	CFLRelease( data );
	data = NULL;
	CFLRelease( data2 );
	data2 = NULL;
	
	// Subdata 2.
	
	s2 = "tester12";
	n2 = sizeof_string( "tester12" );
	err = CFLDataCreate( kCFLAllocatorDefault, s2, n2, &data );
	require_noerr( err, exit );
	err = CFLDataGetDataPtr( data, (void *) &s, &n );
	require_noerr( err, exit );
	
	range.location = 0;
	range.length = (CFLIndex) n2;
	err = CFLDataCreateSubdataWithRangeNoCopy( kCFLAllocatorDefault, data, range, &data2 );
	require_noerr( err, exit );
	err = CFLDataGetDataPtr( data2, (void *) &s2, &n2 );
	require_noerr( err, exit );
	require_action( s2 == s, exit, err = kMismatchErr );
	require_action( n2 == n, exit, err = kMismatchErr );
	
	CFLRelease( data );
	data = NULL;
	CFLRelease( data2 );
	data2 = NULL;
	
	// Subdata 3.
	
	s2 = "tester12";
	n2 = sizeof_string( "tester12" );
	err = CFLDataCreate( kCFLAllocatorDefault, s2, n2, &data );
	require_noerr( err, exit );
	err = CFLDataGetDataPtr( data, (void *) &s, &n );
	require_noerr( err, exit );
	
	range.location = 4;
	range.length = 4;
	err = CFLDataCreateSubdataWithRangeNoCopy( kCFLAllocatorDefault, data, range, &data2 );
	require_noerr( err, exit );
	err = CFLDataGetDataPtr( data2, (void *) &s2, &n2 );
	require_noerr( err, exit );
	require_action( s2 == ( s + 4 ), exit, err = kMismatchErr );
	require_action( n2 == 4, exit, err = kMismatchErr );
	require_action( memcmp( s2, "er12", n2 ) == 0, exit, err = kMismatchErr );
	
	CFLRelease( data );
	data = NULL;
	CFLRelease( data2 );
	data2 = NULL;
	
	//
	// Date
	//
	
	dateComponents.year 	= 2002;
	dateComponents.month 	= 10;
	dateComponents.day 		= 9;
	dateComponents.hour 	= 3;
	dateComponents.minute 	= 44;
	dateComponents.second 	= 27;
	err = CFLDateCreate( kCFLAllocatorDefault, &dateComponents, &date );
	require_noerr( err, exit );

	err = CFLGetTypeID( date, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLDateGetTypeID(), exit, err = kMismatchErr );

	memset( &dateComponents, 0, sizeof( dateComponents ) );
	err = CFLDateGetDate( date, &dateComponents );
	require_noerr( err, exit );
	require_action( ( dateComponents.year 	== 2002 ) 	&& 
					( dateComponents.month 	== 10 ) 	&& 
					( dateComponents.day 	== 9 ) 		&& 
					( dateComponents.hour 	== 3 ) 		&& 
					( dateComponents.minute == 44 ) 	&& 
					( dateComponents.second == 27 ), 
					exit, err = kMismatchErr );
	
	CFLRelease( date );
	date = NULL;
	
	//
	// Dictionary
	//
	
	err = CFLDictionaryCreate( kCFLAllocatorDefault, 0, NULL, NULL, &dict );
	require_noerr( err, exit );
	
	err = CFLGetTypeID( dict, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLDictionaryGetTypeID(), exit, err = kMismatchErr );
	
	err = CFLDictionarySetValue( dict, "test-1", "test-1-value" );
	require_noerr( err, exit );
	
	err = CFLDictionarySetValue( dict, "test-2", "test-2-value" );
	require_noerr( err, exit );
	
	err = CFLDictionaryAddValue( dict, "test-2", "test-2-value-diff" );
	require_noerr( err, exit );
	
	err = CFLDictionaryAddValue( dict, "test-3", "test-3-value" );
	require_noerr( err, exit );
	
	s = "";
	err = CFLDictionaryGetValue( dict, "test-1", (void *) &s );
	require_noerr( err, exit );
	require_action( memcmp( s, "test-1-value", strlen( "test-1-value" ) ) == 0, exit, err = kMismatchErr );
	
	s = "";
	err = CFLDictionaryGetValue( dict, "test-2", (void *) &s );
	require_noerr( err, exit );
	require_action( memcmp( s, "test-2-value", strlen( "test-2-value" ) ) == 0, exit, err = kMismatchErr );
	
	s = "";
	err = CFLDictionaryGetValue( dict, "test-3", (void *) &s );
	require_noerr( err, exit );
	require_action( memcmp( s, "test-3-value", strlen( "test-3-value" ) ) == 0, exit, err = kMismatchErr );
	
	err = CFLDictionaryRemoveValue( dict, "test-1" );
	require_noerr( err, exit );
	
	err = CFLDictionaryRemoveValue( dict, "test-2" );
	require_noerr( err, exit );
	
	err = CFLDictionaryRemoveValue( dict, "test-3" );
	require_noerr( err, exit );
	
	err = CFLDictionaryCreateCopy( kCFLAllocatorDefault, dict, &dict2 );
	require_noerr( err, exit );
	b = ( ( dict != dict2 ) && CFLEqual( dict, dict2 ) ) ? true : false;
	CFLRelease( dict2 );
	require_action( b, exit, err = kMismatchErr );
	
	CFLRelease( dict );
	dict = NULL;
	
	err = CFLDictionaryCreate( kCFLAllocatorDefault, 31, &kCFLDictionaryKeyCallBacksCFLTypes, 
		&kCFLDictionaryValueCallBacksCFLTypes, &dict );
	require_noerr( err, exit );
	
	for( i = 0; i < 333; ++i )
	{
		snprintf( tempString, sizeof( tempString ), "This is a test of the dictionary hash function %d", i );
		err = CFLStringCreateWithText( kCFLAllocatorDefault, tempString, strlen( tempString ), &string );
		require_noerr( err, exit );
		
		err = CFLDictionarySetValue( dict, string, string );
		CFLRelease( string );
		string = NULL;
		require_noerr( err, exit );
	}
	CFLDictionaryStats( dict, &minDepth, &maxDepth );
	
	CFLRelease( dict );
	dict = NULL;
	
	if( inPrint ) printf( "\tdictionary tests: %d entries, table size 31 -> minDepth %d, maxDepth %d\n", i, minDepth, maxDepth );
	
	err = CFLDictionaryCreate( kCFLAllocatorDefault, 31, &kCFLDictionaryKeyCallBacksCFLTypes, 
		&kCFLDictionaryValueCallBacksCFLTypes, &dict );
	require_noerr( err, exit );
	
	for( i = 0; i < 1000; ++i )
	{
		CFLRandomString( 0, 64, tempString );
		err = CFLStringCreateWithText( kCFLAllocatorDefault, tempString, strlen( tempString ), &string );
		require_noerr( err, exit );
		
		err = CFLDictionarySetValue( dict, string, string );
		CFLRelease( string );
		string = NULL;
		require_noerr( err, exit );
	}
	CFLDictionaryStats( dict, &minDepth, &maxDepth );
	
	CFLRelease( dict );
	dict = NULL;
	
	if( inPrint ) printf( "\tdictionary tests: %d entries, table size 31 -> minDepth %d, maxDepth %d\n", i, minDepth, maxDepth );
	
	//
	// Null
	//
	
	err = CFLGetTypeID( kCFLNull, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLNullGetTypeID(), exit, err = kMismatchErr );
	
	obj = CFLRetain( kCFLNull );
	require_action( obj, exit, err = kResponseErr );
	
	err = CFLGetRetainCount( kCFLNull, &i );
	require_noerr( err, exit );
	
	CFLRelease( kCFLNull );
	
	err = CFLGetRetainCount( kCFLNull, &i );
	require_noerr( err, exit );
	
	require_action( CFLEqual( kCFLNull, kCFLNull ), exit, err = kResponseErr );
	require_action( !CFLEqual( kCFLNull, CFLSTR( "string" ) ), exit, err = kResponseErr );
	
	//
	// Number
	//
	
	// 128 (small)
	
	value128.hi = 0;
	value128.lo = 123;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt128Type, &value128, &number );
	require_noerr( err, exit );
	err = CFLGetTypeID( number, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLNumberGetTypeID(), exit, err = kMismatchErr );
	require_action( CFLNumberGetType( number ) == kCFLNumberSInt128Type, exit, err = kTypeErr );
	require_action( CFLNumberGetByteSize( number ) == 16, exit, err = kTypeErr );
	
	value64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt64Type, &value64 );
	require_noerr( err, exit );
	require_action( value64 == 123, exit, err = kMismatchErr );
	
	value32 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt32Type, &value32 );
	require_noerr( err, exit );
	require_action( value32 == 123, exit, err = kMismatchErr );
	
	value16 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt16Type, &value16 );
	require_noerr( err, exit );
	require_action( value16 == 123, exit, err = kMismatchErr );
	
	value8 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt8Type, &value8 );
	require_noerr( err, exit );
	require_action( value8 == 123, exit, err = kMismatchErr );
	
	value64 = 123;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number2 );
	require_noerr( err, exit );
	require_action( CFLEqual( number, number2 ), exit, err = kMismatchErr );
	
#if( CFL_FLOATING_POINT_NUMBERS )
	float32 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat32Type, &float32 );
	require_noerr( err, exit );
	require_action( float32 == 123, exit, err = kMismatchErr );
	
	float64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat64Type, &float64 );
	require_noerr( err, exit );
	require_action( float64 == 123, exit, err = kMismatchErr );
#endif
	
	CFLRelease( number2 );
	number2 = NULL;
	CFLRelease( number );
	number = NULL;
	
	// 128 (small negative)
	
	value128.hi = -1;
	value128.lo = (uint64_t) INT64_C( -123 );
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt128Type, &value128, &number );
	require_noerr( err, exit );
	err = CFLGetTypeID( number, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLNumberGetTypeID(), exit, err = kMismatchErr );
	
	value64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt64Type, &value64 );
	require_noerr( err, exit );
	require_action( value64 == -123, exit, err = kMismatchErr );
	
	value32 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt32Type, &value32 );
	require_noerr( err, exit );
	require_action( value32 == -123, exit, err = kMismatchErr );
	
	value16 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt16Type, &value16 );
	require_noerr( err, exit );
	require_action( value16 == -123, exit, err = kMismatchErr );
	
	value8 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt8Type, &value8 );
	require_noerr( err, exit );
	require_action( value8 == -123, exit, err = kMismatchErr );
	
#if( CFL_FLOATING_POINT_NUMBERS )
	float32 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat32Type, &float32 );
	require_noerr( err, exit );
	require_action( float32 == -123, exit, err = kMismatchErr );
	
	float64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat64Type, &float64 );
	require_noerr( err, exit );
	require_action( float64 == -123, exit, err = kMismatchErr );
#endif
	
	value64 = -123;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number2 );
	require_noerr( err, exit );
	require_action( CFLEqual( number, number2 ), exit, err = kMismatchErr );
	
	CFLRelease( number2 );
	number2 = NULL;
	CFLRelease( number );
	number = NULL;
	
	// 128 (big)
	
	value128.hi = (int64_t) UINT64_C( 0xFFFFFFFFFFFFFFFF );
	value128.lo = UINT64_C( 0xFFFFFFFFFFFFFFFF );
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt128Type, &value128, &number );
	require_noerr( err, exit );
	err = CFLGetTypeID( number, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLNumberGetTypeID(), exit, err = kMismatchErr );
	
	err = CFLNumberGetValue( number, kCFLNumberSInt128Type, &value128 );
	require_noerr( err, exit );
	require_action( ( (uint64_t) value128.hi ) == UINT64_C( 0xFFFFFFFFFFFFFFFF ), exit, err = kMismatchErr );
	require_action( value128.lo == UINT64_C( 0xFFFFFFFFFFFFFFFF ), exit, err = kMismatchErr );
	
	value64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt64Type, &value64 );
	require_noerr( err, exit );
	require_action( ( (uint64_t) value64 ) == UINT64_C( 0xFFFFFFFFFFFFFFFF ), exit, err = kMismatchErr );
	
#if( CFL_FLOATING_POINT_NUMBERS )
	float32 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat32Type, &float32 );
	require_noerr( err, exit );
	require_action( float32 == -1, exit, err = kMismatchErr );
	
	float64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat64Type, &float64 );
	require_noerr( err, exit );
	require_action( float64 == -1, exit, err = kMismatchErr );
#endif
	
	CFLRelease( number );
	number = NULL;
	
	// 64
	
	value64 = 123;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number );
	require_noerr( err, exit );
	err = CFLGetTypeID( number, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLNumberGetTypeID(), exit, err = kMismatchErr );
	
	value64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt64Type, &value64 );
	require_noerr( err, exit );
	require_action( value64 == 123, exit, err = kMismatchErr );
	
	value32 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt32Type, &value32 );
	require_noerr( err, exit );
	require_action( value32 == 123, exit, err = kMismatchErr );
	
	value16 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt16Type, &value16 );
	require_noerr( err, exit );
	require_action( value16 == 123, exit, err = kMismatchErr );
	
	value8 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt8Type, &value8 );
	require_noerr( err, exit );
	require_action( value8 == 123, exit, err = kMismatchErr );

#if( CFL_FLOATING_POINT_NUMBERS )
	float32 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat32Type, &float32 );
	require_noerr( err, exit );
	require_action( float32 == 123, exit, err = kMismatchErr );
	
	float64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat64Type, &float64 );
	require_noerr( err, exit );
	require_action( float64 == 123, exit, err = kMismatchErr );
#endif
	
	CFLRelease( number );
	number = NULL;
	
	// Comparisons
	
	value64 = 0;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number );
	require_noerr( err, exit );
	value64 = 0;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number2 );
	require_noerr( err, exit );
	require_action( CFLNumberCompare( number, number2 ) == 0, exit, err = kResponseErr );
	CFLRelease( number );
	number = NULL;
	CFLRelease( number2 );
	number2 = NULL;
	
	value64 = 123;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number );
	require_noerr( err, exit );
	value64 = 124;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number2 );
	require_noerr( err, exit );
	require_action( CFLNumberCompare( number, number2 ) < 0, exit, err = kResponseErr );
	require_action( CFLNumberCompare( number2, number ) > 0, exit, err = kResponseErr );
	CFLRelease( number );
	number = NULL;
	CFLRelease( number2 );
	number2 = NULL;
	
	value64 = -123;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number );
	require_noerr( err, exit );
	value64 = 124;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number2 );
	require_noerr( err, exit );
	require_action( CFLNumberCompare( number, number2 ) < 0, exit, err = kResponseErr );
	require_action( CFLNumberCompare( number2, number ) > 0, exit, err = kResponseErr );
	CFLRelease( number );
	number = NULL;
	CFLRelease( number2 );
	number2 = NULL;
	
	value64 = -124;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number );
	require_noerr( err, exit );
	value64 = -123;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number2 );
	require_noerr( err, exit );
	require_action( CFLNumberCompare( number, number2 ) < 0, exit, err = kResponseErr );
	require_action( CFLNumberCompare( number2, number ) > 0, exit, err = kResponseErr );
	CFLRelease( number );
	number = NULL;
	CFLRelease( number2 );
	number2 = NULL;
	
#if( CFL_FLOATING_POINT_NUMBERS )
	
	// Float64 (int)
	
	float64 = 123;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number );
	require_noerr( err, exit );
	err = CFLGetTypeID( number, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLNumberGetTypeID(), exit, err = kMismatchErr );
	
	value64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt64Type, &value64 );
	require_noerr( err, exit );
	require_action( value64 == 123, exit, err = kMismatchErr );
	
	value32 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt32Type, &value32 );
	require_noerr( err, exit );
	require_action( value32 == 123, exit, err = kMismatchErr );
	
	value16 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt16Type, &value16 );
	require_noerr( err, exit );
	require_action( value16 == 123, exit, err = kMismatchErr );
	
	value8 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt8Type, &value8 );
	require_noerr( err, exit );
	require_action( value8 == 123, exit, err = kMismatchErr );
	
	float32 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat32Type, &float32 );
	require_noerr( err, exit );
	require_action( float32 == 123, exit, err = kMismatchErr );
	
	float64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat64Type, &float64 );
	require_noerr( err, exit );
	require_action( float64 == 123, exit, err = kMismatchErr );
	
	CFLRelease( number );
	number = NULL;
	
	// Float64 (float)
	
	float64 = 124.3;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number );
	require_noerr( err, exit );
	err = CFLGetTypeID( number, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLNumberGetTypeID(), exit, err = kMismatchErr );
	
	value64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt64Type, &value64 );
	require_noerr( err, exit );
	require_action( value64 == 124, exit, err = kMismatchErr );
	
	value32 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt32Type, &value32 );
	require_noerr( err, exit );
	require_action( value32 == 124, exit, err = kMismatchErr );
	
	value16 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt16Type, &value16 );
	require_noerr( err, exit );
	require_action( value16 == 124, exit, err = kMismatchErr );
	
	value8 = 0;
	err = CFLNumberGetValue( number, kCFLNumberSInt8Type, &value8 );
	require_noerr( err, exit );
	require_action( value8 == 124, exit, err = kMismatchErr );
	
	float32 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat32Type, &float32 );
	require_noerr( err, exit );
	require_action( float32 == 124.3f, exit, err = kMismatchErr );
	
	float64 = 0;
	err = CFLNumberGetValue( number, kCFLNumberFloat64Type, &float64 );
	require_noerr( err, exit );
	require_action( float64 == 124.3, exit, err = kMismatchErr );
	
	CFLRelease( number );
	number = NULL;
	
	float64 = 123.5;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number );
	require_noerr( err, exit );
	float64 = 124.5;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number2 );
	require_noerr( err, exit );
	require_action( CFLNumberCompare( number, number2 ) < 0, exit, err = kResponseErr );
	require_action( CFLNumberCompare( number2, number ) > 0, exit, err = kResponseErr );
	CFLRelease( number );
	number = NULL;
	CFLRelease( number2 );
	number2 = NULL;
	
	float64 = -123.5;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number );
	require_noerr( err, exit );
	float64 = 124.5;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number2 );
	require_noerr( err, exit );
	require_action( CFLNumberCompare( number, number2 ) < 0, exit, err = kResponseErr );
	require_action( CFLNumberCompare( number2, number ) > 0, exit, err = kResponseErr );
	CFLRelease( number );
	number = NULL;
	CFLRelease( number2 );
	number2 = NULL;
	
	float64 = -124.5;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number );
	require_noerr( err, exit );
	float64 = -123.5;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number2 );
	require_noerr( err, exit );
	require_action( CFLNumberCompare( number, number2 ) < 0, exit, err = kResponseErr );
	require_action( CFLNumberCompare( number2, number ) > 0, exit, err = kResponseErr );
	CFLRelease( number );
	number = NULL;
	CFLRelease( number2 );
	number2 = NULL;
	
	float64 = 123.5;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number );
	require_noerr( err, exit );
	value64 = 125;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number2 );
	require_noerr( err, exit );
	require_action( CFLNumberCompare( number, number2 ) < 0, exit, err = kResponseErr );
	require_action( CFLNumberCompare( number2, number ) > 0, exit, err = kResponseErr );
	CFLRelease( number );
	number = NULL;
	CFLRelease( number2 );
	number2 = NULL;
	
	float64 = -123.5;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number );
	require_noerr( err, exit );
	value64 = 125;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number2 );
	require_noerr( err, exit );
	require_action( CFLNumberCompare( number, number2 ) < 0, exit, err = kResponseErr );
	require_action( CFLNumberCompare( number2, number ) > 0, exit, err = kResponseErr );
	CFLRelease( number );
	number = NULL;
	CFLRelease( number2 );
	number2 = NULL;
	
	float64 = -125.5;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number );
	require_noerr( err, exit );
	value64 = -123;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number2 );
	require_noerr( err, exit );
	require_action( CFLNumberCompare( number, number2 ) < 0, exit, err = kResponseErr );
	require_action( CFLNumberCompare( number2, number ) > 0, exit, err = kResponseErr );
	CFLRelease( number );
	number = NULL;
	CFLRelease( number2 );
	number2 = NULL;
	
	float64 = 0;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberFloat64Type, &float64, &number );
	require_noerr( err, exit );
	value64 = 0;
	err = CFLNumberCreate( kCFLAllocatorDefault, kCFLNumberSInt64Type, &value64, &number2 );
	require_noerr( err, exit );
	require_action( CFLNumberCompare( number, number2 ) == 0, exit, err = kResponseErr );
	require_action( CFLNumberCompare( number2, number ) == 0, exit, err = kResponseErr );
	CFLRelease( number );
	number = NULL;
	CFLRelease( number2 );
	number2 = NULL;
#endif
	
	//
	// String
	//
	
	err = CFLStringCreateWithText( kCFLAllocatorDefault, "test", strlen( "test" ), &string );
	require_noerr( err, exit );
	
	err = CFLGetTypeID( string, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLStringGetTypeID(), exit, err = kMismatchErr );
	
	err = CFLStringGetLength( string, &i );
	require_noerr( err, exit );
	require_action( i == (CFLIndex) strlen( "test" ), exit, err = kMismatchErr );
	
	err = CFLStringGetCStringPtr( string, &s, &n );
	require_noerr( err, exit );
	require_action( n == strlen( "test" ), exit, err = kMismatchErr );
	require_action( strcmp( s, "test" ) == 0, exit, err = kMismatchErr );
	
	err = CFLStringSetText( string, "another test", strlen( "another test" ) );
	require_noerr( err, exit );
	
	err = CFLStringSetText( string, "", 0 );
	require_noerr( err, exit );
	
	CFLRelease( string );
	string = NULL;
	
	err = CFLStringCreateWithText( kCFLAllocatorDefault, "", 0, &string );
	require_noerr( err, exit );
	
	err = CFLStringGetCStringPtr( string, &s, &n );
	require_noerr( err, exit );
	require_action( n == 0, exit, err = kMismatchErr );
	require_action( strlen( s ) == 0, exit, err = kMismatchErr );
	
	CFLRelease( string );
	string = NULL;
	
	// Constant CFLStrings
	
	string = CFLSTR( "constant string" );
	
	err = CFLGetTypeID( string, &typeID );
	require_noerr( err, exit );
	require_action( typeID == CFLStringGetTypeID(), exit, err = kMismatchErr );
	
	err = CFLGetRetainCount( string, &i );
	require_noerr( err, exit );
	require_action( i > 0, exit, err = kResponseErr );
	
	CFLRelease( string );
	
	err = CFLGetRetainCount( string, &j );
	require_noerr( err, exit );
	require_action( j == i, exit, err = kResponseErr );
	
	obj = CFLRetain( string );
	require_action( obj, exit, err = kResponseErr );
	
	err = CFLGetRetainCount( string, &j );
	require_noerr( err, exit );
	require_action( j == i, exit, err = kResponseErr );
	
	err = CFLStringGetLength( string, &i );
	require_noerr( err, exit );
	require_action( i == (CFLIndex) strlen( "constant string" ), exit, err = kMismatchErr );
	
	err = CFLStringGetCStringPtr( string, &s, &n );
	require_noerr( err, exit );
	require_action( n == strlen( "constant string" ), exit, err = kMismatchErr );
	require_action( strcmp( s, "constant string" ) == 0, exit, err = kMismatchErr );
	
	require_action( CFLEqual( CFLSTR( "test" ), CFLSTR( "test" ) ), exit, err = kMismatchErr );
	require_action( !CFLEqual( CFLSTR( "abc" ), CFLSTR( "xyz" ) ), exit, err = kMismatchErr );
	
	// Runtime
	
	err = CFLiteRuntimeClassesTest();
	require_noerr( err, exit );
	
exit:
	printf( "CFLiteTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

static int	CFLiteTestCompareCStrings( const void *inLeft, const void *inRight, void *inContext )
{
	(void) inContext;
	
	return( strcmp( (const char *) inLeft, (const char *) inRight ) );
}

#endif // !EXCLUDE_UNIT_TESTS
