/*
	2003.6.2  Add -S option
		   64bit support (use off_t)
		   Add -L option
	2005.10.31
		Change -S and -L parameter
			/L or -L: Long
			/W or -W: Short
			/B or -B: Byte

			/S or -S: Size
			/B or -B: Begin
*/

#include <stdlib.h>
#include <stdio.h>

#include <string.h>	// for memset

void Usage( char *prg )
{
    fprintf( stderr, "%s filename [/Long /Word] [-B(#begin)] [-S(#size)]\n", 
	    	prg );
}

#define MODE_BYTE	0
#define MODE_SHORT	1
#define MODE_LONG	2

int main( int argc, char *argv[] )
{
FILE *fp=NULL;
off_t begin = 0;
off_t size  = 0x7FFFFFFF;	// 2GB
char *filename = NULL;
int mode=MODE_BYTE;
int bRev=0;
int i;

	for( i=1; i<argc; i++ )
	{
//	    if( (argv[i][0]=='-') || (argv[i][0]=='/') )
	    if( (argv[i][0]=='-') )
	    {
	    	switch( argv[i][1] )
		{
		    case 'w' :
		    	bRev = 1;
		    case 'W' :
		    	mode = MODE_SHORT;
		    	break;
		    case 'l' :
		    	bRev = 1;
		    case 'L' :
		    	mode = MODE_LONG;
		    	break;
		    case 'b' :
		    case 'B' :
		    	sscanf( &argv[i][2], "%LX", &begin );
			fprintf( stdout, "Begin = %010LX\n", begin );
			break;
		    case 's' :
		    case 'S' :
		    	sscanf( &argv[i][2], "%LX", &size );
			fprintf( stdout, "Size = %010LX\n", size );
			break;
		    default :
		    	Usage( argv[0] );
			exit( 1 );
		}
	    } else {
	    	if( filename==NULL )
		    filename = argv[i];
		else {
		    Usage( argv[0] );
		    exit( 1 );
		}
	    }
	}

	if( filename==NULL )
	    fp = stdin;
	else 
	    fp = fopen( filename, "rb" );

	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open [%s]\n", filename );
	    Usage( argv[0] );
	    exit( 1 );
	}

	unsigned char buf[1024];
	char str[32];
//	off_t addr = begin;
	long long addr = begin;
	fseeko( fp, begin, SEEK_SET );
	while( 1 )
	{
	    memset( buf, 16, 0 );
	    int readed = fread( buf, 1, 16, fp );
	    fprintf( stdout, "%010LX : ", addr );
	    switch( mode )
	    {
	    	case MODE_BYTE :
		    for( int x=0; x<16; x++ )
		    {
			if( x>=readed )
			    fprintf( stdout, "-- " );
			else
			    fprintf( stdout, "%02X ", buf[x] );
			if( x>=readed )
			    str[x] = 0;
			else if( (buf[x]>=' ') && (buf[x]<0x7F) )
			    str[x] = buf[x];
			else 
			    str[x] =  ' ';
		    }
		    break;
	    	case MODE_SHORT :
		    for( int x=0; x<16; x+=2 )
		    {
			if( x>=readed )
			    fprintf( stdout, "---- " );
			else {
			    if( bRev )
				fprintf( stdout, "%04X ", 
				    (buf[x]<<8)|(buf[x+1]<<0) );
			    else
				fprintf( stdout, "%04X ", 
				    (buf[x]<<0)|(buf[x+1]<<8) );
			}
			for( i=0; i<2; i++ )
			{
			    if( (x+i)>=readed )
				str[x+i] = 0;
			    else if( (buf[x+i]>=' ') && (buf[x+i]<0x7F) )
				str[x+i] = buf[x+i];
			    else 
				str[x+i] =  ' ';
			}
		    }
		    break;
	    	case MODE_LONG :
		    for( int x=0; x<16; x+=4 )
		    {
			if( x>=readed )
			    fprintf( stdout, "-------- " );
			else {
			    if( bRev )
				fprintf( stdout, "%08X ", 
				      (buf[x+0]<<24)|(buf[x+1]<<16)
				    | (buf[x+2]<< 8)|(buf[x+3]<< 0) );
			    else
				fprintf( stdout, "%08X ", 
				      (buf[x+0]<< 0)|(buf[x+1]<< 8)
				    | (buf[x+2]<<16)|(buf[x+3]<<24) );

			}
			for( i=0; i<4; i++ )
			{
			    if( (x+i)>=readed )
				str[x+i] = 0;
			    else if( (buf[x+i]>=' ') && (buf[x+i]<0x7F) )
				str[x+i] = buf[x+i];
			    else 
				str[x+i] =  ' ';
			}
		    }
		    break;
	    }
	    str[16] = 0;
	    fprintf( stdout, "[%s]\n", str );
	    if( readed<16 )
		break;
	    addr += 16;
	    size -= readed;
	    if( size<=0 )
		break;
	}

	return 0;
}

