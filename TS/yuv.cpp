
/*
	yuv.cpp
		2013.9.4  by T.Oda
		2013.9.5  MODE_420 support
*/

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// memset

#define MODE_420	0
#define MODE_422	1
#define MODE_444	2

void Usage( char *prg )
{
	fprintf( stdout, "%s srcFilename dstFilename\n", prg );
	exit( 1 );
}

int main( int argc, char *argv[] )
{
int i;
int args=0;
char *srcFilename=NULL;
char dstFilename[256];
FILE *infile=NULL;
FILE *outfile=NULL;
int mode=MODE_422;
int frameSize=1920*1080*2;

	sprintf( dstFilename, "output.yuv" );
	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case '0' :
		    mode = MODE_420;
		    frameSize = 1920*1080*3/2;
		    break;
		case '2' :
		    mode = MODE_422;
		    frameSize = 1920*1080*4/2;
		    break;
		case '4' :
		    mode = MODE_444;
		    frameSize = 1920*1080*6/2;
		    break;
		default :
		    Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='-' )
	    {
	    } else if( argv[i][0]=='=' )
	    {
	    } else {
	    	switch( args )
		{
		case 0 :
		    srcFilename=argv[i];
		    break;
		case 1 :
		    memcpy( dstFilename, argv[i], strlen( argv[i] ) );
		    break;
		}
		args++;
	    }
	}

	if( srcFilename==NULL )
	{
	    Usage( argv[0] );
	}
	infile = fopen( srcFilename, "rb" );
	if( infile==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", srcFilename );
	    exit( 1 );
	}
	outfile = fopen( dstFilename, "wb" );
	if( outfile==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", dstFilename );
	    exit( 1 );
	}

	unsigned char *InBuf;
	unsigned char *OutBuf;;
	int readed;
	int total=0;
	InBuf = (unsigned char *)malloc( 1920*1080*2 );
	if( InBuf==NULL )
	{
	    fprintf( stdout, "Can't malloc InBuf\n" );
	    exit( 1 );
	}
	OutBuf = (unsigned char *)malloc( 1920*1080*3 );
	if( OutBuf==NULL )
	{
	    fprintf( stdout, "Can't malloc OutBuf\n" );
	    exit( 1 );
	}
	int x, y;
	int written;
	int totalW=0;
	int offset=0;
	// Input Packed 422
	while( 1 )
	{
	    readed = fread( InBuf, 1, 1920*1080*2, infile );
	    if( readed<1 )
	    {
	    	fprintf( stdout, "EOF\n" );
		break;
	    }
//	    fprintf( stdout, "readed=%d\n", readed );
	    total+=readed;
	    // Y
	    for( y=0; y<1080; y++ )
	    {
		for( x=0; x<1920; x++ )
		{
		    OutBuf[y*1920+x] = InBuf[y*1920*2+x*2+1];
		}
	    }
	    offset = 1920*1080;
//	    fprintf( stdout, "mode=%d, offset=%d\n", mode, offset );
	    // U
	    if( mode==MODE_420 )
	    {
		for( y=0; y<1080; y+=2 )
		{
		    for( x=0; x<960; x++ )
		    {
			OutBuf[offset+y/2*960+x] = InBuf[y*1920*2+x*4+2];
		    }
		}
		offset += 960*540;
	    }
	    if( mode==MODE_422 )
	    {
		for( y=0; y<1080; y++ )
		{
		    for( x=0; x<960; x++ )
		    {
			OutBuf[offset+y*960+x] = InBuf[y*1920*2+x*4+2];
		    }
		}
		offset += 960*1080;
	    }
	    if( mode==MODE_444 )
	    {
		for( y=0; y<1080; y++ )
		{
		    for( x=0; x<960; x++ )
		    {
			OutBuf[offset+y*1920+x*2+0] 
				= InBuf[y*1920*2+x*4+2];
			OutBuf[offset+y*1920+x*2+1] 
				= InBuf[y*1920*2+x*4+2];
		    }
		}
		offset += 1920*1080;
	    }
//	    fprintf( stdout, "mode=%d, offset=%d\n", mode, offset );
	    // V
	    if( mode==MODE_420 )
	    {
		for( y=0; y<1080; y+=2 )
		{
		    for( x=0; x<960; x++ )
		    {
			OutBuf[offset+y/2*960+x] = InBuf[y*1920*2+x*4+0];
		    }
		}
		offset += 960*540;
	    }
	    if( mode==MODE_422 )
	    {
		for( y=0; y<1080; y++ )
		{
		    for( x=0; x<960; x++ )
		    {
			OutBuf[offset+y*960+x] = InBuf[y*1920*2+x*4+0];
		    }
		}
		offset += 960*1080;
	    }
	    if( mode==MODE_444 )
	    {
		for( y=0; y<1080; y++ )
		{
		    for( x=0; x<960; x++ )
		    {
			OutBuf[offset+y*1920+x*2+0]
			    = InBuf[y*1920*2+x*4+0];
			OutBuf[offset+y*1920+x*2+1]
			    = InBuf[y*1920*2+x*4+0];
		    }
		}
		offset += 1920*1080;
	    }
//	    written = fwrite( OutBuf, 1, frameSize, outfile );
	    written = fwrite( OutBuf, 1, offset, outfile );
//	    fprintf( stdout, "written=%d\n", written );
	    totalW+=written;
	}
	fprintf( stdout, "Total %d bytes readed\n", total );
	fprintf( stdout, "Total %d bytes written\n", totalW );

	fclose( infile );
	fclose( outfile );

	return 0;
}
