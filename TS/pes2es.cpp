
/*
	pes2es.cpp
		    2012.11.30	pes-es.cpp -> pes2es.cpp
		    2013. 3.27 	Support 00 00 01 E0 -> 00 00 01 EX
		    		+D : display PES_header
				+C : check delta DTS
		    2013. 7.30  00 00 00 01 : output support
		    		use nZero
 */

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// memset

#include "tsLib.h"
#include "parseLib.h"

#include "main.h"

void pes2es_Usage( char *prg )
{
	fprintf( stdout, "%s filename [=AVS] [+Disp] [+CdeltaDTS]\n", prg );
	exit( 1 );
}

int pes2es( int argc, char *argv[] )
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
int bAVS=0;
int bOutputPES=0;
int bForcePesSizeZero=0;

	memset( header, 0, 1024 );
	sprintf( outfile, "outfile.bin" );
	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
		switch( argv[i][1] )
		{
		default :
		    pes2es_Usage( argv[0] );
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
		    pes2es_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='=' )
	    {
		switch( argv[i][1] )
		{
		case 'A' :
		    bAVS=1;
		    break;
		case 'P' :
		    bOutputPES=1;
		    break;
		case 'Z' :
		    bForcePesSizeZero=1;
		    break;
		default :
		    pes2es_Usage( argv[0] );
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
		    pes2es_Usage( argv[0] );
		    break;
		}
		args++;
	    }
	}
	if( filename==NULL )
	{
	    pes2es_Usage( argv[0] );
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
	int nZero=0;
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
		    nZero=0;
		    if( buffer[i]==0x00 )
		    {
		    	state++;
			nZero++;
		    } else {
		    	if( bStart )
		    	fprintf( wp, "%c", buffer[i] );
		    }
		    break;
		case 1 :	// 00
		    if( buffer[i]==0x00 )
		    {	// 00 00
		    	state++;
			nZero++;
		    } else {	// 00 XX
			state=0;
		    	if( bStart )
		    	fprintf( wp, "%c%c", 0x00, buffer[i] );
		    }
		    break;
		case 2 :	// 00 00
		    if( buffer[i]==0x01 )
		    {	// 00 00 .. 01
		    	state++;
		    } else if( buffer[i]==0x00 )
		    {	// 00 00 00
			nZero++;
		    } else {	// 00 00 .. XX
			state=0;
		    	if( bStart )
			{
			    int n;
			    for( n=0; n<nZero; n++ )
			    {
				fprintf( wp, "%c", 0x00 );
			    }
			    fprintf( wp, "%c", buffer[i] );
			}
		    }
		    break;
		case 3 :	// 00 00 01
//		    if( (buffer[i]>=0xE0) && (buffer[i]<=0xFF) )
		    if( (buffer[i]>=0xE0) )
		    {
			memset( header, 0, 1024 );
			head_addr = g_addr-readed+i-3;
			header[0] = 0;
			header[1] = 0;
			header[2] = 1;
			header[3] = buffer[i];
			header_pos=4;
		    	state++;
		    } else {
			state=0;
			if( bAVS==0 )
			    bStart=1;
			if( buffer[i]==0xB0 )	// for AVS
			{
			    bStart=1;
#if 0
			    if( bAVS )
			    fprintf( wp, "%c", 0x00 );
#endif
			}
			    int n;
			    for( n=0; n<nZero; n++ )
			    {
				fprintf( wp, "%c", 0x00 );
			    }
			    fprintf( wp, "%c", 0x01 );
			    fprintf( wp, "%c", buffer[i] );
		    }
		    break;
		// -----------------------
		case 4 :	// 00 00 01 EX
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
			    if( bDisplay )
			    {
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
#if 1
				fprintf( stdout, " :%02X,%02X,%02X,%02X:", 
				header[4], header[5], header[6], header[7] );
#else
				fprintf( stdout, " : "  );
#endif
				if( l_head_addr<0 )
				{
				    fprintf( stdout, "L=%6d,   -  :", 
					pes_len );
				} else {
				    fprintf( stdout, "L=%6d,%6d:" ,
					pes_len, head_addr-l_head_addr );
				}
			    }
//			    if( bStart )
			    if( PTSL!=0xFFFFFFFF )
//			    if( (PTSL-DTSL)==3600 )
			    if( bOutputPES )
			    {
				fwrite( header, 1, header_pos, wp );
//    fprintf( stdout, "len=%d, write header(%d)\n", pes_len, header_pos );
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
#if 0
					   "*** Error(%d) ***\n", delta );
#else
	   "%d->%d:%d **\n", L_DTS, DTS, delta );
#endif
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
#if 0
		case 5 :
		    if( buffer[i]==0x00 )
		    {
		    	state++;
		    } else {
		    	state=4;
		    }
		    break;
		case 6 :
		    if( buffer[i]==0x01 )
		    {
		    	state++;
		    } else if( buffer[i]==0x00 )
		    {
			if( bStart )
		    	fprintf( wp, "%c", 0x00 );
		    } else {
		    	state=4;
		    }
		    break;
		case 7 :
		    fprintf( stdout, "00 00 01 %02X\n", buffer[i] );
		    if( bStart )
		    fprintf( wp, "%c%c%c%c", 0x00, 0x00, 0x01, buffer[i] );
		    state=0;
		    break;
#endif
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
    return pes2es( argc, argv );
}
#endif

