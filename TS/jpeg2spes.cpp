
/*
	jpeg2spes.cpp
			2013.8.13  by T.Oda
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "main.h"
#include "spesLib.h"

void jpeg2spes_Usage( char *prg )
{
	fprintf( stdout, "%s filename1 .. filenameN\n", prg );
	exit( 1 );
}

int jpeg2spes( int argc, char *argv[] )
{
int i;
char *filename=NULL;
char outfile[1024];
struct stat Stat;
int DTS=0;
int PTS=0;

	sprintf( outfile, "out.spes" );
	
	FILE *pOut = fopen( outfile, "wb" );
	if( pOut==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", outfile );
	    exit( 1 );
	}

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case 'E' :
		    bExifReplace = 1;
		    break;
		default :
		    jpeg2spes_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    } else if( argv[i][0]=='=' )
	    {
	    } else {
		filename = argv[i];
		if( stat( filename, &Stat )==0 )
		{
		    fprintf( stdout, "File:%s, size=%d\n",
			    filename, (int)Stat.st_size );
		    FILE *fp = fopen( filename, "rb" );
		    if( fp==NULL )
		    {
			fprintf( stdout, "Can't open %s\n", filename );
			exit( 1 );
		    }
		    int ret = MJPEG( pOut, fp, Stat.st_size, DTS, PTS );
		    if( ret!=0 )
		    {
		    	fprintf( stdout, "MJPEG() ret=%d\n", ret );
		    }
		    fclose( fp );
		    DTS+=90000;
		    PTS+=90000;
		}
	    }
	}

	fclose( pOut );

	return 0;
}

#ifndef MAIN
int main( int argc, char *argv[] )
{
	return jpeg2spes( argc, argv );
}
#endif

