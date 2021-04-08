
/*
	pesEdit.cpp
		2013.1.X

		2013.3.7 use bWrite and from/to
		2013.3.14 -D : deltaDTS
		2013.4.23 print minPTS/maxPTS
		2013.12.5 Support field structure
			  use pesBody : to skip next pes_header
		2013.12.12 fix PTS/DTS constant bit field
*/

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// memcpy

#include "tsLib.h"
#include "parseLib.h"

#include "main.h"

#ifndef MAIN
int bDebug=0;
#endif

void pesEdit_Usage( char *prg )
{
	fprintf( stdout, "%s filename\n", prg );
	exit( 1 );
}

int pesEdit( int argc, char *argv[] )
{
int i;
int args=0;
FILE *infile = NULL;
FILE *outfile= NULL;
char *filename    = NULL;
char *outFilename = NULL;
unsigned char buffer[1024*4];
int deltaDTS=0;
unsigned int PTSH, PTSL, DTSH, DTSL;
unsigned int L_PTSH=0xFFFFFFFF, L_PTSL=0xFFFFFFFF;
unsigned int L_DTSH=0xFFFFFFFF, L_DTSL=0xFFFFFFFF;
int bWrite=0;
int from=(-1);
int to  =(-1);
int sDTS=(-1);
int nFirstDelay=(-1);
int div=1;
int Offset=0;
int bForAllegro=0;

	for( i=1; i<argc; i++ )
	{
	    fprintf( stdout, "argv[%d]=%s\n", i, argv[i] );
	    if( argv[i][0]=='/' )
	    {
	    	switch( argv[i][1] )
		{
		case '1' :
		case '2' :
		case '3' :
		case '4' :
		case '5' :
		case '6' :
		case '7' :
		case '8' :
		case '9' :
		    div = atoi( &argv[i][1] );
		    fprintf( stdout, "div=%d\n", div );
		    if( div<1 )
		    	div=1;
		    break;
		default :
		    if( args==0 )
		    {
			filename = argv[i];
			args++;
		    } else {
			pesEdit_Usage( argv[0] );
		    }
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'A' :
		    fprintf( stdout, "bForAllegro=1\n" );
		    bForAllegro=1;
		    break;
		case 'O' :
		    Offset = atoi( &argv[i][2] );
		    fprintf( stdout, "Offset = %d\n", Offset );
		    break;
		default :
		    pesEdit_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case 'F' :
		    from = atoi( &argv[i][2] );
		    fprintf( stdout, "from = %d\n", from );
		    break;
		case 'T' :
		    to   = atoi( &argv[i][2] );
		    fprintf( stdout, "to   = %d\n", to   );
		    break;
		case 'D' :
		    deltaDTS = atoi( &argv[i][2] );
		    fprintf( stdout, "deltaDTS = %d\n", deltaDTS );
		    break;
		case 'd' :
		    nFirstDelay = atoi( &argv[i][2] );
		    fprintf( stdout, "nFirstDelay = %d\n", nFirstDelay );
		    break;
		default :
		    pesEdit_Usage( argv[0] );
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
	    pesEdit_Usage( argv[0] );

	infile = fopen( filename, "rb" );
	if( infile==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}
	if( outFilename )
	{
	    outfile = fopen( outFilename, "wb" );
	    if( outfile==NULL )
	    {
		fprintf( stdout, "Can't open %s\n", outFilename );
		exit( 1 );
	    }
	}
	int nPES_header=0;
	int readed;
	int readP=0;
	unsigned int nDTS = 0xFFFFFFFF;
	unsigned int nPTS = 0xFFFFFFFF;
	int firstDelay=(-1);
	unsigned int cDTSL = 0xFFFFFFFF;
	unsigned int cDTSH = 0xFFFFFFFF;
	unsigned int minPTS = 0xFFFFFFFF;
	unsigned int maxPTS = 0;
	unsigned int L_wDTS = 0xFFFFFFFF;
	int pesBody=0;
	while( 1 )
	{
	    readed = gread( &buffer[readP], 1, 4-readP, infile );
	    if( readed<1 )
	    	break;
#if 0
	    fprintf( stdout, "%d bytes readed : ", readed );
	    fprintf( stdout, "%02X %02X %02X %02X\n",
	    	buffer[0], buffer[1], buffer[2], buffer[3] );
#endif
	    int bData=0;
	    pesBody -= readed;
	    if( pesBody<0 )
	    	pesBody=0;
	    if( pesBody==0 )
	    {
		if( (buffer[0]==0x00)
		 && (buffer[1]==0x00)
		 && (buffer[2]==0x01)) 
		{
		    if( (buffer[3]>=0xE0)
		     && (buffer[3]<=0xEF) )
			bData=1;
		    if( (buffer[3]>=0xC0)
		     && (buffer[3]<=0xCF) )
			bData=1;
		    if( (buffer[3]==0xBD) )
			bData=1;
		}
	    } else {
//	    	fprintf( stdout, "%8X : skip\n", g_addr );
	    }
	    if( bData )
	    {
//	    	fprintf( stdout, "%8llX : bData(%02X)\n", g_addr, buffer[3] );
	    	int h_addr = g_addr-4;
	    	int bufOffset=4;
		int PES_len = PES_header( infile, &buffer[bufOffset], 0,
		    &PTSH, &PTSL, &DTSH, &DTSL );
		if( PES_len>0 )
		    pesBody=PES_len-4;
		int hour, min, sec, msec;
		TcTime32( PTSH, PTSL, &hour, &min, &sec, &msec );
		if( PTSH==0xFFFFFFFF )
		{	// No PTS/DTS
		} else {
		    if( DTSH==0xFFFFFFFF )
		    {
			cDTSL = PTSL;
			cDTSH = PTSH;
		    } else {
			cDTSL = DTSL;
			cDTSH = DTSH;
		    }
		}
		int hour2, min2, sec2, msec2;
		TcTime32( cDTSH, cDTSL, &hour2, &min2, &sec2, &msec2 );
		if( PTSH!=0xFFFFFFFF )
		{
		    fprintf( stdout, 
"%8X PES_header(%2d:%02d:%02d.%03d, %2d:%02d:%02d.%03d, %4X:%08X,%4X:%08X)\n",
			h_addr,
			    hour2, min2, sec2, msec2,
			    hour, min, sec, msec,
			    DTSH&0xFFFF, DTSL,
			    PTSH, PTSL ); 
		    if( PTSL<minPTS )
		    	minPTS=PTSL;
		    if( PTSL>maxPTS )
		    	maxPTS=PTSL;
		    if( firstDelay<0 )
		    {
			firstDelay = PTSL-cDTSL;
			fprintf( stdout, "firstDelay=%d\n", firstDelay );
		    }
		    int curDeltaDTS = 0;
		    if( L_DTSL!=0xFFFFFFFF )
		    	curDeltaDTS = cDTSL-L_DTSL;
		    if( deltaDTS==0 )
		    {
			if( L_DTSL!=0xFFFFFFFF )
			{
			    deltaDTS = cDTSL-L_DTSL;
			    fprintf( stdout, "deltaDTS=%d\n", deltaDTS );
			}
		    }
		    if( sDTS==(-1) )
			sDTS = cDTSL;
		    if( bForAllegro )
		    {
			if( nDTS!=0xFFFFFFFF )
			    nDTS = nDTS + curDeltaDTS;
			else
			    nDTS = cDTSL;
		    } else {
			if( deltaDTS>0 )
			{
			    if( nDTS!=0xFFFFFFFF )
			    {
				if( cDTSL>L_DTSL )
				{
    // 2013.5.9 comment out for error stream
    //				deltaDTS = cDTSL-L_DTSL;
				}
				nDTS = nDTS+deltaDTS;
			    } else {
				nDTS = cDTSL;
			    }
			} else {
				nDTS = cDTSL;
			}
		    }
		}
//		fprintf( stdout, "deltaDTS=%d\n", deltaDTS );
//		fprintf( stdout, "cDTSL=%8X,PTSL=%8X\n", cDTSL, PTSL );
		nPTS = nDTS+(PTSL-cDTSL);
		// -----------------------------------------------
		int bFrom = 0;
		int bTo   = 0;
		if( from==(-1) )
		    bFrom = 1;
		else if( (cDTSL-sDTS)>=from )
		    bFrom = 1;
		if( to  ==(-1) )
		    bTo   = 1;
		else if( (cDTSL-sDTS)<=to   )
		    bTo   = 1;
		if( bFrom & bTo )
		    bWrite = 1;
		else
		    bWrite = 0;
#if 0
		fprintf( stdout, "From=%d, bTo=%d, bWrite=%d : %8d\n",
			bFrom, bTo, bWrite, cDTSL-sDTS );
#endif
		// -----------------------------------------------
		readP=0;
		nPES_header++;
		if( bWrite )
		if( outfile )
		{
#ifdef DEBUG
		int ii;
		for( ii=0; ii<16; ii++ )
		{
		    fprintf( stdout, "%02X ", buffer[ii] );
		}
		fprintf( stdout, "\n" );
#endif
		    int offset=bufOffset+5;
		    int wPTS = (nPTS-sDTS)/div+Offset;
		    int wDTS = (nDTS-sDTS)/div+Offset;
		    int PES_header_data_len = buffer[bufOffset+4];
		    if( nFirstDelay<0 )
		    {
#ifdef DEBUG
			fprintf( stdout, "wDTS=%8d, wPTS=%8d\n", wDTS, wPTS );
			fprintf( stdout, "PTSH=%d, DTSH=%d\n", PTSH, DTSH );
			fprintf( stdout, "%02X\n",(PTSH<<3) | ((wPTS>>30)<<1) );
#endif
			if( PTSH!=0xFFFFFFFF )
			{
			    buffer[offset+0] = (PTSH<<3) | ((wPTS>>30)<<1);
			    buffer[offset+1] = (wPTS>>22);
			    buffer[offset+2] = (wPTS>>15)<<1;
			    buffer[offset+3] = (wPTS>> 7)<<0;
			    buffer[offset+4] = (wPTS    )<<1;
	
			    if( DTSH!=0xFFFFFFFF )
			    buffer[offset+0] |= 0x31;	// 2013.12.12
			    else
			    buffer[offset+0] |= 0x21;	// 2013.12.12
			    buffer[offset+2] |= 0x01;	// 2013.12.5
			    buffer[offset+4] |= 0x01;	// 2013.12.5
			    offset += 5;
			}
			if( DTSH!=0xFFFFFFFF )
			{
			    buffer[offset+0] = (DTSH<<3) | ((wDTS>>30)<<1);
			    buffer[offset+1] = (wDTS>>22);
			    buffer[offset+2] = (wDTS>>15)<<1;
			    buffer[offset+3] = (wDTS>> 7)<<0;
			    buffer[offset+4] = (wDTS    )<<1;

			    buffer[offset+0] |= 0x11;	// 2013.12.12
			    buffer[offset+2] |= 0x01;	// 2013.12.5
			    buffer[offset+4] |= 0x01;	// 2013.12.5
			    offset += 5;
			}
#ifdef DEBUG
		int ii;
		for( ii=0; ii<16; ii++ )
		{
		    fprintf( stdout, "%02X ", buffer[ii] );
		}
		fprintf( stdout, "\n" );
#endif
			fwrite( &buffer[0], 1, 4, outfile );
			fwrite( &buffer[bufOffset], 
			    1, 5+PES_header_data_len, outfile );
#if 0
			fprintf( stdout, 
			    "PES_len=%d, PES_header_data_len=%d\n",
			    PES_len, PES_header_data_len );
#endif
		    } else {
			wPTS = (nPTS-sDTS)+(nFirstDelay-firstDelay)+Offset;
			wDTS = (nDTS-sDTS)+Offset;
			if( wDTS>(L_wDTS+7200) )
			{
			fprintf( stdout, "L_wDTS=%8X,wDTS=%8X\n",
				L_wDTS, wDTS );
			}
		    	if( PTSH!=0xFFFFFFFF )
			{
//			    PES_len=0;	// 2013.12.5 comment out
			    PES_header_data_len=10;
			    unsigned char pesHeader[188];
			    pesHeader[ 0] = buffer[0];		// 00
			    pesHeader[ 1] = buffer[1];		// 00
			    pesHeader[ 2] = buffer[2];		// 01
			    pesHeader[ 3] = buffer[3];		// E*

			    pesHeader[ 4] = (PES_len>>8)&0xFF;
			    pesHeader[ 5] = (PES_len>>0)&0xFF;
			    pesHeader[ 6] = buffer[6];		// flag1
			    pesHeader[ 7] = buffer[7] | 0xC0;	// flag2
			    pesHeader[ 8] = PES_header_data_len;

			    pesHeader[ 9] = (PTSH<<3) | ((wPTS>>30)<<1);
			    pesHeader[10] = (wPTS>>22);
			    pesHeader[11] = (wPTS>>15)<<1;
			    pesHeader[12] = (wPTS>> 7)<<0;
			    pesHeader[13] = (wPTS    )<<1;

			    pesHeader[ 9] |= 0x21;
			    pesHeader[11] |= 0x01;
			    pesHeader[13] |= 0x01;

			    pesHeader[14] = (0<<3) | ((wDTS>>30)<<1);
			    pesHeader[15] = (wDTS>>22);
			    pesHeader[16] = (wDTS>>15)<<1;
			    pesHeader[17] = (wDTS>> 7)<<0;
			    pesHeader[18] = (wDTS    )<<1;

			    pesHeader[14] |= 0x21;
			    pesHeader[16] |= 0x01;
			    pesHeader[18] |= 0x01;

			    fwrite( &pesHeader[0], 1, 4, outfile );
			    fwrite( &pesHeader[4], 
				1, 5+PES_header_data_len, outfile );
//			    fprintf( stdout, "wDTS=%8d,wPTS=%8d\n", wDTS,wPTS);
			}
			L_wDTS = wDTS;
		    }
		    int d_hour, d_min, d_sec, d_msec;
		    int p_hour, p_min, p_sec, p_msec;
		    TcTime32(     0, wDTS, &d_hour, &d_min, &d_sec, &d_msec );
		    TcTime32(  PTSH, wPTS, &p_hour, &p_min, &p_sec, &p_msec );
		    fprintf( stdout, 
		    "----------(%2d:%02d:%02d.%03d, %2d:%02d:%02d.%03d)\n",
			    d_hour, d_min, d_sec, d_msec,
			    p_hour, p_min, p_sec, p_msec );
		}
		if( DTSH==0xFFFFFFFF )
		{
		    L_DTSH = PTSH;
		    L_DTSL = PTSL;
		} else {
		    L_DTSH = DTSH;
		    L_DTSL = DTSL;
		}
		L_PTSH = PTSH;
		L_PTSL = PTSL;
	    } else {
	    	readP=3;
	    	if( bWrite )
		if( outfile )
		    fwrite( &buffer[0], 1, 1, outfile );
		memcpy( &buffer[0], &buffer[1], 3 );
	    }
	}
	if( bWrite )
	if( outfile )
	    fwrite( &buffer[0], 1, readP, outfile );
	fclose( infile );
	if( outfile )
	    fclose( outfile );
	fprintf( stdout, "%d PES_header found\n", nPES_header );
	if( minPTS!=0xFFFFFFFF )
	    fprintf( stdout, "minPTS=%08X\n", minPTS );
	if( maxPTS!=0x00000000 )
	    fprintf( stdout, "maxPTS=%08X\n", maxPTS );

	return 0;
}

#ifndef MAIN
int main( int argc, char *argv[] )
{
	return pesEdit( argc, argv );
}
#endif

