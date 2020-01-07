/*
	File:    	cstr.c
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
	
	Portions Copyright (C) 2007-2014 Apple Inc. All Rights Reserved.
*/

#include "config.h"

#if STDC_HEADERS
	#include <stdlib.h>
	#include <string.h>
#endif

#include "cstr.h"

#define EXPFACTOR	2		/* Minimum expansion factor */
#define MINSIZE		4		/* Absolute minimum - one word */

static char cstr_empty_string[] = { '\0' };
static cstr_allocator * default_alloc = NULL;

/*
 * It is assumed, for efficiency, that it is okay to pass more arguments
 * to a function than are called for, as long as the required arguments
 * are in proper form.  If extra arguments to malloc() and free() cause
 * problems, define PEDANTIC_ARGS below.
 */
static void * Cmalloc(size_t n, void * heap) { (void) heap; return malloc(n); } // APPLE MODIFICATION: Use size_t
static void Cfree(void * p, void * heap) { (void) heap; free(p); }
static cstr_allocator malloc_allocator = { Cmalloc, Cfree, NULL };

_TYPE( void )
cstr_set_allocator(cstr_allocator * alloc)
{
  default_alloc = alloc;
}

_TYPE( cstr * )
cstr_new_alloc(cstr_allocator * alloc)
{
  cstr * str;

  if(alloc == NULL) {
    if(default_alloc == NULL) {
      default_alloc = &malloc_allocator;
    }
    alloc = default_alloc;
  }

  str = (cstr *) (*alloc->alloc)(sizeof(cstr), alloc->heap);
  if(str) {
    str->data = cstr_empty_string;
    str->length = str->cap = 0;
    str->ref = 1;
    str->allocator = alloc;
  }
  return str;
}

_TYPE( cstr * )
cstr_new()
{
  return cstr_new_alloc(NULL);
}

_TYPE( cstr * )
cstr_dup_alloc(const cstr * str, cstr_allocator * alloc)
{
  cstr * nstr = cstr_new_alloc(alloc);
  if(nstr)
    cstr_setn(nstr, str->data, str->length);
  return nstr;
}

_TYPE( cstr * )
cstr_dup(const cstr * str)
{
  return cstr_dup_alloc(str, NULL);
}

_TYPE( cstr * )
cstr_create(const char * s)
{
  return cstr_createn(s, (int) strlen(s)); // APPLE MODIFICATION: Added int cast to fix warning.
}

_TYPE( cstr * )
cstr_createn(const char * s, int len)
{
  cstr * str = cstr_new();
  if(str) {
    cstr_setn(str, s, len);
  }
  return str;
}

_TYPE( void )
cstr_use(cstr * str)
{
  ++str->ref;
}

_TYPE( void )
cstr_clear_free(cstr * str)
{
  if(--str->ref == 0) {
    if(str->cap > 0) {
      memset(str->data, 0, str->cap);
      (*str->allocator->free)(str->data, str->allocator->heap);
    }
    (*str->allocator->free)(str, str->allocator->heap);
  }
}

_TYPE( void )
cstr_detach(cstr * str, uint8_t ** outPtr, size_t * outLen)
{
  *outPtr = (uint8_t *)str->data;
  *outLen = (size_t)str->length;
  str->length = str->cap = 0;
}

_TYPE( void )
cstr_free(cstr * str)
{
  if(--str->ref == 0) {
    if(str->cap > 0)
      (*str->allocator->free)(str->data, str->allocator->heap);
    (*str->allocator->free)(str, str->allocator->heap);
  }
}

_TYPE( void )
cstr_empty(cstr * str)
{
  if(str->cap > 0)
    (*str->allocator->free)(str->data, str->allocator->heap);
  str->data = cstr_empty_string;
  str->length = str->cap = 0;
}

static int
cstr_alloc(cstr * str, int len)
{
  char * t;

  if(len > str->cap) {
    if(len < EXPFACTOR * str->cap)
      len = EXPFACTOR * str->cap;
    if(len < MINSIZE)
      len = MINSIZE;

    t = (char *) (*str->allocator->alloc)(((size_t)len) * sizeof(char),
					  str->allocator->heap);
    if(t) {
      if(str->data) {
	t[str->length] = 0;
	if(str->cap > 0) {
	  if(str->length > 0)
	    memcpy(t, str->data, str->length);
	  free(str->data);
	}
      }
      str->data = t;
      str->cap = len;
      return 1;
    }
    else
      return -1;
  }
  else
    return 0;
}

_TYPE( int )
cstr_copy(cstr * dst, const cstr * src)
{
  return cstr_setn(dst, src->data, src->length);
}

_TYPE( int )
cstr_set(cstr * str, const char * s)
{
  return cstr_setn(str, s, (int) strlen(s)); // APPLE MODIFICATION: Added int cast to fix warning.
}

_TYPE( int )
cstr_setn(cstr * str, const char * s, int len)
{
  if(cstr_alloc(str, len + 1) < 0)
    return -1;
  str->data[len] = 0;
  if(s != NULL && len > 0)
    memmove(str->data, s, len);
  str->length = len;
  return 1;
}

_TYPE( int )
cstr_set_length(cstr * str, int len)
{
  if(len < str->length) {
    str->data[len] = 0;
    str->length = len;
    return 1;
  }
  else if(len > str->length) {
    if(cstr_alloc(str, len + 1) < 0)
      return -1;
    memset(str->data + str->length, 0, len - str->length + 1);
    str->length = len;
    return 1;
  }
  else
    return 0;
}

_TYPE( int )
cstr_append(cstr * str, const char * s)
{
  return cstr_appendn(str, s, (int) strlen(s)); // APPLE MODIFICATION: Added int cast to fix warning.
}

_TYPE( int )
cstr_appendn(cstr * str, const char * s, int len)
{
  if(cstr_alloc(str, str->length + len + 1) < 0)
    return -1;
  memcpy(str->data + str->length, s, len);
  str->length += len;
  str->data[str->length] = 0;
  return 1;
}

_TYPE( int )
cstr_append_str(cstr * dst, const cstr * src)
{
  return cstr_appendn(dst, src->data, src->length);
}
