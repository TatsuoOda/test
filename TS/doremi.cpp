
/*
	doremi.cpp
		2012.5.22  by T.Oda
 */

#include <stdio.h>
#include <math.h>

int main( int argc, char *argv[] )
{
int i;
double d_freq0 = 440.0;
double d_freq  = 440.0;
double k2_12   = pow( 2.0, 1.0/12.0 );

	for( i=0; i<40; i++ )
	{
	    d_freq = d_freq0 * pow( k2_12, i );
	    fprintf( stdout, "%2d : %4.6f\n", i, d_freq );
	}
}

