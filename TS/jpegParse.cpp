
/*
	jpeg.cpp
	jpegParse.cpp
			2012.1.14 by T.Oda
			2012.2.23 : SOF0 parse
			2012.7.4  : APPX 
			2012.10.10 : jpegPasre.cpp
			2013. 8. 2 : APP-13, APP-14 : apple
			             RST0-7
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>	// memset

#include "parseLib.h"

int bDetail = 0;
int bDebug=0;

int jpegUsage( char *prg )
{
	fprintf( stdout, "%s filename [+D +d +Q +O]\n", prg );
	exit( 1 );
}

int jpeg( int argc, char *argv[] )
{
int i;
int args=0;
char *filename = NULL;
#define BUFFER_SIZE	1024*64
unsigned char buffer[BUFFER_SIZE];
int readed;
FILE *fp=NULL;
int total=0;
int phase=0;
unsigned int size=0;
int bQuantMB=0;
unsigned int EOI_Addr = 0;
unsigned int SOS_Addr = 0;
int bSOI=0;
int bEOI=0;
FILE *pOut=NULL;
char *outfile = NULL;
int bOut=0;
int bDHT=0;
unsigned char JpegBuf[1024*1024];	// Up to 1MB
int nJpegSize=0;

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		default :
		    fprintf( stdout, "Unknown parameter [%s]\n", argv[i] );
		    jpegUsage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'D' :
		    bDetail = 1;
		    break;
		case 'd' :
		    bDebug=1;
		    break;
		case 'Q' :
		    bQuantMB=1;
		    break;
		case 'O' :
		    bOut=1;
		    break;
		default :
		    fprintf( stdout, "Unknown parameter [%s]\n", argv[i] );
		    jpegUsage( argv[0] );
		    break;
		}
	    } else {
	    	switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		case 1 :
		    outfile = argv[i];
		    break;
		default :
		    jpegUsage( argv[0] );
		    break;
		}
		args++;
	    }
	}

	if( filename==NULL )
	{
	    jpegUsage( argv[0] );
	}

	fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open [%s]\n", filename );
	    exit( 1 );
	}
	if( bOut )
	{
	    pOut = fopen( outfile, "wb" );
	    if( pOut==NULL )
	    {
		fprintf( stdout, "Can't open [%s]\n", outfile );
		exit( 1 );
	    }
	}
static char APPstr[16][32] = {
"JFIF",		// APP0
"Exif",		// APP1
"ICC Profile",	// APP2
"?",		// APP3
"?",		// APP4
"?",		// APP5
"?",		// APP6
"?",		// APP7
"?",		// APP8
"?",		// APP9
"?",		// APP10
"?",		// APP11
"?",		// APP12
"?",		// APP13
"Adobe",	// APP14
"-"		// APP15
};
	while( 1 )
	{
	    if( phase>=BUFFER_SIZE )
	    {
	    	fprintf( stdout, "phase is too large(%d)\n", phase );
	    	exit( 1 );
	    }
	    readed = gread( &buffer[phase], 1, 1, fp );
	    if( readed<1 )
	    {
	    	fprintf( stdout, " : EOF@%X\n", total );
		exit( 0 );
	    }
	    total+=readed;

	    switch( phase )
	    {
	    case 0 :
	    	if( buffer[phase]==0xFF )
		{
		    phase++;
		} else {
//		    JpegBuf[nJpegSize++] = buffer[phase];
		}
		break;
	    case 1 :
	    	switch( buffer[phase] )
		{
		case 0xD8 :	// SOI
		    if( bDebug )
		    	fprintf( stdout, "A=0x%X:", total );
		    fprintf( stdout, "[SOI]" );
		    bSOI = 1;
		    bDHT = 0;
		    break;
		case 0xD0 :	// 
		case 0xD1 :	// 
		case 0xD2 :	// 
		case 0xD3 :	// 
		case 0xD4 :	// 
		case 0xD5 :	// 
		case 0xD6 :	// 
		case 0xD7 :	// 
		    fprintf( stdout, "[RST%d]", buffer[phase]-0xD0 );
		    if( buffer[phase]==0xD7 )
			fprintf( stdout, "\n" );
		    break;
		case 0xE0 :	// APP0
		case 0xE1 :	// APP1
		case 0xE2 :	// APP2
		case 0xED :	// APP13
		case 0xEE :	// APP14
		    if( bDebug )
		    	fprintf( stdout, "A=0x%X:", total );
		    fprintf( stdout, "[APP%d-%s]\n", 
		    	buffer[phase]-0xE0, APPstr[buffer[phase]-0xE0] );
		    readed = gread( &buffer[2], 1, 2, fp );
		    total+=readed;
		    size = (buffer[2]<<8) | buffer[3];
//		    fprintf( stdout, "(%02X,%02X)", buffer[2], buffer[3] );
		    if( (size>2) && (size<0x100) )
		    {
		    	char name[8];
			memset( name, 0, 8 );
			readed = gread( &buffer[4], 1, size-2, fp );
			total+=readed;
			memcpy( name, &buffer[4], 4 );
		    	fprintf( stdout, "Size=%04X (%s) ", size, name );
//			for( i=6; (i<size)&&(i<16); i++ )
			for( i=6; (i<size)&&(i<22); i++ )
			{
			    fprintf( stdout, "%02X ", buffer[2+i] );
			}
			fprintf( stdout, "\n" );
		    } else {
		    	fprintf( stdout, "APPX Size=%04X\n", size );
			int s;
			for( s=0; s<(size-2); s++ )
			{
			    readed = gread( &buffer[4], 1, 1, fp );
			    total+=readed;
			}
		    }
//		    phase+=size;
		    phase=0;
		    break;
		case 0xC0 :	// SOF0
		    fprintf( stdout, "[SOF0]" );
		    readed = gread( &buffer[2], 1, 2, fp );
		    total+=readed;
		    size = (buffer[2]<<8) | buffer[3];
		    if( (size>2) && (size<0x100) )
		    {
			readed = gread( &buffer[4], 1, size-2, fp );
			total+=readed;
			if( bDetail )
		    	fprintf( stdout, "Size=%04X\n", size );
			int P  =  buffer[4];
			int Y  = (buffer[5]<<8) | buffer[6];
			int X  = (buffer[7]<<8) | buffer[8];
			int Nf =  buffer[9];
			if( bDetail )
			{
			fprintf( stdout, "P = %d\n", P );
			fprintf( stdout, "X = %d\n", X );
			fprintf( stdout, "Y = %d\n", Y );
			fprintf( stdout, "Nf= %d\n", Nf);
			}
			for( i=0; i<Nf; i++ )
			{
			    int Cn = buffer[10+i*3+0];
			    int HV = buffer[10+i*3+1];
			    int Tn = buffer[10+i*3+2];
			    if( bDetail )
			    fprintf( stdout, "Cn=%02X HV=%02X Qn=%02X\n",
			    	Cn, HV, Tn );
			}
		    } else {
		    	fprintf( stdout, "SOF0 Size=%04X\n", size );
		    }
		    break;
		case 0xC1 :	// SOF1
		    fprintf( stdout, "[SOF1]" );
		    break;
		case 0xC4 :	// DHT
		    fprintf( stdout, "[DHT]" );
		    bDHT=1;
		    break;
		case 0xD9 :	// EOI
		    if( bDebug )
		    	fprintf( stdout, "A=0x%X:", total );
		    fprintf( stdout, "[EOI]" );
		    if( bQuantMB )
		    	fprintf( stdout, "%llX", g_addr );
		    fprintf( stdout, "\n" );
		    EOI_Addr = g_addr;
		    if( bSOI )
		    if( bQuantMB )
		    {
		    	fprintf( stdout, "MCUSize=%6X (SOS@%07X,EOI@%07X)\n",
				EOI_Addr-SOS_Addr, SOS_Addr, EOI_Addr );
		    }
		    bEOI = 1;
		    break;
		case 0xDA :	// SOS
		    fprintf( stdout, "[SOS]" );
		    if( bQuantMB )
		    	fprintf( stdout, "%llX", g_addr );
		    SOS_Addr = g_addr;
		    break;
		case 0xDB :	// DQT
		    fprintf( stdout, "[DQT]" );
		    break;
		case 0xDD :	// DRI
		    fprintf( stdout, "[DRI]" );
		    break;
		case 0x00 :	// data 0xFF
		    break;
		case 0xFE :	// Comment
		    fprintf( stdout, "[COM]" );
		    readed = gread( &buffer[2], 1, 2, fp );
		    total+=readed;
		    size = (buffer[2]<<8) | buffer[3];
		    if( (size>2) && (size<0x100) )
		    {
			readed = gread( &buffer[4], 1, size-2, fp );
			total+=readed;
		    	fprintf( stdout, "Size=%04X (", size );
			for( i=2; (i<size); i++ )
			{
			    if( (buffer[2+i]>=' ') && (buffer[2+i]<=0x7F) )
				fprintf( stdout, "%c", buffer[2+i] );
			    else if( buffer[2+i]==0x0A )
				fprintf( stdout, "[%02X]", buffer[2+i] );
			    else
				fprintf( stdout, "?" );
			}
			fprintf( stdout, ")\n" );
		    } else {
		    	fprintf( stdout, "APP0 Size=%04X\n", size );
		    }
		    break;
		case 0xFF :	// 
		    break;
		default :
		    fprintf( stdout, "Unknown FF %02X(%llX)\n", 
		    	buffer[phase], g_addr );
		    break;
		}
		if( bSOI )
		{
		    if( phase==0 )
		    {
			JpegBuf[nJpegSize++] = buffer[phase];
		    } else {
			if( buffer[phase]!=0xFF )
			{
			    memcpy( &JpegBuf[nJpegSize], buffer, phase+1 );
			    phase=0;
			} else {
			    JpegBuf[nJpegSize++] = 0xFF;
			}
		    }
		    if( bEOI )
		    {
			bSOI = 0;
			fprintf( stdout, "nJpegSize=%d\n", nJpegSize );
			nJpegSize=0;
		    }
		}
		bEOI=0;
		break;
	    default :
		break;
	    }
	}
	fclose( fp );
	if( pOut )
	    fclose( pOut );
}

