/*
	File:    	XPCUtils.c
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
	
	Copyright (C) 2011-2014 Apple Inc. All Rights Reserved.
*/

#include "XPCUtils.h"

#include <stdio.h>

#include "CommonServices.h"
#include "DebugServices.h"

#if( !XPC_LITE_ENABLED )
	#include <CoreFoundation/CFXPCBridge.h>
	#include <xpc/private.h>
#endif

ulog_define( XPCUtils, kLogLevelNotice, kLogFlags_Default, "XPCUtils", NULL );
#define xpc_utils_ulog( LEVEL, ... )		ulog( &log_category_from_name( XPCUtils ), (LEVEL), __VA_ARGS__ )

//===========================================================================================================================
//	xpc_connection_has_entitlement
//===========================================================================================================================

bool	xpc_connection_has_entitlement( xpc_connection_t inCnx, const char *inEntitlementName )
{
	bool				result = false;
	xpc_object_t		value;
	
	value = xpc_connection_copy_entitlement_value( inCnx, inEntitlementName );
	if( value )
	{
		if( ( xpc_get_type( value ) == XPC_TYPE_BOOL ) && xpc_bool_get_value( value ) )
		{
			result = true;
		}
		xpc_release( value );
	}
	return( result );
}

#if( COMPILER_HAS_BLOCKS )
//===========================================================================================================================
//	xpc_connection_set_event_handler_f
//===========================================================================================================================

void	xpc_connection_set_event_handler_f( xpc_connection_t inCnx, xpc_handler_f inHandler, void *inContext )
{
	xpc_connection_set_event_handler( inCnx, 
	^( xpc_object_t inEvent )
	{
		inHandler( inEvent, inContext );
	} );
}
#endif

#if( COMPILER_HAS_BLOCKS )
//===========================================================================================================================
//	xpc_connection_send_message_with_reply_f
//===========================================================================================================================

void
	xpc_connection_send_message_with_reply_f( 
		xpc_connection_t	inCnx, 
		xpc_object_t		inMsg, 
		dispatch_queue_t	inQueue, 
		xpc_handler_f		inHandler, 
		void *				inContext )
{
	xpc_connection_send_message_with_reply( inCnx, inMsg, inQueue, 
	^( xpc_object_t inReply )
	{
		inHandler( inReply, inContext );
	} );
}
#endif

//===========================================================================================================================
//	xpc_dictionary_copy_cf_object
//===========================================================================================================================

CFTypeRef	xpc_dictionary_copy_cf_object( xpc_object_t inDict, const char *inKey, OSStatus *outErr )
{
	OSStatus			err;
	xpc_object_t		xpc;
	CFTypeRef			cf;
	
	cf = NULL;
	xpc = xpc_dictionary_get_value( inDict, inKey );
	require_action_quiet( xpc, exit, err = kNotFoundErr );
	
	cf = _CFXPCCreateCFObjectFromXPCObject( xpc );
	require_action( cf, exit, err = kTypeErr );
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( cf ); 
}

//===========================================================================================================================
//	xpc_dictionary_set_cf_object
//===========================================================================================================================

OSStatus	xpc_dictionary_set_cf_object( xpc_object_t inDict, const char *inKey, CFTypeRef inCFObject )
{
	int					err;
	xpc_object_t		value;
	
	value = _CFXPCCreateXPCObjectFromCFObject( inCFObject );
	require_action( value, exit, err = kTypeErr );
	
	xpc_dictionary_set_value( inDict, inKey, value );
	xpc_release( value );
	err = kNoErr;
	
exit:
	return( err );
}

#if( COMPILER_HAS_BLOCKS )
//===========================================================================================================================
//	xpc_send_message_sync
//===========================================================================================================================

OSStatus	xpc_send_message_sync( const char *inService, uint64_t inFlags, uid_t inUID, xpc_object_t inMsg, xpc_object_t *outReply )
{
	OSStatus					err;
	char *						label	= NULL;
	dispatch_queue_t			queue	= NULL;
	xpc_connection_t			cnx		= NULL;
	dispatch_semaphore_t		sem		= NULL;
	__block xpc_object_t		reply	= NULL;
	
	asprintf( &label, "xpc_send_message_sync:%s", inService );
	require_action( label, exit, err = kNoMemoryErr );
	
	queue = dispatch_queue_create( label, NULL );
	free( label );
	require_action( queue, exit, err = kNoMemoryErr );
	
	cnx = xpc_connection_create_mach_service( inService, queue, inFlags );
	require_action( cnx, exit, err = kUnknownErr );
	xpc_connection_set_event_handler( cnx, ^( xpc_object_t inEvent ) { (void) inEvent; } );
	if( inUID != 0 ) xpc_connection_set_target_uid( cnx, inUID );
	xpc_connection_resume( cnx );
	
	sem = dispatch_semaphore_create( 0 );
	require_action( sem, exit, err = kNoMemoryErr );
	
	if( outReply )
	{
		xpc_connection_send_message_with_reply( cnx, inMsg, queue, 
		^( xpc_object_t inReply )
		{
			xpc_retain( inReply );
			reply = inReply;
			dispatch_semaphore_signal( sem );
		} );
		dispatch_semaphore_wait( sem, DISPATCH_TIME_FOREVER );
		require_action( xpc_get_type( reply ) != XPC_TYPE_ERROR, exit, err = kConnectionErr;
			xpc_utils_ulog( kLogLevelNotice, "### XPC service '%s' error: %{xpc}\n", inService, reply ) );
		*outReply = reply;
		reply = NULL;
	}
	else
	{
		xpc_connection_send_message( cnx, inMsg );
		xpc_connection_send_barrier( cnx, 
		^{ 
			dispatch_semaphore_signal( sem );
		} );
		dispatch_semaphore_wait( sem, DISPATCH_TIME_FOREVER );
	}
	err = kNoErr;
	
exit:
	if( cnx )
	{
		xpc_connection_cancel( cnx );
		xpc_release( cnx );
	}
	if( queue )	arc_safe_dispatch_release( queue );
	if( sem )	arc_safe_dispatch_release( sem );
	if( reply )	arc_safe_dispatch_release( reply );
	return( err );
}
#endif // COMPILER_HAS_BLOCKS
