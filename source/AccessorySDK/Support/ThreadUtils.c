/*
	File:    	ThreadUtils.c
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
	
	Copyright (C) 2010-2014 Apple Inc. All Rights Reserved.
*/

#include "ThreadUtils.h"

#if( TARGET_MACH )
	#include <mach/mach.h>
#endif

//===========================================================================================================================
//	GetMachThreadPriority
//===========================================================================================================================

#if( TARGET_MACH )
int	GetMachThreadPriority( int *outPolicy, OSStatus *outErr )
{
	OSStatus						err;	
	mach_port_t						machThread;
	unsigned int					count;
	thread_basic_info_data_t		threadBasicInfo;
	policy_info_data_t				policyInfo;
	int								currentPriority;
	
	currentPriority = 0;
	
	machThread = pthread_mach_thread_np( pthread_self() );
	count = THREAD_BASIC_INFO_COUNT;
	err = thread_info( machThread, THREAD_BASIC_INFO, (thread_info_t) &threadBasicInfo, &count );
	require_noerr( err, exit );
	if( outPolicy ) *outPolicy = threadBasicInfo.policy;
	
	switch( threadBasicInfo.policy )
	{
		case POLICY_TIMESHARE:
			count = POLICY_TIMESHARE_INFO_COUNT;
			err = thread_info( machThread, THREAD_SCHED_TIMESHARE_INFO, (thread_info_t) &policyInfo.ts, &count );
			require_noerr( err, exit );
			currentPriority = (int) policyInfo.ts.base_priority;
			break;
			
		case POLICY_FIFO:
			count = POLICY_FIFO_INFO_COUNT;
			err = thread_info( machThread, THREAD_SCHED_FIFO_INFO, (thread_info_t) &policyInfo.fifo, &count );
			require_noerr( err, exit );
			currentPriority = (int) policyInfo.fifo.base_priority;
			break;
			
		case POLICY_RR:
			count = POLICY_RR_INFO_COUNT;
			err = thread_info( machThread, THREAD_SCHED_RR_INFO, (thread_info_t) &policyInfo.rr, &count );
			require_noerr( err, exit );
			currentPriority = (int) policyInfo.rr.base_priority;
			break;
			
		default:
			dlogassert( "Unknown Mach thread policy: %d", (int) threadBasicInfo.policy );
			err = kUnsupportedErr;
			goto exit;
	}
	
exit:
	if( outErr ) *outErr = err;
	return( currentPriority );
}
#endif

#if( TARGET_OS_WINDOWS )
//===========================================================================================================================
//	SetThreadName
//===========================================================================================================================

// See <http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx>.
#pragma pack(push,8)
typedef struct
{
	DWORD		dwType;		// Must be 0x1000.
	LPCSTR		szName;		// Pointer to name (in user addr space).
	DWORD		dwThreadID;	// Thread ID (-1=caller thread).
	DWORD		dwFlags;	// Reserved for future use, must be zero.
	
}	THREADNAME_INFO;
#pragma pack(pop)

OSStatus	SetThreadName( const char *inName )
{
	THREADNAME_INFO		info;
	
	info.dwType		= 0x1000;
	info.szName		= (LPCSTR) inName;
	info.dwThreadID	= GetCurrentThreadId();
	info.dwFlags	= 0;
	
	__try
	{
		RaiseException( 0x406D1388 /* MS_VC_EXCEPTION */, 0, sizeof( info ) / sizeof( ULONG_PTR ), (ULONG_PTR *) &info );
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
	}
	return( kNoErr );
}
#endif

//===========================================================================================================================
//	SetCurrentThreadPriority
//===========================================================================================================================

OSStatus	SetCurrentThreadPriority( int inPriority )
{
	OSStatus		err;
	
	if( inPriority == kThreadPriority_TimeConstraint )
	{
		#if( TARGET_MACH )
			thread_time_constraint_policy_data_t		policyInfo;
			mach_msg_type_number_t						policyCount;
			boolean_t									getDefault;
			
			getDefault = true;
			policyCount = THREAD_TIME_CONSTRAINT_POLICY_COUNT;
			err = thread_policy_get( mach_thread_self(), THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t) &policyInfo, 
				&policyCount, &getDefault );
			require_noerr( err, exit );
			
			err = thread_policy_set( mach_thread_self(), THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t) &policyInfo, 
				THREAD_TIME_CONSTRAINT_POLICY_COUNT );
			require_noerr( err, exit );	
		#else
			dlogassert( "Platform doesn't support time constraint threads" );
			err = kUnsupportedErr;
			goto exit;
		#endif
	}
#if( TARGET_OS_POSIX )
	else
	{
		int						policy;
		struct sched_param		sched;
		
		err = pthread_getschedparam( pthread_self(), &policy, &sched );
		require_noerr( err, exit );
		
		sched.sched_priority = inPriority;
		err = pthread_setschedparam( pthread_self(), SCHED_RR, &sched );
		require_noerr( err, exit );
	}
#elif( TARGET_OS_WINDOWS )
	else
	{
		BOOL		good;
		int			priority;
		
		if(      inPriority >= 64 )	priority = THREAD_PRIORITY_TIME_CRITICAL;
		else if( inPriority >= 52 )	priority = THREAD_PRIORITY_HIGHEST;
		else if( inPriority >= 32 )	priority = THREAD_PRIORITY_ABOVE_NORMAL;
		else if( inPriority >= 31 )	priority = THREAD_PRIORITY_NORMAL;
		else if( inPriority >= 11 )	priority = THREAD_PRIORITY_BELOW_NORMAL;
		else if( inPriority >= 1 )	priority = THREAD_PRIORITY_LOWEST;
		else						priority = THREAD_PRIORITY_IDLE;
		
		good = SetThreadPriority( GetCurrentThread(), priority );
		err = map_global_value_errno( good, good );
		require_noerr( err, exit );
	}
#else
	else
	{
		dlogassert( "Platform doesn't support setting thread priority" );
		err = kUnsupportedErr;
		goto exit;
	}
#endif
	
exit:
	return( err );
}
