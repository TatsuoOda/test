
#ifndef _TS_LIB_H
#define _TS_LIB_H

//#define ADDR_MAX        1024*1024*16
#define ADDR_MAX        1024*1024*32

#define MODE_PTS	0
#define MODE_DTS	1

#define MAX_UPDATE      1024*64

#define INVALID_OFFSET     0x12345678

#if 0
extern long long TsAddr [ADDR_MAX];
extern long long PesAddr[ADDR_MAX];
#else
extern unsigned long long *TsAddr ;
extern unsigned long long *PesAddr;
#endif

extern unsigned int g_DTSL;
extern unsigned int g_DTSH;
extern unsigned int g_PTSL;
extern unsigned int g_PTSH;

int initTsAddr( );

int TS( unsigned int High, unsigned int Low, 
	unsigned int *pTSH, unsigned int *pTSL );

//unsigned int SrcAddr( int addr, int type );
long long SrcAddr( long long addr, int type );
char *AddrStr ( unsigned int addr );
char *AddrStr2( unsigned int addr );
char *TsStr( );
char *DtsTimeStr( );

void CannotRead( char *str );
int DoUpdate( int addr, int bitOffset, int size, int value );
int ShowUpdate( );

int  ReadAddrFile( char filename[], 
	char srcFilename[],
	unsigned long long TsAddr[], 
	unsigned long long PesAddr[],
	int *nAddr, int MaxAddr );

int TcTime24( unsigned int vCountH, unsigned int vCountL,
	int *hour, int *min, int *sec, int *msec );
int TcTime32( unsigned int vCountH, unsigned int vCountL,
	int *hour, int *min, int *sec, int *msec );


int PES_header( FILE *fp, unsigned char buffer[], int bDisplay,
	unsigned int *pPTSH, unsigned int *pPTSL, 
	unsigned int *pDTSH, unsigned int *pDTSL );
int parsePES_header( unsigned char buffer[], int bDisplay,
	unsigned int *pPTSH, unsigned int *pPTSL, 
	unsigned int *pDTSH, unsigned int *pDTSL,
	int *pOffset );

unsigned int crc32( unsigned char* data, int length );


#endif // _TS_LIB_H

