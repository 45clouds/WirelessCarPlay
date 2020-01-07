/*
	File:    	LogUtils.h
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
	
	Copyright (C) 1997-2014 Apple Inc. All Rights Reserved.
*/

#ifndef	__LogUtils_h__
#define	__LogUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	group		LogUtils
	@abstract	LogUtils allow logging to be group into categories and control independently.
	@discussion
	
	Syntax:
	
		Empty control string means to delete all actions.
		
		'?' before a control string means to apply it as a default and re-read the prefs afterward.
		
		'+' before a control string means to write the final config string to prefs after applying.
		
		':' to access a variable for a category:
			
			This doesn't use '.' because it's used in name regexes to match any character (e.g. ".*:level=info").
			This doesn't use '->' because '>' is interpreted by shells for redirection to files.
			
			"MyCategory:level" for the "level" variable of MyCategory.
		
		'=' to assign a new value to a variable:
			
			"MyCategory:level=verbose" to set the level to verbose.
		
		';' after the output name for any parameters:
			
			"MyCategory:output=file;path=/var/log/MyCategory.log" to log to "/var/log/MyCategory.log".
		
		',' to separate actions in a control string:
			
			"MyCategory:level=verbose,MyCategory:output=syslog"
			
			Sets the level to verbose and the output type to syslog.
		
		Categories can be specified explicitly or via a simplified regular expression syntax:
		
			C	Matches any literal character C.
			.	Matches any single character.
			^	Matches the beginning of the input string.
			$	Matches the end of the input string
			*	Matches zero or more occurrences of the previous character.
	
	Variables:
	
		"flags" to set flags (separated by ';'):
			
			"none"		-- Clear all flags.
			"time"		-- Print the date/time      in front of all non-continuation log items.
			"pid"		-- Print the process ID     in front of all non-continuation log items.
			"program"	-- Print the program name   in front of all non-continuation log items.
			"category"	-- Print the category name  in front of all non-continuation log items.
			"level"		-- Print the log level name in front of all non-continuation log items.
			"prefix"	-- Print the prefix string  in front of all non-continuation log items.
			"function"	-- Print the function name  in front of all non-continuation log items.
			
			"MyCategory:flags=time;pid;program" to print time, PID, and program name headers.
		
		"level" -- Set the log level.
			
			"MyCategory:level=verbose+1" to set the level to verbose + 1.
		
		"rate" -- Limit the rate of logging. 1st number: period in milliseconds. 2nd number: max logs per period.
		
			"MyCategory:rate=2;3000" to limit it to no more than 2 logs in a 3 second window.
		
		"output" -- Control where the log output goes (parameters separated with ';'):
		
			"MyCategory:output=syslog" to send log output to syslog.
		
		"output2" -- Same as "output", but for a second output (e.g. write to syslog and write to a file).
		
			"MyCategory:output2=syslog" to send log output to syslog.
	
	Outputs:
	
		"asl"				-- Logs to Apple System Log (ASL).
			"facility"			-- Set ASL_KEY_FACILITY. Defaults to "user".
			"level"				-- Level to override the input log level. Format: level=<log level to use>.
			"sender"			-- Set ASL_KEY_SENDER. Defaults to the process name.
		"callback"			-- Logs to a callback function you specify.
			"func"				-- Pointer value (%p) to a LogOutputCallBack function to call.
			"arg"				-- Pointer value (%p) for the last context parameter to pass to the callback function.
		"console"			-- Meta output to choose the output that best represents a debug console.
		"dlc"				-- Logs via the DiagnosticLogCollection framework.
			"level"				-- Level to override default log level. Format: level=<low | medium | high | default>.
			"collection"		-- App/set of apps to log for. Format: any string.
			"component"			-- Component/area to log. If missing/empty uses current app identifier. Otherwise, GUID string.
			"category"			-- Non-user-visible string to store with the log message.
		"file"				-- Logs to a file or one of the standard file-ish things (e.g. stderr, stdout).
			"path"				-- Log file path. May also be "stderr" or "stdout" to go to those special destinations.
			"roll"				-- Control log rolling. Format: roll=<maxSize>#<maxCount>.
			"backup"			-- Control log file backups. Format: backup=<base path to backup files>#<maxCount>.
		"iDebug"			-- Logs using iDebug for IOKit drivers.
		"IOLog"				-- Logs using IOLog for IOKit drivers.
		"kprintf"			-- Logs using kprintf for Mac OS X kernel code.
		"oslog"				-- Logs using os_log for Mac OS X and iOS.
		"syslog"			-- Logs to syslog.
			"level"				-- Level to override the input log level. Format: level=<log level to use>.
		"WindowsDebugger"	-- Logs to Windows debugger.
		"WindowsEventLog"	-- Logs to Windows Event Log.
		"WindowsKernel"		-- Logs to Windows kernel log window for Windows drivers.
	
	Examples:
	
		// Define a category named "MyCategory" to log notice or higher to the console.
		
		ulog_define( MyCategory, kLogLevelNotice, kLogFlags_Default, "MyCategory", NULL );
		#define my_ulog( LEVEL, ... )		ulog( &log_category_from_name( MyCategory ), (LEVEL), __VA_ARGS__ )
		
		// Log to the "MyCategory" category.
		
		my_ulog( kLogLevelNotice, "Test MyCategory\n" );
		
		// Change the "MyCategory" category from the default console output to a file at "/tmp/MyCategory.log" with rolling.
		
		LogControl( "MyCategory:output=file;path=/tmp/MyCategory.log;roll=32K#4" );
		
		# Change the log level of "MyCategory" to verbose.
		mytool logging MyCategoryMyCategory:level=verbose
*/

// LOGUTILS_ENABLED -- Controls if LogUtils is enabled at all (mainly for platforms that can't support it).

#if( !defined( LOGUTILS_ENABLED ) )
	#if( DEBUG_SERVICES_LITE )
		#define LOGUTILS_ENABLED		0
	#else
		#define LOGUTILS_ENABLED		1
	#endif
#endif

#if( LOGUTILS_ENABLED )

// LOGUTILS_CF_ENABLED -- Controls whether CoreFoundation/CFPreferences support is compiled in.

#if( !defined( LOGUTILS_CF_ENABLED ) )
	#if( DEBUG_CF_OBJECTS_ENABLED )
		#define LOGUTILS_CF_ENABLED		1
	#else
		#define LOGUTILS_CF_ENABLED		0
	#endif
#endif

// LOGUTILS_CF_DISTRIBUTED_NOTIFICATIONS -- Controls whether CFDistributedNotification support is compiled in.

#if( !defined( LOGUTILS_CF_DISTRIBUTED_NOTIFICATIONS ) )
	#if( LOGUTILS_CF_ENABLED && TARGET_OS_MACOSX && !COMMON_SERVICES_NO_CORE_SERVICES && DEBUG )
		#define LOGUTILS_CF_DISTRIBUTED_NOTIFICATIONS		1
	#else
		#define LOGUTILS_CF_DISTRIBUTED_NOTIFICATIONS		0
	#endif
#endif

// LOGUTILS_CF_PREFERENCES -- Controls whether CFPreferences support is compiled in.

#if( !defined( LOGUTILS_CF_PREFERENCES ) )
	#if( LOGUTILS_CF_ENABLED && ( ( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES ) || \
	     TARGET_OS_POSIX || TARGET_OS_WINDOWS ) )
		#define LOGUTILS_CF_PREFERENCES		1
	#else
		#define LOGUTILS_CF_PREFERENCES		0
	#endif
#endif

// LOGUTILS_OSLOG_ENABLED -- 1=os_log enabled.

#if( !defined( LOGUTILS_OSLOG_ENABLED ) )
		#define LOGUTILS_OSLOG_ENABLED		0
#endif

// LOG_STATIC_LEVEL -- Controls what logging is conditionalized out at compile time.

#if( !defined( LOG_STATIC_LEVEL ) )
	#define	LOG_STATIC_LEVEL		0
#endif

// CF Constants

#define kLogUtilsRequestNotification		"logutilsreq"	//! User info dictionary contains control string, etc.
#define kLogUtilsAckNotification			"logutilsack"	//! User info dictionary contains control string, etc.
#define kLogUtilsKey_LogConfig				"logconfig" 	//! [String] LogControl string.

// LogFlags

typedef uint32_t		LogFlags;

#define kLogFlags_None				0			//! [0x00] No flags.
#define kLogFlags_Default			( kLogFlags_PrintTime | kLogFlags_PrintPrefix )

#define kLogFlags_PrintTime			( 1 << 0 )	//! [0x01] Print time of each log.
#define kLogFlags_PrintPID			( 1 << 1 )	//! [0x02] Print the process ID (PID) of each log.
#define kLogFlags_PrintProgram		( 1 << 2 )	//! [0x04] Print the program name of each log.
#define kLogFlags_PrintCategory		( 1 << 3 )	//! [0x08] Print the category name of each log.
#define kLogFlags_PrintLevel		( 1 << 4 )	//! [0x10] Print the log level name of each log.
#define kLogFlags_PrintPrefix		( 1 << 5 )	//! [0x20] Print the category name prefix of each log.
#define kLogFlags_PrintFunction		( 1 << 6 )	//! [0x40] Print the function that call ulog.

// LogOutput

typedef int		LogOutputType;

#define kLogOutputType_None					0
#define kLogOutputType_EFI					1
#define kLogOutputType_File					2
#define kLogOutputType_iDebug				3
#define kLogOutputType_IOLog				4
#define kLogOutputType_kprintf				5
#define kLogOutputType_syslog				6
#define kLogOutputType_ThreadX				7
#define kLogOutputType_WindowsDebugger		8
#define kLogOutputType_WindowsEventLog		9
#define kLogOutputType_WindowsKernel		10
#define kLogOutputType_CallBack				11
#define kLogOutputType_ASL					13
#if( LOGUTILS_OSLOG_ENABLED )
#define kLogOutputType_OSLog				14
#endif

typedef struct LogOutput			LogOutput;
typedef struct LogPrintFContext		LogPrintFContext;
typedef void ( *LogOutputWriter )( LogPrintFContext *inContext, LogOutput *inOutput, const char *inStr, size_t inLen );
typedef void ( *LogOutputCallBack )( LogPrintFContext *inPFContext, const char *inStr, size_t inLen, void *inContext );

// LogCategory

typedef struct LogCategory		LogCategory;
struct LogCategory
{
	LogLevel			level;			// Current level for this category.
	LogLevel			initLevel;		// Default level for this category.
	const char *		initConfig;		// Optional config string to apply. May be NULL.
	LogFlags			flags;			// Flags affecting output, etc.
	const char *		name;			// Name of this category. Underscores separate components (e.g. "com_apple_airport").
	const char *		prefixPtr;		// Prefix to use when logging with the kLogFlags_PrintPrefix flag. May be NULL.
	int					prefixLen;		// Length of the prefix. May be 0.
	LogCategory *		next;			// Next category in the list or NULL if this is the last one.
	LogOutput *			output1;		// Output object for this category. Must never be null after init.
	LogOutput *			output2;		// Secondary output object for this category.
	uint64_t			rateInterval;	// Rate interval in UpTicks (e.g. no more than X counts in Y UpTicks).
	uint64_t			rateEnd;		// UpTicks when a rate limit period should end.
	uint32_t			rateMaxCount;	// Max number of logs in a single interval.
	uint32_t			rateCounter;	// Number of logs in the current period.
};

// LogOutput

struct LogOutput
{
	LogOutput *			next;
	int32_t				refCount;
	char *				configStr;
	LogOutputWriter		writer;
	LogOutputType		type;
	union
	{
		#if( TARGET_OS_DARWIN )
		struct
		{
			char *		facility;
			int			priority;
			char *		sender;
			
		}	asl;
		#endif
		
		struct
		{
			LogOutputCallBack		func;
			void *					arg;
		
		}	callback;
		
		
		
		#if( DEBUG_FPRINTF_ENABLED )
		struct
		{
			char *		logFileName;
			FILE *		logFilePtr;
			int64_t		logFileSize;
			int64_t		logFileMaxSize;
			int			logFileMaxCount;
			char *		logBackupFileName;
			int			logBackupFileMaxCount;
			
		}	file;
		#endif
		
		#if( TARGET_OS_POSIX )
		struct
		{
			int		priority;
			
		}	syslog;
		#endif
		
		#if( LOGUTILS_OSLOG_ENABLED )
		struct
		{
			char *		subsystem;
			char *		category;
			Boolean		sensitive;
			void *		logObject;
			
		}	oslog;
		#endif
		
		#if( DEBUG_WINDOWS_EVENT_LOG_ENABLED )
		struct
		{
			HANDLE		source;
		
		}	windowsEventLog;
		#endif
		
	}	config;
};

// LogPrintFContext

struct LogPrintFContext
{
	LogCategory *		category;
	LogLevel			level;
#if( TARGET_OS_DARWIN )
	char				buf[ 2048 ];
#else
	char				buf[ 256 ];
#endif
	size_t				len;
	Boolean				flushOnEnd;
};

// Macros

#define	ulog_define( NAME, LEVEL, FLAGS, PREFIX, CONFIG )				\
	LogCategory		gLogCategory_ ## NAME = 							\
		{ kLogLevelUninitialized, (LEVEL), (CONFIG), (FLAGS), # NAME, 	\
		(PREFIX), (int) sizeof_string( (PREFIX) ), NULL, NULL, NULL, 	\
		0, 0, 0, 0 }

#define	ulog_extern( NAME )					extern LogCategory		gLogCategory_ ## NAME
#define log_category_from_name( NAME )		( gLogCategory_ ## NAME )

#define	log_category_enabled( CATEGORY_PTR, LEVEL )						\
	unlikely(															\
		( ( (LEVEL) & kLogLevelMask ) >= LOG_STATIC_LEVEL ) 		&&	\
		( ( (LEVEL) & kLogLevelMask ) >= (CATEGORY_PTR)->level ) 	&&	\
		( ( (CATEGORY_PTR)->level != kLogLevelUninitialized ) || 		\
		  _LogCategory_Initialize( (CATEGORY_PTR), (LEVEL) ) ) )

ulog_extern( DebugServicesLogging );
ulog_extern( LogUtils );

#if( TARGET_HAS_C99_VA_ARGS )
	#define	lu_ulog( LEVEL, ... )			ulog( &log_category_from_name( LogUtils ), ( LEVEL ), __VA_ARGS__ )
#elif( TARGET_HAS_GNU_VA_ARGS )
	#define	lu_ulog( LEVEL, ARGS... )		ulog( &log_category_from_name( LogUtils ), ( LEVEL ), ## ARGS )
#else
	#define	lu_ulog							LogPrintF_C89 // No VA_ARG macros so we have to do it from a real function.
#endif

// Prototypes

OSStatus	LogUtils_EnsureInitialized( void );
void		LogUtils_Finalize( void );

OSStatus	LogControl( const char *inCmd );
#if( LOGUTILS_CF_ENABLED )
	OSStatus	LogControlCF( CFStringRef inCmd );
#endif
#if( LOGUTILS_CF_PREFERENCES )
	void	LogSetAppID( CFStringRef inAppID );
	#define LogSetNSAppID( APP_ID )		LogSetAppID( (__bridge CFStringRef)(APP_ID) )
#endif
OSStatus	LogSetOutputCallback( const char *inCategoryRegex, int inOutputNum, LogOutputCallBack inCallback, void *inContext );
OSStatus	LogShow( char **outOutput );

Boolean		_LogCategory_Initialize( LogCategory *inCategory, LogLevel inLevel );
void		LogCategory_Remove( LogCategory *inCategory );

LogLevel	LUStringToLevel( const char *inStr );

#if( !TARGET_HAS_VA_ARG_MACROS )
	int		LogPrintF_C89( LogCategory *inCategory, const char *inFunction, LogLevel inLevel, const char *inFormat, ... );
#endif
int			LogPrintF( LogCategory *inCategory, const char *inFunction, LogLevel inLevel, const char *inFormat, ... );
int			LogPrintV( LogCategory *inCategory, const char *inFunction, LogLevel inLevel, const char *inFormat, va_list inArgs );

// ulog

#if( TARGET_HAS_C99_VA_ARGS )
	#define ulog( CATEGORY_PTR, LEVEL, ... )											\
		do																				\
		{																				\
			if( log_category_enabled( (CATEGORY_PTR), (LEVEL) ) )						\
			{																			\
				LogPrintF( (CATEGORY_PTR), __ROUTINE__, (LEVEL), __VA_ARGS__ );			\
			}																			\
																						\
		}	while( 0 )
#elif( TARGET_HAS_GNU_VA_ARGS )
	#define ulog( CATEGORY_PTR, LEVEL, ARGS... )										\
		do																				\
		{																				\
			if( log_category_enabled( (CATEGORY_PTR), (LEVEL) ) )						\
			{																			\
				LogPrintF( (CATEGORY_PTR), __ROUTINE__, (LEVEL), ## ARGS );				\
			}																			\
																						\
		}	while( 0 )
#else
	#define ulog		LogPrintF_C89 // No VA_ARG macros so we have to do it from a real function.
#endif

#define ulogv( CATEGORY_PTR, LEVEL, FORMAT, ARGS )										\
	do																					\
	{																					\
		if( log_category_enabled( (CATEGORY_PTR), (LEVEL) ) )							\
		{																				\
			LogPrintV( (CATEGORY_PTR ), __ROUTINE__, (LEVEL), (FORMAT), (ARGS) );		\
		}																				\
																						\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	LogUtils_Test
	@abstract	Unit test.
*/
#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	LogUtils_Test( void );
#endif

#endif // LOGUTILS_ENABLED

#ifdef __cplusplus
}
#endif

#endif // __LogUtils_h__
