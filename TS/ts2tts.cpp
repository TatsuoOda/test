
/*
	ts2tts.cpp	2012.10.29 by T.Oda

		2012.11.5 -K : Kbps
			  -M : Kbps
 */

#include <stdio.h>
#include <stdlib.h>

#include "main.h"

int ts2tts_Usage( char *prg )
{
    fprintf( stdout, "%s infile outfile [-N] [-Uxxx]\n", prg );
    exit( 1 );
}

int ts2tts( int argc, char *argv[] )
{
int packet=0;
FILE *fp=NULL;
FILE *out=NULL;
char *filename=NULL;
char *outfile=NULL;
int i;
int args=0;
unsigned char buffer[1024];
unsigned int total=0;
unsigned int tts=0;
int unit=1504;
int bNull=0;
int bps=0;
int bDebug=0;

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case 'D' :
		    bDebug = 1;
		    break;
		case 'N' :
		    bNull = 1;
		    break;
		case 'U' :
		    unit = atoi(&argv[i][2]);
		    if( unit<100 )
		    {
		    	fprintf( stdout, "Invalid unit(%d)\n", unit );
			exit( 1 );
		    }
		    break;
		case 'B' :	// Kbps
		    bps = atoi( &argv[i][2] );
		    if( bps<1 )
		    {
		    	fprintf( stdout, "Invalid bps (%d)\n", bps );
			ts2tts_Usage( argv[0] );
		    }
		    break;
		case 'K' :	// Kbps
		    bps = atoi( &argv[i][2] )*1000;
		    if( bps<1 )
		    {
		    	fprintf( stdout, "Invalid Kbps (%d)\n", bps/1000 );
			ts2tts_Usage( argv[0] );
		    }
		    break;
		case 'M' :	// Mbps
		    bps = atoi( &argv[i][2] )*1000*1000;
		    if( bps<1 )
		    {
		    	fprintf( stdout, "Invalid Mbps (%d)\n", bps/1000/1000 );
			ts2tts_Usage( argv[0] );
		    }
		    break;
		default :
		    ts2tts_Usage( argv[0] );
		    break;
		}
	    } else {
		switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		case 1 :
		    outfile  = argv[i];
		    break;
		default :
		    ts2tts_Usage( argv[0] );
		    break;
		}
		args++;
	    }
	}

	if( filename==NULL )
	{
	    fprintf( stdout, "No filename\n" );
	    exit( 1 );
	}

	fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}

	if( outfile )
	{
	    out = fopen( outfile, "wb" );
	    if( out==NULL )
	    {
		fprintf( stdout, "Can't write %s\n", outfile );
		exit( 1 );
	    }
	}

	double dUnit  = 0;
	double dTotal = 0;
	long long ll_total=0;
	if( bps>0 )
	{
	    fprintf( stdout, "bps=%d\n", bps );
	    dUnit = 27000000;
	    dUnit = dUnit*188*8;
	    dUnit = dUnit/bps;
	}
	for( packet=0; ; packet++ )
	{
	    int readed = fread( &buffer[4], 1, 188, fp );
	    if( readed<1 )
	    {
	    	fprintf( stdout, "EOF@0x%X\n", total );
		break;
	    }
	    int PID=((buffer[5]<<8) | (buffer[6])) & 0x1FFF;
	    buffer[0] = (tts>>24)&0xFF;
	    buffer[1] = (tts>>16)&0xFF;
	    buffer[2] = (tts>> 8)&0xFF;
	    buffer[3] = (tts>> 0)&0xFF;
	    if( out )
	    {
//	    	fprintf( stdout, "PID=%4X\n", PID );
		if( (bNull==0) || (PID!=0x1FFF) )
		{
		    int written = fwrite( buffer, 1, 192, out );
		    if( written<1 )
		    fprintf( stdout, "written=%d\n", written );
		}
	    }
	    total+=readed;
	    if( bps>0 )
	    {
	    	dTotal   = dTotal + dUnit;
		long long ld_total = (long long)dTotal;
		unit     = ld_total-ll_total;
		ll_total = ll_total+unit;
		if( bDebug )
		fprintf( stdout, "dUnit=%f, unit=%d\n", dUnit, unit );
	    }
	    tts += unit;
	}
	fclose( fp );

	return 0;
}

#ifndef MAIN
int bDebug=0;
int main( int argc, char *argv[] )
{
	return ts2tts( argc, argv );
}
#endif

