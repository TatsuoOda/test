
/*
	spesParse.h
		2012.6.25 separate from spes.cpp
		2012.10.3 add SPES_OUT_CUT
*/

#define MAX_FRAMES 1024*1024

#define SPES_MODE_READ  	0
#define SPES_MODE_EDIT  	1
#define SPES_MODE_DTS_EDIT      2
#define SPES_MODE_MERGE 	3

#define SPES_OUT_ES          	0
#define SPES_OUT_ES264         	1
#define SPES_OUT_SPES        	2
#define SPES_OUT_CUT        	3
#define SPES_OUT_SSPES        	4

int SpesParse( char *filename, char *diffFile, int nSeek,
	int mode, int outMode, int fileID,
	int bNoPTS, int bDetail, int frames[MAX_FRAMES], 
	unsigned long SPES[MAX_FRAMES*6]
);

int spes( int argc, char *argv[] );

