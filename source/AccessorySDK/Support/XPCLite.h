/*
	File:    	XPCLite.h
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
*/

#ifndef	__XPCLite_h__
#define	__XPCLite_h__

#include "CommonServices.h"

#include CF_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
//	Mappings
//===========================================================================================================================

#if( XPC_LITE_ENABLED )
	#define XPC_ERROR_CONNECTION_INVALID				XPC_ERROR_CONNECTION_INVALID_compat
	#define XPC_ERROR_CONNECTION_INTERRUPTED			XPC_ERROR_CONNECTION_INTERRUPTED_compat
	#define XPC_ERROR_KEY_DESCRIPTION					XPC_ERROR_KEY_DESCRIPTION_compat
	
	#define XPC_TYPE_BOOL								XPC_TYPE_BOOL_compat
	#define XPC_TYPE_CONNECTION							XPC_TYPE_CONNECTION_compat
	#define XPC_TYPE_DICTIONARY							XPC_TYPE_DICTIONARY_compat
	#define XPC_TYPE_ERROR								XPC_TYPE_ERROR_compat
	
	#define xpc_connection_t							xpc_connection_t_compat
	#define xpc_handler_f								xpc_handler_f_compat
	#define xpc_object_t								xpc_object_t_compat
	#define xpc_type_t									xpc_type_t_compat
	
	#define xpc_bool_get_value							xpc_bool_get_value_compat
	#define xpc_copy_description						xpc_copy_description_compat
	#define xpc_dictionary_get_value					xpc_dictionary_get_value_compat
	#define xpc_dictionary_set_value					xpc_dictionary_set_value_compat
	#define xpc_get_type								xpc_get_type_compat
	#define xpc_retain									xpc_retain_compat
	#define xpc_release									xpc_release_compat
	
	#define xpc_dictionary_create						xpc_dictionary_create_compat
	#define xpc_dictionary_create_reply					xpc_dictionary_create_reply_compat
	#define xpc_dictionary_get_int64					xpc_dictionary_get_int64_compat
	#define xpc_dictionary_set_int64					xpc_dictionary_set_int64_compat
	#define xpc_dictionary_get_uint64					xpc_dictionary_get_uint64_compat
	#define xpc_dictionary_set_uint64					xpc_dictionary_set_uint64_compat
	#define xpc_dictionary_get_string					xpc_dictionary_get_string_compat
	#define xpc_dictionary_set_string					xpc_dictionary_set_string_compat
	
	#define xpc_connection_cancel						xpc_connection_cancel_compat
	#define xpc_connection_copy_entitlement_value		xpc_connection_copy_entitlement_value_compat
	#define xpc_connection_create_named_service			xpc_connection_create_named_service_compat
	#define xpc_connection_set_event_handler_f			xpc_connection_set_event_handler_f_compat
	#define xpc_connection_get_pid						xpc_connection_get_pid_compat
	#define xpc_connection_resume						xpc_connection_resume_compat
	#define xpc_connection_send_message					xpc_connection_send_message_compat
	#define xpc_connection_send_message_with_reply_f	xpc_connection_send_message_with_reply_f_compat
	#define xpc_connection_set_target_queue				xpc_connection_set_target_queue_compat
	
	#define _CFXPCCreateCFObjectFromXPCObject			_CFXPCCreateCFObjectFromXPCObject_compat
	#define _CFXPCCreateXPCObjectFromCFObject			_CFXPCCreateXPCObjectFromCFObject_compat
#endif

//===========================================================================================================================
//	Types
//===========================================================================================================================

typedef struct xpc_object_private_compat *			xpc_object_t_compat;
typedef struct xpc_type_private_compat *			xpc_type_t_compat;

typedef xpc_object_t_compat							xpc_connection_t_compat;

typedef void ( *xpc_handler_f_compat )( xpc_object_t_compat inObject, void *inContext );

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define XPC_ERROR_CONNECTION_INVALID_compat				( (xpc_object_t_compat) &_xpc_error_connection_invalid_compat )
extern const struct xpc_type_private_compat				_xpc_error_connection_invalid_compat;

#define XPC_ERROR_CONNECTION_INTERRUPTED_compat			( (xpc_object_t_compat) &_xpc_error_connection_interrupted_compat )
extern const struct xpc_type_private_compat				_xpc_error_connection_interrupted_compat;

#define XPC_ERROR_KEY_DESCRIPTION_compat				"desc"

#define XPC_TYPE_BOOL_compat							( &_xpc_type_bool_compat )
extern const struct xpc_type_private_compat				_xpc_type_bool_compat;

#define XPC_TYPE_CONNECTION_compat						( &_xpc_type_connection_compat )
extern const struct xpc_type_private_compat				_xpc_type_connection_compat;

#define XPC_TYPE_DICTIONARY_compat						( &_xpc_type_dictionary_compat )
extern const struct xpc_type_private_compat				_xpc_type_dictionary_compat;

#define XPC_TYPE_ERROR_compat							( &_xpc_type_error_compat )
extern const struct xpc_type_private_compat				_xpc_type_error_compat;

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

bool				xpc_bool_get_value_compat( xpc_object_t_compat inObj );
char *				xpc_copy_description_compat( xpc_object_t_compat inObj );
xpc_object_t_compat	xpc_dictionary_create_compat( const char * const *inKeys, const xpc_object_t_compat *inValues, size_t inCount );
xpc_object_t_compat	xpc_dictionary_create_reply_compat( xpc_object_t_compat inOrigin );
int64_t				xpc_dictionary_get_int64_compat( xpc_object_t_compat inDict, const char *inKey );
void				xpc_dictionary_set_int64_compat( xpc_object_t_compat inDict, const char *inKey, int64_t inValue );
uint64_t			xpc_dictionary_get_uint64_compat( xpc_object_t_compat inDict, const char *inKey );
void				xpc_dictionary_set_uint64_compat( xpc_object_t_compat inDict, const char *inKey, uint64_t inValue );
const char *		xpc_dictionary_get_string_compat( xpc_object_t_compat inDict, const char *inKey );
void				xpc_dictionary_set_string_compat( xpc_object_t_compat inDict, const char *inKey, const char *inStr );
xpc_object_t_compat	xpc_dictionary_get_value_compat( xpc_object_t_compat inDict, const char *inKey );
void				xpc_dictionary_set_value_compat( xpc_object_t inDict, const char *inKey, xpc_object_t inValue );
xpc_type_t_compat	xpc_get_type_compat( xpc_object_t_compat inObj );
void				xpc_retain_compat( xpc_object_t_compat inObj );
void				xpc_release_compat( xpc_object_t_compat inObj );

// Connections

#define XPC_CONNECTION_SERVICE_LISTENER			( 1U << 0 )
#define XPC_CONNECTION_SERVICE_PRIVILEGED		( 1U << 1 )

void	xpc_connection_cancel_compat( xpc_connection_t_compat inCnx );

xpc_object_t_compat	xpc_connection_copy_entitlement_value_compat( xpc_connection_t_compat inCnx, const char *inEntitlement );

xpc_connection_t_compat	xpc_connection_create_named_service_compat( const char *inName, dispatch_queue_t inQueue, uint64_t inFlags );

void
	xpc_connection_set_event_handler_f_compat( 
		xpc_connection_t_compat	inCnx, 
		xpc_handler_f_compat	inHandler, 
		void *					inContext );

#if( TARGET_OS_POSIX )
	pid_t	xpc_connection_get_pid_compat( xpc_connection_t_compat inCnx );
#endif

void	xpc_connection_resume_compat( xpc_connection_t_compat inCnx );
void	xpc_connection_send_message_compat( xpc_connection_t_compat inCnx, xpc_object_t_compat inMsg );
void
	xpc_connection_send_message_with_reply_f_compat( 
		xpc_connection_t		inCnx, 
		xpc_object_t			inMsg, 
		dispatch_queue_t		inQueue, 
		xpc_handler_f_compat	inHandler, 
		void *					inContext );
void	xpc_connection_set_target_queue_compat( xpc_connection_t_compat inCnx, dispatch_queue_t inQueue );

// Utils

CFTypeRef		_CFXPCCreateCFObjectFromXPCObject_compat( xpc_object_t inXPC );
xpc_object_t	_CFXPCCreateXPCObjectFromCFObject_compat( CFTypeRef inCF );

#ifdef __cplusplus
}
#endif

#endif // __XPCLite_h__
