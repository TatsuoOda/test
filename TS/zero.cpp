
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[] )
{
char *filename = argv[1];
unsigned char buffer[1024*64];
int nZero=0;
int readed;
int addr=0;
int i;

    FILE *fp = fopen( filename, "rb" );
    if( fp==NULL )
    {
    	fprintf( stdout, "Can't open [%s]\n", filename );
	exit( 1 );
    }
    while( 1 )
    {
    	readed = fread( buffer, 1, 1024*64, fp );
	if( readed<1 )
	{
	    fprintf( stdout, "EOF(%X)\n", addr );
	    break;
	}
	for( i=0; i<readed; i++ )
	{
		if( buffer[i]==0 )
		{
		    nZero++;
		} else {
		    if( nZero>1024 )
		    {
		    	fprintf( stdout, "Zero(%7d)@%8X\n", nZero, addr );
		    }
		    nZero=0;
		}
		addr++;
	}
    }
    fclose( fp );
}

