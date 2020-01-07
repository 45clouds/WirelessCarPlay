/*
	File:    	TestUtils.h
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
/*!
	@file		Test API
	@brief		APIs for performing tests.
	@details
	
	To generate HTML documentation from this file run: AsciiDocExtractor.pl <path>
	Note: the trailing "+" on some lines is for asciidoc formatting and is not part of the file format.
	
	@asciidoc_begin	-a tabsize=4
	
	Test Utils
	==========
	:numbered:
	:tabsize: 4
	:toc:
	:toc-placement: manual
	
	Test Utils is a framework for testing code. It provides support for performing tests, checking results, timing tests, 
	controlling which tests are run, logging, and outputting test reports. It has the following features:
	
	- Portable
		* Works in any reasonable C, C++, Objective-C, or Swift environment.
		* Works with any automation system that can invoke a tool and read its output.
	
	- Minimal
		* Setup only requires building a tool with a couple function calls to initialize and finalize it.
		* Configuration is only needed for more advanced usage, such as automation, and even then it's only a few options.
		* Writing tests only requires invoking a few macros to perform tests and check results.
	
	- Scalable
		* Easily used by a single engineer by running a tool and looking at a single line result for each test.
		* Integrates with automation systems via standard, easily parseable text reports.
	
	toc::[]
	
	First-Time Setup
	----------------
	Each component being tested needs a command line tool to run tests. For example, a library for performing linear 
	algebra might have a tool named "TestLinearAlgebraLib". This tool may contain hundreds of tests. For environments 
	that don't support separate processes, this can simply by a function that operations like the main() function of a 
	normal command line tool. The only requirement is that it can be invoke with a set of command line arguments and can 
	produce textual output.
	
	The normal process for first-time setup is to create a new tool project (e.g. new command line tool target in Xcode 
	or new make target in a makefile-based system). The main function of the tool calls TUInitialize() as soon with any 
	command line arguments passed to the tool. The tests to be performed are then executed. Before the tool exits, it
	calls TUFinalize().
	
	Writing Tests
	-------------
	Writing a test requires writing a function and passing it to TUPerformTest(). The test function uses the tu_require* 
	macros to check each result. These macros cause output to be emitted for the user or the automation system to 
	read results. TUSetExpectedTestCount() is used to set the expected number of tests. When developing tests, you 
	typically verify the tests manually the first time and use the total test count it prints to determine the count.
	This manual step of setting the expected test count prevents accidental skipping of tests from going unnoticed.
	
	The tu_require* macros take a result and an exit label. If a test fails (i.e. its condition evaluates to false), the 
	failure will be logged. A test failure causes subsequent tests in the function to be skipped. It doesn't try to 
	continue with tests in the same function since they may rely on previous operations completing successfully. The 
	expectation is that tests failures are identified and fixed quickly and don't remain in a failing state. Individual 
	test failures are always logged, but successes are not logged by default. This can be controlled by invoking the test 
	tool with --level=trace (or any more verbose log level).
	
	-	If you need any global initialization (i.e. initialization for the entire test tool process/environment), put it 
		in the main function, after calling TUInitialize(), but before invoking any tests.
	
	-	If you need any global finalization, put it in the main function, after the exit label, but before calling 
		TUFinalize(). Global finalization must be safe to invoke even if the global initialization code has not been 
		called. It may be invoked in cases where TUInitialize() fails so it needs to be safe in these cases.
	
	-	If you need any test-specific initialization, put it in the test function before invoking any test macros in that 
		function.
	
	-	If you need any test-specific finalization, put it in the test function, after the exit label.
	
	-	Testing code must not allow any C++ or Objective-C exceptions or setjmp/longjmp to jump out of test functions.
	
	The following macros are used for testing:
	
	*tu_require*::
		
		C:		`tu_require( TEST, EXIT_LABEL )` +
		Swift:	`tu_require( TEST )` +
		+
		Tests if TEST is non-zero. Otherwise, it treats it as a failure. +
		For C, jumps to EXIT_LABEL on failures. +
		For Swift, this throws an error on failure.
	
	*tu_require_action*::
		
		C:		`tu_require_action( TEST, EXIT_LABEL, ACTION )` +
		Swift:	not available. +
		+
		Tests if TEST is non-zero. Otherwise, it treats it as a failure and executes an action. +
		Jumps to EXIT_LABEL on failures.
	
	*tu_require_noerr*::
		
		C:		`tu_require_noerr( ERR, EXIT_LABEL )` +
		Swift:	not available. +
		+
		Tests if ERR is 0. Otherwise, it treats it as a failure. +
		Jumps to EXIT_LABEL on failures.
	
	*tu_require_eq*::
		
		C:		not available.
		Swift:	`tu_require_eq( A, B )`. +
		+
		Tests if A is equal to B. Otherwise, it treats it as a failure.
	
	*tu_require_eq( within: )*::
		
		C:		not available. +
		Swift:	`tu_require_eq( A, B, within:C )`. +
		+
		Tests if A is equal to B within a threshold C. Otherwise, it treats it as a failure. +
		For example, 10 is equal to 8 within a threshold of 3 (10-8 <= 3).
	
	*tu_require_ne*::
		
		C:		not available. +
		Swift:	`tu_require_ne( A, B )`. +
		+
		Tests if A is not equal to B. Otherwise, it treats it as a failure.
	
	*tu_require_lt*::
		
		C:		not available. +
		Swift:	`tu_require_lt( A, B )`. +
		+
		Tests if A is less than B. Otherwise, it treats it as a failure.
	
	*tu_require_le*::
		
		C:		not available. +
		Swift:	`tu_require_le( A, B )`. +
		+
		Tests if A is less than or equal to B. Otherwise, it treats it as a failure.
	
	*tu_require_gt*::
		
		C:		not available. +
		Swift:	`tu_require_gt( A, B )`. +
		+
		Tests if A is greater than B. Otherwise, it treats it as a failure.
	
	*tu_require_ge*::
		
		C:		not available. +
		Swift:	`tu_require_ge( A, B )`. +
		+
		Tests if A is greater than or equal to B. Otherwise, it treats it as a failure.
	
	The following shows a minimal, but functional tool for testing the standard C strcmp function:
	----
	static OSStatus	StrCmpTest( TUTestContext *inTestCtx );
	
	int	main( int argc, const char **argv )
	{
		TUInitialize( argc, argv );
		TUSetExpectedTestCount( 4 );
		TUPerformTest( StrCmpTest );
		TUFinalize();
		return( 0 );
	}
	
	static OSStatus	StrCmpTest( TUTestContext *inTestCtx )
	{
		tu_require( strcmp( "", "" ) == 0, exit );
		tu_require( strcmp( "a", "a" ) == 0, exit );
		tu_require( strcmp( "a", "b" ) < 0, exit );
		tu_require( strcmp( "b", "c" ) > 0, exit );
		
	exit:
		return;
	}
	----
	
	Swift Testing
	-------------
	Swift code can also use TestUtils for unit testing. The syntax is a little different due to differences between
	Swift and C, but it's similar and the report format is identical. Each test is implemented by subclassing the TUTest
	base class. The subclass must override the test() function to implement the test. It may also override the setUp()
	and tearDown() functions if it needs to set anything up before the test() function is called and tear it down after
	the test() function returns. An instance of the subclass is passed to the TUPerformTest() function. The following is 
	a complete example.
	
	.main.swift
	----
	TUInitialize()
	TUSetExpectedTestCount( 4 )
	TUPerformTest( StrCmpTest() )
	TUFinalize()
	----
	
	.StrCmpTest.swift
	----
	class StrCmpTest : TUTest
	{
		override func test() throws
		{
			try tu_require( stricmp( "", "" ) == 0 )
			try tu_require( stricmp( "a", "a" ) == 0 )
			try tu_require( stricmp( "a", "b" ) < 0 )
			try tu_require( stricmp( "b", "c" ) > 0 )
		}
	}
	----
	
	Fault Injection
	---------------
	To simulate real errors that are difficult to reproduce in real life and to force the execution of certain code paths, 
	faults can be injected into specific areas of code. These faults are specified by name and can be enabled or disabled
	at runtime. Fault injection macros are added to each area of code where a fault may wanted to be simulated. The code 
	in these macros is stripped out for normal builds, but can be enabled at build time for performing tests.
	
	To add support for fault injection, use the tu_fault_inject_* macros throughout your codebase. These macros take a
	name and pointer to a variable. If the fault is enabled, the variable will be modified to inject a fault. For example, 
	if you have code that checks the range of a sensor value, you would invoke the macro before the bounds check. Then 
	to test that your bounds checking and error handling code works correctly, you would enable the fault to set the 
	variable to a value that is out-of-bounds. This should trigger the error paths in that piece of code. To test with 
	fault injection, build with the TU_FAULTS_ENABLED preprocessor symbol defined to 1. Then enable the specific faults
	you want to test against and verify that the unit tests fail appropriately.
	
	The following is an example of adding fault injection support for sensor bounds checking. For normal, production 
	builds, the fault injection code is completely stripped out. But if fault injection is enabled at build time then
	you can use the TUFaultEnable function to inject a fault by changing the incoming temperature sensor value to a value
	that's out of range (or maybe right at the edge of the value range to simulate extreme, but still valid values that
	may identify a bug in other parts of the code).
	
	.C example
	----
	OSStatus	MyTemperatureSensorHandler( int inTemperature )
	{
		OSStatus		err;
		
		tu_fault_inject_int( "temperature-sensor", &inTemperature );
		require_action( ( inTemperature >= 10 ) && ( inTemperature <= 20 ), exit, err = kRangeErr );
		
		... process temperature sensor.
		
	exit:
		return( err );
	}
	----
	
	.Swift example
	----
	func MyTemperatureSensorHandler( inTemperature : Int ) throws
	{
		var temperature = inTemperature
		tu_fault_inject( "temperature-sensor", &temperature );
		require( ( temperature >= 10 ) && ( temperature <= 20 ), SomeError )
		
		... process temperature sensor.
	}
	
	// Then to enable fault injection for the temperature sensor (adds 10 to temperature):
	
	TUFaultEnable( "test1" )
	{
		if let i = $0 as? Int { return i + 10 }
		return $0
	}
	----
	
	Command Line Options
	--------------------
	The following options may be used when invoking the test tool from the command line to control behavior:
	
	- --BreakOnFail
		* Controls if testing breaks into the debugger on the first failure.
		* Defaults to true.
	
	- --ConvertTURtoJUnit
		* Converts TestUtils Report (.tur) file to a JUnit XML report file.
		* First argument is path to input TUR file (or - to use stdin).
		* Second argument is path to output JUnit XML file (or - to use stdout).
		* Exits when conversion is completed and sets exit status to 1 for failure and 0 for success.
	
	- --DontRunLeaks
		* Controls if leaks shouldn't be run.
		* Defaults to false (i.e. leaks will run).
	
	- --ExcludeNonTestUtilsTests
		* Excludes tests that haven't been converted to TestUtils format.
		* Defaults to running non-TestUtils tests.
	
	- --Filter
		* Comma-separated lists of tests to limit to.
		* Defaults to no filter.
	
	- --JUnitXMLOutputPath _<file path>_
		* Writes JUnit-compatible XML to the specified path.
		* Must be used with the --OutputPath option.
		* Defaults to not writing JUnit.
	
	- --Level _<level>_
		* Controls the verbosity of test results and log messages.
		* <level> may be "all", "chatty", "verbose", "trace", "info", "notice", "warning", "error", or "max".
		* Defaults to "warning".
	
	- --LogControl _<log control string>_
		* Applies a LogUtils log control string for use during testing.
	
	- --OutputPath _<file path>_
		* Redirects output to the specified file path.
		* Defaults to writing to stdout.
	
	- --Qualifier _<qualifier>_
		* Qualifier string to add to each test name (e.g. "linux-arm" -> "StrCmpTest.linux-arm").
	
	- --StopOnFirstFail
		* Controls if testing stops on the first failure. Otherwise, testing continues unless a failed test is marked as fatal.
		* Defaults to true.
	
	- --UserMode
		* User mode is for running tests manually by a user, such as the developer testing code before committing it.
		* When user mode is done, tests results are output in a mode that's easier to read by humans.
		* Defaults to true.
	
	Test Output format
	------------------
	The test output format uses a structure inspired by the Session Description Protocol (SDP). Each line is of the
	format <type>=<value>\n. <type> is a single character defined by this specification. No spaces are allowed before the 
	equal sign. Any spaces after the equal sign are considered part of the value and must not be stripped out by parsers.
	<value> is any number of UTF-8 bytes, terminated with a newline character. The following is an example:
	
		T=
		t=TLV8Test
		f=bool:TLVUtils.c:667, TestTLV8(), "err == kNotFoundErr"
		p=bool:TLVUtils.c:697, TestTLV8(), "type == 0x11"
		f=err:TLVUtils.c:709, TestTLV8(), -6722 kTimeoutErr
		p=err:TLVUtils.c:716, TestTLV8(), 0 kNoErr
		r=2/4
		d=1.532s
		z=
		t=StringUtilsTest
		r=32/32
		d=5.231s
		#=Checking "a" vs "b".
		#=Checking "c" vs "c".
		z=
		Z=
	
	Comment/Log ("#=")
	~~~~~~~~~~~~~~~~~~
	Format:  `#=<Single line comment or log message>` +
	Example: `"#=Manual test run to verify server"`
	
	Comments are included in trace or more verbose logs, but otherwise do not affect testing. There may be multiple 
	comment lines. Logs containing newlines are split across multiple comment lines.
	
	Duration ("d=")
	~~~~~~~~~~~~~~~
	Format:  `d=<duration of test in floating point second>s` +
	Example: `"d=5.231s"`
	
	The "d=" field indicates how long the test took to perform.
	
	Failure ("f=")
	~~~~~~~~~~~~~~
	Format:  `f=bool:<filename>:<line number>, <function name>(), "<test string>"` +
	Format:  `f=err:<filename>:<line number>, <function name>(), <error code> [error description]` +
	Format:  `f=leaks:<filename>:<line number>, <function name>()[, <leaks failed to run error>]` +
	Format:  `f=total:<actual test count>/<expected test count>` +
	Example: `"f=err:Test.c:123, MyFunction(), -6722 kTimeoutErr"` +
	Example: `"f=bool:Test.c:456, MyFunction(), "result == 123""` +
	Example: `"f=leaks:Test.c:456, MyFunction()"` +
	Example: `"f=total:2947/2929"`
	
	The "f=" field reports a single test failure.
	
	Pass ("p=")
	~~~~~~~~~~~
	The "p=" field reports a single test pass. It has the same format as failure "f=" except it uses "p=" instead.
	
	Result ("r=")
	~~~~~~~~~~~~~
	Format:  `r=<# of passed tests>/<total # of tests>` +
	Example: `"r=32/32"`
	
	The "r=" field indicates the overall result of the test: pass or fail. If any sub-test fails, "fail" is reported.
	
	Start Test ("t=")
	~~~~~~~~~~~~~~~~~
	Format:  `t=<name of test>[; <single line description of test>]` +
	Example: `"t=StringUtilsTest; Tests string utility functions"`
	
	The "t=" field indicates the name of the test.
	
	Start of all tests ("T=")
	~~~~~~~~~~~~~~~~~~~~~~~~~
	Format:  `T=` +
	Example: `"T="`
	
	The "T=" field indicates the start of all tests. It must be the first line and can only appear once.
	
	End test ("z=")
	~~~~~~~~~~~~~~~
	Format:  `z=` +
	Example: `"z="`
	
	The "z=" field indicates the end of the test.
	
	End of all tests ("Z=")
	~~~~~~~~~~~~~~~~~~~~~~~
	Format:  `Z=` +
	Example: `"Z="`
	
	The "Z=" field indicates the end of all tests for this tool.
	The test report parser will stop reading the output of all tests after it reads this line.
	
	JUnit Support
	-------------
	JUnit is a unit testing framework for Java. Most Continuous Integration (CI) tools, like Jenkins, support processing
	test reports written in JUnit's XML format. JUnit's XML format isn't well suited to streaming because it requires 
	information, such as the total number of failures, at the beginning of the report. This would force test reporting to
	buffer everything and only emit a report at the end. The environment that TestUtils runs in may have very limited 
	memory and/or little to no persistent storage (e.g. embedded module). To work in these environments, TestUtils writes 
	reports in its own format, which is designed for streaming with only minimal state, then the TestUtils report is 
	converted to JUnit XML by a tool running on the CI server (which generally has plenty of memory and disk space).
	
	The following describes the XML that TestUtil emits to be compatible with JUnit and how it relates to the TestUtils 
	report format.
	
	* "testsuites"		-- Top-level element containing all TestUtils test groups.
		** "testsuite"		-- Element for a TestUtils test group (e.g. one for all "TLVUtils" tests).
			*** "errors"		-- Attribute for the number of errors (not normal failures, but errors with the test process).
			*** "failures"		-- Attribute for the number of failures (e.g. tu_require* reporting a failure).
			*** "name"			-- Attribute for the name of the test group (e.g. "StringUtils").
			*** "package"		-- Attribute for the name of the package (not used by TestUtils).
			*** "skipped"		-- Attribute for the number of skipped tests.
			*** "system-err"	-- Element for stderr from the test group. Body of element contains the text.
			*** "system-out"	-- Element for stdout from the test group. Body of element contains the text.
			*** "testcase"		-- Element for a TestUtil test (e.g. one for "TLV8Test").
				**** "assertions"	-- Attribute for the total number of assertions in the test.
				**** "classname"	-- Attribute for the name of the test (e.g. "TLV8Test").
				**** "error"		-- Element for an error. Body of element contains any textual detail about the error.
					***** "message"		-- Attribute for the error message (e.g. "Leaks failed with 1 EPERM").
					***** "type"		-- Attribute for the type of error (e.g. "Process error").
				**** "failure"		-- Element for a test failure. Body of element contains any textual detail about the failure.
					***** "message"		-- Attribute for the error message (e.g. "ptr != NULL").
					***** "type"		-- Attribute for the type of error (e.g. "bool").
				**** "skipped"			-- Element if the test was skipped (e.g. "<skipped/>"). Missing if not skipped.
				**** "system-err"		-- Element for stderr from the test. Body of element contains the text.
				**** "system-out"		-- Element for stdout from the test. Body of element contains the text.
				**** "time"			-- Attribute for the duration of the test in seconds (e.g. "0.10" for 10 milliseconds).
			*** "tests"			-- Attribute for the number of tests in this group (e.g. "5").
			*** "time"			-- Attribute for the duration of the test group in seconds (e.g. "0.10" for 10 milliseconds).
			*** "timestamp"		-- Attribute for the start time of a test group in ISO 8601 format (e.g. "2014-02-10T12:49:59").
	
	.Example JUnit XML
		<?xml version="1.0" encoding="UTF-8" ?>
		<testsuites>
			<testsuite name="AllTests" tests="2" failures="1" time="6.763000">
				<testcase name="TLV8Test" classname="TLVUtils" time="1.532000">
					<failure type="bool" message="&quot;err == kNotFoundErr&quot;">TLVUtils.c:667, TestTLV8()</failure>
					<failure type="err" message="-6722 kTimeoutErr">TLVUtils.c:709, TestTLV8()</failure>
				</testcase>
				<testcase name="StringUtilsTest" classname"StringUtils" time="5.231000"/>
			</testsuite>
		</testsuites>
	
	Alternatives
	------------
	There are several alternative unit testing tools. The following lists some of them and the reason they weren't chosen.
	
	-	*Google Test/gtest* +
		Google Test/gtest is Google's unit testing framework. It provides basic functionality, but it requires C++, which
		isn't supported in some environments that we need to test. It also relies on JUnit output reports, which don't 
		work in resource-constrained environments due to the way the JUnit report is structured (e.g. total failures 
		reported at the beginning of the report so a device would need to buffer the results of all tests. Some devices
		don't have enough memory to buffer).
	
	-	*XCTest* +
		XCTest is the test framework built into Xcode. It provides basic functionality, but it requires Objective-C and 
		only works with XCode. Many projects need to work in environments that don't support Objective-C or Xcode.
		It also lacks a reporting format for integration with automated testing tools.
	
	References
	----------
	Author: Bob Bradley <bradley@apple.com> +
	Permanent ID of this document: 3e55db1c4347a955675e05d329e7f66c.
	
	- Document IDs +
	<http://cr.yp.to/bib/documentid.html>
	
	- How Jenkins CI parses and displays jUnit output +
	<http://nelsonwells.net/2012/09/how-jenkins-ci-parses-and-displays-junit-output/>
	
	- JUnit XML reporting file format +
	<http://llg.cubic.org/docs/junit/>
	
	- JUnit Format +
	<http://help.catchsoftware.com/display/ET/JUnit+Format>
	
	@asciidoc_end
*/

#ifndef	__TestUtils_h__
#define	__TestUtils_h__

#include <stdarg.h>

#include "CommonServices.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == Types ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		TUTestContext
	@brief		Context for holding test state.
*/
typedef struct
{
	const char *		testName;		// Name of test.
	uint64_t			testStartTicks;	// UpTicks when test started.
	uint32_t			testPasses;		// Number of tests that passed.
	uint32_t			testFails;		// Number of tests that failed.
	OSStatus			testStatus;		// Status of test.
	
}	TUTestContext;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	TUTest_f
	@brief		Function that implements a test.
*/
typedef void ( *TUTest_f )( TUTestContext *inTestCtx );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	TUFlags
	@brief		Flags for control behavior.
*/
typedef uint32_t		TUFlags;
#define kTUFlags_None		0			//! No flags. Deprecated. Use kTUFlag_None instead.
#define kTUFlag_None		0			//! No flags.
#define kTUFlag_Fatal		( 1 << 0 )	//! Test cannot continue if this fails (i.e. always jump to exit label on failure).

#if 0
#pragma mark == Macros ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			tu_require
	@brief		Tests if a boolean test result is true. If false, it treats it as a failure.
*/
#define tu_require( VALUE, LABEL ) \
	do \
	{ \
		int const		_tur_value = !!(VALUE); \
		\
		if( TUTestRequire( inTestCtx, kTUFlag_Fatal, _tur_value, __FILE__, __LINE__, __ROUTINE__, #VALUE ) || !_tur_value ) \
		{ \
			goto LABEL; \
		} \
		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			tu_require_action
	@brief		Tests if a boolean test result is true. If false, it treats it as a failure and executes an action.
*/
#define tu_require_action( VALUE, LABEL, ACTION ) \
	do \
	{ \
		int const		_tur_value = !!(VALUE); \
		\
		if( TUTestRequire( inTestCtx, kTUFlag_Fatal, _tur_value, __FILE__, __LINE__, __ROUTINE__, #VALUE ) || !_tur_value ) \
		{ \
			{ ACTION; } \
			goto LABEL; \
		} \
		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			tu_require_noerr
	@brief		Tests if an OSStatus test result is kNoErr. If not, it treats it as a failure.
*/
#define tu_require_noerr( ERR, LABEL ) \
	do \
	{ \
		OSStatus const		_tutrn_err = (ERR); \
		\
		if( TUTestRequireNoErr( inTestCtx, kTUFlag_Fatal, _tutrn_err, __FILE__, __LINE__, __ROUTINE__ ) || _tutrn_err ) \
		{ \
			goto LABEL; \
		} \
		\
	}	while( 0 )

#if 0
#pragma mark == Functions ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TUInitialize
	@brief		Initializes the test environment with the specified command line arguments.
	@result		Non-zero means the calling macro should jump to the exit label.
	@details	This should be called as the first thing in main of the test tool, before any tests are started.
*/
OSStatus	TUInitialize( int inArgC, const char **inArgV );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TUFinalize
	@brief		Finalizes the test environment.
	@details	This should be called from main, after all tests have ended.
*/
void	TUFinalize( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TUTestCheckLeaks
	@brief		Checks if there are any memory leaks and if so, it treats it as a test failure.
	@result		Non-zero means the calling macro should jump to the exit label. 0 means success.
*/
OSStatus
	TUTestCheckLeaks( 
		TUTestContext *	inTestCtx, 
		TUFlags			inFlags, 
		const char *	inFilename, 
		long			inLineNumber, 
		const char *	inFunction );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TUPerformTest
	@brief		Performs a test.
*/
#define TUPerformTest( FUNC )	_TUPerformTest( #FUNC, (FUNC) )
void	_TUPerformTest( const char *inName, TUTest_f inFunc );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TUSetExpectedTestCount
	@brief		Sets the number of total tests to expect.
*/
void	TUSetExpectedTestCount( uint32_t inTotalTests );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TUTestRequire
	@brief		Tests if a value is true and if it's not, it treats it as a test failure.
	@result		Non-zero means the calling macro should jump to the exit label. 0 means success.
*/
OSStatus
	TUTestRequire( 
		TUTestContext *	inTestCtx, 
		TUFlags			inFlags, 
		int				inValue, 
		const char *	inFilename, 
		long			inLineNumber, 
		const char *	inFunction, 
		const char *	inTestString );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TUTestRequireNoErr
	@brief		Tests if an error is kNoErr and if it is, it treats it as a test failure.
	@result		Non-zero means the calling macro should jump to the exit label. 0 means success.
*/
OSStatus
	TUTestRequireNoErr( 
		TUTestContext *	inTestCtx, 
		TUFlags			inFlags, 
		OSStatus		inErrorCode, 
		const char *	inFilename, 
		long			inLineNumber, 
		const char *	inFunction );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TULogLevelEnabled
	@brief		Returns true if the specified log level is enabled.
*/
Boolean	TULogLevelEnabled( TUTestContext *inTestCtx, LogLevel inLevel );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TULogF / TULogV
	@brief		Logs text in the test report.
	@details	Supports all the same format specifiers as FPrintF from PrintFUtils.h.
*/
void	TULogF( TUTestContext *inTestCtx, LogLevel inLevel, const char *inPrefix, const char *inFormat, ... );
void	TULogV( TUTestContext *inTestCtx, LogLevel inLevel, const char *inPrefix, const char *inFormat, va_list inArgs );

extern Boolean		gTUExcludeNonTestUtilsTests; // Workaround until all tests have been converted to TestUtils.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TUConvertToJUnit
	@brief		Converts a TestUtils Report (TUR) file to JUnit XML format.
	
	@param		inTUPath		Path to input TestUtils Report (TUR) file. May be "-" to read from stdin.
	@param		inJUnitPath		Path to output JUnit XML file. May be "-" to write from stdout.
*/
OSStatus	TUConvertToJUnit( const char *inTUPath, const char *inJUnitPath );

#if 0
#pragma mark == Faults ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defgroup	TUFaultDataType
	@brief		Enables fault injection for the specified name.
*/
typedef uint32_t	TUFaultDataType;
#define kTUFaultDataType_Boolean		1
#define kTUFaultDataType_SInt8			2
#define kTUFaultDataType_UInt8			3
#define kTUFaultDataType_SInt16			4
#define kTUFaultDataType_UInt16			5
#define kTUFaultDataType_SInt32			6
#define kTUFaultDataType_UInt32			7
#define kTUFaultDataType_SInt64			8
#define kTUFaultDataType_UInt64			9
#define kTUFaultDataType_int			10
#define kTUFaultDataType_size_t			11
#define kTUFaultDataType_Float32		20
#define kTUFaultDataType_Float64		21
#define kTUFaultDataType_float			22
#define kTUFaultDataType_double			23

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TUFaultEnable
	@brief		Enables fault injection for the specified name.
*/
OSStatus	TUFaultEnable( const char *inName, Value64 inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TUFaultDisable
	@brief		Disables fault injection for the specified name.
*/
OSStatus	TUFaultDisable( const char *inName );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TUFaultInject
	@brief		Modifies the pointer if fault injection is enabled for the specified name.
*/
OSStatus	TUFaultInject( const char *inName, TUFaultDataType inType, void *inPtr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defgroup	TUFaultInjectionMacros
	@brief		Macros for injecting faults.
	@details	These are disabled by default and only enabled if TU_FAULTS_ENABLED is defined to a non-zero value.
*/
#if( !defined( TU_FAULTS_ENABLED ) )
	#define TU_FAULTS_ENABLED		0
#endif
#if( TU_FAULTS_ENABLED )
	#define tu_fault_inject_boolean( NAME, PTR )		TUFaultInject( (NAME), kTUFaultDataType_Boolean, (PTR) )
	#define tu_fault_inject_sint8( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_SInt8, (PTR) )
	#define tu_fault_inject_uint8( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_UInt8, (PTR) )
	#define tu_fault_inject_sint16( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_SInt16, (PTR) )
	#define tu_fault_inject_uint16( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_UInt16, (PTR) )
	#define tu_fault_inject_sint32( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_SInt32, (PTR) )
	#define tu_fault_inject_uint32( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_UInt32, (PTR) )
	#define tu_fault_inject_sint64( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_SInt64, (PTR) )
	#define tu_fault_inject_uint64( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_UInt64, (PTR) )
	#define tu_fault_inject_int( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_int, (PTR) )
	#define tu_fault_inject_size_t( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_size_t, (PTR) )
	#define tu_fault_inject_float32( NAME, PTR )		TUFaultInject( (NAME), kTUFaultDataType_Float32, (PTR) )
	#define tu_fault_inject_float64( NAME, PTR )		TUFaultInject( (NAME), kTUFaultDataType_Float64, (PTR) )
	#define tu_fault_inject_float( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_float, (PTR) )
	#define tu_fault_inject_double( NAME, PTR )			TUFaultInject( (NAME), kTUFaultDataType_double, (PTR) )
#else
	#define tu_fault_inject_boolean( NAME, PTR )		do {} while( 0 )
	#define tu_fault_inject_sint8( NAME, PTR )			do {} while( 0 )
	#define tu_fault_inject_uint8( NAME, PTR )			do {} while( 0 )
	#define tu_fault_inject_sint16( NAME, PTR )			do {} while( 0 )
	#define tu_fault_inject_uint16( NAME, PTR )			do {} while( 0 )
	#define tu_fault_inject_sint32( NAME, PTR )			do {} while( 0 )
	#define tu_fault_inject_uint32( NAME, PTR )			do {} while( 0 )
	#define tu_fault_inject_sint64( NAME, PTR )			do {} while( 0 )
	#define tu_fault_inject_uint64( NAME, PTR )			do {} while( 0 )
	#define tu_fault_inject_int( NAME, PTR )			do {} while( 0 )
	#define tu_fault_inject_size_t( NAME, PTR )			do {} while( 0 )
	#define tu_fault_inject_float32( NAME, PTR )		do {} while( 0 )
	#define tu_fault_inject_float64( NAME, PTR )		do {} while( 0 )
	#define tu_fault_inject_float( NAME, PTR )			do {} while( 0 )
	#define tu_fault_inject_double( NAME, PTR )			do {} while( 0 )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			TestUtilsTest
	@brief		Unit test.
*/
void	TestUtilsTest( void );

#ifdef __cplusplus
}
#endif

#endif // __TestUtils_h__
