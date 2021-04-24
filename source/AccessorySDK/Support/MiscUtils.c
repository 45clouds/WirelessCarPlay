/*
	File:    	MiscUtils.c
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
	
	Copyright (C) 2001-2016 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
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
#pragma mark -
#pragma mark == Misc ==
#endif

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

#if 0
#pragma mark -
#endif

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

#if( TARGET_OS_POSIX )
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
	vasprintf( &cmd, inFormat, args );
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
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )

#pragma GCC diagnostic ignored "-Wfloat-equal"

//===========================================================================================================================
//	MiscUtilsTest
//===========================================================================================================================

OSStatus	MiscUtilsTest( void )
{
	OSStatus			err;
	uint8_t				buf[ 256 ];
	unsigned int		i;
	uint8_t				u8[ 5 ], u8_16[ 16 ];
	int16_t				s16[ 5 ];
	uint16_t			u16[ 5 ];
	uint32_t			u32[ 5 ];
	uint64_t			u64[ 5 ];
	FILE *				file = NULL;
	char *				line = NULL;
#if( TARGET_OS_POSIX )
	struct stat			sb;
#endif
	
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
	
#if( !TARGET_OS_IPHONE )
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
#endif
	
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
	
	const uint8_t *		src;
	const uint8_t *		end;
	const uint8_t *		nalPtr;
	size_t				nalLen;
	
	src = kNALUnits;
	end = src + sizeof( kNALUnits );
	
	// NAL 1
	
	err = H264GetNextNALUnit( src, end, 4, &nalPtr, &nalLen, &src );
	require_noerr( err, exit );
	require_action( nalLen == 157, exit, err = -1 );
	require_action( memcmp( &kNALUnits[ 4 ], nalPtr, nalLen ) == 0, exit, err = -1 );
	
	// NAL 2
	
	err = H264GetNextNALUnit( src, end, 4, &nalPtr, &nalLen, &src );
	require_noerr( err, exit );
	require_action( nalLen == 155, exit, err = -1 );
	require_action( memcmp( &kNALUnits[ 4 + 157 + 4 ], nalPtr, nalLen ) == 0, exit, err = -1 );
	
	// NAL 3
	
	err = H264GetNextNALUnit( src, end, 4, &nalPtr, &nalLen, &src );
	require_noerr( err, exit );
	require_action( nalLen == 154, exit, err = -1 );
	require_action( memcmp( &kNALUnits[ 4 + 157 + 4 + 155 +4 ], nalPtr, nalLen ) == 0, exit, err = -1 );
	
	// End
	
	err = H264GetNextNALUnit( src, end, 4, &nalPtr, &nalLen, &src );
	require_action( err == kEndingErr, exit, err = -1 );
	require_action( src == end, exit, err = -1 );
	err = kNoErr;
}
	
exit:
	ForgetANSIFile( &file );
	ForgetMem( &line );
	printf( "MiscUtilsTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
