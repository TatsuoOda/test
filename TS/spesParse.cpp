
/*
    spesparse.cpp
	    2011.11.7	-R option : output SPES (out.bin)
	    2012.1.11	-M option : MODE_MERGE
	    2012.2.15	Usage update ()
	    2012.7.12	ignore SPES_Size<=16

	    2012.8.16	TcTime24() check error and return trunked val
	    2012.8.16   Check fout is NULL or not
	    2012.9.17   spesMain
	    2012.9.21   +P : add PES_Header
	                -----------------------
	    		+e : SPES_OUT_ES264
			+R : SPES_OUT_SPES
	                -----------------------
	    		=E : SPES_MODE_EDIT
			=M : SPES_MODE_MERGE
			=T : SPES_MODE_DTS_EDIT
	    2012.9.27   expand code2 field
	    2012.10.3   +E : bAddEOS (for AVC)
	    		+A : bAddAUD (for AVC)
	    2012.10.12  +D : bDTSPTS
	    2012.10.15  +C : 91->DH
	    2012.10.16  +N : bDumpNAL=1
	    		-R : bRemovePrefixNAL=1
	    2012.10.18  +R : bAddPrefixNAL=1
	    	        +S : SPES_OUT_SPES
	    2012.10.19  Use memmove
	    		-A : bRemoveAUD
			-E : bRemoveSliceExtention
			+I : bDumpSEI
	    2012.11.08  Support RealVideo
	    2012.11.29  Support NAL_size over 255
	    2012.12.07  AVC_SliceHeader
	    2012.12.13  AVC_SliceHeader : show PTS
	    2013.2.5    VP8
	    2013.2.5    =P : deltaPTS
	    2013.2.27   Add Real special code
	    2013.3.6    =F : bSaveFrame
	    		VC1(main) & OUT_SPES -> Separate output
	    2013.5. 9	-S : bRemoveSEI
	    		=S : Seek
	    2013.5.10   -R : nRemoveNAL=XX
	    		-p : bRemovePrefixNAL=1
			=e : bEdit=1
			+e =e : Output SPES (size rounded)
	    2013.6.28	nSkip case codec?
			    add VP8/Real/MJPEG
			    test Real OK
			+s : bShowSkip
	    2013.7.31   -s : nRemoveSEI
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>	// memcpy

#include "lib.h"	// GetValue
#include "tsLib.h"
#include "parseLib.h"
#include "spesParse.h"

//#define MAX_BUF_SIZE 1024*1024*2
#define MAX_BUF_SIZE 1024*1024*32

/*
#define MODE_READ	0
#define MODE_EDIT	1
#define MODE_DTS_EDIT	2
#define MODE_MERGE	3
*/

#define CODEC_NONE	0
#define	CODEC_AVC	1
#define CODEC_VC1	2
#define CODEC_MP4	3

/*
#define OUT_ES		0
#define OUT_ES264	1
#define OUT_SPES	2
typedef unsigned int UINT;
*/


// ------------------------------------

#define MAX_FILE 16

static char outFilename[1024];
static int bPES = 0;
static int deltaPTS=0;

static char Start0[4] = { 0x00, 0x00, 0x00, 0x01 };
static char Start [3] = { 0x00, 0x00, 0x01 };

int Title( char *name, unsigned long total )
{
fprintf( stdout, "%s SPES (@0x%X)\n", name, (UINT)total );
fprintf( stdout, "index :  Address :  Size :Cd:---- -   DTS   ,-   PTS   \n" );
    return 0;
}

// ------------------------------------
static int bSPS=0;
static int bAddEOS=0;
static int bAddAUD=0;
static int bDTSPTS=0;

static int bDumpNAL=0;
static int bDumpSEI=0;

static int bRemovePrefixNAL=0;
static int bAddPrefixNAL=0;
static int bRemoveAUD=0;
static int bRemoveSliceExtension=0;
static int bRemoveSEI=0;
static int nRemoveSEI=(-1);
static int nRemoveNAL=(-1);
static int bEdit=0;

static int bSaveFrame=0;
static int bShowSkip=0;

int SPES_Header_PES_Header( unsigned char *buf, FILE *fp )
{
int bPTS, bDTS;
int PTS32=0;
int DTS32=0;
unsigned int PTS=0;
unsigned int DTS=0;
unsigned char obuf[32];
int offset=0;
	bPTS  = (buf[ 5]&0x80) ? 1 : 0;
	bDTS  = (buf[ 5]&0x40) ? 1 : 0;
	PTS32 = (buf[ 5]&0x20) ? 1 : 0;
	DTS32 = (buf[ 5]&0x10) ? 1 : 0;
	PTS   = (buf[ 8]<<24)
	      | (buf[ 9]<<16)
	      | (buf[10]<< 8)
	      | (buf[11]<< 0);
	DTS   = (buf[12]<<24)
	      | (buf[13]<<16)
	      | (buf[14]<< 8)
	      | (buf[15]<< 0);

	obuf[0] = 0x00;
	obuf[1] = 0x00;
	obuf[2] = 0x01;
	obuf[3] = 0xE0;
	obuf[4] = 0x00;
	obuf[5] = 0x00;
	obuf[6] = 0x80;
	obuf[7] = 0x00;
	if( bPTS )
	    obuf[7] |= 0x80;
	if( bDTS )
	    obuf[7] |= 0x40;
	if( bDTS && bPTS )
	    obuf[8] = 10;
	else if( bDTS || bPTS )
	    obuf[8] =  5;
	else
	    obuf[8] =  0;
	offset = 9;
	if( bPTS )
	{
	    obuf[offset+0] = (PTS32<<3) | ((PTS>>30)<<1);
	    obuf[offset+1] = (PTS>>22);
	    obuf[offset+2] = (PTS>>15)<<1;
	    obuf[offset+3] = (PTS>> 7)<<0;
	    obuf[offset+4] = (PTS    )<<1;
	    offset += 5;
	}
	if( bDTS )
	{
	    obuf[offset+0] = (DTS32<<3) | ((DTS>>30)<<1);
	    obuf[offset+1] = (DTS>>22);
	    obuf[offset+2] = (DTS>>15)<<1;
	    obuf[offset+3] = (DTS>> 7)<<0;
	    obuf[offset+4] = (DTS    )<<1;
	    offset += 5;
	}
	int written = fwrite( obuf, 1, offset, fp );	// Add 00
	if( written<offset )
	    fprintf( stdout, "Can't write %d/%d\n",
	    	written, offset );

	return 0;
}

static int avcAdded=0;
static int avcSize=0;
static int avcSum1=0;
static int avcSum2=0;

int AVC_ES( unsigned char *buf, int size, FILE *fp, int total )
{
int sz = 0;
int es_total=0;
int err=0;
int written;

	while( es_total<size )
	{
	    sz = (buf[es_total+0]<<24)
	       | (buf[es_total+1]<<16)
	       | (buf[es_total+2]<< 8)
	       | (buf[es_total+3]<< 0);
	    es_total+=4;
#if 0
	// for youtube premium
	    if( sz==1 )
	    {
	    	sz = size-es_total;
		buf[es_total-4] = (sz>>24)&0xFF;
		buf[es_total-3] = (sz>>16)&0xFF;
		buf[es_total-2] = (sz>> 8)&0xFF;
		buf[es_total-1] = (sz>> 0)&0xFF;
	    }
#endif
//	    fprintf( stdout, "total+sz=%d, size=%d\n", es_total+sz, size );
	    if( ((es_total+sz)>size) || (sz<0) )
	    {
		fprintf( stdout, 
			"sz rounded 0x%08X -> 0x%X @0x%X\n", 
			sz, size-es_total, total );
	    	sz = size-es_total;
		buf[es_total-4] = (sz>>24)&0xFF;
		buf[es_total-3] = (sz>>16)&0xFF;
		buf[es_total-2] = (sz>> 8)&0xFF;
		buf[es_total-1] = (sz>> 0)&0xFF;
		err++;
	    }
//	    if(  (buf[es_total]==0x06)
	    if(  
	         (buf[es_total]==0x65)
//	     || ((buf[es_total]&0x1F)==0x14)
//	     || ((buf[es_total]&0x1F)==0x01)
	     )
	    {
		written = fwrite( Start, 1, 1, fp );	// Add 00
		avcAdded+=1;
	    }
	    written = fwrite( Start, 1, 3, fp );
	    avcAdded+=3;
	    fprintf( stdout, "AVC_ES : %8X : %4X\n", es_total-4, sz );
	    written = fwrite( &buf[es_total], 1, sz, fp );
	    avcSize+=sz;

	    es_total+=written;
	}
	avcSum1+=es_total;
	avcSum2+=size;
	if( avcSum1!=avcSum2 )
	{
	    fprintf( stdout, "total=%d, size=%d\n", es_total, size );
	    fprintf( stdout, "avcSum1 =%d bytes\n", avcSum1 );
	    fprintf( stdout, "avcSum2 =%d bytes\n", avcSum2 );
	    exit( 1 );
	}
	return err;
}

static int ShowHeader( int total, unsigned char *bufA )
{
    fprintf( stdout, "%8X : %02X %02X %02X %02X : %02X\n",
	    total,
	    bufA[0], bufA[1], bufA[2], bufA[3], 
	    bufA[4] );

    return 0;
}

unsigned int lastDTS=0xFFFFFFFF;

int ShowSPES( unsigned char bufA[], 
	unsigned char bufB[],
	FILE *fpB,
	int nPES, int bNG,
	int code, int code2, int SPES_Size, int total,
	int *bDTS, unsigned int *DTS,
	int *bPTS, unsigned int *PTS )
{
int PTS32=0;
int DTS32=0;
int PTS32_B=0;
int DTS32_B=0;
unsigned int PTS_B=0;
unsigned int DTS_B=0;
	*bPTS = (bufA[ 5]&0x80) ? 1 : 0;
	*bDTS = (bufA[ 5]&0x40) ? 1 : 0;
	PTS32 = (bufA[ 5]&0x20) ? 1 : 0;
	DTS32 = (bufA[ 5]&0x10) ? 1 : 0;
	*PTS  = (bufA[ 8]<<24)
	      | (bufA[ 9]<<16)
	      | (bufA[10]<< 8)
	      | (bufA[11]<< 0);
	*DTS  = (bufA[12]<<24)
	      | (bufA[13]<<16)
	      | (bufA[14]<< 8)
	      | (bufA[15]<< 0);
	if( fpB )
	{
	PTS32_B = (bufB[ 5]&0x20) ? 1 : 0;
	DTS32_B = (bufB[ 5]&0x10) ? 1 : 0;
	PTS_B = (bufB[ 8]<<24)
	      | (bufB[ 9]<<16)
	      | (bufB[10]<< 8)
	      | (bufB[11]<< 0);
	DTS_B = (bufB[12]<<24)
	      | (bufB[13]<<16)
	      | (bufB[14]<< 8)
	      | (bufB[15]<< 0);
	}

	switch( code )
	{
	case 'V' :	// Real Header
	    *bPTS = 0;
	    *bDTS = 0;
	    fprintf( stdout, "%5d :A=%8X:S=%6X:%02X:%4X ",
		nPES, total-SPES_Size, SPES_Size, code, code2 );
	    break;
	case 0xA1 :	// Real EOS
	    fprintf( stdout, "%5d :A=%8X:S=%6X:%02X:EOS  ",
		nPES, total-SPES_Size, SPES_Size, code );
	    break;
	case 0x50 :	// XVID
	case 0x31 :	// AVC
	case 0x90 :	// Motion JPEG
	    fprintf( stdout, "%5d :A=%8X:S=%6X:%02X:%4X ",
		nPES, total-SPES_Size, SPES_Size, code, code2 );
	    break;
	default :
	    fprintf( stdout, "%5d :A=%8X:S=%6X:%02X:     ",
		nPES, total-SPES_Size, SPES_Size, code );
	    break;
	}
	if( *bDTS )
	    fprintf( stdout, "%1X %08X,", DTS32,(UINT)*DTS );
	else
	    fprintf( stdout, "- --------," );

	if( *bPTS )
	    fprintf( stdout, "%1X %08X", PTS32, (UINT)*PTS );
	else
	    fprintf( stdout, "- --------" );

	if( fpB )
	{
	fprintf( stdout, " | " );
	if( *bDTS )
	    fprintf( stdout, "%1X %08X,", DTS32_B, (UINT)DTS_B );
	else
	    fprintf( stdout, "- --------," );

	if( *bPTS )
	    fprintf( stdout, "%1X %08X", PTS32_B, (UINT)PTS_B );
	else
	    fprintf( stdout, "- --------" );
	}
	if( fpB )
	{
	    if( bNG==0 )
		fprintf( stdout, " O" );
	    else
		fprintf( stdout, " X" );
	}
	int bDiff=0;
	if( *bDTS )
	{
	    if( lastDTS!=0xFFFFFFFF )
	    {
		if( bDTSPTS==0 )
		{
		    fprintf( stdout, "%7d", *DTS-lastDTS );
		    bDiff++;
		}
	    }
	    lastDTS = *DTS;
	}
//		    if( bShowTime )
	if( *bPTS )
	{
	    if( bDTSPTS )
	    if( *bDTS )
	    {
		fprintf( stdout, " %7d", *PTS-*DTS );
		bDiff++;
	    }
	    int hour, min, sec, msec;
	    TcTime32( PTS32, *PTS, &hour, &min, &sec, &msec );
	    if( bDiff==0 )
		fprintf( stdout, "       " );
	    fprintf( stdout, " P=%2d:%02d:%02d:%03d",
		    hour, min, sec, msec );
	} else if( *bDTS )
	{
	    int hour, min, sec, msec;
	    TcTime32( DTS32, *DTS, &hour, &min, &sec, &msec );
	    if( bDiff==0 )
		fprintf( stdout, "        " );
	    fprintf( stdout, " D=%2d:%02d:%02d:%03d",
		    hour, min, sec, msec );
	}
	fprintf( stdout, "\n" );
	return 0;
}

#define MAX_BASE	30

//int bDebug=0;
#if 0
int BitAddr=0;
int BitSkip=0;
int nZero=0;
//int bDebugGolB=0;
//int bShowGolomb=0;

static int base[MAX_BASE];
#define BIT_BUFFER_SIZE 1024
static unsigned char BitBuffer[BIT_BUFFER_SIZE];
int BitReaded=0;
int BitOffset=0;

void InitBit( )
{
//	fprintf( stdout, "InitBitStream(0x%llX)\n", g_addr );
//	BitAddr  = g_addr;
	BitOffset= 0;
	BitReaded= 0;
	BitSkip  = 0;
	nZero    = 0;
}
int GetBit( unsigned char *buffer, int *pBufferPos, int nBit )
{
static unsigned char BitMask[] = {
0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01
};
	if( bDebug )
	fprintf( stdout, "GetBit(%d)\n", nBit );
	int buffer_pos = *pBufferPos;
	int nPos = BitOffset/8;
	if( (BitReaded/8)>=BIT_BUFFER_SIZE )
	{
	    fprintf( stdout, "BitBuffer full (%d)\n", BitReaded );
	    exit( 1 );
	}
	if( (BitOffset+nBit)>BitReaded )
	{	// ¤¿¤ê¤Ê¤¤¤Î¤Ç¤è¤à
	    int ii;
	    int nByte = BitOffset+nBit-BitReaded;
	    nByte = (nByte+7)/8;
	    if( bDebug )
	    fprintf( stdout, "Requested %d bytes (%d)\n", 
	    	nByte, BitReaded );
	    for( ii=0; ii<nByte; ii++ )
	    {
		BitBuffer[BitReaded/8] = buffer[buffer_pos++];
	    }
	    for( ii=0; ii<nByte; ii++ )
	    {
#if 0
		fprintf( stdout, "%8llX,nZero=%d [%02X]\n", 
			g_addr, nZero, BitBuffer[BitReaded/8+ii] );
#endif
	    	if( nZero==2 )
		{
		    if( BitBuffer[BitReaded/8+ii]==3 )
		    {
		    	// 00 00 03
			if( bDebug )
			    fprintf( stdout, "Found(00 00 03)@0x%X+%d/8\n",
				BitAddr, BitOffset );
#if 0
int iii;
for( iii=0;iii<nByte;iii++ )
{
    fprintf( stdout, "%02X ", BitBuffer[BitReaded/8+iii] );
}
fprintf( stdout, "\n" );
#endif
			BitSkip++;
			int jj;
			for( jj=ii; jj<nByte; jj++ )
			{
			    BitBuffer[BitReaded/8+jj] 
			    	= BitBuffer[BitReaded/8+jj+1];
			}
			BitBuffer[BitReaded/8+nByte-1] = buffer[buffer_pos++];
			nZero=0;
		    }
		}
	    	if( BitBuffer[BitReaded/8+ii] == 0 )
		    nZero++;
		else
		    nZero=0;
	    }
	    if( bDebug )
	    {
	    for( ii=0; ii<nByte; ii++ )
	    {
	    	fprintf( stdout, "%02X ", BitBuffer[BitReaded/8+ii] );
	    }
	    fprintf( stdout, "\n" );
	    }
	    BitReaded += nByte*8;
	}
	unsigned int data=0;
	int rBit = nBit+BitOffset-nPos*8;
	int rP = 0;
	int data8 = (-1);
	while( rBit>0 )
	{
	    data = (data<<8) | BitBuffer[nPos+rP]; 
	    data8= BitBuffer[nPos+rP];
	    if( rP==0 )
	    {
	    	if( BitOffset%8 )
		    data = data & BitMask[BitOffset%8];
	    }
	    rBit-=8;
	    rP++;
	}
	BitOffset += nBit;
	if( rBit<0 )
	    data = data>>(-rBit);
	if( bDebugGolB )
	{
	    int n;
	    int mask = 1<<(nBit-1);
	    int v = data;
	    for( n=0; n<nBit; n++ )
	    {
	    	if( v & mask )
		    fprintf( stdout, "1 " );
		else
		    fprintf( stdout, "0 " );
		v=v<<1;
	    }
	    fprintf( stdout, "\n" );
	}

	if( bDebug )
	fprintf( stdout, "BitOffset=%d, BitReaded=%d, data=0x%X(0x%X)\n",
		BitOffset, BitReaded, data, data8 );
	*pBufferPos = buffer_pos;

	return data;
}

int GetGolomb( unsigned char *buffer, int *pBufferPos  )
{
int nBit;
int i, n;
int bValue, value=0;
/*
static int base[] = {
	0, 1, 3, 7, 15, 31, 
};
*/
	if( bShowGolomb )
	fprintf( stdout, "Golomb:" );
	if( base[1]==0 )
	{
	    for( n=1; n<MAX_BASE; n++ )
	    {
		base[n] = base[n-1]*2+1;
	    }
	}
//	bDebugGolB = 0;
	for( n=0; n<MAX_BASE; n++ )
	{
	    nBit = GetBit( buffer, pBufferPos, 1 );
	    if( bShowGolomb )
	    fprintf( stdout, "%d ", nBit );
	    if( nBit==1 )
	    	break;
	}
	if( n>=MAX_BASE )
	{
	    fprintf( stdout, "Invalid Golomb\n" );
	    return -1;
	}
	bValue = 0;
	for( i=0; i<n; i++ )
	{
	    nBit = GetBit( buffer, pBufferPos, 1 );
	    if( bShowGolomb )
	    fprintf( stdout, "%d ", nBit );
	    bValue = (bValue<<1) | nBit;
	}
//	bDebugGolB = bDebugGol;
	value = base[n] + bValue;
	if( bShowGolomb )
	fprintf( stdout, " : value=%d (%X,%d=%X,%d)\n", 
		value, BitAddr, BitOffset, BitAddr+BitOffset/8, BitOffset%8 );
	return value;
}
#endif

static char PctStr[11][4] = {
"P ",
"B ",
"I ",
"SP",
"SI",
"P ",
"B ",
"I ",
"SP",
"SI",
"- "
};
static char refStr[5] = "n   ";
int AVC_SliceHeader( unsigned char *bufA, int bufPos,
	int nal_ref_idc,
	unsigned int DTS, unsigned int PTS )
{
	InitBitBuffer();
	int first_mb_in_slice = GetGolombFromBuffer( bufA, &bufPos );
	int slice_type        = GetGolombFromBuffer( bufA, &bufPos );
	int pps_id            = GetGolombFromBuffer( bufA, &bufPos );
	int picture_coding_type = (-1);
/* slice_type
0	P
1	B
2	I
3	SP
4	SI
5	P
6	B
7	I
8	SP
9	SI
*/
#define PCT_I	1
#define PCT_P	2
#define PCT_B	3
	if ( first_mb_in_slice == 0 )
	{
	    switch (slice_type) {
	    case 2:
	    case 4:
	    case 7:
	    case 9:
		picture_coding_type = PCT_I;
		break;
	    case 0:
	    case 3:
	    case 5:
	    case 8:
		picture_coding_type = PCT_P;
		break;
	    default:
		picture_coding_type = PCT_B ;
		break;
	    }
	}
	if( bDebug )
	{
	fprintf( stdout, "first_mb  =%d\n", first_mb_in_slice );
	fprintf( stdout, "slice_type=%d\n", slice_type );
	fprintf( stdout, "pps_id    =%d\n", pps_id );
	fprintf( stdout, "pct=%s\n", PctStr[slice_type] );
	}
	if( first_mb_in_slice==0 )
	{
	    int hour, min, sec, msec;
	    int PTS32=0;
	    TcTime32( PTS32, PTS, &hour, &min, &sec, &msec );
	    fprintf( stdout, 
"SliceHeader(nal_ref_idc=%d,slice_type=%d [%c%s]) %2d:%02d:%02d.%03d(%X,%X)\n",
	    nal_ref_idc, slice_type, refStr[nal_ref_idc], PctStr[slice_type],
		hour, min, sec, msec, DTS, PTS );
	}
	return 0;
}

static char bufEOS[32] = {
0x00, 0x00, 0x00, 0x15,	// Size
0x30, 0x00, 0x00, 0x00,	// Code
0x00, 0x00, 0x00, 0x00,	// PTS
0x00, 0x00, 0x00, 0x00,	// DTS
0x00, 0x00, 0x00, 0x01,	// NAL Size
0x0A			// EOS
};
static char bufAUD[32] = {
0x00, 0x00, 0x00, 0x16,	// Size
0x30, 0x00, 0x00, 0x00,	// Code
0x00, 0x00, 0x00, 0x00,	// PTS
0x00, 0x00, 0x00, 0x00,	// DTS
0x00, 0x00, 0x00, 0x02,	// NAL Size
0x09, 0xE0		// Access unit delimiter
};

int SpesParse( char *filename, char *diffFile, int nSeek,
	int mode, int outMode, int fileID,
	int bNoPTS, int bDetail, int frames[MAX_FRAMES],
	unsigned long SPES[MAX_FRAMES*6]
	)
{
int vPTS=0;

FILE *fpA=NULL, *fpB=NULL;
unsigned char *bufA;
unsigned char *bufB;
int bEOF=0;
int readed;
int nError=0;
int bHead=1;
// --------------------------
int bVC1=0;
int bAVC=0;
int bMPEG4=0;
int bDivX=0;
int bJPEG=0;
int bReal=0;
int bVP8 =0;
// --------------------------
unsigned int maxSpesSize=0;
int nInvalid=0;
int editDTS=0;
int i;
unsigned long NextSPES[6];
int nFrame=0;
	int nSPES=0;

	fprintf( stdout, "========================================\n" );
	fprintf( stdout, "SpesParse(%s) outMode=%d\n", filename, outMode );
	for( i=0; i<6; i++ )
	{
	    NextSPES[i] = 0xFFFFFFFF;
	}

	fpA = fopen( filename, "rb" );
	if( fpA==NULL )
	{
	    fprintf( stdout, "Can't open [%s]\n", filename );
	    exit( 1 );
	}
	if( diffFile!=NULL )
	{
	    fpB = fopen( diffFile, "rb" );
	    if( fpB==NULL )
	    {
		fprintf( stdout, "Can't open [%s]\n", diffFile );
		exit( 1 );
	    }
	}
	FILE *fout = NULL;
	if( outMode==SPES_OUT_ES )
	{
	    if( outFilename[0]==0 )
	    	sprintf( outFilename, "out.es" );
	} else if( outMode==SPES_OUT_ES264 )
	{
	    if( outFilename[0]==0 )
	    	sprintf( outFilename, "out.264" );
	} else if( outMode==SPES_OUT_SPES )
	{
	    if( outFilename[0]==0 )
	    	sprintf( outFilename, "out.spes" );
	} else if( outMode==SPES_OUT_SSPES )
	{
	    if( outFilename[0]==0 )
	    	sprintf( outFilename, "out.sspes" );
	} else {
	    if( outFilename[0]==0 )
	    	sprintf( outFilename, "out.bin" );
	}
	if( outFilename[0] )
	    fout = fopen( outFilename, "wb" );
	if( fout==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", outFilename );
	} else {
	    fprintf( stdout, "%s opened\n", outFilename );
	}
	FILE *fedit = NULL;

	if( (mode==SPES_MODE_EDIT) || (mode==SPES_MODE_DTS_EDIT) )
	{
	    fedit = fopen( "edit.bin", "wb" );
	} else if( bEdit )
	{
	    fedit = fopen( "edit.bin", "wb" );
	}
	bufA = (unsigned char *)malloc( MAX_BUF_SIZE );
	if( bufA==NULL )
	{
	    fprintf( stdout, "Can't malloc bufA\n" );
	    exit( 1 );
	}
	bufB = (unsigned char *)malloc( MAX_BUF_SIZE );
	if( bufB==NULL )
	{
	    fprintf( stdout, "Can't malloc bufB\n" );
	    exit( 1 );
	}

//	int bAudio = 0;
	unsigned int SPES_Size;
	int code;
	int code2=0;
	int total=0;
	int nPES=1;
	int headSize=0;
	if( nSeek>0 )
	{
	    fseek( fpA, nSeek, SEEK_SET );
	    total = nSeek;
	}
	lastDTS=0xFFFFFFFF;
	int nSequence=0;
	while( bEOF==0 )
	{
	    int bNG=0;
	    int bPTS=0;
	    int bDTS=0;
	    unsigned int PTS=0;
	    unsigned int DTS=0;
	    int bValid=0;
	    int bFrame=0;
	    unsigned long top=total;

	    memset( bufA, 0, 32 );
	    readed = fread( bufA, 12, 1, fpA );
	    if( readed<1 )
	    {
		fprintf( stdout, "Can't read 12 bytes (1)@%X=%d\n", 
			total, readed );
		bEOF=1;
		break;
	    }
	    if( fpB )
	    {
		readed = fread( bufB, 12, 1, fpB );
		if( readed<1 )
		{
			fprintf( stdout, "Can't read 12 bytes fpB\n" );
		}
	    }
	    bFrame = 0;
#if 0
	    if( bAudio || 
	    ((bufA[0]==0x00) && (bufA[1]==0x00) && (bufA[2]==0x01)) )
	    {	// Audio
		fread( &bufA[12], 2, 1, fpA );
		if( fpB )
		fread( &bufB[12], 2, 1, fpB );
	    	if( bAudio==0 )
		{
		    fprintf( stdout, "Audio SPES\n" );
		    bAudio=1;
		}
		int prefix = (bufA[0]<<16)
			   | (bufA[1]<< 8)
			   | (bufA[2]<< 0);
		int id = bufA[3];
		SPES_Size = (bufA[6]<<24)
			  | (bufA[7]<<16)
			  | (bufA[4]<< 8)
			  | (bufA[5]<< 0);
		int PES_HEADER_len = bufA[8];
		headSize = 14;
		total += headSize;
		int body = SPES_Size-6;
		readed = fread( &bufA[headSize], body, 1, fpA );
		total += body;
		bPTS  = (bufA[ 9]>>4);
		PTS32 = (bufA[10]<<24)
		      |	(bufA[11]<<16)
		      |	(bufA[12]<< 8)
		      |	(bufA[13]<< 0);

		PTS = ((PTS32 & 0x00007FFF)<< 0)
		    | ((PTS32 & 0xFFFF0000)<<15);

		fprintf( stdout, "%5d :A=%7X:S=%5X:%06X:%2X ",
		    nPES, total-SPES_Size, SPES_Size, prefix, id );
		fprintf( stdout, "\n" );
	    } else 
#endif
	    {	// Video
		SPES_Size = (bufA[0]<<24)
			  | (bufA[1]<<16)
			  | (bufA[2]<< 8)
			  | (bufA[3]<< 0);
//	  fprintf( stdout, "SPES_Size=%X\n", SPES_Size );
	        if( bVP8 )
		{
		SPES_Size = (bufA[3]<<24)
			  | (bufA[2]<<16)
			  | (bufA[1]<< 8)
			  | (bufA[0]<< 0);
		SPES_Size += 12;
		} else {
		    fread( &bufA[12], 4, 1, fpA );
		    if( fpB )
		    fread( &bufB[12], 4, 1, fpB );
		}
		code = bufA[4];
// Check SPES valid or not
#define VP8_HEADER 0x444B4946
		if( SPES_Size==VP8_HEADER )
		{
		    fprintf( stdout, "VP8_Header\n" );
		    SPES_Size = 0x20;
		    headSize=16;
		    bValid=1;
		    if( bVP8==0 )
		    	Title( (char *)"VP8", total );
		    bVP8=1;
		} else
		if( SPES_Size==0 )
		{
		    if( code==0xA1 )		// Real : EOS
		    {
			headSize=16;
			SPES_Size=16;
			bValid=1;
		    }
//		} else if( (SPES_Size>16) && (SPES_Size<0x100000) )
//		} else if( (SPES_Size>16) && (SPES_Size<0x140000) )	// MJPEG
		} else if( (SPES_Size>16) && (SPES_Size<0x2000000) )	// MJPEG
		{
		if( code=='V' )			// Real : VIDO
		{
		    if( (bufA[4]=='V')
		     && (bufA[5]=='I')
		     && (bufA[6]=='D')
		     && (bufA[7]=='O') )
		    {
			if( bReal==0 )
			    Title( (char *)"Real", total );
			headSize=16;
			bReal=1;
			bValid=1;
		    }
		} else if( code==0xA0 )		// Real
		{
			headSize=16;
			bReal=1;
			bValid=1;
#if 0
		    if( deltaPTS>0 )
		    {
			bufA[ 8] = ((vPTS/2)>>24)&0xFF;
			bufA[ 9] = ((vPTS/2)>>16)&0xFF;
			bufA[10] = ((vPTS/2)>> 8)&0xFF;
			bufA[11] = ((vPTS/2)>> 0)&0xFF;
			vPTS+=deltaPTS;
		    }
#endif
		} else if( code==0x30 )		// H.264
		{
		    headSize=16;
		    if( bAVC==0 )
		    	Title( (char *)"AVC", total );
		    bAVC=1;
		    bValid=1;
		    bFrame=1;
		 } else if( code==0x31 )	// SPS
		 {
		    headSize=16;
		    code2 = (bufA[6]<<8) | bufA[7];
		    if( bAVC==0 )
		    	Title( (char *)"AVC", total );
		    bAVC=1;
		    bValid=1;
		 } else if( code==0x32 )	// PPS
		 {
		    headSize=16;
		    if( bAVC==0 )
		    	fprintf( stdout, "AVC SPES\n" );
		    bAVC=1;
		    bValid=1;
		 } else if( code==0x20 )	// 
		 {
		    headSize=16;
		    if( bMPEG4==0 )
		    	Title( (char *)"MPEG4", total );
		    bMPEG4=1;
		    bValid=1;
		    bFrame=1;
		    if( deltaPTS>0 )
		    {
			bufA[ 8] = ((vPTS/2)>>24)&0xFF;
			bufA[ 9] = ((vPTS/2)>>16)&0xFF;
			bufA[10] = ((vPTS/2)>> 8)&0xFF;
			bufA[11] = ((vPTS/2)>> 0)&0xFF;
			vPTS+=deltaPTS;
		    }
		 } else if( code==0x21 )	// 
		 {
		    headSize=16;
		    if( bMPEG4==0 )
		    	Title( (char *)"MPEG4", total );
		    bMPEG4=1;
		    bValid=1;
		 } else if( code==0x41 )	// VC1-Parameters
		 {
		    int type = bufA[5];
		    if( bVC1==0 )
		    	Title( (char *)"VC1", total );
		    if( type==0x01 )	// Parameters(Advanced Profile)
		    {
			headSize=16;
			fprintf( stdout, "VC1(%02X) : Advanced\n", type );
		    } else if( type==0x00 )// Parameters(Simple/Main Profile)
		    {
			headSize=16;
			fprintf( stdout, "VC1(%02X) : Simple/Main\n", type );
			nSequence++;
		    	if( outMode == SPES_OUT_SPES )
			if( fout )
			{
			    fclose( fout );
			    sprintf( outFilename, "out-%04d.spes", nSequence );
			    fout = fopen( outFilename, "wb" );
			}
		    } else {
			fprintf( stdout, "Invalid VC1 type(%02X)\n", type );
			exit( 1 );
		    }
		    bVC1 = 1;
		    bValid=1;
		 } else if( code==0x40 )	// VC1-FrameData
		 {
		    headSize=16;
		    bValid=1;
		    bFrame=1;
		    if( deltaPTS>0 )
		    {
			bufA[ 8] = ((vPTS/2)>>24)&0xFF;
			bufA[ 9] = ((vPTS/2)>>16)&0xFF;
			bufA[10] = ((vPTS/2)>> 8)&0xFF;
			bufA[11] = ((vPTS/2)>> 0)&0xFF;
			vPTS+=deltaPTS;
		    }
		 } else if( code==0x50 )	// DivX
		 {
		    if( bDivX==0 )
		    	Title( (char *)"DivX", total );
		    bDivX=1;
		    readed = fread( &bufA[16], 8, 1, fpA );
		    if( readed<1 )
		    {
			fprintf( stdout, "Can't read 8 bytes (2)@%X=%d\n", 
			    total+16, readed );
			bEOF=1;
			break;
		    }
		    int Width, Height;
		    Width    = (bufA[16]<<24)
			     | (bufA[17]<<16)
			     | (bufA[18]<< 8)
			     | (bufA[19]<< 0);
		    Height   = (bufA[20]<<24)
			     | (bufA[21]<<16)
			     | (bufA[22]<< 8)
			     | (bufA[23]<< 0);
		    if( (Width!=0) || (Height!=0) )
		    {
			fprintf( stdout, "Width=%4d, Height=%4d\n",
			    Width, Height );
		    }
		    code2 = bufA[6]>>4;
		    headSize=16+8;
		    bValid=1;
		    if( deltaPTS>0 )
		    {
			bufA[ 8] = ((vPTS/2)>>24)&0xFF;
			bufA[ 9] = ((vPTS/2)>>16)&0xFF;
			bufA[10] = ((vPTS/2)>> 8)&0xFF;
			bufA[11] = ((vPTS/2)>> 0)&0xFF;
			vPTS+=deltaPTS;
		    }
		 } else if( code==0x90 )	// JPEG-FrameData
		 {
		    code2 = bufA[5]&15;
		    headSize=16;
		    if( bJPEG==0 )
		    	Title( (char *)"JPEG", total );
		    bJPEG=1;
		    bValid=1;
		    bFrame=1;
		    if( deltaPTS>0 )
		    {
			bufA[ 8] = ((vPTS/2)>>24)&0xFF;
			bufA[ 9] = ((vPTS/2)>>16)&0xFF;
			bufA[10] = ((vPTS/2)>> 8)&0xFF;
			bufA[11] = ((vPTS/2)>> 0)&0xFF;
			bufA[12] = ((vPTS/2)>>24)&0xFF;
			bufA[13] = ((vPTS/2)>>16)&0xFF;
			bufA[14] = ((vPTS/2)>> 8)&0xFF;
			bufA[15] = ((vPTS/2)>> 0)&0xFF;
			vPTS+=deltaPTS;
		    }
		 } else if( code==0x91 )	// JPEG-DHT
		 {
		    headSize=16;
		    if( bJPEG==0 )
		    	Title( (char *)"JPEG", total );
		    bJPEG=1;
		    bValid=1;
		 } else if( code==0x92 )	// JPEG-DQT
		 {
		    headSize=16;
		    if( bJPEG==0 )
		    	Title( (char *)"JPEG", total );
		    bJPEG=1;
		    bValid=1;
		 } else if( code==0x70 )	// VP8 Frame
		 {
		    headSize=12;
		    if( bVP8==0 )
		    	Title( (char *)"VP8", total );
		    bVP8=1;
		    bValid=1;
		    bFrame=1;
/*
		    *PTS  = (bufA[ 8]<<24)
			  | (bufA[ 9]<<16)
			  | (bufA[10]<< 8)
			  | (bufA[11]<< 0);
*/
		    if( deltaPTS>0 )
		    {
			bufA[ 8] = ((vPTS/2)>>24)&0xFF;
			bufA[ 9] = ((vPTS/2)>>16)&0xFF;
			bufA[10] = ((vPTS/2)>> 8)&0xFF;
			bufA[11] = ((vPTS/2)>> 0)&0xFF;
			vPTS+=deltaPTS;
		    }
		 }
		 }
		 // ------------------------------------------------------
		 if( bValid==0 )
		 {	// Invaild
		    fprintf( stdout, "%8X : %02X %02X %02X %02X : %02X ", 
		    	total, bufA[0], bufA[1], bufA[2], bufA[3], bufA[4] );
		    fprintf( stdout, ": Invalid code (%02X)\n", code );	
		    fflush( stdout );
		    nInvalid ++;
#ifdef DEBUG
		    fprintf( stdout, "bHead=%d\n", bHead );
#endif
		    if( bHead )
		    {
			int nSkip=0;
			while( 1 )
			{
			    memcpy( &bufA[0], &bufA[1], 15 );
			    readed = fread( &bufA[15], 1, 1, fpA );
			    nSkip++;
			    total++;
			    if( readed<1 )
			    {
				fprintf( stdout, "EOF!!\n" );
				exit( 1 );
			    }
			    SPES_Size = (bufA[0]<<24)
				      | (bufA[1]<<16)
				      | (bufA[2]<< 8)
				      | (bufA[3]<< 0);
			    code = bufA[4];
			    // Check SPES_Size is valid or not
			    if( (code>=0x20) && 
			        (SPES_Size>16) && (SPES_Size<0x80000) )
			    {
			    	if( bShowSkip )
				{
				    fprintf( stdout, 
					"%8X : SPES_Size=0x%08X, code=%02X\n",
					total, SPES_Size, code );
				    fflush( stdout );
				}
				int bFound=0;
				unsigned char temp[1024*1024];
				int readSize = SPES_Size+8;
				switch( code )
				{
				case 0x20 :	// MPEG4
				case 0x21 :
				    if( (SPES_Size<0x20000) )
				    {
					readed=fread( temp, 1, readSize, fpA );
					fseek( fpA,  -readSize, SEEK_CUR );
					int nCode = temp[SPES_Size-16+4];
					if( (nCode==0x20) 
					 || (nCode==0x21) )
					{
					fprintf( stdout, "readed(%d/%d)\n",
						readed, readSize );
					fprintf( stdout, 
					    "%02X %02X %02X %02X : %02X\n",
						temp[SPES_Size-16+0],
						temp[SPES_Size-16+1],
						temp[SPES_Size-16+2],
						temp[SPES_Size-16+3],
						temp[SPES_Size-16+4] );
					    bFound=1;
					    bValid=1;
					    bMPEG4=1;
					}
				    }
				    break;
				case 0x30 :	// AVC
				case 0x31 :
				case 0x32 :
				    if( (SPES_Size<0x40000) )
				    {
					readed=fread( temp, 1, readSize, fpA );
					fseek( fpA,  -readSize, SEEK_CUR );
					int nCode = temp[SPES_Size-16+4];
					if( (nCode==0x30) 
					 || (nCode==0x31) || (nCode==0x32) )
					{
					fprintf( stdout, "readed(%d/%d)\n",
						readed, readSize );
					fprintf( stdout, 
					    "%02X %02X %02X %02X : %02X\n",
						temp[SPES_Size-16+0],
						temp[SPES_Size-16+1],
						temp[SPES_Size-16+2],
						temp[SPES_Size-16+3],
						temp[SPES_Size-16+4] );
					    bFound=1;
					    bValid=1;
					    bAVC=1;
					}
				    }
				    break;
				case 0x40 :	// VC1
				case 0x41 :
				    if( (SPES_Size<0x1000) )
				    {
					readed=fread( temp, 1, readSize, fpA );
					fseek( fpA,  -readSize, SEEK_CUR );
					int nCode = temp[SPES_Size-16+4];
					if( (nCode==0x40) 
					 || (nCode==0x41) )
					{
					fprintf( stdout, "readed(%d/%d)\n",
						readed, readSize );
					fprintf( stdout, 
					    "%02X %02X %02X %02X : %02X\n",
						temp[SPES_Size-16+0],
						temp[SPES_Size-16+1],
						temp[SPES_Size-16+2],
						temp[SPES_Size-16+3],
						temp[SPES_Size-16+4] );
					    bFound=1;
					    bValid=1;
					    bVC1=1;
					}
				    }
				    break;
			    // Check VP8
				case 0x70 :	// VP8 Frame
				    {
					readed=fread( temp, 1, readSize, fpA );
					fseek( fpA,  -readSize, SEEK_CUR );
					int nCode = temp[SPES_Size-16+4];
					if( (nCode==0x70) )
					{
					fprintf( stdout, "readed(%d/%d)\n",
						readed, readSize );
					fprintf( stdout, 
					    "%02X %02X %02X %02X : %02X\n",
						temp[SPES_Size-16+0],
						temp[SPES_Size-16+1],
						temp[SPES_Size-16+2],
						temp[SPES_Size-16+3],
						temp[SPES_Size-16+4] );
					    bFound=1;
					    bValid=1;
					    bVP8=1;
					}
				    }
				    break;
				case 0x90 : // MJPEG
				    {
					readed=fread( temp, 1, readSize, fpA );
					fseek( fpA,  -readSize, SEEK_CUR );
					int nCode = temp[SPES_Size-16+4];
					if( (nCode==0xA0) 
					 || (nCode==0xA1) )
					{
					fprintf( stdout, "readed(%d/%d)\n",
						readed, readSize );
					fprintf( stdout, 
					    "%02X %02X %02X %02X : %02X\n",
						temp[SPES_Size-16+0],
						temp[SPES_Size-16+1],
						temp[SPES_Size-16+2],
						temp[SPES_Size-16+3],
						temp[SPES_Size-16+4] );
					    bFound=1;
					    bValid=1;
					    bJPEG=1;
					}
				    }
				    break;
				case 0xA0 : // Real Frame
				    {
					unsigned char temp[1024*1024];
					int readSize = SPES_Size+8;
					readed=fread( temp, 1, readSize, fpA );
					fseek( fpA,  -readSize, SEEK_CUR );
#if 0
					int nCode = temp[SPES_Size-16+4];
					if( (nCode==0xA0) 
					 || (nCode==0xA1) )
#endif
					if( bufA[5]==0xC0 )
					{
					fprintf( stdout, "readed(%d/%d)\n",
						readed, readSize );
					fprintf( stdout, 
					    "%02X %02X %02X %02X : %02X\n",
						temp[SPES_Size-16+0],
						temp[SPES_Size-16+1],
						temp[SPES_Size-16+2],
						temp[SPES_Size-16+3],
						temp[SPES_Size-16+4] );
					    bFound=1;
					    bValid=1;
					    bReal=1;
					}
				    }
				    break;
				default :
				    break;
				}
				if( bFound )
				{
				    fprintf( stdout, "nSkip=%d\n", nSkip );
				    fflush( stdout );
				    headSize=16;
				    ShowHeader( total, bufA );
				    break;
				}
			    }
			}
		    } else {
#if 1
		    	if( bVC1 )
			{
#ifdef DEBUG
			    fprintf( stdout, "Skip as VC1\n" );
			    fflush( stdout );
#endif
			    int nSkip=0;
			    while( 1 )
			    {
				memcpy( &bufA[0], &bufA[1], 15 );
				readed = fread( &bufA[15], 1, 1, fpA );
				nSkip++;
				total++;
				if( readed<1 )
				{
				    fprintf( stdout, "EOF!!\n" );
				    exit( 1 );
				}
				SPES_Size = (bufA[0]<<24)
					  | (bufA[1]<<16)
					  | (bufA[2]<< 8)
					  | (bufA[3]<< 0);
				code = bufA[4];
			// Check VC1
				if( (SPES_Size>16) && (SPES_Size<0x01000)
				&& ((code==0x40) || (code==0x41)) )
				{
				    fprintf( stdout, "nSkip=%d\n", nSkip );
				    headSize=16;
				    ShowHeader( total, bufA );
				    break;
				}
			    }
			} else  {
#endif
			    if( bAVC )
			    {
				int nSkip=0;
#ifdef DEBUG
				fprintf( stdout, "Skip as AVC\n" );
				fflush( stdout );
#endif
				while( 1 )
				{
				    memcpy( &bufA[0], &bufA[1], 15 );
				    readed = fread( &bufA[15], 1, 1, fpA );
				    nSkip++;
				    total++;
				    if( readed<1 )
				    {
					fprintf( stdout, "EOF!!\n" );
					exit( 1 );
				    }
				    SPES_Size = (bufA[0]<<24)
					      | (bufA[1]<<16)
					      | (bufA[2]<< 8)
					      | (bufA[3]<< 0);
				    code = bufA[4];
			// Check AVC
				    if( (SPES_Size>16) && (SPES_Size<0x20000)
				    && ((code==0x30) 
				     || (code==0x31)
				     || (code==0x32)) )
				    {
					fprintf( stdout, "nSkip=%d\n", nSkip );
					headSize=16;
					ShowHeader( total, bufA );
					break;
				    }
				}
			    } else if( bMPEG4 )
			    {
				int nSkip=0;
#ifdef DEBUG
				fprintf( stdout, "Skip as MPEG4\n" );
				fflush( stdout );
#endif
				while( 1 )
				{
				    memcpy( &bufA[0], &bufA[1], 15 );
				    readed = fread( &bufA[15], 1, 1, fpA );
				    nSkip++;
				    total++;
				    if( readed<1 )
				    {
					fprintf( stdout, "EOF!!\n" );
					exit( 1 );
				    }
				    SPES_Size = (bufA[0]<<24)
					      | (bufA[1]<<16)
					      | (bufA[2]<< 8)
					      | (bufA[3]<< 0);
				    code = bufA[4];
			// Check MPEG4
				    if( (SPES_Size>16) && (SPES_Size<0x20000)
				    && ((code==0x20) 
				     || (code==0x31) ))
				    {
					fprintf( stdout, "nSkip=%d\n", nSkip );
					headSize=16;
					ShowHeader( total, bufA );
					break;
				    }
				}
			    } else
			    {
			fprintf( stdout, "maxSpesSize=0x%X\n", maxSpesSize );
				exit( 1 );
			    }
			}
		    }
		}
		bHead = 0;

		if( bNoPTS )
		    bufA[ 5] = bufA[ 5] & 0x3F;	// PTS/DTS invalid
		total+=headSize;
		if( SPES_Size>MAX_BUF_SIZE )
		{
		   fprintf( stdout, "SPES_Size Too large (%X)@%X\n", 
		    	SPES_Size, total );
		   nInvalid++;
		   SPES_Size =  0;
		   bValid=0;
		} else {
		    int readSize = SPES_Size-headSize;
		    if( code==0xA0 )
		    {
		    	readSize +=20;
			SPES_Size+=20;
		    }
		    readed = fread( &bufA[headSize], 1, readSize, fpA );
//		fprintf( stdout, "readSize=%d, readed=%d\n", readSize, readed );
		    if( readed<readSize )
		    {
			ShowSPES( bufA, bufB, fpB, nPES, bNG,
			    code, code2, SPES_Size, total+SPES_Size-headSize,
			    &bDTS, &DTS, &bPTS, &PTS );
			fprintf( stdout, 
			    "Can't read %d(0x%X) bytes (3) @%8X = %8X\n", 
			    readSize, readSize, total, readed );
			bEOF = 1;
			total += readed;
			fprintf( stdout, "EOF@0x%X\n", total );
			fprintf( stdout, "Incomplete SPES\n" );
			break;
		    }
		    if( code==0xA0 )	// Real Frame
		    {
			int NumSegment = (bufA[16]<<24)
				       | (bufA[17]<<16)
				       | (bufA[18]<< 8)
				       | (bufA[19]<< 0);
//		        int AddSize = 16+4+NumSegment*8;
		        int AddSize = NumSegment*8;
#if 0
	int ii;
	for( ii=0; ii<20; ii++ )
	{
	fprintf( stdout, "%02X ", bufA[ii] );
	}
	fprintf( stdout, "\n" );
			fprintf( stdout, "NumSegment=%d, AddSize=%d\n",
				NumSegment, AddSize );
#endif
			readed = fread( &bufA[SPES_Size], 1, AddSize, fpA );
			SPES_Size += AddSize;
		    }
		 }
		 if( bValid )
		 {
		    ShowSPES( bufA, bufB, fpB, nPES, bNG,
		    	code, code2, SPES_Size, total+SPES_Size-headSize,
		    	&bDTS, &DTS, &bPTS, &PTS );
		 }
		 if( bValid )
		 {
		    if( code==0x30 )	// AVC frame data
		    {
static char NAL_typeStr[32][40] = {
"Unspecified",
"Coded slice of a non-IDR picture",
"Coded slice data partition A",
"Coded slice data partition B",
"Coded slice data partition C",
"Coded slice of an IDR picture",
"SEI",
"SPS",
"PPS",
"AUD",
"End of sequence",
"End of stream",
"Filler data",
"SPS extension",
"Prefix NAL unit",
"Subset SPS",
"Reserved",
"Reserved",
"Reserved",
"Coded slice of an auxiliary",
"Coded slice extension",
"Reserved",
"Reserved",
"Reserved",
"Unspecified",
"Unspecified",
"Unspecified",
"Unspecified",
"Unspecified",
"Unspecified",
"Unspecified",
"Unspecified",
};
static char SEIstr[47][40] = {
"buffering_period",
"pic_timing",
"pan_scan_rect",
"filler_payload",
"user_data_registered",
"user_data_unregistered",
"recovery_point",
"dec_ref_pic_marking_repetition",
"spare_pic",
"scene_info",
"sub_seq_info",
"sub_seq_layer_characteristics",
"sub_seq_characteristics",
"full_frame_freeze",
"full_frame_freeze_release",
"full_frame_snapshot",
"progressive_refinement_segment_start",
"progressive_refinement_segment_end",
"motion_constrained_slice_group_set",
"film_grain_characteristics",
"deblocking_filter_display_preference",
"stereo_video_info",
"post_filter_hint",
"tone_mapping_info",
"scalability_info",
"sub_pic_scalable_layer",
"non_required_layer_rep",
"priority_layer_info",
"layers_not_present",
"layer_depenency_change",
"scalable_nesting",
"base_layer_temporal_hrd",
"quality_layer_integrity_check",
"redundant_pic_property",
"tl0_dep_rep_index",
"tl_switching_point",
"parallel_decoding_info",
"mvc_scalable_nesting",
"view_scalability_info",
"multiview_scene_info",
"multiview_acquisition_info",
"non_required_view_component",
"view_dependency_change",
"operation_points_not_present",
"base_viw_temporal_hrd",
"frame_packing_arrangement",
"reserved_sei",
};
unsigned char prefixNAL[8] = {
0x00, 0x00, 0x00, 0x04, 0x4E, 0x00, 0x00, 0x07
};
		    	int pos=headSize;
			int nSEI=(-1);
			while( pos<SPES_Size )
			{
			    int NAL_Size = (bufA[pos+0]<<24)
					 | (bufA[pos+1]<<16)
					 | (bufA[pos+2]<< 8)
					 | (bufA[pos+3]<< 0);
			    int NAL_type = bufA[pos+4];
			    int nal_ref_idc   = (NAL_type>>5)&0x03;
			    int nal_unit_type = (NAL_type>>0)&0x1F;
			    if( bDumpNAL )
			    {
				fprintf( stdout, 
				    "Size=%06X, type=%02X (%s)\n",
				    NAL_Size, NAL_type,
				    NAL_typeStr[NAL_type&0x1F] );
				if( bDumpSEI )
				{
				if( nal_unit_type==1 )	// non-IDR
				{
				    AVC_SliceHeader( bufA, pos+5, 
				    	nal_ref_idc, DTS, PTS );
				}
				if( nal_unit_type==5 )	// IDR
				{
				    AVC_SliceHeader( bufA, pos+5, 
				    	nal_ref_idc, DTS, PTS );
				}
				if( nal_unit_type==6 )	// SEI
				{
				    int pos2=5;
				    while( pos2<=NAL_Size )
				    {
				    int type=bufA[pos+pos2+0];
				    int cnt=1;
				    int size=0;
				    while( 1 )
				    {
				    	size+=bufA[pos+pos2+cnt];
					if( bufA[pos+pos2+cnt]!=0xFF )
					    break;
					cnt++;
				    }
				    fprintf( stdout, 
				    "Size=%4X, SEI(%2d)=", size, type );
				    if( type<46 )
				    {
					fprintf( stdout, "%s\n", SEIstr[type] );
					nSEI = type;
				    } else {
					fprintf( stdout, "%s\n", SEIstr[46] );
				    }
				    pos2+=(size+2);
//			    fprintf( stdout, "pos2=%d/%d\n", pos2, NAL_Size );
				    }
				}
				}
			    }
			    int bRemove=0;
			    // ----------------------------------------------
			    // Remove
			    // ----------------------------------------------
			    if( bRemovePrefixNAL )
			    {
				if( nal_unit_type==14 )
				{
				    fprintf( stdout, "RemovePrefixNAL\n" );
				    bRemove = 1;
				    int nPos = pos+NAL_Size+4;
				    memcpy( &bufA[pos], &bufA[nPos],
					SPES_Size-nPos );
				    SPES_Size-=(NAL_Size+4);
				}
			    }
			    if( bRemoveAUD )
			    {
				if( nal_unit_type==9 )
				{
				    fprintf( stdout, "RemoveAUD\n" );
				    bRemove = 1;
				    int nPos = pos+NAL_Size+4;
				    memcpy( &bufA[pos], &bufA[nPos],
					SPES_Size-nPos );
				    SPES_Size-=(NAL_Size+4);
				}
			    }
			    if( bRemoveSEI )
			    {
				if( nal_unit_type==6 )
				{
				    fprintf( stdout, "RemoveSEI\n" );
				    bRemove = 1;
				    int nPos = pos+NAL_Size+4;
				    memcpy( &bufA[pos], &bufA[nPos],
					SPES_Size-nPos );
				    SPES_Size-=(NAL_Size+4);
				}
			    }
			    if( nRemoveSEI>=0 )
			    {
				if( nal_unit_type==6 )
				{
				    if( nSEI==nRemoveSEI )
				    {
				    fprintf( stdout, "RemoveSEI\n" );
				    bRemove = 1;
				    int nPos = pos+NAL_Size+4;
				    memcpy( &bufA[pos], &bufA[nPos],
					SPES_Size-nPos );
				    SPES_Size-=(NAL_Size+4);
				    }
				}
			    }
			    if( bRemoveSliceExtension )
			    {
				if( nal_unit_type==20 )
				{
				    fprintf( stdout, "RemoveSlice\n" );
				    bRemove = 1;
				    int nPos = pos+NAL_Size+4;
				    memcpy( &bufA[pos], &bufA[nPos],
					SPES_Size-nPos );
				    SPES_Size-=(NAL_Size+4);
				}
			    }
			    if( nRemoveNAL>=0 )
			    {
				if( nal_unit_type==nRemoveNAL )
				{
				    if( bRemove==0 )
				    {
				    fprintf( stdout, "Remove(%02X)\n", 
				    	nRemoveNAL );
				    bRemove = 1;
				    int nPos = pos+NAL_Size+4;
				    memcpy( &bufA[pos], &bufA[nPos],
					SPES_Size-nPos );
				    SPES_Size-=(NAL_Size+4);
				    }
				}
			    }
			    // ----------------------------------------------
			    // ADD
			    if( bAddPrefixNAL )
			    {
				if( (nal_unit_type==1) 	// non IDR
				 || (nal_unit_type==5)	// IDR
				 )
				{
				    fprintf( stdout, "AddPrefixNAL\n" );
				    memmove( &bufA[pos+8], &bufA[pos],
						SPES_Size-pos );
				    memcpy( &bufA[pos], prefixNAL, 8 );
				    bufA[pos+4] = (NAL_type&0xE0) | 14;
				    pos+=8;
				    SPES_Size+=8;
				}
			    }
			    if( bRemove==0 )
			    {
				pos+=(NAL_Size+4);
			    }
			}
		    }
		 }
    #if 1
    		int written=0;
		if( fout )
		{
		    if( outMode==SPES_OUT_ES )
		    {
			// output ES only
			if( bPES )
			    SPES_Header_PES_Header( &bufA[0], fout );
			int bSave=0;
			if( bSaveFrame )
			{
			    if( bFrame )
				bSave = 1;
			} else {
			    bSave = 1;
			}
			if( bSave )	
			{
			    written = fwrite( &bufA[headSize], 
				1, SPES_Size-headSize, fout );
			}
		    } else if( outMode==SPES_OUT_ES264 )	// for AVC
		    {
			if( code==0x31 )	// SPS
			{
			    fprintf( stdout, "SPS\n" );
			    fwrite( Start0, 1, 4, fout ); // 00 00 00 01 67
			    avcAdded+=4;
			    written = fwrite( &bufA[headSize+2], 
				1, SPES_Size-headSize-2, fout );
			    avcSize+=(SPES_Size-headSize-2);
			    bSPS = 1;
			    if( fedit )
				fwrite( &bufA[0], 1, SPES_Size, fedit );
			} else if( code==0x32 ) // PPS
			{
			    fprintf( stdout, "PPS\n" );
			    fwrite( Start0, 1, 4, fout ); // 00 00 00 01 68
			    avcAdded+=4;
			    written = fwrite( &bufA[headSize+2], 
				1, SPES_Size-headSize-2, fout );
			    avcSize+=(SPES_Size-headSize-2);
			    if( fedit )
				fwrite( &bufA[0], 1, SPES_Size, fedit );
			} else {
			    if( bPES )
				SPES_Header_PES_Header( &bufA[0], fout );
			    fwrite( Start0, 1, 1, fout ); // 00
			    avcAdded+=1;
			    AVC_ES( &bufA[headSize], SPES_Size-headSize, 
			    	fout, total);
//			    avcSize+=(SPES_Size-headSize);
			    if( fedit )
				fwrite( &bufA[0], 1, SPES_Size, fedit );
			}
		    } else if( outMode==SPES_OUT_SPES )
		    {
			int bSave=0;
			if( bSaveFrame )
			{
			    if( bFrame )
				bSave = 1;
			} else {
			    bSave = 1;
			}
		    	if( bVP8==0 )
			{
		    	bufA[0] = (SPES_Size>>24)&0xFF;
		    	bufA[1] = (SPES_Size>>16)&0xFF;
		    	bufA[2] = (SPES_Size>> 8)&0xFF;
		    	bufA[3] = (SPES_Size>> 0)&0xFF;
			}
			if( bSave )
			{
			    written = fwrite( &bufA[0], 1, SPES_Size, fout );
			    if( code==0x30 )
			    {
				if( bAddEOS )
				written = fwrite( bufEOS, 1, 21, fout );
				if( bAddAUD )
				written = fwrite( bufAUD, 1, 22, fout );
			    }
			}
		    } else if( outMode==SPES_OUT_CUT )
		    {
			char fname[256];
			fname[0]=0;
			switch( code )
			{
			case 0x31 :	// SPS
			    if( outFilename[0] )
				sprintf( fname, "%s-%04d-sps.spes", 
				    outFilename, nFrame );
			    else
				sprintf( fname, "cut-%04d-sps.spes", nFrame );
			    break;
			case 0x32 :	// PPS
			    if( outFilename[0] )
				sprintf( fname, "%s-%04d-pps.spes", 
				    outFilename, nFrame );
			    else
				sprintf( fname, "cut-%04d-pps.spes", nFrame );
			    break;
			case 0x91 :	// DH
			    if( outFilename[0] )
				sprintf( fname, "%s-%04d-DH.spes", 
				    outFilename, nFrame );
			    else
				sprintf( fname, "cut-%04d-DH.spes", nFrame );
			    break;
			case 0x30 :	// AVC Frame data
			case 0x90 :	// MJPEG Frame data
			    if( outFilename[0] )
				sprintf( fname, "%s-%04d.spes", 
				    outFilename, nFrame );
			    else
				sprintf( fname, "cut-%04d.spes", nFrame );
			    nFrame++;
			    break;
			case 'V' :	// Real Header
			    if( outFilename[0] )
				sprintf( fname, "%s-%04d-RH.spes", 
				    outFilename, nFrame );
			    else
				sprintf( fname, "cut-%04d-RH.spes", nFrame );
			    break;
			case 0xA0 :	// Real Frame
			    if( outFilename[0] )
				sprintf( fname, "%s-%04d.spes", 
				    outFilename, nFrame );
			    else
				sprintf( fname, "cut-%04d.spes", nFrame );
			    nFrame++;
			    break;
			case 0xA1 :	// Real EOS
			    if( outFilename[0] )
				sprintf( fname, "%s-%04d-RE.spes", 
				    outFilename, nFrame );
			    else
				sprintf( fname, "cut-%04d-RE.spes", nFrame );
			    break;
			}
			if( fname[0] )
			{
			    FILE *fcut = fopen( fname, "wb" );
			    if( fcut )
			    {
				written = fwrite( &bufA[0], 1, SPES_Size,fcut);
				fclose( fcut );
			    }
			}
		    } else if( outMode==SPES_OUT_SSPES )
		    {
/*
			if( code==0x31 )	// SPS
			{
			    int *pInt = &fubufA[8];
			    *(pInt+0) = 0;
			    *(pInt+1) = 0;
			    written = fwrite( &bufA[0], 1, SPES_Size,fcut);
			} else {
			    written = fwrite( &bufA[0], 1, SPES_Size,fcut);
			}
*/
		    }
		}
    #endif
		if( fpB )
		{
		     readed = fread( &bufB[headSize], 
		     	SPES_Size-headSize, 1, fpB );
		     for( i=headSize; i<(int)SPES_Size; i++ )
		     {
			    if( bufA[i]!=bufB[i] )
			    {
				if( bDetail )
				fprintf( stdout, "Diff(%8X) : %02X != %02X\n",
				    total-headSize+i, bufA[i], bufB[i] );
				nError++;
				bNG++;
				if( bDetail )
				if( nError>100 )
				{
				    fprintf( stdout, "Too many Errors(%d)\n",
					nError );
				    exit( 1 );
				}
			    }
		     }
		}
		total+=(SPES_Size-headSize);
		if( bValid )
		{
		    if( mode==SPES_MODE_EDIT )
		    {
			if( nPES<MAX_FRAMES )
			if( frames[nPES]>0 )
			    fwrite( bufA, 1, SPES_Size, fedit );
		    } else if( mode==SPES_MODE_DTS_EDIT )
		    {
			if( code==0x40 )	// VC1-FrameData
			{
			    bufA[12] = (editDTS>>24)&0xFF;
			    bufA[13] = (editDTS>>16)&0xFF;
			    bufA[14] = (editDTS>> 8)&0xFF;
			    bufA[15] = (editDTS>> 0)&0xFF;
			    editDTS = editDTS+3000;
			}
			fwrite( bufA, 1, SPES_Size, fedit );
		    }
		    if( SPES_Size>maxSpesSize )
			maxSpesSize = SPES_Size;

		    if( NextSPES[0]<MAX_FILE )
		    {
		    	SPES[nSPES*6+0] = NextSPES[0];
		    	SPES[nSPES*6+1] = NextSPES[1];
		    	SPES[nSPES*6+2] = NextSPES[2];
		    	SPES[nSPES*6+3] = NextSPES[3];
		    	SPES[nSPES*6+4] = NextSPES[4];
		    	SPES[nSPES*6+5] = NextSPES[5];
			nSPES++;
		    }
		    // -------------
		    NextSPES[0] = fileID;
		    NextSPES[1] = top;
		    NextSPES[2] = SPES_Size;
		    if( bDTS )
			NextSPES[3] = DTS;
		    else
			NextSPES[3] = 0xFFFFFFFF;
		    if( bPTS )
			NextSPES[4] = PTS;
		    else
			NextSPES[4] = 0xFFFFFFFF;
		    // -------------
		} else {
		    // -------------
		    NextSPES[0] = 0xFFFFFFFF;
		    NextSPES[1] = 0;
		    NextSPES[2] = 0;
		    // -------------
		}
	    }
	    nPES++;
	}
	fclose( fpA );
	if( fpB )
	    fclose( fpB );
	if( fout )
	{
	    if( outMode==SPES_OUT_ES264 )	// for AVC
	    {
	    	fprintf( stdout, "AVC Size=%d bytes\n", avcSize );
	    	fprintf( stdout, "Added %d bytes\n", avcAdded );
	    	fprintf( stdout, "avcSum1 =%d bytes\n", avcSum1 );
	    	fprintf( stdout, "avcSum2 =%d bytes\n", avcSum2 );
	    }
	    fclose( fout );
	}
	if( fedit )
	    fclose( fedit );
	fprintf( stdout, "maxSpesSize=0x%X\n", maxSpesSize );
	fprintf( stdout, "invalid=%d\n", nInvalid );
	return nSPES;
}


#define CODEC_NONE	0
#define	CODEC_AVC	1
#define CODEC_VC1	2
#define CODEC_MP4	3

//typedef unsigned int UINT;

// ------------------------------------

#define MAX_FILE 16

//#define MAX_FRAMES 1024*1024
int frames[MAX_FRAMES];
int outMode = SPES_OUT_ES;
int bNoPTS = 0;
int bDetail=0;
int nSeek=0;
int nSPES=0;
unsigned long SPES[MAX_FRAMES*6];

static unsigned long total_copy=0;

#if 1
int spesCopy( char *filename, unsigned long offset, unsigned long size,
	FILE *pMerge )
{
#define BUF_SIZE 1024*64
char buffer[BUF_SIZE];
unsigned long total=0;
#if 1
	fprintf( stdout, "spesCopy(%8X : %s,%8X,%8X)", 
		(UINT)total_copy, filename, (UINT)offset, (UINT)size );
#endif
	FILE *fp = fopen( filename, "rb" );
	if( fp )
	{
	    if( fseek( fp, offset, SEEK_SET )!=0 )
	    {
	    	fprintf( stdout, "fseek(0x%X) failed\n", (UINT)offset );
	    };
	    int readed, written;
	    int rest=size;
	    while( total<size )
	    {
	    	int readSize = rest;
		if( readSize>BUF_SIZE )
		    readSize = BUF_SIZE;
		readed = fread( buffer, 1, readSize, fp );
		written = fwrite( buffer, 1, readed, pMerge );
		if( written<readed )
		{
		    fprintf( stdout, "Can't write (%d/%d)\n", written, readed );
		    exit( 1 );
		}
		rest  -= readed;
		total += readed;
	    }
	    fclose( fp );
	}
	fprintf( stdout, " : total=%X\n", (UINT)total );
	total_copy += total;
	return 0;
}
#endif

void spesUsage( char *prg )
{
	fprintf( stdout, 
	"%s [-P(noPTS) -Avc] [+Edit +Merge +e(ES out)] filename\n", prg );
	fprintf( stdout, "\t+I : Dump NAL and SEI\n" );
	fprintf( stdout, "\t+e : output H.264 ES\n" );
	fprintf( stdout, "\t+P : add PES header\n" );
	exit( 1 );
}

int spes( int argc, char *argv[] )
{
char *filename[MAX_FILE];
char *diffFile=NULL;
int i;
int args=0;
int mode    = SPES_MODE_READ;
//int codec   = CODEC_NONE;
int nParam = 0;
int Error=0;
// ------------------

	memset( outFilename, 0, 1024 );
	for( i=0; i<MAX_FILE; i++ )
	{
	    filename[i] = NULL;
	}

	for( i=0; i<MAX_FRAMES; i++ )
	{
	    frames[i] = 0;
	}
	for( i=0; i<MAX_FRAMES*4; i++ )
	{
	    SPES[i] = 0;
	}
	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='=' )
	    {
	    	switch( argv[i][1] )
		{
		case 'E' :
		    mode = SPES_MODE_EDIT;
		    break;
		case 'e' :
		    bEdit = 1;
		    break;
		case 'M' :
		    mode = SPES_MODE_MERGE;
		    break;
		case 'T' :
		    mode = SPES_MODE_DTS_EDIT;
		    break;
		    spesUsage( argv[0] );
		    break;
		case 'P' :
		    deltaPTS = atoi( &argv[i][2] );
		    fprintf( stdout, "deltaPTS=%d\n", deltaPTS );
		    break;
		case 'F' :
		    bSaveFrame=1;
		    break;
		case 'S' :
		    nSeek = atoi( &argv[i][2] );
		    fprintf( stdout, "nSeek=%d\n", nSeek );
		    break;
		default :
		    spesUsage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'e' :
		    outMode = SPES_OUT_ES264;
		    fprintf( stdout, "SPES_OUT_ES264\n" );
		    break;
		case 'S' :
		    outMode = SPES_OUT_SPES;
		    fprintf( stdout, "SPES_OUT_SPES\n" );
		    nParam = atoi( &argv[i][2] );
		    break;
		case 'E' :
		    outMode = SPES_OUT_SPES;
		    fprintf( stdout, "SPES_OUT_SPES\n" );
		    bAddEOS = 1;
		    break;
		case 'A' :
		    outMode = SPES_OUT_SPES;
		    fprintf( stdout, "SPES_OUT_SPES\n" );
		    bAddAUD = 1;
		    break;
		case 'C' :
		    outMode = SPES_OUT_CUT;
		    fprintf( stdout, "SPES_OUT_CUT\n" );
		    break;
		case 'D' :
		    bDTSPTS=1;
		    break;
		case 'P' :
		    bPES = 1;
		    fprintf( stdout, "PES_Header On\n" );
		    break;
		case 'N' :
		    bDumpNAL=1;
		    break;
		case 'I' :
		    bDumpNAL=1;
		    bDumpSEI=1;
		    break;
		case 'R' :
		    bAddPrefixNAL=1;
		    outMode = SPES_OUT_SPES;
		    break;
		case 's' :
		    bShowSkip=1;
		    break;
		default :
		    spesUsage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case 'P' :	// PTS
		    bNoPTS = 1;
		    break;
		case 'V' :
		    bDetail=1;
		    break;
		case 'p' :	// prefix NAL
		    bRemovePrefixNAL=1;
		    break;
		case 'A' :	// AUD
		    bRemoveAUD=1;
//		    codec = CODEC_AVC;
		    break;
		case 'E' :	// Extension
		    bRemoveSliceExtension=1;
		    break;
		case 'S' :	// SEI
		    bRemoveSEI=1;
		    break;
		case 's' :	// SEI
		    nRemoveSEI = GetValue( &argv[i][2], &Error );
		    break;
		case 'R' :	// Remove NAL XX
		    nRemoveNAL = GetValue( &argv[i][2], &Error );
		    break;
		default :
		    spesUsage( argv[0] );
		    break;
		}
	    } else {
	    	switch( args )
		{
		case 0 :
		    filename[0] = argv[i];
		    break;
		case 1 :
		default :
		    if( (outMode==SPES_OUT_ES264)
		     || (outMode==SPES_OUT_ES)
		     || (outMode==SPES_OUT_CUT) )
		    {
		    	memcpy( outFilename, argv[i], strlen(argv[i]) );
			fprintf( stdout, "outFilename=%s\n", outFilename );
		    } else if( mode==SPES_MODE_READ )
		    {
			diffFile = argv[i];
		    } else if( mode==SPES_MODE_MERGE )
		    {
		    	if( args<MAX_FILE )
			    filename[args] = argv[i];
			else {
			    fprintf( stdout, "Too many filename\n" );
			    spesUsage( argv[0] );
			}
		    } else {
			int j;
			int from=0, to=0;
			int phase=0;
		    	for( j=0; j<(int)strlen(argv[i]); j++ )
			{
			    if( (argv[i][j]>='0') && (argv[i][j]<='9') )
			    {
			    	if( phase==0 )
				    from=from*10+argv[i][j]-'0';
				else
				    to  =to  *10+argv[i][j]-'0';
			    } else if( argv[i][j]=='-' )
			    {
			    	phase++;
			    } else {
			    	break;
			    }
			}
			if( from>0 )
			{
			    if( to>from )
			    {
			    	for( j=from; j<=to; j++ )
				{
				    frames[j]=1;
				}
			    } else
			    	frames[from]=1;
			}
		    }
		    break;
		}
	    	args++;
	    }
	}
	if( filename[0]==NULL )
	{
	    spesUsage( argv[0] );
	}
	if( mode==SPES_MODE_MERGE )
	{
	    for( i=0; i<args; i++ )
	    {
		nSPES = SpesParse( filename[i], NULL, nSeek, mode, outMode, i,
		    bNoPTS, bDetail, frames, SPES );
	    }
	    fprintf( stdout, "========================================\n" );
	    fprintf( stdout, "nSPES=%d\n", nSPES );
	    unsigned long lastDTS=0xFFFFFFFF;
	    FILE *pMerge=fopen( "merge.bin", "w" );
	    for( i=0; i<nSPES; i++ )
	    {
	    	int bSkipped=0;
		int diffDTS=(-1);
		unsigned long nDTS = SPES[i*6+3];
//		unsigned long nPTS = SPES[i*6+4];
#if 1
		if( SPES[i*6+0]!=0 )
		{
		    nDTS=lastDTS+3753*180;
		    bSkipped=1;
//		    break;
		}
#endif
		if( nDTS!=0xFFFFFFFF )
		{
		    if( lastDTS!=0xFFFFFFFF )
		    {
		    	diffDTS=nDTS-lastDTS;
			if( diffDTS>3800 )
			    bSkipped=1;
		    }
		}
#if 1
		if( bSkipped )
		{
		    int n;
		    fprintf( stdout, "---------------------------------\n" );
		    for( n=i; n<nSPES; n++ )
		    {
			unsigned long nDTS2 = SPES[n*6+3];
//			unsigned long nPTS2 = SPES[n*6+4];
			unsigned long copy_offset = SPES[n*6+1];
			unsigned long copy_size   = SPES[n*6+2];
#if 0
			fprintf( stdout, "(%2d : %8X %5X %08X %08X :%6d)\n",
			    SPES[n*6+0],
			    copy_offset,
			    copy_size,
			    SPES[n*6+3],
			    SPES[n*6+4],
			    nDTS2-lastDTS
			    );
#endif
			if( (nDTS2>lastDTS) && (nDTS2<nDTS) )
			{
			    spesCopy( filename[SPES[n*6+0]], 
				    copy_offset,	// offset
				    copy_size,		// size
				    pMerge );
			    fprintf( stdout, "%2d : %8X %5X %08X %08X :%6d\n",
			    (UINT)SPES[n*6+0],
			    (UINT)copy_offset,
			    (UINT)copy_size,
			    (UINT)SPES[n*6+3],
			    (UINT)SPES[n*6+4],
			    (UINT)(nDTS2-lastDTS)
			    );
			    lastDTS = nDTS2;
			    diffDTS = nDTS-lastDTS;
			}
		    }
		    fprintf( stdout, "---------------------------------\n" );
		}
#endif
		if( SPES[i*6+0]!=0 )
		    break;
		if( (nDTS==0xFFFFFFFF) 
		 || (nDTS>lastDTS) 
		 || (lastDTS==0xFFFFFFFF) )
		{
		    spesCopy( filename[SPES[i*6+0]], 
			    SPES[i*6+1],	//	offset
			    SPES[i*6+2],	// size
			    pMerge );
		    fprintf( stdout, "%2d : %8X %5X %08X %08X : ",
			    (UINT)SPES[i*6+0],
			    (UINT)SPES[i*6+1],
			    (UINT)SPES[i*6+2],
			    (UINT)SPES[i*6+3],
			    (UINT)SPES[i*6+4]
			    );
		    if( diffDTS>0 )
			fprintf( stdout, "%5d\n", diffDTS );
		    else
			fprintf( stdout, "\n" );
		if( nDTS!=0xFFFFFFFF )
		    lastDTS = nDTS;
		}
	    }
	    fclose( pMerge );
	} else {
	    SpesParse( filename[0], diffFile, nSeek, mode, outMode, 0,
		    bNoPTS, bDetail, frames, SPES );
	}
	return 0;
}

