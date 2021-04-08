/*
	mep.cpp
		2013.1.17  by T.Oda
*/

#include <stdio.h>
#include <stdlib.h>	// exit
#include <sys/types.h>
#include <dirent.h>	// opendir
#include <string.h>	// strlen
#include <fcntl.h>
#include <unistd.h>     // close
#include <sys/mman.h>   // mmap



void Usage( char *prg )
{
	fprintf( stdout, "%s direcotry\n", prg );
	exit( 1 );
}

char outFilename[1024] = "output.bin";

int Replace( char filename[], int address )
{
	fprintf( stdout, "Replace(%s,%X)\n", filename, address );
	int mapSize = 1024*1024*2;
	int fd = open( outFilename, O_RDWR );
	if( fd==(-1) )
	{
	    fprintf( stdout, "Can't open %s\n", outFilename );
	    exit( 1 );
	}
	FILE *fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}

    // mmap
    	unsigned char *data;
	data = (unsigned char *)mmap( (caddr_t)0, mapSize, 
			    PROT_READ | PROT_WRITE, MAP_SHARED,
			    fd, address & 0xFFFFF000 );
	if( data==(unsigned char *)-1) 
	{
	    fprintf( stdout, "Can't mmap\n" );
	    exit( 1 );
	}
	int offset = address & 0xFFF;
	unsigned char *ptr = &data[offset];
	int diff=0;
	int total=0;
	while( 1 )
	{
	    int readed, i;
	    unsigned char buffer[1024];
	    readed = fread( buffer, 1, 1024, fp );
	    if( readed<1 )
		break;
	    for( i=0; i<readed; i++ )
	    {
#if 0
	    	if( total<64 )
		    fprintf( stdout, "%2d : %02X %02X\n", i, *ptr, buffer[i] );
#endif
	    	if( *ptr!=buffer[i] )
		    diff++;
	    	*ptr = buffer[i];
		ptr++;
		total++;
	    }
	}
    // munmap
	if( munmap( data, mapSize )==(-1) )
	{
	    fprintf( stdout, "Can't munmap\n" );
	    exit( 1 );
	}
	fclose( fp );
	close( fd );
	fprintf( stdout, "Replace diff = %d\n", diff );
	return 0;
}

int main( int argc, char *argv[] )
{
/*
 mep_address_info_3.021t.txt
----------------------------------------------------
ayu2_asp0_b_8A1E00B1_S03_0.64.0.0t.bin 0x00000000
ayu2_crisc_b_3.2.1.1t.bin 0x001D2000
ayu2_tsp_b_2.7.0.0t.bin 0x00362000
ayu2_sep_b_0.51.0.0t.bin 0x00382000
ayu2_vdp_b_2.12.0.0t.bin 0x00398000
ayu2_bap_b_2.12.0.0t.bin 0x003D8000
ayu2_crisc_t_3.2.1.1t.bin 0x00800000
ayu2_tsp_t_2.7.0.0t.bin 0x00880000
ayu2_sep_t_0.51.0.0t.bin 0x00888000
ayu2_vdp_t_2.12.0.0t.bin 0x00A12000
ayu2_bap_t_2.12.0.0t.bin 0x00AD2000
ayu2_asp0_t_8A1E00B1_S03_0.64.0.0t.bin 0x00BE0000
ayu2_asp1_t_8A1E00B1_S03_0.64.0.0t.bin 0x00CD0000
ayu2_asp2_t_8A1E00B1_S03_0.64.0.0t.bin 0x00E90000
ayu2_asp3_t_8A1E00B1_S03_0.64.0.0t.bin 0x00F32000
----------------------------------------------------
 Release : Directoryを指定する
	    mep_address_info_X.XXXX.txtに従ってバイナリファイルを接続
 Local   : BaseとなるReleaseバージョンで作成されるバイナリファイルにたいして
 	   一部のMEPコードを差し替える
	   Videoの場合は
	   fw_crisc_b.bin
	   fw_crisc_t.bin
	   fw_bap_b.bin
	   fw_bap_t.bin
	   fw_vdp_b.bin
	   fw_vdp_t.bin
*/
int i;
int args=0;
char *directory=NULL;
char *localDirectory=NULL;
typedef struct tag_address {
char name[16];
int address;
} Address;

Address BaseAddress[] = {
{ "asp0_b" , 0x00000000 },
{ "crisc_b", 0x001D2000 },
{ "tsp_b"  , 0x00362000 },
{ "sep_b"  , 0x00382000 },
{ "vdp_b"  , 0x00398000 },
{ "bap_b"  , 0x003D8000 },
{ "crisc_t", 0x00800000 },
{ "tsp_t"  , 0x00880000 },
{ "sep_t"  , 0x00888000 },
{ "vdp_t"  , 0x00A12000 },
{ "bap_t"  , 0x00AD2000 },
{ "asp0_t" , 0x00BE0000 },
{ "asp1_t" , 0x00CD0000 },
{ "asp2_t" , 0x00E90000 },
{ "asp3_t" , 0x00F32000 },
{ "end"    , 0xFFFFFFFF }
};
	for( i=1; i<argc; i++ )
	{
	    fprintf( stdout, "argv[%d]=%s\n", i, argv[i] );
	    if( argv[i][0]=='-' )
	    {
	    } else if( argv[i][0]=='+' )
	    {
	    } else {
	    	switch( args )
		{
		case 0 :
		    directory=argv[i];
		    break;
		case 1 :
		    localDirectory=argv[i];
		    break;
		default :
		    break;
		}
		args++;
	    }
	}

	if( directory==NULL )
	{
	    Usage( argv[0] );
	}

	char path[1024];
	struct stat Stat;
	if( stat( directory, &Stat )!=0 )
	{
	    fprintf( stdout, "Can't stat %s\n", directory );
	    exit( 1 );
	}
	if( S_ISDIR( Stat.st_mode ) )
	{
	    DIR *pDir = opendir( directory );
	    struct dirent *pEnt;
	    if( pDir==NULL )
	    {
		fprintf( stdout, "Can't open directory(%s)\n", directory );
		exit( 1 );
	    }
	    int bFound=0;
	    while( 1 )
	    {
		pEnt = readdir( pDir );
		if( pEnt==NULL )
		    break;
		int len = strlen( pEnt->d_name );
		if( len>5 )
		{
		    if( strncmp( &pEnt->d_name[len-4], ".txt", 4 )==0 )
		    {
			fprintf( stdout, "txt=%s\n", pEnt->d_name );
			sprintf( path, "%s/%s", directory, pEnt->d_name );
			bFound=1;
			break;
		    }
		}
		if( bFound==0 )
		{
		    fprintf( stdout, "skip %s\n", pEnt->d_name );
		}
	    }
	    if( bFound==0 )
	    {
		fprintf( stdout, "Not found txt file\n" );
		exit( 1 );
	    } else {
		fprintf( stdout, "Open(%s)\n", path );
		FILE *fp = fopen( path, "r" );
		int out_address=0;
		if( fp==NULL )
		{
		    fprintf( stdout, "Can't open\n" );
		} else {
		    char s[1024];
		    FILE *out = fopen( outFilename, "wb" );
		    if( out==NULL )
		    {
			fprintf( stdout, "Can't open %s\n", outFilename );
			exit( 1 );
		    }
		    while( 1 )
		    {
			if( fgets( s, 1024, fp )==NULL )
			    break;
			int len = strlen( s );
			if( len>0 )
			{
			    char filename[1024];
			    char addressS[1024];
			    memset( filename, 0, 1024 );
			    memset( addressS, 0, 1024 );
			    int dst=0;
			    int state=0;
			    for( i=0; i<len; i++ )
			    {
				if( (s[i]==' ') || (s[i]==0) 
				 || (s[i]==0x0A) || (s[i]==0x0D) )
				{
				    state++;
				    dst = 0;
				} else {
				    switch( state )
				    {
				    case 0 :
					filename[dst++] = s[i];
					break;
				    case 1 :
					addressS[dst++] = s[i];
					break;
				    default :
					break;
				    }
				}
			    }
			    fprintf( stdout, "%s", s );
			    int address;
			    if( strncmp( addressS, "0x", 2 )!=0 )
			    {
				fprintf( stdout, "Invalid txt file\n" );
				exit( 1 );
			    }
			    sscanf( addressS, "0x%X", &address );
			    fprintf( stdout, "file=%s, address=%8X(%s)\n",
				    filename, address, addressS );
			    sprintf( path, "%s/%s", directory, filename );
			    FILE *bin = fopen( path, "rb" );
			    if( bin==NULL )
			    {
				fprintf( stdout, "Can't open %s\n", path );
			    } else {
				int readed, written, size;
    #define BUF_SIZE	1024*64
				char buf[BUF_SIZE];
				memset( buf, 0, BUF_SIZE );
				if( address<out_address )
				{
				    fprintf( stdout, 
					    "Invalid address=%8X < %8X\n",
						    address, out_address );
				    exit( 1 );
				}
				while( out_address<address )
				{
				    size = address-out_address;
				    if( size>BUF_SIZE )
					size = BUF_SIZE;
				    written = fwrite( buf, 1, size, out );
				    out_address+=written;
				}
				while( 1 )
				{
				    readed  = fread ( buf, 1, 1024*64, bin );
				    if( readed<1 )
					break;
				    written = fwrite( buf, 1, readed, out );
				    out_address += written;
				    if( written<readed )
				    {
					fprintf( stdout, 
					    "readed=%d, written=%d\n", 
					    readed, written );
				    }
				}
				fclose( bin );
			    }
			}
		    }
		    fclose( fp );
		    fclose( out );
		}
	    }
	    closedir( pDir );
	} else {
	    sprintf( outFilename, directory );
	}

	if( localDirectory )
	{
	    fprintf( stdout, "Update with local FW@%s\n",
		    localDirectory );
	    DIR *pDir = opendir( localDirectory );
	    struct dirent *pEnt;
	    if( pDir==NULL )
	    {
		fprintf( stdout, "Can't open directory(%s)\n", localDirectory );
		exit( 1 );
	    }
	    int bFound=0;
	    while( 1 )
	    {
		pEnt = readdir( pDir );
		if( pEnt==NULL )
		    break;
		int len = strlen( pEnt->d_name );
		if( len>5 )
		{
		    if( strncmp( &pEnt->d_name[len-4], ".bin", 4 )==0 )
		    {
#if 0
			fprintf( stdout, "bin=%s\n", pEnt->d_name );
#endif
			sprintf( path, "%s/%s", localDirectory, pEnt->d_name );
			bFound=1;
			for( i=0; BaseAddress[i].address!=0xFFFFFFFF; i++ )
			{
			    int j;
			    for( j=0; j<strlen(pEnt->d_name); j++ )
			    {
			    	if( strncmp( &pEnt->d_name[j],
				    BaseAddress[i].name, 
				    strlen(BaseAddress[i].name) )==0 )
				{
				    bFound=2;
#if 0
				    fprintf( stdout, "%s : %8X\n", 
				    	BaseAddress[i].name,
					BaseAddress[i].address );
#endif
				    Replace( path, BaseAddress[i].address );
				    break;
				}
			    }
			    if( bFound>1 )
				break;
			}
		    }
		}
		if( bFound==0 )
		{
		    fprintf( stdout, "skip %s\n", pEnt->d_name );
		}
	    }
	    closedir( pDir );
	}
}

