
/*
	cal.cpp
	    calculate : in decimal and hex
		2007.3.21  by T.Oda
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Usage( char *prg )
{
	fprintf( stdout, "%s [$] num +/-/X// num\n", prg );
	exit( 1 );
}

#define MODE_DEC	0
#define MODE_HEX	1

int main( int argc, char *argv[] )
{
/*
cal 13
cal 2 * 3
cal 2 + 3
cal 2 - 3
cal 4 / 2

cal -H 2 * 3
cal -H 0x0A * 0x04

 */
int args=0;
int vals[10];
int nVal=0;
int ops [10];
int nOp =0;
int nResult=0;
int outMode = MODE_DEC;
int i;

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='$' )
	    {
	    	outMode = MODE_HEX;
	    } else {
	    	if( (args&1)==0 )	// value
		{
		    if( strncmp( argv[i], "0x", 2 )==0 )
		    {
			sscanf( &argv[i][2], "%X", &vals[nVal++] );
		    } else {
			vals[nVal++] = atoi( argv[i] );
		    }
		} else {	// operator
		    if( strcmp( argv[i], "-" )==0 )
		    {
		    	ops [nOp++] = '-';
		    } else if( strcmp( argv[i], "+" )==0 )
		    {
		    	ops [nOp++] = '+';
		    } else if( strcmp( argv[i], "X" )==0 )
		    {
		    	ops [nOp++] = '*';
		    } else if( strcmp( argv[i], "/" )==0 )
		    {
		    	ops [nOp++] = '/';
		    } else {
		    	fprintf( stdout, "operator ? [%s]\n", argv[i] );
		    	Usage( argv[0] );
		    }
		}
		args++;
	    }
	}
	if( args==1 )
	{
	    nResult = vals[0];
	} else if( args==3 )
	{
	    switch( ops[0] )
	    {
	    case '-' :
	    	nResult = vals[0] - vals[1];
		break;
	    case '+' :
	    	nResult = vals[0] + vals[1];
		break;
	    case '*' :
	    	nResult = vals[0] * vals[1];
		break;
	    case '/' :
	    	nResult = vals[0] / vals[1];
		break;
	    }
	} else {
	    Usage( argv[0] );
	}
	if( outMode==MODE_DEC )
	    fprintf( stdout, "%d\n", nResult );
	else
	    fprintf( stdout, "%X\n", nResult );

	return 0;
}

