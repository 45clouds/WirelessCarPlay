/*
	File:    	AESUtils.c
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
	
	Copyright (C) 2007-2015 Apple Inc. All Rights Reserved.
*/

#include "AESUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"


#if( !AES_UTILS_USE_COMMON_CRYPTO && !AES_UTILS_USE_GLADMAN_AES && !AES_UTILS_USE_WICED && !AES_UTILS_USE_WINDOWS_API && \
	TARGET_NO_OPENSSL )
static
void AES_cbc_encrypt(const unsigned char *in, unsigned char *out,
		     const unsigned long length, const AES_KEY *key,
		     unsigned char *ivec, const int enc);
#endif

#if( AES_UTILS_USE_WINDOWS_API )
typedef struct
{
	BLOBHEADER		hdr;
	DWORD			len;
	uint8_t			key[ 16 ];
	
}	AESKeyBlob;

static OSStatus	_CreateWindowsCryptoAPIContext( HCRYPTPROV *outProvider, HCRYPTKEY *outKey, const uint8_t *inKey, DWORD inMode );
#endif

//===========================================================================================================================
//	AES_CTR_Init
//===========================================================================================================================

OSStatus
	AES_CTR_Init( 
		AES_CTR_Context *	inContext, 
		const uint8_t		inKey[ kAES_CTR_Size ], 
		const uint8_t		inNonce[ kAES_CTR_Size ] )
{
#if( AES_UTILS_USE_COMMON_CRYPTO )
	OSStatus		err;
	
	inContext->cryptor = NULL;
	err = CCCryptorCreate( kCCEncrypt, kCCAlgorithmAES, kCCOptionECBMode, inKey, kAES_CTR_Size, NULL, 
		&inContext->cryptor );
	check_noerr( err );
	if( err ) return( err );
#elif( AES_UTILS_USE_GLADMAN_AES )
	aes_init();
	aes_encrypt_key128( inKey, &inContext->ctx );
#elif( AES_UTILS_USE_WICED )
	aes_setkey_enc( &inContext->ctx, (unsigned char *) inKey, kAES_CTR_Size * 8 );
#elif( AES_UTILS_USE_WINDOWS_API )
	OSStatus		err;
	
	err = _CreateWindowsCryptoAPIContext( &inContext->provider, &inContext->key, inKey, CRYPT_MODE_ECB );
	check_noerr( err );
	if( err ) return( err );
#else
	AES_set_encrypt_key( inKey, kAES_CTR_Size * 8, &inContext->key );
#endif
	memcpy( inContext->ctr, inNonce, kAES_CTR_Size );
	inContext->used = 0;
	return( kNoErr );
}

//===========================================================================================================================
//	AES_CTR_Update
//===========================================================================================================================

OSStatus	AES_CTR_Update( AES_CTR_Context *inContext, const void *inSrc, size_t inLen, void *inDst )
{
	OSStatus			err;
	const uint8_t *		src;
	uint8_t *			dst;
	uint8_t *			buf;
	size_t				used;
	size_t				i;
	
	// inSrc and inDst may be the same, but otherwise, the buffers must not overlap.
	
#if( DEBUG )
	if( inSrc != inDst ) check_ptr_overlap( inSrc, inLen, inDst, inLen );
#endif
	
	src = (const uint8_t *) inSrc;
	dst = (uint8_t *) inDst;
	
	// If there's any buffered key material from a previous block then use that first.
	
	buf  = inContext->buf;
	used = inContext->used;
	while( ( inLen > 0 ) && ( used != 0 ) ) 
	{
		*dst++ = *src++ ^ buf[ used++ ];
		used %= kAES_CTR_Size;
		inLen -= 1;
	}
	inContext->used = used;
	
	// Process whole blocks.
	
	while( inLen >= kAES_CTR_Size )
	{
		#if( AES_UTILS_USE_COMMON_CRYPTO )
			err = CCCryptorUpdate( inContext->cryptor, inContext->ctr, kAES_CTR_Size, buf, kAES_CTR_Size, &i );
			require_noerr( err, exit );
			require_action( i == kAES_CTR_Size, exit, err = kSizeErr );
		#elif( AES_UTILS_USE_GLADMAN_AES )
			aes_ecb_encrypt( inContext->ctr, buf, kAES_CTR_Size, &inContext->ctx );
		#elif( AES_UTILS_USE_WICED )
			aes_crypt_ecb( &inContext->ctx, AES_ENCRYPT, inContext->ctr, buf );
		#elif( AES_UTILS_USE_WINDOWS_API )
			BOOL		good;
			DWORD		len;
			
			len = kAES_CTR_Size;
			memcpy( buf, inContext->ctr, kAES_CTR_Size );
			good = CryptEncrypt( inContext->key, 0, false, 0, buf, &len, len );
			err = map_global_value_errno( good, good );
			require_noerr( err, exit );
		#else
			AES_encrypt( inContext->ctr, buf, &inContext->key );
		#endif
		BigEndianIntegerIncrement( inContext->ctr, kAES_CTR_Size );
		
		for( i = 0; i < kAES_CTR_Size; ++i )
		{
			dst[ i ] = src[ i ] ^ buf[ i ];
		}
		src   += kAES_CTR_Size;
		dst   += kAES_CTR_Size;
		inLen -= kAES_CTR_Size;
	}
	
	// Process any trailing sub-block bytes. Extra key material is buffered for next time.
	
	if( inLen > 0 )
	{
		#if( AES_UTILS_USE_COMMON_CRYPTO )
			err = CCCryptorUpdate( inContext->cryptor, inContext->ctr, kAES_CTR_Size, buf, kAES_CTR_Size, &i );
			require_noerr( err, exit );
			require_action( i == kAES_CTR_Size, exit, err = kSizeErr );
		#elif( AES_UTILS_USE_GLADMAN_AES )
			aes_ecb_encrypt( inContext->ctr, buf, kAES_CTR_Size, &inContext->ctx );
		#elif( AES_UTILS_USE_WICED )
			aes_crypt_ecb( &inContext->ctx, AES_ENCRYPT, inContext->ctr, buf );
		#elif( AES_UTILS_USE_WINDOWS_API )
			BOOL		good;
			DWORD		len;
			
			len = kAES_CTR_Size;
			memcpy( buf, inContext->ctr, kAES_CTR_Size );
			good = CryptEncrypt( inContext->key, 0, false, 0, buf, &len, len );
			err = map_global_value_errno( good, good );
			require_noerr( err, exit );
		#else
			AES_encrypt( inContext->ctr, buf, &inContext->key );
		#endif
		BigEndianIntegerIncrement( inContext->ctr, kAES_CTR_Size );
		
		for( i = 0; i < inLen; ++i )
		{
			*dst++ = *src++ ^ buf[ used++ ];
		}
		inContext->used = used;
	}
	err = kNoErr;
	
#if( AES_UTILS_USE_COMMON_CRYPTO || AES_UTILS_USE_WINDOWS_API )
exit:
#endif
	return( err );
}

//===========================================================================================================================
//	AES_CTR_Final
//===========================================================================================================================

void	AES_CTR_Final( AES_CTR_Context *inContext )
{
#if( AES_UTILS_USE_COMMON_CRYPTO )
	if( inContext->cryptor ) CCCryptorRelease( inContext->cryptor );
#elif( AES_UTILS_USE_WINDOWS_API )
	if( inContext->key )		CryptDestroyKey( inContext->key );
	if( inContext->provider )	CryptReleaseContext( inContext->provider, 0 );
#endif
	MemZeroSecure( inContext, sizeof( *inContext ) );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AES_CBCFrame_Init
//===========================================================================================================================

OSStatus
	AES_CBCFrame_Init( 
		AES_CBCFrame_Context *	inContext, 
		const uint8_t			inKey[ kAES_CBCFrame_Size ], 
		const uint8_t			inIV[ kAES_CBCFrame_Size ], 
		Boolean					inEncrypt )
{
#if( AES_UTILS_USE_COMMON_CRYPTO )
	OSStatus		err;
	
	inContext->cryptor = NULL;
	err = CCCryptorCreate( inEncrypt ? kCCEncrypt : kCCDecrypt, kCCAlgorithmAES, 0, inKey, kAES_CTR_Size, 
		NULL, &inContext->cryptor );
	check_noerr( err );
	if( err ) return( err );
#elif( AES_UTILS_USE_GLADMAN_AES )
	aes_init();
	if( inEncrypt ) aes_encrypt_key128( inKey, &inContext->ctx.encrypt );
	else			aes_decrypt_key128( inKey, &inContext->ctx.decrypt );
	inContext->encrypt = inEncrypt;
#elif( AES_UTILS_USE_WICED )
	if( inEncrypt ) aes_setkey_enc( &inContext->ctx, (unsigned char *) inKey, kAES_CBCFrame_Size * 8 );
	else			aes_setkey_dec( &inContext->ctx, (unsigned char *) inKey, kAES_CBCFrame_Size * 8 );
	inContext->encrypt = inEncrypt;
#elif( AES_UTILS_USE_WINDOWS_API )
	OSStatus		err;
	
	err = _CreateWindowsCryptoAPIContext( &inContext->provider, &inContext->key, inKey, CRYPT_MODE_CBC );
	check_noerr( err );
	if( err ) return( err );
	
	inContext->encrypt = inEncrypt;
#else
	if( inEncrypt ) AES_set_encrypt_key( inKey, kAES_CBCFrame_Size * 8, &inContext->key );
	else			AES_set_decrypt_key( inKey, kAES_CBCFrame_Size * 8, &inContext->key );
	inContext->mode = inEncrypt ? AES_ENCRYPT : AES_DECRYPT;
#endif
	memcpy( inContext->iv, inIV, kAES_CBCFrame_Size );
	return( kNoErr );
}

//===========================================================================================================================
//	AES_CBCFrame_Update
//===========================================================================================================================

OSStatus	AES_CBCFrame_Update( AES_CBCFrame_Context *inContext, const void *inSrc, size_t inSrcLen, void *inDst )
{
	OSStatus			err;
	const uint8_t *		src;
	const uint8_t *		end;
	uint8_t *			dst;
	size_t				len;
	
	src = (const uint8_t *) inSrc;
	end = src + inSrcLen;
	dst = (uint8_t *) inDst;
	
	// Process whole blocks.
	
	len = inSrcLen & ~( (size_t)( kAES_CBCFrame_Size - 1 ) );
	if( len > 0 )
	{
		#if( AES_UTILS_USE_COMMON_CRYPTO )
			err = CCCryptorReset(  inContext->cryptor, inContext->iv );
			require_noerr( err, exit );
			
			err = CCCryptorUpdate( inContext->cryptor, src, len, dst, len, &len );
			require_noerr( err, exit );
		#elif( AES_UTILS_USE_GLADMAN_AES )
			uint8_t		iv[ kAES_CBCFrame_Size ];
			
			memcpy( iv, inContext->iv, kAES_CBCFrame_Size ); // Use local copy so original IV is not changed.
			if( inContext->encrypt )	aes_cbc_encrypt( src, dst, (int) len, iv, &inContext->ctx.encrypt );
			else						aes_cbc_decrypt( src, dst, (int) len, iv, &inContext->ctx.decrypt );
		#elif( AES_UTILS_USE_WICED )
			uint8_t		iv[ kAES_CBCFrame_Size ];
			
			memcpy( iv, inContext->iv, kAES_CBCFrame_Size ); // Use local copy so original IV is not changed.
			if( inContext->encrypt )	aes_crypt_cbc( &inContext->ctx, AES_ENCRYPT, len, iv, (unsigned char *) src, dst );
			else						aes_crypt_cbc( &inContext->ctx, AES_DECRYPT, len, iv, (unsigned char *) src, dst );
		#elif( AES_UTILS_USE_WINDOWS_API )
			BOOL		good;
			DWORD		dwLen;
			
			good = CryptSetKeyParam( inContext->key, KP_IV, inContext->iv, 0 );
			err = map_global_value_errno( good, good );
			require_noerr( err, exit );
			
			if( src != dst ) memcpy( dst, src, len );
			dwLen = (DWORD) len;
			if( inContext->encrypt )	good = CryptEncrypt( inContext->key, 0, false, 0, dst, &dwLen, dwLen );
			else						good = CryptDecrypt( inContext->key, 0, false, 0, dst, &dwLen );
			err = map_global_value_errno( good, good );
			require_noerr( err, exit );
		#else
			uint8_t		iv[ kAES_CBCFrame_Size ];
			
			memcpy( iv, inContext->iv, kAES_CBCFrame_Size ); // Use local copy so original IV is not changed.
			AES_cbc_encrypt( src, dst, (unsigned long) len, &inContext->key, iv, inContext->mode );
		#endif
		src += len;
		dst += len;
	}
	
	// The remaining bytes are just copied unencrypted.
	
	while( src != end ) *dst++ = *src++;
	err = kNoErr;
	
#if( AES_UTILS_USE_COMMON_CRYPTO || AES_UTILS_USE_WINDOWS_API )
exit:
#endif
	return( err );
}

//===========================================================================================================================
//	AES_CBCFrame_Update2
//===========================================================================================================================

OSStatus
	AES_CBCFrame_Update2( 
		AES_CBCFrame_Context *	inContext, 
		const void *			inSrc1, 
		size_t					inLen1, 
		const void *			inSrc2, 
		size_t					inLen2, 
		void *					inDst )
{
	const uint8_t *		src1 = (const uint8_t *) inSrc1;
	const uint8_t *		end1 = src1 + inLen1;
	const uint8_t *		src2 = (const uint8_t *) inSrc2;
	const uint8_t *		end2 = src2 + inLen2;
	uint8_t *			dst  = (uint8_t *) inDst;
	OSStatus			err;
	size_t				len;
	size_t				i;
#if( !AES_UTILS_USE_COMMON_CRYPTO && !AES_UTILS_USE_WINDOWS_API )
	uint8_t				iv[ kAES_CBCFrame_Size ];
#endif
	
#if( AES_UTILS_USE_COMMON_CRYPTO )
	if( ( inLen1 + inLen2 ) >= kAES_CBCFrame_Size )
	{
		err = CCCryptorReset(  inContext->cryptor, inContext->iv );
		require_noerr( err, exit );
	}
#elif( AES_UTILS_USE_WINDOWS_API )
	if( ( inLen1 + inLen2 ) >= kAES_CBCFrame_Size )
	{
		BOOL		good;
		
		good = CryptSetKeyParam( inContext->key, KP_IV, inContext->iv, 0 );
		err = map_global_value_errno( good, good );
		require_noerr( err, exit );
	}
#else
	memcpy( iv, inContext->iv, kAES_CBCFrame_Size ); // Use local copy so original IV is not changed.
#endif
	
	// Process all whole blocks from buffer 1.
	
	len = inLen1 & ~( (size_t)( kAES_CBCFrame_Size - 1 ) );
	if( len > 0 )
	{
		#if( AES_UTILS_USE_COMMON_CRYPTO )
			err = CCCryptorUpdate( inContext->cryptor, src1, len, dst, len, &len );
			require_noerr( err, exit );
		#elif( AES_UTILS_USE_GLADMAN_AES )
			if( inContext->encrypt )	aes_cbc_encrypt( src1, dst, (int) len, iv, &inContext->ctx.encrypt );
			else						aes_cbc_decrypt( src1, dst, (int) len, iv, &inContext->ctx.decrypt );
		#elif( AES_UTILS_USE_WICED )
			if( inContext->encrypt )	aes_crypt_cbc( &inContext->ctx, AES_ENCRYPT, len, iv, (unsigned char *) src1, dst );
			else						aes_crypt_cbc( &inContext->ctx, AES_DECRYPT, len, iv, (unsigned char *) src1, dst );
		#elif( AES_UTILS_USE_WINDOWS_API )
			BOOL		good;
			DWORD		dwLen;
			
			if( src1 != dst ) memcpy( dst, src1, len );
			dwLen = (DWORD) len;
			if( inContext->encrypt )	good = CryptEncrypt( inContext->key, 0, false, 0, dst, &dwLen, dwLen );
			else						good = CryptDecrypt( inContext->key, 0, false, 0, dst, &dwLen );
			err = map_global_value_errno( good, good );
			require_noerr( err, exit );
		#else
			AES_cbc_encrypt( src1, dst, (unsigned long) len, &inContext->key, iv, inContext->mode );
		#endif
		src1 += len;
		dst  += len;
	}
	
	// If there are any partial block bytes in buffer 1 and enough bytes in buffer 2 to fill a 
	// block then combine them into a temporary buffer and encrypt it.
	
	if( ( src1 != end1 ) && ( ( ( end1 - src1 ) + ( end2 - src2 ) ) >= kAES_CBCFrame_Size ) )
	{
		uint8_t		buf[ kAES_CBCFrame_Size ];
		
		for( i = 0; src1 != end1; ++i )
		{
			buf[ i ] = *src1++;
		}
		for( ; ( i < kAES_CBCFrame_Size ) && ( src2 != end2 ); ++i )
		{
			buf[ i ] = *src2++;
		}
		#if( AES_UTILS_USE_COMMON_CRYPTO )
			err = CCCryptorUpdate( inContext->cryptor, buf, i, dst, i, &i );
			require_noerr( err, exit );
		#elif( AES_UTILS_USE_GLADMAN_AES )
			if( inContext->encrypt )	aes_cbc_encrypt( buf, dst, (int) i, iv, &inContext->ctx.encrypt );
			else						aes_cbc_decrypt( buf, dst, (int) i, iv, &inContext->ctx.decrypt );
		#elif( AES_UTILS_USE_WICED )
			if( inContext->encrypt )	aes_crypt_cbc( &inContext->ctx, AES_ENCRYPT, i, iv, buf, dst );
			else						aes_crypt_cbc( &inContext->ctx, AES_DECRYPT, i, iv, buf, dst );
		#elif( AES_UTILS_USE_WINDOWS_API )
		{
			BOOL		good;
			DWORD		dwLen;
			
			memcpy( dst, buf, i );
			dwLen = (DWORD) i;
			if( inContext->encrypt )	good = CryptEncrypt( inContext->key, 0, false, 0, dst, &dwLen, dwLen );
			else						good = CryptDecrypt( inContext->key, 0, false, 0, dst, &dwLen );
			err = map_global_value_errno( good, good );
			require_noerr( err, exit );
		}
		#else
			AES_cbc_encrypt( buf, dst, (unsigned long) i, &inContext->key, iv, inContext->mode );
		#endif
		dst += i;
	}
	
	// Process any remaining whole blocks in buffer 2.
	
	len = ( (size_t)( end2 - src2 ) ) & ~( (size_t)( kAES_CBCFrame_Size - 1 ) );
	if( len > 0 )
	{
		#if( AES_UTILS_USE_COMMON_CRYPTO )
			err = CCCryptorUpdate( inContext->cryptor, src2, len, dst, len, &len );
			require_noerr( err, exit );
		#elif( AES_UTILS_USE_GLADMAN_AES )
			if( inContext->encrypt )	aes_cbc_encrypt( src2, dst, (int) len, iv, &inContext->ctx.encrypt );
			else						aes_cbc_decrypt( src2, dst, (int) len, iv, &inContext->ctx.decrypt );
		#elif( AES_UTILS_USE_WICED )
			if( inContext->encrypt )	aes_crypt_cbc( &inContext->ctx, AES_ENCRYPT, len, iv, (unsigned char *) src2, dst );
			else						aes_crypt_cbc( &inContext->ctx, AES_DECRYPT, len, iv, (unsigned char *) src2, dst );
		#elif( AES_UTILS_USE_WINDOWS_API )
			BOOL		good;
			DWORD		dwLen;
			
			if( src2 != dst ) memcpy( dst, src2, len );
			dwLen = len;
			if( inContext->encrypt )	good = CryptEncrypt( inContext->key, 0, false, 0, dst, &dwLen, dwLen );
			else						good = CryptDecrypt( inContext->key, 0, false, 0, dst, &dwLen );
			err = map_global_value_errno( good, good );
			require_noerr( err, exit );
		#else
			AES_cbc_encrypt( src2, dst, (unsigned long) len, &inContext->key, iv, inContext->mode );
		#endif
		src2 += len;
		dst  += len;
	}
	
	// Any remaining bytes are just copied unencrypted.
	
	while( src1 != end1 ) *dst++ = *src1++;
	while( src2 != end2 ) *dst++ = *src2++;
	err = kNoErr;
	
#if( AES_UTILS_USE_COMMON_CRYPTO || AES_UTILS_USE_WINDOWS_API )
exit:
#endif
	return( err );
}

//===========================================================================================================================
//	AES_CBCFrame_Final
//===========================================================================================================================

void	AES_CBCFrame_Final( AES_CBCFrame_Context *inContext )
{
#if( AES_UTILS_USE_COMMON_CRYPTO )
	if( inContext->cryptor ) CCCryptorRelease( inContext->cryptor );
#elif( AES_UTILS_USE_WINDOWS_API )
	if( inContext->key )		CryptDestroyKey( inContext->key );
	if( inContext->provider )	CryptReleaseContext( inContext->provider, 0 );
#endif
	MemZeroSecure( inContext, sizeof( *inContext ) );
}

//===========================================================================================================================
//	AES_cbc_encrypt
//===========================================================================================================================

#if( !AES_UTILS_USE_COMMON_CRYPTO && !AES_UTILS_USE_GLADMAN_AES && !AES_UTILS_USE_WICED && !AES_UTILS_USE_WINDOWS_API && \
	TARGET_NO_OPENSSL )
// From OpenSSL
static
void AES_cbc_encrypt(const unsigned char *in, unsigned char *out,
		     const unsigned long length, const AES_KEY *key,
		     unsigned char *ivec, const int enc) {

	unsigned long n;
	unsigned long len = length;
	unsigned char tmp[AES_BLOCK_SIZE];
	const unsigned char *iv = ivec;

	if (AES_ENCRYPT == enc) {
		while (len >= AES_BLOCK_SIZE) {
			for(n=0; n < AES_BLOCK_SIZE; ++n)
				out[n] = in[n] ^ iv[n];
			AES_encrypt(out, out, key);
			iv = out;
			len -= AES_BLOCK_SIZE;
			in += AES_BLOCK_SIZE;
			out += AES_BLOCK_SIZE;
		}
		if (len) {
			for(n=0; n < len; ++n)
				out[n] = in[n] ^ iv[n];
			for(n=len; n < AES_BLOCK_SIZE; ++n)
				out[n] = iv[n];
			AES_encrypt(out, out, key);
			iv = out;
		}
		memcpy(ivec,iv,AES_BLOCK_SIZE);
	} else if (in != out) {
		while (len >= AES_BLOCK_SIZE) {
			AES_decrypt(in, out, key);
			for(n=0; n < AES_BLOCK_SIZE; ++n)
				out[n] ^= iv[n];
			iv = in;
			len -= AES_BLOCK_SIZE;
			in  += AES_BLOCK_SIZE;
			out += AES_BLOCK_SIZE;
		}
		if (len) {
			AES_decrypt(in,tmp,key);
			for(n=0; n < len; ++n)
				out[n] = tmp[n] ^ iv[n];
			iv = in;
		}
		memcpy(ivec,iv,AES_BLOCK_SIZE);
	} else {
		while (len >= AES_BLOCK_SIZE) {
			memcpy(tmp, in, AES_BLOCK_SIZE);
			AES_decrypt(in, out, key);
			for(n=0; n < AES_BLOCK_SIZE; ++n)
				out[n] ^= ivec[n];
			memcpy(ivec, tmp, AES_BLOCK_SIZE);
			len -= AES_BLOCK_SIZE;
			in += AES_BLOCK_SIZE;
			out += AES_BLOCK_SIZE;
		}
		if (len) {
			memcpy(tmp, in, AES_BLOCK_SIZE);
			AES_decrypt(tmp, out, key);
			for(n=0; n < len; ++n)
				out[n] ^= ivec[n];
			for(n=len; n < AES_BLOCK_SIZE; ++n)
				out[n] = tmp[n];
			memcpy(ivec, tmp, AES_BLOCK_SIZE);
		}
	}
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AES_ECB_Init
//===========================================================================================================================

OSStatus	AES_ECB_Init( AES_ECB_Context *inContext, uint32_t inMode, const uint8_t inKey[ kAES_ECB_Size ] )
{
#if( AES_UTILS_USE_COMMON_CRYPTO )
	OSStatus		err;
	
	inContext->cryptor = NULL;
	err = CCCryptorCreate( inMode, kCCAlgorithmAES, kCCOptionECBMode, inKey, kAES_ECB_Size, NULL, &inContext->cryptor );
	check_noerr( err );
	if( err ) return( err );
#elif( AES_UTILS_USE_GLADMAN_AES )
	aes_init();
	if( inMode == kAES_ECB_Mode_Encrypt )	aes_encrypt_key128( inKey, &inContext->ctx.encrypt );
	else									aes_decrypt_key128( inKey, &inContext->ctx.decrypt );
	inContext->encrypt = inMode;
#elif( AES_UTILS_USE_WICED )
	if( inMode == kAES_ECB_Mode_Encrypt ) 	aes_setkey_enc( &inContext->ctx, (unsigned char *) inKey, kAES_ECB_Size * 8 );
	else									aes_setkey_dec( &inContext->ctx, (unsigned char *) inKey, kAES_ECB_Size * 8 );
	inContext->mode = inMode;
#elif( AES_UTILS_USE_WINDOWS_API )
	OSStatus		err;
	
	err = _CreateWindowsCryptoAPIContext( &inContext->provider, &inContext->key, inKey, CRYPT_MODE_ECB );
	check_noerr( err );
	if( err ) return( err );
	inContext->encrypt = ( inMode == kAES_ECB_Mode_Encrypt );
#else
	AES_set_encrypt_key( inKey, kAES_ECB_Size * 8, &inContext->key );
	inContext->cryptFunc = ( inMode == kAES_ECB_Mode_Encrypt ) ? AES_encrypt : AES_decrypt;
#endif
	return( kNoErr );
}

//===========================================================================================================================
//	AES_ECB_Update
//===========================================================================================================================

OSStatus	AES_ECB_Update( AES_ECB_Context *inContext, const void *inSrc, size_t inLen, void *inDst )
{
	OSStatus			err;
	const uint8_t *		src;
	uint8_t *			dst;
	size_t				n;
	
	// inSrc and inDst may be the same, but otherwise, the buffers must not overlap.
	
#if( DEBUG )
	if( inSrc != inDst ) check_ptr_overlap( inSrc, inLen, inDst, inLen );
	if( ( inLen % kAES_ECB_Size ) != 0 ) dlogassert( "ECB doesn't support non-block-sized operations (%zu bytes)", inLen );
#endif
	
	src = (const uint8_t *) inSrc;
	dst = (uint8_t *) inDst;
	for( n = inLen / kAES_ECB_Size; n > 0; --n )
	{
		#if( AES_UTILS_USE_COMMON_CRYPTO )
			size_t		len;
			
			err = CCCryptorUpdate( inContext->cryptor, src, kAES_ECB_Size, dst, kAES_ECB_Size, &len );
			require_noerr( err, exit );
			check( len == kAES_ECB_Size );
		#elif( AES_UTILS_USE_GLADMAN_AES )
			if( inContext->encrypt )	aes_ecb_encrypt( src, dst, kAES_ECB_Size, &inContext->ctx.encrypt );
			else						aes_ecb_decrypt( src, dst, kAES_ECB_Size, &inContext->ctx.decrypt );
		#elif( AES_UTILS_USE_WICED )
			aes_crypt_ecb( &inContext->ctx, inContext->mode, (unsigned char *) src, dst );
		#elif( AES_UTILS_USE_WINDOWS_API )
			BOOL		good;
			DWORD		dwLen;
			
			if( src != dst ) memcpy( dst, src, kAES_ECB_Size );
			dwLen = kAES_ECB_Size;
			if( inContext->encrypt )	good = CryptEncrypt( inContext->key, 0, false, 0, dst, &dwLen, dwLen );
			else						good = CryptDecrypt( inContext->key, 0, false, 0, dst, &dwLen );
			err = map_global_value_errno( good, good );
			require_noerr( err, exit );
		#else
			inContext->cryptFunc( src, dst, &inContext->key );
		#endif
		src += kAES_ECB_Size;
		dst += kAES_ECB_Size;
	}
	err = kNoErr;
	
#if( AES_UTILS_USE_COMMON_CRYPTO || AES_UTILS_USE_WINDOWS_API )
exit:
#endif
	return( err );
}

//===========================================================================================================================
//	AES_ECB_Final
//===========================================================================================================================

void	AES_ECB_Final( AES_ECB_Context *inContext )
{
#if( AES_UTILS_USE_COMMON_CRYPTO )
	if( inContext->cryptor ) CCCryptorRelease( inContext->cryptor );
#elif( AES_UTILS_USE_WINDOWS_API )
	if( inContext->key )		CryptDestroyKey( inContext->key );
	if( inContext->provider )	CryptReleaseContext( inContext->provider, 0 );
#endif
	MemZeroSecure( inContext, sizeof( *inContext ) );
}

#if 0
#pragma mark -
#endif

#if( AES_UTILS_HAS_GCM )

#include "TickUtils.h"

//===========================================================================================================================
//	AES_GCM_Init
//===========================================================================================================================

OSStatus
	AES_GCM_Init( 
		AES_GCM_Context *	inContext, 
		const uint8_t		inKey[ kAES_CGM_Size ], 
		const uint8_t		inNonce[ kAES_CGM_Size ] )
{
	return( AES_GCM_InitEx( inContext, kAES_GCM_Mode_Encrypt, inKey, inNonce ) );
}

OSStatus
	AES_GCM_InitEx( 
		AES_GCM_Context *	inContext, 
		AES_GCM_Mode		inMode, 
		const uint8_t		inKey[ kAES_CGM_Size ], 
		const uint8_t		inNonce[ kAES_CGM_Size ] )
{
	OSStatus		err;
	
#if  ( AES_UTILS_HAS_GLADMAN_GCM )
	(void) inMode;
	
	err = gcm_init_and_key( inKey, kAES_CGM_Size, &inContext->ctx );
	require_noerr( err, exit );
#else
	#error "GCM enabled, but no implementation?"
#endif
	
	if( inNonce ) memcpy( inContext->nonce, inNonce, kAES_CGM_Size );
	
exit:
	return( err );
}

//===========================================================================================================================
//	AES_GCM_InitEx2
//===========================================================================================================================

OSStatus	AES_GCM_InitEx2( AES_GCM_Context *inContext, AES_GCM_Mode inMode, const void *inKeyPtr, size_t inKeyLen )
{
	OSStatus		err;
	
#if  ( AES_UTILS_HAS_GLADMAN_GCM )
	(void) inMode;
	
	err = gcm_init_and_key( (const uint8_t *) inKeyPtr, inKeyLen, &inContext->ctx );
	require_noerr( err, exit );
#else
	#error "GCM enabled, but no implementation?"
#endif
	
exit:
	return( err );
}

//===========================================================================================================================
//	AES_GCM_Final
//===========================================================================================================================

void	AES_GCM_Final( AES_GCM_Context *inContext )
{
#if  ( AES_UTILS_HAS_GLADMAN_GCM )
	gcm_end( &inContext->ctx );
#else
	#error "GCM enabled, but no implementation?"
#endif
	MemZeroSecure( inContext, sizeof( *inContext ) );
}

//===========================================================================================================================
//	AES_GCM_InitMessage
//===========================================================================================================================

#if  ( AES_UTILS_HAS_GLADMAN_GCM )
OSStatus	AES_GCM_InitMessage( AES_GCM_Context *inContext, const uint8_t inNonce[ kAES_CGM_Size ] )
{
	OSStatus		err;
	
	if( inNonce == kAES_CGM_Nonce_Auto )
	{
		BigEndianIntegerIncrement( inContext->nonce, kAES_CTR_Size );
		inNonce = inContext->nonce;
	}
	err = gcm_init_message( inNonce, kAES_CGM_Size, &inContext->ctx );
	require_noerr( err, exit );
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	AES_GCM_InitMessageEx
//===========================================================================================================================

#if  ( AES_UTILS_HAS_GLADMAN_GCM )
OSStatus	AES_GCM_InitMessageEx( AES_GCM_Context *inContext, const void *inNoncePtr, size_t inNonceLen )
{
	OSStatus		err;
	
	err = gcm_init_message( (const uint8_t *) inNoncePtr, inNonceLen, &inContext->ctx );
	require_noerr( err, exit );
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	AES_GCM_FinalizeMessage
//===========================================================================================================================

#if  ( AES_UTILS_HAS_GLADMAN_GCM )
OSStatus	AES_GCM_FinalizeMessage( AES_GCM_Context *inContext, uint8_t outAuthTag[ kAES_CGM_Size ] )
{
	OSStatus		err;
	
	err = gcm_compute_tag( outAuthTag, kAES_CGM_Size, &inContext->ctx );
	require_noerr( err, exit );
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	AES_GCM_VerifyMessage
//===========================================================================================================================

#if  ( AES_UTILS_HAS_GLADMAN_GCM )
OSStatus	AES_GCM_VerifyMessage( AES_GCM_Context *inContext, const uint8_t inAuthTag[ kAES_CGM_Size ] )
{
	OSStatus		err;
	uint8_t			authTag[ kAES_CGM_Size ];
	
	err = gcm_compute_tag( authTag, kAES_CGM_Size, &inContext->ctx );
	require_noerr( err, exit );
	require_action_quiet( memcmp_constant_time( authTag, inAuthTag, kAES_CGM_Size ) == 0, exit, err = kAuthenticationErr );
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	AES_GCM_AddAAD
//===========================================================================================================================

OSStatus	AES_GCM_AddAAD( AES_GCM_Context *inContext, const void *inPtr, size_t inLen )
{
	OSStatus		err;
	
#if  ( AES_UTILS_HAS_GLADMAN_GCM )
	err = gcm_auth_header( inPtr, inLen, &inContext->ctx );
	require_noerr( err, exit );
#else
	#error "GCM enabled, but no implementation?"
#endif
	
exit:
	return( err );
}

//===========================================================================================================================
//	AES_GCM_Encrypt
//===========================================================================================================================

#if  ( AES_UTILS_HAS_GLADMAN_GCM )
OSStatus	AES_GCM_Encrypt( AES_GCM_Context *inContext, const void *inSrc, size_t inLen, void *inDst )
{
	OSStatus		err;
	
	err = gcm_encrypt( inDst, inSrc, inLen, &inContext->ctx );
	require_noerr( err, exit );
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	AES_GCM_Decrypt
//===========================================================================================================================

#if  ( AES_UTILS_HAS_GLADMAN_GCM )
OSStatus	AES_GCM_Decrypt( AES_GCM_Context *inContext, const void *inSrc, size_t inLen, void *inDst )
{
	OSStatus		err;
	
	err = gcm_decrypt( inDst, inSrc, inLen, &inContext->ctx );
	require_noerr( err, exit );
	
exit:
	return( err );
}
#endif

#if( !EXCLUDE_UNIT_TESTS )
#include "PrintFUtils.h"

//===========================================================================================================================
//	AES_GCM_Test
//===========================================================================================================================

typedef struct
{
	const char *	name;
	const char *	keyPtr;
	size_t			keyLen;
	const char *	ptPtr;
	size_t			ptLen;
	const char *	aadPtr;
	size_t			aadLen;
	const char *	ivPtr;
	size_t			ivLen;
	const char *	ctPtr;
	const char *	tagPtr;
	size_t			tagLen;
	
}	AES_GCM_TestVector;

static const AES_GCM_TestVector		kAES_GCM_TestVectors[] =
{
	{
		/* name */
		"Test case #46 from libtom", 
		/* key */
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 
		16, 
		/* PT */	
		"\xA2\xAA\xB3\xAD\x8B\x17\xAC\xDD\xA2\x88\x42\x6C\xD7\xC4\x29\xB7"
		"\xCA\x86\xB7\xAC\xA0\x58\x09\xC7\x0C\xE8\x2D\xB2\x57\x11\xCB\x53"
		"\x02\xEB\x27\x43\xB0\x36\xF3\xD7\x50\xD6\xCF\x0D\xC0\xAC\xB9\x29"
		"\x50\xD5\x46\xDB\x30\x8F\x93\xB4\xFF\x24\x4A\xFA\x9D\xC7\x2B\xCD"
		"\x75\x8D\x2C", 
		67, 
		/* aad */
		"\x68\x8E\x1A\xA9\x84\xDE\x92\x6D\xC7\xB4\xC4\x7F\x44", 
		13, 
		/* iv */
		"\xB7\x21\x38\xB5\xA0\x5F\xF5\x07\x0E\x8C\xD9\x41\x83\xF7\x61\xD8", 
		16, 
		/* ct */	
		"\xCB\xC8\xD2\xF1\x54\x81\xA4\xCC\x7D\xD1\xE1\x9A\xAA\x83\xDE\x56"
		"\x78\x48\x3E\xC3\x59\xAE\x7D\xEC\x2A\xB8\xD5\x34\xE0\x90\x6F\x4B"
		"\x46\x63\xFA\xFF\x58\xA8\xB2\xD7\x33\xB8\x45\xEE\xF7\xC9\xB3\x31"
		"\xE9\xE1\x0E\xB2\x61\x2C\x99\x5F\xEB\x1A\xC1\x5A\x62\x86\xCC\xE8"
		"\xB2\x97\xA8", 
		/* tag */
		"\x8D\x2D\x2A\x93\x72\x62\x6F\x6B\xEE\x85\x80\x27\x6A\x63\x66\xBF", 
		16
	},
	{
		/* name */
		"NIST gcmEncryptExtIV256.rsp 1 (Keylen = 256, IVlen = 96, PTlen = 0, AADlen = 0, Taglen = 128, Count = 0)", 
		/* key */
		"\xB5\x2C\x50\x5A\x37\xD7\x8E\xDA\x5D\xD3\x4F\x20\xC2\x25\x40\xEA"
		"\x1B\x58\x96\x3C\xF8\xE5\xBF\x8F\xFA\x85\xF9\xF2\x49\x25\x05\xB4",
		32, 
		/* PT */	
		"", 
		0, 
		/* aad */
		"", 
		0, 
		/* iv */
		"\x51\x6C\x33\x92\x9D\xF5\xA3\x28\x4F\xF4\x63\xD7",
		12, 
		/* ct */	
		"", 
		/* tag */
		"\xBD\xC1\xAC\x88\x4D\x33\x24\x57\xA1\xD2\x66\x4F\x16\x8C\x76\xF0",
		16
	},
	{
		/* name */
		"NIST gcmEncryptExtIV256.rsp 1 (Keylen = 256, IVlen = 96, PTlen = 104, AADlen = 160, Taglen = 128, Count = 0)", 
		/* key */
		"\x69\xB4\x58\xF2\x64\x4A\xF9\x02\x04\x63\xB4\x0E\xE5\x03\xCD\xF0"
		"\x83\xD6\x93\x81\x5E\x26\x59\x05\x1A\xE0\xD0\x39\xE6\x06\xA9\x70",
		32, 
		/* PT */	
		"\xF3\xE0\xE0\x92\x24\x25\x6B\xF2\x1A\x83\xA5\xDE\x8D",
		13, 
		/* aad */
		"\x03\x6A\xD5\xE5\x49\x4E\xF8\x17\xA8\xAF\x2F\x58\x28\x78\x4A\x4B"
		"\xFE\xDD\x16\x53",
		20, 
		/* iv */
		"\x8D\x1D\xA8\xAB\x5F\x91\xCC\xD0\x92\x05\x94\x4B",
		12, 
		/* ct */	
		"\xC0\xA6\x2D\x77\xE6\x03\x1B\xFD\xC6\xB1\x3A\xE2\x17", 
		/* tag */
		"\xA7\x94\xA9\xAA\xEE\x48\xCD\x92\xE4\x77\x61\xBF\x1B\xAF\xF0\xAF",
		16
	},
	{
		/* name */
		"NIST gcmEncryptExtIV256.rsp 1 (Keylen = 256, IVlen = 96, PTlen = 408, AADlen = 0, Taglen = 128, Count = 0)", 
		/* key */
		"\x1F\xDE\xD3\x2D\x59\x99\xDE\x4A\x76\xE0\xF8\x08\x21\x08\x82\x3A"
		"\xEF\x60\x41\x7E\x18\x96\xCF\x42\x18\xA2\xFA\x90\xF6\x32\xEC\x8A",
		32, 
		/* PT */	
		"\x06\xB2\xC7\x58\x53\xDF\x9A\xEB\x17\xBE\xFD\x33\xCE\xA8\x1C\x63"
		"\x0B\x0F\xC5\x36\x67\xFF\x45\x19\x9C\x62\x9C\x8E\x15\xDC\xE4\x1E"
		"\x53\x0A\xA7\x92\xF7\x96\xB8\x13\x8E\xEA\xB2\xE8\x6C\x7B\x7B\xEE"
		"\x1D\x40\xB0",
		51, 
		/* aad */
		"",
		0, 
		/* iv */
		"\x1F\x3A\xFA\x47\x11\xE9\x47\x4F\x32\xE7\x04\x62",
		12, 
		/* ct */	
		"\x91\xFB\xD0\x61\xDD\xC5\xA7\xFC\xC9\x51\x3F\xCD\xFD\xC9\xC3\xA7"
		"\xC5\xD4\xD6\x4C\xED\xF6\xA9\xC2\x4A\xB8\xA7\x7C\x36\xEE\xFB\xF1"
		"\xC5\xDC\x00\xBC\x50\x12\x1B\x96\x45\x6C\x8C\xD8\xB6\xFF\x1F\x8B"
		"\x3E\x48\x0F",
		/* tag */
		"\x30\x09\x6D\x34\x0F\x3D\x5C\x42\xD8\x2A\x6F\x47\x5D\xEF\x23\xEB",
		16
	},
	{
		/* name */
		"NIST gcmEncryptExtIV256.rsp 1 (Keylen = 256, IVlen = 96, PTlen = 408, AADlen = 384, Taglen = 128, Count = 0)", 
		/* key */
		"\x46\x3B\x41\x29\x11\x76\x7D\x57\xA0\xB3\x39\x69\xE6\x74\xFF\xE7"
		"\x84\x5D\x31\x3B\x88\xC6\xFE\x31\x2F\x3D\x72\x4B\xE6\x8E\x1F\xCA",
		32, 
		/* PT */	
		"\xE7\xD1\xDC\xF6\x68\xE2\x87\x68\x61\x94\x0E\x01\x2F\xE5\x2A\x98"
		"\xDA\xCB\xD7\x8A\xB6\x3C\x08\x84\x2C\xC9\x80\x1E\xA5\x81\x68\x2A"
		"\xD5\x4A\xF0\xC3\x4D\x0D\x7F\x6F\x59\xE8\xEE\x0B\xF4\x90\x0E\x0F"
		"\xD8\x50\x42",
		51, 
		/* aad */
		"\x0A\x68\x2F\xBC\x61\x92\xE1\xB4\x7A\x5E\x08\x68\x78\x7F\xFD\xAF"
		"\xE5\xA5\x0C\xEA\xD3\x57\x58\x49\x99\x0C\xDD\x2E\xA9\xB3\x59\x77"
		"\x49\x40\x3E\xFB\x4A\x56\x68\x4F\x0C\x6B\xDE\x35\x2D\x4A\xEE\xC5",
		48, 
		/* iv */
		"\x61\x1C\xE6\xF9\xA6\x88\x07\x50\xDE\x7D\xA6\xCB",
		12, 
		/* ct */	
		"\x88\x86\xE1\x96\x01\x0C\xB3\x84\x9D\x9C\x1A\x18\x2A\xBE\x1E\xEA"
		"\xB0\xA5\xF3\xCA\x42\x3C\x36\x69\xA4\xA8\x70\x3C\x0F\x14\x6E\x8E"
		"\x95\x6F\xB1\x22\xE0\xD7\x21\xB8\x69\xD2\xB6\xFC\xD4\x21\x6D\x7D"
		"\x4D\x37\x58",
		/* tag */
		"\x24\x69\xCE\xCD\x70\xFD\x98\xFE\xC9\x26\x4F\x71\xDF\x1A\xEE\x9A",
		16
	},
};

static OSStatus	_AES_GCM_TestPerfOne( size_t inLen, size_t inCount, Boolean inBad );

OSStatus	AES_GCM_Test( int inPrint, int inPerf )
{
	OSStatus			err;
	AES_GCM_Context		gcm;
	size_t				i;
	uint8_t				buf[ 1024 ];
	uint8_t				buf2[ 1024 ];
	uint8_t				tag[ kAES_CGM_Size ];
	
	for( i = 0; i < countof( kAES_GCM_TestVectors ); ++i )
	{
		const AES_GCM_TestVector *		test = &kAES_GCM_TestVectors[ i ];
		
		if( inPrint ) printf( "AES-GCM Test: %s\n", test->name );
		
		// Encrypt (InitEx)
		
		if( ( test->keyLen == 16 ) && ( test->ivLen == 16 ) )
		{
			err = AES_GCM_InitEx( &gcm, kAES_GCM_Mode_Encrypt, (const uint8_t *) test->keyPtr, kAES_CGM_Nonce_None );
			require_noerr( err, exit );
			
			err = AES_GCM_InitMessage( &gcm, (const uint8_t *) test->ivPtr );
			require_noerr( err, exit );
			
			err = AES_GCM_AddAAD( &gcm, test->aadPtr, test->aadLen );
			require_noerr( err, exit );
			
			err = AES_GCM_Encrypt( &gcm, test->ptPtr, test->ptLen, buf );
			require_noerr( err, exit );
			require_action( memcmp( buf, test->ctPtr, test->ptLen ) == 0, exit, err = -1;
				dlog( kLogLevelError, "Bad:\n%1.1H\nGood:\n%1.1H\n\n", 
					buf, (int) test->ptLen, (int) test->ptLen, 
					test->ctPtr, (int) test->ptLen, (int) test->ptLen ) );
			
			err = AES_GCM_FinalizeMessage( &gcm, tag );
			require_noerr( err, exit );
			require_action( memcmp( tag, test->tagPtr, test->tagLen ) == 0, exit, err = -1 );
			
			AES_GCM_Final( &gcm );
		}
		
		// Encrypt (InitEx2)
		
		err = AES_GCM_InitEx2( &gcm, kAES_GCM_Mode_Encrypt, test->keyPtr, test->keyLen );
		require_noerr( err, exit );
		
		err = AES_GCM_InitMessageEx( &gcm, test->ivPtr, test->ivLen );
		require_noerr( err, exit );
		
		err = AES_GCM_AddAAD( &gcm, test->aadPtr, test->aadLen );
		require_noerr( err, exit );
		
		err = AES_GCM_Encrypt( &gcm, test->ptPtr, test->ptLen, buf );
		require_noerr( err, exit );
		require_action( memcmp( buf, test->ctPtr, test->ptLen ) == 0, exit, err = -1;
			dlog( kLogLevelError, "Bad:\n%1.1H\nGood:\n%1.1H\n\n", 
				buf, (int) test->ptLen, (int) test->ptLen, 
				test->ctPtr, (int) test->ptLen, (int) test->ptLen ) );
		
		err = AES_GCM_FinalizeMessage( &gcm, tag );
		require_noerr( err, exit );
		require_action( memcmp( tag, test->tagPtr, test->tagLen ) == 0, exit, err = -1 );
		
		AES_GCM_Final( &gcm );
		
		// Encrypt (InitEx2, byte-at-a-time)
		
		err = AES_GCM_InitEx2( &gcm, kAES_GCM_Mode_Encrypt, test->keyPtr, test->keyLen );
		require_noerr( err, exit );
		
		err = AES_GCM_InitMessageEx( &gcm, test->ivPtr, test->ivLen );
		require_noerr( err, exit );
		
		err = AES_GCM_AddAAD( &gcm, test->aadPtr, test->aadLen );
		require_noerr( err, exit );
		
		for( i = 0; i < test->ptLen; ++i )
		{
			err = AES_GCM_Encrypt( &gcm, &test->ptPtr[ i ], 1, &buf[ i ] );
			require_noerr( err, exit );
		}
		require_action( memcmp( buf, test->ctPtr, test->ptLen ) == 0, exit, err = -1;
			dlog( kLogLevelError, "Bad:\n%1.1H\nGood:\n%1.1H\n\n", 
				buf, (int) test->ptLen, (int) test->ptLen, 
				test->ctPtr, (int) test->ptLen, (int) test->ptLen ) );
		
		err = AES_GCM_FinalizeMessage( &gcm, tag );
		require_noerr( err, exit );
		require_action( memcmp( tag, test->tagPtr, test->tagLen ) == 0, exit, err = -1 );
		
		AES_GCM_Final( &gcm );
		
		// Decrypt good (InitEx)
		
		if( ( test->keyLen == 16 ) && ( test->ivLen == 16 ) )
		{
			err = AES_GCM_InitEx( &gcm, kAES_GCM_Mode_Decrypt, (const uint8_t *) test->keyPtr, kAES_CGM_Nonce_None );
			require_noerr( err, exit );
			
			err = AES_GCM_InitMessage( &gcm, (const uint8_t *) test->ivPtr );
			require_noerr( err, exit );
			
			err = AES_GCM_AddAAD( &gcm, test->aadPtr, test->aadLen );
			require_noerr( err, exit );
			
			err = AES_GCM_Decrypt( &gcm, buf, test->ptLen, buf2 );
			require_noerr( err, exit );
			require_action( memcmp( buf2, test->ptPtr, test->ptLen ) == 0, exit, err = -1 );
			
			err = AES_GCM_VerifyMessage( &gcm, (const uint8_t *) test->tagPtr );
			require_noerr( err, exit );
			
			AES_GCM_Final( &gcm );
		}
		
		// Decrypt good (InitEx2)
		
		err = AES_GCM_InitEx2( &gcm, kAES_GCM_Mode_Decrypt, test->keyPtr, test->keyLen );
		require_noerr( err, exit );
		
		err = AES_GCM_InitMessageEx( &gcm, test->ivPtr, test->ivLen );
		require_noerr( err, exit );
		
		err = AES_GCM_AddAAD( &gcm, test->aadPtr, test->aadLen );
		require_noerr( err, exit );
		
		err = AES_GCM_Decrypt( &gcm, buf, test->ptLen, buf2 );
		require_noerr( err, exit );
		require_action( memcmp( buf2, test->ptPtr, test->ptLen ) == 0, exit, err = -1 );
		
		err = AES_GCM_VerifyMessage( &gcm, (const uint8_t *) test->tagPtr );
		require_noerr( err, exit );
		
		AES_GCM_Final( &gcm );
		
		// Decrypt good (InitEx2, byte-at-a-time)
		
		err = AES_GCM_InitEx2( &gcm, kAES_GCM_Mode_Decrypt, test->keyPtr, test->keyLen );
		require_noerr( err, exit );
		
		err = AES_GCM_InitMessageEx( &gcm, test->ivPtr, test->ivLen );
		require_noerr( err, exit );
		
		err = AES_GCM_AddAAD( &gcm, test->aadPtr, test->aadLen );
		require_noerr( err, exit );
		
		for( i = 0; i < test->ptLen; ++i )
		{
			err = AES_GCM_Decrypt( &gcm, &buf[ i ], 1, &buf2[ i ] );
			require_noerr( err, exit );
		}
		require_action( memcmp( buf2, test->ptPtr, test->ptLen ) == 0, exit, err = -1 );
		
		err = AES_GCM_VerifyMessage( &gcm, (const uint8_t *) test->tagPtr );
		require_noerr( err, exit );
		
		AES_GCM_Final( &gcm );
		
		// Decrypt bad (InitEx)
		
		if( ( test->keyLen == 16 ) && ( test->ivLen == 16 ) )
		{
			err = AES_GCM_InitEx( &gcm, kAES_GCM_Mode_Decrypt, (const uint8_t *) test->keyPtr, kAES_CGM_Nonce_None );
			require_noerr( err, exit );
			
			err = AES_GCM_InitMessage( &gcm, (const uint8_t *) test->ivPtr );
			require_noerr( err, exit );
			
			err = AES_GCM_AddAAD( &gcm, test->aadPtr, test->aadLen );
			require_noerr( err, exit );
			
			buf[ test->ptLen / 2 ] ^= 0x01;
			err = AES_GCM_Decrypt( &gcm, buf, test->ptLen, buf2 );
			require_noerr( err, exit );
			require_action( memcmp( buf2, test->ptPtr, test->ptLen ) != 0, exit, err = -1 );
			buf[ test->ptLen / 2 ] ^= 0x01;
			
			err = AES_GCM_VerifyMessage( &gcm, (const uint8_t *) test->tagPtr );
			require_action( err != kNoErr, exit, err = -1 );
			
			AES_GCM_Final( &gcm );
		}
		
		// Decrypt bad (InitEx2)
		
		err = AES_GCM_InitEx2( &gcm, kAES_GCM_Mode_Decrypt, test->keyPtr, test->keyLen );
		require_noerr( err, exit );
		
		err = AES_GCM_InitMessageEx( &gcm, test->ivPtr, test->ivLen );
		require_noerr( err, exit );
		
		err = AES_GCM_AddAAD( &gcm, test->aadPtr, test->aadLen );
		require_noerr( err, exit );
		
		buf[ test->ptLen / 2 ] ^= 0x01;
		err = AES_GCM_Decrypt( &gcm, buf, test->ptLen, buf2 );
		require_noerr( err, exit );
		if( test->ptLen > 0 ) require_action( memcmp( buf2, test->ptPtr, test->ptLen ) != 0, exit, err = -1 );
		buf[ test->ptLen / 2 ] ^= 0x01;
		
		err = AES_GCM_VerifyMessage( &gcm, (const uint8_t *) test->tagPtr );
		if( test->ptLen > 0 )	require_action( err != kNoErr, exit, err = -1 );
		else					require_noerr( err, exit );
		AES_GCM_Final( &gcm );
	}
	
	// Performance test.
	
	if( inPerf )
	{
		err = _AES_GCM_TestPerfOne( 1500, 1000, false );
		require_noerr( err, exit );
		err = _AES_GCM_TestPerfOne( 1500, 1000, true );
		require_noerr( err, exit );
		
		err = _AES_GCM_TestPerfOne( 50000, 100, false );
		require_noerr( err, exit );
		err = _AES_GCM_TestPerfOne( 50000, 100, true );
		require_noerr( err, exit );
		
		err = _AES_GCM_TestPerfOne( 5000000, 10, false );
		require_noerr( err, exit );
		err = _AES_GCM_TestPerfOne( 5000000, 10, true );
		require_noerr( err, exit );
	}
	err = kNoErr;
	
exit:
	printf( "AES_GCM_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	_AES_GCM_TestPerfOne
//===========================================================================================================================

static OSStatus	_AES_GCM_TestPerfOne( size_t inLen, size_t inCount, Boolean inBad )
{
	OSStatus			err;
	AES_GCM_Context		gcm;
	size_t				i;
	uint8_t *			buf1;
	uint8_t *			buf2 = NULL;
	uint8_t				tag[ kAES_CGM_Size ];
	uint64_t			ticks;
	double				d;
	
	buf1 = (uint8_t *) malloc( inLen );
	require_action( buf1, exit, err = kNoMemoryErr );
	memset( buf1, 'a', inLen );
	
	buf2 = (uint8_t *) malloc( inLen );
	require_action( buf2, exit, err = kNoMemoryErr );
	
	err = AES_GCM_InitEx( &gcm, kAES_GCM_Mode_Encrypt, 
		(const uint8_t *) "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF",
		(const uint8_t *) "\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF\x00" );
	require_noerr( err, exit );
	
	tag[ 14 ] = 0;
	ticks = UpTicks();
	for( i = 0; i < inCount; ++i )
	{
		err = AES_GCM_InitMessage( &gcm, (const uint8_t *) "\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF\x00" );
		require_noerr( err, exit );
		
		err = AES_GCM_AddAAD( &gcm, NULL, 0 ); // Hack until <radar:14831979> is fixed.
		require_noerr( err, exit );
		
		err = AES_GCM_Encrypt( &gcm, buf1, inLen, buf2 );
		require_noerr( err, exit );
		
		err = AES_GCM_FinalizeMessage( &gcm, tag );
		require_noerr( err, exit );
		
		if( inBad ) break;
	}
	ticks = UpTicks() - ticks;
	d = ( (double) ticks ) / UpTicksPerSecond();
	if( !inBad )
	{
		FPrintF( stderr, "\tAES-GCM encrypt (%zu bytes): %f (%f µs, %.2f MB/sec), BUF: %.3H, MAC: %.3H\n", 
			inLen, d, ( 1000000 * d ) / i, ( i * inLen ) / ( d * 1048576.0 ), buf2, 16, 16, tag, 16, 16 );
	}
	
	AES_GCM_Final( &gcm );
	
	err = AES_GCM_InitEx( &gcm, kAES_GCM_Mode_Decrypt, 
		(const uint8_t *) "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF",
		(const uint8_t *) "\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF\x00" );
	require_noerr( err, exit );
	
	if( inBad )
	{
		tag[ 14 ] ^= 1;
	}
	
	ticks = UpTicks();
	for( i = 0; i < inCount; ++i )
	{
		err = AES_GCM_InitMessage( &gcm, (const uint8_t *) "\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF\x00" );
		require_noerr( err, exit );
		
		err = AES_GCM_AddAAD( &gcm, NULL, 0 ); // Hack until <radar:14831979> is fixed.
		require_noerr( err, exit );
		
		err = AES_GCM_Decrypt( &gcm, buf2, inLen, buf1 );
		require_noerr( err, exit );
		
		err = AES_GCM_VerifyMessage( &gcm, tag );
		if( inBad )	require_action( err != kNoErr, exit, err = kIntegrityErr );
		else		require_noerr( err, exit );
	}
	ticks = UpTicks() - ticks;
	d = ( (double) ticks ) / UpTicksPerSecond();
	FPrintF( stderr, "\tAES-GCM decrypt %s (%zu bytes): %f (%f µs, %.2f MB/sec), BUF: %.3H, MAC: %.3H\n", 
		inBad ? "bad" : "good", inLen, d, ( 1000000 * d ) / i, ( i * inLen ) / ( d * 1048576.0 ), buf1, 16, 16, tag, 16, 16 );
	
	AES_GCM_Final( &gcm );
	err = kNoErr;
	
exit:
	FreeNullSafe( buf1 );
	FreeNullSafe( buf2 );
	return( err );
}

#endif // !EXCLUDE_UNIT_TESTS
#endif // AES_UTILS_HAS_GCM

#if 0
#pragma mark -
#endif

#if( AES_UTILS_USE_WINDOWS_API )
//===========================================================================================================================
//	_CreateWindowsCryptoAPIContext
//===========================================================================================================================

static OSStatus	_CreateWindowsCryptoAPIContext( HCRYPTPROV *outProvider, HCRYPTKEY *outKey, const uint8_t *inKey, DWORD inMode )
{
	OSStatus		err;
	HCRYPTPROV		provider = 0;
	HCRYPTKEY		key = 0;
	BOOL			good;
	AESKeyBlob		blob;
	
	good = CryptAcquireContext( &provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT );
	err = map_global_value_errno( good, good );
	require_noerr( err, exit );
	
	memset( &blob, 0, sizeof( blob ) );
	blob.hdr.bType		= PLAINTEXTKEYBLOB;
	blob.hdr.bVersion	= CUR_BLOB_VERSION;
	blob.hdr.reserved	= 0;
	blob.hdr.aiKeyAlg	= CALG_AES_128;
	blob.len			= kAES_CTR_Size;
	memcpy( blob.key, inKey, kAES_CTR_Size );
	
	good = CryptImportKey( provider, (const BYTE *) &blob, (DWORD) sizeof( blob ), 0, 0, &key );
	err = map_global_value_errno( good, good );
	require_noerr( err, exit );
	
	good = CryptSetKeyParam( key, KP_MODE, (BYTE *) &inMode, 0 );
	err = map_global_value_errno( good, good );
	require_noerr( err, exit );
	
	*outProvider = provider;
	provider = 0;

	*outKey = key;
	key = 0;

exit:
	if( key )		CryptDestroyKey( key );
	if( provider )	CryptReleaseContext( provider, 0 );
	return( err );
}
#endif

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	AESUtils_Test
//===========================================================================================================================

OSStatus	AESUtils_Test( int inPrint, int inPerf )
{
	OSStatus					err;
	AES_CTR_Context				ctx;
	AES_CBCFrame_Context		cbcFrameContext;
	AES_ECB_Context				ecb;
	const uint8_t *				key;
	const uint8_t *				ctr;
	const uint8_t *				iv;
	const uint8_t *				input;
	const uint8_t *				expected;
	const uint8_t *				expected2;
	const uint8_t *				inputPtr;
	const uint8_t *				inputEnd;
	uint8_t						output[ 64 ];
	uint8_t						output2[ 64 ];
	uint8_t *					outputPtr;
	size_t						len;
	size_t						i;
	uint8_t						bigInput[ 1024 ];
	uint8_t						bigOutput[ 1024 ];
	uint8_t						bigOutput2[ 1024 ];
	
	// ---------------------------------------------------------------
	// AES-CTR Test Vector F.5.1 from NIST Special Publication 800-38A
	// ---------------------------------------------------------------
	
	if( inPrint ) printf( "AES-CTR Test Vector F.5.1\n" );
	
	key			= (const uint8_t *) "\x2B\x7E\x15\x16\x28\xAE\xD2\xA6\xAB\xF7\x15\x88\x09\xCF\x4F\x3C";
	
	ctr			= (const uint8_t *) "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF";

	input		= (const uint8_t *) "\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
									"\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51"
									"\x30\xC8\x1C\x46\xA3\x5C\xE4\x11\xE5\xFB\xC1\x19\x1A\x0A\x52\xEF"
									"\xF6\x9F\x24\x45\xDF\x4F\x9B\x17\xAD\x2B\x41\x7B\xE6\x6C\x37\x10";
	inputEnd	= input + 64;
	
	expected	= (const uint8_t *) "\x87\x4D\x61\x91\xB6\x20\xE3\x26\x1B\xEF\x68\x64\x99\x0D\xB6\xCE"
									"\x98\x06\xF6\x6B\x79\x70\xFD\xFF\x86\x17\x18\x7B\xB9\xFF\xFD\xFF"
									"\x5A\xE4\xDF\x3E\xDB\xD5\xD3\x5E\x5B\x4F\x09\x02\x0D\xB0\x3E\xAB"
									"\x1E\x03\x1D\xDA\x2F\xBE\x03\xD1\x79\x21\x70\xA0\xF3\x00\x9C\xEE";
	
	// Verify processing as whole blocks.
	
	memset( output, 'a', sizeof( output ) );
	err = AES_CTR_Init( &ctx, key, ctr );
	require_noerr( err, exit );
	
	len = kAES_CTR_Size;
	for( inputPtr = input, outputPtr = output; inputPtr < inputEnd; inputPtr += len, outputPtr += len )
	{
		AES_CTR_Update( &ctx, inputPtr, len, outputPtr );
	}
	AES_CTR_Final( &ctx );
	require_action( memcmp( output, expected, sizeof( output ) ) == 0, exit, err = kResponseErr );
	
	// Verify processing as bytes.
	
	memset( output, 'b', sizeof( output ) );
	err = AES_CTR_Init( &ctx, key, ctr );
	require_noerr( err, exit );
	
	len = 1;
	for( inputPtr = input, outputPtr = output; inputPtr < inputEnd; inputPtr += len, outputPtr += len )
	{
		AES_CTR_Update( &ctx, inputPtr, 1, outputPtr );
	}
	AES_CTR_Final( &ctx );
	require_action( memcmp( output, expected, sizeof( output ) ) == 0, exit, err = kResponseErr );
	
	// Verify processing as different sized chunks part 1.
	
	memset( output, 'c', sizeof( output ) );
	err = AES_CTR_Init( &ctx, key, ctr );
	require_noerr( err, exit );
	
	inputPtr	= input;
	outputPtr	= output;
	len =  1; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; //  1
	len =  2; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; //  3
	len =  3; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; //  6
	len =  4; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; // 10
	len =  5; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; // 15
	len =  6; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; // 21
	len =  7; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; // 28
	len =  8; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; // 36
	len =  9; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; // 45
	len = 10; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; // 55
	len =  9; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len;					// 64
	AES_CTR_Final( &ctx );
	require_action( inputPtr == inputEnd, exit, err = kResponseErr );
	require_action( memcmp( output, expected, sizeof( output ) ) == 0, exit, err = kResponseErr );
	
	// Verify processing as different sized chunks part 2.
	
	memset( output, 'd', sizeof( output ) );
	err = AES_CTR_Init( &ctx, key, ctr );
	require_noerr( err, exit );
	
	inputPtr	= input;
	outputPtr	= output;
	len =  1; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; //  1
	len = 30; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; // 31
	len = 18; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len; outputPtr += len; // 49
	len = 15; AES_CTR_Update( &ctx, inputPtr, len, outputPtr ); inputPtr  += len;					// 64
	AES_CTR_Final( &ctx );
	require_action( inputPtr == inputEnd, exit, err = kResponseErr );
	require_action( memcmp( output, expected, sizeof( output ) ) == 0, exit, err = kResponseErr );
	
	// ---------------------------------------------------------------
	// AES-CTR Test Vector F.5.2 from NIST Special Publication 800-38A
	// ---------------------------------------------------------------
	
	if( inPrint ) printf( "AES-CTR Test Vector F.5.2\n" );
	
	key			= (const uint8_t *) "\x2B\x7E\x15\x16\x28\xAE\xD2\xA6\xAB\xF7\x15\x88\x09\xCF\x4F\x3C";
	
	ctr			= (const uint8_t *) "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF";

	input		= (const uint8_t *) "\x87\x4D\x61\x91\xB6\x20\xE3\x26\x1B\xEF\x68\x64\x99\x0D\xB6\xCE"
									"\x98\x06\xF6\x6B\x79\x70\xFD\xFF\x86\x17\x18\x7B\xB9\xFF\xFD\xFF"
									"\x5A\xE4\xDF\x3E\xDB\xD5\xD3\x5E\x5B\x4F\x09\x02\x0D\xB0\x3E\xAB"
									"\x1E\x03\x1D\xDA\x2F\xBE\x03\xD1\x79\x21\x70\xA0\xF3\x00\x9C\xEE";
	inputEnd	= input + 64;
	
	expected	= (const uint8_t *) "\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
									"\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51"
									"\x30\xC8\x1C\x46\xA3\x5C\xE4\x11\xE5\xFB\xC1\x19\x1A\x0A\x52\xEF"
									"\xF6\x9F\x24\x45\xDF\x4F\x9B\x17\xAD\x2B\x41\x7B\xE6\x6C\x37\x10";
	
	memset( output, 'b', sizeof( output ) );
	err = AES_CTR_Init( &ctx, key, ctr );
	require_noerr( err, exit );
	
	len = 1;
	for( inputPtr = input, outputPtr = output; inputPtr < inputEnd; inputPtr += len, outputPtr += len )
	{
		AES_CTR_Update( &ctx, inputPtr, 1, outputPtr );
	}
	AES_CTR_Final( &ctx );
	require_action( memcmp( output, expected, sizeof( output ) ) == 0, exit, err = kResponseErr );
	
	// --------------------------------------------------------------------------
	// F.2.1 CBC-AES128.Encrypt test vector from NIST Special Publication 800-38A
	// --------------------------------------------------------------------------
	
	if( inPrint ) printf( "F.2.1 CBC-AES128.Encrypt test vector\n" );
	
	key			= (const uint8_t *) "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c";
	iv		 	= (const uint8_t *) "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f";
	input		= (const uint8_t *) "\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
									"\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51"
									"\x30\xC8\x1C\x46\xA3\x5C\xE4\x11\xE5\xFB\xC1\x19\x1A\x0A\x52\xEF"
									"\xF6\x9F\x24\x45\xDF\x4F\x9B\x17\xAD\x2B\x41\x7B\xE6\x6C\x37\x10";
	inputEnd	= input + 64;
		
	expected	= (const uint8_t *) "\x76\x49\xAB\xAC\x81\x19\xB2\x46\xCE\xE9\x8E\x9B\x12\xE9\x19\x7D"
									"\x50\x86\xCB\x9B\x50\x72\x19\xEE\x95\xDB\x11\x3A\x91\x76\x78\xB2"
									"\x73\xBE\xD6\xB8\xE3\xC1\x74\x3B\x71\x16\xE6\x9E\x22\x22\x95\x16"
									"\x3F\xF1\xCA\xA1\x68\x1F\xAC\x09\x12\x0E\xCA\x30\x75\x86\xE1\xA7";

	expected2	= (const uint8_t *) "\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
									"\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51"
									"\x30\xC8\x1C\x46\xA3\x5C\xE4\x11\xE5\xFB\xC1\x19\x1A\x0A\x52\xEF"
									"\xF6\x9F\x24\x45\xDF\x4F\x9B\x17\xAD\x2B\x41\x7B\xE6\x6C\x37\x10";

	// Verify encrypting as whole blocks.
	
	memset( output, 'a', sizeof( output ) );
	err = AES_CBCFrame_Init( &cbcFrameContext, key, iv, true );
	require_noerr( err, exit );
	
	AES_CBCFrame_Update( &cbcFrameContext, input, (size_t)( inputEnd - input ), output );
	AES_CBCFrame_Final( &cbcFrameContext );
	require_action( memcmp( output, expected, sizeof( output ) ) == 0, exit, err = kResponseErr );
	
	// Verify decrypting as whole blocks.
	
	memset( output2, 'a', sizeof( output2 ) );
	err = AES_CBCFrame_Init( &cbcFrameContext, key, iv, false );
	require_noerr( err, exit );
	
	AES_CBCFrame_Update( &cbcFrameContext, output, (size_t)( inputEnd - input ), output2 );
	AES_CBCFrame_Final( &cbcFrameContext );
	require_action( memcmp( output2, expected2, sizeof( output2 ) ) == 0, exit, err = kResponseErr );
	
	// Verify when processing in chunks.
	
	for( len = 0; len < sizeof( bigInput ); ++len )
	{
		for( i = 0; i < len; ++i )
		{
			memset( bigOutput, 'a', len );
			err = AES_CBCFrame_Init( &cbcFrameContext, key, iv, true );
			require_noerr( err, exit );
			AES_CBCFrame_Update2( &cbcFrameContext, &bigInput[ 0 ], i, &bigInput[ i ], len - i, bigOutput );
			AES_CBCFrame_Final( &cbcFrameContext );
			
			memset( bigOutput2, 'b', len );
			err = AES_CBCFrame_Init( &cbcFrameContext, key, iv, true );
			require_noerr( err, exit );
			AES_CBCFrame_Update( &cbcFrameContext, bigInput, len, bigOutput2 );
			AES_CBCFrame_Final( &cbcFrameContext );
			
			require_action( memcmp( bigOutput, bigOutput2, len ) == 0, exit, err = kResponseErr );
		}
	}
	
	// --------------------------------------------------------------------------
	// F.1.1 ECB-AES128.Encrypt test vector from NIST Special Publication 800-38A
	// --------------------------------------------------------------------------
	
	if( inPrint ) printf( "F.1.1 ECB-AES128.Encrypt test vector\n" );
	
	key			= (const uint8_t *) "\x2B\x7E\x15\x16\x28\xAE\xD2\xA6\xAB\xF7\x15\x88\x09\xCF\x4F\x3C";
	
	input		= (const uint8_t *) "\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
									"\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51"
									"\x30\xC8\x1C\x46\xA3\x5C\xE4\x11\xE5\xFB\xC1\x19\x1A\x0A\x52\xEF"
									"\xF6\x9F\x24\x45\xDF\x4F\x9B\x17\xAD\x2B\x41\x7B\xE6\x6C\x37\x10";
	len = 64;
		
	expected	= (const uint8_t *) "\x3A\xD7\x7B\xB4\x0D\x7A\x36\x60\xA8\x9E\xCA\xF3\x24\x66\xEF\x97"
									"\xF5\xD3\xD5\x85\x03\xB9\x69\x9D\xE7\x85\x89\x5A\x96\xFD\xBA\xAF"
									"\x43\xB1\xCD\x7F\x59\x8E\xCE\x23\x88\x1B\x00\xE3\xED\x03\x06\x88"
									"\x7B\x0C\x78\x5E\x27\xE8\xAD\x3F\x82\x23\x20\x71\x04\x72\x5D\xD4";
	
	err = AES_ECB_Init( &ecb, kAES_ECB_Mode_Encrypt, key );
	require_noerr( err, exit );
	err = AES_ECB_Update( &ecb, input, len, output );
	AES_ECB_Final( &ecb );
	require_noerr( err, exit );
	require_action( memcmp( output, expected, len ) == 0, exit, err = -1 );
	
	// --------------------------------------------------------------------------
	// F.1.2 ECB-AES128.Decrypt test vector from NIST Special Publication 800-38A
	// --------------------------------------------------------------------------
	
	if( inPrint ) printf( "F.1.2 ECB-AES128.Decrypt test vector\n" );
	
	key			= (const uint8_t *) "\x2B\x7E\x15\x16\x28\xAE\xD2\xA6\xAB\xF7\x15\x88\x09\xCF\x4F\x3C";
	
	input		= (const uint8_t *) "\x3A\xD7\x7B\xB4\x0D\x7A\x36\x60\xA8\x9E\xCA\xF3\x24\x66\xEF\x97"
									"\xF5\xD3\xD5\x85\x03\xB9\x69\x9D\xE7\x85\x89\x5A\x96\xFD\xBA\xAF"
									"\x43\xB1\xCD\x7F\x59\x8E\xCE\x23\x88\x1B\x00\xE3\xED\x03\x06\x88"
									"\x7B\x0C\x78\x5E\x27\xE8\xAD\x3F\x82\x23\x20\x71\x04\x72\x5D\xD4";
	len = 64;
		
	expected	= (const uint8_t *) "\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
									"\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51"
									"\x30\xC8\x1C\x46\xA3\x5C\xE4\x11\xE5\xFB\xC1\x19\x1A\x0A\x52\xEF"
									"\xF6\x9F\x24\x45\xDF\x4F\x9B\x17\xAD\x2B\x41\x7B\xE6\x6C\x37\x10";
	
	err = AES_ECB_Init( &ecb, kAES_ECB_Mode_Decrypt, key );
	require_noerr( err, exit );
	err = AES_ECB_Update( &ecb, input, len, output );
	AES_ECB_Final( &ecb );
	require_noerr( err, exit );
	require_action( memcmp( output, expected, len ) == 0, exit, err = -1 );
	
	// AES-GCM Tests
	
#if( AES_UTILS_HAS_GCM )
	AES_GCM_Test( inPrint, inPerf );
#else
	(void) inPerf;
#endif
	
	err = kNoErr;
	
exit:
	printf( "AESUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
