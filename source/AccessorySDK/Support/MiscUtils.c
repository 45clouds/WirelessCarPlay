/*
	File:    	MiscUtils.c
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
	
	Copyright (C) 2001-2015 Apple Inc. All Rights Reserved.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "MiscUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "CFUtils.h"
#include "DataBufferUtils.h"
#include "MathUtils.h"
#include "RandomNumberUtils.h"
#include "PrintFUtils.h"
#include "StringUtils.h"
#include "TickUtils.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <ctype.h>
	#include <stdarg.h>
	#include <stddef.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
#endif

#if( TARGET_OS_DARWIN )
	#include <dlfcn.h>
	#include <sys/stat.h>
	
	#if( !COMMON_SERVICES_NO_CORE_SERVICES )
		#include <IOKit/IOKitLib.h>
		#include <Security/Security.h>
	#endif
#elif( TARGET_OS_NETBSD )
	#include <sys/stat.h>
#elif( TARGET_OS_BSD )
	#include <sys/stat.h>
#endif

#if( TARGET_MACH )
	#include <mach/mach_vm.h>
	#include <mach/mach.h>
	#include <mach/vm_map.h>
#endif

#if( TARGET_OS_POSIX )
	#include <ftw.h>
	#include <pthread.h>
	#include <pwd.h>
	#include <spawn.h>
	#include <sys/mman.h>
	#include <sys/sysctl.h>
	#include <sys/wait.h>
	#include <unistd.h>
#endif

#if( TARGET_OS_QNX )
	#include <devctl.h>
	#include <sys/procfs.h>
#endif

//===========================================================================================================================
//	External
//===========================================================================================================================

// Mac OS X shared libraries (e.g. bundles) cannot access "environ" directly because it's only available when the complete 
// program is linked so access has to go through _NSGetEnviron().

#if( TARGET_KERNEL )
	#define environ_compat()	kernel_environ
	
	char *		kernel_environ[ 1 ] = { NULL };
#elif( TARGET_MACH )
	#include <crt_externs.h>
	
	#define environ_compat()	( *_NSGetEnviron() )
#elif( !TARGET_OS_WINDOWS )
	extern char **		environ;
	
	#define environ_compat()	( environ )
#endif

#if 0
#pragma mark == FramesPerSecond ==
#endif

//===========================================================================================================================
//	FPSInit
//===========================================================================================================================

void	FPSInit( FPSData *inData, int inPeriods )
{
	inData->smoothingFactor = 2.0 / ( inPeriods + 1 );
	inData->ticksPerSecF	= (double) UpTicksPerSecond();
	inData->periodTicks		= UpTicksPerSecond();
	inData->lastTicks		= UpTicks();
	inData->totalFrameCount	= 0;
	inData->lastFrameCount	= 0;
	inData->lastFPS			= 0;
	inData->averageFPS		= 0;
}

//===========================================================================================================================
//	FPSUpdate
//===========================================================================================================================

void	FPSUpdate( FPSData *inData, uint32_t inFrameCount )
{
	uint64_t		nowTicks, deltaTicks;
	uint32_t		deltaFrames;
	double			deltaSecs;
	
	inData->totalFrameCount += inFrameCount;
	
	nowTicks   = UpTicks();
	deltaTicks = nowTicks - inData->lastTicks;
	if( deltaTicks >= inData->periodTicks )
	{
		deltaSecs				= deltaTicks / inData->ticksPerSecF;
		deltaFrames				= inData->totalFrameCount - inData->lastFrameCount;
		inData->lastFrameCount	= inData->totalFrameCount;
		inData->lastTicks		= nowTicks;
		inData->lastFPS			= deltaFrames / deltaSecs;
		inData->averageFPS		= ( ( 1.0 - inData->smoothingFactor ) * inData->averageFPS ) + 
										  ( inData->smoothingFactor   * inData->lastFPS );
	}
}

#if 0
#pragma mark -
#pragma mark == Misc ==
#endif

//===========================================================================================================================
//	qsort-compatibile comparison functions.
//===========================================================================================================================

DEFINE_QSORT_NUMERIC_COMPARATOR( int8_t,	qsort_cmp_int8 )
DEFINE_QSORT_NUMERIC_COMPARATOR( uint8_t,	qsort_cmp_uint8 )
DEFINE_QSORT_NUMERIC_COMPARATOR( int16_t,	qsort_cmp_int16 )
DEFINE_QSORT_NUMERIC_COMPARATOR( uint16_t,	qsort_cmp_uint16 )
DEFINE_QSORT_NUMERIC_COMPARATOR( int32_t,	qsort_cmp_int32 )
DEFINE_QSORT_NUMERIC_COMPARATOR( uint32_t,	qsort_cmp_uint32 )
DEFINE_QSORT_NUMERIC_COMPARATOR( int64_t,	qsort_cmp_int64 )
DEFINE_QSORT_NUMERIC_COMPARATOR( uint64_t,	qsort_cmp_uint64 )
DEFINE_QSORT_NUMERIC_COMPARATOR( float,		qsort_cmp_float )
DEFINE_QSORT_NUMERIC_COMPARATOR( double,	qsort_cmp_double )

//===========================================================================================================================
//	QSortPtrs
//
//	QuickSort code derived from the simple quicksort code from the book "The Practice of Programming".
//===========================================================================================================================

void	QSortPtrs( void *inPtrArray, size_t inPtrCount, ComparePtrsFunctionPtr inCmp, void *inContext )
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
	
	QSortPtrs( ptrArray, last, inCmp, inContext );
	QSortPtrs( &ptrArray[ last + 1 ], ( inPtrCount - last ) - 1, inCmp, inContext );
}

int	CompareIntPtrs( const void *inLeft, const void *inRight, void *inContext )
{
	int const		a = *( (const int *) inLeft );
	int const		b = *( (const int *) inRight );
	
	(void) inContext;
	
	return( ( a > b ) - ( a < b ) );
}

int	CompareStringPtrs( const void *inLeft, const void *inRight, void *inContext )
{
	(void) inContext; // Unused
	
	return( strcmp( (const char *) inLeft, (const char *) inRight ) );
}

//===========================================================================================================================
//	MemReverse
//===========================================================================================================================

void	MemReverse( const void *inSrc, size_t inLen, void *inDst )
{
	check( ( inSrc == inDst ) || !PtrsOverlap( inSrc, inLen, inDst, inLen ) );
	
	if( inSrc == inDst )
	{
		if( inLen > 1 )
		{
			uint8_t *		left  = (uint8_t *) inDst;
			uint8_t *		right = left + ( inLen - 1 );
			uint8_t			temp;
		
			while( left < right )
			{
				temp		= *left;
				*left++		= *right;
				*right--	= temp;
			}
		}
	}
	else
	{
		const uint8_t *		src = (const uint8_t *) inSrc;
		const uint8_t *		end = src + inLen;
		uint8_t *			dst = (uint8_t *) inDst;
		
		while( src < end )
		{
			*dst++ = *( --end );
		}
	}
}

//===========================================================================================================================
//	Swap16Mem
//===========================================================================================================================

void	Swap16Mem( const void *inSrc, size_t inLen, void *inDst )
{
	const uint16_t *			src = (const uint16_t *) inSrc;
	const uint16_t * const		end = src + ( inLen / 2 );
	uint16_t *					dst = (uint16_t *) inDst;
	
	check( ( inLen % 2 ) == 0 );
	check( IsPtrAligned( src, 2 ) );
	check( IsPtrAligned( dst, 2 ) );
	check( ( src == dst ) || !PtrsOverlap( src, inLen, dst, inLen ) );
	
	while( src != end )
	{
		*dst++ = ReadSwap16( src );
		++src;
	}
}

//===========================================================================================================================
//	SwapUUID
//===========================================================================================================================

void	SwapUUID( const void *inSrc, void *inDst )
{
	uint8_t * const		dst = (uint8_t *) inDst;
	uint8_t				tmp[ 16 ];
	
	check( ( inSrc == inDst ) || !PtrsOverlap( inSrc, 16, inDst, 16 ) );
	
	memcpy( tmp, inSrc, 16 );
	dst[  0 ] = tmp[  3 ];
	dst[  1 ] = tmp[  2 ];
	dst[  2 ] = tmp[  1 ];
	dst[  3 ] = tmp[  0 ];
	dst[  4 ] = tmp[  5 ];
	dst[  5 ] = tmp[  4 ];
	dst[  6 ] = tmp[  7 ];
	dst[  7 ] = tmp[  6 ];
	dst[  8 ] = tmp[  8 ];
	dst[  9 ] = tmp[  9 ];
	dst[ 10 ] = tmp[ 10 ];
	dst[ 11 ] = tmp[ 11 ];
	dst[ 12 ] = tmp[ 12 ];
	dst[ 13 ] = tmp[ 13 ];
	dst[ 14 ] = tmp[ 14 ];
	dst[ 15 ] = tmp[ 15 ];
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CopySmallFile
//===========================================================================================================================

#if( TARGET_HAS_STD_C_LIB )
OSStatus	CopySmallFile( const char *inSrcPath, const char *inDstPath )
{
	OSStatus		err;
	FILE *			srcFile;
	FILE *			dstFile;
	size_t			bufSize;
	uint8_t *		buf;
	size_t			readSize;
	size_t			writtenSize;
	
	dstFile	= NULL;
	buf		= NULL;
	
	srcFile = fopen( inSrcPath, "rb" );
	err = map_global_value_errno( srcFile, srcFile );
	require_noerr( err, exit );
	
	dstFile = fopen( inDstPath, "wb" );
	err = map_global_value_errno( dstFile, dstFile );
	require_noerr( err, exit );
	
	bufSize = 256 * 1024;
	buf = (uint8_t *) malloc( bufSize );
	require_action( buf, exit, err = kNoMemoryErr );
	
	for( ;; )
	{
		readSize = fread( buf, 1, bufSize, srcFile );
		if( readSize == 0 ) break;
		
		writtenSize = fwrite( buf, 1, readSize, dstFile );
		err = map_global_value_errno( writtenSize == readSize, dstFile );
		require_noerr( err, exit );
	}
	
exit:
	if( buf )		free( buf );
	if( dstFile )	fclose( dstFile );
	if( srcFile )	fclose( srcFile );
	return( err );
}
#endif

//===========================================================================================================================
//	CopyFileDataByFile
//===========================================================================================================================

OSStatus	CopyFileDataByFile( FILE *inFile, char **outPtr, size_t *outLen )
{
	OSStatus		err;
	char *			buf = NULL;
	size_t			maxLen;
	size_t			offset;
	char *			tmp;
	size_t			len;
	
	maxLen = 0;
	offset = 0;
	for( ;; )
	{
		if( offset >= maxLen )
		{
			if(      maxLen <  160000 )	maxLen = 160000;
			else if( maxLen < 4000000 )	maxLen *= 2;
			else						add_saturate( maxLen, 4000000, SIZE_MAX );
			require_action( maxLen < SIZE_MAX, exit, err = kSizeErr );
			tmp = (char *) realloc( buf, maxLen + 1 );
			require_action( tmp, exit, err = kNoMemoryErr );
			buf = tmp;
		}
		
		len = fread( &buf[ offset ], 1, maxLen - offset, inFile );
		if( len == 0 ) break;
		offset += len;
	}
	
	// Shrink the buffer to avoid wasting memory and null terminate.
	
	tmp = (char *) realloc( buf, offset + 1 );
	require_action( tmp, exit, err = kNoMemoryErr );
	buf = tmp;
	buf[ offset ] = '\0';
	
	*outPtr = buf;
	if( outLen ) *outLen = offset;
	buf = NULL;
	err = kNoErr;
	
exit:
	if( buf ) free( buf );
	return( err );
}

//===========================================================================================================================
//	CopyFileDataByPath
//===========================================================================================================================

OSStatus	CopyFileDataByPath( const char *inPath, char **outPtr, size_t *outLen )
{
	OSStatus		err;
	FILE *			file;
	
	file = fopen( inPath, "rb" );
	err = map_global_value_errno( file, file );
	require_noerr_quiet( err, exit );
	
	err = CopyFileDataByFile( file, outPtr, outLen );
	fclose( file );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	ReadANSIFile
//===========================================================================================================================

OSStatus	ReadANSIFile( FILE *inFile, void *inBuf, size_t inSize, size_t *outSize )
{
	uint8_t *		ptr;
	size_t			n;
	
	ptr = (uint8_t *) inBuf;
	while( inSize > 0 )
	{
		n = fread( ptr, 1, inSize, inFile );
		if( n == 0 ) break;
		ptr    += n;
		inSize -= n;
	}
	if( outSize ) *outSize = (size_t)( ptr - ( (uint8_t *) inBuf ) );
	return( kNoErr );
}

//===========================================================================================================================
//	WriteANSIFile
//===========================================================================================================================

OSStatus	WriteANSIFile( FILE *inFile, const void *inBuf, size_t inSize )
{
	const uint8_t *		ptr;
	size_t				n;
	
	ptr = (const uint8_t *) inBuf;
	while( inSize > 0 )
	{
		n = fwrite( ptr, 1, inSize, inFile );
		if( n == 0 ) break;
		ptr    += n;
		inSize -= n;
	}
	return( kNoErr );
}

//===========================================================================================================================
//	CreateTXTRecordWithCString
//===========================================================================================================================

OSStatus	CreateTXTRecordWithCString( const char *inString, uint8_t **outTXTRec, size_t *outTXTLen )
{
	OSStatus			err;
	DataBuffer			dataBuf;
	const char *		src;
	const char *		end;
	char				buf[ 256 ];
	size_t				len;
	uint8_t *			ptr;
	
	DataBuffer_Init( &dataBuf, NULL, 0, SIZE_MAX );
	
	src = inString;
	end = src + strlen( src );
	while( ParseQuotedEscapedString( src, end, " ", buf, sizeof( buf ), &len, NULL, &src ) )
	{
		err = DataBuffer_Grow( &dataBuf, 1 + len, &ptr );
		require_noerr( err, exit );
		
		*ptr++ = (uint8_t) len;
		memcpy( ptr, buf, len );
	}
	
	err = DataBuffer_Detach( &dataBuf, outTXTRec, outTXTLen );
	require_noerr( err, exit );
	
exit:
	DataBuffer_Free( &dataBuf );
	return( err );	
}

//===========================================================================================================================
//	TXTRecordGetNextItem
//===========================================================================================================================

Boolean
	TXTRecordGetNextItem( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		const char **		outKeyPtr, 
		size_t *			outKeyLen, 
		const uint8_t **	outValuePtr, 
		size_t *			outValueLen, 
		const uint8_t **	outSrc )
{
	const uint8_t *		end;
	const uint8_t *		keyPtr;
	const uint8_t *		keyEnd;
	size_t				len;
	
	if( inSrc >= inEnd )
	{
		return( false );
	}
	
	len = *inSrc++;
	end = inSrc + len;
	if( end > inEnd )
	{
		len = (size_t)( inEnd - inSrc );
		dlogassert( "bad TXT record: \n%1.1H", inSrc, len, 256 );
		return( false );
	}
	
	for( keyPtr = inSrc; ( inSrc < end ) && ( *inSrc != '=' ); ++inSrc ) {}
	keyEnd = inSrc;
	if( inSrc < end ) inSrc += 1; // Skip '='.
	
	*outKeyPtr		= (const char *) keyPtr;
	*outKeyLen		= (size_t)( keyEnd - keyPtr );
	*outValueLen	= (size_t)( end - inSrc );
	*outValuePtr	= inSrc;
	*outSrc			= end;
	return( true );
}

#if( TARGET_HAS_C_LIB_IO )
//===========================================================================================================================
//	fcopyline
//===========================================================================================================================

OSStatus	fcopyline( FILE *inFile, char **outLine, size_t *outLen )
{
	OSStatus		err;
	char *			line = NULL;
	size_t			maxLen, len;
	char *			dst;
	char *			ptr;
	size_t			offset;
	
	require_action_quiet( !feof( inFile ), exit, err = kEndingErr );
	
	maxLen = 128;
	line = (char *) malloc( maxLen );
	require_action( line, exit, err = kNoMemoryErr );
	
	offset = 0;
	for( ;; )
	{
		dst = line + offset;
		len = maxLen - offset;
		memset( dst, '\n', len );
		ptr = fgets( dst, (int) len, inFile );
		if( !ptr )
		{
			if( feof( inFile ) )
			{
				require_action_quiet( offset > 0, exit, err = kEndingErr );
				break;
			}
			err = errno_safe();
			goto exit;
		}
		
		// If no newline is found, all of our memset newlines were overwritten with data (and a null).
		// This means the line is longer than our buffer so we need to grow the buffer and continue reading.
		
		ptr = memchr( dst, '\n', len );
		if( !ptr )
		{
			require_action( maxLen <= ( SIZE_MAX / 2 ), exit, err = kSizeErr );
			offset = maxLen - 1;
			maxLen *= 2;
			ptr = (char *) realloc( line, maxLen );
			require_action( ptr, exit, err = kNoMemoryErr );
			line = ptr;
			continue;
		}
		
		// We have a full line, but if the file ended without a newline, the newline may be from our memset.
		// If the newline is not at the end and it's followed by a null then fgets wrote the null and the newline
		// is the true end of the line. Otherwise, the newline is from our memset and fgets wrote a null right before
		// it so the character before the null is the true end of the line.
		
		if( ( ( ptr + 1 ) < ( dst + len ) ) && ( ptr[ 1 ] == '\0' ) )
		{
			offset = (size_t)( ptr - line );
			*ptr = '\0';
		}
		else
		{
			offset = (size_t)( ( ptr - 1 ) - line );
		}
		break;
	}
	
	if( outLine )
	{
		*outLine = line;
		line = NULL;
	}
	if( outLen ) *outLen = offset;
	err = kNoErr;
	
exit:
	FreeNullSafe( line );
	return( err );
}
#endif // TARGET_HAS_C_LIB_IO

//===========================================================================================================================
//	GetHomePath
//===========================================================================================================================

#if( TARGET_OS_POSIX )
char *	GetHomePath( char *inBuffer, size_t inMaxLen )
{
	char *				path = NULL;
	long				len;
	char *				buf;
	struct passwd		pwdStorage;
	struct passwd *		pwdPtr;
	
	if( inMaxLen < 1 ) return( "" );
	*inBuffer = '\0';
	
	len = sysconf( _SC_GETPW_R_SIZE_MAX );
	if( ( len <= 0 ) || ( len > SSIZE_MAX ) ) len = 4096;
	
	buf = (char *) malloc( (size_t) len );
	check( buf );
	if( buf )
	{
		pwdPtr = NULL;
		if( ( getpwuid_r( getuid(), &pwdStorage, buf, (size_t) len, &pwdPtr ) == 0 ) && pwdPtr )
		{
			path = pwdPtr->pw_dir;
		}
	}
	if( !path ) path = getenv( "HOME" );
	if( !path ) path = ( getuid() == 0 ) ? "/root" : ".";
	strlcpy( inBuffer, path, inMaxLen );
	if( buf ) free( buf );
	return( path );
}
#endif

#if( TARGET_OS_WINDOWS )
//===========================================================================================================================
//	getprogname
//===========================================================================================================================

const char *	getprogname( void )
{
	static Boolean					sInitialized;
	static CRITICAL_SECTION			sProgramNameLock;
	static LONG						sProgramNameLockState = 0;
	static char						sProgramName[ 64 ];
	
	InitializeCriticalSectionOnce( &sProgramNameLock, &sProgramNameLockState );	
	EnterCriticalSection( &sProgramNameLock );
	if( !sInitialized )
	{
		TCHAR		path[ PATH_MAX + 1 ];
		TCHAR *		name;
		TCHAR *		extension;
		TCHAR		c;
		char *		dst;
		char *		lim;
		
		path[ 0 ] = '?';
		GetModuleFileName( NULL, path, (DWORD) countof( path ) );
		path[ countof( path ) - 1 ] = '\0';
		
		name = _tcsrchr( path, '\\' );
		if( name )	name += 1;
		else		name = path;
		
		extension = _tcsrchr( name, '.' );
		if( extension ) *extension = '\0';

		dst = sProgramName;
		lim = dst + ( countof( sProgramName ) - 1 );
		while( ( ( c = *name++ ) != '\0' ) && ( dst < lim ) )
		{
			if( ( c < 32 ) || ( c > 126 ) ) continue;
			*dst++ = (char) c;
		}
		*dst = '\0';
		
		sInitialized = true;
	}
	LeaveCriticalSection( &sProgramNameLock );
	return( sProgramName );
}
#endif

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
//===========================================================================================================================
//	IOKitCopyParentOfClass
//===========================================================================================================================

io_object_t	IOKitCopyParentOfClass( io_object_t inService, const char *inClassName, OSStatus *outErr )
{
	io_object_t		service, parent;
	OSStatus		err;
	
	service = inService;
	for( ;; )
	{
		err = IORegistryEntryGetParentEntry( service, kIOServicePlane, &parent );
		if( service != inService ) IOObjectRelease( service );
		if( err ) { service = IO_OBJECT_NULL; break; }
		service = parent;
		if( IOObjectConformsTo( service, inClassName ) ) break;
	}
	
	if( outErr ) *outErr = err;
	return( service );
}
#endif

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	mkparent
//===========================================================================================================================

OSStatus	mkparent( const char *inPath, mode_t inMode )
{
	OSStatus		err;
	char			parentPath[ PATH_MAX ];
	
	err = GetParentPath( inPath, kSizeCString, parentPath, sizeof( parentPath ), NULL );
	require_noerr_quiet( err, exit );
	if( *parentPath != '\0' )
	{
		err = mkpath( parentPath, inMode, inMode );
		require_noerr_quiet( err, exit );
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	mkpath
//
//	Copied from the mkdir tool and tweaked to work with const paths and without console output.
//===========================================================================================================================

int	mkpath( const char *path, mode_t mode, mode_t dir_mode )
{
	char buf[PATH_MAX];
	size_t len;
	struct stat sb;
	char *slash;
	int done, err;

	len = strlen(path);
	if(len > (sizeof(buf) - 1)) len = sizeof(buf) - 1;
	memcpy(buf, path, len);
	buf[len] = '\0';
	slash = buf;

	for (;;) {
		slash += strspn(slash, "/");
		slash += strcspn(slash, "/");
		done = (*slash == '\0');
		*slash = '\0';

		err = mkdir(buf, done ? mode : dir_mode);
		if ((err < 0) && (errno != EEXIST)) {
			/*
			 * Can't create; path exists or no perms.
			 * stat() path to determine what's there now.
			 */
			err = errno;
			if (stat(buf, &sb) < 0) {
				/* Not there; use mkdir()'s error */
				return err ? err : -1;
			}
			if (!S_ISDIR(sb.st_mode)) {
				/* Is there, but isn't a directory */
				return ENOTDIR;
			}
		} else if (done) {
			/*
			 * Created ok, and this is the last element
			 */
			/*
			 * The mkdir() and umask() calls both honor only the
			 * file permission bits, so if you try to set a mode
			 * including the sticky, setuid, setgid bits you lose
			 * them. So chmod().
			 */
			if ((mode & ~(S_IRWXU|S_IRWXG|S_IRWXO)) != 0 &&
				chmod(buf, mode) == -1) {
				err = errno;
				return err ? err : -1;
			}
		}
		if (done) {
			break;
		}
		*slash = '/';
	}
	return 0;
}
#endif // TARGET_OS_POSIX

//===========================================================================================================================
//	NormalizePath
//===========================================================================================================================

#if( TARGET_OS_POSIX )
char *	NormalizePath( const char *inSrc, size_t inLen, char *inDst, size_t inMaxLen, uint32_t inFlags )
{
	const char *		src = inSrc;
	const char *		end = ( inLen == kSizeCString ) ? ( inSrc + strlen( inSrc ) ) : ( inSrc + inLen );
	const char *		ptr;
	char *				dst;
	char *				lim;
	size_t				len;
	char				buf1[ PATH_MAX ];
	char				buf2[ PATH_MAX ];
	const char *		replacePtr;
	const char *		replaceEnd;
	
	// If the path is exactly "~" then expand to the current user's home directory.
	// If the path is exactly "~user" then expand to "user"'s home directory.
	// If the path begins with "~/" then expand the "~/" to the current user's home directory.
	// If the path begins with "~user/" then expand the "~user/" to "user"'s home directory.
	
	dst = buf1;
	lim = dst + ( sizeof( buf1 ) - 1 );
	if( !( inFlags & kNormalizePathDontExpandTilde ) && ( src < end ) && ( *src == '~' ) )
	{
		replacePtr = NULL;
		replaceEnd = NULL;
		for( ptr = inSrc + 1; ( src < end ) && ( *src != '/' ); ++src ) {}
		len = (size_t)( src - ptr );
		if( len == 0 ) // "~" or "~/". Expand to current user's home directory.
		{
			replacePtr = getenv( "HOME" );
			if( replacePtr ) replaceEnd = replacePtr + strlen( replacePtr );
		}
		else // "~user" or "~user/". Expand to "user"'s home directory.
		{
			struct passwd *		pw;
			
			len = Min( len, sizeof( buf2 ) - 1 );
			memcpy( buf2, ptr, len );
			buf2[ len ] = '\0';
			pw = getpwnam( buf2 );
			if( pw && pw->pw_dir )
			{
				replacePtr = pw->pw_dir;
				replaceEnd = replacePtr + strlen( replacePtr );
			}
		}
		if( replacePtr ) while( ( dst < lim ) && ( replacePtr < replaceEnd ) ) *dst++ = *replacePtr++;
		else src = inSrc;
	}
	while( ( dst < lim ) && ( src < end ) ) *dst++ = *src++;
	*dst = '\0';
	
	// Resolve the path to remove ".", "..", and sym links.
	
	if( !( inFlags & kNormalizePathDontResolve ) )
	{
		if( inMaxLen >= PATH_MAX )
		{
			dst = realpath( buf1, inDst );
			if( !dst ) strlcpy( inDst, buf1, inMaxLen );
		}
		else
		{
			dst = realpath( buf1, buf2 );
			strlcpy( inDst, dst ? buf2 : buf1, inMaxLen );
		}
	}
	else
	{
		strlcpy( inDst, buf1, inMaxLen );
	}
	return( ( inMaxLen > 0 ) ? inDst : "" );
}
#endif // TARGET_OS_POSIX

//===========================================================================================================================
//	NumberListStringCreateFromUInt8Array
//===========================================================================================================================

#if( TARGET_HAS_STD_C_LIB )

static int	__NumberListStringCreateFromUInt8ArraySorter( const void *inLeft, const void *inRight );

OSStatus	NumberListStringCreateFromUInt8Array( const uint8_t *inArray, size_t inCount, char **outStr )
{
	OSStatus		err;
	DataBuffer		dataBuf;
	uint8_t *		sortedArray;
	size_t			i;
	uint8_t			x;
	uint8_t			y;
	uint8_t			z;
	char			buf[ 32 ];
	char *			ptr;
	
	sortedArray = NULL;
	DataBuffer_Init( &dataBuf, NULL, 0, SIZE_MAX );
	
	if( inCount > 0 )
	{
		sortedArray = (uint8_t *) malloc( inCount * sizeof( *inArray ) );
		require_action( sortedArray, exit, err = kNoMemoryErr );
		
		memcpy( sortedArray, inArray, inCount * sizeof( *inArray ) );
		qsort( sortedArray, inCount, sizeof( *sortedArray ), __NumberListStringCreateFromUInt8ArraySorter );
		
		i = 0;
		while( i < inCount )
		{
			x = sortedArray[ i++ ];
			y = x;
			while( ( i < inCount ) && ( ( ( z = sortedArray[ i ] ) - y ) <= 1 ) )
			{
				y = z;
				++i;
			}
			
			ptr = buf;
			if( x == y )		ptr  += snprintf( ptr, sizeof( buf ), "%d", x );
			else				ptr  += snprintf( ptr, sizeof( buf ), "%d-%d", x, y );
			if( i < inCount )  *ptr++ = ',';
			
			err = DataBuffer_Append( &dataBuf, buf, (size_t)( ptr - buf ) );
			require_noerr( err, exit );
		}
	}
	
	err = DataBuffer_Append( &dataBuf, "", 1 );
	require_noerr( err, exit );
	
	*outStr = (char *) dataBuf.bufferPtr;
	dataBuf.bufferPtr = NULL;
	
exit:
	if( sortedArray ) free( sortedArray );
	DataBuffer_Free( &dataBuf );
	return( err );
}

static int	__NumberListStringCreateFromUInt8ArraySorter( const void *inLeft, const void *inRight )
{
	return( *( (const uint8_t *)  inLeft ) - *( (const uint8_t *)  inRight ) );
}
#endif // TARGET_HAS_STD_C_LIB

#if( TARGET_OS_BSD && !TARGET_KERNEL )
//===========================================================================================================================
//	GetProcessNameByPID
//===========================================================================================================================

char *	GetProcessNameByPID( pid_t inPID, char *inNameBuf, size_t inMaxLen )
{
	OSStatus				err;
	int						mib[ 4 ];
	struct kinfo_proc		info;
	size_t					len;
	
	if( inMaxLen < 1 ) return( "" );
	
	mib[ 0 ] = CTL_KERN;
	mib[ 1 ] = KERN_PROC;
	mib[ 2 ] = KERN_PROC_PID;
	mib[ 3 ] = (int) inPID;
	
	memset( &info, 0, sizeof( info ) );
	len = sizeof( info );
	err = sysctl( mib, 4, &info, &len, NULL, 0 );
	err = map_global_noerr_errno( err );
	if( !err )	strlcpy( inNameBuf, info.kp_proc.p_comm, inMaxLen );
	else		*inNameBuf = '\0';
	return( inNameBuf );
}
#endif

#if( TARGET_OS_LINUX )
//===========================================================================================================================
//	GetProcessNameByPID
//===========================================================================================================================

char *	GetProcessNameByPID( pid_t inPID, char *inNameBuf, size_t inMaxLen )
{
	char		path[ PATH_MAX ];
	FILE *		file;
	char *		ptr;
	size_t		len;
	
	if( inMaxLen < 1 ) return( "" );
	
	snprintf( path, sizeof( path ), "/proc/%lld/cmdline", (long long) inPID );
	file = fopen( path, "r" );
	*path = '\0';
	if( file )
	{
		ptr = fgets( path, sizeof( path ), file );
		if( !ptr ) *path = '\0';
		fclose( file );
	}
	ptr = strrchr( path, '/' );
	ptr = ptr ? ( ptr + 1 ) : path;
	len = strlen( ptr );
	if( len >= inMaxLen ) len = inMaxLen - 1;
	memcpy( inNameBuf, ptr, len );
	inNameBuf[ len ] = '\0';
	return( inNameBuf );
}
#endif

#if( TARGET_OS_QNX )
//===========================================================================================================================
//	GetProcessNameByPID
//===========================================================================================================================

char *	GetProcessNameByPID( pid_t inPID, char *inNameBuf, size_t inMaxLen )
{
	OSStatus		err;
	char			path[ PATH_MAX + 1 ];
	int				fd;
	char *			ptr;
	size_t			len;
	struct
	{
		procfs_debuginfo	info;
		char				buf[ PATH_MAX ];
		
	}	name;
	
	if( inMaxLen < 1 ) return( "" );
	
	snprintf( path, sizeof( path ), "/proc/%lld/as", (long long) inPID );
	fd = open( path, O_RDONLY | O_NONBLOCK );
	err = map_fd_creation_errno( fd );
	check_noerr( err );
	if( !err )
	{
		err = devctl( fd, DCMD_PROC_MAPDEBUG_BASE, &name, sizeof( name ), NULL );
		check_noerr( err );
		close( fd );
	}
	if( !err )
	{
		ptr = strrchr( name.info.path, '/' );
		ptr = ptr ? ( ptr + 1 ) : path;
		len = strlen( ptr );
	}
	else
	{
		ptr = "?";
		len = 1;
	}
	if( len >= inMaxLen ) len = inMaxLen - 1;
	memcpy( inNameBuf, ptr, len );
	inNameBuf[ len ] = '\0';
	return( inNameBuf );
}
#endif

//===========================================================================================================================
//	RemovePath
//===========================================================================================================================

#if( TARGET_OS_POSIX )
static int	_RemovePathCallBack( const char *inPath, const struct stat *inStat, int inFlags, struct FTW *inFTW );

OSStatus	RemovePath( const char *inPath )
{
	OSStatus		err;
	
	err = nftw( inPath, _RemovePathCallBack, 64, FTW_CHDIR | FTW_DEPTH | FTW_MOUNT | FTW_PHYS );
	err = map_global_noerr_errno( err );
	if( err == ENOTDIR )
	{
		err = remove( inPath );
		err = map_global_noerr_errno( err );
	}
	return( err );
}

static int	_RemovePathCallBack( const char *inPath, const struct stat *inStat, int inFlags, struct FTW *inFTW )
{
	int		err;
	
	(void) inStat;
	(void) inFlags;
	(void) inFTW;
	
	err = remove( inPath );
	err = map_global_noerr_errno( err );
	return( err );
}
#endif

#if( TARGET_HAS_C_LIB_IO )
//===========================================================================================================================
//	RollLogFiles
//===========================================================================================================================

OSStatus	RollLogFiles( FILE **ioLogFile, const char *inEndMessage, const char *inBaseName, int inMaxFiles )
{
	OSStatus		err;
	char			oldPath[ PATH_MAX + 1 ];
	char			newPath[ PATH_MAX + 1 ];
	int				i;
	
	// Append a message to the current log file so viewers know it has rolled then close it.
	
	if( ioLogFile && *ioLogFile )
	{
		if( inEndMessage ) fprintf( *ioLogFile, "%s", inEndMessage );
		fclose( *ioLogFile );
		*ioLogFile = NULL;
	}
	
	// Delete the oldest log file.
	
	snprintf( oldPath, sizeof( oldPath ), "%s.%d", inBaseName, inMaxFiles - 1 );
	remove( oldPath );
	
	// Shift all the log files down by 1.
	
	for( i = inMaxFiles - 2; i > 0; --i )
	{
		snprintf( oldPath, sizeof( oldPath ), "%s.%d", inBaseName, i );
		snprintf( newPath, sizeof( newPath ), "%s.%d", inBaseName, i + 1 );
		rename( oldPath, newPath );
	}
	if( inMaxFiles > 1 )
	{
		snprintf( newPath, sizeof( newPath ), "%s.%d", inBaseName, 1 );
		rename( inBaseName, newPath );
	}
	
	// Open a new, empty log file to continue logging.
	
	if( ioLogFile )
	{
		*ioLogFile = fopen( inBaseName, "w" );
		err = map_global_value_errno( *ioLogFile, *ioLogFile );
		require_noerr_quiet( err, exit );
	}
	err = kNoErr;
	
exit:
	return( err );
}
#endif

#if( TARGET_OS_WINDOWS )
//===========================================================================================================================
//	RunningWindowsVistaOrLater
//===========================================================================================================================

Boolean	RunningWindowsVistaOrLater( void )
{
	OSVERSIONINFOEX		osvi;
	DWORDLONG			conditionMask;

	memset( &osvi, 0, sizeof( osvi ) );
	osvi.dwOSVersionInfoSize	= sizeof( osvi );
	osvi.dwMajorVersion			= 6; // Windows Vista
	osvi.dwMinorVersion			= 0;
	
	conditionMask = 0;
	VER_SET_CONDITION( conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL );
	VER_SET_CONDITION( conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL );
	
	if( VerifyVersionInfo( &osvi, VER_MAJORVERSION | VER_MINORVERSION, conditionMask ) )
	{
		return( true );
	}
	return( false );
}
#endif

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	RunProcessAndCaptureOutput
//===========================================================================================================================

OSStatus	RunProcessAndCaptureOutput( const char *inCmdLine, char **outResponse )
{
	return( RunProcessAndCaptureOutputEx( inCmdLine, outResponse, NULL ) );
}

OSStatus	RunProcessAndCaptureOutputEx( const char *inCmdLine, char **outResponse, size_t *outLen )
{
	OSStatus		err;
	DataBuffer		dataBuf;
	
	DataBuffer_Init( &dataBuf, NULL, 0, SIZE_MAX );
	
	err = DataBuffer_RunProcessAndAppendOutput( &dataBuf, inCmdLine );
	require_noerr_quiet( err, exit );
	
	err = DataBuffer_Append( &dataBuf, "", 1 );
	require_noerr( err, exit );
	
	*outResponse = (char *) DataBuffer_GetPtr( &dataBuf );
	if( outLen ) *outLen =  DataBuffer_GetLen( &dataBuf );
	dataBuf.bufferPtr = NULL;
	
exit:
	DataBuffer_Free( &dataBuf );
	return( err );
}
#endif

//===========================================================================================================================
//	SCDynamicStoreCopyComputerName
//===========================================================================================================================

#if( !TARGET_OS_DARWIN && TARGET_OS_POSIX )
CFStringRef	SCDynamicStoreCopyLocalHostName( SCDynamicStoreRef inStore )
{
	char			buf[ 256 ];
	CFStringRef		cfStr;
	
	(void) inStore; // Unused
	
	buf[ 0 ] = '\0';
	gethostname( buf, sizeof( buf ) );
	buf[ sizeof( buf ) - 1 ] = '\0';
	
	cfStr = CFStringCreateWithCString( NULL, buf, kCFStringEncodingUTF8 );
	require( cfStr, exit );
		
exit:
	return( cfStr );
}
#endif

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	systemf
//===========================================================================================================================

int	systemf( const char *inLogPrefix, const char *inFormat, ... )
{
	int			err;
	va_list		args;
	char *		argv[] = { "/bin/sh", "-c", NULL, NULL };
	char *		cmd;
	pid_t		pid, pid2;
	int			status;
	
	cmd = NULL;
	va_start( args, inFormat );
	VASPrintF( &cmd, inFormat, args );
	va_end( args );
	require_action( cmd, exit, err = kUnknownErr );
	if( inLogPrefix ) fprintf( stderr, "%s%s\n", inLogPrefix, cmd );
	
	argv[ 2 ] = cmd;
	err = posix_spawn( &pid, argv[ 0 ], NULL, NULL, argv, environ_compat() );
	free( cmd );
	require_noerr_quiet( err, exit );
	do
	{
		pid2 = waitpid( pid, &status, 0 );
			
	}	while( ( pid2 == -1 ) && ( errno == EINTR ) );
	require_action_quiet( pid2 != -1, exit, err = errno_safe() );
	require_action_quiet( pid2 !=  0, exit, err = kUnexpectedErr );
	
	if(      WIFEXITED( status ) )		err = WEXITSTATUS( status );
	else if( WIFSIGNALED( status ) )	err = POSIXSignalToOSStatus( WTERMSIG( status ) );
	else if( WIFSTOPPED( status ) )		err = POSIXSignalToOSStatus( WSTOPSIG( status ) );
	else if( WIFCONTINUED( status ) )	err = POSIXSignalToOSStatus( SIGCONT );
	else								err = kUnknownErr;
	
exit:
	return( err );
}
#endif

#if 0
#pragma mark -
#pragma mark == Packing/Unpacking ==
#endif

//===========================================================================================================================
//	PackData
//===========================================================================================================================

OSStatus	PackData( void *inBuffer, size_t inMaxSize, size_t *outSize, const char *inFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = PackDataVAList( inBuffer, inMaxSize, outSize, inFormat, args );
	va_end( args );
	
	return( err );
}

//===========================================================================================================================
//	PackDataVAList
//===========================================================================================================================

OSStatus	PackDataVAList( void *inBuffer, size_t inMaxSize, size_t *outSize, const char *inFormat, va_list inArgs )
{
	OSStatus			err;
	const char *		fmt;
	char				c;
	const uint8_t *		src;
	const uint8_t *		end;
	uint8_t *			dst;
	uint8_t *			lim;
	uint8_t				u8;
	uint16_t			u16;
	uint32_t			u32;
	size_t				size;
	
	dst = (uint8_t *) inBuffer;
	lim = dst + inMaxSize;
	
	// Loop thru each character in the format string, decode it, and pack the data appropriately.
	
	fmt = inFormat;
	for( c = *fmt; c != '\0'; c = *++fmt )
	{
		int		altForm;
		
		// Ignore non-% characters like spaces since they can aid in format string readability.
		
		if( c != '%' ) continue;
		
		// Flags
		
		altForm = 0;
		for( ;; )
		{
			c = *++fmt;
			if( c == '#' ) altForm += 1;
			else break;
		}
		
		// Format specifiers.
		
		switch( c )
		{
			case 'b':	// %b: Write byte (8-bit); arg=unsigned int
				
				require_action( dst < lim, exit, err = kOverrunErr );
				u8 = (uint8_t) va_arg( inArgs, unsigned int );
				*dst++ = u8;
				break;
			
			case 'H':	// %H: Write big endian half-word (16-bit); arg=unsigned int
				
				require_action( ( lim - dst ) >= 2, exit, err = kOverrunErr );
				u16	= (uint16_t) va_arg( inArgs, unsigned int );
				*dst++ 	= (uint8_t)( ( u16 >>  8 ) & 0xFF );
				*dst++ 	= (uint8_t)(   u16         & 0xFF );
				break;
			
			case 'W':	// %W: Write big endian word (32-bit); arg=unsigned int
				
				require_action( ( lim - dst ) >= 4, exit, err = kOverrunErr );
				u32	= (uint32_t) va_arg( inArgs, unsigned int );
				*dst++ 	= (uint8_t)( ( u32 >> 24 ) & 0xFF );
				*dst++ 	= (uint8_t)( ( u32 >> 16 ) & 0xFF );
				*dst++ 	= (uint8_t)( ( u32 >>  8 ) & 0xFF );
				*dst++ 	= (uint8_t)(   u32         & 0xFF );
				break;
			
			case 's':	// %s/%#s: Write string/length byte-prefixed string; arg=const char *inStr (null-terminated)
						// Note: Null terminator is not written.
				
				src = va_arg( inArgs, const uint8_t * );
				require_action( src, exit, err = kParamErr );
				
				for( end = src; *end; ++end ) {}
				size = (size_t)( end - src );
				
				if( altForm ) // Pascal-style length byte-prefixed string
				{
					require_action( size <= 255, exit, err = kSizeErr );
					require_action( ( 1 + size ) <= ( (size_t)( lim - dst ) ), exit, err = kOverrunErr );
					*dst++ = (uint8_t) size;
				}
				else
				{
					require_action( size <= ( (size_t)( lim - dst ) ), exit, err = kOverrunErr );
				}
				while( src < end )
				{
					*dst++ = *src++;
				}
				break;
			
			case 'n':	// %n: Write N bytes; arg 1=const void *inData, arg 2=unsigned int inSize
				
				src = va_arg( inArgs, const uint8_t * );
				require_action( src, exit, err = kParamErr );
				
				size = (size_t) va_arg( inArgs, unsigned int );
				require_action( size <= ( (size_t)( lim - dst ) ), exit, err = kOverrunErr );
				
				end = src + size;
				while( src < end )
				{
					*dst++ = *src++;
				}
				break;
			
			default:
				dlogassert( "unknown format specifier: %%%c", c );
				err = kUnsupportedErr;
				goto exit;
		}
	}
	
	*outSize = (size_t)( dst - ( (uint8_t *) inBuffer ) );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	UnpackData
//===========================================================================================================================

OSStatus	UnpackData( const void *inData, size_t inSize, const char *inFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = UnpackDataVAList( inData, inSize, inFormat, args );
	va_end( args );
	
	return( err );
}

//===========================================================================================================================
//	UnpackDataVAList
//===========================================================================================================================

OSStatus	UnpackDataVAList( const void *inData, size_t inSize, const char *inFormat, va_list inArgs )
{
	OSStatus				err;
	const char *			fmt;
	char					c;
	const uint8_t *			src;
	const uint8_t *			end;
	const uint8_t *			ptr;
	uint16_t				u16;
	uint32_t				u32;
	size_t					size;
	uint8_t *				bArg;
	uint16_t *				hArg;
	uint32_t *				wArg;
	const uint8_t **		ptrArg;
	size_t *				sizeArg;
		
	src = (const uint8_t *) inData;
	end = src + inSize;
	
	// Loop thru each character in the format string, decode it, and unpack the data appropriately.
	
	fmt = inFormat;
	for( c = *fmt; c != '\0'; c = *++fmt )
	{
		int		altForm;
		
		// Ignore non-% characters like spaces since they can aid in format string readability.
		
		if( c != '%' ) continue;
		
		// Flags
		
		altForm = 0;
		for( ;; )
		{
			c = *++fmt;
			if( c == '#' ) altForm += 1;
			else break;
		}
		
		// Format specifiers.
		
		switch( c )
		{
			case 'b':	// %b: Read byte (8-bit); arg=uint8_t *outByte
				
				require_action( altForm == 0, exit, err = kFlagErr );
				require_action_quiet( ( end - src ) >= 1, exit, err = kUnderrunErr );
				bArg = va_arg( inArgs, uint8_t * );
				if( bArg ) *bArg = *src;
				++src;
				break;
			
			case 'H':	// %H: Read big endian half-word (16-bit); arg=uint16_t *outU16
				
				require_action( ( end - src ) >= 2, exit, err = kUnderrunErr );
				u16  = (uint16_t)( *src++ << 8 );
				u16 |= (uint16_t)( *src++ );
				hArg = va_arg( inArgs, uint16_t * );
				if( hArg ) *hArg = u16;
				break;
			
			case 'W':	// %H: Read big endian word (32-bit); arg=uint32_t *outU32
				
				require_action( ( end - src ) >= 4, exit, err = kUnderrunErr );
				u32  = (uint32_t)( *src++ << 24 );
				u32 |= (uint32_t)( *src++ << 16 );
				u32 |= (uint32_t)( *src++ << 8 );
				u32 |= (uint32_t)( *src++ );
				wArg = va_arg( inArgs, uint32_t * );
				if( wArg ) *wArg = u32;
				break;
			
			case 's':	// %s: Read string; arg 1=const char **outStr, arg 2=size_t *outSize (size excludes null terminator).
				
				if( altForm ) // Pascal-style length byte-prefixed string
				{
					require_action( src < end, exit, err = kUnderrunErr );
					size = *src++;
					require_action( size <= (size_t)( end - src ), exit, err = kUnderrunErr );
					
					ptr = src;
					src += size;
				}
				else
				{
					for( ptr = src; ( src < end ) && ( *src != 0 ); ++src ) {}
					require_action( src < end, exit, err = kUnderrunErr );
					size = (size_t)( src - ptr );
					++src;
				}
				
				ptrArg = va_arg( inArgs, const uint8_t ** );
				if( ptrArg ) *ptrArg = ptr;
				
				sizeArg	= va_arg( inArgs, size_t * );
				if( sizeArg ) *sizeArg = size;
				break;
			
			case 'n':	// %n: Read N bytes; arg 1=size_t inSize, arg 2=const uint8_t **outData
				
				size = (size_t) va_arg( inArgs, unsigned int );
				require_action( size <= (size_t)( end - src ), exit, err = kUnderrunErr );
				
				ptrArg = va_arg( inArgs, const uint8_t ** );
				if( ptrArg ) *ptrArg = src;
				
				src += size;
				break;
			
			default:
				dlogassert( "unknown format specifier: %%%c", c );
				err = kUnsupportedErr;
				goto exit;
		}
	}
	err = kNoErr;
	
exit:
	return( err );
}

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	PackingUnpackingTest
//===========================================================================================================================

OSStatus	PackingUnpackingTest( void );
OSStatus	PackingUnpackingTest( void )
{
	static const uint8_t		kData[] = 
	{
		0xAA, 
		0xBB, 0xCC, 
		0x11, 0x22, 0x33, 0x44, 
		0x68, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x70, 0x65, 0x6F, 0x70, 0x6C, 0x65, 	// hello people
		0x06, 0x70, 0x61, 0x73, 0x63, 0x61, 0x6C, 									// \6pascal
		0x74, 0x65, 0x73, 0x74, 0x73												// tests
	};
	OSStatus		err;
	uint8_t			buf[ 128 ];
	size_t			size;
	uint8_t			b;
	uint16_t		h;
	uint32_t		w;
	char *			s1Ptr;
	char *			s2Ptr;
	size_t			s2Len;
	uint8_t *		ptr;
	
	err = PackData( buf, sizeof( buf ), &size, "%b %H %W %s %#s %n", 0xAA, 0xBBCC, 0x11223344, "hello people", 
		"pascal", "tests", 5 );
	require_noerr( err, exit );
	require_action( size == 31, exit, err = kSizeErr );
	require_action( memcmp( buf, kData, size ) == 0, exit, err = kResponseErr );
	
	err = UnpackData( kData, sizeof( kData ), "%b %H %W %n %#s %n", &b, &h, &w, 12, &s1Ptr, &s2Ptr, &s2Len, 5, &ptr );
	require_noerr( err, exit );
	require_action( b == 0xAA, exit, err = kResponseErr );
	require_action( h == 0xBBCC, exit, err = kResponseErr );
	require_action( w == 0x11223344, exit, err = kResponseErr );
	require_action( memcmp( s1Ptr, "hello people", 12 ) == 0, exit, err = kSizeErr );
	require_action( ( s2Len == 6 ) && ( memcmp( s2Ptr, "pascal", s2Len ) == 0 ), exit, err = kSizeErr );
	require_action( memcmp( ptr, "tests", 5 ) == 0, exit, err = kResponseErr );

	
exit:
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#pragma mark == EDID ==
#endif

#if  ( TARGET_OS_DARWIN )
CFDataRef	CopyEDIDbyUUID( CFStringRef inUUID, OSStatus *outErr )
{
	(void) inUUID;
	
	if( outErr ) *outErr = kUnsupportedErr;
	return( NULL );
}
#endif

//===========================================================================================================================
//	CreateEDIDDictionaryWithBytes
//===========================================================================================================================

static OSStatus	_ParseEDID_CEABlock( const uint8_t *inData, CFMutableDictionaryRef inEDIDDict );
static OSStatus	_ParseEDID_CEABlock_HDMI( const uint8_t *inData, size_t inSize, CFMutableDictionaryRef inCAEDict );

CFDictionaryRef	CreateEDIDDictionaryWithBytes( const uint8_t *inData, size_t inSize, OSStatus *outErr )
{
	CFDictionaryRef				result		= NULL;
	CFMutableDictionaryRef		edidDict	= NULL;
	OSStatus					err;
	const uint8_t *				src;
	const uint8_t *				end;
	uint8_t						u8;
	uint32_t					u32;
	char						strBuf[ 256 ];
	const char *				strPtr;
	const char *				strEnd;
	char						c;
	CFStringRef					key;
	CFStringRef					cfStr;
	int							i;
	char *						dst;
	int							extensionCount;
	
	require_action_quiet( inSize >= 128, exit, err = kSizeErr );
	require_action_quiet( memcmp( inData, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00", 8 ) == 0, exit, err = kFormatErr );
	
	src = inData;
	end = src + 128;
	for( u8 = 0; src < end; ++src ) u8 += *src;
	require_action_quiet( u8 == 0, exit, err = kChecksumErr );
	
	edidDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( edidDict, exit, err = kNoMemoryErr ); 
	
	CFDictionarySetData( edidDict, kEDIDKey_RawBytes, inData, inSize );
	
	// Manufacturer. It's 3 characters that get encoded as 2 bytes.
	
	strBuf[ 0 ] = (char)( ( ( inData[ 8 ] & 0x7C ) >> 2 ) + '@' );
	strBuf[ 1 ] = (char)( ( ( inData[ 8 ] & 0x03 ) << 3 ) + ( ( inData[ 9 ] & 0xE0 ) >> 5 ) + '@' );
	strBuf[ 2 ] = (char)( (   inData[ 9 ] & 0x1F ) + '@' );
	strBuf[ 3 ] = '\0';
	
	cfStr = CFStringCreateWithCString( NULL, strBuf, kCFStringEncodingUTF8 );
	if( !cfStr )
	{
		// Malformed manufacturer so %-escape the raw bytes.
		
		snprintf( strBuf, sizeof( strBuf ), "<<%%%02X%%%02X>>", inData[ 8 ], inData[ 9 ] );
		cfStr = CFStringCreateWithCString( NULL, strBuf, kCFStringEncodingUTF8 );
		require_action( cfStr, exit, err = kNoMemoryErr );
	}
	CFDictionarySetValue( edidDict, kEDIDKey_Manufacturer, cfStr );
	CFRelease( cfStr );
	
	// Product ID
	
	u32 = ReadLittle16( &inData[ 10 ] );
	CFDictionarySetInt64( edidDict, kEDIDKey_ProductID, u32 );
	
	// Serial number
	
	u32 = ReadLittle32( &inData[ 12 ] );
	CFDictionarySetInt64( edidDict, kEDIDKey_SerialNumber, u32 );
	
	// Manufacture date (year and week).
	
	CFDictionarySetInt64( edidDict, kEDIDKey_WeekOfManufacture, inData[ 16 ] );
	CFDictionarySetInt64( edidDict, kEDIDKey_YearOfManufacture, 1990 + inData[ 17 ] );
	
	// EDID versions.
	
	CFDictionarySetInt64( edidDict, kEDIDKey_EDIDVersion, inData[ 18 ] );
	CFDictionarySetInt64( edidDict, kEDIDKey_EDIDRevision, inData[ 19 ] );
	
	// $$$ TO DO: Parse bytes 20-53.
	
	// Parse descriptor blocks.
	
	src = &inData[ 54 ]; // Descriptor Block 1 (4 of them total, 18 bytes each).
	for( i = 0; i < 4; ++i )
	{
		// If the first two bytes are 0 then it's a monitor descriptor.
		
		if( ( src[ 0 ] == 0 ) && ( src[ 1 ] == 0 ) )
		{
			key = NULL;
			u8 = src[ 3 ];
			if(      u8 == 0xFC ) key = kEDIDKey_MonitorName;
			else if( u8 == 0xFF ) key = kEDIDKey_MonitorSerialNumber;
			else {} // $$$ TO DO: parse other descriptor block types.
			if( key )
			{
				dst = strBuf;
				strPtr = (const char *) &src[  5 ];
				strEnd = (const char *) &src[ 18 ];
				while( ( strPtr < strEnd ) && ( ( c = *strPtr ) != '\n' ) && ( c != '\0' ) )
				{
					c = *strPtr++;
					if( ( c >= 32 ) && ( c <= 126 ) )
					{
						*dst++ = c;
					}
					else
					{
						dst += snprintf( dst, 3, "%%%02X", (uint8_t) c );
					}
				}
				*dst = '\0';
				CFDictionarySetCString( edidDict, key, strBuf, kSizeCString );
			}
			else
			{
				// $$$ TO DO: parse other descriptor blocks.
			}
		}
		else
		{
			// $$$ TO DO: parse video timing descriptors.
		}
		
		src += 18; // Move to the next descriptor block.
	}
	
	// Parse extension blocks in EDID versions 1.1 or later.
	
	extensionCount = 0;
	u32 = ReadBig16( &inData[ 18 ] ); // Combine version and revision for easier comparisons.
	if( u32 >= 0x0101 )
	{
		extensionCount = inData[ 126 ];
	}
	src = &inData[ 128 ];
	end = inData + inSize;
	for( i = 0; i < extensionCount; ++i )
	{
		if( ( end - src ) < 128 ) break;
		if( src[ 0 ] == 2 ) _ParseEDID_CEABlock( src, edidDict );
		src += 128;
	}
	
	result = edidDict;
	edidDict = NULL;
	err = kNoErr;	
	
exit:
	if( edidDict )	CFRelease( edidDict );
	if( outErr )	*outErr = err;
	return( result );	
}

//===========================================================================================================================
//	_ParseEDID_CEABlock
//===========================================================================================================================

static OSStatus	_ParseEDID_CEABlock( const uint8_t *inData, CFMutableDictionaryRef inEDIDDict )
{
	OSStatus					err;
	CFMutableDictionaryRef		ceaDict;
	uint8_t						u8;
	const uint8_t *				src;
	const uint8_t *				end;
	uint8_t						dtdOffset;
	uint32_t					regID;
	uint8_t						tag, len;
	
	ceaDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( ceaDict, exit, err = kNoMemoryErr ); 
	CFDictionarySetValue( inEDIDDict, kEDIDKey_CEABlock, ceaDict );
	
	// Verify the checksum.
	
	src = inData;
	end = src + 128;
	for( u8 = 0; src < end; ++src ) u8 += *src;
	require_action_quiet( u8 == 0, exit, err = kChecksumErr );
	
	// Revision.
	
	CFDictionarySetInt64( inEDIDDict, kCEAKey_Revision, inData[ 0 ] );
	
	// $$$ TO DO: Parse general video info.
	
	// Parse each data block.
	
	dtdOffset = inData[ 2 ];
	require_action_quiet( dtdOffset <= 128, exit, err = kRangeErr );
	
	src = &inData[ 4 ];
	end = &inData[ dtdOffset ];
	for( ; src < end; src += ( 1 + len ) )
	{
		tag = src[ 0 ] >> 5;
		len = src[ 0 ] & 0x1F;
		require_action_quiet( ( src + 1 + len ) <= end, exit, err = kUnderrunErr );
		
		switch( tag )
		{
			case 1:
				// $$$ TO DO: Parse Audio Data Block.
				break;
			
			case 2:
				// $$$ TO DO: Parse Video Data Block.
				break;
			
			case 3:
				if( len < 3 ) break;
				regID = ReadLittle24( &src[ 1 ] );
				switch( regID )
				{
					case 0x000C03:
						_ParseEDID_CEABlock_HDMI( src, len + 1, ceaDict );
						break;
					
					default:
						break;
				}
				break;
			
			case 4:
				// $$$ TO DO: Parse Speaker Allocation Data Block.
				break;
			
			default:
				break;
		}
	}
	
	// $$$ TO DO: Parse Detailed Timing Descriptors (DTDs).
	
	err = kNoErr;
	
exit:
	if( ceaDict ) CFRelease( ceaDict );
	return( err );
}

//===========================================================================================================================
//	ParseEDID_CEABlock_HDMI
//
//	Note: "inData" points to the data block header byte and "inSize" is the size of the data including the header byte 
//	(inSize = len + 1). This makes a easier to follow byte offsets from the HDMI spec, which are header relative.
//===========================================================================================================================

static OSStatus	_ParseEDID_CEABlock_HDMI( const uint8_t *inData, size_t inSize, CFMutableDictionaryRef inCAEDict )
{
	OSStatus					err;
	CFMutableDictionaryRef		hdmiDict;
	const uint8_t *				ptr;
	const uint8_t *				end;
	int16_t						s16;
	uint32_t					u32;
	uint8_t						latencyFlags;
	const uint8_t *				latencyPtr;
	
	hdmiDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( hdmiDict, exit, err = kNoMemoryErr ); 
	CFDictionarySetValue( inCAEDict, kEDIDKey_HDMI, hdmiDict );
	
	ptr = inData;
	end = inData + inSize;
	require_action_quiet( ( end - ptr ) >= 6, exit, err = kSizeErr );
	
	// Source Physical Address.
	
	u32 = ReadBig16( &ptr[ 4 ] );
	CFDictionarySetInt64( hdmiDict, kHDMIKey_SourcePhysicalAddress, u32 );
	
	if( inSize > 8 )
	{
		// Parse latency. Note: latency value = ( latency-in-milliseconds / 2 ) + 1 (max value of 251 = 500 ms).
		
		latencyFlags = inData[ 8 ];
		if( latencyFlags & 0x80 )
		{
			latencyPtr = &inData[ 9 ];
			require_action_quiet( ( latencyPtr + 1 ) < end, exit, err = kUnderrunErr );
			
			// Video latency.
			
			s16 = *latencyPtr++;
			s16 = ( s16 - 1 ) * 2;
			CFDictionarySetInt64( hdmiDict, kHDMIKey_VideoLatencyMs, s16 );
			
			// Audio latency.
			
			s16 = *latencyPtr++;
			s16 = ( s16 - 1 ) * 2;
			CFDictionarySetInt64( hdmiDict, kHDMIKey_AudioLatencyMs, s16 );
			
			// Parse interlaced latency.
			
			if( latencyFlags & 0x40 )
			{
				require_action_quiet( ( latencyPtr + 1 ) < end, exit, err = kUnderrunErr );
				
				// Video latency.
				
				s16 = *latencyPtr++;
				s16 = ( s16 - 1 ) * 2;
				CFDictionarySetInt64( hdmiDict, kHDMIKey_VideoLatencyInterlacedMs, s16 );
				
				// Audio latency.
				
				s16 = *latencyPtr++;
				s16 = ( s16 - 1 ) * 2;
				CFDictionarySetInt64( hdmiDict, kHDMIKey_AudioLatencyInterlacedMs, s16 );
			}
		}
	}
	err = kNoErr;
	
exit:
	if( hdmiDict ) CFRelease( hdmiDict );
	return( err );
}

#if 0
#pragma mark -
#pragma mark == H.264 ==
#endif

//===========================================================================================================================
//	H264ConvertAVCCtoAnnexBHeader
//===========================================================================================================================

OSStatus
	H264ConvertAVCCtoAnnexBHeader( 
		const uint8_t *		inAVCCPtr,
		size_t				inAVCCLen,
		uint8_t *			inHeaderBuf,
		size_t				inHeaderMaxLen,
		size_t *			outHeaderLen,
		size_t *			outNALSize,
		const uint8_t **	outNext )
{
	const uint8_t *				src = inAVCCPtr;
	const uint8_t * const		end = src + inAVCCLen;
	uint8_t * const				buf = inHeaderBuf;
	uint8_t *					dst = buf;
	const uint8_t * const		lim = dst + inHeaderMaxLen;
	OSStatus					err;
	size_t						nalSize;
	int							i, n;
	uint16_t					len;
	
	// AVCC Format is documented in ISO/IEC STANDARD 14496-15 (AVC file format) section 5.2.4.1.1.
	//
	// [0x00] version = 1.
	// [0x01] AVCProfileIndication		Profile code as defined in ISO/IEC 14496-10.
	// [0x02] Profile Compatibility		Byte between profile_IDC and level_IDC in SPS from ISO/IEC 14496-10.
	// [0x03] AVCLevelIndication		Level code as defined in ISO/IEC 14496-10.
	// [0x04] LengthSizeMinusOne		0b111111xx where xx is 0, 1, or 3 mapping to 1, 2, or 4 bytes for nal_size.
	// [0x05] SPSCount					0b111xxxxx where xxxxx is the number of SPS entries that follow this field.
	// [0x06] SPS entries				Variable-length SPS array. Each entry has the following structure:
	//		uint16_t	spsLen			Number of bytes in the SPS data until the next entry or the end (big endian).
	//		uint8_t		spsData			SPS entry data.
	// [0xnn] uint8_t	PPSCount		Number of Picture Parameter Sets (PPS) that follow this field.
	// [0xnn] PPS entries				Variable-length PPS array. Each entry has the following structure:
	//		uint16_t	ppsLen			Number of bytes in the PPS data until the next entry or the end (big endian).
	//		uint8_t		ppsData			PPS entry data.
	//
	// Annex-B format is documented in the H.264 spec in Annex-B.
	// Each NAL unit is prefixed with 0x00 0x00 0x00 0x01 and the nal_size from the AVCC is removed.
	
	// Write the SPS NAL units.
	
	require_action( ( end - src ) >= 6, exit, err = kSizeErr );
	nalSize	= ( src[ 4 ] & 0x03 ) + 1;
	n		=   src[ 5 ] & 0x1F;
	src		=  &src[ 6 ];
	for( i = 0; i < n; ++i )
	{
		require_action( ( end - src ) >= 2, exit, err = kUnderrunErr );
		len = (uint16_t)( ( src[ 0 ] << 8 ) | src[ 1 ] );
		src += 2;
		
		require_action( ( end - src ) >= len, exit, err = kUnderrunErr );
		if( inHeaderBuf )
		{
			require_action( ( lim - dst ) >= ( 4 + len ), exit, err = kOverrunErr );
			dst[ 0 ] = 0x00;
			dst[ 1 ] = 0x00;
			dst[ 2 ] = 0x00;
			dst[ 3 ] = 0x01;
			memcpy( dst + 4, src, len );
		}
		src += len;
		dst += ( 4 + len );
	}
	
	// Write PPS NAL units.
	
	if( ( end - src ) >= 1 )
	{
		n = *src++;
		for( i = 0; i < n; ++i )
		{
			require_action( ( end - src ) >= 2, exit, err = kUnderrunErr );
			len = (uint16_t)( ( src[ 0 ] << 8 ) | src[ 1 ] );
			src += 2;
			
			require_action( ( end - src ) >= len, exit, err = kUnderrunErr );
			if( inHeaderBuf )
			{
				require_action( ( lim - dst ) >= ( 4 + len ), exit, err = kOverrunErr );
				dst[ 0 ] = 0x00;
				dst[ 1 ] = 0x00;
				dst[ 2 ] = 0x00;
				dst[ 3 ] = 0x01;
				memcpy( dst + 4, src, len );
			}
			src += len;
			dst += ( 4 + len );
		}
	}
	
	if( outHeaderLen )	*outHeaderLen	= (size_t)( dst - buf );
	if( outNALSize )	*outNALSize		= nalSize;
	if( outNext )		*outNext		= src;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	H264EscapeEmulationPrevention
//===========================================================================================================================

Boolean
	H264EscapeEmulationPrevention( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		const uint8_t **	outDataPtr, 
		size_t *			outDataLen, 
		const uint8_t **	outSuffixPtr, 
		size_t *			outSuffixLen, 
		const uint8_t **	outSrc )
{
	const uint8_t *		ptr;
	
	// To avoid normal data being confused with a start code prefix, insert an emulation prevent byte 0x03 as needed.
	//
	// 0x00,0x00,0x00		-> 0x00,0x00,0x03,0x00
	// 0x00,0x00,0x01		-> 0x00,0x00,0x03,0x01
	// 0x00,0x00,0x02		-> 0x00,0x00,0x03,0x02
	// 0x00,0x00,0x03,0xXX	-> 0x00,0x00,0x03,0x03,0xXX
	//
	// See H.264 spec section 7.4.1.1 for details.
	
	for( ptr = inSrc; ( inEnd - ptr ) >= 3; ++ptr )
	{
		if( ( ptr[ 0 ] == 0x00 ) && ( ptr[ 1 ] == 0x00 ) )
		{
			if( ptr[ 2 ] <= 0x02 )
			{
				*outDataPtr		= inSrc;
				*outDataLen		= (size_t)( ( ptr + 2 ) - inSrc );
				if(      ptr[ 2 ] == 0x00 )	*outSuffixPtr = (const uint8_t *) "\x03\x00";
				else if( ptr[ 2 ] == 0x01 )	*outSuffixPtr = (const uint8_t *) "\x03\x01";
				else						*outSuffixPtr = (const uint8_t *) "\x03\x02";
				*outSuffixLen	= 2;
				*outSrc			= ptr + 3;
				return( true );
			}
			else if( ( ptr[ 2 ] == 0x03 ) && ( ( inEnd - ptr ) >= 4 ) )
			{
				*outDataPtr		= inSrc;
				*outDataLen		= (size_t)( ( ptr + 3 ) - inSrc );
				*outSuffixPtr	= (const uint8_t *) "\x03";
				*outSuffixLen	= 1;
				*outSrc			= ptr + 3;
				return( true );
			}
		}
	}
	
	// The last byte of a NAL unit must not end with 0x00 so if it's 0x00 then append a 0x03 byte.
	
	*outDataPtr = inSrc;
	*outDataLen = (size_t)( inEnd - inSrc );
	if( ( inSrc != inEnd ) && ( inEnd[ -1 ] == 0x00 ) )
	{
		*outSuffixPtr = (const uint8_t *) "\x03";
		*outSuffixLen = 1;
	}
	else
	{
		*outSuffixPtr = NULL;
		*outSuffixLen = 0;
	}
	*outSrc = inEnd;
	return( inSrc != inEnd );
}

//===========================================================================================================================
//	H264RemoveEmulationPrevention
//===========================================================================================================================

OSStatus
	H264RemoveEmulationPrevention( 
		const uint8_t *	inSrc, 
		size_t			inLen, 
		uint8_t *		inBuf, 
		size_t			inMaxLen, 
		size_t *		outLen )
{
	const uint8_t * const		end = inSrc + inLen;
	uint8_t *					dst = inBuf;
	uint8_t * const				lim = inBuf + inMaxLen;
	OSStatus					err;
	
	// Replace 00 00 03 xx with 0x00 0x00 xx.
	// If the last 2 bytes are 0x00 0x03 then remove the 0x03
	// See H.264 spec section 7.4.1.1 for details.
	
	while( ( end - inSrc ) >= 3 )
	{
		if( ( inSrc[ 0 ] == 0x00 ) && ( inSrc[ 1 ] == 0x00 ) && ( inSrc[ 2 ] == 0x03 ) )
		{
			require_action_quiet( ( lim - dst ) >= 2, exit, err = kOverrunErr );
			dst[ 0 ] = inSrc[ 0 ];
			dst[ 1 ] = inSrc[ 1 ];
			inSrc += 3;
			dst   += 2;
		}
		else
		{
			require_action_quiet( dst < lim, exit, err = kOverrunErr );
			*dst++ = *inSrc++;
		}
	}
	if( ( ( end - inSrc ) >= 2 ) && ( inSrc[ 0 ] == 0x00 ) && ( inSrc[ 1 ] == 0x03 ) )
	{
		require_action_quiet( dst < lim, exit, err = kOverrunErr );
		*dst++ = inSrc[ 0 ];
		inSrc += 2;
	}
	while( ( end - inSrc ) >= 1 )
	{
		require_action_quiet( dst < lim, exit, err = kOverrunErr );
		*dst++ = *inSrc++;
	}
	
	if( outLen ) *outLen = (size_t)( dst - inBuf );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	H264GetNextNALUnit
//===========================================================================================================================

OSStatus
	H264GetNextNALUnit( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		size_t				inNALSize, 
		const uint8_t * *	outDataPtr, 
		size_t *			outDataLen,
		const uint8_t **	outSrc )
{
	OSStatus		err;
	size_t			len;
	
	require_action_quiet( inSrc != inEnd, exit, err = kEndingErr );
	require_action_quiet( ( (size_t)( inEnd - inSrc ) ) >= inNALSize, exit, err = kUnderrunErr );
	switch( inNALSize )
	{
		case 1:
			len = *inSrc++;
			break;
		
		case 2:
			len = ReadBig16( inSrc );
			inSrc += 2;
			break;
		
		case 4:
			len = ReadBig32( inSrc );
			inSrc += 4;
			break;
		
		default:
			err = kParamErr;
			goto exit;
	}
	require_action_quiet( ( (size_t)( inEnd - inSrc ) ) >= len, exit, err = kUnderrunErr );
	
	*outDataPtr = inSrc;
	*outDataLen = len;
	*outSrc		= inSrc + len;
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == MirroredRingBuffer ==
#endif

#if( TARGET_MACH )
//===========================================================================================================================
//	MirroredRingBufferInit
//===========================================================================================================================

OSStatus	MirroredRingBufferInit( MirroredRingBuffer *inRing, size_t inMinSize, Boolean inPowerOf2 )
{
	OSStatus				err;
	mach_port_t				task;
	mach_vm_size_t			len;
	mach_vm_address_t		base, addr;
	vm_prot_t				currentProtection, maxProtection;
	
	// Allocate a buffer 2x the size so we can remap the 2nd chunk to the 1st chunk.
	// If we're requiring a power-of-2 sized buffer then make sure the length is increased to the nearest power of 2.
	
	task = mach_task_self();
	base = 0;
	len = mach_vm_round_page( inMinSize );
	if( inPowerOf2 ) len = iceil2( (uint32_t) len );
	require_action( len == mach_vm_round_page( len ), exit, err = kInternalErr );
	
	err = mach_vm_allocate( task, &base, len * 2, VM_FLAGS_ANYWHERE );
	require_noerr( err, exit );
	
	// Remap the 2nd chunk to the 1st chunk.
	
	addr = base + len;
	err = mach_vm_remap( task, &addr, len, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, task, base, false, 
		&currentProtection, &maxProtection, VM_INHERIT_DEFAULT );
	require_noerr( err, exit );
	
	// Success
	
	inRing->buffer		= (uint8_t *)(uintptr_t) base;
	inRing->end			= inRing->buffer + len;
	inRing->size		= (uint32_t) len;
	inRing->mask		= (uint32_t)( len - 1 );
	inRing->readOffset	= 0;
	inRing->writeOffset	= 0;
	base = 0;
	
exit:
	if( base ) mach_vm_deallocate( task, base, len * 2 );
	return( err );
}
#endif

#if( TARGET_OS_POSIX && !TARGET_MACH )
//===========================================================================================================================
//	MirroredRingBufferInit
//===========================================================================================================================

OSStatus	MirroredRingBufferInit( MirroredRingBuffer *inRing, size_t inMinSize, Boolean inPowerOf2 )
{
	char			path[] = "/tmp/MirrorRingBuffer-XXXXXX";
	OSStatus		err;
	long			pageSize;
	int				fd;
	size_t			len;
	uint8_t *		base = (uint8_t *) MAP_FAILED;
	uint8_t *		addr;
	
	// Create a temp file and remove it from the file system, but keep a file descriptor to it.
	
	fd = mkstemp( path );
	err = map_global_value_errno( fd >= 0, fd );
	require_noerr( err, exit );
	
	err = unlink( path );
	err = map_global_noerr_errno( err );
	check_noerr( err );
	
	// Resize the file to the size of the ring buffer.
	
	pageSize = sysconf( _SC_PAGE_SIZE );
	err = map_global_value_errno( pageSize > 0, pageSize );
	require_noerr( err, exit );
	len = RoundUp( inMinSize, (size_t) pageSize );
	if( inPowerOf2 ) len = iceil2( (uint32_t) len );
	err = ftruncate( fd, len );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	// Allocate memory for 2x the ring buffer size and map the two halves on top of each other.
	
	base = (uint8_t *) mmap( NULL, len * 2, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0 );
	err = map_global_value_errno( base != MAP_FAILED, base );
	require_noerr( err, exit );
	
	addr = (uint8_t *) mmap( base, len, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0 );
	err = map_global_value_errno( addr == base, addr );
	require_noerr( err, exit );
	
	addr = (uint8_t *) mmap( base + len, len, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0 );
	err = map_global_value_errno( addr == ( base + len ), addr );
	require_noerr( err, exit );
	
	// Success
	
	inRing->buffer		= base;
	inRing->end			= base + len;
	inRing->size		= (uint32_t) len;
	inRing->mask		= (uint32_t)( len - 1 );
	inRing->readOffset	= 0;
	inRing->writeOffset	= 0;
	base = (uint8_t *) MAP_FAILED;
	
exit:
	if( base != MAP_FAILED )	munmap( base, len * 2 );
	if( fd >= 0 )				close( fd );
	return( err );
}
#endif // TARGET_OS_POSIX && !TARGET_MACH

#if( TARGET_OS_WINDOWS )
//===========================================================================================================================
//	MirroredRingBufferInit
//===========================================================================================================================

OSStatus	MirroredRingBufferInit( MirroredRingBuffer *inRing, size_t inMinSize, Boolean inPowerOf2 )
{
	OSStatus		err;
	SYSTEM_INFO		info;
	size_t			len;
	HANDLE			mapFile;
	uint8_t *		base = NULL;
	uint8_t *		addr;
	uint8_t *		addr2;
	int				tries;
	
	// Allocate a buffer 2x the size so we can remap the 2nd chunk to the 1st chunk.
	// If we're requiring a power-of-2 sized buffer then make sure the length is increased to the nearest power of 2.
	
	GetSystemInfo( &info );
	len = RoundUp( inMinSize, info.dwAllocationGranularity );
	if( inPowerOf2 ) len = iceil2( (uint32_t) len );
	
	mapFile = CreateFileMapping( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, len * 2, NULL );
	err = map_global_value_errno( mapFile, mapFile );
	require_noerr( err, exit );
	
	addr2 = NULL;
	for( tries = 1; tries < 100; ++tries )
	{
		// Map the whole thing to let the system find a logical address range then unmap it.
		
		base = (uint8_t *) MapViewOfFile( mapFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, len * 2 );
		err = map_global_value_errno( base, base );
		require_noerr( err, exit );
		UnmapViewOfFile( base );
		
		// Now try to map to two logical address ranges to the same physical pages.
		// This may fail if another thread came in and stole that address so we'll have to try on failure.
		
		addr = (uint8_t *) MapViewOfFileEx( mapFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, len, base );
		if( !addr ) continue;
		
		addr2 = (uint8_t *) MapViewOfFileEx( mapFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, len, base + len );
		if( addr2 ) break;
		
		// Failed so unmap and try again.
		
		UnmapViewOfFile( addr );
	}
	require_action( addr2, exit, err = kNoMemoryErr );
	
	inRing->mapFile		= mapFile;
	inRing->buffer		= base;
	inRing->end			= base + len;
	inRing->size		= (uint32_t) len;
	inRing->mask		= (uint32_t)( len - 1 );
	inRing->readOffset	= 0;
	inRing->writeOffset	= 0;
	mapFile	= NULL;
	
exit:
	if( mapFile ) CloseHandle( mapFile );
	return( err );
}
#endif

//===========================================================================================================================
//	MirroredRingBufferFree
//===========================================================================================================================

#if( TARGET_MACH )
void	MirroredRingBufferFree( MirroredRingBuffer *inRing )
{
	OSStatus		err;
	
	if( inRing->buffer )
	{
		err = mach_vm_deallocate( mach_task_self(), (vm_address_t)(uintptr_t) inRing->buffer, inRing->size * 2 );
		check_noerr( err );
		inRing->buffer = NULL;
	}
	memset( inRing, 0, sizeof( *inRing ) );
}
#endif

#if( TARGET_OS_POSIX && !TARGET_MACH )
void	MirroredRingBufferFree( MirroredRingBuffer *inRing )
{
	OSStatus		err;
	
	if( inRing->buffer )
	{
		err = munmap( inRing->buffer, inRing->size * 2 );
		err = map_global_noerr_errno( err );
		check_noerr( err );
		inRing->buffer = NULL;
	}
	memset( inRing, 0, sizeof( *inRing ) );
}
#endif

#if( TARGET_OS_WINDOWS )
void	MirroredRingBufferFree( MirroredRingBuffer *inRing )
{
	OSStatus		err;
	BOOL			good;
	
	if( inRing->buffer )
	{
		good = UnmapViewOfFile( inRing->buffer );
		err = map_global_value_errno( good, good );
		check_noerr( err );
		
		good = UnmapViewOfFile( inRing->buffer + inRing->size );
		err = map_global_value_errno( good, good );
		check_noerr( err );
		
		inRing->buffer = NULL;
	}
	if( inRing->mapFile )
	{
		good = CloseHandle( inRing->mapFile );
		err = map_global_value_errno( good, good );
		check_noerr( err );
		inRing->mapFile = NULL;
	}
	memset( inRing, 0, sizeof( *inRing ) );
}
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	MirroredRingBufferTest
//===========================================================================================================================

OSStatus	MirroredRingBufferTest( int print );
OSStatus	MirroredRingBufferTest( int print )
{
	OSStatus				err;
	MirroredRingBuffer		ring  = kMirroredRingBufferInit;
	MirroredRingBuffer		ring2 = kMirroredRingBufferInit;
	MirroredRingBuffer		ring3 = kMirroredRingBufferInit;
	size_t					i;
	uint32_t				counter;
	char					str[ 32 ];
	const char *			src;
	char *					dst;
	size_t					len;
	
	err = MirroredRingBufferInit( &ring, 4096, true );
	require_noerr( err, exit );
	
	// Write to 1st chunk and verify that it shows up in the 2nd chunk.
	
	counter = 0;
	for( i = 0; i < ring.size; i += 4 )
	{
		*( (uint32_t *) &ring.buffer[ i ] ) = counter++;
	}
	if( print )
	{
		dlog( kLogLevelMax, "%.2H\n\n", ring.buffer, (int) ring.size, (int) ring.size );
		dlog( kLogLevelMax, "%.2H\n\n", ring.buffer + ring.size, (int) ring.size, (int) ring.size );
	}
	require_action( memcmp( ring.buffer, ring.buffer + ring.size, ring.size ) == 0, exit, err = -1 );
	
	// Write to 2nd chunk and verify that it shows up in the 1st chunk.
	
	for( i = 0; i < ring.size; i += 4 )
	{
		*( (uint32_t *) &ring.buffer[ ring.size + i ] ) = counter++;
	}
	if( print )
	{
		dlog( kLogLevelMax, "%.2H\n\n", ring.buffer, (int) ring.size, (int) ring.size );
		dlog( kLogLevelMax, "%.2H\n\n", ring.buffer + ring.size, (int) ring.size, (int) ring.size );
	}
	require_action( memcmp( ring.buffer, ring.buffer + ring.size, ring.size ) == 0, exit, err = -1 );
	
	// Test macros.
	
	require_action( MirroredRingBufferGetBytesUsed( &ring ) == 0, exit, err = -1 );
	require_action( MirroredRingBufferGetBytesFree( &ring ) == ring.size, exit, err = -1 );
	
	for( i = 0; i < 10000; ++i )
	{
		require_action( MirroredRingBufferGetBytesFree( &ring ) > 10, exit, err = -1 );
		
		len = (size_t) snprintf( str, sizeof( str ), "Test %d", (int) i );
		dst = (char *) MirroredRingBufferGetWritePtr( &ring );
		memcpy( dst, str, len );
		MirroredRingBufferWriteAdvance( &ring, len );
		require_action( MirroredRingBufferGetBytesUsed( &ring ) == len, exit, err = -1 );
		
		src = (const char *) MirroredRingBufferGetReadPtr( &ring );
		require_action( strncmp( src, str, len ) == 0, exit, err = -1 );
		MirroredRingBufferReadAdvance( &ring, len );
	}
	if( print )
	{
		dlog( kLogLevelMax, "%.2H\n\n", ring.buffer, (int) ring.size, (int) ring.size );
		dlog( kLogLevelMax, "%.2H\n\n", ring.buffer + ring.size, (int) ring.size, (int) ring.size );
	}
	require_action( memcmp( ring.buffer, ring.buffer + ring.size, ring.size ) == 0, exit, err = -1 );
	
	MirroredRingBufferFree( &ring );
	
	// Test multiple buffers.
	
	err = MirroredRingBufferInit( &ring, 10000, true );
	require_noerr( err, exit );
	require_action( ring.size >= 10000, exit, err = -1 );
	
	err = MirroredRingBufferInit( &ring2, 512 * 1000, true );
	require_noerr( err, exit );
	require_action( ring2.size >= 512 * 1000, exit, err = -1 );
	
	err = MirroredRingBufferInit( &ring3, 3000, true );
	require_noerr( err, exit );
	require_action( ring3.size >= 3000, exit, err = -1 );
	
	MirroredRingBufferFree( &ring );
	MirroredRingBufferFree( &ring2 );
	MirroredRingBufferFree( &ring3 );
	
	for( i = 0; i < 100; ++i )
	{
		err = MirroredRingBufferInit( &ring, 8192, true );
		require_noerr( err, exit );
		
		err = MirroredRingBufferInit( &ring2, 8192, true );
		require_noerr( err, exit );
		
		err = MirroredRingBufferInit( &ring3, 8192, true );
		require_noerr( err, exit );
		
		require_action( !PtrsOverlap( ring.buffer, ring.size, ring2.buffer, ring2.size ), exit, err = -1 );
		require_action( !PtrsOverlap( ring.buffer, ring.size, ring3.buffer, ring3.size ), exit, err = -1 );
		require_action( !PtrsOverlap( ring.buffer, ring.size, ring2.buffer, ring2.size ), exit, err = -1 );
		require_action( !PtrsOverlap( ring2.buffer, ring2.size, ring3.buffer, ring3.size ), exit, err = -1 );
		
		MirroredRingBufferFree( &ring );
		MirroredRingBufferFree( &ring2 );
		MirroredRingBufferFree( &ring3 );
	}
	
exit:
	MirroredRingBufferFree( &ring );
	MirroredRingBufferFree( &ring2 );
	MirroredRingBufferFree( &ring3 );
	printf( "MirroredRingBufferTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif

#if 0
#pragma mark -
#pragma mark == Morse Code ==
#endif

//===========================================================================================================================
//	MorseCode
//===========================================================================================================================

#define dit		kMorseCodeAction_Dit // 1 unit of time.
#define dah		kMorseCodeAction_Dah // 3 units of time.

typedef unsigned char		MorseSymbol[ 8 ];

static const MorseSymbol	kMorseCodeAlpha[] =
{
	{dit, dah},				// A
	{dah, dit, dit, dit},	// B
	{dah, dit, dah, dit},	// C
	{dah, dit, dit},		// D
	{dit},					// E
	{dit, dit, dah, dit},	// F
	{dah, dah, dit},		// G
	{dit, dit, dit, dit},	// H
	{dit, dit},				// I
	{dit, dah, dah, dah},	// J
	{dah, dit, dah},		// K
	{dit, dah, dit, dit},	// L
	{dah, dah},				// M
	{dah, dit},				// N
	{dah, dah, dah},		// O
	{dit, dah, dah, dit},	// P
	{dah, dah, dit, dah},	// Q
	{dit, dah, dit},		// R
	{dit, dit, dit},		// S
	{dah},					// T
	{dit, dit, dah},		// U
	{dit, dit, dit, dah},	// V
	{dit, dah, dah},		// W
	{dah, dit, dit, dah},	// X
	{dah, dit, dah, dah},	// Y
	{dah, dah, dit, dit}	// Z
};

static const MorseSymbol	kMorseCodeDigit[] =
{
	{dah, dah, dah, dah, dah},	// 0
	{dit, dah, dah, dah, dah}, 	// 1
	{dit, dit, dah, dah, dah}, 	// 2
	{dit, dit, dit, dah, dah}, 	// 3
	{dit, dit, dit, dit, dah}, 	// 4
	{dit, dit, dit, dit, dit}, 	// 5
	{dah, dit, dit, dit, dit}, 	// 6
	{dah, dah, dit, dit, dit}, 	// 7
	{dah, dah, dah, dit, dit}, 	// 8
	{dah, dah, dah, dah, dit}, 	// 9
};

static const MorseSymbol	kMorseCodePunct[] =
{
	{dit, dah, dit, dit, dah, dit}, 	// "
	{dit, dah, dah, dah, dah, dit},		// '
	{dah, dit, dah, dah, dit}, 			// (
	{dah, dit, dah, dah, dit, dah}, 	// )
	{dah, dah, dit, dit, dah, dah},		// ,
	{dah, dit, dit, dit, dit, dah},		// -
	{dit, dah, dit, dah, dit, dah}, 	// .
	{dah, dit, dit, dah, dit},			// /
	{dah, dah, dah, dit, dit, dit},		// :
	{dit, dit, dah, dah, dit, dit}, 	// ?
	{dit, dah, dah, dit, dah, dit},		// @
	{dah, dit, dah, dit, dah, dah},		// !
	{dit, dah, dit, dit, dit},			// &
	{dah, dit, dah, dit, dah, dit},		// ;
	{dah, dit, dit, dit, dah},			// =
	{dit, dah, dit, dah, dah},			// +
	{dit, dit, dah, dah, dit, dah},		// _
	{dit, dit, dit, dah, dit, dit, dah}	// $
};

typedef struct
{
	MorseCodeFlags			flags;
	MorseCodeActionFunc		actionFunc;
	void *					actionArg;
	unsigned int			unitMics;
	
}	MorseCodeContext;

static void	_MorseCodeDoChar( MorseCodeContext *inContext, const unsigned char *inSymbols );
static void	_MorseCodeAction( MorseCodeContext *inContext, MorseCodeAction inAction );

//===========================================================================================================================
//	MorseCode
//===========================================================================================================================

void
	MorseCode( 
		const char *		inMessage, 
		int					inSpeed, 
		MorseCodeFlags		inFlags, 
		MorseCodeActionFunc inActionFunc, 
		void *				inActionArg )
{
	MorseCodeContext		context;
	char					c;
	
	context.flags		= inFlags;
	context.actionFunc	= inActionFunc;
	context.actionArg	= inActionArg;
	
	if( inSpeed == 0 )	inSpeed  = 10; // Default to 10 words per minute.
	if( inSpeed  > 0 )	context.unitMics = (unsigned int)( ( 1200 * 1000 ) /  inSpeed ); // > 0 means words per minute
	else				context.unitMics = (unsigned int)( ( 6000 * 1000 ) / -inSpeed ); // < 0 means characters per minute (negated).
	
	if( !( inFlags & kMorseCodeFlags_RawActions ) )
	{
		_MorseCodeAction( &context, kMorseCodeAction_Off ); // Start with it off.
		_MorseCodeAction( &context, kMorseCodeAction_WordDelay );
	}
	while( ( c = *inMessage++ ) != '\0' )
	{
		if(      isalpha_safe( c ) )	_MorseCodeDoChar( &context, kMorseCodeAlpha[ toupper( c ) - 'A' ] );
		else if( isdigit_safe( c ) )	_MorseCodeDoChar( &context, kMorseCodeDigit[ c - '0' ] );
		else if( c == '"' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 0 ] );
		else if( c == '\'' )			_MorseCodeDoChar( &context, kMorseCodePunct[ 1 ] );
		else if( c == '(' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 2 ] );
		else if( c == ')' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 3 ] );
		else if( c == ',' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 4 ] );
		else if( c == '-' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 5 ] );
		else if( c == '.' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 6 ] );
		else if( c == '/' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 7 ] );
		else if( c == ':' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 8 ] );
		else if( c == '?' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 9 ] );
		else if( c == '@' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 10 ] );
		else if( c == '!' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 11 ] );
		else if( c == '&' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 12 ] );
		else if( c == ';' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 13 ] );
		else if( c == '=' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 14 ] );
		else if( c == '+' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 15 ] );
		else if( c == '_' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 16 ] );
		else if( c == '$' )				_MorseCodeDoChar( &context, kMorseCodePunct[ 17 ] );
		else if( isspace_safe( c ) )	_MorseCodeAction( &context, kMorseCodeAction_WordDelay );
		else dlog( kLogLevelWarning, "### bad morse code letter '%c' (0x%02x)\n", c, c );
		
		if( ( *inMessage != '\0' ) && !isspace_safe( c ) )
		{
			_MorseCodeAction( &context, kMorseCodeAction_CharDelay );
		}
	}
}

//===========================================================================================================================
//	_MorseCodeDoChar
//===========================================================================================================================

static void	_MorseCodeDoChar( MorseCodeContext *inContext, const unsigned char *inSymbols )
{
	unsigned char		symbol;
	
	while( ( symbol = *inSymbols++ ) != 0 )
	{
		_MorseCodeAction( inContext, symbol );
		if( *inSymbols != 0 ) _MorseCodeAction( inContext, kMorseCodeAction_MarkDelay );
	}
}

//===========================================================================================================================
//	_MorseCodeAction
//===========================================================================================================================

static void	_MorseCodeAction( MorseCodeContext *inContext, MorseCodeAction inAction )
{
	if( inContext->flags & kMorseCodeFlags_RawActions )
	{
		inContext->actionFunc( inAction, inContext->actionArg );
	}
	else
	{
		if( inAction == kMorseCodeAction_Dit )
		{
			inContext->actionFunc( kMorseCodeAction_On, inContext->actionArg );
			usleep( 1 * inContext->unitMics );
			inContext->actionFunc( kMorseCodeAction_Off, inContext->actionArg );
		}
		else if( inAction == kMorseCodeAction_Dah )
		{
			inContext->actionFunc( kMorseCodeAction_On, inContext->actionArg );
			usleep( 3 * inContext->unitMics );
			inContext->actionFunc( kMorseCodeAction_Off, inContext->actionArg );
		}
		else if( inAction == kMorseCodeAction_MarkDelay )	usleep( 1 * inContext->unitMics );
		else if( inAction == kMorseCodeAction_CharDelay )	usleep( 3 * inContext->unitMics );
		else if( inAction == kMorseCodeAction_WordDelay )	usleep( 7 * inContext->unitMics );
		else if( inAction == kMorseCodeAction_Off )			inContext->actionFunc( inAction, inContext->actionArg );
		else if( inAction == kMorseCodeAction_On )			inContext->actionFunc( inAction, inContext->actionArg );
		else dlogassert( "BUG: bad action %d", inAction );
	}
}

#if 0
#pragma mark -
#pragma mark == PID Controller ==
#endif

//===========================================================================================================================
//	PIDInit
//===========================================================================================================================

void	PIDInit( PIDContext *inPID, double pGain, double iGain, double dGain, double dPole, double iMin, double iMax )
{
	inPID->iState	= 0.0;
	inPID->iMax		= iMax;
	inPID->iMin		= iMin;
	inPID->iGain	= iGain;
	
	inPID->dState	= 0.0;
	inPID->dpGain	= 1.0 - dPole;
	inPID->dGain	= dGain * ( 1.0 - dPole );
	
	inPID->pGain	= pGain;
}

//===========================================================================================================================
//	PIDUpdate
//===========================================================================================================================

double	PIDUpdate( PIDContext *inPID, double input )
{
	double		output;
	double		dTemp;
	double		pTerm;
	
	pTerm = input * inPID->pGain; // Proportional.
	
	// Update the integrator state and limit it.
	
	inPID->iState = inPID->iState + ( inPID->iGain * input );
	output = inPID->iState + pTerm;
	if( output > inPID->iMax )
	{
		inPID->iState = inPID->iMax - pTerm;
		output = inPID->iMax;
	}
	else if( output < inPID->iMin )
	{
		inPID->iState = inPID->iMin - pTerm;
		output = inPID->iMin;
	}
	
	// Update the differentiator state.
	
	dTemp = input - inPID->dState;
	inPID->dState += inPID->dpGain * dTemp;
	output += dTemp * inPID->dGain;
	return( output );
}

#if 0
#pragma mark -
#pragma mark == Security ==
#endif




#if( TARGET_OS_MACOSX && !COMMON_SERVICES_NO_CORE_SERVICES )
//===========================================================================================================================
//	HasCodeSigningRequirementByPath
//===========================================================================================================================

Boolean	HasCodeSigningRequirementByPath( const char *inPath, CFStringRef inRequirementString, OSStatus *outErr )
{
	OSStatus				err;
	CFURLRef				url;
	SecStaticCodeRef		staticCode = NULL;
	SecRequirementRef		requirement;
	
	url = CFURLCreateFromFileSystemRepresentation( NULL, (const uint8_t *) inPath, (CFIndex) strlen( inPath ), false );
	require_action( url, exit, err = kFormatErr );
	
	err = SecStaticCodeCreateWithPath( url, kSecCSDefaultFlags, &staticCode );
	CFRelease( url );
	require_noerr_quiet( err, exit );
	
	err = SecRequirementCreateWithString( inRequirementString, kSecCSDefaultFlags, &requirement );
	require_noerr( err, exit );
	
	err = SecStaticCodeCheckValidity( staticCode, kSecCSDefaultFlags, requirement );
	CFRelease( requirement );
	require_noerr_quiet( err, exit );
	
exit:
	CFReleaseNullSafe( staticCode );
	if( outErr ) *outErr = err; 
	return( err ? false : true );
}
#endif

#if( TARGET_OS_MACOSX && !COMMON_SERVICES_NO_CORE_SERVICES )
//===========================================================================================================================
//	HasCodeSigningRequirementByPID
//===========================================================================================================================

Boolean	HasCodeSigningRequirementByPID( pid_t inPID, CFStringRef inRequirementString, OSStatus *outErr )
{
	OSStatus				err;
	int64_t					s64;
	CFNumberRef				num;
	CFDictionaryRef			attrs;
	SecCodeRef				code = NULL;
	SecRequirementRef		requirement;
	
	s64 = inPID;
	num = CFNumberCreate( NULL, kCFNumberSInt64Type, &s64 );
	require_action( num, exit, err = kNoMemoryErr );
	
	attrs = CFDictionaryCreate( NULL, (const void **) &kSecGuestAttributePid, (const void **) &num, 1, 
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	CFRelease( num );
	require_action( attrs, exit, err = kNoMemoryErr );
	
	err = SecCodeCopyGuestWithAttributes( NULL, attrs, kSecCSDefaultFlags, &code );
	CFRelease( attrs );
	require_noerr( err, exit );
	
	err = SecRequirementCreateWithString( inRequirementString, kSecCSDefaultFlags, &requirement );
	require_noerr( err, exit );
	
	err = SecCodeCheckValidity( code, kSecCSDefaultFlags, requirement );
	CFRelease( requirement );
	require_noerr_quiet( err, exit );
	
exit:
	CFReleaseNullSafe( code );
	if( outErr ) *outErr = err; 
	return( err ? false : true );
}
#elif( TARGET_OS_DARWIN )
Boolean	HasCodeSigningRequirementByPID( pid_t inPID, CFStringRef inRequirementString, OSStatus *outErr )
{
	(void) inPID;
	(void) inRequirementString;
	
	if( outErr ) *outErr = kUnsupportedErr;
	return( false );
}
#endif

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )

#if( TARGET_OS_DARWIN && !TARGET_IPHONE_SIMULATOR && !COMMON_SERVICES_NO_CORE_SERVICES )
	#include <IOKit/usb/IOUSBLib.h>
#endif

//===========================================================================================================================
//	MiscUtilsTest
//===========================================================================================================================

OSStatus	MiscUtilsTest( void )
{
	OSStatus			err;
	uint8_t				buf[ 256 ];
	unsigned int		i;
	int8_t				s8[ 5 ];
	uint8_t				u8[ 5 ], u8_16[ 16 ];
	int16_t				s16[ 5 ];
	uint16_t			u16[ 5 ];
	int32_t				s32[ 5 ];
	uint32_t			u32[ 5 ];
	int64_t				s64[ 5 ];
	uint64_t			u64[ 5 ];
	float				f[ 5 ];
	double				d[ 5 ];
	Boolean				b;
	FILE *				file = NULL;
	char *				line = NULL;
	size_t				len;
#if( TARGET_OS_DARWIN && !TARGET_IPHONE_SIMULATOR && !COMMON_SERVICES_NO_CORE_SERVICES )
	io_object_t			iokitService = IO_OBJECT_NULL;
	io_object_t			iokitParent  = IO_OBJECT_NULL;
#endif
#if( TARGET_OS_POSIX )
	struct stat			sb;
#endif
	
	// BitArray
	
	require_action( BitArray_MinBytes( NULL, 0 ) == 0, exit, err = -1 );
	require_action( BitArray_MinBytes( "\x00\x00\x00", 3 ) == 0, exit, err = -1 );
	require_action( BitArray_MinBytes( "\x01\x00\x00", 3 ) == 1, exit, err = -1 );
	require_action( BitArray_MinBytes( "\x00\x01\x00", 3 ) == 2, exit, err = -1 );
	require_action( BitArray_MinBytes( "\x00\x00\x03", 3 ) == 3, exit, err = -1 );
	require_action( BitArray_MinBytes( "\x11\x00\x10", 3 ) == 3, exit, err = -1 );
	require_action( BitArray_MinBytes( "\x01\x00\x00\x01", 4 ) == 4, exit, err = -1 );
	
	require_action( BitArray_MaxBytes( 0 ) == 0, exit, err = -1 );
	require_action( BitArray_MaxBytes( 1 ) == 1, exit, err = -1 );
	require_action( BitArray_MaxBytes( 7 ) == 1, exit, err = -1 );
	require_action( BitArray_MaxBytes( 8 ) == 1, exit, err = -1 );
	require_action( BitArray_MaxBytes( 15 ) == 2, exit, err = -1 );
	require_action( BitArray_MaxBytes( 16 ) == 2, exit, err = -1 );
	require_action( BitArray_MaxBytes( 17 ) == 3, exit, err = -1 );
	
	BitArray_Clear( buf, 4 );
	BitArray_SetBit( buf, 2 );
	BitArray_SetBit( buf, 8 );
	BitArray_SetBit( buf, 17 );
	BitArray_SetBit( buf, 23 );
	require_action( ( buf[ 0 ] == 0x20 ) && ( buf[ 1 ] == 0x80 ) && ( buf[ 2 ] == 0x41 ) && ( buf[ 3 ] == 0x00 ), exit, err = -1 );
	for( i = 0; i < 32; ++i )
	{
		b = ( i == 2 ) || ( i == 8 ) || ( i == 17 ) || ( i == 23 );
		require_action( BitArray_GetBit( buf, 4, i ) == b, exit, err = -1 );
	}
	BitArray_ClearBit( buf, 2 );
	for( i = 0; i < 32; ++i )
	{
		b = ( i == 8 ) || ( i == 17 ) || ( i == 23 );
		require_action( BitArray_GetBit( buf, 4, i ) == b, exit, err = -1 );
	}
	BitArray_ClearBit( buf, 8 );
	for( i = 0; i < 32; ++i )
	{
		b = ( i == 17 ) || ( i == 23 );
		require_action( BitArray_GetBit( buf, 4, i ) == b, exit, err = -1 );
	}
	BitArray_ClearBit( buf, 17 );
	for( i = 0; i < 32; ++i )
	{
		b = ( i == 23 );
		require_action( BitArray_GetBit( buf, 4, i ) == b, exit, err = -1 );
	}
	BitArray_ClearBit( buf, 23 );
	for( i = 0; i < 32; ++i )
	{
		require_action( BitArray_GetBit( buf, 4, i ) == 0, exit, err = -1 );
	}
	require_action( ( buf[ 0 ] == 0x00 ) && ( buf[ 1 ] == 0x00 ) && ( buf[ 2 ] == 0x00 ) && ( buf[ 3 ] == 0x00 ), exit, err = -1 );
	
	// Big Endian
	
	require_action( ReadBig16( "\x11\x22" ) == 0x1122, exit, err = -1 );
	require_action( ReadBig24( "\x11\x22\x33" ) == 0x112233, exit, err = -1 );
	require_action( ReadBig32( "\x11\x22\x33\x44" ) == 0x11223344, exit, err = -1 );
	require_action( ReadBig48( "\x11\x22\x33\x44\x55\x66" ) == UINT64_C( 0x112233445566 ), exit, err = -1 );
	require_action( ReadBig64( "\x11\x22\x33\x44\x55\x66\x77\x88" ) == UINT64_C( 0x1122334455667788 ), exit, err = -1 );
	
	WriteBig16( buf, 0x1122 );
	require_action( memcmp( buf, "\x11\x22", 2 ) == 0, exit, err = -1 );
	
	WriteBig24( buf, 0x112233 );
	require_action( memcmp( buf, "\x11\x22\x33", 3 ) == 0, exit, err = -1 );
	
	WriteBig32( buf, 0x11223344 );
	require_action( memcmp( buf, "\x11\x22\x33\x44", 4 ) == 0, exit, err = -1 );
	
	WriteBig48( buf, UINT64_C( 0x112233445566 ) );
	require_action( memcmp( buf, "\x11\x22\x33\x44\x55\x66", 6 ) == 0, exit, err = -1 );
	
	WriteBig64( buf, UINT64_C( 0x1122334455667788 ) );
	require_action( memcmp( buf, "\x11\x22\x33\x44\x55\x66\x77\x88", 8 ) == 0, exit, err = -1 );
	
	// Little Endian
	
	require_action( ReadLittle16( "\x22\x11" ) == 0x1122, exit, err = -1 );
	require_action( ReadLittle24( "\x33\x22\x11" ) == 0x112233, exit, err = -1 );
	require_action( ReadLittle32( "\x44\x33\x22\x11" ) == 0x11223344, exit, err = -1 );
	require_action( ReadLittle48( "\x66\x55\x44\x33\x22\x11" ) == UINT64_C( 0x112233445566 ), exit, err = -1 );
	require_action( ReadLittle64( "\x88\x77\x66\x55\x44\x33\x22\x11" ) == UINT64_C( 0x1122334455667788 ), exit, err = -1 );
	
	WriteLittle16( buf, 0x1122 );
	require_action( memcmp( buf, "\x22\x11", 2 ) == 0, exit, err = -1 );
		
	WriteLittle24( buf, 0x112233 );
	require_action( memcmp( buf, "\x33\x22\x11", 3 ) == 0, exit, err = -1 );
	
	WriteLittle32( buf, 0x11223344 );
	require_action( memcmp( buf, "\x44\x33\x22\x11", 4 ) == 0, exit, err = -1 );
	
	WriteLittle48( buf, UINT64_C( 0x112233445566 ) );
	require_action( memcmp( buf, "\x66\x55\x44\x33\x22\x11", 6 ) == 0, exit, err = -1 );
	
	WriteLittle64( buf, UINT64_C( 0x1122334455667788 ) );
	require_action( memcmp( buf, "\x88\x77\x66\x55\x44\x33\x22\x11", 8 ) == 0, exit, err = -1 );
	
	// Floating point.
	
	require_action( ReadBigFloat32( "\x42\xF6\xE9\x79" ) == 123.456f, exit, err = -1 );
	require_action( ReadLittleFloat32( "\x79\xE9\xF6\x42" ) == 123.456f, exit, err = -1 );
	
	WriteBigFloat32( buf, 123.456f );
	require_action( memcmp( buf, "\x42\xF6\xE9\x79", 4 ) == 0, exit, err = -1 );
	WriteLittleFloat32( buf, 123.456f );
	require_action( memcmp( buf, "\x79\xE9\xF6\x42", 4 ) == 0, exit, err = -1 );
	
	require_action( ReadBigFloat64( "\x40\x5E\xDD\x2F\x1A\x9F\xBE\x77" ) == 123.456, exit, err = -1 );
	require_action( ReadLittleFloat64( "\x77\xBE\x9F\x1A\x2F\xDD\x5E\x40" ) == 123.456, exit, err = -1 );
	
	WriteBigFloat64( buf, 123.456 );
	require_action( memcmp( buf, "\x40\x5E\xDD\x2F\x1A\x9F\xBE\x77", 8 ) == 0, exit, err = -1 );
	WriteLittleFloat64( buf, 123.456 );
	require_action( memcmp( buf, "\x77\xBE\x9F\x1A\x2F\xDD\x5E\x40", 8 ) == 0, exit, err = -1 );
	
	// Host order
	
	u8[ 0 ] = 0x11;
	Write8( buf, u8[ 0 ] );
	require_action( Read8( buf ) == u8[ 0 ], exit, err = -1 );
	require_action( memcmp( buf, u8, 1 ) == 0, exit, err = -1 );
	
	u16[ 0 ] = 0x1122;
	WriteHost16( buf, u16[ 0 ] );
	require_action( ReadHost16( buf ) == u16[ 0 ], exit, err = -1 );
	require_action( memcmp( buf, u16, 1 ) == 0, exit, err = -1 );
	
	u32[ 0 ] = 0x11223344;
	WriteHost32( buf, u32[ 0 ] );
	require_action( ReadHost32( buf ) == u32[ 0 ], exit, err = -1 );
	require_action( memcmp( buf, u32, 1 ) == 0, exit, err = -1 );
	
	u64[ 0 ] = UINT64_C( 0x112233445566 );
	WriteHost48( buf, u64[ 0 ] );
	require_action( ReadHost48( buf ) == UINT64_C( 0x112233445566 ), exit, err = -1 );
	
	u64[ 0 ] = UINT64_C( 0x1122334455667788 );
	WriteHost64( buf, u64[ 0 ] );
	require_action( ReadHost64( buf ) == u64[ 0 ], exit, err = -1 );
	require_action( memcmp( buf, u64, 1 ) == 0, exit, err = -1 );
	
	// Read/Write Swap
	
	WriteSwap16( &u16[ 0 ], 0x1122 );
	require_action( ReadSwap16( &u16[ 0 ] ) == 0x1122, exit, err = -1 );
	require_action( ReadHost16( &u16[ 0 ] ) == 0x2211, exit, err = -1 );
	
	WriteSwap32( &u32[ 0 ], 0x11223344 );
	require_action( ReadSwap32( &u32[ 0 ] ) == 0x11223344, exit, err = -1 );
	require_action( ReadHost32( &u32[ 0 ] ) == 0x44332211, exit, err = -1 );
	
	WriteSwap64( &u64[ 0 ], UINT64_C( 0x1122334455667788 ) );
	require_action( ReadSwap64( &u64[ 0 ] ) == UINT64_C( 0x1122334455667788 ), exit, err = -1 );
	require_action( ReadHost64( &u64[ 0 ] ) == UINT64_C( 0x8877665544332211 ), exit, err = -1 );
	
	// Conditional swaps
	
#if( TARGET_RT_BIG_ENDIAN )	
	require_action( HostToLittle16( 0x1122 ) == 0x2211, exit, err = -1 );
	require_action( LittleToHost16( 0x1122 ) == 0x2211, exit, err = -1 );
	require_action( HostToLittle32( 0x11223344 ) == 0x44332211, exit, err = -1 );
	require_action( LittleToHost32( 0x11223344 ) == 0x44332211, exit, err = -1 );
	require_action( HostToLittle64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x8877665544332211 ), exit, err = -1 );
	require_action( LittleToHost64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x8877665544332211 ), exit, err = -1 );
	
	require_action( HostToBig16( 0x1122 ) == 0x1122, exit, err = -1 );
	require_action( BigToHost16( 0x1122 ) == 0x1122, exit, err = -1 );
	require_action( HostToBig32( 0x11223344 ) == 0x11223344, exit, err = -1 );
	require_action( BigToHost32( 0x11223344 ) == 0x11223344, exit, err = -1 );
	require_action( HostToBig64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x1122334455667788 ), exit, err = -1 );
	require_action( BigToHost64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x1122334455667788 ), exit, err = -1 );
	
	require_action( hton16( 0x1122 ) == 0x1122, exit, err = -1 );
	require_action( ntoh16( 0x1122 ) == 0x1122, exit, err = -1 );
	require_action( hton32( 0x11223344 ) == 0x11223344, exit, err = -1 );
	require_action( ntoh32( 0x11223344 ) == 0x11223344, exit, err = -1 );
	require_action( hton64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x1122334455667788 ), exit, err = -1 );
	require_action( ntoh64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x1122334455667788 ), exit, err = -1 );
#else
	require_action( HostToLittle16( 0x1122 ) == 0x1122, exit, err = -1 );
	require_action( LittleToHost16( 0x1122 ) == 0x1122, exit, err = -1 );
	require_action( HostToLittle32( 0x11223344 ) == 0x11223344, exit, err = -1 );
	require_action( LittleToHost32( 0x11223344 ) == 0x11223344, exit, err = -1 );
	require_action( HostToLittle64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x1122334455667788 ), exit, err = -1 );
	require_action( LittleToHost64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x1122334455667788 ), exit, err = -1 );
	
	require_action( HostToBig16( 0x1122 ) == 0x2211, exit, err = -1 );
	require_action( BigToHost16( 0x1122 ) == 0x2211, exit, err = -1 );
	require_action( HostToBig32( 0x11223344 ) == 0x44332211, exit, err = -1 );
	require_action( BigToHost32( 0x11223344 ) == 0x44332211, exit, err = -1 );
	require_action( HostToBig64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x8877665544332211 ), exit, err = -1 );
	require_action( BigToHost64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x8877665544332211 ), exit, err = -1 );
	
	require_action( hton16( 0x1122 ) == 0x2211, exit, err = -1 );
	require_action( ntoh16( 0x1122 ) == 0x2211, exit, err = -1 );
	require_action( hton32( 0x11223344 ) == 0x44332211, exit, err = -1 );
	require_action( ntoh32( 0x11223344 ) == 0x44332211, exit, err = -1 );
	require_action( hton64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x8877665544332211 ), exit, err = -1 );
	require_action( ntoh64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x8877665544332211 ), exit, err = -1 );
#endif
	
	// Unconditional swaps
	
	require_action( Swap16( 0x1122 ) == 0x2211, exit, err = -1 );
	require_action( Swap32( 0x11223344 ) == 0x44332211, exit, err = -1 );
	require_action( Swap64( UINT64_C( 0x1122334455667788 ) ) == UINT64_C( 0x8877665544332211 ), exit, err = -1 );
	
	// MemReverse
	
	buf[ 0 ] = 0x11;
	buf[ 1 ] = 0x22;
	buf[ 2 ] = 0x33;
	MemReverse( buf, 3, buf );
	require_action( buf[ 0 ] == 0x33, exit, err = -1 );
	require_action( buf[ 1 ] == 0x22, exit, err = -1 );
	require_action( buf[ 2 ] == 0x11, exit, err = -1 );
	
	buf[ 0 ] = 0x11;
	buf[ 1 ] = 0x22;
	buf[ 2 ] = 0x33;
	buf[ 3 ] = 0x44;
	MemReverse( buf, 4, buf );
	require_action( buf[ 0 ] == 0x44, exit, err = -1 );
	require_action( buf[ 1 ] == 0x33, exit, err = -1 );
	require_action( buf[ 2 ] == 0x22, exit, err = -1 );
	require_action( buf[ 3 ] == 0x11, exit, err = -1 );
	
	memset( u8, 0, sizeof( u8 ) );
	buf[ 0 ] = 0x11;
	buf[ 1 ] = 0x22;
	buf[ 2 ] = 0x33;
	buf[ 3 ] = 0x44;
	buf[ 4 ] = 0x55;
	MemReverse( buf, 5, u8 );
	require_action( u8[ 0 ] == 0x55, exit, err = -1 );
	require_action( u8[ 1 ] == 0x44, exit, err = -1 );
	require_action( u8[ 2 ] == 0x33, exit, err = -1 );
	require_action( u8[ 3 ] == 0x22, exit, err = -1 );
	require_action( u8[ 4 ] == 0x11, exit, err = -1 );
	
	// MemZeroSecure
	
	memset( buf, 'a', sizeof( buf ) );
	for( i = 0; ( i < sizeof( buf ) ) && ( buf[ i ] == 'a' ); ++i ) {}
	MemZeroSecure( buf, 0 );
	for( i = 0; ( i < sizeof( buf ) ) && ( buf[ i ] == 'a' ); ++i ) {}
	require_action( i == sizeof( buf ), exit, err = -1 );
	
	memset( buf, 'a', sizeof( buf ) );
	for( i = 0; ( i < sizeof( buf ) ) && ( buf[ i ] == 'a' ); ++i ) {}
	MemZeroSecure( buf, 1 );
	require_action( buf[ 0 ] == 0, exit, err = -1 );
	for( i = 1; ( i < sizeof( buf ) ) && ( buf[ i ] == 'a' ); ++i ) {}
	require_action( i == sizeof( buf ), exit, err = -1 );
	
	memset( buf, 'a', sizeof( buf ) );
	for( i = 0; ( i < sizeof( buf ) ) && ( buf[ i ] == 'a' ); ++i ) {}
	MemZeroSecure( &buf[ sizeof( buf ) - 1 ], 1 );
	for( i = 0; ( i < ( sizeof( buf ) - 1 ) ) && ( buf[ i ] == 'a' ); ++i ) {}
	require_action( buf[ sizeof( buf ) - 1 ] == 0, exit, err = -1 );
	require_action( i == ( sizeof( buf ) - 1 ), exit, err = -1 );
	
	memset( buf, 'a', sizeof( buf ) );
	for( i = 0; ( i < sizeof( buf ) ) && ( buf[ i ] == 'a' ); ++i ) {}
	MemZeroSecure( buf, sizeof( buf ) );
	for( i = 0; ( i < sizeof( buf ) ) && ( buf[ i ] == 0 ); ++i ) {}
	require_action( i == sizeof( buf ), exit, err = -1 );
	
	// Swap16Mem
	
	memset( u16, 0, sizeof( u16 ) );
	u16[ 0 ] = 0x11AA;
	u16[ 1 ] = 0x22BB;
	u16[ 2 ] = 0x33CC;
	u16[ 3 ] = 0x44DD;
	u16[ 4 ] = 0x55EE;
	Swap16Mem( u16, sizeof( u16 ), u16 );
	require_action( u16[ 0 ] == 0xAA11, exit, err = -1 );
	require_action( u16[ 1 ] == 0xBB22, exit, err = -1 );
	require_action( u16[ 2 ] == 0xCC33, exit, err = -1 );
	require_action( u16[ 3 ] == 0xDD44, exit, err = -1 );
	require_action( u16[ 4 ] == 0xEE55, exit, err = -1 );
	
	memset( u16, 0, sizeof( u16 ) );
	s16[ 0 ] = 0x11AA;
	s16[ 1 ] = 0x22BB;
	s16[ 2 ] = 0x33CC;
	s16[ 3 ] = 0x44DD;
	s16[ 4 ] = 0x55EE;
	Swap16Mem( s16, sizeof( s16 ), u16 );
	require_action( u16[ 0 ] == 0xAA11, exit, err = -1 );
	require_action( u16[ 1 ] == 0xBB22, exit, err = -1 );
	require_action( u16[ 2 ] == 0xCC33, exit, err = -1 );
	require_action( u16[ 3 ] == 0xDD44, exit, err = -1 );
	require_action( u16[ 4 ] == 0xEE55, exit, err = -1 );
	
	// BigEndianIntegerIncrement
	
	memset( u8_16, 0, sizeof( u8_16 ) );
	for( i = 0; i < 300; ++i ) BigEndianIntegerIncrement( u8_16, 16 );
	require_action( memcmp( u8_16, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x2C", 16 ) == 0, exit, err = -1 );
	
	memcpy( u8_16, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 16 );
	BigEndianIntegerIncrement( u8_16, 16 );
	require_action( memcmp( u8_16, "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16 ) == 0, exit, err = -1 );
	
	memcpy( u8_16, "\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 16 );
	BigEndianIntegerIncrement( u8_16, 16 );
	require_action( memcmp( u8_16, "\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16 ) == 0, exit, err = -1 );
	
	memcpy( u8_16, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 16 );
	BigEndianIntegerIncrement( u8_16, 16 );
	require_action( memcmp( u8_16, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16 ) == 0, exit, err = -1 );
	
	memcpy( u8_16, "\xFE\xFF\xFF\xFF\xFF\xFF\xFF\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 16 );
	BigEndianIntegerIncrement( u8_16, 16 );
	require_action( memcmp( u8_16, "\xFE\xFF\xFF\xFF\xFF\xFF\xFF\x01\x00\x00\x00\x00\x00\x00\x00\x00", 16 ) == 0, exit, err = -1 );
	
	memcpy( u8_16, "\x00\xFF", 2 );
	BigEndianIntegerIncrement( &u8_16[ 1 ], 1 );
	require_action( memcmp( u8_16, "\x00\x00", 2 ) == 0, exit, err = -1 );
	
	// LittleEndianIntegerIncrement
	
	memset( u8_16, 0, sizeof( u8_16 ) );
	for( i = 0; i < 300; ++i ) LittleEndianIntegerIncrement( u8_16, 16 );
	require_action( memcmp( u8_16, "\x2C\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16 ) == 0, exit, err = -1 );
	
	memcpy( u8_16, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00", 16 );
	LittleEndianIntegerIncrement( u8_16, 16 );
	require_action( memcmp( u8_16, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 16 ) == 0, exit, err = -1 );
	
	memcpy( u8_16, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE", 16 );
	LittleEndianIntegerIncrement( u8_16, 16 );
	require_action( memcmp( u8_16, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF", 16 ) == 0, exit, err = -1 );
	
	memcpy( u8_16, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 16 );
	LittleEndianIntegerIncrement( u8_16, 16 );
	require_action( memcmp( u8_16, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16 ) == 0, exit, err = -1 );
	
	memcpy( u8_16, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFE", 16 );
	LittleEndianIntegerIncrement( u8_16, 16 );
	require_action( memcmp( u8_16, "\x00\x00\x00\x00\x00\x00\x00\x00\x01\xFF\xFF\xFF\xFF\xFF\xFF\xFE", 16 ) == 0, exit, err = -1 );
	
	memcpy( u8_16, "\xFF\x00", 2 );
	LittleEndianIntegerIncrement( &u8_16[ 0 ], 1 );
	require_action( memcmp( u8_16, "\x00\x00", 2 ) == 0, exit, err = -1 );
	
	// Misc
	
	require_action( BCDByteToDecimal( 0x00 ) == 0, exit, err = kResponseErr );
	require_action( BCDByteToDecimal( 0x01 ) == 1, exit, err = kResponseErr );
	require_action( BCDByteToDecimal( 0x10 ) == 10, exit, err = kResponseErr );
	require_action( BCDByteToDecimal( 0x71 ) == 71, exit, err = kResponseErr );
	require_action( BCDByteToDecimal( 0x99 ) == 99, exit, err = kResponseErr );
	
	require_action( DecimalByteToBCD(  0 ) == 0x00, exit, err = kResponseErr );
	require_action( DecimalByteToBCD(  1 ) == 0x01, exit, err = kResponseErr );
	require_action( DecimalByteToBCD( 10 ) == 0x10, exit, err = kResponseErr );
	require_action( DecimalByteToBCD( 71 ) == 0x71, exit, err = kResponseErr );
	require_action( DecimalByteToBCD( 99 ) == 0x99, exit, err = kResponseErr );
	
	// MinPowerOf2BytesForValue
	
	require_action( MinPowerOf2BytesForValue( 0 ) == 1, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 1 ) == 1, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 127 ) == 1, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 128 ) == 1, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 250 ) == 1, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 255 ) == 1, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 256 ) == 2, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 1000 ) == 2, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 50000 ) == 2, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 65535 ) == 2, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 65536 ) == 4, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 100000 ) == 4, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 16777215 ) == 4, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 16777216 ) == 4, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 2147483648U ) == 4, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( 2147483649U ) == 4, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 4294967295 ) ) == 4, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 4294967296 ) ) == 8, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x7FFFFFFFFFFFFFFF ) ) == 8, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x8000000000000000 ) ) == 8, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0xFFFFFFFFFFFFFFFF ) ) == 8, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x2 ) ) == 1, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x62 ) ) == 1, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x162 ) ) == 2, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0xA162 ) ) == 2, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x3A162 ) ) == 4, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0xB3A162 ) ) == 4, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x5B3A162 ) ) == 4, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x85B3A162 ) ) == 4, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x585B3A162 ) ) == 8, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0xA585B3A162 ) ) == 8, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x3A585B3A162 ) ) == 8, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x13A585B3A162 ) ) == 8, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x813A585B3A162 ) ) == 8, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x3813A585B3A162 ) ) == 8, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0xA3813A585B3A162 ) ) == 8, exit, err = kResponseErr );
	require_action( MinPowerOf2BytesForValue( UINT64_C( 0x6A3813A585B3A162 ) ) == 8, exit, err = kResponseErr );
	
	// AAAAAAAAAA
	//           BBBBBBBBBB
	require_action( !PtrsOverlap( 10, 10, 20, 10 ), exit, err = -1 );
	
	// AAAAAAAAAA
	//             BBBBBBBBBB
	require_action( !PtrsOverlap( 10, 10, 22, 10 ), exit, err = -1 );
	
	// BBBBBBBBBB
	//           AAAAAAAAAA
	require_action( !PtrsOverlap( 20, 10, 10, 10 ), exit, err = -1 );
	
	// BBBBBBBBBB
	//             AAAAAAAAAA
	require_action( !PtrsOverlap( 22, 10, 10, 10 ), exit, err = -1 );
	
	// AAAAAAAAAA
	//      BBBBBBBBBB
	require_action( PtrsOverlap( 10, 10, 15, 10 ), exit, err = -1 );
	
	// BBBBBBBBBB
	//      AAAAAAAAAA
	require_action( PtrsOverlap( 15, 10, 10, 10 ), exit, err = -1 );
	
	// AAAAAAAAAA
	//     BB
	require_action( PtrsOverlap( 10, 10, 14, 2 ), exit, err = -1 );
	
	// BBBBBBBBBB
	//     AA
	require_action( PtrsOverlap( 14, 2, 10, 10 ), exit, err = -1 );
	
	// AAAAAAAAAA
	//
	require_action( !PtrsOverlap( 10, 10, 10, 0 ), exit, err = -1 );
	
	//
	// BBBBBBBBBB
	require_action( !PtrsOverlap( 10, 0, 10, 10 ), exit, err = -1 );
	
	// AAAAAAAAAA
	// B
	require_action( PtrsOverlap( 10, 10, 10, 1 ), exit, err = -1 );
																		
	// AAAAAAAAAA
	//          B
	require_action( PtrsOverlap( 10, 10, 19, 1 ), exit, err = -1 );
		
	// A
	// BBBBBBBBBB
	require_action( PtrsOverlap( 10, 1, 10, 10 ), exit, err = -1 );
	
	//          A
	// BBBBBBBBBB
	require_action( PtrsOverlap( 19, 1, 10, 10 ), exit, err = -1 );
	
	// A
	//  B
	require_action( !PtrsOverlap( 10, 1, 11, 1 ), exit, err = -1 );
	
	// B
	//  A
	require_action( !PtrsOverlap( 11, 1, 10, 1 ), exit, err = -1 );
	
	// qsort comparators.
	
	s8[ 0 ] = 1; s8[ 1 ] = 2; s8[ 2 ] = 3; s8[ 3 ] = -2; s8[ 4 ] = -1;
	qsort( s8, countof( s8 ), sizeof( *s8 ), qsort_cmp_int8 );
	require_action( ( s8[ 0 ] == -2 ) && ( s8[ 1 ] == -1 ) && ( s8[ 2 ] == 1 ) && ( s8[ 3 ] == 2 ) && ( s8[ 4 ] == 3 ), exit, err = -1 );
	
	u8[ 0 ] = 4; u8[ 1 ] = 5; u8[ 2 ] = 1; u8[ 3 ] = 2; u8[ 4 ] = 3;
	qsort( u8, countof( u8 ), sizeof( *u8 ), qsort_cmp_int8 );
	require_action( ( u8[ 0 ] == 1 ) && ( u8[ 1 ] == 2 ) && ( u8[ 2 ] == 3 ) && ( u8[ 3 ] == 4 ) && ( u8[ 4 ] == 5 ), exit, err = -1 );
	
	s16[ 0 ] = 1; s16[ 1 ] = 2; s16[ 2 ] = 3; s16[ 3 ] = -2; s16[ 4 ] = -1;
	qsort( s16, countof( s16 ), sizeof( *s16 ), qsort_cmp_int16 );
	require_action( ( s16[ 0 ] == -2 ) && ( s16[ 1 ] == -1 ) && ( s16[ 2 ] == 1 ) && ( s16[ 3 ] == 2 ) && ( s16[ 4 ] == 3 ), exit, err = -1 );
	
	u16[ 0 ] = 4; u16[ 1 ] = 5; u16[ 2 ] = 1; u16[ 3 ] = 2; u16[ 4 ] = 3;
	qsort( u16, countof( u16 ), sizeof( *u16 ), qsort_cmp_int16 );
	require_action( ( u16[ 0 ] == 1 ) && ( u16[ 1 ] == 2 ) && ( u16[ 2 ] == 3 ) && ( u16[ 3 ] == 4 ) && ( u16[ 4 ] == 5 ), exit, err = -1 );
	
	s32[ 0 ] = 1; s32[ 1 ] = 2; s32[ 2 ] = 3; s32[ 3 ] = -2; s32[ 4 ] = -1;
	qsort( s32, countof( s32 ), sizeof( *s32 ), qsort_cmp_int32 );
	require_action( ( s32[ 0 ] == -2 ) && ( s32[ 1 ] == -1 ) && ( s32[ 2 ] == 1 ) && ( s32[ 3 ] == 2 ) && ( s32[ 4 ] == 3 ), exit, err = -1 );
	
	u32[ 0 ] = 4; u32[ 1 ] = 5; u32[ 2 ] = 1; u32[ 3 ] = 2; u32[ 4 ] = 3;
	qsort( u32, countof( u32 ), sizeof( *u32 ), qsort_cmp_int32 );
	require_action( ( u32[ 0 ] == 1 ) && ( u32[ 1 ] == 2 ) && ( u32[ 2 ] == 3 ) && ( u32[ 3 ] == 4 ) && ( u32[ 4 ] == 5 ), exit, err = -1 );
	
	s64[ 0 ] = 1; s64[ 1 ] = 2; s64[ 2 ] = 3; s64[ 3 ] = -2; s64[ 4 ] = -1;
	qsort( s64, countof( s64 ), sizeof( *s64 ), qsort_cmp_int64 );
	require_action( ( s64[ 0 ] == -2 ) && ( s64[ 1 ] == -1 ) && ( s64[ 2 ] == 1 ) && ( s64[ 3 ] == 2 ) && ( s64[ 4 ] == 3 ), exit, err = -1 );
	
	u64[ 0 ] = 4; u64[ 1 ] = 5; u64[ 2 ] = 1; u64[ 3 ] = 2; u64[ 4 ] = 3;
	qsort( u64, countof( u64 ), sizeof( *u64 ), qsort_cmp_int64 );
	require_action( ( u64[ 0 ] == 1 ) && ( u64[ 1 ] == 2 ) && ( u64[ 2 ] == 3 ) && ( u64[ 3 ] == 4 ) && ( u64[ 4 ] == 5 ), exit, err = -1 );
	
	f[ 0 ] = 1; f[ 1 ] = 2; f[ 2 ] = 3; f[ 3 ] = -2; f[ 4 ] = -1;
	qsort( f, countof( f ), sizeof( *f ), qsort_cmp_float );
	require_action( ( f[ 0 ] == -2 ) && ( f[ 1 ] == -1 ) && ( f[ 2 ] == 1 ) && ( f[ 3 ] == 2 ) && ( f[ 4 ] == 3 ), exit, err = -1 );
	
	d[ 0 ] = 4; d[ 1 ] = 5; d[ 2 ] = 1; d[ 3 ] = 2; d[ 4 ] = 3;
	qsort( d, countof( d ), sizeof( *d ), qsort_cmp_double );
	require_action( ( d[ 0 ] == 1 ) && ( d[ 1 ] == 2 ) && ( d[ 2 ] == 3 ) && ( d[ 3 ] == 4 ) && ( d[ 4 ] == 5 ), exit, err = -1 );
	
	// QSortPtrs
{
	int *		intArray;
	int **		intPtrArray;
	size_t		intCount;
	size_t		intIndex;
	int			x, y;
	
	intCount = 567834;
	intArray = (int *) malloc( intCount * sizeof( *intArray ) );
	require_action( intArray, exit, err = kNoMemoryErr );
	intPtrArray = (int **) malloc( intCount * sizeof( *intPtrArray ) );
	require_action( intPtrArray, exit, err = kNoMemoryErr );
	
	for( intIndex = 0; intIndex < intCount; ++intIndex )	intArray[ intIndex ] = (int) Random32();
	for( intIndex = 0; intIndex < intCount; ++intIndex )	intPtrArray[ intIndex ] = &intArray[ intIndex ];
	QSortPtrs( intPtrArray, intCount, CompareIntPtrs, NULL );
	for( intIndex = 0; intIndex < ( intCount - 1 ); ++intIndex )
	{
		x = *( intPtrArray[ intIndex ] );
		y = *( intPtrArray[ intIndex + 1 ] );
		require_action( x <= y, exit, err = -1 );
	}
	
	for( intIndex = 0; intIndex < intCount; ++intIndex )	intArray[ intIndex ] = (int) intIndex;
	for( intIndex = 0; intIndex < intCount; ++intIndex )	intPtrArray[ intIndex ] = &intArray[ intIndex ];
	QSortPtrs( intPtrArray, intCount, CompareIntPtrs, NULL );
	for( intIndex = 0; intIndex < ( intCount - 1 ); ++intIndex )
	{
		require_action( *( intPtrArray[ intIndex ] ) <= *( intPtrArray[ intIndex + 1 ] ), exit, err = -1 );
	}
	
	for( intIndex = 0; intIndex < intCount; ++intIndex )	intArray[ intIndex ] = (int)( intCount - intIndex );
	for( intIndex = 0; intIndex < intCount; ++intIndex )	intPtrArray[ intIndex ] = &intArray[ intIndex ];
	QSortPtrs( intPtrArray, intCount, CompareIntPtrs, NULL );
	for( intIndex = 0; intIndex < ( intCount - 1 ); ++intIndex )
	{
		require_action( *( intPtrArray[ intIndex ] ) <= *( intPtrArray[ intIndex + 1 ] ), exit, err = -1 );
	}
	
	free( intArray );
	free( intPtrArray );
}
	// CopyFileDataByPath
{
	char *		dataPtr;
	size_t		dataLen;
	
	file = fopen( "/tmp/MiscUtilsTest.txt", "w" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	for( i = 0; i < 1000; ++i )
	{
		fprintf( file, "This is a test of a file with some data in it: %u\n", i );
	}
	ForgetANSIFile( &file );
	
	err = CopyFileDataByPath( "/tmp/MiscUtilsTest.txt", &dataPtr, &dataLen );
	remove( "/tmp/MiscUtilsTest.txt" );
	require_noerr( err, exit );
	free( dataPtr );
}	
	// CreateTXTRecordWithCString
{
	uint8_t *			txtRec;
	size_t				txtLen;
	
	err = CreateTXTRecordWithCString( 
		"path=/foo/bar/index.html name1=ab\\x63 'name2=my \\x12 name 2' \"name3=my \\x6eam\\145 3\" name4=ab\\143", 
		&txtRec, &txtLen );
	require_noerr( err, exit );
	
	free( txtRec );
}

#if( TARGET_OS_DARWIN && !TARGET_IPHONE_SIMULATOR && !COMMON_SERVICES_NO_CORE_SERVICES )
	// IOKitCopyParentOfClass
	
	iokitService = IOServiceGetMatchingService( kIOMasterPortDefault, IOServiceMatching( kIOUSBInterfaceClassName ) );
	require_action( iokitService, exit, err = -1 );
	
	iokitParent = IOKitCopyParentOfClass( iokitService, kIOUSBDeviceClassName, &err );
	require_noerr( err, exit );
	require_action( iokitParent, exit, err = -1 );
	require_action( IOObjectConformsTo( iokitParent, kIOUSBDeviceClassName ), exit, err = -1 );
	IOObjectForget( &iokitParent );
	
	iokitParent = IOKitCopyParentOfClass( iokitService, "MiscUtils-fakeclass", &err );
	require_action( err != kNoErr, exit, err = -1 );
	require_action( !iokitParent, exit, err = -1 );
	
	IOObjectForget( &iokitService );
#endif
	
	// fcopyline test 1
	
	file = fopen( "/tmp/fcopyline-test.txt", "w" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	fprintf( file, "Line 1" );
	ForgetANSIFile( &file );
	
	file = fopen( "/tmp/fcopyline-test.txt", "r" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	i    = 0;
	line = NULL;
	len  = 0;
	while( ( err = fcopyline( file, &line, &len ) ) == kNoErr )
	{
		++i;
		require_action( line, exit, err = -1 );
		require_action( strnlen( line, len ) <= len, exit, err = -1 );
		require_action( MemEqual( line, len, "Line 1", 6 ), exit, err = -1 );
		ForgetMem( &line );
	}
	require_action( err == kEndingErr, exit, err = -1 );
	require_action( !line, exit, err = -1 );
	require_action( i == 1, exit, err = -1 );
	ForgetANSIFile( &file );
	
	// fcopyline test 2
	
	file = fopen( "/tmp/fcopyline-test.txt", "w" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	fprintf( file, "A 1\n" );
	fprintf( file, "AA 2\n" );
	fprintf( file, "AAA 3\n" );
	fprintf( file, "AA 4\n" );
	fprintf( file, "A 5\n" );
	ForgetANSIFile( &file );
	
	file = fopen( "/tmp/fcopyline-test.txt", "r" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	i    = 0;
	line = NULL;
	len  = 0;
	while( ( err = fcopyline( file, &line, &len ) ) == kNoErr )
	{
		++i;
		require_action( line, exit, err = -1 );
		require_action( strnlen( line, len ) <= len, exit, err = -1 );
		if(      i == 1 ) require_action( MemEqual( line, len, "A 1", 3 ), exit, err = -1 );
		else if( i == 2 ) require_action( MemEqual( line, len, "AA 2", 4 ), exit, err = -1 );
		else if( i == 3 ) require_action( MemEqual( line, len, "AAA 3", 5 ), exit, err = -1 );
		else if( i == 4 ) require_action( MemEqual( line, len, "AA 4", 4 ), exit, err = -1 );
		else if( i == 5 ) require_action( MemEqual( line, len, "A 5", 3 ), exit, err = -1 );
		ForgetMem( &line );
	}
	require_action( err == kEndingErr, exit, err = -1 );
	require_action( !line, exit, err = -1 );
	require_action( i == 5, exit, err = -1 );
	ForgetANSIFile( &file );
	
	// fcopyline test 3
	
	file = fopen( "/tmp/fcopyline-test.txt", "w" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	fprintf( file, "Line 1\n" );
	fwrite( "Line" "\0" "2\n", 1, 7, file );
	fprintf( file, "Longer line 3\n" );
	ForgetANSIFile( &file );
	
	file = fopen( "/tmp/fcopyline-test.txt", "r" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	i    = 0;
	line = NULL;
	len  = 0;
	while( ( err = fcopyline( file, &line, &len ) ) == kNoErr )
	{
		++i;
		require_action( line, exit, err = -1 );
		require_action( strnlen( line, len ) <= len, exit, err = -1 );
		if(      i == 1 ) require_action( MemEqual( line, len, "Line 1", 6 ), exit, err = -1 );
		else if( i == 2 ) require_action( MemEqual( line, len, "Line" "\0" "2", 6 ), exit, err = -1 );
		else if( i == 3 ) require_action( MemEqual( line, len, "Longer line 3", 13 ), exit, err = -1 );
		ForgetMem( &line );
	}
	require_action( err == kEndingErr, exit, err = -1 );
	require_action( !line, exit, err = -1 );
	require_action( i == 3, exit, err = -1 );
	ForgetANSIFile( &file );
	
	// fcopyline test 4
	
	file = fopen( "/tmp/fcopyline-test.txt", "w" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	fprintf( file, "A slightly longer Line 1\n" );
	fprintf( file, "A line that is much longer than the previous line 2\n" );
	fprintf( file, "Line 3\n" );
	ForgetANSIFile( &file );
	
	file = fopen( "/tmp/fcopyline-test.txt", "r" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	i    = 0;
	line = NULL;
	len  = 0;
	while( ( err = fcopyline( file, &line, &len ) ) == kNoErr )
	{
		++i;
		require_action( line, exit, err = -1 );
		require_action( strnlen( line, len ) <= len, exit, err = -1 );
		if(      i == 1 ) require_action( MemEqual( line, len, "A slightly longer Line 1", 24 ), exit, err = -1 );
		else if( i == 2 ) require_action( MemEqual( line, len, "A line that is much longer than the previous line 2", 51 ), exit, err = -1 );
		else if( i == 3 ) require_action( MemEqual( line, len, "Line 3", 6 ), exit, err = -1 );
		ForgetMem( &line );
	}
	require_action( err == kEndingErr, exit, err = -1 );
	require_action( !line, exit, err = -1 );
	require_action( i == 3, exit, err = -1 );
	ForgetANSIFile( &file );
	
	// fcopyline test 5
	
	file = fopen( "/tmp/fcopyline-test.txt", "w" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	ForgetANSIFile( &file );
	
	file = fopen( "/tmp/fcopyline-test.txt", "r" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	i    = 0;
	line = NULL;
	len  = 0;
	while( ( err = fcopyline( file, &line, &len ) ) == kNoErr )
	{
		++i;
		ForgetMem( &line );
	}
	require_action( err == kEndingErr, exit, err = -1 );
	require_action( !line, exit, err = -1 );
	require_action( i == 0, exit, err = -1 );
	ForgetANSIFile( &file );
	
	// fcopyline test 6
	
	file = fopen( "/tmp/fcopyline-test.txt", "w" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	fprintf( file, "\n" );
	ForgetANSIFile( &file );
	
	file = fopen( "/tmp/fcopyline-test.txt", "r" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	i    = 0;
	line = NULL;
	len  = 0;
	while( ( err = fcopyline( file, &line, &len ) ) == kNoErr )
	{
		++i;
		require_action( line, exit, err = -1 );
		require_action( strnlen( line, len ) <= len, exit, err = -1 );
		if( i == 1 ) require_action( MemEqual( line, len, "", 0 ), exit, err = -1 );
		ForgetMem( &line );
	}
	require_action( err == kEndingErr, exit, err = -1 );
	require_action( !line, exit, err = -1 );
	require_action( i == 1, exit, err = -1 );
	ForgetANSIFile( &file );
	
#if( TARGET_OS_POSIX )
	// mkparent
	
	RemovePath( "/tmp/MiscUtils" );
	
	err = mkparent( "/tmp/MiscUtils/a/b/c/d.txt", S_IRWXU );
	require_noerr( err, exit );
	err = stat( "/tmp/MiscUtils/a/b/c", &sb );
	require_noerr( err, exit );
	err = stat( "/tmp/MiscUtils/a/b/c/d.txt", &sb );
	require_action( err != 0, exit, err = kResponseErr );
	RemovePath( "/tmp/MiscUtils" );
	
	err = mkparent( "/tmp/MiscUtils/a/b/c/", S_IRWXU );
	require_noerr( err, exit );
	err = stat( "/tmp/MiscUtils/a/b/c", &sb );
	require_noerr( err, exit );
	RemovePath( "/tmp/MiscUtils" );
#endif
	
	// NormalizePath
	
#if( TARGET_OS_POSIX && TARGET_OS_MACOSX )
{
	char			path1[ PATH_MAX ];
	char			path2[ PATH_MAX ];
	const char *	user;
	
	user = getenv( "USER" );
	if( !user ) user = "";
	
	snprintf( path2, sizeof( path2 ), "/Users/%s/Music/iTunes", user );
	NormalizePath( "~/Music/iTunes", kSizeCString, path1, sizeof( path1 ), 0 );
	require_action( strcmp( path1, path2 ) == 0, exit, err = -1 );
	
	snprintf( path1, sizeof( path1 ), "~%s/Music/iTunes", user );
	snprintf( path2, sizeof( path2 ), "/Users/%s/Music/iTunes", user );
	NormalizePath( path1, kSizeCString, path1, sizeof( path1 ), 0 );
	require_action( strcmp( path1, path2 ) == 0, exit, err = -1 );
	
	snprintf( path2, sizeof( path2 ), "/Users/%s", user );
	NormalizePath( "~", kSizeCString, path1, sizeof( path1 ), 0 );
	require_action( strcmp( path1, path2 ) == 0, exit, err = -1 );
	
	snprintf( path1, sizeof( path1 ), "~%s", user );
	snprintf( path2, sizeof( path2 ), "/Users/%s", user );
	NormalizePath( path1, kSizeCString, path1, sizeof( path1 ), 0 );
	require_action( strcmp( path1, path2 ) == 0, exit, err = -1 );
	
	*path1 = '\0';
	NormalizePath( "/System/Library/CoreServices/SystemVersion.plist", kSizeCString, path1, sizeof( path1 ), 0 );
	require_action( strcmp( path1, "/System/Library/CoreServices/SystemVersion.plist" ) == 0, exit, err = -1 );
	
	*path1 = '\0';
	NormalizePath( "/System/Library/CoreServices/../../Library/CoreServices/SystemVersion.plist", kSizeCString, path1, sizeof( path1 ), 0 );
	require_action( strcmp( path1, "/System/Library/CoreServices/SystemVersion.plist" ) == 0, exit, err = -1 );
	
	*path1 = '\0';
	NormalizePath( "/System/Library///CoreServices/./SystemVersion.plist", kSizeCString, path1, sizeof( path1 ), 0 );
	require_action( strcmp( path1, "/System/Library/CoreServices/SystemVersion.plist" ) == 0, exit, err = -1 );
	
	*path1 = '\0';
	NormalizePath( "/tmp", kSizeCString, path1, sizeof( path1 ), 0 );
	require_action( strcmp( path1, "/private/tmp" ) == 0, exit, err = -1 );
	
	snprintf( path1, sizeof( path1 ), "~%s.profile", user );
	NormalizePath( path1, kSizeCString, path2, sizeof( path2 ), 0 );
	snprintf( path2, sizeof( path2 ), "~%s.profile", user );
	require_action( strcmp( path1, path2 ) == 0, exit, err = -1 );
}
#endif
	
	// NumberListStringCreateFromUInt8Array
	
#if( TARGET_HAS_STD_C_LIB )
{
	char *				s;
	unsigned int		j;
	
	s = NULL;
	i = 0;
	buf[ i++ ] = 11; buf[ i++ ] = 0; buf[ i++ ] = 5; buf[ i++ ] = 4; buf[ i++ ] = 3; buf[ i++ ] = 2; buf[ i++ ] = 1;
	buf[ i++ ] = 7; buf[ i++ ] = 9; buf[ i++ ] = 12; buf[ i++ ] = 13; buf[ i++ ] = 14; buf[ i++ ] = 15;
	err = NumberListStringCreateFromUInt8Array( buf, i, &s );
	require_noerr( err, exit );
	require_action( strcmp( s, "0-5,7,9,11-15" ) == 0, exit, err = kResponseErr );
	free( s );
	
	s = NULL;
	for( i = 0; i < 64; ++i ) buf[ i ] = (uint8_t) i;
	err = NumberListStringCreateFromUInt8Array( buf, i, &s );
	require_noerr( err, exit );
	require_action( strcmp( s, "0-63" ) == 0, exit, err = kResponseErr );
	free( s );
	
	s = NULL;
	j = 0;
	for( i = 1; i <= 4; ++i ) buf[ j++ ] = (uint8_t) i;
	for( i = 4; i <  8; ++i ) buf[ j++ ] = (uint8_t) i;
	buf[ j++ ] = (uint8_t) i; buf[ j++ ] = (uint8_t) i; buf[ j++ ] = (uint8_t) i;
	for( i = 100; i <= 150; ++i ) buf[ j++ ] = (uint8_t) i;
	err = NumberListStringCreateFromUInt8Array( buf, j, &s );
	require_noerr( err, exit );
	require_action( strcmp( s, "1-8,100-150" ) == 0, exit, err = kResponseErr );
	free( s );
	
	s = NULL;
	j = 0;
	buf[ j++ ] = 0; buf[ j++ ] = 2; buf[ j++ ] = 4; buf[ j++ ] = 5; buf[ j++ ] = 7; buf[ j++ ] = 9; buf[ j++ ] = 11;
	err = NumberListStringCreateFromUInt8Array( buf, j, &s );
	require_noerr( err, exit );
	require_action( strcmp( s, "0,2,4-5,7,9,11" ) == 0, exit, err = kResponseErr );
	free( s );
	
	s = NULL;
	j = 0;
	for( i = 1; i <= 8; ++i ) buf[ j++ ] = (uint8_t) i;
	buf[ j++ ] = 10; buf[ j++ ] = 12;
	for( i = 14; i <= 20; ++i ) buf[ j++ ] = (uint8_t) i;
	for( i = 22; i <= 24; ++i ) buf[ j++ ] = (uint8_t) i;
	err = NumberListStringCreateFromUInt8Array( buf, j, &s );
	require_noerr( err, exit );
	require_action( strcmp( s, "1-8,10,12,14-20,22-24" ) == 0, exit, err = kResponseErr );
	free( s );
}
#endif // TARGET_HAS_STD_C_LIB
	
#if( TARGET_OS_POSIX )
	// RemovePath
	
	file = fopen( "/tmp/a.txt", "wb" );
	require_action( file, exit, err = kOpenErr );
	fwrite( "test", 1, 4, file );
	ForgetANSIFile( &file );
	err = stat( "/tmp/a.txt", &sb );
	require_noerr( err, exit );
	err = RemovePath( "/tmp/a.txt" );
	require_noerr( err, exit );
	err = stat( "/tmp/a.txt", &sb );
	require_action( err != 0, exit, err = kResponseErr );
	
	// systemf
	
	remove( "/tmp/MiscUtilsTest " );
	err = systemf( NULL, "/bin/rm /tmp/MiscUtilsTest >/dev/null 2>&1" );
	require_action( err != 0, exit, err = kResponseErr );
	
	err = systemf( NULL, "/usr/bin/touch /tmp/MiscUtilsTest" );
	require_noerr( err, exit );
	require_action( stat( "/tmp/MiscUtilsTest", &sb ) == 0, exit, err = kResponseErr );
	
	err = systemf( NULL, "/bin/rm /tmp/MiscUtilsTest" );
	require_noerr( err, exit );
	require_action( stat( "/tmp/MiscUtilsTest", &sb ) != 0, exit, err = kResponseErr );
#endif
	
	// Packing/Unpacking
	
	err = PackingUnpackingTest();
	require_noerr( err, exit );
	
	// H.264 header tests
{
	static const uint8_t		kAVCC1[] =
	{
		0x01, 0x64, 0xC0, 0x28, 0xFF, 0xE1, 0x00, 0x11, 0x67, 0x64, 0xC0, 0x28, 0xAC, 0x56, 0x20, 0x08, 
		0xC1, 0xEF, 0x9E, 0xE6, 0xE0, 0x40, 0x40, 0x40, 0x40, 0x01, 0x00, 0x05, 0x28, 0xEE, 0x05, 0x72, 
		0xC0
	};
	
	static const uint8_t		kAVCC2[] =
	{
		0x01, 0x64, 0xC0, 0x28, 0xFF, 0xE1, 0x00, 0x10, 0x67, 0x64, 0xC0, 0x28, 0xAC, 0x56, 0x20, 0x17, 
		0x05, 0x3E, 0xA5, 0x9B, 0x81, 0x01, 0x01, 0x01, 0x01, 0x00, 0x04, 0x28, 0xEE, 0x0B, 0xCB
	};
	
	uint8_t				annexBBuf[ 256 ];
	size_t				annexBLen;
	size_t				nalSize;
	const uint8_t *		next;
	
	annexBLen	= 0;
	nalSize		= 0;
	next		= NULL;
	err = H264ConvertAVCCtoAnnexBHeader( kAVCC1, sizeof( kAVCC1 ), annexBBuf, sizeof( annexBBuf ), &annexBLen, &nalSize, &next );
	require_noerr( err, exit );
	
	annexBLen	= 0;
	nalSize		= 0;
	next		= NULL;
	err = H264ConvertAVCCtoAnnexBHeader( kAVCC2, sizeof( kAVCC2 ), annexBBuf, sizeof( annexBBuf ), &annexBLen, &nalSize, &next );
	require_noerr( err, exit );
}
	
	// H.264 NAL unit tests.
{
	static const uint8_t		kNALUnits[] =
	{
		// NAL 1 (157 bytes)
		0x00, 0x00, 0x00, 0x9D, 0x21, 0x00, 0xE6, 0xE4, 0x20, 0xB2, 0x14, 0x41, 0xC9, 0x86, 0x5F, 0xFE, 
		0x8C, 0xB0, 0x11, 0x26, 0x69, 0x82, 0x10, 0x72, 0x2F, 0x77, 0x2C, 0x10, 0xA2, 0xB8, 0x4A, 0x4B, 
		0x8A, 0x0E, 0xF9, 0x1C, 0xCC, 0xB0, 0xCB, 0x6B, 0x69, 0xBA, 0x61, 0x03, 0x4D, 0x36, 0x64, 0xD2, 
		0xE9, 0xA8, 0xB1, 0x9F, 0x7C, 0x88, 0xC0, 0xA5, 0xFC, 0x18, 0x3A, 0xDA, 0x30, 0xDB, 0xE7, 0x19, 
		0x58, 0x75, 0x08, 0x82, 0x1B, 0xBD, 0x0C, 0x7C, 0x5A, 0x74, 0x00, 0x00, 0xF7, 0x19, 0xE1, 0x19, 
		0xFC, 0x56, 0x46, 0x25, 0xC6, 0x52, 0x06, 0x17, 0x38, 0x86, 0x15, 0x94, 0x7F, 0x7F, 0x23, 0xDB, 
		0xF8, 0xE0, 0xBE, 0x7A, 0x77, 0xB8, 0x59, 0xB1, 0x17, 0x3A, 0x9F, 0xB4, 0x00, 0x00, 0x02, 0xF9, 
		0x22, 0x58, 0x00, 0xE5, 0x9C, 0x9D, 0x51, 0x82, 0x35, 0x5F, 0xE8, 0x2D, 0xE3, 0x11, 0x5B, 0x4D, 
		0x09, 0x9A, 0x59, 0x76, 0x86, 0xD4, 0x75, 0x3C, 0x94, 0x73, 0x24, 0x63, 0x5E, 0xD6, 0x32, 0x17, 
		0xC0, 0x08, 0x54, 0x38, 0x65, 0xB9, 0xF8, 0x4E, 0xFF, 0xF2, 0xB2, 0xBA, 0xDB, 0x03, 0x8C, 0xE3, 
		0x80, 
	
		// NAL 2 (155 bytes)
		0x00, 0x00, 0x00, 0x9B, 0x21, 0x00, 0xE6, 0xE5, 0x29, 0x64, 0x22, 0x19, 0x7F, 0xFE, 0x8C, 0xB0, 
		0x11, 0x26, 0x69, 0x82, 0x10, 0x72, 0x2F, 0x77, 0x2C, 0x10, 0xA2, 0xB8, 0x4A, 0x4B, 0x8A, 0x0E, 
		0xF9, 0x1C, 0xCC, 0xB0, 0x00, 0x00, 0x01, 0xBA, 0x61, 0x03, 0x4D, 0x36, 0x64, 0xD2, 0xE9, 0xA8, 
		0xB1, 0x9F, 0x7C, 0x88, 0xC0, 0xA5, 0xFC, 0x18, 0x3A, 0xDA, 0x30, 0xDB, 0xE7, 0x19, 0x58, 0x75, 
		0x08, 0x82, 0x1B, 0xBD, 0x0C, 0x7C, 0x5A, 0x74, 0x57, 0xD4, 0xF7, 0x18, 0xE0, 0x6D, 0x39, 0x92, 
		0x2A, 0xA0, 0xB3, 0xB7, 0xBE, 0xB7, 0x2E, 0x8B, 0x53, 0x00, 0x1E, 0x6E, 0xFE, 0x8D, 0x55, 0xEA, 
		0xB7, 0x62, 0x1C, 0x2C, 0x38, 0xB3, 0x62, 0x2E, 0x75, 0x3F, 0x69, 0x1D, 0xA8, 0xC7, 0x13, 0x88, 
		0xA4, 0x08, 0xC5, 0xFA, 0xAB, 0x86, 0x61, 0xE8, 0xF7, 0x41, 0x87, 0x18, 0x8A, 0xDA, 0x68, 0x4C, 
		0xD2, 0xCB, 0xB4, 0x36, 0xA3, 0xAA, 0x5F, 0x75, 0xAD, 0x63, 0x1A, 0xF6, 0xB1, 0x90, 0xBE, 0x00, 
		0x42, 0xA1, 0xC3, 0x2D, 0xCF, 0xC2, 0x77, 0xFF, 0x95, 0x95, 0xC9, 0x88, 0xA6, 0xC7, 0x1C, 
	
		// NAL 3 (154 bytes)
		0x00, 0x00, 0x00, 0x9A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x2F, 0xFE, 0x8C, 0xB0, 0x11, 0x26, 
		0x69, 0x82, 0x10, 0x72, 0x2F, 0x77, 0x2C, 0x10, 0xA2, 0xB8, 0x4A, 0x4B, 0x8A, 0x0E, 0xF9, 0x1C, 
		0xCC, 0xB0, 0xCB, 0x6B, 0x69, 0xBA, 0x61, 0x03, 0x4D, 0x36, 0x64, 0xD2, 0xE9, 0xA8, 0xB1, 0x9F, 
		0x7C, 0x88, 0xC0, 0xA5, 0xFC, 0x18, 0x3A, 0xDA, 0x30, 0xDB, 0xE7, 0x19, 0x58, 0x75, 0x08, 0x82, 
		0x1B, 0xBD, 0x0C, 0x7C, 0x5A, 0x74, 0x57, 0xD4, 0xF7, 0x19, 0xF0, 0x11, 0xFC, 0x56, 0x46, 0x25, 
		0xC6, 0x52, 0x06, 0x17, 0x38, 0x86, 0x15, 0x94, 0x7F, 0x7F, 0x23, 0xDB, 0xF8, 0xE0, 0xBE, 0x7A, 
		0x77, 0xB8, 0x59, 0xB1, 0x17, 0x3A, 0x9F, 0xB4, 0x8E, 0xD4, 0x61, 0xF9, 0x22, 0x58, 0x00, 0xE5, 
		0x9C, 0x9D, 0x51, 0x82, 0x35, 0x5F, 0xE8, 0x2D, 0xE3, 0x11, 0x5B, 0x4D, 0x09, 0x9A, 0x59, 0x76, 
		0x86, 0xD4, 0x75, 0x3C, 0x94, 0x73, 0x24, 0x63, 0x5E, 0xD6, 0x32, 0x17, 0xC0, 0x08, 0x54, 0x38, 
		0x65, 0x00, 0x00, 0x03, 0x03, 0xF2, 0xB2, 0xBA, 0xDB, 0x03, 0x8C, 0xE3, 0x80, 0x00
	};
	
	// NAL 3 with emulation bytes removed (154 bytes)
	static const uint8_t		kNALUnit3[] =
	{
		                        0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x2F, 0xFE, 0x8C, 0xB0, 0x11, 0x26, 
		0x69, 0x82, 0x10, 0x72, 0x2F, 0x77, 0x2C, 0x10, 0xA2, 0xB8, 0x4A, 0x4B, 0x8A, 0x0E, 0xF9, 0x1C, 
		0xCC, 0xB0, 0xCB, 0x6B, 0x69, 0xBA, 0x61, 0x03, 0x4D, 0x36, 0x64, 0xD2, 0xE9, 0xA8, 0xB1, 0x9F, 
		0x7C, 0x88, 0xC0, 0xA5, 0xFC, 0x18, 0x3A, 0xDA, 0x30, 0xDB, 0xE7, 0x19, 0x58, 0x75, 0x08, 0x82, 
		0x1B, 0xBD, 0x0C, 0x7C, 0x5A, 0x74, 0x57, 0xD4, 0xF7, 0x19, 0xF0, 0x11, 0xFC, 0x56, 0x46, 0x25, 
		0xC6, 0x52, 0x06, 0x17, 0x38, 0x86, 0x15, 0x94, 0x7F, 0x7F, 0x23, 0xDB, 0xF8, 0xE0, 0xBE, 0x7A, 
		0x77, 0xB8, 0x59, 0xB1, 0x17, 0x3A, 0x9F, 0xB4, 0x8E, 0xD4, 0x61, 0xF9, 0x22, 0x58, 0x00, 0xE5, 
		0x9C, 0x9D, 0x51, 0x82, 0x35, 0x5F, 0xE8, 0x2D, 0xE3, 0x11, 0x5B, 0x4D, 0x09, 0x9A, 0x59, 0x76, 
		0x86, 0xD4, 0x75, 0x3C, 0x94, 0x73, 0x24, 0x63, 0x5E, 0xD6, 0x32, 0x17, 0xC0, 0x08, 0x54, 0x38, 
		0x65, 0x00, 0x00, 0x03, 0x03, 0xF2, 0xB2, 0xBA, 0xDB, 0x03, 0x8C, 0xE3, 0x80, 0x00
	};
	
	const uint8_t *		src;
	const uint8_t *		end;
	const uint8_t *		nalSrc;
	const uint8_t *		nalPtr;
	const uint8_t *		nalEnd;
	size_t				nalLen;
	const uint8_t *		dataPtr;
	size_t				dataLen;
	const uint8_t *		suffixPtr;
	size_t				suffixLen;
	Boolean				more;
	uint8_t				nalBuf[ 256 ];
	uint8_t *			nalDst;
	uint8_t *			nalLim;
	size_t				nalLen2;
	
	src = kNALUnits;
	end = src + sizeof( kNALUnits );
	
	// NAL 1
	
	err = H264GetNextNALUnit( src, end, 4, &nalPtr, &nalLen, &src );
	require_noerr( err, exit );
	require_action( nalLen == 157, exit, err = -1 );
	require_action( memcmp( &kNALUnits[ 4 ], nalPtr, nalLen ) == 0, exit, err = -1 );
	nalEnd = nalPtr + nalLen;
	
	more = H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr );
	require_action( more, exit, err = -1 );
	require_action( suffixLen == 2, exit, err = -1 );
	require_action( memcmp( suffixPtr, "\x03\x02", suffixLen ) == 0, exit, err = -1 );
	
	more = H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr );
	require_action( more, exit, err = -1 );
	require_action( suffixLen == 0, exit, err = -1 );
	
	more = H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr );
	require_action( !more, exit, err = -1 );
	require_action( nalPtr == nalEnd, exit, err = -1 );
	
	// NAL 2
	
	err = H264GetNextNALUnit( src, end, 4, &nalPtr, &nalLen, &src );
	require_noerr( err, exit );
	require_action( nalLen == 155, exit, err = -1 );
	require_action( memcmp( &kNALUnits[ 4 + 157 + 4 ], nalPtr, nalLen ) == 0, exit, err = -1 );
	nalEnd = nalPtr + nalLen;
	
	more = H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr );
	require_action( more, exit, err = -1 );
	require_action( suffixLen == 2, exit, err = -1 );
	require_action( memcmp( suffixPtr, "\x03\x01", suffixLen ) == 0, exit, err = -1 );
	
	more = H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr );
	require_action( more, exit, err = -1 );
	require_action( suffixLen == 0, exit, err = -1 );
	
	more = H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr );
	require_action( !more, exit, err = -1 );
	require_action( nalPtr == nalEnd, exit, err = -1 );
	
	// NAL 3
	
	err = H264GetNextNALUnit( src, end, 4, &nalPtr, &nalLen, &src );
	require_noerr( err, exit );
	require_action( nalLen == 154, exit, err = -1 );
	require_action( memcmp( &kNALUnits[ 4 + 157 + 4 + 155 +4 ], nalPtr, nalLen ) == 0, exit, err = -1 );
	nalEnd = nalPtr + nalLen;
	
	more = H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr );
	require_action( more, exit, err = -1 );
	require_action( suffixLen == 2, exit, err = -1 );
	require_action( memcmp( suffixPtr, "\x03\x00", suffixLen ) == 0, exit, err = -1 );
	
	more = H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr );
	require_action( more, exit, err = -1 );
	require_action( suffixLen == 1, exit, err = -1 );
	require_action( memcmp( suffixPtr, "\x03", suffixLen ) == 0, exit, err = -1 );
	
	more = H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr );
	require_action( more, exit, err = -1 );
	require_action( suffixLen == 1, exit, err = -1 );
	require_action( memcmp( suffixPtr, "\x03", suffixLen ) == 0, exit, err = -1 );
	
	more = H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr );
	require_action( more, exit, err = -1 );
	require_action( suffixLen == 1, exit, err = -1 );
	require_action( memcmp( suffixPtr, "\x03", suffixLen ) == 0, exit, err = -1 );
	
	more = H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr );
	require_action( !more, exit, err = -1 );
	require_action( nalPtr == nalEnd, exit, err = -1 );
	
	// End
	
	err = H264GetNextNALUnit( src, end, 4, &nalPtr, &nalLen, &src );
	require_action( err == kEndingErr, exit, err = -1 );
	require_action( src == end, exit, err = -1 );
	
	// Removes
	
	i = 1;
	src = kNALUnits;
	end = src + sizeof( kNALUnits );
	while( H264GetNextNALUnit( src, end, 4, &nalSrc, &nalLen, &src ) == kNoErr )
	{
		nalPtr = nalSrc;
		nalEnd = nalPtr + nalLen;
		nalDst = nalBuf;
		nalLim = nalDst + sizeof( nalBuf );
		while( H264EscapeEmulationPrevention( nalPtr, nalEnd, &dataPtr, &dataLen, &suffixPtr, &suffixLen, &nalPtr ) )
		{
			require_action( ( (size_t)( nalLim - nalDst ) ) >= ( dataLen + suffixLen ), exit, err = -1 );
			if( dataPtr )   memcpy( nalDst, dataPtr,   dataLen );   nalDst += dataLen;
			if( suffixPtr )	memcpy( nalDst, suffixPtr, suffixLen ); nalDst += suffixLen;
		}
		
		err = H264RemoveEmulationPrevention( nalBuf, (size_t)( nalDst - nalBuf ), nalBuf, sizeof( nalBuf ), &nalLen2 );
		require_noerr( err, exit );
		if( i == 3 )
		{
			nalLen = sizeof( kNALUnit3 );
			require_action( nalLen2 == nalLen, exit, err = -1 );
			require_action( memcmp( nalBuf, kNALUnit3, nalLen2 ) == 0, exit, err = -1 );
		}
		else
		{
			require_action( nalLen2 == nalLen, exit, err = -1 );
			require_action( memcmp( nalBuf, nalSrc, nalLen2 ) == 0, exit, err = -1 );
		}
		++i;
	}
}
	
exit:
	ForgetANSIFile( &file );
	ForgetMem( &line );
	remove( "/tmp/fcopyline-test.txt" );
#if( TARGET_OS_DARWIN && !TARGET_IPHONE_SIMULATOR && !COMMON_SERVICES_NO_CORE_SERVICES )
	IOObjectForget( &iokitService );
	IOObjectForget( &iokitParent );
#endif
	printf( "MiscUtilsTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
