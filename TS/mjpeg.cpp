/*
	mjpeg.cpp
		2011.12.1
*/

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// strlen

int Usage( char *prg )
{
	fprintf( stdout, "%s filename\n", prg );
	exit( 1 );
}

#define MODE_NONE	0
#define MODE_SOI	1


unsigned char MJPGDHTSeg[0x1A4] = {
 /* JPEG DHT Segment for YCrCb omitted from MJPG data */
0xFF,0xC4,0x01,0xA2,
0x00,0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x01,0x00,0x03,0x01,0x01,0x01,0x01,
0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
0x08,0x09,0x0A,0x0B,0x10,0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,
0x00,0x01,0x7D,0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,
0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08,0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,0x24,
0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28,0x29,0x2A,0x34,
0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,
0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,
0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,
0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,
0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,
0xDA,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,
0xF8,0xF9,0xFA,0x11,0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,
0x02,0x77,0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,0x15,0x62,
0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26,0x27,0x28,0x29,0x2A,
0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,
0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,
0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,
0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,
0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,
0xD9,0xDA,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
0xF9,0xFA
};

int main( int argc, char *argv[] )
{
int i;
char *filename=NULL;
int args=0;
int readed;
unsigned char buf[1024];
int state=0;
int mode=MODE_NONE;
int index=0;
FILE *outfile=NULL;
char ofilename[1024];
char head[1024];
int nSOI=0;
unsigned long addr=0;
unsigned char last[1024];
int bAddDHT = 0;
int bDHT=0;

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'H' :	// add DHT
		    bAddDHT = 1;
		    break;
		}
	    } else {
		switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		default :
		    break;
		}
	    }
	}

	if( filename==NULL )
	{
	    Usage( argv[0] );
	}

	FILE *fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}
	for( i=strlen(filename)-1; i>0; i-- )
	{
	    if( filename[i]=='.' )
		break;
	}
	memset( head, 0, 1024 );
	memset( last, 0, 1024 );
	if( i>=0 )
	{
	    memcpy( head, filename, i );
	    head[i] = 0;
	}
	while( 1 )
	{
	    readed = fread( &buf[state], 1, 1, fp ); addr++;
	    if( readed<1 )
	    {
	    	fprintf( stdout, "EOF\n" );
		break;
	    }
	    switch( state )
	    {
	    case 0 :
	    	if( buf[0]==0xFF )
		{
		    state++;
		} else {
		    if( mode==MODE_SOI )
			fwrite( &buf[0], 1, 1, outfile );
			memcpy( &last[0], &last[1], 1023 );
			last[1023] = buf[0];
		}
		break;
	    case 1 :
	    	if( buf[1]==0xD8 )	// SOI(FF D8)
		{
		    bDHT=0;
		    state++;
		} else if( buf[1]==0xD9 )	// EOI(FF D9)
		{
		    if( mode==MODE_SOI )
		    {
			fwrite( &buf[0], 1, 2, outfile );
		    }
		    memcpy( &last[0], &last[1], 1023 );
		    last[1023] = buf[0];
		    if( nSOI>0 )
		    {
			fprintf( stdout, "EOI(%8X) ", addr-2 );
		    	nSOI--;
			if( nSOI==0 )
			{
			    fclose( outfile );
			    outfile=NULL;
			    mode=MODE_NONE;
			    fprintf( stdout, "\n" );
			}
		    } else {
		    }
		    state=0;
		} else if( buf[1]==0xDA )	// SOS(FF DA)
		{
		    // Start of Scan
		    if( mode==MODE_SOI )
		    {
#if 1
			if( bDHT==0 )
			{
			    fprintf( stdout, "(DHT) " );
			    fwrite( MJPGDHTSeg, 1, 0x1A4, outfile );
			}
#endif
		    	fprintf( stdout, "SOS " );
			fwrite( &buf[0], 1, 2, outfile );
		    }
		    state=0;
/*
		} else if( buf[1]==0xE0 )	// APP0(FF E0)
		{
		    if( mode==MODE_SOI )
		    {
		    	fprintf( stdout, "APP0 " );
			fwrite( &buf[0], 1, 2, outfile );
		    }
		    state=0;
*/
		} else if( buf[1]==0xDB )	// DQT(FF DB)
		{
		    if( mode==MODE_SOI )
		    {
		    	fprintf( stdout, "DQT " );
			fwrite( &buf[0], 1, 2, outfile );
		    }
		    state=0;
		} else if( buf[1]==0xC0 )	// SOF0(FF C0)
		{
		    if( mode==MODE_SOI )
		    {
		    	fprintf( stdout, "SOF0 " );
			fwrite( &buf[0], 1, 2, outfile );
		    }
		    state=0;
		} else if( buf[1]==0xC1 )	// SOF1(FF C1)
		{
		    if( mode==MODE_SOI )
		    {
		    	fprintf( stdout, "SOF1 " );
			fwrite( &buf[0], 1, 2, outfile );
		    }
		    state=0;
		} else if( buf[1]==0xC4 )	// DHT(FF C4)
		{
		    bDHT++;
		    readed = fread( &buf[2], 1, 1, fp ); addr++;
		    readed = fread( &buf[3], 1, 1, fp ); addr++;
		    if( buf[2]!=0xFF )
		    {
			if( mode==MODE_SOI )
			{
			    fprintf( stdout, "DHT(%02X %02X)@%X ",
				    buf[2], buf[3], addr-4 );
			} else {
			    fprintf( stdout, "%02X DHT(%02X %02X)@%X\n",
				    last[1023], buf[2], buf[3], addr-4 );
			}
		    }
		    if( mode==MODE_SOI )
			fwrite( &buf[0], 1, 4, outfile );
			memcpy( &last[0], &last[1], 1023 );
			last[1023] = buf[0];
			memcpy( &last[0], &last[1], 1023 );
			last[1023] = buf[1];
			memcpy( &last[0], &last[1], 1023 );
			last[1023] = buf[2];
			memcpy( &last[0], &last[1], 1023 );
			last[1023] = buf[3];
		    state=0;
		} else if( buf[1]==0xDD )	// FF DD
		{
		    if( mode==MODE_SOI )
		    {
		    	if( bAddDHT )
			{
			    fwrite( MJPGDHTSeg, 1, 0x1A4, outfile );
			    bDHT=1;
			}
			fwrite( &buf[0], 1, 2, outfile );
		    }
		    state=0;
		} else if( buf[1]==0xFF )	// FF FF
		{
		    if( mode==MODE_SOI )
			fwrite( &buf[0], 1, 1, outfile );
			memcpy( &last[0], &last[1], 1023 );
			last[1023] = buf[0];
		} else {
		    if( mode==MODE_SOI )
			fwrite( &buf[0], 1, 2, outfile );
			memcpy( &last[0], &last[1], 1023 );
			last[1023] = buf[0];
			memcpy( &last[0], &last[1], 1023 );
			last[1023] = buf[1];
		    state=0;
		}
		break;
	    case 2 :
	    	if( buf[2]==0xFF )	// SOI fixed
		{
		    state++;
		} else {
		    if( mode==MODE_SOI )
			fwrite( &buf[0], 1, 3, outfile );
			memcpy( &last[0], &last[1], 1023 );
			last[1023] = buf[0];
			memcpy( &last[0], &last[1], 1023 );
			last[1023] = buf[1];
			memcpy( &last[0], &last[1], 1023 );
			last[1023] = buf[2];
		    state=0;
		}
		break;
	    case 3 :
	    	// FF D8 FF E0/DB
	    	if( (buf[3]==0xE0) || (buf[3]==0xDB) )
		{
		    readed = fread( &buf[4], 1, 1, fp ); addr++;
		    if( buf[4]!=0xFF )
		    {
			if( nSOI==0 )
			{
			    mode=MODE_SOI;
			    fprintf( stdout, "%8X : ", addr-5 );
			    if( outfile!=NULL )
				fclose( outfile );
			    sprintf( ofilename, "%s-%04d.jpg", head, index );
//			    fprintf( stdout, "%s SOI ", ofilename );
			    fprintf( stdout, "%04d SOI ", index );
			    outfile = fopen( ofilename, "wb" );
			    index++;
			}
			fwrite( &buf[0], 1, 5, outfile );
			memcpy( &last[0], &last[0], 1023 );
			last[1023] = buf[0];
			memcpy( &last[0], &last[0], 1023 );
			last[1023] = buf[1];
			memcpy( &last[0], &last[0], 1023 );
			last[1023] = buf[2];
			memcpy( &last[0], &last[0], 1023 );
			last[1023] = buf[3];
			memcpy( &last[0], &last[0], 1023 );
			last[1023] = buf[4];
			nSOI++;
			if( buf[3]==0xE0 )
			{
			    char app0[8];
			    fprintf( stdout, "APP0" );
			    readed = fread( &buf[5], 1, 6, fp ); addr++;
			    fwrite( &buf[5], 1, 6, outfile );
			    memset( app0, 0, 8 );
			    memcpy( app0, &buf[6], 4 );
			    fprintf( stdout, "(%s:%d) ", app0, buf[10] );
			}
		    } else {
			fprintf( stdout, "Skip(%02X %02X %02X %02X %02X)\n",
			    buf[0], buf[1], buf[2], buf[3], buf[4] );
		    }
		    state=0;
		} else {
		    fprintf( stdout, "Skip(%02X %02X %02X %02X)\n",
		    	buf[0], buf[1], buf[2], buf[3] );
		    if( mode==MODE_SOI )
			fwrite( &buf[0], 1, 4, outfile );
			memcpy( &last[0], &last[0], 1023 );
			last[1023] = buf[0];
			memcpy( &last[0], &last[0], 1023 );
			last[1023] = buf[1];
			memcpy( &last[0], &last[0], 1023 );
			last[1023] = buf[2];
			memcpy( &last[0], &last[0], 1023 );
			last[1023] = buf[3];
		    state=0;
		}
		break;
	    }
	    fflush( stdout );
	}
	fclose( fp );
}

