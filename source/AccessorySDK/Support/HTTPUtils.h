/*
	File:    	HTTPUtils.h
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
	
	Copyright (C) 2007-2014 Apple Inc. All Rights Reserved.
*/

#ifndef	__HTTPUtils_h__
#define	__HTTPUtils_h__

#include <stdarg.h>
#include <time.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "URLUtils.h"

#if( TARGET_HAS_SOCKETS )
	#include "NetUtils.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == HTTPStatus ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@enum		HTTPStatus
	@abstract	HTTP-style status codes.
*/
typedef enum
{
	// Informational 1xx
	
	kHTTPStatus_Continue							= 100, // "Continue"
	kHTTPStatus_SwitchingProtocols					= 101, // "Switching Protocols"
	kHTTPStatus_Processing							= 102, // "Processing" (WebDAV - RFC 2518)
	kHTTPStatus_Checkpoint							= 103, // "Checkpoint"
	
	// Successful 2xx
	
	kHTTPStatus_OK									= 200, // "OK"
	kHTTPStatus_Created								= 201, // "Created"
	kHTTPStatus_Accepted							= 202, // "Accepted"
	kHTTPStatus_NonAuthoritativeInfo				= 203, // "Non-Authoritative Information"
	kHTTPStatus_NoContent							= 204, // "No Content"
	kHTTPStatus_ResetContent						= 205, // "Reset Content"
	kHTTPStatus_PartialContent						= 206, // "Partial Content"
	kHTTPStatus_MultiStatus							= 207, // "Multi-Status" (WebDAV)
	kHTTPStatus_AlreadyReported						= 208, // "Already Reported" (WebDAV)
	kHTTPStatus_ContentDifferent					= 210, // "Content Different" (WebDAV)
	kHTTPStatus_IMUsed								= 226, // "IM Used" (Delta Encoding, RFC 3229)
	kHTTPStatus_LowOnStorageSpace					= 250, // "Low on Storage Space"
	
	// Redirection 3xx
	
	kHTTPStatus_MultipleChoices						= 300, // "Multiple Choices"
	kHTTPStatus_MovePermanently						= 301, // "Moved Permanently"
	kHTTPStatus_Found								= 302, // "Found"
	kHTTPStatus_SeeOther							= 303, // "See Other"
	kHTTPStatus_NotModified							= 304, // "Not Modified"
	kHTTPStatus_UseProxy							= 305, // "Use Proxy"
	kHTTPStatus_SwitchProxy							= 306, // "Switch Proxy" -- No longer used.
	kHTTPStatus_TemporaryRedirect					= 307, // "Temporary Redirect"
	kHTTPStatus_PermanentRedirect					= 308, // "Permanent Redirect"
	kHTTPStatus_MovedLocation						= 330, // "Moved Location" (iCloud)
	kHTTPStatus_GoingAway							= 350, // "Going Away"
	kHTTPStatus_LoadBalancing						= 351, // "Load Balancing"
	
	// Client Error 4xx
	
	kHTTPStatus_BadRequest							= 400, // "Bad Request"
	kHTTPStatus_Unauthorized						= 401, // "Unauthorized"
	kHTTPStatus_PaymentRequired						= 402, // "Payment Required"
	kHTTPStatus_Forbidden							= 403, // "Forbidden"
	kHTTPStatus_NotFound							= 404, // "Not Found"
	kHTTPStatus_MethodNotAllowed					= 405, // "Method Not Allowed"
	kHTTPStatus_NotAcceptable						= 406, // "Not Acceptable"
	kHTTPStatus_ProxyAuthenticationRequired			= 407, // "Proxy Authentication Required"
	kHTTPStatus_RequestTimeout						= 408, // "Request Timeout"
	kHTTPStatus_Conflict							= 409, // "Conflict"
	kHTTPStatus_Gone								= 410, // "Gone"
	kHTTPStatus_LengthRequired						= 411, // "Length Required"
	kHTTPStatus_PreconditionFailed					= 412, // "Precondition Failed"
	kHTTPStatus_RequestEntityTooLarge				= 413, // "Request Entity Too Large"
	kHTTPStatus_RequestURITooLong					= 414, // "Request URI Too Long"
	kHTTPStatus_UnsupportedMediaType				= 415, // "Unsupported Media Type"
	kHTTPStatus_RequestedRangeNotSatisfiable		= 416, // "Requested Range Not Satisfiable"
	kHTTPStatus_ExpectationFailed					= 417, // "Expectation Failed"
	kHTTPStatus_IamATeapot							= 418, // "I'm a teapot" (April Fools joke)
	kHTTPStatus_AuthenticationTimeout				= 419, // "Authentication Timeout"
	kHTTPStatus_EnhanceYourCalm						= 420, // "Enhance Your Calm" (Twitter)
	kHTTPStatus_NotAuthoritative					= 421, // "Not Authoritative" (HTTP2)
	kHTTPStatus_UnprocessableEntity					= 422, // "Unprocessable Entity" (WebDAV)
	kHTTPStatus_Locked								= 423, // "Locked" (WebDAV)
	kHTTPStatus_FailedDependency					= 424, // "Failed Dependency" (WebDAV)
	kHTTPStatus_UnorderedCollection					= 425, // "Unordered Collection"
	kHTTPStatus_UpgradeRequired						= 426, // "Upgrade Required"
	kHTTPStatus_PreconditionRequired				= 428, // "Precondition Required"
	kHTTPStatus_TooManyRequests						= 429, // "Too Many Requests"
	kHTTPStatus_RequestHeaderFieldsTooLarge			= 431, // "Request Header Fields Too Large"
	kHTTPStatus_LoginTimeout						= 440, // "Login Timeout" (Microsoft)
	kHTTPStatus_NoResponse							= 444, // "No Response"
	kHTTPStatus_RetryWith							= 449, // "Retry With"
	kHTTPStatus_BlockedByParentalControls			= 450, // "Blocked by Parental Controls" (Microsoft)
	kHTTPStatus_ParameterNotUnderstood				= 451, // "Parameter Not Understood"
	kHTTPStatus_ConferenceNotFound					= 452, // "Conference Not Found"
	kHTTPStatus_NotEnoughBandwidth					= 453, // "Not Enough Bandwidth"
	kHTTPStatus_SessionNotFound						= 454, // "Session Not Found"
	kHTTPStatus_MethodNotValidInThisState			= 455, // "Method Not Valid In This State"
	kHTTPStatus_HeaderFieldNotValid					= 456, // "Header Field Not Valid"
	kHTTPStatus_InvalidRange						= 457, // "Invalid Range"
	kHTTPStatus_ParameterIsReadOnly					= 458, // "Parameter Is Read-Only"
	kHTTPStatus_AggregateOperationNotAllowed		= 459, // "Aggregate Operation Not Allowed"
	kHTTPStatus_OnlyAggregateOperationAllowed		= 460, // "Only Aggregate Operation Allowed"
	kHTTPStatus_UnsupportedTransport				= 461, // "Unsupported Transport"
	kHTTPStatus_DestinationUnreachable				= 462, // "Destination Unreachable"
	kHTTPStatus_DestinationProhibited				= 463, // "Destination Prohibited"
	kHTTPStatus_DataTransportNotReadyYet			= 464, // "Data Transport Not Ready Yet"
	kHTTPStatus_NotificationReasonUnknown			= 465, // "Notification Reason Unknown"
	kHTTPStatus_KeyManagementError					= 466, // "Key Management Error"
	kHTTPStatus_ConnectionAuthorizationRequired		= 470, // "Connection Authorization Required"
	kHTTPStatus_ConnectionCredentialsNotAccepted	= 471, // "Connection Credentials not accepted"
	kHTTPStatus_FailureToEstablishSecureConnection	= 472, // "Failure to establish secure connection"
	kHTTPStatus_InvalidCollblob						= 475, // "Invalid collblob"
	kHTTPStatus_ClientClosedRequest					= 499, // "Client Closed Request" (Nginx)
	
	// Server Error 5xx
	
	kHTTPStatus_InternalServerError					= 500, // "Internal Server Error"
	kHTTPStatus_NotImplemented						= 501, // "Not Implemented"
	kHTTPStatus_BadGatway							= 502, // "Bad Gateway"
	kHTTPStatus_ServiceUnavailable					= 503, // "Service Unavailable"
	kHTTPStatus_GatewayTimeout						= 504, // "Gateway Timeout"
	kHTTPStatus_VersionNotSupported					= 505, // "Version Not Supported"
	kHTTPStatus_VariantAlsoNegotiates				= 506, // "Variant Also Negotiates"
	kHTTPStatus_InsufficientStorage					= 507, // "Insufficient Storage" (WebDAV)
	kHTTPStatus_LoopDetected						= 508, // "Loop Detected" (WebDAV)
	kHTTPStatus_BandwidthLimitExceeded				= 509, // "Bandwidth Limit Exceeded"
	kHTTPStatus_NotExtended							= 510, // "Not Extended"
	kHTTPStatus_NetworkAuthenticationRequired		= 511, // "Network Authentication Required"
	kHTTPStatus_OriginError							= 520, // "Origin Error" (Cloudflare)
	kHTTPStatus_ConnectionTimedOut					= 522, // "Connection timed out"
	kHTTPStatus_ProxyDeclinedRequest				= 523, // "Proxy Declined Request" (Cloudflare)
	kHTTPStatus_TimeoutOccurred						= 524, // "Timeout occurred" (Cloudflare)
	kHTTPStatus_OptionNotSupported					= 551, // "Option Not Supported"
	kHTTPStatus_ProxyUnavailable					= 553, // "Proxy Unavailable"
	kHTTPStatus_NetworkReadTimeout					= 598, // "Network read timeout error"
	kHTTPStatus_NetworkConnectTimeout				= 599, // "Network connect timeout error"
	
	kHTTPStatus_End
	
}	HTTPStatus;

#define IsHTTPStatusCode( X )					( ( (X) >= 100 ) && ( (X) <= 599 ) )
#define IsHTTPStatusCode_Informational( X )		( ( (X) >= 100 ) && ( (X) <= 199 ) )
#define IsHTTPStatusCode_Success( X )			( ( (X) >= 200 ) && ( (X) <= 299 ) )
#define IsHTTPStatusCode_Redirect( X )			( ( (X) >= 300 ) && ( (X) <= 399 ) )
#define IsHTTPStatusCode_Client( X )			( ( (X) >= 400 ) && ( (X) <= 499 ) )
#define IsHTTPStatusCode_Server( X )			( ( (X) >= 500 ) && ( (X) <= 599 ) )

#define kHTTPErrorBase							200000
#define HTTPStatusToOSStatus( X )				( kHTTPErrorBase + (X) )
#define IsHTTPOSStatus( X )						( ( (X) >= 200100 ) && ( (X) <= 200599 ) )
#define IsHTTPOSStatus_Informational( X )		( ( (X) >= 200100 ) && ( (X) <= 200199 ) )
#define IsHTTPOSStatus_Success( X )				( ( (X) >= 200200 ) && ( (X) <= 200299 ) )
#define IsHTTPOSStatus_Redirect( X )			( ( (X) >= 200300 ) && ( (X) <= 200399 ) )
#define IsHTTPOSStatus_Client( X )				( ( (X) >= 200400 ) && ( (X) <= 200499 ) )
#define IsHTTPOSStatus_Server( X )				( ( (X) >= 200500 ) && ( (X) <= 200599 ) )

#if 0
#pragma mark -
#pragma mark == HTTPHeader ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	group		HTTP header fields
	@abstract	Constants for standard HTTP header fields.
*/
#define kHTTPHeader_Accept				"Accept"
#define kHTTPHeader_AcceptLanguage		"Accept-Language"
#define kHTTPHeader_AcceptRanges		"Accept-Ranges"
#define kHTTPHeader_Authorization		"Authorization"
#define kHTTPHeader_Connection			"Connection"
#define kHTTPHeader_ContentLength		"Content-Length"
#define kHTTPHeader_ContentRange		"Content-Range"
#define kHTTPHeader_ContentType			"Content-Type"
#define kHTTPHeader_CSeq				"CSeq"		// RTSP
#define kHTTPHeader_Date				"Date"
#define kHTTPHeader_Host				"Host"
#define kHTTPHeader_LastModified		"Last-Modified"
#define kHTTPHeader_Location			"Location"
#define kHTTPHeader_Public				"Public"
#define kHTTPHeader_Range				"Range"
#define kHTTPHeader_RTPInfo				"RTP-Info"	// RTSP
#define kHTTPHeader_Session				"Session"	// RTSP
#define kHTTPHeader_Server				"Server"
#define kHTTPHeader_TransferEncoding	"Transfer-Encoding"
#define kHTTPHeader_Transport			"Transport"
#define kHTTPHeader_Upgrade				"Upgrade"
#define kHTTPHeader_UserAgent			"User-Agent"
#define kHTTPHeader_WWWAuthenticate		"WWW-Authenticate"

#define kHTTPVersionString_1pt1			"HTTP/1.1"

//---------------------------------------------------------------------------------------------------------------------------
/*!	group		HTTPMethods
	@abstract	Constants for HTTP request methods.
*/
typedef enum
{
	kHTTPMethod_Unset	= 0, 
	kHTTPMethod_GET		= 1, 
	kHTTPMethod_POST	= 2, 
	kHTTPMethod_PUT		= 3, 
	kHTTPMethod_DELETE	= 4
	
}	HTTPMethod;

#define kHTTPMethodString_GET			"GET"
#define kHTTPMethodString_POST			"POST"
#define kHTTPMethodString_PUT			"PUT"
#define kHTTPMethodString_DELETE		"DELETE"

//---------------------------------------------------------------------------------------------------------------------------
/*!	group		HTTPHeader
	@abstract	Support for building and parsing HTTP headers.
*/
typedef struct
{
	char				buf[ 8192 ];		//! Buffer holding the start line and all headers.
	size_t				len;				//! Number of bytes in the header.
	const char *		extraDataPtr;		//! Ptr within "buf" for any extra data beyond the header.
	size_t				extraDataLen;		//! Length of any extra data beyond the header.
	
	const char *		methodPtr;			//! Request method (e.g. "GET"). "$" for interleaved binary data.
	size_t				methodLen;			//! Number of bytes in request method.
	HTTPMethod			method;				//! Parsed request method.
	const char *		urlPtr;				//! Request absolute or relative URL or empty if not a request.
	size_t				urlLen;				//! Number of bytes in URL.
	URLComponents		url;				//! Parsed URL components.
	const char *		protocolPtr;		//! Request or response protocol (e.g. "HTTP/1.1").
	size_t				protocolLen;		//! Number of bytes in protocol.
	int					statusCode;			//! Response status code (e.g. 200 for HTTP OK).
	const char *		reasonPhrasePtr;	//! Response reason phrase (e.g. "OK" for an HTTP 200 OK response).
	size_t				reasonPhraseLen;	//! Number of bytes in reason phrase.
	
	uint8_t				channelID;			//! Interleaved binary data channel ID. 0 for other message types.
	uint64_t			contentLength;		//! Number of bytes following the header. May be 0.
	Boolean				persistent;			//! true=Do not close the connection after this message.
	
	OSStatus			firstErr;			//! First error that occurred or kNoErr.
	
}	HTTPHeader;

OSStatus	HTTPHeader_InitRequest( HTTPHeader *inHeader, const char *inMethod, const char *inURL, const char *inProtocol );
OSStatus	HTTPHeader_InitRequestF( HTTPHeader *inHeader, const char *inProtocol, const char *inMethod, const char *inFormat, ... );
OSStatus
	HTTPHeader_InitRequestV( 
		HTTPHeader *	inHeader, 
		const char *	inProtocol, 
		const char *	inMethod, 
		const char *	inFormat, 
		va_list			inArgs );
OSStatus
	HTTPHeader_InitResponse(
		HTTPHeader *	inHeader,
		const char *	inProtocol,
		int				inStatusCode,
		const char *	inReasonPhrase );
OSStatus
	HTTPHeader_InitResponseEx( 
		HTTPHeader *	inHeader, 
		const char *	inProtocol, 
		int				inStatusCode, 
		const char *	inReasonPhrase, 
		OSStatus		inError );
OSStatus	HTTPHeader_Commit( HTTPHeader *inHeader );
OSStatus	HTTPHeader_Uncommit( HTTPHeader *inHeader );
OSStatus	HTTPHeader_SetField( HTTPHeader *inHeader, const char *inName, const char *inFormat, ... );
OSStatus	HTTPHeader_SetFieldV( HTTPHeader *inHeader, const char *inName, const char *inFormat, va_list inArgs );

OSStatus	HTTPHeader_Parse( HTTPHeader *ioHeader );
Boolean		HTTPHeader_Validate( HTTPHeader *inHeader );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPGetHeaderField
	@abstract	Parses a raw HTTP header to get a specific header field (if present).
	@discussion
	
	The header does not need to be null terminated, but must use either LF or CRLF line endings. The header may begin 
	with an HTTP-style start line (e.g. "GET / HTTP/1.0\r\n") and may end with a blank line, but neither is required.
	If inName is NULL, the next valid header field will be returned so it can be used for iteration.
*/
OSStatus
	HTTPGetHeaderField( 
		const char *	inHeaderPtr, 
		size_t			inHeaderLen, 
		const char *	inName, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		const char **	outNext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPIsChunked
	@abstract	Searches for a Transfer-Encoding header field and returns true if it describes a chunked encoding.
*/
Boolean	HTTPIsChunked( const char *inHeaderPtr, size_t inHeaderLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPMakeDateString
	@abstract	Build a date/time string as specified in RFC 1123 (e.g. "Sun, 18 Mar 2007 09:12:42 GMT").
	
	@param		inBuffer	Receives date string. Must be at least 32 bytes.
	
	@result		Ptr to input buffer.
*/
char *	HTTPMakeDateString( time_t inTime, char *inBuffer, size_t inMaxLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPParseByteRangeRequest
	@abstract	Parses an HTTP-style byte range request value (e.g. "bytes=1000-1999") and returns the start/end range.
*/
OSStatus	HTTPParseByteRangeRequest( const char *inStr, size_t inLen, int64_t *outStart, int64_t *outEnd );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPParseParameter
	@abstract	Parses an HTTP parameter.
*/
OSStatus
	HTTPParseParameter( 
		const void *	inSrc, 
		const void *	inEnd, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		char *			outDelimiter, 
		const char **	outSrc );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPParseRTPInfo
	@abstract	Parses an RTP-Info header field.
*/
OSStatus	HTTPParseRTPInfo( const char *inHeaderPtr, size_t inHeaderLen, uint16_t *outSeq, uint32_t *outTS );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPParseToken
	@abstract	Parses an HTTP token.
*/
OSStatus
	HTTPParseToken( 
		const void *	inSrc, 
		const void *	inEnd, 
		const char **	outTokenPtr, 
		const char **	outTokenEnd, 
		const char **	outSrc );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPScanFHeaderValue
	@abstract	Finds an HTTP header field by name and then applies a scanf-style format string on the found value.
	@discussion
	
	The header does not need to be null terminated, but must use either LF or CRLF line endings. The header may begin 
	with an HTTP-style start line (e.g. "GET / HTTP/1.0\r\n") and may end with a blank line, but neither is required.
	
	@result		The number of successfully parsed items or a negative error code if there is a failure.
*/
int	HTTPScanFHeaderValue( const char *inHeaderPtr, size_t inHeaderLen, const char *inName, const char *inFormat, ... );

#if 0
#pragma mark -
#pragma mark == HTTPAuthorization ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		HTTPTimedNonce
	@abstract	Support for generating and validating a nonce that's based on timestamps.
	@discussion
	
	HTTPMakeTimedNonce() should be given a nonce buffer that can hold 64 bytes to be safe.
*/
#define kHTTPTimedNonceETagPtr			"YjM5ODQ4MDE0MmI3YTI4ZmQ1MzdjNGIxMDVmNzYxMDg="
#define kHTTPTimedNonceETagLen			sizeof_string( kHTTPTimedNonceETagPtr )

#define kHTTPTimedNoncePrivateKeyPtr	"\xa3\xcf\x76\x3b\x1f\x24\x85\xa9\x64\x10\xbb\x9f\x73\x06\x58\x38"
#define kHTTPTimedNoncePrivateKeyLen	sizeof_string( kHTTPTimedNoncePrivateKeyPtr )

OSStatus
	HTTPMakeTimedNonce( 
		const char *	inETagPtr, 
		size_t			inETagLen, 
		const void *	inKeyPtr, 
		size_t			inKeyLen, 
		char *			inNonceBuf, 
		size_t			inNonceLen, 
		size_t *		outNonceLen );

OSStatus
	HTTPVerifyTimedNonce( 
		const char *	inNoncePtr, 
		size_t			inNonceLen, 
		unsigned int	inGoodSecs, 
		const char *	inETagPtr, 
		size_t			inETagLen, 
		const void *	inKeyPtr, 
		size_t			inKeyLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		HTTPAuthorization
	@abstract	Support for doing HTTP basic and digest authentication.
*/
typedef uint32_t	HTTPAuthorizationScheme;
#define kHTTPAuthorizationScheme_None		0			//! No authorization.
#define kHTTPAuthorizationScheme_Basic		( 1 << 0 )	//! Basic authorization is supported and/or required.
#define kHTTPAuthorizationScheme_Digest		( 1 << 1 )	//! Digest authorization is supported and/or required.

// HTTPClientAuthorizationInfo

typedef struct HTTPClientAuthorizationInfo *	HTTPClientAuthorizationInfoRef;
typedef struct HTTPClientAuthorizationInfo		HTTPClientAuthorizationInfo;
struct HTTPClientAuthorizationInfo
{
	// User code must fill in these fields.
	
	HTTPAuthorizationScheme		allowedAuthSchemes;	// Authorization schemes we'll allow.
	Boolean						uppercaseHex;		// true=Use uppercase hex digits to support old clients.
													// false=Use RFC 2617-compliant lowercase hex digits should be used.
	const char *				username;
	const char *				password;
	
	HTTPHeader *				requestHeader;		// Request header sent (or about to be sent).
	const HTTPHeader *			responseHeader;		// Last response from the server.
	
	// Fields managed by HTTPApplyAuthentication
	
	char *						algorithmStr;
	size_t						algorithmLen;
	
	char *						authSchemeStr;
	size_t						authSchemeLen;
	
	char *						domainStr;
	size_t						domainLen;
	
	char *						nonceStr;
	size_t						nonceLen;
	
	char *						opaqueStr;
	size_t						opaqueLen;
	
	char *						realmStr;
	size_t						realmLen;
	
	char *						qopStr;
	size_t						qopLen;
	
	char *						staleStr;
	size_t						staleLen;
	
	HTTPAuthorizationScheme		lastAuthScheme;		// Last authentication scheme used.
};

void		HTTPClientAuthorization_Init( HTTPClientAuthorizationInfoRef inAuthInfo );
void		HTTPClientAuthorization_Free( HTTPClientAuthorizationInfoRef inAuthInfo );
OSStatus	HTTPClientAuthorization_Apply( HTTPClientAuthorizationInfoRef inAuthInfo );

// HTTPAuthorizationInfo

typedef struct HTTPAuthorizationInfo *		HTTPAuthorizationInfoRef;
typedef struct HTTPAuthorizationInfo		HTTPAuthorizationInfo;

typedef HTTPStatus	( *HTTPAuthorization_CopyPasswordPtr )( HTTPAuthorizationInfoRef inInfo, char **outPassword );
typedef Boolean		( *HTTPAuthorization_IsValidNoncePtr )( HTTPAuthorizationInfoRef inInfo );

struct HTTPAuthorizationInfo
{
	// User code must fill in these fields.
	
	HTTPAuthorizationScheme				serverScheme;			// Authorization scheme used by the server.
	const char *						serverPassword;			// Optional password instead of copyPasswordFunction.
	const void *						serverTimedNonceKeyPtr;	// Optional timed nonce key instead of isValidNonceFunction.
	size_t								serverTimedNonceKeyLen; // ... and length.
	
	HTTPAuthorization_CopyPasswordPtr	copyPasswordFunction;	// Function to get the password.
	void *								copyPasswordContext;
	
	HTTPAuthorization_IsValidNoncePtr	isValidNonceFunction;	// Function to check if the nonce is valid.
	void *								isValidNonceContext;
	
	const char *						headerPtr;				// HTTP request headers (may include request line).
	size_t								headerLen;				// Length of the HTTP request headers.
	
	const char *						requestMethodPtr;		// HTTP request method (e.g. "POST").
	size_t								requestMethodLen;
	
	const char *						requestURLPtr;			// HTTP request URL.
	size_t								requestURLLen;
	
	// Fields parsed by HTTPVerifyAuthentication
	
	const char *						requestUsernamePtr;
	size_t								requestUsernameLen;
	
	const char *						requestPasswordPtr;
	size_t								requestPasswordLen;
	
	const char *						requestRealmPtr;
	size_t								requestRealmLen;
	
	const char *						requestNoncePtr;
	size_t								requestNonceLen;
	
	const char *						requestURIPtr;
	size_t								requestURILen;
	
	const char *						requestResponsePtr;
	size_t								requestResponseLen;
	
	const char *						requestAlgorithmPtr;
	size_t								requestAlgorithmLen;
	
	const char *						requestCNoncePtr;
	size_t								requestCNonceLen;
	
	const char *						requestOpaquePtr;
	size_t								requestOpaqueLen;
	
	const char *						requestQOPPtr;
	size_t								requestQOPLen;
	
	const char *						requestNCPtr;
	size_t								requestNCLen;
	
	// Results of the verification.
	
	Boolean								staleNonce;	// True if nonce was rejected because it's too old.
	Boolean								badMatch;	// True if password was checked and failed.
};

HTTPStatus	HTTPVerifyAuthorization( HTTPAuthorizationInfoRef ioAuthInfo );

#if 0
#pragma mark -
#pragma mark == Networking ==
#endif

#define kHTTPBonjourServiceType		"_http._tcp."

#if( TARGET_HAS_SOCKETS )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPDownloadFile
	@abstract	Downloads a file via HTTP into a malloc'd buffer.
*/
OSStatus	HTTPDownloadFile( const char *inURL, size_t inMaxSize, char **outData, size_t *outSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPReadHeader
	@abstract	Reads an HTTP header (start line, all header fields, etc.) into the specified buffer in a non-blocking manner.
	@discussion
	
	This may need to be called multiple times to read the entire header. Callers should initialize the inHeader->len to 0 
	then call this function each time data is readable, until it returns one of the following results:
	
	kNoErr:
		The header was read successfully into the buffer and fully parsed.
		Any extra data that was read, but not used is tracked by inHeader->extraDataPtr and inHeader->extraDataLen.
	
	EWOULDBLOCK:
		There was not enough data available to completely read the header.
		The caller should call this function again with same header when more data is available.
	
	kNoSpaceErr:
		The header could not be read completely because the buffer was not big enough. The header is probably bad 
		because most HTTP headers will fit in the default buffer in HTTPHeader. Or it could just be a huge header.
	
	Any other error:
		The header could not be read because of an error. The caller should treat this as a hard error and stop.
*/
OSStatus	HTTPReadHeader( HTTPHeader *inHeader, NetTransportRead_f inRead_f, void *inRead_ctx );
#define		SocketReadHTTPHeader( SOCK, HEADER )	HTTPReadHeader( (HEADER), SocketTransportRead, (void *)(intptr_t)(SOCK) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPReadHeader
	@abstract	Reads a line in a non-blocking manner.
	@discussion	See HTTPReadHeader for error reporting details.
	
	@param		outPtr		Receives a pointer within "inHeader->buf" to the line data.
	@param		outLen		Receives the number of bytes in the line (excludes line ending).
*/
OSStatus	HTTPReadLine( HTTPHeader *inHeader, NetTransportRead_f inRead_f, void *inRead_ctx, const char **outPtr, size_t *outLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NetSocket_HTTPReadHeader
	@abstract	Reads an HTTP header and parses it.
	@discussion
	
	This function blocks until it reads the complete header, a timeout occurs (if the timeout is >= 0), or the NetSocket
	is canceled. If it returns kNoErr, the HTTPHeader will be fully initialized and parsed. No pre-initialization needed.
	
	WARNING: This associates the HTTPHeader's buffer as part of the NetSocket structure so even after this call returns.
	This is used to handle leftover body data since the internal read may have read more than just the header.
	Subsequent calls to NetSocket_Read() will read from that leftover data until it is consumed.
*/
OSStatus	NetSocket_HTTPReadHeader( NetSocketRef inSock, HTTPHeader *inHeader, int32_t inTimeoutSecs );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPNetUtils_Test	
	@abstract	Unit test.
*/
#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	HTTPNetUtils_Test( void );
#endif

#endif // TARGET_HAS_SOCKETS

#if 0
#pragma mark -
#pragma mark == MIME Types ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Apple Plist MIME types
	@abstract	MIME types for Apple property lists (plists).
	@discussion
	
	There are several different MIME types being used for XML plists, but they each have issues:
		
		application/x-apple-plist
			- XML plists are readable by casual users so a text/ type seems better.
			- Can't tell the difference between binary and XML plists.
		
		application/x-plist
			- XML plists are readable by casual users so a text/ type seems better.
			- Too generic. Other things also called plists, but don't conform to the Apple plist DTD.
			- Can't tell the difference between binary and XML plists.

		application/xml
			- XML plists are readable by casual users so a text/ type seems better.
			- Too generic. Other things are also XML, but don't conform to the Apple plist DTD.
		
		text/plist
			- Should be prefixed with "x-" because it's not registered with IANA per RFC-2045.
			- Too generic. Other things also called plists, but don't conform to the Apple plist DTD.
		
		text/x-plist
			- Too generic. Other things also called plists, but don't conform to the Apple plist DTD.
	
	The following types were chosen to avoid the problems mentioned above:
	
		Binary plist:	application/x-apple-binary-plist
		XML plist:		text/x-apple-plist+xml
			- "x-" prefix to conform to RFC-2045 for experimental/unregistered MIME types.
			- Differentiates binary vs XML plists.
			- Follows the RFC-3023 convention of using +xml for XML media types.
*/
#define kMIMEType_AppleBinaryPlist		"application/x-apple-binary-plist"
#define kMIMEType_AppleXMLPlist			"text/x-apple-plist+xml"
#define MIMETypeIsPlist( PTR, LEN ) \
	( (Boolean)( ( strnicmpx( (PTR), (LEN), kMIMEType_AppleBinaryPlist ) == 0 ) || \
				 ( strnicmpx( (PTR), (LEN), kMIMEType_AppleXMLPlist )    == 0 ) ) )

#define kMIMEType_Binary				"application/octet-stream"
#define kMIMEType_DMAP					"application/x-dmap-tagged"
#define kMIMEType_Form					"application/x-www-form-urlencoded"
#define kMIMEType_HAP_JSON				"application/hap+json"
#define kMIMEType_ImagePrefix			"image/"
#define kMIMEType_JSON					"application/json"
#define kMIMEType_SDP					"application/sdp"
#define kMIMEType_TextHTML				"text/html"
#define kMIMEType_TextParameters		"text/parameters"
#define kMIMEType_TextPlain				"text/plain"
#define kMIMEType_TextXML				"text/xml"
#define kMIMEType_TLV8					"application/x-tlv8" // 8-bit type, 8-bit length, N-byte value.

#if 0
#pragma mark == Debugging ==
#endif

#if( LOGUTILS_ENABLED )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	LogHTTPMessage	
	@abstract	Logs an HTTP message.
*/
void
	LogHTTP( 
		LogCategory *		inRequestCategory, 
		LogCategory *		inResponseCategory, 
		const void *		inHeaderPtr, size_t inHeaderLen, 
		const void *		inBodyPtr,   size_t inBodyLen );

#if( DEBUG )
	#define dlog_http( REQUEST_CAT, RESPONSE_CAT, HEADER_PTR, HEADER_LEN, BODY_PTR, BODY_LEN )	\
		LogHTTP( ( REQUEST_CAT ), ( RESPONSE_CAT ), ( HEADER_PTR ), ( HEADER_LEN ), ( BODY_PTR ), ( BODY_LEN ) )
#else
	#define dlog_http( REQUEST_CAT, RESPONSE_CAT, HEADER_PTR, HEADER_LEN, BODY_PTR, BODY_LEN )	do {} while( 0 )
#endif

#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPUtils_Test	
	@abstract	Unit test.
*/
#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	HTTPUtils_Test( void );
#endif

#ifdef __cplusplus
}
#endif

#endif // __HTTPUtils_h__
