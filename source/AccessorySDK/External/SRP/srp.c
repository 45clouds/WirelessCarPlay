/*
	File:    	srp.c
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
	
	Portions Copyright (C) 2007-2015 Apple Inc. All Rights Reserved.
*/
/*
 * Copyright (c) 1997-2007  The Stanford SRP Authentication Project
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 *
 * IN NO EVENT SHALL STANFORD BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER OR NOT ADVISED OF
 * THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF LIABILITY, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Redistributions in source or binary form must retain an intact copy
 * of this copyright notice.
 */

#include "config.h"
#include "srp.h"

static int library_initialized = 0;

_TYPE( SRP_RESULT )
SRP_initialize_library()
{
  if(library_initialized == 0) {
    BigIntegerInitialize();
    library_initialized = 1;
  }
  return SRP_SUCCESS;
}

_TYPE( SRP_RESULT )
SRP_finalize_library()
{
  if(library_initialized > 0) {
    library_initialized = 0;
    BigIntegerFinalize();
  }
  return SRP_SUCCESS;
}

static int srp_modulus_min_bits = SRP_DEFAULT_MIN_BITS;

_TYPE( SRP_RESULT )
SRP_set_modulus_min_bits(int minbits)
{
  srp_modulus_min_bits = minbits;
  return SRP_SUCCESS;
}

_TYPE( int )
SRP_get_modulus_min_bits()
{
  return srp_modulus_min_bits;
}

static int
default_secret_bits_cb(int modsize)
{
  (void) modsize; /* unused */ // APPLE MODIFICATION: Unused parameters.
  return 256;
  /*return modsize;*/   /* Warning: Very Slow */
}

static SRP_SECRET_BITS_CB srp_sb_cb = default_secret_bits_cb;

_TYPE( SRP_RESULT )
SRP_set_secret_bits_cb(SRP_SECRET_BITS_CB cb)
{
  srp_sb_cb = cb;
  return SRP_SUCCESS;
}

_TYPE( int )
SRP_get_secret_bits(int modsize)
{
  return (*srp_sb_cb)(modsize);
}

_TYPE( SRP_SERVER_LOOKUP * )
SRP_SERVER_LOOKUP_new(SRP_SERVER_LOOKUP_METHOD * meth)
{
  SRP_SERVER_LOOKUP * slu = (SRP_SERVER_LOOKUP *) malloc(sizeof(SRP_SERVER_LOOKUP));
  if(slu == NULL)
    return NULL;

  slu->meth = meth;
  slu->data = NULL;
  if(slu->meth->init == NULL || (*slu->meth->init)(slu) == SRP_SUCCESS)
    return slu;
  free(slu);
  return NULL;
}

_TYPE( SRP_RESULT )
SRP_SERVER_LOOKUP_free(SRP_SERVER_LOOKUP * slu)
{
  if(slu->meth->finish)
    (*slu->meth->finish)(slu);
  free(slu);
  return SRP_SUCCESS;
}

_TYPE( SRP_RESULT )
SRP_SERVER_do_lookup(SRP_SERVER_LOOKUP * slu, SRP * srp, cstr * username)
{
  return (*slu->meth->lookup)(slu, srp, username);
}

_TYPE( SRP * )
SRP_new(SRP_METHOD * meth, const SRPHashDescriptor *hashDesc)
{
  SRP * srp = (SRP *) malloc(sizeof(SRP));

  if(srp == NULL)
    return NULL;

  srp->flags = 0;
  srp->username = cstr_new();
  srp->bctx = BigIntegerCtxNew();
  srp->modulus = NULL;
  srp->accel = NULL;
  srp->generator = NULL;
  srp->salt = NULL;
  srp->verifier = NULL;
  srp->password = NULL;
  srp->pubkey = NULL;
  srp->secret = NULL;
  srp->u = NULL;
  srp->key = NULL;
  srp->ex_data = cstr_new();
  srp->param_cb = NULL;
  srp->meth = meth;
  srp->meth_data = NULL;
  srp->slu = NULL;
  srp->hashDesc = hashDesc;
#if SRP_TESTS
  srp->test_random = NULL;
#endif
  if(srp->meth->init == NULL || (*srp->meth->init)(srp) == SRP_SUCCESS)
    return srp;
  free(srp);
  return NULL;
}

_TYPE( SRP_RESULT )
SRP_free(SRP * srp)
{
  if(srp->meth->finish)
    (*srp->meth->finish)(srp);

  if(srp->username)
    cstr_clear_free(srp->username);
  if(srp->modulus)
    BigIntegerFree(srp->modulus);
  if(srp->accel)
    BigIntegerModAccelFree(srp->accel);
  if(srp->generator)
    BigIntegerFree(srp->generator);
  if(srp->salt)
    cstr_clear_free(srp->salt);
  if(srp->verifier)
    BigIntegerClearFree(srp->verifier);
  if(srp->password)
    BigIntegerClearFree(srp->password);
  if(srp->pubkey)
    BigIntegerFree(srp->pubkey);
  if(srp->secret)
    BigIntegerClearFree(srp->secret);
  if(srp->u)
    BigIntegerFree(srp->u);
  if(srp->key)
    BigIntegerClearFree(srp->key);
  if(srp->bctx)
    BigIntegerCtxFree(srp->bctx);
  if(srp->ex_data)
    cstr_clear_free(srp->ex_data);
#if SRP_TESTS
  if (srp->test_random)
    cstr_free(srp->test_random);
#endif
  free(srp);
  return SRP_SUCCESS;
}

#if SRP_TESTS
_TYPE( SRP_RESULT )
SRP_set_test_random(SRP * srp, const void *data, int len)
{
  if (srp->test_random)
    cstr_free(srp->test_random);
  srp->test_random = cstr_createn((const char *)data, len);
  return(srp->test_random ? SRP_SUCCESS : SRP_ERROR);
}
#endif

_TYPE( SRP_RESULT )
SRP_set_server_lookup(SRP * srp, SRP_SERVER_LOOKUP * lookup)
{
  srp->slu = lookup;
  return SRP_SUCCESS;
}

_TYPE( SRP_RESULT )
SRP_set_client_param_verify_cb(SRP * srp, SRP_CLIENT_PARAM_VERIFY_CB cb)
{
  srp->param_cb = cb;
  return SRP_SUCCESS;
}

_TYPE( SRP_RESULT )
SRP_set_username(SRP * srp, const char * username)
{
  cstr_set(srp->username, username);
  if(srp->slu)
    return SRP_SERVER_do_lookup(srp->slu, srp, srp->username);
  else
    return SRP_SUCCESS;
}

_TYPE( SRP_RESULT )
SRP_set_user_raw(SRP * srp, const unsigned char * user, int userlen)
{
  cstr_setn(srp->username, (const char *) user, userlen); // APPLE MODIFICATION: Casts to fix warnings.
  if(srp->slu)
    return SRP_SERVER_do_lookup(srp->slu, srp, srp->username);
  else
    return SRP_SUCCESS;
}

_TYPE( SRP_RESULT )
SRP_set_params(SRP * srp, const unsigned char * modulus, int modlen,
	       const unsigned char * generator, int genlen,
	       const unsigned char * salt, int saltlen)
{
  SRP_RESULT rc;

  if(modulus == NULL || generator == NULL || salt == NULL)
    return SRP_ERROR;

  /* Set fields in SRP context */
  srp->modulus = BigIntegerFromBytes(modulus, modlen);
  if(srp->flags & SRP_FLAG_MOD_ACCEL)
    srp->accel = BigIntegerModAccelNew(srp->modulus, srp->bctx);
  srp->generator = BigIntegerFromBytes(generator, genlen);
  if(srp->salt == NULL)
    srp->salt = cstr_new();
  cstr_setn(srp->salt, (const char *) salt, saltlen); // APPLE MODIFICATION: Casts to fix warnings.

  /* Now attempt to validate parameters */
  if(BigIntegerBitLen(srp->modulus) < SRP_get_modulus_min_bits())
    return SRP_ERROR;

  if(srp->param_cb) {
    rc = (*srp->param_cb)(srp, modulus, modlen, generator, genlen);
    if(!SRP_OK(rc))
      return rc;
  }

  return (*srp->meth->params)(srp, modulus, modlen, generator, genlen,
			      salt, saltlen);
}

_TYPE( SRP_RESULT )
SRP_set_authenticator(SRP * srp, const unsigned char * a, int alen)
{
  return (*srp->meth->auth)(srp, a, alen);
}

_TYPE( SRP_RESULT )
SRP_set_auth_password(SRP * srp, const char * password)
{
  return (*srp->meth->passwd)(srp, (const unsigned char *)password,
			      (int) strlen(password)); // APPLE MODIFICATION: Cast to int to fix warning.
}

_TYPE( SRP_RESULT )
SRP_set_auth_password_raw(SRP * srp,
			  const unsigned char * password, int passlen)
{
  return (*srp->meth->passwd)(srp, password, passlen);
}

_TYPE( SRP_RESULT )
SRP_gen_pub(SRP * srp, cstr ** result)
{
  return (*srp->meth->genpub)(srp, result);
}

_TYPE( SRP_RESULT )
SRP_add_ex_data(SRP * srp, const unsigned char * data, int datalen)
{
  cstr_appendn(srp->ex_data, (const char *) data, datalen); // APPLE MODIFICATION: Casts to fix warnings.
  return SRP_SUCCESS;
}

_TYPE( SRP_RESULT )
SRP_compute_key(SRP * srp, cstr ** result,
		const unsigned char * pubkey, int pubkeylen)
{
  return (*srp->meth->key)(srp, result, pubkey, pubkeylen);
}

_TYPE( SRP_RESULT )
SRP_verify(SRP * srp, const unsigned char * proof, int prooflen)
{
  return (*srp->meth->verify)(srp, proof, prooflen);
}

_TYPE( SRP_RESULT )
SRP_respond(SRP * srp, cstr ** proof)
{
  return (*srp->meth->respond)(srp, proof);
}

_TYPE( SRP_RESULT )
SRP_use_engine(const char * engine)
{
  if(BigIntegerOK(BigIntegerUseEngine(engine)))
    return SRP_SUCCESS;
  else
    return SRP_ERROR;
}

_TYPE( void )
t_mgf1(
     unsigned char * mask,
     unsigned masklen,
     const unsigned char * seed,
     unsigned seedlen)
{
  SHA1_CTX ctxt;
  unsigned i = 0;
  unsigned pos = 0;
  unsigned char cnt[4];
  unsigned char hout[SHA_DIGESTSIZE];

  while(pos < masklen) {
    cnt[0] = (unsigned char)((i >> 24) & 0xFF);
    cnt[1] = (unsigned char)((i >> 16) & 0xFF);
    cnt[2] = (unsigned char)((i >> 8) & 0xFF);
    cnt[3] = (unsigned char)(i & 0xFF);
    SHA1Init(&ctxt);
    SHA1Update(&ctxt, seed, seedlen);
    SHA1Update(&ctxt, cnt, 4);

    if(pos + SHA_DIGESTSIZE > masklen) {
      SHA1Final(hout, &ctxt);
      memcpy(mask + pos, hout, masklen - pos);
      pos = masklen;
    }
    else {
      SHA1Final(mask + pos, &ctxt);
      pos += SHA_DIGESTSIZE;
    }

    ++i;
  }

  memset(hout, 0, sizeof(hout));
  memset((unsigned char *)&ctxt, 0, sizeof(ctxt));
}

#if 0
#pragma mark -
#endif

// SHA-1

static void	_SRPSHA1Init( SRPHashCtx *ctx )
{
	SHA1_Init( &ctx->sha1 );
}

static void	_SRPSHA1Update( SRPHashCtx *ctx, const void *inData, size_t inLen )
{
	SHA1_Update( &ctx->sha1, inData, inLen );
}

static void	_SRPSHA1Final( uint8_t *outDigest, SRPHashCtx *ctx )
{
	SHA1_Final( outDigest, &ctx->sha1 );
}

const SRPHashDescriptor		kSRPHashDescriptor_SHA1 = 
{
	SHA_DIGEST_LENGTH,		// digestLen
	SHA_DIGEST_LENGTH,		// keyLen
	_SRPSHA1Init, 
	_SRPSHA1Update, 
	_SRPSHA1Final
};

const SRPHashDescriptor		kSRPHashDescriptor_SHA1Interleaved = 
{
	SHA_DIGEST_LENGTH,		// digestLen
	2 * SHA_DIGEST_LENGTH,	// keyLen
	_SRPSHA1Init, 
	_SRPSHA1Update, 
	_SRPSHA1Final
};

// SHA-512

static void	_SRPSHA512Init( SRPHashCtx *ctx )
{
	SHA512_Init( &ctx->sha512 );
}

static void	_SRPSHA512Update( SRPHashCtx *ctx, const void *inData, size_t inLen )
{
	SHA512_Update( &ctx->sha512, inData, inLen );
}

static void	_SRPSHA512Final( uint8_t *outDigest, SRPHashCtx *ctx )
{
	SHA512_Final( outDigest, &ctx->sha512 );
}

const SRPHashDescriptor		kSRPHashDescriptor_SHA512 = 
{
	SHA512_DIGEST_LENGTH,	// digestLen
	SHA512_DIGEST_LENGTH,	// keyLen
	_SRPSHA512Init, 
	_SRPSHA512Update, 
	_SRPSHA512Final
};

#if 0
#pragma mark -
#endif

#if SRP_TESTS

typedef struct
{
	const void *				modulusPtr;
	size_t						modulusLen;
	const void *				generatorPtr;
	size_t						generatorLen;
	const SRPHashDescriptor *	hashDesc;
	const char *				username;
	const char *				password;
	const void *				aPrivatePtr;
	size_t						aPrivateLen;
	const void *				aPublicPtr;
	size_t						aPublicLen;
	const void *				bPrivatePtr;
	size_t						bPrivateLen;
	const void *				bPublicPtr;
	size_t						bPublicLen;
	const void *				saltPtr;
	size_t						saltLen;
	const void *				verifierPtr;
	size_t						verifierLen;
	const void *				uPtr;
	size_t						uLen;
	const void *				kPtr;
	size_t						kLen;
	
}	SRPTestVector;

static OSStatus	_libsrp_test_one( const SRPTestVector *inTV );

static const SRPTestVector		kSRPTestVectors[] = 
{
	// Test from RFC 5054 (SRP-1024, SHA-1 non-interleaved).
	{
		// Modulus
		"\xEE\xAF\x0A\xB9\xAD\xB3\x8D\xD6\x9C\x33\xF8\x0A\xFA\x8F\xC5\xE8"
		"\x60\x72\x61\x87\x75\xFF\x3C\x0B\x9E\xA2\x31\x4C\x9C\x25\x65\x76"
		"\xD6\x74\xDF\x74\x96\xEA\x81\xD3\x38\x3B\x48\x13\xD6\x92\xC6\xE0"
		"\xE0\xD5\xD8\xE2\x50\xB9\x8B\xE4\x8E\x49\x5C\x1D\x60\x89\xDA\xD1"
		"\x5D\xC7\xD7\xB4\x61\x54\xD6\xB6\xCE\x8E\xF4\xAD\x69\xB1\x5D\x49"
		"\x82\x55\x9B\x29\x7B\xCF\x18\x85\xC5\x29\xF5\x66\x66\x0E\x57\xEC"
		"\x68\xED\xBC\x3C\x05\x72\x6C\xC0\x2F\xD4\xCB\xF4\x97\x6E\xAA\x9A"
		"\xFD\x51\x38\xFE\x83\x76\x43\x5B\x9F\xC6\x1D\x2F\xC0\xEB\x06\xE3",
		128, 
		
		// Generator
		"\x02", 1, 
		
		// Hash
		&kSRPHashDescriptor_SHA1, 
		
		// Username (I), Password (p)
		"alice", "password123", 
		
		// A private (a)
		"\x60\x97\x55\x27\x03\x5C\xF2\xAD\x19\x89\x80\x6F\x04\x07\x21\x0B"
		"\xC8\x1E\xDC\x04\xE2\x76\x2A\x56\xAF\xD5\x29\xDD\xDA\x2D\x43\x93", 
		32, 
		
		// A public (A)
		"\x61\xD5\xE4\x90\xF6\xF1\xB7\x95\x47\xB0\x70\x4C\x43\x6F\x52\x3D"
		"\xD0\xE5\x60\xF0\xC6\x41\x15\xBB\x72\x55\x7E\xC4\x43\x52\xE8\x90"
		"\x32\x11\xC0\x46\x92\x27\x2D\x8B\x2D\x1A\x53\x58\xA2\xCF\x1B\x6E"
		"\x0B\xFC\xF9\x9F\x92\x15\x30\xEC\x8E\x39\x35\x61\x79\xEA\xE4\x5E"
		"\x42\xBA\x92\xAE\xAC\xED\x82\x51\x71\xE1\xE8\xB9\xAF\x6D\x9C\x03"
		"\xE1\x32\x7F\x44\xBE\x08\x7E\xF0\x65\x30\xE6\x9F\x66\x61\x52\x61"
		"\xEE\xF5\x40\x73\xCA\x11\xCF\x58\x58\xF0\xED\xFD\xFE\x15\xEF\xEA"
		"\xB3\x49\xEF\x5D\x76\x98\x8A\x36\x72\xFA\xC4\x7B\x07\x69\x44\x7B", 
		128,
		
		// B private (b)
		"\xE4\x87\xCB\x59\xD3\x1A\xC5\x50\x47\x1E\x81\xF0\x0F\x69\x28\xE0"
		"\x1D\xDA\x08\xE9\x74\xA0\x04\xF4\x9E\x61\xF5\xD1\x05\x28\x4D\x20", 
		32, 
		
		// B public (B)
		"\xBD\x0C\x61\x51\x2C\x69\x2C\x0C\xB6\xD0\x41\xFA\x01\xBB\x15\x2D"
		"\x49\x16\xA1\xE7\x7A\xF4\x6A\xE1\x05\x39\x30\x11\xBA\xF3\x89\x64"
		"\xDC\x46\xA0\x67\x0D\xD1\x25\xB9\x5A\x98\x16\x52\x23\x6F\x99\xD9"
		"\xB6\x81\xCB\xF8\x78\x37\xEC\x99\x6C\x6D\xA0\x44\x53\x72\x86\x10"
		"\xD0\xC6\xDD\xB5\x8B\x31\x88\x85\xD7\xD8\x2C\x7F\x8D\xEB\x75\xCE"
		"\x7B\xD4\xFB\xAA\x37\x08\x9E\x6F\x9C\x60\x59\xF3\x88\x83\x8E\x7A"
		"\x00\x03\x0B\x33\x1E\xB7\x68\x40\x91\x04\x40\xB1\xB2\x7A\xAE\xAE"
		"\xEB\x40\x12\xB7\xD7\x66\x52\x38\xA8\xE3\xFB\x00\x4B\x11\x7B\x58", 
		128, 
		
		// Salt (s)
		"\xBE\xB2\x53\x79\xD1\xA8\x58\x1E\xB5\xA7\x27\x67\x3A\x24\x41\xEE", 
		16, 
		
		// Verifier (v)
		"\x7E\x27\x3D\xE8\x69\x6F\xFC\x4F\x4E\x33\x7D\x05\xB4\xB3\x75\xBE"
		"\xB0\xDD\xE1\x56\x9E\x8F\xA0\x0A\x98\x86\xD8\x12\x9B\xAD\xA1\xF1"
		"\x82\x22\x23\xCA\x1A\x60\x5B\x53\x0E\x37\x9B\xA4\x72\x9F\xDC\x59"
		"\xF1\x05\xB4\x78\x7E\x51\x86\xF5\xC6\x71\x08\x5A\x14\x47\xB5\x2A"
		"\x48\xCF\x19\x70\xB4\xFB\x6F\x84\x00\xBB\xF4\xCE\xBF\xBB\x16\x81"
		"\x52\xE0\x8A\xB5\xEA\x53\xD1\x5C\x1A\xFF\x87\xB2\xB9\xDA\x6E\x04"
		"\xE0\x58\xAD\x51\xCC\x72\xBF\xC9\x03\x3B\x56\x4E\x26\x48\x0D\x78"
		"\xE9\x55\xA5\xE2\x9E\x7A\xB2\x45\xDB\x2B\xE3\x15\xE2\x09\x9A\xFB", 
		128, 
		
		// Random Scrambling Parameter (u)
		"\xCE\x38\xB9\x59\x34\x87\xDA\x98\x55\x4E\xD4\x7D\x70\xA7\xAE\x5F"
		"\x46\x2E\xF0\x19", 
		20, 
		
		// Premaster Secret (k)
		"\xB0\xDC\x82\xBA\xBC\xF3\x06\x74\xAE\x45\x0C\x02\x87\x74\x5E\x79"
		"\x90\xA3\x38\x1F\x63\xB3\x87\xAA\xF2\x71\xA1\x0D\x23\x38\x61\xE3"
		"\x59\xB4\x82\x20\xF7\xC4\x69\x3C\x9A\xE1\x2B\x0A\x6F\x67\x80\x9F"
		"\x08\x76\xE2\xD0\x13\x80\x0D\x6C\x41\xBB\x59\xB6\xD5\x97\x9B\x5C"
		"\x00\xA1\x72\xB4\xA2\xA5\x90\x3A\x0B\xDC\xAF\x8A\x70\x95\x85\xEB"
		"\x2A\xFA\xFA\x8F\x34\x99\xB2\x00\x21\x0D\xCC\x1F\x10\xEB\x33\x94"
		"\x3C\xD6\x7F\xC8\x8A\x2F\x39\xA4\xBE\x5B\xEC\x4E\xC0\xA3\x21\x2D"
		"\xC3\x46\xD7\xE4\x74\xB2\x9E\xDE\x8A\x46\x9F\xFE\xCA\x68\x6E\x5A", 
		128
	},
	
	// Test 2 (SRP-3072, SHA-512)
	{
		// Modulus
		"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xC9\x0F\xDA\xA2\x21\x68\xC2\x34"
		"\xC4\xC6\x62\x8B\x80\xDC\x1C\xD1\x29\x02\x4E\x08\x8A\x67\xCC\x74"
		"\x02\x0B\xBE\xA6\x3B\x13\x9B\x22\x51\x4A\x08\x79\x8E\x34\x04\xDD"
		"\xEF\x95\x19\xB3\xCD\x3A\x43\x1B\x30\x2B\x0A\x6D\xF2\x5F\x14\x37"
		"\x4F\xE1\x35\x6D\x6D\x51\xC2\x45\xE4\x85\xB5\x76\x62\x5E\x7E\xC6"
		"\xF4\x4C\x42\xE9\xA6\x37\xED\x6B\x0B\xFF\x5C\xB6\xF4\x06\xB7\xED"
		"\xEE\x38\x6B\xFB\x5A\x89\x9F\xA5\xAE\x9F\x24\x11\x7C\x4B\x1F\xE6"
		"\x49\x28\x66\x51\xEC\xE4\x5B\x3D\xC2\x00\x7C\xB8\xA1\x63\xBF\x05"
		"\x98\xDA\x48\x36\x1C\x55\xD3\x9A\x69\x16\x3F\xA8\xFD\x24\xCF\x5F"
		"\x83\x65\x5D\x23\xDC\xA3\xAD\x96\x1C\x62\xF3\x56\x20\x85\x52\xBB"
		"\x9E\xD5\x29\x07\x70\x96\x96\x6D\x67\x0C\x35\x4E\x4A\xBC\x98\x04"
		"\xF1\x74\x6C\x08\xCA\x18\x21\x7C\x32\x90\x5E\x46\x2E\x36\xCE\x3B"
		"\xE3\x9E\x77\x2C\x18\x0E\x86\x03\x9B\x27\x83\xA2\xEC\x07\xA2\x8F"
		"\xB5\xC5\x5D\xF0\x6F\x4C\x52\xC9\xDE\x2B\xCB\xF6\x95\x58\x17\x18"
		"\x39\x95\x49\x7C\xEA\x95\x6A\xE5\x15\xD2\x26\x18\x98\xFA\x05\x10"
		"\x15\x72\x8E\x5A\x8A\xAA\xC4\x2D\xAD\x33\x17\x0D\x04\x50\x7A\x33"
		"\xA8\x55\x21\xAB\xDF\x1C\xBA\x64\xEC\xFB\x85\x04\x58\xDB\xEF\x0A"
		"\x8A\xEA\x71\x57\x5D\x06\x0C\x7D\xB3\x97\x0F\x85\xA6\xE1\xE4\xC7"
		"\xAB\xF5\xAE\x8C\xDB\x09\x33\xD7\x1E\x8C\x94\xE0\x4A\x25\x61\x9D"
		"\xCE\xE3\xD2\x26\x1A\xD2\xEE\x6B\xF1\x2F\xFA\x06\xD9\x8A\x08\x64"
		"\xD8\x76\x02\x73\x3E\xC8\x6A\x64\x52\x1F\x2B\x18\x17\x7B\x20\x0C"
		"\xBB\xE1\x17\x57\x7A\x61\x5D\x6C\x77\x09\x88\xC0\xBA\xD9\x46\xE2"
		"\x08\xE2\x4F\xA0\x74\xE5\xAB\x31\x43\xDB\x5B\xFC\xE0\xFD\x10\x8E"
		"\x4B\x82\xD1\x20\xA9\x3A\xD2\xCA\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",
		384, 
		
		// Generator
		"\x05", 1, 
		
		// Hash
		&kSRPHashDescriptor_SHA512, 
		
		// Username (I), Password (p)
		"alice", "password123", 
		
		// A private (a)
		"\x60\x97\x55\x27\x03\x5C\xF2\xAD\x19\x89\x80\x6F\x04\x07\x21\x0B"
		"\xC8\x1E\xDC\x04\xE2\x76\x2A\x56\xAF\xD5\x29\xDD\xDA\x2D\x43\x93", 
		32, 
		
		// A public (A)
		"\xFA\xB6\xF5\xD2\x61\x5D\x1E\x32\x35\x12\xE7\x99\x1C\xC3\x74\x43"
		"\xF4\x87\xDA\x60\x4C\xA8\xC9\x23\x0F\xCB\x04\xE5\x41\xDC\xE6\x28"
		"\x0B\x27\xCA\x46\x80\xB0\x37\x4F\x17\x9D\xC3\xBD\xC7\x55\x3F\xE6"
		"\x24\x59\x79\x8C\x70\x1A\xD8\x64\xA9\x13\x90\xA2\x8C\x93\xB6\x44"
		"\xAD\xBF\x9C\x00\x74\x5B\x94\x2B\x79\xF9\x01\x2A\x21\xB9\xB7\x87"
		"\x82\x31\x9D\x83\xA1\xF8\x36\x28\x66\xFB\xD6\xF4\x6B\xFC\x0D\xDB"
		"\x2E\x1A\xB6\xE4\xB4\x5A\x99\x06\xB8\x2E\x37\xF0\x5D\x6F\x97\xF6"
		"\xA3\xEB\x6E\x18\x20\x79\x75\x9C\x4F\x68\x47\x83\x7B\x62\x32\x1A"
		"\xC1\xB4\xFA\x68\x64\x1F\xCB\x4B\xB9\x8D\xD6\x97\xA0\xC7\x36\x41"
		"\x38\x5F\x4B\xAB\x25\xB7\x93\x58\x4C\xC3\x9F\xC8\xD4\x8D\x4B\xD8"
		"\x67\xA9\xA3\xC1\x0F\x8E\xA1\x21\x70\x26\x8E\x34\xFE\x3B\xBE\x6F"
		"\xF8\x99\x98\xD6\x0D\xA2\xF3\xE4\x28\x3C\xBE\xC1\x39\x3D\x52\xAF"
		"\x72\x4A\x57\x23\x0C\x60\x4E\x9F\xBC\xE5\x83\xD7\x61\x3E\x6B\xFF"
		"\xD6\x75\x96\xAD\x12\x1A\x87\x07\xEE\xC4\x69\x44\x95\x70\x33\x68"
		"\x6A\x15\x5F\x64\x4D\x5C\x58\x63\xB4\x8F\x61\xBD\xBF\x19\xA5\x3E"
		"\xAB\x6D\xAD\x0A\x18\x6B\x8C\x15\x2E\x5F\x5D\x8C\xAD\x4B\x0E\xF8"
		"\xAA\x4E\xA5\x00\x88\x34\xC3\xCD\x34\x2E\x5E\x0F\x16\x7A\xD0\x45"
		"\x92\xCD\x8B\xD2\x79\x63\x93\x98\xEF\x9E\x11\x4D\xFA\xAA\xB9\x19"
		"\xE1\x4E\x85\x09\x89\x22\x4D\xDD\x98\x57\x6D\x79\x38\x5D\x22\x10"
		"\x90\x2E\x9F\x9B\x1F\x2D\x86\xCF\xA4\x7E\xE2\x44\x63\x54\x65\xF7"
		"\x10\x58\x42\x1A\x01\x84\xBE\x51\xDD\x10\xCC\x9D\x07\x9E\x6F\x16"
		"\x04\xE7\xAA\x9B\x7C\xF7\x88\x3C\x7D\x4C\xE1\x2B\x06\xEB\xE1\x60"
		"\x81\xE2\x3F\x27\xA2\x31\xD1\x84\x32\xD7\xD1\xBB\x55\xC2\x8A\xE2"
		"\x1F\xFC\xF0\x05\xF5\x75\x28\xD1\x5A\x88\x88\x1B\xB3\xBB\xB7\xFE",
		384, 
		
		// B private (b)
		"\xE4\x87\xCB\x59\xD3\x1A\xC5\x50\x47\x1E\x81\xF0\x0F\x69\x28\xE0"
		"\x1D\xDA\x08\xE9\x74\xA0\x04\xF4\x9E\x61\xF5\xD1\x05\x28\x4D\x20", 
		32, 
		
		// B public (B)
		"\x40\xF5\x70\x88\xA4\x82\xD4\xC7\x73\x33\x84\xFE\x0D\x30\x1F\xDD"
		"\xCA\x90\x80\xAD\x7D\x4F\x6F\xDF\x09\xA0\x10\x06\xC3\xCB\x6D\x56"
		"\x2E\x41\x63\x9A\xE8\xFA\x21\xDE\x3B\x5D\xBA\x75\x85\xB2\x75\x58"
		"\x9B\xDB\x27\x98\x63\xC5\x62\x80\x7B\x2B\x99\x08\x3C\xD1\x42\x9C"
		"\xDB\xE8\x9E\x25\xBF\xBD\x7E\x3C\xAD\x31\x73\xB2\xE3\xC5\xA0\xB1"
		"\x74\xDA\x6D\x53\x91\xE6\xA0\x6E\x46\x5F\x03\x7A\x40\x06\x25\x48"
		"\x39\xA5\x6B\xF7\x6D\xA8\x4B\x1C\x94\xE0\xAE\x20\x85\x76\x15\x6F"
		"\xE5\xC1\x40\xA4\xBA\x4F\xFC\x9E\x38\xC3\xB0\x7B\x88\x84\x5F\xC6"
		"\xF7\xDD\xDA\x93\x38\x1F\xE0\xCA\x60\x84\xC4\xCD\x2D\x33\x6E\x54"
		"\x51\xC4\x64\xCC\xB6\xEC\x65\xE7\xD1\x6E\x54\x8A\x27\x3E\x82\x62"
		"\x84\xAF\x25\x59\xB6\x26\x42\x74\x21\x59\x60\xFF\xF4\x7B\xDD\x63"
		"\xD3\xAF\xF0\x64\xD6\x13\x7A\xF7\x69\x66\x1C\x9D\x4F\xEE\x47\x38"
		"\x26\x03\xC8\x8E\xAA\x09\x80\x58\x1D\x07\x75\x84\x61\xB7\x77\xE4"
		"\x35\x6D\xDA\x58\x35\x19\x8B\x51\xFE\xEA\x30\x8D\x70\xF7\x54\x50"
		"\xB7\x16\x75\xC0\x8C\x7D\x83\x02\xFD\x75\x39\xDD\x1F\xF2\xA1\x1C"
		"\xB4\x25\x8A\xA7\x0D\x23\x44\x36\xAA\x42\xB6\xA0\x61\x5F\x3F\x91"
		"\x5D\x55\xCC\x3B\x96\x6B\x27\x16\xB3\x6E\x4D\x1A\x06\xCE\x5E\x5D"
		"\x2E\xA3\xBE\xE5\xA1\x27\x0E\x87\x51\xDA\x45\xB6\x0B\x99\x7B\x0F"
		"\xFD\xB0\xF9\x96\x2F\xEE\x4F\x03\xBE\xE7\x80\xBA\x0A\x84\x5B\x1D"
		"\x92\x71\x42\x17\x83\xAE\x66\x01\xA6\x1E\xA2\xE3\x42\xE4\xF2\xE8"
		"\xBC\x93\x5A\x40\x9E\xAD\x19\xF2\x21\xBD\x1B\x74\xE2\x96\x4D\xD1"
		"\x9F\xC8\x45\xF6\x0E\xFC\x09\x33\x8B\x60\xB6\xB2\x56\xD8\xCA\xC8"
		"\x89\xCC\xA3\x06\xCC\x37\x0A\x0B\x18\xC8\xB8\x86\xE9\x5D\xA0\xAF"
		"\x52\x35\xFE\xF4\x39\x30\x20\xD2\xB7\xF3\x05\x69\x04\x75\x90\x42",
		384, 
		
		// Salt (s)
		"\xBE\xB2\x53\x79\xD1\xA8\x58\x1E\xB5\xA7\x27\x67\x3A\x24\x41\xEE", 
		16, 
		
		// Verifier (v)
		"\x9B\x5E\x06\x17\x01\xEA\x7A\xEB\x39\xCF\x6E\x35\x19\x65\x5A\x85"
		"\x3C\xF9\x4C\x75\xCA\xF2\x55\x5E\xF1\xFA\xF7\x59\xBB\x79\xCB\x47"
		"\x70\x14\xE0\x4A\x88\xD6\x8F\xFC\x05\x32\x38\x91\xD4\xC2\x05\xB8"
		"\xDE\x81\xC2\xF2\x03\xD8\xFA\xD1\xB2\x4D\x2C\x10\x97\x37\xF1\xBE"
		"\xBB\xD7\x1F\x91\x24\x47\xC4\xA0\x3C\x26\xB9\xFA\xD8\xED\xB3\xE7"
		"\x80\x77\x8E\x30\x25\x29\xED\x1E\xE1\x38\xCC\xFC\x36\xD4\xBA\x31"
		"\x3C\xC4\x8B\x14\xEA\x8C\x22\xA0\x18\x6B\x22\x2E\x65\x5F\x2D\xF5"
		"\x60\x3F\xD7\x5D\xF7\x6B\x3B\x08\xFF\x89\x50\x06\x9A\xDD\x03\xA7"
		"\x54\xEE\x4A\xE8\x85\x87\xCC\xE1\xBF\xDE\x36\x79\x4D\xBA\xE4\x59"
		"\x2B\x7B\x90\x4F\x44\x2B\x04\x1C\xB1\x7A\xEB\xAD\x1E\x3A\xEB\xE3"
		"\xCB\xE9\x9D\xE6\x5F\x4B\xB1\xFA\x00\xB0\xE7\xAF\x06\x86\x3D\xB5"
		"\x3B\x02\x25\x4E\xC6\x6E\x78\x1E\x3B\x62\xA8\x21\x2C\x86\xBE\xB0"
		"\xD5\x0B\x5B\xA6\xD0\xB4\x78\xD8\xC4\xE9\xBB\xCE\xC2\x17\x65\x32"
		"\x6F\xBD\x14\x05\x8D\x2B\xBD\xE2\xC3\x30\x45\xF0\x38\x73\xE5\x39"
		"\x48\xD7\x8B\x79\x4F\x07\x90\xE4\x8C\x36\xAE\xD6\xE8\x80\xF5\x57"
		"\x42\x7B\x2F\xC0\x6D\xB5\xE1\xE2\xE1\xD7\xE6\x61\xAC\x48\x2D\x18"
		"\xE5\x28\xD7\x29\x5E\xF7\x43\x72\x95\xFF\x1A\x72\xD4\x02\x77\x17"
		"\x13\xF1\x68\x76\xDD\x05\x0A\xE5\xB7\xAD\x53\xCC\xB9\x08\x55\xC9"
		"\x39\x56\x64\x83\x58\xAD\xFD\x96\x64\x22\xF5\x24\x98\x73\x2D\x68"
		"\xD1\xD7\xFB\xEF\x10\xD7\x80\x34\xAB\x8D\xCB\x6F\x0F\xCF\x88\x5C"
		"\xC2\xB2\xEA\x2C\x3E\x6A\xC8\x66\x09\xEA\x05\x8A\x9D\xA8\xCC\x63"
		"\x53\x1D\xC9\x15\x41\x4D\xF5\x68\xB0\x94\x82\xDD\xAC\x19\x54\xDE"
		"\xC7\xEB\x71\x4F\x6F\xF7\xD4\x4C\xD5\xB8\x6F\x6B\xD1\x15\x81\x09"
		"\x30\x63\x7C\x01\xD0\xF6\x01\x3B\xC9\x74\x0F\xA2\xC6\x33\xBA\x89",
		384, 
		
		// Random Scrambling Parameter (u)
		"\x03\xAE\x5F\x3C\x3F\xA9\xEF\xF1\xA5\x0D\x7D\xBB\x8D\x2F\x60\xA1"
		"\xEA\x66\xEA\x71\x2D\x50\xAE\x97\x6E\xE3\x46\x41\xA1\xCD\x0E\x51"
		"\xC4\x68\x3D\xA3\x83\xE8\x59\x5D\x6C\xB5\x6A\x15\xD5\xFB\xC7\x54"
		"\x3E\x07\xFB\xDD\xD3\x16\x21\x7E\x01\xA3\x91\xA1\x8E\xF0\x6D\xFF",
		64, 
		
		// Premaster Secret (k)
		"\xF1\x03\x6F\xEC\xD0\x17\xC8\x23\x9C\x0D\x5A\xF7\xE0\xFC\xF0\xD4"
		"\x08\xB0\x09\xE3\x64\x11\x61\x8A\x60\xB2\x3A\xAB\xBF\xC3\x83\x39"
		"\x72\x68\x23\x12\x14\xBA\xAC\xDC\x94\xCA\x1C\x53\xF4\x42\xFB\x51"
		"\xC1\xB0\x27\xC3\x18\xAE\x23\x8E\x16\x41\x4D\x60\xD1\x88\x1B\x66"
		"\x48\x6A\xDE\x10\xED\x02\xBA\x33\xD0\x98\xF6\xCE\x9B\xCF\x1B\xB0"
		"\xC4\x6C\xA2\xC4\x7F\x2F\x17\x4C\x59\xA9\xC6\x1E\x25\x60\x89\x9B"
		"\x83\xEF\x61\x13\x1E\x6F\xB3\x0B\x71\x4F\x4E\x43\xB7\x35\xC9\xFE"
		"\x60\x80\x47\x7C\x1B\x83\xE4\x09\x3E\x4D\x45\x6B\x9B\xCA\x49\x2C"
		"\xF9\x33\x9D\x45\xBC\x42\xE6\x7C\xE6\xC0\x2C\x24\x3E\x49\xF5\xDA"
		"\x42\xA8\x69\xEC\x85\x57\x80\xE8\x42\x07\xB8\xA1\xEA\x65\x01\xC4"
		"\x78\xAA\xC0\xDF\xD3\xD2\x26\x14\xF5\x31\xA0\x0D\x82\x6B\x79\x54"
		"\xAE\x8B\x14\xA9\x85\xA4\x29\x31\x5E\x6D\xD3\x66\x4C\xF4\x71\x81"
		"\x49\x6A\x94\x32\x9C\xDE\x80\x05\xCA\xE6\x3C\x2F\x9C\xA4\x96\x9B"
		"\xFE\x84\x00\x19\x24\x03\x7C\x44\x65\x59\xBD\xBB\x9D\xB9\xD4\xDD"
		"\x14\x2F\xBC\xD7\x5E\xEF\x2E\x16\x2C\x84\x30\x65\xD9\x9E\x8F\x05"
		"\x76\x2C\x4D\xB7\xAB\xD9\xDB\x20\x3D\x41\xAC\x85\xA5\x8C\x05\xBD"
		"\x4E\x2D\xBF\x82\x2A\x93\x45\x23\xD5\x4E\x06\x53\xD3\x76\xCE\x8B"
		"\x56\xDC\xB4\x52\x7D\xDD\xC1\xB9\x94\xDC\x75\x09\x46\x3A\x74\x68"
		"\xD7\xF0\x2B\x1B\xEB\x16\x85\x71\x4C\xE1\xDD\x1E\x71\x80\x8A\x13"
		"\x7F\x78\x88\x47\xB7\xC6\xB7\xBF\xA1\x36\x44\x74\xB3\xB7\xE8\x94"
		"\x78\x95\x4F\x6A\x8E\x68\xD4\x5B\x85\xA8\x8E\x4E\xBF\xEC\x13\x36"
		"\x8E\xC0\x89\x1C\x3B\xC8\x6C\xF5\x00\x97\x88\x01\x78\xD8\x61\x35"
		"\xE7\x28\x72\x34\x58\x53\x88\x58\xD7\x15\xB7\xB2\x47\x40\x62\x22"
		"\xC1\x01\x9F\x53\x60\x3F\x01\x69\x52\xD4\x97\x10\x08\x58\x82\x4C",
		384
	},
};

OSStatus	libsrp_test( void )
{
	OSStatus		err;
	size_t			i;
	
	for( i = 0; i < countof( kSRPTestVectors ); ++i )
	{
		err = _libsrp_test_one( &kSRPTestVectors[ i ] );
		require_noerr( err, exit );
	}
	err = kNoErr;
	
exit:
	printf( "libsrp: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

static OSStatus	_libsrp_test_one( const SRPTestVector *inTV )
{
	OSStatus		err;
	SRP *			clientCtx		= NULL;
	cstr *			clientPK		= NULL;
	cstr *			clientSecret	= NULL;
	cstr *			clientProof		= NULL;
	SRP *			serverCtx		= NULL;
	cstr *			serverPK		= NULL;
	cstr *			serverSecret	= NULL;
	cstr *			serverProof		= NULL;
	uint8_t			buf[ 384 ];
	
	// Setup client.
	
	clientCtx = SRP_new( SRP6a_client_method(), inTV->hashDesc );
	require_action( clientCtx, exit, err = kUnknownErr );
	
	err = SRP_set_test_random( clientCtx, inTV->aPrivatePtr, (int) inTV->aPrivateLen );
	require_noerr( err, exit );
	
	err = SRP_set_username( clientCtx, inTV->username );
	require_noerr( err, exit );
	
	err = SRP_set_params( clientCtx, inTV->modulusPtr, (int) inTV->modulusLen, 
		inTV->generatorPtr, (int) inTV->generatorLen, inTV->saltPtr, (int) inTV->saltLen );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	err = SRP_gen_pub( clientCtx, &clientPK );
	require_noerr_action( err, exit, err = kUnknownErr );
	require_action( MemEqual( clientPK->data, (size_t) clientPK->length, inTV->aPublicPtr, (size_t) inTV->aPublicLen ), exit, err = -1 );
	
	err = SRP_set_auth_password( clientCtx, inTV->password );
	require_noerr( err, exit );
	require_action( sizeof( buf ) >= inTV->verifierLen, exit, err = -1 );
	memset( buf, 0, inTV->aPublicLen );
	err = ( BigIntegerToBytes( clientCtx->verifier, buf, (int) inTV->verifierLen ) == (int) inTV->verifierLen ) ? kNoErr : -1;
	require_noerr( err, exit );
	require_action( memcmp( buf, inTV->verifierPtr, inTV->verifierLen ) == 0, exit, err = -1 );
	
	// Setup server.
	
	serverCtx = SRP_new( SRP6a_server_method(), inTV->hashDesc );
	require_action( serverCtx, exit, err = kUnknownErr );
	
	err = SRP_set_test_random( serverCtx, inTV->bPrivatePtr, (int) inTV->bPrivateLen );
	require_noerr( err, exit );
	
	err = SRP_set_username( serverCtx, inTV->username );
	require_noerr( err, exit );
	
	err = SRP_set_params( serverCtx, inTV->modulusPtr, (int) inTV->modulusLen, 
		inTV->generatorPtr, (int) inTV->generatorLen, inTV->saltPtr, (int) inTV->saltLen );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	err = SRP_set_auth_password( serverCtx, inTV->password );
	require_noerr( err, exit );
	
	err = SRP_gen_pub( serverCtx, &serverPK );
	require_noerr_action( err, exit, err = kUnknownErr );
	require_action( MemEqual( serverPK->data, (size_t) serverPK->length, inTV->bPublicPtr, inTV->bPublicLen ), exit, err = -1 );
	
	// Generate client results.
	
	err = SRP_compute_key( clientCtx, &clientSecret, (const uint8_t *) serverPK->data, serverPK->length );
	require_noerr_action( err, exit, err = kUnknownErr );
	require_action( clientCtx->u, exit, err = -1 );
	require_action( sizeof( buf ) >= inTV->uLen, exit, err = -1 );
	BigIntegerToBytes( clientCtx->u, buf, (int) inTV->uLen );
	require_action( memcmp( buf, inTV->uPtr, inTV->uLen ) == 0, exit, err = -1 );
	require_action( clientCtx->key, exit, err = -1 );
	require_action( sizeof( buf ) >= inTV->kLen, exit, err = -1 );
	BigIntegerToBytes( clientCtx->key, buf, (int) inTV->kLen );
	require_action( memcmp( buf, inTV->kPtr, inTV->kLen ) == 0, exit, err = -1 );
	
	err = SRP_respond( clientCtx, &clientProof );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	// Generate server results.
	
	err = SRP_compute_key( serverCtx, &serverSecret, (const uint8_t *) clientPK->data, clientPK->length );
	require_noerr_action( err, exit, err = kUnknownErr );
	require_action( serverCtx->u, exit, err = -1 );
	require_action( sizeof( buf ) >= inTV->uLen, exit, err = -1 );
	BigIntegerToBytes( serverCtx->u, buf, (int) inTV->uLen );
	require_action( memcmp( buf, inTV->uPtr, inTV->uLen ) == 0, exit, err = -1 );
	require_action( serverCtx->key, exit, err = -1 );
	require_action( sizeof( buf ) >= inTV->kLen, exit, err = -1 );
	BigIntegerToBytes( serverCtx->key, buf, (int) inTV->kLen );
	require_action( memcmp( buf, inTV->kPtr, inTV->kLen ) == 0, exit, err = -1 );
	
	// Verify proofs.
	
	err = SRP_verify( serverCtx, (const uint8_t *) clientProof->data, clientProof->length );
	require_noerr_action_quiet( err, exit, err = kAuthenticationErr );
	
	err = SRP_respond( serverCtx, &serverProof );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	err = SRP_verify( clientCtx, (const uint8_t *) serverProof->data, serverProof->length );
	require_noerr_action( err, exit, err = kUnknownErr );
	
exit:
	SRP_forget( &clientCtx );
	SRP_cstr_forget( &clientPK );
	SRP_cstr_forget( &clientSecret );
	SRP_cstr_forget( &clientProof );
	SRP_forget( &serverCtx );
	SRP_cstr_forget( &serverPK );
	SRP_cstr_forget( &serverSecret );
	SRP_cstr_forget( &serverProof );
	return( err );
}
#endif
