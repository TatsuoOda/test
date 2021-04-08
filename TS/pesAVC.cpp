
/*
	pesAVC.cpp
		2013.7.17 separate from pesParse.cpp
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

extern int *Packets;

extern int bShowDetail;		// AVC show NAL parse
static int bShowPOC=0;
static int bFramePackingArrangement=0;

extern int bDebugSkip;		// Dump with skipContent()
extern int bShowBitAddr;		// Show BitAddr/Offset/Readed
extern int bDebugSEI;		// Debug SEI
extern int bShowNALinfo;		// CountNAL message
extern int bUserData;
extern int bShowMvcExtention;
extern int bShowMvcScalable;
static int bDumpSPS=0;
static int bParsePOC=1;
static int bParseSlice=0;
static int bParseSliceHeader=1;
//static int bAlarmAVC=1;
extern int bTimeAnalyze;

#if 0
extern int g_DTSL;
extern int g_DTSH;
extern int g_PTSL;
extern int g_PTSH;
#endif

static int es_addr =  0;

extern int SpecialStartAddr;
extern int SpecialStartBits;
extern int SpecialEndAddr;
extern int SpecialEndBits;

extern int nSequence;
extern int nSequenceExtension;
extern int nSequenceDisplayExtension;
int nSelSt=(-1);
int nSelEn=(-1);
extern int bDumpSequenceDisplayExtension;
extern int bDumpSequenceExtension;
extern int bDumpSequence;

#define EDIT_NONE	0
#define EDIT_CUT	1
#define EDIT_INS	2
static int bEditSPS=EDIT_NONE;

int bRemovePicStruct= EDIT_NONE;
int nEditFrameMbs = (-1);
// make timing_info_present_flag=0 and remove timing_info
int bRemoveTimingInfo=0;	


#define MAX_SPS 32
int avc_nal_hrd_parameters_present_flag[MAX_SPS];
int avc_vcl_hrd_parameters_present_flag[MAX_SPS];
int avc_pic_struct_present_flag[MAX_SPS];
int avc_CpbDpbDelaysPresentFlag[MAX_SPS];

int PPStoSPS[MAX_SPS];
int avc_PPS_id = (-1);

int avc_cpb_cnt = (-1);
int avc_CpgDpbDelaysPresentFlag = (-1);
int avc_time_offset_length = (-1);

int avc_initial_cpb_removal_delay_length = (-1);
int avc_cpb_removal_delay_length = (-1);
int avc_dpb_output_delay_length = (-1);
int avc_separate_colour_plane_flag=(-1);
int avc_frame_mbs_only_flag = (-1);
int avc_IdrPicFlag = 0;
int avc_pic_order_cnt_type = (-1);
int avc_log2_max_pic_order_cnt_lsb_minus4 = (-1);
int avc_log2_max_frame_num_minus4 = (-1);
int avc_delta_pic_order_always_zero_flag  = (-1);
// PPS
int avc_bottom_field_pic_order_in_frame_present_flag = (-1);
int avc_redundant_pic_cnt_present_flag = (-1);

int n_vui_parameters = 0;
int n_sequenceParameters = 0;
int n_pan_scan = 0;


#if 0
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
int bShow=0;
int bSkipError=0;
//static int bShowSEI=0;
int bDSS=0;

int bDebugRemux=0;
static int DSS_PCT=(-1);
static char DSS_Addr[80];

int validStart = (-1);
int validEnd   = (-1);

#if 0
static int g_DTSL = (-1);
static int g_DTSH = (-1);
static int g_PTSL = (-1);
static int g_PTSH = (-1);
#endif

#define MAX_PACKETS 1024*64
//int Packets[MAX_PACKETS+4];	// 16bit

unsigned int *g_update_addrS = NULL;
unsigned int *g_update_bit_S = NULL;	// remove start
unsigned int *g_update_bit_E = NULL;	// remove end
unsigned int *g_update_bit_T = NULL;	// structure end : д─дсды
unsigned int *g_update_mode  = NULL;

int g_update_countS=0;

// ---------------------------------------------
// AVC
// for each SPS
//int avc_nal_hrd_parameters_present_flag=(-1);
//int avc_vcl_hrd_parameters_present_flag=(-1);
//int avc_pic_struct_present_flag = (-1);
//int avc_CpbDpbDelaysPresentFlag = (-1);
// ---------------------------------------------
// header item
//
int items[ID_MAX];
int value[ID_MAX];

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



void TS_addr_Dump( long *TS_addr, int nTS )
{
int n;
    fprintf( stdout, "TS_addr\n" );
	for( n=0; n<nTS; n++ )
	{
	    fprintf( stdout, "TS(%5d) : %8X\n", n, (UINT)TS_addr[n] );
	}
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
#endif

// BitAddr
static int b_user_data=0;
static unsigned int picture_coding_type=0;

void InitBitStream( );
int GetBitStream( FILE *fp, int nBit );
int GetBitStreamGolomb( FILE *fp );
int UpdateProcedure( int nCycle, int id, int size );
void ShowBitAddr( );
int UpdateProcedureGolomb( int nCycle, int id, int size, int nValue );
void SetSpecial( int mode );
int GetBitAddr();
int GetBitOffset();
int GetByteSkip();

#define MAX_BASE	30

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

// -------------------------------------------------------------------------

void init_AVC( )
{
int i;
	for( i=0; i<MAX_SPS; i++ )
	{
	    avc_nal_hrd_parameters_present_flag[i]=(-1);
	    avc_vcl_hrd_parameters_present_flag[i]=(-1);
	    avc_pic_struct_present_flag[i]=(-1);
	    avc_CpbDpbDelaysPresentFlag[i]=(-1);
	    PPStoSPS[i] = (-1);
	}
}

int skipContent( FILE *fp, unsigned char *buffer, int bDump )
{
int i;
int zCount=0;
int readed;
	if( bShowDetail )
	{
	fprintf( stdout, "***********************************************\n" );
	fprintf( stdout, " skipContent(%llX)\n", g_addr );
	fprintf( stdout, "***********************************************\n" );
	}
	while( 1 )
	{
	    readed = gread( &buffer[zCount], 1, 1, fp );
	    if( readed<1 )
	    {
	    	CannotRead( NULL );
	    	return -1;
	    }
	    if( bDump )
	    {
	    	for( i=0; i<readed; i++ )
		{
		    fprintf( stdout, "%02X ", buffer[zCount+i] );
		}
	    }
	    switch( zCount )
	    {
	    case 0 :
	    	if( buffer[zCount]==0 )
		    zCount++;
		break;
	    case 1 :	// 00 XX
	    	if( buffer[zCount]==0 )
		    zCount++;
		else
		    zCount=0;
		break;
	    case 2 :	// 00 00 XX
	    	if( buffer[zCount]==0 )	// 00 00 00
		{
			// Do nothing
		} else if( buffer[zCount]==1 ) {	// 00 00 01
//		    fprintf( stdout, "Found\n" );
		    readed = gread( &buffer[3], 1, 1, fp );
		    if( readed<1 )
		    {
			CannotRead( NULL );
			return -1;
		    }
		    if( bDump )
			fprintf( stdout, "%02X\n", buffer[3] );
		    if( bShowDetail )
		    fprintf( stdout, "skipped(%s)\n", AddrStr(g_addr) );
		    return 1;
		} else {	// 00 00 XX
		    zCount = 0;
		}
		break;
	    }
	}
	if( bDebug )
	    fprintf( stdout, "%llX\n", g_addr );
	return 0;
}

int hrd_parameters( FILE *fp, unsigned char *buffer, int SPS_id )
{
int n;
int bit_rate_scale = (-1);
int cpb_size_scale = (-1);
int bit_rate_value[256];
int cpb_size_value[256];
int cbr_flag[256];
	fprintf( stdout, "hrd_parameters(%d)\n", SPS_id );
	for( n=0; n<256; n++ )
	{
	    bit_rate_value[n] =  (-1);
	    cpb_size_value[n] =  (-1);
	    cbr_flag[n]       =  (-1);
	}
	// -----------------------------------------
	avc_cpb_cnt = GetBitStreamGolomb( fp );
	bit_rate_scale = GetBitStream( fp, 4 );
	cpb_size_scale = GetBitStream( fp, 4 );
	if( bShowDetail )
	fprintf( stdout, "cpb_cnt=%d\n", avc_cpb_cnt );
	if( bShowDetail )
	fprintf( stdout, "bit_rate_scale=%d\n", bit_rate_scale );
	if( bShowDetail )
	fprintf( stdout, "cpb_size_scale=%d\n", cpb_size_scale );
	for( n=0; n<=avc_cpb_cnt; n++ )
	{
	    bit_rate_value[n] = GetBitStreamGolomb( fp );
	    if( bShowDetail )
	    fprintf( stdout, "bit_rate_value[%d]=%d\n", n, bit_rate_value[n] );
	    cpb_size_value[n] = GetBitStreamGolomb( fp );
	    if( bShowDetail )
	    fprintf( stdout, "cpb_size_value[%d]=%d\n", n, cpb_size_value[n] );
	    cbr_flag[n] = GetBitStream( fp, 1 );
	    if( bShowDetail )
	    fprintf( stdout, "cbr_flag      [%d]=%d\n", n, cbr_flag[n] );
	}
	avc_initial_cpb_removal_delay_length = GetBitStream( fp, 5 )+1;
	avc_cpb_removal_delay_length = GetBitStream( fp, 5 )+1;
	avc_dpb_output_delay_length = GetBitStream( fp, 5 )+1;
	if( bShowDetail )
	fprintf( stdout, "initial_cpb_removal_delay_length=%d\n",
		avc_initial_cpb_removal_delay_length );
	if( bShowDetail )
	fprintf( stdout, "cpb_removal_delay_length=%d\n",
		avc_cpb_removal_delay_length );
	if( bShowDetail )
	fprintf( stdout, "dpb_output_delay_length=%d\n",
		avc_dpb_output_delay_length );
#if 1	// 2011.9.23
	avc_CpbDpbDelaysPresentFlag[SPS_id] = 1;
	if( bShowDetail )
	fprintf( stdout, "avc_CpbDpbDelaysPresentFlag is set to 1\n" );;
#endif
	avc_time_offset_length = GetBitStream( fp, 5 );
	if( bShowDetail )
	fprintf( stdout, "time_offset_length=%d\n", avc_time_offset_length );
	return 0;
}

#define EXTENDED_SAR	255

int vui_parameters( FILE *fp, unsigned char *buffer, int SPS_id,
    int *pNum_units_in_tick, int *pTime_scale, int *pFixed_frame_rate_flag )
{
int aspect_ratio_info_present_flag = (-1);
int aspect_ratio = (-1);
int sar_width  = (-1);
int sar_height = (-1);
int overscan_info_present_flag = (-1);
int overscan_info = (-1);
int video_signal_type_present_flag = (-1);
int video_format = (-1);
int video_full_range_flag = (-1);
int colour_description_present_flag = (-1);
int transfer_characteristics = (-1);
int colour_primaries = (-1);
int matrix_coefficients = (-1);
int chroma_loc_info_present_flag = (-1);
int chroma_sample_loc_type_top_field = (-1);
int chroma_sample_loc_type_bottom_field = (-1);
int timing_info_present_flag = (-1);
int num_units_in_tick = (-1);
int time_scale = (-1);
int fixed_frame_rate_flag = (-1);
int low_delay_hrd_flag = (-1);
int bitstream_restriction_flag = (-1);
int motion_ectors_over_pic_boundaries_flag = (-1);
int max_bytes_per_pic_denom = (-1);
int max_bits_per_pic_denom = (-1);
int max_mv_length_horizontal = (-1);
int max_mv_length_vertical = (-1);
int num_reorder_frames = (-1);
int max_dec_frame_buffering = (-1);
	// -----------------------------------------------------
	avc_nal_hrd_parameters_present_flag[SPS_id] = (-1);
	avc_vcl_hrd_parameters_present_flag[SPS_id] = (-1);
	avc_pic_struct_present_flag        [SPS_id] = (-1);
	avc_CpbDpbDelaysPresentFlag        [SPS_id] = (-1);
	// -----------------------------------------------------
	fprintf( stdout, "vui_parameters(%d)\n", SPS_id );
	if( bRemoveTimingInfo )
	    fprintf( stdout, "bRemoveTimingInfo=%d\n", bRemoveTimingInfo );
	aspect_ratio_info_present_flag = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "aspect_ratio_info=%d\n", 
		aspect_ratio_info_present_flag );
	if( aspect_ratio_info_present_flag )
	{
	    aspect_ratio = GetBitStream( fp, 8 );
	    if( bShowDetail )
	    fprintf( stdout, "aspect_ratio=%d\n", aspect_ratio );
	    if( aspect_ratio==EXTENDED_SAR )
	    {
		//
		UpdateProcedure( n_vui_parameters, ID_SARW, 16 );
		//
	    	sar_width  = GetBitStream( fp, 16 );
		//
		UpdateProcedure( n_vui_parameters, ID_SARH, 16 );
		//
	    	sar_height = GetBitStream( fp, 16 );
		if( bShowDetail )
		fprintf( stdout, "sar_width=%d\n", sar_width );
		if( bShowDetail )
		fprintf( stdout, "sar_height=%d\n", sar_height );
	    }
	}
	int bEditOVS=0;
	int bUpdateOVS=UpdateProcedure( n_vui_parameters, ID_OVSP, 1 );
	overscan_info_present_flag = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "overscan_info_present=%d\n", 
		overscan_info_present_flag );
	if( bUpdateOVS )
	{
	    if( bShowDetail )
	    fprintf( stdout, "ovs(%d->%d)\n", 
		    overscan_info_present_flag, value[ID_OVSP] );
	    if( overscan_info_present_flag==1 )
	    {
		if( value[ID_OVSP]==0 )
		{
		    bEditOVS=1;
		    bEditSPS=EDIT_CUT;
		    SetSpecial( SET_START );
		    fprintf( stdout, "bEditSPS=%d\n", bEditSPS );
		}
	    } else {
		if( value[ID_OVSP]==1 )	// add OVS
		{
		    bEditOVS=1;
		    bEditSPS=EDIT_INS;
		    SetSpecial( SET_START );
		    fprintf( stdout, "bEditSPS=%d\n", bEditSPS );
		}
	    }
	}

	if( overscan_info_present_flag )
	{
#if 0
	    if( bShowDetail )
	    fprintf( stdout, "ovs:BitAddr=(%s), BitOffset=%d\n",
	    	AddrStr(BitAddr), BitOffset );
#endif
	    UpdateProcedure( n_vui_parameters, ID_OVSA, 1 );
	    overscan_info = GetBitStream( fp, 1 );
	    if( bShowDetail )
	    fprintf( stdout, "overscan_info_appropriate_flag=%d\n", 
	    	overscan_info );
	}
	if( bEditOVS )
	{
		SetSpecial( SET_END );
	}

	video_signal_type_present_flag = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "video_signal_type_present=%d\n", 
	    video_signal_type_present_flag );
	if( video_signal_type_present_flag )
	{
	    video_format = GetBitStream( fp, 3 );
	    if( bShowDetail )
	    fprintf( stdout, "video_format=%d\n", video_format );
	    video_full_range_flag = GetBitStream( fp, 1 );
	    if( bShowDetail )
	    fprintf( stdout, "video_full_range=%d\n", video_full_range_flag );
	    colour_description_present_flag = GetBitStream( fp, 1 );
	    if( bShowDetail )
	    fprintf( stdout, "colour_description_present=%d\n", 
	    	colour_description_present_flag );
	    if( colour_description_present_flag )
	    {
		//
		UpdateProcedure( n_vui_parameters, ID_CP,  8 );
		//
		colour_primaries = GetBitStream( fp, 8 );
	    if( bShowDetail )
		fprintf( stdout, "colour_primaries=%d\n", colour_primaries );
		//
		UpdateProcedure( n_vui_parameters, ID_TC,  8 );
		//
		transfer_characteristics = GetBitStream( fp, 8 );
	    if( bShowDetail )
		fprintf( stdout, "transfer_characteristics=%d\n", 
			transfer_characteristics );
		//
		UpdateProcedure( n_vui_parameters, ID_MXC,  8 );
		//
		matrix_coefficients = GetBitStream( fp, 8 );
	    if( bShowDetail )
		fprintf( stdout, "matrix_coefficients=%d\n", 
			matrix_coefficients );
	    }
	}
	chroma_loc_info_present_flag = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "chroma_loc_info_present=%d\n", 
	    chroma_loc_info_present_flag );
	if( chroma_loc_info_present_flag )
	{
	    chroma_sample_loc_type_top_field = GetBitStreamGolomb( fp );
	    chroma_sample_loc_type_bottom_field = GetBitStreamGolomb( fp );
	}

	if( bRemoveTimingInfo )
	{	// make timing_info_present_flag = 0
	    DoUpdate( GetBitAddr(), GetBitOffset(), 1, 0 );
	}
	timing_info_present_flag = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "timing_info_present=%d\n", timing_info_present_flag );
	if( timing_info_present_flag==1 )
	{
	    if( bRemoveTimingInfo )
	    {
		bEditSPS = EDIT_CUT;
		SetSpecial( SET_START );
		ShowBitAddr( );
	    }
//
	    num_units_in_tick = GetBitStream( fp, 32 );
	    if( bShowDetail )
	    fprintf( stdout, "num_units_in_tick=%d\n", num_units_in_tick );
	    time_scale = GetBitStream( fp, 32 );
	    if( bShowDetail )
	    fprintf( stdout, "time_scale=%d\n", time_scale );
	    //
//	    fprintf( stdout, "UpdatePRocedure(ID_FRC)\n" );
	    UpdateProcedure( n_vui_parameters, ID_FRC,  1 );
	    //
	    fixed_frame_rate_flag = GetBitStream( fp, 1 );
	    if( bShowDetail )
	    fprintf( stdout, "fixed_frame_rate_flag=%d\n", 
		    fixed_frame_rate_flag );
//
	    if( bRemoveTimingInfo )
	    if( bEditSPS )
	    {
		ShowBitAddr( );
	    }
//
	}
	if( bRemoveTimingInfo )
	{
	    bEditSPS = EDIT_CUT;
	    SetSpecial( SET_END );
	}
	avc_nal_hrd_parameters_present_flag[SPS_id] = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "nal_hrd_parameters_present(%d)=%d\n", 
		SPS_id,
		avc_nal_hrd_parameters_present_flag[SPS_id] );
	if( avc_nal_hrd_parameters_present_flag[SPS_id]==1 )
	{
	    hrd_parameters( fp, buffer, SPS_id );
	}
	avc_vcl_hrd_parameters_present_flag[SPS_id] = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "vcl_hrd_parameters_present(%d)=%d\n", 
		SPS_id,
		avc_vcl_hrd_parameters_present_flag[SPS_id] );
	if( avc_vcl_hrd_parameters_present_flag[SPS_id]==1 )
	{
	    hrd_parameters( fp, buffer, SPS_id );
	}
	if( avc_nal_hrd_parameters_present_flag[SPS_id] 
	 || avc_vcl_hrd_parameters_present_flag[SPS_id] )
	    low_delay_hrd_flag = GetBitStream( fp, 1 );

	if( bRemovePicStruct )
	{	// make pic_struct_present_flag = 0
	    ShowBitAddr( );
	    DoUpdate( GetBitAddr(), GetBitOffset()+GetByteSkip()*8, 1, 0 );
	}
	avc_pic_struct_present_flag[SPS_id] = GetBitStream( fp, 1 );

	if( bShowDetail )
	fprintf( stdout, "pic_struct_present_flag(%d)=%d\n", 
		SPS_id,
		avc_pic_struct_present_flag[SPS_id] );

	bitstream_restriction_flag = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "bitstream_restriction_flag=%d\n", 
		bitstream_restriction_flag );
	if( bitstream_restriction_flag==1 )
	{
	    motion_ectors_over_pic_boundaries_flag = GetBitStream( fp, 1 );
	    max_bytes_per_pic_denom = GetBitStreamGolomb( fp );
	    max_bits_per_pic_denom = GetBitStreamGolomb( fp );
	    max_mv_length_horizontal = GetBitStreamGolomb( fp );
	    max_mv_length_vertical = GetBitStreamGolomb( fp );
	    num_reorder_frames = GetBitStreamGolomb( fp );
	    max_dec_frame_buffering = GetBitStreamGolomb( fp );
	}
	n_vui_parameters ++;
	*pNum_units_in_tick = num_units_in_tick;
	*pTime_scale        = time_scale;
	*pFixed_frame_rate_flag = fixed_frame_rate_flag;
	return 0;
}

void DumpBitBuffer( );
int SequenceParameterSet( FILE *fp, unsigned char *buffer )
{
int n;
int profile_idc=(-1);
int constraint=(-1);
int level_idc=(-1);
int seq_parameter_set_id=(-1);
int chroma_format_idc=(-1);
int bit_depth_luma=(-1);
int bit_depth_chroma=(-1);
int bypass_flag = (-1);
int matrix_present_flag = (-1);
int offset_for_non_ref  = (-1);
int offset_for_top_for_bottom  = (-1);
int num_ref_frames  = (-1);
int offset_for_ref_frame[256];
int max_num_ref_frames  = (-1);
int gaps_allowed_flag = (-1);
int pic_width   = (-1);
int pic_height  = (-1);
int adaptive_frame_field_flag = (-1);
int direct_8x8_inference_flag = (-1);
int frame_cropping_flag = (-1);
int frame_crop_left_offset   = (-1);
int frame_crop_right_offset  = (-1);
int frame_crop_top_offset    = (-1);
int frame_crop_bottom_offset = (-1);
int vui_parameters_present_flag = (-1);
int ret=0;

//	bDebugGol = 1;
	bEditSPS = EDIT_NONE;
	for( n=0; n<256; n++  )
	{
	    offset_for_ref_frame[n] = (-1);
	}
	if( bShowDetail )
	fprintf( stdout, "SequenceParameterSet()\n" );
	// -------------------------------------------
	InitBitStream( );
	bDebugGolB = bDebugGol;
	//
	UpdateProcedure( n_sequenceParameters, ID_PALI,  8 );
	//
	profile_idc = GetBitStream( fp, 8 );
	if( bShowDetail )
	fprintf( stdout, "profile=%d\n", profile_idc );
	constraint  = GetBitStream( fp, 8 );
	if( bShowDetail )
	fprintf( stdout, "constraint=%d\n", constraint );
	//
	UpdateProcedure( n_sequenceParameters, ID_LVL,  8 );
	//
	level_idc   = GetBitStream( fp, 8 );
	if( bShowDetail )
	fprintf( stdout, "level=%d\n", level_idc );
	seq_parameter_set_id = GetBitStreamGolomb( fp );
	if( bShowDetail )
	fprintf( stdout, "seq_parameter_set_id=%d\n", seq_parameter_set_id );
	switch( profile_idc )
	{
	case 100 :
	case 110 :
	case 122 :
	case 244 :
	case  44 :
	case  83 :
	case  86 :
	case 118 :
	case 128 :
	    chroma_format_idc = GetBitStreamGolomb( fp );
	    if( bShowDetail )
	    fprintf( stdout, "chroma_format=%d\n", chroma_format_idc );
	    if( chroma_format_idc==3 )
	    {
	    	avc_separate_colour_plane_flag = GetBitStream( fp, 1 );
	    }
	    bit_depth_luma = GetBitStreamGolomb( fp );
	    bit_depth_chroma = GetBitStreamGolomb( fp );
	    if( bShowDetail )
	    fprintf( stdout, "bit_depth_luma  =%d\n", bit_depth_luma );
	    if( bShowDetail )
	    fprintf( stdout, "bit_depth_chroma=%d\n", bit_depth_chroma );
	    bypass_flag = GetBitStream( fp, 1 );
	    if( bShowDetail )
	    fprintf( stdout, "bypass_flag=%d\n", bypass_flag );
	    matrix_present_flag = GetBitStream( fp, 1 );
	    if( bShowDetail )
	    fprintf( stdout, "scaling_matrix_flag=%d\n", matrix_present_flag );
	    if( matrix_present_flag )
	    {
	    	int i;
	    	int n = chroma_format_idc!=3 ? 8 : 12;
		if( bShowDetail )
	    	fprintf( stdout, "matrix_present(%d)\n", n );
	    	for( i=0; i<n; i++ )
		{
		    int seq_scaling_list_present_flag;
		    seq_scaling_list_present_flag  = GetBitStream( fp, 1 );
		    if( seq_scaling_list_present_flag )
		    {
		    	int bits = 0;
		    	if( i<6 )
			{
			    bits=16;
			} else {
			    bits=64;
			}
			int b;
			for( b=0; b<bits; b++ )
			{
		    	int delta_scale = GetBitStreamGolomb( fp );
			if( bShowDetail )
			fprintf( stdout, "delta_scale=%d\n", delta_scale );
			}
		    }
		}
//		if( bShowDetail )
//		fprintf( stdout, "matrix_present_flag : under construction\n" );
//		EXIT();
	    }
	    break;
	}
	avc_log2_max_frame_num_minus4 = GetBitStreamGolomb( fp );
	if( bShowDetail )
	{
	    fprintf( stdout, "log2_max_frame_num_minus4=%d\n", 
		avc_log2_max_frame_num_minus4 );
	}
	avc_pic_order_cnt_type = GetBitStreamGolomb( fp );
	if( bShowDetail )
	fprintf( stdout, "pic_order_cnt_type=%d\n", avc_pic_order_cnt_type );
	if( avc_pic_order_cnt_type==0 )
	{
	    avc_log2_max_pic_order_cnt_lsb_minus4 = GetBitStreamGolomb( fp );
	    if( bShowDetail )
	    {
		fprintf( stdout, "log2_max_pic_order_cnt_lsb_minus4=%d\n", 
		    avc_log2_max_pic_order_cnt_lsb_minus4 );
		fprintf( stdout, "MaxPicOrderCntLsb=%d\n",
			1<<(avc_log2_max_pic_order_cnt_lsb_minus4+4+1) );
	    }
	} else if( avc_pic_order_cnt_type==1 )
	{
	    avc_delta_pic_order_always_zero_flag  = GetBitStream( fp, 1 );
	    offset_for_non_ref  = GetBitStreamGolomb( fp );
	    offset_for_top_for_bottom  = GetBitStreamGolomb( fp );
	    num_ref_frames  = GetBitStreamGolomb( fp );
	    if( bShowDetail )
	    {
		fprintf( stdout, "delta_pic_order_always_zero_flag=%d\n", 
		    avc_delta_pic_order_always_zero_flag );
		fprintf( stdout, 
		    "offset_for_non_ref=%d\n", offset_for_non_ref );
		fprintf( stdout, "offset_for_top_for_bottom=%d\n", 
		    offset_for_top_for_bottom );
		fprintf( stdout, "num_ref_frames=%d\n", num_ref_frames );
	    }
	    for( n=0; n<num_ref_frames; n++ )
	    {
		offset_for_ref_frame[n]  = GetBitStreamGolomb( fp );
	    	if( bShowPOC )
		fprintf( stdout, "offset_for_ref_frame[%d]=%d\n",
			n, offset_for_ref_frame[n] );
	    }
	}
	max_num_ref_frames  = GetBitStreamGolomb( fp );
//	if( bShowDetail )
	fprintf( stdout, "max_num_ref_frames=%d\n", max_num_ref_frames );
	gaps_allowed_flag = GetBitStream( fp, 1 );
	int pic_width_in_mbs_minus1        ;
	int pic_height_in_map_units_minus1 ;
	//
#if 1
	if( bShowDetail )
	fprintf( stdout, 
	"pic_width_in_mbs_minus1 at (0x%X,%d),(0x%X,%d),(0x%llX)\n", 
		GetBitAddr(), GetBitOffset(), 
		GetBitAddr()+GetBitOffset()/8, GetBitOffset()%8,
		SrcAddr(GetBitAddr()+GetBitOffset()/8,0) );
#endif
	int BitSt = GetBitOffset();
	pic_width_in_mbs_minus1        = GetBitStreamGolomb( fp );
	UpdateProcedureGolomb( n_sequenceParameters, ID_HSV, 
		GetBitOffset()-BitSt,
		GolombValue(GolombNext(pic_width_in_mbs_minus1,1)) );
	///
	BitSt = GetBitOffset();
	pic_height_in_map_units_minus1 = GetBitStreamGolomb( fp );
	UpdateProcedureGolomb( n_sequenceParameters, ID_VSV, 
		GetBitOffset()-BitSt,
		GolombValue(GolombNext(pic_width_in_mbs_minus1,1)) );
	//
	pic_width = (pic_width_in_mbs_minus1+1)*16;
	pic_height= (pic_height_in_map_units_minus1+1)*16;
	if( bShowDetail )
	fprintf( stdout, "pic_width =%d\n", pic_width );
	if( bShowDetail )
	fprintf( stdout, "pic_height=%d\n", pic_height );

	if( nEditFrameMbs>=0 )
	{	// make frame_mbs_only_flag = X
	    ShowBitAddr( );
	    DoUpdate( GetBitAddr(), GetBitOffset()+GetByteSkip()*8, 
	    	1, nEditFrameMbs );
	}
	avc_frame_mbs_only_flag = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "frame_mbs_only_flag=%d\n", avc_frame_mbs_only_flag );

	if( nEditFrameMbs>=0 )
	{
	    if( avc_frame_mbs_only_flag==1 )
	    {
	    	if( nEditFrameMbs==0 )	// 1->0
		{
		    SetSpecial( SET_START_END );
		    bEditSPS = EDIT_INS;
		}
	    } else {
	    	if( nEditFrameMbs==1 )	// 0->1
		{
		    SetSpecial( SET_START_END );
		    bEditSPS = EDIT_CUT;
		}
	    }
	}
	if( avc_frame_mbs_only_flag==0 )
	{
	    adaptive_frame_field_flag = GetBitStream( fp, 1 );
	} 
	direct_8x8_inference_flag = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "direct_8x8=%d\n", direct_8x8_inference_flag );
	frame_cropping_flag = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "cropping=%d\n", frame_cropping_flag );
	if( frame_cropping_flag )
	{
	    frame_crop_left_offset   = GetBitStreamGolomb( fp );
	    frame_crop_right_offset  = GetBitStreamGolomb( fp );
	    frame_crop_top_offset    = GetBitStreamGolomb( fp );
	    frame_crop_bottom_offset = GetBitStreamGolomb( fp );
	    if( bShowDetail )
	    {
		fprintf( stdout, "crop_left  =%d\n", frame_crop_left_offset );
		fprintf( stdout, "crop_right =%d\n", frame_crop_right_offset );
		fprintf( stdout, "crop_top   =%d\n", frame_crop_top_offset );
		fprintf( stdout, "crop_bottom=%d\n", frame_crop_bottom_offset );
	    }
	}
	vui_parameters_present_flag = GetBitStream( fp, 1 );
	if( bShowDetail )
	fprintf( stdout, "vui_parameters=%d\n", vui_parameters_present_flag );

	int time_scale=(-1);
	int num_units_in_tick=(-1);
	int fixed_framerate_flag=(-1);
	if( vui_parameters_present_flag )
	{
	    vui_parameters( fp, buffer, seq_parameter_set_id,
	    	&num_units_in_tick, &time_scale, &fixed_framerate_flag );
	}
//	bDebugGol = 0;

	n_sequenceParameters++;

	{
	static char fixed[][8] = { " - ", "nofix", "fixed" };
	char profile_str[80];
	char ip = avc_frame_mbs_only_flag ? 'P' : 'I';
	int height = pic_height;
	if( avc_frame_mbs_only_flag==0 )
	    height=height*2;
	switch( profile_idc )
	{
	case 66 :
	    sprintf( profile_str, "Constrained Baseline profile" );
	    sprintf( profile_str, "Extended profile" );
	    break;
	case 77 :
	    sprintf( profile_str, "Main profile" );
	    break;
	}

	if( time_scale<0 )
	{
	fprintf( stdout, 
"SPS(%d) : Profile(%d) Const(0x%02X) Level(%d) : %4dx%4d no_fps %c (%s)\n",
	    seq_parameter_set_id,
	    profile_idc,
	    constraint,
	    level_idc,
	    pic_width,
	    height,
	    ip,
	    fixed[fixed_framerate_flag+1] );
	} else {
	fprintf( stdout, 
"SPS(%d) : Profile(%d) Const(0x%02X) Level(%d) : %4dx%4d %d/%d/2Hz %c (%s)\n",
	    seq_parameter_set_id,
	    profile_idc,
	    constraint,
	    level_idc,
	    pic_width,
	    height,
	    time_scale,
	    num_units_in_tick,
	    ip,
	    fixed[fixed_framerate_flag+1] );
	}
	}

#if 0
	if( skipContent( fp, buffer, 0 )==1 )
	{
	    ret = 1;
	} else {
	    EXIT();
	}
#endif
	if( bEditSPS )
	{
	    int SpecialTermAddr=GetBitAddr();
	    int SpecialTermBits=GetBitOffset();
	    fprintf( stdout, "Special%d (%X,%d) (%X,%d) (%X,%d)\n",
	    	bEditSPS,
	    	SpecialStartAddr, SpecialStartBits,
	    	SpecialEndAddr, SpecialEndBits,
	    	SpecialTermAddr, SpecialTermBits );
	    g_update_addrS[g_update_countS] = GetBitAddr();
	    g_update_bit_S[g_update_countS] = SpecialStartBits;
	    g_update_bit_E[g_update_countS] = SpecialEndBits;
	    g_update_bit_T[g_update_countS] = SpecialTermBits;
	    g_update_mode [g_update_countS] = bEditSPS;
	    g_update_countS++;
	}
	if( bDumpSPS )
	{
	    int i=0;
	    for( i=0; i<4; i++ )
	    {
	    	fprintf( stdout, "%02X ", buffer[i] );
	    }
	    DumpBitBuffer( );
#if 0
void DumpBitBuffer( )
{
	    if( BitBuffer )
	    {
		for( j=0; j<BitReaded/8; j++ )
		{
		    fprintf( stdout, "%02X ", BitBuffer[j] );
		    if( ((i+j)&15)==15 )
			fprintf( stdout, "\n" );
		}
	    }
	    if( (i+j)&15 )
		fprintf( stdout, "\n" );
}
#endif
	}
	return ret;
}

// 2012.9.27 : try
int PictureParameterSet( FILE *fp, unsigned char *buffer )
{
int ret=0;
int i;

	if( bShowDetail )
	fprintf( stdout, "PictureParameterSet()\n" );
	// -------------------------------------------
	InitBitStream( );
	bDebugGolB = bDebugGol;
	//
	//
	int pic_parameter_set_id = GetBitStreamGolomb( fp );
	int seq_parameter_set_id = GetBitStreamGolomb( fp );
	PPStoSPS[pic_parameter_set_id] = seq_parameter_set_id;
	avc_PPS_id = pic_parameter_set_id;
	if( bShowDetail )
	{
		fprintf( stdout, "PPStoSPS[%d]=%d\n", 
			pic_parameter_set_id,
			seq_parameter_set_id );
	}
	int entropy_coding_mode_flag = GetBitStream( fp, 1 );
	avc_bottom_field_pic_order_in_frame_present_flag =GetBitStream( fp, 1 );
	int num_slice_groups_minus1 = GetBitStreamGolomb( fp );

	int slice_group_change_direction_flag=(-1);
	int slice_group_change_rate_minus1   =(-1);
	int pic_size_in_map_units_minus1     =(-1);
	if( num_slice_groups_minus1>0 )
	{
	    int iGroup;
	    int slice_group_map_type = GetBitStreamGolomb( fp );
	    switch( slice_group_map_type )
	    {
	    case 0 :
	    	for( iGroup=0; iGroup<=num_slice_groups_minus1; iGroup++ )
		{
		    int run_length_minus1 = GetBitStreamGolomb( fp );
		    if( bShowDetail )
		    fprintf( stdout, "run_length_minus1=%d\n",
			    run_length_minus1 );
		}
		break;
	    case 2 :
	    	for( iGroup=0; iGroup<=num_slice_groups_minus1; iGroup++ )
		{
		    int top_left = GetBitStreamGolomb( fp );
		    int bottom_right = GetBitStreamGolomb( fp );
		    if( bShowDetail )
		    {
		    	fprintf( stdout, "top_left=%d\n", top_left );
			fprintf( stdout, "bottom_right=%d\n", bottom_right );
		    }
		}
		break;
	    case 3 :
	    case 4 :
	    case 5 :
		slice_group_change_direction_flag = GetBitStream( fp, 1 );
		slice_group_change_rate_minus1 = GetBitStreamGolomb( fp );
		break;
	    case 6 :
	    	pic_size_in_map_units_minus1 = GetBitStreamGolomb( fp );
		for( i=0; i<=pic_size_in_map_units_minus1; i++ )
		{
		    int slice_group_id = GetBitStream( fp, 
					    num_slice_groups_minus1+1 );
		    if( bShowDetail )
		    {
		    	fprintf( stdout, "slice_group_id[%d]=%d\n",
				i, slice_group_id );
		    }
		}
		break;
	    }
	}
	int num_ref_idx_l0_default_active_minus1 = GetBitStreamGolomb( fp );
	int num_ref_idx_l1_default_active_minus1 = GetBitStreamGolomb( fp );
	int weighted_pred_flag  = GetBitStream( fp, 1 );
	int weighted_bipred_idc = GetBitStream( fp, 2 );
	int pic_init_qp_minus26 = GetBitStreamGolomb( fp );
	int pic_init_qs_minus26 = GetBitStreamGolomb( fp );
	int chroma_qp_index_offset = GetBitStreamGolomb( fp );
	int deblocking_filter_control_present_flag = GetBitStream( fp, 1 );
	int constrained_intra_pred_flag    = GetBitStream( fp, 1 );
	avc_redundant_pic_cnt_present_flag = GetBitStream( fp, 1 );

	if( bShowDetail )
	{
	    fprintf( stdout, "pic_parameter_set_id = %d\n", 
	    	pic_parameter_set_id );
	    fprintf( stdout, "seq_parameter_set_id = %d\n",
		seq_parameter_set_id );
	    fprintf( stdout, "entropy_coding_mode_flag = %d\n",
		entropy_coding_mode_flag );
	    fprintf( stdout, 
	    	"bottom_field_pic_order_in_frame_present_flag = %d\n", 
		avc_bottom_field_pic_order_in_frame_present_flag );
	    fprintf( stdout, "num_slice_groups_minus1 = %d\n",
		num_slice_groups_minus1 );
	    fprintf( stdout, "num_ref_idx_l0_default_active_minus1=%d\n",
		num_ref_idx_l0_default_active_minus1 );
	    fprintf( stdout, "num_ref_idx_l1_default_active_minus1=%d\n",
		num_ref_idx_l1_default_active_minus1 );
	    fprintf( stdout, "weighted_pred_flag =%d\n",
		weighted_pred_flag  );
	    fprintf( stdout, "weighted_bipred_idc=%d\n",
		weighted_bipred_idc );
	    fprintf( stdout, "pic_init_qp_minus26=%d\n",
		pic_init_qp_minus26 );
	    fprintf( stdout, "pic_init_qs_minus26=%d\n",
		pic_init_qs_minus26 );
	    fprintf( stdout, "chroma_qp_index_offset=%d\n",
		chroma_qp_index_offset );
	    fprintf( stdout, "deblocking_filter_control_present_flag=%d\n",
		deblocking_filter_control_present_flag );
	    fprintf( stdout, "constrained_intra_pred_flag=%d\n",
		constrained_intra_pred_flag );
	    fprintf( stdout, "redundant_pic_cnt_present_flag=%d\n",
		avc_redundant_pic_cnt_present_flag );
	}
#if 1
	if( skipContent( fp, buffer, 0 )==1 )
	{
	    ret = 1;
	} else {
	    EXIT();
	}
#endif
	return ret;
}

int ScalabilitySEI( FILE *fp, unsigned char *buffer )
{
int n, j;
int temporal_id_nesting_flag = (-1);
int priority_layer_info_present_flag = (-1);
int priority_id_setting_flag = (-1);
int num_layers_minus1 = (-1);
int layer_id = (-1);
int priority_id = (-1);
int discardable_flag = (-1);
int dependency_id = (-1);
int quality_id = (-1);
int temporal_id = (-1);
int sub_pic_layer_flag = (-1);
int sub_region_layer_flag = (-1);
int iroi_division_info_present_flag = (-1);
int profile_level_info_present_flag = (-1);
int bitrate_info_present_flag = (-1);
int frm_rate_info_present_flag = (-1);
int frm_size_info_present_flag = (-1);
int layer_dependency_info_present_flag = (-1);
int parameter_sets_info_present_flag = (-1);
int bitstream_restriction_info_present_flag = (-1);
int exact_inter_layer_pred_flag = (-1);
int exact_sample_value_match_flag = (-1);
int layer_conversion_flag = (-1);
int layer_output_flag = (-1);
int layer_profile_level_idc = (-1);
int avg_bitrate = (-1);
int max_bitrate_layer = (-1);
int max_bitrate_layer_representation = (-1);
int max_bitrate_calc_window = (-1);
int constant_frm_rate_idc = (-1);
int avg_frm_rate = (-1);
int frm_width_in_mbs_minus1  = (-1);
int frm_height_in_mbs_minus1 = (-1);
int base_region_layer_id = (-1);
int dynamic_rect_flag = (-1);
int horizontal_offset = (-1);
int vertical_offset   = (-1);
int region_width      = (-1);
int region_height     = (-1);
int roi_id = (-1);
int iroi_grid_flag = (-1);
int grid_width_in_mbs_minus1 = (-1);
int grid_height_in_mbs_minus1 = (-1);
int num_rois_minus1 = (-1);
int first_mb_in_roi = (-1);
int roi_width_in_mbs_minus1 = (-1);
int roi_height_in_mbs_minus1 = (-1);

	// ------------------------------------------------------
	fprintf( stdout, "ScalabilitySEI()\n" );
	InitBitStream( );
//	bDebugGolB = bDebugGol = 1;
	bDebugGolB = bDebugGol;
	temporal_id_nesting_flag         = GetBitStream( fp, 1 );
	priority_layer_info_present_flag = GetBitStream( fp, 1 );
	priority_id_setting_flag         = GetBitStream( fp, 1 );
	num_layers_minus1                = GetBitStreamGolomb( fp );
	for( n=0; n<=num_layers_minus1; n++ )
	{
	    layer_id                                = GetBitStreamGolomb( fp );
	    priority_id                             = GetBitStream( fp, 6 );
	    discardable_flag                        = GetBitStream( fp, 1 );
	    dependency_id                           = GetBitStream( fp, 3 );
	    quality_id                              = GetBitStream( fp, 4 );
	    temporal_id                             = GetBitStream( fp, 3 );
	    sub_pic_layer_flag                      = GetBitStream( fp, 1 );
	    sub_region_layer_flag                   = GetBitStream( fp, 1 );
	    iroi_division_info_present_flag         = GetBitStream( fp, 1 );
	    profile_level_info_present_flag         = GetBitStream( fp, 1 );
	    bitrate_info_present_flag               = GetBitStream( fp, 1 );
	    frm_rate_info_present_flag              = GetBitStream( fp, 1 );
	    frm_size_info_present_flag              = GetBitStream( fp, 1 );
	    layer_dependency_info_present_flag      = GetBitStream( fp, 1 );
	    parameter_sets_info_present_flag        = GetBitStream( fp, 1 );
	    bitstream_restriction_info_present_flag = GetBitStream( fp, 1 );
	    exact_inter_layer_pred_flag             = GetBitStream( fp, 1 );
	    if( sub_pic_layer_flag || iroi_division_info_present_flag )
		exact_sample_value_match_flag       = GetBitStream( fp, 1 );
	    layer_conversion_flag                   = GetBitStream( fp, 1 );
	    layer_output_flag                       = GetBitStream( fp, 1 );
	    if( profile_level_info_present_flag )
	    {
		layer_profile_level_idc = GetBitStream( fp, 24 );
		fprintf( stdout, "layer_profile_level=%X\n",
			layer_profile_level_idc );
	    }
	    if( bitrate_info_present_flag==1 )
	    {
		avg_bitrate                      = GetBitStream( fp, 16 );
		max_bitrate_layer                = GetBitStream( fp, 16 );
		max_bitrate_layer_representation = GetBitStream( fp, 16 );
		max_bitrate_calc_window          = GetBitStream( fp, 16 );
	    }
	    if( frm_rate_info_present_flag==1 )
	    {
		constant_frm_rate_idc = GetBitStream( fp, 2 );
		avg_frm_rate          = GetBitStream( fp, 16 );
	    }
	    if( frm_size_info_present_flag==1 )
	    {
		frm_width_in_mbs_minus1  = GetBitStreamGolomb( fp );
		frm_height_in_mbs_minus1 = GetBitStreamGolomb( fp );
	    }
	    if( sub_region_layer_flag==1 )
	    {
		base_region_layer_id = GetBitStreamGolomb( fp );
		dynamic_rect_flag = GetBitStream( fp, 1 );
		if( dynamic_rect_flag==0 )
		{
		    horizontal_offset = GetBitStream( fp, 16 );
		    vertical_offset   = GetBitStream( fp, 16 );
		    region_width      = GetBitStream( fp, 16 );
		    region_height     = GetBitStream( fp, 16 );
		}
	    }
	    if( sub_pic_layer_flag==1 )
		roi_id = GetBitStreamGolomb( fp );
	    if( iroi_division_info_present_flag==1 )
	    {
		iroi_grid_flag = GetBitStream( fp, 1 );
		if( iroi_grid_flag )
		{
		    grid_width_in_mbs_minus1 = GetBitStreamGolomb( fp );
		    grid_height_in_mbs_minus1 = GetBitStreamGolomb( fp );
		} else {
		    num_rois_minus1 = GetBitStreamGolomb( fp );
		    for( j=0; j<num_rois_minus1; j++ )
		    {
			first_mb_in_roi = GetBitStreamGolomb( fp );
			roi_width_in_mbs_minus1 = GetBitStreamGolomb( fp );
			roi_height_in_mbs_minus1 = GetBitStreamGolomb( fp );
		    }
		}
	    }
	}

	return 0;
}
// =============================================================
// SEI
//
int initial_cpb_removal_delay[8];
int initial_cpb_removal_delay_offset[8];

void SEI( char msg[], int payloadSize )
{
	if( bShowDetail )
	fprintf( stdout, "==============\n" );
	fprintf( stdout, "SEI : %s(%s) size=0x%X\n", 
		msg, AddrStr2(g_addr), payloadSize );
}

int buffering_period( FILE *fp, unsigned char buffer[], int payloadSize )
{
int set_id=(-1);
int i;
	SEI( "buffering_period SEI", payloadSize );
#if 1
	set_id = GetBitStreamGolomb( fp );
	if( bShowDetail )
	fprintf( stdout, "id=%d\n", set_id );
	int NalHrdBpPresentFlag = avc_nal_hrd_parameters_present_flag[PPStoSPS[avc_PPS_id]];
	int VclHrdBpPresentFlag = avc_vcl_hrd_parameters_present_flag[PPStoSPS[avc_PPS_id]];
	if( bShowDetail )
	fprintf( stdout, "NalHrdBpPrsentFlag=%d\n", NalHrdBpPresentFlag );
	if( bShowDetail )
	fprintf( stdout, "VclHrdBpPrsentFlag=%d\n", VclHrdBpPresentFlag );
	if( bShowDetail )
	fprintf( stdout, "cpb_cnt=%d\n", avc_cpb_cnt );
	if( NalHrdBpPresentFlag==1 )
	{
	    for( i=0; i<=avc_cpb_cnt; i++ )
	    {
	    	initial_cpb_removal_delay[i] = 
		    GetBitStream( fp, avc_initial_cpb_removal_delay_length );
	    	initial_cpb_removal_delay_offset[i] =
		    GetBitStream( fp, avc_initial_cpb_removal_delay_length );
		if( bShowDetail )
		fprintf( stdout, "initial_cpb_removal_delay[%d]=%d\n",
			i, initial_cpb_removal_delay[i] );
		if( bShowDetail )
		fprintf( stdout, "initial_cpb_removal_delay_offset[%d]=%d\n",
			i, initial_cpb_removal_delay_offset[i] );
	    }
	}
	if( VclHrdBpPresentFlag==1 )
	{
	    for( i=0; i<=avc_cpb_cnt; i++ )
	    {
	    	initial_cpb_removal_delay[i] = 
		    GetBitStream( fp, avc_initial_cpb_removal_delay_length );
	    	initial_cpb_removal_delay_offset[i] =
		    GetBitStream( fp, avc_initial_cpb_removal_delay_length );
		if( bShowDetail )
		fprintf( stdout, "initial_cpb_removal_delay[%d]=%d\n",
			i, initial_cpb_removal_delay[i] );
		if( bShowDetail )
		fprintf( stdout, "initial_cpb_removal_delay_offset[%d]=%d\n",
			i, initial_cpb_removal_delay_offset[i] );
	    }
	}
	return 0;
#else
	if( skipContent( fp, buffer, 0 )==1 )
	{
	} else {
	    EXIT();
	}
	return 1;
#endif
}


int pic_timing( FILE *fp, unsigned char buffer[], int payloadSize )
{
int NumClockTS=1;
int i;
int cpb_removal_delay = (-1);
int dpb_output_delay  = (-1);
int pic_struct = (-1);
int clock_timestamp_flag = (-1);
int ct_type = (-1);
int nuit_field_base_flag = (-1);
int counting_type = (-1);
int full_timestamp_flag = (-1);
int discontinuity_flag = (-1);
int cnt_dropped_flag = (-1);
int n_frames = (-1);
int seconds_value = (-1);
int minutes_value = (-1);
int hours_value   = (-1);
int seconds_flag  = (-1);
int minutes_flag  = (-1);
int hours_flag  = (-1);
int time_offset = (-1);

//	bDebugGolB = bDebugGol = 1;
	bDebugGolB = bDebugGol;
	SEI( "pic_timing SEI", payloadSize );
#if 1
	if( bRemovePicStruct )
	{
	    SetSpecial( SET_START );
	}

	if( bShowDetail )
	fprintf( stdout, "CpbDpbDelaysPresentFlag(%d:%d)=%d\n",
		avc_PPS_id, PPStoSPS[avc_PPS_id],
		avc_CpbDpbDelaysPresentFlag[PPStoSPS[avc_PPS_id]] );

	if( avc_CpbDpbDelaysPresentFlag[PPStoSPS[avc_PPS_id]]==1 )
	{
	    cpb_removal_delay = GetBitStream( fp, 
	    	avc_cpb_removal_delay_length );
	    dpb_output_delay  = GetBitStream( fp, 
	    	avc_dpb_output_delay_length );
	    if( bShowDetail )
	    fprintf( stdout, "cpb_removal_delay=%d(0x%X)\n", 
	    	cpb_removal_delay, cpb_removal_delay );
	    if( bShowDetail )
	    fprintf( stdout, "dpb_output_delay =%d\n", dpb_output_delay  );
//	    if( bShowDetail==0 )
	    {
	    	if( bTimeAnalyze )
		fprintf( stdout, "pic_timing(%4d,%4d)\n", 
			cpb_removal_delay, dpb_output_delay );
	    }
	}

	if( bShowDetail )
	fprintf( stdout, "pic_struct_present_flag(%d)=%d\n",
	    avc_PPS_id,
	    avc_pic_struct_present_flag[PPStoSPS[avc_PPS_id]] );
	if( avc_pic_struct_present_flag[PPStoSPS[avc_PPS_id]]==1 )
	{
/*
	    SpecialStartAddr = BitAddr;
	    SpecialStartBits = BitOffset;
*/
	    pic_struct = GetBitStream( fp, 4 );
	    switch( pic_struct )
	    {
	    case 0 :
	    case 1 :
	    case 2 :
	    	NumClockTS = 1;
		break;
	    case 3 :
	    case 4 :
	    case 7 :
	    	NumClockTS = 2;
		break;
	    case 5 :
	    case 6 :
	    case 8 :
	    	NumClockTS = 3;
		break;
	    default :
	    	fprintf( stdout, "Unknown pic_struct(%d)\n", pic_struct );
		break;
	    }
	    if( bShowDetail )
	    fprintf( stdout, "pic_struct=%d\n", pic_struct );
	    if( bShowDetail )
	    fprintf( stdout, "NumClockTS=%d\n", NumClockTS );
	    for( i=0; i<NumClockTS; i++ )
	    {
		if( bShowDetail )
		fprintf( stdout, "NumClockTS(%d/%d)\n", i, NumClockTS );
		clock_timestamp_flag = GetBitStream( fp, 1 );
		if( bShowDetail )
		    fprintf( stdout, "clock_timestamp_flag=%d\n",
			clock_timestamp_flag );
		if( clock_timestamp_flag==1 )
		{
		    ct_type              = GetBitStream( fp, 2 );
		    if( bShowDetail )
		    fprintf( stdout, "ct_type=%d\n", ct_type );
		    nuit_field_base_flag = GetBitStream( fp, 1 );
		    counting_type        = GetBitStream( fp, 5 );
		    full_timestamp_flag  = GetBitStream( fp, 1 );
		    discontinuity_flag   = GetBitStream( fp, 1 );
		    cnt_dropped_flag     = GetBitStream( fp, 1 );
		    n_frames             = GetBitStream( fp, 8 );
		    if( bShowDetail )
		    fprintf( stdout, "n_frames=%d\n", n_frames );
		    if( bShowDetail )
		    fprintf( stdout, "full_timestamp_flag=%d\n", 
		    	full_timestamp_flag );
		    if( full_timestamp_flag==1 )
		    {
			seconds_value = GetBitStream( fp, 6 );
			minutes_value = GetBitStream( fp, 6 );
			hours_value   = GetBitStream( fp, 5 );
		    } else {
			seconds_flag  = GetBitStream( fp, 1 );
			if( bShowDetail )
			fprintf( stdout, "seconds_flag=%d\n", seconds_flag );
			if( seconds_flag )
			{
			    seconds_value = GetBitStream( fp, 6 );
			    minutes_flag  = GetBitStream( fp, 1 );
			    if( bShowDetail )
			    fprintf( stdout, "minutes_flag=%d\n", minutes_flag );
			    if( minutes_flag )
			    {
				minutes_value = GetBitStream( fp, 6 );
				hours_flag  = GetBitStream( fp, 1 );
				fprintf( stdout, "hours_flag=%d\n", 
				    hours_flag );
				if( hours_flag )
				{
				    hours_value = GetBitStream( fp, 5 );
				}
			    }
			}
		    }
		    if( seconds_value>=0 )
		    fprintf( stdout, "seconds_value=%d\n", seconds_value );
		    if( minutes_value>=0 )
		    fprintf( stdout, "minutes_value=%d\n", minutes_value );
		    if( hours_value>=0 )
		    fprintf( stdout, "hours_value  =%d\n", hours_value );
		    if( avc_time_offset_length>0 )
		    {
			if( bShowDetail )
			fprintf( stdout, "time_offset_length=%d\n", 
				avc_time_offset_length );
//			time_offset = GetBitStreamGolomb( fp );
// 2011/9/29
			time_offset = GetBitStream( fp, 
				avc_time_offset_length );
			if( bShowDetail )
			fprintf( stdout, "time_offset =%d(0x%X)\n", 
				time_offset, time_offset );
		    }
		}
	    }
	    if( bRemovePicStruct )
	    {
		int addr1 = SpecialStartAddr + SpecialStartBits/8;
		int addr2 = GetBitAddr()     + GetBitOffset()  /8;
		int addr;
		int SP=GetByteSkip();
		DoUpdate( addr1-2, 0, 8, 0x03 );	// filler_payload
		DoUpdate( addr1-1, 0, 8, payloadSize+SP );
		for( addr=addr1; addr<=(addr2+SP); addr++ )
		{
		    DoUpdate( addr, 0, 8, 0xFF );	// filler_payload
		}
	    }
	}
	return 0;
#else
	if( skipContent( fp, buffer, bDebugSkip )==1 )
	{
	} else {
	    EXIT();
	}
	return 1;
#endif
}

// 2011.7.28
int filler_payload( FILE *fp, unsigned char buffer[], int payloadSize )
{
int i;
int ff_byte;
int error=0;

	bDebugGolB = bDebugGol;
	SEI( "filler_payload", payloadSize );
#if 1
	for( i=0; i<payloadSize; i++ )
	{
	    ff_byte = GetBitStream( fp, 8 );
	    if( ff_byte!=0xFF )
	    {
	    	fprintf( stdout, "ff_byte=0x%02X\n", ff_byte );
		error++;
	    }
	}
	if( error==0 )
	    return 0;
	else
	    return 1;
#else
	if( skipContent( fp, buffer, bDebugSkip )==1 )
	{
	} else {
	    EXIT();
	}
	return 1;
#endif
}

int user_data_registered( FILE *fp, unsigned char buffer[], int payloadSize )
{
int i;
	bDebugGolB = bDebugGol;
	SEI( "user_data_registered", payloadSize );
#if 1
	unsigned char user_data;
	if( bUserData )
	{
	    fprintf( stdout, "[UserData] pts  = 0x%04X%06X\n", 
//		    userdata_ptsh, userdata_ptsl );
		    g_PTSH, g_PTSL );
	    fprintf( stdout, "[UserData] size = 0x%08X\n", payloadSize );
	    fprintf( stdout, "[UserData] data =" );
	}
	for( i=0; i<payloadSize; i++ )
	{
	    user_data = GetBitStream( fp, 8 );
	    if( bUserData )
		fprintf( stdout, " %02X", user_data );
	}
	if( bUserData )
	    fprintf( stdout, "\n" );
	b_user_data = 1;
	return 0;	// 2012.11.12
#else
	if( skipContent( fp, buffer, bDebugSkip )==1 )
	{
	} else {
	    EXIT();
	}
	return 1;
#endif
}

int user_data_unregistered( FILE *fp, unsigned char buffer[], int payloadSize )
{
int i;
	bDebugGolB = bDebugGol;
	SEI( "user_data_unregistered", payloadSize );
#if 1
	unsigned char user_data;
	if( bUserData )
	{
	    fprintf( stdout, "[UserData] pts  = 0x%04X%06X\n", 
//		    userdata_ptsh, userdata_ptsl );
		    g_PTSH, g_PTSL );
	    fprintf( stdout, "[UserData] size = 0x%08X\n", payloadSize );
	    fprintf( stdout, "[UserData] data =" );
	}
	for( i=0; i<payloadSize; i++ ) 
	{
	    user_data = GetBitStream( fp, 8 );
	    if( bUserData )
		fprintf( stdout, " %02X", user_data );
	}
	if( bUserData )
	    fprintf( stdout, "\n" );
	b_user_data = 1;
	return 0;	// 2012.11.12
#else
	if( skipContent( fp, buffer, bDebugSkip )==1 )
	{
	} else {
	    EXIT();
	}
	return 1;
#endif
}

int recovery_point( FILE *fp, unsigned char buffer[], int payloadSize )
{
int recovery_frame_cnt       = (-1);
int exact_match_flag         = (-1);
int broken_link_flag         = (-1);
int changing_slice_group_idc = (-1);

//	bDebugGolB = bDebugGol = 1;
	bDebugGolB = bDebugGol;
	SEI( "recovery_point SEI", payloadSize );
#if 1
	recovery_frame_cnt       = GetBitStreamGolomb( fp );
	exact_match_flag         = GetBitStream( fp, 1 );
	broken_link_flag         = GetBitStream( fp, 1 );
	changing_slice_group_idc = GetBitStream( fp, 2 );
	if( bShowDetail )
	{
	fprintf( stdout, "recovery_frame_cnt=%d\n", recovery_frame_cnt );
	fprintf( stdout, "exact_match_flag=%d\n", exact_match_flag );
	fprintf( stdout, "broken_link_flag=%d\n", broken_link_flag );
	fprintf( stdout, "changing_slice_group=%d\n", 
		changing_slice_group_idc );
	}
	return 0;
#else
	if( skipContent( fp, buffer, bDebugSkip )==1 )
	{
	} else {
	    EXIT();
	}
	return 1;
#endif
}
int frame_packing_arrangement( FILE *fp, unsigned char buffer[], 
	int payloadSize )
{
int frame_packing_arrangement_id       = (-1);
int frame_packing_arrangement_cancel_flag = (-1);
int frame_packing_arrangement_type = (-1);
int quincunx_sampling_flag         = (-1);
int content_interpretation_type    = (-1);
int spatial_flipping_flag          = (-1);
int frame0_flipped_flag            = (-1);
int field_views_flag               = (-1);
int current_frame_is_frame0_flag   = (-1);
int frame0_self_contained_flag     = (-1);
int frame1_self_contained_flag     = (-1);
int frame0_grid_position_x = (-1);
int frame0_grid_position_y = (-1);
int frame1_grid_position_x = (-1);
int frame1_grid_position_y = (-1);
int frame_packing_arrangement_reserved_byte = (-1);
int frame_packing_arrangement_repetition_period = (-1);
int frame_packing_arrangement_extension_flag = (-1);
int i;
//	bDebugGolB = bDebugGol = 1;
	bDebugGolB = bDebugGol;
	SEI( "frame_packing_arrangement SEI", payloadSize );
	
	int pos = ftell(fp);
	unsigned int l_addr = g_addr;
//	fprintf( stdout, "pos=0x%x\n", pos );
	unsigned char user_data;
	if( bUserData )
	{
	    fprintf( stdout, "[UserData] pts  = 0x%04X%06X\n", 
//		    userdata_ptsh, userdata_ptsl );
		    g_PTSH, g_PTSL );
	    fprintf( stdout, "[UserData] size = 0x%08X\n", payloadSize );
	    fprintf( stdout, "[UserData] data =" );
	}
	for( i=0; i<payloadSize; i++ )
	{
	    user_data = GetBitStream( fp, 8 );
	    if( bUserData )
		fprintf( stdout, " %02X", user_data );
	}
	if( bUserData )
	    fprintf( stdout, "\n" );
	b_user_data = 1;
	fseek( fp, pos, SEEK_SET );
	g_addr = l_addr;
	InitBitStream();
#if 1
	frame_packing_arrangement_id       = GetBitStreamGolomb( fp );
	fprintf( stdout, "frame_packing_arrangement_id=%d\n",
		frame_packing_arrangement_id );
	frame_packing_arrangement_cancel_flag = GetBitStream( fp, 1 );
	fprintf( stdout, "frame_packing_arrangement_cancel_flag=%d\n",
		frame_packing_arrangement_cancel_flag );
	if( frame_packing_arrangement_cancel_flag==0 )
	{
	    frame_packing_arrangement_type = GetBitStream( fp, 7 );
	    fprintf( stdout, "frame_packing_arrangement_type=%d\n",
		frame_packing_arrangement_type );
	    quincunx_sampling_flag         = GetBitStream( fp, 1 );
	    content_interpretation_type    = GetBitStream( fp, 6 );
	    spatial_flipping_flag          = GetBitStream( fp, 1 );
	    frame0_flipped_flag            = GetBitStream( fp, 1 );
	    field_views_flag               = GetBitStream( fp, 1 );
	    current_frame_is_frame0_flag   = GetBitStream( fp, 1 );
	    frame0_self_contained_flag     = GetBitStream( fp, 1 );
	    frame1_self_contained_flag     = GetBitStream( fp, 1 );
	    if( (quincunx_sampling_flag==0)
	    && (frame_packing_arrangement_type!=5) )
	    {
		frame0_grid_position_x = GetBitStream( fp, 4 );
		frame0_grid_position_y = GetBitStream( fp, 4 );
		frame1_grid_position_x = GetBitStream( fp, 4 );
		frame1_grid_position_y = GetBitStream( fp, 4 );
	    }
	    frame_packing_arrangement_reserved_byte = GetBitStream( fp, 8 );
	    frame_packing_arrangement_repetition_period = 
	    	GetBitStreamGolomb( fp );
	}
	frame_packing_arrangement_extension_flag = GetBitStream( fp, 1 );
	fprintf( stdout, "frame_packing_arrangement_extension_flag=%d\n",
	    frame_packing_arrangement_extension_flag );

	if( bFramePackingArrangement )
	    fprintf( stdout, 
	    "FPA[%d %d %d %d %d %d %d %d %d %d %d %1x%1x%1x%1x %02x %d %d]\n", 
	        frame_packing_arrangement_id,
	        frame_packing_arrangement_cancel_flag,
	        frame_packing_arrangement_type,
	        quincunx_sampling_flag,
	        content_interpretation_type,
	        spatial_flipping_flag,
	        frame0_flipped_flag,
	        field_views_flag,
	        current_frame_is_frame0_flag,
	        frame0_self_contained_flag,
	        frame1_self_contained_flag,
	        (frame0_grid_position_x == (-1) ? 0 : frame0_grid_position_x),
	        (frame0_grid_position_y == (-1) ? 0 : frame0_grid_position_y),
	        (frame1_grid_position_x == (-1) ? 0 : frame1_grid_position_x),
	        (frame1_grid_position_y == (-1) ? 0 : frame1_grid_position_y),
	        frame_packing_arrangement_reserved_byte,
	        frame_packing_arrangement_repetition_period,
	        frame_packing_arrangement_extension_flag );

	return 0;
#else
	if( skipContent( fp, buffer, bDebugSkip )==1 )
	{
	} else {
	    EXIT();
	}
	return 1;
#endif
}
int sub_seq_characteristics( FILE *fp, unsigned char buffer[], int payloadSize )
{
	SEI( "sub_seq_characteristics SEI", payloadSize );
	fprintf( stdout, "No parse\n" );
	if( skipContent( fp, buffer, 0 )==1 )
	{
	} else {
	    EXIT();
	}
	return 1;
}

int scene_info( FILE *fp, int payloadSize )
{
int scene_info_present_flag = (-1);
int scene_id = (-1);
int scene_transition_type = (-1);
int second_scene_id = (-1);
	// -------------------------------------------------
	SEI( "scene_info", payloadSize );
	scene_info_present_flag = GetBitStream( fp, 1 );
	fprintf( stdout, "scene_info_present_flag = %d\n", 
		scene_info_present_flag );
	if( scene_info_present_flag==1 )
	{
	    scene_id = GetBitStreamGolomb( fp );
	    fprintf( stdout, "scene_id = %d\n", scene_id );
	    scene_transition_type = GetBitStreamGolomb( fp );
	    fprintf( stdout, "scene_transition_typ = %d\n", 
	    	scene_transition_type );
	    if( scene_transition_type>3 )
		second_scene_id = GetBitStreamGolomb( fp );
	}
	return 0;
}

//int pan_scan_rect( FILE *fp, unsigned char *buffer )
int pan_scan_rect( FILE *fp, int payloadSize )
{
int id=(-1);
int cancel_flag=(-1);
int cnt_minus1=(-1);
int left_offset=(-1);
int right_offset=(-1);
int top_offset=(-1);
int bottom_offset=(-1);
int period=(-1);
int i;
	// -------------------------------------------------
	SEI( "pan_scan_rect", payloadSize );
	int tmp = bDebugGolB;
	bDebugGolB = 1;

	id          = GetBitStreamGolomb( fp );
	cancel_flag = GetBitStream( fp, 1 );
	fprintf( stdout, "id=%d\n", id );
	fprintf( stdout, "pan_scan_rect_cancel_flag=%d\n", cancel_flag );
	if( cancel_flag==0 )
	{
	    cnt_minus1  = GetBitStreamGolomb( fp );
	    for( i=0; i<=cnt_minus1; i++ )
	    {
		int BitSt;
		BitSt = GetBitOffset();
		left_offset   = GetBitStreamGolomb( fp );
//		int next = GolombNext( left_offset, value[ID_PANL] );
//		fprintf( stdout, "Update Golomb(%d->%d)\n", left_offset, next );
		UpdateProcedureGolomb( n_pan_scan, 
		    ID_PANL, GetBitOffset()-BitSt,
		    GolombValue(GolombNext(left_offset, value[ID_PANL])) );

		BitSt = GetBitOffset();
		right_offset  = GetBitStreamGolomb( fp );
		UpdateProcedureGolomb( n_pan_scan, 
		    ID_PANR, GetBitOffset()-BitSt,
		    GolombValue(GolombNext(right_offset, value[ID_PANR])) );

		BitSt = GetBitOffset();
		top_offset    = GetBitStreamGolomb( fp );
		UpdateProcedureGolomb( n_pan_scan, 
		    ID_PANT, GetBitOffset()-BitSt,
		    GolombValue(GolombNext(top_offset, value[ID_PANT])) );

		BitSt = GetBitOffset();
		bottom_offset = GetBitStreamGolomb( fp );
		UpdateProcedureGolomb( n_pan_scan, 
		    ID_PANB, GetBitOffset()-BitSt,
		    GolombValue(GolombNext(bottom_offset, value[ID_PANB])) );

		fprintf( stdout, 
		"pan_scan_cnt(%d) : left=%d, right=%d, top=%d, bottom=%d\n",
			i,
			left_offset,
			right_offset,
			top_offset,
			bottom_offset );
	    }
	    period = GetBitStreamGolomb( fp );
	    fprintf( stdout, "pan_scan_repetition_period=%d\n", period );
	}

	bDebugGolB = tmp;
	n_pan_scan ++;

	return 0;
}

int quality_layer_integrity_check( FILE *fp, unsigned char *buffer )
{
int i;
int cnt_minus1 = (-1);

	// -------------------------------------------------
	fprintf( stdout, "==============\n" );
	fprintf( stdout, "quality_layer_integrity_check() : (%s)\n", 
		AddrStr(GetBitAddr()+GetBitOffset()/8) );
	int tmp = bDebugGolB;
	bDebugGolB = 1;

	{
	    int entry_dependency_id;
	    int quality_layer_crc  ;
	    cnt_minus1  = GetBitStreamGolomb( fp );
	    fprintf( stdout, "num_minus1=%d\n", cnt_minus1 );
	    for( i=0; i<=cnt_minus1; i++ )
	    {
		entry_dependency_id   = GetBitStream( fp,  3 );
		quality_layer_crc     = GetBitStream( fp, 16 );
		fprintf( stdout, "%d : %d, %d\n",
			i, 
			entry_dependency_id,
			quality_layer_crc );
	    }
	}

	bDebugGolB = tmp;

	return 0;
}

int deblocking_filter_display_preference( FILE *fp, unsigned char *buffer,
	int payloadSize )
{
	SEI( "deblocking_filter_display_preference SEI", payloadSize );
	fprintf( stdout, "No parse\n" );
	if( skipContent( fp, buffer, 0 )==1 )
	{
	} else {
	    EXIT();
	}
	return 1;
}

int SupplementalEnhancementInformation( FILE *fp, unsigned char *buffer );

int mvc_scalable_nesting( FILE *fp, unsigned char buffer[], int payloadSize )
{
int ret=(-1);
	fprintf( stdout, "-----------------------------------------------\n" );
	SEI( "mvc_scalable_nesting SEI", payloadSize );
	int operation_point_flag = GetBitStream( fp, 1 );
	if( bShowMvcScalable )
	fprintf( stdout, "operation_point_flag=%d\n", operation_point_flag );
	if( operation_point_flag==1 )
	{
	    int num_view_components_op_minus1 = GetBitStreamGolomb( fp );
	    if( bShowMvcScalable )
	    fprintf( stdout, "num_view_components_op_minus1=%d\n",
	    	num_view_components_op_minus1 );
	    int i;
	    for( i=0; i<=num_view_components_op_minus1; i++ )
	    {
	    	int sei_op_view_id = GetBitStream( fp, 10 );
		if( bShowMvcScalable )
		fprintf( stdout, "sei_op_view_id = %2d\n", sei_op_view_id );
	    }
	    int sei_op_temporal_id = GetBitStream( fp, 3 );
	    if( bShowMvcScalable )
	    fprintf( stdout, "temporal_id = %d\n", sei_op_temporal_id );
	} else {
	    int all_view_components_in_au_flag = GetBitStream( fp, 1 );
	    if( bShowMvcScalable )
	    fprintf( stdout, "all_view_components_in_au_flag = %d\n", 
	    	all_view_components_in_au_flag );
	    if( all_view_components_in_au_flag==0 )
	    {
		int num_view_components_minus1 = GetBitStreamGolomb( fp );
		if( bShowMvcScalable )
		fprintf( stdout, "num_view_components_minus1=%d\n",
		    num_view_components_minus1 );
		int i;
		for( i=0; i<=num_view_components_minus1; i++ )
		{
		    int sei_view_id = GetBitStream( fp, 10 );
		    if( bShowMvcScalable )
		    fprintf( stdout, "sei_view_id = %2d\n", sei_view_id );
		}
	    }
	}
	fprintf( stdout, "-----------------------------------------------\n" );
	int aa = GetBitOffset()&7;
	if( bDebugSEI )
	fprintf( stdout, "BitOffset=%d, aa=%d\n", GetBitOffset(), aa );
	if( aa>0 )
	{
	     int dummy = GetBitStream( fp, 8-aa );
	     if( bDebug )
	     fprintf( stdout, "Dummy %d bits=0x%X\n", 8-aa,dummy );
	}
	ret = SupplementalEnhancementInformation( fp, buffer );
	if( ret==1 )
	{
	    buffer[1] = GetBitStream( fp, 8 );
	    buffer[2] = GetBitStream( fp, 8 );
	    buffer[3] = GetBitStream( fp, 8 );
	    fprintf( stdout, "(%02X %02X %02X)\n",
	    	buffer[1], buffer[2], buffer[3] );
	} else {
	    if( skipContent( fp, buffer, 0 )==1 )
	    {
	    } else {
		EXIT();
	    }
	}
	return 1;
}

int scalability_info( FILE *fp, unsigned char buffer[], int payloadSize )
{
	fprintf( stdout, "-----------------------------------------------\n" );
	SEI( "scalability_info SEI", payloadSize );

	int temporal_id_nesting_flag = GetBitStream( fp, 1 );
	int priority_layer_info_present_flag = GetBitStream( fp, 1 );
	int priority_id_setting_flag = GetBitStream( fp, 1 );
	int num_layers_minus1 = GetBitStreamGolomb( fp );

fprintf( stdout, "temporal_id_nesting_flag=%d\n", temporal_id_nesting_flag );
fprintf( stdout, "priority_layer_info_present_flag=%d\n", priority_layer_info_present_flag );
fprintf( stdout, "priority_id_setting_flag=%d\n", priority_id_setting_flag );
fprintf( stdout, "num_layers_minus1=%d\n", num_layers_minus1 );
/*
	for( i=0; i<=num_layers_minus1; i++ )
	{
	    int layer_id = GetBitStreamGolomb( fp );
	    int priority_id = GetBitStream( fp, 6 );
	}
*/
	if( skipContent( fp, buffer, 0 )==1 )
	{
	} else {
	    EXIT();
	}
	return 1;
}
int view_scalability_info( FILE *fp, unsigned char buffer[], int payloadSize )
{
	fprintf( stdout, "-----------------------------------------------\n" );
	SEI( "view_scalability_info SEI", payloadSize );

	if( skipContent( fp, buffer, 0 )==1 )
	{
	} else {
	    EXIT();
	}
	return 1;
}
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
int reserved_sei( FILE *fp, unsigned char buffer[], int payloadSize )
{
int i;
	fprintf( stdout, "-----------------------------------------------\n" );
	SEI( "reserved SEI", payloadSize );
#if 1
	for( i=0; i<payloadSize; i++ ) 
	{
	    int dummy = GetBitStream( fp, 8 );
	    if( bDebug )
	    	fprintf( stdout, "%02X ", dummy );
	}
	    if( bDebug )
	    	fprintf( stdout, "\n" );
#else
	if( skipContent( fp, buffer, 0 )==1 )
	{
	} else {
	    EXIT();
	}
#endif
	fprintf( stdout, "-----------------------------------------------\n" );
	return 1;
}

int SupplementalEnhancementInformation( FILE *fp, unsigned char *buffer )
{
int payloadType = (-1);
int payloadSize = (-1);
int nFF=0;
	// -------------------------------------------------
	if( bShowDetail )
	fprintf( stdout, "SupplementalEnhancementInformation()\n" );

	if( bDebugSEI )
	fprintf( stdout, "Addr(%s)\n", AddrStr(g_addr) );

	if( Packets==NULL )
	{
	    fprintf( stdout, "Packets=NULL\n" );
	    return -1;
	}
	int nSEI=0;
	InitBitStream( );
	while( 1 )
	{
	    if( bDebugSEI )
	    {
	    	fprintf( stdout, "+++++++++++++++(%2d)++++++++++++++\n", nSEI );
	    }
	    nFF=0;
	    do {
		payloadType     = GetBitStream( fp, 8 );
		if( payloadType==0xFF )
		    nFF++;
	    } while( payloadType==0xFF );
	    payloadType += nFF*0xFF;

	    nFF=0;
	    do {
		payloadSize     = GetBitStream( fp, 8 );
		if( payloadSize==0xFF )
		    nFF++;
	    } while( payloadSize==0xFF );
	    payloadSize += nFF*0xFF;
	    if( bDebugSEI )
	    {
	    fprintf( stdout, "payloadType=0x%X\n", payloadType );
	    fprintf( stdout, "payloadSize=0x%X\n", payloadSize );
	    }
	    Packets[0x0600 | payloadType]++;

//	    InitBitStream( );
	    int sei_start = GetBitOffset();
	    // SEI payload
	    if( payloadSize==0 )
	    {
		if( bDebugSEI )
	    	fprintf( stdout, "SEI done\n" );
#if 0
		char str[1024];
		sprintf( str, "SEI(%02X) ??", payloadType );
		SEI( str, payloadSize );
#endif
	    	break;
	    }
	    switch( payloadType )
	    {
	    case 0 :
		if( buffering_period( fp, buffer, payloadSize )==1 )
		    return 4;
		break;
	    case 1 :
		if( pic_timing( fp, buffer, payloadSize )==1 )
		    return 4;
		break;
	    case 3 :	// 2011.7.28
		if( filler_payload( fp, buffer, payloadSize )!=0 )
		    return 4;
		break;
	    case 4 :
		if( user_data_registered( fp, buffer, payloadSize )!=0 )
		    return 4;
		break;
	    case 5 :
		if( user_data_unregistered( fp, buffer, payloadSize )!=0 )
		    return 4;
		break;

	    case 6 :
		if( recovery_point( fp, buffer, payloadSize )!=0 )
		    return 4;
		break;
	    case 45 :
		if( frame_packing_arrangement( fp, buffer, payloadSize )==1)
		    return 4;
		break;

	    case 12 :
		sub_seq_characteristics( fp, buffer, payloadSize );
		return 4;
	    case 9 :
		scene_info( fp, payloadSize );
		break;
	    case 2 :
		pan_scan_rect( fp, payloadSize );
		break;
	// akagawa
	    case 32 :
		quality_layer_integrity_check( fp, buffer );
		break;
	// AKB
	    case 20 :
		if( deblocking_filter_display_preference( fp, buffer,
			payloadSize )==1 )
			return 4;
		break;
	// MVC (victor)
	    case 37 :
		if( mvc_scalable_nesting( fp, buffer, payloadSize )==1 )
		    return 4;
		break;
	    case 24 :
		if( scalability_info( fp, buffer, payloadSize )==1 )
		    return 4;
		break;
	// MVC (allegro)
	    case 38 :
	    	if( view_scalability_info( fp, buffer, payloadSize )==1 )
		    return 4;
	    	break;
	    default :
		fprintf( stdout, 
			"payloadType(%d) : not implemented (%s)\n",
			payloadType, AddrStr(g_addr) );
		if( payloadType>45 )
		{
		    if( reserved_sei( fp, buffer, payloadSize )==1 )
			return 4;
		} else {
		    if( bSkipError==0 )
			EXIT();
		    else
		    	fprintf( stdout, "SkipError\n" );
		}
		break;
	    }
	    ShowBitAddr( );

	    int rest = payloadSize*8;
	    if( bDebugSEI )
	    {
	    fprintf( stdout, 
		    "sei_start=%d,restBit=%d,BitOffset=%d,ByteSkip=%d*8\n", 
		    sei_start,
		    rest, GetBitOffset(), GetByteSkip() );
	    fprintf( stdout, "rest=%d, payloadSize=%d\n", rest, payloadSize);
	    fprintf( stdout, "BitOffset=%d, ByteSkip=%d\n", 
	    	GetBitOffset(), GetByteSkip());
	    }
	    if( (GetBitOffset()-GetByteSkip()*8-sei_start)>rest )
	    {
	    	fprintf( stdout, "payloadSize=%d\n", payloadSize );
	    	fprintf( stdout, 
		    "restBit=%d < (BitOffset=%d,ByteSkip=%d*8,sei_start=%d)\n", 
		    rest, GetBitOffset(), GetByteSkip(), sei_start );
		if( bSkipError==0 )
		    EXIT();
		else
		    fprintf( stdout, "SkipError\n" );
	    }
	    rest -= (GetBitOffset()-sei_start);	// 2011.9.29
	    while( rest>0 )
	    {
	    	int dummy;
	    	int size = (rest&7) ? rest&7 : 8;
		dummy = GetBitStream( fp, size );
	    if( bDebugSEI )
		fprintf( stdout, 
		"dummy(rest=%2d, size=%d, dummy=%02X)\n", rest, size, dummy );
		rest -= size;
	    }
	    if( bDebugSEI )
	    {
	    fprintf( stdout, "g_addr=%llX\n", g_addr );
	    fprintf( stdout, "BitOffset=%d, ByteSkip=%d\n", 
	    	GetBitOffset(), GetByteSkip());
	    }
	    nSEI++;
	}
	    if( bDebugSEI )
	    	fprintf( stdout, "===============(%2d)===============n", nSEI );
	buffer[0]=0;
	buffer[1]=0;
	if( payloadType==0 )
	    return 2;	// readed size
	else
	    return 1;	// readed size
}
// --------------------------------------------------------------------------

int nal_unit_header_svc_extension( FILE *fp, unsigned char *buffer )
{
	fprintf( stdout, "-----------------------------------------------\n" );
	fprintf( stdout, "nal_unit_header_svc_extension()\n" );
	if( skipContent( fp, buffer, 0 )==1 )
	{
	} else {
	    EXIT();
	}
	return 0;
}
int mvc_extension = 0;
int mvc_non_idr_flag     = (-1);
int mvc_priority_id      = (-1);
int mvc_view_id          = (-1);
int mvc_temporal_id      = (-1);
int mvc_anchor_pic_flag  = (-1);
int mvc_inter_view_flag  = (-1);
int nal_unit_header_mvc_extension( FILE *fp, unsigned char *buffer )
{
	fprintf( stdout, "nal_unit_header_mvc_extension()\n" );
	mvc_extension = 1;
	mvc_non_idr_flag     = GetBitStream( fp, 1 );
	mvc_priority_id      = GetBitStream( fp, 6 );
	mvc_view_id          = GetBitStream( fp, 10 );
	mvc_temporal_id      = GetBitStream( fp, 3 );
	mvc_anchor_pic_flag  = GetBitStream( fp, 1 );
	mvc_inter_view_flag  = GetBitStream( fp, 1 );
	int reserved_one_bit = GetBitStream( fp, 1 );
	if( bShowMvcExtention )
	{
	fprintf( stdout, "non_idr_flag    =%d\n", mvc_non_idr_flag );
	fprintf( stdout, "priority_id     =%d\n", mvc_priority_id );
	fprintf( stdout, "view_id         =%d\n", mvc_view_id );
	fprintf( stdout, "temporal_id     =%d\n", mvc_temporal_id );
	fprintf( stdout, "anchor_pic_flag =%d\n", mvc_anchor_pic_flag );
	fprintf( stdout, "inter_view_flag =%d\n", mvc_inter_view_flag );
	fprintf( stdout, "reserved_one_bit=%d\n", reserved_one_bit );
	}
	return 0;
}

// -------------------------------------------------------------

int AccessUnitDelimiter( FILE *fp, unsigned char *buffer )
{
int picType=(-1);
	// -------------------------------------------------
	InitBitStream( );
	picType = GetBitStream( fp, 3 );
	if( bShowDetail )
	fprintf( stdout, "AccessUnitDelimiter(%d)\n", picType );
	avc_IdrPicFlag = 0;
	if( picType==0 )
	{
	    fprintf( stdout, "IDR begin(%s)\n", AddrStr(g_addr) );
	    avc_IdrPicFlag = 1;
	    // Reset parameters
	    avc_cpb_cnt = (-1);
	    avc_time_offset_length = (-1);

	    avc_nal_hrd_parameters_present_flag[PPStoSPS[avc_PPS_id]]=(-1);
	    avc_vcl_hrd_parameters_present_flag[PPStoSPS[avc_PPS_id]]=(-1);
	    avc_pic_struct_present_flag        [PPStoSPS[avc_PPS_id]]=(-1);
	    avc_CpbDpbDelaysPresentFlag        [PPStoSPS[avc_PPS_id]]=(-1);
	}
	return 0;
}

int FillerData( FILE *fp, unsigned char *buffer )
{
int ffByte;
int nSize=0;
	InitBitStream( );
	int tmp = bDebugGolB;
	bDebugGolB = 0;
	while( 1 )
	{
//	fprintf( stdout, "0x%llX,%d : ", g_addr, BitOffset );
	    ffByte = GetBitStream( fp, 8 );
	    nSize++;
	    if( ffByte!=0xFF )
		break;
	}
	bDebugGolB = tmp;
	if( bShowDetail )
	fprintf( stdout, "FillerData() : %d bytes\n", nSize );
	return 0;
}

int Slice_header( FILE *fp, unsigned char *buffer, 
	int nal_unit_type, int nal_ref_idc )
{
int field_pic_flag = (-1);
int bEdit = EDIT_NONE;
int tmp = bDebugGolB;

	if( bParseSliceHeader==0 )
	    return 0;
	InitBitStream( );
	bDebugGolB = 0;

//	if( bShowDetail )
//	fprintf( stdout, "Slice_header()\n" );
	int first_mb_in_slice    = GetBitStreamGolomb( fp );
	int slice_type           = GetBitStreamGolomb( fp );
	int pic_parameter_set_id = GetBitStreamGolomb( fp );
	if( PPStoSPS[pic_parameter_set_id]<0 )
	{
	    fprintf( stdout, "************ Error ************\n" );
	    fprintf( stdout, "Invalid pic_parameter_set_id(%d)\n",
		    pic_parameter_set_id );
//	    EXIT();
	}
	avc_PPS_id = pic_parameter_set_id;
//	fprintf( stdout, "PPS_id=%d\n", avc_PPS_id );
	if( avc_separate_colour_plane_flag==1 )
	{
	    int colour_plane_id = GetBitStream( fp, 1 );
	    if( bShowDetail )
		fprintf( stdout, "colour_plane_id = %d\n", colour_plane_id );
	}
	int frame_num_bits = avc_log2_max_frame_num_minus4+4;
	int frame_num            = GetBitStream( fp, frame_num_bits );
	if( bShowDetail )
	{
	fprintf( stdout, "first_mb_in_slice = %d\n", first_mb_in_slice );
	fprintf( stdout, "slice_type        = %d\n", slice_type        );
	fprintf( stdout, "pic_parameter_set = %d\n", pic_parameter_set_id );
	fprintf( stdout, "frame_num         = %d(%dbits)\n", 
		frame_num, frame_num_bits );
	}

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
	    if (b_user_data) 
	    {
	    	if( bUserData )
		    fprintf( stdout, 
			"[UserData] picture_coding_type = 0x%08X\n", 
			(UINT)picture_coding_type );
		b_user_data = 0;
	    }
	}

	if( nEditFrameMbs>=0 )
	{
	    if( avc_frame_mbs_only_flag==0 )
	    {
		if( nEditFrameMbs==1 )	// 0->1
		{	// delete field_pic_flag
		    SetSpecial( SET_START );
		    bEdit = EDIT_CUT;
		}
	    } else {
		if( nEditFrameMbs==0 )	// 1->0
		{	// add field_pic_flag
		    SetSpecial( SET_START_END );
		    bEdit = EDIT_INS;
		}
	    }
	}
	if( avc_frame_mbs_only_flag==0 )
	{
	    field_pic_flag = GetBitStream( fp, 1 );
	    if( bShowDetail )
	    fprintf( stdout, "field_pic_flag = %d\n", field_pic_flag );
	    if( field_pic_flag )
	    {
		int bottom_field_flag = GetBitStream( fp, 1 );
		if( bShowDetail )
		    fprintf( stdout, "bottom_field_flag = %d\n", 
			bottom_field_flag );
	    }
	} 
	if( nEditFrameMbs>=0 )
	{
	    if( avc_frame_mbs_only_flag==0 )
	    {
		if( nEditFrameMbs==1 )	// 0->1
		{	// delete field_pic_flag
		    SetSpecial( SET_END );
		}
	    } else {
		if( nEditFrameMbs==0 )	// 1->0
		{	// add field_pic_flag
		}
	    }
	}

	if( bEdit )
	{
	    int SpecialTermAddr=GetBitAddr();
	    int SpecialTermBits=GetBitOffset();
	    fprintf( stdout, "Special%d (%X,%d) (%X,%d) (%X,%d)\n",
	    	bEditSPS,
	    	SpecialStartAddr, SpecialStartBits,
	    	SpecialEndAddr, SpecialEndBits,
	    	SpecialTermAddr, SpecialTermBits );
	    g_update_addrS[g_update_countS] = GetBitAddr();
	    g_update_bit_S[g_update_countS] = SpecialStartBits;
	    g_update_bit_E[g_update_countS] = SpecialEndBits;
	    g_update_bit_T[g_update_countS] = SpecialTermBits;
	    g_update_mode [g_update_countS] = bEdit;
	    g_update_countS++;
	}

	int pic_order_cnt_lsb   = (-1);
	int delta_pic_order_cnt = (-1);
	if( bParsePOC )
	{
	    if( avc_IdrPicFlag )
	    {
		int idr_pic_id = GetBitStreamGolomb( fp );
		if( bShowDetail )
		fprintf( stdout, "idr_pic_id = %d\n", idr_pic_id );
	    }
	    if( avc_pic_order_cnt_type==0 )
	    {
		int pic_order_cnt_lsb_bits=
			avc_log2_max_pic_order_cnt_lsb_minus4+4;
		pic_order_cnt_lsb = GetBitStream( fp, pic_order_cnt_lsb_bits );
		if( bShowDetail )
		{
		    fprintf( stdout, "pic_order_cnt_lsb=%d(%dbits)\n", 
			    pic_order_cnt_lsb, pic_order_cnt_lsb_bits );
		}
		if( avc_bottom_field_pic_order_in_frame_present_flag 
		 && (field_pic_flag==0) )
		{
		    int delta_pic_order_cnt_bottom = GetBitStreamGolomb( fp );
		    if( bShowDetail )
		    {
			fprintf( stdout, "delta_pic_order_cnt_bottom = %d\n", 
			    delta_pic_order_cnt_bottom );
		    }
		}
	    } else if( (avc_pic_order_cnt_type==1)
		    && (avc_delta_pic_order_always_zero_flag==0))
	    {
		delta_pic_order_cnt = GetBitStreamGolomb( fp );
		if( bShowDetail )
		{
		    fprintf( stdout, "delta_pic_order_cnt[0]=%d\n", 
			    delta_pic_order_cnt );
		}
		if( avc_bottom_field_pic_order_in_frame_present_flag 
		 && (field_pic_flag==0) )
		{
		    delta_pic_order_cnt = GetBitStreamGolomb( fp );
		    if( bShowDetail )
		    {
			fprintf( stdout, "delta_pic_order_cnt[1]=%d\n", 
				delta_pic_order_cnt );
		    }
		}
	    }
	}
	if( bParsePOC )
	if( bParseSlice )
	{
	    fprintf( stdout, "picture_coding_type=%d\n",
		    picture_coding_type );
	    if( avc_redundant_pic_cnt_present_flag )
	    {
		int redundant_pic_cnt = GetBitStreamGolomb( fp );
		if( bShowDetail )
		{
		    fprintf( stdout, "redundant_pic_cnt=%d\n",
			    redundant_pic_cnt );
		}
	    }
	    if( picture_coding_type==PCT_B )
	    {
		int direct_spatial_mv_pred_flag = GetBitStream( fp, 1 );
		if( bShowDetail )
		{
		    fprintf( stdout, "direct_spatial_mv_pred_flag=%d\n",
			    direct_spatial_mv_pred_flag );
		}
	    }
	    if( (picture_coding_type==PCT_P)
	     || (picture_coding_type==PCT_B) )
	    {
		int num_ref_idx_active_override_flag = GetBitStream( fp, 1 );
		if( num_ref_idx_active_override_flag )
		{
		    int num_ref_idx_active_minus1 = GetBitStreamGolomb( fp );
		    if( bShowDetail )
		    {
			fprintf( stdout, "num_ref_idx_active_minus1=%d\n",
			    num_ref_idx_active_minus1 );
		    }
		    if( picture_coding_type==PCT_B )
		    {
			int num_ref_idx_ll_active_minus1 = 
				GetBitStreamGolomb( fp );
			if( bShowDetail )
			{
			    fprintf( stdout, 
			    	"num_ref_idx_ll_active_minus1=%d\n",
				num_ref_idx_ll_active_minus1 );
			}
		    }
		}
	    }
	    if( nal_unit_type==20 )
	    {
    //	    ref_pic_list_mvc_modification();
	    } else {
    //	    ref_pic_list_modification();
    fprintf( stdout, "ref_pic_list_modification()\n" );
    fprintf( stdout, "slice_type=%d\n", slice_type );
		if( ((slice_type%5)!=2) && ((slice_type%5)!=4) )
		{	// !PCT_I (PCT_P or PCT_B)
		    int ref_pic_list_modification_flag_l0 = 
		    	GetBitStream( fp, 1 );
		    if( bShowDetail )
		    {
			fprintf( stdout, 
			    "ref_pic_list_modification_flag_l0=%d\n",
			    ref_pic_list_modification_flag_l0 );
		    }
		    if( ref_pic_list_modification_flag_l0 )
		    {
		    	int nCount=0;
			int modification_of_pic_nums_idc = 0;
			while( modification_of_pic_nums_idc !=3 )
			{
			    modification_of_pic_nums_idc = 
				    GetBitStreamGolomb( fp );
			    if( bShowDetail )
			    {
				fprintf( stdout, 
				    "%d : modification_of_pic_nums_idc=%d\n",
				    nCount++, modification_of_pic_nums_idc );
			    }
			    if( (modification_of_pic_nums_idc==0)
			     || (modification_of_pic_nums_idc==1) )
			    {
				int abs_diff_pic_num_minus1 = 
				    GetBitStreamGolomb( fp );
				if( bShowDetail )
				{
				    fprintf( stdout, 
					"abs_diff_pic_num_minus1=%d\n",
					    abs_diff_pic_num_minus1 );
				}
			    } else if( modification_of_pic_nums_idc==2 )
			    {
				int long_term_pic_num = 
					GetBitStreamGolomb( fp );
				if( bShowDetail )
				{
				    fprintf( stdout, "long_term_pic_num=%d\n",
					    long_term_pic_num );
				}
			    }
			}
		    }
		}
		if( (slice_type%5)==1 )
		{	// PCT_B
		    int ref_pic_list_modification_flag_l1 = 
		    	GetBitStream( fp, 1 );
		    if( bShowDetail )
		    {
			fprintf( stdout, 
			    "ref_pic_list_modification_flag_l1=%d\n",
			    ref_pic_list_modification_flag_l1 );
		    }
		    if( ref_pic_list_modification_flag_l1 )
		    {
			int modification_of_pic_nums_idc = 0;
		    	int nCount=0;
			while( modification_of_pic_nums_idc !=3 )
			{
			    modification_of_pic_nums_idc = 
				    GetBitStreamGolomb( fp );
			    if( bShowDetail )
			    {
				fprintf( stdout, 
				    "%d : modification_of_pic_nums_idc=%d\n",
				    nCount, modification_of_pic_nums_idc );
			    }
			    if( (modification_of_pic_nums_idc==0)
			     || (modification_of_pic_nums_idc==1) )
			    {
				int abs_diff_pic_num_minus1 = 
				    GetBitStreamGolomb( fp );
				if( bShowDetail )
				{
				    fprintf( stdout, 
				    	"abs_diff_pic_num_minus1=%d\n",
					    abs_diff_pic_num_minus1 );
				}
			    } else if( modification_of_pic_nums_idc==2 )
			    {
				int long_term_pic_num = GetBitStreamGolomb( fp );
				if( bShowDetail )
				{
				    fprintf( stdout, "long_term_pic_num=%d\n",
					    long_term_pic_num );
				}
			    }
			}
		    }
		}
	    }
	}

/*
int mvc_non_idr_flag     = (-1);
int mvc_priority_id      = (-1);
int mvc_view_id          = (-1);
int mvc_temporal_id      = (-1);
int mvc_anchor_pic_flag  = (-1);
int mvc_inter_view_flag  = (-1);
*/
	if( mvc_extension )
	{
	    fprintf( stdout, 
	    	"MVC:SliceHeader(type=%d,pct=%d,anchor=%d,view=%d)\n", 
		slice_type, picture_coding_type, 
		mvc_anchor_pic_flag, mvc_view_id );
	    mvc_anchor_pic_flag = (-1);
	    mvc_extension = 0;
	} else {
	    if( pic_order_cnt_lsb>=0 )
	    {
		fprintf( stdout, 
		    "SliceHeader(type=%d,pct=%d,poc_lsb=%3d,%dbits)\n", 
		    slice_type, picture_coding_type, 
		    pic_order_cnt_lsb,
		    avc_log2_max_pic_order_cnt_lsb_minus4+4 );
	    } else if( delta_pic_order_cnt>=0 )
	    {
		fprintf( stdout, 
		    "SliceHeader(type=%d,pct=%d,deltapoc=%3d,%dbits)\n", 
		    slice_type, picture_coding_type, 
		    delta_pic_order_cnt,
		    avc_log2_max_pic_order_cnt_lsb_minus4+4 );
	    }
	}

	bDebugGolB = tmp;
	return 0;
}

int Counts[256];

int CountNAL( int addrS, int bitsS, int addrE, int bitsE, int code )
{
	addrS -= 4;
	if( bShowNALinfo )
	fprintf( stdout, "CountNAL(0x%X,%d-0x%X,%d, Code(%d))\n", 
		addrS, bitsS, addrE, bitsE, code );
	int addr_diff = addrE-addrS;
	int bits_diff = bitsE-bitsS;
//	int size = (7+addr_diff+bits_diff)/8;
	int size = addr_diff+(7+bits_diff)/8;	// 2012.9.28
	if( bShowNALinfo )
	fprintf( stdout, "Code(%02X) : %X:%d(%X:%d) - %X:%d(%X:%d) (0x%X)\n", 
		code, 
		addrS, bitsS, (addrS+bitsS/8), bitsS%8,
		addrE, bitsE, (addrE+bitsE/8), bitsE%8,
		size );
	Counts[code] += size;
	return size;
}

int ShowSliceSize( UINT Ug_addr, int es_addr, UINT PTSH, UINT PTSL )
{
	if( PTSH==0xFFFFFFFF )
	{
	    fprintf( stdout, 
		"Slice size=%6lld (%8X,%8llX) PTS=-:--------\n", 
		g_addr-es_addr-4-4, es_addr+4, g_addr-4 );
	} else {
	    int hour, min, sec, msec;
	    TcTime32( PTSH, PTSL, &hour, &min, &sec, &msec );
	    fprintf( stdout, 
	    "Slice size=%6lld (%8X,%8llX) PTS=%1X:%08X (%2d:%02d:%02d.%03d)\n", 
		g_addr-es_addr-4-4, es_addr+4, g_addr-4,
		PTSH, PTSL, hour, min, sec, msec );
	}
	return 0;
}

// ------------------------------------------------------------

int AnalyzeAVC( FILE *fp, FILE *pts_fp )
{
int eof=0;
unsigned char buffer[BUF_SIZE];
unsigned int ID;
int readed=0;
int readP=0, size;
int i, ret=0;
int addrS=(-1);
int addrE=(-1);
int bitsS=(-1);
int bitsE=(-1);
int bFirst=1;
unsigned int PTSH=0xFFFFFFFF, PTSL=0xFFFFFFFF, DTSH=0xFFFFFFFF, DTSL=0xFFFFFFFF;

	if( Packets==NULL )
	{
	    fprintf( stdout, "Packets=NULL\n" );
	    return -1;
	}
	for( i=0; i<256; i++ )
	{
	    Counts[i] = 0;
	}
	while( eof==0 )
	{
	    if( readP<4 )
	    {
	    	size = 4-readP;
//		fprintf( stdout, "gread@%d, %d\n", readP, size );
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
	    	if( bFirst )
		{
		    if( skipContent( fp, buffer, 0 )==1 )
		    {
			readP=4;
		    } else {
		    	eof=1;
		    }
		} else {
		    fprintf( stdout, 
		"AnalyzeAVC : Invalid Prefix %02X %02X %02X : @(%s)\n",
			buffer[0], buffer[1], buffer[2], AddrStr(g_addr-4) );
		    if( (buffer[1]==0) && (buffer[2]==0) )
		    {
			memcpy( &buffer[0], &buffer[1], 3 );
			readed = gread( &buffer[3], 1, 1, fp );
			continue;
		    }
		    if( bSkipError )
		    {
		    	fprintf( stdout, "SkipError\n" );
		    	// Skip to next start code
			if( skipContent( fp, buffer, 0 )==1 )
			{
			    readP=4;
			} else {
			    eof=1;
			}
		    } else {
			EXIT();
		    }
		}
	    } else {	// buffer[0:2]=00 00 01
		bFirst=0;
		ID = buffer[3];
		int forbidden_zero_bit = (ID>>7) &    1;
		int nal_ref_idc        = (ID>>5) &    3;
		int nal_unit_type      = (ID>>0) & 0x1F;
		readP=0;
		{
//		    if( bDump )
			fprintf( stdout, "%02X %02X %02X %02X : ",
			    buffer[0], buffer[1], buffer[2], buffer[3] );
		    Packets[ID]++;
		    InitBitStream( );
		    addrS = GetBitAddr();
		    bitsS = GetBitOffset();
		    // ID : nal_unit_typ = 0..31
		    if( (ID>=0xE0) && (ID<=0xEF) )	// Video
		    {
			// use gread() only
			unsigned int pes_addr = g_addr-4;
			if( bShowDetail )
			fprintf( stdout, 
	"---------------------------------------------------------\n" );
			PES_header( fp, buffer, bDisplayTS,
				&PTSH, &PTSL, &DTSH, &DTSL );
			if( PTSH==0xFFFFFFFF )
			{
			    fprintf( stdout, "ID=%02X : PES_header(%s)\n", 
				ID, AddrStr2(pes_addr) );
			} else {
			    int hour, min, sec, msec;
			    TcTime32( PTSH, PTSL, &hour, &min, &sec, &msec );
			    if( DTSH!=0xFFFFFFFF )
			    {
			    int hour2, min2, sec2, msec2;
			    TcTime32( DTSH, DTSL, &hour2, &min2, &sec2, &msec2);
			    fprintf( stdout, 
    "ID=%02X : PES_header(%s) PTS=%2d:%02d:%02d.%03d,DTS=%2d:%02d:%02d,%03d\n", 
				ID, AddrStr2(pes_addr), 
				hour, min, sec, msec,
				hour2, min2, sec2, msec2 );
			    } else {
			    fprintf( stdout, 
			"ID=%02X : PES_header(%s) PTS=%2d:%02d:%02d.%03d\n", 
				ID, AddrStr2(pes_addr), hour, min, sec, msec );
			    }
			}
			fprintf( pts_fp, "%8X %8llX : %4X %08X, %4X %08X\n",
				pes_addr, SrcAddr(pes_addr,0),
				(UINT)(PTSH & 0xFFFF), PTSL, 
				(UINT)(DTSH & 0xFFFF), DTSL );
			//
#if 0
			BitAddr = g_addr;
#else
			InitBitStream( );
#endif
			bitsE = 0;
			addrE = GetBitAddr();
			bitsE = GetBitOffset();
			CountNAL( addrS, bitsS, addrE, bitsE, 0xE0 );
			es_addr = g_addr;	// 2012.9.3
		    } else if( forbidden_zero_bit==0 )
		    {
			switch( nal_unit_type )
			{
		    // 1 :
			case 0x01 :
			    fprintf( stdout, 
				"ID=%02X : Slice (non-IDR picture) (%s)\n", 
				ID, AddrStr2(g_addr) );
			    fflush( stdout );
			    Slice_header( fp, buffer, 1, nal_ref_idc );
	#if 1
			    if( skipContent( fp, buffer, 0 )==1 )
			    {
				readP=4;
			    } else {
				eof=1;
			    }
			    addrE = g_addr;
			    bitsE = 0;
	#else
			    addrE = GetBitAddr();
			    bitsE = GetBitOffset();
	#endif
			    CountNAL( addrS, bitsS, addrE, bitsE, 0x1 );
			    ShowSliceSize( g_addr, es_addr, PTSH, PTSL );
			    break;
		    // 5 : IDR picutre slice
			case 0x05 :	// 2012.2.9
			    fprintf( stdout, 
				"ID=%02X : Slice (IDR picture) (%s)\n", 
				ID, AddrStr2(g_addr) );
			    Slice_header( fp, buffer, 5, nal_ref_idc );

			    if( skipContent( fp, buffer, 0 )==1 )
			    {
				readP=4;
			    } else {
				eof=1;
			    }
			    addrE = g_addr;
			    bitsE = 0;
			    CountNAL( addrS, bitsS, addrE, bitsE, 0x5 );
			    ShowSliceSize( g_addr, es_addr, PTSH, PTSL );
			    break;
		    // 6 : SEI
			case 6 :
			    fprintf( stdout, "ID=%02X : SEI(%s)\n", 
				    ID, AddrStr2(g_addr) );
			    ret = SupplementalEnhancementInformation( fp, 
			    	buffer );
			    if( ret>0 )
				readP = ret;
//			    fprintf( stdout, "ret=%d, readP=%d\n", ret, readP );
			    addrE = GetBitAddr();
			    bitsE = GetBitOffset();
			    CountNAL( addrS, bitsS, addrE, bitsE, 0x06 );
			    break;
		    // 7 : SPS
			case 0x07 :	// 2012.2.9
			case 0x27 :	// 2010.12.9
			case 0x67 :
			    fprintf( stdout, 
				"ID=%02X : Sequence parameter set(%s) (%s)\n", 
				ID, AddrStr(g_addr), DtsTimeStr() );
			    ret = SequenceParameterSet( fp, buffer );
			    if( ret==1 )
				readP=4;
			    else if( ret<0 )
				eof=1;
			    addrE = GetBitAddr();
			    bitsE = GetBitOffset();
			    CountNAL( addrS, bitsS, addrE, bitsE, 0x7 );
			    break;
		    // 8 : PPS
			case 0x08 :	// 2012.2.9
			    fprintf( stdout, 
				"ID=%02X : Picture parameter set:PPS(%s)\n", 
				ID, AddrStr(g_addr) );
	#if 0
			    if( skipContent( fp, buffer, 0 )==1 )
			    {
				readP=4;
			    } else {
				eof=1;
			    }
			    addrE = g_addr;
			    bitsE = 0;
	#else
			    ret = PictureParameterSet( fp, buffer );
			    if( ret==1 )
				readP=4;
			    else if( ret<0 )
				eof=1;
			    addrE = GetBitAddr();
			    bitsE = GetBitOffset();
	#endif
			    CountNAL( addrS, bitsS, addrE, bitsE, 0x8 );
			    break;
		    // 9 : AU delimiter
			case 9 :
			    fprintf( stdout, "ID=%02X : AUD(%s)\n", 
					ID, AddrStr2(g_addr) );
			    ret = AccessUnitDelimiter( fp, buffer );
			    if( ret<0 )
				eof=1;
			    else if( ret>0 )
				readP = ret;
			    addrE = GetBitAddr();
			    bitsE = GetBitOffset();
			    CountNAL( addrS, bitsS, addrE, bitsE, 0x09 );
			    break;
		    // 10 : End of Sequence
			case 10 :
			    fprintf( stdout, 
				"ID=%02X : End of sequence (%s)\n", 
				ID, AddrStr(g_addr) );
	#if 1
			    if( skipContent( fp, buffer, 0 )==1 )
			    {
				readP=4;
			    } else {
				eof=1;
			    }
			    addrE = g_addr;
			    bitsE = 0;
	#else
			    addrE = GetBitAddr();
			    bitsE = GetBitOffset();
	#endif
			    size = CountNAL( addrS, bitsS, addrE, bitsE, 0xA );
			    break;
		    // 11 : End of stream
			case 11 :	// 2012/5/7 for intel encoder
			    fprintf( stdout, 
				"ID=%02X : End of stream (%s)\n", 
				ID, AddrStr(g_addr) );
	#if 1
			    if( skipContent( fp, buffer, 0 )==1 )
			    {
				readP=4;
			    } else {
				eof=1;
			    }
			    addrE = g_addr;
			    bitsE = 0;
	#else
			    addrE = GetBitAddr();
			    bitsE = GetBitOffset();
	#endif
			    size = CountNAL( addrS, bitsS, addrE, bitsE, 0xA );
			    break;
		    // 12 : Filler data
			case 0x0C :
			    fprintf( stdout, "ID=%02X : FillerData(%s)\n", 
				ID, AddrStr2(g_addr) );
			    ret = FillerData( fp, buffer );
			    if( ret<0 )
				eof=1;
			    else if( ret>0 )
				readP = ret;
			    addrE = GetBitAddr();
			    bitsE = GetBitOffset();
			    size = CountNAL( addrS, bitsS, addrE, bitsE, 0xC );
			    if( bShowDetail )
			    fprintf( stdout, "Filler size=%d\n", size );
			    break;
		    // 14 : Prefix NAL unit
			case 0x0E :
			    fprintf( stdout, "ID=%02X : Prefix NAL unit(%s)\n", 
				ID, AddrStr(g_addr) );
			    if( skipContent( fp, buffer, 0 )==1 )
			    {
				readP=4;
			    } else {
				eof=1;
			    }
			    addrE = g_addr;
			    bitsE = 0;
			    break;
		    // 15
			case 0x0F :	// Subset sequence parameter set
			    fprintf( stdout, 
			"ID=%02X : Subset sequence parameter set (%s)\n", 
				ID, AddrStr(g_addr) );
			    ret = SequenceParameterSet( fp, buffer );
			    if( ret<0 )
				eof=1;
			    if( skipContent( fp, buffer, 0 )==1 )
			    {
				readP=4;
			    } else {
				eof=1;
			    }
			    addrE = g_addr;
			    bitsE = 0;
			    break;
		    // 20 : Coded slice extension
			case 0x14 :
			    fprintf( stdout, 
				"ID=%02X : Coded slice extension (%s)\n", 
				ID, AddrStr(g_addr) );
			    {
				int svc_extension_flag = GetBitStream( fp, 1 );
				if( bShowMvcExtention )
				fprintf( stdout, "svc_extension_flag=%d\n",
					svc_extension_flag );
				if( svc_extension_flag )
				{
				    nal_unit_header_svc_extension( fp, buffer );
				} else {
				    nal_unit_header_mvc_extension( fp, buffer );
				}
			    }
			    Slice_header( fp, buffer, 20, nal_ref_idc );

			    if( skipContent( fp, buffer, 0 )==1 )
			    {
				readP=4;
			    } else {
				eof=1;
			    }
			    addrE = g_addr;
			    bitsE = 0;
			    {
			    int bShow = bShowNALinfo;
    //			bShowNALinfo = 1;
			    size = CountNAL( addrS, bitsS, addrE, bitsE, 0xA );
			    bShowNALinfo = bShow;
			    }
			    ShowSliceSize( g_addr, es_addr, PTSH, PTSL );
			    break;
		    // 24
			case 0x18 :
			    fprintf( stdout, 
				"ID=%02X : VDRD NAL unit (%s)\n", 
				ID, AddrStr(g_addr) );
			    if( skipContent( fp, buffer, 0 )==1 )
			    {
				readP=4;
			    } else {
				eof=1;
			    }
			    addrE = g_addr;
			    bitsE = 0;
			    break;
			default :
			    fprintf( stdout, 
			    "ID=%02X : Not implemented in AnaylzeAVC (%s)\n", 
				ID, AddrStr(g_addr) );
			    if( bSkipError==0 )
			    {
				EXIT();
			    } else {
				fprintf( stdout, "=========================\n");
				fprintf( stdout, "SkipError\n" );
				if( skipContent( fp, buffer, 0 )==1 )
				{
				    readP=4;
				} else {
				    eof=1;
				}
			    }
			    break;
			}
		    } else {
		    	fprintf( stdout, "forbidden_zero_bit!=0\n" );
			    if( bSkipError==0 )
			    {
				EXIT();
			    } else {
				fprintf( stdout, "=========================\n");
				fprintf( stdout, "SkipError\n" );
				if( skipContent( fp, buffer, 0 )==1 )
				{
				    readP=4;
				} else {
				    eof=1;
				}
			    }
		    }
		}
	    }
	}
	fprintf( stdout, "eof=%d\n", eof );
	ShowUpdate( );
	for( i=0; i<256; i++ )
	{
	    if( Counts[i]>0 )
		fprintf( stdout, "Code[%02X] = 0x%X\n", i, Counts[i] );
	}
	return 0;
}

