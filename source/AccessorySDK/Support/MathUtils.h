/*
	File:    	MathUtils.h
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
	
	Copyright (C) 2001-2014 Apple Inc. All Rights Reserved.
*/

#ifndef	__MathUtils_h__
#define	__MathUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AverageCeil64 / AverageFloor64
	@abstract	Average of two 64-bit values "( x + y ) / 2" without causing overflow.
	@discussion	Based on code from the book Hacker's Delight.
*/
STATIC_INLINE uint64_t	AverageCeil64( uint64_t x, uint64_t y )
{
	return( ( x | y ) - ( ( x ^ y ) >> 1 ) );
}

STATIC_INLINE uint64_t	AverageFloor64( uint64_t x, uint64_t y )
{
	return( ( x & y ) + ( ( x ^ y ) >> 1 ) );
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CountDigits32 / CountDigits64
	@abstract	Returns the number of digits in the number at the specified base (e.g. 999 base 10 has 3 digits).
*/
uint8_t	CountDigits32( uint32_t inNum, uint32_t inBase );
uint8_t	CountDigits64( uint64_t inNum, uint64_t inBase );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	gcd64
	@abstract	Finds the Greatest Common Denominator using Euclid's algorithm.
*/
int64_t	gcd64( int64_t a, int64_t b );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	iceil2
	@abstract	Rounds up to an integral power of 2 <= x. 0 and 1 are considered powers of 2.
	@discussion
	
	iceil2    is valid for 0 >= x <= 2^31.
	iceil2_64 is valid for 0 >= x <= 2^63.
*/
uint32_t	iceil2( uint32_t x );
uint64_t	iceil2_64( uint64_t x );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ifloor2
	@abstract	Rounds down to an integral power of 2 >= x. 0 and 1 are considered powers of 2.
	@discussion
	
	ifloor2    is valid for 0 >= x <= 2^31.
	ifloor2_64 is valid for 0 >= x <= 2^63.
*/
uint32_t	ifloor2( uint32_t x );
uint64_t	ifloor2_64( uint64_t x );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ilog2
	@abstract	Integer logarithm base 2. Note: ilog2( 0 ) returns 0. Valid for 0 >= x <= 2^32-1.
*/
#if( TARGET_VISUAL_STUDIO_2005_OR_LATER )

	STATIC_INLINE uint8_t	ilog2( uint32_t x )
	{
		unsigned long		i;
		
		// _BitScanReverse is undefined for 0 so OR in 1 so ilog2( x <= 1 ) is 0. Faster than "if( x != 0 ) ...".
		
		_BitScanReverse( &i, (unsigned long)( x | 1 ) );
		return( (uint8_t) i );
	}
	
	#define HAS_INLINE_ILOG2		1

#elif( TARGET_HAS_BUILTIN_CLZ )

	STATIC_INLINE uint8_t	ilog2( uint32_t x )
	{
		// __builtin_clz is undefined for 0 so OR in 1 so ilog2( x <= 1 ) is 0. Faster than "if( x != 0 ) ...".
		
		return( (uint8_t)( 31 - __builtin_clz( x | 1 ) ) );
	}
	
	#define HAS_INLINE_ILOG2		1

#else
	uint8_t	ilog2( uint32_t x );
	
	#define HAS_INLINE_ILOG2		0
#endif

uint8_t	ilog2_64( uint64_t x );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ipow10
	@abstract	Raise 10 to an integer exponent.
*/
uint32_t	ipow10( uint32_t inExponent );
uint64_t	ipow10_64( uint64_t inExponent );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	isin
	@abstract	Integer sine approximation.
	
	@param		x	Input in the range -1.0 to +1.0 in Q14 format.
	@result		Output in Q14 format.
	
	@discussion
	
	Note that this approximates sin(pi/2 * x) so to make this work like the ANSI C sin(), you need to normalize from 
	a range of 0 to 2pi to -1.0 to 1.0. Since the result is for one quadrant, you need to multiply it by -4.
	
	See FloatToQ/QToFloat macros for converting between floating point and fixed point.
*/
int32_t	isin( int32_t x );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		128-bit signed math.
	@abstract	Functions for working with 128-bit signed integers.
*/

// r = ( hi << 64 ) | lo
STATIC_INLINE void	int128_set( int128_compat * const r, int64_t hi, uint64_t lo )
{
	r->hi = hi;
	r->lo = lo;
}

// r = x + y
STATIC_INLINE void	int128_add( int128_compat * const r, const int128_compat * const x, const int128_compat * const y )
{
	uint64_t oldLo = x->lo;
	r->lo = oldLo + y->lo;
	r->hi = x->hi + y->hi + ( oldLo > r->lo );
}

// r = x + lo
STATIC_INLINE void	int128_addlo( int128_compat * const r, const int128_compat * const x, uint64_t lo )
{
	uint64_t oldLo = x->lo;
	r->lo = oldLo + lo;
	r->hi = x->hi + ( oldLo > r->lo );
}

// r = x - y
STATIC_INLINE void	int128_sub( int128_compat * const r, const int128_compat * const x, const int128_compat * const y )
{
	uint64_t oldLo = x->lo;
	r->lo = oldLo - y->lo;
	r->hi = x->hi - ( y->hi + ( oldLo < r->lo ) );
}

// r <<= n
STATIC_INLINE void	int128_lshift( int128_compat * const r, uint8_t n )
{
	check( n < 64 );
	r->hi = (int64_t)( ( (uint64_t)( r->hi << n ) ) | ( r->lo >> ( 64 - n ) ) );
	r->lo <<= n;
}

// r >>= n (with sign-fill)
STATIC_INLINE void	int128_rshift( int128_compat * const r, uint8_t n )
{
	check( n < 64 );
	r->lo = ( r->lo >> n ) | ( (uint64_t)( r->hi << ( 64 - n ) ) );
	r->hi = ASR64( r->hi, n );
}

// x < y
STATIC_INLINE int	int128_lt( int128_compat * const x, int128_compat * const y )
{
	return( ( x->hi < y->hi ) || ( ( x->hi == y->hi ) && ( x->lo < y->lo ) ) );
}

// x > y
STATIC_INLINE int	int128_gt( int128_compat * const x, int128_compat * const y )
{
	return( ( x->hi > y->hi ) || ( ( x->hi == y->hi ) && ( x->lo > y->lo ) ) );
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Multiply64x64
	@abstract	Multiplies two 64-bit unsigned integers and returns the upper and lower halfs of a 128-bit result.
*/
void	Multiply64x64( uint64_t a, uint64_t b, uint64_t *outHi, uint64_t *outLo );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TranslateValue
	@abstract	Translates a value from one range to another (e.g. -30.0 to 0.0 translated to 0.0 to 1.0).
*/
double	TranslateValue( double inValue, double inOldMin, double inOldMax, double inNewMin, double inNewMax );

#if 0
#pragma mark == Statistics ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	Average
	@abstract	Determines average of an array of numbers.
*/
double	Average( const double x[], size_t n );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	EuclideanDistance
	@abstract	Determines Euclidean distance between two arrays of numbers.
	@discussion	See <http://en.wikipedia.org/wiki/Euclidean_distance>.
*/
double	EuclideanDistance( const double x[], const double *y, int n );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Exponentially Weighted Moving Average (EWMA) for floating point.
	@abstract	See <http://en.wikipedia.org/wiki/Moving_average>.
*/
#define kEWMAFlags_StartWithFirstValue		0x00
#define kEWMAFlags_StartWithZero			0x01

typedef struct
{
	double			alpha;
	double			average;
	unsigned char	flags;	
	
}	EWMA_FP_Data;

#define EWMA_FP_Init( DATA, ALPHA, FLAGS ) \
	do \
	{ \
		(DATA)->alpha   = (ALPHA); \
		(DATA)->average = 0; \
		(DATA)->flags   = (FLAGS); \
		\
	}	while( 0 )

#define EWMA_FP_Update( DATA, X ) \
	do \
	{ \
		if( (DATA)->flags ) \
		{ \
			(DATA)->average += ( (DATA)->alpha * ( (X) - (DATA)->average ) ); \
		} \
		else \
		{ \
			(DATA)->average = (X); \
			(DATA)->flags = 1; \
		} \
		\
	}	while( 0 )

#define EWMA_FP_Get( DATA )		(DATA)->average

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	InterquartileRange
	@abstract	Calculates the Interquartile range and upper and lower outlier ranges for an array of values.
	@discussion	<http://en.wikipedia.org/wiki/Interquartile_Range>.
				Note: "tmp" may be the same as "x". "tmp" may be NULL if "set" is already sorted.
*/
double	InterquartileRange( const double *set, double *tmp, size_t n, double *outLower, double *outUpper );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MedianAbsoluteDeviation
	@abstract	Calculates the median absolute deviation of an array of values.
	@discussion	See <http://en.wikipedia.org/wiki/Median_absolute_deviation>.
				Note: "tmp1" may be NULL if "set" is already sorted.
				Note: If "tmp1" is specified and not the same as "tmp2", the result will be sorted.
				Note: "tmp1" and "set" may be the same, but the contents will be modified.
				Note: "tmp1" and "tmp2" may be the same, but "tmp1" won't end up sorted.
*/
double	MedianAbsoluteDeviation( const double *set, double *tmp1, double *tmp2, size_t n, double *outMedian );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MovingAverageF
	@abstract	Floating point exponentially weighted moving average (EWMA).
	@discussion	See <http://en.wikipedia.org/wiki/Moving_average>.
	
	@param		AVG		Current average. This is often initialized to the first value.
	@param		X		New value to add to the average.
	@param		ALPHA	Smoothing factor between 0 and 1. To express in time periods: ALPHA = 2/(N+1), such as N=19: 2/(19/1) = 0.1.
	
	@result		New average.
*/
#define MovingAverageF( AVG, X, ALPHA )		( ( (AVG) * ( 1 - (ALPHA) ) ) + ( (X) * (ALPHA) ) )
#define MovingAveragePeriodsToAlpha( N )	( 2 / ( (N) + 1 ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MovingAverageGet / MovingAverageUpdate
	@abstract	Integer exponentially weighted moving average (EWMA).
	@discussion	See <http://en.wikipedia.org/wiki/Moving_average>.
	
	@param		SCALED_AVG		Current scaled up average.
	@param		X				New value to add to the average.
	@param		SCALE_FACTOR	Multiplier to scale up the average to minimize integer truncation.
	@param		PERIOD			Divisor to control how many steps before a value fades away.
	
	@result		New scaled up average.
*/
#define MovingAverageGet( SCALED_AVG, SCALE_FACTOR, PERIOD ) \
	( ( (SCALED_AVG) + ( ( (SCALE_FACTOR) * (PERIOD) ) / 2 ) ) / ( (SCALE_FACTOR) * (PERIOD) ) )

#define MovingAverageUpdate( SCALED_AVG, X, SCALE_FACTOR, PERIOD ) \
	( ( (SCALED_AVG) + ( (X) * (SCALE_FACTOR) ) ) - ( ( (SCALED_AVG) + ( (PERIOD) / 2 ) ) / (PERIOD) ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PearsonCorrelation
	@abstract	Determines Pearson product-moment correlation coefficient of two arrays of numbers.
	@discussion	See <http://en.wikipedia.org/wiki/Pearson_product-moment_correlation_coefficient>
	@result		-1 negative correlation <=> 0 not correlated <=> 1 correlated.
*/
double	PearsonCorrelation( const double x[], const double *y, int n );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	StandardDeviation
	@abstract	Calculates the Sample Standard Deviation or Population Standard Deviation of an array of values.
	@discussion	See <http://en.wikipedia.org/wiki/Standard_deviation>.
	
	@param		inSample	If true, calculate a Sample Standard Deviation.
							If false, calculate a Population Standard Deviation.
*/
double	StandardDeviation( const double *x, size_t n, Boolean inSample );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TorbenMedian
	@abstract	Median function that works with constant arrays.
	@discussion
	
	Algorithm by Torben Mogensen, implementation by N. Devillard.
	See <http://ndevilla.free.fr/median/median/median.html> for more information and alternate implementations.
*/
int32_t	TorbenMedian32( const int32_t m[], int n );
int64_t	TorbenMedian64( const int64_t m[], int n );
double	TorbenMedianF( const double m[], int n );

#if 0
#pragma mark == Debugging ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	MathUtils_Test
	@abstract	Unit test.
*/
#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	MathUtils_Test( int inPrint );
#endif

#ifdef __cplusplus
}
#endif

#endif // __MathUtils_h__
