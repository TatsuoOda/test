
/*
	parseLib.cpp
		2012.8.22 by T.Oda
		2013.11.11 g_addr -> long long
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>	// memcpy

//unsigned int g_addr=0;
long long g_addr=0;
int bDebugGol=0;
int bDebugGolB=0;
int bShowGolomb=0;		// Dump Golomb parse
extern int bDebug;

void EXIT( )
{
	fprintf( stdout, "EXITing(0x%llX)\n", g_addr );
	exit( 1 );
}

int gread( unsigned char *buffer, int size1, int size2, FILE *fp )
{
#if 0
	fprintf( stdout, "gread(%d,%d:from:%X)\n", size1, size2, g_addr );
#endif
int readed = fread( buffer, size1, size2, fp );
	if( readed<size2 )
	{
	    fprintf( stdout, "readed(%d/%d)\n", readed, size2 );
	    if( readed<1 )
		fprintf( stdout, "EOF(0x%llX)\n", g_addr );
	}

	g_addr += readed*size1;
	return readed;
}

int Gread( unsigned char *buffer, int size1, int size2, FILE *fp,
	int *pTotal1, int *pTotal2)
{
	int readed = gread( buffer, size1, size2, fp );
	if( readed<size2 )
	    EXIT( );
	if( pTotal1 )
	    *pTotal1 = *pTotal1 + size1*size2;
	if( pTotal2 )
	    *pTotal2 = *pTotal2 + size1*size2;
	return readed;
}

unsigned int Short( unsigned char *buffer )
{
unsigned short size=0;
	size = (buffer[0]<< 8)
	     | (buffer[1]<< 0);
	return size;
}
unsigned int Long( unsigned char *buffer )
{
unsigned int size=0;
	size = (buffer[0]<<24)
	     | (buffer[1]<<16)
	     | (buffer[2]<< 8)
	     | (buffer[3]<< 0);
	return size;
}
long long LongLong( unsigned char *buffer )
{
long long size=0;
unsigned int sizeL, sizeH;
	sizeH= (buffer[0]<<24)
	     | (buffer[1]<<16)
	     | (buffer[2]<< 8)
	     | (buffer[3]<< 0);
	sizeL= (buffer[4]<<24)
	     | (buffer[5]<<16)
	     | (buffer[6]<< 8)
	     | (buffer[7]<< 0);
	size = (((long long)sizeH)<<32)
	     | (((long long)sizeL)<< 0);
	return size;
}

unsigned int Short_little( unsigned char *buffer )
{
unsigned short size=0;
	size = (buffer[1]<< 8)
	     | (buffer[0]<< 0);
	return size;
}
unsigned int Long_little( unsigned char *buffer )
{
unsigned int size=0;
	size = (buffer[3]<<24)
	     | (buffer[2]<<16)
	     | (buffer[1]<< 8)
	     | (buffer[0]<< 0);
	return size;
}

int Copy( FILE *out, FILE *in, int size )
{
int total=0;
int sz;
char buf[1024*64];
int readed, written;

	while( total<size )
	{
	    sz = size-total;
	    if( sz>(1024*64) )
	    	sz = 1024*64;
	    readed = fread( buf, 1, sz, in );
	    if( readed<1 )
	    {
	    	fprintf( stdout, "Can't read 0x%X bytes (0x%X/0x%X)\n",
			sz, total, size );
		exit( 1 );
		break;
	    }
	    written = fwrite( buf, 1, readed, out );
	    if( written<1 )
	    {
	    	fprintf( stdout, "Can't write\n" );
		break;
	    }
	    total+=written;
	}
	return 0;
}

// -----------------------------------------------------------------

#define BIT_BUFFER_SIZE 1024
static unsigned char BitBuffer[BIT_BUFFER_SIZE];
static unsigned int BitAddr = 0;
static int BitOffset=0;
static int BitReaded=0;
static int BitSkip  =0;
static int nZero=0;

void InitBitBuffer( )
{
	BitAddr  = g_addr;
	BitOffset= 0;
	BitReaded= 0;
	BitSkip  = 0;
	nZero    = 0;
}
int GetBitFromBuffer( unsigned char *buffer, int *pBufOffset, int nBit )
{
static unsigned char BitMask[] = {
0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01
};
	int bufOffset = *pBufOffset;
	if( bDebug )
	fprintf( stdout, "GetBitFromBuffer(%d)\n", nBit );
	int nPos = BitOffset/8;
	if( (BitReaded/8)>=BIT_BUFFER_SIZE )
	{
	    fprintf( stdout, "BitBuffer full (%d)\n", BitReaded );
	    fflush( stdout );
	    EXIT();
	}
	if( (BitOffset+nBit)>BitReaded )
	{
	    int ii;
	    int nByte = BitOffset+nBit-BitReaded;
	    nByte = (nByte+7)/8;
	    if( bDebug )
	    fprintf( stdout, "Requested %d bytes (%d)\n", 
	    	nByte, BitReaded );
	    memcpy( &BitBuffer[BitReaded/8], &buffer[bufOffset], nByte );
	    bufOffset+=nByte;
	    for( ii=0; ii<nByte; ii++ )
	    {
	    	if( nZero==2 )
		{
		    if( BitBuffer[BitReaded/8+ii]==3 )
		    {
		    	// 00 00 03
			if( bDebug )
			    fprintf( stdout, "Found(00 00 03)@0x%X+%d/8\n",
				BitAddr, BitOffset );
			BitSkip++;
			int jj;
			for( jj=ii; jj<nByte; jj++ )
			{
			    BitBuffer[BitReaded/8+jj] 
			    	= BitBuffer[BitReaded/8+jj+1];
			}
			memcpy( &BitBuffer[BitReaded/8+nByte-1], 
			    &buffer[bufOffset], 1 );
			bufOffset+=1;
			nZero=0;
//			break;	// comment out 2012.8.23
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
	*pBufOffset = bufOffset;

	return data;
}

#define MAX_BASE	30
static int base[MAX_BASE];
int GetGolombFromBuffer( unsigned char *buffer, int *pBufOffset )
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
	bDebugGolB = 0;
	for( n=0; n<MAX_BASE; n++ )
	{
	    nBit = GetBitFromBuffer( buffer, pBufOffset, 1 );
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
	    nBit = GetBitFromBuffer( buffer, pBufOffset, 1 );
	    if( bShowGolomb )
	    fprintf( stdout, "%d ", nBit );
	    bValue = (bValue<<1) | nBit;
	}
	bDebugGolB = bDebugGol;
	value = base[n] + bValue;
	if( bShowGolomb )
	fprintf( stdout, " : value=%d (%X,%d=%X,%d)\n", 
		value, BitAddr, BitOffset, BitAddr+BitOffset/8, BitOffset%8 );
	return value;
}

