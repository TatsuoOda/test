
/*
	spesMerge.cpp
		2012.10.12  by T.Oda
		2013. 7.26 =M : for MVC (base+nonBase)
 */

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// memset

#include "main.h"

/*
	for AVC only
	merge between SPS area from several files

	spesMerge filename1 filename2 .. filenameN -1 -2 -3
	file1 file2 file3 file1 file2 file3 ....

	spesMerge filename1 filename2 .. filenameN -1 -2 -3 -2 
	file1 file2 file3 file2 file1 file2 file3 ....
*/

int ReadSequence( char *filename, FILE *fp, FILE *outfile )
{
int readed, written;
int SPES_Size=0;
int code=(-1);
#define MAX_FRAME_SIZE	1024*1024
unsigned char header[16];
unsigned char buffer[MAX_FRAME_SIZE];
int nData=0;
int nFrame=0;

	for( nData=0; ; nData++ )
	{
	    fprintf( stdout, "%s : read header(%d)\n", filename, nData );
	    memset( header, 0, 16 );
	    readed = fread( header, 1, 16, fp );
	    if( readed<16 )
	    {
		fprintf( stdout, "%s : readed=%d/16\n", filename, readed );
		return nFrame;
	    }
	    int i;
	    for( i=0; i<16; i++ )
	    {
	    	fprintf( stdout, "%02X ", header[i] );
	    }
	    fprintf( stdout, "\n" );
	    fflush( stdout );
	    SPES_Size = (header[0]<<24)
		      | (header[1]<<16)
		      | (header[2]<< 8)
		      | (header[3]<< 0);
	    code = header[4];
	    fprintf( stdout, "SPES_Size=%8X, code=%02X\n", SPES_Size, code );
	    fflush( stdout );
	    if( nFrame>0 )
	    {
	    	if( code==0x31 )	// SPS
		{
		    fseek( fp, -16, SEEK_CUR );
		    return nFrame;
		}
	    }
	    if( code==0x30 )
	    {
	    	nFrame++;
		fprintf( stdout, "%s : %d\n", filename, nFrame );
	    }
	    if( SPES_Size>=MAX_FRAME_SIZE )
	    {
		fprintf( stdout, "%s : Too large frame(%d)\n", 
		    filename, SPES_Size );
		exit( 1 );
	    }
	    SPES_Size-=16;
	    if( outfile )
	    {
		readed = fread( buffer, 1, SPES_Size, fp );
		if( readed<SPES_Size )
		{
		    fprintf( stdout, "%s : readed=%d/%d\n", 
			    filename, readed, SPES_Size );
		    exit( 1 );
		}
		written = fwrite( header, 1, 16, outfile );
		written = fwrite( buffer, 1, SPES_Size, outfile );
	    } else {
	    	readed = fseek( fp, SPES_Size, SEEK_CUR );
	    }
	}
	return nFrame;
}

int ReadFrame( char *filename, FILE *fp, FILE *outfile )
{
int readed, written;
int SPES_Size=0;
int code=(-1);
#define MAX_FRAME_SIZE	1024*1024
unsigned char header[16];
unsigned char buffer[MAX_FRAME_SIZE];
int nData=0;
int nFrame=0;

//	for( nData=0; ; nData++ )
	for( nFrame=0; nFrame<1; )
	{
	    fprintf( stdout, "%s : read header(%d)\n", filename, nData );
	    memset( header, 0, 16 );
	    readed = fread( header, 1, 16, fp );
	    if( readed<16 )
	    {
		fprintf( stdout, "%s : readed=%d/16\n", filename, readed );
		return nFrame;
	    }
	    int i;
	    for( i=0; i<16; i++ )
	    {
	    	fprintf( stdout, "%02X ", header[i] );
	    }
	    fprintf( stdout, "\n" );
	    fflush( stdout );
	    SPES_Size = (header[0]<<24)
		      | (header[1]<<16)
		      | (header[2]<< 8)
		      | (header[3]<< 0);
	    code = header[4];
	    fprintf( stdout, "SPES_Size=%8X, code=%02X\n", SPES_Size, code );
	    fflush( stdout );
/*
	    if( nFrame>0 )
	    {
	    	if( code==0x31 )	// SPS
		{
		    fseek( fp, -16, SEEK_CUR );
		    return nFrame;
		}
	    }
*/
	    if( code==0x30 )
	    {
	    	nFrame++;
		fprintf( stdout, "%s : %d\n", filename, nFrame );
	    }
	    if( SPES_Size>=MAX_FRAME_SIZE )
	    {
		fprintf( stdout, "%s : Too large frame(%d)\n", 
		    filename, SPES_Size );
		exit( 1 );
	    }
	    SPES_Size-=16;
	    if( outfile )
	    {
		readed = fread( buffer, 1, SPES_Size, fp );
		if( readed<SPES_Size )
		{
		    fprintf( stdout, "%s : readed=%d/%d\n", 
			    filename, readed, SPES_Size );
		    exit( 1 );
		}
		written = fwrite( header, 1, 16, outfile );
		written = fwrite( buffer, 1, SPES_Size, outfile );
	    } else {
	    	readed = fseek( fp, SPES_Size, SEEK_CUR );
	    }
	}
	return nFrame;
}
#define MODE_AVC	1
#define MODE_MVC	2

int spesMerge_Usage( char *prg )
{
	fprintf( stdout, "%s filename1 .. filenamen -# .. -#\n",
		prg );
	exit( 1 );
}

int spesMerge( int argc, char *argv[] )
{
int i;
#define MAX_SEQUENCE	256
#define MAX_FILE	256
int Sequence[MAX_SEQUENCE];
int nSequence=0;
char *filename[MAX_FILE];
int nFile=0;
FILE *fp[MAX_FILE];
FILE *outfile = NULL;
int mode=MODE_AVC;

	for( i=0; i<MAX_FILE; i++ )
	{
	    filename[i] = NULL;
	    fp[i] = NULL;
	}
	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case '1' :
		case '2' :
		case '3' :
		case '4' :
		case '5' :
		case '6' :
		case '7' :
		case '8' :
		case '9' :
		    if( nSequence<MAX_FILE )
		    {
			Sequence[nSequence++] = atoi( &argv[i][1] );
		    } else {
			fprintf( stdout, "Too many sequences\n" );
			exit( 1 );
		    }
		    break;
		default :
		    spesMerge_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		default :
		    spesMerge_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='=' )
	    {
	    	switch( argv[i][1] )
		{
		case 'M' :	// MVC view
		    mode = MODE_MVC;
		    break;
		default :
		    spesMerge_Usage( argv[0] );
		    break;
		}
	    } else {
	    	if( nFile<MAX_FILE )
		{
		    filename[nFile++] = argv[i];
		} else {
		    fprintf( stdout, "Too many files\n" );
		    exit( 1 );
		}
	    }
	}
	if( nSequence==0 )
	{
	    Sequence[nSequence++] = 1;
	}
	outfile = fopen( "output.spes", "wb" );
	if( outfile==NULL )
	{
	    fprintf( stdout, "Can'T open output.seps\n" );
	    exit( 1 );
	}
	for( i=0; i<nFile; i++ )
	{
	    fp[i] = fopen( filename[i], "rb" );
	    if( fp[i]==NULL )
	    {
	    	fprintf( stdout, "Can't open %s\n", filename[i] );
		exit( 1 );
	    }
	}
	for( i=0; i<nSequence; i++ )
	{
	    if( (Sequence[i]<1) || (Sequence[i]>nFile) )
	    {
	    	fprintf( stdout, "Invalid sequence[%d]=%d\n",
		    i, Sequence[i] );
	    }
	}
	// ----------------------------------------------------
	if( mode==MODE_AVC )
	{
	    int seq=0;
	    for( seq=0; ; seq++ )
	    {
		int n;
//		int SPES_Size=0;
//		int code=(-1);
		int frame=0;
		int total=0;
		for( n=0; n<nFile; n++ )
		{
		    if( (n+1)==Sequence[seq%nSequence] )
		    {
			// Copy to output
			frame = ReadSequence( filename[n], fp[n], outfile );
		    } else {
			// Seek next
			frame = ReadSequence( filename[n], fp[n], NULL );
		    }
		    total+=frame;
		}
		if( frame==0 )
		{
		    fprintf( stdout, "EOF\n" );
		    break;
		}
	    }
	} else if( mode==MODE_MVC )
	{
	    int seq=0;
	    for( seq=0; ; seq++ )
	    {
//		int SPES_Size=0;
//		int code=(-1);
		int frame;
		int total=0;
		// Copy to output
		frame = ReadFrame( filename[0], fp[0], outfile );
		total+=frame;
		frame = ReadFrame( filename[1], fp[1], outfile );
		total+=frame;
		if( frame==0 )
		{
		    fprintf( stdout, "EOF\n" );
		    break;
		}
	    }
	}
	// ----------------------------------------------------
	for( i=0; i<nFile; i++ )
	{
	    fclose( fp[i] );
	}
	fclose( outfile );

	return 0;
}

#ifndef MAIN
int main( int argc, char *argv[] )
{
	return spesMerge( argc, argv );
}
#endif

