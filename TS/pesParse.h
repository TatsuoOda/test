
/*
	pesParse.h
		2012.8.15 separate from pes.cpp
 */

#define MAX_UPDATE      1024*64

#define ID_HSV		 1
#define ID_VSV		 2
#define ID_ARI		 3
#define ID_FRC		 4
#define ID_PALI		 5
#define ID_PS		 6
#define ID_CP		 7
#define ID_TC		 8
#define ID_MXC		 9
#define ID_DHS		10
#define ID_DVS		11
// for AVC
#define ID_SARW		12
#define ID_SARH		13
#define ID_LVL		14
//
#define ID_PANL		15
#define ID_PANR		16
#define ID_PANT		17
#define ID_PANB		18

#define ID_OVSP		19	// present_flag
#define ID_OVSA		20	// appropriate_flag

#define ID_MAX		21

extern int items[ID_MAX];
extern int value[ID_MAX];
extern int Counts[256];

extern unsigned int *g_update_addr;
extern unsigned int *g_update_bits;
extern unsigned int *g_update_size;
extern unsigned int *g_update_data;
extern int g_update_count;

extern unsigned int *g_update_addrS;
extern unsigned int *g_update_bit_S;	// remove start
extern unsigned int *g_update_bit_E;	// remove end
extern unsigned int *g_update_bit_T;	// structure end
extern unsigned int *g_update_mode ;
extern int g_update_countS;

extern int validStart;
extern int validEnd  ;

extern int bDebug;
extern int bDump ;
extern int bDumpSlice;
extern int bDisplayTS;
extern int bShow;
extern int bSkipError;
//extern int bShowDetail;              // AVC show NAL parse
/*
extern int bShowGolomb;              // Dump Golomb parse
*/
extern int bDebugSkip;               // Dump with skipContent()
extern int bShowBitAddr;             // Show BitAddr/Offset/Readed

extern int bDebugSEI;                // Debug SEI
extern int bShowNALinfo;             // CountNAL message
//extern int bDebugPES;                // Debug PES_header
extern int bUserData;
extern int bShowMvcExtention;
extern int bShowMvcScalable;

extern int PtsOffset;
extern int DtsOffset;
extern int fromPts;
extern int toPts;
extern int fromDts;
extern int toDts;

extern int bDumpSequenceDisplayExtension;
extern int bDumpSequenceExtension;
extern int bDumpSequence;

extern int nSelSt;
extern int nSelEn;
//int bDumpSequenceDisplayExtension = 0;
//int bDumpSequenceExtension = 0;
//int bDumpSequence = 0;

//#define MAX_PACKETS 1024*64
//extern int *Packets;     // 16bit

void SetSpecial( int mode );

#define SET_START	0
#define SET_START_END	1
#define SET_END		2

void EXIT( );

int PES_header( FILE *fp, unsigned char buffer[], int bDisplay,
	unsigned int *pPTSH, unsigned int *pPTSL, 
	unsigned int *pDTSH, unsigned int *pDTSL );
//	unsigned long &PTSH, unsigned long &PTSL, 
//	unsigned long &DTSH, unsigned long &DTSL );

int AnalyzeHeader( FILE *fp, FILE *pts_fp );
int AnalyzeMPG( FILE *fp, FILE *pts_fp );
int AnalyzeAVC( FILE *fp, FILE *pts_fp );
int Remux( char *srcFilename, char *outFilename,
        char *textFilename, FILE *pts_fp, int Threshold );

