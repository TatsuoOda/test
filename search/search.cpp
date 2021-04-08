/*
	search.cpp
		2008.4.21  by T.Oda
		2008.5.7 : CheckText: buf = unsigned char
		2008.6.1 : Add -E option (extension)
		2008.7.3 : Debug --option
		2009.11.11 : use lstat
*/

#include <stdio.h>

#include <stdlib.h>	// for exit

#include <sys/types.h>	// for opendir
#include <dirent.h>	// for opendir

#include <sys/stat.h>	// for fstat
#include <unistd.h>

#include <string.h>	// for strcmp

void Usage( char *prg )
{
	fprintf( stdout, "%s directoryName [/V]\n", prg );
	exit( 1 );
}

#define KEYWORDS 32

#define MODE_AND	0
#define MODE_OR		1

int nFiles=0;
int nDirs=0;
int nText=0;
int bVerbose=0;
int mode=MODE_AND;
int bFile=0;

int CheckText( char *filename, int size )
{
int i;
int bText=0;
//char buf[4096];
unsigned char buf[4096];
int readSize = (size>4095) ? 4096 : size;

FILE *fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
//	    fprintf( stdout, "Can't open [%s]\n", filename );
//	    exit( 1 );
	    return 0;
	}

	int readed = fread( buf, 1, readSize, fp );
	for( i=0; i<readed; i++ )
	{
	    if( buf[i]<8 )
	    {
	    	if( bVerbose>2 )
		    fprintf( stdout, "%s contains [%02X]\n", 
		    	filename, buf[i] );
	    	break;
	    }
	}
	if( i==readed )
	{
	    if( bVerbose>2 )
		fprintf( stdout, "%s is text\n", filename );
	    bText++;
	    nText++;
	}
	fclose( fp );

	return bText;
}

static char CfgList[][64] = {
#if 0
"CFG_GLOBAL_VC03",
"CFG_GLOBAL_VC03",
"CFG_GLOBAL_VC03_SMI",
"CFG_GLOBAL_VC03_SUPPORT",
"CFG_GLOBAL_OMX_SUPPORT",
"CFG_GLOBAL_OMXCTL_SUPPORT",
"CFG_GLOBAL_OMX_CLIENT_SUPPORT",
"CFG_GLOBAL_LCDCTL_SUPPORT",
#else
"CFG_GLOBAL",
#endif
""
};

int DoWork( char *filename, int size, 
	char *pKeywords[KEYWORDS], char *pKeywordsX[KEYWORDS], int bExec )
{
int i;
int bText=0;
char str[1024+1];
int bGlobal=0;
int bFound = 0;
int nFound = 0;
int mKey, nKey, xKey;

	memset( str, 1024, 0 );
	FILE *fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open [%s]\n", filename );
	    exit( 1 );
	}
	if( bVerbose>1 )
	    fprintf( stdout, "DoWork(%s)\n", filename );

	int bOK=0;
	if( pKeywords[0]!=NULL )
	{
	    int bShowFile=0;
	    while( 1 )
	    {
	    	int len;
		if( fgets( str, 1024, fp )==NULL )
		    break;
		len = strlen( str );
		if( len>1023 )
		{
		    fprintf( stdout, "%s : len=%d\n", filename, len );
		    exit( 1 );
		}
		for( mKey=0; mKey<KEYWORDS; mKey++ )
		{
		    if( pKeywords[mKey]==NULL )
			break;
		}
		nFound = 0;
		for( nKey=0; nKey<KEYWORDS; nKey++ )
		{
		    if( pKeywords[nKey]==NULL )
			break;
		    int keyLen = strlen(pKeywords[nKey]);
		    if( keyLen<1 )
			break;
		    bFound=0;
		    for( i=0; i<(len-keyLen); i++ )
		    {
			if( strncmp( pKeywords[nKey], &str[i], keyLen )==0 )
			{
			    bFound++;
			    nFound++;
			    break;
			}
		    }
		    if( (nFound>0) && (pKeywordsX[0]) )
		    {
			for( xKey=0; xKey<KEYWORDS; xKey++ )
			{
			    if( pKeywordsX[xKey]==NULL )
				break;
			    int keyLenX = strlen(pKeywordsX[xKey]);
			    if( keyLenX<1 )
				break;
			    for( i=0; i<(len-keyLenX); i++ )
			    {
				if( strncmp( pKeywordsX[xKey], 
					&str[i], keyLenX )==0 )
				{
				    bFound--;
				    nFound--;
				    break;
				}
			    }
			}
		    }
		    if( mode==MODE_AND )
		    if( bFound==0 )
			break;
		}
		if( ((mode==MODE_AND) && (nFound>=mKey))
		||  ((mode==MODE_OR ) && (nFound>0)) )
		{
		    if( bExec==0 )
		    {
			if( bShowFile==0 )
			fprintf( stdout, "File=%s\n", filename );
			if( bFile==0 )
			fprintf( stdout, "%s", str );
		    }
		    bShowFile++;
		    bOK++;
		}
	    }
	} else {
	    while( 1 )
	    {
		if( fgets( str, 1024, fp )==NULL )
		    break;
		int len = strlen( str );
		if( len>1023 )
		{
		    fprintf( stdout, "%s : len=%d\n", filename, len );
		    exit( 1 );
		}
		{
		    int n;
		    for( n=0; ; n++ )
		    {
			int nn = strlen(CfgList[n]);
			if( nn<1 )
			    break;
			for( i=0; i<(len-nn); i++ )
			{
			    if( strncmp( CfgList[n], &str[i], nn )==0 )
			    {
				bOK++;
    #if 0
				fprintf( stdout, "********************\n" );
				fprintf( stdout, "%s\n", filename );
				fprintf( stdout, "%s", str );
    #endif
			    }
			}
		    }
		} 
    #if 0
		else if( strncmp( str, "export", 6 )==0 )
		{
		    fprintf( stdout, "%s\n", filename );
		    fprintf( stdout, "%s", str );
		}
    #endif
	    }
	}

static char include[] = "#include \"/brcm_shared/include.h\"";

	if( bExec && bOK )
	{
	    int len = strlen(filename);
	    if( len>1 )
	    {
		if( filename[len-2]=='.' )
		{
		    char cmd[1024];
		    if( (filename[len-1]=='c') 
		     || (filename[len-1]=='h') )
		    {
		    	if( bExec==1 )
			{
			    fprintf( stdout, "%s\n", filename );
			    fseek( fp, 0L, SEEK_SET );
			    if( fgets( str, 1024, fp )==NULL )
			    {
				fprintf( stdout, "??\n" );
			    } else {
				if( strncmp( str, include, strlen(include))!=0 )
				{
				sprintf( cmd, "mv %s %s-\n", 
					filename, filename );
				fprintf( stdout, "%s", cmd );
				system( cmd );

				sprintf( cmd, "echo \'%s\' > %s\n", 
					include, filename );
				fprintf( stdout, "%s", cmd );
				system( cmd );

				sprintf( cmd, "cat %s- >> %s\n", 
					filename, filename );
				fprintf( stdout, "%s", cmd );
				system( cmd );
				}
			    }
			} else {
			    sprintf( cmd, "vi %s", filename );
			    system( cmd );
			}
		    }
		}
	    }
	}
	fclose( fp );

	return 0;
}

int maxSize=0;

int DoSearch( char *pDirname, 
	char *pKeywords [KEYWORDS], 
	char *pKeywordsX[KEYWORDS],
	char *pExtensions[KEYWORDS], int bExec )
{
int ret;
	struct dirent **namelist;
	if( bVerbose )
	fprintf( stdout, "Enter %s\n", pDirname );
	int nList = scandir( pDirname, &namelist, 0, alphasort );
	if( nList<0 )
	{
	    if( bVerbose )
	    fprintf( stdout, "scandir return %d\n", nList );
	    return -1;
	}
	if( bVerbose )
	    fprintf( stdout, "nList=%d\n", nList );
	for( int n=0; n<nList; n++ )
	{
	    struct stat Stat;
	    char filename[1024];
	    sprintf( filename, "%s/%s", pDirname, namelist[n]->d_name );
	    ret = lstat( filename, &Stat );
	    if( bVerbose )
	    fprintf( stdout, "(%s)=%d,%X\n", filename, ret, Stat.st_mode );
	    if( S_ISDIR( Stat.st_mode ) )
	    {	// Directory
	    	if( strcmp( namelist[n]->d_name, "." )==0 )
		    continue;
	    	if( strcmp( namelist[n]->d_name, ".." )==0 )
		    continue;
		if( bVerbose )
	    	fprintf( stdout, "Directory:%s\n", namelist[n]->d_name );
		nDirs++;
		DoSearch( filename, pKeywords, pKeywordsX, pExtensions, bExec );
	    } else if( S_ISREG( Stat.st_mode ) )
	    {	// File
//		fprintf( stdout, "%s\n", namelist[n]->d_name );
		if( Stat.st_size>0 )
		{
		    int e;
		    int bText=1;
		    char Ext[128];
		    int len = strlen(namelist[n]->d_name);
		    Ext[0] = 0;
		    for( e=len-1; e>=0; e-- )
		    {
			if( namelist[n]->d_name[e]=='.' )
			{
			    memcpy( Ext, &namelist[n]->d_name[e], len-e );
			    Ext[len-e] = 0;
			    break;
			}
		    }
		    if( pExtensions[0]!=NULL )
		    {
			bText=0;
			if( Ext[0]!=0 )
			{
    //			fprintf( stdout, "Ext=[%s]\n", Ext );
			    for( e=0; e<KEYWORDS; e++ )
			    {
				if( pExtensions[e]==NULL )
				    break;
				if( strcmp( pExtensions[e], Ext )==0 )
				{
    #if 0
				    fprintf( stdout, "[%s][%s] %s\n", 
				    pExtensions[e], Ext, namelist[n]->d_name );
    #endif
				    bText=1;
				    break;
				}
			    }
			    if( bVerbose>2 )
				    fprintf( stdout, 
					"%s : bText=%d\n", filename, bText );
			}
		    }
		    if( bText )
			bText = CheckText( filename, Stat.st_size );
		    if( bText )
		    {
			DoWork( filename, Stat.st_size, 
				pKeywords, pKeywordsX, bExec );
		    } else {
		    	if( bVerbose>1 )
			    fprintf( stdout, "%s : not text\n", filename );
		    }
		}
		nFiles++;
	    }
	    free( namelist[n] );
	}
	free( namelist );
	if( bVerbose )
	fprintf( stdout, "Leave %s\n", pDirname );
	return nList;
}

int main( int argc, char *argv[] )
{
char *pDirName=NULL;
int i;
char *pKeywords  [KEYWORDS];
char *pKeywordsX [KEYWORDS];
char *pExtensions[KEYWORDS];
int args=0;
int bResult=0;
int bExec=0;
int nExt=0;
int xArgs=0;

	for( i=0; i<KEYWORDS; i++ )
	{
	    pKeywords  [i]=NULL;
	    pKeywordsX [i]=NULL;
	    pExtensions[i]=NULL;
	}

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		    case 'O' :
		    	mode = MODE_OR;
		    	break;
		    case 'V' :
		    	bVerbose = atoi( &argv[i][2] );
			if( bVerbose==0 )
			    bVerbose = 1;
			break;
		    case 'R' :
		    	bResult++;
			break;
		    case 'E' :	// extension limit : ex. -E.c -E.h
		    	pExtensions[nExt++] = &argv[i][2];
			if( nExt>=KEYWORDS )
			{
			    fprintf( stdout, 
			    	"Too many extension (%d)\n", nExt );
			    exit( 1 );
			}
			break;
		    case 'F' :	// Filename only
		    	bFile = 1;
			break;
		    case 'X' :
		    	bExec = atoi( &argv[i][2] );
			if( bExec==0 )
			    bExec = 1;
		    	fprintf( stdout, "bExec=%d\n", bExec );
			break;
		    case '-' :
			pKeywordsX[xArgs++] = &argv[i][2];
			if( xArgs>=KEYWORDS )
			{
			    fprintf( stdout, 
				"Too many keywordsX(%d)\n", xArgs );
			    exit( 1 );
			}
			break;
		    default :
			Usage( argv[0] );
			break;
		}
	    } else {
	    	switch( args )
		{
		case 0 :
		    pDirName = argv[i];
		    break;
		case 1 :
		case 2 :
		case 3 :
		case 4 :
		case 5 :
		case 6 :
		case 7 :
		case 8 :
		case 9 :
		    pKeywords[args-1] = argv[i];
		    break;
		default :
		    Usage( argv[0] );
		    break;
		}
		args++;
	    }
	}
	if( pDirName==NULL )
	{
	    Usage( argv[0] );
	}

	DIR *pDir = opendir( pDirName );
	if( pDir==NULL )
	{
	    fprintf( stdout, "Can't open directory[%s]\n", pDirName );
	    exit( 1 );
	}
	for( i=0; i<args; i++ )
	{
	    if( pKeywords[i] )
	    	fprintf( stdout, "Keywords [%d] = %s\n", i, pKeywords[i] );
	    else
		break;
	}
	for( i=0; i<xArgs; i++ )
	{
	    if( pKeywordsX[i] )
	    	fprintf( stdout, "KeywordsX[%d] = %s\n", i, pKeywordsX[i] );
	    else
		break;
	}
	for( i=0; i<nExt; i++ )
	{
	    if( pExtensions[i] )
	    	fprintf( stdout, "Extensions[%d] = %s\n", i, pExtensions[i] );
	    else
		break;
	}

	DoSearch( pDirName, pKeywords, pKeywordsX, pExtensions, bExec );

/*
	fprintf( stdout, "----\n" );
	while( 1 )
	{
	    struct dirent *pDirent = readdir( *pDir );
	    if( pDirent==NULL )
	    {
	    	fprintf( stdout, "??\n" );
	    	exit( 1 );
	    }
	    fprintf( stdout, "%s\n", pDirent->d_name );
	    DIR *pNext = readdir( pDir );
	    if( pNext==NULL )
		break;
	    pDir = pNext;
	}
*/
	closedir( pDir );

	if( bResult )
	{
	    fprintf( stdout, "nText =%d\n", nText );
	    fprintf( stdout, "nFiles=%d\n", nFiles );
	    fprintf( stdout, "nDirs =%d\n", nDirs );
	}
}

