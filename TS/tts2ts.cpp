
/*
	tts2ts.cpp
		2012.5.1  by T.Oda
		2012.10.29 : -D : bDebug=1
				Support ATS Wrap
				Support stream which first interval not fixed
		2012.10.30 : use rest : correct output bitrate : OK
		2013.12. 5 : use pareLib and gread
		2013.12.24 : debug OK
 */

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// memset

//#include "tsLib.h"
#include "lib.h"
#include "parseLib.h"

#include "main.h"

#ifndef MAIN
int bDebug=0;
#endif

void tts2ts_Usage( char *prg )
{
	fprintf( stdout, "%s src-filename dst-filename\n", prg );
	exit( 1 );
}

int tts2ts( int argc, char *argv[] )
{
FILE *infile=NULL;
FILE *outfile=NULL;
char *filename=NULL;
char *dstname =NULL;
int i;
int args=0;
unsigned char buf[1024*4];
unsigned char off[188];
int bDebug=0;

	memset( off, 0xFF, 188 );
	off[0] = 0x47;
	off[1] = 0x1F;
	off[3] = 0x10;
	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case 'D' :
		    bDebug = 1;
		    break;
		default :
		    tts2ts_Usage( argv[0] );
		    break;
		}
	    	
	    } else if( argv[i][0]=='+' )
	    {
	    } else {
	    	switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		case 1 :
		    dstname  = argv[i];
		    break;
		default :
		    tts2ts_Usage( argv[0] );
		    break;
		}
		args++;
	    }
	}

	if( filename==NULL )
	    tts2ts_Usage( argv[0] );
	if( dstname ==NULL )
	    tts2ts_Usage( argv[0] );

	infile = fopen( filename, "rb" );
	if( infile==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}
	outfile = fopen( dstname, "wb" );
	if( outfile==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", dstname );
	    exit( 1 );
	}
	// -----------------------------------------------------------
	// Check Unit
	// -----------------------------------------------------------
	int readed=0, written=0;
	unsigned int total=0;
	unsigned int L_PCR=0xFFFFFFFF;
	unsigned int PCR=0;
	int diff=(-1);
	int unit=0;
	int packet=0;
	fprintf( stdout, "Check Unit\n" );
	for( packet=0; packet<256; packet++ )
	{
	    int nDiff=0;
	    readed = fread( buf, 1, 192, infile );
	    if( readed<192 )
	    {
	    	fprintf( stdout, "EOF@0x%X\n", total );
	    	break;
	    }
	    PCR = (buf[0]<<24)
	        | (buf[1]<<16)
	        | (buf[2]<< 8)
	        | (buf[3]<< 0);
	    fprintf( stdout, "%6d : %8X : %8X ", packet, total, PCR );
	    if( L_PCR!=0xFFFFFFFF )
	    {
	    	if( PCR>L_PCR )
		{
		    nDiff = PCR-L_PCR;
		} else  {
//		    nDiff = (0xFFFFFFFF-L_PCR)+PCR+1;
		    nDiff = (0x40000000-L_PCR)+PCR;
		}
		fprintf( stdout, "%8d\n", nDiff );
		if( diff==nDiff )
		{
		    unit = diff;
		    break;
		}
	    } else {
		fprintf( stdout, "\n" );
	    }
	    L_PCR = PCR;
	    diff = nDiff;
	    total+=readed;
	}
	if( unit>0 )
	    fprintf( stdout, "unit=%d\n", unit );
	else {
	    fprintf( stdout, "Unit not valid\n" );
	    exit( 1 );
	}
	// -----------------------------------------------------------
	// output TS
	// -----------------------------------------------------------
	L_PCR=0xFFFFFFFF;
	fseek( infile, 0, SEEK_SET );
	packet=0;
	total=0;
	int rest=0;
	int totalW=0;
	for( packet=0; ; packet++ )
	{
	    readed = gread( buf, 1, 192, infile );
	    if( readed<192 )
	    {
	    	fprintf( stdout, "EOF@0x%X\n", total );
	    	break;
	    }
	    PCR = (buf[0]<<24)
	        | (buf[1]<<16)
	        | (buf[2]<< 8)
	        | (buf[3]<< 0);
	
	    unsigned long long LL=0;
	    int bAdapt   = buf[4+3]&0x20;
	    int adaptLen = buf[4+4];
	    int bPCR = buf[4+5]&0x10;
	    if( (bAdapt==0) || (adaptLen==0) )
		bPCR = 0;
	    if( bPCR )
	    {
	   	UINT PCR_H, PCR_L;
		PCR_H = (buf[4+ 6]<< 8) | (buf[4+ 7]<< 0);
		PCR_L = (buf[4+ 8]<<24) | (buf[4+ 9]<<16)
		      | (buf[4+10]<< 8) | (buf[4+11]<< 0);
		LL = (unsigned long long)PCR_H;
		LL = (LL<<32) | PCR_L;
		LL = LL>>15;
	    }
	    int nSpace=0;
	    int nDiff=0;
	    if( L_PCR!=0xFFFFFFFF )
	    {
	    	nDiff = PCR-L_PCR;
	    	if( PCR>L_PCR )
		{
		    nDiff = PCR-L_PCR;
		} else  {
		    nDiff = (0xFFFFFFFF-L_PCR)+PCR+1;
//		    nDiff = (0x40000000-L_PCR)+PCR;
		    fprintf( stdout, "\nPCR rewind ? : %8X -> %8X\n",
		    	L_PCR, PCR );
		}
	    } else {
	    	nDiff = unit;
	    }
	    int cDiff = nDiff+rest;
	    int sDiff = cDiff;
	    if( bDebug )
	    fprintf( stdout, "%8d %8d(%5d)", nDiff, cDiff, rest );
	    if( cDiff!=unit )
	    {
		while( cDiff>unit )
		{
		    written = fwrite( off, 1, 188, outfile );
		    nSpace++;
		    cDiff-=unit;
		    totalW+=written;
		}
		rest=cDiff-unit;
	    }
	    fprintf( stdout, 
	    "%8X : PCR=%8X,%8X : nSpace=%4d(sDiff=%7d,cDiff=%4d) : rest=%5d", 
		totalW, L_PCR, PCR, nSpace, sDiff, cDiff, rest );
	    if( bPCR )
	    fprintf( stdout, " %8llX ", LL );
	    if( nSpace>10000 )	// 10000 packet
	    {
		fprintf( stdout, "Too large nSpace (%d)\n", nSpace );
		EXIT( );
	    }
	    fprintf( stdout, "\n" );
	    L_PCR = PCR;
	    written = fwrite( &buf[4], 1, 188, outfile );
	    if( written<188 )
	    {
	    	fprintf( stdout, "Can't write (0x%X)\n", total );
	    }
	    totalW+=written;
	    total+=readed;
	}
	fclose( infile );
	fclose( outfile );
	return 0;
}

#ifndef MAIN
int main( int argc, char *argv[] )
{
	return tts2ts( argc, argv );
}
#endif

