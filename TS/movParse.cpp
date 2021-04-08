
/*
	movParse.cpp
		2012.8.22 by T.Oda
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
		2012.9.12 : mvhd,tkhd,mdhd.. : add code for size>total
			    Support netflix MP4
			    Check version
		2012.9.20 : use STTS, CTTS for AVC DTS/PTS calculation
		2012.9.21 : STTS[]->STTS[][2]
			    -S : SPS is added only top. (default every key)
		2012.9.26 : SPS frame_rate
		2012.10.3 : malloc Video[], STSZ, CTTS
		2012.10.19: 
			    +E : bOutputFrame
			    -A : nAVC
			    -M : nMVC
		2012.10.22: -D : bDumpAtom = 1
		2013.2.5    DSCN2574.MOV : ctts table over
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parseLib.h"
#include "spesLib.h"

extern int bDetail;
int bCTTS=0;
int mdhd_TimeScale = (-1);
long long mdhd_Duration  = (-1);

static int bShowSTSZ = 0;
static int bKeySPS = 1;
static int nAVC = 1;
static int nMVC = 1;
static int bOutputFrame = 0;

static int bDumpAtom = 0;

int VideoTimeScale = (-1);
long long VideoDuration  = (-1);
int VideoSamples   = (-1);

// ----------------------------
// for SPS
#define MAX_FORCE_FRAMERATES 1024
int nForceFramerate = 0;
int forceFramerates[MAX_FORCE_FRAMERATES];
// ----------------------------

typedef unsigned int UINT;

//#define MAX_STSZ 1024*64
//#define MAX_STSZ 1024*128
#define MAX_STSZ 1024*256
int nSTSZ=0;
//unsigned int STSZ[MAX_STSZ];
unsigned int *STSZ = NULL;
#define MAX_STCO 1024*16
int nSTCO=0;
unsigned int STCO[MAX_STCO];
#define MAX_STSC 1024*16
int nSTSC=0;
unsigned int STSC[MAX_STSC][3];
#define MAX_STTS 1024*32
int nSTTS=0;
unsigned int STTS[MAX_STTS][2];
//#define MAX_CTTS 1024*64
#define MAX_CTTS 1024*128
int nCTTS=0;
//unsigned int CTTS[MAX_CTTS][2];
unsigned int *CTTS = NULL;
#define MAX_STSS 1024*16
int nSTSS=0;
unsigned int STSS[MAX_STSS];

#define MAX_AVCC 1024*16
int nAVCC=0;
unsigned char avcC[MAX_AVCC];
int nMVCC=0;
unsigned char mvcC[MAX_AVCC];

#define MAX_MPEG4 1024*16
int nMPEG4=0;
unsigned char esds[MAX_MPEG4];

int nVideo=0;
int bVersion=0;
#define VIDEO_UNIT	6
//unsigned int Video[1024*1024*VIDEO_UNIT];
unsigned int *Video = NULL;

static char CompType[8];

#define MAX_TRUN 1024
int Trun[MAX_TRUN][4];
int nTrun = 0;


// ---------------------------------------------
#define BUF_SIZE 1024*1024
unsigned char buf[BUF_SIZE];

void Level( int level )
{
int i;
	for( i=0; i<level; i++ )
	{
	    fprintf( stdout, "+" );
	}
}

long long skip( FILE *fp, long long size )
{
long long total=0;
int readed;

	while( total<size )
	{
	    int sz = size-total;
	    if( sz>BUF_SIZE )
	    	sz = BUF_SIZE;
	    readed = gread( buf, 1, sz, fp );
	    if( readed<1 )
	    	EXIT();
	    total+=readed;
	}
	
	return total;
}

int SkipAtom( const char *title, FILE *fp, int size, int level )
{
	Level( level );
	fprintf( stdout, "[%s] %8llX : size=%8X\n", title, g_addr-8, size );
	return skip( fp, size-8 );
}

int Version( FILE *fp, int *pTotal )
{
int readed;
int version = (-1);
	readed = Gread( buf, 1, 4, fp, pTotal, NULL );
	version = buf[0];
	if( bVersion )
	{
	fprintf( stdout, "Version= %02X\n", buf[0] );
	fprintf( stdout, "Flags  = %02X %02X %02X\n", buf[1], buf[2], buf[3] );
	}
	return version;
}

void AtomHead( const char *name, int size )
{
	fprintf( stdout, "[%s] %8llX : size=%8X\n", 
		name, (g_addr-8), (UINT)size );
	if( bDumpAtom )
	fprintf( stdout, "=======================\n" );
}

int header( char *name, int size, int level )
{
	Level( level );
	fprintf( stdout, "(%s) %8llX : size=%8X\n", 
		name, (g_addr-8), (UINT)size );
	return 0;
}

// ----------------------------------------------------------------
// atom
// ----------------------------------------------------------------

int BoxEnd( FILE *fp, int size, int total, int bShow )
{
	if( size>total )
	{
	    if( bShow )
		fprintf( stdout, "total=%d, size=%d\n", total, size );
	    skip( fp, size-total );
	}
	if( bDumpAtom )
	fprintf( stdout, "=======================\n" );
	return (size-total);
}

int mdat( FILE *fp, long long size )
{
int readed;
int total=8;
	if( size==1 )
	{
	    memset( buf, 0, 8 );
	    readed = Gread( buf, 1, 8, fp, &total, NULL );
	    if( readed<4 )
	    	EXIT();
	    size = LongLong( buf );
	    fprintf( stdout, "[mdat] %8llX : sizeL=%16llX\n", g_addr-8, size );
	} else
	    fprintf( stdout, "[mdat] %8llX : size =%8llX\n", g_addr-8, size );

	if( nTrun>0 )
	{
	    int e;
	    char dstFile[1024];
	    for( e=0; e<nTrun; e++ )
	    {
		sprintf( dstFile, "%06d.es", nVideo );
		int sz = Trun[e][1];
	    	Video[nVideo*VIDEO_UNIT+0] = g_addr;
		if( e<MAX_TRUN )
		{
	    	Video[nVideo*VIDEO_UNIT+1] = sz = Trun[e][1];
	    	Video[nVideo*VIDEO_UNIT+2] = Trun[e][3]; // key
	    	Video[nVideo*VIDEO_UNIT+3] = Trun[e][0]; // delta DTS
	    	Video[nVideo*VIDEO_UNIT+4] = Trun[e][2]; // PTS-DTS offset
		} else {
		    fprintf( stdout, "Too large entries(trun)\n" );
		    exit( 1 );
		}
		nVideo++;

		if( bOutputFrame )
		{
		    FILE *out = fopen( dstFile, "wb" );
		    if( out==NULL )
		    {
			fprintf( stdout, "Can't open [%s]\n", dstFile );
			exit( 1 );
		    }
		    Copy( out, fp, sz );
    #if 0
		    if( bVerbose )
		    {
		    fprintf( stdout, "%s : %8X\n", dstFile, sz );
		    }
		    if( Copy( out, fp, sz )==0 )
			nCount++;
    #endif
		    fclose( out );
		} else {
		    fseek( fp, sz, SEEK_CUR );
		}
		total+=sz;
		g_addr+=sz;
	    }
	    fprintf( stdout, "nVideo=%d\n", nVideo );
	    nTrun = 0;
	}
	BoxEnd( fp, size, total, 1 );

	return 0;
}

int mvhd( FILE *fp, int size )
{
int readed;
int total=8;
	AtomHead( "mvhd", size );
#if 0
	skip( fp, size );
#else
	memset( buf, 0, 8 );
	int version = Version( fp, &total );

    // Creation time (32)->(64)
	if( version==0 )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int CreationTime = Long(buf);
	    if( bDumpAtom )
	    fprintf( stdout, "Creation time          = 0x%X\n", CreationTime );
	} else {
	    readed = Gread( buf, 1, 8, fp, &total, NULL );
	    long long CreationTime = LongLong(buf);
	    if( bDumpAtom )
	    fprintf( stdout, "Creation time          = 0x%llX\n",CreationTime );
	}
    // Modification time (32)->(64)
    	if( version==0 )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int ModificationTime = Long(buf);
	    if( bDumpAtom )
	    fprintf( stdout,
	    	"Modification time      = 0x%X\n",ModificationTime);
	} else {
	    readed = Gread( buf, 1, 8, fp, &total, NULL );
	    long long ModificationTime = LongLong(buf);
	    if( bDumpAtom )
	    fprintf( stdout,
	    	"Modification time      = 0x%llX\n",ModificationTime);
	}
    // Time scale (32)
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int TimeScale = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Time scale             = %d\n", TimeScale );
    // Duration (32)->(64)
    	if( version==0 )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int Duration = Long(buf);
	    if( bDumpAtom )
	    fprintf( stdout, "Duration               = %d\n", Duration );
	} else {
	    readed = Gread( buf, 1, 8, fp, &total, NULL );
	    long long Duration = LongLong(buf);
	    if( bDumpAtom )
	    fprintf( stdout, "Duration               = %lld\n", Duration );
	}
    // Preferred rate (32) fixed-point
	readed = Gread( buf, 1, 4, fp, &total, NULL );
//	int Rate = Long(buf);
	int RateH = Short( &buf[0] );
	int RateL = Short( &buf[2] );
	if( bDumpAtom )
	fprintf( stdout, "Preferred rate         = %d.%04d\n", RateH, RateL );
    // Preferred Volume (16)
	readed = Gread( buf, 1, 2, fp, &total, NULL );
	int Volume = Short(buf);
	if( bDumpAtom )
	fprintf( stdout, "Volume                 = 0x%X\n", Volume   );
    // Reserved
	readed = Gread( buf, 1, 10, fp, &total, NULL );
	readed = Gread( buf, 1, 36, fp, &total, NULL );

    // Preview time
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int PreviewTime = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Preview time           = %d\n", PreviewTime );
    // Preview duration
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int PreviewDuration = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Preview duration       = %d\n", PreviewDuration );
    // Poster time
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int PosterTime = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Poster time            = %d\n", PosterTime );
    // Selection time
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int SelectionTime = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Selection time         = %d\n", SelectionTime );
    // Selection duration
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int SelectionDuration = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Selection duration     = %d\n", SelectionDuration );
    // Current time
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int CurrentTime = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Current time           = %d\n", CurrentTime );
    // Next track ID
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int trackID = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Next track ID          = %d\n", trackID );

	BoxEnd( fp, size, total, 1 );
#endif

	return 0;
}

int tkhd( FILE *fp, int size )
{
int readed;
int total=8;
int version=(-1);
	AtomHead( "tkhd", size );
#if 0
	skip( fp, size );
#else
	memset( buf, 0, 8 );
	version = Version( fp, &total );

    // Creation time (32)->(64)
    	if( version==0 )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int CreationTime = Long(buf);
	    if( bDumpAtom )
	    fprintf( stdout, 
	    	"Creation time          = 0x%X\n", CreationTime );
	} else {
	    readed = Gread( buf, 1, 8, fp, &total, NULL );
	    long long CreationTime = LongLong(buf);
	    if( bDumpAtom )
	    fprintf( stdout, 
	    	"Creation time          = 0x%llX\n", CreationTime );
	}
    // Modification time (32)->(64)
    	if( version==0 )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int ModificationTime = Long(buf);
	    if( bDumpAtom )
	    fprintf( stdout, 
	    	"Modification time      = 0x%X\n", ModificationTime );
	} else {
	    readed = Gread( buf, 1, 8, fp, &total, NULL );
	    long long ModificationTime = LongLong(buf);
	    if( bDumpAtom )
	    fprintf( stdout, 
	    	"Modification time      = 0x%llX\n", ModificationTime );
	}

    // Track ID (32)
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int TrackID = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Track ID               = %d\n", TrackID );
    // Reserved
	readed = Gread( buf, 1, 4, fp, &total, NULL );
    // Duration (32)->(64)
    	if( version==0 )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int Duration = Long(buf);
	    if( bDumpAtom )
	    fprintf( stdout, "Duration               = %d\n", Duration );
	} else {
	    readed = Gread( buf, 1, 8, fp, &total, NULL );
	    long long Duration = LongLong(buf);
	    if( bDumpAtom )
	    fprintf( stdout, "Duration               = %lld\n", Duration );
	}
    // Reserved (8byte)
	readed = Gread( buf, 1, 8, fp, &total, NULL );
    // Layer (16)
	readed = Gread( buf, 1, 2, fp, &total, NULL );
	int Layer = Short(buf);
	if( bDumpAtom )
	fprintf( stdout, "Layer                  = 0x%X\n", Layer    );
    // Altenate group (16)
	readed = Gread( buf, 1, 2, fp, &total, NULL );
	int Group = Short(buf);
	if( bDumpAtom )
	fprintf( stdout, "Alternate group        = 0x%X\n", Group    );
    // Volume (16)
	readed = Gread( buf, 1, 2, fp, &total, NULL );
	int Volume = Short(buf);
	if( bDumpAtom )
	fprintf( stdout, "Volume                 = 0x%X\n", Volume   );
    // Reserved
	readed = Gread( buf, 1, 2, fp, &total, NULL );
    // Matrix structure (36byte)
	readed = Gread( buf, 1, 36, fp, &total, NULL );
    // Track width (32) fixed-point
	readed = Gread( buf, 1, 4, fp, &total, NULL );
//	int TrackWidth = Long(buf);
	int WidthH = Short( &buf[0] );
	int WidthL = Short( &buf[2] );
	if( bDumpAtom )
	fprintf( stdout, "Track width            = %d.%04d\n", 
		WidthH, WidthL );
    // Track height (32)
	readed = Gread( buf, 1, 4, fp, &total, NULL );
//	int TrackHeight = Long(buf);
	int HeightH = Short( &buf[0] );
	int HeightL = Short( &buf[2] );
	if( bDumpAtom )
	fprintf( stdout, "Track height           = %d.%04d\n", 
		HeightH, HeightL );

	BoxEnd( fp, size, total, 1 );
#endif

	return 0;
}

int hdlr( FILE *fp, int size )
{
int total=8;
int readed;
char Type[8];
char subType[8];
	memset( Type, 0, 8 );
	memset( subType, 0, 8 );

	AtomHead( "hdlr", size );
	memset( buf, 0, 8 );
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	memcpy( Type, buf, 4 );
	if( bDumpAtom )
	fprintf( stdout, "Component type         = [%s]\n", Type );
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	memcpy( subType, buf, 4 );
	if( bDumpAtom )
	fprintf( stdout, "Component subtype      = [%s]\n", subType );
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	if( bDumpAtom )
	fprintf( stdout, "Component manufacturer = [%s]\n", buf );

	BoxEnd( fp, size, total, bDumpAtom );

	if( strcmp( Type, "mhlr" )==0 )
	    memcpy( CompType, subType, 4 );
	else if( Type[0]==0x00 )
	    memcpy( CompType, subType, 4 );
	return 0;
}

int stts( FILE *fp, int size )
{
int total=8;
int readed;
unsigned long start = g_addr-8;
	AtomHead( "stts", size );

	Version( fp, &total );

	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int entries = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Number of entries      = %d\n", entries );

	int e;
	nSTTS=0;
	for( e=0; e<entries; e++ )
	{
	    if( (g_addr-start)>=size )
	    {
	    	fprintf( stdout, "*** Size over\n" );
		break;
	    }
	// Sample count
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int count = Long(buf);
	// Sample duration
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int duration = Long(buf);
	    if( bDumpAtom )
	    fprintf( stdout, "%5d : %5d, %5d\n", e, count, duration );
	    STTS[nSTTS][0] = count;
	    STTS[nSTTS][1] = duration;
	    nSTTS++;
	    if( nSTTS>=MAX_STTS )
	    {
	    	fprintf( stdout, "nSTTS over %d\n", nSTTS );
		EXIT();
	    }
	}
	BoxEnd( fp, size, total, 1 );
	return 0;
}

int stss( FILE *fp, int size )
{
int total=8;
int readed;
unsigned long start = g_addr-8;
    fprintf( stdout, "[stss] %8llX : size=%8X (Sync Sample)\n", 
    	g_addr-8, size );
	if( bDumpAtom )
	fprintf( stdout, "=======================\n" );

	Version( fp, &total );

	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int entries = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Number of entries      = %d\n", entries );

	int e;
	for( e=0; e<entries; e++ )
	{
	    if( (g_addr-start)>=size )
	    {
	    	fprintf( stdout, "*** Size over\n" );
		break;
	    }
	// Sample count
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int count = Long(buf);
	    if( bDumpAtom )
	    fprintf( stdout, "%5d : %5d\n", e, count );
	    STSS[nSTSS] = count;
	    nSTSS++;
	    if( nSTSS>=MAX_STSS )
	    {
	    	fprintf( stdout, "nSTSS over %d\n", nSTSS );
		EXIT();
	    }
	}

	if( bDumpAtom )
	fprintf( stdout, "=======================\n" );
	return 0;
} 

int clef( FILE *fp, int size )
{
int total=8;
int readed;
	AtomHead( "clef", size );

	Version( fp, &total );

    // Track width (32) fixed-point
	readed = Gread( buf, 1, 4, fp, &total, NULL );
//	int TrackWidth = Long(buf);
	int WidthH = Short( &buf[0] );
	int WidthL = Short( &buf[2] );
	fprintf( stdout, "Clean width            = %d.%04d\n", 
		WidthH, WidthL );
    // Track height (32)
	readed = Gread( buf, 1, 4, fp, &total, NULL );
//	int TrackHeight = Long(buf);
	int HeightH = Short( &buf[0] );
	int HeightL = Short( &buf[2] );
	fprintf( stdout, "Clean height           = %d.%04d\n", 
		HeightH, HeightL );

	fprintf( stdout, "=======================\n" );
	return 0;
} 

int prof( FILE *fp, int size )
{
int total=8;
int readed;
	AtomHead( "prof", size );

	Version( fp, &total );

    // Track width (32) fixed-point
	readed = Gread( buf, 1, 4, fp, &total, NULL );
//	int TrackWidth = Long(buf);
	int WidthH = Short( &buf[0] );
	int WidthL = Short( &buf[2] );
	fprintf( stdout, "Aperture Width         = %d.%04d\n", 
		WidthH, WidthL );
    // Track height (32)
	readed = Gread( buf, 1, 4, fp, &total, NULL );
//	int TrackHeight = Long(buf);
	int HeightH = Short( &buf[0] );
	int HeightL = Short( &buf[2] );
	fprintf( stdout, "Aperture Height        = %d.%04d\n", 
		HeightH, HeightL );

	fprintf( stdout, "=======================\n" );
	return 0;
} 

int enof( FILE *fp, int size )
{
int total=8;
int readed;
	AtomHead( "enof", size );

	Version( fp, &total );

    // Track width (32) fixed-point
	readed = Gread( buf, 1, 4, fp, &total, NULL );
//	int TrackWidth = Long(buf);
	int WidthH = Short( &buf[0] );
	int WidthL = Short( &buf[2] );
	fprintf( stdout, "Encoded  Width         = %d.%04d\n", 
		WidthH, WidthL );
    // Track height (32)
	readed = Gread( buf, 1, 4, fp, &total, NULL );
//	int TrackHeight = Long(buf);
	int HeightH = Short( &buf[0] );
	int HeightL = Short( &buf[2] );
	fprintf( stdout, "Encoded  Height        = %d.%04d\n", 
		HeightH, HeightL );

	fprintf( stdout, "=======================\n" );
	return 0;
} 


int ctts( FILE *fp, int size )
{
int total=8;
int readed;
unsigned long start = g_addr-8;
	AtomHead( "ctts", size );

	Version( fp, &total );

	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int entries = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Number of entries      = %d\n", entries );

	int e;
	nCTTS=0;
	if( CTTS==NULL )
	{
	    fprintf( stdout, "CTTS=NULL\n" );
	    return -1;
	}
	for( e=0; e<entries; e++ )
	{
	    if( (g_addr-start)>=size )
	    {
	    	fprintf( stdout, "*** Size over\n" );
		break;
	    }
	// Sample count
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int count = Long(buf);
	// compositionOffset
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int offset = Long(buf);
	    if( bCTTS&1 )
	    fprintf( stdout, "%5d : %5d, %5d\n", e, count, offset );
	    CTTS[nCTTS*2+0] = count;
	    CTTS[nCTTS*2+1] = offset;
	    nCTTS++;
	    if( nCTTS>=MAX_CTTS )
	    {
	    	fprintf( stdout, "nCTTS Over : %d\n", nCTTS );
		EXIT();
	    }
	}
	if( bCTTS&2 )
	{
	    if( (nSTTS>0) && (STTS[0]>0) )
	    {
	    	int dts  = 0;
		int pts  = 0;
//		int unit = STTS[0];
		int unit = STTS[0][1];
		for( e=0; e<nCTTS; e++ )
		{
		    int c;
		    int count  = CTTS[e*2+0];
		    int offset = CTTS[e*2+1];
//		    fprintf( stdout, "%4d, %4d (%4d)\n", count, offset, unit );
		    for( c=0; c<count; c++ )
		    {
		    	pts = dts+offset/unit;
		    	fprintf( stdout, "%4d : %4d\n", dts, pts );
			dts++;
		    }
		}
	    } else {
	    	fprintf( stdout, "No STTS\n" );
	    }
	}

	if( bDumpAtom )
	fprintf( stdout, "=======================\n" );
	return 0;
}


int stsd( FILE *fp, int size )
{
int total=8;
int readed;
unsigned long start = g_addr-8;
	AtomHead( "stsd", size );

	Version( fp, &total );

	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int entries = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Number of entries      = %d\n", entries );

	int e;
	for( e=0; e<entries; e++ )
	{
	    if( (g_addr-start)>=size )
	    {
	    	fprintf( stdout, "*** Size over\n" );
		break;
	    }
	// Sample descripiton size
	    int sz;
	    int e_total=4;
	    readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    sz = Long( buf );
	    if( bDumpAtom )
	    fprintf( stdout, "sz=0x%X\n", sz );
	// Data format
	    readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    if( bDumpAtom )
	    {
		if( (buf[2]<' ') || (buf[3]<' ' ) )
		{
		    fprintf( stdout, "Data format            = [%08X] : ", 
			(UINT)Long(buf) );
		} else {
		    fprintf( stdout, "Data format            = [%s] : ", buf );
		}
	    }
	    int bVideo=0;
	    int bAudio=0;
	    if( strncmp( (char *)buf, "jpeg", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "JPEG\n" );
	    	bVideo=1;
		VideoTimeScale = mdhd_TimeScale;
		VideoDuration  = mdhd_Duration;
	    } else if( strncmp( (char *)buf, "rle ", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "Animation\n" );
	    	bVideo=1;
	    } else if( strncmp( (char *)buf, "png ", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "Portable Network Graphics\n" );
	    	bVideo=1;
	    } else if( strncmp( (char *)buf, "mjpa", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "Motion-JPEG(format A)\n" );
	    	bVideo=1;
		VideoTimeScale = mdhd_TimeScale;
		VideoDuration  = mdhd_Duration;
	    } else if( strncmp( (char *)buf, "mjpb", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "Motion-JPEG(format B)\n" );
	    	bVideo=1;
	    } else if( strncmp( (char *)buf, "SVQ1", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "Sorenson video 1\n" );
	    	bVideo=1;
	    } else if( strncmp( (char *)buf, "SVQ3", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "Sorenson video 3\n" );
	    	bVideo=1;
	    } else if( strncmp( (char *)buf, "mp4v", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "MPEG-4 video\n" );
	    	bVideo=1;
		VideoTimeScale = mdhd_TimeScale;
		VideoDuration  = mdhd_Duration;
	    } else if( strncmp( (char *)buf, "avc1", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "H.264 video\n" );
	    	bVideo=1;
		VideoTimeScale = mdhd_TimeScale;
		VideoDuration  = mdhd_Duration;
	    } else if( strncmp( (char *)buf, "dvc ", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "NTSC DV-25 video\n" );
	    	bVideo=1;
	    } else if( strncmp( (char *)buf, "dvcp", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "PAL  DV-25 video\n" );
	    	bVideo=1;
	// ---------------------------------------------
	    } else if( strncmp( (char *)buf, "raw ", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "Uncompressed 8bit audio\n" );
	    	bAudio=1;
	    } else if( strncmp( (char *)buf, "twos", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "16bit big-endian audio\n" );
	    	bAudio=1;
	    } else if( strncmp( (char *)buf, "sowt", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "16bit little-endian audio\n" );
	    	bAudio=1;
	    } else if( strncmp( (char *)buf, "ulaw", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "uLaw 2:1 audio\n" );
	    	bAudio=1;
	    } else if( strncmp( (char *)buf, "alaw", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "aLaw 2:1 audio\n" );
	    	bAudio=1;
	    } else if( strncmp( (char *)buf, "dvca", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "DV Audio\n" );
	    	bAudio=1;
	    } else if( strncmp( (char *)buf, ".mp3", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "MPEG-1 layer 3 Audio\n" );
	    	bAudio=1;
	    } else if( strncmp( (char *)buf, "mp4a", 4 )==0 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "MPEG-4 AAC Audio\n" );
	    	bAudio=1;
	    } else if( strncmp( (char *)buf, "ac-3", 4 )==0 )
	    {
		if( bDumpAtom )
	fprintf( stdout, "Digital Audio Compression Standard(AC-3) Audio\n" );
	    	bAudio=1;
	    } else if( Long( buf )==0x6D730002 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "Microsoft ADPCM\n" );
	    	bAudio=1;
	    } else if( Long( buf )==0x6D730011 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "Intel ADPCM\n" );
	    	bAudio=1;
	    } else if( Long( buf )==0x6D730055 )
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "MP3 CBR only\n" );
	    	bAudio=1;
	    } else if( strncmp( (char *)buf, "MAC6", 4 )==0 ) // 2012.7.4
	    {
		if( bDumpAtom )
	    	fprintf( stdout, "MACE 6:1\n" );
	    	bAudio=1;
	    } else if( strncmp( (char *)buf, "rtp ", 4 )==0 ) // 2012.9.17
	    {	// vibl
		if( bDumpAtom )
	    	fprintf( stdout, "RTP\n" );
	    } else {
		if( bDumpAtom )
	    	fprintf( stdout, "%02X %02X %02X %02X Unknown format\n",
			buf[0], buf[1], buf[2], buf[3] );
		EXIT();
	    }
	// Reserved
	    readed = Gread( buf, 1, 6, fp, &total, &e_total );
	// Data reference index
	    readed = Gread( buf, 1, 2, fp, &total, &e_total );
	    int index= Short( buf );
	    if( bDumpAtom )
	    fprintf( stdout, "Data reference index   = %d\n", index );

	    if( bVideo )
	    {
	    // Version (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
//		int Version=Short(buf);
	    // Revision level (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
	    // Vendor (32)
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
		buf[4] = 0;
	    if( bDumpAtom )
	    {
	    if( buf[0]>=' ' )
		fprintf( stdout, "Verndor                = [%s]\n", buf );
	    else
	    fprintf( stdout, "Verndor                = %02X %02X %02X %02X\n",
	    	buf[0], buf[1], buf[2], buf[3] );
	    }
	    // Temporal quality (32)
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    // Spatial quality (32)
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    // Width (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
		int width = Short( buf );
		if( bDumpAtom )
		fprintf( stdout, "Width                  = %4d\n", width );
	    // Height (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
		int height = Short( buf );
		if( bDumpAtom )
		fprintf( stdout, "Height                 = %4d\n", height );
	    // Horizontal resolution (32)
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
		int h_res = Long( buf );
		if( bDumpAtom )
		fprintf( stdout, "Horizontal resolution  = 0x%X\n", h_res );
	    // Vertial resoltion (32)
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
		int v_res = Long( buf );
		if( bDumpAtom )
		fprintf( stdout, "Vertical   resolution  = 0x%X\n", v_res );
	    // Data size (32)
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
		int dataSize = Long( buf );
		if( bDumpAtom )
		fprintf( stdout, "Data size              = %4d\n", dataSize );
	    // Frame count (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
		int frameCount = Short( buf );
		if( bDumpAtom )
		fprintf( stdout, "Frame count            = %4d\n", frameCount);
	    // Compressor name (32byte)
		readed = Gread( buf, 1, 32, fp, &total, &e_total );
		buf[32] = 0;
		if( bDumpAtom )
		fprintf( stdout, "Compressor name        = " );
		if( bDumpAtom )
		{
		if( buf[0]==0 )
		    fprintf( stdout, "NULL\n" );
		else {
#if 1
		    fprintf( stdout, "[%s]\n", &buf[1] );
#else
		    int ii;
		    fprintf( stdout, "[" );
		    for( ii=0; (ii<buf[0]) && (ii<32); ii++ )
		    {
		    	if( buf[ii+1]<0x7E )
			fprintf( stdout, "%c", buf[ii+1] );
			else
			fprintf( stdout, "(%02X)", buf[ii+1] );
		    }
		    fprintf( stdout, "]\n" );
#endif
		}
		}
	    // Depth (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
		int depth = Short( buf );
		if( bDumpAtom )
		fprintf( stdout, "Depth                  = %2d\n", depth );
	    // Color table ID (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
		int ctable = Short( buf );
		if( bDumpAtom )
		fprintf( stdout, "Color table ID         = %4d\n", ctable );

		while( sz>e_total )
		{
		if( bDumpAtom )
		fprintf( stdout, "size=%d, e_total=%d\n", sz, e_total );
	    // Size
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    	int e_size = Long( buf );
		if( bDumpAtom )
		fprintf( stdout, "Extension size         = 0x%X\n", e_size );
	    // Type
	    	char Type[5];
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
		memcpy( Type, buf, 4 );
		Type[4] = 0;
		if( bDumpAtom )
		fprintf( stdout, "Extension type         = [%s]\n", Type );
	    //
	    	int ii;
		readed = Gread( buf, 1, e_size-8, fp, &total, &e_total );
		if( strcmp( Type, "avcC" )==0 )	// AVC
		{
		    nAVCC = e_size-8;
		    if( nAVCC>=MAX_AVCC )
		    {
		    	fprintf( stdout, "Too large nAVCC(0x%X)\n", nAVCC );
			EXIT();
		    }
		    memcpy( avcC, buf, nAVCC );
		}
		if( strcmp( Type, "mvcC" )==0 )	// MVC
		{
		    nMVCC = e_size-8;
		    memcpy( mvcC, buf, nMVCC );
		    if( nMVCC>=MAX_AVCC )
		    {
		    	fprintf( stdout, "Too large nMVCC(0x%X)\n", nMVCC );
			EXIT();
		    }
		}
		if( strcmp( Type, "esds" )==0 )	// MPEG4
		{
		    nMPEG4 = e_size-8;
		    memcpy( esds, buf, nMPEG4 );
		    if( nMPEG4>=MAX_MPEG4 )
		    {
		    	fprintf( stdout, "Too large nMPEG4(0x%X)\n", nMPEG4 );
			EXIT();
		    }
		}
		if( bDumpAtom )
		{
	    	for( ii=0; ii<(e_size-8); ii++ )
		{
		    fprintf( stdout, "%02X ", buf[ii] );
		    if( (ii%16)==15)
			fprintf( stdout, "\n" );
		}
		if( (ii%16)!=0)
		    fprintf( stdout, "\n" );
		}
		}
		if( bDumpAtom )
		fprintf( stdout, "size=%d, e_total=%d\n", sz, e_total );
	    }
	    if( bAudio )
	    {
	    // Version (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
	    	int Version = Short( buf );
		if( bDumpAtom )
		fprintf( stdout, "Version                = %d\n", Version );
	    // Revision (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
	    	int Revision = Short( buf );
		if( bDumpAtom )
		fprintf( stdout, "Revision               = %d\n", Revision );
	    // Vendor (32)
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    	int Vendor = Long( buf );
		if( bDumpAtom )
		fprintf( stdout, "Vendor                 = %8X ", Vendor );
		if( (buf[0]>' ') && (buf[0]<0x7F) )
		{
		    buf[4] = 0;
		    if( bDumpAtom )
		    fprintf( stdout, "[%s]\n", buf );
		} else {
		    if( bDumpAtom )
		    fprintf( stdout, "\n" );
		}
	    // Number of channels (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
	    	int channels = Short( buf );
		if( bDumpAtom )
		fprintf( stdout, "Number of channels     = %d\n", channels );
	    // Sample size (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
	    	int SampleSize = Short( buf );
		if( bDumpAtom )
		fprintf( stdout, "Sample Size            = %d\n", SampleSize );
	    // Compression ID (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
	    	int CompressionID = Short( buf );
		if( bDumpAtom )
		fprintf( stdout, "Compression ID         = %d\n", 
			CompressionID );
	    // Packet size (16)
		readed = Gread( buf, 1, 2, fp, &total, &e_total );
	    	int PacketSize = Short( buf );
		if( bDumpAtom )
		fprintf( stdout, "PcketSize              = %d\n", PacketSize );
	    // Sample rate(32) fixed-point
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    	int SampleRateH = Short( &buf[0] );
	    	int SampleRateL = Short( &buf[2] );
		if( bDumpAtom )
		fprintf( stdout, "SampleRate             = %d.%03d\n", 
		    SampleRateH, SampleRateL );

		if( Version==1 )
		{
	    // Samples per packet (32)
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    	int SamplesPerPacket = Long( buf );
	    if( bDumpAtom )
	    fprintf( stdout, "Samples per packet     = %d\n", SamplesPerPacket);
	    // Bytes per packet (32)
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    	int BytesPerPacket = Long( buf );
	    if( bDumpAtom )
	    fprintf( stdout, "Bytes   per packet     = %d\n", BytesPerPacket );
	    // Bytes per frame (32)
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    	int BytesPerFrame = Long( buf );
	    if( bDumpAtom )
	    fprintf( stdout, "Bytes   per frame      = %d\n", BytesPerFrame );
	    // Bytes per sample (32)
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    int BytesPerSample = Long( buf );
	    if( bDumpAtom )
	    fprintf( stdout, "Bytes   per sample     = %d\n", BytesPerSample );
		}
		while( sz>e_total )
		{
		if( bDumpAtom )
		fprintf( stdout, "size=%d, e_total=%d\n", sz, e_total );
	    // Size
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
	    	int e_size = Long( buf );
		if( bDumpAtom )
		fprintf( stdout, "Extension size         = 0x%X\n", e_size );
	    // Type
		readed = Gread( buf, 1, 4, fp, &total, &e_total );
		buf[4] = 0;
		if( bDumpAtom )
		fprintf( stdout, "Extension type         = [%s]\n", buf );
	    	int ii;
	    	for( ii=8; ii<e_size; ii++ )
		{
		    readed = Gread( buf, 1, 1, fp, &total, &e_total );
		    if( bDumpAtom )
		    {
		    fprintf( stdout, "%02X ", buf[0] );
		    if( ((ii-8)%16)==15)
			fprintf( stdout, "\n" );
		    }
		}
		if( bDumpAtom )
		if( ((ii-8)%16)!=0)
		    fprintf( stdout, "\n" );
		if( bDumpAtom )
		fprintf( stdout, "size=%d, e_total=%d\n", sz, e_total );
		if( bDumpAtom )
		fprintf( stdout, "-----------------------\n" );
		}
	    }

	    total+=skip( fp, sz-e_total );
	}
	if( bDumpAtom )
	fprintf( stdout, "size=%d, total=%d\n", size, total );

	skip( fp, size-total );
	if( bDumpAtom )
	fprintf( stdout, "=======================\n" );
	return 0;
}

int mdhd( FILE *fp, int size )
{
int readed;
int total=8;

	AtomHead( "mdhd", size );
	if( bDumpAtom )
	fprintf( stdout, "g_addr=%8llX\n", g_addr );

	int version = Version( fp, &total );

    // Creation time (32)->(64)
    	if( version==0 )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int CreationTime = Long(buf);
	    if( bDumpAtom )
	    fprintf( stdout, 
	    	"Creation time          = 0x%X\n", CreationTime );
	} else {
	    readed = Gread( buf, 1, 8, fp, &total, NULL );
	    long long CreationTime = LongLong(buf);
	    if( bDumpAtom )
	    fprintf( stdout, 
	    	"Creation time          = 0x%llX\n", CreationTime );
	}
    // Modification time (32)->(64)
    	if( version==0 )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int ModificationTime = Long(buf);
	    if( bDumpAtom )
	    fprintf( stdout, 
	    	"Modification time      = 0x%X\n", ModificationTime );
	} else {
	    readed = Gread( buf, 1, 8, fp, &total, NULL );
	    long long ModificationTime = LongLong(buf);
	    if( bDumpAtom )
	    fprintf( stdout, 
	    	"Modification time      = 0x%llX\n", ModificationTime );
	}
    // Time scale (32)
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int TimeScale = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Time scale             = %d fps\n", TimeScale );
	mdhd_TimeScale = TimeScale;
	if( bDumpAtom )
	fprintf( stdout, "mdhd_TimeScale=%d\n", mdhd_TimeScale );
    // Duration (32)->(64)
    long long Duration=0;
    	if( version==0 )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    Duration = Long(buf);
	} else {
	    readed = Gread( buf, 1, 8, fp, &total, NULL );
	    Duration = LongLong(buf);
	}
	if( bDumpAtom )
	fprintf( stdout, "Duration               = %lld\n", Duration );
	mdhd_Duration = Duration;
	if( bDumpAtom )
	fprintf( stdout, "mdhd_Duration=%lld\n", mdhd_Duration );
    // Language (16)
	readed = Gread( buf, 1, 2, fp, &total, NULL );
	int Language = Short(buf);
	if( bDumpAtom )
	fprintf( stdout, "Language               = 0x%X\n", Language );
    // Quality (16)
	readed = Gread( buf, 1, 2, fp, &total, NULL );
	int Quality = Short(buf);
	if( bDumpAtom )
	fprintf( stdout, "Quality                = 0x%X\n", Quality );

	BoxEnd( fp, size, total, bDumpAtom );

	return 0;
}

int vmhd( FILE *fp, int size )
{
int readed;
int total=8;

	AtomHead( "vmhd", size );

	Version( fp, &total );

    // Graphics mode (2byte)
	readed = Gread( buf, 1, 2, fp, &total, NULL );
	if( bDumpAtom )
	fprintf( stdout, "Graphics mode          = 0x%02X%02X\n", 
		buf[0], buf[1] );
    // Opcolor (6byte)
	readed = Gread( buf, 1, 6, fp, &total, NULL );
	if( bDumpAtom )
	fprintf( stdout, 
	"Opcolor                = 0x%02X%02X 0x%02X%02X 0x%02X%02X\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5] );
	if( bDumpAtom )
	fprintf( stdout, "=======================\n" );

	return 0;
}

int dref( FILE *fp, int size )
{
int readed;
int total=8;

	AtomHead( "dref", size );

	Version( fp, &total );

    // Number of entries (32bit)
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int entries = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Number of entries      = %d\n", entries );

	int e;
	for( e=0; e<entries; e++ )
	{
#if 0
	    if( (g_addr-start)>=size )
	    {
	    	fprintf( stdout, "*** Size over\n" );
		break;
	    }
#endif
	    // Size
	    int stotal=total;
	    readed = Gread( buf, 1, 4, fp, &total, NULL );

	    int Size = Long(buf);
	    if( bDumpAtom )
	    fprintf( stdout, "%4d : ", e );
	    // Type
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    if( bDumpAtom )
		fprintf( stdout, "[%c%c%c%c] : ", 
	    	buf[0], buf[1], buf[2], buf[3]);
	    if( bDumpAtom )
	    fprintf( stdout, "Size   = 0x%X\n", Size );

	    Version( fp, &total );
	    if( Size>(total-stotal) )
	    {
	    	int n=0;
		fprintf( stdout, "Data = ...\n" );
		while( Size>(total-stotal) )
		{
		    readed = Gread( buf, 1, 1, fp, &total, NULL );
		    fprintf( stdout, "%02X ", (UINT)buf[0] );
		    n++;
		    if( (n&15)==0 )
		    	fprintf( stdout, "\n" );
		}
	    }
	    
	}
	if( bDumpAtom )
	fprintf( stdout, "=======================\n" );

	return 0;
}

int elst( FILE *fp, int size )
{
int readed;
int total=8;

	AtomHead( "elst", size );

	Version( fp, &total );

    // Number of entries (32bit)
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int entries = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Number of entries      = %d\n", entries );

	int e;
	for( e=0; e<entries; e++ )
	{
	    // Track duration
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int duration = Long(buf);
	    // Media time
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int time = Long(buf);
	    // Media rate
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int rate = Long(buf);
	    if( bDumpAtom )
		fprintf( stdout, "%3d : %8d, %8d, %8d\n", 
		    e, duration, time, rate );
	}
	if( bDumpAtom )
	fprintf( stdout, "=======================\n" );

	return 0;
}

int stsz( FILE *fp, int size )
{
int total=8;
int readed;
unsigned long start = g_addr-8;

	AtomHead( "stsz", size );

	Version( fp, &total );

	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int SampleSize = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Sample size            = %d\n", SampleSize );

	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int entries = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Number of entries      = %d\n", entries );

	int e;
	nSTSZ=0;
	if( STSZ==NULL )
	{
	    fprintf( stdout, "STSZ=NULL\n" );
	    return -1;
	}
	for( e=0; e<entries; e++ )
	{
	    if( (g_addr-start)>=size )
	    {
	    	fprintf( stdout, "*** Size over\n" );
		break;
	    }
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int offset = Long( buf );
	    STSZ[nSTSZ++] = offset;
	    if( bDetail )
	    	fprintf( stdout, "%5d : %8X\n", e, offset );
	    if( nSTSZ>=MAX_STSZ )
	    {
	    	fprintf( stdout, "nSTSZ=%d\n", nSTSZ );
		EXIT();
	    }
	}
	BoxEnd( fp, size, total, 0 );
	return 0;
}

int stco( FILE *fp, int size )
{
int total=8;
int readed;
	AtomHead( "stco", size );
	if( bDumpAtom )
	fprintf( stdout, "=======================\n" );

	Version( fp, &total );

	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int entries = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Number of entries      = %d\n", entries );

	int e;
	nSTCO=0;
	for( e=0; e<entries; e++ )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int offset = Long( buf );
	    STCO[nSTCO++] = offset;
	    if( bDetail )
	    	fprintf( stdout, "%5d : %8X\n", e, offset );
	    if( nSTCO>=MAX_STCO )
	    {
	    	fprintf( stdout, "nSTCO=%d\n", nSTCO );
		EXIT();
	    }
	}
	BoxEnd( fp, size, total, 0 );
	return 0;
}

int stsc( FILE *fp, int size )
{
int total=8;
int readed;
	AtomHead( "stsc", size );

	Version( fp, &total );

	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int entries = Long(buf);
	if( bDumpAtom )
	fprintf( stdout, "Number of entries      = %d\n", entries );

	int e;
	nSTSC=0;
	for( e=0; e<entries; e++ )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int chunk = Long( buf );
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int samples = Long( buf );
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    int ID = Long( buf );

	    STSC[nSTSC][0] = chunk;
	    STSC[nSTSC][1] = samples;
	    STSC[nSTSC][2] = ID;
	    nSTSC++;
	    if( bDetail )
	    	fprintf( stdout, "%5d : %6d, %6d, %6d\n", 
			e, chunk, samples, ID );
	    if( nSTSC>=MAX_STSC )
	    {
	    	fprintf( stdout, "nSTSC=%d\n", nSTSC );
		EXIT();
	    }
	}
	BoxEnd( fp, size, total, 0 );
	return 0;
}

int trun( FILE *fp, int Size, int level )
{
int total=8;
int readed=0;
int totalSize = 0;

	fprintf( stdout, "[trun] %8llX : size=%8X \n", g_addr-8, Size );
	fprintf( stdout, "=======================\n" );

	Version( fp, &total );

	readed = Gread( buf, 1, 4, fp, &total, NULL );
	if( readed<4 )
	    return -1;
	int entries = Long(buf);
	fprintf( stdout, "Sample Count = %d\n", entries );

	readed = Gread( buf, 1, 4, fp, &total, NULL );
	if( readed<4 )
	    return -1;
	int offset = Long(buf);
	fprintf( stdout, "offset       = %X\n", offset );

	readed = Gread( buf, 1, 4, fp, &total, NULL );
	if( readed<4 )
	    return -1;
	int flags = Long(buf);
	if( bDetail )
	    fprintf( stdout, "flags=%X\n", (UINT)flags );

	for( nTrun=0; nTrun<entries; nTrun++ )
	{
	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    if( readed<4 )
		break;
	    int duration = Long(buf);

	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    if( readed<4 )
		break;
	    int size = Long(buf);
	    totalSize += size;

	    readed = Gread( buf, 1, 4, fp, &total, NULL );
	    if( readed<4 )
		break;
	    int offset = Long(buf);

	    fprintf( stdout, "Trun[%4d] : %8X, %8X, %8X\n",
	    	nTrun, duration, size, offset );
	    if( nTrun<MAX_TRUN )
	    {
		Trun[nTrun][0] = duration;
		Trun[nTrun][1] = size;
		Trun[nTrun][2] = offset;
		Trun[nTrun][3] = (nTrun==0) ? 1 : 0;
	    }
	}
	fprintf( stdout, "totalSize = %X\n", totalSize );
	return 0;
}

int traf( FILE *fp, int Size, int level )
{
int total=8;
int readed=0;
long long size=0;
unsigned char buffer[1024*1024];

	Level( level );
	fprintf( stdout, "[traf] %8llX : size=%8X \n", g_addr-8, Size );
	fprintf( stdout, "=======================\n" );

	level++;
	while( total<Size )
	{
	    memset( buffer, 0, 1024 );
	    readed = Gread( buffer, 1, 4, fp, &total, NULL );
	    if( readed<4 )
	    	break;
	    size = Long( buffer );

	    readed = Gread( buffer, 1, 4, fp, &total, NULL );
	    if( readed<4 )
	    	break;
	    if( strncmp( (char *)buffer, "XXXX", 4 )==0 )
	    {
	    	SkipAtom( "XXXX", fp, size, level );
		total += (size-8);
	    } else if( strncmp( (char *)buffer, "tfhd", 4 )==0 )
	    {
	    	SkipAtom( "tfhd", fp, size, level );
		total += (size-8);
	    } else if( strncmp( (char *)buffer, "tfdt", 4 )==0 )
	    {
	    	SkipAtom( "tfdt", fp, size, level );
		total += (size-8);
	    } else if( strncmp( (char *)buffer, "trun", 4 )==0 )
	    {
		Level( level );
	    	trun( fp, size, level );
//	    	SkipAtom( "trun", fp, size, level );
		total += (size-8);
	    } else if( strncmp( (char *)buffer, "sdtp", 4 )==0 )
	    {
	    	SkipAtom( "sdtp", fp, size, level );
		total += (size-8);
	    } else if( strncmp( (char *)buffer, "sbgp", 4 )==0 )
	    {
	    	SkipAtom( "sbgp", fp, size, level );
		total += (size-8);
	// ---------------------------------------------
	    } else {
	    	fprintf( stdout, "[%s] %8llX : size=%8llX : Unknown\n", 
			buffer, (g_addr-4), size );
		break;
	    }
	}
	BoxEnd( fp, size, total, 0 );
	return 0;
}

int moof( FILE *fp, int Size, int level )
{
int total=8;
int readed=0;
long long size=0;
unsigned char buffer[1024*1024];

	fprintf( stdout, "[moof] %8llX : size=%8X \n", g_addr-8, Size );
	fprintf( stdout, "=======================\n" );

/*
	Version( fp, &total );
	readed = Gread( buf, 1, 4, fp, &total, NULL );
	int entries = Long(buf);
*/
//	Level( level );
	level++;
	while( total<Size )
	{
	    memset( buffer, 0, 1024 );
	    readed = Gread( buffer, 1, 4, fp, &total, NULL );
	    if( readed<4 )
	    	break;
	    size = Long( buffer );

	    readed = Gread( buffer, 1, 4, fp, &total, NULL );
	    if( readed<4 )
	    	break;
	    if( strncmp( (char *)buffer, "XXXX", 4 )==0 )
	    {
	    	SkipAtom( "XXXX", fp, size, level );
		total += (size-8);
	    } else if( strncmp( (char *)buffer, "mfhd", 4 )==0 )
	    {
	    	SkipAtom( "mfhd", fp, size, level );
		total += (size-8);
	    } else if( strncmp( (char *)buffer, "traf", 4 )==0 )
	    {
	    	traf( fp, size, level );
//	    	SkipAtom( "traf", fp, size, level );
		total += (size-8);
/*
	    } else if( strncmp( (char *)buffer, "mdat", 4 )==0 )
	    {
		Level( level );
		mdat( fp, size );
*/
	// ---------------------------------------------
	    } else {
	    	fprintf( stdout, "[%s] %8llX : size=%8llX : Unknown\n", 
			buffer, (g_addr-4), size );
		break;
	    }
#if 0
	    if( Size!=0 )
	    if( (g_addr-start)>=Size )
	    {
	    	int i;
		for( i=0; i<level; i++ )
		{
		    fprintf( stdout, "-" );
		}
		fprintf( stdout, "%s Done (%8llX/%8X)\n", 
			title, (g_addr-start), (UINT)Size );
/*
		if( strcmp( title, "minf" )==0 )
		{
		}
*/
	    	return 0;
	    }
#endif
	}
	BoxEnd( fp, size, total, 0 );
	return 0;
}

static int SttsCount=0;
static int SttsCur  =0;
static int SttsDelta=0;

int GetDeltaDTS( int *pStts )
{
int stts = *pStts;

	if( SttsCur<SttsCount )
	{
	    SttsCur++;
	} else {
	    if( stts < nSTTS )
	    {
	    	SttsCount = STTS[stts][0];
	    	SttsDelta = STTS[stts][1];
		stts++;
		SttsCur   = 1;
	    } else {
	    	fprintf( stdout, "stts table over\n" );
	    }
	}
	*pStts = stts;
#if 0
	if( SttsDelta==0 )
	{
	    fprintf( stdout, "GetDeltaDTS():stts=%d, SttsCur=%d\n",
	    	stts, SttsCur );
	}
#endif
	return SttsDelta;
}

static int CttsCount=0;
static int CttsCur  =0;
static int CttsDelta=0;

int GetDeltaPTS( int *pCtts )
{
int ctts = *pCtts;

	if( CTTS==NULL )
	{
	    fprintf( stdout, "CTTS=NULL\n" );
	    return -1;
	}
	if( CttsCur<CttsCount )
	{
	    CttsCur++;
	} else {
	    if( ctts < nCTTS )
	    {
	    	CttsCount = CTTS[ctts*2+0];
	    	CttsDelta = CTTS[ctts*2+1];
		ctts++;
		CttsCur   = 1;
	    } else {
	    	fprintf( stdout, "ctts table over(%d>=%d)\n",
			ctts, nCTTS );
	    }
	}
	*pCtts = ctts;
	return CttsDelta;
}

int InitDeltaFunc( )
{
    SttsCount=0;
    SttsCur  =0;
    SttsDelta=0;
    CttsCount=0;
    CttsCur  =0;
    CttsDelta=0;
    return 0;
}

// ------------------------------------------------------------

int ParseMOV( char *title, FILE *fp, unsigned long Size, int level )
{
unsigned char buffer[1024*1024];
long long size=0;
int readed=0;

unsigned long start = g_addr-8;

	if( STSZ==NULL )
	{
	    fprintf( stdout, "STSZ=NULL\n" );
	    return -1;
	}
	if( strcmp( title, "top" )!=0 )
	    header( title, Size, level );
	level++;
	while( 1 )
	{
	    memset( buffer, 0, 1024 );
	    readed = gread( buffer, 1, 4, fp );
	    if( readed<4 )
	    	break;
//	    	EXIT();
	    size = Long( buffer );
	    readed = gread( buffer, 1, 4, fp );
	    if( readed<4 )
//	    	EXIT();
	    	break;
	    if( strncmp( (char *)buffer, "ftyp", 4 )==0 )
	    {
	    	SkipAtom( "ftyp", fp, size, level );
	    } else if( strncmp( (char *)buffer, "mdat", 4 )==0 )
	    {
		Level( level );
		mdat( fp, size );
	    } else if( strncmp( (char *)buffer, "moov", 4 )==0 )
	    {
		ParseMOV( (char *)"moov", fp, size, level );
	    } else if( strncmp( (char *)buffer, "wide", 4 )==0 )
	    {
	    	header( (char *)"wide", size, level );
	    } else if( strncmp( (char *)buffer, "mvhd", 4 )==0 ) // 
	    {
		Level( level );
		mvhd( fp, size );
	    } else if( strncmp( (char *)buffer, "trak", 4 )==0 ) // 
	    {
	    	ParseMOV( (char *)"trak", fp, size, level );
	    } else if( strncmp( (char *)buffer, "tkhd", 4 )==0 ) // 
	    {
		Level( level );
		tkhd( fp, size );
	    } else if( strncmp( (char *)buffer, "edts", 4 )==0 ) // 
	    {
	    	header( (char *)"edts", size, level );
	    } else if( strncmp( (char *)buffer, "elst", 4 )==0 ) // 
	    {
		Level( level );
//	    	SkipAtom( (char *)"elst", fp, size, level );
		elst( fp, size );
	    } else if( strncmp( (char *)buffer, "mdia", 4 )==0 ) // 
	    {
	    	ParseMOV( (char *)"mdia", fp, size, level );
	    } else if( strncmp( (char *)buffer, "mdhd", 4 )==0 ) // 
	    {
		Level( level );
		mdhd( fp, size );
	    } else if( strncmp( (char *)buffer, "hdlr", 4 )==0 ) // 
	    {
		Level( level );
		hdlr( fp, size );
	    } else if( strncmp( (char *)buffer, "minf", 4 )==0 ) // 
	    {
		ParseMOV( (char *)"minf", fp, size, level );
	    } else if( strncmp( (char *)buffer, "vmhd", 4 )==0 ) // 
	    {
		Level( level );
		vmhd( fp, size );
	    } else if( strncmp( (char *)buffer, "dinf", 4 )==0 ) // 
	    {
	    	header( (char *)"dinf", size, level );
	    } else if( strncmp( (char *)buffer, "dref", 4 )==0 ) // 
	    {
		Level( level );
		dref( fp, size );
	    } else if( strncmp( (char *)buffer, "stbl", 4 )==0 ) // 
	    {
	    	header( (char *)"stbl", size, level );
	    } else if( strncmp( (char *)buffer, "stsd", 4 )==0 ) // 
	    {
		Level( level );
		stsd( fp, size );
	    } else if( strncmp( (char *)buffer, "stts", 4 )==0 ) // 
	    {
//	    	SkipAtom( "stts", fp, size, level );
		Level( level );
		stts( fp, size );
	    } else if( strncmp( (char *)buffer, "stsc", 4 )==0 ) // 
	    {
//	    	SkipAtom( "stsc", fp, size, level );
		Level( level );
		stsc( fp, size );
	    } else if( strncmp( (char *)buffer, "stsz", 4 )==0 ) // 
	    {	// Sample Size atom
		Level( level );
		stsz( fp, size );
	    } else if( strncmp( (char *)buffer, "stco", 4 )==0 ) // 
	    {	// Chunk Offset atom
		Level( level );
		stco( fp, size );
	    } else if( strncmp( (char *)buffer, "udta", 4 )==0 ) // 
	    {
	    	SkipAtom( "udta", fp, size, level );
	    } else if( strncmp( (char *)buffer, "smhd", 4 )==0 ) // 
	    {
	    	SkipAtom( "smhd", fp, size, level );
	    } else if( strncmp( (char *)buffer, "stss", 4 )==0 ) // 
	    {
//	    	SkipAtom( "stss", fp, size, level );
		Level( level );
		stss( fp, size );
	    } else if( strncmp( (char *)buffer, "tapt", 4 )==0 ) // netflix
	    {
	    	header( (char *)"tapt", size, level );
	    } else if( strncmp( (char *)buffer, "clef", 4 )==0 ) // netflix
	    {
//	    	SkipAtom( "clef", fp, size, level );
		Level( level );
		clef( fp, size );
	    } else if( strncmp( (char *)buffer, "prof", 4 )==0 ) // netflix
	    {
//	    	SkipAtom( "prof", fp, size, level );
		Level( level );
		prof( fp, size );
	    } else if( strncmp( (char *)buffer, "enof", 4 )==0 ) // netflix
	    {
//	    	SkipAtom( "enof", fp, size, level );
		Level( level );
		enof( fp, size );
	    } else if( strncmp( (char *)buffer, "sdtp", 4 )==0 ) // netflix
	    {
	    	SkipAtom( "sdtp", fp, size, level );
	    } else if( strncmp( (char *)buffer, "meta", 4 )==0 ) // netflix
	    {
	    	header( (char *)"meta", size, level );
	    } else if( strncmp( (char *)buffer, "keys", 4 )==0 ) // netflix
	    {
	    	SkipAtom( "keys", fp, size, level );
	    } else if( strncmp( (char *)buffer, "ilst", 4 )==0 ) // netflix
	    {
	    	SkipAtom( "ilst", fp, size, level );
	    } else if( strncmp( (char *)buffer, "pifo", 4 )==0 ) // mvc
	    {
	    	SkipAtom( "pifo", fp, size, level );
	    } else if( strncmp( (char *)buffer, "sifo", 4 )==0 ) // mvc
	    {
	    	SkipAtom( "sifo", fp, size, level );
	    } else if( strncmp( (char *)buffer, "satt", 4 )==0 ) // mvc
	    {
	    	SkipAtom( "satt", fp, size, level );
	    } else if( strncmp( (char *)buffer, "mvci", 4 )==0 ) // mvc
	    {
	    	SkipAtom( "mvci", fp, size, level );
	    } else if( strncmp( (char *)buffer, "ctts", 4 )==0 ) // mvc
	    {
//	    	SkipAtom( "ctts", fp, size, level );
		Level( level );
		ctts( fp, size );
	    } else if( strncmp( (char *)buffer, "free", 4 )==0 ) // MP4
	    {
	    	SkipAtom( "free", fp, size, level );
	    } else if( strncmp( (char *)buffer, "pnot", 4 )==0 ) // olympus
	    {
	    	SkipAtom( "pnot", fp, size, level );
	    } else if( strncmp( (char *)buffer, "PICT", 4 )==0 ) // olympus
	    {
	    	SkipAtom( "PICT", fp, size, level );
	    } else if( strncmp( (char *)buffer, "iods", 4 )==0 ) // ambarella
	    {
	    	SkipAtom( "iods", fp, size, level );
	// ---------------------------------------------
	// VWG stream
	    } else if( strncmp( (char *)buffer, "uuid", 4 )==0 ) // ambarella
	    {
	    	SkipAtom( "uuid", fp, size, level );
	    } else if( strncmp( (char *)buffer, "co64", 4 )==0 ) // chunk offset
	    {
	    	SkipAtom( "co64", fp, size, level );
	// ---------------------------------------------
	// Netflix MVC stream
	    } else if( strncmp( (char *)buffer, "sgpd", 4 )==0 )
	    {
	    	SkipAtom( "sgpd", fp, size, level );
	    } else if( strncmp( (char *)buffer, "vwid", 4 )==0 )
	    {
	    	SkipAtom( "vwid", fp, size, level );
	    } else if( strncmp( (char *)buffer, "mvex", 4 )==0 )
	    {
	    	SkipAtom( "mvex", fp, size, level );
	    } else if( strncmp( (char *)buffer, "sidx", 4 )==0 )
	    {
	    	SkipAtom( "sidx", fp, size, level );
	    } else if( strncmp( (char *)buffer, "moof", 4 )==0 )
	    {
//	    	SkipAtom( "moof", fp, size, level );
		Level( level );
		moof( fp, size, level );
	    } else if( strncmp( (char *)buffer, "mfra", 4 )==0 )
	    {
	    	SkipAtom( "mfra", fp, size, level );
	    } else if( strncmp( (char *)buffer, "tref", 4 )==0 )// vibl
	    {
	    	SkipAtom( "tref", fp, size, level );
	    } else if( strncmp( (char *)buffer, "hmhd", 4 )==0 )// vibl
	    {
	    	SkipAtom( "hmhd", fp, size, level );
	// ---------------------------------------------
	    } else {
	    	fprintf( stdout, "[%s] %8llX : size=%8llX : Unknown\n", 
			buffer, (g_addr-4), size );
		break;
//		EXIT();
	    }
	    if( Size!=0 )
	    if( (g_addr-start)>=Size )
	    {
	    	int i;
		for( i=0; i<level; i++ )
		{
		    fprintf( stdout, "-" );
		}
		fprintf( stdout, "%s Done (%8llX/%8X)\n", 
			title, (g_addr-start), (UINT)Size );
		if( strcmp( title, "minf" )==0 )
		{
		    if( bDumpAtom )
		    fprintf( stdout, "MINF(%s)\n", CompType );
		    if( strcmp( CompType, "vide" )==0 )
		    {
			fprintf( stdout, "Video Info Done\n" );
			VideoSamples = nSTSZ;
#if 0
			STCO[nSTCO] = 0xFFFFFFFF;
			if( (nSTSZ>0) && (nSTCO>0) )
			{
			    int sz=0, co=0;
			    unsigned long offset=0;
			    if( sz<nSTSZ )
			    for( co=0; co<nSTCO; co++ )
			    {
				offset = STCO[co];
			    	while( (offset+STSZ[sz])<=STCO[co+1] )
				{
				    size   = STSZ[sz];
				    int nOffset = offset+size;
				    fprintf( stdout, 
				    "(%4d,%4d) %8X %8X : %8X\n",
				    	co, sz, offset, size, nOffset );
				    Video[nVideo*VIDEO_UNIT+0] = offset;
				    Video[nVideo*VIDEO_UNIT+1] = size;
				    nVideo++;
				    offset = nOffset;
				    sz++;
				    if( sz>=nSTSZ )
					break;
				}
			    }
			}
#else
			fprintf( stdout, "nSTSZ=%d\n", nSTSZ );
			fprintf( stdout, "nSTCO=%d\n", nSTCO );
			fprintf( stdout, "nSTSC=%d\n", nSTSC );
			fprintf( stdout, "nSTSS=%d\n", nSTSS );
			fprintf( stdout, "nCTTS=%d\n", nCTTS );
			if( (nSTSZ>0) && (nSTCO>0) && (nSTSC>0) )
			{
/*
	    STSC[nSTSC][0] = chunk;
	    STSC[nSTSC][1] = samples;
	    STSC[nSTSC][2] = ID;
*/
			    int sc, co, sz=0;
			    int stts=0;
			    int ctts=0;
			    InitDeltaFunc( );
			    STSC[nSTSC][0] = 0x7FFFFFFF;
			    int ss=0;
			    unsigned int frame=1;
			    for( sc=0; sc<nSTSC; sc++ )
			    {
			    	int samples = STSC[sc  ][1];
			    	int st      = STSC[sc  ][0];
			    	int en      = STSC[sc+1][0];
				if( en>(nSTCO+1) )
				    en = nSTCO+1;
				if( st>en )
				{
				    fprintf( stdout, 
				    "Invalid(%d) st=%d, en=%d\n",
				    	sc, st, en );
				    EXIT();
				}
				int bFirst=1;
				for( co=st; co<en; co++ )
				{
				    int s;
				    unsigned long addr = STCO[co-1];
				    for( s=0; s<samples; s++ )
				    {
				    	unsigned long nAddr = addr + STSZ[sz];
				// start, size, end
//				if( bShowSTSZ | bFirst )
				if( bShowSTSZ )
				fprintf( stdout, 
				    "(%3d,%4d,%5d,%2d) %8X %8X %8X\n",
				    sc, co, sz, s,
				    (UINT)addr, (UINT)STSZ[sz], (UINT)nAddr );

					Video[nVideo*VIDEO_UNIT+0] = addr;
					Video[nVideo*VIDEO_UNIT+1] = STSZ[sz];
					Video[nVideo*VIDEO_UNIT+2] = 0;
					int deltaDTS = GetDeltaDTS( &stts );
					Video[nVideo*VIDEO_UNIT+3] = deltaDTS;
					int deltaPTS = deltaDTS;
					if( nCTTS>0 )
					{
					deltaPTS = GetDeltaPTS( &ctts );
					} else {
					if( bFirst && (s==0) )
				fprintf( stdout, "deltaDTS=%d\n", deltaDTS );
					}
					Video[nVideo*VIDEO_UNIT+4] = deltaPTS;
					if( nSTSS==0 )
					{
					    Video[nVideo*VIDEO_UNIT+2] = 1;
					} else {
					    if( frame==STSS[ss] )
					    {
						Video[nVideo*VIDEO_UNIT+2] = 1;
						ss++;
					    }
					}

					nVideo++;

		    			addr = nAddr;
					sz++;
					frame++;
					if( sz>nSTSZ )
					    break;
				    }
				    if( sz>nSTSZ )
					break;
				    bFirst=0;
				}
				if( sz>nSTSZ )
				    break;
			    }
			} else {
			    fprintf( stdout, 
				"No STSZ or STCO or STSC(%d,%d,%d)\n",
				nSTSZ, nSTCO, nSTSC );
			}
#endif
		    }
		}

	    	return 0;
	    }
	}
	return 0;
}

// ---------------------------------------------
void movUsage( char *prg )
{
	fprintf( stdout, "%s filename [+Mjpeg +Avc +4(mpeg4)]\n", prg );
	exit( 1 );
}

#define OUTPUT_NONE	0
#define OUTPUT_JPEG	1
#define OUTPUT_MJPEG	2
#define OUTPUT_AVC	3
#define OUTPUT_MPEG4	4

int movMain( int argc, char *argv[] )
{
int i;
int args=0;
char *filename=NULL;
int bVerbose=0;
int bOutput=0;
int Output=OUTPUT_NONE;
char *outfile=NULL;

//	unsigned int Video[1024*1024*VIDEO_UNIT];
	Video = (unsigned int *)malloc( 1024*1024*VIDEO_UNIT );
	if( Video==NULL )
	{
	    fprintf( stdout, "Can't malloc Video\n" );
	    exit( 1 );
	}
	STSZ = (unsigned int *)malloc( MAX_STSZ*4 );
	if( STSZ==NULL )
	{
	    fprintf( stdout, "Can't malloc STSZ\n" );
	    exit( 1 );
	}
	CTTS = (unsigned int *)malloc( MAX_CTTS*4*2 );
	if( CTTS==NULL )
	{
	    fprintf( stdout, "Can't malloc CTTS\n" );
	    exit( 1 );
	}

	for( i=0; i<MAX_FORCE_FRAMERATES; i++ )
	{
	    forceFramerates[i] = (-1);
	}
	nForceFramerate = 0;
	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case 'D' :
		    bDumpAtom = 1;
		    break;
		case 'V' :
		    bVerbose = 1;
		    break;
		case 'S' :
		    bKeySPS = 0;
		    break;
		case 'A' :
		    nAVC=atoi(&argv[i][2]);
		    fprintf( stdout, "nAVC=%d\n", nAVC );
		    break;
		case 'M' :
		    nMVC=atoi(&argv[i][2]);
		    fprintf( stdout, "nMVC=%d\n", nMVC );
		    break;
		default :
		    fprintf( stdout, "Unknown Option [%s]\n", argv[i] );
		    movUsage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'E' :
		    bOutputFrame=1;
		    break;
		case 'F' :
		    forceFramerates[nForceFramerate] = atoi(&argv[i][2]);
		    fprintf( stdout, "forceFramerate[%d]=%d\n",
		    	nForceFramerate, forceFramerates[nForceFramerate] );
		    nForceFramerate++;
		    break;
		case 'V' :
		    bVersion = 1;
		    break;
		case 'S' :
		    bDetail=1;
		    break;
		case 'C' :
		    bCTTS|=1;
		    break;
		case 'c' :
		    bCTTS|=2;
		    break;
		case 'Z' :
		    bShowSTSZ = 1;
		case 'J' :
		    bOutput=1;
		    Output=OUTPUT_JPEG;
		    break;
		case 'M' :
		    bOutput=1;
		    Output=OUTPUT_MJPEG;
		    break;
		case 'A' :
		    bOutput=1;
		    Output=OUTPUT_AVC;
		    break;
		case '4' :
		    bOutput=1;
		    Output=OUTPUT_MPEG4;
		    break;
		default :
		    movUsage( argv[0] );
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
		    movUsage( argv[0] );
		    break;
		}
		args++;
	    }
	}

	if( filename==NULL )
	{
	    movUsage( argv[0] );
	}
	FILE *fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open [%s]\n", filename );
	    exit( 1 );
	}

	ParseMOV( (char *)"top", fp, 0, 0 );

	fclose( fp );

	if( bOutput )
	{
	    int total=0;
	    long long delta=0;
	    char dstFile[1024];
	    int nCount=0;
	    fp = fopen( filename, "rb" );
	    if( fp==NULL )
	    {
		fprintf( stdout, "Can't open [%s]\n", filename );
		exit( 1 );
	    }
	    if( Output==OUTPUT_JPEG )
	    {
		fprintf( stdout, "Start Output JPEG files\n" );
		for( i=0; i<nVideo; i++ )
		{
		    int offset=Video[i*VIDEO_UNIT+0];
		    int size  =Video[i*VIDEO_UNIT+1];
		    sprintf( dstFile, "%05d.jpg", i );
		    FILE *out = fopen( dstFile, "wb" );
		    if( out==NULL )
		    {
			fprintf( stdout, "Can't open [%s]\n", dstFile );
			exit( 1 );
		    }
		    fseek( fp, offset, SEEK_SET );
		    if( bVerbose )
		    {
		    fprintf( stdout, "%s : %8X %8X\n", dstFile, offset, size );
		    }
		    if( Copy( out, fp, size )==0 )
			nCount++;
		    fclose( out );
		    total+=size;
		}
		fprintf( stdout, "%d JPEG files\n", nCount );
	    } else if( Output==OUTPUT_MJPEG )
	    {
	    	int unit=0;
		fprintf( stdout, "Samples =%d\n", VideoSamples );
		fprintf( stdout, "Duration=%lld\n", VideoDuration );
	    	fprintf( stdout, "VideoTimeScale=%d\n", VideoTimeScale );
	    	if( VideoSamples>0 )
		    unit = VideoDuration/VideoSamples;
		else
		    exit( 1 );
		fprintf( stdout, "Unit    =%d\n", unit );
	    	int PTS, DTS;
		fprintf( stdout, "Start Output SPES\n" );
		if( outfile )
		    sprintf( dstFile, outfile );
		else
		    sprintf( dstFile, "output.spes" );
	    	FILE *out = fopen( dstFile, "wb" );
		DTS = 0;
		for( i=0; i<nVideo; i++ )
		{
		    int offset = Video[i*VIDEO_UNIT+0];
		    int size   = Video[i*VIDEO_UNIT+1];
//		    int key    = Video[i*VIDEO_UNIT+2];
		    PTS = DTS;

		    fseek( fp, offset, SEEK_SET );
		    MJPEG( out, fp, size, DTS, PTS );

//		    DTS += 1*90000*1000/VideoTimeScale;
		    DTS += 1*90000*unit/VideoTimeScale;
		    total+=size;
		}
		fclose( out );
		fprintf( stdout, "File : %s\n", dstFile );
		fprintf( stdout, "%d frames outputted\n", nVideo );
	    } else if( Output==OUTPUT_AVC )
	    {
	    	int unit=0;
		fprintf( stdout, "Samples =%d\n", VideoSamples );
		fprintf( stdout, "Duration=%lld\n", VideoDuration );
	    	fprintf( stdout, "VideoTimeScale=%d\n", VideoTimeScale );
		if( (VideoSamples==0) && (nVideo>0) )
		{
		    VideoSamples = nVideo;
		    fprintf( stdout, "Samples =%d\n", VideoSamples );
		}
	    	if( VideoSamples>0 )
		    unit = VideoDuration/VideoSamples;
		else
		    exit( 1 );
		fprintf( stdout, "Unit    =%d\n", unit );
		if( VideoTimeScale>0 )
		    delta = (long long)90000*(long long)unit/VideoTimeScale;
		fprintf( stdout, "delta =%d\n", (int)delta );
	    	int PTS, DTS;
		fprintf( stdout, "Start Output SPES\n" );
		if( outfile )
		    sprintf( dstFile, outfile );
		else
		    sprintf( dstFile, "output.spes" );
	    	FILE *out = fopen( dstFile, "wb" );
		DTS = 0;
	    	int DTS0 = DTS;
		int nDTS = DTS;
		int maxSize = 0;
		int SPS_count=0;
		for( i=0; i<nVideo; i++ )
		{
		    int bPTS=0;
		    int offset   = Video[i*VIDEO_UNIT+0];
		    int size     = Video[i*VIDEO_UNIT+1];
		    int key      = Video[i*VIDEO_UNIT+2];
		    int deltaDTS = Video[i*VIDEO_UNIT+3];
		    int deltaPTS = Video[i*VIDEO_UNIT+4];
		    if( (deltaDTS>0) || (deltaPTS>0) )
		    {
		    	DTS  = nDTS;
			if( VideoTimeScale>0 )
			    delta = (long long)deltaDTS*(long long)90000
				  /VideoTimeScale;
			nDTS = DTS+delta;
			if( VideoTimeScale>0 )
			    delta = (long long)deltaPTS*(long long)90000
				  /VideoTimeScale;
			PTS  = DTS+delta;
			bPTS = 1;
//			fprintf( stdout, "DTS=%8d, PTS=%8d\n", DTS, PTS );
		    } else {
#if 0
		    	fprintf( stdout, "nVideo=%d : deltaDTS=%d\n", 
				i, deltaDTS );
#endif
			delta = i*(long long)90000*(long long)unit
			      /VideoTimeScale;
			DTS = DTS0 + delta;
			delta = 2*90000*(long long)unit/VideoTimeScale;
			PTS = DTS+delta;
		    }

		    fseek( fp, offset, SEEK_SET );
		    if( (i==0) || (bKeySPS && (key==1)) )
		    {
#if 0
			if( nMVCC>0 )
			{
			    fprintf( stdout, "use mvcC:" );
			    SPS( out, mvcC, nMVCC, frame_rate );
			} else {
			    fprintf( stdout, "use avcC:" );
			    SPS( out, avcC, nAVCC, frame_rate );
			}
#else
			fprintf( stdout, "use avcC:" );
			if( nForceFramerate>0 )
			{
			    fprintf( stdout, "frc(%d/%d)=%d\n",
			    	SPS_count, nForceFramerate,
				forceFramerates[SPS_count%nForceFramerate] );
			    SPS( out, avcC, nAVCC, 
			    	forceFramerates[SPS_count%nForceFramerate] );
			} else {
			    int n;
			    for( n=0; n<nAVC; n++ )
			    {
				SPS( out, avcC, nAVCC, -1 );
			    }
			}
			if( nMVCC>0 )
			{
			    int n;
			    fprintf( stdout, "use mvcC:" );
			    for( n=0; n<nMVC; n++ )
			    {
				SPS( out, mvcC, nMVCC, -1 );
			    }
			}
			if( nAVC<0 )
			{
			    int n;
			    for( n=0; n<-nAVC; n++ )
			    {
				SPS( out, avcC, nAVCC, -1 );
			    }
			}
			SPS_count++;
#endif
			AVC( out, fp, size, DTS, PTS );
		    } else {
			if( bPTS )
			    AVC( out, fp, size, DTS, PTS );
			else
			    AVC( out, fp, size, DTS, -1 );
		    }
		    if( size>maxSize )
		    	maxSize = size;
		    total+=size;
		}
		fclose( out );
		fprintf( stdout, "File : %s\n", dstFile );
		fprintf( stdout, "%d frames outputted\n", nVideo );
		fprintf( stdout, "Max frame size = %d\n", maxSize );
	    } else if( Output==OUTPUT_MPEG4 )
	    {
	    	int unit=0;
		fprintf( stdout, "Samples =%d\n", VideoSamples );
		fprintf( stdout, "Duration=%lld\n", VideoDuration );
	    	fprintf( stdout, "VideoTimeScale=%d\n", VideoTimeScale );
	    	if( VideoSamples>0 )
		    unit = VideoDuration/VideoSamples;
		else
		    exit( 1 );
		fprintf( stdout, "Unit    =%d\n", unit );
		fprintf( stdout, "Start Output SPES\n" );
		if( outfile )
		    sprintf( dstFile, outfile );
		else
		    sprintf( dstFile, "output.spes" );
	    	FILE *out = fopen( dstFile, "wb" );
	    	MPEG4H( out, esds, nMPEG4 );
		int DTS = 0;
		for( i=0; i<nVideo; i++ )
		{
		    int offset = Video[i*VIDEO_UNIT+0];
		    int size   = Video[i*VIDEO_UNIT+1];
//		    int key    = Video[i*VIDEO_UNIT+2];
		    fseek( fp, offset, SEEK_SET );
		    MPEG4( out, fp, size, DTS, -1 );

		    DTS += 1*90000*unit/VideoTimeScale;
		    total+=size;
		}
		fclose( out );
		fprintf( stdout, "File : %s\n", dstFile );
		fprintf( stdout, "%d frames outputted\n", nVideo );
	    }
	    fclose( fp );
	    fprintf( stdout, "Total   = %d bytes\n", total );
	    if( nVideo>0 )
	    fprintf( stdout, "Average = %d bytes/frame\n",
	    	total/nVideo );
	} else {
	    if( nVideo>0 )
	    	fprintf( stdout, "%d frames\n", nVideo );
	}
	return 0;
}

