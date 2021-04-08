
/*
	aviParse.cpp
		2012.8.22  by T.Oda
		
		2012.8.29 : Support SPES output
		2012.10.12 : Merge main
		2012.10.12 : +J : MJPEG SPES output
		2012.10.12 : +M : MPEG4 SPES output
		2012.11.14 : handle videoScale/Rate, audioScale/Rate
		2013. 2.21 : +O : bSave=1
		2013. 3. 7 : over 4GB file (multi Riff)
		2013. 8. 6 : use MAX_BUF_SIZE
		2013. 9. 6 : bSave : 1/2/3/4/5/6/7/8
		        0 : no write
			1 : 0XXX.video
			2 : output.yuv packed
			3 : output.yuv planar 420
			4 : output.yuv planar 422
			5 : output.yuv planar 444
			6 : output.yuv planar 420 (Half)
			7 : output.yuv planar 422 (Half)
			8 : output.yuv planar 444 (Half)
		2013.10.9 : List() : use malloc for buffer
				     use MAX_BUF_SIZE_L
				     correct padding
 */

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>

#include "parseLib.h"
#include "spesLib.h"

#define MODE_VIDEO	1
#define MODE_JPEG	2

#define CODEC_MJPEG	1
#define CODEC_MPEG4	2

typedef unsigned int UINT;

extern int bDebug;

static int mode=MODE_VIDEO;
static int codec=CODEC_MJPEG;
static int bSPES=0;
static int bSave=0;

static int videoScale=(-1);
static int videoRate =(-1);
static int audioScale=(-1);
static int audioRate =(-1);

static int nRiff=0;

static int biWidth  = (-1);
static int biHeight = (-1);

#define MODE_NONE	0
#define MODE_RIFF	1
#define MODE_LIST	2

#define MAX_BUF_SIZE	1024*1024*2
#define MAX_BUF_SIZE_L	1024*1024*8

int Riff( char FourCC[], FILE *fp, int size );

int IxChunk( char FourCC[], FILE *fp, int size )
{
int total=0;
int readed;
unsigned char buffer[MAX_BUF_SIZE];
int Entry=(-1);
int IndexSubType=(-1);
int IndexType=(-1);
int EntriesInUse=(-1);
unsigned char ChunkId[16];
int OffsetL=(-1);
int OffsetH=(-1);
int Reserved=(-1);
int n;

	fprintf( stdout, "IxChunk(%s:0x%X)@0x%llX\n", 
		FourCC, (UINT)size, (g_addr-12) );
	{
	    readed = gread( buffer, 1, 2, fp ); total+=2;
	    Entry = Short_little( buffer );

	    readed = gread( buffer, 1, 2, fp ); total+=2;
	    IndexSubType = buffer[0];
	    IndexType    = buffer[1];

	    fprintf( stdout, "PerEntry     = %d\n", Entry );
	    fprintf( stdout, "IndexType    = %d\n", IndexType );
	    fprintf( stdout, "IndexSubType = %d\n", IndexSubType );

	    switch( IndexType )
	    {
	    case 0 :
		fprintf( stdout, "AVI Super Index Chunk\n" );
	    	if( IndexSubType==0 )
		{
		    fprintf( stdout, "Not implemented(%d)\n", IndexType );
		    EXIT();
		} else if( IndexSubType==1 )
		{
		    fprintf( stdout, "Not implemented(%d)\n", IndexType );
		    EXIT();
		} else {
		    fprintf( stdout, "Illegal IndexType(%d)\n", IndexType );
		    EXIT();
		}
		break;
	    case 1 :
	    	if( IndexSubType==0 )
		{
		    fprintf( stdout, "AVI Standard Index Chunk\n" );
		    readed = gread( buffer, 1, 4, fp ); total+=4;
		    EntriesInUse = Long_little( buffer );

		    memset( ChunkId, 0, 16 );
		    readed = gread( ChunkId, 1, 4, fp ); total+=4;

		    readed = gread( buffer, 1, 4, fp ); total+=4;
		    OffsetL = Long_little( buffer );

		    readed = gread( buffer, 1, 4, fp ); total+=4;
		    OffsetH = Long_little( buffer );

		    readed = gread( buffer, 1, 4, fp ); total+=4;
		    Reserved = Long_little( buffer );

		    fprintf( stdout, "[%s] %8X %8X\n",
		    	(char *)ChunkId, OffsetH, OffsetL );

		    for( n=0; n<EntriesInUse; n++ )
		    {
		    	int Offset, Size;
			readed = gread( buffer, 1, 4, fp ); total+=4;
			Offset = Long_little( buffer );

			readed = gread( buffer, 1, 4, fp ); total+=4;
			Size  = Long_little( buffer );
			fprintf( stdout, "%8X %8X\n", Offset, Size );
		    }
		} else if( IndexSubType==1 )
		{
		    fprintf( stdout, "AVI Field Index Chunk\n" );
		} else {
		    fprintf( stdout, "Illegal IndexType(%d)\n", IndexType );
		    EXIT();
		}
		break;
	    default :
	    	fprintf( stdout, "Unknown IndexType(%d)\n", IndexType );
		break;
	    }
	    fprintf( stdout, "IxChunk(%s:0x%X/0x%X)@0x%llX\n", 
	    	FourCC, (UINT)total, (UINT)size, g_addr);
	    while( total<size )
	    {
		readed = gread( buffer, 1, 4, fp ); total+=4;
	    }
	}
	fprintf( stdout, "IxChunk Done @0x%llX (total=0x%X)\n", 
		g_addr, total );
	return total;
}

static int nVideo=0;
//static int nAudio=0;

#define MODE_420	0
#define MODE_422	1
#define MODE_444	2

int Packed2Planar( unsigned char *InBuf, unsigned char *OutBuf,
	int width, int height, int mode )
{
int x, y;
int offset=0;
//int written=0;
	{
/*
	    readed = fread( InBuf, 1, 1920*1080*2, infile );
	    if( readed<1 )
	    {
	    	fprintf( stdout, "EOF\n" );
		break;
	    }
//	    fprintf( stdout, "readed=%d\n", readed );
	    total+=readed;
*/
	    // Y
	    for( y=0; y<height; y++ )
	    {
		for( x=0; x<width; x++ )
		{
		    OutBuf[y*width+x] = InBuf[y*width*2+x*2+1];
		}
	    }
	    offset = width*height;
//	    fprintf( stdout, "mode=%d, offset=%d\n", mode, offset );
	    // U
	    if( mode==MODE_420 )
	    {
		for( y=0; y<height; y+=2 )
		{
		    for( x=0; x<(width/2); x++ )
		    {
			OutBuf[offset+y/2*(width/2)+x] = InBuf[y*width*2+x*4+2];
		    }
		}
		offset += (width/2)*(height/2);
	    }
	    if( mode==MODE_422 )
	    {
		for( y=0; y<height; y++ )
		{
		    for( x=0; x<(width/2); x++ )
		    {
			OutBuf[offset+y*(width/2)+x] = InBuf[y*width*2+x*4+2];
		    }
		}
		offset += (width/2)*height;
	    }
	    if( mode==MODE_444 )
	    {
		for( y=0; y<height; y++ )
		{
		    for( x=0; x<(width/2); x++ )
		    {
			OutBuf[offset+y*width+x*2+0] 
				= InBuf[y*width*2+x*4+2];
			OutBuf[offset+y*width+x*2+1] 
				= InBuf[y*width*2+x*4+2];
		    }
		}
		offset += width*height;
	    }
//	    fprintf( stdout, "mode=%d, offset=%d\n", mode, offset );
	    // V
	    if( mode==MODE_420 )
	    {
		for( y=0; y<height; y+=2 )
		{
		    for( x=0; x<(width/2); x++ )
		    {
			OutBuf[offset+y/2*(width/2)+x] = InBuf[y*width*2+x*4+0];
		    }
		}
		offset += (width/2)*(height/2);
	    }
	    if( mode==MODE_422 )
	    {
		for( y=0; y<height; y++ )
		{
		    for( x=0; x<(width/2); x++ )
		    {
			OutBuf[offset+y*(width/2)+x] = InBuf[y*width*2+x*4+0];
		    }
		}
		offset += (width/2)*height;
	    }
	    if( mode==MODE_444 )
	    {
		for( y=0; y<height; y++ )
		{
		    for( x=0; x<(width/2); x++ )
		    {
			OutBuf[offset+y*width+x*2+0]
			    = InBuf[y*width*2+x*4+0];
			OutBuf[offset+y*width+x*2+1]
			    = InBuf[y*width*2+x*4+0];
		    }
		}
		offset += width*height;
	    }
//	    written = fwrite( OutBuf, 1, offset, outfile );
	}
	return offset;
}

int SaveFp( FILE *pWrite, FILE *fp, int size, int mode )
{
int readed, written;
int total=0;
//int rest=size;
unsigned char *buffer;
	buffer = (unsigned char *)malloc( size );
	if( buffer==NULL )
	{
	    fprintf( stdout, "Can't alloc %d bytes\n", size );
	    exit( 1 );
	}
//	fprintf( stdout, "Width=%d, Height=%d\n", biWidth, biHeight );
#if 1
	readed  = gread ( buffer, 1, size, fp );
	total += readed;
//	fprintf( stdout, "%d/%d\n", readed, size );
	if( pWrite )
	{
	    if( mode<3 )
	    {
		written = fwrite( buffer, 1, readed, pWrite );
		if( readed<size )
		{
		    fprintf( stdout, "readed=%d<%d\n", readed, size );
		    exit( 1 );
		}
		if( written<readed )
		{
		    fprintf( stdout, "written=%d<%d\n", written, readed );
		    exit( 1 );
		}
	    } else {
	    	int size = biWidth*biHeight*2;
		int yuvMode=0;
		switch( mode )
		{
		case 3 :
		    yuvMode=MODE_420;
		    size = biWidth*biHeight*3/2;
		    break;
		case 4 :
		    yuvMode=MODE_422;
		    size = biWidth*biHeight*4/2;
		    break;
		case 5 :
		    yuvMode=MODE_444;
		    size = biWidth*biHeight*6/2;
		    break;
		}
		unsigned char *OutBuf = (unsigned char *)malloc( size );
		if( OutBuf==NULL )
		{
		    fprintf( stdout, "Can't alloc %d bytes\n", size );
		    exit( 1 );
		}
		Packed2Planar( buffer, OutBuf, biWidth, biHeight, yuvMode );
		written = fwrite( OutBuf, 1, size, pWrite );
		free( OutBuf );
	    }
	}
#else
	while( rest>0 )
	{
	    int read = rest;
	    if( read>1024*4 )
	    	read = 1024*4;
	    readed = gread( buffer, 1, read, fp );
	    if( readed<1 )
	    {
		fprintf( stdout, "Can't read\n" );
		break;
	    }
	    if( pWrite )
	    {
		written = fwrite( buffer, 1, readed, pWrite );
		if( written<1 )
		{
		    fprintf( stdout, "Can't write\n" );
		    break;
		}
		total+=written;
	    } else {
		total+=readed;
	    }
	    rest-=readed;
	}
#endif
	free( buffer );
	return total;
}

int Save( char filename[], FILE *fp, int size, int bSave )
{
int total=0;
	FILE *pWrite = NULL;
	if( bSave )
	{
	    pWrite = fopen( filename, "wB" );
	    if( pWrite==NULL )
	    {
		fprintf( stdout, "Can't open %s\n", filename );
		return 0;
	    }
	    fprintf( stdout, "Save(%s:0x%X)\n", filename, size );
	}
	total = SaveFp( pWrite, fp, size, 0 );
	if( pWrite )
	    fclose( pWrite );
//	fprintf( stdout, "total=%X\n", total );
	return total;
}

int Index( FILE *fp, int size )
{
unsigned char buffer[1024];
char Name[16];
int total=0;
int nMode=(-1);
int nOffset=(-1);
int nSize=(-1);
int readed;

	fprintf( stdout, "Index(0x%X)\n", size );
	while( total<size )
	{
	    memset( Name, 0, 16 );
	    readed = gread( (unsigned char *)Name, 1, 4, fp );
	    if( readed<1 )
		return -1;
	    total+=readed;

	    readed = gread( buffer, 1, 4, fp );
	    if( readed<1 )
		return -1;
	    total+=readed;
	    nMode = Long_little(buffer);

	    readed = gread( buffer, 1, 4, fp );
	    if( readed<1 )
		return -1;
	    total+=readed;
	    nOffset = Long_little(buffer);

	    readed = gread( buffer, 1, 4, fp );
	    if( readed<1 )
		return -1;
	    total+=readed;
	    nSize = Long_little(buffer);

	    fprintf( stdout, "%s : (%2X) %8X, %8X\n",
	    	Name, nMode, nOffset, nSize );
	}
	fprintf( stdout, "Index Done(%X/%X)@0x%llX\n", 
		total, size, g_addr );
	return total;
}

int DumpText( unsigned char buffer[], int size2 )
{
	int i;
	int nc=0;
	for( i=0; i<size2; i++ )
	{
	    if( buffer[i]==0 )
	    {
		if( nc>0 )
		    fprintf( stdout, "\n" );
		nc=0;
	    } else {
		fprintf( stdout, "%c", buffer[i] );
		nc++;
	    }
	    if( buffer[i]>=0x80 )
		break;
	}
	return 0;
}

// ---------------------------------------------------------------------

int List( char fourCC[], FILE *fp, int size )
{
char nFourCC[8];
//unsigned char buffer[MAX_BUF_SIZE_L];
unsigned char *buffer;
int total=0;
int readed;
int sz;
int DTS=0, PTS=0;
int deltaFrame = 0;
int nFrame = 0;
FILE *pWrite = NULL;

	buffer = (unsigned char *)malloc( MAX_BUF_SIZE_L );
	if( buffer==NULL )
	{
	    fprintf( stdout, "Can't malloc@List\n" );
	    exit( 1 );
	}
	fprintf( stdout, "%02X,%02X,%02X,%02X\n",
		fourCC[0], fourCC[1],
		fourCC[2], fourCC[3] );
	fflush( stdout );
	fprintf( stdout, "List(%s:0x%X)@0x%llX\n", 
		fourCC, (UINT)size, (g_addr-12) );
	fflush( stdout );
	total=4;
	if( bSPES )
	{
	    pWrite = fopen( "output.spes", "wb" );
	    if( (videoScale>0) && (videoRate>0) )
	    {
	    	deltaFrame = 90000*videoScale/videoRate;
		fprintf( stdout, "deltaFrame=%d\n", deltaFrame );
	    }
	} else {
	    if( bSave>1 )
	    {
	    	if( nVideo==0 )
		{
		    pWrite = fopen( "output.yuv", "wb" );
		} else {
		    pWrite = fopen( "output.yuv", "ab" );
		}
		if( pWrite==NULL )
		{
		    fprintf( stdout, "Can't open output.yuv\n" );
		    exit( 1 );
		}
	    }
	}
	if( bSPES )
	{
	    switch( codec )
	    {
	    case CODEC_MJPEG :
		MJPEGH( pWrite );
		break;
	    case CODEC_MPEG4 :
		break;
	    default :
		break;
	    }
	}
	while( total<size )
	{
	    int size2;
	    fprintf( stdout, "%X/%X\n", total, size );
	    fflush( stdout );
	    memset( nFourCC, 0, 8 );
	    readed = gread( (unsigned char *)nFourCC, 1, 4, fp );
	    if( readed<4 )
	    {
		fprintf( stdout, "Can't read nFourCC@0x%llX\n", g_addr );
		break;
	    }
	    total+=4;
	    readed = gread( buffer, 1, 4, fp );
	    if( readed<4 )
	    {
		fprintf( stdout, "Can't read size@0x%llX\n", g_addr );
		break;
	    }
	    total+=4;
	    size2 = Long_little(buffer);
	    fprintf( stdout, "%8llX : [%s] Size=0x%X\n", 
	    	(g_addr-8), nFourCC, (UINT)size2 );
	    fflush( stdout );
//	    fprintf( stdout, "List : 0x%X/0x%X\n", total, size );
	    if( strncmp( nFourCC, "LIST", 4 )==0 )
	    {
		memset( nFourCC, 0, 8 );
		readed = gread( (unsigned char *)nFourCC, 1, 4, fp );
		if( readed<4 )
		{
		fprintf( stdout, "Can't read LIST(nFourCC)@0x%llX\n", 
			g_addr );
		    break;
		}
		total+=4;
		fprintf( stdout, "before List(%s,fp,%X)\n",
			nFourCC, size2 );
		    fflush( stdout );
	    	sz = List( nFourCC, fp, size2 );
		total+=sz;
	    } else if( strncmp( nFourCC, "ix", 2 )==0 )
	    {
	    	sz = IxChunk( nFourCC, fp, size2 );
		total+=sz;
	    } else if( strncmp( nFourCC, "idx1", 4 )==0 )
	    {
		fprintf( stdout, "idx1 Size=0x%X\n", size2 );
		sz = Index( fp, size2 );
		total+=sz;
		fprintf( stdout, "---------------\n" );
	    } else if( (strncmp( nFourCC, "00dc", 4 )==0)
		    || (strncmp( nFourCC, "01dc", 4 )==0) 
		    || (strncmp( nFourCC, "00db", 4 )==0) 
		    || (strncmp( nFourCC, "01db", 4 )==0) 
	    ) 
	    {	// Compressed Video track
	    	char filename[1024];
		if( bSPES )
		{
#if 1
		    switch( codec )
		    {
		    case CODEC_MJPEG :
			MJPEG( pWrite, fp, size2, DTS, PTS );
			break;
		    case CODEC_MPEG4 :
			MPEG4( pWrite, fp, size2, DTS, -1 );
			break;
		    default :
			break;
		    }
		    nFrame++;
		    if( (videoScale>0) && (videoRate>0) )
		    {
#if 0
			fprintf( stdout, "Scale=%d, Rate=%d\n", 
			    videoScale, videoRate );
#endif
		    	long long lDTS = 90000;
			lDTS = lDTS*nFrame;
			lDTS = lDTS*videoScale;
			lDTS = lDTS/videoRate;
		    	DTS = (int)lDTS;
		    	PTS = (int)lDTS;
		    }
#else
		    fseek( fp, size2, SEEK_CUR );
#endif
		    total += size2;
		} else {
		    if( mode==MODE_JPEG )
			sprintf( filename, "%04d.jpg", nVideo );
		    else
			sprintf( filename, "%04d.video", nVideo );
		    switch( bSave )
		    {
		    case 0 :	// no write
		    case 1 :	// Separate file
		    case 2 :	// packed
			total += Save( filename, fp, size2, bSave );
			break;
		    case 3 :	// All frame	420/422/444
		    case 4 :	// All frame	420/422/444
		    case 5 :	// All frame	420/422/444
			total += SaveFp( pWrite, fp, size2, bSave );
			break;
		    case 6 :	// Half frame (60P->30P) 420/422/444
		    case 7 :	// Half frame (60P->30P) 420/422/444
		    case 8 :	// Half frame (60P->30P) 420/422/444
		    	{
			FILE *pW = NULL;
			if( (nVideo & 1)==0 )
			    pW = pWrite;
			total += SaveFp( pW, fp, size2, bSave-3 );
			}
			break;
		    case  9 :	// Half frame (60P->30I) 420/422/444
		    case 10 :	// Half frame (60P->30I) 420/422/444
		    case 11 :	// Half frame (60P->30I) 420/422/444
			break;
		    }
//		    fprintf( stdout, "total=%X\n", total );
		    nFrame++;
		}
	    	nVideo++;
#if 0
		if( (size2%2)>0 )
		{
		    int rr = 2-(size2%2);
//		    if( bDebug )
			fprintf( stdout, "Padding(%d)\n", rr );
		    readed = gread( buffer, 1, rr, fp );
		    if( readed<1 )
		    {
		    	free( buffer );
			return -1;
		    }
		    total+=readed;
		}
#endif
	    } else if( (strncmp( nFourCC, "00wb", 4 )==0)
		    || (strncmp( nFourCC, "01wb", 4 )==0) )
	    {	// Audio
		    total += Save( (char *)"audio.bin", fp, size2, 0 );
	    } else {
		if( size2>MAX_BUF_SIZE_L )
		{
		    fprintf( stdout, "List : Too large size(0x%X)\n", size2 );
		    EXIT();
		}
		if( size2>0 )
		{
		    readed = gread( buffer, 1, size2, fp );
		    fprintf( stdout, "List else : readed=%d\n", readed );
		    if( readed<0 )
		    {
		    	free( buffer );
			return -1;
		    }
		    total+=readed;

		    if( strncmp( nFourCC, "dmlh", 4 )==0 )
		    {
			unsigned long TotalFrames = Long_little( buffer );
			fprintf( stdout, "-------------------------\n" );
			fprintf( stdout, "TotalFrames = %d\n",(int)TotalFrames);
			fprintf( stdout, "-------------------------\n" );
		    } else if( strncmp( nFourCC, "strh", 4 )==0 )
		    {
			char fccType[8];
			char fccHandler[8];
			int Scale;
			int Rate;
			int Start;
			int Length;
			memset( fccType, 0, 8 );
			memset( fccHandler, 0, 8 );
			memcpy( fccType   , &buffer[0], 4 );
			memcpy( fccHandler, &buffer[4], 4 );
			Scale = Long_little( &buffer[20] );
			Rate  = Long_little( &buffer[24] );
			Start = Long_little( &buffer[28] );
			Length= Long_little( &buffer[32] );
			    
			fprintf( stdout, "-------------------------\n" );
			fprintf( stdout, "fccType   =[%s]\n", fccType );
			if( fccHandler[0]>=' ' )
			{
			    fprintf( stdout, "fccHandler=[%s]\n", fccHandler );
			    if( strncmp( fccType, "vids", 4 )==0 )
			    {
				if( strncmp( fccHandler, "mjpg", 4 )==0 )
				    mode=MODE_JPEG;
				if( strncmp( fccHandler, "MJPG", 4 )==0 )
				    mode=MODE_JPEG;
			    }
			}
			fprintf( stdout, "Scale     =%d\n", Scale );
			fprintf( stdout, "Rate      =%d\n", Rate  );
			fprintf( stdout, "Start     =%d\n", Start );
			fprintf( stdout, "Length    =%d\n", Length );
			fprintf( stdout, "-------------------------\n" );
			if( strncmp(fccType,"vids",4)==0 )
			{
			    videoScale = Scale;
			    videoRate  = Rate ;
			}
			if( strncmp(fccType,"auds",4)==0 )
			{
			    audioScale = Scale;
			    audioRate  = Rate ;
			}
		    } else if( strncmp( nFourCC, "strf", 4 )==0 )
		    {
			if( size2==40 )	// BITMAPINFOHEADER
			{
			    char Compress[8];
			    memset( Compress, 0, 8 );
    //			int biSize        = Long_little ( &buffer[ 0] );
			        biWidth       = Long_little ( &buffer[ 4] );
			        biHeight      = Long_little ( &buffer[ 8] );
			    int biPlanes      = Short_little( &buffer[12] );
			    int biBitCount    = Short_little( &buffer[14] );
			    memcpy( Compress, &buffer[16], 4 );
			    int biSizeImage   = Long_little ( &buffer[20] );
			    fprintf( stdout, "-------------------------\n" );
			    fprintf( stdout, "Width    = %d\n", biWidth );
			    fprintf( stdout, "Height   = %d\n", biHeight);
			    fprintf( stdout, "Planes   = %d\n", biPlanes );
			    fprintf( stdout, "BitCount = %d\n", biBitCount );
			    fprintf( stdout, "Compress = %s\n", Compress );
			    fprintf( stdout, "SizeImage= %d\n", biSizeImage );
			    fprintf( stdout, "-------------------------\n" );
				if( strncmp( Compress, "MJPG", 4 )==0 )
				    mode=MODE_JPEG;
			} else if( size2==18 )	// WAVEFORMATEX
			{
			int wFormatTag      = Short_little( &buffer[ 0] );
			int nChannels       = Short_little( &buffer[ 2] );
			int nSamplesPerSec  = Long_little ( &buffer[ 4] );
			int nAvgBytesPerSec = Long_little ( &buffer[ 8] );
			int nBlockAlign     = Short_little( &buffer[12] );
			int wBitsPerSample  = Short_little( &buffer[14] );
			int cbSize          = Short_little( &buffer[16] );
			fprintf(stdout,"-------------------------\n" );
			fprintf(stdout,"wFormatTag     = %d\n",wFormatTag );
			fprintf(stdout,"nChannels      = %d\n",nChannels  );
			fprintf(stdout,"nSamplesPerSec = %d\n",nSamplesPerSec);
			fprintf(stdout,"nAvgBytesPerSec= %d\n",nAvgBytesPerSec);
			fprintf(stdout,"nBlockAlign    = %d\n",nBlockAlign);
			fprintf(stdout,"wBitsPerSample = %d\n",wBitsPerSample);
			fprintf(stdout,"cbSize         = %d\n", cbSize );
			fprintf(stdout,"-------------------------\n" );
			} else {
			}
		    } else if( strncmp( nFourCC, "strd", 4 )==0 )
		    {
			fprintf( stdout, "-------------------------\n" );
			DumpText( buffer, size2 );
			fprintf( stdout, "-------------------------\n" );
		    } else if( strncmp( nFourCC, "IDIT", 4 )==0 )
		    {
			fprintf( stdout, "-------------------------\n" );
			DumpText( buffer, size2 );
			fprintf( stdout, "-------------------------\n" );
		    } else if( strncmp( nFourCC, "ISFT", 4 )==0 )
		    {
			fprintf( stdout, "-------------------------\n" );
			DumpText( buffer, size2 );
			fprintf( stdout, "-------------------------\n" );
		    } else if( strncmp( nFourCC, "ICRD", 4 )==0 )
		    {
			fprintf( stdout, "-------------------------\n" );
			DumpText( buffer, size2 );
			fprintf( stdout, "-------------------------\n" );
		    } else if( strncmp( nFourCC, "avih", 4 )==0 )
		    {
			fprintf( stdout, "-------------------------\n" );
			int frameDuration = Long_little ( &buffer[ 0] );
			int dwTotalFrames = Long_little ( &buffer[16] );
			int StartFrame    = Long_little ( &buffer[20] );
			int nStreams      = Long_little ( &buffer[24] );
			int nBufferSize   = Long_little ( &buffer[28] );
			int nWidth        = Long_little ( &buffer[32] );
			int nHeight       = Long_little ( &buffer[36] );
			fprintf( stdout, "frameDuration = %d\n", frameDuration);
			fprintf( stdout, "dwTotalFrames = %d\n", dwTotalFrames);
			fprintf( stdout, "StartFrame    = %d\n", StartFrame   );
			fprintf( stdout, "nStreams      = %d\n", nStreams     );
			fprintf( stdout, "BufferSize    = %d\n", nBufferSize  );
			fprintf( stdout, "Width         = %d\n", nWidth );
			fprintf( stdout, "Height        = %d\n", nHeight );
			fprintf( stdout, "-------------------------\n" );
		    } else if( strncmp( nFourCC, "indx", 4 )==0 )
		    {
			fprintf( stdout, "-------------------------\n" );
    //		    DumpText( buffer, size2 );
			fprintf( stdout, "-------------------------\n" );
		    } else {
			fprintf( stdout, "-------------------------\n" );
			fprintf( stdout, "else[%s]\n", nFourCC );
			fprintf( stdout, "-------------------------\n" );
		    }
#if 0
		    if( (size2%2)>0 )
		    {
			int rr = 2-(size2%2);
//			if( bDebug )
			    fprintf( stdout, "Padding(%d)\n", rr );
			readed = gread( buffer, 1, rr, fp );
			if( readed<1 )
			{
			    free( buffer );
			    return -1;
			}
			total+=readed;
		    }
#endif
		}
	    }
	    if( (size2%2)>0 )
	    {
		int rr = 2-(size2%2);
		    fprintf( stdout, "Padding(%d)\n", rr );
		readed = gread( buffer, 1, rr, fp );
		if( readed<1 )
		{
		    free( buffer );
		    return -1;
		}
		total+=readed;
	    }
	    if( bDebug )
	    {
		fprintf( stdout, "List(%s) 0x%X/0x%X\n", 
			fourCC, total, size );
	    }
	}
	if( pWrite )
	    fclose( pWrite );
	fprintf( stdout, "List(%s) Done(0x%X/0x%X)@0x%llX\n", 
		fourCC, (UINT)total, (UINT)size, g_addr );
	free( buffer );
	return total-4;
}

int NoParse( char *name, FILE *fp, int size )
{
unsigned char buffer[1024];
int total=0;
int readed;

	fprintf( stdout, "%s(0x%X)\n", name, size );
	while( total<size )
	{
	    int read = size-total;
	    if( read>1024 )
	    	read=1024;
	    readed = gread( buffer, 1, read, fp );
//	    fprintf( stdout, "readed=%d\n", readed );
	    if( readed<1 )
		return -1;
	    total+=readed;
	}
	fprintf( stdout, "%s Done@0x%llX\n", name, g_addr );
	return total;
}


int Riff( char FourCC[], FILE *fp, int size )
{
char Name[16];
int readed, sz;
unsigned char buffer[1024*1024];
int total=0;
int nSize;

	nRiff++;
	fprintf( stdout, "Riff(%s:0x%X) : %d\n", FourCC, size, nRiff );
	total=4;
	while( total<size )
	{
	    memset( Name, 0, 8 );
	    readed = gread( (unsigned char *)Name, 1, 4, fp );
	    if( readed<1 )
		break;
	    total+=readed;

	    readed = gread( buffer, 1, 4, fp );
	    if( readed<1 )
		break;
	    nSize = Long_little(buffer);
	    total+=readed;

	    if( bDebug )
	    {
		fprintf( stdout, "Name=[%s]\n", Name );
		fprintf( stdout, "nSize=0x%X\n", nSize );
		fflush( stdout );
	    }

	    if( strncmp( Name, "LIST", 4 )==0 )
	    {
		char nFourCC[16];
		fprintf( stdout, "LIST Size=0x%X\n", nSize );
		memset( nFourCC, 0, 16 );
		readed = gread( (unsigned char *)nFourCC, 1, 4, fp );
		if( readed<1 )
		    break;
		total+=readed;
		fprintf( stdout, "FourCC[%s]\n", nFourCC );
		sz = List( nFourCC, fp, nSize );
//		total+=(sz+4);
		total+=sz;
	    } else if( strncmp( Name, "idx1", 4 )==0 )
	    {
		fprintf( stdout, "idx1 Size=0x%X\n", nSize );
		sz = Index( fp, nSize );
		total+=sz;
	    } else if( strncmp( Name, "JUNK", 4 )==0 )
	    {
	    	fprintf( stdout, "JUNK Size=0x%X\n", nSize );
		fflush( stdout );
		sz = NoParse( (char *)"JUNK", fp, nSize );
		total+=sz;
	    } else if( strncmp( Name, "IDIT", 4 )==0 )
	    {
	    	fprintf( stdout, "IDIT Size=0x%X\n", nSize );
		fflush( stdout );
		sz = NoParse( (char *)"IDIT", fp, nSize );
		total+=sz;
	    } else if( strncmp( Name, "PrmA", 4 )==0 )	// Edius output
	    {
	    	fprintf( stdout, "%s Size=0x%X\n", Name, nSize );
		fflush( stdout );
		sz = NoParse( Name, fp, nSize );
		total+=sz;
	    } else if( strncmp( Name, "CtsI", 4 )==0 )	// Edius output
	    {
	    	fprintf( stdout, "%s Size=0x%X\n", Name, nSize );
		fflush( stdout );
		sz = NoParse( Name, fp, nSize );
		total+=sz;
/*
	    } else if( strncmp( Name, "avih", 4 )==0 )
	    {
	    	fprintf( stdout, "avih Size=0x%X\n", nSize );
		fflush( stdout );
		sz = NoParse( (char *)"avih", fp, nSize );
		total+=sz;
*/
	    } else {
		fprintf( stdout, "Unknown[%s] @0x%llX\n", Name, g_addr );
		fflush( stdout );
		break;
	    }
	    fprintf( stdout, "Riff(%s) 0x%X/0x%X\n", FourCC, total, size );
	    fflush( stdout );
	}
	fprintf( stdout, "Riff Done(%X/%X)\n", total, size );
	fprintf( stdout, "-------------------------------------------------\n");
	return 0;
}

// -------------------------------------------

void aviUsage( char *prg )
{
	fprintf( stdout, "%s filename\n", prg );
	exit( 1 );
}

int avi( int argc, char *argv[] )
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
		    case 'O' :
		    	bSave = atoi( &argv[i][2] );
			if( bSave<1 )
			    bSave = 1;
			break;
		    case 'S' :
		    	bSPES = 1;
		    	break;
		    case 'J' :
		    	bSPES = 1;
			codec = CODEC_MJPEG;
		    	break;
		    case 'M' :
		    	bSPES = 1;
			codec = CODEC_MPEG4;
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
	    aviUsage( argv[0] );
	}

	FILE *fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open [%s]\n", filename );
	    exit( 1 );
	}
	char Name[16];
	while( 1 )
	{
	    memset( Name, 0, 16 );
	    readed = gread( (unsigned char *)Name, 1, 4, fp );
//	    fprintf( stdout, "readed=%d\n", readed );
	    if( readed<1 )
	    {
	    	fprintf( stdout, "EOF\n" );
		exit( 0 );
	    }

	    memset( buffer, 0, 16 );
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
		for( i=0; i<4; i++ )
		{
		    fprintf( stdout, "%02X ", Name[i] );
		}
		for( i=0; i<4; i++ )
		{
		    fprintf( stdout, "%02X ", buffer[i] );
		}
		fprintf( stdout, ": [%s]\n", buffer );
		EXIT();
	    }
	}
	fclose( fp );
	return 0;
}

