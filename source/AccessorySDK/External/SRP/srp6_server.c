/*
	File:    	srp6_server.c
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
#include <assert.h>

#include "config.h"
#include "srp.h"

/*
 * SRP-6 has two minor refinements relative to SRP-3/RFC2945:
 * 1. The verifier is multipled by three in the server's
 *    calculation for B.
 * 2. The value of u is taken as the hash of A and B,
 *    instead of the top 32 bits of the hash of B.
 *    This eliminates the old restriction where the
 *    server had to receive A before it could send B.
 */

/*
 * The RFC2945 server keeps track of the running hash state via
 * SRPHashCtx structures pointed to by the meth_data pointer.
 * The "hash" member is the hash value that will be sent to the
 * other side; the "ckhash" member is the hash value expected
 * from the other side.  The server also keeps two more "old"
 * hash states, for backwards-compatibility.
 */
struct server_meth_st {
  SRPHashCtx hash;
  SRPHashCtx ckhash;
  SRPHashCtx oldhash;
  SRPHashCtx oldckhash;
  unsigned char k[SRP_MAX_DIGEST_SIZE];
  unsigned char r[SRP_MAX_DIGEST_SIZE];
};

#define SERVER_CTXP(srp)    ((struct server_meth_st *)(srp)->meth_data)

static SRP_RESULT
srp6_server_init(SRP * srp)
{
  srp->magic = SRP_MAGIC_SERVER;
  srp->flags = SRP_FLAG_MOD_ACCEL;
  srp->meth_data = malloc(sizeof(struct server_meth_st));
  srp->hashDesc->init_f(&SERVER_CTXP(srp)->hash);
  srp->hashDesc->init_f(&SERVER_CTXP(srp)->ckhash);
  srp->hashDesc->init_f(&SERVER_CTXP(srp)->oldhash);
  srp->hashDesc->init_f(&SERVER_CTXP(srp)->oldckhash);
  return SRP_SUCCESS;
}

static SRP_RESULT
srp6a_server_init(SRP * srp)
{
  srp->magic = SRP_MAGIC_SERVER;
  srp->flags = SRP_FLAG_MOD_ACCEL | SRP_FLAG_LEFT_PAD;
  srp->meth_data = malloc(sizeof(struct server_meth_st));
  srp->hashDesc->init_f(&SERVER_CTXP(srp)->hash);
  srp->hashDesc->init_f(&SERVER_CTXP(srp)->ckhash);
  srp->hashDesc->init_f(&SERVER_CTXP(srp)->oldhash);
  srp->hashDesc->init_f(&SERVER_CTXP(srp)->oldckhash);
  return SRP_SUCCESS;
}

static SRP_RESULT
srp6_server_finish(SRP * srp)
{
  if(srp->meth_data) {
    memset(srp->meth_data, 0, sizeof(struct server_meth_st));
    free(srp->meth_data);
  }
  return SRP_SUCCESS;
}

static SRP_RESULT
srp6_server_params(SRP * srp, const unsigned char * modulus, int modlen,
		   const unsigned char * generator, int genlen,
		   const unsigned char * salt, int saltlen)
{
  unsigned char buf1[SRP_MAX_DIGEST_SIZE], buf2[SRP_MAX_DIGEST_SIZE];
  SRPHashCtx ctxt;
  int i;

  /* Fields set by SRP_set_params */

  /* Update hash state */
  srp->hashDesc->init_f(&ctxt);
  srp->hashDesc->update_f(&ctxt, modulus, (size_t)modlen);
  srp->hashDesc->final_f(buf1, &ctxt);	/* buf1 = H(modulus) */

  srp->hashDesc->init_f(&ctxt);
  srp->hashDesc->update_f(&ctxt, generator, (size_t)genlen);
  srp->hashDesc->final_f(buf2, &ctxt);	/* buf2 = H(generator) */

  for(i = 0; i < (int) srp->hashDesc->digestLen; ++i)
    buf1[i] ^= buf2[i];		/* buf1 = H(modulus) XOR H(generator) */

  /* ckhash: H(N) xor H(g) */
  srp->hashDesc->update_f(&SERVER_CTXP(srp)->ckhash, buf1, srp->hashDesc->digestLen);

  srp->hashDesc->init_f(&ctxt);
  srp->hashDesc->update_f(&ctxt, srp->username->data, (size_t)srp->username->length);
  srp->hashDesc->final_f(buf1, &ctxt);	/* buf1 = H(user) */

  /* ckhash: (H(N) xor H(g)) | H(U) */
  srp->hashDesc->update_f(&SERVER_CTXP(srp)->ckhash, buf1, srp->hashDesc->digestLen);

  /* ckhash: (H(N) xor H(g)) | H(U) | s */
  srp->hashDesc->update_f(&SERVER_CTXP(srp)->ckhash, salt, (size_t)saltlen);

  return SRP_SUCCESS;
}

static SRP_RESULT
srp6_server_auth(SRP * srp, const unsigned char * a, int alen)
{
  /* On the server, the authenticator is the verifier */
  srp->verifier = BigIntegerFromBytes(a, alen);

  return SRP_SUCCESS;
}

/* Normally this method isn't called, except maybe by test programs */
static SRP_RESULT
srp6_server_passwd(SRP * srp, const unsigned char * p, int plen)
{
  SRPHashCtx ctxt;
  unsigned char dig[SRP_MAX_DIGEST_SIZE];

  srp->hashDesc->init_f(&ctxt);
  srp->hashDesc->update_f(&ctxt, srp->username->data, (size_t)srp->username->length);
  srp->hashDesc->update_f(&ctxt, ":", 1);
  srp->hashDesc->update_f(&ctxt, p, (size_t)plen);
  srp->hashDesc->final_f(dig, &ctxt);	/* dig = H(U | ":" | P) */

  srp->hashDesc->init_f(&ctxt);
  srp->hashDesc->update_f(&ctxt, srp->salt->data, (size_t)srp->salt->length);
  srp->hashDesc->update_f(&ctxt, dig, srp->hashDesc->digestLen);
  srp->hashDesc->final_f(dig, &ctxt);	/* dig = H(s | H(U | ":" | P)) */
  memset(&ctxt, 0, sizeof(ctxt));

  srp->password = BigIntegerFromBytes(dig, (int) srp->hashDesc->digestLen);
  memset(dig, 0, srp->hashDesc->digestLen);

  /* verifier = g^x mod N */
  srp->verifier = BigIntegerFromInt(0);
  BigIntegerModExp(srp->verifier, srp->generator, srp->password, srp->modulus, srp->bctx, srp->accel);

  return SRP_SUCCESS;
}

/* NOTE: this clobbers k */
static SRP_RESULT
srp6_server_genpub_ex(SRP * srp, cstr ** result, BigInteger k)
{
  cstr * bstr;
  int slen = (SRP_get_secret_bits(BigIntegerBitLen(srp->modulus)) + 7) / 8;

  if(result == NULL)
    bstr = cstr_new();
  else {
    if(*result == NULL)
      *result = cstr_new();
    bstr = *result;
  }

  cstr_set_length(bstr, BigIntegerByteLen(srp->modulus));
#if SRP_TESTS
  if (srp->test_random) {
    assert(srp->test_random->length == slen);
    memcpy(bstr->data, srp->test_random->data, slen);
  } else
#endif
    t_random((unsigned char *) bstr->data, (size_t)slen);
  srp->secret = BigIntegerFromBytes((const unsigned char *) bstr->data, slen);
  srp->pubkey = BigIntegerFromInt(0);

  /* B = kv + g^b mod n (blinding) */
  BigIntegerMul(srp->pubkey, k, srp->verifier, srp->bctx);
  BigIntegerModExp(k, srp->generator, srp->secret, srp->modulus, srp->bctx, srp->accel);
  BigIntegerAdd(k, k, srp->pubkey);
  BigIntegerMod(srp->pubkey, k, srp->modulus, srp->bctx);

  BigIntegerToCstr(srp->pubkey, bstr);

  /* oldckhash: B */
  srp->hashDesc->update_f(&SERVER_CTXP(srp)->oldckhash, bstr->data, (size_t)bstr->length);

  if(result == NULL)	/* bstr was a temporary */
    cstr_clear_free(bstr);

  return SRP_SUCCESS;
}

static SRP_RESULT
srp6_server_genpub(SRP * srp, cstr ** result)
{
  SRP_RESULT ret;
  BigInteger k;

  k = BigIntegerFromInt(3);
  ret = srp6_server_genpub_ex(srp, result, k);
  BigIntegerClearFree(k);
  return ret;
}

static SRP_RESULT
srp6a_server_genpub(SRP * srp, cstr ** result)
{
  SRP_RESULT ret;
  BigInteger k;
  cstr * s;
  SRPHashCtx ctxt;
  unsigned char dig[SRP_MAX_DIGEST_SIZE];

  srp->hashDesc->init_f(&ctxt);
  s = cstr_new();
  BigIntegerToCstr(srp->modulus, s);
  srp->hashDesc->update_f(&ctxt, s->data, (size_t)s->length);
  if(srp->flags & SRP_FLAG_LEFT_PAD)
    BigIntegerToCstrEx(srp->generator, s, s->length);
  else
    BigIntegerToCstr(srp->generator, s);
  srp->hashDesc->update_f(&ctxt, s->data, (size_t)s->length);
  srp->hashDesc->final_f(dig, &ctxt);
  cstr_free(s);

  k = BigIntegerFromBytes(dig, (int) srp->hashDesc->digestLen);
  if(BigIntegerCmpInt(k, 0) == 0)
    ret = SRP_ERROR;
  else
    ret = srp6_server_genpub_ex(srp, result, k);
  BigIntegerClearFree(k);
  return ret;
}

static SRP_RESULT
srp6_server_key(SRP * srp, cstr ** result,
		const unsigned char * pubkey, int pubkeylen)
{
  cstr * s;
  BigInteger t1, t2, t3;
  SRPHashCtx ctxt;
  unsigned char dig[SRP_MAX_DIGEST_SIZE];
  int modlen;

  modlen = BigIntegerByteLen(srp->modulus);
  if(pubkeylen > modlen)
    return SRP_ERROR;

  /* ckhash: (H(N) xor H(g)) | H(U) | s | A */
  srp->hashDesc->update_f(&SERVER_CTXP(srp)->ckhash, pubkey, (size_t)pubkeylen);

  s = cstr_new();
  BigIntegerToCstr(srp->pubkey, s);	/* get encoding of B */

  /* ckhash: (H(N) xor H(g)) | H(U) | s | A | B */
  srp->hashDesc->update_f(&SERVER_CTXP(srp)->ckhash, s->data, (size_t)s->length);

  /* hash: A */
  srp->hashDesc->update_f(&SERVER_CTXP(srp)->hash, pubkey, (size_t)pubkeylen);
  /* oldhash: A */
  srp->hashDesc->update_f(&SERVER_CTXP(srp)->oldhash, pubkey, (size_t)pubkeylen);

  /* Compute u = H(pubkey || mypubkey) */
  srp->hashDesc->init_f(&ctxt);
  if(srp->flags & SRP_FLAG_LEFT_PAD) {
    if(pubkeylen < modlen) {
      cstr_set_length(s, modlen);
      memcpy(s->data + (modlen - pubkeylen), pubkey, pubkeylen);
      memset(s->data, 0, modlen - pubkeylen);
      srp->hashDesc->update_f(&ctxt, s->data, (size_t)modlen);
      BigIntegerToCstrEx(srp->pubkey, s, modlen);
    }
    else {
      srp->hashDesc->update_f(&ctxt, pubkey, (size_t)pubkeylen);
      if(s->length < modlen)
	BigIntegerToCstrEx(srp->pubkey, s, modlen);
    }
  }
  else {
    srp->hashDesc->update_f(&ctxt, pubkey, (size_t)pubkeylen);
  }
  srp->hashDesc->update_f(&ctxt, s->data, (size_t)s->length);
  srp->hashDesc->final_f(dig, &ctxt);	/* dig = H(A || B) */
  srp->u = BigIntegerFromBytes(dig, (int) srp->hashDesc->digestLen);

  /* compute A*v^u */
  t1 = BigIntegerFromInt(0);
  BigIntegerModExp(t1, srp->verifier, srp->u, srp->modulus, srp->bctx, srp->accel); /* t1 = v^u */
  t2 = BigIntegerFromBytes(pubkey, pubkeylen); /* t2 = A */
  t3 = BigIntegerFromInt(0);
  BigIntegerModMul(t3, t2, t1, srp->modulus, srp->bctx); /* t3 = A*v^u (mod N) */
  BigIntegerFree(t2);

  if(BigIntegerCmpInt(t3, 1) <= 0) {	/* Reject A*v^u == 0,1 (mod N) */
    BigIntegerClearFree(t1);
    BigIntegerClearFree(t3);
    cstr_free(s);
    return SRP_ERROR;
  }

  BigIntegerAddInt(t1, t3, 1);
  if(BigIntegerCmp(t1, srp->modulus) == 0) {  /* Reject A*v^u == -1 (mod N) */
    BigIntegerClearFree(t1);
    BigIntegerClearFree(t3);
    cstr_free(s);
    return SRP_ERROR;
  }

  srp->key = BigIntegerFromInt(0);
  BigIntegerModExp(srp->key, t3, srp->secret, srp->modulus, srp->bctx, srp->accel);  /* (Av^u)^b */
  BigIntegerClearFree(t1);
  BigIntegerClearFree(t3);

  /* convert srp->key into session key, update hashes */
  BigIntegerToCstr(srp->key, s);
  if(srp->hashDesc->keyLen == 40) {
    t_mgf1(SERVER_CTXP(srp)->k, (unsigned int) srp->hashDesc->keyLen, (const unsigned char *) s->data, (unsigned int)s->length); /* Interleaved hash */
  } else {
    srp->hashDesc->init_f(&ctxt);
    srp->hashDesc->update_f(&ctxt, s->data, (size_t)s->length);
    srp->hashDesc->final_f(SERVER_CTXP(srp)->k, &ctxt);
  }
  cstr_clear_free(s);

  /* ckhash: (H(N) xor H(g)) | H(U) | s | A | B | K */
  srp->hashDesc->update_f(&SERVER_CTXP(srp)->ckhash, SERVER_CTXP(srp)->k, srp->hashDesc->keyLen);
  /* ckhash: (H(N) xor H(g)) | H(U) | s | A | B | K | ex_data */
  if(srp->ex_data->length > 0)
    srp->hashDesc->update_f(&SERVER_CTXP(srp)->ckhash, srp->ex_data->data, (size_t)srp->ex_data->length);

  /* oldhash: A | K */
  srp->hashDesc->update_f(&SERVER_CTXP(srp)->oldhash, SERVER_CTXP(srp)->k, srp->hashDesc->keyLen);
  /* oldckhash: B | K */
  srp->hashDesc->update_f(&SERVER_CTXP(srp)->oldckhash, SERVER_CTXP(srp)->k, srp->hashDesc->keyLen);

  if(result) {
    if(*result == NULL)
      *result = cstr_new();
    cstr_setn(*result, (const char *) SERVER_CTXP(srp)->k, (int) srp->hashDesc->keyLen);
  }

  return SRP_SUCCESS;
}

static SRP_RESULT
srp6_server_verify(SRP * srp, const unsigned char * proof, int prooflen)
{
  unsigned char expected[SRP_MAX_DIGEST_SIZE];

  srp->hashDesc->final_f(expected, &SERVER_CTXP(srp)->oldckhash);
  if(prooflen == (int) srp->hashDesc->digestLen && memcmp(expected, proof, (size_t) prooflen) == 0) {
    srp->hashDesc->final_f(SERVER_CTXP(srp)->r, &SERVER_CTXP(srp)->oldhash);
    return SRP_SUCCESS;
  }
  srp->hashDesc->final_f(expected, &SERVER_CTXP(srp)->ckhash);
  if(prooflen == (int) srp->hashDesc->digestLen && memcmp(expected, proof, (size_t) prooflen) == 0) {
    /* hash: A | M | K */
    srp->hashDesc->update_f(&SERVER_CTXP(srp)->hash, expected, srp->hashDesc->digestLen);
    srp->hashDesc->update_f(&SERVER_CTXP(srp)->hash, SERVER_CTXP(srp)->k, srp->hashDesc->keyLen);
    srp->hashDesc->final_f(SERVER_CTXP(srp)->r, &SERVER_CTXP(srp)->hash);
    return SRP_SUCCESS;
  }
  return SRP_ERROR;
}

static SRP_RESULT
srp6_server_respond(SRP * srp, cstr ** proof)
{
  if(proof == NULL)
    return SRP_ERROR;

  if(*proof == NULL)
    *proof = cstr_new();

  cstr_set_length(*proof, (int) srp->hashDesc->digestLen);
  memcpy((*proof)->data, SERVER_CTXP(srp)->r, srp->hashDesc->digestLen);
  return SRP_SUCCESS;
}

static SRP_METHOD srp6_server_meth = {
  "SRP-6 server (tjw)",
  srp6_server_init,
  srp6_server_finish,
  srp6_server_params,
  srp6_server_auth,
  srp6_server_passwd,
  srp6_server_genpub,
  srp6_server_key,
  srp6_server_verify,
  srp6_server_respond,
  NULL
};

_TYPE( SRP_METHOD * )
SRP6_server_method()
{
  return &srp6_server_meth;
}

static SRP_METHOD srp6a_server_meth = {
  "SRP-6a server (tjw)",
  srp6a_server_init,
  srp6_server_finish,
  srp6_server_params,
  srp6_server_auth,
  srp6_server_passwd,
  srp6a_server_genpub,
  srp6_server_key,
  srp6_server_verify,
  srp6_server_respond,
  NULL
};

_TYPE( SRP_METHOD * )
SRP6a_server_method()
{
  return &srp6a_server_meth;
}
