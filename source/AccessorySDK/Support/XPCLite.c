/*
	File:    	XPCLite.c
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
	
	Copyright (C) 2014 Apple Inc. All Rights Reserved.
	
	XPC Lite -- A lightweight and portable implementation of Apple's XPC API.
*/

#include "XPCLite.h"

#include "CommonServices.h"
#include "DebugServices.h"

//===========================================================================================================================
//	Internals
//===========================================================================================================================

struct xpc_type_private_compat
{
	int		reserved;
};

const struct xpc_type_private_compat		_xpc_error_connection_invalid_compat		= { 0 };
const struct xpc_type_private_compat		_xpc_error_connection_interrupted_compat	= { 0 };
const struct xpc_type_private_compat		_xpc_type_bool_compat						= { 0 };
const struct xpc_type_private_compat		_xpc_type_connection_compat					= { 0 };
const struct xpc_type_private_compat		_xpc_type_dictionary_compat					= { 0 };
const struct xpc_type_private_compat		_xpc_type_error_compat						= { 0 };

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	xpc_bool_get_value_compat
//===========================================================================================================================

bool	xpc_bool_get_value_compat( xpc_object_t_compat inObj )
{
	(void) inObj;
	
	return( false );
}

//===========================================================================================================================
//	xpc_copy_description_compat
//===========================================================================================================================

char *	xpc_copy_description_compat( xpc_object_t_compat inObj )
{
	(void) inObj;
	
	return( NULL );
}

//===========================================================================================================================
//	xpc_dictionary_create_compat
//===========================================================================================================================

xpc_object_t_compat	xpc_dictionary_create_compat( const char * const *inKeys, const xpc_object_t_compat *inValues, size_t inCount )
{
	(void) inKeys;
	(void) inValues;
	(void) inCount;
	
	return( NULL );
}

//===========================================================================================================================
//	xpc_dictionary_create_reply_compat
//===========================================================================================================================

xpc_object_t_compat	xpc_dictionary_create_reply_compat( xpc_object_t_compat inOrigin )
{
	(void) inOrigin;
	
	return( NULL );
}

//===========================================================================================================================
//	xpc_dictionary_get_int64_compat
//===========================================================================================================================

int64_t	xpc_dictionary_get_int64_compat( xpc_object_t_compat inDict, const char *inKey )
{
	(void) inDict;
	(void) inKey;
	
	return( 0 );
}

//===========================================================================================================================
//	xpc_dictionary_set_int64_compat
//===========================================================================================================================

void	xpc_dictionary_set_int64_compat( xpc_object_t_compat inDict, const char *inKey, int64_t inValue )
{
	(void) inDict;
	(void) inKey;
	(void) inValue;
}

//===========================================================================================================================
//	xpc_dictionary_get_uint64_compat
//===========================================================================================================================

uint64_t	xpc_dictionary_get_uint64_compat( xpc_object_t_compat inDict, const char *inKey )
{
	(void) inDict;
	(void) inKey;
	
	return( 0 );
}

//===========================================================================================================================
//	xpc_dictionary_set_uint64_compat
//===========================================================================================================================

void	xpc_dictionary_set_uint64_compat( xpc_object_t_compat inDict, const char *inKey, uint64_t inValue )
{
	(void) inDict;
	(void) inKey;
	(void) inValue;
}

//===========================================================================================================================
//	xpc_dictionary_get_string_compat
//===========================================================================================================================

const char *	xpc_dictionary_get_string_compat( xpc_object_t_compat inDict, const char *inKey )
{
	(void) inDict;
	(void) inKey;
	
	return( NULL );
}

//===========================================================================================================================
//	xpc_dictionary_set_string_compat
//===========================================================================================================================

void	xpc_dictionary_set_string_compat( xpc_object_t_compat inDict, const char *inKey, const char *inStr )
{
	(void) inDict;
	(void) inKey;
	(void) inStr;
}

//===========================================================================================================================
//	xpc_dictionary_get_value
//===========================================================================================================================

xpc_object_t_compat	xpc_dictionary_get_value_compat( xpc_object_t_compat inDict, const char *inKey )
{
	(void) inDict;
	(void) inKey;
	
	return( NULL );
}

//===========================================================================================================================
//	xpc_dictionary_set_value_compat
//===========================================================================================================================

void	xpc_dictionary_set_value_compat( xpc_object_t inDict, const char *inKey, xpc_object_t inValue )
{
	(void) inDict;
	(void) inKey;
	(void) inValue;
}

//===========================================================================================================================
//	xpc_get_type_compat
//===========================================================================================================================

xpc_type_t_compat	xpc_get_type_compat( xpc_object_t_compat inObj )
{
	(void) inObj;
	
	return( NULL );
}

//===========================================================================================================================
//	xpc_retain_compat
//===========================================================================================================================

void	xpc_retain_compat( xpc_object_t_compat inObj )
{
	(void) inObj;
}

//===========================================================================================================================
//	xpc_release_compat
//===========================================================================================================================

void	xpc_release_compat( xpc_object_t_compat inObj )
{
	(void) inObj;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	xpc_connection_cancel_compat
//===========================================================================================================================

void	xpc_connection_cancel_compat( xpc_connection_t_compat inCnx )
{
	(void) inCnx;
}

//===========================================================================================================================
//	xpc_connection_copy_entitlement_value_compat
//===========================================================================================================================

xpc_object_t_compat	xpc_connection_copy_entitlement_value_compat( xpc_connection_t_compat inCnx, const char *inEntitlement )
{
	(void) inCnx;
	(void) inEntitlement;
	
	return( NULL );
}

//===========================================================================================================================
//	xpc_connection_create_named_service_compat
//===========================================================================================================================

xpc_connection_t_compat	xpc_connection_create_named_service_compat( const char *inName, dispatch_queue_t inQueue, uint64_t inFlags )
{
	(void) inName;
	(void) inQueue;
	(void) inFlags;
	
	return( NULL );
}

//===========================================================================================================================
//	xpc_connection_set_event_handler_f_compat
//===========================================================================================================================

void
	xpc_connection_set_event_handler_f_compat( 
		xpc_connection_t_compat	inCnx, 
		xpc_handler_f_compat	inHandler, 
		void *					inContext )
{
	(void) inCnx;
	(void) inHandler;
	(void) inContext;
}

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	xpc_connection_get_pid_compat
//===========================================================================================================================

pid_t	xpc_connection_get_pid_compat( xpc_connection_t_compat inCnx )
{
	(void) inCnx;
	
	return( 0 );
}
#endif

//===========================================================================================================================
//	xpc_connection_resume_compat
//===========================================================================================================================

void	xpc_connection_resume_compat( xpc_connection_t_compat inCnx )
{
	(void) inCnx;
}

//===========================================================================================================================
//	xpc_connection_send_message_compat
//===========================================================================================================================

void	xpc_connection_send_message_compat( xpc_connection_t_compat inCnx, xpc_object_t_compat inMsg )
{
	(void) inCnx;
	(void) inMsg;
}

//===========================================================================================================================
//	xpc_connection_send_message_with_reply_f_compat
//===========================================================================================================================

void
	xpc_connection_send_message_with_reply_f_compat( 
		xpc_connection_t		inCnx, 
		xpc_object_t			inMsg, 
		dispatch_queue_t		inQueue, 
		xpc_handler_f_compat	inHandler, 
		void *					inContext )
{
	(void) inCnx;
	(void) inMsg;
	(void) inQueue;
	(void) inHandler;
	(void) inContext;
}

//===========================================================================================================================
//	xpc_connection_set_target_queue_compat
//===========================================================================================================================

void	xpc_connection_set_target_queue_compat( xpc_connection_t_compat inCnx, dispatch_queue_t inQueue )
{
	(void) inCnx;
	(void) inQueue;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_CFXPCCreateCFObjectFromXPCObject_compat
//===========================================================================================================================

CFTypeRef	_CFXPCCreateCFObjectFromXPCObject_compat( xpc_object_t inXPC )
{
	(void) inXPC;
	
	return( NULL );
}

//===========================================================================================================================
//	_CFXPCCreateXPCObjectFromCFObject_compat
//===========================================================================================================================

xpc_object_t	_CFXPCCreateXPCObjectFromCFObject_compat( CFTypeRef inCF )
{
	(void) inCF;
	
	return( NULL );
}
