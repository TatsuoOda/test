
/*
	pes.cpp
 */

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// memcpy

#define BUF_SIZE	1024*4

void Usage( char *prg )
{
	fprintf( stdout, "%s filename\n", prg );
	exit( 1 );
}

int TS( unsigned long High, unsigned long Low, 
	unsigned long *pTSH, unsigned long *pTSL )
{
unsigned long TS32_30, TS29_15, TS14_00;
unsigned long TS32, TS31_00;
	TS32_30 = (High>>1) & 0x0007;
	TS29_15 = (Low>>17) & 0x7FFF;
	TS14_00 = (Low>> 1) & 0x7FFF;
	TS32 = TS32_30>>2;
	TS31_00 = (TS32_30<<30) | (TS29_15<<15) | (TS14_00);
	*pTSH = TS32;
	*pTSL = TS31_00;
	return TS31_00;
}

int main( int argc, char *argv[] )
{
int i;
char *filename = NULL;
int args=0;
FILE *fp;
unsigned char buffer[BUF_SIZE];
int readed;
int ID, len;

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
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
	    Usage( argv[0] );
	}
	fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}
	int addr=0;
	readed = fread( &buffer[0], 1, 512, fp );
	while( 1 )
	{
	    int offset=0;
	    readed = fread( &buffer[512], 1, 512, fp );
#if 0
	    fprintf( stdout, "%d bytes readed\n", readed );
	    int y;
	    for( y=0; y<4; y++ )
	    {
		for( i=0; i<16; i++ )
		{
		    fprintf( stdout, "%02X ", buffer[i+y*16] );
		}
		fprintf( stdout, "\n" );
	    }
#endif
	    if( readed<512 )
	    {
	    	fprintf( stdout, "Can't read Prefix\n" );
	    	break;
	    }
	    for( i=0; i<512; i++ )
	    {
	    	if( (buffer[i+0]==0)
		 && (buffer[i+1]==0)
		 && (buffer[i+2]==1) )
		{
		    fprintf( stdout, "%8X : %02X %02X %02X %02X : %02X\n",
		    	addr, 
			buffer[i+0], buffer[i+1], buffer[i+2], buffer[i+3],
			buffer[i+4] );
		}
		addr++;
	    }
	    memcpy( &buffer[0], &buffer[512], 512 );
	}
	fclose( fp );
}

