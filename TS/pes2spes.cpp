
/*
	pes2spes.cpp
		    2013. 7.17	
 */

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// memset

#include "tsLib.h"
#include "parseLib.h"

#include "main.h"

void pes2spes_Usage( char *prg )
{
	fprintf( stdout, "%s filename [=AVS] [+Disp] [+CdeltaDTS]\n", prg );
	exit( 1 );
}

#define MODE_NONE	 0
#define MODE_NON_IDR	 1
#define MODE_IDR	 5
#define MODE_SEI	 6
#define MODE_SPS	 7
#define MODE_PPS	 8
#define MODE_AUD	 9
#define MODE_EOSQ	10
#define MODE_EOST	11
#define MODE_FILLER	12

unsigned int g_PTS = 0xFFFFFFFF;
unsigned int g_DTS = 0xFFFFFFFF;

int pes2spes( int argc, char *argv[] )
{
char *filename=NULL;
int i;
int args=0;
unsigned char buffer[1024];
unsigned char header[1024];
int readed;
int total=0;
int state=0;
char outfile[1024];
int header_pos=0;
int bDisplay=0;
unsigned int L_DTS=0xFFFFFFFF;
int nDelta=0;
int bStart=0;
int bOutputPES=0;
int bForcePesSizeZero=0;
int mode = MODE_NONE;
unsigned char *PES_buffer;
unsigned char *SPES_buffer;
int bufPos=0;
int spesPos=0;

	PES_buffer = (unsigned char *)malloc( 1024*1024 );
	if( PES_buffer==NULL )
	{
	    fprintf( stdout, "Can't alloc PES_buffer\n" );
	    exit( 1 );
	}
	SPES_buffer = (unsigned char *)malloc( 1024*1024 );
	if( SPES_buffer==NULL )
	{
	    fprintf( stdout, "Can't alloc SPES_buffer\n" );
	    exit( 1 );
	}
	memset( header, 0, 1024 );
	sprintf( outfile, "outfile.spes" );
	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
		switch( argv[i][1] )
		{
		default :
		    pes2spes_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
		switch( argv[i][1] )
		{
		case 'D' :
		    bDisplay = 1;
		    break;
		case 'C' :
		    nDelta = atoi( &argv[i][2] );
		    break;
		default :
		    pes2spes_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='=' )
	    {
		switch( argv[i][1] )
		{
		case 'P' :
		    bOutputPES=1;
		    break;
		case 'Z' :
		    bForcePesSizeZero=1;
		    break;
		default :
		    pes2spes_Usage( argv[0] );
		    break;
		}
	    } else {
		switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		case 1 :
		    sprintf( outfile, "%s", argv[i] );
		    break;
		default :
		    pes2spes_Usage( argv[0] );
		    break;
		}
		args++;
	    }
	}
	if( filename==NULL )
	{
	    pes2spes_Usage( argv[0] );
	}
	FILE *fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open [%s]\n", filename );
	    exit( 1 );
	}

	FILE *wp = fopen( outfile, "wb" );
	if( wp==NULL )
	{
	    fprintf( stdout, "Can't write [%s]\n", outfile );
	    exit( 1 );
	}
	int head_addr=0;
	int l_head_addr=(-1);
	int nSEI=0;
	while( 1 )
	{
	    readed = gread( buffer, 1, 1024, fp );
	    if( readed<1 )
	    {
//	    	fprintf( stdout, "EOF(0x%X)\n", total );
		break;
	    }
	    if( bDisplay==0 )
	    {
	    	if( (g_addr-readed)%0x100000==0 )
		    fprintf( stdout, "%8llX\n", g_addr-readed );
	    }
	    total+=readed;
	    for( i=0; i<readed; i++ )
	    {
	    	switch( state )
		{
		case 0 :
		    switch( mode )
		    {
		    case MODE_SEI :
			if( nSEI<2 )
			fprintf( stdout, "SEI(%02X)\n", buffer[i] );
			nSEI++;
			break;
		    }
		    if( buffer[i]==0x00 )
		    {
		    	state++;
		    } else {
		    	if( bStart )
			{
			PES_buffer[bufPos++] = buffer[i];
			}
		    }
		    break;
		case 1 :	// 00
		    if( buffer[i]==0x00 )
		    {
		    	state++;
		    } else {	// 00 XX
			state=0;
		    	if( bStart )
			{
			PES_buffer[bufPos++] = 0x00;
			PES_buffer[bufPos++] = buffer[i];
			}
		    }
		    break;
		case 2 :	// 00 00
		    if( buffer[i]==0x01 )
		    {
		    	state++;
		    } else if( buffer[i]==0x00 )
		    {
		    	if( bStart )
			{
//			PES_buffer[bufPos++] = 0x00;
			}
		    } else {
			state=0;
		    	if( bStart )
			{
			PES_buffer[bufPos++] = 0x00;
			PES_buffer[bufPos++] = 0x00;
			PES_buffer[bufPos++] = buffer[i];
			}
		    }
		    break;
		case 3 :	// 00 00 01
		    if( (bufPos>0) && (mode!=MODE_NONE) )
		    {
//		    	fprintf( stdout, "bufPos=%d\n", bufPos );
			if( mode==MODE_SPS )
			{
			    fprintf( stdout, "Write SPS\n" );
			    fprintf( wp, "%c%c%c%c",
			    	((bufPos+18)>>24)&0xFF,
			    	((bufPos+18)>>16)&0xFF,
			    	((bufPos+18)>> 8)&0xFF,
			    	((bufPos+18)>> 0)&0xFF );
			    fprintf( wp, "%c%c%c%c", 0x31, 0x03, 0x00, 0x00 );
			    fprintf( wp, "%c%c%c%c", 0x00, 0x00, 0x00, 0x00 );
			    fprintf( wp, "%c%c%c%c", 0x00, 0x00, 0x00, 0x00 );
			    fprintf( wp, "%c%c",
			    	((bufPos+0)>> 8)&0xFF,
			    	((bufPos+0)>> 0)&0xFF );
			    fwrite( PES_buffer, 1, bufPos, wp );
			    spesPos = 0;
			} else if( mode==MODE_PPS ) {
			    fprintf( stdout, "Write PPS\n" );
			    fprintf( wp, "%c%c%c%c",
			    	((bufPos+18)>>24)&0xFF,
			    	((bufPos+18)>>16)&0xFF,
			    	((bufPos+18)>> 8)&0xFF,
			    	((bufPos+18)>> 0)&0xFF );
			    fprintf( wp, "%c%c%c%c", 0x32, 0x03, 0x00, 0x00 );
			    fprintf( wp, "%c%c%c%c", 0x00, 0x00, 0x00, 0x00 );
			    fprintf( wp, "%c%c%c%c", 0x00, 0x00, 0x00, 0x00 );
			    fprintf( wp, "%c%c",
			    	((bufPos+0)>> 8)&0xFF,
			    	((bufPos+0)>> 0)&0xFF );
			    fwrite( PES_buffer, 1, bufPos, wp );
			    spesPos = 0;
			} else {
#if 0
			fprintf( stdout, "Add SPES_buffer(%d,%d)\n", 
				spesPos, bufPos );
#endif
//			if( mode!=MODE_AUD )
			{
			SPES_buffer[spesPos++] = ((bufPos+0)>>24)&0xFF;
			SPES_buffer[spesPos++] = ((bufPos+0)>>16)&0xFF;
			SPES_buffer[spesPos++] = ((bufPos+0)>> 8)&0xFF;
			SPES_buffer[spesPos++] = ((bufPos+0)>> 0)&0xFF;
			}
			}
			switch( mode )
			{
			    break;
			case MODE_SPS :
			case MODE_PPS :
			    break;
			case MODE_AUD :
			case MODE_SEI :
			case MODE_IDR :
			case MODE_NON_IDR :
			case MODE_EOSQ :
			case MODE_EOST :
			case MODE_FILLER :
			    memcpy( &SPES_buffer[spesPos], PES_buffer, bufPos );
			    spesPos += bufPos;
			    break;
			default :
			    fprintf( stdout, "Unsupport(%02X)\n", mode );
			    exit( 1 );
			}
		    }
		    bufPos = 0;
		    if( (buffer[i]>=0xE0) )	// PES header
		    {
			memset( header, 0, 1024 );
			head_addr = g_addr-readed+i-3;
			header[0] = 0;
			header[1] = 0;
			header[2] = 1;
			header[3] = buffer[i];
			header_pos=4;
		    	state++;
			if( spesPos>0 )
			{
			    fprintf( stdout, "spesPos=%d\n", spesPos );
			    fprintf( wp, "%c%c%c%c",
			    	((spesPos+16)>>24)&0xFF,
			    	((spesPos+16)>>16)&0xFF,
			    	((spesPos+16)>> 8)&0xFF,
			    	((spesPos+16)>> 0)&0xFF );
			    fprintf( wp, "%c%c%c%c", 0x30, 0xC0, 0x00, 0x00 );
			    fprintf( wp, "%c%c%c%c", 
			    	(g_PTS>>24)&0xFF,
			    	(g_PTS>>16)&0xFF,
			    	(g_PTS>> 8)&0xFF,
			    	(g_PTS>> 0)&0xFF );
			    fprintf( wp, "%c%c%c%c", 
			    	(g_DTS>>24)&0xFF,
			    	(g_DTS>>16)&0xFF,
			    	(g_DTS>> 8)&0xFF,
			    	(g_DTS>> 0)&0xFF );
			    fwrite( SPES_buffer, 1, spesPos, wp );
			    spesPos = 0;
			}
			mode = MODE_NONE;
		    } else {
			state=0;
			fprintf( stdout, "%8llX : 00 00 01 %02X", 
				g_addr-readed+i-3, buffer[i] );
			mode = MODE_NONE;
			switch( buffer[i] )
			{
			case 0x01 :
			case 0x21 :
			case 0x41 :
			case 0x61 :
			    mode = MODE_NON_IDR;
			    fprintf( stdout, " : NonIDR" );
			    break;
			case 0x05 :
			case 0x25 :
			case 0x45 :
			case 0x65 :
			    mode = MODE_IDR;
			    fprintf( stdout, " : IDR" );
			    break;
			case 6 : // SEI
			    mode = MODE_SEI;
			    fprintf( stdout, " : SEI" );
			    nSEI=0;
			    break;
			case 0x07 :
			case 0x27 :
			case 0x47 :
			case 0x67 :
			    mode = MODE_SPS;
			    fprintf( stdout, " : SPS(%8X,%8X)", g_DTS, g_PTS );
			    break;
			case 0x0F :
			case 0x2F :
			case 0x4F :
			case 0x6F :
			    mode = MODE_SPS;
			    fprintf( stdout, " : SPS(%8X,%8X)", g_DTS, g_PTS );
			    break;
			case 0x08 :
			case 0x28 :
			case 0x48 :
			case 0x68 :
			    mode = MODE_PPS;
			    fprintf( stdout, " : PPS" );
			    break;
			case 0x09 :
			    mode = MODE_AUD;
			    fprintf( stdout, " : AUD" );
			    break;
			case 0x0A :
			    mode = MODE_EOSQ;
			    fprintf( stdout, " : EOSeq" );
			    break;
			case 0x0B :
			    mode = MODE_EOST;
			    fprintf( stdout, " : EOStm" );
			    break;
			case 0x0C :
			    mode = MODE_FILLER;
			    fprintf( stdout, " : FILLER" );
			    break;
			// MVC
			case 0x18 :
			    mode = MODE_FILLER;
			    fprintf( stdout, " : VDRD" );
			    break;
			case 0x14 :
			case 0x34 :
			case 0x54 :
			case 0x74 :
			    mode = MODE_FILLER;
			    fprintf( stdout, " : ext" );
			    break;
			//
			default :
			    fprintf( stdout, "?????" );
			    exit( 1 );
			    break;
			}
			fprintf( stdout, "\n" );
			PES_buffer[bufPos++] = buffer[i];

			bStart=1;
		    }
		    break;
		// -----------------------
		case 4 :	// PES_header
		    if( header_pos<1024 )
		    {
			header[header_pos++] = buffer[i];
		    }
		    if( header_pos>=9 )
		    {
			int len = header[8];
			if( header_pos>(len+8) )
			{
			    state = 0;
			    unsigned int PTSH, PTSL, DTSH, DTSL;
			    unsigned int PTS=0xFFFFFFFF;
			    unsigned int DTS=0xFFFFFFFF;
			    PTSL=0xFFFFFFFF;
			    if( bForcePesSizeZero )
			    {
				header[4] = 0;
				header[5] = 0;
			    }
			    int offset;
			    int pes_len = parsePES_header(
				&header[4], 0,
				&PTSH, &PTSL, &DTSH, &DTSL, &offset );
			    if( PTSL!=0xFFFFFFFF )
			    {
				PTS=PTSL;
				DTS=DTSL;
				if( DTS==0xFFFFFFFF )
				    DTS=PTS;
				g_PTS = PTS;
				g_DTS = DTS;
			    }
			    if( bDisplay )
			    {
				if( PTSL!=0xFFFFFFFF )
				{
				    PTS=PTSL;
				    DTS=DTSL;
				    if( DTS==0xFFFFFFFF )
					DTS=PTS;
				    int dh, dm, ds, dms;
				    int ph, pm, ps, pms;
				    TcTime32( 0, DTS, &dh, &dm, &ds, &dms );
				    TcTime32( 0, PTS, &ph, &pm, &ps, &pms );
				    fprintf( stdout, 
		"addr=%8X : DTS=%2d:%02d:%02d.%03d PTS=%2d:%02d:%02d.%03d",
		    head_addr, dh, dm, ds, dms, ph, pm, ps, pms );
				} else {
				    fprintf( stdout, 
		"addr=%8X : DTS=--:--:--.--- PTS=--:--:--.---",
					head_addr );
				}
				fprintf( stdout, " :%02X,%02X,%02X,%02X:", 
				header[4], header[5], header[6], header[7] );
				if( l_head_addr<0 )
				{
				    fprintf( stdout, "L=%6d,   -  :", 
					pes_len );
				} else {
				    fprintf( stdout, "L=%6d,%6d:" ,
					pes_len, head_addr-l_head_addr );
				}
			    }
			    if( PTSL!=0xFFFFFFFF )
			    if( bOutputPES )
			    {
#if 0
				fwrite( header, 1, header_pos, wp );
#endif
			    }
			    l_head_addr = head_addr;
			// -------------------------------
			    if( PTSL!=0xFFFFFFFF )
			    {
				if( L_DTS!=0xFFFFFFFF )
				{
				    int delta = DTS-L_DTS;
				    int error=0;
				    if( nDelta>0 )
				    {
					if( delta!=nDelta )
					    error=1;
				    } else if( nDelta<0 )
				    {
					if( (delta<+nDelta*5) 
					 || (delta>-nDelta*5) )
					     error=1;
				    }
				    if( error )
				    {
				       fprintf( stdout, 
	   "%d->%d:%d **\n", L_DTS, DTS, delta );
				    } else {
					if( bDisplay )
					fprintf( stdout, "\n" );
				    }
				} else {
				    if( bDisplay )
					fprintf( stdout, "\n" );
				}
				L_DTS=DTS;
			    } else {
				    if( bDisplay )
					fprintf( stdout, "\n" );
			    }
			}
		    }
		    break;
		}
	    }
	}
	fclose( fp );
	fclose( wp );
	fprintf( stdout, "%s generated\n", outfile );
	return 0;
}

#ifndef MAIN
int bDebug=0;

int main( int argc, char *argv[] )
{
	return pes2spes( argc, argv );
}
#endif

