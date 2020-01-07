/*
	File:    	ChaCha20Poly1305.h
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
	
	Copyright (C) 2013-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__ChaCha20Poly1305_h__
#define	__ChaCha20Poly1305_h__

#include "CommonServices.h"
#include "DebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == chacha20 ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		chacha20
	@abstract	Encrypts/decrypts N bytes of data with a 32-byte key and 8-byte nonce starting from an 8-byte counter.
	@discussion	See DJB's page on ChaCha20 <http://cr.yp.to/chacha.html>.
	
	The 64x64 version uses a 64-bit nonce and a 64-bit block counter. This mode supports up to 2^64 blocks (2^70 bytes 
	since each block is 64 bytes).
	
	The 96x32 version uses a 96-bit nonce and a 32-bit block counter. The upper 32-bits of the nonce are needed by some
	protocols, such as TLS, for a sender ID. Other protocols normally set the upper 32 bits to zero. This mode supports
	up to 2^32 blocks (2^38 bytes since each block is 64 bytes).
	
	The upper 32 bits of the 96-bit nonce uses the same slot as the upper 32 bits of the counter in 64-bit nonce mode. 
	Therefore, they are equivalent if the upper 32 bits of the 96-bit nonce are 0 and no more than 2^32 blocks are processed.
*/
#define CHACHA_BLOCKBYTES		64

typedef struct
{
	uint32_t	state[ 16 ];
	uint8_t		buffer[ CHACHA_BLOCKBYTES ];
	size_t		leftover;
	
}	chacha20_state;

void	chacha20_init_64x64( chacha20_state *inState, const uint8_t inKey[ 32 ], const uint8_t inNonce[ 8 ], uint64_t inCounter );
void	chacha20_init_96x32( chacha20_state *inState, const uint8_t inKey[ 32 ], const uint8_t inNonce[ 12 ], uint32_t inCounter );
size_t	chacha20_update( chacha20_state *inState, const void *inSrc, size_t inLen, void *inDst );
size_t	chacha20_final( chacha20_state *inState, void *inDst );
void
	chacha20_all_64x64( 
		const uint8_t inKey[ 32 ], const uint8_t inNonce[ 8 ], uint64_t inCounter, 
		const void *inSrc, size_t inLen, void *inDst );
void
	chacha20_all_96x32( 
		const uint8_t inKey[ 32 ], const uint8_t inNonce[ 12 ], uint32_t inCounter, 
		const void *inSrc, size_t inLen, void *inDst );

#if 0
#pragma mark -
#pragma mark == poly1305 ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		poly1305
	@abstract	Generates a 16-byte Poly1305 Message Authentication Code from N bytes of data and a 32-byte key.
	@discussion	See DJB's paper on Poly1305 <http://cr.yp.to/mac.html> for details.
*/
typedef struct
{
	uint32_t		r0, r1, r2, r3, r4;
	uint32_t		s1, s2, s3, s4;
	uint32_t		h0, h1, h2, h3, h4;
	uint8_t			buf[ 16 ];
	size_t			buf_used;
	uint8_t			key[ 16 ];
	
}	poly1305_state;

void	poly1305_init( poly1305_state *inState, const uint8_t inKey[ 32 ] );
void	poly1305_update( poly1305_state *inState, const uint8_t *inPtr, size_t inLen );
void	poly1305_final( poly1305_state *inState, uint8_t outMAC[ 16 ] );
void	poly1305( uint8_t outMAC[ 16 ], const uint8_t *inPtr, size_t inLen, const uint8_t inKey[ 32 ] );

#if 0
#pragma mark -
#pragma mark == chacha20-poly1305 ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		chacha20-poly1305
	@abstract	Encrypts and authenticates or decrypts and verifies data.
	@discussion	See <https://tools.ietf.org/html/rfc7539> for details.
	
	The 64x64 version uses a 64-bit nonce and a 64-bit block counter. This mode supports up to 2^64 blocks (2^70 bytes 
	since each block is 64 bytes).
	
	The 96x32 version uses a 96-bit nonce and a 32-bit block counter. The upper 32-bits of the nonce are needed by some
	protocols, such as TLS, for a sender ID. Other protocols normally set the upper 32 bits to zero. This mode supports
	up to 2^32 blocks (2^38 bytes since each block is 64 bytes).
	
	The upper 32 bits of the 96-bit nonce uses the same slot as the upper 32 bits of the counter in 64-bit nonce mode. 
	Therefore, they are equivalent if the upper 32 bits of the 96-bit nonce are 0 and no more than 2^32 blocks are processed.
	
	@example
	
	// Encrypt a packet using the update API.
	
	chacha20_poly1305_state		state;
	uint8_t						key[ 32 ];
	uint8_t						nonce[ 8 ];
	size_t						n;
	uint8_t						ciphertext[ 4 ];
	uint8_t						authTag[ 16 ];
	uint8_t						plaintext[ 4 ];
	OSStatus					err;
	
	... fill in key with your key.
	... fill in nonce with your nonce.
	... nonce can be a simpler counter: 0, 1, 2, etc.
	... same nonce must never be used twice with the same key.
	chacha20_poly1305_init_64x64( &state, key, nonce );
	chacha20_poly1305_add_aad( &state, "header", 6 );
	n = chacha20_poly1305_encrypt( &state, "data", 4, ciphertext );
	n += chacha20_poly1305_final( &state, &ciphertext[ n ], authTag );
	... Send ciphertext and authTag to the peer.
	
	// Decrypt the packet using the update API.
	
	chacha20_poly1305_init_64x64( &state, key, nonce );
	chacha20_poly1305_add_aad( &state, "header", 6 );
	n = chacha20_poly1305_encrypt( &state, ciphertext, 4, plaintext );
	n += chacha20_poly1305_verify( &state, &plaintext[ n ], authTag, &err );
	if( err )	... Reject packet.
	else		... Use plaintext.
*/
typedef struct
{
	chacha20_state		chacha20;
	poly1305_state		poly1305;
	size_t				aadLen;
	size_t				dataLen;
	Boolean				padded;
	
}	chacha20_poly1305_state;

void	chacha20_poly1305_init_64x64( chacha20_poly1305_state *inState, const uint8_t inKey[ 32 ], const uint8_t inNonce[ 8 ] );
void	chacha20_poly1305_init_96x32( chacha20_poly1305_state *inState, const uint8_t inKey[ 32 ], const uint8_t inNonce[ 12 ] );
void	chacha20_poly1305_add_aad( chacha20_poly1305_state *inState, const void *inSrc, size_t inLen );
size_t	chacha20_poly1305_encrypt( chacha20_poly1305_state *inState, const void *inSrc, size_t inLen, void *inDst );
size_t	chacha20_poly1305_decrypt( chacha20_poly1305_state *inState, const void *inSrc, size_t inLen, void *inDst );
size_t	chacha20_poly1305_final( chacha20_poly1305_state *inState, void *inDst, uint8_t outAuthTag[ 16 ] );
size_t	chacha20_poly1305_verify( chacha20_poly1305_state *inState, void *inDst, const uint8_t inAuthTag[ 16 ], OSStatus *outErr );

void
	chacha20_poly1305_encrypt_all_64x64( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t	inNonce[ 8 ], 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inPlaintextPtr, 
		size_t			inPlaintextLen, 
		void *			inCiphertextBuf, 
		uint8_t			outAuthTag[ 16 ] );
void
	chacha20_poly1305_encrypt_all_96x32( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t	inNonce[ 12 ], 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inPlaintextPtr, 
		size_t			inPlaintextLen, 
		void *			inCiphertextBuf, 
		uint8_t			outAuthTag[ 16 ] );

OSStatus
	chacha20_poly1305_decrypt_all_64x64( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t	inNonce[ 8 ], 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inCiphertextPtr, 
		size_t			inCiphertextLen, 
		void *			inPlaintextBuf, 
		const uint8_t	inAuthTag[ 16 ] );
OSStatus
	chacha20_poly1305_decrypt_all_96x32( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t	inNonce[ 12 ], 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inCiphertextPtr, 
		size_t			inCiphertextLen, 
		void *			inPlaintextBuf, 
		const uint8_t	inAuthTag[ 16 ] );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ChaCha20Poly1305Test
	@abstract	Unit test.
*/
OSStatus	ChaCha20Poly1305Test( int inPrint, int inPerf );

#ifdef __cplusplus
}
#endif

#endif // __ChaCha20Poly1305_h__
