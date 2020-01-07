/*
	File:    	CommandLineUtils.c
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
	
	Copyright (C) 2008-2015 Apple Inc. All Rights Reserved.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "CommandLineUtils.h"

#include <ctype.h>
#include <stdio.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "MiscUtils.h"
#include "PrintFUtils.h"
#include "StringUtils.h"

#if( CF_ENABLED )
	#include CF_HEADER
#endif

#if( CLI_HAS_INTERACTIVE )
	#include <readline/history.h>
	#include <readline/readline.h>
	
	#include "SoftLinking.h"
	
	#include LIBDISPATCH_HEADER
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define kCLIIndentWidth		4

typedef uint32_t		CLIFindFlags;
#define kCLIFindFlags_None					0
#define kCLIFindFlag_NonGlobal				( 1U << 0 ) // Find only options that aren't marked as global only.

typedef uint32_t		CLIPrintOptionsFlags;
#define kCLIPrintOptionsFlags_None			0
#define kCLIPrintOptionsFlag_LongHelp		( 1U << 0 )
#define kCLIPrintOptionsFlag_NonGlobal		( 1U << 1 )

typedef uint32_t		CLIValueFlags;
#define kCLIValueFlags_None					0
#define kCLIValueFlags_Short				( 1U << 0 )
#define kCLIValueFlags_Unset				( 1U << 1 )

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static OSStatus		_CLIParseShortOption( CLIOption inOptions[] );
static OSStatus		_CLIParseLongOption( CLIOption inOptions[], const char *inArg );
static OSStatus		_CLIGetValue( CLIOption *inOption, CLIValueFlags inFlags );
static OSStatus		_CLIGetArg( const CLIOption *inOption, CLIValueFlags inFlags, const char **outArg );
static void			_CLIOptionError( const CLIOption *inOption, const char *inReason, CLIValueFlags inFlags );
static OSStatus		_CLIReorderArg( const char *inArg );
static CLIOption *	_CLIFindOption( CLIOption *inOptions, CLIOptionType inType, CLIFindFlags inFlags, const char *inName );
static OSStatus		_CLICheckMissingOptions( CLIOption *inOption );
static OSStatus		_CLIPrepareForMetaCommand( CLIOption *inOptions );

static void			_CLIHelp_PrintSummary( Boolean inFull );
static void			_CLIHelp_PrintCommand( CLIOption *inOption, Boolean inProcessSubCommands );
static void			_CLIHelp_PrintUsageLine( CLIOption *inOption, const char *inPrefix, int inIndent );
static int			_CLIHelp_PrintOptions( const CLIOption *inOptions, int inIndent, const char *inLabel, CLIPrintOptionsFlags inFlags );
static int			_CLIHelp_PrintOptionName( const CLIOption *inOption, FILE *inFile );

#if( CLI_HAS_INTERACTIVE )
	static char **	_CLIInteractiveCompleter( const char *inText, int inStart, int inEnd );
	static char *	_CLIInteractiveGenerator( const char *inText, int inState );
	static void		_CLIInteractiveHandleInput( void *inArg );
	static void		_CLIInteractiveHandleLine( char *inLine );
	static void		_CLIInteractiveHandleSigInt( void *inArg );
	static OSStatus	_CLIInteractiveReset( void );
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

int							gArgI				= 0;
int							gArgC				= 0;
const char **				gArgV				= NULL;
const char **				gArgVAlt			= NULL;
static int					gMutableArgC		= 0;
static const char **		gMutableArgV		= NULL;

const char *				gProgramPath		= "???";
const char *				gProgramName		= "???";
const char *				gProgramLongName	= NULL;
volatile int				gExitCode			= 0;

static CLIOption *			gRootOptions		= NULL;
static const char *			gOptionPtr			= NULL;
static Boolean				gEndedOptions		= false;
CLIOption *					gCLICurrentCommand	= NULL;
CLIOption *					gCLICurrentOption	= NULL;
static int					gCLIProgressMax		= 0;

#if( CLI_HAS_INTERACTIVE )
	Boolean							gCLIInteractiveMode			= false;
	static const CLIOption *		gCLIInteractiveNextCommand	= NULL;
	static dispatch_source_t		gCLIInteractiveInputSource	= NULL;
	static dispatch_source_t		gCLIInteractiveSigIntSource	= NULL;
	static int						gCLIInteractiveSigIntCount	= 0;
#endif

//===========================================================================================================================
//	CLIInit
//===========================================================================================================================

void	CLIInit( int inArgC, const void *inArgV )
{
	const char *		s;
	
	gArgI = 0;
	gArgC = inArgC;
	gArgV = (const char **) inArgV;
	gProgramPath = ( inArgC > 0 ) ? gArgV[ gArgI++ ] : "?";
	s = strrchr( gProgramPath, '/' );
#if( TARGET_OS_WINDOWS )
	if( !s ) s = strrchr( gProgramPath, '\\' );
#endif
	gProgramName = s ? ( s + 1 ) : gProgramPath;
}

//===========================================================================================================================
//	CLIFree
//===========================================================================================================================

void	CLIFree( void )
{
	gArgC = 0;
	if( gArgVAlt )
	{
		free( (void *) gArgVAlt );
		gArgVAlt = NULL;
	}
	gMutableArgC = 0;
	if( gMutableArgV )
	{
		free( (void *) gMutableArgV );
		gMutableArgV = NULL;
	}
}

//===========================================================================================================================
//	CLIParse
//===========================================================================================================================

OSStatus	CLIParse( CLIOption inOptions[], CLIFlags inFlags )
{
	OSStatus			err;
	const char *		arg;
	CLIOption *			option;
	int					top, leaf;
	
	top = !gRootOptions;
	if( top )
	{
		_CLIPrepareForMetaCommand( inOptions );
		gRootOptions = inOptions;
	}
	gOptionPtr = NULL;
	
	// Parse options.
	
	for( ; !gEndedOptions && ( gArgI < gArgC ); ++gArgI )
	{
		arg = gArgV[ gArgI ];
		if( ( arg[ 0 ] != '-' ) || ( arg[ 1 ] == '\0' ) || isdigit_safe( arg[ 1 ] ) ) // Non-option.
		{
			if( !( inFlags & kCLIFlags_ReorderArgs ) ) break;
			err = _CLIReorderArg( arg );
			require_noerr( err, exit );
			continue;
		}
		if( arg[ 1 ] != '-' ) // Short options.
		{
			for( gOptionPtr = arg + 1; gOptionPtr; )
			{
				err = _CLIParseShortOption( inOptions );
				if( err ) goto exit;
			}
		}
		else if( arg[ 2 ] != '\0' ) // Long option.
		{
			err = _CLIParseLongOption( inOptions, arg + 2 );
			if( err ) goto exit;
		}
		else // "--" (end of options).
		{
			++gArgI;
			gEndedOptions = true;
			break;
		}
	}
	_CLIReorderArg( NULL ); // Reorder the rest of the args if needed.
	
	// Print usage if there is no command and there's a possible command in the list.
	
	if( inFlags & kCLIFlags_DontProcessCommands )
	{
		err = kNoErr;
		goto exit;
	}
	if( gArgI >= gArgC )
	{
		if( !_CLIFindOption( inOptions, kCLIOptionType_Command, kCLIFindFlags_None, NULL ) )
		{
			err = kNoErr;
			goto exit;
		}
		else if( inOptions->parentOption )
		{
			_CLIHelp_PrintCommand( inOptions->parentOption, false );
			err = kCLIArgErr;
			goto exit;
		}
		
		_CLIHelp_PrintSummary( false );
		err = kCLIArgErr;
		goto exit;
	}
	
	// Process commands.
	
	arg = gArgV[ gArgI ];
	option = _CLIFindOption( inOptions, kCLIOptionType_Command, kCLIFindFlags_None, arg );
	if( option )
	{
		gCLICurrentCommand = option;
		option->parentOption = inOptions->parentOption; // Save parent for walking back up to the root.
		++gArgI;
		if( option->subOptions )
		{
			leaf = !_CLIFindOption( option->subOptions, kCLIOptionType_Command, kCLIFindFlags_None, NULL );
			option->subOptions->parentOption = option; // Save parent for walking back up to the root.
			err = CLIParse( option->subOptions, leaf ? ( inFlags | kCLIFlags_ReorderArgs ) : inFlags );
			if( err ) goto exit;
		}
		else
		{
			leaf = true;
		}
		if( leaf )
		{
			// Parse and reorder any remaining args to handle options after the last command.
			
			err = CLIParse( inOptions, inFlags | kCLIFlags_ReorderArgs | kCLIFlags_DontProcessCommands );
			if( err ) goto exit;
		}
		
		err = _CLICheckMissingOptions( option );
		if( err ) goto exit;
		
		gCLICurrentOption = option;
		if( option->commandCallBack ) option->commandCallBack();
		err = kNoErr;
		goto exit;
	}
	else if( !_CLIFindOption( inOptions, kCLIOptionType_Command, kCLIFindFlags_None, NULL ) )
	{
		err = kNoErr;
		goto exit;
	}
	
	if( inOptions->parentOption )
	{
		fprintf( stderr, "error: unknown %s command '%s'. See '%s help %s' for a list of commands.\n", 
			inOptions->parentOption->longName, arg, gProgramName, inOptions->parentOption->longName );
	}
	else
	{
		fprintf( stderr, "error: unknown command '%s'. See '%s help' for a list of commands.\n", arg, gProgramName );
	}
	err = kCLIArgErr;
	goto exit;
	
exit:
	// If we're exiting from the top level and there's no error then warn about missing args. This is done last to give 
	// each level a chance to consume arguments. This also converts kEndingErr to kNoErr only at the top level so 
	// sub-levels don't process commands while unwinding, but still returns success to the caller.
	
	if( top )
	{
		if( !err ) for( ; gArgI < gArgC; ++gArgI ) fprintf( stderr, "warning: unused argument '%s'.\n", gArgV[ gArgI ] );
		if( err == kEndingErr ) err = kNoErr;
	}
	return( err );
}

//===========================================================================================================================
//	_CLIParseShortOption
//===========================================================================================================================

static OSStatus	_CLIParseShortOption( CLIOption inOptions[] )
{
	OSStatus		err;
	CLIOption *		option;
	Boolean			top = false;
	
	for( ;; )
	{
		for( option = inOptions; option->type != kCLIOptionType_End; ++option )
		{
			if( CLIOption_IsOption( option ) && ( option->shortName == gOptionPtr[ 0 ] ) )
			{
				gOptionPtr = ( gOptionPtr[ 1 ] != '\0' ) ? ( gOptionPtr + 1 ) : NULL;
				err = _CLIGetValue( option, kCLIValueFlags_Short );
				goto exit;
			}
		}
		
		if( top ) break;
		inOptions = inOptions->parentOption;
		if( !inOptions )
		{
			inOptions = gRootOptions;
			top = true;
		}
	}
	
	fprintf( stderr, "error: unknown option '%c'.\n", *gOptionPtr );
	err = kCLIArgErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CLIParseLongOption
//===========================================================================================================================

static OSStatus	_CLIParseLongOption( CLIOption inOptions[], const char *inArg )
{
	OSStatus			err;
	const char *		namePtr;
	const char *		nameEnd;
	size_t				nameLen;
	CLIValueFlags		flags;
	CLIOption *			option;
	Boolean				top = false;
	
	namePtr = inArg;
	nameEnd = strchr( namePtr, '=' );
	if( !nameEnd ) nameEnd = namePtr + strlen( namePtr );
	nameLen = (size_t)( nameEnd - namePtr );
	
	flags = kCLIValueFlags_None;
	if( ( nameLen >= 3 ) && ( strnicmp( namePtr, "no-", 3 ) == 0 ) )
	{
		flags |= kCLIValueFlags_Unset;
		namePtr += 3;
		nameLen -= 3;
	}
	
	for( ;; )
	{
		for( option = inOptions; option->type != kCLIOptionType_End; ++option )
		{
			if( !CLIOption_IsOption( option ) )							continue;
			if( !option->longName )										continue;
			if( strnicmp( option->longName, namePtr, nameLen ) != 0 )	continue;
			if( option->longName[ nameLen ] != '\0' )					continue;
		
			if( *nameEnd != '\0' ) gOptionPtr = nameEnd + 1;
			err = _CLIGetValue( option, flags );
			goto exit;
		}
		
		if( top ) break;
		inOptions = inOptions->parentOption;
		if( !inOptions )
		{
			inOptions = gRootOptions;
			top = true;
		}
	}
	
	fprintf( stderr, "error: unknown option '%s'.\n", inArg );
	err = kCLIArgErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CLIGetValue
//===========================================================================================================================

static OSStatus	_CLIGetValue( CLIOption *inOption, CLIValueFlags inFlags )
{
	int const			unset		= ( inFlags & kCLIValueFlags_Unset ) != 0;
	int const			optional	= ( inOption->flags & kCLIOptionFlags_OptionalArgument ) != 0;
	int const			noArg		= ( inOption->flags & kCLIOptionFlags_NoArgument ) != 0;
	OSStatus			err;
	char *				end;
	const char *		arg;
	
	if( unset && ( inOption->flags & kCLIOptionFlags_NoNegate ) )
	{
		_CLIOptionError( inOption, "can't be negated", inFlags & ~kCLIValueFlags_Unset );
		err = kCLIArgErr;
		goto exit;
	}
	if( ( unset && gOptionPtr ) || ( !( inFlags & kCLIValueFlags_Short ) && gOptionPtr && noArg ) )
	{
		_CLIOptionError( inOption, "doesn't take a value", inFlags );
		err = kCLIArgErr;
		goto exit;
	}
	switch( inOption->type )
	{
		case kCLIOptionType_Boolean:
			*( (int *)( inOption->valuePtr ) ) = unset ? 0 : *( (int *)( inOption->valuePtr ) ) + 1;
			err = 0;
			break;
		
		case kCLIOptionType_String:
			if( unset )
			{
				*( (const char ** )( inOption->valuePtr ) ) = NULL;
				err = 0;
			}
			else if( optional && !gOptionPtr )
			{
				*( (const char ** )( inOption->valuePtr ) ) = (const char *)( inOption->defaultValue );
				err = 0;
			}
			else
			{
				err = _CLIGetArg( inOption, inFlags, (const char **)( inOption->valuePtr ) );
			}
			break;
		
		#if( CF_ENABLED )
		case kCLIOptionType_CFString:
		{
			CFStringRef *		cfValuePtr = (CFStringRef *) inOption->valuePtr;
			CFStringRef			cfstr;
			
			if( unset )
			{
				ForgetCF( cfValuePtr );
				err = 0;
			}
			else if( optional && !gOptionPtr )
			{
				ReplaceCF( cfValuePtr, (CFStringRef) inOption->defaultValue );
				err = 0;
			}
			else
			{
				err = _CLIGetArg( inOption, inFlags, &arg );
				require_noerr_quiet( err, exit );
				
				cfstr = CFStringCreateWithCString( NULL, arg, kCFStringEncodingUTF8 );
				require_action( cfstr, exit, err = kCLINoMemoryErr );
				CFReleaseNullSafe( *cfValuePtr );
				*cfValuePtr = cfstr;
			}
			break;
		}
		#endif
		
		case kCLIOptionType_MultiString:
			if( unset )
			{
				StringArray_Free( *( (char ***) inOption->valuePtr ), *inOption->valueCountPtr );
				*inOption->valueCountPtr = 0;
				err = 0;
			}
			else if( optional && !gOptionPtr )
			{
				err = StringArray_Append( (char ***) inOption->valuePtr, inOption->valueCountPtr, 
					(const char *)( inOption->defaultValue ) );
				require_noerr_action( err, exit, err = kCLINoMemoryErr );
			}
			else
			{
				err = _CLIGetArg( inOption, inFlags, &arg );
				require_noerr_quiet( err, exit );
				
				err = StringArray_Append( (char ***) inOption->valuePtr, inOption->valueCountPtr, arg );
				require_noerr_action( err, exit, err = kCLINoMemoryErr );
			}
			break;
		
		case kCLIOptionType_CallBack:
			gCLICurrentOption = inOption;
			if( unset )							err = inOption->optionCallBack( inOption, NULL, 1 );
			else if( noArg )					err = inOption->optionCallBack( inOption, NULL, 0 );
			else if( optional && !gOptionPtr )	err = inOption->optionCallBack( inOption, NULL, 0 );
			else
			{
				err = _CLIGetArg( inOption, inFlags, &arg );
				if( !err ) err = inOption->optionCallBack( inOption, arg, 0 );
			}
			break;
		
		case kCLIOptionType_Integer:
			if( unset )
			{
				*( (int *)( inOption->valuePtr ) ) = 0;
				err = 0;
			}
			else if( optional && !gOptionPtr )
			{
				*( (int *)( inOption->valuePtr ) ) = (int) inOption->defaultValue;
				err = 0;
			}
			else
			{
				err = _CLIGetArg( inOption, inFlags, &arg );
				if( err ) break;
				
				*( (int *)( inOption->valuePtr ) ) = (int) strtol( arg, &end, 0 );
				if( *end == '\0' ) break;
				
				_CLIOptionError( inOption, "expects a numeric value", inFlags );
				err = kCLIArgErr;
			}
			break;
		
		default:
			fprintf( stderr, "BUG: unknown option type %d\n", inOption->type );
			err = kCLIInternalErr;
			break;
	}
	if( !err ) inOption->flags |= kCLIOptionFlags_Specified;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CLIGetArg
//===========================================================================================================================

static OSStatus	_CLIGetArg( const CLIOption *inOption, CLIValueFlags inFlags, const char **outArg )
{
	if( gOptionPtr )
	{
		*outArg = gOptionPtr;
		gOptionPtr = NULL;
	}
	else if( ( gArgI + 1 ) < gArgC )
	{
		*outArg = gArgV[ ++gArgI ];
	}
	else
	{
		_CLIOptionError( inOption, "requires a value", inFlags );
		return( kCLIArgErr );
	}
	return( kNoErr );
}

//===========================================================================================================================
//	_CLIOptionError
//===========================================================================================================================

static void	_CLIOptionError( const CLIOption *inOption, const char *inReason, CLIValueFlags inFlags )
{
	fprintf( stderr, "error: option " );
	if(      inFlags & kCLIValueFlags_Short )	fprintf( stderr, "'%c' ",    inOption->shortName );
	else if( inFlags & kCLIValueFlags_Unset )	fprintf( stderr, "'no-%s' ", inOption->longName );
	else										fprintf( stderr, "'%s' ",    inOption->longName );
	fprintf( stderr, "%s.\n", inReason );
}

//===========================================================================================================================
//	_CLIReorderArg
//===========================================================================================================================

static OSStatus	_CLIReorderArg( const char *inArg )
{
	OSStatus		err;
	
	if( inArg )
	{
		if( gMutableArgV == NULL )
		{
			gMutableArgV = (const char **) malloc( ( (size_t)( gArgC + 1 ) ) * sizeof( *gMutableArgV ) );
			require_action( gMutableArgV, exit, err = kCLINoMemoryErr );
		}
		gMutableArgV[ gMutableArgC++ ] = inArg;
	}
	else if( gMutableArgC > 0 ) // NULL input means to reorder the rest of the args if needed.
	{
		for( ; gArgI < gArgC; ++gArgI )
		{
			gMutableArgV[ gMutableArgC++ ] = gArgV[ gArgI ];
		}
		gMutableArgV[ gMutableArgC ] = NULL;
		
		gArgI = 0;
		gArgC = gMutableArgC;
		gArgV = gMutableArgV;
		gMutableArgC = 0;
	}
	err = kNoErr; 
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CLIFindOption
//===========================================================================================================================

static CLIOption *	_CLIFindOption( CLIOption *inOptions, CLIOptionType inType, CLIFindFlags inFlags, const char *inName )
{
	CLIOption *		option;
	
	if( inOptions )
	{
		for( option = inOptions; option->type != kCLIOptionType_End; ++option )
		{
			if( ( ( inType == kCLIOptionType_Any ) || ( inType == option->type ) ||
				  ( ( inType == kCLIOptionType_AnyOption ) && CLIOption_IsOption( option ) ) ) &&
				( !( inFlags & kCLIFindFlag_NonGlobal ) || !( option->flags & kCLIOptionFlags_GlobalOnly ) ) &&
				( ( inName == NULL ) || ( stricmp( option->longName, inName ) == 0 ) ) )
			{
				return( option );
			}
		}
	}
	return( NULL );
}

//===========================================================================================================================
//	_CLICheckMissingOptions
//===========================================================================================================================

static OSStatus	_CLICheckMissingOptions( CLIOption *inOption )
{
	CLIOption *		option;
	int				widest, width;
	int				missing;
	
	missing = 0;
	widest = 0;
	for( option = inOption->subOptions; option && ( option->type != kCLIOptionType_End ); ++option )
	{
		if( !( option->flags & kCLIOptionFlags_Required ) )	continue;
		if(    option->flags & kCLIOptionFlags_Specified )	continue;
		
		width = _CLIHelp_PrintOptionName( option, NULL );
		if( width > widest ) widest = width;
		
		missing = 1;
	}
	if( missing )
	{
		for( option = inOption->subOptions; option && ( option->type != kCLIOptionType_End ); ++option )
		{
			if( !( option->flags & kCLIOptionFlags_Required ) )	continue;
			if(    option->flags & kCLIOptionFlags_Specified )	continue;
			
			fprintf( stderr, "error: " );
			width = _CLIHelp_PrintOptionName( option, stderr );
			fprintf( stderr, "%*s not specified\n", widest - width, "" );
		}
	}
	return( missing ? kCLIArgErr : kNoErr );
}

//===========================================================================================================================
//	_CLIPrepareForMetaCommand
//===========================================================================================================================

static OSStatus	_CLIPrepareForMetaCommand( CLIOption *inOptions )
{
	OSStatus			err;
	CLIOption *			option;
	int					i, j;
	const char **		newArgV;
	
	for( option = inOptions; option->type != kCLIOptionType_End; ++option )
	{
		if( option->type != kCLIOptionType_Command )			continue;
		if( !( option->flags & kCLIOptionFlags_MetaCommand ) )	continue;
		if( stricmp( option->longName, gProgramName ) != 0 )	continue;
		
		// Rebuild argv with the first arg being the program name and backing up to point to it.
		// This is handle cases where there may be multiple links to the same binary, but each with a different name.
		// For example, you could symlink /usr/local/bin/test.01 to /usr/local/bin/test and then have a meta command
		// named "test.01". So when /usr/local/bin/test.01 is invoked, "test.01" will be inserted as argv[1]. This 
		// makes it look like the tool was invoked with "test.01" as the command and it's processed normally.
		
		newArgV = (const char **) malloc( ( (size_t)( gArgC + 2 ) ) * sizeof( *newArgV ) );
		require_action( newArgV, exit, err = kCLINoMemoryErr );
		
		i = 0;
		j = 0;
		if( gArgC > 0 ) newArgV[ j++ ] = gArgV[ i++ ];
		newArgV[ j++ ] = gProgramName;
		while( i < gArgC ) newArgV[ j++ ] = gArgV[ i++ ];
		newArgV[ j ] = NULL;
		
		gArgI = ( gArgC > 0 ) ? 1 : 0;
		gArgC = j;
		gArgV = newArgV;
		FreeNullSafe( (void *) gArgVAlt );
		gArgVAlt = newArgV;
		break;
	}
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif
	
//===========================================================================================================================
//	CLIHelpCommand
//===========================================================================================================================

void	CLIHelpCommand( const char *inCmd )
{
	CLIOption *		option;
	
	if( inCmd )
	{
		option = _CLIFindOption( gRootOptions, kCLIOptionType_Command, kCLIFindFlags_None, inCmd );
		if( option )	_CLIHelp_PrintCommand( option, true );
		else			fprintf( stderr, "error: unknown command '%s'.\n", inCmd );
	}
	else
	{
		_CLIHelp_PrintSummary( true );
	}
}

void	_CLIHelpCommand( void )
{
	CLIHelpCommand( ( gArgI < gArgC ) ? gArgV[ gArgI++ ] : NULL );
}

//===========================================================================================================================
//	_CLIHelpOption
//===========================================================================================================================

OSStatus	_CLIHelpOption( CLIOption *inOption, const char *inArg, int inUnset )
{
	CLIOption *		option;
	
	(void) inOption;
	(void) inUnset;
	
	if( !inArg && ( ( gArgI + 1 ) < gArgC ) )
	{
		inArg = gArgV[ ++gArgI ];
	}
	if( inArg )
	{
		option = _CLIFindOption( gRootOptions, kCLIOptionType_Command, kCLIFindFlags_None, inArg );
		if( option )	_CLIHelp_PrintCommand( option, false );
		else			fprintf( stderr, "error: unknown command '%s'.\n", inArg );
	}
	else if( gCLICurrentCommand && ( gCLICurrentCommand->commandCallBack != _CLIHelpCommand ) )
	{
		_CLIHelp_PrintCommand( gCLICurrentCommand, false );
	}
	else
	{
		_CLIHelp_PrintSummary( true );
	}
	return( kEndingErr );
}

//===========================================================================================================================
//	_CLIHelp_PrintSummary
//===========================================================================================================================

static void	_CLIHelp_PrintSummary( Boolean inFull )
{
	const CLIOption *		command;
	int						nShownOptions, nShownCommands, hidden;
	int						widest, width;
	
	_CLIHelp_PrintUsageLine( gRootOptions, NULL, 0 );
	fputc( '\n', stderr );
	nShownOptions = _CLIHelp_PrintOptions( gRootOptions, 1 * kCLIIndentWidth, "Global options:", kCLIPrintOptionsFlags_None );
	
	widest = 0;
	for( command = gRootOptions; command->type != kCLIOptionType_End; ++command )
	{
		if( command->type != kCLIOptionType_Command ) continue;
		if( !inFull && ( command->flags & kCLIOptionFlags_NotCommon ) ) continue;
		width = (int) strlen( command->longName );
		if( width > widest ) widest = width;
	}
	hidden = false;
	for( command = gRootOptions; command->type != kCLIOptionType_End; ++command )
	{
		if( command->type != kCLIOptionType_Command ) continue;
		if( !inFull && ( command->flags & kCLIOptionFlags_NotCommon ) ) { hidden = true; break; }
	}
	nShownCommands = 0;
	for( command = gRootOptions; command->type != kCLIOptionType_End; ++command )
	{
		if( command->type != kCLIOptionType_Command ) continue;
		if( !inFull && ( command->flags & kCLIOptionFlags_NotCommon ) ) continue;
		if( nShownCommands == 0 ) fprintf( stderr, "%s:\n", inFull || !hidden ? "Commands" : "Commonly used commands" );
		fprintf( stderr, "%*s%-*s    %s\n", 1 * kCLIIndentWidth, "", widest, command->longName, command->shortHelp );
		++nShownCommands;
	}
	if( nShownOptions || nShownCommands ) fputc( '\n', stderr );
	if( hidden )			fprintf( stderr, "See '%s help' for a full list of commands.\n", gProgramName );
	if( nShownCommands )	fprintf( stderr, "See '%s help <command>' for more info about a command.\n\n", gProgramName );
	else if( hidden )		fputc( '\n', stderr );
}

//===========================================================================================================================
//	_CLIHelp_PrintCommand
//===========================================================================================================================

static void	_CLIHelp_PrintCommand( CLIOption *inOption, Boolean inProcessSubCommands )
{
	CLIOption *			command = NULL;
	CLIOption *			option;
	const char *		arg;
	int					nCommands, width, widest;
	
	if( inProcessSubCommands && inOption->subOptions && ( gArgI < gArgC ) )
	{
		arg = gArgV[ gArgI++ ];
		command = _CLIFindOption( inOption->subOptions, kCLIOptionType_Command, kCLIFindFlags_None, arg );
		if( !command )
		{
			fprintf( stderr, "error: unknown %s sub-command '%s'.\n", inOption->longName, arg );
			goto exit;
		}
		command->parentOption = inOption;
	}
	if( !command ) command = inOption;
	fprintf( stderr, "\n%s\n", command->shortHelp );
	if( command->longHelp ) fprintf( stderr, "\n%s\n", command->longHelp );
	
	_CLIHelp_PrintUsageLine( command, NULL, 0 );
	fputc( '\n', stderr );
	
	if( _CLIFindOption( gRootOptions, kCLIOptionType_AnyOption, kCLIFindFlag_NonGlobal, NULL ) )
	{
		_CLIHelp_PrintOptions( gRootOptions, 1 * kCLIIndentWidth, "Global options:", 
			kCLIPrintOptionsFlag_LongHelp | kCLIPrintOptionsFlag_NonGlobal );
	}
	if( command->parentOption && command->parentOption->subOptions )
	{
		_CLIHelp_PrintOptions( command->parentOption->subOptions, 1 * kCLIIndentWidth, "Parent options:", 
			kCLIPrintOptionsFlag_LongHelp );
	}
	if( command->subOptions )
	{
		_CLIHelp_PrintOptions( command->subOptions, 1 * kCLIIndentWidth, "Options:", kCLIPrintOptionsFlag_LongHelp );
		
		widest = 0;
		for( option = command->subOptions; option->type != kCLIOptionType_End; ++option )
		{
			if( option->type != kCLIOptionType_Command ) continue;
			width = (int) strlen( option->longName );
			if( width > widest ) widest = width;
		}
		nCommands = 0;
		for( option = command->subOptions; option->type != kCLIOptionType_End; ++option )
		{
			if( option->type != kCLIOptionType_Command ) continue;
			if( nCommands == 0 ) fprintf( stderr, "Commands:\n" );
			fprintf( stderr, "%*s%-*s    %s\n", 1 * kCLIIndentWidth, "", widest, option->longName, option->shortHelp );
			++nCommands;
		}
		
		for( option = command->subOptions; 
			( option = _CLIFindOption( option, kCLIOptionType_Section, kCLIFindFlags_None, NULL ) ) != NULL; 
			++option )
		{
			fprintf( stderr, "%s\n", option->shortHelp );
			FPrintF( stderr, "%1{text}", option->longHelp, kSizeCString );
			fprintf( stderr, "\n" );
		}
		
		if( nCommands )
		{
			fprintf( stderr, "\nSee '%s help %s <command>' for more info about a command.\n\n", gProgramName, command->longName );
		}
	}
	
exit:
	return;
}

//===========================================================================================================================
//	_CLIHelp_PrintUsageLine
//===========================================================================================================================

static void	_CLIHelp_PrintUsageLine( CLIOption *inOption, const char *inPrefix, int inIndent )
{
	int		root;
	
	root = ( inOption == gRootOptions );
	fprintf( stderr, "%s%*s%s", inPrefix ? inPrefix : "\nUsage: ", inIndent, "", gProgramName );
	if( _CLIFindOption( gRootOptions, kCLIOptionType_AnyOption, root ? kCLIFindFlags_None : kCLIFindFlag_NonGlobal, NULL ) )
	{
		fprintf( stderr, " [global options]" );
	}
	if( root )
	{
		if( _CLIFindOption( gRootOptions, kCLIOptionType_Command, kCLIFindFlags_None, NULL ) )
		{
			fprintf( stderr, " <command> [options] [args]" );
		}
	}
	else
	{
		const CLIOption *		option;
		
		if( inOption->parentOption )
		{
			fprintf( stderr, " %s", inOption->parentOption->longName );
			if( inOption->parentOption->subOptions && 
				_CLIFindOption( inOption->parentOption->subOptions, kCLIOptionType_AnyOption, kCLIFindFlags_None, NULL ) )
			{
				fprintf( stderr, " [parent options]" );
			}
		}
		fprintf( stderr, " %s", inOption->longName );
		if( inOption->subOptions )
		{
			if( _CLIFindOption( inOption->subOptions, kCLIOptionType_Command, kCLIFindFlags_None, NULL ) )
			{
				fprintf( stderr, " [options] <command> [sub-options] [args]" );
			}
			else
			{
				if( _CLIFindOption( inOption->subOptions, kCLIOptionType_AnyOption, kCLIFindFlags_None, NULL ) )
				{
					fprintf( stderr, " [options]" );
				}
				for( option = inOption->subOptions; option->type != kCLIOptionType_End; ++option )
				{
					char		startChar, endChar;
					
					if( option->type != kCLIOptionType_Argument ) continue;
					startChar = ( option->flags & kCLIOptionFlags_OptionalArgument ) ? '[' : '<';
					endChar   = ( option->flags & kCLIOptionFlags_OptionalArgument ) ? ']' : '>';
					fprintf( stderr, " %c%s%c", startChar, option->longName, endChar );
				}
			}
		}
	}
	fputc( '\n', stderr );
}

//===========================================================================================================================
//	_CLIHelp_PrintOptions
//===========================================================================================================================

static int	_CLIHelp_PrintOptions( const CLIOption *inOptions, int inIndent, const char *inLabel, CLIPrintOptionsFlags inFlags )
{
	const CLIOption *		option;
	int						labelShown;
	int						nShownOptions;
	int						widest, width;
	
	labelShown = 0;
	widest = 0;
	for( option = inOptions; option->type != kCLIOptionType_End; ++option )
	{
		if( !CLIOption_IsOption( option ) && ( option->type != kCLIOptionType_Argument ) ) continue;
		if( ( inFlags & kCLIPrintOptionsFlag_NonGlobal ) && ( option->flags & kCLIOptionFlags_GlobalOnly ) ) continue;
		if( !labelShown && inLabel )
		{
			fprintf( stderr, "%s", inLabel );
			labelShown = 1;
		}
		width = _CLIHelp_PrintOptionName( option, NULL );
		if( width > widest ) widest = width;
	}
	nShownOptions = 0;
	for( option = inOptions; option->type != kCLIOptionType_End; ++option )
	{
		if( ( inFlags & kCLIPrintOptionsFlag_NonGlobal ) && ( option->flags & kCLIOptionFlags_GlobalOnly ) ) continue;
		
		if( option->type == kCLIOptionType_Group )
		{
			fputc( '\n', stderr );
			if( *option->shortHelp ) fprintf( stderr, "%*s%s\n", inIndent, "", option->shortHelp );
			++nShownOptions;
			continue;
		}
		if( !CLIOption_IsOption( option ) && ( option->type != kCLIOptionType_Argument ) ) continue;
		
		if( nShownOptions == 0 ) fputc( '\n', stderr );
		fprintf( stderr, "%*s", inIndent, "" );
		width = _CLIHelp_PrintOptionName( option, stderr );
		fprintf( stderr, "%*s    %s\n", widest - width, "", option->shortHelp );
		if( ( inFlags & kCLIPrintOptionsFlag_LongHelp ) && option->longHelp )
		{
			FPrintF( stderr, "%*{text}", ( inIndent / kCLIIndentWidth ) + 1, option->longHelp, kSizeCString );
		}
		++nShownOptions;
	}
	if( labelShown ) fputc( '\n', stderr );
	return( nShownOptions );
}

//===========================================================================================================================
//	_CLIHelp_PrintOptionName
//===========================================================================================================================

static int	_CLIHelp_PrintOptionName( const CLIOption *inOption, FILE *inFile )
{
	int		width;
	
	width = 0;
	if( inOption->shortName != '\0' )
	{
		width += FPrintF( inFile, "-%c", inOption->shortName );
		if( inOption->longName ) width += FPrintF( inFile, ", " );
	}
	if( inOption->type == kCLIOptionType_Argument )
	{
		char		startChar, endChar;
		
		startChar = ( inOption->flags & kCLIOptionFlags_OptionalArgument ) ? '[' : '<';
		endChar   = ( inOption->flags & kCLIOptionFlags_OptionalArgument ) ? ']' : '>';
		width += FPrintF( inFile, "%c%s%c", startChar, inOption->longName, endChar );
	}
	else
	{
		if( inOption->longName ) width += FPrintF( inFile, "--%s", inOption->longName );
		if( !( inOption->flags & kCLIOptionFlags_NoArgument ) )
		{
			int					literal;
			const char *		format;
			
			literal = ( inOption->flags & kCLIOptionFlags_LiteralArgHelp ) || !inOption->argHelp;
			if( inOption->flags & kCLIOptionFlags_OptionalArgument )
			{
				if( inOption->longName ) format = literal ? "[=%s]" : "[=<%s>]";
				else					 format = literal ? "[%s]"  : "[<%s>]";
			}
			else						 format = literal ? " %s" : " <%s>";
			width += FPrintF( inFile, format, inOption->argHelp ? inOption->argHelp : "..." );
		}
	}
	return( width );
}

//===========================================================================================================================
//	Built-in Version
//===========================================================================================================================

OSStatus	_CLIVersionOption( CLIOption *inOption, const char *inArg, int inUnset )
{
	(void) inArg;
	(void) inUnset;
	
	fprintf( stdout, "%s version %s (%s)\n", gProgramLongName ? gProgramLongName : gProgramName, 
		(const char *) inOption->valuePtr, (const char *) inOption->valueCountPtr );
	return( kEndingErr );
}

void	_CLIVersionCommand( void )
{
	_CLIVersionOption( gCLICurrentOption, NULL, 0 );
}

#if 0
#pragma mark -
#endif

#if( CLI_HAS_INTERACTIVE )

//===========================================================================================================================
//	Soft linking
//===========================================================================================================================

SOFT_LINK_LIBRARY( "/usr/lib", readline )

SOFT_LINK_FUNCTION_VOID_RETURN( readline, rl_callback_read_char, 
	( void ), 
	() )
#define rl_callback_read_char		soft_rl_callback_read_char

SOFT_LINK_FUNCTION( readline, rl_completion_matches, 
	char **, 
	( const char *inText, rl_compentry_func_t *inGenerator ), 
	( inText, inGenerator ) )
#define rl_completion_matches		soft_rl_completion_matches

SOFT_LINK_FUNCTION_VOID_RETURN( readline, rl_callback_handler_install, 
	( const char *inPrompt, VCPFunction *inHandler ), 
	( inPrompt, inHandler ) )
#define rl_callback_handler_install		soft_rl_callback_handler_install

SOFT_LINK_FUNCTION_VOID_RETURN( readline, rl_callback_handler_remove, 
	( void ), 
	() )
#define rl_callback_handler_remove		soft_rl_callback_handler_remove

SOFT_LINK_FUNCTION( readline, add_history, 
	int, 
	( const char *line ), 
	( line ) )
#define add_history		soft_add_history

SOFT_LINK_FUNCTION( readline, history_expand, 
	int, 
	( char *inLine, char **outLine ), 
	( inLine, outLine ) )
#define history_expand		soft_history_expand

SOFT_LINK_FUNCTION( readline, history_get, 
	HIST_ENTRY *, 
	( int inIndex ), 
	( inIndex ) )
#define history_get		soft_history_get

SOFT_LINK_FUNCTION( readline, read_history, 
	int, 
	( const char *inPath ), 
	( inPath ) )
#define read_history		soft_read_history

SOFT_LINK_FUNCTION( readline, write_history, 
	int, 
	( const char *inPath ), 
	( inPath ) )
#define write_history		soft_write_history

SOFT_LINK_FUNCTION( readline, history_truncate_file, 
	int, 
	( const char *path, int maxCount ), 
	( path, maxCount ) )
#define history_truncate_file		soft_history_truncate_file

SOFT_LINK_VARIABLE( readline, rl_attempted_completion_function, CPPFunction * )
#define rl_attempted_completion_function	( *var_rl_attempted_completion_function() )

SOFT_LINK_VARIABLE( readline, rl_instream, FILE * )
#define rl_instream		( *var_rl_instream() )

SOFT_LINK_VARIABLE( readline, rl_line_buffer, char * )
#define rl_line_buffer	( *var_rl_line_buffer() )

SOFT_LINK_VARIABLE( readline, rl_readline_name, char * )
#define rl_readline_name	( *var_rl_readline_name() )

SOFT_LINK_VARIABLE( readline, history_base, int )
#define history_base	( *var_history_base() )

SOFT_LINK_VARIABLE( readline, history_length, int )
#define history_length	( *var_history_length() )

//===========================================================================================================================
//	_CLIInteractiveOption
//===========================================================================================================================

OSStatus	_CLIInteractiveOption( CLIOption *inOption, const char *inArg, int inUnset )
{
	OSStatus				err;
	char					historyPath[ PATH_MAX ];
	size_t					len;
	dispatch_queue_t		queue;
	void ( *oldSig)( int );
	
	(void) inOption;
	(void) inArg;
	(void) inUnset;
	
	if( gCLIInteractiveMode )
	{
		fprintf( stderr, "error: already in interactive mode.\n" );
		return( kEndingErr );
	}
	
	// Ignore signals since we're going to handle SIGINT via GCD instead of a signal handler.
	// This needs to be done before setting up readline because it replaces and later invokes the saved off signal handler.
	
	oldSig = signal( SIGINT, SIG_IGN );
	
	// Set up tab completions.
	
	rl_readline_name = (char *) gProgramName;
	rl_attempted_completion_function = _CLIInteractiveCompleter;
	
	// Restore history from disk.
	
	*historyPath = '\0';
	GetHomePath( historyPath, sizeof( historyPath ) );
	len = strlen( historyPath );
	snprintf( &historyPath[ len ], sizeof( historyPath ) - len, "/.%s_history", gProgramName );
	read_history( historyPath );
	
	// Listen for user input.
	
	queue = dispatch_get_main_queue();
	gCLIInteractiveInputSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, (uintptr_t) fileno( rl_instream ), 0, queue );
	require_action( gCLIInteractiveInputSource, exit, err = kUnknownErr );
	dispatch_source_set_event_handler_f( gCLIInteractiveInputSource, _CLIInteractiveHandleInput );
	dispatch_resume( gCLIInteractiveInputSource );
	
	gCLIInteractiveSigIntSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, queue );
	require_action( gCLIInteractiveSigIntSource, exit, err = kUnknownErr );
	dispatch_source_set_event_handler_f( gCLIInteractiveSigIntSource, _CLIInteractiveHandleSigInt );
	dispatch_resume( gCLIInteractiveSigIntSource );
	
	// If arguments were already provided along with the -i option then process them first.
	
	if( ( gArgI + 1 ) < gArgC )
	{
		if( oldSig != SIG_ERR ) signal( SIGINT, oldSig );
		++gArgI;
		gCLIInteractiveMode = true;
		CLIParse( gRootOptions, 0 );
		gCLIInteractiveMode = false;
		signal( SIGINT, SIG_IGN );
	}
	
	// Process user input until the user quits.
	
	err = _CLIInteractiveReset();
	require_noerr( err, exit );
	@autoreleasepool
	{
		NSRunLoop *rl = [NSRunLoop currentRunLoop];
		gCLIInteractiveMode = true;
		while( gCLIInteractiveMode && [rl runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]] ) {}
	}
	rl_callback_handler_remove();
	
	// Save history to disk.
	
	write_history( historyPath );
	history_truncate_file( historyPath, 512 );
	err = kEndingErr;
	
exit:
	if( oldSig != SIG_ERR ) signal( SIGINT, oldSig );
	dispatch_source_forget( &gCLIInteractiveInputSource );
	dispatch_source_forget( &gCLIInteractiveSigIntSource );
	return( err );
}

//===========================================================================================================================
//	_CLIInteractiveCompleter
//===========================================================================================================================

static char **	_CLIInteractiveCompleter( const char *inText, int inStart, int inEnd )
{
	(void) inEnd;
	
	if( inStart == 0 ) return( rl_completion_matches( inText, _CLIInteractiveGenerator ) );
	return( NULL );
}

//===========================================================================================================================
//	_CLIInteractiveGenerator
//===========================================================================================================================

static char *	_CLIInteractiveGenerator( const char *inText, int inState )
{
	size_t					len;
	const CLIOption *		command;
	
	len = strlen( inText );
	for( command = ( inState == 0 ) ? gRootOptions : gCLIInteractiveNextCommand; 
		 command->type != kCLIOptionType_End; 
		 command = gCLIInteractiveNextCommand )
	{
		gCLIInteractiveNextCommand = command + 1;
		
		if( command->type != kCLIOptionType_Command )			continue;
		if( strnicmp( command->longName, inText, len ) != 0 )	continue;
		return( strdup( command->longName ) );
	}
	return( NULL );
}

//===========================================================================================================================
//	_CLIInteractiveHandleInput
//===========================================================================================================================

static void	_CLIInteractiveHandleInput( void *inArg )
{
	(void) inArg;
	
	gCLIInteractiveSigIntCount = 0; // Reset on any input so we only print help on multiple SIGINT's without input.
	rl_callback_read_char();
}

//===========================================================================================================================
//	_CLIInteractiveHandleLine
//===========================================================================================================================

static void	_CLIInteractiveHandleLine( char *inLine )
{
	int					result, i;
	char *				line = NULL;
	HIST_ENTRY *		entry;
	Boolean				add, process;
	OSStatus			err;
	int					argc = 0;
	char **				argv = NULL;
	
	result = history_expand( inLine, &line );
	if( result && ( *line != '\0' ) ) fprintf( stderr, "%s\n", line );
	require_action_quiet( ( result >= 0 ) && ( result != 2 ), exit, err = kNoErr );
	require_action_quiet( line, exit, err = kNoErr );
	
	// Process commands specific to interactive mode, such as exiting or showing history.
	
	process = true;
	if( ( stricmp( line, "q" ) == 0 ) || ( stricmp( line, "quit" ) == 0 ) || ( stricmp( line, "exit" ) == 0 ) )
	{
		err = kEndingErr;
		goto exit;
	}
	else if( stricmp( line, "history" ) == 0 )
	{
		for( i = 0; i < history_length; ++i )
		{
			entry = history_get( history_base + i );
			if( !entry ) continue;
			fprintf( stderr, "%d: %s\n", i + 1, entry->line );
		}
		process = false;
	}
	else if( stricmp( line, "?" ) == 0 )
	{
		free( line );
		line = strdup( "help" );
		require_action( line, exit, err = kNoMemoryErr );
	}
	
	// Add the line to the history if it's not the same as the last history item.
	
	add = true;
	if( history_length > 0 )
	{
		entry = history_get( history_base + ( history_length - 1 ) );
		if( entry && ( strcmp( entry->line, line ) == 0 ) ) // Note: case sensitive so users can fix case mistakes.
		{
			add = false;
		}
	}
	if( add ) add_history( line );
	require_action_quiet( process, exit, err = kNoErr );
	
	// Process the line as if it was entered from the shell.
	
	err = ParseCommandLineIntoArgV( line, &argc, &argv );
	require_noerr( err, exit );
	CLIFree();
	gArgI = 0;
	gArgC = argc;
	gArgV = (const char **) argv;
	CLIParse( gRootOptions, 0 );
	FreeCommandLineArgV( argc, argv );
	
exit:
	FreeNullSafe( line );
	FreeNullSafe( inLine );
	if( !err ) err = _CLIInteractiveReset();
	if( err )  gCLIInteractiveMode = false;
}

//===========================================================================================================================
//	_CLIInteractiveHandleSigInt
//===========================================================================================================================

static void	_CLIInteractiveHandleSigInt( void *inArg )
{
	(void) inArg;
	
	fprintf( stderr, "\n" );
	if( ++gCLIInteractiveSigIntCount >= 3 )
	{
		fprintf( stderr, "Enter 'q', 'quit', or 'exit' to exit interactive mode.\n" );
		gCLIInteractiveSigIntCount = 0;
	}
	*rl_line_buffer = '\0';
	_CLIInteractiveReset();
}

//===========================================================================================================================
//	_CLIInteractiveReset
//===========================================================================================================================

static OSStatus	_CLIInteractiveReset( void )
{
	OSStatus		err;
	char *			prompt;
	
	prompt = NULL;
	ASPrintF( &prompt, "%s> ", gProgramName );
	require_action( prompt, exit, err = kNoMemoryErr );
	rl_callback_handler_remove();
	rl_callback_handler_install( prompt, _CLIInteractiveHandleLine );
	free( prompt );
	err = kNoErr;
	
exit:
	if( err ) gCLIInteractiveMode = false;
	return( err );
}
#endif // CLI_HAS_INTERACTIVE

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CLIArgToValueOrErrQuit
//===========================================================================================================================

int	CLIArgToValueOrErrQuit( const char *inLabel, ... )
{
	va_list				args;
	const char *		arg;
	const char *		name;
	int					value;
	char *				errorStr;
	int					count, totalCount;
	
	arg = ( gArgI < gArgC ) ? gArgV[ gArgI++ ] : NULL;
	require_quiet( arg, error );
	
	va_start( args, inLabel );
	for( ;; )
	{
		name = va_arg( args, const char * );
		if( !name ) goto error;
		
		if( stricmp( name, kCLIArg_AnyTrueish ) == 0 )
		{
			if( IsTrueString( arg, SIZE_MAX ) )
			{
				value = 1;
				break;
			}
		}
		else if( stricmp( name, kCLIArg_AnyFalseish ) == 0 )
		{
			if( IsFalseString( arg, SIZE_MAX ) )
			{
				value = 0;
				break;
			}
		}
		else if( stricmp( name, kCLIArg_AnyInt ) == 0 )
		{
			if( SNScanF( arg, kSizeCString, "%i", &value ) == 1 )
			{
				break;
			}
		}
		else
		{
			value = va_arg( args, int );
			if( stricmp( arg, name ) == 0 )
			{
				break;
			}
		}
	}
	va_end( args );
	return( value );
	
error:
	errorStr = NULL;
	if( arg )	AppendPrintF( &errorStr, "error: bad %s: '%s'. It must be ", inLabel, arg );
	else		AppendPrintF( &errorStr, "error: no %s specified. It must be ", inLabel );
	
	// Count the number of items to know if we should use ", " or "or ".
	
	totalCount = 0;
	va_start( args, inLabel );
	for( ;; )
	{
		name = va_arg( args, const char * );
		if( !name ) break;
		if(      ( stricmp( name, kCLIArg_AnyTrueish )	== 0 ) ) {}
		else if( ( stricmp( name, kCLIArg_AnyFalseish )	== 0 ) ) {}
		else if( ( stricmp( name, kCLIArg_AnyInt )		== 0 ) ) {}
		else va_arg( args, int );
		 ++totalCount;
	}
	va_end( args );
	
	// Add a label for each allowed option.
	
	count = 0;
	va_start( args, inLabel );
	for( ;; )
	{
		name = va_arg( args, const char * );
		if( !name ) break;
		if(      ( stricmp( name, kCLIArg_AnyTrueish )	== 0 ) ) name = kAnyTrueMessage;
		else if( ( stricmp( name, kCLIArg_AnyFalseish )	== 0 ) ) name = kAnyFalseMessage;
		else if( ( stricmp( name, kCLIArg_AnyInt )		== 0 ) ) name = "an integer";
		else va_arg( args, int );
		
		AppendPrintF( &errorStr, "%s%s", MapCountToOrSeparator( count, totalCount ), name );
		++count;
	}
	va_end( args );
	
	ErrQuit( 1, "%s.\n", errorStr ? errorStr : "internal failure" );
	begin_unreachable_code_paths();
	FreeNullSafe( errorStr );
	end_unreachable_code_paths();
	return( 0 );
}

//===========================================================================================================================
//	CLIProgressClear
//===========================================================================================================================

void	CLIProgressClear( void )
{
	if( gCLIProgressMax > 0 )
	{
		FPrintF( stderr, "%*s\r", gCLIProgressMax, "" );
		gCLIProgressMax = 0;
	}
}

//===========================================================================================================================
//	CLIProgressUpdate
//===========================================================================================================================

void	CLIProgressUpdate( const char *inFormat, ... )
{
	va_list		args;
	int			n;
	
	va_start( args, inFormat );
	n = FPrintF( stderr, "%V", inFormat, &args );
	va_end( args );
	if( n < gCLIProgressMax )
	{
		FPrintF( stderr, "%*s\r", gCLIProgressMax - n, "" );
	}
	else
	{
		FPrintF( stderr, "\r" );
	}
	gCLIProgressMax = n;
}

//===========================================================================================================================
//	ErrQuit
//===========================================================================================================================

void	ErrQuit( int inExitCode, const char *inFormat, ... )
{
	va_list		args;
	
	va_start( args, inFormat );
	VFPrintF( stderr, inFormat, args );
	va_end( args );
	
	exit( inExitCode );
}
