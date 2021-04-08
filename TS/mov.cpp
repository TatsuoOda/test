
/*
	mov.cpp
		2012.1.6  by T.Oda
		2012.2.9 : Support CTTS
			 : Support OUTPUT_AVC
			 : +J output JPEG
			 : +A output AVC 
		2012.2.14 : After Invalid code, updated (AVC/VC1)
		2012.2.14 : AVC's SPES : add first PTS
		2012.4. 7 : +M output MJPG
		2012.4. 9 : +4 output MPEG4
		2012.7. 5 : mdat : support 8byte size
			    co64
		2012.8.22 : movParse
		2012.9.13 : +A : always DTS valid
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parseLib.h"
#include "movParse.h"

// ----------------------------------------------------------
// parse option

int bDetail=0;
int bDebug=0;

extern int bCTTS;
extern int bVersion;
extern int bShowSTSZ;

// ----------------------------------------------------------
// parsed info
extern int VideoTimeScale;
extern long long VideoDuration ;
extern int VideoSamples  ;

extern int nVideo;
extern unsigned long Video[1024*1024*4];

#define MAX_AVCC 1024*16
extern int nAVCC;
extern unsigned char avcC[MAX_AVCC];
extern int nMVCC;
extern unsigned char mvcC[MAX_AVCC];
#define MAX_MPEG4 1024*16
extern int nMPEG4;
extern unsigned char esds[MAX_MPEG4];


// ----------------------------------------------------------

int main( int argc, char *argv[] )
{
	return movMain( argc, argv );
}

