/*
	File:    	utfconv.c
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
	
	Copyright (C) 2006-2015 Apple Inc. All Rights Reserved.
	
	Includes Unicode 3.2 decomposition code derived from Core Foundation
*/

#include "utfconv.h"

#if( defined( KERNEL ) || defined( _KERNEL ) )
	#include <sys/param.h>
	
	#include <sys/endian.h>
	#include <sys/errno.h>
	#include <sys/malloc.h>
#else
	#include "CommonServices.h"
	#include "DebugServices.h"
	
	#if( TARGET_HAS_STD_C_LIB )
		#include <stddef.h>
		#include <stdlib.h>
		#include <string.h>
	#endif
#endif

/*
 * UTF-8 (Unicode Transformation Format)
 *
 * UTF-8 is the Unicode Transformation Format that serializes a Unicode
 * character as a sequence of one to four bytes. Only the shortest form
 * required to represent the significant Unicode bits is legal.
 * 
 * UTF-8 Multibyte Codes
 *
 * Bytes   Bits   Unicode Min  Unicode Max   UTF-8 Byte Sequence (binary)
 * -----------------------------------------------------------------------------
 *   1       7       0x0000        0x007F    0xxxxxxx
 *   2      11       0x0080        0x07FF    110xxxxx 10xxxxxx
 *   3      16       0x0800        0xFFFF    1110xxxx 10xxxxxx 10xxxxxx
 *   4      21      0x10000      0x10FFFF    11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 * -----------------------------------------------------------------------------
 */


#define UNICODE_TO_UTF8_LEN(c)  \
	((c) < 0x0080 ? 1 : ((c) < 0x0800 ? 2 : (((c) & 0xf800) == 0xd800 ? 2 : 3)))

#define UCS_ALT_NULL	0x2400

/* Surrogate Pair Constants */
#define SP_HALF_SHIFT	10
#define SP_HALF_BASE	0x0010000U
#define SP_HALF_MASK	0x3FFU

#define SP_HIGH_FIRST	0xD800U
#define SP_HIGH_LAST	0xDBFFU
#define SP_LOW_FIRST	0xDC00U
#define SP_LOW_LAST	0xDFFFU


#include "utfconvdata.h"


/*
 * Test for a combining character.
 *
 * Similar to __CFUniCharIsNonBaseCharacter except that
 * unicode_combinable also includes Hangul Jamo characters.
 */
STATIC_INLINE int
unicode_combinable(uint16_t character)
{
	const uint8_t *bitmap = __CFUniCharCombiningBitmap;
	uint8_t value;

	if (character < 0x0300)
		return (0);

	value = bitmap[(character >> 8) & 0xFF];

	if (value == 0xFF) {
		return (1);
	} else if (value) {
		bitmap = bitmap + ((value - 1) * 32) + 256;
		return (bitmap[(character & 0xFF) / 8] & (1 << (character % 8)) ? 1 : 0);
	}
	return (0);
}

/*
 * Test for a precomposed character.
 *
 * Similar to __CFUniCharIsDecomposableCharacter.
 */
STATIC_INLINE int
unicode_decomposeable(uint16_t character) {
	const uint8_t *bitmap = __CFUniCharDecomposableBitmap;
	uint8_t value;
	
	if (character < 0x00C0)
		return (0);

	value = bitmap[(character >> 8) & 0xFF];

	if (value == 0xFF) {
		return (1);
	} else if (value) {
		bitmap = bitmap + ((value - 1) * 32) + 256;
		return (bitmap[(character & 0xFF) / 8] & (1 << (character % 8)) ? 1 : 0);
	}
    	return (0);
}


/*
 * Get the combing class.
 *
 * Similar to CFUniCharGetCombiningPropertyForCharacter.
 */
STATIC_INLINE uint8_t
get_combining_class(uint16_t character) {
	const uint8_t *bitmap = __CFUniCharCombiningPropertyBitmap;

	uint8_t value = bitmap[(character >> 8)];

	if (value) {
		bitmap = bitmap + (value * 256);
		return bitmap[character % 256];
	}
	return (0);
}


static int unicode_decompose(uint16_t character, uint16_t *convertedChars);

static uint16_t unicode_combine(uint16_t base, uint16_t combining);

static void priortysort(uint16_t* characters, int count);

static uint16_t  ucs_to_sfm(uint16_t ucs_ch, int lastchar);

static uint16_t  sfm_to_ucs(uint16_t ucs_ch);


static signed char utf_extrabytes[32] = {
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	-1, -1, -1, -1, -1, -1, -1, -1,  1,  1,  1,  1,  2,  2,  3, -1
};

static const unsigned char hexdigits[16] = {
	 '0',  '1',  '2',  '3',  '4',  '5',  '6', '7',
	 '8',  '9',  'A',  'B',  'C',  'D',  'E', 'F'
};

/*
 * utf8_encodelen - Calculate the UTF-8 encoding length
 *
 * This function takes a Unicode input string, ucsp, of ucslen bytes
 * and calculates the size of the UTF-8 output in bytes (not including
 * a NULL termination byte). The string must reside in kernel memory.
 *
 * If '/' chars are possible in the Unicode input then an alternate
 * (replacement) char should be provided in altslash.
 *
 * FLAGS
 *    UTF_REVERSE_ENDIAN:  Unicode byte order is opposite current runtime
 *
 *    UTF_BIG_ENDIAN:  Unicode byte order is always big endian
 *
 *    UTF_LITTLE_ENDIAN:  Unicode byte order is always little endian
 *
 *    UTF_DECOMPOSED:  generate fully decomposed output
 *
 *    UTF_PRECOMPOSED is ignored since utf8_encodestr doesn't support it
 *
 * ERRORS
 *    None
 */
size_t
utf8_encodelen(const uint16_t * ucsp, size_t ucslen, uint16_t altslash, int flags)
{
	uint16_t ucs_ch;
	uint16_t * chp = NULL;
	uint16_t sequence[8];
	int extra = 0;
	int charcnt;
	int swapbytes = (flags & UTF_REVERSE_ENDIAN);
	int decompose = (flags & UTF_DECOMPOSED);
	size_t len;

	charcnt = (int)(ucslen / 2);
	len = 0;

	while (charcnt-- > 0) {
		if (extra > 0) {
			--extra;
			ucs_ch = *chp++; // Note: chp is from the local, aligned "sequence" array.
		} else {
			if (swapbytes)	ucs_ch = ReadSwap16(ucsp);
			else			ucs_ch = ReadHost16(ucsp);
			++ucsp;
			if (ucs_ch == '/') {
				ucs_ch = (uint16_t)(altslash ? altslash : '_');
			} else if (ucs_ch == '\0') {
				ucs_ch = UCS_ALT_NULL;
			} else if (decompose && unicode_decomposeable(ucs_ch)) {
				extra = unicode_decompose(ucs_ch, sequence) - 1;
				charcnt += extra;
				ucs_ch = sequence[0];
				chp = &sequence[1];
			}
		}
		len += UNICODE_TO_UTF8_LEN(ucs_ch);
	}

	return (len);
}


/*
 * utf8_encodestr - Encodes a Unicode string to UTF-8
 *
 * NOTES:
 *    The resulting UTF-8 string is NULL terminated.
 *
 *    If '/' chars are allowed on disk then an alternate
 *    (replacement) char must be provided in altslash.
 *
 * input flags:
 *    UTF_REVERSE_ENDIAN: Unicode byteorder is opposite current runtime
 *
 *    UTF_BIG_ENDIAN:  Unicode byte order is always big endian
 *
 *    UTF_LITTLE_ENDIAN:  Unicode byte order is always little endian
 *
 *    UTF_DECOMPOSED:  generate fully decomposed output
 *
 *    UTF_NO_NULL_TERM:  don't add NULL termination to UTF-8 output
 *
 * result:
 *    ENAMETOOLONG: Name didn't fit; only buflen bytes were encoded
 *
 *    EINVAL: Illegal char found; char was replaced by an '_'.
 */
int
utf8_encodestr(const uint16_t * ucsp, size_t ucslen, uint8_t * utf8p,
               size_t * utf8len, size_t buflen, uint16_t altslash, int flags)
{
	uint8_t * bufstart;
	uint8_t * bufend;
	uint16_t ucs_ch;
	uint16_t * chp = NULL;
	uint16_t sequence[8];
	int extra = 0;
	int charcnt;
	int swapbytes = (flags & UTF_REVERSE_ENDIAN);
	int nullterm  = ((flags & UTF_NO_NULL_TERM) == 0);
	int decompose = (flags & UTF_DECOMPOSED);
	int sfmconv = (flags & UTF_SFM_CONVERSIONS);
	int result = 0;

	bufstart = utf8p;
	bufend = bufstart + buflen;
	if (nullterm)
		--bufend;
	charcnt = (int)(ucslen / 2);

	while (charcnt-- > 0) {
		if (extra > 0) {
			--extra;
			ucs_ch = *chp++; // Note: chp is from the local, aligned "sequence" array.
		} else {
			if (swapbytes)	ucs_ch = ReadSwap16(ucsp);
			else			ucs_ch = ReadHost16(ucsp);
			++ucsp;

			if (decompose && unicode_decomposeable(ucs_ch)) {
				extra = unicode_decompose(ucs_ch, sequence) - 1;
				charcnt += extra;
				ucs_ch = sequence[0];
				chp = &sequence[1];
			}
		}

		/* Slash and NULL are not permitted */
		if (ucs_ch == '/') {
			if (altslash)
				ucs_ch = altslash;
			else {
				ucs_ch = '_';
				result = EINVAL;
			}
		} else if (ucs_ch == '\0') {
			ucs_ch = UCS_ALT_NULL;
		}

		if (ucs_ch < 0x0080) {
			if (utf8p >= bufend) {
				result = ENAMETOOLONG;
				break;
			}			
			*utf8p++ = (uint8_t)ucs_ch;

		} else if (ucs_ch < 0x800) {
			if ((utf8p + 1) >= bufend) {
				result = ENAMETOOLONG;
				break;
			}
			*utf8p++ = (uint8_t)(0xc0 | (ucs_ch >> 6));
			*utf8p++ = (uint8_t)(0x80 | (0x3f & ucs_ch));

		} else {
			/* These chars never valid Unicode. */
			if (ucs_ch == 0xFFFE || ucs_ch == 0xFFFF) {
				result = EINVAL;
				break;
			}

			/* Combine valid surrogate pairs */
			if (ucs_ch >= SP_HIGH_FIRST && ucs_ch <= SP_HIGH_LAST
				&& charcnt > 0) {
				uint16_t ch2;
				uint32_t pair;

				if (swapbytes)	ch2 = ReadSwap16(ucsp);
				else			ch2 = ReadHost16(ucsp);
				if (ch2 >= SP_LOW_FIRST && ch2 <= SP_LOW_LAST) {
					pair = ((ucs_ch - SP_HIGH_FIRST) << SP_HALF_SHIFT)
						+ (ch2 - SP_LOW_FIRST) + SP_HALF_BASE;
					if ((utf8p + 3) >= bufend) {
						result = ENAMETOOLONG;
						break;
					}
					--charcnt;
					++ucsp;				
					*utf8p++ = (uint8_t)(0xf0 | (pair >> 18));
					*utf8p++ = (uint8_t)(0x80 | (0x3f & (pair >> 12)));
					*utf8p++ = (uint8_t)(0x80 | (0x3f & (pair >> 6)));
					*utf8p++ = (uint8_t)(0x80 | (0x3f & pair));
					continue;
				}
			} else if (sfmconv) {
				ucs_ch = sfm_to_ucs(ucs_ch);
				if (ucs_ch < 0x0080) {
					if (utf8p >= bufend) {
						result = ENAMETOOLONG;
						break;
					}			
					*utf8p++ = (uint8_t)ucs_ch;
					continue;
				}
			}
			if ((utf8p + 2) >= bufend) {
				result = ENAMETOOLONG;
				break;
			}
			*utf8p++ = (uint8_t)(0xe0 | (ucs_ch >> 12));
			*utf8p++ = (uint8_t)(0x80 | (0x3f & (ucs_ch >> 6)));
			*utf8p++ = (uint8_t)(0x80 | (0x3f & ucs_ch));
		}	
	}
	
	*utf8len = (size_t)(utf8p - bufstart);
	if (nullterm)
		*utf8p = '\0';

	return (result);
}


/*
 * utf8_decodestr - Decodes a UTF-8 string back to Unicode
 *
 * NOTES:
 *    The input UTF-8 string does not need to be null terminated
 *    if utf8len is set.
 *
 *    If '/' chars are allowed on disk then an alternate
 *    (replacement) char must be provided in altslash.
 *
 * input flags:
 *    UTF_REVERSE_ENDIAN:  Unicode byte order is opposite current runtime
 *
 *    UTF_BIG_ENDIAN:  Unicode byte order is always big endian
 *
 *    UTF_LITTLE_ENDIAN:  Unicode byte order is always little endian
 *
 *    UTF_DECOMPOSED:  generate fully decomposed output (NFD)
 *
 *    UTF_PRECOMPOSED:  generate precomposed output (NFC)
 *
 *    UTF_ESCAPE_ILLEGAL:  percent escape any illegal UTF-8 input
 *
 * result:
 *    ENAMETOOLONG: Name didn't fit; only ucslen chars were decoded.
 *
 *    EINVAL: Illegal UTF-8 sequence found.
 */
int
utf8_decodestr(const uint8_t* utf8p, size_t utf8len, uint16_t* ucsp,
               size_t *ucslen, size_t buflen, uint16_t altslash, int flags)
{
	uint16_t* bufstart;
	uint16_t* bufend;
	unsigned int ucs_ch, ucs_ch2, ucs_ch3;
	unsigned int byte;
	int combcharcnt = 0;
	int result = 0;
	int decompose, precompose, swapbytes, escaping;
	int sfmconv;
	int extrabytes;

	decompose  = (flags & UTF_DECOMPOSED);
	precompose = (flags & UTF_PRECOMPOSED);
	swapbytes  = (flags & UTF_REVERSE_ENDIAN);
	escaping   = (flags & UTF_ESCAPE_ILLEGAL);
	sfmconv    = (flags & UTF_SFM_CONVERSIONS);

	bufstart = ucsp;
	bufend = (uint16_t *)((uint8_t *)ucsp + buflen);

	while (utf8len-- > 0 && (byte = *utf8p++) != '\0') {
		if (ucsp >= bufend)
			goto toolong;

		/* check for ascii */
		if (byte < 0x80) {
			ucs_ch = sfmconv ? ucs_to_sfm((uint16_t)byte, utf8len == 0) : byte;
		} else {
			uint32_t ch;

			extrabytes = utf_extrabytes[byte >> 3];
			if ((extrabytes < 0) || ((int)utf8len < extrabytes)) {
				goto escape;
			}
			utf8len -= ((size_t)extrabytes);

			switch (extrabytes) {
			case 1:
				ch = byte; ch <<= 6;   /* 1st byte */
				byte = *utf8p++;       /* 2nd byte */
				if ((byte >> 6) != 2)
					goto escape2;
				ch += byte;
				ch -= 0x00003080U;
				if (ch < 0x0080)
					goto escape2;
				ucs_ch = ch;
			        break;
			case 2:
				ch = byte; ch <<= 6;   /* 1st byte */
				byte = *utf8p++;       /* 2nd byte */
				if ((byte >> 6) != 2)
					goto escape2;
				ch += byte; ch <<= 6;
				byte = *utf8p++;       /* 3rd byte */
				if ((byte >> 6) != 2)
					goto escape3;
				ch += byte;
				ch -= 0x000E2080U;
				if (ch < 0x0800)
					goto escape3;
				if (ch >= 0xD800) {
					if (ch <= 0xDFFF)
						goto escape3;
					if (ch == 0xFFFE || ch == 0xFFFF)
						goto escape3;
				}
				ucs_ch = ch;
				break;
			case 3:
				ch = byte; ch <<= 6;   /* 1st byte */
				byte = *utf8p++;       /* 2nd byte */
				if ((byte >> 6) != 2)
					goto escape2;
				ch += byte; ch <<= 6;
				byte = *utf8p++;       /* 3rd byte */
				if ((byte >> 6) != 2)
					goto escape3;
				ch += byte; ch <<= 6;
				byte = *utf8p++;       /* 4th byte */
				if ((byte >> 6) != 2)
					goto escape4;
			        ch += byte;
				ch -= 0x03C82080U + SP_HALF_BASE;
				ucs_ch = (ch >> SP_HALF_SHIFT) + SP_HIGH_FIRST;
				if (ucs_ch < SP_HIGH_FIRST || ucs_ch > SP_HIGH_LAST)
					goto escape4;
				if (swapbytes)	WriteSwap16(ucsp, (uint16_t) ucs_ch);
				else			WriteHost16(ucsp, (uint16_t) ucs_ch);
				++ucsp;
				if (ucsp >= bufend)
					goto toolong;
				ucs_ch = (ch & SP_HALF_MASK) + SP_LOW_FIRST;
				if (ucs_ch < SP_LOW_FIRST || ucs_ch > SP_LOW_LAST) {
					--ucsp;
					goto escape4;
				}
				if (swapbytes)	WriteSwap16(ucsp, (uint16_t) ucs_ch);
				else			WriteHost16(ucsp, (uint16_t) ucs_ch);
				++ucsp;
				continue;
			default:
				result = EINVAL;
				goto exit;
			}
			if (decompose) {
				if (unicode_decomposeable((uint16_t)ucs_ch)) {
					uint16_t sequence[8];
					int count, i;

					/* Before decomposing a new unicode character, sort 
					 * previous combining characters, if any, and reset
					 * the counter.
					 */
					if (combcharcnt > 1) {
						priortysort(ucsp - combcharcnt, combcharcnt);
					}
					combcharcnt = 0;

					count = unicode_decompose((uint16_t)ucs_ch, sequence);
					for (i = 0; i < count; ++i) {
						ucs_ch = sequence[i];
						if (swapbytes)	WriteSwap16(ucsp, (uint16_t) ucs_ch);
						else			WriteHost16(ucsp, (uint16_t) ucs_ch);
						++ucsp;
						if (ucsp >= bufend)
							goto toolong;
					}
					combcharcnt += count - 1;
					continue;			
				}
			} else if (precompose && (ucsp != bufstart)) {
				uint16_t composite, base;

				if (unicode_combinable((uint16_t)ucs_ch)) {
					if (swapbytes)	base = ReadSwap16(ucsp - 1);
					else			base = ReadHost16(ucsp - 1);
					composite = unicode_combine(base, (uint16_t)ucs_ch);
					if (composite) {
						--ucsp;
						ucs_ch = composite;
					}
				}
			}
			if (ucs_ch == UCS_ALT_NULL)
				ucs_ch = '\0';
		}
		if (ucs_ch == altslash)
			ucs_ch = '/';

		/*
		 * Make multiple combining character sequences canonical
		 */
		if (unicode_combinable((uint16_t)ucs_ch)) {
			++combcharcnt;   /* start tracking a run */
		} else if (combcharcnt) {
			if (combcharcnt > 1) {
				priortysort(ucsp - combcharcnt, combcharcnt);
			}
			combcharcnt = 0;  /* start over */
		}

		if (swapbytes)	WriteSwap16(ucsp, (uint16_t) ucs_ch);
		else			WriteHost16(ucsp, (uint16_t) ucs_ch);
		++ucsp;
		continue;

		/* 
		 * Escape illegal UTF-8 into something legal.
		 */
escape4:
		utf8p -= 3;
		goto escape;
escape3:
		utf8p -= 2;
		goto escape;
escape2:
		utf8p -= 1;
escape:
		if (!escaping) {
			result = EINVAL;
			goto exit;
		}
		if (extrabytes > 0)
			utf8len += ((size_t)extrabytes);
		byte = *(utf8p - 1);

		if ((ucsp + 2) >= bufend)
			goto toolong;

		/* Make a previous combining sequence canonical. */
		if (combcharcnt > 1) {
			priortysort(ucsp - combcharcnt, combcharcnt);
		}
		combcharcnt = 0;
		
		ucs_ch  = '%';
		ucs_ch2 = hexdigits[byte >> 4];
		ucs_ch3 = hexdigits[byte & 0x0F];
		if (swapbytes) {
			WriteSwap16(ucsp, (uint16_t) ucs_ch);  ++ucsp;
			WriteSwap16(ucsp, (uint16_t) ucs_ch2); ++ucsp;
			WriteSwap16(ucsp, (uint16_t) ucs_ch3); ++ucsp;
		} else {
			WriteHost16(ucsp, (uint16_t) ucs_ch);  ++ucsp;
			WriteHost16(ucsp, (uint16_t) ucs_ch2); ++ucsp;
			WriteHost16(ucsp, (uint16_t) ucs_ch3); ++ucsp;
		}
	}
	/*
	 * Make a previous combining sequence canonical
	 */
	if (combcharcnt > 1) {
		priortysort(ucsp - combcharcnt, combcharcnt);
	}
exit:
	*ucslen = (size_t)((uint8_t*)ucsp - (uint8_t*)bufstart);

	return (result);

toolong:
	result = ENAMETOOLONG;
	goto exit;
}


/*
 * utf8_validatestr - Check for a valid UTF-8 string.
 */
int
utf8_validatestr(const uint8_t* utf8p, size_t utf8len)
{
	unsigned int byte;
	uint32_t ch;
	unsigned int ucs_ch;
	size_t extrabytes;

	while (utf8len-- > 0 && (byte = *utf8p++) != '\0') {
		if (byte < 0x80)
			continue;  /* plain ascii */

		extrabytes = (size_t)(utf_extrabytes[byte >> 3]);

		if (utf8len < extrabytes)
			goto invalid;
		utf8len -= extrabytes;

		switch (extrabytes) {
		case 1:
			ch = byte; ch <<= 6;   /* 1st byte */
			byte = *utf8p++;       /* 2nd byte */
			if ((byte >> 6) != 2)
				goto invalid;
			ch += byte;
			ch -= 0x00003080U;
			if (ch < 0x0080)
				goto invalid;
			break;
		case 2:
			ch = byte; ch <<= 6;   /* 1st byte */
			byte = *utf8p++;       /* 2nd byte */
			if ((byte >> 6) != 2)
				goto invalid;
			ch += byte; ch <<= 6;
			byte = *utf8p++;       /* 3rd byte */
			if ((byte >> 6) != 2)
				goto invalid;
			ch += byte;
			ch -= 0x000E2080U;
			if (ch < 0x0800)
				goto invalid;
			if (ch >= 0xD800) {
				if (ch <= 0xDFFF)
					goto invalid;
				if (ch == 0xFFFE || ch == 0xFFFF)
					goto invalid;
			}
			break;
		case 3:
			ch = byte; ch <<= 6;   /* 1st byte */
			byte = *utf8p++;       /* 2nd byte */
			if ((byte >> 6) != 2)
				goto invalid;
			ch += byte; ch <<= 6;
			byte = *utf8p++;       /* 3rd byte */
			if ((byte >> 6) != 2)
				goto invalid;
			ch += byte; ch <<= 6;
			byte = *utf8p++;       /* 4th byte */
			if ((byte >> 6) != 2)
				goto invalid;
			ch += byte;
			ch -= 0x03C82080U + SP_HALF_BASE;
			ucs_ch = (ch >> SP_HALF_SHIFT) + SP_HIGH_FIRST;
			if (ucs_ch < SP_HIGH_FIRST || ucs_ch > SP_HIGH_LAST)
				goto invalid;
			ucs_ch = (ch & SP_HALF_MASK) + SP_LOW_FIRST;
			if (ucs_ch < SP_LOW_FIRST || ucs_ch > SP_LOW_LAST)
				goto invalid;
			break;
		default:
			goto invalid;
		}
		
	}
	return (0);
invalid:
	return (EINVAL);
}

/*
 * utf8_normalizestr - Normalize a UTF-8 string (NFC or NFD)
 *
 * This function takes an UTF-8 input string, instr, of inlen bytes
 * and produces normalized UTF-8 output into a buffer of buflen bytes
 * pointed to by outstr. The size of the output in bytes (not including
 * a NULL termination byte) is returned in outlen. In-place conversions
 * are not supported (i.e. instr != outstr).]
 
 * FLAGS
 *    UTF_DECOMPOSED:  output string will be fully decomposed (NFD)
 *
 *    UTF_PRECOMPOSED:  output string will be precomposed (NFC)
 *
 *    UTF_NO_NULL_TERM:  do not add null termination to output string
 *
 *    UTF_ESCAPE_ILLEGAL:  percent escape any illegal UTF-8 input
 *
 * ERRORS
 *    ENAMETOOLONG:  output did not fit or input exceeded MAXPATHLEN bytes
 *
 *    EINVAL:  illegal UTF-8 sequence encountered or invalid flags
 */
int
utf8_normalizestr(const uint8_t* instr, size_t inlen, uint8_t* outstr,
                  size_t *outlen, size_t buflen, int flags)
{
	uint16_t unicodebuf[32];
	uint16_t* unistr = NULL;
	size_t unicode_bytes;
	size_t uft8_bytes;
	size_t inbuflen;
	uint8_t *outbufstart, *outbufend;
	const uint8_t *inbufstart;
	unsigned int byte;
	int decompose, precompose;
	int result = 0;

	if (flags & ~(UTF_DECOMPOSED | UTF_PRECOMPOSED | UTF_NO_NULL_TERM | UTF_ESCAPE_ILLEGAL)) {
		return (EINVAL);
	}
	decompose = (flags & UTF_DECOMPOSED);
	precompose = (flags & UTF_PRECOMPOSED);
	if ((decompose && precompose) || (!decompose && !precompose)) {
		return (EINVAL);
	}
	outbufstart = outstr;
	outbufend = outbufstart + buflen;
	inbufstart = instr;
	inbuflen = inlen;

	while (inlen-- > 0 && (byte = *instr++) != '\0') {
		if (outstr >= outbufend) {
			result = ENAMETOOLONG;
			goto exit;
		}
		if (byte >= 0x80) {
			goto nonASCII;
		}
		/* ASCII is already normalized. */
		*outstr++ = (uint8_t)byte;
	}
exit:
	*outlen = (size_t)(outstr - outbufstart);
	if (((flags & UTF_NO_NULL_TERM) == 0)) {
		if (outstr < outbufend)
			*outstr = '\0';
		else
			result = ENAMETOOLONG;
	}
	return (result);


	/* 
	 * Non-ASCII uses the existing utf8_encodestr/utf8_decodestr
	 * functions to perform the normalization.  Since this will
	 * presumably be used to normalize filenames in the back-end
	 * (on disk or over-the-wire), it should be fast enough.
	 */
nonASCII:

	/* Make sure the input size is reasonable. */
	if (inbuflen > PATH_MAX) {
		result = ENAMETOOLONG;
		goto exit;
	}
	/*
	 * Compute worst case Unicode buffer size.
	 *
	 * For pre-composed output, every UTF-8 input byte will be at
	 * most 2 Unicode bytes.  For decomposed output, 2 UTF-8 bytes
	 * (smallest composite char sequence) may yield 6 Unicode bytes
	 * (1 base char + 2 combining chars).
	 */
	unicode_bytes = precompose ? (inbuflen * 2) : (inbuflen * 3);

	if (unicode_bytes <= sizeof(unicodebuf))
		unistr = &unicodebuf[0];
	else {
		unistr = (uint16_t*)utfmalloc(unicode_bytes);
		if (!unistr) {
			result = ENOMEM;
			goto exit;
		}
	}

	/* Normalize the string. */
	result = utf8_decodestr(inbufstart, inbuflen, unistr, &unicode_bytes,
	                        unicode_bytes, 0, flags & ~UTF_NO_NULL_TERM);
	if (result == 0) {
		/* Put results back into UTF-8. */
		result = utf8_encodestr(unistr, unicode_bytes, outbufstart,
		                        &uft8_bytes, buflen, 0, UTF_NO_NULL_TERM);
		outstr = outbufstart + uft8_bytes;
	}
	if (unistr && unistr != &unicodebuf[0]) {
		utffree(unistr);
	}
	goto exit;
}


 /*
  * Unicode 3.2 decomposition code (derived from Core Foundation)
  */

STATIC_INLINE uint32_t
getmappedvalue32(const unicode_mappings32 *theTable, size_t numElem,
		uint16_t character)
{
	const unicode_mappings32 *p, *q, *divider;

	if ((character < theTable[0]._key) || (character > theTable[numElem-1]._key))
		return (0);

	p = theTable;
	q = p + (numElem-1);
	while (p <= q) {
		divider = p + ((q - p) >> 1);	/* divide by 2 */
		if (character < divider->_key) { q = divider - 1; }
		else if (character > divider->_key) { p = divider + 1; }
		else { return (divider->_value); }
	}
	return (0);
}

#define RECURSIVE_DECOMPOSITION	(1 << 15)
#define EXTRACT_COUNT(value)	(((value) >> 12) & 0x0007)

STATIC_INLINE uint16_t
getmappedvalue16(const unicode_mappings16 *theTable, size_t numElem,
		uint16_t character)
{
	const unicode_mappings16 *p, *q, *divider;

	if ((character < theTable[0]._key) || (character > theTable[numElem-1]._key))
		return (0);

	p = theTable;
	q = p + (numElem-1);
	while (p <= q) {
		divider = p + ((q - p) >> 1);	/* divide by 2 */
		if (character < divider->_key)
			q = divider - 1;
		else if (character > divider->_key)
			p = divider + 1;
		else
			return (divider->_value);
	}
	return (0);
}


static uint32_t
unicode_recursive_decompose(uint16_t character, uint16_t *convertedChars)
{
	uint16_t value;
	uint32_t length;
	uint16_t firstChar;
	uint16_t theChar;
	const uint16_t *bmpMappings;
	uint32_t usedLength;

	value = getmappedvalue16(__CFUniCharDecompositionTable, __UniCharDecompositionTableLength, character);
	length = EXTRACT_COUNT(value);
	firstChar = (uint16_t)(value & 0x0FFF);
	theChar = firstChar;
	bmpMappings = (length == 1 ? &theChar : __CFUniCharMultipleDecompositionTable + firstChar);
	usedLength = 0;

	if (value & RECURSIVE_DECOMPOSITION) {
	    usedLength = unicode_recursive_decompose((uint16_t)*bmpMappings, convertedChars);
	
	    --length;	/* Decrement for the first char */
	    if (!usedLength)
	    	return 0;
	    ++bmpMappings;
	    convertedChars += usedLength;
	}
	
	usedLength += length;
	
	while (length--)
		*(convertedChars++) = *(bmpMappings++);
	
	return (usedLength);
}
    
#define HANGUL_SBASE 0xAC00
#define HANGUL_LBASE 0x1100
#define HANGUL_VBASE 0x1161
#define HANGUL_TBASE 0x11A7

#define HANGUL_SCOUNT 11172
#define HANGUL_LCOUNT 19
#define HANGUL_VCOUNT 21
#define HANGUL_TCOUNT 28
#define HANGUL_NCOUNT (HANGUL_VCOUNT * HANGUL_TCOUNT)

/*
 * unicode_decompose - decompose a composed Unicode char
 *
 * Composed Unicode characters are forbidden on
 * HFS Plus volumes. ucs_decompose will convert a
 * composed character into its correct decomposed
 * sequence.
 *
 * Similar to CFUniCharDecomposeCharacter
 */
static int
unicode_decompose(uint16_t character, uint16_t *convertedChars)
{
	if ((character >= HANGUL_SBASE) &&
	    (character <= (HANGUL_SBASE + HANGUL_SCOUNT))) {
		uint32_t length;

		character -= HANGUL_SBASE;
		length = (character % HANGUL_TCOUNT ? 3 : 2);

		*(convertedChars++) =
			(uint16_t)(character / HANGUL_NCOUNT + HANGUL_LBASE);
		*(convertedChars++) =
			(uint16_t)((character % HANGUL_NCOUNT) / HANGUL_TCOUNT + HANGUL_VBASE);
		if (length > 2)
			*convertedChars = (uint16_t)((character % HANGUL_TCOUNT) + HANGUL_TBASE);
		return ((int)length);
	} else {
		return ((int)unicode_recursive_decompose(character, convertedChars));
	}
}

/*
 * unicode_combine - generate a precomposed Unicode char
 *
 * Precomposed Unicode characters are required for some volume
 * formats and network protocols.  unicode_combine will combine
 * a decomposed character sequence into a single precomposed
 * (composite) character.
 *
 * Similar toCFUniCharPrecomposeCharacter but unicode_combine
 * also handles Hangul Jamo characters.
 */
static uint16_t
unicode_combine(uint16_t base, uint16_t combining)
{
	uint32_t value;

	/* Check HANGUL */
	if ((combining >= HANGUL_VBASE) && (combining < (HANGUL_TBASE + HANGUL_TCOUNT))) {
		/* 2 char Hangul sequences */
		if ((combining < (HANGUL_VBASE + HANGUL_VCOUNT)) &&
		    (base >= HANGUL_LBASE && base < (HANGUL_LBASE + HANGUL_LCOUNT))) {
		    return ((uint16_t)(HANGUL_SBASE +
		            ((base - HANGUL_LBASE)*(HANGUL_VCOUNT*HANGUL_TCOUNT)) +
		            ((combining  - HANGUL_VBASE)*HANGUL_TCOUNT)));
		}
	
		/* 3 char Hangul sequences */
		if ((combining > HANGUL_TBASE) &&
		    (base >= HANGUL_SBASE && base < (HANGUL_SBASE + HANGUL_SCOUNT))) {
			if ((base - HANGUL_SBASE) % HANGUL_TCOUNT)
				return (0);
			else
				return ((uint16_t)(base + (combining - HANGUL_TBASE)));
		}
	}

	value = getmappedvalue32(__CFUniCharPrecompSourceTable, __CFUniCharPrecompositionTableLength, combining);
	if (value) {
		value = getmappedvalue16(__CFUniCharBMPPrecompDestinationTable + (value & 0xFFFF), (value >> 16), base);
	}
	return ((uint16_t)value);
}


/*
 * priortysort - order combining chars into canonical order
 *
 * Similar to CFUniCharPrioritySort
 */
static void
priortysort(uint16_t* characters, int count)
{
	uint32_t p1, p2;
	uint16_t *ch1, *ch2;
	uint16_t *end;
	int changes;
	uint16_t tmp, tmp2;

	end = characters + count;
	do {
		changes = 0;
		ch1 = characters;
		ch2 = characters + 1;
		tmp = ReadHost16(ch1);
		p2 = get_combining_class(tmp);
		while (ch2 < end) {
			p1 = p2;
			tmp = ReadHost16(ch2);
			p2 = get_combining_class(tmp);
			if (p1 > p2 && p2 != 0) {
				tmp  = ReadHost16(ch1);
				tmp2 = ReadHost16(ch2);
				WriteHost16(ch1, tmp2);
				WriteHost16(ch2, tmp);
				changes = 1;
				
				/*
				 * Make sure that p2 contains the combining class for the
				 * character now stored at *ch2.  This isn't required for
				 * correctness, but it will be more efficient if a character
				 * with a large combining class has to "bubble past" several
				 * characters with lower combining classes.
				 */
				p2 = p1;
			}
			++ch1;
			++ch2;
		}
	} while (changes);
}


/*
 * Invalid NTFS filename characters are encodeded using the
 * SFM (Services for Macintosh) private use Unicode characters.
 *
 * These should only be used for SMB, MSDOS or NTFS.
 *
 *    Illegal NTFS Char   SFM Unicode Char
 *  ----------------------------------------
 *    0x01-0x1f           0xf001-0xf01f
 *    '"'                 0xf020
 *    '*'                 0xf021
 *    '/'                 0xf022
 *    '<'                 0xf023
 *    '>'                 0xf024
 *    '?'                 0xf025
 *    '\'                 0xf026
 *    '|'                 0xf027
 *    ' '                 0xf028  (Only if last char of the name)
 *    '.'                 0xf029  (Only if last char of the name)
 *  ----------------------------------------
 *
 *  Reference: http://support.microsoft.com/kb/q117258/
 */

#define MAX_SFM2MAC           0x29
#define SFMCODE_PREFIX_MASK   0xf000 

/*
 * In the Mac OS 9 days the colon was illegal in a file name. For that reason
 * SFM had no conversion for the colon. There is a conversion for the
 * slash. In Mac OS X the slash is illegal in a file name. So for us the colon
 * is a slash and a slash is a colon. So we can just replace the slash with the
 * colon in our tables and everything will just work. 
 */
static uint8_t
sfm2mac[42] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,   /* 00 - 07 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,   /* 08 - 0F */
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,   /* 10 - 17 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,   /* 18 - 1F */
	0x22, 0x2a, 0x3a, 0x3c, 0x3e, 0x3f, 0x5c, 0x7c,   /* 20 - 27 */
	0x20, 0x2e                                        /* 28 - 29 */
};

static uint8_t
mac2sfm[112] = {
	0x20, 0x21, 0x20, 0x23, 0x24, 0x25, 0x26, 0x27,	  /* 20 - 27 */
	0x28, 0x29, 0x21, 0x2b, 0x2c, 0x2d, 0x2e, 0x22,   /* 28 - 2f */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,   /* 30 - 37 */
	0x38, 0x39, 0x22, 0x3b, 0x23, 0x3d, 0x24, 0x25,   /* 38 - 3f */
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,   /* 40 - 47 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,   /* 48 - 4f */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,   /* 50 - 57 */
	0x58, 0x59, 0x5a, 0x5b, 0x26, 0x5d, 0x5e, 0x5f,   /* 58 - 5f */
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,   /* 60 - 67 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,   /* 68 - 6f */
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,   /* 70 - 77 */
	0x78, 0x79, 0x7a, 0x7b, 0x27, 0x7d, 0x7e, 0x7f    /* 78 - 7f */
};


/*
 * Encode illegal NTFS filename characters into SFM Private Unicode characters
 *
 * Assumes non-zero ASCII input.
 */
static uint16_t
ucs_to_sfm(uint16_t ucs_ch, int lastchar)
{
	/* The last character of filename cannot be a space or period. */
	if (lastchar) {
		if (ucs_ch == 0x20)
			return (0xf028);
		else if (ucs_ch == 0x2e)
			return (0xf029);
	}
	/* 0x01 - 0x1f is simple transformation. */
	if (ucs_ch <= 0x1f) {
		return ((uint16_t)(ucs_ch | 0xf000));
	} else /* 0x20 - 0x7f */ {
		uint16_t lsb;

		lsb = mac2sfm[ucs_ch - 0x0020];
		if (lsb != ucs_ch)
			return((uint16_t)(0xf000 | lsb)); 
	}
	return (ucs_ch);
}

/*
 * Decode any SFM Private Unicode characters
 */
static uint16_t
sfm_to_ucs(uint16_t ucs_ch)
{
	if (((ucs_ch & 0xffC0) == SFMCODE_PREFIX_MASK) && 
	    ((ucs_ch & 0x003f) <= MAX_SFM2MAC)) {
		ucs_ch = sfm2mac[ucs_ch & 0x003f];
	}
	return (ucs_ch);
}

/*
 * utf32_to_utf16 - Converts a UTF-16 string to UTF-32. Ported from public Unicode, Inc. code.
 *
 * This function takes a UTF-16 input string, inUTF16, of inUTF16ByteLen 
 * bytes and produces the UTF-32 output into a buffer of inUTF32BufByteLen 
 * bytes pointed to by inUTF32Buf. The size of the output in bytes is 
 * returned in outUTF32ByteLen. The UTF-32 string is *not* null terminated.
 *
 * NOTES:
 *    The resulting UTF-32 string is not NULL terminated.
 *
 * input flags:
 *    UTF_STRICT: Fail on irregular sequences and isolated surrogates.
 *
 * result:
 *    ENAMETOOLONG: Name didn't fit; only buflen bytes were encoded
 *
 *    EINVAL: Illegal char found.
 */
int
utf16_to_utf32(const uint16_t *inUTF16, size_t inUTF16ByteLen, 
               uint32_t *inUTF32Buf, size_t *outUTF32ByteLen, size_t inUTF32BufByteLen, 
               int inFlags)
{
	int result = 0;
	const uint16_t* source = inUTF16;
	const uint16_t* sourceEnd = (const uint16_t*)(((const uint8_t*)source) + inUTF16ByteLen);
	uint32_t* target = inUTF32Buf;
	uint32_t* targetEnd = (uint32_t*)(((uint8_t*)target) + inUTF32BufByteLen);
	uint32_t ch, ch2;
	while (source < sourceEnd) {
		ch = *source++;
		/* If we have a surrogate pair, convert to UTF-32 first. */
		if (ch >= SP_HIGH_FIRST && ch <= SP_HIGH_LAST) {
			/* If the 16 bits following the high surrogate are in the source buffer... */
			if (source < sourceEnd) {
				ch2 = *source;
				/* If it's a low surrogate, convert to UTF-32. */
				if (ch2 >= SP_LOW_FIRST && ch2 <= SP_LOW_LAST) {
					ch = ((ch - SP_HIGH_FIRST) << SP_HALF_SHIFT) + (ch2 - SP_LOW_FIRST) + SP_HALF_BASE;
					++source;
				} else if (inFlags & UTF_STRICT) { /* it's an unpaired high surrogate */
					--source; /* return to the illegal value itself */
					result = EINVAL;
					break;
				}
			} else { /* We don't have the 16 bits following the high surrogate. */
				--source; /* return to the high surrogate */
				result = EINVAL;
				break;
			}
		} else if (inFlags & UTF_STRICT) {
			/* UTF-16 surrogate values are illegal in UTF-32 */
			if (ch >= SP_LOW_FIRST && ch <= SP_LOW_LAST) {
				--source; /* return to the illegal value itself */
				result = EINVAL;
				break;
			}
		}
		if (target >= targetEnd) {
			result = ENAMETOOLONG;
			break;
		}
		*target++ = ch;
	}
	*outUTF32ByteLen = (size_t)(((uint8_t*)target) - ((uint8_t*)inUTF32Buf));
	return result;
}

/*
 * utf8_encodestr_copy - Encodes a UTF-16 string into UTF-8 into a malloc'd buffer.
 *
 * NOTES:
 *    On success, the caller must use utffree to free the UTF-8 buffer when no longer needed.
 *    Otherwise, the same notes as utf8_encodestr.
 *
 * FLAGS:
 *    Same flags as utf8_encodestr.
 *
 * ERRORS:
 *    Same errors as utf8_encodestr.
 */
int
utf8_encodestr_copy( const uint16_t *inUTF16, size_t inUTF16Bytes, 
					 void *outUTF8, size_t *outUTF8Bytes, 
					 uint16_t inAltSlash, int inFlags )
{
	int			err;
	size_t		utf8Bytes;
	uint8_t *	utf8;
	
	utf8Bytes = utf8_encodelen(inUTF16, inUTF16Bytes, inAltSlash, inFlags ) + 1;
	utf8 = (uint8_t *) utfmalloc( utf8Bytes );
	if( !utf8 ) { err = ENOMEM; goto exit; }
	
	err = utf8_encodestr( inUTF16, inUTF16Bytes, utf8, &utf8Bytes, utf8Bytes, inAltSlash, inFlags );
	if( err ) goto exit;
	
	*( (uint8_t **) outUTF8 )			= utf8;
	if( outUTF8Bytes ) *outUTF8Bytes	= utf8Bytes;
	utf8 = NULL;
	
exit:
	if( utf8 ) utffree( utf8 );
	return( err );
}

/*
 * utf8_decodestr_copy - Decodes a UTF-8 string into UTF-16 into a malloc'd buffer.
 *
 * NOTES:
 *    On success, the caller must use utffree to free the UTF-16 buffer when no longer needed.
 *    Otherwise, the same notes as utf8_decodestr.
 *
 * FLAGS:
 *    Same flags as utf8_decodestr.
 *
 * ERRORS:
 *    Same errors as utf8_decodestr.
 */
int
utf8_decodestr_copy(
	const void *inUTF8Ptr, size_t inUTF8Bytes, 
	void *outUTF16, size_t *outUTF16Bytes, 
	uint16_t inAltslash, int inFlags )
{
	OSStatus			err;
	const uint8_t *		utf8;
	uint16_t *			utf16Ptr;
	size_t				utf16Len;
	
	utf16Len = inUTF8Bytes * sizeof( uint16_t );
	utf16Ptr = (uint16_t *) utfmalloc( utf16Len + sizeof( uint16_t ) );
	if( !utf16Ptr ) { err = ENOMEM; goto exit; }
	
	utf8 = (const uint8_t *) inUTF8Ptr;
	err = utf8_decodestr( utf8, inUTF8Bytes, utf16Ptr, &utf16Len, utf16Len, inAltslash, inFlags );
	if( err ) goto exit;
	utf16Ptr[ utf16Len / sizeof( uint16_t ) ] = 0;
	
	*( (uint16_t **) outUTF16 )			= utf16Ptr;
	if( outUTF16Bytes ) *outUTF16Bytes	= utf16Len;
	utf16Ptr = NULL;
	
exit:
	if( utf16Ptr ) utffree( utf16Ptr );
	return( err );
}

/*
 * latin1_to_utf8 - Converts Windows Latin-1 to UTF-8 into a malloc'd buffer.
 *
 * NOTES:
 *    On success, the caller must use utffree to free the UTF-8 buffer when no longer needed.
 *    Otherwise, the same notes as utf8_encodestr.
 *
 * FLAGS:
 *    Same flags as utf8_encodestr.
 *
 * ERRORS:
 *    Same errors as utf8_encodestr.
 */
int
	latin1_to_utf8_copy( 
		const void *	inLatin1Ptr, 
		size_t			inLatin1Len, 
		void *			outUTF8Ptr, 
		size_t *		outUTF8Len, 
		uint16_t		inAltSlash, 
		int				inFlags )
{
	int					err;
	const uint8_t *		src;
	const uint8_t *		end;
	size_t				utf16len;
	uint16_t *			utf16buf;
	uint16_t *			utf16ptr;
	
	// Convert Windows Latin-1 (ANSI Codepage 1252) to UTF-16 in a temporary buffer.
	
	utf16len = inLatin1Len * 2;
	utf16buf = (uint16_t *) utfmalloc( utf16len + 2 );
	if( !utf16buf ) { err = ENOMEM; goto exit; };
	static_analyzer_mem_zeroed( utf16buf, utf16len ); // Remove when <radar:15309659> is fixed.
	utf16ptr = utf16buf;
	
	src = (const uint8_t *) inLatin1Ptr;
	end = src + inLatin1Len;
	while( src != end )
	{
		*utf16ptr++ = kWindowsLatin1ToUTF16[ *src++ ];
	}
	
	// Convert UTF-16 to UTF-8 into a new buffer.
	
	err = utf8_encodestr_copy( utf16buf, utf16len, outUTF8Ptr, outUTF8Len, inAltSlash, inFlags );
	
exit:
	if( utf16buf ) utffree( utf16buf );
	return( err );
}

/*
	macroman_to_utf8 - Converts MacRoman to UTF-8 into a malloc'd buffer.
	
	NOTES:
		On success, the caller must use utffree to free the UTF-8 buffer when no longer needed.
		Otherwise, the same notes as utf8_encodestr.
	
	FLAGS:
		Same flags as utf8_encodestr.
	
	ERRORS:
		Same errors as utf8_encodestr.
*/
int
	macroman_to_utf8_copy( 
		const void *	inMacRomanPtr, 
		size_t			inMacRomanLen, 
		void *			outUTF8Ptr, 
		size_t *		outUTF8Len, 
		uint16_t		inAltSlash, 
		int				inFlags )
{
	int					err;
	const uint8_t *		src;
	const uint8_t *		end;
	size_t				utf16len;
	uint16_t *			utf16buf;
	uint16_t *			utf16ptr;
	
	// Convert MacRoman to UTF-16 into a temporary buffer.
	
	utf16len = inMacRomanLen * 2;
	utf16buf = (uint16_t *) utfmalloc( utf16len + 2 );
	if( !utf16buf ) { err = ENOMEM; goto exit; };
	static_analyzer_mem_zeroed( utf16buf, utf16len );
	utf16ptr = utf16buf;
	
	src = (const uint8_t *) inMacRomanPtr;
	end = src + inMacRomanLen;
	while( src < end )
	{
		*utf16ptr++ = kMacRomanToUTF16[ *src++ ];
	}
	
	// Convert UTF-16 to UTF-8 into a new buffer.
	
	err = utf8_encodestr_copy( utf16buf, utf16len, outUTF8Ptr, outUTF8Len, inAltSlash, inFlags );
	
exit:
	if( utf16buf ) utffree( utf16buf );
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	utfconv_Test
//===========================================================================================================================

OSStatus	utfconv_Test( void )
{
	OSStatus			err;
	const char *		src;
	size_t				len;
	const char *		expectedPtr;
	size_t				expectedLen;
	char *				resultPtr;
	size_t				resultLen;
	
	resultPtr = NULL;
	
	src = "test" "\xA9\xAA\xA8\xF0\x9F\xA5" "end";
	len = strlen( src );
	expectedPtr = "test" "\xc2\xa9\xe2\x84\xa2\xc2\xae\xef\xa3\xbf\xc3\xbc\xe2\x80\xa2" "end";
	expectedLen = strlen( expectedPtr );
	err = macroman_to_utf8_copy( src, len, &resultPtr, &resultLen, '/', 0 );
	require_noerr( err, exit );
	require_action( ( resultLen == expectedLen ) && ( memcmp( resultPtr, expectedPtr, resultLen ) == 0 ), exit, err = -1 );
	utffree( resultPtr );
	resultPtr = NULL;
	
exit:
	if( resultPtr ) utffree( resultPtr );
	printf( "utfconv_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
