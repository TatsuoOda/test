
/*
	tsParse.cpp
		2012.9.24 : separate from ts.cpp
	ts.cpp
		2013.7.31 : return to ts.cpp

	ts.cpp
			2010.10.XX  by T.Oda

	2011.4.8	Add '-a' option : bAll      : Save from begining
	2011.4.8	Add '-s' option : bShowAddr : show address
	2011.9.2	Add '+P' option : bPrintPCR : show PCR & address
	2011.9.5		Output pcr.txt file
	2011.9.13	'-W' option : Update PCR -WXX (PCR offset)
			'-c' option : bCutEnd
	2011.9.21	Output TTS.txt (TsPacketSize=192)
	2011.9.27	PCR 33bit
	2011.10.28	Debug : Last packet not broken
	2011.10.29	Debug : No PCR : make adapt_len = (-1) : NG
			Use TsPacketSize not WriteSize
	2011.10.31	Change PES-XXXX.txt output format
			TS-addr, TS-addr2, PES-addr, PES-size, PCR
	2011.11.23	TTS PES-XXXX.bin not correct
	2012. 4.23	PES-XXXX.txt : add TTS timestamp
	2012. 5.13	'+D' option : Output in directory mode
	2012. 5.14 	'-G' PcrGain : use with -W(PCR_UPDATE)
	2012. 6. 4	ParsePts : Video E1-EF, Audio C1-DF support
	2012. 6.14	*.pts : address fix
	2012. 7. 6	TS2TTS
	2012. 7. 9	PCR ins : fixed
	2012. 7.10	g_addr over check
			-g : PcrMbps : fixed bitrate
	2012. 8. 9	Bug fix : SrcAddr check bRecord
	2012. 8.23	+a : show adaptation field
			+m : mask discontinuity indicator
	2012. 9.10	use bGetPts : active when -r
	2012. 9.11	+p : bGetPTS=1
	2012.10. 2	Audio detect MaxPackets
	2012.10. 9	PES-XXXX.txt : address:32bit->40bit
			use PCRH[PID]
	2012.10.25	TTS Analyze : PCR.txt 
	2012.10.29	PcrMbps->PcrKbps
	2012.11.12	No sync : treat as eof
	2013. 7.31	use main.h
	2013. 8. 2	check bPayload to output PES-XXXX.bin
	2013.12.10	Show Continuity Counter error
*/

//#define ODA

#include <stdio.h>
#include <stdlib.h>	// exit
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>	// gettimeofday

#include "tsLib.h"
#include "parseLib.h"
#include "lib.h"

#include "main.h"

/*
	options
		-D : MODE_DUMP  :Show packet info
		-d : MODE_DUMP2 :Show packet info and dump packet
		-r : MODE_DUMP3 :bRecord=1 : make PES-XXXX.bin
*/
void tsUsage( char *prg )
{
	fprintf( stdout, "%s ts-filename [output-filename]\n", prg );
	fprintf( stdout, "\t[-D : Show packet info]\n" );
	fprintf( stdout, "\t[-d : Show packet info and dump packet]\n" );
	fprintf( stdout, "\t[-r : make PES-XXXX.bin]\n" );
	fprintf( stdout, "\t[-O -pXX : Only selected pid remain]\n" );
	fprintf( stdout, "\t[-w -pXX : Analyze PCR]\n" );
	fprintf( stdout, "\t[-W#offset -pXX -g#MHz : Update PCR]\n" );
	fprintf( stdout, "\t[-C -SXX -EYY : Cut]\n" );
	exit( 1 );
}

#define TS_PACKET_SIZE 188

#define MODE_ADDR		0
#define MODE_DUMP		1
#define MODE_DUMP2		2
#define MODE_DUMP3		3
#define MODE_PID		4
#define MODE_PAT		5
#define MODE_PMT		6
#define MODE_CUT		9
#define MODE_OFF		10
#define MODE_PCR_ANALYZE	11
#define MODE_PCR_UPDATE		12
#define MODE_CUT_END		13
#define MODE_TS2TTS		14

#define MODE_TEST		19

#define MAX_PIDS 64

//static int bDebug = 0;
int bGetPTS = 0;
int TsPacketSize = TS_PACKET_SIZE;
int bRemoveOtherPacket = 0;
int bCheckContinuous = 0;
int bWriteMsg=0;
int bAll=0;
int bShowAddr=0;
int bPrintPCR=0;
int bCutEnd=0;
int bDumpAdapt=0;
int bMaskDiscon=0;

int bAllegroMVC=0;
// ------------------------------
#define MAX_PCRS 1024*1024

double d_keisuL = 0;
unsigned long long PCR0=0xFFFFFFFF, PCR1=0;
unsigned int addr0=0xFFFFFFFF, addr1=0;
int PcrOffset=0;
int PcrGain=200;
int PcrKbps=0;
unsigned int EndPcr=0;
// ------------------------------

int ParsePts( FILE *bin_fp, FILE *pts_fp )
{
unsigned char bytes [1024];
unsigned char buffer[1024];
int ReadOK;
unsigned int PTSH, PTSL, DTSH, DTSL;
int nPTS=0;
unsigned int pes_addr=0xFFFFFFFF;
#define MAX_DATA_SIZE   1024*64
	unsigned char *dataBuf = (unsigned char *)malloc( MAX_DATA_SIZE );
	if( dataBuf==NULL )
	{
	    fprintf( stdout, "Can't alloc dataBuf\n" );
	    return -1;
	}
	g_addr=0;
	int state=0;
	while( 1 )
	{
	    ReadOK = gread( &bytes[state], 1, 1, bin_fp );
	    if( ReadOK<1 )
	    {
	    	fprintf( stdout, "EOF\n" );
		break;
	    }
	    switch( state )
	    {
	    case 0 :
	    	if( bytes[0]==0x00 )	// 00
		    state++;
		break;
	    case 1 :
	    	if( bytes[1]==0x00 )	// 00 00
		    state++;
		else
		    state=0;
		break;
	    case 2 :
	    	if( bytes[2]==0x01 )	// 00 00 01
		    state++;
		else if( bytes[2]==0x00 )	// 00 00 00
		{
		} else
		    state=0;
		break;
	    case 3 :
	    	if( ((bytes[3]>=0xE0) && (bytes[3]<=0xEF)) 	// Video
		 || ((bytes[3]>=0xC0) && (bytes[3]<=0xDF))	// Audio
		 || ((bytes[3]==0xBD)                    )	// Audio
		) {
		    int a0 = g_addr;
		    pes_addr = g_addr-4;
#if 0		    
fprintf( stdout, "PES_header(%llX:00 00 01 %02X)\n", g_addr, bytes[3] );
#endif
		    int pes_len = PES_header( bin_fp, buffer, 0, 
		    	&PTSH, &PTSL, &DTSH, &DTSL );
		    int hour, min, sec, msec;
		    TcTime32( PTSH, PTSL, &hour, &min, &sec, &msec );

		    if( (PTSH & 0xFFFF)!=0xFFFF )
		    fprintf( pts_fp, 
		    "%8X %10llX : %4X %08X, %4X %08X (%3d:%02d:%02d.%03d)\n",
			pes_addr, SrcAddr(pes_addr,0),
			PTSH & 0xFFFF, PTSL, DTSH & 0xFFFF, DTSL,
			hour, min, sec, msec );
		    nPTS++;

		    int a1 = g_addr;
		    int readSize = pes_len-(a1-a0)+2;
		    if( readSize>=MAX_DATA_SIZE )
		    {
		    	fprintf( stdout, "Too large data(0x%X)\n", readSize );
			exit( 1 );
		    }
		    if( readSize>0 )
		    {
//			fprintf( pts_fp, "skip(%d bytes)\n", readSize );
    #if 1
			int readed = gread( dataBuf, 1, readSize, bin_fp );
			if( readed<readSize )
			    fprintf( stdout, "readed=%d/%d\n", 
			    	readed, readSize );
    #else
			fseek( bin_fp, readSize, SEEK_CUR );
    #endif
		    }
		}
		state = 0;
		break;
	    }
	}
	return nPTS;
}

#define MAX_PID	8192

int TsDump( char filename[], char editfile[], char directory[],
	int pid[], int nPid, int mode,
	int PCR_PID[MAX_PCRS],
	unsigned int PCR_addr[MAX_PCRS],
	long long PCR_value[MAX_PCRS]
	)
{
int nPCR=0;
FILE *ts_fp=NULL;
unsigned char bytes[1024*4];
int readed;
int PID=(-1);
int PCR_pid=(-1);
unsigned int   PCR_H=0xFFFFFFFF;
unsigned int   PCR_L=0xFFFFFFFF;
unsigned int L_PCR_H=0xFFFFFFFF;
unsigned int L_PCR_L=0xFFFFFFFF;
//int i;
int ii;
struct stat Stat;
FILE *edit=NULL;
FILE *pcr_fp = NULL;
FILE *tts_fp = NULL;
FILE      *out_fp [MAX_PID];
FILE      *addr_fp[MAX_PID];
int      bPayload [MAX_PID];
int      nPayload [MAX_PID];
int      nCount   [MAX_PID];
//unsigned int nAddr[MAX_PID];
long long nAddr[MAX_PID];
unsigned int nLast[MAX_PID];
int 	    Lcount[MAX_PID];
int	   NGcount[MAX_PID];
unsigned int PCRH[MAX_PID];
unsigned int PCRL[MAX_PID];
unsigned int L_tts_ts     = 0xFFFFFFFF;
unsigned int L_tts_ts_PCR = 0xFFFFFFFF;
unsigned int tts_ts       = 0xFFFFFFFF;
unsigned int tts_ts_min   = 0xFFFFFFFF;
unsigned int tts_ts_max   = 0;

static char outfile[] = "PES.bin";
int bRecord=0;
int bPrint=1;
// Error
int nSyncError=0;
unsigned int L_PCR_addr = 0xFFFFFFFF;
int PCR_interval_Error=0;
int total_written=0;
unsigned int nLastAll=0;
struct timeval timeval0, timeval1;
#define MAX_PACKETS 1024*1024*64
int MaxPackets = MAX_PACKETS;
//int TsPID[MAX_PACKETS];
int *TsPID = NULL;

	while( MaxPackets>1024 )
	{
	    TsPID = (int *)malloc( 4*MaxPackets );
	    if( TsPID!=NULL )
		break;
	    MaxPackets = MaxPackets/2;
	}
	fprintf( stdout, "MaxPackets=%d\n", MaxPackets );

	for( ii=0; ii<MaxPackets; ii++ )
	{
	    TsPID[ii] = -1;
	}
	g_addr = 0;

	fprintf( stdout, "TsDump(%s:%d:%4.4f)\n", filename, mode, d_keisuL );
	char pcrFilename[1024];
	if( directory[0] )
	    sprintf( pcrFilename, "%s/PCR.txt", directory );
	else
	    sprintf( pcrFilename, "PCR.txt" );
	pcr_fp = fopen( pcrFilename, "w" );
	if( pcr_fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", pcrFilename );
	}
	if( TsPacketSize==192 )
	{
	    tts_fp = fopen( "TTS.txt", "w" );
	}
	if( mode==2 )
	if( bAll )
	    bRecord = 1;
	for( ii=0; ii<MAX_PID; ii++ )
	{
	    out_fp  [ii] = NULL;
	    addr_fp [ii] = NULL;
	    bPayload[ii] = 0;
	    nPayload[ii] = 0;
	    nCount  [ii] = 0;
	    nAddr   [ii] = 0;
	    nLast   [ii] = 0;
	    Lcount  [ii] = (-1);
	    NGcount [ii] = 0;
	    PCRH    [ii] = 0xFFFFFFFF;
	    PCRL    [ii] = 0xFFFFFFFF;
	}
	if( stat( filename, &Stat )!=0 )
	{
	    fprintf( stdout, "Can't stat %s\n", filename );
	    exit( 1 );
	}
	int size = (int)Stat.st_size;
	int packets = size/TsPacketSize;
	fprintf( stdout, "size=%d, packets=%d\n", size, packets );
	ts_fp = fopen ( filename, "rb" );
	if( ts_fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}
	if( editfile!=NULL )
	{
	    edit = fopen( editfile, "wb" );
	    if( edit==NULL )
	    {
	    	fprintf( stdout, "Can't write %s\n", editfile );
		exit( 1 );
	    }
	}
	if( mode<0 )
	    bPrint = 0;
	if( mode==2 )
	    bPrint = 0;
	int byteOffset=0;
	if( TsPacketSize==192 )
	{
	    byteOffset=4;
	}
	int bEOF=0;
	int bAppend=0;
	int nEdit=0;
	int AddOne=0;
	int cnt;
	for( cnt=0; ; cnt++ )
	{
	    int n;
	    int SyncOK=0;
	    unsigned long long newPCR     = 0;
	    unsigned long long newPCR_27M = 0;
	    if( (cnt%10000)==0 )
	    {
		if( bDebug )
		    fprintf( stdout, "\r%d ", cnt );
		fflush( stdout );
	    }
	    int ReadOK;
	    if( bEOF==0 )
	    {
		ReadOK = gread( bytes, TsPacketSize, 1, ts_fp );
		if( ReadOK<1 )
		{
		    fprintf( stdout, "EOF@%d(addr=0x%llX,%lld)\n", 
			    cnt, g_addr, g_addr );
		    bEOF=1;
		} else {
		    readed = TsPacketSize;
		}
	    }
	    if( bEOF )
	    {
		if( (EndPcr==0) || (mode>=0) || (d_keisuL==0))
		    break;
	    }
	    if( bEOF && (AddOne==0))
	    {	// Add null packet
		memset( bytes, 0, 256 );
		bytes[byteOffset+0] = 0x47;
		bytes[byteOffset+1] = 0x1F;
		bytes[byteOffset+2] = 0xFF;
		bytes[byteOffset+3] = 0x10;
		if( g_addr>=0xFFFF0000 )
		{
		    fprintf( stdout, "g_addr is too large(0x%llX)\n", g_addr );
		    exit( 1 );
		}
		g_addr += TsPacketSize;
		SyncOK=1;
		PID=0x1FFF;
//		fprintf( stdout, "(Add One)" );
		AddOne++;
	    }
	    unsigned int curAddr = g_addr-TsPacketSize;
	    double d_offset=0;
	    if( curAddr>addr0 )
	    {
		d_offset = (curAddr-addr0);
		d_offset = d_offset*d_keisuL;
		newPCR = (unsigned long long)d_offset+PCR0;

		d_offset = (curAddr-addr0);
		d_offset = d_offset*300*d_keisuL;
		newPCR_27M = (unsigned long long)d_offset+PCR0;
	    }
	    int nRetry=0;
	    while( (SyncOK==0) && (bEOF==0) )
	    {
		if( bytes[byteOffset+0]==0x47 )
		{
		    PID = ((bytes[byteOffset+1]<<8) 
		         | (bytes[byteOffset+2]<<0)) & 0x1FFF;
		    SyncOK=1;
		} else {
		    // Sync not found
		    int bSync=0;
		    PID = 0xFFFF;
		    if( nRetry==0 )
		    {
		    	int j;
			nSyncError++;
			fprintf( stdout, "Invalid Sync Byte(%02X)@%llX\n", 
			    bytes[byteOffset+0], g_addr-TsPacketSize );
			fprintf( stdout, "%8llX : ", g_addr-TsPacketSize );
			for( j=0; j<(byteOffset+4); j++ )
			{
			    fprintf( stdout, "%02X ", bytes[j] );
			}
			fprintf( stdout, "\n" );
		    }
		    for( n=byteOffset; n<TsPacketSize; n++ )
		    {
			if( bytes[n]==0x47 )
			{
			    fprintf( stdout, "Sync? @%llX\n", 
			    	g_addr-TsPacketSize+n );
			    memcpy( bytes, &bytes[n], TsPacketSize-n );
			    ReadOK = gread( &bytes[TsPacketSize-n], 
			    	n, 1, ts_fp );
			    if( ReadOK<1 )
			    	bEOF = 1;
			    else
			    	readed = TsPacketSize-n;
			    nRetry++;
			    bSync=1;
			    break;
			}
		    }
		    if( n==TsPacketSize )
		    {
			    ReadOK = gread( &bytes[0], TsPacketSize, 1, ts_fp );
			    if( ReadOK<1 )
			    {
				fprintf( stdout, "EOF@%d(addr=0x%llX,%lld)\n", 
					cnt, g_addr, g_addr );
				break;
			    } else
			    	readed = TsPacketSize;
			    for( n=0; n<TsPacketSize; n++ )
			    {
			    	if( bytes[n]==0x47 )
				{
				    bSync=1;
				    break;
				}
			    }
			    if( bSync==0 )
			    {
			    	fprintf( stdout, "Sync not found@0x%llX\n",
				    g_addr );
			    	bEOF = 1;
				break;
			    }
		    }
		}
		if( byteOffset==0 )
		{
		    int key = getc( ts_fp );
		    ungetc( key, ts_fp );
		    if( (key!=EOF) && (key!=0x47) )
		    {
			SyncOK=0;
			bytes[0] = 0;
		    }
		} else {
		    if( tts_fp )
		    {
		    	UINT PCR_H=0;
		    	UINT PCR_L=0;
			unsigned long long LL=0;
		    	int bAdapt   = bytes[4+3]&0x20;
			int adaptLen = bytes[4+4];
			int bPCR     = bytes[4+5]&0x10;
			if( (bAdapt==0) || (adaptLen==0) )
			    bPCR = 0;
		    	tts_ts = (bytes[0]<<24)
			       | (bytes[1]<<16)
			       | (bytes[2]<< 8)
			       | (bytes[3]<< 0);
		       if( bPCR )
		       {
			    PCR_H = (bytes[4+ 6]<< 8) | (bytes[4+ 7]<< 0);
			    PCR_L = (bytes[4+ 8]<<24) | (bytes[4+ 9]<<16)
				  | (bytes[4+10]<< 8) | (bytes[4+11]<< 0);
			    LL = (unsigned long long)PCR_H;
			    LL = (LL<<32) | PCR_L;
			    LL = LL>>15;
		       }
			fprintf( tts_fp, 
			    "%8llX : %08X : %02X %02X %02X %02X : %02X %02X", 
			    g_addr-TsPacketSize, tts_ts,
			    bytes[4], bytes[5], bytes[6], bytes[7],
			    bytes[8], bytes[9] );
			if( L_tts_ts==0xFFFFFFFF )
			{
			    fprintf( tts_fp, " :        -" );
			} else {
			    int diff = tts_ts-L_tts_ts;
			    if( L_tts_ts>tts_ts )
			    {
			    	diff = 0xFFFFFFFF-L_tts_ts+tts_ts+1;
			    }
			    fprintf( tts_fp, " : %8d", diff );
			    if( diff>tts_ts_max )
				tts_ts_max = diff;
			    if( diff<tts_ts_min )
				tts_ts_min = diff;
			}
			if( bPCR )
			    fprintf( tts_fp, " (PCR:%9llX)", LL );
			fprintf( tts_fp, "\n" );
			L_tts_ts = tts_ts;
		    }
		}
	    }
	    int bDo=0;
	    int bPidMatch=0;
	    if( pid[0]<0 )
		bDo=1;
	    for( ii=0; ii<nPid; ii++ )
	    {
		if( PID==pid[ii] )
		{
		    bDo=1;
		    bPidMatch=1;
		    break;
		}
	    }
	    if( PID==0x1FFF )
	    	bDo=1;
	    unsigned char *byteS = &bytes[byteOffset];
	    if( SyncOK )
	    {
		int nTs = g_addr/TsPacketSize-1;
		if( nTs>=MaxPackets )
		{
		    fprintf( stdout, "Too large packets(%d)\n", nTs );
		    exit( 1 );
		}
		TsPID[nTs] = PID;
	    	if( bShowAddr )
	    	fprintf( stdout, "%5d:%8llX-%8llX\n",
			cnt, g_addr-TsPacketSize, g_addr );
		int ts_error     = (byteS[1]>>7)&1;
		int payloadStart = (byteS[1]>>6)&1;
		int ts_priority  = (byteS[1]>>5)&1;
		int adapt        = (byteS[3]>>4)&3;
		int count        =  byteS[3]    &15;
		int adapt_len    =  byteS[4];
		int bOut=0;
		int skip=0;
		int bAdapt  =0;
#if 1
		if( PID==pid[0] )
		fprintf( stdout, 
		"%5d:%8llX : %4X :  E=%d, P=%d, p=%d, a=%d, c=%2d, l=%3d\n",
		cnt, g_addr-TsPacketSize, PID,
		ts_error, payloadStart, ts_priority,
		adapt, count, adapt_len );
#endif
		if( bDo )
		{
		    switch( adapt )
		    {
		    case 0 :	// ?
//			fprintf( stdout, "adapt=%d\n", adapt );
			break;
		    case 1 :	// no adapt
			bPayload[PID]=1;
			bAdapt  =0;
			break;
		    case 2 :	// adapt
			bPayload[PID]=0;
			if( adapt_len>0 )	// 2011/9/2
			    bAdapt  =1;
			break;
		    case 3 :	// adapt+payload
			bPayload[PID]=1;
//			if( adapt_len>0 )	// 2011/9/2
			    bAdapt  =1;
			break;
		    default :
			fprintf( stdout, "Invalid adapt(%d)\n", adapt );
			exit( 1 );
			break;
		    }
		    if( bDumpAdapt )
		    {
			if( bAdapt )
			{
			    fprintf( stdout, 
				    "PID=%04X : adapt_len=%3d %02X %02X\n", 
				    PID, adapt_len, byteS[5], byteS[6] );
			}
		    }
		    if( bMaskDiscon )
		    {
			if( bAdapt )
			    byteS[5] = byteS[5] & 0x7F;
		    }

		    skip+=4;	// Header 4byte
		    if( PID==0x1FFF )
		    {
    #if 0
			fprintf( stdout, 
				"%5d : %04X : %X,%X : ", i, PID, adapt, count );
			fprintf( stdout, "\n" );
    #endif
    			if( (L_PCR_addr!=0xFFFFFFF) && (d_keisuL>0) )
			{
			    d_offset = curAddr-L_PCR_addr;
			    d_offset = d_offset*d_keisuL;
			    int i_offset = (int)d_offset;
			    // PCR should be every 100ms (9000)
			    // PCR should be every 100ms (1sec=27000000)
			    if( (i_offset>8900) && (PCR_interval_Error==0) )
			    {
				if( mode==(-1) )	// PCR_UPDATE
				{
				    PID=pid[0];
				    bPidMatch = 1;
				    bAdapt  =1;
				    byteS[ 1] =  0x20 | ((PID&0xFF00)>>8);
				    byteS[ 2] =  (PID&0x00FF);
				    byteS[ 3] = 0x2C;
				    byteS[ 4] = 0xB7;
				    byteS[ 5] = 0x10;
				    byteS[ 6] = 0x00;
				    byteS[ 7] = 0x00;
				    byteS[ 8] = 0x00;
				    byteS[ 9] = 0x00;
				    byteS[10] = 0x00;
				    byteS[11] = 0x00;
				    // --------------------------------
				    ts_error     = (byteS[1]>>7)&1;
				    payloadStart = (byteS[1]>>6)&1;
				    ts_priority  = (byteS[1]>>5)&1;
				    adapt = (byteS[3]>>4)&3;
				    count =  byteS[3]    &15;
				    adapt_len = byteS[4];
#if 0
	    fprintf( stdout, " PCR ins : %X->%X : %d :",
	    	L_PCR_addr, curAddr, i_offset );
#else
	    fprintf( stdout, " (PCR ins:%llX) ", g_addr );
#endif
				    nEdit++;
#if 0
				    if( nEdit>100 )
					exit( 1 );
#endif
				} else {
				    fprintf( stdout, 
				    	"PCR interval Error(%d)\n", i_offset );
				    fflush( stdout );
				    PCR_interval_Error=1;
				}
			    } else {
				PCR_interval_Error=0;
			    }
			}
		    }
		    {
		    	int flag  = (-1);
		    	int bDisc = (-1);
			int bEXT  = (-1);
			int bPCR  = (-1);
		    	nCount[PID]++;
			if( bAdapt )
			{
			  if( adapt_len>0 )	// 2012.1.26
			  {
			    if( bPrint )
			    fprintf( stdout, 
			    "%8llX(%5d): %04X : %X,%X : ", 
			    g_addr-TsPacketSize, cnt, PID, adapt, count );
			    bOut++;
			    flag = byteS[5];
			    bDisc = flag & (1<<7);	// Discontinuity indi
//			    int bRand = flag & (1<<6);	// Random Access indi
//			    int bPri  = flag & (1<<5);	// Elementary st pri
			    bPCR  = flag & (1<<4);	// contain a PCR field
//			    int bOPCR = flag & (1<<3);	// contain an OPCR field
//			    int bSPR  = flag & (1<<2);	// Splicing point flag
//			    int bPRV  = flag & (1<<1);	// private data flag
			    bEXT  = flag & (1<<0);	// extension flag
			    if( bPCR )
			    {
				unsigned long long LL;
				PCR_pid = PID;
				PCR_H = (byteS[ 6]<< 8) | (byteS[ 7]<< 0);
				PCR_L = (byteS[ 8]<<24) | (byteS[ 9]<<16)
				      | (byteS[10]<< 8) | (byteS[11]<< 0);
				LL = (unsigned long long)PCR_H;
				LL = (LL<<32) | PCR_L;
				PCRH[PID] = PCR_H;
				PCRL[PID] = PCR_L;

				if( bPrint || bPrintPCR )
				{
				fprintf( stdout, "%02X %02X %02X %02X : ",
				    byteS[0], byteS[1], byteS[2], byteS[3]);
				fprintf( stdout, 
				    "%02X (%02X) PCR=%04X,%08X(%8X):", 
				adapt_len, flag, PCR_H, PCR_L, (int)(LL>>15) );
				if( bPrint==0 )
				    fprintf( stdout, "%8llX\n", 
					    g_addr-TsPacketSize );
				}
				if( pcr_fp )
				{
				    fprintf( pcr_fp, 
					"%8llX : %04X : %08X,%08X %09llX",
					g_addr-TsPacketSize,
					PID,
					PCR_H, PCR_L, LL>>15 );
				    if( tts_fp )
				    {
					fprintf( pcr_fp, ",%8X", tts_ts );
				    }
				    fprintf( pcr_fp, "\n" );
				}
				{
				    if( nPCR>=MAX_PCRS )
				    {
					fprintf( stdout, 
					    "nPCR=%d over\n", nPCR );
					exit( 1 );
				    }
				    PCR_PID  [nPCR] = PID;
				    PCR_addr [nPCR] = curAddr;
				    PCR_value[nPCR] = LL;
				    nPCR++;
				}
				if( bPidMatch )
				{
				    if( curAddr>=addr0 )
				    {
/*
    PCR : 
    	program_clock_reference_base 33bit
	Reserved 6bit
	program_clock_reference_extension 9bit

*/
					if( mode==(-1) )	// PCR_UPDATE
					{
					unsigned long long curPCR =
					    (LL>>15);
					newPCR = newPCR+PcrOffset;
					newPCR = newPCR & 0x1FFFFFFFFLL;
					fprintf( stdout, 
					"PCR update(%8X) %9llX->%9llX\n",
					curAddr, curPCR, newPCR );
#if 0
			    unsigned int nPCR_H = newPCR>>17;
			    unsigned int nPCR_L = ((newPCR<<15) &0xFFFF8000);
#else
			    unsigned int nPCR_base = newPCR_27M/300;
			    unsigned int nPCR_ext  = newPCR_27M%300;
			    unsigned int nPCR_H = nPCR_base>>17;
			    unsigned int nPCR_L = ((nPCR_base<<15) &0xFFFF8000)
			                        |   nPCR_ext;
#endif
//					   ((newPCR_27M<<15) &0xFFFF8000);
//				  (PCR_L & 0x7FFF) | ((newPCR<<15) &0xFFFF8000);

					byteS[ 6] = (nPCR_H>> 8)&0xFF;
					byteS[ 7] = (nPCR_H>> 0)&0xFF;
					byteS[ 8] = (nPCR_L>>24)&0xFF;
					byteS[ 9] = (nPCR_L>>16)&0xFF;
					byteS[10] = (nPCR_L>> 8)&0xFF;
					byteS[11] = (nPCR_L>> 0)&0xFF;
					}
					L_PCR_addr = curAddr;
				    }
				}
			    } else {
				if( bPrint )
				fprintf( stdout, "%02X (%02X) (No PCR) : ",
				    adapt_len, flag );
			    }
			  }
			  skip+=(1+adapt_len);
			}
			if( bPayload[PID] )
			{
			    nPayload[PID]++;
			    if( bPrint )
			    {
				if( bAdapt==0 )
				{
				    fprintf( stdout, 
					"%8llX(%5d): %04X : %X,%X : ",
				g_addr-TsPacketSize, cnt, PID, adapt, count );
				    bOut++;
				}
			    }
			    if( payloadStart )
			    {
				if( bPrint )
				{
				    if( bOut==0 )
				    fprintf( stdout, "%8llX(%5d): ", 
				    	g_addr-TsPacketSize, cnt );
				    fprintf( stdout, "Payload Start" );
				}
				if( mode==2 )
				    bRecord = 1;
			    } else {
				if( bPrint )
				fprintf( stdout, "(Payload)" );
			    }
			}
// --------------------------------------------------------------
			if( flag>=0 )
			    if( bPrint )
				fprintf( stdout, "%02X : ", flag );
			if( bDisc>0 )
			    if( bPrint )
				fprintf( stdout, "(Discon)" );
			if( bEXT>0 )
			    if( bPrint )
				fprintf( stdout, "(EXT)" );
// --------------------------------------------------------------
			if( Lcount[PID]>=0 )
			{
#if 0
	fprintf( stdout, "Check(%4d:%d->%d)\n", PID, Lcount[PID], count );
#endif
			    int Ncount = (Lcount[PID]+1)&15;
			    if( count!=Ncount )
			    {
				if( bCheckContinuous )
				{
				    fprintf( stdout, 
				    "\n*** CC Update(%4d:%d:%d->%d) ***", 
					PID, Lcount[PID], count, Ncount );
				    byteS[3] = (byteS[3] & 0xF0) | Ncount;
//				    count = Ncount;	// comment out 20131210
				}
				NGcount[PID]++;
			    }
			}
			Lcount[PID] = count;
		    }
		    if( bOut )
		    {
			if( bPrint )
			fprintf( stdout, "\n" );
			bOut=0;
		    }
		    if( mode==1 )
		    {
			fprintf( stdout, "%5d : %04X\n", cnt, PID );
			int j;
			for( j=0; j<TS_PACKET_SIZE; j++ )
			{
			    if( j%16==0 )
				fprintf( stdout, "%03X : ", j );
			    fprintf( stdout, "%02X ", bytes[j] );
			    if( j%16==15 )
				fprintf( stdout, "\n" );
			}
			if( j&15 )
				fprintf( stdout, "\n" );
		    }
		    int writeSize = 188-skip;
		    if( bPayload[PID]==0 )
		    	writeSize = 0;
//fprintf( stdout, "Write(%04X) : bPayload=%d\n", outPID, bPayload[outPID] );
#ifdef ODA
fprintf( stdout, "skip=%d, writeSize=%d, %d\n", skip, writeSize, TsPacketSize );
fflush( stdout );
#endif
		    if( bRecord )
		    {
		    	int outPID=PID;
			int adrPID=PID;
			if( bAllegroMVC )
			if( PID==0x81 )
			{
			    outPID=0x80;
			    adrPID=0x80;
			}
			if( out_fp[outPID]==NULL )
			{
			    char binFilename[1024];
			    if( directory[0] )
			    {
				sprintf( binFilename, "%s/PES-%04X.bin", 
				    directory, outPID );
			    } else
				sprintf( binFilename, "PES-%04X.bin", outPID );
			    fprintf( stdout, "open(%s)\n", binFilename );
			    out_fp[outPID] = fopen( binFilename, "wb" );
			    if( out_fp[outPID]==NULL )
			    {
				fprintf( stdout, "Can't open bin [%s]\n",
				    binFilename );
				exit( 1 );
			    }
			}
			if( addr_fp[adrPID]==NULL )
			{
			    char addrFilename [1024];
			    if( directory[0] )
				sprintf( addrFilename , "%s/PES-%04X.txt", 
					directory, adrPID );
			    else
				sprintf( addrFilename , "PES-%04X.txt", adrPID);
			    fprintf( stdout, "open(%s)\n", addrFilename );
			    addr_fp[adrPID] = fopen( addrFilename, "wb" );
			    if( addr_fp[adrPID]==NULL )
			    {
				fprintf( stdout, "Can't open addr [%s]\n",
				    addrFilename );
				exit( 1 );
			    }
			    fprintf( addr_fp[adrPID], "%s\n", filename );
			}
			//
			if( writeSize>0 )
			{
			    char str[256];
			    unsigned int cPCRH = PCR_H;
			    unsigned int cPCRL = PCR_L;
			    unsigned int cPCRpid = PCR_pid;
			    if( PCRH[adrPID]!=0xFFFFFFFF )
			    {
			    	cPCRH=PCRH[adrPID];
			    	cPCRL=PCRL[adrPID];
				cPCRpid = adrPID;
			    }
			    unsigned long long LL = cPCRH;
			    unsigned long long LL_MASK = 0xFFFFFFFFFLL;
			    LL = (LL<<32) | cPCRL;
			    LL = LL>>15;
			    sprintf( str, 
			    "%10llX, %10llX, %10llX, %4X, %8X %8X %9llX %4X", 
				g_addr-TsPacketSize,	// TS addr(top)
				g_addr-TsPacketSize+skip+byteOffset,
							// TS addr(pes start)
				nAddr[adrPID], 		// PES addr
				writeSize,		// PES size
				cPCRH, cPCRL, 
				LL & LL_MASK, 
				cPCRpid & 0xFFFF );
			    fprintf( addr_fp[adrPID], "%s", str );
			    if( tts_fp==NULL )
			    	fprintf( addr_fp[adrPID], "\n" );
			    else
			    	fprintf( addr_fp[adrPID], " %8X\n", tts_ts );
			    int written = fwrite( &byteS[skip], 
				    writeSize, 1, out_fp[outPID] );
			    if( written<1 )
			    {
				fprintf( stdout, "Can't write %s (%d)\n", 
				    outfile, writeSize );
				break;
			    } else {
				if( bWriteMsg )
				{
				fprintf( stdout, "%5d : %04X : %d : ", 
				    cnt, adrPID, adapt );
				fprintf( stdout, "%d written\n", writeSize );
				}
			    }
			} else {
			    if( bPrint )
			    {
			    fprintf( stdout, "%5d : %04X : %d : ", 
			    	cnt, adrPID, adapt );
			    fprintf( stdout, "No write data\n" );
			    }
			}
			if( writeSize>0 )
			{
			    nAddr[adrPID] += writeSize;
			    nLast[adrPID] = g_addr;
			}
		    }
		}
	    }
	    // ---------------------------------------------------------
	    // for TTS
	    // ---------------------------------------------------------
	    if( tts_fp )
	    {
	    	long long PCR_LL = PCR_H;
		PCR_LL = (PCR_LL<<32) | PCR_L;
		PCR_LL = PCR_LL>>15;
	    	long long L_PCR_LL = L_PCR_H;
		L_PCR_LL = (L_PCR_LL<<32) | L_PCR_L;
		L_PCR_LL = L_PCR_LL>>15;
		int diffPCR = PCR_LL-L_PCR_LL;
		if( diffPCR!=0 )
		{
		    int diffTTS = tts_ts-L_tts_ts_PCR;
		    if( diffTTS != (diffPCR*300) )
		    {
#if 0
			fprintf( stdout, 
			    "diffPCR=%8d, diffTTS=%8d\n",
			    diffPCR, diffTTS );
#endif
			tts_ts = L_tts_ts_PCR+(diffPCR*300);
			bytes[0] = (tts_ts>>24)&0xFF;
			bytes[1] = (tts_ts>>16)&0xFF;
			bytes[2] = (tts_ts>> 8)&0xFF;
			bytes[3] = (tts_ts>> 0)&0xFF;
		    }
		    L_PCR_H = PCR_H;
		    L_PCR_L = PCR_L;
		    L_tts_ts_PCR = tts_ts;
		}
	    }
	    // ---------------------------------------------------------
	    if( editfile )
	    {
	    	int written=(-1);
	    	if( mode<0 )	// PCR update/CUT_END
		{
		} else {
		    if( bPidMatch
		     || (PID<2) )
		    {
		    } else {
			if( bRemoveOtherPacket )
			{
			    memset( bytes, TsPacketSize, 0xFF );
			    byteS[0] = 0x47;
			    byteS[1] = 0x1F;
			    byteS[2] = 0xFF;
			    byteS[3] = 0x1F;
			}
		    }
		}
		if( (bCutEnd==0) || (nLastAll>=g_addr) )
		{
		    written = fwrite( bytes, TsPacketSize, 1, edit );
		    if( written<1 )
		    {
			fprintf( stdout, "Can't write %s\n", editfile );
			break;
		    }
		    total_written += TsPacketSize;
#if 0
		    fprintf( stdout, "write(%d),0x%X\n", 
		    	TsPacketSize, total_written );
#endif
		    AddOne=0;
		}
	    }
	    if( bEOF )
	    {
		if( newPCR>EndPcr )
		{
		    fprintf( stdout, "newPCR=%9llX, EndPcr=%9X\n",
			    newPCR, EndPcr );
		    break;
		} else {
		    if( bAppend==0 )
		    fprintf( stdout, "WritePCR(%9llX\n", newPCR );
		    bAppend=1;
		}
		fprintf( stdout, "WritePCR(%9llX/%8X@%8llX)\n", 
			newPCR, EndPcr, g_addr );
	    }
	}
	// ----------------------------------------------------
	if( total_written>0 )
	    fprintf( stdout, "total_written=0x%X\n", total_written );
	if( tts_fp )
	    fprintf( tts_fp, "min=%8d, max=%8d\n", tts_ts_min, tts_ts_max ) ;
	fprintf( stdout, "nPCR=%d\n", nPCR );
	for( ii=0; ii<MAX_PID; ii++ )
	{
	    if( bAllegroMVC )
	    if( ii==0x81 )
		continue;
	    if( out_fp[ii] )
	    {
		fclose( out_fp[ii] );
	    }
	    if( addr_fp[ii] )
	    {
		fclose( addr_fp[ii] );
	    }
	}
	if( edit )
	    fclose( edit );
	fclose( ts_fp );
	nLastAll = 0;
	for( ii=0; ii<MAX_PID; ii++ )
	{
	    if( nCount[ii]>0 )
		fprintf( stdout, "PID(%04X) : nCount  =%d\n", ii, nCount[ii] );
	    if( nPayload[ii]>0 )
	    {
		fprintf( stdout, "PID(%04X) : nPayload=%d\n", ii, nPayload[ii]);
		fprintf( stdout, "            Continuity Counter Error=%d\n",
			NGcount[ii] );
	    }
	    if( nAddr[ii]>0 )
	    {
		fprintf( stdout, "PID(%04X) : nAddr   =%8llX(%8lld) %8X\n", 
			ii, nAddr[ii], nAddr[ii], nLast[ii] );
		if( ii<0x1FFF )
		    if( nLast[ii]>nLastAll )
		    	nLastAll = nLast[ii];
	    }
	}
	fprintf( stdout, "nLastAll=%8X\n", nLastAll );
	if( nSyncError>0 )
	    fprintf( stdout, "Sync Error = %d\n", nSyncError );
	if( pcr_fp )
	    fclose( pcr_fp );
	if( tts_fp )
	    fclose( tts_fp );
	
	if( bGetPTS )
	{
	// -----------------------------------------
	// get All PTS
	int IDS[MAX_PID];
	int nID=0;
	initTsAddr( );
	for( ii=1; ii<(MAX_PID-1); ii++ )
	{
	    if( addr_fp[ii] )
	    {
	    	FILE *pts_fp, *bin_fp;
	    	char binFile[1024];
	    	char txtFile[1024];
		char ptsFile[1024];
		if( directory[0] )
		{
		    sprintf( binFile, "%s/PES-%04X.bin", directory, ii );
		    sprintf( txtFile, "%s/PES-%04X.txt", directory, ii );
		    sprintf( ptsFile, "%s/PES-%04X.pts", directory, ii );
		} else {
		    sprintf( binFile, "PES-%04X.bin", ii );
		    sprintf( txtFile, "PES-%04X.txt", ii );
		    sprintf( ptsFile, "PES-%04X.pts", ii );
		}
	    	fprintf( stdout, "Parse [%s] (0x%X)\n", filename, ii );
		int nTS=0;
		char tsFilename[1024];
		if( ReadAddrFile( txtFile, tsFilename, TsAddr, PesAddr,
		    &nTS, ADDR_MAX )<0 )
		    exit( 1 );
		bin_fp = fopen( binFile, "r" );
		if( bin_fp==NULL )
		{
		    fprintf( stdout, "Can't open [%s]\n", binFile );
		    exit( 1 );
		}
//		fprintf( stdout, "bin[%s]\n", binFile );
		pts_fp = fopen( ptsFile, "w" );
		if( pts_fp==NULL )
		{
		    fprintf( stdout, "Can't open [%s]\n", ptsFile );
		    exit( 1 );
		}
//		fprintf( stdout, "Pts[%s]\n", ptsFile );
		gettimeofday( &timeval0, NULL );
		fprintf( stdout, "ParsePts(%s,%s)\n", binFile, ptsFile );
		int nPTS = ParsePts( bin_fp, pts_fp );
		gettimeofday( &timeval1, NULL );
		int tt;
		tt = (timeval1.tv_sec -timeval0.tv_sec )*1000;
		tt +=(timeval1.tv_usec-timeval0.tv_usec)/1000;
		fprintf( stdout, "Parsed(%d ms) nPTS=%d\n", tt, nPTS );
		fflush( stdout );
		if( nPTS>0 )
		{
		    IDS[nID++] = ii;
		}
		fclose( bin_fp );
		fclose( pts_fp );
	    }
	}
	int p;
	char buffer[1024];
	char pcr_ptsFilename[1024];
	if( directory[0] )
	    sprintf( pcr_ptsFilename, "%s/PCR-PTS.txt", directory );
	else
	    sprintf( pcr_ptsFilename, "PCR-PTS.txt" );
	FILE *out = fopen( pcr_ptsFilename, "w" );
	if( out==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", pcr_ptsFilename );
	    exit( 1 );
	}
	fprintf( out, "%10s,%10s,", "bytes", "PCR" );
	for( p=0; p<nID; p++ )
	{
	    fprintf( out, "PES-%04X(DTS),", IDS[p] );
	    fprintf( out, "PES-%04X(PTS),", IDS[p] );
	}
	fprintf( out, "\n" );
	fprintf( stdout, "Generate PCR.txt\n" );
	pcr_fp = fopen( pcrFilename, "r" );
	if( pcr_fp==NULL )
	{
	    fprintf( stdout, "Can't open PCR.txt\n" );
	    exit( 1 );
	}
	if( pcr_fp )
	{
	    while( 1 )
	    {
	    	long long ADDR, PCR;
		int Pid, pcr_h, pcr_l;
	    	if( fgets( buffer, 1024, pcr_fp )==NULL )
		    break;
		sscanf( buffer, "%8llX : %04X : %08X,%08X %09llX\n",
			&ADDR, &Pid, &pcr_h, &pcr_l, &PCR );
		if( (pid[0]<0) || (pid[0]==Pid ) )
		{
		    fprintf( out, "%10d,%10d,", 
			(unsigned int)ADDR, (unsigned int)PCR );
		    for( ii=0; ii<nID; ii++ )
		    {
			fprintf( out, ",," );
		    }
		    fprintf( out, "\n" );
		}
	    }
	    fclose( pcr_fp );
	}
	fprintf( stdout, "PCR done\n" );
	FILE *pts_fp=NULL;
	for( p=0; p<nID; p++ )
	{
	    char ptsFilename[1024];
	    if( directory[0] )
		sprintf( ptsFilename, "%s/PES-%04X.pts", directory, IDS[p] );
	    else
		sprintf( ptsFilename, "PES-%04X.pts", IDS[p] );
	    fprintf( stdout, "open(%s)\n", ptsFilename );
	    fflush( stdout );
	    pts_fp = fopen( ptsFilename, "r" );
	    if( pts_fp==NULL )
	    {
	    	fprintf( stdout, "Can't open %s\n", ptsFilename );
		exit( 1 );
	    }
	    fgets( buffer, 1024, pts_fp );
	    while( 1 )
	    {
	    	if( fgets( buffer, 1024, pts_fp )==NULL )
		    break;
		int ts_addr, pes_addr;
		int ptsH, ptsL;
		int dtsH, dtsL;
		sscanf( (char *)buffer, "%8X %8X : %4X %8X, %4X %8X",
		    &pes_addr, &ts_addr, &ptsH, &ptsL, &dtsH, &dtsL );
#if 0
		fprintf( out, "%10d,          ,", ts_addr );
#else
		fprintf( out, "%10d,,", ts_addr );
#endif
		for( ii=0; ii<p; ii++ )
		{
#if 0
		    fprintf( out, "          ," );
#else
		    fprintf( out, ",," );
#endif
		}
		if( dtsL==0xFFFFFFFF )
		    fprintf( out, "%10d,", ptsL );
		else
		    fprintf( out, "%10d,", dtsL );
		fprintf( out, "%10d,", ptsL );
		for( ii=p+1; ii<nID; ii++ )
		{
#if 0
		    fprintf( out, "          ," );
#else
		    fprintf( out, ",," );
#endif
		}
		fprintf( out, "\n" );
	    }
	    fclose( pts_fp );
	}
	fclose( out );
	}
	fprintf( stdout, "TsDump(%s) done\n", filename );

	return nPCR;
}

int TsTest( char filename[], char outFilename[], int start, int pid )
{
FILE *fp=NULL;
unsigned char bytes[1024*4];
int readed;
int PID=(-1);
int PCR_H=(-1);
int PCR_L=(-1);
int i;
struct stat Stat;
FILE *out_fp [MAX_PID];
int bPayload [MAX_PID];
int nPayload [MAX_PID];
int nCount   [MAX_PID];
int nAddr    [MAX_PID];
int 	     Lcount[MAX_PID];
int bPrint=1;
	for( i=0; i<MAX_PID; i++ )
	{
	    out_fp  [i] = NULL;
	    bPayload[i] = 0;
	    nPayload[i] = 0;
	    nCount  [i] = 0;
	    nAddr   [i] = 0;
	    Lcount  [i] = (-1);
	}
	if( stat( filename, &Stat )!=0 )
	{
	    fprintf( stdout, "Can't stat %s\n", filename );
	    exit( 1 );
	}
	int size = (int)Stat.st_size;
//	int packets = size/TS_PACKET_SIZE;
	int packets = size/TsPacketSize;
	fprintf( stdout, "size=%d, packets=%d\n", size, packets );
	fp = fopen ( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}
	for( i=0; ; i++ )
	{
	    if( (i%10000)==0 )
	    {
		if( bDebug )
		    fprintf( stdout, "\r%d ", i );
    //		    fprintf( stdout, "*" );
		fflush( stdout );
	    }
	    readed = gread( bytes, TsPacketSize, 1, fp );
	    if( readed<1 )
	    {
		fprintf( stdout, "\nEOF@%d(addr=0x%llX,%lld)\n", 
			i, g_addr, g_addr );
		break;
	    }
	    if( bytes[0]==0x47 )
		PID = ((bytes[1]<<8) | (bytes[2]<<0)) & 0x1FFF;
	    else {
	    	PID = 0xFFFF;
		fprintf( stdout, "Invalid Sync Byte(%02X)\n", bytes[0] );
	    }
//	    int ts_error     = (bytes[1]>>7)&1;
	    int payloadStart = (bytes[1]>>6)&1;
//	    int ts_priority  = (bytes[1]>>5)&1;
	    int adapt = (bytes[3]>>4)&3;
	    int count = bytes[3]&15;
	    int adapt_len = bytes[4];
	    int bOut=0;
	    int skip=0;
	    int bAdapt  =0;
	    if( (PID==pid) || (pid<0) )
	    {
	    	switch( adapt )
		{
		case 0 :	// ?
		    fprintf( stdout, "adapt=%d\n", adapt );
		    break;
		case 1 :	// no adapt
		    bPayload[PID]=1;
		    bAdapt  =0;
		    break;
		case 2 :	// adapt
		    bPayload[PID]=0;
		    bAdapt  =1;
		    break;
		case 3 :	// adapt+payload
		    bPayload[PID]=1;
		    bAdapt  =1;
		    break;
		default :
		    fprintf( stdout, "Invalid adapt(%d)\n", adapt );
		    exit( 1 );
		    break;
		}
		skip+=4;	// Header 4byte
		if( PID==0x1FFF )
		{
#if 0
		    fprintf( stdout, 
			    "%5d : %04X : %X,%X : ", i, PID, adapt, count );
		    fprintf( stdout, "\n" );
#endif
		} else {
		    if( bAdapt )
		    {
		    	if( bPrint )
			fprintf( stdout, 
			    "%5d : %04X : %X,%X : ", i, PID, adapt, count );
			bOut++;
			int flag = bytes[5];
			int bPCR  = flag & (1<<4);
//			int bOPCR = flag & (1<<3);
//			int bSPR  = flag & (1<<2);
//			int bPRV  = flag & (1<<1);
//			int bEXT  = flag & (1<<0);
			if( bPrint )
			{
			    if( bPCR )
			    {
				PCR_H = (bytes[ 6]<< 8) | (bytes[ 7]<< 0);
				PCR_L = (bytes[ 8]<<24) | (bytes[ 9]<<16)
				      | (bytes[10]<< 8) | (bytes[11]<< 0);
				fprintf( stdout, 
				    "%02X (%02X) PCR : %04X,%08X : ", 
				    adapt_len, flag, PCR_H, PCR_L );
			    } else {
				fprintf( stdout, "%02X (%02X) (No PCR) : ",
				    adapt_len, flag );
			    }
			}
			skip+=(1+adapt_len);
			if( Lcount[PID]>=0 )
			{
#if 1
			    if( bCheckContinuous )
			    if( ((Lcount[PID]+0)&15)!=count )
			    {
				fprintf( stdout, 
				"*** Update(%d:%d) ***\n", Lcount[PID], count );
				bytes[3] = (bytes[3] & 0xF0) | Lcount[PID];
			    }
#else
			    bytes[3] = (bytes[3] & 0xF0) | Lcount[PID];
#endif
			}
		    }
		    if( bPayload[PID] )
		    {
			nPayload[PID]++;
			if( payloadStart )
			{
			    if( bPrint )
			    {
				if( bOut==0 )
				fprintf( stdout, "%5d : ", i );
				fprintf( stdout, "Payload Start\n" );
			    }
			    bOut=0;
			} else {
			}
		    }
		}
		if( bOut )
		{
		    if( bPrint )
		    fprintf( stdout, "\n" );
		    bOut=0;
		}
	    }
	}
	fclose( fp );
	return 0;
}

int GetTable( unsigned char **ptr,
	int *pTableID,
	int *pSecLen,
	int *pID,
	int *pSecNo,
	int *pLsecNo )
{
unsigned char *bytes = *ptr;
	//
	int section, ID;
	int tableID = *(bytes++);		// 5
	section = *(bytes++);			// 6
	section = (section<<8) | *(bytes++);	// 7
	int secLen = section & 0xFFF;
	if( (section>>12) != 0xB )
	{
	    fprintf( stdout, "Invalid section (%4X)\n", section );
	    return -1;
	}
	ID = *(bytes++);			// 8
	ID = (ID<<8) | *(bytes++);		// 9
	int version = *(bytes++);		// 10
	int secNo   = *(bytes++);		// 11
	int LsecNo  = *(bytes++);		// 12
	//
	*pTableID = tableID;
	*pSecLen = secLen;
	*pID = ID;
	*pSecNo = secNo;
	*pLsecNo = LsecNo;
	*ptr = bytes;
	if( bDebug )
	fprintf( stdout, "version=%d\n", version );
	return 0;
}

int DumpPAT( unsigned char *ptr )
{
	int tableID, secLen, TSID, secNo, LsecNo;
	if( GetTable( &ptr, 
		&tableID, &secLen, &TSID, &secNo, &LsecNo )<0 )
	    return -1;
	fprintf( stdout, "Table=%02X, Len=%4X, TSID=%04X (%02X:%02X)\n",
		tableID, secLen, TSID, secNo, LsecNo );
	int j;
	secLen-=8;
	for( j=0; j<(secLen-4); j+=4 )
	{
	    int programNo, PMT_PID;
	    programNo = *(ptr++);
	    programNo = (programNo<<8) | *(ptr++);
	    PMT_PID   = *(ptr++);
	    PMT_PID   = (PMT_PID  <<8) | *(ptr++);
	    fprintf( stdout, "Program = %04X, PMT_PID=%04X\n",
		programNo, PMT_PID & 0x1FFF );
	}
	int CRC;
	CRC = *(ptr++);
	CRC = (CRC<<8) | *(ptr++);
	CRC = (CRC<<8) | *(ptr++);
	CRC = (CRC<<8) | *(ptr++);
	fprintf( stdout, "CRC=%08X\n", CRC );
	return 0;
}

void DumpPMT( unsigned char *ptr )
{
int j, k;
int secPos=1;
	int tableID, secLen, PGMID, secNo, LsecNo;
	GetTable( &ptr, 
		&tableID, &secLen, &PGMID, &secNo, &LsecNo );
	fprintf( stdout, "Table=%02X, Len=%4X, PGMID=%04X (%02X:%02X)\n",
		tableID, secLen, PGMID, secNo, LsecNo );
	secPos+=8;
//	fprintf( stdout, "secPos=%d, secLen=%d\n", secPos, secLen );
	int PCR_pid, PGM_info;
	PCR_pid = *(ptr++);
	PCR_pid = (PCR_pid<<8) | *(ptr++);
	PGM_info = *(ptr++);
	PGM_info = (PGM_info<<8) | *(ptr++);
	if( (PCR_pid & 0xE000)!=0xE000 )
	    fprintf( stdout, "Invalid PCR_pid(%X)\n", PCR_pid );
	if( (PGM_info & 0xF000)!=0xF000 )
	    fprintf( stdout, "Invalid PGM_info(%X)\n", PGM_info );
	secPos+=4;
	PCR_pid  = PCR_pid  & 0x1FFF;
	PGM_info = PGM_info & 0x0FFF;
	fprintf( stdout, "PCR_pid=%04X, PGM_info=%04X : ",
		PCR_pid, PGM_info );
	for( j=0; j<PGM_info; j++ )
	{
	    fprintf( stdout, "%02X ", *(ptr++) );
	    secPos++;
	}
	fprintf( stdout, "\n" );
//	fprintf( stdout, "secPos=%d, secLen=%d\n", secPos, secLen );
	for( ; secPos<secLen; )
	{
	    int streamType, elemPID, ES_len;
	    streamType = *(ptr++);
	    elemPID    = *(ptr++);
	    elemPID    = (elemPID<<8) | *(ptr++);
	    ES_len     = (*ptr++);
	    ES_len     = (ES_len <<8) | *(ptr++);
	    secPos+=5;
	    if( (elemPID&0xE000)!=0xE000 )
	    	fprintf( stdout, "Invalid elemPID(%X)\n", elemPID );
	    if( (ES_len&0xF000)!=0xF000 )
	    	fprintf( stdout, "Invalid ES_len(%X)\n", ES_len );
	    elemPID = elemPID & 0x1FFF;
	    ES_len  = ES_len  & 0x0FFF;
	    fprintf( stdout, "StrmType=%02X, PID=%04X, info=%02X : ",
	    	streamType, elemPID, ES_len );
	    for( k=0; k<ES_len; k++ )
	    {
	    	fprintf( stdout, "%02X ", *(ptr++) );
		secPos++;
	    }
	    fprintf( stdout, "\n" );
//	    fprintf( stdout, "secPos=%d, secLen=%d\n", secPos, secLen );
	}
	int CRC;
	CRC = *(ptr++);
	CRC = (CRC<<8) | *(ptr++);
	CRC = (CRC<<8) | *(ptr++);
	CRC = (CRC<<8) | *(ptr++);
	fprintf( stdout, "CRC=%08X\n", CRC );
}

int TsPAT( char filename[] )
{
FILE *fp=NULL;
unsigned char bytes[1024*4];
int readed;
int PID=(-1);
int i;
struct stat Stat;
int total=0;
	if( stat( filename, &Stat )!=0 )
	{
	    fprintf( stdout, "Can't stat %s\n", filename );
	    exit( 1 );
	}
	int size = (int)Stat.st_size;
//	int packets = size/TS_PACKET_SIZE;
	int packets = size/TsPacketSize;
	fprintf( stdout, "size=%d, packets=%d\n", size, packets );
	fp = fopen ( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}
	for( i=0; ; i++ )
	{
	    if( (i%10000)==0 )
	    {
		if( bDebug )
		    fprintf( stdout, "\r%d ", i );
    //		    fprintf( stdout, "*" );
		fflush( stdout );
	    }
	    readed = gread( bytes, TsPacketSize, 1, fp );
	    if( readed<1 )
	    {
		fprintf( stdout, "\nEOF@%d(addr=0x%llX,%lld)\n", 
			i, g_addr, g_addr );
		break;
	    }
	    if( bytes[0]==0x47 )
		PID = ((bytes[1]<<8) | (bytes[2]<<0)) & 0x1FFF;
//	    int adapt = (bytes[3]>>4)&3;
//	    int count = bytes[3]&15;
	    int adapt_len = bytes[4];
	    if( PID==0x0000 )	// PAT
	    {
//		fprintf( stdout, "adapt=%d, count=%d, adapt_len=%d\n",
//		    adapt, count, adapt_len );
		fprintf( stdout, "PID = %04X (%X)\n", PID, total );
		if( adapt_len==0 )
		    DumpPAT( &bytes[5] );
		else
		    DumpPAT( &bytes[5+adapt_len+1] );
	    } 
	    total += TsPacketSize;
	}
	fclose( fp );
	return 0;
}

int TsPMT( char filename[], int pids[MAX_PIDS], int nPid )
{
FILE *fp=NULL;
unsigned char bytes[1024*4];
int readed;
int PID=(-1);
int i;
struct stat Stat;
int total=0;
	if( stat( filename, &Stat )!=0 )
	{
	    fprintf( stdout, "Can't stat %s\n", filename );
	    exit( 1 );
	}
	int size = (int)Stat.st_size;
//	int packets = size/TS_PACKET_SIZE;
	int packets = size/TsPacketSize;
	fprintf( stdout, "size=%d, packets=%d\n", size, packets );
	fp = fopen ( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}
	for( i=0; ; i++ )
	{
	    if( (i%10000)==0 )
	    {
		if( bDebug )
		    fprintf( stdout, "\r%d ", i );
    //		    fprintf( stdout, "*" );
		fflush( stdout );
	    }
	    readed = gread( bytes, TsPacketSize, 1, fp );
	    if( readed<1 )
	    {
		fprintf( stdout, "\nEOF@%d(addr=0x%llX,%lld)\n", 
			i, g_addr, g_addr );
		break;
	    }
	    if( bytes[0]==0x47 )
		PID = ((bytes[1]<<8) | (bytes[2]<<0)) & 0x1FFF;
	    int adapt = (bytes[3]>>4)&3;
//	    int count = bytes[3]&15;
	    int adapt_len = bytes[4];
	    int bDump = 0;
	    int n;
	    for( n=0; n<nPid; n++ )
	    {
		if( PID==pids[n] )	// PMT
		    bDump = 1;
	    }
	    if( bDump )	
	    {
	    	unsigned char *nPtr = &bytes[5];
	    	if( adapt & 2 )
		{
		    fprintf( stdout, "adapt_len = %d\n", adapt_len );
		    nPtr = &bytes[5+adapt_len+1];
		}
		fprintf( stdout, "PID = %04X (%X)\n", PID, total );
	    	DumpPMT( nPtr );
	    } 
	    total += TsPacketSize;
	}
	fclose( fp );
	return 0;
}

int TsPid( char filename[] )
{
FILE *fp=NULL;
unsigned char bytes[1024*4];
int readed;
int PID=(-1);
int i;
int PIDs[MAX_PID];
struct stat Stat;
	for( i=0; i<MAX_PID; i++ )
	    PIDs[i] = 0;
	if( stat( filename, &Stat )!=0 )
	{
	    fprintf( stdout, "Can't stat %s\n", filename );
	    exit( 1 );
	}
	int size = (int)Stat.st_size;
//	int packets = size/TS_PACKET_SIZE;
	int packets = size/TsPacketSize;
	fprintf( stdout, "size=%d, packets=%d\n", size, packets );
	fp = fopen ( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}
	for( i=0; ; i++ )
	{
	    if( (i%10000)==0 )
	    {
		if( bDebug )
		    fprintf( stdout, "\r%d ", i );
    //		    fprintf( stdout, "*" );
		fflush( stdout );
	    }
	    readed = gread( bytes, TsPacketSize, 1, fp );
	    if( readed<1 )
	    {
		fprintf( stdout, "\nEOF@%d(addr=0x%llX,%lld)\n", 
			i, g_addr, g_addr );
		break;
	    }
	    if( bytes[0]==0x47 )
		PID = ((bytes[1]<<8) | (bytes[2]<<0)) & 0x1FFF;
	    if( PIDs[PID]==0 )
	    {
		fprintf( stdout, "PID = %04X\n", PID );
	    }
	    PIDs[PID]++;
	}
	fclose( fp );
	fprintf( stdout, "-----------------------------------------------\n" );
	for( i=0; i<MAX_PID; i++ )
	{
	    if( PIDs[i]>0 )
	    {
		fprintf( stdout, "PID = %04X : %8d packets\n", i, PIDs[i] );
	    }
	}
	return 0;
}


#define MAX 1000	// 0-1000 

int TsCut( char *filename, char *outfile, int start, int end, int len )
{
struct stat Stat;
FILE *fp=NULL;
FILE *out=NULL;
long long ll;
int i;
#define CUT_BUF_SIZE	1024*64
unsigned char bytes[CUT_BUF_SIZE];
	if( TsPacketSize>=CUT_BUF_SIZE )
	{
	    fprintf( stdout, "Too large TsPacketSize(%d)\n",
		    TsPacketSize );
	    exit( 1 );
	}
	if( filename==NULL )
	{
	    fprintf( stdout, "Invaid filename\n" );
	    return -1;
	}
	if( outfile==NULL )
	{
	    fprintf( stdout, "Invaid outfile\n" );
	    return -1;
	}

	// File size
	if( stat( filename, &Stat )!=0 )
	{
	    fprintf( stdout, "Can't stat %s\n", filename );
	    return -1;
	}
	long long size = Stat.st_size;
	int packets = size/TsPacketSize;
	int st=(-1), en=(-1);
	fprintf( stdout, "size=0x%llX, packets=%d\n", size, packets );

	if( (start>=0) && (end>=0) )
	{	// unit is percentage
	    if( (start>0) && (end>0) )
		len = end-start;
	    else if( (start>0) && (len>0) )
		end = start+len;
	    else if( (end>0) && (len>0) )
		start = end-len;
	    if( (start<0) || (start>MAX) )
	    {
		fprintf( stdout, "Invalid start (%d)\n", start );
		fprintf( stdout, 
		    "Start and End 's unit is 1/10 percentage (0-1000)\n" );
		return -1;
	    }
	    if( (end<0) || (end>MAX) )
	    {
		fprintf( stdout, "Invalid end (%d)\n", end );
		return -1;
	    }
	    if( end<=start )
	    {
		fprintf( stdout, "Invalid start/end (%d,%d)\n", start, end );
		return -1;
	    }
	    ll = packets;
	    st = ll*start/MAX;
	    ll = packets;
	    en = ll*end/MAX;
	    fprintf( stdout, "St(%5d/10 %%) : %8d packets\n", start, st );
	    fprintf( stdout, "En(%5d/10 %%) : %8d packets\n", end  , en );
	} else if( (start<=0) && (end<0) )
	{	// unit is packet
	    start = -start;
	    end   = -end;
	    if( (start>0) && (end>0) )
		len = end-start;
	    else if( (start>0) && (len>0) )
		end = start+len;
	    else if( (end>0) && (len>0) )
		start = end-len;
	    if( end<=start )
	    {
		fprintf( stdout, "Invalid start/end (%d,%d)\n", start, end );
		return -1;
	    }
	    fprintf( stdout, "start=%d\n", start );
	    fprintf( stdout, "end  =%d\n", end   );
	    st = start;
	    en = end;
	    fprintf( stdout, "St : %8d\n", st );
	    fprintf( stdout, "En : %8d\n", en );
	}

	fprintf( stdout, "output file=%s\n", outfile );
	fp = fopen ( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    return -1;
	}
	for( i=0; i<st; )
	{
	    int rest=st-i;
	    int Size;
	    if( rest>1024 )
	    	Size = TsPacketSize*1024;
	    else
	    	Size = TsPacketSize*rest;
	    fseek( fp, Size, SEEK_CUR );
	    i+=(Size/TsPacketSize);
	}
	out = fopen( outfile, "wb" );
	if( out==NULL )
	{
	    fprintf( stdout, "Can't write %s\n", outfile );
	    fclose( fp );
	    return -1;
	}
	if( en==packets )
	    en++;
	long long total=0;
	for( ; i<en; )
	{
	    int Size = TsPacketSize;
	    int rest = en-i;
	    if( rest>1 )
	    {
	    	Size = TsPacketSize*rest;
		if( (Size>CUT_BUF_SIZE) || (Size<0) )
		{
		    Size = CUT_BUF_SIZE/TsPacketSize;
		    Size = Size*TsPacketSize;
		}
	    }
//	    fprintf( stdout, "Read(%d)\n", Size );
	    int readed = gread( bytes, Size, 1, fp );
	    if( readed<1 )
	    {
		fprintf( stdout, "\nEOF@%d(addr=0x%llX,%lld)\n", 
			i, g_addr, g_addr );
		break;
	    }
	    int written = fwrite( bytes, Size, 1, out );
	    if( written<1 )
	    {
	    	fprintf( stdout, "Can't write %s\n", outfile );
		break;
	    }
	    i+=(Size/TsPacketSize);
	    total+=Size;
	}
	fprintf( stdout, "0x%llX(%lld) bytes written\n", total, total );

	fclose( fp );
	fclose( out );
	return 0;
}

int TsOff( char *filename, char *outfile, int pids[MAX_PIDS] )
{
FILE *fp=NULL;
FILE *out=NULL;
unsigned char bytes[1024*4];
unsigned char off  [1024];
int readed;
int PID=(-1);
int i;
int PIDs [MAX_PID];
int PIDon[MAX_PID];
struct stat Stat;

	fprintf( stdout, "TsOff(%s,%s)\n", filename, outfile );
	if( outfile==NULL )
	{
	    fprintf( stdout, "Invaid outfile\n" );
	    return -1;
	}
	if( strcmp( filename, outfile )==0 )
	{
	    fprintf( stdout, "Src and Dst is same ?\n" );
	    exit( 1 );
	}
	for( i=0; i<1024; i++ )
	    off[i] = 0xFF;
	off[0] = 0x47;
	off[1] = 0x1F;
	off[2] = 0xFF;
	off[3] = 0x10;

	for( i=0; i<MAX_PID; i++ )
	{
	    PIDs [i] = 0;
	    PIDon[i] = 0;
	}
	for( i=0; i<MAX_PIDS; i++ )
	{
	    if( pids[i]<0 )
		break;
	    PIDon[pids[i]] = 1;
	}
	PIDon[0x0000] = 1;
	PIDon[0x1FFF] = 1;
	if( stat( filename, &Stat )!=0 )
	{
	    fprintf( stdout, "Can't stat %s\n", filename );
	    exit( 1 );
	}
	long long size = Stat.st_size;
	int packets = size/TsPacketSize;
	fprintf( stdout, "size=0x%llX, packets=%d\n", size, packets );
	fp = fopen ( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    return -1;
	}
	out = fopen( outfile, "wb" );
	if( out==NULL )
	{
	    fprintf( stdout, "Can't write %s\n", outfile );
	    fclose( fp );
	    return -1;
	}
	int total=0;
	for( i=0; ; i++ )
	{
	    if( (i%10000)==0 )
	    {
		if( bDebug )
		    fprintf( stdout, "\r%d ", i );
    //		    fprintf( stdout, "*" );
		fflush( stdout );
	    }
	    readed = gread( bytes, TsPacketSize, 1, fp );
	    if( readed<1 )
	    {
		fprintf( stdout, "\nEOF@%d(addr=0x%llX,%lld)\n", 
			i, g_addr, g_addr );
		break;
	    }
	    if( bytes[0]==0x47 )
		PID = ((bytes[1]<<8) | (bytes[2]<<0)) & 0x1FFF;
	    if( PIDs[PID]==0 )
	    {
		fprintf( stdout, "PID = %04X\n", PID );
	    }
	    PIDs[PID]++;
	    int written;
	    if( PIDon[PID] )
	    	written = fwrite( bytes, 1, TsPacketSize, out ); 
	    else 
	    	written = fwrite( off, 1, TsPacketSize, out ); 

	    if( written<1 )
	    {
	    	fprintf( stdout, "Can't write (0x%X)\n", total );
		break;
	    }
	    total+=written;
	}
	fclose( fp );
	fclose( out );
	fprintf( stdout, "-----------------------------------------------\n" );
/*
	for( i=0; i<MAX_PID; i++ )
	{
	    if( PIDs[i]>0 )
	    {
		fprintf( stdout, "PID = %04X : %8d packets\n", i, PIDs[i] );
	    }
	}
*/

	return 0;
}

int ts( int argc, char *argv[] )
{
int i;
int args=0;
char *srcFilename=NULL;
char *outFilename=NULL;
char directory[1024];
int mode = MODE_ADDR;
int start=0;
int end=0;
int len=(-1);
int Error=0;
int pids[MAX_PIDS];
int nPid=0;
int nTsSize=0;
int targetAddr=(-1);
int targetSize=(-1);
int firstSync=(-1);

int *PCR_PID = NULL;
unsigned int *PCR_addr = NULL;
long long *PCR_value = NULL;
int nPCR=0;

	PCR_PID   = (int *)calloc( MAX_PCRS, sizeof(int) );
	if( PCR_PID == NULL )
	{
	    fprintf( stdout, "Can't alloc PCR_PID\n" );
	    exit( 1 );
	}
	PCR_addr  = (unsigned int *)calloc( MAX_PCRS, sizeof(int) );
	if( PCR_addr == NULL )
	{
	    fprintf( stdout, "Can't alloc PCR_addr\n" );
	    exit( 1 );
	}
	PCR_value = (long long *)calloc( MAX_PCRS, sizeof(long long) );
	if( PCR_value == NULL )
	{
	    fprintf( stdout, "Can't alloc PCR_value\n" );
	    exit( 1 );
	}

	memset( directory, 0, 1024 );
	for( i=0; i<MAX_PIDS; i++ )
	{
	    pids[i] = (-1);
	}

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='=' )
	    {
	    	switch( argv[i][1] )
		{
		    case 'T' :
		    	mode = MODE_TS2TTS;
			break;
		    case 'A' :
			bAllegroMVC=1;
			break;
		    default :
		    	tsUsage( argv[0] );
			break;
		}
	    } else if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		    case 'V' :
		    	bDebug = 1;
			break;
		    case 'a' :
		    	bAll = 1;
			break;
		    case 's' :
		    	bShowAddr = 1;
			break;
		    case 'e' :
		    	if( argv[i][2]=='s' )	// sec
			{
			EndPcr = GetValue( &argv[i][3], &Error )*   90000;
			} else {	// 90KHz 
			EndPcr = GetValue( &argv[i][2], &Error );
			}
			break;

		// ---------------------------------------------------
		// MODE_XXXX
		// ---------------------------------------------------
		    case 'D' :
		    	mode = MODE_DUMP;
			break;
		    case 'd' :
		    	mode = MODE_DUMP2;
			break;
		    case 'r' :
		    	mode = MODE_DUMP3;
			bGetPTS = 1;
			break;
		    case 'P' :
		    	mode = MODE_PID;
			break;
		    case 'A' :
		    	mode = MODE_PAT;
			break;
		    case 'M' :
		    	mode = MODE_PMT;
			break;
		    case 'C' :
		    	mode = MODE_CUT;
			break;
		    case 'O' :
		    	mode = MODE_OFF;
			break;
		    case 't' :
		    	mode = MODE_TEST;
			break;
		    case 'w' :
		    	mode = MODE_PCR_ANALYZE;
			break;
		    case 'W' :
		    	mode = MODE_PCR_UPDATE;
			PcrOffset = GetValue( &argv[i][2], &Error );
			break;
		    case 'c' :
		    	mode = MODE_CUT_END;
		    	bCutEnd = 1;
		    	break;
		// ---------------------------------------------------
		    case 'G' :
			PcrGain = GetValue( &argv[i][2], &Error );
			if( PcrGain<2 )
			{
			    fprintf( stdout, 
				"PcrGain(1/2%%) should greater than 3\n" );
			    exit( 1 );
			}
			break;
		    case 'g' :
			PcrKbps = GetValue( &argv[i][2], &Error );
			if( PcrKbps<1 )
			{
			  fprintf( stdout, "PcrKbps should greater than 0\n" );
			    exit( 1 );
			}
			break;
		    case 'T' :
		    	nTsSize = GetValue( &argv[i][2], &Error );
		    	break;
		    case 'p' :
		    	if( nPid>MAX_PIDS )
			{
			    fprintf( stdout, "Too many nPid(%d)\n", nPid );
			    tsUsage( argv[0] );
			}
		    	pids[nPid++] = GetValue( &argv[i][2], &Error );
		    	break;
		    case 'S' :
		    	start = atoi( &argv[i][2] );
		    	break;
		    case 'E' :
		    	end = atoi( &argv[i][2] );
		    	break;
		    case 'L' :
		    	len = atoi( &argv[i][2] );
		    	break;
		    case 'R' :
		    	bRemoveOtherPacket = 1;
		    	break;
		    default :
		    	tsUsage( argv[0] );
			break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		    case 'D' :
//			bDirectory=1;
			if( argv[i][2]>0 )
			    memcpy( directory, &argv[i][2], strlen(argv[i]) );
			break;
		    case 'P' :
			bPrintPCR=1;
			break;
		    case 'p' :
			bGetPTS=1;
			break;
		    case 'C' :
		    	bCheckContinuous = 1;
			break;
		    case 'W' :
		    	bWriteMsg = 1;
			break;
		    case 'A' :
		    	targetAddr = GetValue( &argv[i][2], &Error );
		    	break;
		    case 'S' :
		    	targetSize = GetValue( &argv[i][2], &Error );
		    	break;
		    case 'a' :
			bDumpAdapt=1;
			break;
		    case 'm' :
			bMaskDiscon=1;
			break;
		    default :
		    	tsUsage( argv[0] );
			break;
		}
	    } else {
	    	switch( args )
		{
		case 0 :
		    srcFilename = argv[i];
		    break;
		case 1 :
		    outFilename = argv[i];
		    break;
		}
		args++;
	    }
	}
	if( directory[0] )
	{
	    if( directory[0]==0 )
	    {
	    	int len = strlen(srcFilename);
	    	memcpy( directory, srcFilename, len );
		for( i=0; i<len; i++ )
		{
		    if( directory[i]=='.' )
		    {
		    	directory[i] = 0;
			break;
		    }
		}
		mode_t mode = S_IFDIR //   0040000   ディレクトリ
			    | S_IRUSR //   00400     所有者の読み込み許可
			    | S_IWUSR //   00200     所有者の書き込み許可
		            | S_IXUSR //   00100     所有者の実行許可
			    ;
	    	if( mkdir( directory, mode )!=0 )
		{
		    fprintf( stdout, "Can't create Directory(%s)\n",
			directory );
		}
	    }
	}
	if( nTsSize>0 )
	{
	    TsPacketSize = nTsSize;
	} else {
	    if( (mode!=MODE_ADDR) )
	    {
		char buffer[512];
		FILE *fp = fopen( srcFilename, "rb" );
		if( fp==NULL )
		{
		    fprintf( stdout, "Can't open src[%s]\n", srcFilename );
		    exit( 1 );
		}
		int readed = gread( (unsigned char *)buffer, 512, 1, fp );
		if( readed<1 )
		{
		    fprintf( stdout, "Can't read\n" );
		    exit( 1 );
		} else {
		    if( buffer[0]==0x47 )
		    {
		    	firstSync=0;
		    } else {
			fprintf( stdout, "first bytes isn'5 0x47 (%02X)\n",
			    buffer[0] );
			for( i=0; i<256; i++ )
			{
			    if( buffer[i]==0x47 )
			    {
			    	fprintf( stdout, "First Sync@%d\n",
				    i );
				firstSync=i;
				break;
			    }
			}
		    }
		    if( firstSync<0 )
		    {
		    	fprintf( stdout, "NoSync\n" );
			exit( 1 );
		    }
		    {
			int s;
			for( s=188; s<256; s++ )
			{
			    if( buffer[firstSync+s]==0x47 )
			    {
				fprintf( stdout, "TsPacketSize = %d\n", s );
				TsPacketSize = s;
				if( TsPacketSize!=188 )
				{
				    fprintf( stderr, "TsPacketSize=%d\n",
				    	TsPacketSize );
				}
				break;
			    }
			}
		    }
		}
		fclose( fp );
		fprintf( stdout, "TsPacketSize=%d\n", TsPacketSize );
		g_addr = 0;
	    }
	}
	if( targetAddr>=0 )
	{
//	    unsigned char bytes[TS_PACKET_SIZE+1];
	    unsigned char bytes[1024];
	    if( TsPacketSize>1023 )
	    {
	    	fprintf( stdout, "Too large TsPacketSize(%d)\n",
			TsPacketSize );
		exit( 1 );
	    }
	    fprintf( stdout, "TargetAddr=0x%X\n", targetAddr );
	    int nA = targetAddr/TsPacketSize;
//	    int tA = (nA-1)*TsPacketSize;
	    int tA = nA*TsPacketSize;
	    fprintf( stdout, "A=0x%X\n", tA );
	    FILE *fp_read = fopen( srcFilename, "rb" );
	    if( fp_read==NULL )
	    {
	    	fprintf( stdout, "Can't open %s\n", srcFilename );
		exit( 1 );
	    }
	    FILE *fp_write = fopen( outFilename, "wb" );
	    if( fp_write==NULL )
	    {
	    	fprintf( stdout, "Can't open %s\n", outFilename );
		exit( 1 );
	    }
	    fseek( fp_read, tA, SEEK_SET );
	    for( i=0; ; i++ )
	    {
	    	if( targetSize>=0 )
		if( i>=targetSize )
		    break;
		int readed = fread( bytes, TsPacketSize, 1, fp_read );
		if( readed<1 )
		{
		    fprintf( stdout, "Can't read@%d\n", i );
		    break;
		}
		int written = fwrite( bytes, TsPacketSize, 1, fp_write );
		if( written<1 )
		{
		    fprintf( stdout, "Can't write@%d\n", i );
		    break;
		}
	    }
	    fclose( fp_read );
	    fclose( fp_write );
	    return 0;
	}
	switch( mode )
	{
	case MODE_ADDR :
	    for( i=0; i<16; i++ )
	    {
		fprintf( stdout, "%2d: 0x%04X (%d)\n",
		    i, i*TS_PACKET_SIZE, i*TS_PACKET_SIZE );
	    }
	    break;
	case MODE_DUMP :
	    nPCR = TsDump( srcFilename, outFilename, directory, pids, nPid, 0,
		    PCR_PID, PCR_addr, PCR_value );
	    break;
	case MODE_DUMP2 :
	    nPCR = TsDump( srcFilename, outFilename, directory, pids, nPid, 1,
		    PCR_PID, PCR_addr, PCR_value );
	    break;
	case MODE_DUMP3 :
	    nPCR = TsDump( srcFilename, outFilename, directory, pids, nPid, 2,
		    PCR_PID, PCR_addr, PCR_value );
	    break;
	case MODE_PCR_ANALYZE :
	case MODE_PCR_UPDATE :
	case MODE_TS2TTS :
	    nPCR = TsDump( srcFilename, outFilename, directory, pids, nPid, 2,
		    PCR_PID, PCR_addr, PCR_value );
	    {
	    double d_keisu0=0;
	    int bNoCont=0;
	    int pcr_count=0;
	    unsigned int L_PCR_addr=(-1);
	    int PCR_pids[0x2000];
	    for( i=0; i<0x2000; i++ )
	    {
	    	PCR_pids[i] = 0;
	    }
	    fprintf( stdout, "nPCR=%d\n", nPCR );
	    for( i=0; i<nPCR; i++ )
	    {
	    	// PCR : 33bit,6bit,9bit
		// 33bit PCR
		//  6bit reserve
		//  9bit PCR_ext
	    	unsigned long long LL = PCR_value[i];
		unsigned int LL_H = LL>>32;
		unsigned int LL_L = LL & 0xFFFFFFFF;
		unsigned long long PCR_LL = LL>>15; // 33bit
		if( PCR_pids[PCR_PID[i]]==0 )
		    fprintf( stdout, "PCR_pid=0x%X\n", PCR_PID[i] );
		PCR_pids[PCR_PID[i]]++;
		if( PCR_PID[i]==pids[0] )
		{
		    pcr_count++;
		    if( PCR0==0xFFFFFFFF )
		    {
		    	PCR0  = PCR_LL;
			addr0 = PCR_addr[i];
			PCR1  = PCR0;
			addr1 = addr0;
		    } else {
		    	PCR1  = PCR_LL;
			addr1 = PCR_addr[i];
		    }
		    unsigned int bytes = addr1-addr0;
		    // ----- Update Here ---- //
		    unsigned int pcr_diff  = PCR1 -PCR0;
		    if( PCR1<PCR0 )
		    {
		    	unsigned int nPCR0 = (0x200000000ll-PCR0);
		    	pcr_diff = PCR1+nPCR0;
		    }
		    // ----- Update Here ---- //
//		    if( (pcr_diff>0) && (bytes>0))
		    if( (bytes>0) )
		    {
		    	int nCR=0;
		    	double d_keisu = pcr_diff;
			d_keisu = d_keisu/bytes;
			fprintf( stdout, "%4X : %8X,%09llX(%8.8f)",
			    PCR_PID[i],
			    PCR_addr[i],
			    PCR_LL & 0x1FFFFFFFFll,
			    d_keisu );
			if( d_keisu0!=0 )
			{
			    double d_diff = d_keisu-d_keisu0;
			    int diff = (int)(d_diff*100000);
//			    if( abs(diff)>1 )
//			    if( abs(diff)>2 )
			    if( abs(diff)>10 )
			    {
			    	fprintf( stdout, ": PCR diff(%d)\n", diff );
				nCR++;
				bNoCont=1;
			    } else {
			    	if( bNoCont==0 )
				    d_keisuL = d_keisu;
				else if( d_keisuL==0 )
				    d_keisuL = d_keisu;
			    }
			}
#if 0
			if( d_keisu==0 )
			{
			    fprintf( stdout, "d_keisu=%f : Error\n", d_keisu );
			    fprintf( stdout, "PCR0 = 0x%X, PCR1 = 0x%X\n",
			    	PCR0, PCR1 );
			    double d_diff = d_keisu-d_keisu0;
			    int diff = (int)(d_diff*100000);
			    fprintf( stdout, "d_diff=%f(%d)\n", d_diff, diff );
			    exit( 1 );
			}
#endif
			if( nCR==0 )
			{
			    if( L_PCR_addr>0 )
			    {
				double d_offset = PCR_addr[i]-L_PCR_addr;
				d_offset = d_offset*d_keisuL;
			    	fprintf( stdout, "%8X (%4.8f)\n",
					PCR_addr[i]-L_PCR_addr, d_offset );
			    } else
				fprintf( stdout, "\n" );
			}
//			if( bNoCont==0 )
			    d_keisu0 = d_keisu;
		    } else
		    fprintf( stdout, "%4X : %8X,%09llX(%8X:%8X)\n",
			    PCR_PID[i],
			    PCR_addr[i],
			    PCR_LL, LL_H, LL_L );
		    L_PCR_addr = PCR_addr[i];
		}
	    }
	    if( pids[0]>=0 )
	    {
		fprintf( stdout, "pcr_count=%d\n", pcr_count );
		fprintf( stdout, "d_keisuL=%4.8f\n", d_keisuL );
		if( d_keisuL>0 )
		{
		    double mbps = 90000*8;
		    mbps = mbps/d_keisuL;
		    fprintf( stdout, "%d Kbps\n", (int)mbps/1000 );
		}
	    } else {
		fprintf( stdout, "No pid specified\n" );
	    }
	    }
	    if( mode==MODE_PCR_UPDATE )
	    {
	    	if( PcrKbps>0 )
		{
		    d_keisuL = 90000*8;
		    d_keisuL = d_keisuL/PcrKbps;
		    d_keisuL = d_keisuL/1000;
//		    d_keisuL = d_keisuL/1000;
		} else {
		    d_keisuL = d_keisuL*PcrGain/200;
		}
		fprintf( stdout, "d_keisuL = %f\n", d_keisuL );
		nPCR = TsDump( srcFilename, outFilename, directory, 
			pids, nPid, -1,
			PCR_PID, PCR_addr, PCR_value );
	    } else if( mode==MODE_TS2TTS )
	    {
	    	if( TsPacketSize!=188 )
		{
		    fprintf( stdout, "TsPacketSize=%d\n", TsPacketSize );
		    exit( 1 );
		}
	    	FILE *ts_fp = NULL;
		unsigned char bytes[1024*4];
	    	fprintf( stdout, "PCR0  = %lld\n", PCR0 );
		fprintf( stdout, "addr0 = 0x%X\n", addr0 );
		ts_fp = fopen ( srcFilename, "rb" );
		if( ts_fp==NULL )
		{
		    fprintf( stdout, "Can't open %s\n", srcFilename );
		    exit( 1 );
		}
		FILE *fp_write = fopen( outFilename, "wb" );
		if( fp_write==NULL )
		{
		    fprintf( stdout, "Can't open %s\n", outFilename );
		    exit( 1 );
		}
		g_addr=0;
		int ReadOK, readed;
		unsigned char pcr_buf[4];
		memset( pcr_buf, 0, 4 );
		for( i=0; ; i++ )
		{
		    unsigned int Addr = g_addr;
		    ReadOK = gread( bytes, TsPacketSize, 1, ts_fp );
		    if( ReadOK<1 )
		    {
			fprintf( stdout, "EOF@%d(addr=0x%llX,%lld)\n", 
				i, g_addr, g_addr );
			break;
		    } else {
			readed = TsPacketSize;
		    }
		    double pcr_d = Addr;
		    pcr_d = pcr_d*d_keisuL;
		    unsigned int new_pcr = (unsigned int)pcr_d;
		    pcr_buf[0] = (new_pcr>>24)&0xFF;
		    pcr_buf[1] = (new_pcr>>16)&0xFF;
		    pcr_buf[2] = (new_pcr>> 8)&0xFF;
		    pcr_buf[3] = (new_pcr>> 0)&0xFF;
		    fwrite( pcr_buf, 1, 4, fp_write );
		    fwrite( bytes, 1, TsPacketSize, fp_write );
		}
		fclose( ts_fp );
		fclose( fp_write );
	    }
	    break;
	case MODE_CUT_END :
	    nPCR = TsDump( srcFilename, outFilename, directory, pids, nPid, 2,
			PCR_PID, PCR_addr, PCR_value );
	    nPCR = TsDump( srcFilename, outFilename, directory, pids, nPid, -2,
			PCR_PID, PCR_addr, PCR_value );
	    break;
	case MODE_PMT :
	    TsPMT( srcFilename, pids, nPid );
	    break;
	case MODE_PAT :
	    TsPAT( srcFilename );
	    break;
	case MODE_PID :
	    TsPid( srcFilename );
	    break;
	case MODE_CUT :
	    TsCut( srcFilename, outFilename, start, end, len );
	    break;
	case MODE_OFF :
	    TsOff( srcFilename, outFilename, pids );
	    break;
	case MODE_TEST :
	    TsTest( srcFilename, outFilename, start, pids[0] );
	    break;
	default :
	    fprintf( stdout, "Unknown mode (%d)\n", mode );
	    exit( 1 );
	}
	if( argc==1) {
	} else {
	}
	return 0;
}

#ifndef MAIN
int bDebug=0;
int main( int argc, char *argv[] )
{
	return ts( argc, argv );
}
#endif

