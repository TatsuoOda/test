
/*
	avi.cpp
 */

#if 0
#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>

#include "parseLib.h"

#define MODE_VIDEO	1
#define MODE_JPEG	2

int bDebug=0;

extern int mode;
extern int bSPES;

typedef unsigned int UINT;

// -------------------------------------------
int Riff( char FourCC[], FILE *fp, int size );
// -------------------------------------------

void Usage( char *prg )
{
	fprintf( stdout, "%s filename\n", prg );
	exit( 1 );
}

int main( int argc, char *argv[] )
{
char *filename=NULL;
int i;
int args=0;
unsigned char buffer[1024*1024];
int readed;
int size;

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		    case 'D' :
		    case 'd' :
		    	bDebug=1;
		    	break;
		    default :
		    	fprintf( stdout, "Unknown option -%c\n",
				argv[i][1] );
			exit( 1 );
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		    case 'S' :
		    	bSPES = 1;
		    	break;
		    default :
		    	fprintf( stdout, "Unknown option +%c\n",
				argv[i][1] );
			exit( 1 );
		}
	    } else {
	    	switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		default :
		    break;
		}
		args++;
	    }
	}

	if( filename==NULL )
	{
	    Usage( argv[0] );
	}

	FILE *fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open [%s]\n", filename );
	    exit( 1 );
	}
	char Name[16];
	{
	    memset( Name, 0, 16 );
	    readed = gread( (unsigned char *)Name, 1, 4, fp );
//	    fprintf( stdout, "readed=%d\n", readed );
	    if( readed<1 )
	    	EXIT();

	    readed = gread( buffer, 1, 4, fp );
	    if( readed<1 )
	    	EXIT();
	    size = Long_little(buffer);

	    if( strncmp( Name, "RIFF", 4 )==0 )
	    {
		char FourCC[16];
		fprintf( stdout, "RIFF Size=0x%X\n", size );
		memset( FourCC, 0, 16 );
		readed = gread( (unsigned char *)FourCC, 1, 4, fp );
		if( readed<1 )
		    EXIT();
		fprintf( stdout, "FourCC[%s]\n", FourCC );
		Riff( FourCC, fp, size );
	    } else {
	    	fprintf( stdout, "Not RIFF [%s]\n", Name );
	    }
	}

	fclose( fp );
}
#else

int bDebug=0;

int avi( int argc, char *argv[] );

int main( int argc, char *argv[] )
{
	return( avi( argc, argv ) );
}

#endif

