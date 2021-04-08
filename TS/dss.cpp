
/*
	dss.cpp
		2013.7.31  by T.Oda
		2013.8. 9
*/

#include <stdio.h>
#include <stdlib.h>	// exit

#include "parseLib.h"

#include "main.h"

#define MAX_PID	8192

void dssUsage( char *prg )
{
	fprintf( stdout, "%s filename\n", prg );
	exit( 1 );
}

int dss( int argc, char *argv[] )
{
unsigned char buffer[1024];
int i;
int readed;
int args=0;
char *filename = NULL;
char *outfilename = NULL;
FILE *fp=NULL;
FILE *outfile=NULL;
FILE *out_fp [MAX_PID];
FILE *addr_fp[MAX_PID];
FILE *tts_fp =NULL;
long long nAddr[MAX_PID];
char str[1024];

	for( i=0; i<MAX_PID; i++ )
	{
	    out_fp [i] = NULL;
	    addr_fp[i] = NULL;
	    nAddr  [i] = 0;
	}

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    } else if( argv[i][0]=='+' )
	    {
	    } else if( argv[i][0]=='=' )
	    {
	    } else {
	    	switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		case 1 :
		    outfilename = argv[i];
		    break;
		default :
		    dssUsage( argv[0] );
		    break;
		}
		args++;
	    }
	}

	if( filename==NULL )
	{
	    dssUsage( argv[0] );
	}
	fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}
	if( outfilename!=NULL )
	{
	    outfile = fopen( outfilename, "wb" );
	    if( outfile==NULL )
	    {
		fprintf( stdout, "Can't open %s\n", outfilename );
		exit( 1 );
	    }
	}

	int packetSize=130;
	int skip=3;
	int byteOffset=0;
	for( i=0; ; i++ )
	{
	    readed = gread( buffer, 1, packetSize, fp );
	    if( readed<130 )
	    {
	    	fprintf( stdout, "EOF(%d)\n", readed );
		break;
	    }
	    fprintf( stdout, "%8d : %02X %02X %02X\n",
	    	i, buffer[0], buffer[1], buffer[2] );
	    int PID = ((buffer[0]<<8) | buffer[1])&0x1FFF;
	    if( out_fp[PID]==NULL )
	    {
	    	char path[1024];
		sprintf( path, "DSS-%04X.bin", PID );
	    	out_fp[PID] = fopen( path, "wb" );
		if( out_fp[PID]==NULL )
		{
		    fprintf( stdout, "Can't open %s\n", path );
		    exit( 1 );
		}
	    }
	    if( addr_fp[PID]==NULL )
	    {
	    	char path[1024];
		sprintf( path, "DSS-%04X.txt", PID );
	    	addr_fp[PID] = fopen( path, "w" );
		if( addr_fp[PID]==NULL )
		{
		    fprintf( stdout, "Can't open %s\n", path );
		    exit( 1 );
		}
		fprintf( addr_fp[PID], "%s\n", filename );
	    }
	    int writeSize = 130-skip;
	    if( out_fp[PID] )
	    	fwrite( &buffer[3], 1, writeSize, out_fp[PID] );
	    if( outfile )
	    	fwrite( &buffer[3], 1, writeSize, outfile );
	    if( addr_fp[PID] )
	    {
#if 0
		unsigned int cPCRH = PCR_H;
		unsigned int cPCRL = PCR_L;
		unsigned int cPCRpid = PCR_pid;
		if( PCRH[adrPID]!=0xFFFFFFFF )
		{
		    cPCRH=PCRH[PID];
		    cPCRL=PCRL[PID];
		    cPCRpid = PID;
		}
		unsigned long long LL = cPCRH;
		unsigned long long LL_MASK = 0xFFFFFFFFFLL;
		LL = (LL<<32) | cPCRL;
		LL = LL>>15;
#endif
		sprintf( str, "%10llX, %10llX, %10llX, %4X, %8X %8X %9llX %4X",
		    g_addr-packetSize,	// TS addr(top)
		    g_addr-packetSize+skip+byteOffset,
					   	// TS addr(pes start)
		    nAddr[PID], 		// PES addr
		    writeSize,			// PES size
		    0, 0, 0LL, PID
		    );		// PES size
#if 0
		    writeSize,		// PES size
		    cPCRH, cPCRL, 
		    LL & LL_MASK, 
		    cPCRpid & 0xFFFF );
#endif
		fprintf( addr_fp[PID], "%s", str );
		if( tts_fp==NULL )
		    fprintf( addr_fp[PID], "\n" );
	    }
	    if( writeSize>0 )
		nAddr[PID] += writeSize;
	}
	fclose( fp );
	if( outfile )
	    fclose( outfile );
	for( i=0; i<MAX_PID; i++ )
	{
	    if( out_fp[i] )
		fclose( out_fp[i] );
	    if( addr_fp[i] )
		fclose( addr_fp[i] );
	}

	return 0;
}

#ifndef MAIN
int bDebug=0;

int main( int argc, char *argv[] )
{
	return dss( argc, argv );
}
#endif

