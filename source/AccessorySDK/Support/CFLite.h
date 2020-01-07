/*
	File:    	CFLite.h
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

#ifndef	__CFLite_h__
#define	__CFLite_h__

#include "CommonServices.h"

#if 0
#pragma mark == Configuration ==
#endif

//===========================================================================================================================
//	Configuration
//===========================================================================================================================

// CFL_FLOATING_POINT_NUMBERS -- 1=Support floating point number objects. 0=Don't support them.

#if( !defined( CFL_FLOATING_POINT_NUMBERS ) )
	#if( TARGET_HAS_FLOATING_POINT_SUPPORT )
		#define	CFL_FLOATING_POINT_NUMBERS		1
	#else
		#define	CFL_FLOATING_POINT_NUMBERS		0
	#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == General - Types ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLIndex
	@abstract	An integer type used throughout Core Foundation in several programmatic roles: as an array index and for 
				count, size, and length parameters and return values. 

	@discussion
	
	Core Foundation types as CFLIndex all parameters and return values that might grow over time as the processor's address 
	size changes. On architectures where pointer sizes are a different size (say, 64 bits) CFLIndex might be declared to be 
	also 64 bits, independent of the size of int. If you type your own variables that interact with Core Foundation as 
	CFLIndex, your code will have a higher degree of source compatibility in the future.
	
	@constant	kCFLIndexEnd	Meta-value to pass to supported routines to indicate the end.
*/
typedef int		CFLIndex;

#define	kCFLIndexEnd		( (CFLIndex) -1 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLHashCode
	@abstract	A type for hash codes returned by the Core Foundation function CFLHash.
*/
typedef uint32_t		CFLHashCode;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLRange
	@abstract	Type for a specifying a range.
*/
typedef struct
{
	CFLIndex		location;
	CFLIndex		length;

}	CFLRange;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLTypeID
	@abstract	A type for unique, constant integer values that identify particular Core Foundation opaque types.
	
	@discussion
	
	The CFLTypeID type defines a type ID in Core Foundation. A type ID is an integer that identifies the opaque type to 
	which a Core Foundation object "belongs." You use type IDs in various contexts, such as when you are operating on 
	heterogeneous collections. Base Services provide programmatic interfaces for obtaining and evaluating type IDs. 
	
	Because the value for a type ID can change from release to release, your code should not rely on stored or hard-coded 
	type IDs nor should it hard-code any observed properties of a type ID (such as, for example, it being a small integer). 
*/
typedef uint32_t		CFLTypeID;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLObjectRef
	@abstract	An untyped "generic" reference to any Core Foundation object. 
	
	@discussion
	
	The CFLObjectRef type is one of the base types defined in Core Foundation's Base Services. It is used as the type and 
	return value in several "polymorphic" functions defined in Base Services. It is a generic object reference that acts 
	as a placeholder for references to true Core Foundation objects. 
*/
typedef const void *		CFLObjectRef;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLAllocatorRef
	@abstract	A reference to a CFLAllocator object in Core Foundation. 
	
	@discussion
	
	The CFLAllocatorRef type is a reference type used in many Core Foundation parameters and function results. It refers 
	to a CFLAllocator object, which allocates, reallocates, and deallocates memory for Core Foundation objects. CFLAllocator 
	is an opaque type, defined in Core Foundation's Base Services, that defines the characteristics of CFLAllocator objects.
	
	@constant	kCFLAllocatorDefault	Default allocator.
	@constant	kCFLAllocatorMalloc		Used for a bytes deallocator to have free called when done.
	@constant	kCFLAllocatorNull		Used for a bytes deallocator to not try to deallocate the memory.
*/
typedef struct CFLAllocator *		CFLAllocatorRef;

#define	kCFLAllocatorDefault		( (CFLAllocatorRef)(uintptr_t) 0 )
#define	kCFLAllocatorMalloc			( (CFLAllocatorRef)(uintptr_t) -1 )
#define	kCFLAllocatorNull			( (CFLAllocatorRef)(uintptr_t) -2 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLStringRef
	@abstract	A reference to a CFLString object in Core Foundation. 
*/
typedef struct CFLString *		CFLStringRef;

#if 0
#pragma mark -
#pragma mark == General - Functions ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLGetTypeID
	@abstract	Obtains the unique identifier of the opaque type to which a Core Foundation object belongs. 
	
	@param		inObject	A generic reference of type CFLObjectRef. Pass a reference to any Core Foundation object whose 
							type ID you want to obtain. 

	@param		outTypeID	Ptr to receive a value of type CFLTypeID that identifies the opaque type of a Core Foundation object. 
	
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	
	@discussion
	
	This function returns a value that uniquely identifies the opaque type of any Core Foundation object. You can compare 
	this value with the known CFLTypeID identifier obtained with a "GetTypeID" function specific to a type (e.g. 
	CFLDateGetTypeID). These values might change from release to release or platform to platform.
*/
OSStatus	CFLGetTypeID( CFLObjectRef inObject, CFLTypeID *outTypeID );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLGetRetainCount
	
	@abstract	Obtains the retention count of a Core Foundation object. 
	
	@param		inObject	A generic reference of type CFLObjectRef. Pass a reference to any Core Foundation object whose 
							retention count you want to obtain.

	@param		outCount	Ptr to receive a number representing the retention count of an object.
	
	@result		An error code indicating failure reason or kNoErr (0) if successful.
*/
OSStatus	CFLGetRetainCount( CFLObjectRef inObject, CFLIndex *outCount );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLRetain
	@abstract	Retains a Core Foundation object. 
	
	@param		inObject	A generic reference of type CFLObjectRef Pass a reference to a Core Foundation object that you 
							want to retain.
	
	@discussion
	
	Retains a Core Foundation object by incrementing its retention count. You should retain a Core Foundation object when 
	you receive it from elsewhere (that is, you did not create or copy it) and you want it to persist. If you retain a 
	Core Foundation object you are responsible for releasing it (see the CFLRelease function). 
*/
CFLObjectRef	CFLRetain( CFLObjectRef inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLRelease
	@abstract	Releases a Core Foundation object. 
	
	@param		inObject	A generic reference of type CFLObjectRef. Pass a reference to any Core Foundation object that 
							you want to release (that is, relinquish your claim of ownership).

	@discussion
	
	Releases a Core Foundation object by decrementing its retention count. If that count consequently becomes zero the 
	memory allocated to the object is deallocated and the object is destroyed. If you create, copy, or explictly retain 
	(see the CFLRetain function) a Core Foundation object, you are responsible for releasing it when you no longer need it. 
*/
void	CFLRelease( CFLObjectRef inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLEqual
	@abstract	Determines whether the two Core Foundation objects referenced in the parameters are equal.
	
	@param		inLeft		A generic reference of type CFLObjectRef that refers to any Core Foundation object. Pass a 
							reference to the first object used in the comparison.

	@param		inRight		A generic reference of type CFLObjectRef that refers to any Core Foundation object. Pass a 
							reference to the second object used in the comparison.
		
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	
	@discussion
	
	When CFLEqual is called, Core Foundation returns true if the two objects are of the same type and equal, or false if 
	otherwise.
	
	Equality is something specific to each Core Foundation type. For example, two CFLNumbers are equal if the numeric 
	values they represent are equal. Two CFLStrings are equal if they represent identical sequences of characters, 
	regardless of encoding. 
*/
Boolean	CFLEqual( CFLObjectRef inLeft, CFLObjectRef inRight );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLHash
	@abstract	Obtains a code that can be used to identify an object in a hashing structure. 
	
	@param		inObject		A generic reference of type CFLObjectRef. Pass a reference to any Core Foundation object 
								whose hashing value you want to obtain.
	
	@discussion
	
	Two objects that are equal (as determined by the CFLEqual function) have the same hashing value. However, the converse 
	is not true: two objects with the same hashing value might not be equal. That is, hashing values are not unique. 
	
	The hashing value for an object might change from release to release or from platform to platform.
*/
CFLHashCode	CFLHash( CFLObjectRef inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLCopy
	@abstract	Copies an object and any objects it may contain.
	@param		inSrc	A generic reference of type CFLObjectRef. Pass a reference to any CF object you wish to copy.
	@param		outDst	Ptr to receive a CFLObjectRef for the copied object.
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	@discussion
	
	The caller of this function receives a reference to the returned object. The caller also implicitly retains the object, 
	and is responsible for releasing it. 
*/
OSStatus	CFLCopy( CFLObjectRef inSrc, CFLObjectRef *outDst );

#if 0
#pragma mark -
#pragma mark == Array - Types ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLArrayRef
	@abstract	This is the type of a reference to a CFLArray.
*/
typedef struct CFLArray *		CFLArrayRef;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLArrayCallBackRetain
	@abstract	This is called to retain an object.
*/

typedef const void * ( *CFLArrayCallBackRetain )( CFLAllocatorRef inAllocator, const void *inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLArrayCallBackRelease
	@abstract	This is called to release an object.
*/
typedef void ( *CFLArrayCallBackRelease )( CFLAllocatorRef inAllocator, const void *inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLArrayCallBackEqual
	@abstract	This is called to determine if 2 objects are equal. A non-0 result indicates inequality.
*/
typedef Boolean	( *CFLArrayCallBackEqual )( const void *inLeft, const void *inRight );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLArrayCopyDescriptionCallBack
*/
typedef CFLStringRef	( *CFLArrayCopyDescriptionCallBack )( const void *inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		CFLArrayCallBacks
	@abstract	Structure containing the callbacks of a CFLArray.
	
	@field		retain
	
		The callback used to add a retain for the array on values as they are put into the array. This callback returns the 
		value to store in the array, which is usually the value parameter passed to this callback, but may be a different 
		value if a different value should be stored in the array. The array's allocator is passed as the first argument.

	@field		release
	
		The callback used to remove a retain previously added for the array from values as they are removed from the array. 
		The array's allocator is passed as the first argument.
	
	@field		equal		The callback used to compare values in the array for equality for some operations. 	
*/
typedef struct CFLArrayCallBacks	CFLArrayCallBacks;
struct CFLArrayCallBacks
{
	CFLIndex							version;
	CFLArrayCallBackRetain				retain;
	CFLArrayCallBackRelease				release;
	CFLArrayCopyDescriptionCallBack		copyDescription;
	CFLArrayCallBackEqual				equal;
};

//---------------------------------------------------------------------------------------------------------------------------
/*!	@constant	kCFLArrayCallBacksCFLTypes
	@abstract	Pre-defined constant structure for a collection representing Core Foundation objects.
*/
IMPORT_GLOBAL const CFLArrayCallBacks		kCFLArrayCallBacksCFLTypes;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@constant	kCFLArrayCallBacksNull
	@abstract	Pre-defined constant structure for all-NULL callbacks.
*/
IMPORT_GLOBAL const CFLArrayCallBacks		kCFLArrayCallBacksNull;

#if 0
#pragma mark -
#pragma mark == Array - Functions ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayGetTypeID
	@abstract	Obtains the Core Foundation type identifier for CFLArray.
	@result		The Core Foundation type identifier for CFLArray.
*/
CFLTypeID	CFLArrayGetTypeID( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayCreate
	@abstract	Creates an array.
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	@discussion
	
	The caller of this function receives a reference to the returned object. The caller also implicitly retains the object, 
	and is responsible for releasing it. 
*/
OSStatus	CFLArrayCreate( CFLAllocatorRef inAllocator, const CFLArrayCallBacks *inCallBacks, CFLArrayRef *outRef );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayCreateCopy
	@abstract	Creates an array with the items from another array.
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	@discussion
	
	The caller of this function receives a reference to the returned object. The caller also implicitly retains the object, 
	and is responsible for releasing it. 
*/
OSStatus	CFLArrayCreateCopy( CFLAllocatorRef inAllocator, CFLArrayRef inArray, CFLArrayRef *outArray );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayGetCount
	@abstract	Returns the number of values currently in the array. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLArrayGetCount( CFLArrayRef inObject, CFLIndex *outCount );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayGetValues
	@abstract	Gets multiple values from an array.
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLArrayGetValues( CFLArrayRef inObject, CFLIndex inIndex, CFLIndex inCount, const void **inValues );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayGetValueAtIndex
	@abstract	Retrieves the value at the given index. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLArrayGetValueAtIndex( CFLArrayRef inObject, CFLIndex inIndex, void *outValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArraySetValueAtIndex
	@abstract	Changes the value with the given index in the array. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.
*/
OSStatus	CFLArraySetValueAtIndex( CFLArrayRef inObject, CFLIndex inIndex, const void *inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayInsertValueAtIndex
	@abstract	Adds the value to the array giving it the given index. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLArrayInsertValueAtIndex( CFLArrayRef inObject, CFLIndex inIndex, const void *inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayAppendValue
	@abstract	Adds the value to the array giving it the new largest index. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLArrayAppendValue( CFLArrayRef inObject, const void *inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayRemoveValueAtIndex
	@abstract	Removes the value with the given index from the array. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/

OSStatus	CFLArrayRemoveValueAtIndex( CFLArrayRef inObject, CFLIndex inIndex );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayRemoveAllValues
	@abstract	Removes all the values from the array, making it empty. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLArrayRemoveAllValues( CFLArrayRef inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayContainsValue
	@abstract	Checks if given value is found in the array.
	@param		inStart		Index in the array to start searching. Must be within the bounds of the array.
	@param		inEnd		1 past the index to stop searching (e.g. &array[ inIndex ]). Must be <= count of array.
	@result		true if the value exists in the array, false otherwise.
*/
Boolean	CFLArrayContainsValue( CFLArrayRef inObject, CFLIndex inStart, CFLIndex inEnd, const void *inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArrayApplyFunction
	@abstract	Calls a function for each value in the array.
*/
typedef void ( *CFLArrayApplierFunction )( const void *inValue, void *inContext );

OSStatus	CFLArrayApplyFunction( CFLArrayRef inArray, CFLRange inRange, CFLArrayApplierFunction inApplier, void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLArraySortValues
	@abstract	Calls a function for each value in the array.
*/
typedef int		( *CFLComparatorFunction )( const void *inLeft, const void *inRight, void *inContext );

OSStatus	CFLArraySortValues( CFLArrayRef inArray, CFLRange inRange, CFLComparatorFunction inCmp, void *inContext );

#if 0
#pragma mark -
#pragma mark == Boolean ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLBooleanRef
	@abstract	The type of a reference to a CFLBoolean object.
	@constant	kCFLBooleanTrue		True (1) value.
	@constant	kCFLBooleanFalse	False (0) value.
	@discussion
	
	CFLBoolean objects are used to wrap boolean values for use in Core Foundation property lists and collection types.
*/
typedef struct CFLBoolean *		CFLBooleanRef;

IMPORT_GLOBAL CFLBooleanRef			kCFLBooleanTrue;
IMPORT_GLOBAL CFLBooleanRef			kCFLBooleanFalse;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLBooleanGetTypeID
	@abstract	Obtains the Core Foundation type identifier for CFLBoolean.
	@result		The Core Foundation type identifier for CFLBoolean.
*/
CFLTypeID	CFLBooleanGetTypeID( void );

#if 0
#pragma mark -
#pragma mark == Data ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLDataRef
	@abstract	The type of a reference to a CFLData object. 
*/
typedef struct CFLData *		CFLDataRef;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDataGetTypeID
	@abstract	Obtains the Core Foundation type identifier for CFLData.
	@result		The Core Foundation type identifier for CFLData.
*/
CFLTypeID	CFLDataGetTypeID( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDataCreate
	@abstract	Creates a CFLData object from program-supplied data.
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	@discussion
	
	The caller of this function receives a reference to the returned object. The caller also implicitly retains the object, 
	and is responsible for releasing it. 
*/
OSStatus	CFLDataCreate( CFLAllocatorRef inAllocator, const void *inData, size_t inSize, CFLDataRef *outRef );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDataCreateNoCopy
	@abstract	Creates a CFLData object from program-supplied data without copying it.
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	
	@param		inData				Ptr to existing memory. This memory must remain valid as long as this object is alive.
	@param		inSize				Number of bytes in "inData".
	@param		inBytesDeallocator	Allocator to use to deallocate "inData".
									May be kCFLAllocatorMalloc to call free to deallocate the bytes.
									May be kCFLAllocatorNull to do nothing to deallocate the bytes (caller's responsibility).
	
	@discussion
	
	The caller of this function receives a reference to the returned object. The caller also implicitly retains the object, 
	and is responsible for releasing it. 
*/
OSStatus
	CFLDataCreateNoCopy( 
		CFLAllocatorRef		inAllocator, 
		const void *		inData, 
		size_t				inSize, 
		CFLAllocatorRef		inBytesDeallocator, 
		CFLDataRef *		outRef );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDataCreateSubdataWithRangeNoCopy
	@abstract	Creates a CFLData object from a range of bytes within another CFLData without copying it.
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	
	@param		inData				Data to create subdata from.
	@param		inRange				Range of bytes within "inData" to create the subdata from.
	@discussion
	
	The caller must ensure that the bytes of "inData" are not modified after this call.
	The caller of this function receives a reference to the returned object. The caller also implicitly retains the object, 
	and is responsible for releasing it. 
*/
OSStatus
	CFLDataCreateSubdataWithRangeNoCopy( 
		CFLAllocatorRef	inAllocator, 
		CFLDataRef		inData, 
		CFLRange		inRange, 
		CFLDataRef *	outRef );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDataGetDataPtr
	@abstract	Obtains a pointer to the bytes of a CFLData object.
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
	@discussion
	
	This function either returns the requested pointer immediately, with no memory allocations and no copying.
*/
OSStatus	CFLDataGetDataPtr( CFLDataRef inObject, void *outDataPtr, size_t *outSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDataSetData
	@abstract	Replaces the contents of the object with the specified data.
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLDataSetData( CFLDataRef inObject, const void *inData, size_t inSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDataAppendData
	@abstract	Appends data to the object.
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLDataAppendData( CFLDataRef inObject, const void *inData, size_t inSize );

#if 0
#pragma mark -
#pragma mark == Date ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLDateRef
	@abstract	The type of a reference to a CFLDate object. 
*/
typedef struct CFLDate *		CFLDateRef;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		CFLDateComponents
	@abstract	Components of a date.
*/
typedef struct CFLDateComponents	CFLDateComponents;
struct CFLDateComponents
{
	int		year;
	int		month;
	int		day;
	int		hour;
	int		minute;
	int		second;
};

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDateGetTypeID
	@abstract	Obtains the Core Foundation type identifier for CFLDate objects. 
	@result		The Core Foundation type identifier for CFLDate objects. 
*/
CFLTypeID	CFLDateGetTypeID( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDateCreate
	@abstract	Creates a CFLDate object using the specified data components. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	@discussion
	
	The caller of this function receives a reference to the returned object. The caller also implicitly retains the object, 
	and is responsible for releasing it. 
*/
OSStatus	CFLDateCreate( CFLAllocatorRef inAllocator, const CFLDateComponents *inDate, CFLDateRef *outRef );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDateGetDate
	@abstract	Gets the date as date components.
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLDateGetDate( CFLDateRef inObject, CFLDateComponents *outDate );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDateSetDate
	@abstract	Sets the date from date components.
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLDateSetDate( CFLDateRef inObject, const CFLDateComponents *inDate );

#if 0
#pragma mark -
#pragma mark == Dictionary - Types ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLDictionaryRef
	@abstract	This is the type of a reference to a CFLDictionary.
*/
typedef struct CFLDictionary *		CFLDictionaryRef;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLDictionaryCallBackRetain
	@abstract	This is called to retain an object.
*/
typedef const void * ( *CFLDictionaryCallBackRetain )( CFLAllocatorRef inAllocator, const void *inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLDictionaryCallBackRelease
	@abstract	This is called to release an object.
*/
typedef void ( *CFLDictionaryCallBackRelease )( CFLAllocatorRef inAllocator, const void *inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLDictionaryCallBackEqual
	@abstract	This is called to determine if 2 objects are equal. A non-0 result indicates inequality.
*/
typedef Boolean ( *CFLDictionaryCallBackEqual )( const void *inLeft, const void *inRight );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLDictionaryCallBackHash
	@abstract	This is called to calculate the hash code for an object.
*/
typedef CFLHashCode ( *CFLDictionaryCallBackHash )( const void *inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLDictionaryCopyDescriptionCallBack
*/
typedef CFLStringRef	( *CFLDictionaryCopyDescriptionCallBack )( const void *inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		CFLDictionaryKeyCallBacks
	@abstract	Structure containing the callbacks for keys of a CFLDictionary.
*/
typedef struct CFLDictionaryKeyCallBacks	CFLDictionaryKeyCallBacks;
struct CFLDictionaryKeyCallBacks
{
	CFLIndex									version;
	CFLDictionaryCallBackRetain					retain;
	CFLDictionaryCallBackRelease				release;
	CFLDictionaryCopyDescriptionCallBack		copyDescription;
	CFLDictionaryCallBackEqual					equal;
	CFLDictionaryCallBackHash					hash;
};

//---------------------------------------------------------------------------------------------------------------------------
/*!	@constant	kCFLDictionaryKeyCallBacksCFLTypes
	@abstract	Pre-defined constant structure for a collection representing Core Foundation objects.
*/
IMPORT_GLOBAL const CFLDictionaryKeyCallBacks			kCFLDictionaryKeyCallBacksCFLTypes;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@constant	kCFLDictionaryKeyCallBacksNull
	@abstract	Pre-defined constant structure for all-NULL callbacks.
*/
IMPORT_GLOBAL const CFLDictionaryKeyCallBacks			kCFLDictionaryKeyCallBacksNull;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		CFLDictionaryValueCallBacks
	@abstract	Structure containing the callbacks for values of a CFLDictionary. 
*/
typedef struct CFLDictionaryValueCallBacks		CFLDictionaryValueCallBacks;
struct CFLDictionaryValueCallBacks
{
	CFLIndex									version;
	CFLDictionaryCallBackRetain					retain;
	CFLDictionaryCallBackRelease				release;
	CFLDictionaryCopyDescriptionCallBack		copyDescription;
	CFLDictionaryCallBackEqual					equal;
};

//---------------------------------------------------------------------------------------------------------------------------
/*!	@constant	kCFLDictionaryValueCallBacksCFLTypes
	@abstract	Pre-defined constant structure for a collection representing Core Foundation objects.
*/
IMPORT_GLOBAL const CFLDictionaryValueCallBacks		kCFLDictionaryValueCallBacksCFLTypes;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@constant	kCFLDictionaryValueCallBacksNull
	@abstract	Pre-defined constant structure for a collection representing Core Foundation objects.
*/
IMPORT_GLOBAL const CFLDictionaryValueCallBacks		kCFLDictionaryValueCallBacksNull;

#if 0
#pragma mark -
#pragma mark == Dictionary - Functions ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryGetTypeID
	@abstract	Obtains the Core Foundation type identifier for CFLDictionary objects.
	@result		The Core Foundation type identifier for CFLDictionary objects.
*/
CFLTypeID	CFLDictionaryGetTypeID( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryCreate
	@abstract	Creates an empty dictionary.
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	@discussion
	
	The caller of this function receives a reference to the returned object. The caller also implicitly retains the object, 
	and is responsible for releasing it. 
*/
OSStatus
	CFLDictionaryCreate( 
		CFLAllocatorRef 					inAllocator, 
		CFLIndex							inCapacity, 
		const CFLDictionaryKeyCallBacks *	inKeyCallBacks, 
		const CFLDictionaryValueCallBacks *	inValueCallBacks, 
		CFLDictionaryRef *					outRef );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryCreateCopy
	@abstract	Creates a dictionary with the items from another dictionary.
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	@discussion
	
	The caller of this function receives a reference to the returned object. The caller also implicitly retains the object, 
	and is responsible for releasing it. 
*/
OSStatus	CFLDictionaryCreateCopy( CFLAllocatorRef inAllocator, CFLDictionaryRef inDictionary, CFLDictionaryRef *outDictionary );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryGetCount
	@abstract	Returns the number of values currently in the dictionary. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLDictionaryGetCount( CFLDictionaryRef inObject, CFLIndex *outCount );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryGetValue
	@abstract	Retrieves the value associated with the given key. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLDictionaryGetValue( CFLDictionaryRef inObject, const void *inKey, void *outValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionarySetValue
	@abstract	Sets the value of the key in the dictionary.
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLDictionarySetValue( CFLDictionaryRef inObject, const void *inKey, const void *inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryRemoveValue
	@abstract	Adds a value if the key is not already present.
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLDictionaryAddValue( CFLDictionaryRef inObject, const void *inKey, const void *inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryRemoveValue
	@abstract	Removes the value of the key from the dictionary. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLDictionaryRemoveValue( CFLDictionaryRef inObject, const void *inKey );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryRemoveAllValues
	@abstract	Removes all the values from the dictionary, making it empty. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLDictionaryRemoveAllValues( CFLDictionaryRef inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryContainsKey
	@abstract	Determines if the specified key exists in the dictionary.
	@param		inObject	Dictionary to get the keys and values from.
	@param		inKey		Key to check for existence.
	@result		A Boolean value indicating that the key exists or not. 
*/
Boolean	CFLDictionaryContainsKey( CFLDictionaryRef inObject, const void *inKey );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryCopyKeysAndValues
	@abstract	Copies all the keys and values for a dictionary into newly malloc'd arrays.
	
	@param		inObject	Dictionary to get the keys and values from.
	
	@param		outKeys		Ptr to receive malloc'd array of void * keys. May be NULL if keys are not needed.
							The caller must be freed (with the ANSI C free function) the array on success if it is non-NULL.
	
	@param		outValues	Ptr to receive malloc'd array of void * values. May be NULL if values are not needed.
							The caller must be freed (with the ANSI C free function) the array on success if it is non-NULL.

	@param		outCount	Ptr to receive the number of entries in the dictionary. May be NULL if count is not needed.
	
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	
	@discussion
	
	Note: The keys and values in the returned arrays are not retained so they should be not be released. Only the arrays 
	themselves need to be freed (with the ANSI C free function) on success and if they are non-NULL.
*/
OSStatus	CFLDictionaryCopyKeysAndValues( CFLDictionaryRef inObject, void *outKeys, void *outValues, CFLIndex *outCount );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryGetKeysAndValues
	@abstract	Fills the two buffers with the keys and values from the dictionary.
	
	@param		inObject	Dictionary to get the keys and values from.
	
	@param		ioKeys		A C array of pointer-sized values to be filled with keys from the dictionary. The keys and 
							values C arrays are parallel to each other (that is, the items at the same indices form a 
							key-value pair from the dictionary). This parameter may be NULL if the keys are not desired.
							If this parameter is not a valid pointer to a C array of at least CFDictionaryGetCount() 
							pointers, or NULL, the behavior is undefined.
	
	@param		ioValues	A C array of pointer-sized values to be filled with values from the dictionary. The keys and 
							values C arrays are parallel to each other (that is, the items at the same indices form a 
							key-value pair from the dictionary). This parameter may be NULL if the values are not desired.
							If this parameter is not a valid pointer to a C array of at least CFDictionaryGetCount() 
							pointers, or NULL, the behavior is undefined.
	
	@result		An error code indicating failure reason or kNoErr (0) if successful.
*/
OSStatus	CFLDictionaryGetKeysAndValues( CFLDictionaryRef inObject, const void **ioKeys, const void **ioValues );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLDictionaryApplyFunction
	@abstract	Calls a function for each key/value pair in the dictionary.
*/
typedef void	( *CFLDictionaryApplierFunction )( const void *inKey, const void *inValue, void *inContext );

OSStatus	CFLDictionaryApplyFunction( CFLDictionaryRef inDict, CFLDictionaryApplierFunction, void *inContext );

#if 0
#pragma mark -
#pragma mark == Null ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLNullRef
	@abstract	The type of a reference to a CFLNull object.
	@constant	kCFLNull	Null object.
*/
typedef struct CFLNull *		CFLNullRef;

IMPORT_GLOBAL CFLNullRef		kCFLNull;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLNullGetTypeID
	@abstract	Obtains the Core Foundation type identifier for CFLNull.
	@result		The Core Foundation type identifier for CFLNull.
*/
CFLTypeID	CFLNullGetTypeID( void );

#if 0
#pragma mark -
#pragma mark == Number ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@enum		CFLNumberType
	@abstract	Type of number.
*/
typedef enum
{
	kCFLNumberInvalidType	= 0, 
	kCFLNumberSInt8Type		= 1,
	kCFLNumberSInt16Type	= 2,
	kCFLNumberSInt32Type	= 3,
	kCFLNumberSInt64Type	= 4,
	kCFLNumberSInt128Type	= 15, 
	kCFLNumberCharType		= 7,
	kCFLNumberShortType		= 8,
	kCFLNumberIntType		= 9,
	kCFLNumberLongType		= 10,
	kCFLNumberLongLongType	= 11,
#if( CFL_FLOATING_POINT_NUMBERS )
	kCFLNumberFloat32Type	= 5,
	kCFLNumberFloat64Type	= 6,
	kCFLNumberFloatType		= 12,
	kCFLNumberDoubleType	= 13,
#endif
	kCFLNumberCFIndexType	= 14,
	kCFLNumberMaxType		= 15
	
}	CFLNumberType;

#if( CFL_FLOATING_POINT_NUMBERS )
	#define CFLNumberTypeIsFloatType( TYPE ) \
		( ( (TYPE) == kCFLNumberFloat32Type )	|| \
		  ( (TYPE) == kCFLNumberFloat64Type )	|| \
		  ( (TYPE) == kCFLNumberFloatType )		|| \
		  ( (TYPE) == kCFLNumberDoubleType ) )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLNumberRef
	@abstract	The type of a reference to a CFLNumber object. 
	@discussion	CFLNumber objects are used to wrap numerical values for use in CF property lists and collection types.
*/
typedef struct CFLNumber *		CFLNumberRef;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLNumberGetTypeID
	@abstract	Obtains the Core Foundation type identifier for CFLNumber objects. 
	@result		The Core Foundation type identifier for CFLNumber objects. 
*/
CFLTypeID	CFLNumberGetTypeID( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLNumberCreateWithInteger
	@abstract	Creates a CFLNumber object using the specified integer value. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	@discussion
	
	The caller of this function receives a reference to the returned object. The caller also implicitly retains the object, 
	and is responsible for releasing it. 
*/
OSStatus	CFLNumberCreate( CFLAllocatorRef inAllocator, CFLNumberType inType, const void *inValue, CFLNumberRef *outRef );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLNumberGetByteSize
	@abstract	Gets the number of bytes for the number's value.
	@result		Number of bytes or 0 if the object is invalid.
*/
CFLIndex	CFLNumberGetByteSize( CFLNumberRef inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLNumberGetType
	@abstract	Gets the type of a number.
	@result		Type of number or kCFLNumberInvalidType if the object is invalid.
*/
CFLNumberType	CFLNumberGetType( CFLNumberRef inObject );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLNumberGetValue
	@abstract	Obtains the value of the specified CFLNumber object.
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLNumberGetValue( CFLNumberRef inObject, CFLNumberType inType, void *outValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLNumberIsFloatType
	@abstract	Returns true if the number is valid and a floating point number. Returns false otherwise.
*/
#if( CFL_FLOATING_POINT_NUMBERS )
	Boolean	CFLNumberIsFloatType( CFLNumberRef inNumber );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLNumberCompare
	@abstract	Returns < 0 if left < right, 0 if left == right, and > 0 if left > right.
*/
int	CFLNumberCompare( CFLNumberRef inLeft, CFLNumberRef inRight );

#if 0
#pragma mark -
#pragma mark == String ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	CFLSTR
	@abstract	Creates a compile-time constant string object.
	
	@discussion
	
	CFLSTR(), not being a "Copy" or "Create" function, does not return a new reference for you. So, you should not release 
	the return value. This is much like constant C or Pascal strings --- when you use "hello world" in a program, you do 
	not free it.
	
	However, strings returned from CFLSTR() can be retained and released in a properly nested fashion, just like any other 
	CF type. That is, if you pass a CFLSTR() return value to a function such as CFLDictionarySetValue(), the function can 
	retain it, then later, when it's done with it, it can release it.
*/
#define	kCFLStringConstantHeaderSize		8

#if( TARGET_RT_BIG_ENDIAN )
	#define	CFLSTR( X )		( (CFLStringRef)( ( "V\x07\x01\x00\x7F\xFF\xFF\xFF" X ) ) )
#else
	#define	CFLSTR( X )		( (CFLStringRef)( ( "V\x07\x01\x00\xFF\xFF\xFF\x7F" X ) ) )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLStringGetTypeID
	@abstract	Obtains the Core Foundation type identifier for CFLString objects. 
	@result		The Core Foundation type identifier for CFLString objects. 
*/
CFLTypeID	CFLStringGetTypeID( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLStringCreateWithText
	@abstract	Creates a CFLString object with the specified text. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.
	@discussion
	
	The caller of this function receives a reference to the returned object. The caller also implicitly retains the object, 
	and is responsible for releasing it. 
*/
OSStatus
	CFLStringCreateWithText( 
		CFLAllocatorRef 	inAllocator, 
		const void *		inText, 
		size_t 				inTextSize, 
		CFLStringRef *		outRef );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLStringGetLength
	@abstract	Obtains the number of characters in a CFLString object. 
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLStringGetLength( CFLStringRef inObject, CFLIndex *outLength );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLStringGetCStringPtr
	@abstract	Quickly obtains a pointer to a C-string buffer containing the characters of a CFLString object.
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLStringGetCStringPtr( CFLStringRef inObject, const char **outCString, size_t *outSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLStringSetText
	@abstract	Set the text of the object with the specified text.
	@result		An error code indicating failure reason or kNoErr (0) if successful.	
*/
OSStatus	CFLStringSetText( CFLStringRef inObject, const void *inText, size_t inTextSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLStringAppendText
	@abstract	Append text to the string.
*/
OSStatus	CFLStringAppendText( CFLStringRef inObject, const void *inText, size_t inSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLStringFindAndReplace
	@abstract	Replace all occurrences of inStringToFind in inString with inReplacementString.
	@discussion	NOTE: Not all options supported currently.
*/
CFLIndex
	CFLStringFindAndReplace( 
		CFLStringRef	inString, 
		CFLStringRef	inStringToFind, 
		CFLStringRef	inReplacementString, 
		CFLIndex		inLocation, 
		CFLIndex		inLength, 
		uint32_t		inCompareOptions );

#if 0
#pragma mark -
#pragma mark == Runtime ==
#endif

//===========================================================================================================================
//	Runtime
//===========================================================================================================================

typedef struct
{
	uint8_t		signature;
	uint8_t		type;
	uint8_t		flags;
	uint8_t		pad;
	int32_t		retainCount;
	
}	CFLObject;

check_compile_time( offsetof( CFLObject, signature )	== 0 );
check_compile_time( offsetof( CFLObject, type )			== 1 );
check_compile_time( offsetof( CFLObject, flags )		== 2 );
check_compile_time( offsetof( CFLObject, pad )			== 3 );
check_compile_time( offsetof( CFLObject, retainCount )	== 4 );
check_compile_time( sizeof( CFLObject )					== kCFLStringConstantHeaderSize );

typedef Boolean		( *CFLEqualCallBack )( CFLObjectRef inLeft, CFLObjectRef inRight );
typedef CFLHashCode	( *CFLHashCallBack )( CFLObjectRef inObject );
typedef void		( *CFLFreeCallBack )( CFLObjectRef inObject );

typedef struct
{
	const char *			name;
	CFLFreeCallBack			freeObj;
	CFLEqualCallBack		equal;
	CFLHashCallBack			hash;

}	CFLRuntimeClass;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLRuntimeFinalize
	@abstract	Releases all memory used by the runtime and puts things back into a default state.
*/
void	CFLRuntimeFinalize( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLRuntimeRegisterClass
	@abstract	Registers a class to allow for custom CFLite objects. Use with CFLRuntimeCreateInstance.
	
	@param		inClass		Class definition containing callbacks and other info to implement the custom class.
	@param		outTypeID	Returns a type ID for the custom class that can be used with CFLRuntimeCreateInstance.
*/
OSStatus	CFLRuntimeRegisterClass( const CFLRuntimeClass * const inClass, CFLTypeID *outTypeID );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLRuntimeCreateInstance
	@abstract	Creates an instance of a class registered with CFLRuntimeRegisterClass.
	
	@param		inAllocator		Use kCFLAllocatorDefault.
	@param		inTypeID		Type ID of the instance to create. Use the type ID returned from CFLRuntimeRegisterClass.
	@param		inExtraBytes	Number of bytes needed beyond the base class: sizeof( YourClass ) - sizeof( CFLObject ).
	@param		outObj			Receives the created CFLite object. Use CFRelease to release it.
*/
OSStatus	CFLRuntimeCreateInstance( CFLAllocatorRef inAllocator, CFLTypeID inTypeID, size_t inExtraBytes, void *outObj );

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLiteTest
	@abstract	Unit test.
*/
OSStatus	CFLiteTest( int inPrint );

#ifdef __cplusplus
}
#endif

#endif // __CFLite_h__
