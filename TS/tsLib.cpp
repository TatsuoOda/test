
/*
	tsLib.cpp
		2011.10.31 by T.Oda

		2012.9.6 SrcAddr() add type parameter
		2012.9.7 PES_header
		2013.7.26 SrcAddr() check fixed
			  g_DTSL/H,g_PTSL/H : int -> unsigned int
		2013.12.12 TS() : Check constant bits
		2013.12.19 add crc32()
*/

#include <stdio.h>
#include <string.h>	// strlen
#include <stdlib.h>	// free

#include "tsLib.h"
#include "parseLib.h"

typedef unsigned int UINT;

int bDebugPES=0;
int bDebugUpdate=0;
int bDebugUpdateTS=0;

unsigned int g_DTSL = 0xFFFFFFFF;
unsigned int g_DTSH = 0xFFFFFFFF;
unsigned int g_PTSL = 0xFFFFFFFF;
unsigned int g_PTSH = 0xFFFFFFFF;

int PtsOffset=INVALID_OFFSET;
int DtsOffset=INVALID_OFFSET;
unsigned int fromPts=INVALID_OFFSET;
unsigned int toPts=INVALID_OFFSET;
unsigned int fromDts=INVALID_OFFSET;
unsigned int toDts=INVALID_OFFSET;

#if 0
unsigned int g_update_addr[MAX_UPDATE];
unsigned int g_update_bits[MAX_UPDATE];
unsigned int g_update_size[MAX_UPDATE];
unsigned int g_update_data[MAX_UPDATE];
#else
unsigned int *g_update_addr = NULL;
unsigned int *g_update_bits = NULL;
unsigned int *g_update_size = NULL;
unsigned int *g_update_data = NULL;
#endif
int g_update_count=0;

#if 0
long long TsAddr [ADDR_MAX];
long long PesAddr[ADDR_MAX];
#else
unsigned long long *TsAddr0=NULL;
unsigned long long *TsAddr =NULL;
unsigned long long *PesAddr=NULL;
#endif

// ----------------------------------------------------

int TS( unsigned int  High, unsigned int Low, 
	unsigned int *pTSH, unsigned int *pTSL )
{
unsigned long TS32_30, TS29_15, TS14_00;
unsigned long TS32, TS31_00;
int const1 = High & 0xF1;
int const2 = Low  & 0x00010001;
	TS32_30 = (High>>1) & 0x0007;
	TS29_15 = (Low>>17) & 0x7FFF;
	TS14_00 = (Low>> 1) & 0x7FFF;
	TS32 = TS32_30>>2;
	TS31_00 = (TS32_30<<30) | (TS29_15<<15) | (TS14_00);
	*pTSH = TS32;
	*pTSL = TS31_00;
	if( ((const1!=0x31) && (const1!=0x11) && (const1!=0x21) )
	 || (const2 !=0x00010001) )
	{
	    fprintf( stdout, "PTS/DTS Error(%02X:%08X,%02X:%08X)\n",
		    High, Low, const1, const2 );
	}
	return TS31_00;
}

int initTsAddr( )
{
	if( TsAddr )
	    free( TsAddr );
	if( TsAddr0 )
	    free( TsAddr0 );
	if( PesAddr )
	    free( PesAddr );
	TsAddr0 = (unsigned long long *)calloc( 8, ADDR_MAX );
	if( TsAddr0==NULL )
	{
	    fprintf( stdout, "Can't alloc TsAddr0\n" );
	    exit( 1 );
	}
	TsAddr = (unsigned long long *)calloc( 8, ADDR_MAX );
	if( TsAddr==NULL )
	{
	    fprintf( stdout, "Can't alloc TsAddr\n" );
	    exit( 1 );
	}
	PesAddr = (unsigned long long *)calloc( 8, ADDR_MAX );
	if( PesAddr==NULL )
	{
	    fprintf( stdout, "Can't alloc PesAddr\n" );
	    exit( 1 );
	}
	memset( PesAddr, 0xFF, 8*ADDR_MAX );
	return 0;
}

static int L_index=0;
static int L_addr=0;

// type=1 : ts Packet address
// type=0 : address
//long long SrcAddr( int addr, int type )
long long SrcAddr( long long addr, int type )
{
int i;
long long PesAddr0 = 0 ;
long long tsAddr0 = 0;
long long tsAddrX = 0;
long long addrL = addr;
//	fprintf( stdout, "SrcAddr(%X)\n", addr );
	if( PesAddr==NULL )
	{
	    fprintf( stdout, "PesAddr=NULL\n" );
	    return 0xFFFFFFFF;
	}
	long long pesAddr = PesAddr[0];
//	if( pesAddr==0xFFFFFFFF )
	if( pesAddr<0 )
	    return 0xFFFFFFFF;
	int i_start=0;
	if( addr>L_addr )
	{
//	    i_start = L_index;
	    i_start = L_index-1;
	    if( i_start<0 )
	    	i_start=0;
	}
	L_addr = addr;
	for( i=i_start; i<ADDR_MAX; i++ )
	{
	    pesAddr =PesAddr[i];
	    long long tsAddr  =TsAddr [i];
	    long long tsAddr00=TsAddr0[i];
#if 0
	    fprintf( stdout, 
		"i=%d : pesAddr=%llX, tsAddr=%llX, tsAddr00=%llX\n",
		i, pesAddr, tsAddr, tsAddr00 );
#endif
	    L_index = i;
	    if( pesAddr==0xFFFFFFFF )
	    {
	    	if( type==0 )
		{
		    long long retAddr=tsAddrX + (addrL-PesAddr0);
//if( (retAddr<0) || (retAddr>0x7FFFFFFF) )
if( (retAddr<0)  )
{
	fprintf( stdout, "tsAddrX  =%llX\n", tsAddrX );
	fprintf( stdout, "addrL    =%llX\n", addrL );
	fprintf( stdout, "PesAddr0 =%llX\n", PesAddr0 );
	fprintf( stdout, "retAddr  =%llX\n", retAddr );
	fprintf( stdout, "tsAddr[%d]=%llX\n", i-2, TsAddr[i-2] );
	fprintf( stdout, "tsAddr[%d]=%llX\n", i-1, TsAddr[i-1] );
	fprintf( stdout, "tsAddr[%d]=%llX\n", i, TsAddr[i] );
	exit( 1 );
}
		    return retAddr;
		} else {
		    return tsAddr0;
		}
	    }
//	    fprintf( stdout, "%4d:%8X:%8X,%8X\n", i, addr, pesAddr, tsAddr  );
	    if( addrL<pesAddr )
	    {
	    	if( type==0 )
		{
		    long long retAddr = tsAddrX + (addrL-PesAddr0);
#if 0			
		    fprintf( stdout, "addr=%X, pesAddr=%llX\n",
		    	addr, pesAddr );
		    fprintf( stdout, "tsAddrX=%llX, PesAddr0=%llX\n",
		    	tsAddrX, PesAddr0 );
//		    return tsAddrX + (addr-PesAddr0);
#endif
		    return retAddr;
		} else {
		    return tsAddr0;
		}
	    }
	    PesAddr0 = pesAddr;
	    tsAddrX  = tsAddr ;
	    tsAddr0  = tsAddr00;
#if 0
	    fprintf( stdout, "PesAddr0=%X, tsAddrX=%X, tsAddr0=%X\n",
	    	pesAddr, tsAddr, tsAddr00 );
#endif
	}
	return 0xFFFFFFFF;
}

static char AddrStrData[80];

char *AddrStr( unsigned int addr )
{
unsigned int SrcA = SrcAddr(addr,0);

    if( SrcA==0xFFFFFFFF )
	sprintf( AddrStrData, "0x%08X", addr );
    else
	sprintf( AddrStrData, "0x%X,0x%X", addr, SrcA );
    return AddrStrData;
}

char *AddrStr2( unsigned int addr )
{
unsigned int SrcA = SrcAddr(addr,0);
unsigned int SrcB = SrcAddr(addr,1);

    if( SrcA==0xFFFFFFFF )
	sprintf( AddrStrData, "0x%X", addr );
    else
	sprintf( AddrStrData, "0x%X,0x%X:0x%X", addr, SrcA, SrcB );
    return AddrStrData;
}


static char TsStrData[80];
static unsigned int L_DTSL=0xFFFFFFFF;
char *TsStr( )
{
    int diff = g_DTSL-L_DTSL;
//    if( (L_DTSL!=0xFFFFFFFF) && ((g_DTSL-L_DTSL>0) && (g_DTSL-L_DTSL<10000)) )
    if( (L_DTSL!=0xFFFFFFFFu) && (g_DTSL>L_DTSL) && (diff<10000) )
    {
	sprintf( TsStrData, "PTS=%8X,DTS=%8X(%4d)", 
		g_PTSL, g_DTSL, diff );
    } else {
	sprintf( TsStrData, "PTS=%8X,DTS=%8X", g_PTSL, g_DTSL );
    }
    L_DTSL = g_DTSL;
    return TsStrData;
}

static char DtsTimeData[80];
char *DtsTimeStr( )
{
	if( g_DTSH==0xFFFFFFFF )
	{
	    sprintf( DtsTimeData, "---:--:--.---" );
	} else {
	    int hour, min, sec, msec;
	    TcTime32( g_DTSH, g_DTSL, &hour, &min, &sec, &msec );
	    sprintf( DtsTimeData, "%3d:%02d:%02d.%03d", hour, min, sec, msec );
	}
	return DtsTimeData;
}

void CannotRead( char *str )
{
	if( str!=NULL )
	    fprintf( stdout, "Can't read %s@(%s)\n", str, AddrStr(g_addr) );
	else
	    fprintf( stdout, "Can't read @(%s)\n", AddrStr(g_addr) );
}

int  ReadAddrFile( char filename[], 
	char srcFilename[],
	unsigned long long TsAddr[], 
	unsigned long long PesAddr[],
	int *nAddr, int MaxAddr )
{
UINT ui;
char TsBuffer[1024];
int nTS = *nAddr;
	srcFilename[0] = 0;
	if( TsAddr==NULL )
	{
	    fprintf( stdout, "TsAddr=NULL\n" );
	    exit( 1 );
	}
	fprintf( stdout, "Read %s (%d)\n", filename, nTS );
	fflush( stdout );
	FILE *addr_fp = fopen( filename, "r" );
	if( addr_fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    return -1;
	}
	if( fgets( srcFilename, 1023, addr_fp )==NULL )
	{
	    fprintf( stdout, "Can't get srcFilename\n" );
	    return -1;
	}
	for( ui=0; ui<strlen(srcFilename); ui++ )
	{
	    if( srcFilename[ui]=='\n' )
		srcFilename[ui] = 0;
	    if( srcFilename[ui]=='\r' )
		srcFilename[ui] = 0;
	}
	fprintf( stdout, "srcFilename = %s\n", srcFilename );
	fflush( stdout );
//	fprintf( stdout, "TsAddr=%llX, PesAddr=%llX\n", TsAddr, PesAddr );
	while( 1 )
	{
	    if( fgets( TsBuffer, 1024, addr_fp )==NULL )
	    {
	    	fprintf( stdout, "EOF\n" );
		break;
	    }
#if 0
	    fprintf( stdout, "%s", TsBuffer );
	    fflush( stdout );
#endif
	    long long ts_addr_top, ts_addr, pes_addr;
	    int size, pcrH, pcrL;
	    unsigned long long PCR;
//	    if( TsBuffer[0]!=' ' )
	    int len = strlen( TsBuffer );
	    // len = 75
/*
	    fprintf( stdout, "len==%d\n", len );
	    fprintf( stdout, "TsBuffer[11]=%02X\n", TsBuffer[11] );
	    fprintf( stdout, "TsBuffer[12]=%02X\n", TsBuffer[12] );
*/
	    if( (TsBuffer[10]!=',')
	     || (TsBuffer[11]!=' ')
	     || (len<70)
	    ) {
	    	fprintf( stdout, "Invalid AddrFile(%s)\n", filename );
		fprintf( stdout, "TsBuffer=%s", TsBuffer );
		fprintf( stdout, "TsBuffer[10]=%c[%02X]\n", 
			TsBuffer[10], TsBuffer[10] );
		fprintf( stdout, "TsBuffer[11]=%c[%02X]\n", 
			TsBuffer[11], TsBuffer[11] );
		fprintf( stdout, "len=%d\n", len );
		return -1;
	    }
	    sscanf( TsBuffer, "%llX, %llX, %llX, %X, %X %X %llX",
	    	&ts_addr_top, &ts_addr, &pes_addr, &size, &pcrH, &pcrL, &PCR );
	    if(PesAddr==NULL )
	    {
		TsAddr [nTS] = ts_addr_top;
		TsAddr0[nTS] = ts_addr_top;
	    } else {
		TsAddr0[nTS] = ts_addr_top;
		TsAddr [nTS] = ts_addr;
		PesAddr[nTS] = pes_addr;
	    }
#if 0
	    fprintf( stdout, "%8llX,%8llX,%8llX\n", 
	    	TsAddr0[nTS],
	    	TsAddr [nTS],
	    	PesAddr[nTS] );
#endif
	    nTS++;
	    if( nTS>=MaxAddr )
	    {
	    	fprintf( stdout, "Too many TS (%d)\n", nTS );
		return -1;
	    }
#if 1
	    TsAddr [nTS] = 0xFFFFFFFF;
	    TsAddr0[nTS] = 0xFFFFFFFF;
	    if( PesAddr!=NULL )
		PesAddr[nTS] = 0xFFFFFFFF;
#endif
	}
	fclose( addr_fp );
	fprintf( stdout, "Read done\n" );
	fflush( stdout );
	*nAddr = nTS;
	return 0;
}

#if 0
int TcTime24( unsigned int vCountH, unsigned int vCountL,
	int *hour, int *min, int *sec, int *msec )
{
int Error=0;
unsigned long long llCount;
	llCount = vCountH & 0x3FFFFFFF;
	llCount = (llCount<<24) | vCountL;
	*msec = llCount/90;
	*sec  = *msec/1000;
	*min  = (*sec/60)%60;
	*hour = (*sec/60/60);

	*msec = *msec % 1000;
	*sec  = *sec  %   60;
	return Error;
}

int TcTime32( unsigned int vCountH, unsigned int vCountL,
	int *hour, int *min, int *sec, int *msec )
{
int Error=0;
unsigned long long llCount;
	llCount = vCountH & 0x3FFFFFFF;
	llCount = (llCount<<32) | vCountL;
	*msec = llCount/90;
	*sec  = *msec/1000;
	*min  = (*sec/60)%60;
	*hour = (*sec/60/60);

	*msec = *msec % 1000;
	*sec  = *sec  %   60;
	return Error;
}
#else
int TcTime24( unsigned int vCountH, unsigned int vCountL,
	int *hour, int *min, int *sec, int *msec )
{
int Error=0;
unsigned long long llCount;
	llCount = vCountH & 0x3FFFFFFF;
	llCount = (llCount<<24) | vCountL;
	*msec = llCount/90;
	*sec  = *msec/1000;
	*min  = (*sec/60)%60;
	*hour = (*sec/60/60);

	*msec = *msec % 1000;
	*sec  = *sec  %   60;
	if( *hour<0 )
	{
	    fprintf( stdout, "TcTime24(%8X,%8X) %9LX : %3d:%02d:%02d",
		    vCountH, vCountL, llCount, *hour, *min, *sec );
	    *hour = 99;
	    *min  = 99;
	    *sec  = 99;
	    *msec = 999;
	    Error=1;
	}
	return Error;
}
int TcTime32( unsigned int vCountH, unsigned int vCountL,
	int *hour, int *min, int *sec, int *msec )
{
int Error=0;
unsigned long long llCount;
	llCount = vCountH & 0x3FFFFFFF;
	llCount = (llCount<<32) | vCountL;
	*msec = llCount/90;
	*sec  = *msec/1000;
	*min  = (*sec/60)%60;
	*hour = (*sec/60/60);

	*msec = *msec % 1000;
	*sec  = *sec  %   60;
	return Error;
}
#endif

int DoUpdate( int addr, int bitOffset, int size, int value )
{
#if 1
	fprintf( stdout, "DoUpdate(0x%X,%d,%d,%d)\n",
		addr, bitOffset, size, value );
#endif
	if( g_update_addr==NULL )
	{
	    fprintf( stdout, "g_update_addr==NULL\n" );
	    EXIT();
	}
	if( g_update_count>=MAX_UPDATE )
	{
	    fprintf( stdout, "g_update_count is too large (%d>%d)\n",
		    g_update_count, MAX_UPDATE );
	    EXIT();
	}
	g_update_addr[g_update_count] = addr;
	g_update_bits[g_update_count] = bitOffset;
	g_update_size[g_update_count] = size;
	g_update_data[g_update_count] = value;
	if( bDebugUpdate )
	{
	    fprintf( stdout, "update Addr=%X(%X),bit=%2d,size=%2d,0x%02X\n",
		(UINT)g_update_addr[g_update_count],
		(UINT)SrcAddr(g_update_addr[g_update_count],0),
		(int )g_update_bits[g_update_count],
		(int )g_update_size[g_update_count],
		(UINT)g_update_data[g_update_count] );
	}
	g_update_count++;
	return 0;
}

int ShowUpdate( )
{
    if( g_update_addr==NULL )
    {
	fprintf( stdout, "g_update_addr==NULL\n" );
	EXIT();
    }
    if( g_update_addr[0]==0xFFFFFFFF )
    {
	fprintf( stdout, "No g_update\n" );
	return 0;
    } else {
	fprintf( stdout, "addr=%X\n", g_update_addr[0] );
	fprintf( stdout, "bits=%X\n", g_update_bits[0] );
	fprintf( stdout, "size=%X\n", g_update_size[0] );
	fprintf( stdout, "data=%X\n", g_update_data[0] );
	return 1;
    }
}

int UpdateTS( int stAddr, unsigned int TSH, unsigned int TSL, 
	int modeTS, int bPTS, int bDTS, int TSoffset )
{
int code=0;
int nTS[5];
	if( modeTS==0 )
	    fprintf( stdout, "UpdatePTS(%02X:%08X,%d)\n", TSH, TSL, TSoffset );
	else
	    fprintf( stdout, "UpdateDTS(%02X:%08X,%d)\n", TSH, TSL, TSoffset );
	long long TSLL = (((long long)TSH)<<32)
		       | (((long long)TSL)<< 0);
	long long nTSLL = TSLL + TSoffset;
	if( bDebugUpdateTS )
	    fprintf( stdout, "TSLL=%08X,%08X -> %08X,%08X\n", 
		(int)( TSLL>>32), (int) TSLL,
		(int)(nTSLL>>32), (int)nTSLL );
	TSLL = nTSLL;
	switch( modeTS )
	{
	case MODE_PTS :
	    if( bDTS==0 )
	    	code = 2;
	    else
	    	code = 3;
	    break;
	case MODE_DTS :
	    if( bPTS==0 )
		code = 0;
	    else
		code = 1;
	    break;
	default :
	    fprintf( stdout, "Illegal\n" );
	    EXIT();
	}
	// 4,3,1
	nTS[0] = (code<<4) | (((TSLL>>30) & 7)<<1) | 1;
	nTS[1] =  ((TSLL>>22) & 0xFF);
	nTS[2] = (((TSLL>>15) & 0xFF)<<1) | 1;
	nTS[3] =  ((TSLL>> 7) & 0xFF);
	nTS[4] = (((TSLL>> 0) & 0xFF)<<1) | 1;
	DoUpdate( stAddr, 0, 8, nTS[0] );
	DoUpdate( stAddr, 8, 8, nTS[1] );
	DoUpdate( stAddr,16, 8, nTS[2] );
	DoUpdate( stAddr,24, 8, nTS[3] );
	DoUpdate( stAddr,32, 8, nTS[4] );
	int TS_H = (nTS[0]<< 0);
	int TS_L = (nTS[1]<<24)
		 | (nTS[2]<<16)
		 | (nTS[3]<< 8)
		 | (nTS[4]<< 0);
	unsigned int nTSH, nTSL;
	TS( TS_H, TS_L, &nTSH, &nTSL );
	if( bDebugUpdateTS )
	{
	    fprintf( stdout, "%02X,%08X\n", TS_H, TS_L );
	    fprintf( stdout, "new TS=%02X:%08X\n", nTSH, nTSL );
	}
	return 0;
}

int PES_header( FILE *fp, unsigned char buffer[], int bDisplay,
	unsigned int *pPTSH, unsigned int *pPTSL, 
	unsigned int *pDTSH, unsigned int *pDTSL )
{
int len;
int readed;
int offset=0;
	*pPTSH=0xFFFFFFFF;
	*pPTSL=0xFFFFFFFF;
	*pDTSH=0xFFFFFFFF;
	*pDTSL=0xFFFFFFFF;
	if( bDisplay>0 )
	{
	fprintf( stdout, "------------------------\n" );
	UINT addr = SrcAddr(g_addr,0);
	if( addr==0xFFFFFFFF )
	    fprintf( stdout, "PES_header(%s)\n", AddrStr(g_addr) );
	else
	    fprintf( stdout, "PES_header(%s)(0x%llX)\n", 
	    	AddrStr(g_addr), SrcAddr(g_addr,1) );
	}
	readed = gread( &buffer[0], 1, 2, fp );
	len = (buffer[0]<<8) | buffer[1];
	if( bDebugPES )
	fprintf( stdout, "PES_len=%8X\n", len );
	readed = gread( &buffer[2], 1, 3, fp );
	if( readed<3 )
	{
	    CannotRead( (char *)"PES header top" );
	    return -1;
	}
	int bPTS=0;
	int bDTS=0;
	int flag1=buffer[2];
	int flag2=buffer[3];
	int PES_header_data_length = buffer[4];
#if 0
	if( (flag1>>6)!=2 )
	    fprintf( stdout, "Invalid flag1 (%02X)\n", flag1 );
#endif
	bPTS = (flag2 & 0x80) ? 1 : 0;
	bDTS = (flag2 & 0x40) ? 1 : 0;
	flag1 = flag1 & 0x3F;
	flag2 = flag2 & 0x3F;
	if( bDebugPES )
	fprintf( stdout, 
	    "flag1=%02X:bPTS=%d,bDTS=%d:flag2=%02X, PES_head_len=%02X\n", 
	    flag1, bPTS, bDTS, flag2, PES_header_data_length );

	int head_addr = g_addr-offset;
	int stAddr, enAddr;
	offset=5;
	readed = gread( &buffer[offset], 1, PES_header_data_length, fp );
	if( readed<PES_header_data_length )
	{
	    CannotRead( (char *)"PES header data" );
	    return -1;
	}
	int PTS_H=(-1), PTS_L=(-1), DTS_H=(-1), DTS_L=(-1);
	if( bPTS )
	{
	    PTS_H = (buffer[offset+0]<< 0);
	    PTS_L = (buffer[offset+1]<<24)
		  | (buffer[offset+2]<<16)
		  | (buffer[offset+3]<< 8)
		  | (buffer[offset+4]<< 0);
	    if( bDisplay>1 )
	    fprintf( stdout, "PTS=%02X:%08X (%02X,%02X,%02X,%02X,%02X)\n",
		    PTS_H, PTS_L, 
		    buffer[offset+0], buffer[offset+1], 
		    buffer[offset+2], buffer[offset+3],
		    buffer[offset+4] );
	    TS( PTS_H, PTS_L, pPTSH, pPTSL );
	    if( bDisplay>1 )
		fprintf( stdout, "PTS=%02X:%08X\n", *pPTSH, *pPTSL );

	    stAddr = head_addr+offset-5;
	    enAddr = head_addr+offset+0;
	    if( bDisplay>0 )
	    {
	    	int hour, min, sec, msec;
		TcTime32( *pPTSH, *pPTSL, &hour, &min, &sec, &msec );
		fprintf( stdout, 
			"PTS(%8X,%8X)=%8X,%08X (%3d:%02d:%02d.%03d)\n", 
			stAddr, enAddr, *pPTSH, *pPTSL,
			hour, min, sec, msec );
	    }
	    if( PtsOffset!=INVALID_OFFSET )
	    {
	    	if( ((fromPts!=INVALID_OFFSET) && (toPts!=INVALID_OFFSET)
		  && (*pPTSL>=fromPts) && (*pPTSL<=toPts))

		  || (((fromPts!=INVALID_OFFSET) && (toPts==INVALID_OFFSET))
		  && (*pPTSL>=fromPts) )

		  || (((fromPts==INVALID_OFFSET) && (toPts!=INVALID_OFFSET))
		  && (*pPTSL<=toPts) )

		  || ((fromPts==INVALID_OFFSET) && (toPts==INVALID_OFFSET))
		)
		{
		UpdateTS( stAddr, *pPTSH, *pPTSL, MODE_PTS, 
			bPTS, bDTS, PtsOffset );
#if 0
		fprintf( stdout, "PtsUpdate(%8X,%8X)\n", PTSL, PtsOffset );
#endif
		}
	    }
	    offset+=5;
	} else {
#if 0
	    if( bDump )
		fprintf( stdout, "No PTS\n" );
#endif
	}
	if( bDTS )
	{
	    DTS_H = (buffer[offset+0]<< 0);
	    DTS_L = (buffer[offset+1]<<24)
		  | (buffer[offset+2]<<16)
		  | (buffer[offset+3]<< 8)
		  | (buffer[offset+4]<< 0);
	    if( bDisplay>1 )
		fprintf( stdout, "DTS=%02X:%08X (%02X,%02X,%02X,%02X,%02X)\n",
		    DTS_H, DTS_L, 
		    buffer[offset+0], buffer[offset+1], 
		    buffer[offset+2], buffer[offset+3],
		    buffer[offset+4] );
	    TS( DTS_H, DTS_L, pDTSH, pDTSL );
	    if( bDisplay>1 )
		fprintf( stdout, "DTS=%02X:%08X\n", *pDTSH, *pDTSL );

	    stAddr = head_addr+offset+0;
	    enAddr = head_addr+offset+5;
	    if( bDisplay>0 )
	    {
	    	int hour, min, sec, msec;
		TcTime32( *pDTSH, *pDTSL, &hour, &min, &sec, &msec );
		fprintf( stdout, 
			"DTS(%8X,%8X)=%8X,%08X (%3d:%02d:%02d.%03d)\n", 
			stAddr, enAddr, *pDTSH, *pDTSL, hour, min, sec, msec );
	    }
	    if( DtsOffset!=INVALID_OFFSET )
	    {
	    	if( ((fromDts!=INVALID_OFFSET) && (toDts!=INVALID_OFFSET)
		  && (*pDTSL>=fromDts) && (*pDTSL<=toDts))

		  || (((fromDts!=INVALID_OFFSET) && (toDts==INVALID_OFFSET))
		  && (*pDTSL>=fromDts) )

		  || (((fromDts==INVALID_OFFSET) && (toDts!=INVALID_OFFSET))
		  && (*pDTSL<=toDts) )

		  || ((fromDts==INVALID_OFFSET) && (toDts==INVALID_OFFSET))
		)
		UpdateTS( stAddr, *pDTSH, *pDTSL, MODE_DTS, 
			bPTS, bDTS, DtsOffset );
	    }
	} else {
#if 0
	    if( bDump )
		fprintf( stdout, "No DTS\n" );
#endif
	}
	if( bDisplay )
	{
	    if( bPTS & bDTS )
	    {
		fprintf( stdout, "PTSDTS=%02X:%08X,%02X:%08X diff(%5d)\n",
		    *pPTSH, *pPTSL, *pDTSH, *pDTSL, (*pPTSL-*pDTSL) );
	    } else if( bPTS )
	    {
		fprintf( stdout, "PTSDTS=%02X:%08X,--:-------- diff(%5d)\n",
		    *pPTSH, *pPTSL, 0 );
	    } else if( bDTS )
	    {
		fprintf( stdout, "PTSDTS=--:--------,%02X:%08X diff(  ?  )\n",
		    *pDTSH, *pDTSL );
	    }
	    if( bDisplay>0 )
		fprintf( stdout, "------------------------\n" );
	}
#if 0
	userdata_ptsh = *pPTSH;
	userdata_ptsl = *pPTSL;
#endif
	g_PTSL =  *pPTSL;
	g_PTSH =  *pPTSH;
	if( *pDTSH==0xFFFFFFFF )
	{
	    g_DTSL = *pPTSL;
	    g_DTSH = *pPTSH;
	} else {
	    g_DTSL = *pDTSL;
	    g_DTSH = *pDTSH;
	}

	return len;
}

// 2013.4.22 add parameter *pOffset
int parsePES_header( unsigned char buffer[], int bDisplay,
	unsigned int *pPTSH, unsigned int *pPTSL, 
	unsigned int *pDTSH, unsigned int *pDTSL, int *pOffset )
{
int len;
//int readed;
int offset=0;
	*pPTSH=0xFFFFFFFF;
	*pPTSL=0xFFFFFFFF;
	*pDTSH=0xFFFFFFFF;
	*pDTSL=0xFFFFFFFF;
	len = (buffer[0]<<8) | buffer[1];
	int bPTS=0;
	int bDTS=0;
	int flag1=buffer[2];
	int flag2=buffer[3];
//	int PES_header_data_length = buffer[4];
	bPTS = (flag2 & 0x80) ? 1 : 0;
	bDTS = (flag2 & 0x40) ? 1 : 0;
	flag1 = flag1 & 0x3F;
	flag2 = flag2 & 0x3F;
	offset=5;
//	readed = gread( &buffer[offset], 1, PES_header_data_length, fp );
	int PTS_H=(-1), PTS_L=(-1), DTS_H=(-1), DTS_L=(-1);
	if( bPTS )
	{
	    PTS_H = (buffer[offset+0]<< 0);
	    PTS_L = (buffer[offset+1]<<24)
		  | (buffer[offset+2]<<16)
		  | (buffer[offset+3]<< 8)
		  | (buffer[offset+4]<< 0);
	    if( bDisplay>1 )
	    fprintf( stdout, "PTS=%02X:%08X (%02X,%02X,%02X,%02X,%02X)\n",
		    PTS_H, PTS_L, 
		    buffer[offset+0], buffer[offset+1], 
		    buffer[offset+2], buffer[offset+3],
		    buffer[offset+4] );
	    TS( PTS_H, PTS_L, pPTSH, pPTSL );
	    if( bDisplay>1 )
		fprintf( stdout, "PTS=%02X:%08X\n", *pPTSH, *pPTSL );

	    if( bDisplay>0 )
	    {
	    	int hour, min, sec, msec;
		TcTime32( *pPTSH, *pPTSL, &hour, &min, &sec, &msec );
		fprintf( stdout, 
			"PTS=%8X,%08X (%3d:%02d:%02d.%03d)\n", 
			*pPTSH, *pPTSL, hour, min, sec, msec );
	    }
	    offset+=5;
	}
	if( bDTS )
	{
	    DTS_H = (buffer[offset+0]<< 0);
	    DTS_L = (buffer[offset+1]<<24)
		  | (buffer[offset+2]<<16)
		  | (buffer[offset+3]<< 8)
		  | (buffer[offset+4]<< 0);
	    if( bDisplay>1 )
		fprintf( stdout, "DTS=%02X:%08X (%02X,%02X,%02X,%02X,%02X)\n",
		    DTS_H, DTS_L, 
		    buffer[offset+0], buffer[offset+1], 
		    buffer[offset+2], buffer[offset+3],
		    buffer[offset+4] );
	    TS( DTS_H, DTS_L, pDTSH, pDTSL );
	    if( bDisplay>1 )
		fprintf( stdout, "DTS=%02X:%08X\n", *pDTSH, *pDTSL );

	    if( bDisplay>0 )
	    {
	    	int hour, min, sec, msec;
		TcTime32( *pDTSH, *pDTSL, &hour, &min, &sec, &msec );
		fprintf( stdout, 
			"DTS=%8X,%08X (%3d:%02d:%02d.%03d)\n", 
			*pDTSH, *pDTSL, hour, min, sec, msec );
	    }
	    offset+=5;
	}
	*pOffset = offset;

	return len;
}

// ---------------------
static unsigned int CRC32[256] = {
0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc,
0x17c56b6b, 0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f,
0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a,
0x384fbdbd, 0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75, 0x6a1936c8,
0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e,
0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84,
0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d, 0xd4326d90, 0xd0f37027,
0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022,
0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077,
0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c,
0x2e003dc5, 0x2ac12072, 0x128e9dcf, 0x164f8078, 0x1b0ca6a1,
0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb,
0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d,
0x40d816ba, 0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692, 0x8aad2b2f,
0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044,
0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050, 0xe9362689,
0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683,
0xd1799b34, 0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59,
0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c,
0x774bb0eb, 0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53, 0x251d3b9e,
0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48,
0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2,
0xe6ea3d65, 0xeba91bbc, 0xef68060b, 0xd727bbb6, 0xd3e6a601,
0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604,
0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6,
0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad,
0x81b02d74, 0x857130c3, 0x5d8a9099, 0x594b8d2e, 0x5408abf7,
0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd,
0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b,
0x0fdc1bec, 0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654, 0xc5a92679,
0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12,
0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676, 0xea23f0af,
0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5,
0x9e7d9662, 0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06,
0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03,
0xb1f740b4
};
unsigned int crc32( unsigned char* data, int length )
{
    unsigned int crc = 0xFFFFFFFF;
    while ( length-- > 0 ) 
    {
	crc = (crc<<8) ^ CRC32[((crc>>24) ^ (unsigned int)(*data++)) & 0xFF];
    }
    return crc;
}

