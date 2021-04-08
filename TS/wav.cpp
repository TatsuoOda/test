
/*
	wav.cpp
		2012.5.18  by T.Oda
		2013.11.11 -b : nBit : sampling bit
				support 8/16/24/32bit
*/

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// memset
#include <math.h>	// sin

int Usage( char *prg )
{
	fprintf( stdout, 
"%s -g -f#freq -d#duration -s#sampling -b #bits -m#max -o#onTime filename \n", 
		prg );
	exit( 1 );
}

#define MODE_GEN	1
#define MODE_PARSE	2

int main( int argc, char *argv[] )
{
int i;
int args=0;
int mode=MODE_GEN;
int freq =  440;	// Ra
double d_freq = 440.0;
int duration = 1000;	// 1000ms = 1sec
int sampling = 48000;	// 48KHz
//int max      =  50;
int max      = 100;
int onTime   = 1000;
int bDebug   = 0;
char *filename = NULL;
int nBit=16;	// 16bit

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case 'g' :
		    mode = MODE_GEN;
		    break;
		case 'f' :
		    freq = atoi( &argv[i][2] );
		    d_freq = atof( &argv[i][2] );
		    if( freq<0 )
		    {
		    	fprintf( stdout, "frequency rate (%d) < 0\n",
				freq );
			exit( 1 );
		    }
		    fprintf( stdout, "d_freq=%f\n", d_freq );
		    break;
		case 'b' :	// sampling bits
		    nBit = atoi( &argv[i][2] );
		    if( (nBit<8) || (nBit>32) )
		    {
		    	fprintf( stdout, "Invalid bits(%d)\n", nBit );
			exit( 1 );
		    }
		    break;
		case 'd' :
		    duration = atoi( &argv[i][2] );
		    if( duration<0 )
		    {
		    	fprintf( stdout, "duration (%d) < 0\n", duration );
			exit( 1 );
		    }
		    break;
		case 'o' :
		    onTime = atoi( &argv[i][2] );
		    if( onTime<0 )
		    {
		    	fprintf( stdout, "onTime (%d) < 0\n", onTime );
			exit( 1 );
		    }
		    break;
		case 's' :
		    sampling = atoi( &argv[i][2] );
		    if( sampling<8000 )
		    {
		    	fprintf( stdout, "sampling rate (%d) < 8000\n",
				sampling );
			exit( 1 );
		    }
		    break;
		case 'm' :
		    max = atoi( &argv[i][2] );
		    if( max<0 )
		    {
		    	fprintf( stdout, "max (%d) < 0\n", max );
			exit( 1 );
		    }
		    break;
		default :
		    Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'd' :
		    bDebug = 1;
		    break;
		default :
		    Usage( argv[0] );
		    break;
		}
	    } else {
	    	switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		default :
		    Usage( argv[0] );
		    break;
		}
		args++;
	    }
	}
	fprintf( stdout, "Sampling Freq = %d Hz\n", sampling );
	fprintf( stdout, "Sampling bits = %d\n", nBit );
	fprintf( stdout, "frequency     = %f Hz\n", d_freq );
	fprintf( stdout, "duration      = %d ms\n", duration );
	int nByte = nBit/8;
	if( nByte<1 )
	{
	    fprintf( stdout, "Invalid size(nByte=%d)\n", nByte );
	    exit( 1 );
	}

#define BUFUNIT		1024
#define MAX_BYTES	4
	unsigned char buffer[BUFUNIT*MAX_BYTES];	// max 32bit
	if( mode==MODE_GEN )
	{
	    int written;
	    int chan=1;	// mono
//	    int dataSize = sampling*nBit/8*chan;
	    int dataSize = sampling*nByte*chan;
	    int fileSize = dataSize+8+0x12+0x04+0x08;

	    fprintf( stdout, "dataSize = %d\n", dataSize );
	    fprintf( stdout, "fileSize = %d\n", fileSize );

	    unsigned int   *pLong;
	    unsigned short *pShort;
	    if( filename==NULL )
	    {
	    	Usage( argv[0] );
	    }
	    FILE *fp = fopen( filename, "wb" );
	    if( fp==NULL )
	    {
	    	fprintf( stdout, "Can't open [%s]\n", filename );
		exit( 1 );
	    }
	    memset( buffer, 255, BUFUNIT*MAX_BYTES );
	    sprintf( (char *)&buffer[ 0], "RIFF" );

	    pLong = (unsigned int *)&buffer[ 4];
	    *pLong = fileSize;

	    sprintf( (char *)&buffer[ 8], "WAVE" );
	    sprintf( (char *)&buffer[12], "fmt " );
	    pLong = (unsigned int *)&buffer[16];
	    *pLong = 0x12;

	    pShort = (unsigned short *)&buffer[20];
	    *pShort = 1;	// PCM

	    pShort = (unsigned short *)&buffer[22];
	    *pShort = chan;	// channel number

	    pLong = (unsigned int *)&buffer[24];
	    *pLong = sampling;	// sampling rate

	    pLong = (unsigned int *)(&buffer[28]);
	    *pLong = sampling*nByte*chan;	// byte/sec

	    pShort = (unsigned short *)(&buffer[32]);
	    *(pShort+0) = nByte*chan;	// 	block

	    pShort = (unsigned short *)&buffer[34];
	    *pShort = nBit;	// bit / sample

	    pShort = (unsigned short *)&buffer[36];
	    *pShort = 0;	// Extension size

	    sprintf( (char *)&buffer[38], "data" );
	    pLong = (unsigned int *)&buffer[42];
	    *pLong = dataSize;
#if 0
	    for( i=0; i<48; i++ )
	    {
	    	buffer[i] = i;
	    }
#endif
	    for( i=0; i<48; i++ )
	    {
	    	fprintf( stdout, "%02X ", buffer[i] );
		if( (i%16)==15 )
		    fprintf( stdout, "\n" );
	    }
	    written = fwrite( buffer, 1, 46, fp );

	    double rad = 0;
	    int s, index;
	    memset( buffer, 0, BUFUNIT*MAX_BYTES );
	    int bMute=0;
	    int L_level = 0;
	    int offset;
	    long long fullRange = 1;
	    for( s=0; s<nByte; s++ )
	    {
	    	fullRange = fullRange*256;
	    }
	    fullRange = fullRange/2 - 1;
	    fprintf( stdout, "fullRange=%lld\n", fullRange );
	    for( s=0; s<sampling*duration/1000; s++ )
	    {
		index  = s % BUFUNIT;
//	    	offset = (s*nByte)%(BUFUNIT*nByte);
	    	offset = index*nByte;
		rad = 3.141592653589793*s*d_freq*2/sampling;
	    	double dl = sin(rad);
		double df = fullRange;
		dl = dl*df*max/100;
		int level = (int)dl;
		long long nn = s;
//		nn = s*1000*1000/sampling/duration;
		nn = s*1000/sampling;
#if 0
		fprintf( stdout,
		"%6d, %6d, %6d : %4d : %d\n", 
			s, sampling, duration, (int)nn, level );
#endif
#if 1
// fade
		if( nn>=onTime )
		{
//		    int gain = 100-(nn-onTime)*1;	// 100ms
//		    int gain = 100-(nn-onTime)*2;	// 50ms
		    int gain = 100-(nn-onTime)*4;	// 25ms
		    if( gain<0 )
		    	level = 0;
		    else
		    	level = level*gain/100;
#if 0
		fprintf( stdout,
		    "%6d, %6d, %6d : %3d %5d\n", 
			s, (int)nn, onTime, gain, level );
#endif
		}
#else
		if( nn>=onTime )
		{
		    if( bMute==0 )
		    {
		    	if( (level==0) 
			 ||((L_level<0) && (level>0))
			 ||((L_level>0) && (level<0)) )
			{
			    bMute = 1;
			    fprintf( stdout, "bMute@%d(%d->%d)\n", 
			    	s, L_level, level );
			}
		    }
		}
		L_level = level;
		if( bMute )
		    level = 0;
#endif
		if( bDebug )
fprintf( stdout, "%5d(%4d) : %f %6d(%4X)\n", s, offset, rad, level, level );
		switch( nByte )
		{
		case 1 :
		    buffer[offset+0] = (level & 0x000000FF)>> 0;
		    break;
		case 2 :
		    buffer[offset+0] = (level & 0x000000FF)>> 0;
		    buffer[offset+1] = (level & 0x0000FF00)>> 8;
		    break;
		case 3 :
		    buffer[offset+0] = (level & 0x000000FF)>> 0;
		    buffer[offset+1] = (level & 0x0000FF00)>> 8;
		    buffer[offset+2] = (level & 0x00FF0000)>>16;
		    break;
		case 4 :
		    buffer[offset+0] = (level & 0x000000FF)>> 0;
		    buffer[offset+1] = (level & 0x0000FF00)>> 8;
		    buffer[offset+2] = (level & 0x00FF0000)>>16;
		    buffer[offset+3] = (level & 0xFF000000)>>24;
		    break;
		default :
		    fprintf( stdout, "Unsupport bits(%d)\n", nBit );
		    exit( 1 );
		}
		if( index==(BUFUNIT-1) )
		{
		    written = fwrite( buffer, nByte, BUFUNIT, fp );
		    if( bDebug )
		    fprintf( stdout, "%d words written\n", written );
		    memset( buffer, 0, BUFUNIT*nByte );
		}
	    }
	    if( index<(BUFUNIT-1) )
	    {
		    written = fwrite( buffer, nByte, index+1, fp );
		    if( bDebug )
		    fprintf( stdout, "%d words written\n", written );
	    }
	    fclose( fp );
	}


}

