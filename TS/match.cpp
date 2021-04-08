/*
	match.cpp
		2013.10.7  by T.Oda

		made for analyze MJPEG STD buffer broken
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int Search( char *filename, int offset, unsigned char *Compare )
{
int i;
unsigned char buf[2048];
	fprintf( stdout, "Search(%s:%X:%02X,%02X,%02X,%02X)\n",
		filename, offset, 
		Compare[0], Compare[1], Compare[2], Compare[3] );
    FILE *fp = fopen( filename, "rb" );
    if( fp==NULL )
    {
    	fprintf( stdout, "Can't open %s\n", filename );
	exit( 1 );
    }
    int j=0;
    int total=offset;
    fseek( fp, total, SEEK_SET );
    int readed=fread( buf, 1, 1024, fp );
    while( 1 )
    {
    	int readed=fread( &buf[1024], 1, 1024, fp );
	if( readed<1 )
	{
	    fprintf( stdout, "EOF@%X\n", total );
	    break;
	}
	for( i=0; i<1024; i++ )
	{
	    j=0;
	    int nSame=0;
	    if( buf[i]==Compare[j] )
	    {
#if 0
	    	fprintf( stdout, "%X(%X,%03X):%02X\n", 
			total+i, total, i, buf[i] );
#endif
		int src=0;
		while( 1 )
		{
		    for( j=0; j<1024; j++ )
		    {
			if( buf[i+j]==Compare[src++] )
			{
			    nSame++;
    //			fprintf( stdout, "(%d)", nSame );
			} else {
			    if( nSame>3 )
			    fprintf( stdout, "total=%X,i=%X,j=%X, nSame=%d\n", 
				total, i,j, nSame );
			    break;
			}
		    }
		    if( j<1024 )
			break;
		    total+=readed;
		    memcpy( buf, &buf[1024], 1024 );
		    readed=fread( &buf[1024], 1, 1024, fp );
		    if( readed<1 )
		    {
		    	fprintf( stdout, "EOF\n" );
			break;
		    }
		}
		if( nSame>16 )
		{
		    fprintf( stdout, 
	    "nSame=%4d : %X- : %02X %02X %02X %02X : %02X %02X %02X %02X\n", 
			nSame, total+i,
			Compare[0], Compare[1], Compare[2], Compare[3],
			buf[i+0], buf[i+1], buf[i+2], buf[i+3] );
		}
	    }
	}
	total+=readed;
	memcpy( buf, &buf[1024], 1024 );
    }
    fclose( fp );
}

void Usage( char *prg )
{
    fprintf( stdout, "%s file1 file2\n", prg );
    exit( 1 );
}

int main( int argc, char *argv[] )
{
char *filename1=NULL, *filename2=NULL;
int i;
int args=0;
FILE *pFile1, *pFile2;
unsigned char buf1[1024], buf2[1024];

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    } else {
	    	switch( args )
		{
		case 0 :
		    filename1 = argv[i];
		    break;
		case 1 :
		    filename2 = argv[i];
		    break;
		default :
		    Usage( argv[0] );
		    break;
		}
		args++;
	    }
	}

	if( (filename1==NULL) || (filename2==NULL) )
	{
		Usage( argv[0] );
	}
	pFile1 = fopen( filename1, "rb" );
	if( pFile1==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename1 );
	    exit( 1 );
	}
	pFile2 = fopen( filename2, "rb" );
	if( pFile2==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename2 );
	    exit( 1 );
	}

	int total1=0;
	int total2=0;
	int bFirst=1;
	while( 1 )
	{
	    int readed1, readed2;
	    readed1 = fread( buf1, 1, 1024, pFile1 );
	    readed2 = fread( buf2, 1, 1024, pFile2 );
	    if( readed1<1 )
	    {
		fprintf( stdout, "EOF(1)\n" );
		break;
	    }
	    if( readed1<2 )
	    {
		fprintf( stdout, "EOF(2)\n" );
		break;
	    }
	    for( i=0; i<1024; i++ )
	    {
	    	if( buf1[i] != buf2[i] )
		{
		    int bufSize=1024*1024*8;
		    unsigned char Compare[bufSize];
		    if( bFirst )
		    {
			fprintf( stdout, "Differ %X,%X:%02X,%02X\n", 
			    total1+i, total2+i, buf1[i], buf2[i] );
			memcpy( Compare, &buf2[i], 1024-i );
			fread( &Compare[1024-i], 1, i+bufSize-1024, pFile2 );
			Search( filename1, total1+i, Compare );
			fseek( pFile1, total1, SEEK_SET );
//			exit( 1 );
		    }
		    bFirst=0;
		} else {
		    bFirst=1;
		}
	    }
	    total1+=readed1;
	    total2+=readed2;
	}

	fclose( pFile1 );
	fclose( pFile2 );

	return 0;
}


