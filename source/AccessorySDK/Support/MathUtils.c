/*
	File:    	MathUtils.c
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

#include "MathUtils.h"

#include <float.h>
#include <limits.h>
#include <math.h>

#include "CommonServices.h"

//===========================================================================================================================
//	CountDigits
//===========================================================================================================================

uint8_t	CountDigits32( uint32_t inNum, uint32_t inBase )
{
	uint8_t		digits;
	
	digits = 0;
	do
	{
		++digits;
		inNum /= inBase;
	
	}	while( inNum != 0 );
	return( digits );
}

uint8_t	CountDigits64( uint64_t inNum, uint64_t inBase )
{
	uint8_t		digits;
	
	digits = 0;
	do
	{
		++digits;
		inNum /= inBase;
	
	}	while( inNum != 0 );
	return( digits );
}

//===========================================================================================================================
//	gcd64
//
//	Finds the Greatest Common Denominator using Euclid's algorithm.
//===========================================================================================================================

int64_t	gcd64( int64_t a, int64_t b )
{
	int64_t		c;
	
	while( a > 0 )
	{
		if( a < b )
		{
			 c = a;
			 a = b;
			 b = c;
		}
		a -= b;
	}
	return( b );
}

//===========================================================================================================================
//	iceil2
//
//	Rounds x up to the closest integral power of 2. Based on code from the book Hacker's Delight.
//===========================================================================================================================

uint32_t	iceil2( uint32_t x )
{
	check( x <= UINT32_C( 0x80000000 ) );
	
	x = x - 1;
	x |= ( x >> 1 );
	x |= ( x >> 2 );
	x |= ( x >> 4 );
	x |= ( x >> 8 );
	x |= ( x >> 16 );
	return( x + 1 );
}

uint64_t	iceil2_64( uint64_t x )
{
	check( x <= UINT64_C( 0x8000000000000000 ) );
	
	x = x - 1;
	x |= ( x >> 1 );
	x |= ( x >> 2 );
	x |= ( x >> 4 );
	x |= ( x >> 8 );
	x |= ( x >> 16 );
	x |= ( x >> 32 );
	return( x + 1 );
}

//===========================================================================================================================
//	ifloor2
//
//	Rounds x down to the closest integral power of 2. Based on code from the book Hacker's Delight.
//===========================================================================================================================

uint32_t	ifloor2( uint32_t x )
{
	x |= ( x >> 1 );
	x |= ( x >> 2 );
	x |= ( x >> 4 );
	x |= ( x >> 8 );
	x |= ( x >> 16 );
	return( x - ( x >> 1 ) );
}

uint64_t	ifloor2_64( uint64_t x )
{
	x |= ( x >> 1 );
	x |= ( x >> 2 );
	x |= ( x >> 4 );
	x |= ( x >> 8 );
	x |= ( x >> 16 );
	x |= ( x >> 32 );
	return( x - ( x >> 1 ) );
}

//===========================================================================================================================
//	ilog2
//
//	Integer logarithm base 2. Note: ilog2( 0 ) returns 0. Based on code from the book Hacker's Delight.
//===========================================================================================================================

#if( !HAS_INLINE_ILOG2 )

#if( COMPILER_VISUAL_CPP )
	#pragma warning( disable:4146 )	// Disable "unary minus operator applied to unsigned type, result still unsigned".
#endif

uint8_t	ilog2(uint32_t x)
{
	uint32_t y, m, n;

	y = -(x >> 16);		// If left half of x is 0,
	m = (y >> 16) & 16;	// set n = 16.  If left half
	n = 16 - m;			// is nonzero, set n = 0 and
	x = x >> m;			// shift x right 16.
						// Now x is of the form 0000xxxx.
	y = x - 0x100;		// If positions 8-15 are 0,
	m = (y >> 16) & 8;	// add 8 to n and shift x left 8.
	n = n + m;
	x = x << m;

	y = x - 0x1000;		// If positions 12-15 are 0,
	m = (y >> 16) & 4;	// add 4 to n and shift x left 4.
	n = n + m;
	x = x << m;

	y = x - 0x4000;		// If positions 14-15 are 0,
	m = (y >> 16) & 2;	// add 2 to n and shift x left 2.
	n = n + m;
	x = x << m;

	y = x >> 14;		// Set y = 0, 1, 2, or 3.
	m = y & ~(y >> 1);	// Set m = 0, 1, 2, or 2 resp.
	x = n + 2 - m;
	
	return( (uint8_t)( 31 - x ) );
}

#if( COMPILER_VISUAL_CPP )
	#pragma warning( default:4201 )	// Re-enable "unary minus operator applied to unsigned type, result still unsigned".
#endif

#endif // !HAS_INLINE_ILOG2

uint8_t	ilog2_64( uint64_t x )
{
	uint32_t		y;
	
	if( x <= UINT32_MAX )
	{
		return( ilog2( (uint32_t) x ) );
	}
	
	for( y = 0; ( x >>= 1 ) != 0; ) ++y;
	return( (uint8_t) y );
}

//===========================================================================================================================
//	ipow10
//===========================================================================================================================

uint32_t	ipow10( uint32_t inExponent )
{
	uint32_t		x;
	
	check( inExponent <= 9 );
	if( inExponent <= 9 )	for( x = 1; inExponent-- > 0; x *= 10 ) {}
	else					x = 1000000000; // Saturate at the largest power of 10 (10^9).
	return( x );
}

uint64_t	ipow10_64( uint64_t inExponent )
{
	uint64_t		x;
	
	check( inExponent <= 19 );
	if( inExponent <= 19 )	for( x = 1; inExponent-- > 0; x *= 10 ) {}
	else					x = UINT64_C( 10000000000000000000 ); // Saturate at the largest power of 10 (10^19).
	return( x );
}

//===========================================================================================================================
//	isin
//
//	Integer sine approximation. Based on isin_S4() code from <http://www.coranac.com/2009/07/sines/>
//===========================================================================================================================

#define ISIN_qN		13
#define ISIN_qA		12
#define ISIN_B		19900
#define ISIN_C		3516

int32_t	isin( int32_t x )
{
	int c, y;
	
	c= x<<(30-ISIN_qN);			// Semi-circle info into carry.
	x -= 1<<ISIN_qN;			// sine -> cosine calc
	
	x= x<<(31-ISIN_qN);			// Mask with PI
	x= x>>(31-ISIN_qN);			// Note: SIGNED shift! (to qN)
	x= x*x>>(2*ISIN_qN-14);		// x=x^2 To Q14
	
	y= ISIN_B - (x*ISIN_C>>14);	// B - x^2*C
	y= (1<<ISIN_qA)-(x*y>>16);	// A - x^2*(B-x^2*C)
	
	return c>=0 ? y : -y;
}

//===========================================================================================================================
//	Multiply64x64
//
//	Multiplies two 64-bit unsigned integers and returns the upper and lower halves of the 128-bit result.
//===========================================================================================================================

void	Multiply64x64( uint64_t a, uint64_t b, uint64_t *outHi, uint64_t *outLo )
{
	uint32_t	u1, u0;
	uint32_t	v1, v0;
	uint64_t	t;
	uint32_t	k;
	uint32_t	w3, w2, w1, w0;
	
	// See Knuth V2 Section 4.3.1 Algorithm M for details.
	
	u1 = (uint32_t)( a >> 32 );
	u0 = (uint32_t)( a & 0xFFFFFFFFU );
	
	v1 = (uint32_t)( b >> 32 );
	v0 = (uint32_t)( b & 0xFFFFFFFFU );
	
	t  = u0;
	t *= v0;
	w0 = (uint32_t)( t & 0xFFFFFFFFU );
	k  = (uint32_t)( t >> 32 );
	
	t  = u1;
	t *= v0;
	t += k;
	w1 = (uint32_t)( t & 0xFFFFFFFFU );
	w2 = (uint32_t)( t >> 32 );
	
	t  = u0;
	t *= v1;
	t += w1;
	w1 = (uint32_t)( t & 0xFFFFFFFFU );
	k  = (uint32_t)( t >> 32 );
	
	t  = u1;
	t *= v1;
	t += w2;
	t += k;
	w2 = (uint32_t)( t & 0xFFFFFFFFU );
	w3 = (uint32_t)( t >> 32 );
	
	*outHi = ( ( (uint64_t) w3 ) << 32 ) | w2;
	*outLo = ( ( (uint64_t) w1 ) << 32 ) | w0;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	Average
//===========================================================================================================================

double	Average( const double x[], size_t n )
{
	double		avg;
	size_t		i;
	
	avg = 0;
	for( i = 0; i < n; ++i )
	{
		avg += x[ i ];
	}
	if( n > 0 ) avg /= n;
	return( avg );
}

//===========================================================================================================================
//	EuclideanDistance
//
//	See <http://en.wikipedia.org/wiki/Euclidean_distance>.
//===========================================================================================================================

double	EuclideanDistance( const double x[], const double *y, int n )
{
	double		sum;
	double		distance;
	int			i;
	
	sum = 0;
	for( i = 0; i < n; ++i )
	{
		double		tmp;
		
		tmp = x[ i ] - y[ i ];
		sum += ( tmp * tmp );
	}
	distance = sqrt( sum );
	return( distance );
}

//===========================================================================================================================
//	InterquartileRange
//
//	See <http://en.wikipedia.org/wiki/Interquartile_Range>.
//	See <http://en.wikipedia.org/wiki/Quartile>
//===========================================================================================================================

double	InterquartileRange( const double *set, double *tmp, size_t n, double *outLower, double *outUpper )
{
	size_t		mid, mid2;
	double		q1, q3, iqr, err;
	
	if( n < 4 )
	{
		if( outLower ) *outLower = DBL_MIN;
		if( outUpper ) *outUpper = DBL_MAX;
		return( DBL_MAX );
	}
	
	if( tmp )
	{
		memcpy( tmp, set, n * sizeof( *set ) );
		qsort( tmp, n, sizeof( *tmp ), qsort_cmp_double );
		set = tmp;
	}
	
	mid = n / 2;
	if( ( n % 2 ) == 0 )
	{
		if( ( mid % 2 ) == 0 )
		{
			mid2 = mid / 2;
			q1 = ( set[ mid2 - 1 ] + set[ mid2 ] ) / 2;
			
			mid2 = ( n + mid ) / 2;
			q3 = ( set[ mid2 - 1 ] + set[ mid2 ] ) / 2;
		}
		else
		{
			q1 = set[ mid / 2 ];
			q3 = set[ ( n + mid ) / 2 ];
		}
	}
	else
	{
		if( ( mid % 2 ) == 0 )
		{
			mid2 = mid / 2;
			q1 = ( set[ mid2 - 1 ] + set[ mid2 ] ) / 2;
			
			mid2 = ( n + ( mid + 1 ) ) / 2;
			q3 = ( set[ mid2 - 1 ] + set[ mid2 ] ) / 2;
		}
		else
		{
			q1 = set[ mid / 2 ];
			q3 = set[ ( n + ( mid + 1 ) ) / 2 ];
		}
	}
	
	iqr = q3 - q1;
	err = 1.5 * iqr;
	if( outLower ) *outLower = q1 - err;
	if( outUpper ) *outUpper = q3 + err;
	return( iqr );
}

//===========================================================================================================================
//	MedianAbsoluteDeviation
//===========================================================================================================================

double	MedianAbsoluteDeviation( const double *set, double *tmp1, double *tmp2, size_t n, double *outMedian )
{
	double		median, mad;
	size_t		i;
	
	if( n == 0 )
	{
		if( outMedian ) *outMedian = 0;
		return( 0 );
	}
	else if( n == 1 )
	{
		if( outMedian ) *outMedian = set[ 0 ];
		if( tmp1 )		tmp1[ 0 ]  = set[ 0 ];
		return( 0 );
	}
	
	if( tmp1 )
	{
		memcpy( tmp1, set, n * sizeof( *set ) );
		qsort( tmp1, n, sizeof( *tmp1 ), qsort_cmp_double );
		set = tmp1;
	}
	if( n % 2 ) median = set[ n / 2 ];
	else		median = ( set[ ( n / 2 ) - 1 ] + set[ n / 2 ] ) / 2;
	if( outMedian ) *outMedian = median;
	
	for( i = 0; i < n; ++i )
	{
		tmp2[ i ] = fabs( set[ i ] - median );
	}
	qsort( tmp2, n, sizeof( *tmp2 ), qsort_cmp_double );
	if( n % 2 ) mad = tmp2[ n / 2 ];
	else		mad = ( tmp2[ ( n / 2 ) - 1 ] + tmp2[ n / 2 ] ) / 2;
	return( mad );
}

//===========================================================================================================================
//	PearsonCorrelation
//
//	Converted from pseudocode on <http://en.wikipedia.org/wiki/Pearson_product-moment_correlation_coefficient>.
//===========================================================================================================================

double	PearsonCorrelation( const double x[], const double *y, int n )
{
	double		sum_sq_x, sum_sq_y;
	double		sum_coproduct;
	double		mean_x, mean_y;
	double		pop_sd_x, pop_sd_y;
	double		cov_x_y;
	double		correlation;
	int			i;
	
	if( n < 1 ) return( 1.0 );
	
	sum_sq_x		= 0;
	sum_sq_y		= 0;
	sum_coproduct	= 0;
	mean_x			= x[ 0 ];
	mean_y			= y[ 0 ];
	
	for( i = 2; i <= n; ++i )
	{
		double		sweep;
		double		delta_x, delta_y;
		
		sweep			= ( i - 1.0 ) / i;
		delta_x			= x[ i - 1 ] - mean_x;
		delta_y			= y[ i - 1 ] - mean_y;
		sum_sq_x		+= delta_x * delta_x * sweep;
		sum_sq_y		+= delta_y * delta_y * sweep;
		sum_coproduct	+= delta_x * delta_y * sweep;
		mean_x			+= delta_x / i;
		mean_y			+= delta_y / i;
	}
	pop_sd_x	= sqrt( sum_sq_x / n );
	pop_sd_y	= sqrt( sum_sq_y / n );
	cov_x_y		= sum_coproduct / n;
	correlation = cov_x_y / ( pop_sd_x * pop_sd_y );
	return( correlation );
}

//===========================================================================================================================
//	StandardDeviation
//===========================================================================================================================

double	StandardDeviation( const double *x, size_t n, Boolean inSample )
{
	size_t		i;
	double		sum, avg, tmp;
	
	if( n <= 1 ) return( 0 );
	
	sum = 0;
	for( i = 0; i < n; ++i )
	{
		sum += x[ i ];
	}
	avg = sum / n;
	
	sum = 0;
	for( i = 0; i < n; ++i )
	{
		tmp = x[ i ] - avg;
		sum += ( tmp * tmp );
	}
	sum /= ( inSample ? ( n - 1 ) : n ); // Sample Standard Deviation vs Population Standard Deviation.
	return( sqrt( sum ) );
}

//===========================================================================================================================
//	TorbenMedian
//
//	Algorithm by Torben Mogensen, implementation by N. Devillard.
//	See <http://ndevilla.free.fr/median/median/median.html> for more information and alternate implementations.
//===========================================================================================================================

#define TorbenMedianImplementation( TORBEN_TYPE, TORBEN_NAME ) \
	TORBEN_TYPE	TORBEN_NAME( const TORBEN_TYPE m[], int n ) \
	{ \
		int i, less, greater, equal; \
		TORBEN_TYPE min, max, guess, maxltguess, mingtguess; \
		\
		min = max = m[0] ; \
		for (i=1 ; i<n ; i++) { \
			if (m[i]<min) min=m[i]; \
			if (m[i]>max) max=m[i]; \
		} \
		\
		for (;;) { \
			guess = (min+max)/2; \
			less = 0; greater = 0; equal = 0; \
			maxltguess = min ; \
			mingtguess = max ; \
			for (i=0; i<n; i++) { \
				if (m[i]<guess) { \
					less++; \
					if (m[i]>maxltguess) maxltguess = m[i] ; \
				} else if (m[i]>guess) { \
					greater++; \
					if (m[i]<mingtguess) mingtguess = m[i] ; \
				} else equal++; \
			} \
			if (less <= (n+1)/2 && greater <= (n+1)/2) break ; \
			else if (less>greater) max = maxltguess ; \
			else min = mingtguess; \
		} \
		if (less >= (n+1)/2) return maxltguess; \
		else if (less+equal >= (n+1)/2) return guess; \
		else return mingtguess; \
	}

TorbenMedianImplementation( int32_t, TorbenMedian32 )
TorbenMedianImplementation( int64_t, TorbenMedian64 )
TorbenMedianImplementation( double,  TorbenMedianF )

//===========================================================================================================================
//	TranslateValue
//===========================================================================================================================

double	TranslateValue( double inValue, double inOldMin, double inOldMax, double inNewMin, double inNewMax )
{
	return( ( ( ( inValue - inOldMin ) / ( inOldMax - inOldMin ) ) * ( inNewMax - inNewMin ) ) + inNewMin );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	MathUtils_Test
//===========================================================================================================================

#define FillValuesF( A, B, C, D, E, F, G, H, I, J, K ) \
	valuesF[  0 ] = (A); \
	valuesF[  1 ] = (B); \
	valuesF[  2 ] = (C); \
	valuesF[  3 ] = (D); \
	valuesF[  4 ] = (E); \
	valuesF[  5 ] = (F); \
	valuesF[  6 ] = (G); \
	valuesF[  7 ] = (H); \
	valuesF[  8 ] = (I); \
	valuesF[  9 ] = (J); \
	valuesF[ 10 ] = (K)

OSStatus	MathUtils_Test( int inPrint )
{
	OSStatus			err;
	EWMA_FP_Data		ewma;
	unsigned int		i;
	uint64_t			a, b, hi, lo;
	int32_t				values32[ 10 ];
	int64_t				values64[ 10 ];
	double				valuesF[ 11 ];
	double				tempF[ 11 ];
	double				d, d2, d3;
	
	require_action( AverageCeil64( 10, 15 ) == 13, exit, err = -1 );
	require_action( AverageFloor64( 10, 15 ) == 12, exit, err = -1 );
	require_action( AverageCeil64( UINT64_C( 10000000000000000000 ), UINT64_C( 12000000000000000000 ) ) == 
		UINT64_C( 11000000000000000000 ), exit, err = -1 );
	require_action( AverageFloor64( UINT64_C( 10000000000000000000 ), UINT64_C( 12000000000000000000 ) ) == 
		UINT64_C( 11000000000000000000 ), exit, err = -1 );
	require_action( AverageCeil64( UINT64_C( 12000000000000000000 ), UINT64_C( 10000000000000000000 ) ) == 
		UINT64_C( 11000000000000000000 ), exit, err = -1 );
	require_action( AverageFloor64( UINT64_C( 12000000000000000000 ), UINT64_C( 10000000000000000000 ) ) == 
		UINT64_C( 11000000000000000000 ), exit, err = -1 );
	require_action( AverageCeil64( UINT64_C( 18000000000000000100 ), UINT64_C( 10000000000000000200 ) ) == 
		UINT64_C( 14000000000000000150 ), exit, err = -1 );
	require_action( AverageFloor64( UINT64_C( 18000000000000000100 ), UINT64_C( 10000000000000000200 ) ) == 
		UINT64_C( 14000000000000000150 ), exit, err = -1 );
	
	require_action( CountDigits32( 0, 2 ) == 1, exit, err = -1 );
	require_action( CountDigits32( 5, 2 ) == 3, exit, err = -1 );
	require_action( CountDigits32( UINT32_C( 4294967295 ), 2 ) == 32, exit, err = -1 );
	require_action( CountDigits32( 0, 10 ) == 1, exit, err = -1 );
	require_action( CountDigits32( 9, 10 ) == 1, exit, err = -1 );
	require_action( CountDigits32( 10, 10 ) == 2, exit, err = -1 );
	require_action( CountDigits32( 99999999, 10 ) == 8, exit, err = -1 );
	require_action( CountDigits32( UINT32_C( 2147483647 ), 10 ) == 10, exit, err = -1 );
	require_action( CountDigits32( UINT32_C( 2147483648 ), 10 ) == 10, exit, err = -1 );
	require_action( CountDigits32( UINT32_C( 4294967295 ), 10 ) == 10, exit, err = -1 );
	require_action( CountDigits32( 0, 16 ) == 1, exit, err = -1 );
	require_action( CountDigits32( 0xFFF, 16 ) == 3, exit, err = -1 );
	require_action( CountDigits32( UINT32_C( 0xFFFFFFFF ), 16 ) == 8, exit, err = -1 );
	require_action( CountDigits32( 0, 8 ) == 1, exit, err = -1 );
	require_action( CountDigits32( 0123, 8 ) == 3, exit, err = -1 );
	require_action( CountDigits32( UINT32_C( 037777777777 ), 8 ) == 11, exit, err = -1 );
	
	require_action( CountDigits64( 0, 2 ) == 1, exit, err = -1 );
	require_action( CountDigits64( 5, 2 ) == 3, exit, err = -1 );
	require_action( CountDigits64( UINT32_C( 4294967295 ), 2 ) == 32, exit, err = -1 );
	require_action( CountDigits64( 0, 10 ) == 1, exit, err = -1 );
	require_action( CountDigits64( 9, 10 ) == 1, exit, err = -1 );
	require_action( CountDigits64( 10, 10 ) == 2, exit, err = -1 );
	require_action( CountDigits64( 99999999, 10 ) == 8, exit, err = -1 );
	require_action( CountDigits64( UINT32_C( 2147483647 ), 10 ) == 10, exit, err = -1 );
	require_action( CountDigits64( UINT32_C( 2147483648 ), 10 ) == 10, exit, err = -1 );
	require_action( CountDigits64( UINT32_C( 4294967295 ), 10 ) == 10, exit, err = -1 );
	require_action( CountDigits64( 0, 16 ) == 1, exit, err = -1 );
	require_action( CountDigits64( 0xFFF, 16 ) == 3, exit, err = -1 );
	require_action( CountDigits64( UINT32_C( 0xFFFFFFFF ), 16 ) == 8, exit, err = -1 );
	require_action( CountDigits64( 0, 8 ) == 1, exit, err = -1 );
	require_action( CountDigits64( 0123, 8 ) == 3, exit, err = -1 );
	require_action( CountDigits64( UINT32_C( 037777777777 ), 8 ) == 11, exit, err = -1 );
	require_action( CountDigits64( UINT64_C( 9223372036854775807 ), 10 ) == 19, exit, err = -1 );
	require_action( CountDigits64( UINT64_C( 18446744073709551615 ), 10 ) == 20, exit, err = -1 );
	
	EWMA_FP_Init( &ewma, 0.1, kEWMAFlags_StartWithFirstValue );
	EWMA_FP_Update( &ewma, 71 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 70 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 69 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 68 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 64 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 65 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 72 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 78 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 75 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 75 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 75 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	EWMA_FP_Update( &ewma, 70 ); if( inPrint ) dlog( kLogLevelMax, "%.2f\n", EWMA_FP_Get( &ewma ) );
	require_action( RoundTo( EWMA_FP_Get( &ewma ), .01 ) == 71.50, exit, err = -1 );
	
	require_action( gcd64( 1, 1 ) == 1, exit, err = kResponseErr );
	require_action( gcd64( 10, 5 ) == 5, exit, err = kResponseErr );
	require_action( gcd64( 1000, 120 ) == 40, exit, err = kResponseErr );
	require_action( gcd64( 1000000000, 166666600 ) == 200, exit, err = kResponseErr );
	
	require_action( iceil2( 0 ) == 0, exit, err = kResponseErr );
	require_action( iceil2( 1 ) == 1, exit, err = kResponseErr );
	require_action( iceil2( 2 ) == 2, exit, err = kResponseErr );
	require_action( iceil2( 32 ) == 32, exit, err = kResponseErr );
	require_action( iceil2( 4096 ) == 4096, exit, err = kResponseErr );
	require_action( iceil2( UINT32_C( 0x80000000 ) ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	require_action( iceil2( 3 ) == 4, exit, err = kResponseErr );
	require_action( iceil2( 17 ) == 32, exit, err = kResponseErr );
	require_action( iceil2( 2289 ) == 4096, exit, err = kResponseErr );
	require_action( iceil2( 26625 ) == 32768, exit, err = kResponseErr );
	require_action( iceil2( 2146483648 ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	require_action( iceil2( 2147483647 ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	
	require_action( iceil2_64( 0 ) == 0, exit, err = kResponseErr );
	require_action( iceil2_64( 1 ) == 1, exit, err = kResponseErr );
	require_action( iceil2_64( 2 ) == 2, exit, err = kResponseErr );
	require_action( iceil2_64( 32 ) == 32, exit, err = kResponseErr );
	require_action( iceil2_64( 4096 ) == 4096, exit, err = kResponseErr );
	require_action( iceil2_64( UINT32_C( 0x80000000 ) ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	require_action( iceil2_64( 3 ) == 4, exit, err = kResponseErr );
	require_action( iceil2_64( 17 ) == 32, exit, err = kResponseErr );
	require_action( iceil2_64( 2289 ) == 4096, exit, err = kResponseErr );
	require_action( iceil2_64( 26625 ) == 32768, exit, err = kResponseErr );
	require_action( iceil2_64( 2146483648 ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	require_action( iceil2_64( 2147483647 ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	require_action( iceil2_64( UINT64_C( 4294967297 ) ) == UINT64_C( 0x200000000 ), exit, err = kResponseErr );
	require_action( iceil2_64( UINT64_C( 26225225412000 ) ) == UINT64_C( 0x200000000000 ), exit, err = kResponseErr );
	require_action( iceil2_64( UINT64_C( 38361162946163923 ) ) == UINT64_C( 0x100000000000000 ), exit, err = kResponseErr );
	require_action( iceil2_64( UINT64_C( 0x7FFFFFFFFFFFFFE ) ) == UINT64_C( 0x800000000000000 ), exit, err = kResponseErr );
	require_action( iceil2_64( UINT64_C( 0x7FFFFFFFFFFFFFF ) ) == UINT64_C( 0x800000000000000 ), exit, err = kResponseErr );
	require_action( iceil2_64( UINT64_C( 0x800000000000000 ) ) == UINT64_C( 0x800000000000000 ), exit, err = kResponseErr );
	
	require_action( ifloor2( 0 ) == 0, exit, err = kResponseErr );
	require_action( ifloor2( 1 ) == 1, exit, err = kResponseErr );
	require_action( ifloor2( 2 ) == 2, exit, err = kResponseErr );
	require_action( ifloor2( 32 ) == 32, exit, err = kResponseErr );
	require_action( ifloor2( 4096 ) == 4096, exit, err = kResponseErr );
	require_action( ifloor2( UINT32_C( 0x80000000 ) ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	require_action( ifloor2( 3 ) == 2, exit, err = kResponseErr );
	require_action( ifloor2( 31 ) == 16, exit, err = kResponseErr );
	require_action( ifloor2( 4095 ) == 2048, exit, err = kResponseErr );
	require_action( ifloor2( 41234 ) == 32768, exit, err = kResponseErr );
	require_action( ifloor2( 2146483648 ) == 1073741824, exit, err = kResponseErr );
	require_action( ifloor2( UINT32_C( 0xFFFFFFFF ) ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	
	require_action( ifloor2_64( 0 ) == 0, exit, err = kResponseErr );
	require_action( ifloor2_64( 1 ) == 1, exit, err = kResponseErr );
	require_action( ifloor2_64( 2 ) == 2, exit, err = kResponseErr );
	require_action( ifloor2_64( 32 ) == 32, exit, err = kResponseErr );
	require_action( ifloor2_64( 4096 ) == 4096, exit, err = kResponseErr );
	require_action( ifloor2_64( UINT32_C( 0x80000000 ) ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	require_action( ifloor2_64( 3 ) == 2, exit, err = kResponseErr );
	require_action( ifloor2_64( 31 ) == 16, exit, err = kResponseErr );
	require_action( ifloor2_64( 4095 ) == 2048, exit, err = kResponseErr );
	require_action( ifloor2_64( 41234 ) == 32768, exit, err = kResponseErr );
	require_action( ifloor2_64( 2146483648 ) == 1073741824, exit, err = kResponseErr );
	require_action( ifloor2_64( UINT32_C( 0xFFFFFFFF ) ) == UINT32_C( 0x80000000 ), exit, err = kResponseErr );
	require_action( ifloor2_64( UINT64_C( 4294967297 ) ) == UINT64_C( 0x100000000 ), exit, err = kResponseErr );
	require_action( ifloor2_64( UINT64_C( 26225225412000 ) ) == UINT64_C( 0x100000000000 ), exit, err = kResponseErr );
	require_action( ifloor2_64( UINT64_C( 38361162946163923 ) ) == UINT64_C( 0x80000000000000 ), exit, err = kResponseErr );
	require_action( ifloor2_64( UINT64_C( 0x7FFFFFFFFFFFFFE ) ) == UINT64_C( 0x400000000000000 ), exit, err = kResponseErr );
	require_action( ifloor2_64( UINT64_C( 0x7FFFFFFFFFFFFFF ) ) == UINT64_C( 0x400000000000000 ), exit, err = kResponseErr );
	require_action( ifloor2_64( UINT64_C( 0x800000000000000 ) ) == UINT64_C( 0x800000000000000 ), exit, err = kResponseErr );
	
	require_action( ilog2( 0 ) == 0, exit, err = kResponseErr );
	require_action( ilog2( 1 ) == 0, exit, err = kResponseErr );
	require_action( ilog2( 2 ) == 1, exit, err = kResponseErr );
	require_action( ilog2( 5 ) == 2, exit, err = kResponseErr );
	require_action( ilog2( 1234 ) == 10, exit, err = kResponseErr );
	require_action( ilog2( UINT32_C( 24000000 ) ) == 24, exit, err = kResponseErr );
	require_action( ilog2( UINT32_C( 1000000000 ) ) == 29, exit, err = kResponseErr );
	require_action( ilog2( UINT32_C( 0xFFFFFFFF ) ) == 31, exit, err = kResponseErr );
	for( i = 0; i < 32; ++i ) require_action( ilog2( 1 << i ) == i, exit, err = kResponseErr );
	
	require_action( ilog2_64( 0 ) == 0, exit, err = kResponseErr );
	require_action( ilog2_64( 1 ) == 0, exit, err = kResponseErr );
	require_action( ilog2_64( 2 ) == 1, exit, err = kResponseErr );
	require_action( ilog2_64( 5 ) == 2, exit, err = kResponseErr );
	require_action( ilog2_64( 1234 ) == 10, exit, err = kResponseErr );
	require_action( ilog2_64( UINT32_C( 24000000 ) ) == 24, exit, err = kResponseErr );
	require_action( ilog2_64( UINT32_C( 1000000000 ) ) == 29, exit, err = kResponseErr );
	require_action( ilog2_64( UINT32_C( 0xFFFFFFFF ) ) == 31, exit, err = kResponseErr );
	for( i = 0; i < 64; ++i ) require_action( ilog2_64( UINT64_C( 1 ) << i ) == i, exit, err = kResponseErr );
	
	require_action( ipow10( 0 ) == 1, exit, err = -1 );
	require_action( ipow10( 1 ) == 10, exit, err = -1 );
	require_action( ipow10( 2 ) == 100, exit, err = -1 );
	require_action( ipow10( 3 ) == 1000, exit, err = -1 );
	require_action( ipow10( 4 ) == 10000, exit, err = -1 );
	require_action( ipow10( 5 ) == 100000, exit, err = -1 );
	require_action( ipow10( 6 ) == 1000000, exit, err = -1 );
	require_action( ipow10( 7 ) == 10000000, exit, err = -1 );
	require_action( ipow10( 8 ) == 100000000, exit, err = -1 );
	require_action( ipow10( 9 ) == 1000000000, exit, err = -1 );
	
	require_action( ipow10_64(  0 ) == 1, exit, err = -1 );
	require_action( ipow10_64(  1 ) == 10, exit, err = -1 );
	require_action( ipow10_64(  2 ) == 100, exit, err = -1 );
	require_action( ipow10_64(  3 ) == 1000, exit, err = -1 );
	require_action( ipow10_64(  4 ) == 10000, exit, err = -1 );
	require_action( ipow10_64(  5 ) == 100000, exit, err = -1 );
	require_action( ipow10_64(  6 ) == 1000000, exit, err = -1 );
	require_action( ipow10_64(  7 ) == 10000000, exit, err = -1 );
	require_action( ipow10_64(  8 ) == 100000000, exit, err = -1 );
	require_action( ipow10_64(  9 ) == 1000000000, exit, err = -1 );
	require_action( ipow10_64( 10 ) == UINT64_C( 10000000000 ), exit, err = -1 );
	require_action( ipow10_64( 11 ) == UINT64_C( 100000000000 ), exit, err = -1 );
	require_action( ipow10_64( 12 ) == UINT64_C( 1000000000000 ), exit, err = -1 );
	require_action( ipow10_64( 13 ) == UINT64_C( 10000000000000 ), exit, err = -1 );
	require_action( ipow10_64( 14 ) == UINT64_C( 100000000000000 ), exit, err = -1 );
	require_action( ipow10_64( 15 ) == UINT64_C( 1000000000000000 ), exit, err = -1 );
	require_action( ipow10_64( 16 ) == UINT64_C( 10000000000000000 ), exit, err = -1 );
	require_action( ipow10_64( 17 ) == UINT64_C( 100000000000000000 ), exit, err = -1 );
	require_action( ipow10_64( 18 ) == UINT64_C( 1000000000000000000 ), exit, err = -1 );
	require_action( ipow10_64( 19 ) == UINT64_C( 10000000000000000000 ), exit, err = -1 );
	
	require_action( isin( -16384 ) ==     0, exit, err = -1 ); //  0/8 pi radian
	require_action( isin( -14336 ) == -1576, exit, err = -1 ); //  1/8 pi radian
	require_action( isin( -12288 ) == -2908, exit, err = -1 ); //  2/8 pi radian
	require_action( isin( -10240 ) == -3789, exit, err = -1 ); //  3/8 pi radian
	require_action( isin(  -8192 ) == -4096, exit, err = -1 ); //  4/8 pi radian
	require_action( isin(  -6144 ) == -3789, exit, err = -1 ); //  5/8 pi radian
	require_action( isin(  -4096 ) == -2908, exit, err = -1 ); //  6/8 pi radian
	require_action( isin(  -2048 ) == -1576, exit, err = -1 ); //  7/8 pi radian
	require_action( isin(      0 ) ==     0, exit, err = -1 ); //  8/8 pi radian
	require_action( isin(   2048 ) ==  1576, exit, err = -1 ); //  9/8 pi radian
	require_action( isin(   4096 ) ==  2908, exit, err = -1 ); // 10/8 pi radian
	require_action( isin(   6143 ) ==  3789, exit, err = -1 ); // 11/8 pi radian
	require_action( isin(   8192 ) ==  4096, exit, err = -1 ); // 12/8 pi radian
	require_action( isin(  10240 ) ==  3789, exit, err = -1 ); // 13/8 pi radian
	require_action( isin(  12288 ) ==  2908, exit, err = -1 ); // 14/8 pi radian
	require_action( isin(  14335 ) ==  1577, exit, err = -1 ); // 15/8 pi radian
	require_action( isin(  16384 ) ==     0, exit, err = -1 ); // 16/8 pi radian
	
	// Shifts
	
	require_action( ASR32( INT32_C( 0xFFFF000F ),  4 ) == (int32_t) INT32_C( 0xFFFFF000 ), exit, err = -1 );
	require_action( ASR32( INT32_C( 0xFFFFFFFF ),  4 ) == (int32_t) INT32_C( 0xFFFFFFFF ), exit, err = -1 );
	require_action( ASR32( INT32_C( 0xFFFFFFFF ),  1 ) == (int32_t) INT32_C( 0xFFFFFFFF ), exit, err = -1 );
	require_action( ASR32( INT32_C( 0xFFFFFFFF ), 31 ) == (int32_t) INT32_C( 0xFFFFFFFF ), exit, err = -1 );
	require_action( ASR32( INT32_C( 0x7FFF000F ),  4 ) == (int32_t) INT32_C( 0x07FFF000 ), exit, err = -1 );
	require_action( ASR64( INT64_C( 0xFFFFFFFF0000000F ),  4 ) == (int64_t) INT64_C( 0xFFFFFFFFF0000000 ), exit, err = -1 );
	require_action( ASR64( INT64_C( 0xFFFFFFFFFFFFFFFF ),  4 ) == (int64_t) INT64_C( 0xFFFFFFFFFFFFFFFF ), exit, err = -1 );
	require_action( ASR64( INT64_C( 0xFFFFFFFFFFFFFFFF ), 31 ) == (int64_t) INT64_C( 0xFFFFFFFFFFFFFFFF ), exit, err = -1 );
	require_action( ASR64( INT64_C( 0xFFFFFFFFFFFFFFFF ), 63 ) == (int64_t) INT64_C( 0xFFFFFFFFFFFFFFFF ), exit, err = -1 );
	require_action( ASR64( INT64_C( 0x7FFFFFFF00000000 ),  4 ) == (int64_t) INT64_C( 0x07FFFFFFF0000000 ), exit, err = -1 );
	
	require_action( LSR32( INT32_C( 0xFFFF000F ), 4 ) == 0x0FFFF000, exit, err = -1 );
	require_action( LSR32( INT32_C( 0x7FFF000F ), 4 ) == 0x07FFF000, exit, err = -1 );
	require_action( LSR64( INT64_C( 0xFFFFFFFF00000000 ), 4 ) == INT64_C( 0x0FFFFFFFF0000000 ), exit, err = -1 );
	require_action( LSR64( INT64_C( 0x7FFFFFFF00000000 ), 4 ) == INT64_C( 0x07FFFFFFF0000000 ), exit, err = -1 );
	
	// InterquartileRange
	
	FillValuesF( 102, 104, 105, 107, 108, 109, 110, 112, 115, 116, 118 );
	d = InterquartileRange( valuesF, valuesF, 11, &d2, &d3 );
	require_action( d == 10, exit, err = -1 );
	require_action( d2 == 90, exit, err = -1 );
	require_action( d3 == 130, exit, err = -1 );
	
	FillValuesF( 6, 47, 49, 15, 42, 41, 7, 39, 43, 40, 36 );
	d = InterquartileRange( valuesF, valuesF, 11, &d2, &d3 );
	require_action( d == 28, exit, err = -1 );
	require_action( d2 == -27, exit, err = -1 );
	require_action( d3 == 85, exit, err = -1 );
	
	FillValuesF( 2, 2, 4, 7, 0, 0, 0, 0, 0, 0, 0 );
	d = InterquartileRange( valuesF, valuesF, 4, &d2, &d3 );
	require_action( d == 3.5, exit, err = -1 );
	require_action( d2 == -3.25, exit, err = -1 );
	require_action( d3 == 10.75, exit, err = -1 );
		
	FillValuesF( 2, 2, 4, 6, 7, 0, 0, 0, 0, 0, 0 );
	d = InterquartileRange( valuesF, valuesF, 5, &d2, &d3 );
	require_action( d == 4.5, exit, err = -1 );
	require_action( d2 == -4.75, exit, err = -1 );
	require_action( d3 == 13.25, exit, err = -1 );
	
	FillValuesF( 2, 2, 4, 6, 6, 8, 0, 0, 0, 0, 0 );
	d = InterquartileRange( valuesF, valuesF, 6, &d2, &d3 );
	require_action( d == 4, exit, err = -1 );
	require_action( d2 == -4, exit, err = -1 );
	require_action( d3 == 12, exit, err = -1 );
	
	// MedianAbsoluteDeviation
	
	FillValuesF( -999, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	d = MedianAbsoluteDeviation( valuesF, valuesF, tempF, 0, &d2 );
	require_action( d == 0, exit, err = -1 );
	require_action( d2 == 0, exit, err = -1 );
	require_action( valuesF[ 0 ] == -999, exit, err = -1 );
	
	FillValuesF( 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	d = MedianAbsoluteDeviation( valuesF, valuesF, tempF, 1, &d2 );
	require_action( d == 0, exit, err = -1 );
	require_action( d2 == 2, exit, err = -1 );
	require_action( valuesF[ 0 ] == 2, exit, err = -1 );
	
	FillValuesF( 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	d = MedianAbsoluteDeviation( valuesF, valuesF, tempF, 2, &d2 );
	require_action( d == 0.5, exit, err = -1 );
	require_action( d2 == 2.5, exit, err = -1 );
	require_action( valuesF[ 0 ] == 2, exit, err = -1 );
	
	FillValuesF( 2, 1, 2, 6, 1, 9, 4, 0, 0, 0, 0 );
	d = MedianAbsoluteDeviation( valuesF, valuesF, tempF, 7, &d2 );
	require_action( d == 1, exit, err = -1 );
	require_action( d2 == 2, exit, err = -1 );
	require_action( valuesF[ 0 ] == 1, exit, err = -1 );
	
	FillValuesF( 8, 25, 35, 41, 50, 75, 75, 79, 92, 129, 0 );
	d = MedianAbsoluteDeviation( valuesF, valuesF, tempF, 10, &d2 );
	require_action( d  == 24.5, exit, err = -1 );
	require_action( d2 == 62.5, exit, err = -1 );
	require_action( valuesF[ 0 ] == 8, exit, err = -1 );
	
	// int128
{
	int128_compat		a128, b128;
	uint64_t			u64;
	
	a128.hi = -1;
	a128.lo = UINT64_C( 0xFFFFFFFFFFFFFFFF );
	
	b128.hi = 0;
	b128.lo = 1;
	int128_add( &a128, &a128, &b128 );
	require_action( ( a128.hi == 0 ) && ( a128.lo == 0 ), exit, err = -1 );
	
	b128.hi = 0;
	b128.lo = 1;
	int128_sub( &a128, &a128, &b128 );
	require_action( ( a128.hi == -1 ) && ( a128.lo == UINT64_C( 0xFFFFFFFFFFFFFFFF ) ), exit, err = -1 );
	
	// rshift
	
	int128_rshift( &a128, 1 );
	require_action( ( a128.hi == -1 ) && ( a128.lo == UINT64_C( 0xFFFFFFFFFFFFFFFF ) ), exit, err = -1 );
	
	u64 = UINT64_C( 0x7FFFFFFFFFFFFFFF );
	a128.hi = INT64_C( 0x7FFFFFFFFFFFFFFF );
	a128.lo = UINT64_C( 0xFFFFFFFFFFFFFFFF );
	for( i = 0; i < 63; ++i )
	{
		int128_rshift( &a128, 1 );
		u64 >>= 1;
		require_action( ( ( (uint64_t) a128.hi ) == u64 ) && ( a128.lo == UINT64_C( 0xFFFFFFFFFFFFFFFF ) ), exit, err = -1 );
	}
	
	u64 = UINT64_C( 0xFFFFFFFFFFFFFFFF );
	for( i = 0; i < 64; ++i )
	{
		int128_rshift( &a128, 1 );
		u64 >>= 1;
		require_action( ( a128.hi == 0 ) && ( a128.lo == u64 ), exit, err = -1 );
	}
	
	a128.hi = -50;
	a128.lo = 0;
	int128_rshift( &a128, 1 );
	require_action( ( a128.hi == -25 ) && ( a128.lo == 0 ), exit, err = -1 );
	
	a128.hi = -50;
	a128.lo = UINT64_C( 0x8000000000000000 );
	int128_rshift( &a128, 1 );
	require_action( ( a128.hi == -25 ) && ( a128.lo == UINT64_C( 0x4000000000000000 ) ), exit, err = -1 );
	
	// lshift
	
	a128.hi = 0;
	a128.lo = 1;
	u64 = 1;
	for( i = 0; i < 63; ++i )
	{
		int128_lshift( &a128, 1 );
		u64 <<= 1;
		require_action( ( a128.hi == 0 ) && ( a128.lo == u64 ), exit, err = -1 );
	}
	u64 = 1;
	for( i = 0; i < 65; ++i )
	{
		int128_lshift( &a128, 1 );
		require_action( ( ( (uint64_t) a128.hi ) == u64 ) && ( a128.lo == 0 ), exit, err = -1 );
		u64 <<= 1;
	}
}
	
	// Multiply64x64
	
	Multiply64x64( 0, 0, &hi, &lo );
	require_action( ( hi == 0 ) && ( lo == 0 ), exit, err = kResponseErr );
	
	a = 1234;
	b = 123;
	Multiply64x64( a, b, &hi, &lo );
	require_action( ( hi == 0 ) && ( lo == 151782 ), exit, err = kResponseErr );
	
	a = 1234567;
	b = 1234;
	Multiply64x64( a, b, &hi, &lo );
	require_action( ( hi == 0 ) && ( lo == 1523455678 ), exit, err = kResponseErr );
	
	a = 987234974;
	b = 896362;
	Multiply64x64( a, b, &hi, &lo );
	require_action( ( hi == 0 ) && ( lo == UINT64_C( 884919915764588 ) ), exit, err = kResponseErr );
	
	a = 1176792499;
	b = 987234974;
	Multiply64x64( a, b, &hi, &lo );
	require_action( ( hi == 0 ) && ( lo == UINT64_C( 1161770712153660026 ) ), exit, err = kResponseErr );
	
	a = UINT64_C( 8562917290710175917 );
	b = UINT64_C( 8362519193860661512 );
	Multiply64x64( a, b, &hi, &lo );
	require_action( ( hi == UINT64_C( 3881853616707397821 ) ) && ( lo == UINT64_C( 12277394883465777768 ) ), exit, err = kResponseErr );
	
	a = UINT64_C( 9223372036854775807 );
	b = UINT64_C( 18446744073709551615 );
	Multiply64x64( a, b, &hi, &lo );
	require_action( ( hi == UINT64_C( 9223372036854775806 ) ) && ( lo == UINT64_C( 9223372036854775809 ) ), exit, err = kResponseErr );
	
	a = UINT64_C( 18446744073709551615 );
	b = UINT64_C( 18446744073709551615 );
	Multiply64x64( a, b, &hi, &lo );
	require_action( ( hi == UINT64_C( 18446744073709551614 ) ) && ( lo == 1 ), exit, err = kResponseErr );
	
	// Average
	
	valuesF[ 0 ] = 4;
	valuesF[ 1 ] = 2;
	valuesF[ 2 ] = 5;
	valuesF[ 3 ] = 8;
	valuesF[ 4 ] = 6;
	d = Average( valuesF, 5 );
	require_action( RoundTo( d, 0.001 ) == 5.000, exit, err = -1 );
	
	d = Average( valuesF, 0 );
	require_action( d == 0, exit, err = -1 );
	
	d = Average( valuesF, 1 );
	require_action( RoundTo( d, 0.001 ) == 4.000, exit, err = -1 );
	
	// StandardDeviation
	
	valuesF[ 0 ] = 4;
	valuesF[ 1 ] = 2;
	valuesF[ 2 ] = 5;
	valuesF[ 3 ] = 8;
	valuesF[ 4 ] = 6;
	d = StandardDeviation( valuesF, 5, true );
	require_action( RoundTo( d, 0.001 ) == 2.236, exit, err = -1 );
	
	valuesF[ 0 ] = 4;
	valuesF[ 1 ] = 2;
	valuesF[ 2 ] = 5;
	valuesF[ 3 ] = 8;
	valuesF[ 4 ] = 6;
	d = StandardDeviation( valuesF, 5, false );
	require_action( RoundTo( d, 0.001 ) == 2.000, exit, err = -1 );
	
	// TorbenMedian
	
	for( i = 0; i < 10; ++i ) values32[ i ] = (int32_t)( i + 1 );
	require_action( TorbenMedian32( values32, 10 ) == 5, exit, err = -1 );
	
	for( i = 0; i < 10; ++i ) values64[ i ] = UINT64_C( 0x100000001 ) + i;
	require_action( TorbenMedian64( values64, 10 ) == ( UINT64_C( 0x100000000 ) + 5 ), exit, err = -1 );
	
	for( i = 0; i < 10; ++i ) valuesF[ i ] = ( (double)( i + 1 ) ) / 10;
	require_action( TorbenMedianF( valuesF, 10 ) == 0.5, exit, err = -1 );
	
	// TranslateValue
	
	require_action( TranslateValue( -30,  -30, 0, 0, 1 ) == 0.0, exit, err = kResponseErr );
	require_action( TranslateValue( -15,  -30, 0, 0, 1 ) == 0.5, exit, err = kResponseErr );
	require_action( TranslateValue( -7.5, -30, 0, 0, 1 ) == 0.75, exit, err = kResponseErr );
	require_action( TranslateValue( 0,    -30, 0, 0, 1 ) == 1.0, exit, err = kResponseErr );
	
	err = kNoErr;
	
exit:
	printf( "MathUtils_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
