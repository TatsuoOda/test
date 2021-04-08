
/*
	yuv2avi.cpp
		2013.2.21 by T.Oda

		2013.3.6 MAX_WIDTH=4096, MAX_HEIGHT=2160
*/

#include <stdio.h>
#include <stdlib.h>	// exit
#include <sys/types.h>	// stat
#include <sys/stat.h>	// stat
#include <unistd.h>	// stat

#define MAX_WIDTH	4096
#define MAX_HEIGHT	2160

#define DUMY 0xFF
#define ZEROx16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 
static char UYVY_Header[512] = {
'R' ,'I' ,'F' ,'F' ,DUMY,DUMY,DUMY,DUMY,'A' ,'V' ,'I' ,' ' ,'L' ,'I' ,'S' ,'T' ,
0xC0,0x00,0x00,0x00,'h' ,'d' ,'r' ,'l' ,'a' ,'v' ,'i' ,'h' ,0x38,0x00,0x00,0x00,
0x1A,0x41,0x00,0x00,0x00,0xE0,0xD4,0x0E,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,
// nFrame
DUMY,DUMY,DUMY,DUMY,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x48,0x3F,0x00,
//   width         ,      height
DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'L' ,'I' ,'S' ,'T' ,0x74,0x00,0x00,0x00,
's' ,'t' ,'r' ,'l' ,'s' ,'t' ,'r' ,'h' ,0x38,0x00,0x00,0x00,'v' ,'i' ,'d' ,'s' ,
'U' ,'Y' ,'V' ,'Y' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
//                         fps         ,                   ,     nFrame
0x01,0x00,0x00,0x00,DUMY,DUMY,DUMY,DUMY,0x00,0x00,0x00,0x00,DUMY,DUMY,DUMY,DUMY,
// frameSize
DUMY,DUMY,DUMY,DUMY,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x73,0x74,0x72,0x66,0x28,0x00,0x00,0x00,0x28,0x00,0x00,0x00,
//   width         ,      height
DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,0x01,0x00,0x10,0x00,'U' ,'Y' ,'V' ,'Y' ,
0x00,0x48,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,'J' ,'U' ,'N' ,'K' ,0x18,0x01,0x00,0x00,0x04,0x00,0x00,0x00,
ZEROx16, ZEROx16, ZEROx16, ZEROx16, ZEROx16, ZEROx16, ZEROx16, ZEROx16,
ZEROx16, ZEROx16, ZEROx16, ZEROx16, ZEROx16, ZEROx16, ZEROx16, ZEROx16,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,'L' ,'I' ,'S' ,'T' ,DUMY,DUMY,DUMY,DUMY,'m' ,'o' ,'v' ,'i' 
};
static char I420_Header[256] = {
'R' ,'I' ,'F' ,'F' ,DUMY,DUMY,DUMY,DUMY,'A' ,'V' ,'I' ,' ' ,'L' ,'I' ,'S' ,'T' ,
0xC0,0x00,0x00,0x00,'h' ,'d' ,'r' ,'l' ,'a' ,'v' ,'i' ,'h' ,0x38,0x00,0x00,0x00,
0x35,0x82,0x00,0x00,0xDC,0x74,0x6A,0x07,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,
DUMY,DUMY,DUMY,DUMY,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'L' ,'I' ,'S' ,'T' ,0x74,0x00,0x00,0x00,
's' ,'t' ,'r' ,'l' ,'s' ,'t' ,'r' ,'h' ,0x38,0x00,0x00,0x00,'v' ,'i' ,'d' ,'s' ,
'I' ,'4' ,'2' ,'0' ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x01,0x00,0x00,0x00,DUMY,DUMY,DUMY,DUMY,0x00,0x00,0x00,0x00,DUMY,DUMY,DUMY,DUMY,
DUMY,DUMY,DUMY,DUMY,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,'s' ,'t' ,'r' ,'f' ,0x28,0x00,0x00,0x00,0x28,0x00,0x00,0x00,
DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,DUMY,0x01,0x00,0x0C,0x00,'I' ,'4' ,'2' ,'0' ,
0x00,0xA4,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,'J' ,'U' ,'N' ,'K' ,0x18,0x00,0x00,0x00,0x04,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,'L' ,'I' ,'S' ,'T' ,DUMY,DUMY,DUMY,DUMY,'m' ,'o' ,'v' ,'i' 
};

void Usage( char *prg )
{
	fprintf( stdout, "%s filename\n", prg );
	exit( 1 );
}

int main( int argc, char *argv[] )
{
int i;
int args=0;
char *filename=NULL;
static char defaultOutfile[] = "outfile.avi";
char *outFilename=defaultOutfile;
int Width =1920;
int Height=1080;
int fps=60;
int bSGI=0;
int bMono=0;
int bPlane=0;
int HeaderSize=512;
int forceFrame=0;
int nSkipFrame=0;

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='/' )
	    {
	    	switch( argv[i][1] )
		{
		default :
		    Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case 'W' :
		    Width = atoi( &argv[i][2] );
		    if( Width>MAX_WIDTH )
		    {
		    	fprintf( stdout, "Tool large width(%d)\n", Width );
			exit( 1 );
		    }
		    break;
		case 'H' :
		    Height = atoi( &argv[i][2] );
		    if( Height>MAX_HEIGHT )
		    {
		    	fprintf( stdout, "Tool large height(%d)\n", Height );
			exit( 1 );
		    }
		    break;
		case 'F' :
		    fps = atoi( &argv[i][2] );
		    break;
		case 'N' :
		    forceFrame = atoi( &argv[i][2] );
		    break;
		case 'S' :
		    nSkipFrame = atoi( &argv[i][2] );
		    break;
		default :
		    Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'M' :	// mono chrome
		    bMono=1;
		    break;
		case 'P' :	// plane format  (I420)
		    bPlane = 1;
		    break;
		default :
		    Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='=' )
	    {
	    	switch( argv[i][1] )
		{
		case 'S' :	// sgi format
		    bSGI=1;
		    break;
		default :
		    Usage( argv[0] );
		    break;
		}
	    } else {
	    	switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		case 1 :
		    outFilename = argv[i];
		    break;
		}
		args++;
	    }
	}
	if( filename==NULL )
	{
	    Usage( argv[0] );
	}
	if( (Width<1) || (Height<1) || (fps<1) )
	{
	    fprintf( stdout, "Width =%d\n", Width );
	    fprintf( stdout, "Height=%d\n", Height );
	    fprintf( stdout, "fps   =%d\n", fps );
	    Usage( argv[0] );
	}

	struct stat Stat;
	if( stat( filename, &Stat )!=0 )
	{
	    fprintf( stdout, "Can't stat %s\n", filename );
	    Usage( argv[0] );
	}
	FILE *infile = fopen( filename, "rb" );
	if( infile==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    Usage( argv[0] );
	}
	FILE *outfile = fopen( outFilename, "wb" );
	if( outfile==NULL )
	{
	    fprintf( stdout, "Can't write %s\n", outFilename );
	    Usage( argv[0] );
	}
	// 400
	fprintf( stdout, "filesize=%X\n", Stat.st_size );
	int frameSize = Width*Height+Width*Height/4+Width*Height/4;
	if( bSGI )
	    frameSize = Width*Height*2*3;
	fprintf( stdout, "frameSize=%d\n", frameSize );
	int nFrame = Stat.st_size/frameSize;
	fprintf( stdout, "nFrame=%d\n", nFrame );
	int remain = Stat.st_size-frameSize*nFrame;
	if( remain>0 )
	{
	    fprintf( stdout, "Filesize has fraction (%d)\n", remain );
	}
	if( forceFrame>0 )
	{
	    nFrame = forceFrame;
	    fprintf( stdout, "nFrame changed to %d\n", nFrame );
	}
#if 0
	nFrame=30;	// for Debug
#endif
	int aviFrameSize = Width*Height*2;
	if( bPlane )
	{
	    aviFrameSize = Width*Height*3/2;
	    HeaderSize = 256;
	}
	int aviVideoSize = (aviFrameSize+8)*nFrame;
	int moviSize  = aviVideoSize+4;
	int aviIndexSize = 16*nFrame;
	int totalSize = HeaderSize+aviVideoSize+(aviIndexSize+0);
	fprintf( stdout, "moviSize    =%X\n", moviSize );
	fprintf( stdout, "aviIndexSize=%X\n", aviIndexSize );
	fprintf( stdout, "totalSize   =%X\n", totalSize );
	// --------------------------------------------------------
	UYVY_Header[0x004] = (totalSize>> 0)&0xFF;
	UYVY_Header[0x005] = (totalSize>> 8)&0xFF;
	UYVY_Header[0x006] = (totalSize>>16)&0xFF;
	UYVY_Header[0x007] = (totalSize>>24)&0xFF;

	UYVY_Header[0x040] = (Width>> 0)&0xFF;
	UYVY_Header[0x041] = (Width>> 8)&0xFF;
	UYVY_Header[0x042] = (Width>>16)&0xFF;
	UYVY_Header[0x043] = (Width>>24)&0xFF;

	UYVY_Header[0x044] = (Height>> 0)&0xFF;
	UYVY_Header[0x045] = (Height>> 8)&0xFF;
	UYVY_Header[0x046] = (Height>>16)&0xFF;
	UYVY_Header[0x047] = (Height>>24)&0xFF;

	UYVY_Header[0x0B0] = (Width>> 0)&0xFF;
	UYVY_Header[0x0B1] = (Width>> 8)&0xFF;
	UYVY_Header[0x0B2] = (Width>>16)&0xFF;
	UYVY_Header[0x0B3] = (Width>>24)&0xFF;

	UYVY_Header[0x0B4] = (Height>> 0)&0xFF;
	UYVY_Header[0x0B5] = (Height>> 8)&0xFF;
	UYVY_Header[0x0B6] = (Height>>16)&0xFF;
	UYVY_Header[0x0B7] = (Height>>24)&0xFF;

	UYVY_Header[0x084] = (fps   >> 0)&0xFF;
	UYVY_Header[0x085] = (fps   >> 8)&0xFF;
	UYVY_Header[0x086] = (fps   >>16)&0xFF;
	UYVY_Header[0x087] = (fps   >>24)&0xFF;

	UYVY_Header[0x030] = (nFrame>> 0)&0xFF;
	UYVY_Header[0x031] = (nFrame>> 8)&0xFF;
	UYVY_Header[0x032] = (nFrame>>16)&0xFF;
	UYVY_Header[0x033] = (nFrame>>24)&0xFF;

	UYVY_Header[0x08C] = (nFrame>> 0)&0xFF;
	UYVY_Header[0x08D] = (nFrame>> 8)&0xFF;
	UYVY_Header[0x08E] = (nFrame>>16)&0xFF;
	UYVY_Header[0x08F] = (nFrame>>24)&0xFF;

	UYVY_Header[0x090] = (aviFrameSize>> 0)&0xFF;
	UYVY_Header[0x091] = (aviFrameSize>> 8)&0xFF;
	UYVY_Header[0x092] = (aviFrameSize>>16)&0xFF;
	UYVY_Header[0x093] = (aviFrameSize>>24)&0xFF;
#if 0
	int audioLength = 0;
	UYVY_Header[0x108] = (audioLength>> 0)&0xFF;
	UYVY_Header[0x109] = (audioLength>> 8)&0xFF;
	UYVY_Header[0x10A] = (audioLength>>16)&0xFF;
	UYVY_Header[0x10B] = (audioLength>>24)&0xFF;
#endif
	UYVY_Header[0x1F8] = (moviSize>> 0)&0xFF;
	UYVY_Header[0x1F9] = (moviSize>> 8)&0xFF;
	UYVY_Header[0x1FA] = (moviSize>>16)&0xFF;
	UYVY_Header[0x1FB] = (moviSize>>24)&0xFF;
	// --------------------------------------------------------
	I420_Header[0x004] = (totalSize>> 0)&0xFF;
	I420_Header[0x005] = (totalSize>> 8)&0xFF;
	I420_Header[0x006] = (totalSize>>16)&0xFF;
	I420_Header[0x007] = (totalSize>>24)&0xFF;

	I420_Header[0x040] = (Width>> 0)&0xFF;
	I420_Header[0x041] = (Width>> 8)&0xFF;
	I420_Header[0x042] = (Width>>16)&0xFF;
	I420_Header[0x043] = (Width>>24)&0xFF;

	I420_Header[0x044] = (Height>> 0)&0xFF;
	I420_Header[0x045] = (Height>> 8)&0xFF;
	I420_Header[0x046] = (Height>>16)&0xFF;
	I420_Header[0x047] = (Height>>24)&0xFF;

	I420_Header[0x0B0] = (Width>> 0)&0xFF;
	I420_Header[0x0B1] = (Width>> 8)&0xFF;
	I420_Header[0x0B2] = (Width>>16)&0xFF;
	I420_Header[0x0B3] = (Width>>24)&0xFF;

	I420_Header[0x0B4] = (Height>> 0)&0xFF;
	I420_Header[0x0B5] = (Height>> 8)&0xFF;
	I420_Header[0x0B6] = (Height>>16)&0xFF;
	I420_Header[0x0B7] = (Height>>24)&0xFF;

	I420_Header[0x084] = (fps   >> 0)&0xFF;
	I420_Header[0x085] = (fps   >> 8)&0xFF;
	I420_Header[0x086] = (fps   >>16)&0xFF;
	I420_Header[0x087] = (fps   >>24)&0xFF;

	I420_Header[0x030] = (nFrame>> 0)&0xFF;
	I420_Header[0x031] = (nFrame>> 8)&0xFF;
	I420_Header[0x032] = (nFrame>>16)&0xFF;
	I420_Header[0x033] = (nFrame>>24)&0xFF;

	I420_Header[0x08C] = (nFrame>> 0)&0xFF;
	I420_Header[0x08D] = (nFrame>> 8)&0xFF;
	I420_Header[0x08E] = (nFrame>>16)&0xFF;
	I420_Header[0x08F] = (nFrame>>24)&0xFF;

	I420_Header[0x090] = (aviFrameSize>> 0)&0xFF;
	I420_Header[0x091] = (aviFrameSize>> 8)&0xFF;
	I420_Header[0x092] = (aviFrameSize>>16)&0xFF;
	I420_Header[0x093] = (aviFrameSize>>24)&0xFF;

	I420_Header[0x0F8] = (moviSize>> 0)&0xFF;
	I420_Header[0x0F9] = (moviSize>> 8)&0xFF;
	I420_Header[0x0FA] = (moviSize>>16)&0xFF;
	I420_Header[0x0FB] = (moviSize>>24)&0xFF;

	int written, readed;
	if( bPlane )
	{
	    written = fwrite( I420_Header, 1, HeaderSize, outfile );
	} else {
	    written = fwrite( UYVY_Header, 1, HeaderSize, outfile );
	}
	if( written<HeaderSize )
	{
	    fprintf( stdout, "Can't write (%d)\n", written );
	    exit( 1 );
	}
	unsigned char Outbuf[MAX_WIDTH*2];
	unsigned char *DataY=NULL;
	unsigned char *DataU=NULL;
	unsigned char *DataV=NULL;
	DataY = (unsigned char *)malloc( MAX_WIDTH*MAX_HEIGHT*2 );
	DataU = (unsigned char *)malloc( MAX_WIDTH*MAX_HEIGHT*2 );
	DataV = (unsigned char *)malloc( MAX_WIDTH*MAX_HEIGHT*2 );
	int f, x, y;
	if( bSGI )
	{
	    for( f=0; f<nFrame; f++ )
	    {
		fprintf( stdout, "(%6d)", f );
		fflush( stdout );
		readed = fread( DataY, 1, Width*Height*2, infile );
		readed = fread( DataV, 1, Width*Height*2, infile );
		readed = fread( DataU, 1, Width*Height*2, infile );
		if( readed<1 )
		{
		    fprintf( stdout, "Can't read (%d)\n", readed );
		    exit( 1 );
		}
		Outbuf[0] = '0';
		Outbuf[1] = '0';
		Outbuf[2] = 'd';
		Outbuf[3] = 'b';
		Outbuf[4] = (aviFrameSize>> 0)&0xFF;
		Outbuf[5] = (aviFrameSize>> 8)&0xFF;
		Outbuf[6] = (aviFrameSize>>16)&0xFF;
		Outbuf[7] = (aviFrameSize>>24)&0xFF;
		int headSize=8;
		written = fwrite( Outbuf, 1, headSize, outfile );
		for( y=0; y<Height; y++ )
		{
		    for( x=0; x<Width; x++ )
		    {
		    	int yy = Height-1-y;
			yy = y;
			switch( x&1 )
			{
			case 0 :
			    Outbuf[x*2+0] = DataU[(yy*Width+x)*2];
			    if( bMono )
			    Outbuf[x*2+0] = 0x80;
			    Outbuf[x*2+1] = DataY[(yy*Width+x)*2];
			    Outbuf[x*2+1] = DataV[(yy*Width+x)*2];
			    break;
			default :
			    Outbuf[x*2+0] = DataV[(yy*Width+x)*2];
			    if( bMono )
			    Outbuf[x*2+0] = 0x80;
			    Outbuf[x*2+1] = DataY[(yy*Width+x)*2];
			    Outbuf[x*2+1] = DataV[(yy*Width+x)*2];
			    break;
			}
		    }
		    written = fwrite( Outbuf, 1, Width*2, outfile );
		    if( written<Width*2 )
		    {
			fprintf( stdout, "Can't write (%d)\n", written );
			exit( 1 );
		    }
		    fflush( outfile );
		}
	    }
	} else {
	    if( nSkipFrame>0 )
	    {
		fprintf( stdout, "nSkipFrame=%d\n", nSkipFrame );
	    	for( f=0; f<nSkipFrame; f++ )
		{
		    fseek( infile, Width*Height*3/2, SEEK_CUR );
		}
	    }
	    for( f=0; f<nFrame; f++ )
	    {
		fprintf( stdout, "(%6d)", f );
		fflush( stdout );
		readed = fread( DataY, 1, Width*Height/1, infile );
		readed = fread( DataU, 1, Width*Height/4, infile );
		readed = fread( DataV, 1, Width*Height/4, infile );
		if( readed<1 )
		{
		    fprintf( stdout, "Can't read (%d)\n", readed );
		    exit( 1 );
		}
		Outbuf[0] = '0';
		Outbuf[1] = '0';
		Outbuf[2] = 'd';
		Outbuf[3] = 'b';
		Outbuf[4] = (aviFrameSize>> 0)&0xFF;
		Outbuf[5] = (aviFrameSize>> 8)&0xFF;
		Outbuf[6] = (aviFrameSize>>16)&0xFF;
		Outbuf[7] = (aviFrameSize>>24)&0xFF;
		int headSize=8;
		written = fwrite( Outbuf, 1, headSize, outfile );
		if( bPlane )
		{
		    readed = fwrite( DataY, 1, Width*Height/1, outfile );
		    readed = fwrite( DataU, 1, Width*Height/4, outfile );
		    readed = fwrite( DataV, 1, Width*Height/4, outfile );
		} else {
		    for( y=0; y<Height; y++ )
		    {
			for( x=0; x<Width; x++ )
			{
			    switch( x&1 )
			    {
			    case 0 :
				Outbuf[x*2+0] = DataU[(y/2*Width+x)/2];
				Outbuf[x*2+1] = DataY[(y*Width+x)/1];
				break;
			    default :
				Outbuf[x*2+0] = DataV[(y/2*Width+x)/2];
				Outbuf[x*2+1] = DataY[(y*Width+x)/1];
				break;
			    }
			}
			written = fwrite( Outbuf, 1, Width*2, outfile );
			if( written<Width*2 )
			{
			    fprintf( stdout, "Can't write (%d)\n", written );
			    exit( 1 );
			}
			fflush( outfile );
		    }
		}
	    }
	}
	// index
	fprintf( stdout, "Write index\n" );
	Outbuf[0] = 'i';
	Outbuf[1] = 'd';
	Outbuf[2] = 'x';
	Outbuf[3] = '1';
	Outbuf[4] = (aviIndexSize>> 0)&0xFF;
	Outbuf[5] = (aviIndexSize>> 8)&0xFF;
	Outbuf[6] = (aviIndexSize>>16)&0xFF;
	Outbuf[7] = (aviIndexSize>>24)&0xFF;
	written = fwrite( Outbuf, 1, 8, outfile );
	for( f=0; f<nFrame; f++ )
	{
	    fprintf( stdout, "(%6d)", f );
	    fflush( stdout );
	    int address = HeaderSize+(aviFrameSize+8)*f;
	    Outbuf[ 0] = '0';
	    Outbuf[ 1] = '0';
	    Outbuf[ 2] = 'd';
	    Outbuf[ 3] = 'b';
	    if( bPlane )
	    Outbuf[ 4] = 0x12;
	    else
	    Outbuf[ 4] = 0x10;
	    Outbuf[ 5] = 0x00;
	    Outbuf[ 6] = 0x00;
	    Outbuf[ 7] = 0x00;
	    Outbuf[ 8] = (address>> 0)&0xFF;
	    Outbuf[ 9] = (address>> 8)&0xFF;
	    Outbuf[10] = (address>>16)&0xFF;
	    Outbuf[11] = (address>>24)&0xFF;
	    Outbuf[12] = (aviFrameSize>> 0)&0xFF;
	    Outbuf[13] = (aviFrameSize>> 8)&0xFF;
	    Outbuf[14] = (aviFrameSize>>16)&0xFF;
	    Outbuf[15] = (aviFrameSize>>24)&0xFF;
	    written = fwrite( Outbuf, 1, 16, outfile );
	    if( written<16 )
	    {
		fprintf( stdout, "Can't write (%d)\n", written );
		exit( 1 );
	    }
	    fflush( outfile );
	}
	fprintf( stdout, "\nWrite Done\n" );
	fflush( stdout );

	free( DataY );
	free( DataU );
	free( DataV );

	fclose( infile );
	fclose( outfile );

	return 0;
}

