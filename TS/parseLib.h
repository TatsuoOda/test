
/*
	parseLib.cpp
		2012.8.22 by T.Oda
*/

extern int bDebug;

//extern unsigned int g_addr;
extern long long g_addr;

extern int bDebugGol;
extern int bDebugGolB;
extern int bShowGolomb;		// Dump Golomb parse

void EXIT( );

int gread( unsigned char *buffer, int size1, int size2, FILE *fp );

int Gread( unsigned char *buffer, int size1, int size2, FILE *fp,
	int *pTotal1, int *pTotal2);

unsigned int Short( unsigned char *buffer );
unsigned int Long( unsigned char *buffer );
long long LongLong( unsigned char *buffer );

unsigned int Short_little( unsigned char *buffer );
unsigned int Long_little( unsigned char *buffer );

int Copy( FILE *out, FILE *in, int size );

// ------------------------------------
void InitBitBuffer( );
int GetBitFromBuffer( unsigned char *buffer, int *pBufOffset, int nBit );
int GetGolombFromBuffer( unsigned char *buffer, int *pBufOffset );

