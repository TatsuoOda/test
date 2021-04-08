
/*
	ttsEdit.cpp
				2013.3.19  by T.Oda
*/

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// memset

#include "tsLib.h"	// TcTime

#include "main.h"

// ----------------------------------------------

void ttsEdit_Usage( char *prg )
{
	fprintf( stdout, "%s filename\n", prg );
	exit( 1 );
}

int ttsEdit( int argc, char *argv[] )
{
int i;
unsigned char buffer[1024];
int args=0;
char *inFilename=NULL;
char *outFilename=NULL;
FILE *infile=NULL;
FILE *outfile=NULL;
int nNull=0;
int nPID=(-1);

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case 'P' :	// PID
		    nPID = atoi( &argv[i][2] );
		    break;
		default :
		    ttsEdit_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'N' :	// Add Null packet at end
		    nNull = atoi( &argv[i][2] );
		    break;
		default :
		    ttsEdit_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='/' )
	    {
	    	switch( argv[i][1] )
		{
		default :
		    ttsEdit_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='=' )
	    {
	    	switch( argv[i][1] )
		{
		default :
		    ttsEdit_Usage( argv[0] );
		    break;
		}
	    } else {
		switch( args )
		{
		case 0 :
		    inFilename = argv[i];
		    break;
		case 1 :
		    outFilename = argv[i];
		    break;
		}
		args++;
	    }
	}

	if( inFilename==NULL )
	{
	    ttsEdit_Usage( argv[0] );
	}
	infile = fopen( inFilename, "rb" );
	if( infile==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", inFilename );
	    exit( 1 );
	}
	if( outFilename )
	{
	    outfile= fopen( outFilename, "wb" );
	}

	int total=0;
	int tts;
	int L_tts = (-1);
	int delta=100000;
	int PID=(-1);
	while( 1 )
	{
	    int readed = fread( buffer, 1, 192, infile );
	    total+=readed;
	    if( readed<192 )
	    {
	    	fprintf( stdout, "EOF@%X\n", total );
		break;
	    }
	    tts = (buffer[0]<<24)
	        | (buffer[1]<<16)
	        | (buffer[2]<< 8)
	        | (buffer[3]<< 0);
	    if( L_tts>=0 )
	    {
	    	fprintf( stdout, "tts=%8X : delta=%d\n", tts, tts-L_tts );
		if( (tts-L_tts)<delta )
		    delta=tts-L_tts;
	    }
	    if( buffer[4]==0x47 )
	    {
	    	PID = ((buffer[5]<<8) | buffer[6]) & 0x1FFF;
		if( nPID==PID )
		{
		    int ii;
		    if( (buffer[ 8]==0x00)
		     && (buffer[ 9]==0x00)
		     && (buffer[10]==0x01)
		     && ((buffer[11]&0xF0)==0xE0) )
		    {
			for( ii=0; ii<16; ii++ )
			{
			    fprintf( stdout, "%02X ", buffer[4+ii] );
			}
			fprintf( stdout, "\n" );
			int bDisplay=1;
			unsigned int PTSH, PTSL, DTSH, DTSL;
			int offset;
			int len = parsePES_header( 
				&buffer[12], bDisplay,
				&PTSH, &PTSL, &DTSH, &DTSL, &offset );
		    } else {
#if 0
			for( ii=0; ii<8; ii++ )
			{
			    fprintf( stdout, "%02X ", buffer[4+ii] );
			}
			fprintf( stdout, "\n" );
#endif
		    }
		}
	    }
	    if( outfile )
	    {
	    	int written = fwrite( buffer, 1, 192, outfile );
		if( written<192 )
		{
		    fprintf( stdout, "Can't write@%X\n", total );
		}
	    }
	    L_tts = tts;
	}
	fclose( infile );

	if( outfile )
	{
	    for( i=0; i<nNull; i++ )
	    {
		tts += delta;
	    	memset( buffer, 0, 192 );
	    	buffer[0] = (tts>>24)&0xFF;
	    	buffer[1] = (tts>>16)&0xFF;
	    	buffer[2] = (tts>> 8)&0xFF;
	    	buffer[3] = (tts>> 0)&0xFF;
		buffer[4] = 0x47;
		buffer[5] = 0x1F;
		buffer[6] = 0xFF;
		buffer[7] = 0x10;
	    	int written = fwrite( buffer, 1, 192, outfile );
	    }
	    fclose( outfile );
	}
}

#ifndef MAIN
int bDebug=0;
int main( int argc, char *argv[] )
{
	return ttsEdit( argc, argv );
}
#endif

