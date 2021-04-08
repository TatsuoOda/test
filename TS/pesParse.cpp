
/*
	pesParse.cpp
		2012.8.15 separate from pes.cpp
		2012.8.17 Use AddrSrc
		2012.8.23 GetBitStream() : processing "00 00 03" bug fixed
		2012.9.21 Delete userdata_ptsh/l
		2012.9.24 Merge shira source (frame_packing_arrangement info)
		2012.9.26 pes() : pes.cpp -> pesParse.cpp 
		2012.9.26 +O : bShowPOC
		2012.9.27 +S : bDumpSPS
			    pic_order_cnt_lsb
			    Support
				avc_log2_max_pic_order_cnt_lsb_minus4
				avc_log2_max_frame_num_minus4
			  +F : bFramePackingArrangement=1
			  	Parse PPS
		2012.10.3 malloc Packets
			  malloc g_update_addr_S/..
			  malloc g_update_addr/..
			  malloc BitBuffer
		2012.10.5 Parse PPS more
			  Parse Slice header more (ref_pic_list)
		2012.10.9 +s : bParseSlice=1
			  +e : bDebugSEI=1
			  bug fix : modification_of_pic_nums_idc
		2012.10.10 separate nal_ref_idc, nal_unit_type
			   check forbidden_zero_bit
		2012.11.12 user_data_registered/unregister should return 0
		2012.11.15 use bAlarmAVC, nAlarm : -M : bAlarmAVC=0
		2013.2.7   view_scalability_info()
				not parse internal
		2013.4.10  AVS
		2013.5. 9  =P : bDebugPES
		2013.6.17 =D : bDSS
		2013.7.17  Separate to pesAVC.cpp
		2013.7.29 +T : bTimeAnalyze
		2013.8. 9 Show correct dss stream address using StrAddr2()
		2013.12.5 ID=BD : AC3 in MPEG TS
		2014. 1.9 Use BitSkip in UpdateProcedure
 */

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// strlen

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>	// close

#include <sys/mman.h>	// mmap

#include "lib.h"
#include "tsLib.h"
#include "parseLib.h"
#include "pesParse.h"

#define BUF_SIZE	1024*4
//#define ADDR_MAX	1024*1024*16
#define MAX_UPDATE	1024*64

#define INVALID_OFFSET	0x12345678

//typedef unsigned int UINT;

// -------------------------------------------------------------
static int bUseAddrFile=1;
static int bShowUpdate=0;
extern int bDebugPES;

// -------------------------------------------------------------
extern int bDebug;
int bDump  = 0;
int bDumpSlice=0;
int bDisplayTS=0;
//int bDebugGol=0;
//int bDebugGolB=0;
//int bShowGolomb=0;		// Dump Golomb parse
int bShowSyntax=0;
int bSkipError=0;
int bShowDetail=0;		// AVC show NAL parse
//static int bShowSEI=0;
int bDebugSkip=0;		// Dump with skipContent()
int bShowBitAddr=0;		// Show BitAddr/Offset/Readed
int bDebugSEI=0;		// Debug SEI
int bShowNALinfo=0;		// CountNAL message
int bDSS=0;
int bUserData=0;
int bShowMvcExtention=0;
int bShowMvcScalable=0;
static int bDumpSPS=0;
//static int bParsePOC=1;
static int bParseSlice=0;
static int bAlarmAVC=1;
int bTimeAnalyze=0;

int bDebugRemux=0;
static int bShowPOC=0;
static int bFramePackingArrangement=0;

static int DSS_PCT=(-1);
static char DSS_Addr[80];

int validStart = (-1);
int validEnd   = (-1);

#define EDIT_NONE	0
#define EDIT_CUT	1
#define EDIT_INS	2
//static int bEditSPS=EDIT_NONE;

extern int bRemovePicStruct;
extern int nEditFrameMbs;
// make timing_info_present_flag=0 and remove timing_info
extern int bRemoveTimingInfo;	

void init_AVC();

#if 0
static int g_DTSL = (-1);
static int g_DTSH = (-1);
static int g_PTSL = (-1);
static int g_PTSH = (-1);
extern int g_DTSL;
extern int g_DTSH;
extern int g_PTSL;
extern int g_PTSH;
#endif

static int es_addr =  0;

int SpecialStartAddr=0;
int SpecialStartBits=0;
int SpecialEndAddr=0;
int SpecialEndBits=0;

int nSequence=0;
int nSequenceExtension=0;
int nSequenceDisplayExtension=0;
int bDumpSequenceDisplayExtension = 0;
int bDumpSequenceExtension = 0;
int bDumpSequence = 0;

#define MAX_PACKETS 1024*64
//int Packets[MAX_PACKETS+4];	// 16bit
int *Packets=NULL;

unsigned int *g_update_addrS = NULL;
unsigned int *g_update_bit_S = NULL;	// remove start
unsigned int *g_update_bit_E = NULL;	// remove end
unsigned int *g_update_bit_T = NULL;	// structure end : つめる
unsigned int *g_update_mode  = NULL;

int g_update_countS=0;

// ---------------------------------------------
// header item
//
int items[ID_MAX];
int value[ID_MAX];

// BitAddr
//#define BIT_BUFFER_SIZE	1024*64
#define BIT_BUFFER_SIZE	1024*1024*4

//static unsigned char BitBuffer[BIT_BUFFER_SIZE];
static unsigned char *BitBuffer = NULL;
static unsigned int BitAddr = 0;
static int BitOffset=0;
static int BitReaded=0;
static int ByteSkip  =0;
static int nZero=0;
static int b_user_data=0;
static unsigned int picture_coding_type=0;

int GetBitAddr()
{
	return BitAddr;
}
int GetBitOffset()
{
	return BitOffset;
}
int GetByteSkip()
{
	return ByteSkip;
}

void ShowBitAddr( )
{
	if( bShowBitAddr )
	{
	    int target = BitAddr+BitOffset/8;
	    fprintf( stdout, "Target   =0x%X(0x%X) %dbit\n", 
	    	(UINT)target, (UINT)SrcAddr(target,0), BitOffset%8 );

	    fprintf( stdout, "BitAddr  =0x%X(0x%X)\n", 
	    	(UINT)BitAddr, (UINT)SrcAddr(BitAddr,0) );
	    fprintf( stdout, "BitOffset=%d\n", BitOffset );
	    fprintf( stdout, "BitReaded=%d\n", BitReaded );
	}
}

void DumpBitBuffer( )
{
int j;
	if( BitBuffer )
	{
	    for( j=0; j<BitReaded/8; j++ )
	    {
		fprintf( stdout, "%02X ", BitBuffer[j] );
#if 0
		if( ((i+j)&15)==15 )
#else
		if( (j&15)==15 )
#endif
		    fprintf( stdout, "\n" );
	    }
	}
}

void InitBitStream( )
{
//	fprintf( stdout, "InitBitStream(0x%llX)\n", g_addr );
	BitAddr  = g_addr;
	BitOffset= 0;
	BitReaded= 0;
	ByteSkip  = 0;
	nZero    = 0;
}
int GetBitStream( FILE *fp, int nBit )
{
static unsigned char BitMask[] = {
0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01
};
	if( BitBuffer==NULL )
	{
	    BitBuffer = (unsigned char *)malloc( BIT_BUFFER_SIZE );
	    if( BitBuffer==NULL )
	    {
	    	fprintf( stdout, "Can't malloc BitBuffer\n" );
		exit( 1 );
	    }
	}
	if( bDebug )
	fprintf( stdout, "GetBitStream(%d)\n", nBit );
	int nPos = BitOffset/8;
	if( (BitReaded/8)>=BIT_BUFFER_SIZE )
	{
	    fprintf( stdout, "BitBuffer full (%d)\n", BitReaded );
	    EXIT();
	}
	if( (BitOffset+nBit)>BitReaded )
	{	// たりないのでよむ
	    int ii;
	    int nByte = BitOffset+nBit-BitReaded;
	    nByte = (nByte+7)/8;
	    if( bDebug )
	    fprintf( stdout, "Requested %d bytes (%d)\n", 
	    	nByte, BitReaded );
	    int readed = gread( &BitBuffer[BitReaded/8], nByte, 1, fp );
	    if( readed<1 )
	    {
		CannotRead( NULL );
		return -1;
	    }
	    for( ii=0; ii<nByte; ii++ )
	    {
#if 0
		fprintf( stdout, "%8X,nZero=%d [%02X]\n", 
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
			ByteSkip++;
			int jj;
			for( jj=ii; jj<nByte; jj++ )
			{
			    BitBuffer[BitReaded/8+jj] 
			    	= BitBuffer[BitReaded/8+jj+1];
			}
			readed = gread( &BitBuffer[BitReaded/8+nByte-1],
				1, 1, fp );
			if( readed<1 )
			{
			    CannotRead( NULL );
			    return -1;
			}
			nZero=0;
//			if( BitBuffer[BitReaded/8+nByte-1]==0 ) // 2012.5.7
//			if( BitBuffer[BitReaded/8+ii]==0 ) // 2012.8.23
//			    nZero++;	// 2012.5.7
#if 0
for( iii=0;iii<nByte;iii++ )
{
    fprintf( stdout, "%02X ", BitBuffer[BitReaded/8+iii] );
}
fprintf( stdout, "\n" );
#endif
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
	    fprintf( stdout, "%d bits readed\n", readed*nByte*8 );
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
	fprintf( stdout, 
"BitAddr=%X(%X), BitOffset=%d, BitReaded=%d, ByteSkip=%d, data=0x%X(0x%X)\n",
	    BitAddr, (UINT)SrcAddr(BitAddr,0), 
	    BitOffset, BitReaded, ByteSkip, data, data8 );
	return data;
}

// --------------------------------------------------
// Exp-Golomb codes
//#define MAX_BASE	26
#define MAX_BASE	30
static int base[MAX_BASE];
int GetBitStreamGolomb( FILE *fp )
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
#if 0
	base[0] = 0;
#else
	if( base[1]==0 )
	{
	    for( n=1; n<MAX_BASE; n++ )
	    {
		base[n] = base[n-1]*2+1;
	    }
	}
#endif
	bDebugGolB = 0;
	for( n=0; n<MAX_BASE; n++ )
	{
	    nBit = GetBitStream( fp, 1 );
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
	    nBit = GetBitStream( fp, 1 );
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

#if 0
int GolombNext( int cVal, int step )
{
int base[MAX_BASE];	// 0, 1, 3, 7, 15, 31, 
int value = 0;
int n, i;
	base[0] = 0;
	for( n=1; n<MAX_BASE; n++ )
	{
	    base[n] = base[n-1]*2+1;
//	    fprintf( stdout, "base[%d]=%d, cVal=%d\n", n, base[n], cVal );
	    if( cVal<base[n] )
	    	break;
	}
	if( n>=MAX_BASE )
	    return cVal;
	value = cVal;
	if( step>=0 )
	{
	    for( i=0; i<step; i++ )
	    {
		value=value+1;
		if( value>=base[n] )
		    value = base[n-1];
	    }
	} else {
	    for( i=0; i<-step; i++ )
	    {
		value=value-1;
		if( value<=base[n-1] )
		    value = base[n]-1;
	    }
	}
//	fprintf( stdout, "GolombNext(%d) = %d\n", cVal, value );
	return value;
}

int GolombValue( int cVal )
{
int base[MAX_BASE+1];	// 0, 1, 3, 7, 15, 31, 
int value =0;
int n;
	base[0] = 0;
	for( n=0; n<MAX_BASE; n++ )
	{
	    base[n+1] = base[n]*2+1;
	    if( cVal<base[n+1] )
	    	break;
	}
	if( n>=MAX_BASE )
	{
	    fprintf( stdout, "Can't convert GolmbValue(%d)\n", cVal );
	    EXIT();
	}
	value = (1<<n) + (cVal-base[n]);
//	fprintf( stdout, "GolombValue(%d) = 0x%X\n", cVal, value );
	return value;
}
#endif

int UpdateProcedure( int nCycle, int id, int size )
{
	if( nSelSt>=0 )
	{
#if 1
	    if( items[id]>0 )
	    fprintf( stdout, 
		"UpdateProcedure() : nSelSt=%d, nCycle=%d, items[%d]=%d\n",
		nSelSt, nCycle, id, items[id] );
#endif
	    if( (nCycle>=nSelSt) && ((nSelEn<0) || (nCycle<=nSelEn)) )
	    if( items[id]>0 )
	    {
//	    	DoUpdate( BitAddr, BitOffset, size, value[id] );
	    	DoUpdate( BitAddr+ByteSkip, BitOffset, size, value[id] );
		return 1;
	    }
	}
	return 0;
}

int UpdateProcedureGolomb( int nCycle, int id, int size, int nValue )
{
	if( nSelSt>=0 )
	{
	    if( (nCycle>=nSelSt) && ((nSelEn<0) || (nCycle<=nSelEn)) )
	    if( items[id]>0 )
	    {
		fprintf( stdout, "UpdateProcedureGolomb(%X,%d,%d:%d)\n",
			BitAddr, BitOffset-size, size, nValue );
//		DoUpdate( BitAddr, BitOffset-size, size, nValue );
		DoUpdate( BitAddr+ByteSkip, BitOffset-size, size, nValue );
	    }
	}
	return 0;
}

void SetSpecial( int mode )
{
	switch( mode )
	{
	case SET_START :
	    SpecialStartAddr = BitAddr;
	    SpecialStartBits = BitOffset;
	    break;
	case SET_START_END :
	    SpecialStartAddr = BitAddr;
	    SpecialStartBits = BitOffset;
	    SpecialEndBits   = BitOffset+1;
	    break;
	case SET_END :
	    SpecialEndAddr   = BitAddr;
	    SpecialEndBits   = BitOffset;
	    break;
	}
}

int MPEG_Sequence( FILE *fp, unsigned char buffer[] )
{
/*
HSV	12bit
VSV	12bit
ARI	4bit
FRC	4bit
BRV	18bit
MRK	1bit
VBSV	10bit
CPF	1bit
LIQM	1bit
IQM	8*64bit
LNIQM	1bit
NIQM	8*64bit
---------------
(512*2)*n+64bit
1088bit=136byte
*/
int HSV, VSV, ARI, FRC, BRV, MRK, VBSV, CPF, LIQM, LNIQM;
int dummy, i;
	fprintf( stdout, "Sequence() : (%s)\n", AddrStr2(g_addr-4) );
	fprintf( stdout, "**********************************************\n" );
 	InitBitStream();
	UpdateProcedure( nSequence, ID_HSV, 12 );
 	HSV = GetBitStream( fp, 12 );
	UpdateProcedure( nSequence, ID_VSV, 12 );
 	VSV = GetBitStream( fp, 12 );
	UpdateProcedure( nSequence, ID_ARI,  4 );
 	ARI = GetBitStream( fp,  4 );
	UpdateProcedure( nSequence, ID_FRC,  4 );
 	FRC = GetBitStream( fp,  4 );
 	BRV = GetBitStream( fp, 18 );
 	MRK = GetBitStream( fp,  1 );
 	VBSV= GetBitStream( fp, 10 );
 	CPF = GetBitStream( fp,  1 );
 	LIQM= GetBitStream( fp,  1 );
	if( LIQM>0 )
	{
	    for( i=0; i<16; i++ )
		dummy= GetBitStream( fp,  32 );
	}
 	LNIQM= GetBitStream( fp,  1 );
	if( LNIQM>0 )
	{
	    for( i=0; i<16; i++ )
		dummy= GetBitStream( fp,  32 );
	}
	if( bDumpSequence )
	{
	    fprintf( stdout, "HSV=%d\n", HSV );
	    fprintf( stdout, "VSV=%d\n", VSV );
	    fprintf( stdout, "ARI=%d\n", ARI );
	    fprintf( stdout, "FRC=%d\n", FRC );
	    fprintf( stdout, "BRV=%d\n", BRV );
	}
	nSequence++;
	return 0;
}

int MPEG_SequenceExtension( FILE *fp, unsigned char buffer[] )
{
int PALI, PS, CF, HSE, VSE, BRE, MRK, VBSE, LD, FREn, FREd;
	fprintf( stdout, "SequenceExtension() : (%s)\n", AddrStr2(g_addr-5) );
	fprintf( stdout, "----------------------------------------------\n" );
	UpdateProcedure( nSequence, ID_PALI, 8 );
	PALI = GetBitStream( fp, 8 );
	UpdateProcedure( nSequence, ID_PS,   1 );
	PS   = GetBitStream( fp, 1 );
	CF   = GetBitStream( fp, 2 );
	HSE  = GetBitStream( fp, 2 );
	VSE  = GetBitStream( fp, 2 );
	BRE  = GetBitStream( fp,12 );
	MRK  = GetBitStream( fp, 1 );
	VBSE = GetBitStream( fp, 8 );
	LD   = GetBitStream( fp, 1 );
	FREn = GetBitStream( fp, 2 );
	FREd = GetBitStream( fp, 2 );
	if( bDumpSequenceExtension )
	{
	    fprintf( stdout, "PALI=%d\n", PALI );
	    fprintf( stdout, "PS  =%d\n", PS   );
	    fprintf( stdout, "CF  =%d\n", CF   );
	    fprintf( stdout, "LD  =%d\n", LD   );
	}
	nSequenceExtension++;
	return 0;
}

int MPEG_SequenceDisplayExtension( FILE *fp, unsigned char buffer[] )
{
int VF, CD, CP, TC, MXC, DHS, MRK, DVS;
	CP = (-1);
	TC = (-1);
	MXC= (-1);

	fprintf( stdout, "SequenceDisplayExtension() : (%s)\n", 
	    AddrStr2(g_addr-5) );
	ShowBitAddr( );
	fprintf( stdout, "==============================================\n" );
	VF   = GetBitStream( fp, 3 );
	CD   = GetBitStream( fp, 1 );
	if( CD==1 )
	{
	    UpdateProcedure( nSequence, ID_CP,   8 );
	    CP   = GetBitStream( fp, 8 );
	    UpdateProcedure( nSequence, ID_TC,   8 );
	    TC   = GetBitStream( fp, 8 );
	    UpdateProcedure( nSequence, ID_MXC,  8 );
	    MXC  = GetBitStream( fp, 8 );
	}
	UpdateProcedure( nSequence, ID_DHS, 14 );
	DHS  = GetBitStream( fp,14 );
	MRK  = GetBitStream( fp, 1 );
	UpdateProcedure( nSequence, ID_DVS, 14 );
	DVS  = GetBitStream( fp,14 );
	if( bDumpSequenceDisplayExtension )
	{
	    fprintf( stdout, "VF =%d\n", VF );
	    fprintf( stdout, "CD =%d\n", CD );
	    fprintf( stdout, "CP =%d\n", CP );
	    fprintf( stdout, "TC =%d\n", TC );
	    fprintf( stdout, "MXC=%d\n", MXC );
	    fprintf( stdout, "DHS=%d\n", DHS );
	    fprintf( stdout, "DVS=%d\n", DVS );
	}
	nSequenceDisplayExtension++;
	return 0;
}
int MPEG_SequenceScalableExtension( FILE *fp, unsigned char buffer[] )
{
int SM, LID, LLPHS, MRK, LLPVS, HSFm, HSFn, VSFm, VSFn, PME, MTPS, PMO, PMF;
	fprintf( stdout, "SequenceScalableExtension() : (%s)\n", 
	    AddrStr2(g_addr-5) );
	SM   = GetBitStream( fp, 2 );
	LID  = GetBitStream( fp, 4 );
	LLPHS= GetBitStream( fp,14 );
	MRK  = GetBitStream( fp, 1 );
	LLPVS= GetBitStream( fp,14 );
	HSFm = GetBitStream( fp, 5 );
	HSFn = GetBitStream( fp, 5 );
	VSFm = GetBitStream( fp, 5 );
	VSFn = GetBitStream( fp, 5 );
	PME  = GetBitStream( fp, 1 );
	MTPS = GetBitStream( fp, 1 );
	PMO  = GetBitStream( fp, 3 );
	PMF  = GetBitStream( fp, 3 );
	return 0;
}

int MPEG_PictureCodingExtention( FILE *fp, unsigned char buffer[] )
{
int FHFC, FVFC, BHFC, BVFC, IDP, PSTR, TFF, FPFD, CMV, QST, IVF;
int AS, RFF, C420T, PF, CDF;
//int VA, FS, SC, BA, SCP;
//	if( bShowSyntax )
	fprintf( stdout, "PictureCodingExtension() : (%s)\n", 
	    AddrStr2(g_addr-5) );
	FHFC = GetBitStream( fp, 4 );
	FVFC = GetBitStream( fp, 4 );
	BHFC = GetBitStream( fp, 4 );
	BVFC = GetBitStream( fp, 4 );
	IDP  = GetBitStream( fp, 2 );
	PSTR = GetBitStream( fp, 2 );
	TFF  = GetBitStream( fp, 1 );
	FPFD = GetBitStream( fp, 1 );
	CMV  = GetBitStream( fp, 1 );
	QST  = GetBitStream( fp, 1 );
	IVF  = GetBitStream( fp, 1 );
	AS   = GetBitStream( fp, 1 );
	RFF  = GetBitStream( fp, 1 );
	C420T= GetBitStream( fp, 1 );
	PF   = GetBitStream( fp, 1 );
	CDF  = GetBitStream( fp, 1 );
#if 0	// PAL
	VA   = GetBitStream( fp, 1 );
	FS   = GetBitStream( fp, 3 );
	SC   = GetBitStream( fp, 1 );
	BA   = GetBitStream( fp, 7 );
	SCP  = GetBitStream( fp, 8 );
#endif
//	fprintf( stdout, "PSTR=%d\n", PSTR );
	return 0;
}

int MPEG_PictureDisplayExtension( FILE *fp, unsigned char buffer[] )
{
int FCHO, FCVO;
int MRK;
int i;
	fprintf( stdout, "PictureDisplayExtension() : (%s)\n", 
	    AddrStr(g_addr-5) );
	ShowBitAddr( );
/*
	ESC,ESCI,<FCHO,MRK,FCVO,MRK>
*/
	for( i=0; i<2; i++ )
	{
	    FCHO = GetBitStream( fp, 16 );// Frame Center Horizontal Offset
	    MRK  = GetBitStream( fp,  1 );// Marker Bit
	    FCVO = GetBitStream( fp, 16 );// Frame Center Vertical Offset
	    MRK  = GetBitStream( fp,  1 );// Marker Bit
	    fprintf( stdout, "FCHO[%d]=%4d\n", i, FCHO );
	    fprintf( stdout, "FCVO[%d]=%4d\n", i, FCVO );
	}
	return 0;
}

int MPEG_QuantMatrixExtension( FILE *fp, unsigned char buffer[] )
{
int LIQM, LNIQM, LCIQM, LCNIQM;
int dummy, i;
	fprintf( stdout, "QuantMatrixExtension() : (%s)\n", 
	    AddrStr(g_addr-5) );
	LIQM = GetBitStream( fp, 1 );
	if( LIQM>0 )
	{
	    for( i=0; i<16; i++ )
		dummy= GetBitStream( fp,  32 );
	}
	LNIQM = GetBitStream( fp, 1 );
	if( LNIQM>0 )
	{
	    for( i=0; i<16; i++ )
		dummy= GetBitStream( fp,  32 );
	}
	LCIQM = GetBitStream( fp, 1 );
	if( LCIQM>0 )
	{
	    for( i=0; i<16; i++ )
		dummy= GetBitStream( fp,  32 );
	}
	LCNIQM = GetBitStream( fp, 1 );
	if( LCNIQM>0 )
	{
	    for( i=0; i<16; i++ )
		dummy= GetBitStream( fp,  32 );
	}
	return 0;
}

int MPEG_Extension( FILE *fp, unsigned char buffer[] )
{
int ESCI;
unsigned int topAddr = g_addr-4;
 	InitBitStream();
	ESCI = GetBitStream( fp, 4 );
	fprintf( stdout, "Extension(%d) : (%s)\n", ESCI, AddrStr2(topAddr) );
	switch( ESCI )
	{
	case 1 :	// Sequence Extension
	    MPEG_SequenceExtension( fp, buffer );
//	    fprintf( stdout, "\n" );
	    break;
	case 2 :	// Sequence Display Extension
	    MPEG_SequenceDisplayExtension( fp, buffer );
	    break;
	case 3 :	// Quant Matrix Extension
	    MPEG_QuantMatrixExtension( fp, buffer );
	    break;
	case 5 :	// Sequence Scalable Extension
	    MPEG_SequenceScalableExtension( fp, buffer );
	    break;
	case 8 :	// Picture Coding Extension
	    MPEG_PictureCodingExtention( fp, buffer );
	    break;
	case 7 :	// Picture Display Extension
	    MPEG_PictureDisplayExtension( fp, buffer );
	    break;
	case 9 :	// Picture Spatial Scalable Extension
	case 10 :	// Picture Temporal Scalable Extension
	default :
	    fprintf( stdout, "Unknow ESCI(0x%02X)\n", ESCI );
	    EXIT();
	    break;
	}
	if( bDump )
	fprintf( stdout, "%d bits readed\n", BitReaded );
	return 0;
}

int MPEG_GOP( FILE *fp, unsigned char buffer[] )
{
int TC, CG, BL;
	if( bShowSyntax )
	fprintf( stdout, "GOP()\n" );
 	InitBitStream();
	TC   = GetBitStream( fp,25 );	// Time Code
	CG   = GetBitStream( fp, 1 );	// Closed GOP flag
	BL   = GetBitStream( fp, 1 );	// Broken Link flag
	if( bShowSyntax )
	fprintf( stdout, "ClosedGOP=%d\n", CG );
	if( bDump )
	fprintf( stdout, "%d bits readed\n", BitReaded );
	return 0;
}

int MPEG_Picture( FILE *fp, unsigned char buffer[] )
{
unsigned int topAddr = g_addr-4;
int TR, PCT, VD, EBP, EIP;
 	InitBitStream();
//	bDebug=1;
	TR   = GetBitStream( fp,10 );
	PCT  = GetBitStream( fp, 3 );
	VD   = GetBitStream( fp,16 );
#if 0	// Not used with MPEG2
	FPFV = GetBitStream( fp, 1 );
	FFC  = GetBitStream( fp, 3 );
	FPBV = GetBitStream( fp, 1 );
	BFC  = GetBitStream( fp, 3 );
#endif
	EBP  = GetBitStream( fp, 1 );
	EBP  = GetBitStream( fp, 1 );
	while( EBP>0 )
	{
	    EIP  = GetBitStream( fp, 3 );
	    EBP  = GetBitStream( fp, 1 );
	}
	if( bDSS )
	{
	    DSS_PCT=PCT;
	    sprintf( DSS_Addr, "%s", AddrStr2(topAddr) );
	    fprintf( stdout, "DSS_Picture(%d) : (%s) (------------)\n", 
		PCT, AddrStr2(topAddr) );
	} else {
	    fprintf( stdout, "MPEG_Picture(%d) : (%s) (%s)\n", 
		PCT, AddrStr2(topAddr), TsStr() );
	}
	if( bShowDetail )
	    fprintf( stdout, "TR=%3d\n", TR );
	picture_coding_type = PCT;
	if( bDump )
	fprintf( stdout, "%d bits readed\n", BitReaded );
//	bDebug=0;
/*
	PictureCodingExtention( fp, buffer );
	QuantMatrixExtention( fp, buffer );
	PictureDisplayExtention( fp, buffer );
	PictureSpatialScalableExtention( fp, buffer );
	PictureTemporalScalableExtention( fp, buffer );
*/
	return 0;
}

int SeekHeader( FILE *fp, unsigned char buffer[] )
{
int state=0;
int readed;
int total=0;
	while( state<3 )
	{
	    readed = gread( buffer, 1, 1, fp );
	    if( readed<1 )
	    {
		CannotRead( "(SeekHeader)" );
		return -1;
	    }
	    total+=1;
	    switch( state )
	    {
	    case 0 :
	    	if( buffer[0]==0x00 )
		    state++;
		break;
	    case 1 :
	    	if( buffer[0]==0x00 )
		    state++;
		else
		    state=0;
		break;
	    case 2 :
	    	if( buffer[0]==0x01 )
		    state++;
		else if( buffer[0]==0x00 )
		    state=state;
		else
		    state = 0;
		break;
	    }
	}
	return total;
}

int UserDataTimestamp( unsigned char UserData[] )
{
unsigned int timeStamp=0;
	timeStamp = 
	    ((UserData[0]&0x03)<<30)
	  | ((UserData[1]&0x7F)<<23)
	  | ((UserData[2]&0xFF)<<15)
	  | ((UserData[3]&0x7F)<< 8)
	  | ((UserData[4]&0xFF)<< 0);
#if 0
        fprintf( stdout, "UserDataTimestamp(%2X %2X %2X %2X %2X:%8X)\n",
		UserData[0], UserData[1], UserData[2], UserData[3], UserData[4],
		timeStamp );
	fprintf( stdout, "%8X %8X %8X : %8X\n",
	    ((UserData[2]&0xFF)<<15),
	    ((UserData[3]&0xEF)<< 8),
	    ((UserData[4]&0xFF)<< 0),
	    ((UserData[2]&0xFF)<<15)
	  | ((UserData[3]&0xEF)<< 8)
	  | ((UserData[4]&0xFF)<< 0) );
#endif
	return timeStamp/300;
}

int MPEG_UserData( FILE *fp, unsigned char buffer[] )
{
int total=0;
int i;
	if( bShowSyntax )
	    fprintf( stdout, "UserData() : " );
	fprintf( stdout, "UserData(%s)\n", AddrStr(g_addr-4) );

	int pos = ftell(fp)-4;
	unsigned int l_addr = g_addr-4;

	total = SeekHeader( fp, buffer );
	if( total<0 )
	    return -1;
#if 1
//	fprintf( stdout, "fseek(%X)\n", pos );
	fseek( fp, pos, SEEK_SET );
	g_addr = l_addr;
	InitBitStream();

	unsigned char UserData[256];
	int nUserData=0;
	unsigned char user_data;
	int userDataSize = total+1;
	if( bUserData )
	{
	    if( bDSS==0 )
	    fprintf( stdout, 
		"[UserData] pts  = 0x%04X%06X\n", g_PTSH, g_PTSL );
	    fprintf( stdout, 
		"[UserData] picture_coding_type = 0x%X\n", 
		picture_coding_type );
	    fprintf( stdout, "[UserData] size = 0x%08X\n", userDataSize );
	    fprintf( stdout, "[UserData] data =" );
	}
	for( i=0; i<userDataSize; i++ ) 	// read before 00 00 01
	{
	    user_data = GetBitStream( fp, 8 );
	    if( nUserData<256 )
	    {
	    	UserData[nUserData++] = user_data;
	    }
	    if( bUserData )
		fprintf( stdout, " %02X", user_data );
	}
	if( bUserData )
	    fprintf( stdout, "\n" );
	b_user_data = 1;

	if( bDSS )
	{
	    int pos=4;
	    while( UserData[pos]!=0 )
	    {
		int user_data_length = UserData[pos++];
		int user_data_type   = UserData[pos++];
		switch( user_data_type )
		{
		case 2 :	// PTS
		    g_PTSH=0;
		    g_PTSL=UserDataTimestamp( &UserData[pos] );
//		    fprintf( stdout, "PTS=%8X\n", g_PTSL );
//		    pos+=5;
		    pos+=(user_data_length-1);
		    break;
		case 4 :	// DTS
		    g_DTSH=0;
		    g_DTSL=UserDataTimestamp( &UserData[pos] );
//		    fprintf( stdout, "DTS=%8X\n", g_DTSL );
//		    pos+=5;
		    pos+=(user_data_length-1);
		    break;
		case 9 :	// closed caption
		    fprintf( stdout, 
			"[UserData] CC   = %02X %02X\n", 
			    UserData[pos+0], UserData[pos+1] );
		    pos+=(user_data_length-1);
		    break;
		default :
		    pos+=(user_data_length-1);
		    break;
		}
	    }
	    fprintf( stdout, 
		"[UserData] pts  = 0x%08X\n", g_PTSL );
	    fprintf( stdout, 
		"[UserData] dts  = 0x%08X\n", g_DTSL );
	    fprintf( stdout, "DSS_Picture(%d) : (%s) (%s)\n", 
		DSS_PCT, DSS_Addr, TsStr() );
	}

	user_data = GetBitStream( fp, 8 );	// 00
	if( user_data!=0x00 )
	    fprintf( stdout, "Invalid %02X (UserData0)\n", user_data );
	user_data = GetBitStream( fp, 8 );	// 00
	if( user_data!=0x00 )
	    fprintf( stdout, "Invalid %02X (UserData1)\n", user_data );
	user_data = GetBitStream( fp, 8 );	// 01
	if( user_data!=0x01 )
	    fprintf( stdout, "Invalid %02X (UserData2)\n", user_data );
#endif
	if( bDump )
	    fprintf( stdout, "%d bytes readed\n", total );
	return 0;
}


// SSC : 00 00 01 AF
int MPEG_Slice( FILE *fp, unsigned char buffer[], int ID )
{
int total=0;
int QSC, ISF, IS, RB, EBS, EIS;
int MBE;
	if( bDumpSlice )
	fprintf( stdout, "Slice(%02X) : (%s) : ", ID, AddrStr(g_addr-4) );

 	InitBitStream();
#if 1
//	bDebug = 1;
#if 0
	SVPE = GetBitStream( fp, 3 );
	PBP  = GetBitStream( fp, 7 );
	if( bDumpSlice )
	{
	fprintf( stdout, "SVPE=%d\n", SVPE );
	fprintf( stdout, "PBP =%d\n", PBP  );
	}
#endif
	QSC  = GetBitStream( fp, 5 );
	ISF  = GetBitStream( fp, 1 );
	if( bDumpSlice )
	{
	fprintf( stdout, "QSC =%d\n", QSC  );
	fprintf( stdout, "ISF =%d\n", ISF  );
	}
	if( ISF>0 )
	{
	    IS   = GetBitStream( fp, 1 );	// Intra Slice Flag
	    RB   = GetBitStream( fp, 7 );	// Reserved BIts
	    if( bDumpSlice )
	    {
	    fprintf( stdout, "IS =%d\n", IS );
	    fprintf( stdout, "RB =%d\n", RB );
	    }
	}
	EBS=1;
	while( EBS==1 )
	{
	    EBS  = GetBitStream( fp, 1 );
	    EIS  = GetBitStream( fp, 8 );
	    if( bDumpSlice )
	    {
	    fprintf( stdout, "EBS=%d\n", EBS );
	    fprintf( stdout, "EIS=%d\n", EIS );
	    }
	}
	MBE  = GetBitStream( fp, 11 );
	if( bDumpSlice )
	fprintf( stdout, "MBE=0x%X\n", MBE );

//	bDebug = 0;

	total = SeekHeader( fp, buffer );
	if( total<0 )
	    return -1;
#else
	total = SeekHeader( fp, buffer );
	if( total<0 )
	    return -1;
#endif
	if( bDumpSlice )
	fprintf( stdout, "%d bytes readed\n", total );
	return 3;
}

// ------------------------------------------------------------
//	Analyze
// ------------------------------------------------------------
/*
H.264
NAL header = 0,nal_ref_idc[1:0], nal_unit_type[4:0]
nal_unit_type =
1 : non-IDR picture
5 : IDR picutre slice
6 : SEI
7 : SPS
8 : PPS
9 : AU delimiter
10 : End of Sequence
11 : End of Stream
12 : Filler data
13 : Sequence Parameter Set Extrention
14..18 : 
19 : 
20..23:
24..31:
*/
int AnalyzeMPG( FILE *fp, FILE *pts_fp )
{
int bNoRead=0;
int eof=0;
unsigned char buffer[BUF_SIZE];
unsigned int ID;
int readed;
unsigned int PTSH, PTSL, DTSH, DTSL;
int readP=0;
unsigned int pes_addr=0xFFFFFFFF;
int i;
#define MAX_DATA_SIZE	1024*64
unsigned char dataBuf[MAX_DATA_SIZE];
int nAlarm=0;

	while( eof==0 )
	{
#if 0
	    if( bNoRead==0 )
	    {
		readed = gread( buffer, 1, 4, fp );
		if( readed<4 )
		{
		    CannotRead( "Prefix" );
		    break;
		}
	    }
#else
	    if( readP<4 )
	    {
	    	int size;
	    	size = 4-readP;
#if 0
		fprintf( stdout, "gread@%d, %d\n", readP, size );
#endif
		readed = gread( &buffer[readP], 1, size, fp );
		if( readed<size )
		{
		    CannotRead( "Prefix" );
		    break;
		}
		readP+=size;
	    }
#endif
	    if( (buffer[0]==0x00)
	     && (buffer[1]==0x00)
	     && (buffer[2]==0x00) )
	    {
	    	int bWarn=0;
#if 1
		if ( buffer[3]==1 )
		{
		    bWarn=1;
		}
#endif
	    	memcpy( &buffer[0], &buffer[1], 3 );
		readed = gread( &buffer[3], 1, 1, fp );
		if( readed<1 )
		{
		    CannotRead( "Prefix" );
		    break;
		}
		if( bWarn )
		{
		    if( bAlarmAVC )
		    {
		    	if( (buffer[3]>0x00) && (buffer[3]<0xB0) )
			{
			    if( nAlarm<10 )
			    {
				fprintf( stdout, 
				"%8llX : File may be AVC ?  Add -AVC option : ",
				    g_addr-4 );
				fprintf( stdout, 
				    "00 00 00 01 %02X\n", buffer[3] );
			    }
			    nAlarm++;
			}
		    }
		}
		continue;
	    }

	    if( (buffer[0]!=0x00)
	     || (buffer[1]!=0x00)
	     || (buffer[2]!=0x01) )
	    {
	    	fprintf( stdout, 
		"AnalyzeMPG : Invalid Prefix %02X %02X %02X : (%s)\n",
			buffer[0], buffer[1], buffer[2], AddrStr(g_addr-4) );
		while( 1 )
		{
		    memcpy( &buffer[0], &buffer[1], 3 );
		    readed = gread( &buffer[3], 1, 1, fp );
		    if( readed<1 )
		    {
		    	fprintf( stdout, "EOF\n" );
			EXIT();
		    }
		    if( (buffer[0]==0)
		     && (buffer[1]==0)
		     && (buffer[2]==1) )
		     {
		     	fprintf( stdout, "00 00 01 : %llX\n", g_addr );
		     	break;
		     }
		}
	    }
#if 0
	    for( i=0; i<4; i++ )
	    {
		fprintf( stdout, "%02X ", buffer[i] );
	    }
	    fprintf( stdout, ": %llX", g_addr );
	    fprintf( stdout, "\n" );
#endif
	    // 00 00 01 XX(ID)
	    ID = buffer[3];
	    if( (ID>=0xB0) || bDumpSlice )
	    {
		if( bShowSyntax )
		{
		    for( i=0; i<4; i++ )
		    {
			fprintf( stdout, "%02X ", buffer[i] );
		    }
		    fprintf( stdout, ": " );
		    fprintf( stdout, "\n" );
		}
    //	    fprintf( stdout, "ID = %02X\n", ID );
	    }
	    readP = 0;
	    if( (ID>=1) && (ID<=0xAF) )
	    {
		    readP =  MPEG_Slice( fp, buffer, ID );
		    if( readP<0 )
		    {
		    	eof=1;
			break;
		    }
		    buffer[0] = 0x00;
		    buffer[1] = 0x00;
		    buffer[2] = 0x01;
		    readed = gread( &buffer[3], 1, 1, fp );
		    readP = 4;
		    bNoRead=1;
	    } else {
		if( bShowSyntax )
	    	if( bNoRead )
		{
		    fprintf( stdout, "%02X %02X %02X %02X : ",
			buffer[0], buffer[1], buffer[2], buffer[3] );
		    fprintf( stdout, "\n" );
		}
		bNoRead=0;
		int pes_len=(-1);
		pes_addr = g_addr-4;
		switch( ID )
		{
		case 0xE0 :	// Video
		case 0xE1 :	// 
		case 0xE2 :	// 
		case 0xE3 :	// 
		case 0xE4 :	// 
		case 0xE5 :	// 
		case 0xE6 :	// 
		case 0xE7 :	// 
		case 0xE8 :	// 
		case 0xE9 :	// 
		case 0xEA :	// 
		case 0xEB :	// 
		case 0xEC :	// 
		case 0xED :	// 
		case 0xEE :	// 
		case 0xEF :	// 
		    pes_len = PES_header( fp, buffer, bDisplayTS, 
		    	&PTSH, &PTSL, &DTSH, &DTSL );
		    {
		    	if( PTSH==0xFFFFFFFF )
			{
			    fprintf( stdout, "ID=%02X : PES_header(%s)\n", 
				ID, AddrStr2(pes_addr) );
			} else {
			    int hour, min, sec, msec;
			    TcTime32( PTSH, PTSL, &hour, &min, &sec, &msec );
			    fprintf( stdout, 
			"ID=%02X : PES_header(%s) PTS=%2d:%02d:%02d.%03d\n", 
				ID, AddrStr2(pes_addr), hour, min, sec, msec );
			}
		    }
		    break;
		case 0xB2 :
		    if( MPEG_UserData( fp, buffer )<0 )
		    {
		    	eof=1;
		    	break;
		    }
		    buffer[0] = 0x00;
		    buffer[1] = 0x00;
		    buffer[2] = 0x01;
		    readed = gread( &buffer[3], 1, 1, fp );
		    bNoRead=1;
		    readP = 4;
		    break;
		case 0xB3 :	// SHC
		    MPEG_Sequence( fp, buffer );
		    break;
		case 0xB5 :	// ESC
		    MPEG_Extension( fp, buffer );
		    break;
		case 0xB8 :	// GSC
		    MPEG_GOP( fp, buffer );
		    break;
		case 0x00 :	// PSC
		    MPEG_Picture( fp, buffer );
		    break;
		case 0xAF :	// SSC
		    break;
		case 0xB7 :	// Sequence end
		    fprintf( stdout, "Sequence end\n" );
		    break;
		case 0xC0 :	// Audio
		case 0xC1 :
		case 0xC2 :
		case 0xC3 :
		case 0xC4 :
		case 0xC5 :
		case 0xC6 :
		case 0xC7 :
		case 0xC8 :
		case 0xC9 :
		case 0xCA :
		case 0xCB :
		case 0xCC :
		case 0xCD :
		case 0xCE :
		case 0xCF :

		case 0xBD :	// AC3 in MPEG TS
		    {
		    int a0 = g_addr;
		    pes_len = PES_header( fp, buffer, bDisplayTS,
		    	&PTSH, &PTSL, &DTSH, &DTSL );
		    if( PTSH==0xFFFFFFFF )
		    {
			fprintf( stdout, "ID=%02X : PES_header(%s)\n", 
			    ID, AddrStr2(pes_addr) );
		    } else {
			int hour, min, sec, msec;
			TcTime32( PTSH, PTSL, &hour, &min, &sec, &msec );
			fprintf( stdout, 
		    "ID=%02X : PES_header(%s) PTS=%2d:%02d:%02d.%03d\n", 
			    ID, AddrStr2(pes_addr), hour, min, sec, msec );
		    }
		    fprintf( pts_fp, "%8X %8llX : %4X %08X, %4X %08X\n",
			    pes_addr, SrcAddr(pes_addr,0),
			    (UINT)(PTSH & 0xFFFF), PTSL, 
			    (UINT)(DTSH & 0xFFFF), DTSL );
		    int a1 = g_addr;
		    int readSize = pes_len-(a1-a0)+2;
#if 0
		    fprintf( stdout, "pes_len=%d, a1-a0=%d, read=%d\n",
		    	pes_len, a1-a0, readSize );
#endif
		    if( readSize>=MAX_DATA_SIZE )
		    {
		    	fprintf( stdout, "Too large data (0x%X)\n",
				readSize );
			exit( 1 );
		    }
		    readed = gread( dataBuf, 1, readSize, fp );
		    }
		    break;
		default :
		    fprintf( stdout, 
		    	"Not implemented (%02X) in AnaylzeMPG(%s)\n",
			    ID, AddrStr(pes_addr) );
		    break;
		}
	    }
	}
	fprintf( stdout, "nSequence=%d\n", nSequence );
	fprintf( stdout, "nSequenceExtension=%d\n", nSequenceExtension );
	fprintf( stdout, "nSequenceDisplayExtension=%d\n", 
		nSequenceDisplayExtension );
#if 0
	if( g_update_addr[0]==0xFFFFFFFF )
	{
	    fprintf( stdout, "No g_update\n" );
	} else {
	    fprintf( stdout, "addr=%X\n", g_update_addr[0] );
	    fprintf( stdout, "bits=%X\n", g_update_bits[0] );
	    fprintf( stdout, "size=%X\n", g_update_size[0] );
	    fprintf( stdout, "data=%X\n", g_update_data[0] );
	}
#endif
	return 0;
}

// ----------------------------------------------------------------------
// Analayze AVS
// ----------------------------------------------------------------------
int AVS_low_delay = (-1);
int AVS_fixed_picture_qp = (-1);
int AVS_skip_mode_flag = (-1);
int AVS_chroma_format = (-1);
int AVS_picture_reference_flag = (-1);

int AVS_PictureType=(-1);
int AVS_PictureStructure=(-1);
int AVS_MbRow=(-1);
int AVS_MbIndex=(-1);
int AVS_MbWidth=(-1);
int AVS_MbHeight=(-1);
int AVS_NumberOfReference=(-1);
int AVS_MbType=(-1);
int AVS_MbTypeIndex=(-1);
int AVS_MvNum=(-1);
int AVS_MbWeightingFlag=0;
int AVS_MbCBP=(-1);
int AVS_MbCBP422=(-1);

#define AVS_NONE	0
#define AVS_I_FRAME	1
#define AVS_P_FRAME	2
#define AVS_B_FRAME	3
int AVS_CurrentPicture=AVS_NONE;

#define AVS_P_SKIP	0

#define AVS_B_SKIP	 0
#define AVS_B_8x8	23
#define AVS_I_8x8	24

int AVS_sequence_header( FILE *fp, unsigned char buffer[] )
{
	fprintf( stdout, "AVS_sequence_header() : (%s)\n", AddrStr(g_addr-4) );
	fprintf( stdout, "**********************************************\n" );
 	InitBitStream();

	int profile_id = GetBitStream( fp, 8 );
	int level_id = GetBitStream( fp, 8 );
	int progressive_sequence = GetBitStream( fp, 1 );
	int horizontal_size = GetBitStream( fp, 14 );
	int vertical_size = GetBitStream( fp, 14 );
	AVS_chroma_format = GetBitStream( fp, 2 );
	int sample_precision = GetBitStream( fp, 3 );
	int aspect_ratio = GetBitStream( fp, 4 );
	int frame_rate_code = GetBitStream( fp, 4 );
	int bit_rate_lower = GetBitStream( fp, 18 );
	int marker_bit = GetBitStream( fp, 1 );
	int bit_rate_upper = GetBitStream( fp, 12 );
	AVS_low_delay = GetBitStream( fp, 1 );
	int marker_bit2= GetBitStream( fp, 1 );
	int bbv_buffer_size = GetBitStream( fp, 18 );
#if 0
	int reserved_bits = GetBitStream( fp, 2 );
#else
	GetBitStream( fp, 2 );
#endif

//	if( bDumpSequence )
	{
	    fprintf( stdout, "profile_id          =%d\n", profile_id );
	    fprintf( stdout, "level_id            =%d\n", level_id   );
	    fprintf( stdout, "progressive_sequence=%d\n", progressive_sequence);
	    fprintf( stdout, "horizontal_size     =%d\n", horizontal_size );
	    fprintf( stdout, "vertical_size       =%d\n", vertical_size   );
	    fprintf( stdout, "sample_precision    =%d\n", sample_precision);
	    fprintf( stdout, "aspect_ratio        =%d\n", aspect_ratio    );
	    fprintf( stdout, "frame_rate_code     =%d\n", frame_rate_code );
	    fprintf( stdout, "bit_rate_lower      =%d\n", bit_rate_lower  );
	    fprintf( stdout, "marker_bit          =%d\n", marker_bit      );
	    fprintf( stdout, "bit_rate_upper      =%d\n", bit_rate_upper  );
	    fprintf( stdout, "low_delay           =%d\n", AVS_low_delay   );
	    fprintf( stdout, "marker_bit2         =%d\n", marker_bit2     );
	    fprintf( stdout, "bbv_buffer_size     =%d\n", bbv_buffer_size );
	}
	AVS_MbWidth  =  (horizontal_size+15)/16;
	AVS_MbHeight = ((vertical_size  +31)/32)*2;

	nSequence++;
	return 0;
}
int AVS_sequence_display_extension( FILE *fp, unsigned char buffer[],
	int extension_id )
{
//	int extension_id = GetBitStream( fp, 4 );
	int video_format = GetBitStream( fp, 3 );
	int sample_range = GetBitStream( fp, 1 );
	int colour_description = GetBitStream( fp, 1 );

	fprintf( stdout, "extension_id           =%d\n",
		extension_id            );
	fprintf( stdout, "video_format           =%d\n",
		video_format            );
	fprintf( stdout, "sample_range           =%d\n",
		sample_range            );

	if( colour_description )
	{
	    int colour_primaries = GetBitStream( fp, 8 );
	    int transfer_characteristics = GetBitStream( fp, 8 );
	    int matrix_coefficients = GetBitStream( fp, 8 );
	    fprintf( stdout, "colour_primaries         = %d\n", 
	    	colour_primaries );
	    fprintf( stdout, "transfer_characteristics = %d\n", 
	    	transfer_characteristics );
	    fprintf( stdout, "matrix_coefficients      = %d\n", 
	    	matrix_coefficients );
	}
	int display_horizontal_size = GetBitStream( fp, 14 );
	int marker_bit = GetBitStream( fp, 1 );
	fprintf( stdout, "marker_bit=%d\n", marker_bit );
	int display_vertical_size = GetBitStream( fp, 14 );
#if 0
	int reserved_bits = GetBitStream( fp, 2 );
#else
	GetBitStream( fp, 2 );
#endif
	fprintf( stdout, "display_horizontal_size=%d\n",
		display_horizontal_size );
	fprintf( stdout, "display_vertical_size =%d\n",
		display_vertical_size );
	return 0;
}

int AVS_extension_data( FILE *fp, unsigned char buffer[], int id )
{
	fprintf( stdout, "AVS_extension_data() : (%s)\n", AddrStr(g_addr-4) );
	fprintf( stdout, "**********************************************\n" );
 	InitBitStream();

	if( id==0 )	// after sequence header
	{
	    int next_bits4 = GetBitStream( fp, 4 );
	    fprintf( stdout, "next_bits4=%d\n", next_bits4 );
	    if( next_bits4== 2 )	// picture display extension
	    {
	    	fprintf( stdout, "pictrue display  extension\n" );
	    	AVS_sequence_display_extension( fp, buffer, next_bits4 );
//		EXIT();
	    } else if( next_bits4== 4 )	// copyright extension
	    {
	    	fprintf( stdout, "copyright extension\n" );
		EXIT();
	    } else if( next_bits4==11 )	// camera parameters extension
	    {
	    	fprintf( stdout, "camera parameters extension\n" );
	    } else {
		int state=0;
		int bFirst=1;
		while( 1 )
		{
		    int next_bits8;
		    if( bFirst )
		    {
			next_bits8 = (next_bits4<<4) | GetBitStream( fp, 4 );
		    } else {
			next_bits8 = GetBitStream( fp, 8 );
		    }
		    bFirst=0;
		    fprintf( stdout, "state=%d, next_bits8=%02X\n",
		    	state, next_bits8 );
		    switch( state )
		    {
		    case 0 :
		    	if( next_bits8==0 )
			    state++;
			else {
			    fprintf( stdout, "%02X ", next_bits8 );
			}
			break;
		    case 1 :
		    	if( next_bits8==0 )
			{
			    state++;
			} else {
			    fprintf( stdout, "00 %02X ", next_bits8 );
			    state = 0;
			}
			break;
		    case 2 :
		    	if( next_bits8==0 )
			{
			    fprintf( stdout, "00 " );
			} else if( next_bits8==1 )
			{
			    buffer[0] = 0;
			    buffer[1] = 0;
			    buffer[2] = 1;
			    return 3;
			} else {
			    fprintf( stdout, "00 00 %02X ", next_bits8 );
			    state = 0;
			}
			break;
		    }
		}
	    }
	} else {	// after picture header
	    	fprintf( stdout, "after picture header\n" );
		EXIT();
	}
	return 0;
}

int AVS_user_data( FILE *fp, unsigned char buffer[] )
{
	fprintf( stdout, "AVS_user_data() : (%s)\n", AddrStr(g_addr-4) );
	fprintf( stdout, "**********************************************\n" );
 	InitBitStream();

	int state=0;
	int userCount=0;
	while( 1 )
	{
	    int next_bits8;
	    next_bits8 = GetBitStream( fp, 8 );
#if 0
	    fprintf( stdout, "state=%d, next_bits8=%02X\n",
		state, next_bits8 );
#endif
	    buffer[userCount++] = next_bits8;
	    switch( state )
	    {
	    case 0 :
		if( next_bits8==0 )
		    state++;
		else {
		    fprintf( stdout, "%02X ", next_bits8 );
		}
		break;
	    case 1 :
		if( next_bits8==0 )
		{
		    state++;
		} else {
		    fprintf( stdout, "00 %02X ", next_bits8 );
		    state = 0;
		}
		break;
	    case 2 :
		if( next_bits8==0 )
		{
		    fprintf( stdout, "00 " );
		} else if( next_bits8==1 )
		{
		    fprintf( stdout, "\n" );
		    fprintf( stdout, "UserData=[%s]\n", buffer );
		    buffer[0] = 0;
		    buffer[1] = 0;
		    buffer[2] = 1;
		    return 3;
		} else {
		    fprintf( stdout, "00 00 %02X ", next_bits8 );
		    state = 0;
		}
		break;
	    }
	}

	return 0;
}
int AVS_i_picture_header( FILE *fp, unsigned char buffer[] )
{
	fprintf( stdout, "AVS_i_picture_header() : (%s)\n", AddrStr(g_addr-4) );
	fprintf( stdout, "**********************************************\n" );
 	InitBitStream();

	int bbv_delay = GetBitStream( fp, 16 );
	fprintf( stdout, "bbv_delay=%04X\n", bbv_delay );
	int time_code_flag = GetBitStream( fp, 1 );
	if( time_code_flag )
	{
	    int time_code = GetBitStream( fp, 24 );
	    fprintf( stdout, "time_code=%06X\n", time_code );
	}
	int marker_bit = GetBitStream( fp, 1 );
	fprintf( stdout, "marker_bit=%d\n", marker_bit );
	int picture_distance = GetBitStream( fp, 8 );
	fprintf( stdout, "picture_distance=%d\n", picture_distance );

	if( AVS_low_delay )
	{
	    fprintf( stdout, "AVS_low_delay=%d\n", AVS_low_delay );
	    EXIT();
	}
	int progressive_frame = GetBitStream( fp, 1 );
	fprintf( stdout, "progressive_frame=%d\n", progressive_frame );
	int picture_structure = (-1);
	if( progressive_frame==0 )
	{
	    picture_structure = GetBitStream( fp, 1 );
	    fprintf( stdout, "picture_structure=%d\n", picture_structure );
	    AVS_PictureStructure = picture_structure;
	} else {
	    AVS_PictureStructure = 1;
	}
	int top_field_first = GetBitStream( fp, 1 );
	int repeat_first_field = GetBitStream( fp, 1 );
	AVS_fixed_picture_qp = GetBitStream( fp, 1 );
	int picture_qp = GetBitStream( fp, 6 );
	fprintf( stdout, "picture_qp     =%d\n", picture_qp      );
	fprintf( stdout, "top_field_first=%d\n", top_field_first );
	fprintf( stdout, "repeat_first_field=%d\n", repeat_first_field );
	if( progressive_frame==0 )
	{
	    if( picture_structure==0 )
	    {
		AVS_skip_mode_flag = GetBitStream( fp, 1 );
		fprintf( stdout, "skip_mode_flag = %d\n", AVS_skip_mode_flag );
	    }
	}
#if 0
	int reserved_bits = GetBitStream( fp, 4 );
#else
	GetBitStream( fp, 4 );
#endif
	int loop_filter_disable = GetBitStream( fp, 1 );
	fprintf( stdout, "loop_filter_disable=%d\n", loop_filter_disable );
	if( loop_filter_disable==0 )
	{
	    int loop_filter_parameter_flag = GetBitStream( fp, 1 );
	    fprintf( stdout, "loop_filter_parameter_flag=%d\n", 
	    	loop_filter_parameter_flag );
	    if( loop_filter_parameter_flag )
	    {
		int alpha_c_offset = GetBitStreamGolomb( fp );
		int beta_offset = GetBitStreamGolomb( fp );
		fprintf( stdout, "alpha_c_offset=%d\n", alpha_c_offset );
		fprintf( stdout, "beta_offset   =%d\n", beta_offset );
	    }
	}
	AVS_CurrentPicture=AVS_I_FRAME;

	return 0;
}

int AVS_block( FILE *fp, unsigned char buffer[], int i )
{
	fprintf( stdout, "AVS_block(%d)\n", i );
	int EOB=0;
	if( ((i< 6) && (AVS_MbCBP    & (1<<i)))
	||  ((i>=6) && (AVS_MbCBP422 & (1<<(i-6)))) )
	{
	    while( 1 )
	    {
		int trans_coefficient = GetBitStreamGolomb( fp );
		fprintf( stdout, "trans_coefficient=%d\n", trans_coefficient );
		if( trans_coefficient>=59 )
		{
		int escape_level_diff = GetBitStreamGolomb( fp );
		fprintf( stdout, "escape_level_diff=%d\n", escape_level_diff );
		}
		if( trans_coefficient==EOB )
		    break;
	    }
	}
	return 0;
}

int AVS_macroblock( FILE *fp, unsigned char buffer[] )
{
int i;
int mb_type = (-1);

	if( (AVS_PictureType!=0) 
	 || ((AVS_PictureStructure==0) 
	  && (AVS_MbIndex>=AVS_MbWidth*AVS_MbHeight/2)) )
	{
	    mb_type = GetBitStreamGolomb( fp );
	}
	if( AVS_CurrentPicture==AVS_I_FRAME )
	{
	    if( AVS_PictureStructure==1 )
	    {
		AVS_MbType = AVS_I_8x8;
		AVS_MvNum  = 0;
	    } else {
		if( AVS_MbIndex < AVS_MbWidth*AVS_MbHeight/2 )
		{
		    AVS_MbType = AVS_I_8x8;
		    AVS_MvNum = 0;
		} else {
		    if( AVS_skip_mode_flag==1 )
		    {
			AVS_MbTypeIndex = mb_type+1;
		    } else {
			AVS_MbTypeIndex = mb_type+0;
		    }
		}
	    }
	}
	if( (AVS_MbType!=AVS_P_SKIP) && (AVS_MbType!=AVS_B_SKIP) )
	{
	    if( AVS_MbType==AVS_B_8x8 )
	    {
	    	for( i=0; i<4; i++ )
		{
		    int mb_part_type = GetBitStream( fp, 2 );
		    fprintf( stdout, "mb_part_type=%d\n", mb_part_type );
		}
	    }
	    if( AVS_MbType==AVS_I_8x8 )
	    {
	    	for( i=0; i<4; i++ )
		{
		    int pred_mode_flag = GetBitStream( fp, 1 );
		    if( pred_mode_flag==0 )
		    {
			int intra_luma_pred_mode = GetBitStream( fp, 2 );
			fprintf( stdout, "intra_luma_pred_mode=%d\n",
				intra_luma_pred_mode );
		    }
		}
		int intra_chroma_pred_mode = GetBitStreamGolomb( fp );
		fprintf( stdout, "intra_chroma_pred_mode=%d\n",
			intra_chroma_pred_mode );
		if( AVS_chroma_format==2 )
		{
		    int intra_chroma_pred_mode_422 = GetBitStreamGolomb( fp );
		    fprintf( stdout, "intra_chroma_pred_mode_422=%d\n",
			    intra_chroma_pred_mode_422 );
		}
	    }
	    if(( (AVS_PictureType==1)
	     || ((AVS_PictureType==2) && (AVS_PictureStructure==0)))
	     && (AVS_picture_reference_flag==0) )
	    {
	    	for( i=0; i<AVS_MvNum; i++ )
		{
#if 1
		    int mb_reference_index = GetBitStream( fp, 1 );
#else
		    int mb_reference_index = GetBitStream( fp, 2 );
#endif
			fprintf( stdout, "mb_reference_index=%d\n",
				mb_reference_index );
		}
	    }
	    for( i=0; i<AVS_MvNum; i++ )
	    {
		int mv_diff_x = GetBitStreamGolomb( fp );
		int mv_diff_y = GetBitStreamGolomb( fp );
		fprintf( stdout, "(%d,%d)\n", mv_diff_x, mv_diff_y );
	    }
	    if( AVS_MbWeightingFlag==1 )
	    {
		int weighting_prediction = GetBitStream( fp, 1 );
		fprintf( stdout, "weighting_prediction = %d\n",
			weighting_prediction );
	    }

	    if( ((AVS_MbTypeIndex>=24) && (AVS_PictureType==2))
	     || ((AVS_MbTypeIndex>= 5) && (AVS_PictureType!=2)) )
	    {
		int cbp = GetBitStreamGolomb( fp );
		AVS_MbCBP = cbp;
	    }
	    if( AVS_chroma_format==2 )
	    {
		int cbp_422 = GetBitStreamGolomb( fp );
		AVS_MbCBP422 = cbp_422;
	    }
	    if( ((AVS_MbCBP>0)
	     || ((AVS_MbCBP422>0) && (AVS_chroma_format==2)))
	     && (AVS_fixed_picture_qp==0) )
	    {
		int mb_qp_delta = GetBitStreamGolomb( fp );
		fprintf( stdout, "mb_qp_delta = %d\n", mb_qp_delta );
	    }
	    for( i=0; i<6; i++ )
	    {
	    	AVS_block( fp, buffer, i );
	    }
	    if( AVS_chroma_format==2 )
	    {
	    	for( i=6; i<8; i++ )
		{
	    	AVS_block( fp, buffer, i );
		}
	    }
	}
	return 0;
}

int AVS_slice( FILE *fp, unsigned char buffer[], int slice_vertical_position )
{
	AVS_MbWeightingFlag = 0;
	AVS_MbRow = slice_vertical_position;
	AVS_MbIndex = AVS_MbRow*AVS_MbWidth;

	if( AVS_fixed_picture_qp==0 )
	{
	    int fixed_slice_qp = GetBitStream( fp, 1 );
	    int slice_qp = GetBitStream( fp, 6 );
	    fprintf( stdout, "fixed_slice_qp = %d\n", fixed_slice_qp );
	    fprintf( stdout, "slice_qp       = %d\n", slice_qp );
	}

fprintf( stdout, "AVS_PictureType=%d\n", AVS_PictureType );
fprintf( stdout, "AVS_PictureStructure=%d\n", AVS_PictureStructure );
fprintf( stdout, "AVS_MbIndex=%d\n", AVS_MbIndex );
fprintf( stdout, "AVS_MbWidth=%d\n", AVS_MbWidth );
fprintf( stdout, "AVS_MbHeight=%d\n", AVS_MbHeight );

	if( (AVS_PictureType!=0) 
	 || ((AVS_PictureStructure==0) 
	  && (AVS_MbIndex>=AVS_MbWidth*AVS_MbHeight/2)) )
	{
	    int slice_weighting_flag = GetBitStream( fp, 1 );
	    if( slice_weighting_flag==1 )
	    {
	    	int i;
		for( i=0; i<AVS_NumberOfReference; i++ )
		{
		    int luma_scale = GetBitStream( fp, 8 );
		    int luma_shift = GetBitStream( fp, 8 );
		    int marker_bit = GetBitStream( fp, 1 );
		    int chroma_scale = GetBitStream( fp, 8 );
		    int chroma_shift = GetBitStream( fp, 8 );
		    int marker_bit2= GetBitStream( fp, 1 );
		    fprintf( stdout, "luma_scale  =%d\n", luma_scale );
		    fprintf( stdout, "luma_shift  =%d\n", luma_shift );
		    fprintf( stdout, "marker_bit=%d\n", marker_bit );
		    fprintf( stdout, "chroma_scale=%d\n", chroma_scale );
		    fprintf( stdout, "chroma_shift=%d\n", chroma_shift );
		    fprintf( stdout, "marker_bit=%d\n", marker_bit2 );
		}
	    }
	    int mb_weighting_flag = GetBitStream( fp, 1 );
	    AVS_MbWeightingFlag = mb_weighting_flag;
	}
//	while( 1 )
	{
	    if( (AVS_PictureType!=0)
	     || ((AVS_PictureStructure==0) 
	      && (AVS_MbIndex>=AVS_MbWidth*AVS_MbHeight/2) ))
	    {
		if( AVS_skip_mode_flag==1 )
		{
		    int mb_skip_run = GetBitStreamGolomb( fp );
		    fprintf( stdout, "mb_skip_run=%d\n", mb_skip_run );
		}
	    }
	    if( AVS_MbIndex<(AVS_MbWidth*AVS_MbHeight) )
	    {
		AVS_macroblock( fp, buffer );
	    }
	}

	return 0;
}

int AnalyzeAVS( FILE *fp, FILE *pts_fp )
{
int bNoRead=0;
int eof=0;
unsigned char buffer[BUF_SIZE];
unsigned int ID;
int readed;
unsigned int PTSH, PTSL, DTSH, DTSL;
int readP=0;
unsigned int pes_addr=0xFFFFFFFF;
int i;
#define MAX_DATA_SIZE	1024*64
unsigned char dataBuf[MAX_DATA_SIZE];
int nAlarm=0;

	while( eof==0 )
	{
	    if( readP<4 )
	    {
	    	int size;
	    	size = 4-readP;
		readed = gread( &buffer[readP], 1, size, fp );
		if( readed<size )
		{
		    CannotRead( "Prefix" );
		    break;
		}
		readP+=size;
	    }
	    if( (buffer[0]==0x00)
	     && (buffer[1]==0x00)
	     && (buffer[2]==0x00) )
	    {
#if 1
		if ( buffer[3]==1 )
		{
		    if( bAlarmAVC )
		    {
		    if( nAlarm<10 )
		    fprintf( stdout, "File may be AVC ?  Add -AVC option\n" );
		    }
		    nAlarm++;
		}
#endif
	    	memcpy( &buffer[0], &buffer[1], 3 );
		readed = gread( &buffer[3], 1, 1, fp );
		if( readed<1 )
		{
		    CannotRead( "Prefix" );
		    break;
		}
		continue;
	    }

	    if( (buffer[0]!=0x00)
	     || (buffer[1]!=0x00)
	     || (buffer[2]!=0x01) )
	    {
	    	fprintf( stdout, 
		"AnalyzeAVS : Invalid Prefix %02X %02X %02X : (%s)\n",
			buffer[0], buffer[1], buffer[2], AddrStr(g_addr-4) );
		fprintf( stdout, "Search next start code\n" );
//	exit( 1 );
		while( 1 )
		{
		    memcpy( &buffer[0], &buffer[1], 3 );
		    readed = gread( &buffer[3], 1, 1, fp );
		    if( readed<1 )
		    {
		    	fprintf( stdout, "EOF\n" );
			EXIT();
		    }
		    if( (buffer[0]==0)
		     && (buffer[1]==0)
		     && (buffer[2]==1) )
		     {
		     	fprintf( stdout, "00 00 01 : Addr=0x%llX\n", g_addr );
		     	break;
		     }
		}
	    }
#if 0
	    for( i=0; i<4; i++ )
	    {
		fprintf( stdout, "%02X ", buffer[i] );
	    }
	    fprintf( stdout, ": %llX", g_addr );
	    fprintf( stdout, "\n" );
#endif
	    // 00 00 01 XX(ID)
	    ID = buffer[3];
	    if( (ID>=0xB0) || bDumpSlice )
	    {
		if( bShowSyntax )
		{
		    for( i=0; i<4; i++ )
		    {
			fprintf( stdout, "%02X ", buffer[i] );
		    }
		    fprintf( stdout, ": " );
		    fprintf( stdout, "\n" );
		}
    //	    fprintf( stdout, "ID = %02X\n", ID );
	    }
	    readP = 0;
// --------------------------------------------------
	    if( bShowSyntax )
	    if( bNoRead )
	    {
		fprintf( stdout, "%02X %02X %02X %02X : ",
		    buffer[0], buffer[1], buffer[2], buffer[3] );
		fprintf( stdout, "\n" );
	    }
	    bNoRead=0;
	    int pes_len=(-1);
	    pes_addr = g_addr-4;
	    if( (ID>=0x00) && (ID<=0xAF) )
	    {	// slice_start_code
		fprintf( stdout, "%02X : slice_start_code\n", ID );
		readP = AVS_slice( fp, buffer, ID );
	    } else if( (ID>=0xE0) && (ID<=0xEF) )
	    {	// Video PES header
		pes_len = PES_header( fp, buffer, bDisplayTS, 
		    &PTSH, &PTSL, &DTSH, &DTSL );
		{
		    if( PTSH==0xFFFFFFFF )
		    {
			fprintf( stdout, "ID=%02X : PES_header(%s)\n", 
			    ID, AddrStr2(pes_addr) );
		    } else {
			int hour, min, sec, msec;
			TcTime32( PTSH, PTSL, &hour, &min, &sec, &msec );
			fprintf( stdout, 
		    "ID=%02X : PES_header(%s) PTS=%2d:%02d:%02d.%03d\n", 
			    ID, AddrStr2(pes_addr), hour, min, sec, msec );
		    }
		}
	    } else if( (ID>=0xC0) && (ID<=0xCF) )
	    {	// Audio PES header
		int a0 = g_addr;
		pes_len = PES_header( fp, buffer, bDisplayTS,
		    &PTSH, &PTSL, &DTSH, &DTSL );
		if( PTSH==0xFFFFFFFF )
		{
		    fprintf( stdout, "ID=%02X : PES_header(%s)\n", 
			ID, AddrStr2(pes_addr) );
		} else {
		    int hour, min, sec, msec;
		    TcTime32( PTSH, PTSL, &hour, &min, &sec, &msec );
		    fprintf( stdout, 
		"ID=%02X : PES_header(%s) PTS=%2d:%02d:%02d.%03d\n", 
			ID, AddrStr2(pes_addr), hour, min, sec, msec );
		}
		fprintf( pts_fp, "%8X %8llX : %4X %08X, %4X %08X\n",
			pes_addr, SrcAddr(pes_addr,0),
			(UINT)(PTSH & 0xFFFF), PTSL, 
			(UINT)(DTSH & 0xFFFF), DTSL );
		int a1 = g_addr;
		int readSize = pes_len-(a1-a0)+2;
#if 0
		fprintf( stdout, "pes_len=%d, a1-a0=%d, read=%d\n",
		    pes_len, a1-a0, readSize );
#endif
		if( readSize>=MAX_DATA_SIZE )
		{
		    fprintf( stdout, "Too large data (0x%X)\n",
			    readSize );
		    exit( 1 );
		}
		readed = gread( dataBuf, 1, readSize, fp );
	    } else {
		switch( ID )
		{
		case 0xB0 :	// video_sequence_start_code
		    fprintf( stdout, "%02X : video_sequence_start_code\n", ID );
		    readP = AVS_sequence_header( fp, buffer );
		    break;
#if 0
		case 0xB1 :	// video_sequence_end_code
		    fprintf( stdout, "video_sequence_end_code\n" );
		    break;
#endif
		case 0xB2 :	// User_data_start_code
		    fprintf( stdout, "%02X : User_data_start_code\n", ID );
		    readP = AVS_user_data( fp, buffer );
		    break;
		case 0xB3 :	// i_picture_start_code
		    fprintf( stdout, "%02X : i_picture_start_code\n", ID );
		    readP = AVS_i_picture_header( fp, buffer );
		    AVS_PictureType = 0;
		    AVS_NumberOfReference = 1;
		    break;
		case 0xB5 :	// 
		    fprintf( stdout, "%02X : extension_start_code\n", ID );
		    readP = AVS_extension_data( fp, buffer, 0 );
		    break;
		case 0xB6 :	// 
		    fprintf( stdout, "%02X : pb_picture_start_code\n", ID );
		    break;
		default :
		    fprintf( stdout, 
			"Not implemented (%02X) in AnaylzeAVS(%s)\n",
			    ID, AddrStr(pes_addr) );
		    exit( 1 );
		    break;
		}
	    }
	}
	fprintf( stdout, "nSequence=%d\n", nSequence );
	fprintf( stdout, "nSequenceExtension=%d\n", nSequenceExtension );
	fprintf( stdout, "nSequenceDisplayExtension=%d\n", 
		nSequenceDisplayExtension );
#if 0
	if( g_update_addr[0]==0xFFFFFFFF )
	{
	    fprintf( stdout, "No g_update\n" );
	} else {
	    fprintf( stdout, "addr=%X\n", g_update_addr[0] );
	    fprintf( stdout, "bits=%X\n", g_update_bits[0] );
	    fprintf( stdout, "size=%X\n", g_update_size[0] );
	    fprintf( stdout, "data=%X\n", g_update_data[0] );
	}
#endif
	return 0;
}
// --------------------------------------------------------------------
// end of Analayze AVS
// --------------------------------------------------------------------

static void ShowHeader( unsigned char *buffer )
{
int i;
	for( i=0; i<4; i++ )
	{
	    fprintf( stdout, "%02X ", buffer[i] );
	}
	fprintf( stdout, " : (%s) : ", AddrStr(g_addr) );
}

int nPict=0;
int nPes=0;
int oPes=(-1);
int nShc=0;
int FirstDts=(-1);
int FirstPts=(-1);

int ParseHeader( FILE *fp, unsigned char *buffer, FILE *pts_fp )
{
int ID=buffer[3];
unsigned int PTSH=0xFFFFFFFF, PTSL=0xFFFFFFFF;
unsigned int DTSH=0xFFFFFFFF, DTSL=0xFFFFFFFF;
int HeaderAddr = (-1);
int NoPtsAddr  = (-1);
	if( ID<0x80 )
	{
	    if( ID==0 )
	    {
	    	int TR, PCT;
		HeaderAddr = g_addr-4;
		ShowHeader( buffer );
		InitBitStream( );
		TR  = GetBitStream( fp, 10 );
		PCT = GetBitStream( fp, 3 );
		fprintf( stdout, "TR=%4d, PCT=%d : Pict(%4d)\n", 
			TR, PCT, nPict );
		if( (PCT>=1) && (PCT<=3) )
		{
		    nPict++;
		} else {
		    fprintf( stdout, "Error\n" );
		    int RST= GetBitStream( fp, 3 );
		    buffer[4] = TR>>2;
		    buffer[5] = ((TR&3)<<6) 
		    	       | (PCT<<3)
			       | (RST<<0);
		    return 2;
		}
	    } else {
	    	if( oPes!=nPes )
		{
		    HeaderAddr = g_addr-4;
		    ShowHeader( buffer );
		    fprintf( stdout, "\n" );
		}
		oPes=nPes;
	    }
	} else {
	    HeaderAddr = g_addr-4;
	    ShowHeader( buffer );
	    oPes=nPes;
	    int bPTS=0;
	    switch( ID )
	    {
	    case 0xE0 :	// PES_header
	    case 0xE1 :	//
	    case 0xE2 :	//
	    case 0xE3 :	//
	    case 0xE4 :	//
	    case 0xE5 :	//
	    case 0xE6 :	//
	    case 0xE7 :	//
	    case 0xE8 :	//
	    case 0xE9 :	//
	    case 0xEA :	//
	    case 0xEB :	//
	    case 0xEC :	//
	    case 0xED :	//
	    case 0xEE :	//
	    case 0xEF :	//

	    case 0xC0 :	// PES_header : Audio

		PES_header( fp, buffer, 0, &PTSH, &PTSL, &DTSH, &DTSL );
		if( PTSH!=0xFFFFFFFF )
		{
		    fprintf( stdout, "PTS=%4X %06X ", PTSH, PTSL );
		    bPTS=1;
		}
		if( DTSH!=0xFFFFFFFF )
		    fprintf( stdout, "DTS=%4X %06X", DTSH, DTSL );
		else {
		    if( bPTS )
		    {
		    DTSH=PTSH;
		    DTSL=PTSL;
		    fprintf( stdout, "DTS=%4X %06X", PTSH, PTSL );
		    }
		}
		fprintf( stdout, ": Pes(%4d)", nPes );
		fprintf( pts_fp, "%8X %8llX : %4X %08X, %4X %08X\n",
			HeaderAddr, SrcAddr(HeaderAddr,0),
			(UINT)(PTSH & 0xFFFF), PTSL, 
			(UINT)(DTSH & 0xFFFF), DTSL );
		if( bPTS )
		{
		    if( FirstPts==(-1) )
		    	FirstPts = PTSL;
		    if( FirstDts==(-1) )
		    {
		    if( DTSH!=0xFFFFFFFF )
		    	FirstDts = DTSL;
		    else
		    	FirstDts = PTSL;
		    }
		    if( fromDts!=INVALID_OFFSET )
		    {
		    	int valid=0;
		    	if( (DTSL-FirstDts)>=fromDts )
			{
			    if( toDts==INVALID_OFFSET )
			    {
			    	valid = 1;
			    } else {
			    	if( (DTSL-FirstDts)<toDts )
				    valid = 1;
			    }
			}
#if 0
			fprintf( stdout, 
			"\nvalid=%d\n %8X %8X %8X", 
			valid, fromDts, DTSL, toDts );
#endif
			if( valid )
			{
			    if( validStart<0 )
			    {
				validStart = HeaderAddr;
				if( NoPtsAddr!=(-1) )
				    validStart = NoPtsAddr;
//			    fprintf( stdout, "validStart=%X\n", validStart );
			    }
			} else {
			    if( validStart>=0 )
			    	if( validEnd<0 )
				{
				    validEnd = HeaderAddr;
//			    fprintf( stdout, "validEnd=%X\n", validEnd );
				}
			}
		    }
		    NoPtsAddr = (-1);
		} else {
		    if( NoPtsAddr==(-1) )
			NoPtsAddr = HeaderAddr;
		}
		nPes++;
		es_addr = g_addr;	// 2012.9.3
		break;
	    case 0xB2 :	// UserData
	    	fprintf( stdout, "UserData" );
		break;
	    case 0xB3 :	// SHC
	    	fprintf( stdout, "SHC (%4d)", nShc );
		nShc++;
		break;
	    case 0xB5 :	// ESC
	    	fprintf( stdout, "ESC" );
		break;
	    case 0xB8 :	// GSC
	    	fprintf( stdout, "GSC" );
		break;
	    case 0x00 :	// PSC
	    	fprintf( stdout, "PSC" );
		break;
	    default :
		fprintf( stdout, 
		    "Not implemented (%02X) in Header\n", ID );
//		return 1;
		break;
	    }
	    fprintf( stdout, "\n" );
	}
	return 0;
}

int AnalyzeHeader( FILE *fp, FILE *pts_fp )
{
int eof=0;
unsigned char buffer[BUF_SIZE];
int ID;
int readed;
int nPos=0;
int ret=0;
	nPict=0;
	nPes =0;
	nShc =0;

	while( eof==0 )
	{
	    readed = gread( &buffer[nPos], 1, 1, fp );
	    if( readed<1 )
	    {
		fprintf( stdout, "EOF@(%s)\n", AddrStr(g_addr) );
		break;
	    }
	    switch( nPos )
	    {
	    case 0 :
		if( buffer[nPos]==0 )	// 00
		    nPos++;
		break;
	    case 1 :
		if( buffer[nPos]==0 )	// 00 00
		    nPos++;
		else
		    nPos=0;
		break;
	    case 2 :
		if( buffer[nPos]==0 )	// 00 00 00
		{
		} else if( buffer[nPos]==1 )	// 00 00 01
		    nPos++;
		else
		    nPos=0;
		break;
	    case 3 :
	    	ID = buffer[nPos];
		ret = ParseHeader( fp, buffer, pts_fp );
		if( ret>0 )	// Error
		{
		    buffer[0] = buffer[0+ret];
		    buffer[1] = buffer[1+ret];
		    buffer[2] = buffer[2+ret];
		    buffer[3] = buffer[3+ret];
		    if( buffer[1]==0 )
		    {
		    	if( buffer[2]==0 )
			{
			    if( buffer[3]==1 )
			    	nPos=3;
			    else
			    	nPos=0;
			} else {
			    	nPos=0;
			}
		    } else if( buffer[2]==0 )
		    {
		    	if( buffer[3]==0 )
			    nPos=2;
			else
			    nPos=0;
		    } else if( buffer[3]==0 )
		    	nPos=1;
		    else
		    	nPos=0;
		    fprintf( stdout, "%02X %02X %02X %02X\n",
		    	buffer[0],
		    	buffer[1],
		    	buffer[2],
		    	buffer[3] );
		} else {
		    nPos=0;
		}
		break;
	    }
	}

	fprintf( stdout, "fromDts   =%8X(%d)\n", fromDts, fromDts );
	fprintf( stdout, "toDts     =%8X(%d)\n", toDts  , toDts   );
	fprintf( stdout, "validStart=%X\n", validStart );
	fprintf( stdout, "validEnd  =%X\n", validEnd   );
	return 0;
}

// ------------------------------------------------------------

void TS_addr_Dump( long *TS_addr, int nTS )
{
int n;
    fprintf( stdout, "TS_addr\n" );
	for( n=0; n<nTS; n++ )
	{
	    fprintf( stdout, "TS(%5d) : %8X\n", n, (UINT)TS_addr[n] );
	}
}

int Remux( char *srcFilename, char *outFilename, 
	char *textFilename, FILE *pts_fp, int Threshold )
{
int index;

FILE *ts_fp = fopen( "ts.bin", "wb" );

#define MAX_PCR 1024*1024*1
long long *PCR_addr;
long long *PCR_data;
int nPCR=0;
#define MAX_DTS 1024*1024*1
long long *DTS_addr;
long long *DTS_data;
int nDTS=0;
// 16M*188=3G max
#define MAX_TS 1024*1024*16
unsigned long long *TS_addr = NULL;
int nTS=0;

#define MAX_DATA 1024*1024*8
unsigned long long *DATA_addr = NULL;
long long *DATA_pts  = NULL;
int nDATA=0;
#define MAX_NULL 1024*1024*8
unsigned long long *NULL_addr = NULL;
int nNULL=0;

int n;

	fprintf( stdout, "Remux(%s->%s)\n", srcFilename, outFilename );
	fprintf( stdout, "Threshold=%d(%d)\n", Threshold, Threshold/90000 );

	PCR_addr = (long long *)malloc( MAX_PCR*8 );
	if( PCR_addr==NULL )
	{
	    fprintf( stdout, "Can't alloc PCR_addr\n" );
	    exit( 1 );
	}
	PCR_data = (long long *)malloc( MAX_PCR*8 );
	if( PCR_data==NULL )
	{
	    fprintf( stdout, "Can't alloc PCR_data\n" );
	    exit( 1 );
	}
	DTS_addr = (long long *)malloc( MAX_DTS*8 );
	if( DTS_addr==NULL )
	{
	    fprintf( stdout, "Can't alloc DTS_addr\n" );
	    exit( 1 );
	}
	DTS_data = (long long *)malloc( MAX_DTS*8 );
	if( DTS_data==NULL )
	{
	    fprintf( stdout, "Can't alloc DTS_data\n" );
	    exit( 1 );
	}
	TS_addr = (unsigned long long *)malloc( MAX_TS*8 );
	if( TS_addr==NULL )
	{
	    fprintf( stdout, "Can't alloc TS_addr\n" );
	    exit( 1 );
	}
	DATA_addr = (unsigned long long *)malloc( MAX_DATA*8 );
	if( DATA_addr==NULL )
	{
	    fprintf( stdout, "Can't alloc DATA_addr\n" );
	    exit( 1 );
	}
	DATA_pts  = (long long *)malloc( MAX_DATA*8 );
	if( DATA_pts==NULL )
	{
	    fprintf( stdout, "Can't alloc DATA_pts\n" );
	    exit( 1 );
	}
	NULL_addr = (unsigned long long *)malloc( MAX_NULL*8 );
	if( NULL_addr==NULL )
	{
	    fprintf( stdout, "Can't alloc NULL_addr\n" );
	    exit( 1 );
	}

	fprintf( stdout, "Copy to ts.bin with erasing ts file\n" );
	if( ts_fp==NULL )
	{
	    fprintf( stdout, "Can't open ts.bin\n" );
	    exit( 1 );
	}
	FILE *src_fp = fopen( srcFilename, "rb" );
	if( src_fp==NULL )
	{
	    fprintf( stdout, 
		"Remux(srcFilename) Can't open %s\n", srcFilename );
	    exit( 1 );
	}
	FILE *outfile = fopen( outFilename, "wb" );
	if( outfile==NULL )
	{
	    fprintf( stdout, "Remux(outfile) Can't open %s\n", outFilename );
	    exit( 1 );
	}
	int total = 0;
#define TS_BUF_SIZE 188*1024
	unsigned char TsBuffer[TS_BUF_SIZE];
	unsigned char NullBuffer[192];
	int size, readed, written=0;

	// Extract ts
	memset( NullBuffer, 0xFF, 192 );
	NullBuffer[0] = 0x47;
	NullBuffer[1] = 0x1F;
	NullBuffer[2] = 0xFF;
	NullBuffer[3] = 0x1F;
	int validText=1;
	if( ReadAddrFile( textFilename, 
		srcFilename, DATA_addr, NULL, &nDATA, MAX_DATA )<0 )
	{
	    validText=0;
//	    exit( 1 );
	}
// -------------------------------------------------------------------
// Copy XX.ts to ts.bin and overwrite with null packet
// -------------------------------------------------------------------
	if( validText )
	{
	    for( index=0; index<nDATA; index++ )
	    {
		int ts_addr = DATA_addr[index];
		if( ts_addr>total )
		{
    //		fprintf( stdout, "Copy(%X:%X)\n", ts_addr-total, total );
		    while( total<ts_addr )
		    {
			int size = ts_addr-total;
			if( size>TS_BUF_SIZE )
			    size = TS_BUF_SIZE;
			readed = fread( TsBuffer, 1, size, src_fp );
			if( readed<size )
			{
			    fprintf( stdout, "readed=%d/%d@0x%X\n", 
				readed, size, total );
			    exit( 1 );
			}
			int rest = size;
			while( rest>0 )
			{
			    written = fwrite( TsBuffer, 1, rest, outfile );
			    if( written<1 )
			    {
				fprintf( stdout, "TsBuffer:written=%d/%d@0x%X\n", 
				    written, rest, total );
				exit( 1 );
			    }
			    rest -= written;
			}
			total += readed;
		    }
		}
		size = 188;
    //	    fprintf( stdout, "Write(%X:%X)\n", size, total );
		readed = fread( TsBuffer, 1, size, src_fp );
		if( readed<size )
		{
		    fprintf( stdout, "readed=%d/%d@0x%X\n", 
			readed, size, total );
		    exit( 1 );
		}
		written = fwrite( TsBuffer, 1, size, ts_fp );
		if( written<size )
		{
		    fprintf( stdout, "TsBuffer:written=%d/%d@0x%X\n", 
			written, size, total );
		    exit( 1 );
		}
		written = fwrite( NullBuffer, 1, size, outfile );
		if( written<size )
		{
		    fprintf( stdout, "NullBuffer:written=%d/%d@0x%X\n", 
			written, size, total );
		    exit( 1 );
		}
		total += readed;
	    }
	    // Rest
	    {
		size = TS_BUF_SIZE;
		while( written>0 )
		{
		    readed = fread( TsBuffer, 1, size, src_fp );
		    written = fwrite( TsBuffer, 1, readed, outfile );
		}
	    }
	} else {
	    while( 1 )
	    {
	    	int size=1024*4;
		int readed;
		readed  = fread ( TsBuffer, 1, size, src_fp );
		if( readed<1 )
		    break;
		written = fwrite( TsBuffer, 1, readed, outfile );
	    }
	}
	fclose( src_fp );
	fclose( outfile );
	fclose( ts_fp );

	fprintf( stdout, "Copy done\n" );

	fprintf( stdout, "nTS=%d\n", nTS );

#if 0
	TS_addr_Dump( TS_addr, nTS );
#endif
// ----------------------------------------------------------
//  Read PTS file
// ----------------------------------------------------------
	// DTS
	fprintf( stdout, "Read pts file\n" );
	while( 1 )
	{
	    if( fgets( (char *)TsBuffer, 1024, pts_fp )==NULL )
	    {
	    	fprintf( stdout, "EOF\n" );
		break;
	    }
	    int addr1, ptsAddr;
	    int ptsH, ptsL;
	    int dtsH, dtsL;
	    sscanf( (char *)TsBuffer, "%X %X : %X %X, %X %X",
	    	&addr1, &ptsAddr, &ptsH, &ptsL, &dtsH, &dtsL );
	    DTS_addr[nDTS] = ptsAddr;
//	    DTS_data[nDTS] = ptsL;
	    int DTS = dtsL;
	    if( DTS==0xFFFFFFFF )
	    	DTS = ptsL;
	    DTS_data[nDTS] = DTS;	 // use DTS
	    nDTS++;
	    if( nDTS>=MAX_DTS )
	    {
	    	fprintf( stdout, "Too many DTS (%d)\n", nDTS );
		exit( 1 );
	    }
//	    fprintf( stdout, "%8X : %8X\n", addr2, ptsL );
	}
	fprintf( stdout, "Read done (%d DTS)\n", nDTS );

// ----------------------------------------------------------
//  Read PCR file
// ----------------------------------------------------------
	fprintf( stdout, "Read PCR file\n" );
	FILE *pcr_fp = fopen( "PCR.txt", "r" );
	if( pcr_fp==NULL )
	{
	    fprintf( stdout, "Can't open PCR.txt\n" );
	    exit( 1 );
	}
	while( 1 )
	{
	    if( fgets( (char *)TsBuffer, 1024, pcr_fp )==NULL )
	    {
	    	fprintf( stdout, "EOF\n" );
		break;
	    }
	    int addr, pid, pcrH, pcrL;
	    unsigned long long PCR;
	    sscanf( (char *)TsBuffer, "%X : %X : %X,%X %llX",
	    	&addr, &pid, &pcrH, &pcrL, &PCR );
	    PCR_addr[nPCR] = addr;
	    PCR_data[nPCR] = PCR;
	    nPCR++;
	    if( nPCR>=MAX_PCR )
	    {
	    	fprintf( stdout, "Too many PCR (%d)\n", nPCR );
		exit( 1 );
	    }
//	    fprintf( stdout, "%8X : %9llX\n", addr, PCR );
	}
	fclose( pcr_fp );
	fprintf( stdout, "Read done (%d PCR)\n", nPCR );

	// NULL(PID=0x1FFF) data
	if( ReadAddrFile( "PES-1FFF.txt", 
		srcFilename, NULL_addr, NULL, &nNULL, 
		MAX_NULL )<0 )
	    exit( 1 );
#if 0
	TS_addr_Dump( TS_addr, nTS );
#endif
	int nD=0, nN=0;
	long long DataAddr, NullAddr;
	for( n=0; ; n++ )
	{
	    if( (nD>=nDATA) && (nN>=nNULL) )
	    	break;
	    if( nD<nDATA )
	    	DataAddr = DATA_addr[nD];
	    else
	    	DataAddr = 0xFFFFFFFFFFll;
	    if( nN<nNULL )
	    	NullAddr = NULL_addr[nN];
	    else
	    	NullAddr = 0xFFFFFFFFFFll;
	    if( DataAddr<NullAddr )
	    {
	    	TS_addr[nTS] = DataAddr;
		nD++;
	    } else if( DataAddr>NullAddr )
	    {
	    	TS_addr[nTS] = NullAddr;
		nN++;
	    } else
	    	break;
//	    fprintf( stdout, "%5d : %8X\n", nTS, TS_addr[nTS] );
	    nTS++;
	}
#if 0
	TS_addr_Dump( TS_addr, nTS );
#endif
// ----------------------------------------------------------
//  Write new TS file
// ----------------------------------------------------------
	fprintf( stdout, "Update\n" );
	// Make TS_addr and PCR
	int nPcr=0;
//	unsigned long long PCR = PCR_data[0];
	long long PCR = PCR_data[0];
	n=0;
	int pk, pt=0;
	int nFree=0;
	long long DTS = DTS_data[0];
	int fd;
	int mapSize = 4096*2;
	ts_fp = fopen( "ts.bin", "rb" );
	if( ts_fp==NULL )
	{
	    fprintf( stdout, "Can't open ts.bin\n" );
	    exit( 1 );
	}
	fd = open( outFilename, O_RDWR );
	if( fd==(-1) )
	{
	    fprintf( stdout, 
	    "Remux(outFilename) Can't open %s\n", outFilename );
	    exit( 1 );
	}
	unsigned int WrAddr = 0;
	int nCont=0;
	int L_Free=(-2);
	for( pk=0; pk<nDATA; pk++ )
	{
	    if( bDebugRemux )
		fprintf( stdout, "DATA(%6d): %8llX : ", pk, DATA_addr[pk] );
	    while( (DTS_addr[pt]-0x20)<DATA_addr[pk] )
	    {
		pt++;
		if( pt>=nDTS )
		    break;
		DTS = DTS_data[pt];
//		fprintf( stdout, "%8X %9llX\n", DTS_addr[pt], DTS );
	    }
	    unsigned long nAddr = DTS_addr[pt+1]; 
	    int FrameSize = nAddr-DTS_addr[pt];
	    if( FrameSize<0 )
	    	FrameSize = 0;
	    int deltaPCR = FrameSize * (PCR_data[10]-PCR_data[0])
	                             / (PCR_addr[10]-PCR_addr[0]);
	    fprintf( stdout, "DTS=%9llX (%llX) (%8X,%6d)\n",
	    	DTS, DTS-Threshold, FrameSize, deltaPCR );
	    // Search free ts area
	    int bNoSpace=0;
	    while( 1 )
	    {
		if( nFree>=nTS )
		{
		    fprintf( stdout, "No more free area (%d)\n", nFree );
		    bNoSpace=1;
		    break;
		}
//		fprintf( stdout, "%6d : %10d : ", nFree, TS_addr[nFree] );
		while( PCR_addr[nPcr]<TS_addr[nFree] )
		{
#if 0
		    fprintf( stdout, 
			"nPcr=%5d, PCR_addr=%8X, TS_addr=%8X\n",
				nPcr, PCR_addr[nPcr], TS_addr[nFree] );
#endif
		    nPcr++;
		    if( nPcr>=nPCR )
			break;
		    PCR = PCR_data[nPcr];
		}
#if 0
	    	fprintf( stdout, "nFree=%d, nPcr=%5d, nPCR=%5d, PCR=%9llX\n", 
			nFree, nPcr, nPCR, PCR );
		fflush( stdout );
#endif
		if( PCR>=(DTS-Threshold-deltaPCR) )
		{
		    if( bDebugRemux )
		    {
			fprintf( stdout, "TS(%6d) : %9llX : ", 
			    nFree, TS_addr[nFree] );
			fprintf( stdout, "PCR=%9llX,DTS=%8X\n", 
				PCR, (UINT)DTS );
		    }
		    if( (nFree-L_Free)==1 )
		    	nCont++;
		    else
		    	nCont=0;
		    L_Free = nFree;
//		    if( nCont>128 )
//		    if( nCont>32 )
		    if( nCont>16 )
		    {
		    	fprintf( stdout, "Insert (nCont=%d)\n", nCont );
		    	nFree++;
		    }
		    WrAddr = TS_addr[nFree];
		    nFree++;
		    break;
		}
		nFree++;
	    }
	    if( bNoSpace )
	    	break;
	    if( bDebugRemux )
		fprintf( stdout, "WrAddr(%8X)\n", WrAddr );
	    {
		unsigned char *data;
	    // mmap
		data = (unsigned char *)mmap( (caddr_t)0, mapSize, 
				    PROT_READ | PROT_WRITE, MAP_SHARED,
				    fd, WrAddr & 0xFFFFF000 );
		if( data==(unsigned char *)-1) 
		{
		    fprintf( stdout, "Can't mmap\n" );
		    exit( 1 );
		}
		int offset = WrAddr & 0xFFF;
		unsigned char *ptr = &data[offset];
#if 0
		fprintf( stdout, "Write(%8X:%8X)\n", 
			WrAddr & 0xFFFFF000,
			offset );
#endif
		readed = fread( ptr, 1, 188, ts_fp );
		if( readed<188 )
		{
			fprintf( stdout, "readed=%d\n", readed );
		}
	    // munmap
		if( munmap( data, mapSize )==(-1) )
		{
		    fprintf( stdout, "Can't munmap\n" );
		    exit( 1 );
		}
	    }
	}
	close( fd );

	// Rest
	int rest=0;
	if( pk<nDATA )
	{
	    FILE *append = fopen( outFilename, "ab" );
	    if( append==NULL )
	    {
	    	fprintf( stdout, "Can't append\n" );
		exit( 1 );
	    }
	    for( ; pk<nDATA; pk++ )
	    {
	    	int written;
		unsigned char restBuf[188];
		readed = fread( restBuf, 1, 188, ts_fp );
		if( readed<188 )
		{
		    fprintf( stdout, "rest=%d\n", rest );
		    break;
		}
		rest+=readed;
		written = fwrite( restBuf, 1, 188, append );
		if( written<188 )
		{
		    fprintf( stdout, "Can't write(%X)\n", rest );
		}
		written = fwrite( NullBuffer, 1, 188, append );
		if( written<188 )
		{
		    fprintf( stdout, "Can't write(%X)\n", rest );
		}
	    }
	    fclose( append );
	}
	fprintf( stdout, "rest=%X\n", rest );
	fclose( ts_fp );
	fprintf( stdout, "done\n" );

	fprintf( stdout, "nFree=%d, nTS=%d\n", nFree, nTS );

	return 0;
}

// -------------------------------------------------------------------------

#define MODE_MPG	0
#define MODE_AVC	1
#define MODE_AVS	2
#define MODE_HEADER	8
#define MODE_REMUX	9

// ---------------------------------------------
// header item
//
/*
#define ID_HSV		 1
#define ID_VSV		 2
#define ID_ARI		 3
#define ID_FRC		 4
#define ID_PALI		 5
#define ID_PS		 6
#define ID_CP		 7
#define ID_TC		 8
#define ID_MXC		 9
#define ID_DHS		10
#define ID_DVS		11
// for AVC
#define ID_SARW		12
#define ID_SARH		13
#define ID_LVL		14
//
#define ID_PANL		15
#define ID_PANR		16
#define ID_PANT		17
#define ID_PANB		18

#define ID_OVSP		19	// present_flag
#define ID_OVSA		20	// appropriate_flag

#define ID_MAX		21
*/
typedef struct HeaderItem {
int id;
char name[10];
} headerItem_t;

static struct HeaderItem headerItem[] = {
{ID_HSV,	"HSV"	},		// horizontal_size_value
{ID_VSV,	"VSV"	},		// vertical_size_value
{ID_ARI,	"ARI"	},		// aspect_ratio_information
{ID_FRC,	"FRC"	},		// frame_rate
{ID_PALI,	"PALI"	},		// profile_and_level_indication
{ID_PS,		"PS"	},		// prograssive_sequence
{ID_CP,		"CP"	},		// colour_primaries
{ID_TC,		"TC"	},		// transfer_characteristics
{ID_MXC,	"MXC"	},		// matrix_coefficients
{ID_DHS,	"DHS"	},		// display_horizontal_size
{ID_DVS,	"DVS"	},		// display_vertical_size

{ID_SARW,	"SARW"	},		// sar_width
{ID_SARH,	"SARH"	},		// sar_height
{ID_LVL,	"LVL"	},		// level

{ID_PANL,	"PANL"	},		// pan_left_offset
{ID_PANR,	"PANR"	},		// pan_right_offset
{ID_PANT,	"PANT"	},		// pan_top_offset
{ID_PANB,	"PANB"	},		// pan_bottom_offset

{ID_OVSP,	"OVSP"	},		// overscan_present_flag
{ID_OVSA,	"OVSA"	},		// overscan_appropriate_flag

{-1,		""	}
};
int bDebugUpdateBits = 0;

int UpdateBits( unsigned char buffer[], int bitOffset, int size, int data )
{
int i;
unsigned int inData, wrData, wrMask, outData;
unsigned int MaskA[32];
unsigned int MaskB[32];
int nByte = (bitOffset+size+7)/8;

	if( bDebugUpdateBits )
	fprintf( stdout, "nByte=%d\n", nByte );
	MaskA[1] = 0x80000000;
	MaskB[1] = 0x00000001;
	for( i=2; i<32; i++ )
	{
	    MaskA[i] = (MaskA[i-1]>>1) | 0x80000000;
	    MaskB[i] = (MaskB[i-1]<<1) | 0x00000001;
	}
	inData = (buffer[0]<<24)
	       | (buffer[1]<<16)
	       | (buffer[2]<< 8)
	       | (buffer[3]<< 0);
	wrData = (data&MaskB[size])<<(32-bitOffset-size);
	wrMask =       MaskA[size] >>bitOffset;
	outData= (inData & (wrMask^0xFFFFFFFF))
		| wrData;
	unsigned int nOutData = outData;
	for( i=0; i<nByte; i++ )
	{
	    buffer[i] = (nOutData>>24)&0xFF;
	    if( bDebugUpdateBits )
	    fprintf( stdout, "%08X %02X\n", nOutData, buffer[i] );
	    nOutData=nOutData<<8;
	}
	if( bShowUpdate )
	{
	    fprintf( stdout, "UpdateBits(%d,%d,%d)\n", bitOffset, size, data );
	    fprintf( stdout, "inData =%8X\n", inData );
	    fprintf( stdout, "wrData =%8X\n", wrData );
	    fprintf( stdout, "wrMask =%8X\n", wrMask );
	    fprintf( stdout, "outData=%8X\n", outData );
	}
	if( bDebugUpdateBits )
	{
	    for( i=0; i<4; i++ )
	    {
		fprintf( stdout, "%02X ", buffer[i] );
	    }
	    fprintf( stdout, "\n" );
	}
	return 0;
}

int ReadByte( unsigned char *ptr, int addr, int bit )
{
	unsigned char *ubuffer = (unsigned char *)(ptr+(addr & 0x0FFF));
	int byteOffset = bit/8;
	unsigned char byte = ubuffer[byteOffset];
	return byte;
}

int ReadBit( unsigned char *ptr, int addr, int bit )
{
	int bitOffset  = bit%8;
	unsigned char byte = ReadByte( ptr, addr, bit );
/*
	fprintf( stdout, "ReadBit(%X,%d)=%02X,%d\n", 
		addr, bit, byte, bitOffset );
*/
	if( byte & (1<<(7-bitOffset)) )
	    return 1;
	else
	    return 0;
}

int WriteBit( unsigned char *ptr, int addr, int bit, int data )
{
	unsigned char *ubuffer = (unsigned char *)(ptr+(addr & 0x0FFF));
	int byteOffset = bit/8;
	int bitOffset  = bit%8;
	unsigned char byte = ubuffer[byteOffset];
	unsigned char Set  = (1<<(7-bitOffset));
	unsigned char Mask = (1<<(7-bitOffset)) ^ 0xFF;
	if( data )
	    byte = byte | Set;
	else
	    byte = byte & Mask;
	ubuffer[byteOffset] = byte;
/*
	fprintf( stdout, "WriteBit(%X,%d:%d) = %02X\n", 
		addr, bit, data, byte );
*/
	return 0;
}

void swap( unsigned int *value1, unsigned int *value2 )
{
unsigned int tmp = *value1;
	*value1 = *value2;
	*value2 = tmp;
}

int CopyFile( FILE *infile, FILE *outfile, int size )
{
unsigned char buffer[BUF_SIZE];
int bFull = 0;
	if( size<0 )
	    bFull = 1;
	int rest = size;
	while( (rest>0) || bFull )
	{
	    int readed, written;
	    int copySize=0;
	    if( bFull )
	    	copySize = BUF_SIZE;
	    else
		copySize = (rest>BUF_SIZE) ? BUF_SIZE
	    				   : rest;
	    readed  = fread ( buffer, 1, copySize, infile );
	    if( readed<1 )
	    {
	    	fprintf( stdout, "EOF\n" );
		break;
	    }
	    written = fwrite( buffer, 1, readed, outfile );
	    if( written<1 )
	    {
	    	fprintf( stdout, "Can't write(rest=%X)\n", rest );
		return -1;
	    }
	    rest -= written;
	}
	return 0;
}

// -------------------------------------------------------------------------

void pesUsage( char *prg )
{
	fprintf( stdout, "%s filename\n", prg );
	exit( 1 );
}

int pes( int argc, char *argv[] )
{
int i;
char *filename = NULL;
int args=0;
FILE *bin_fp = NULL;
FILE *pts_fp = NULL;
//unsigned char buffer[BUF_SIZE];
int addr=0;
int Error=0;
int nItem = (-1);
int mode = MODE_MPG;
int bEditDirect=0;
int Threshold=90000;	// 1sec

	fprintf( stdout, "pes start\n" );
	fflush( stdout );
//int Packets[MAX_PACKETS+4];	// 16bit
	Packets = (int *)malloc( 4*(MAX_PACKETS+4) );
	if( Packets==NULL )
	{
	    fprintf( stdout, "Can't malloc Packtes\n" );
	    exit( 1 );
	}
	for( i=0; i<(MAX_PACKETS+4); i++ )
	{
	    Packets[i] = 0;
	}
	Packets[MAX_PACKETS+0] = 0x12345678;
	Packets[MAX_PACKETS+1] = 0x9ABCDEF0;
	for( i=0; i<ID_MAX; i++ )
	{
	    items[i] = 0;
	    value[i] = (-1);
	}

#if 1
	init_AVC();
#else
	for( i=0; i<MAX_SPS; i++ )
	{
	    avc_nal_hrd_parameters_present_flag[i]=(-1);
	    avc_vcl_hrd_parameters_present_flag[i]=(-1);
	    avc_pic_struct_present_flag[i]=(-1);
	    avc_CpbDpbDelaysPresentFlag[i]=(-1);
	    PPStoSPS[i] = (-1);
	}
#endif
	g_update_addr = (unsigned int *)malloc( 4*MAX_UPDATE );
	g_update_bits = (unsigned int *)malloc( 4*MAX_UPDATE );
	g_update_size = (unsigned int *)malloc( 4*MAX_UPDATE );
	g_update_data = (unsigned int *)malloc( 4*MAX_UPDATE );

	g_update_addrS = (unsigned int *)malloc( 4*MAX_UPDATE );
	g_update_bit_S = (unsigned int *)malloc( 4*MAX_UPDATE );// remove start
	g_update_bit_E = (unsigned int *)malloc( 4*MAX_UPDATE );// remove end
	g_update_bit_T = (unsigned int *)malloc( 4*MAX_UPDATE );//structure end 
	g_update_mode  = (unsigned int *)malloc( 4*MAX_UPDATE );
	if( g_update_addrS==NULL )
	{
	    fprintf( stdout, "Can't malloc g_update_addrS\n" );
	    exit( 1 );
	}
	if( g_update_bit_S==NULL )
	{
	    fprintf( stdout, "Can't malloc g_update_bit_S\n" );
	    exit( 1 );
	}
	if( g_update_bit_E==NULL )
	{
	    fprintf( stdout, "Can't malloc g_update_bit_E\n" );
	    exit( 1 );
	}
	if( g_update_bit_E==NULL )
	{
	    fprintf( stdout, "Can't malloc g_update_bit_E\n" );
	    exit( 1 );
	}
	if( g_update_mode==NULL )
	{
	    fprintf( stdout, "Can't malloc g_update_mode\n" );
	    exit( 1 );
	}
	if( g_update_addr==NULL )
	{
	    fprintf( stdout, "Can't malloc g_update_addr\n" );
	    exit( 1 );
	}
	if( g_update_bits==NULL )
	{
	    fprintf( stdout, "Can't malloc g_update_bits\n" );
	    exit( 1 );
	}
	if( g_update_size==NULL )
	{
	    fprintf( stdout, "Can't malloc g_update_size\n" );
	    exit( 1 );
	}
	if( g_update_data==NULL )
	{
	    fprintf( stdout, "Can't malloc g_update_data\n" );
	    exit( 1 );
	}
	for( i=0; i<MAX_UPDATE; i++ )
	{
	    g_update_addr[i] = 0xFFFFFFFF;
	    g_update_bits[i] = 0;
	    g_update_size[i] = 0;
	    g_update_data[i] = 0;
	    g_update_mode[i] = 0;
	}
	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='=' )
	    {
	    	switch( argv[i][1] )
		{
		case 'S' :
		    mode = MODE_AVS;
		    break;

		case 'b' :
		    bShowBitAddr=1;		// Show BitAddr/Offset/Readed
		    break;
		case 't' :
		    // make timing_info_present_flag=0 and remove timing_info
		    bRemoveTimingInfo = 1;
		    fprintf( stdout, "RemoveTimingInfo\n" );
		    break;
		case 'p' :
		    // make pic_struct_present_flag=0 and remove pic_struct
		    bRemovePicStruct = 1;
		    fprintf( stdout, "RemovePicStruct\n" );
		    break;
		case 'f' :
		    // make frame_mbs_only_flag=0
		    nEditFrameMbs = atoi( &argv[i][2] );
		    fprintf( stdout, "EditFrameMbs=%d\n", nEditFrameMbs );
		    break;
		case 'P' :
		    bDebugPES=1;
		    break;
		case 'D' :
		    bDSS=1;
		    break;
		default :
		    fprintf( stdout, "Unknown parameter %s\n", argv[i] );
		    exit( 1 );
		}
	    } else if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case 'N' :
		    bShowDetail=0;
		    bShowNALinfo=0;
		    break;
		case 'n' :
//		    bShowDetail=1;
		    bShowNALinfo=1;
		    break;
		case 'D' :
		    bDump = 1;
		    break;
		case 'W' :
		    bShowSyntax = 1;
		    break;
		case 'E' :
		    bSkipError=1;
		    break;
		case 'F' :
//		    bForceAnalyze=1;
		    break;
// -----------------------------------------------------------------	
		case 'A' :
		    mode = MODE_AVC;
		    break;
		case 'M' :
		    mode = MODE_MPG;
		    bAlarmAVC=0;
		    break;
		case 'H' :
		    mode = MODE_HEADER;
		    break;
		case 'h' :
		    mode = MODE_HEADER;
		    break;
		case 'R' :
		    fprintf( stdout, "Remux\n" );
		    mode = MODE_REMUX;
		    Threshold = atoi( &argv[i][2] );
		    if( Threshold<1 )
		    	Threshold=90000;	// 1sec
		    break;
// -----------------------------------------------------------------	
		case 'V' :
		    bDebug = 1;
		    break;
		case 'S' :
		    bDumpSequenceDisplayExtension = 1;
		    bDumpSequenceExtension = 1;
		    bDumpSequence = 1;
		    break;
		case 'a' :
		    bUseAddrFile = 0;
		    fprintf( stdout, "bUseAddrFile=%d\n", bUseAddrFile );
		    break;
		case 'T' :
		    bDisplayTS = 1;
		    break;
		case 's' :
		    bDumpSlice = 1;
		    break;
		case 'C' :
		    nSelSt = atoi( &argv[i][2] );
		    break;
		case 'c' :
		    nSelEn = atoi( &argv[i][2] );
		    break;
		case 'I' :
		    {
			int j;
		    	int id=(-1);
			char *keyword = &argv[i][2];
			int len = strlen(keyword);
		    	for( j=0; headerItem[j].id>=0; j++ )
			{
#if 0
			    fprintf( stdout, "%d:%s,%d\n",
			    	j, headerItem[j].name, headerItem[j].id );
#endif
			    if( strncmp( keyword, headerItem[j].name, len )==0 )
			    {
				id = headerItem[j].id;
				break;
			    }
			}
			if( id<0 )
			{
			    fprintf( stdout, "id(%s) not found\n", keyword );
			    EXIT();
			} else {
			    fprintf( stdout, "%s = %d\n", keyword, id );
			}
			nItem = id;
			items[nItem]++;
			fprintf( stdout, "items[%d]=%d\n", 
			    nItem, items[nItem] );
		    }
		    break;
		case 'p' :
		    fromPts = GetValue( &argv[i][2], &Error );
		    break;
		case 'd' :
		    fromDts = GetValue( &argv[i][2], &Error );
		    break;
		case 'i' :
		    if( nItem>=0 )
		    {
			value[nItem] = GetValue( &argv[i][2], &Error );
			fprintf( stdout, "value[%d] = %d\n", 
				nItem, value[nItem] );
		    }
		    break;
		default :
		    fprintf( stdout, "Unknown parameter %s\n", argv[i] );
		    exit( 1 );
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'u' :
		    bShowUpdate=1;
		    break;
		case 'S' :
		    bDumpSPS=1;
		    break;
		case 's' :
		    bParseSlice=1;
		    break;
		case 'e' :
		    bDebugSEI=1;		// Debug SEI
		    break;
		case 'U' :
		    bUserData=1;
		    break;
		case 'F' :
		    bFramePackingArrangement=1;
		    break;
		case 'P' :
		    PtsOffset = atoi( &argv[i][2] );
		    fprintf( stdout, "PtsOffset=%d\n", PtsOffset );
		    bShowDetail = 0;
		    break;
		case 'p' :
		    toPts = GetValue( &argv[i][2], &Error );
		    break;
		case 'd' :
		    toDts = GetValue( &argv[i][2], &Error );
		    break;
		case 'D' :
		    DtsOffset = atoi( &argv[i][2] );
		    fprintf( stdout, "DtsOffset=%d\n", DtsOffset );
		    bShowDetail = 0;
		    break;
		case 'E' :
		    bEditDirect = 1;
		    fprintf( stdout, "bEditDirect=%d\n", bEditDirect );
		    break;
		case 'Z' :
		    bShowDetail = 1;
		    break;
		case 'O' :
		    bShowPOC = 1;
		    break;
		case 'M' :
		    bShowMvcExtention=1;
		    break;
		case 'm' :
		    bShowMvcScalable=1;
		    break;
		case 'T' :
		    bTimeAnalyze=1;
		    break;
		default :
		    fprintf( stdout, "Unknown parameter %s\n", argv[i] );
		    exit( 1 );
		}
	    } else {
	    	switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		case 1 :
		    break;
		}
	    }
	}

	if( filename==NULL )
	{
	    pesUsage( argv[0] );
	}
	char srcFilename[1024];
	char binFilename[1024];
	char txtFilename [1024];
	char ptsFilename[1024];
	char outFilename[1024];
	int period=(-1);
	for( i=0; i<strlen(filename); i++ )
	{
	    if( filename[i]=='.' )
	    	period = i;
	}
	if( period>0 )
	{
	    char head[1024];
	    memcpy( head, filename, strlen(filename)+1 );
	    head[period] = 0;
	    memcpy( binFilename, filename, strlen(filename)+1 );
	    snprintf( txtFilename, 1023, "%s.txt", head );
	    snprintf( ptsFilename, 1023, "%s.pts", head );
	} else {
	    snprintf( binFilename, 1023, "%s.bin", filename );
	    snprintf( txtFilename, 1023, "%s.txt", filename );
	    snprintf( ptsFilename, 1023, "%s.pts", filename );
	}

	bin_fp = fopen( binFilename, "rb" );
	if( bin_fp==NULL )
	{
	    fprintf( stdout, "binFilename : Can't open %s\n", binFilename );
	    exit( 1 );
	}
	if( mode==MODE_REMUX )
	{
	    pts_fp = fopen( ptsFilename, "r" );
	    if( pts_fp==NULL )
	    {
		fprintf( stdout, "ptsFilename : Can't read %s\n", ptsFilename );
		exit( 1 );
	    }
	} else {
	    pts_fp = fopen( ptsFilename, "w" );
	    if( pts_fp==NULL )
	    {
		fprintf( stdout, 
			"ptsFilename : Can't write %s\n", ptsFilename );
		exit( 1 );
	    }
	}

	// --------------------------------------------------------
	fprintf( stdout, "pes:%s\n", binFilename );
	fprintf( stdout, "pts:%s\n", ptsFilename );
	switch( mode )
	{
	case MODE_MPG :
	    fprintf( stdout, "MPEG mode\n" );
	    break;
	case MODE_AVC :
	    fprintf( stdout, "AVC mode\n" );
	    break;
	case MODE_AVS :
	    fprintf( stdout, "AVS mode\n" );
	    break;
	case MODE_HEADER :
	    fprintf( stdout, "Header mode\n" );
	    break;
	case MODE_REMUX :
	    fprintf( stdout, "Remux mode\n" );
	    break;
	default :
	    fprintf( stdout, "Unknown  mode (%d)\n", mode );
	    break;
	}
	// --------------------------------------------------------
	// Read Address table
	initTsAddr( );
	if( bUseAddrFile )
	{
	    int nTS=0;
	    if( ReadAddrFile( txtFilename, 
	    	srcFilename, TsAddr, PesAddr,
	    	&nTS, ADDR_MAX )<0 )
	    {
	    	fprintf( stdout, "Can't use AddrFile ignore\n" );
//		exit( 1 );
	    }
	}
	fprintf( stdout, "PesAddr[0]=%X\n", (UINT)PesAddr[0] );
	// --------------------------------------------------------
	//  Analyze
#if 0
	g_update_addr[g_update_count] = 0x1D8E654;
	g_update_bits[g_update_count] = 32;
	g_update_size[g_update_count] = 14;
	g_update_data[g_update_count] = 712;
	g_update_count++;

	g_update_addr[g_update_count] = 0x1D8E654;
	g_update_bits[g_update_count] = 47;
	g_update_size[g_update_count] = 14;
	g_update_data[g_update_count] = 478;
	g_update_count++;

	g_update_addr[g_update_count] = 0x1DDABBE;
	g_update_bits[g_update_count] = 0x00;
	g_update_size[g_update_count] = 12;
	g_update_data[g_update_count] = 712;
	g_update_count++;
#else
	switch( mode )
	{
	case MODE_HEADER :
	    AnalyzeHeader( bin_fp, pts_fp );
	    break;
	case MODE_MPG :
	    AnalyzeMPG( bin_fp, pts_fp );
	    break;
	case MODE_AVC :
	    AnalyzeAVC( bin_fp, pts_fp );
	    break;
	case MODE_AVS :
	    AnalyzeAVS( bin_fp, pts_fp );
	    break;
	case MODE_REMUX :
	    if( srcFilename[0]==0 )
	    {
	    	FILE *fp = fopen( "PES-1FFF.txt", "r" );
		if( fp )
		{
		    if( fgets( srcFilename, 1023, fp )==NULL )
		    {
			fprintf( stdout, "Can't get srcFilename\n" );
			return -1;
		    }
		    fclose( fp );
		    for( i=0; i<strlen(srcFilename); i++ )
		    {
			if( srcFilename[i]=='\n' )
			    srcFilename[i] = 0;
			if( srcFilename[i]=='\r' )
			    srcFilename[i] = 0;
		    }
		    fprintf( stdout, "srcFilename = %s\n", srcFilename );
		}
	    }
	    sprintf( outFilename, "%s-", srcFilename );
	    Remux( srcFilename, outFilename, txtFilename, pts_fp, Threshold );
	    break;
	default :
	    fprintf( stdout, "Unknown format (%d)\n", mode );
	    exit( 1 );
	    break;
	}
#endif
	if( bin_fp )
	{
	    fclose( bin_fp );
	    bin_fp = NULL;
	}
	if( pts_fp )
	{
	    fclose( pts_fp );
	    pts_fp = NULL;
	}

	if( mode==MODE_HEADER )
	{
	    if( fromDts!=INVALID_OFFSET )
	    if( validStart>=0 )
	    {
	    	int total=0;
	    	char writeFilename[1024];
		sprintf( writeFilename, "out.bin" );
	    	FILE *wr_fp = fopen( writeFilename, "wb" );
		if( wr_fp==NULL )
		{
		    fprintf( stdout, 
		    	"Can't write %s\n", writeFilename );
		    exit( 1 );
		}
		bin_fp = fopen( binFilename, "rb" );
		if( bin_fp==NULL )
		{
		    fprintf( stdout, 
		    	"binFilename : Can't open %s\n", binFilename );
		    exit( 1 );
		}
		fseek( bin_fp, validStart, SEEK_SET );
		int Unit=1024*16;
		char copyBuf[Unit];
		int rest=0x7FFFFFFF;	// max
		if( validEnd>0 )
		    rest = validEnd-validStart;
		while( rest>0 )
		{
		    int readed=0;
		    int size=0;
		    if( rest>Unit )
			size = Unit;
		    else
			size = rest;
		    readed = fread( copyBuf, 1, size, bin_fp );
		    if( readed<1 )
			break;
		    fwrite( copyBuf, 1, readed, wr_fp );
		    rest -= readed;
		    total += readed;
		}
		fclose( wr_fp );
		fclose( bin_fp );
		fprintf( stdout, "%d bytes written\n", total );
	    }
	}

	// Make updated file
	int j, n;
	if( g_update_count>0 )
	{
	    fprintf( stdout, 
	    	"----------------------------------------------\n" );
	    fprintf( stdout, "g_update_count =%d\n", g_update_count );
	    fprintf( stdout, "g_update_countS=%d\n", g_update_countS );
	    fprintf( stdout, "Now swap\n" );
	    for( j=0; j<(g_update_count-0); j++ )
	    {
		int bSwapped=0;
		for( i=0; i<j; i++ )
		{
		    int bit1 = g_update_addr[i+0]*8+g_update_bits[i+0];
		    int bit2 = g_update_addr[i+1]*8+g_update_bits[i+1];
		    if( bit1>bit2 )
		    {
			swap( &g_update_addr[i+0], &g_update_addr[i+1] );
			swap( &g_update_bits[i+0], &g_update_bits[i+1] );
			swap( &g_update_size[i+0], &g_update_size[i+1] );
			swap( &g_update_data[i+0], &g_update_data[i+1] );
			bSwapped++;
		    }
		}
		if( bSwapped==0 )
		    break;
	    }
	    for( n=0; n<g_update_count; n++ )
	    {
	    	if( bShowUpdate )
		{
		    fprintf( stdout, "update 0x%X,%d,%d,%d\n",
			g_update_addr[n],
			g_update_bits[n],
			g_update_size[n],
			g_update_data[n] );
		}
	    }
	    fprintf( stdout, "%d item updated\n", g_update_count );
	}

	if( (g_update_count>0) || (g_update_countS>0) )
	{
	    if( bEditDirect==0 )
	    {
		sprintf( outFilename, "%s-", srcFilename );
		FILE *src_fp = fopen( srcFilename, "rb" );
		if( src_fp==NULL )
		{
		    fprintf( stdout, 
		    "srcFilename : Can't open %s\n", srcFilename );
		    exit( 1 );
		}
		FILE *outfile = fopen( outFilename, "wb" );
		if( outfile==NULL )
		{
		    fprintf( stdout, "outfile : Can't open %s\n", outFilename );
		    exit( 1 );
		}
		fprintf( stdout, "Copy %s to %s\n", srcFilename, outFilename );
		CopyFile( src_fp, outfile, -1 );

		fclose( src_fp );
		fclose( outfile );
	    } else {
		sprintf( outFilename, "%s", srcFilename );
	    }
	    // ------------------------------------------------
	    int fd;
	    int n;
	    int mapSize = 4096*2;
	    fprintf( stdout, "Update %s\n", outFilename );
	    fd = open( outFilename, O_RDWR );
	    if( fd==(-1) )
	    {
	    	fprintf( stdout, "outFilename : Can't open %s\n", outFilename );
		exit( 1 );
	    }
	    unsigned char *data;
	    // Update Bits
	    for( n=0; n<g_update_count; n++ )
	    {
	    	if( bShowUpdate )
		{
		    fprintf( stdout, "addr=0x%08X, bits=%d\n",
			g_update_addr[n], g_update_bits[n] );
		}

		addr = SrcAddr(g_update_addr[n] + g_update_bits[n]/8,0);
		g_update_bits[n]=g_update_bits[n]%8;

	    	if( bShowUpdate )
		{
		    fprintf( stdout, "addr=0x%08X, bits=%d\n",
			addr, g_update_bits[n] );
		}
	    // mmap
		data = (unsigned char *)mmap( (caddr_t)0, mapSize, 
				    PROT_READ | PROT_WRITE, MAP_SHARED,
				    fd, addr & 0xFFFFF000 );
		if( data==(unsigned char *)-1) 
		{
		    fprintf( stdout, "Can't mmap\n" );
		    exit( 1 );
		}
	    // update data
		{
		    unsigned char *ubuffer =
			    (unsigned char *)(data+(addr & 0x0FFF));
		    UpdateBits( ubuffer, 
			g_update_bits[n], g_update_size[n], g_update_data[n] );
		}
	    // munmap
		if( munmap( data, mapSize )==(-1) )
		{
		    fprintf( stdout, "Can't munmap\n" );
		    exit( 1 );
		}
	    }
	    // Special Update
	    for( n=0; n<g_update_countS; n++ )
	    {
		int bit;	
		fprintf( stdout, "=== %d\n", n );
		fprintf( stdout, 
		    "Special : addr=%8X, bits=%d, %d, %d (mode=%d)\n",
		    g_update_addrS[n], 
		    g_update_bit_S[n],
		    g_update_bit_E[n],
		    g_update_bit_T[n],
		    g_update_mode [n]
		    );
	    // mmap
		addr = SrcAddr(g_update_addrS[n],0);
		data = (unsigned char *)mmap( (caddr_t)0, mapSize, 
				    PROT_READ | PROT_WRITE, MAP_SHARED,
				    fd, addr & 0xFFFFF000 );
		if( data==(unsigned char *)-1) 
		{
		    fprintf( stdout, "Can't mmap\n" );
		    exit( 1 );
		}
#if 1
		fprintf( stdout, "addr=%X\n", addr );
		int j;
		for( j=0; j<16; j++ )
		{
		    fprintf( stdout, "%4d : ", (j*16+0)*8 );
		    for( i=0; i<16; i++ )
		    {
			int b = ReadByte( data, addr, (j*16+i)*8 );
			fprintf( stdout, "%02X ", b );
		    }
		    fprintf( stdout, "\n" );
		}
#endif
		if( g_update_mode[n]==EDIT_CUT )
		{
		    // S,E,T : S<-E
		    fprintf( stdout, "EDIT_CUT\n" );
		    for( bit=g_update_bit_E[n]; bit<g_update_bit_T[n]; bit++ )
		    {
			fprintf( stdout, "addr=%8X, bits=%d\n", addr, bit );
			int b, dst_bit;
			dst_bit = g_update_bit_S[n]+bit-g_update_bit_E[n];
			b = ReadBit( data, addr, bit );
			WriteBit( data, addr, dst_bit, b );
			fprintf( stdout, "%4X,%3d : %d : %4X,%3d : %02X\n",
			    addr, bit, b, addr, dst_bit,
			    ReadByte( data, addr, dst_bit ) );
		    }
		    for( bit=g_update_bit_S[n]
		    	    +g_update_bit_T[n]-g_update_bit_E[n];
			    bit<=g_update_bit_T[n]; bit++ )
		    {
			WriteBit( data, addr, bit, 0 );
		    }
		} else if( g_update_mode[n]==EDIT_INS )
		{
		    // S,E,T : S->E
		    fprintf( stdout, "EDIT_INS\n" );
//		    for( bit=g_update_bit_E[n]; bit<g_update_bit_T[n]; bit++ )
		    for( bit=g_update_bit_T[n]-1; 
		    	bit>=g_update_bit_E[n]; bit-- )
		    {
			fprintf( stdout, "addr=%8X, bits=%d\n", addr, bit );
			int b, dst_bit;
//			dst_bit = g_update_bit_S[n]+bit-g_update_bit_E[n];
// 2012/5/8 Edit
			dst_bit = bit+g_update_bit_E[n]-g_update_bit_S[n];
			b = ReadBit( data, addr, bit );
			WriteBit( data, addr, dst_bit, b );
			fprintf( stdout, "%4X,%3d : %d : %4X,%3d : %02X\n",
			    addr, bit, b, addr, dst_bit,
			    ReadByte( data, addr, dst_bit ) );
		    }
		    int b=0;
		    for( bit=g_update_bit_S[n]; 
		    	 bit<g_update_bit_E[n]; bit++ )
		    {
			WriteBit( data, addr, bit, b );
		    }
		}
	    // munmap
		if( munmap( data, mapSize )==(-1) )
		{
		    fprintf( stdout, "Can't munmap\n" );
		    exit( 1 );
		}
	    }
	    close( fd );
	}
	fprintf( stdout, "Packets[MAX_PACKETS+0] = %8X\n", 
		Packets[MAX_PACKETS+0] );
	fprintf( stdout, "Packets[MAX_PACKETS+1] = %8X\n", 
		Packets[MAX_PACKETS+1] );
	for( i=0; i<MAX_PACKETS; i++ )
	{
	    if( Packets[i]>0 )
	    {
		fprintf( stdout, "Packet[%04X] = %d\n", i, Packets[i] );
	    }
	}
	for( i=0; i<256; i++ )
	{
	    if( Counts[i]>0 )
	    {
		fprintf( stdout, "Counts[%04X] = %d\n", i, Counts[i] );
	    }
	}
	return 0;
}

