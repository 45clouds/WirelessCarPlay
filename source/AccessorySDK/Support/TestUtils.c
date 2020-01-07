/*
	File:    	TestUtils.c
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
	
	Copyright (C) 2014-2015 Apple Inc. All Rights Reserved.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "TestUtils.h"

#include <ctype.h>
#include <stdio.h>

#include "AtomicUtils.h"
#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "LogUtils.h"
#include "MiscUtils.h"
#include "PrintFUtils.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "TickUtils.h"

#include CF_HEADER

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#define kTUFaultHashBucketCount		31

#define kTUKey_Detail				CFSTR( "detail" )
#define kTUKey_Duration				CFSTR( "duration" )
#define kTUKey_Failures				CFSTR( "failures" )
#define kTUKey_Message				CFSTR( "message" )
#define kTUKey_Name					CFSTR( "name" )
#define kTUKey_Passes				CFSTR( "passes" )
#define kTUKey_Total				CFSTR( "total" )
#define kTUKey_Type					CFSTR( "type" )

typedef struct
{
	const char *		prefix;
	Boolean				started;
	
}	TULogContext;

#define kTUTestContextInitializer			{ "", 0, 0, 0, kNoErr }
#define TUTestContextInitialize( CTX )		memset( (CTX), 0, sizeof( *(CTX) ) )

typedef struct TUFaultNode		TUFaultNode;
struct TUFaultNode
{
	TUFaultNode *		next;
	char *				name;
	Value64				value;
};

typedef struct
{
	CFMutableArrayRef			testsArray;
	CFMutableArrayRef			globalFailuresArray;
	Boolean						gotEnd;
	CFMutableDictionaryRef		testDict;
	CFMutableArrayRef			failuresArray;
	Boolean						gotResult;
	Boolean						gotDuration;
	
}	TUConvertToJUnitContext;

static Boolean	_TUTestEnabled( const char *inName );
static int		_TULogPrintFCallback( const char *inStr, size_t inSize, void *inContext );
static void		_TUPrintF( const char *inFormat, ... );
static int		_TUPrintFCallback( const char *inStr, size_t inSize, void *inContext );
static OSStatus	_TUFaultLookup( const char *inName, TUFaultNode ***outSlot );
static OSStatus	_TUConvertToJUnitProcessLine( TUConvertToJUnitContext *ctx, const char *inLine );
static OSStatus
	_TUConvertToJUnitParseAssertion( 
		const char *	inValue, 
		const char **	outTypePtr, 
		size_t *		outTypeLen, 
		const char **	outMessagePtr, 
		size_t *		outMessageLen, 
		const char **	outDetailPtr, 
		size_t *		outDetailLen );
static OSStatus	_TUConvertToJUnitWriteJUnitXML( TUConvertToJUnitContext *ctx, const char *inJUnitPath );

// Configuration

static Boolean				gTUBreakOnFail				= true;
static Boolean				gTUDontRunLeaks				= false;
Boolean						gTUExcludeNonTestUtilsTests	= false;
static const char *			gTUFilter					= NULL;
static const char *			gTUJUnitXMLOutputPath		= NULL;
static LogLevel				gTULogLevel					= kLogLevelNotice;
static const char *			gTUOutputPath				= NULL;
static const char *			gTUQualifier				= NULL;
static Boolean				gTUStopOnFirstFail			= true;
static int32_t				gTUTotalExpectedTests		= 0;
static Boolean				gTUUserMode					= true;

// State

static pthread_mutex_t		gTUFaultLock				= PTHREAD_MUTEX_INITIALIZER;
static TUFaultNode *		gTUFaultHashTable[ kTUFaultHashBucketCount ];
static FILE *				gTUOutputFile				= NULL;
static FILE *				gTUOutputFilePtr			= NULL;
static int32_t				gTUTotalPasses				= 0;
static int32_t				gTUTotalFailures			= 0;

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	TUInitialize
//===========================================================================================================================

OSStatus	TUInitialize( int inArgC, const char **inArgV )
{
	OSStatus			err;
	int					i;
	const char *		arg;
	const char *		arg2;
	const char *		arg3;
	
	for( i = 1; i < inArgC; )
	{
		arg = inArgV[ i++ ];
		
		if( 0 ) {}
		
		// BreakOnFail
		
		else if( stricmp( arg, "--BreakOnFail" ) == 0 )
		{
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an argument\n", arg ); continue; }
			arg2 = inArgV[ i++ ];
			if(      IsTrueString(  arg2, kSizeCString ) ) gTUBreakOnFail = true;
			else if( IsFalseString( arg2, kSizeCString ) ) gTUBreakOnFail = false;
			else { FPrintF( stdout, "#=error: '%s' option bad argument '%s'\n", arg, arg2 ); continue; }
		}
		
		// ConvertTURtoJUnit
		
		else if( stricmp( arg, "--ConvertTURtoJUnit" ) == 0 )
		{
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an input path argument\n", arg ); exit( 1 ); }
			arg2 = inArgV[ i++ ];
			
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an output path argument\n", arg ); exit( 1 ); }
			arg3 = inArgV[ i++ ];
			
			err = TUConvertToJUnit( arg2, arg3 );
			exit( err ? 1 : 0 );
		}
		
		// DontRunLeaks
		
		else if( stricmp( arg, "--DontRunLeaks" ) == 0 )
		{
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an argument\n", arg ); continue; }
			arg2 = inArgV[ i++ ];
			if(      IsTrueString(  arg2, kSizeCString ) ) gTUDontRunLeaks = true;
			else if( IsFalseString( arg2, kSizeCString ) ) gTUDontRunLeaks = false;
			else { FPrintF( stdout, "#=error: '%s' option bad argument '%s'\n", arg, arg2 ); continue; }
		}
		
		// ExcludeNonTestUtilsTests
		
		else if( stricmp( arg, "--ExcludeNonTestUtilsTests" ) == 0 )
		{
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an argument\n", arg ); continue; }
			arg2 = inArgV[ i++ ];
			if(      IsTrueString(  arg2, kSizeCString ) ) gTUExcludeNonTestUtilsTests = true;
			else if( IsFalseString( arg2, kSizeCString ) ) gTUExcludeNonTestUtilsTests = false;
			else { FPrintF( stdout, "#=error: '%s' option bad argument '%s'\n", arg, arg2 ); continue; }
		}
		
		// Filter
		
		else if( stricmp( arg, "--Filter" ) == 0 )
		{
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an argument\n", arg ); continue; }
			arg2 = inArgV[ i++ ];
			gTUFilter = arg2;
		}
		
		// JUnitXMLOutputPath
		
		else if( stricmp( arg, "--JUnitXMLOutputPath" ) == 0 )
		{
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an argument\n", arg ); continue; }
			arg2 = inArgV[ i++ ];
			gTUJUnitXMLOutputPath = arg2;
		}
		
		// Level
		
		else if( stricmp( arg, "--Level" ) == 0 )
		{
			LogLevel		level;
			
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an argument\n", arg ); continue; }
			arg2 = inArgV[ i++ ];
			level = LUStringToLevel( arg2 );
			if( level == kLogLevelUninitialized )
			{
				FPrintF( stdout, "#=error: '%s' option bad argument '%s'\n", arg, arg2 );
				continue;
			}
			gTULogLevel = level;
		}
		
		// LogControl
		
		else if( stricmp( arg, "--LogControl" ) == 0 )
		{
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an argument\n", arg ); continue; }
			arg2 = inArgV[ i++ ];
			err = LogControl( arg2 );
			if( err ) { FPrintF( stdout, "#=error: LogControl( \"%s\" ) failed: %#m\n", arg2, err ); continue; }
		}
		
		// OutputPath
		
		else if( stricmp( arg, "--OutputPath" ) == 0 )
		{
			FILE *		file;
			
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an argument\n", arg ); continue; }
			arg2 = inArgV[ i++ ];
			
			file = fopen( arg2, "w" );
			err = map_global_value_errno( file, file );
			if( err ) { FPrintF( stdout, "#=error: Open output path '%s' failed: %#m\n", arg2, err ); continue; }
			
			ForgetANSIFile( &gTUOutputFile );
			gTUOutputFile		= file;
			gTUOutputFilePtr	= file;
			gTUOutputPath		= arg2;
		}
		
		// Qualifier
		
		else if( stricmp( arg, "--Qualifier" ) == 0 )
		{
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an argument\n", arg ); continue; }
			arg2 = inArgV[ i++ ];
			gTUQualifier = arg2;
		}
		
		// StopOnFirstFail
		
		else if( stricmp( arg, "--StopOnFirstFail" ) == 0 )
		{
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an argument\n", arg ); continue; }
			arg2 = inArgV[ i++ ];
			if(      IsTrueString(  arg2, kSizeCString ) ) gTUStopOnFirstFail = true;
			else if( IsFalseString( arg2, kSizeCString ) ) gTUStopOnFirstFail = false;
			else { FPrintF( stdout, "#=error: '%s' option bad argument '%s'\n", arg, arg2 ); continue; }
		}
		
		// UserMode
		
		else if( stricmp( arg, "--UserMode" ) == 0 )
		{
			if( i >= inArgC ) { FPrintF( stdout, "#=error: '%s' option requires an argument\n", arg ); continue; }
			arg2 = inArgV[ i++ ];
			if(      IsTrueString(  arg2, kSizeCString ) ) gTUUserMode = true;
			else if( IsFalseString( arg2, kSizeCString ) ) gTUUserMode = false;
			else { FPrintF( stdout, "#=error: '%s' option bad argument '%s'\n", arg, arg2 ); continue; }
		}
		
		// Unknown
		
		else
		{
			FPrintF( stdout, "#=error: unknown option '%s'\n", arg );
		}
	}
	if( !gTUOutputPath && gTUJUnitXMLOutputPath )
	{
		FPrintF( stdout, "#=error: --JUnitXMLOutputPath specified when not using a TU file\n" );
	}
	if( !gTUOutputFilePtr ) gTUOutputFilePtr = stdout;
	setvbuf( gTUOutputFilePtr, NULL, _IOLBF, BUFSIZ );
	setvbuf( stdout, NULL, _IOLBF, BUFSIZ );
	setvbuf( stderr, NULL, _IOLBF, BUFSIZ );
	
	if( !gTUUserMode ) _TUPrintF( "T=\n" );
	gTUTotalPasses			= 0;
	gTUTotalFailures		= 0;
	gTUTotalExpectedTests	= 0;
	return( kNoErr );
}

//===========================================================================================================================
//	TUFinalize
//===========================================================================================================================

void	TUFinalize( void )
{
	int32_t		actualTests;
	
	if( !gTUDontRunLeaks )
	{
		TUTestCheckLeaks( NULL, kTUFlags_None, __FILE__, __LINE__, __ROUTINE__ );
	}
	
	// For user mode, print a summary so it's easy to quickly see the final result.
	
	if( gTUUserMode )
	{
		if( gTUTotalPasses >= gTUTotalExpectedTests )
		{
			_TUPrintF( "\nAll %d tests passed\n", gTUTotalPasses );
		}
		else
		{
			_TUPrintF( "### %d tests failed, %d of %d tests passed\n", gTUTotalFailures, gTUTotalPasses, gTUTotalExpectedTests );
		}
	}
	
	// Check that the total number of tests matches the expected count.
	
	actualTests = gTUTotalPasses + gTUTotalFailures;
	if( ( actualTests == 0 ) || ( actualTests != gTUTotalExpectedTests ) )
	{
		if( gTUUserMode )
		{
			_TUPrintF( "### Total tests mismatch: %d actual vs %d expected\n", actualTests, gTUTotalExpectedTests );
		}
		else
		{
			_TUPrintF( "f=total:%d/%d\n", actualTests, gTUTotalExpectedTests );
		}
	}
	else if( TULogLevelEnabled( NULL, kLogLevelTrace ) )
	{
		if( gTUUserMode )	_TUPrintF( "Total tests matched expected: %d total tests\n", actualTests );
		else				_TUPrintF( "p=total:%d/%d\n", actualTests, gTUTotalExpectedTests );
	}
	
	// Mark the end of all tests and close the output file.
	
	if( !gTUUserMode ) _TUPrintF( "Z=\n" );
	
	ForgetANSIFile( &gTUOutputFile );
	gTUOutputFilePtr = NULL;
	
	if( gTUOutputPath && gTUJUnitXMLOutputPath )
	{
		TUConvertToJUnit( gTUOutputPath, gTUJUnitXMLOutputPath );
	}
}

//===========================================================================================================================
//	TUSetExpectedTestCount
//===========================================================================================================================

void	TUSetExpectedTestCount( uint32_t inTotalTests )
{
	gTUTotalExpectedTests = (int32_t) inTotalTests;
}

//===========================================================================================================================
//	_TUPerformTest
//===========================================================================================================================

void	_TUPerformTest( const char *inName, TUTest_f inFunc )
{
	TUTestContext		ctx = kTUTestContextInitializer;
	uint64_t			ticks;
	Boolean				pass;
	
#if( COMPILER_OBJC )
	@autoreleasepool
	{
	@try
	{
#endif
	
	require_quiet( _TUTestEnabled( inName ), exit );
	if( !gTUOutputFilePtr ) gTUOutputFilePtr = stdout;
	ctx.testName = inName;
	
	if( !gTUUserMode )
	{
		if( gTUQualifier && ( *gTUQualifier != '\0' ) )
		{
			_TUPrintF( "t=%s.%s\n", inName, gTUQualifier );
		}
		else
		{
			_TUPrintF( "t=%s\n", inName );
		}
	}
	
	ticks = UpTicks();
	inFunc( &ctx );
	ticks = UpTicks() - ticks;
	
#if( COMPILER_OBJC )
	} // Ends @try
	@catch( NSException *exception )
	{
		ticks = UpTicks() - ticks;
		atomic_add_32( &gTUTotalFailures, 1 );
		++ctx.testFails;
		if( gTUUserMode )	_TUPrintF( "### Objective-C exception thrown: %@\n", exception );
		else				TULogF( NULL, kLogLevelNotice, "Objective-C exception: ", "%@", exception );
		if( gTUBreakOnFail && DebugIsDebuggerPresent() )
		{
			DebugEnterDebugger( true );
		}
	}
	} // Ends @autoreleasepool
#endif
	
	pass = ( ctx.testPasses > 0 ) && ( ctx.testFails == 0 );
	if( gTUUserMode )
	{
		_TUPrintF( "%s%s: %s\n", pass ? "" : "### ", inName, pass ? "passed" : "FAILED" );
	}
	else
	{
		_TUPrintF( "r=%u/%u\n", ctx.testPasses, ctx.testPasses + ctx.testFails );
		_TUPrintF( "d=%fs\n", UpTicksToSecondsF( ticks ) );
		_TUPrintF( "z=\n" );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	TUTestCheckLeaks
//===========================================================================================================================

OSStatus
	TUTestCheckLeaks( 
		TUTestContext *	inTestCtx, 
		TUFlags			inFlags, 
		const char *	inFilename, 
		long			inLineNumber, 
		const char *	inFunction )
{
#if( TARGET_OS_DARWIN )
	OSStatus			fatalErr = kNoErr;
	OSStatus			err;
	char				leaksCmd[ 64 ];
	char				leaksOutputPath[ 64 ];
	char				buf[ 128 ];
	FILE *				leaksProcess		= NULL;
	FILE *				leaksOutputFile		= NULL;
	Boolean				createdLeaksOutput	= false;
	size_t				n, n2;
	Boolean				pass = false;
	
	// Run leaks and read all of it's output to a temporary file.
	
	snprintf( leaksCmd, sizeof( leaksCmd ), "/usr/bin/leaks %d", (int) getpid() );
	leaksProcess = popen( leaksCmd, "r" );
	err = map_global_value_errno( leaksProcess, leaksProcess );
	require_noerr_quiet( err, exit );
	
	snprintf( leaksOutputPath, sizeof( leaksOutputPath ), "/tmp/leaks-%d-output", (int) getpid() );
	leaksOutputFile = fopen( leaksOutputPath, "w" );
	err = map_global_value_errno( leaksOutputFile, leaksOutputFile );
	require_noerr_quiet( err, exit );
	createdLeaksOutput = true;
	
	for( ;; )
	{
		n = fread( buf, 1, sizeof( buf ), leaksProcess );
		if( n == 0 ) break;
		
		n2 = fwrite( buf, 1, n, leaksOutputFile );
		require_action_quiet( n2 == n, exit, err = kWriteErr );
		require_noerr_quiet( err, exit );
	}
	fclose( leaksOutputFile );
	leaksOutputFile = NULL;
	
	// Wait for leaks to exit and get the result.
	
	err = pclose( leaksProcess );
	leaksProcess = NULL;
	if( err == -1 )
	{
		err = errno;
		if( !err ) err = -1;
	}
	else if( err != 0 )
	{
		err = WEXITSTATUS( err );
	}
	
	// If leaks reported an error (non-zero exit status) then optionally write the full leaks report to the test report.
	
	pass = !err;
	if( err )
	{
		if( TULogLevelEnabled( inTestCtx, kLogLevelNotice ) )
		{
			leaksOutputFile = fopen( leaksOutputPath, "r" );
			err = map_global_value_errno( leaksOutputFile, leaksOutputFile );
			require_noerr_quiet( err, exit );
			
			for( ;; )
			{
				n = fread( buf, 1, sizeof( buf ), leaksOutputFile );
				if( n == 0 ) break;
				TULogF( inTestCtx, kLogLevelNotice, "Leaks: ", "%.*s", (int) n, buf );
			}
			fclose( leaksOutputFile );
			leaksOutputFile = NULL;
		}
	}
	err = kNoErr;
	
exit:
	if( leaksProcess )			pclose( leaksProcess );
	if( leaksOutputFile )		fclose( leaksOutputFile );
	if( createdLeaksOutput )	remove( leaksOutputPath );
	if( inTestCtx )
	{
		atomic_add_32( pass ? &gTUTotalPasses : &gTUTotalFailures, 1 );
		inTestCtx->testPasses += pass;
		inTestCtx->testFails  += !pass;
	}
	if( !pass && ( ( inFlags & kTUFlag_Fatal ) || gTUStopOnFirstFail ) ) fatalErr = kEndingErr;
	if( !pass || TULogLevelEnabled( inTestCtx, kLogLevelTrace ) )
	{
		if( pass )
		{
			if( gTUUserMode )	_TUPrintF( "Leaks passed: " );
			else				_TUPrintF( "p=leaks:" );
		}
		else
		{
			if( gTUUserMode )	_TUPrintF( "### Leaks FAILED: " );
			else				_TUPrintF( "f=leaks:" );
		}
		_TUPrintF( "%s:%ld, %###s()", GetLastFilePathSegment( inFilename, kSizeCString, NULL ), inLineNumber, inFunction );
		if( err ) _TUPrintF( ", %#m", err );
		_TUPrintF( "\n" );
	}
	if( !pass && gTUBreakOnFail && DebugIsDebuggerPresent() )
	{
		DebugEnterDebugger( true );
	}
	return( fatalErr );
#else
	(void) inFlags;
	(void) inFilename;
	(void) inLineNumber;
	(void) inFunction;
	
	TULogF( inTestCtx, kLogLevelInfo, "", "Leaks not supported on this platform\n" );
	return( kNoErr );
#endif
}

//===========================================================================================================================
//	_TUTestEnabled
//===========================================================================================================================

static Boolean	_TUTestEnabled( const char *inName )
{
	const char *		src;
	const char *		end;
	const char *		ptr;
	size_t				len;
	
	if( gTUFilter )
	{
		src = gTUFilter;
		end = src + strlen( src );
		while( TextSep( src, end, ",", &ptr, &len, &src ) )
		{
			if( strnicmpx( ptr, len, inName ) == 0 )
			{
				return( true );
			}
		}
		return( false );
	}
	return( true );
}

//===========================================================================================================================
//	TUTestRequire
//===========================================================================================================================

OSStatus
	TUTestRequire( 
		TUTestContext *	inTestCtx, 
		TUFlags			inFlags, 
		int				inValue, 
		const char *	inFilename, 
		long			inLineNumber, 
		const char *	inFunction, 
		const char *	inTestString )
{
	OSStatus		fatalErr = kNoErr;
	Boolean			pass;
	
	pass = inValue != false;
	atomic_add_32( pass ? &gTUTotalPasses : &gTUTotalFailures, 1 );
	inTestCtx->testPasses += pass;
	inTestCtx->testFails  += !pass;
	if( !pass && ( ( inFlags & kTUFlag_Fatal ) || gTUStopOnFirstFail ) ) fatalErr = kEndingErr;
	require_quiet( !pass || TULogLevelEnabled( inTestCtx, kLogLevelTrace ), exit );
	
	if( gTUUserMode )	_TUPrintF( pass ? "" : "### " );
	else				_TUPrintF( "%c=bool:", pass ? 'p' : 'f' );
	_TUPrintF( "%s:%ld, %###s(), %''s\n", 
		GetLastFilePathSegment( inFilename, kSizeCString, NULL ), inLineNumber, inFunction, inTestString );
	
	if( !pass && gTUBreakOnFail && DebugIsDebuggerPresent() )
	{
		DebugEnterDebugger( true );
	}
	
exit:
	if( !inValue && !inTestCtx->testStatus ) inTestCtx->testStatus = kValueErr;
	return( fatalErr );
}

//===========================================================================================================================
//	TUTestRequireNoErr
//===========================================================================================================================

OSStatus
	TUTestRequireNoErr( 
		TUTestContext *	inTestCtx, 
		TUFlags			inFlags, 
		OSStatus		inErrorCode, 
		const char *	inFilename, 
		long			inLineNumber, 
		const char *	inFunction )
{
	OSStatus		fatalErr = kNoErr;
	Boolean			pass;
	
	pass = !inErrorCode;
	atomic_add_32( pass ? &gTUTotalPasses : &gTUTotalFailures, 1 );
	inTestCtx->testPasses += pass;
	inTestCtx->testFails  += !pass;
	if( !pass && ( ( inFlags & kTUFlag_Fatal ) || gTUStopOnFirstFail ) ) fatalErr = inErrorCode;
	require_quiet( !pass || TULogLevelEnabled( inTestCtx, kLogLevelTrace ), exit );
	
	if( gTUUserMode )	_TUPrintF( pass ? "" : "### " );
	else				_TUPrintF( "%c=err:", pass ? 'p' : 'f' );
	_TUPrintF( "%s:%ld, %###s(), %#m\n", 
		GetLastFilePathSegment( inFilename, kSizeCString, NULL ), inLineNumber, inFunction, inErrorCode );
	
	if( !pass && gTUBreakOnFail && DebugIsDebuggerPresent() )
	{
		DebugEnterDebugger( true );
	}
	
exit:
	if( inErrorCode && !inTestCtx->testStatus ) inTestCtx->testStatus = inErrorCode;
	return( fatalErr );
}

//===========================================================================================================================
//	TULogLevelEnabled
//===========================================================================================================================

Boolean	TULogLevelEnabled( TUTestContext *inTestCtx, LogLevel inLevel )
{
	(void) inTestCtx;
	
	return( gTULogLevel <= inLevel );
}

//===========================================================================================================================
//	TULogF
//===========================================================================================================================

void	TULogF( TUTestContext *inTestCtx, LogLevel inLevel, const char *inPrefix, const char *inFormat, ... )
{
	va_list		args;
	
	if( inLevel < gTULogLevel ) return;
	va_start( args, inFormat );
	TULogV( inTestCtx, inLevel, inPrefix, inFormat, args );
	va_end( args );
}

//===========================================================================================================================
//	TULogV
//===========================================================================================================================

void	TULogV( TUTestContext *inTestCtx, LogLevel inLevel, const char *inPrefix, const char *inFormat, va_list inArgs )
{
	TULogContext		ctx = { inPrefix ? inPrefix : "", false };
	
	(void) inTestCtx;
	
	if( inLevel < gTULogLevel ) return;
	VCPrintF( _TULogPrintFCallback, &ctx, inFormat, inArgs );
}

//===========================================================================================================================
//	_TULogPrintFCallback
//===========================================================================================================================

static int	_TULogPrintFCallback( const char *inStr, size_t inSize, void *inContext )
{
	TULogContext * const		ctx = (TULogContext *) inContext;
	const char *				src = inStr;
	const char * const			end = inStr + inSize;
	const char *				ptr;
	
	while( src < end )
	{
		if( !ctx->started )
		{
			FPrintF( gTUOutputFilePtr, "%s%s", gTUUserMode ? "" : "#=", ctx->prefix );
			ctx->started = true;
		}
		for( ptr = src; ( src < end ) && ( *src != '\n' ); ++src ) {}
		FPrintF( gTUOutputFilePtr, "%.*s", (int)( src - ptr ), ptr );
		if( src < end )
		{
			++src;
			FPrintF( gTUOutputFilePtr, "\n" );
			ctx->started = false;
		}
	}
	if( ctx->started && ( inSize == 0 ) )
	{
		FPrintF( gTUOutputFilePtr, "\n" );
		ctx->started = false;
	}
	return( (int) inSize );
}

//===========================================================================================================================
//	_TUPrintF
//===========================================================================================================================

static void	_TUPrintF( const char *inFormat, ... )
{
	va_list		args;
	
	va_start( args, inFormat );
	VCPrintF( _TUPrintFCallback, NULL, inFormat, args );
	va_end( args );
}

//===========================================================================================================================
//	_TUPrintFCallback
//===========================================================================================================================

static int	_TUPrintFCallback( const char *inStr, size_t inSize, void *inContext )
{
	(void) inContext;
	
	if( inSize > 0 )
	{
		if( gTUOutputFilePtr )	FPrintF( gTUOutputFilePtr, "%.*s", (int) inSize, inStr );
		if( !gTUUserMode )		FPrintF( stderr, "%.*s", (int) inSize, inStr );
	}
	return( (int) inSize );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	TUFaultEnable
//===========================================================================================================================

OSStatus	TUFaultEnable( const char *inName, Value64 inValue )
{
	OSStatus			err;
	TUFaultNode **		slot;
	TUFaultNode *		node = NULL;
	
	pthread_mutex_lock( &gTUFaultLock );
	
	err = _TUFaultLookup( inName, &slot );
	if( !err )
	{
		( *slot )->value = inValue;
	}
	else
	{
		node = (TUFaultNode *) calloc( 1, sizeof( *node ) );
		require_action( node, exit, err = kNoMemoryErr );
		
		node->name = strdup( inName );
		require_action( node->name, exit, err = kNoMemoryErr );
		node->value = inValue;
		
		node->next = *slot;
		*slot = node;
		node = NULL;
		err = kNoErr;
	}
	
exit:
	FreeNullSafe( node );
	pthread_mutex_unlock( &gTUFaultLock );
	return( err );
}

//===========================================================================================================================
//	TUFaultDisable
//===========================================================================================================================

OSStatus	TUFaultDisable( const char *inName )
{
	OSStatus			err;
	TUFaultNode **		slot;
	TUFaultNode *		node;
	
	pthread_mutex_lock( &gTUFaultLock );
	
	err = _TUFaultLookup( inName, &slot );
	require_noerr_quiet( err, exit );
	
	node = *slot;
	*slot = node->next;
	free( node->name );
	free( node );
	
exit:
	pthread_mutex_unlock( &gTUFaultLock );
	return( err );
}

//===========================================================================================================================
//	TUFaultInject
//===========================================================================================================================

OSStatus	TUFaultInject( const char *inName, TUFaultDataType inType, void *inPtr )
{
	OSStatus			err;
	TUFaultNode **		slot;
	TUFaultNode *		node;
	
	pthread_mutex_lock( &gTUFaultLock );
	
	err = _TUFaultLookup( inName, &slot );
	require_noerr_quiet( err, exit );
	node = *slot;
	
	switch( inType )
	{
		case kTUFaultDataType_Boolean:	*( (Boolean *)  inPtr ) = (Boolean)  node->value.s64; break;
		case kTUFaultDataType_SInt8:	*( (int8_t *)   inPtr ) = (int8_t)   node->value.s64; break;
		case kTUFaultDataType_UInt8:	*( (uint8_t *)  inPtr ) = (uint8_t)  node->value.u64; break;
		case kTUFaultDataType_SInt16:	*( (int16_t *)  inPtr ) = (int16_t)  node->value.s64; break;
		case kTUFaultDataType_UInt16:	*( (uint16_t *) inPtr ) = (uint16_t) node->value.u64; break;
		case kTUFaultDataType_SInt32:	*( (int32_t *)  inPtr ) = (int32_t)  node->value.s64; break;
		case kTUFaultDataType_UInt32:	*( (uint32_t *) inPtr ) = (uint32_t) node->value.u64; break;
		case kTUFaultDataType_SInt64:	*( (int64_t *)  inPtr ) = (int64_t)  node->value.s64; break;
		case kTUFaultDataType_UInt64:	*( (uint64_t *) inPtr ) = (uint64_t) node->value.u64; break;
		case kTUFaultDataType_int:		*( (int *)      inPtr ) = (int)      node->value.s64; break;
		case kTUFaultDataType_size_t:	*( (size_t *)   inPtr ) = (size_t)   node->value.u64; break;
		case kTUFaultDataType_Float32:	*( (Float32 *)  inPtr ) = (Float32)  node->value.f64; break;
		case kTUFaultDataType_Float64:	*( (Float64 *)  inPtr ) = (Float64)  node->value.f64; break;
		case kTUFaultDataType_float:	*( (float *)    inPtr ) = (float)    node->value.f64; break;
		case kTUFaultDataType_double:	*( (double *)   inPtr ) = (double)   node->value.f64; break;
		default: err = kUnsupportedDataErr; goto exit;
	}
	
exit:
	pthread_mutex_unlock( &gTUFaultLock );
	return( err );
}

//===========================================================================================================================
//	_TUFaultLookup
//
//	Assumes gTUFaultLock is locked.
//===========================================================================================================================

static OSStatus	_TUFaultLookup( const char *inName, TUFaultNode ***outSlot )
{
	uint32_t			hash;
	const uint8_t *		ptr;
	uint8_t				b;
	TUFaultNode **		slot;
	TUFaultNode *		node;
	
	// FNV-1a hash.
	
	hash = 0x811c9dc5U;
	for( ptr = (const uint8_t *) inName; ( b = *ptr ) != '\0'; ++ptr )
	{
		hash ^= b;
		hash *= 0x01000193;
	}
	hash %= kTUFaultHashBucketCount;
	
	// Search for an existing node with the specified name or the slot where a new one would go.
	
	for( slot = &gTUFaultHashTable[ hash ]; ( node = *slot ) != NULL; slot = &node->next )
	{
		if( stricmp( node->name, inName ) == 0 )
		{
			break;
		}
	}
	if( outSlot ) *outSlot = slot;
	return( node ? kNoErr : kNotFoundErr );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	TUConvertToJUnit
//===========================================================================================================================

OSStatus	TUConvertToJUnit( const char *inTUPath, const char *inJUnitPath )
{
	TUConvertToJUnitContext		ctx;
	OSStatus					err;
	FILE *						file = NULL;
	FILE *						filePtr;
	char *						line;
	
	memset( &ctx, 0, sizeof( ctx ) );
	
	// Parse the TestUtils report into a dictionary.
	
	if( strcmp( inTUPath, "-" ) == 0 )
	{
		filePtr = stdin;
	}
	else
	{
		file = fopen( inTUPath, "r" );
		err = map_global_value_errno( file, file );
		require_noerr_quiet( err, exit );
		filePtr = file;
	}
	
	while( ( err = fcopyline( filePtr, &line, NULL ) ) == kNoErr )
	{
		err = _TUConvertToJUnitProcessLine( &ctx, line );
		ForgetMem( &line );
		require_noerr_quiet( err, exit );
	}
	require_action_quiet( err == kEndingErr, exit, err = kReadErr );
	require_action_quiet( ctx.gotEnd, exit, err = kUnexpectedErr );
	
	// Write the dictionary out as a JUnit XML report.
	
	err = _TUConvertToJUnitWriteJUnitXML( &ctx, inJUnitPath );
	require_noerr_quiet( err, exit );
	
exit:
	ForgetANSIFile( &file );
	ForgetCF( &ctx.testsArray );
	ForgetCF( &ctx.globalFailuresArray );
	ForgetCF( &ctx.testDict );
	ForgetCF( &ctx.failuresArray );
	if( err ) FPrintF( stdout, "#=error: Convert TestUtils '%s' to JUnit '%s' failed: %#m\n", inTUPath, inJUnitPath, err );
	return( err );
}

//===========================================================================================================================
//	_TUConvertToJUnitProcessLine
//===========================================================================================================================

static OSStatus	_TUConvertToJUnitProcessLine( TUConvertToJUnitContext *ctx, const char *inLine )
{
	OSStatus				err;
	char					type;
	const char *			value;
	unsigned int			passes, total;
	double					d;
	int						n;
	const char *			typePtr;
	size_t					typeLen;
	const char *			messagePtr;
	size_t					messageLen;
	const char *			detailPtr;
	size_t					detailLen;
	CFMutableArrayRef		failuresArray;
	
	require_action_quiet( !ctx->gotEnd, exit, err = kUnexpectedErr );
	
	type = inLine[ 0 ];
	require_action_quiet( type != '\0', exit, err = kMalformedErr );
	require_action_quiet( inLine[ 1 ] == '=', exit, err = kMalformedErr );
	value = &inLine[ 2 ];
	
	switch( type )
	{
		// Test report begin ("T=").
		
		case 'T':
			require_action_quiet( !ctx->testsArray, exit, err = kAlreadyInUseErr );
			ctx->testsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			require_action( ctx->testsArray, exit, err = kNoMemoryErr );
			break;
		
		// Test report end ("Z=").
		
		case 'Z':
			require_action_quiet( ctx->testsArray, exit, err = kNotPreparedErr );
			require_action_quiet( !ctx->testDict, exit, err = kUnexpectedErr );
			ctx->gotEnd = true;
			break;
		
		// Start test ("t=").
		
		case 't':
			require_action_quiet( ctx->testsArray, exit, err = kNotPreparedErr );
			require_action_quiet( !ctx->testDict, exit, err = kAlreadyInUseErr );
			
			ctx->testDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			require_action( ctx->testDict, exit, err = kNoMemoryErr );
			require_action_quiet( *value != '\0', exit, err = kNameErr );
			CFDictionarySetCString( ctx->testDict, kTUKey_Name, value, kSizeCString );
			CFArrayAppendValue( ctx->testsArray, ctx->testDict );
			break;
		
		// End test ("z=").
		
		case 'z':
			require_action_quiet( ctx->testsArray, exit, err = kNotPreparedErr );
			require_action_quiet( ctx->testDict, exit, err = kNotPreparedErr );
			ForgetCF( &ctx->testDict );
			ForgetCF( &ctx->failuresArray );
			ctx->gotResult		= false;
			ctx->gotDuration	= false;
			break;
		
		// Result ("r=").
		
		case 'r':
			require_action_quiet( !ctx->gotResult, exit, err = kDuplicateErr );
			n = sscanf( value, "%u/%u", &passes, &total );
			require_action_quiet( n == 2, exit, err = kMalformedErr );
			require_action_quiet( passes <= total, exit, err = kRangeErr );
			
			require_action_quiet( ctx->testDict, exit, err = kNotPreparedErr );
			err = CFDictionarySetUInt64( ctx->testDict, kTUKey_Passes, passes );
			require_noerr( err, exit );
			err = CFDictionarySetUInt64( ctx->testDict, kTUKey_Total, total );
			require_noerr( err, exit );
			ctx->gotResult = true;
			break;
		
		// Pass ("p=").
		
		case 'p':
			if( ctx->testDict )
			{
				require_action_quiet( !ctx->gotResult, exit, err = kUnexpectedErr );
			}
			
			// JUnit doesn't have a "pass" entry so verify the line is formatted correctly, but don't do anything with it.
			
			err = _TUConvertToJUnitParseAssertion( value, &typePtr, &typeLen, &messagePtr, &messageLen, &detailPtr, &detailLen );
			require_noerr_quiet( err, exit );
			break;
		
		// Failure ("f=").
		
		case 'f':
			if( ctx->testDict )
			{
				require_action_quiet( !ctx->gotResult, exit, err = kUnexpectedErr );
				if( !ctx->failuresArray )
				{
					ctx->failuresArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
					require_action( ctx->failuresArray, exit, err = kNoMemoryErr );
					CFDictionarySetValue( ctx->testDict, kTUKey_Failures, ctx->failuresArray );
				}
				failuresArray = ctx->failuresArray;
			}
			else
			{
				if( !ctx->globalFailuresArray )
				{
					ctx->globalFailuresArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
					require_action( ctx->globalFailuresArray, exit, err = kNoMemoryErr );
				}
				failuresArray = ctx->globalFailuresArray;
			}
			
			err = _TUConvertToJUnitParseAssertion( value, &typePtr, &typeLen, &messagePtr, &messageLen, &detailPtr, &detailLen );
			require_noerr_quiet( err, exit );
			
			err = CFPropertyListAppendFormatted( NULL, failuresArray, 
				"{"
					"%kO=%.*s"	// type
					"%kO=%.*s"	// message
					"%kO=%.*s"	// detail
				"}", 
				kTUKey_Type,	(int) typeLen,		typePtr, 
				kTUKey_Message,	(int) messageLen,	messagePtr, 
				kTUKey_Detail,	(int) detailLen,	detailPtr );
			require_noerr( err, exit );
			break;
		
		// Duration ("d=").
		
		case 'd':
			require_action_quiet( !ctx->gotDuration, exit, err = kDuplicateErr );
			require_action_quiet( ctx->testDict, exit, err = kNotPreparedErr );
			
			n = sscanf( value, "%lfs", &d );
			require_action_quiet( n == 1, exit, err = kMalformedErr );
			require_action_quiet( d >= 0, exit, err = kRangeErr );
			err = CFDictionarySetDouble( ctx->testDict, kTUKey_Duration, d );
			require_noerr( err, exit );
			ctx->gotDuration = true;
			break;
		
		// Comment ("#=").
		
		case '#':
			break;
		
		// Unknown.
		
		default:
			break;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_TUConvertToJUnitParseAssertion
//===========================================================================================================================

static OSStatus
	_TUConvertToJUnitParseAssertion( 
		const char *	inValue, 
		const char **	outTypePtr, 
		size_t *		outTypeLen, 
		const char **	outMessagePtr, 
		size_t *		outMessageLen, 
		const char **	outDetailPtr, 
		size_t *		outDetailLen )
{
	OSStatus			err;
	const char *		typePtr;
	size_t				typeLen;
	const char *		detailPtr;
	size_t				detailLen;
	const char *		messagePtr;
	size_t				messageLen;
	
	// Parse lines such as the following (without outer quotes):
	//
	// "err:Test.c:123, MyFunction(), -6722 kTimeoutErr"
	// "bool:Test.c:456, MyFunction(), "result == 123""
	// "leaks:Test.c:456, MyFunction()"
	// "f=total:2947/2929"
	
	typePtr = inValue;
	while( ( *inValue != '\0' ) && ( *inValue != ':' ) ) ++inValue;
	typeLen = (size_t)( inValue - typePtr );
	require_action_quiet( *inValue == ':', exit, err = kMalformedErr );
	++inValue;
	
	if( strnicmpx( typePtr, typeLen, "leaks" ) == 0 )
	{
		messagePtr	= inValue;
		messageLen	= strlen( inValue );
		detailPtr	= NULL;
		detailLen	= 0;
	}
	else if( strnicmpx( typePtr, typeLen, "total" ) == 0 )
	{
		messagePtr	= inValue;
		messageLen	= strlen( inValue );
		detailPtr	= NULL;
		detailLen	= 0;
	}
	else
	{
		detailPtr = inValue;
		while( ( *inValue != '\0' ) && ( *inValue != ',' ) ) ++inValue;
		require_action_quiet( *inValue == ',', exit, err = kMalformedErr );
		++inValue;
		while( ( *inValue != '\0' ) && ( *inValue != ',' ) ) ++inValue;
		detailLen = (size_t)( inValue - detailPtr );
		require_action_quiet( *inValue == ',', exit, err = kMalformedErr );
		++inValue;
		
		while( ( *inValue != '\0' ) && isspace_safe( *inValue ) ) ++inValue;
		messagePtr = inValue;
		messageLen = strlen( inValue );
	}
	
	*outTypePtr		= typePtr;
	*outTypeLen		= typeLen;
	*outMessagePtr	= messagePtr;
	*outMessageLen	= messageLen;
	*outDetailPtr	= detailPtr;
	*outDetailLen	= detailLen;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_TUConvertToJUnitWriteJUnitXML
//===========================================================================================================================

static OSStatus	_TUConvertToJUnitWriteJUnitXML( TUConvertToJUnitContext *ctx, const char *inJUnitPath )
{
	OSStatus			err;
	FILE *				file = NULL;
	FILE *				filePtr;
	CFIndex				testIndex, testCount, failureIndex, failureCount;
	CFDictionaryRef		testDict, failureDict;
	uint32_t			totalTests, totalFailures, tests, passes;
	double				totalDuration, duration;
	CFStringRef			name, type;
	CFArrayRef			failureArray;
	char *				cptr;
	char *				message = NULL;
	char *				detail = NULL;
	Boolean				passed;
	
	if( strcmp( inJUnitPath, "-" ) == 0 )
	{
		filePtr = stdout;
	}
	else
	{
		file = fopen( inJUnitPath, "w" );
		err = map_global_value_errno( file, file );
		require_noerr_quiet( err, exit );
		filePtr = file;
	}
	
	FPrintF( filePtr, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n" );
	FPrintF( filePtr, "<testsuites>\n" );
	
	// Count the totals first since it needs to be written in the XML start element.
	
	totalTests		= 0;
	totalFailures	= 0;
	totalDuration	= 0;
	testCount = ctx->testsArray ? CFArrayGetCount( ctx->testsArray ) : 0;
	for( testIndex = 0; testIndex < testCount; ++testIndex )
	{
		testDict = CFArrayGetCFDictionaryAtIndex( ctx->testsArray, testIndex, &err );
		require_noerr( err, exit );
		
		tests = CFDictionaryGetUInt32( testDict, kTUKey_Total, &err );
		require_noerr( err, exit );
		totalTests += ( tests > 0 );
		
		passes = CFDictionaryGetUInt32( testDict, kTUKey_Passes, &err );
		require_noerr( err, exit );
		totalFailures += ( tests != passes );
		
		duration = CFDictionaryGetDouble( testDict, kTUKey_Duration, &err );
		require_noerr( err, exit );
		totalDuration += duration;
	}
	totalFailures += (uint32_t)( ctx->globalFailuresArray ? CFArrayGetCount( ctx->globalFailuresArray ) : 0 );
	
	FPrintF( filePtr, "	<testsuite name=\"AllTests\" tests=\"%u\" failures=\"%u\" time=\"%f\">\n", 
		totalTests, totalFailures, totalDuration );
	
	// Write each test as a JUnit testcase.
	
	for( testIndex = 0; testIndex < testCount; ++testIndex )
	{
		testDict = CFArrayGetCFDictionaryAtIndex( ctx->testsArray, testIndex, &err );
		require_noerr( err, exit );
		
		name = CFDictionaryGetCFString( testDict, kTUKey_Name, &err );
		require_noerr( err, exit );
		
		tests = CFDictionaryGetUInt32( testDict, kTUKey_Total, &err );
		require_noerr( err, exit );
		
		passes = CFDictionaryGetUInt32( testDict, kTUKey_Passes, &err );
		require_noerr( err, exit );
		
		duration = CFDictionaryGetDouble( testDict, kTUKey_Duration, &err );
		require_noerr( err, exit );
		
		passed = ( tests == passes ) ? true : false;
		FPrintF( filePtr, "		<testcase name=\"%@\" classname=\"%@\" time=\"%f\"%s>\n", 
			name, name, duration, passed ? "/" : "" );
		
		// Write each failure as its own element.
		
		failureArray = CFDictionaryGetCFArray( testDict, kTUKey_Failures, NULL );
		require_action( ( failureArray != NULL ) == !passed, exit, err = kInternalErr );
		failureCount = failureArray ? CFArrayGetCount( failureArray ) : 0;
		for( failureIndex = 0; failureIndex < failureCount; ++failureIndex )
		{
			failureDict = CFArrayGetCFDictionaryAtIndex( failureArray, failureIndex, &err );
			require_noerr( err, exit );
			
			type = CFDictionaryGetCFString( failureDict, kTUKey_Type, &err );
			require_noerr( err, exit );
			
			cptr = CFDictionaryCopyCString( failureDict, kTUKey_Message, &err );
			require_noerr( err, exit );
			err = XMLEscapeCopy( cptr, kSizeCString, &message, NULL );
			free( cptr );
			require_noerr( err, exit );
			
			cptr = CFDictionaryCopyCString( failureDict, kTUKey_Detail, &err );
			require_noerr( err, exit );
			err = XMLEscapeCopy( cptr, kSizeCString, &detail, NULL );
			free( cptr );
			require_noerr( err, exit );
			
			FPrintF( filePtr, "			<failure type=\"%@\" message=\"%s\">%s</failure>\n", type, message, detail );
			ForgetMem( &message );
			ForgetMem( &detail );
		}
		
		if( tests != passes ) FPrintF( filePtr, "		</testcase>\n" );
	}
	
	// Write each global failure as a failed test case.
	
	testCount = ctx->globalFailuresArray ? CFArrayGetCount( ctx->globalFailuresArray ) : 0;
	for( testIndex = 0; testIndex < testCount; ++testIndex )
	{
		failureDict = CFArrayGetCFDictionaryAtIndex( ctx->globalFailuresArray, testIndex, &err );
		require_noerr( err, exit );
		
		type = CFDictionaryGetCFString( failureDict, kTUKey_Type, &err );
		require_noerr( err, exit );
		
		cptr = CFDictionaryCopyCString( failureDict, kTUKey_Message, &err );
		require_noerr( err, exit );
		err = XMLEscapeCopy( cptr, kSizeCString, &message, NULL );
		free( cptr );
		require_noerr( err, exit );
		
		FPrintF( filePtr, "		<testcase name=\"global.%@\" classname=\"global.%@\">\n", type, type );
		FPrintF( filePtr, "			<failure type=\"%@\" message=\"%s\"/>\n", type, message, detail );
		FPrintF( filePtr, "		</testcase>\n" );
		ForgetMem( &message );
	}
	
	FPrintF( filePtr, "	</testsuite>\n" );
	FPrintF( filePtr, "</testsuites>\n" );
	err = kNoErr;
	
exit:
	FreeNullSafe( message );
	FreeNullSafe( detail );
	ForgetANSIFile( &file );
	return( err );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	TestUtilsTest
//===========================================================================================================================

static void	TestUtilsMacrosTest( TUTestContext *inTestCtx );
static void	TestUtilsFaultsTest( TUTestContext *inTestCtx );
static void	TestUtilsJUnitTest( TUTestContext *inTestCtx );
#if( COMPILER_OBJC )
	static void	TestUtilsObjectiveCExceptionsTest( TUTestContext *inTestCtx );
#endif

void	TestUtilsTest( void )
{
	gTUStopOnFirstFail = false;
	
	TUPerformTest( TestUtilsMacrosTest );
	TUPerformTest( TestUtilsFaultsTest );
	TUPerformTest( TestUtilsJUnitTest );
#if( COMPILER_OBJC )
	TUPerformTest( TestUtilsObjectiveCExceptionsTest );
#endif
}

//===========================================================================================================================
//	TestUtilsMacrosTest
//===========================================================================================================================

static void	TestUtilsMacrosTest( TUTestContext *inTestCtx )
{
	Boolean		breakOnFail;
	int			x;
	
	breakOnFail = gTUBreakOnFail;
	gTUBreakOnFail = false;
		
	tu_require_noerr( kNoErr, exit );
	tu_require_noerr( kNoMemoryErr, exit );
	tu_require( true, exit );
	tu_require( false, exit );
	
	x = 0;
	tu_require_action( 1, exit, x = 1 );
	tu_require( x == 0, exit );
	tu_require_action( 0, exit, x = 1 );
	tu_require( x == 0, exit );
	
	tu_require_action( 1, exit, x = 1 );
	tu_require( x == 0, exit );
	tu_require_action( 0, exit1, x = 1 );
	x = 2;
exit1:
	tu_require( x == 1, exit );
	
	x = 0;
	tu_require_action( true, exit, x = 1 );
	tu_require( x == 0, exit );
	tu_require_action( false, exit, x = 1 );
	tu_require( x == 0, exit );
	
	tu_require_action( true, exit, x = 1 );
	tu_require( x == 0, exit );
	tu_require_action( false, exit2, x = 1 );
	x = 2;
exit2:
	tu_require( x == 1, exit );
	
	TULogF( inTestCtx, kLogLevelTrace, "", "SHOULD NOT SEE: Test 1\n" );
	TULogF( inTestCtx, kLogLevelMax, "", "SHOULD SEE: Test 2\n" );
	TULogF( inTestCtx, kLogLevelMax, "Prefix: ", "SHOULD SEE: Test 3\nSHOULD SEE: Test 4\nSHOULD SEE: Test 5\n" );
	TULogF( inTestCtx, kLogLevelMax, "", "SHOULD SEE: Test %d\nSHOULD SEE: Test %d\nSHOULD SEE: Test %d\n", 123, 456, 789 );
	
exit:
	gTUBreakOnFail = breakOnFail;
}

//===========================================================================================================================
//	TestUtilsFaultsTest
//===========================================================================================================================

static void	TestUtilsFaultsTest( TUTestContext *inTestCtx )
{
	OSStatus		err;
	Value64			v64;
	
	v64.s64 = 123;
	err = TUFaultEnable( "test.fault", v64 );
	tu_require_noerr( err, exit );
	
	v64.s64 = 10;
	err = TUFaultInject( "test.fault", kTUFaultDataType_SInt64, &v64.s64 );
	tu_require_noerr( err, exit );
	tu_require( v64.s64 == 123, exit );
	
	v64.s64 = 10;
	tu_fault_inject_sint64( "test.fault", &v64.s64 );
	tu_require( v64.s64 == 123, exit );
	
	err = TUFaultDisable( "test.fault" );
	tu_require_noerr( err, exit );
	
	v64.s64 = 10;
	err = TUFaultInject( "test.fault", kTUFaultDataType_SInt64, &v64.s64 );
	tu_require( err != kNoErr, exit );
	tu_require( v64.s64 == 10, exit );
	
	v64.s64 = 10;
	tu_fault_inject_sint64( "test.fault", &v64.s64 );
	tu_require( v64.s64 == 10, exit );
	
exit:
	return;
}

//===========================================================================================================================
//	TestUtilsJUnitTest
//===========================================================================================================================

static void	TestUtilsJUnitTest( TUTestContext *inTestCtx )
{
	OSStatus		err;
	FILE *			file;
	
	// Write a test file to convert.
	
	file = fopen( "/tmp/TestUtils.tur", "w" );
	err = map_global_value_errno( file, file );
	tu_require_noerr( err, exit );
	FPrintF( file, 
		"T=\n"
		"t=TLV8Test\n"
		"f=bool:TLVUtils.c:667, TestTLV8(), \"err == kNotFoundErr\"\n"
		"p=bool:TLVUtils.c:697, TestTLV8(), \"type == 0x11\"\n"
		"f=err:TLVUtils.c:709, TestTLV8(), -6722 kTimeoutErr\n"
		"p=err:TLVUtils.c:716, TestTLV8(), 0 kNoErr\n"
		"r=2/4\n"
		"d=1.532s\n"
		"z=\n"
		"t=StringUtilsTest\n"
		"r=32/32\n"
		"d=5.231s\n"
		"#=Checking \"a\" vs \"b\".\n"
		"#=Checking \"c\" vs \"c\".\n"
		"z=\n"
		"f=leaks:TestUtils.c:247, TUFinalize()\n"
		"Z=\n" );
	fclose( file );
	file = NULL;
	
	err = TUConvertToJUnit( "/tmp/TestUtils.tur", "/tmp/TestUtils.xml" );
	tu_require_noerr( err, exit );
	
exit:
	ForgetANSIFile( &file );
	remove( "/tmp/TestUtils.tur" );
}

#if( COMPILER_OBJC )
//===========================================================================================================================
//	TestUtilsObjectiveCExceptionsTest
//===========================================================================================================================

static void	TestUtilsObjectiveCExceptionsTest( TUTestContext *inTestCtx )
{
	tu_require( true, exit );
	@throw [NSException exceptionWithName:@"FakeException" reason:@"Just a test" userInfo:@{ @"key" : @"value" }];
	tu_require( 1, exit );
	
exit:
	return;
}
#endif // COMPILER_OBJC

#endif // !EXCLUDE_UNIT_TESTS
