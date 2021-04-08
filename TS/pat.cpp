
/*
	pat.cpp
				2013.12.19	By T.Oda

				2013.12.19 Parse PAT & PMT with CRCC
				2014. 1.10 PID=0x10 : NIT (0x40)
					   PID=0x11 : SDT (0x42)
*/

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// strlen
#include <unistd.h>	// close

#if 0
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>	// mmap
#endif

#include "main.h"

#include "lib.h"
#include "tsLib.h"
#include "parseLib.h"
#include "pesParse.h"

// ---------------------------------------------------------------------

void patUsage( char *prg )
{
	fprintf( stdout, "%s filename\n", prg );
	exit( 1 );
}

int pat( int argc, char *argv[] )
{
int i;
int args=0;
char *filename=NULL;
FILE *bin_fp=NULL;

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='=' )
	    {
	    	switch( argv[i][1] )
		{
		default :
		    patUsage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		default :
		    patUsage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'D' :
		    bDump = 1;
		    break;
		default :
		    patUsage( argv[0] );
		    break;
		}
	    } else {
	    	switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		default :
		    patUsage( argv[0] );
		    break;
		}
		args++;
	    }
	}
	if( filename==NULL )
	{
	    fprintf(stdout, "No filename\n" );
	    patUsage( argv[0] );
	}

	char srcFilename[1024];
	char binFilename[1024];
	char txtFilename [1024];
//	char ptsFilename[1024];
//	char outFilename[1024];
	int period=(-1);
	for( i=0; i<strlen(filename); i++ )
	{
	    if( filename[i]=='.' )
	    	period = i;
	}
	if( period>0 )
	{
	    char head[1024];
	    memcpy( head, filename, strlen(filename)+1 );
	    head[period] = 0;
	    memcpy( binFilename, filename, strlen(filename)+1 );
	    snprintf( txtFilename, 1023, "%s.txt", head );
//	    snprintf( ptsFilename, 1023, "%s.pts", head );
	} else {
	    snprintf( binFilename, 1023, "%s.bin", filename );
	    snprintf( txtFilename, 1023, "%s.txt", filename );
//	    snprintf( ptsFilename, 1023, "%s.pts", filename );
	}

	bin_fp = fopen( binFilename, "rb" );
	if( bin_fp==NULL )
	{
	    fprintf( stdout, "binFilename : Can't open %s\n", binFilename );
	    exit( 1 );
	}
	int bUseAddrFile=1;

	// --------------------------------------------------------
	// Read Address table
	initTsAddr( );
	if( bUseAddrFile )
	{
	    int nTS=0;
	    if( ReadAddrFile( txtFilename, 
	    	srcFilename, TsAddr, PesAddr,
	    	&nTS, ADDR_MAX )<0 )
	    {
	    	fprintf( stdout, "Can't use AddrFile ignore\n" );
//		exit( 1 );
	    }
	}
	fprintf( stdout, "PesAddr[0]=%X\n", (UINT)PesAddr[0] );
	// --------------------------------------------------------
	int zCount=0;
	int readed;
#define BUF_SIZE	(1024*64)

#define STATE_INIT	 0
#define STATE_PAT	 2
#define STATE_PMT	 3
#define STATE_PMT2	 4
#define STATE_PMT3	 5
#define STATE_PMT4	 6
#define STATE_NIT	 7
#define STATE_NIT2	 8
#define STATE_NIT3	 9
#define STATE_NIT4	10
#define STATE_SDT	11
#define STATE_SDT2	12
#define STATE_CRC	 98
#define STATE_END	100
	unsigned char buffer[BUF_SIZE];
	int state = STATE_INIT;
	int readCount=1;
	int table_id = (-1);
	int section_length = (-1);
	int version_number = (-1);
	int current_next_indicator = (-1);
	int section_number = (-1);
	int last_section_number = (-1);
	int program_number = (-1);
	// for PAT
	int transport_stream_id = (-1);
	int program_map_PID = (-1);
	int PCR_PID = (-1);
	int program_info_length = (-1);
	// for PMT
	int stream_type = (-1);
	int elementary_PID = (-1);
	int ES_info_length = (-1);
	// for NIT
	int network_id = (-1);
	int network_len = (-1);
	int transport_len = (-1);
	int descript_len = (-1);
	int nTransport = 0;

	unsigned int crc = 0, crcX;
	int reserve;

	int index=0;
	while( zCount<BUF_SIZE )
	{
	if( state<STATE_END )
	{
	    if( state==STATE_INIT )
		fprintf( stdout, "========================================\n" );
	    else
		fprintf( stdout, "----------------------------------------\n" );
//	    fprintf( stdout, "State=%d\n", state );
	}
	    switch( state )
	    {
	    case STATE_INIT :	
	    	// table_id, section_syntax_indicator, section_length
	    	readCount=4;
		break;
	    case 1 :
	    	readCount=5;
	    	break;
	    case STATE_NIT :
	    	readCount=2;
	    	break;
	    case STATE_NIT2 :
	    	readCount=network_len+2;
	    	break;
	    case STATE_NIT3 :
	    	readCount=6;
	    	break;
	    case STATE_NIT4 :
		readCount=descript_len;
	    	break;
	    case STATE_SDT :
	    	readCount=8;
	    	break;
	    case STATE_SDT2 :
	    	readCount=descript_len;
	    	break;
	    case STATE_PAT :
	    	readCount=4;
	    	break;
	    case STATE_PMT :
	    	readCount=4;
	    	break;
	    case STATE_PMT2 :
	    	readCount=program_info_length;
	    	break;
	    case STATE_PMT3 :
	    	readCount=5;
	    	break;
	    case STATE_PMT4 :
	    	readCount=ES_info_length;
	    	break;
	    case STATE_CRC :
	    	readCount=4;
	    	break;
	    case STATE_END :
	    	if( bDump )
	    	fprintf( stdout, "index=%d\n", index );
		while( index>184 )
		{
		    index-=184;
		}
	    	if( bDump )
	    	fprintf( stdout, "index=%d\n", index );
	    	readCount=184-index;
	    	if( bDump )
	    	fprintf( stdout, "readCount=%d\n", readCount );
	    	break;
	    default :
	    	readCount=1;
		break;
	    }
	    readed = gread( &buffer[zCount], 1, readCount, bin_fp );
	    if( readed<1 )
	    {
	    	CannotRead( NULL );
	    	return -1;
	    }
	    if( bDump )
	    {
	    	fprintf( stdout, "readed=%d : ", readed );
	    	for( i=0; i<readed; i++ )
		{
		    fprintf( stdout, "%02X ", buffer[zCount+i] );
		}
		fprintf( stdout, "\n" );
	    }
	    switch( state )
	    {
	    case STATE_INIT :
	    	table_id = buffer[1];
		section_length = ((buffer[2]&0x0F)<<8)
			       |   buffer[3];
		fprintf( stdout, "table_id=%d\n", table_id );
		fprintf( stdout, "section_length=%d\n", section_length );
		state = 1;
		break;
	    case 1 :
	    	if( table_id==0 )	// PAT
		{
		    fprintf( stdout, "Table = PAT (%d)\n", table_id );
		    transport_stream_id = (buffer[4]<<8) | buffer[5];
		    state = STATE_PAT;
		    fprintf( stdout, "transport_stream_id=%d(0x%X)\n",
		    	transport_stream_id, transport_stream_id );
		} else if( table_id==2 )	// PMT
		{
		    fprintf( stdout, "Table = PMT (%d)\n", table_id );
		    program_number = (buffer[4]<<8) | buffer[5];
		    fprintf( stdout, "program_number=%d(0x%X)\n", 
		    	program_number, program_number );
		    state = STATE_PMT;
		} else if( table_id==64 )	// NIT
		{
		    fprintf( stdout, "Table = NIT (%d)\n", table_id );
		    network_id = (buffer[4]<<8) | buffer[5];
		    fprintf( stdout, "network_id=%d(0x%X)\n", 
		    	network_id, network_id );
		    state = STATE_NIT;
		} else if( table_id==66 )	// SDT
		{
		    fprintf( stdout, "Table = SDT (%d)\n", table_id );
		    transport_stream_id = (buffer[4]<<8) | buffer[5];
		    fprintf( stdout, "transport_stream_id=%d(0x%X)\n", 
		    	transport_stream_id, transport_stream_id );
		    state = STATE_SDT;
		} else if( table_id>=64 )	// User private
		{
		    fprintf( stdout, "User pribate(%d)\n", table_id );
		    exit( 1 );
		} else {
		    fprintf( stdout, "Unknown table_id(%d)\n", table_id );
		    exit( 1 );
		}
		version_number      = (buffer[6]>>1) & 0x1F;
		current_next_indicator = buffer[6] & 1;
		section_number      = buffer[7];
		last_section_number = buffer[8];
		fprintf( stdout, "version_number=%d\n", version_number );
		fprintf( stdout, "current_next_indicator=%d\n",
			current_next_indicator );
		fprintf( stdout, "section_number=%d\n", section_number );
		fprintf( stdout, "last_section_number=%d\n", 
			last_section_number );
		index = 9;
		break;
	    case STATE_NIT :
	    	network_len = buffer[index++] & 0x0F;
		network_len = (network_len<<8) | buffer[index++];
		fprintf( stdout, "network_len=%d\n", network_len );
		state = STATE_NIT2;
		break;
	    case STATE_NIT2 :
	    	if( network_len>0 )
		{
		    int top=index;
#if 0
		    for( i=0; i<network_len; i++ )
		    {
			fprintf( stdout, "%02X ", buffer[index++] );
		    }
		    fprintf( stdout, "\n" );
#else
		    while( (index-top)<network_len )
		    {
			int tag = buffer[index++];
			int len = buffer[index++];
			fprintf( stdout, "Tag = %d(0x%X)\n", tag, tag );
			fprintf( stdout, "Len = %d\n", len );
			switch( tag )
			{
			case 0x40 : // network-name
			    fprintf( stdout, "Network-name : " );
			    for( i=0; i<len; i++ )
			    {
				fprintf( stdout, "%c", buffer[index++] );
			    }
			    fprintf( stdout, "\n" );
			    break;
			case 0x4A : // link
			    {
			    	int service_id;
				int link_type;
			    	transport_stream_id = buffer[index++];
			    	transport_stream_id = (transport_stream_id<<8)
						    | buffer[index++];
			    	network_id = buffer[index++];
			    	network_id = (network_id<<8)
					   | buffer[index++];
			    	service_id = buffer[index++];
			    	service_id = (service_id<<8)
					   | buffer[index++];
			    	link_type  = buffer[index++];
				fprintf( stdout, 
				    "transport_stream_id = %d(0x%X)\n",
				    transport_stream_id, transport_stream_id );
				fprintf( stdout, "network_id = %d(0x%X)\n",
					network_id, network_id );
				fprintf( stdout, "service_id = %d(0x%X)\n",
					service_id, service_id );
				fprintf( stdout, "link_type = %d(0x%X)\n",
					link_type, link_type );
				fprintf( stdout, "Priate Data : " );
				for( i=7; i<len; i++ )
				{
				    fprintf( stdout, "%02X ", buffer[index++] );
				}
				fprintf( stdout, "\n" );
			    }
			    break;
			}
		    }
#endif
		}
	    	transport_len = buffer[index++] & 0x0F;
		transport_len = (transport_len<<8) | buffer[index++];
		fprintf( stdout, "transport_len=%d\n", transport_len );
		nTransport = 0;
		state = STATE_NIT3;
		break;
	    case STATE_NIT3 :
	    	{
		    int network_id;
		    transport_stream_id = buffer[index++];
		    transport_stream_id = (transport_stream_id<<8) 
		    			| buffer[index++];
		    network_id = buffer[index++];
		    network_id = (network_id<<8) | buffer[index++];
		    descript_len = buffer[index++] & 0x0F;
		    descript_len = (descript_len<<8) | buffer[index++];
		    fprintf( stdout, "transport(%d)\n", nTransport );
		    fprintf( stdout, "transport_stream_id = %d(0x%X)\n", 
		    	transport_stream_id, transport_stream_id );
		    fprintf( stdout, "network_id   = %d(0x%X)\n", 
		    	network_id, network_id );
		    fprintf( stdout, "descript_len = %d\n", descript_len );
		    nTransport += 6;
		}
		state = STATE_NIT4;
		break;
	    case STATE_NIT4 :
		{
		    int n;
		    int top = index;
		    fprintf( stdout, "descript : " );
		    for( n=0; n<descript_len; n++ )
		    {
			fprintf( stdout, "%02X ", buffer[index++] );
		    }
		    fprintf( stdout, "\n" );
		    int tag  = buffer[top+0];
		    int len  = buffer[top+1];
		    int freq = (buffer[top+2]<<24)
		    	     | (buffer[top+3]<<16)
		    	     | (buffer[top+4]<< 8)
		    	     | (buffer[top+5]<< 0);
		    int type = buffer[top+7]>>4;
		    int FEC  = buffer[top+7] & 0xF;
		    int modulation = buffer[top+8];
		    int rate = (buffer[top+ 9]<<24)
		             | (buffer[top+10]<<16)
		             | (buffer[top+11]<< 8)
		             | (buffer[top+12]<< 0);
		    rate=rate>>4;
		    fprintf( stdout, "tag = 0x%02X\n", tag );
		    fprintf( stdout, "len  = %d\n", len );
		    fprintf( stdout, "freq = %8X\n", freq );
		    switch( type )
		    {
		    case 0x1 :
		     	fprintf( stdout, "type = multi frame\n" );
			break;
		    case 0xF :
		     	fprintf( stdout, "type = no multi frame\n" );
			break;
		    default :
		     	fprintf( stdout, "type = reserve\n" );
			break;
		    }
		    switch( FEC )
		    {
		    case 0 :
		     	fprintf( stdout, "FEC = none\n" );
			break;
		    case 1 :
		     	fprintf( stdout, "FEC = outer encoding\n" );
			break;
		    case 2 :
		     	fprintf( stdout, "FEC = Reed-Solomon\n" );
			break;
		    default :
		     	fprintf( stdout, "FEC = reserve\n" );
			break;
		    }
		    switch( modulation )
		    {
		    case 0x00 :
		     	fprintf( stdout, "modulation = None\n" );
			break;
		    case 0x03 :
		     	fprintf( stdout, "modulation = 64QAM\n" );
			break;
		    case 0x05 :
		     	fprintf( stdout, "modulation = 256QAM\n" );
			break;
		    default :
		     	fprintf( stdout, "modulation = unknown(%d)\n", 
			modulation );
			break;
		    }
		}
		nTransport+=descript_len;
		if( nTransport<transport_len )
		    state = STATE_NIT3;
		else
		    state = STATE_CRC;
		break;

	    case STATE_SDT :
		{
		int EIT, scramble, dummy, service_id;
	    	network_id = buffer[index++] & 0x0F;
		network_id = (network_id<<8) | buffer[index++];
		dummy = buffer[index++];
	    	service_id = buffer[index++];
		service_id = (service_id<<8) | buffer[index++];
	    	EIT = buffer[index++];
		descript_len = buffer[index++];
		scramble = descript_len>>4;
		descript_len = ((descript_len&0xF)<<8) | buffer[index++];

		fprintf( stdout, "network_id=%d(0x%X)\n", 
			network_id, network_id );
		fprintf( stdout, "dummy=%d\n", dummy );
		fprintf( stdout, "service_id=%d(0x%X)\n", 
			service_id, service_id );
		fprintf( stdout, "EIT=%d\n", EIT );
		fprintf( stdout, "scramble=%d\n", scramble );
		fprintf( stdout, "descript_len=%d\n", descript_len );
		state = STATE_SDT2;
		}
		break;
	    case STATE_SDT2 :
		{
		    int n;
		    int top = index;
		    fprintf( stdout, "descript : " );
		    for( n=0; n<descript_len; n++ )
		    {
			fprintf( stdout, "%02X ", buffer[index++] );
		    }
		    fprintf( stdout, "\n" );
		    // サービス記述子
		    int tag      = buffer[top+0];
//		    int len      = buffer[top+1];
		    int type     = buffer[top+2];
		    int name_len = buffer[top+3];
		    fprintf( stdout, "tag  = 0x%02X\n", tag );
		    fprintf( stdout, "Type = %d\n", type );
		    fprintf( stdout, "Carrier : " );
		    for( n=0; n<name_len; n++ )
		    {
		    	fprintf( stdout, "%c", buffer[top+4+n] );
		    }
		    fprintf( stdout, "\n" );
		    name_len = buffer[top+4+n];
		    top = top+4+n;
		    fprintf( stdout, "Service : " );
		    for( n=0; n<name_len; n++ )
		    {
		    	fprintf( stdout, "%c", buffer[top+n] );
		    }
		    fprintf( stdout, "\n" );
		}
		state = STATE_CRC;
		break;

	    case STATE_PAT :
	    	program_number = buffer[index++];
	    	program_number = (program_number<<8) | buffer[index++];
		program_map_PID = buffer[index++] & 0x1F;
		program_map_PID = (program_map_PID<<8) | buffer[index++];
		fprintf( stdout, "program_number =0x%X(%d)\n", 
			program_number, program_number );
		if( program_number==0 )
		    fprintf( stdout, "network_PID    =0x%X\n", program_map_PID);
		else
		    fprintf( stdout, "program_map_PID=0x%X\n", program_map_PID);
		fprintf( stdout, "index=%d, section_length=%d\n",
			index, section_length );
		if( index>=section_length )
		    state = STATE_CRC;
		break;
	    case STATE_PMT :
		reserve = buffer[index] & 0xE0;
		if( reserve!=0xE0 )
		    fprintf( stdout, 
		    "Invalid reserve area for PCR_PID\n" );
		PCR_PID = buffer[index++] & 0x1F;
		PCR_PID = (PCR_PID<<8) | buffer[index++];
		reserve = buffer[index] & 0xF0;
		if( reserve!=0xF0 )
		    fprintf( stdout, 
		    "Invalid reserve area for program_info_length\n" );
		program_info_length = buffer[index++] & 0x0F;
		program_info_length = (program_info_length<<8)|buffer[index++];
		fprintf( stdout, "PCR_PID=0x%X\n", PCR_PID );
		fprintf( stdout, "program_info_length=%d\n", 
			program_info_length );
		if( program_info_length>0 )
		    state = STATE_PMT2;
		else
		    state = STATE_PMT3;
		break;
	    case STATE_PMT2 :
	    	fprintf( stdout, "ProgramInfo : " );
	    	for( i=0; i<program_info_length; i++ )
		{
		    fprintf( stdout, "%02X ", buffer[index++] );
		}
		fprintf( stdout, "\n" );
		state = STATE_PMT3;
		break;
	    case STATE_PMT3 :
	    	stream_type = buffer[index++];
		elementary_PID = buffer[index++] & 0x1F;
		elementary_PID = (elementary_PID<<8) | buffer[index++];
		ES_info_length = buffer[index++] & 0x0F;
		ES_info_length = (ES_info_length<<8) | buffer[index++];
		fprintf( stdout, "stream_type=%d\n", stream_type );
		fprintf( stdout, "elementary_PID=0x%X\n", elementary_PID );
		fprintf( stdout, "ES_info_length = %d\n", ES_info_length );
		if( ES_info_length>0 )
		{
		    state = STATE_PMT4;
		} else {
		    fprintf( stdout, "index=%d, section_length=%d\n",
		    	index, section_length );
		    if( index>=section_length )
			state = STATE_CRC;
		}
		break;
	    case STATE_PMT4 :
	    	fprintf( stdout, "ES_info : " );
	    	for( i=0; i<ES_info_length; i++ )
		{
		    fprintf( stdout, "%02X ", buffer[index++] );
		}
		fprintf( stdout, "\n" );
		fprintf( stdout, "index=%d, section_length=%d\n",
		    index, section_length );
		if( index>=section_length )
		    state = STATE_CRC;
		else
		    state = STATE_PMT3;
		break;
	    case STATE_CRC :
		crcX = 0;
	    	crc = crc32( &buffer[1], index-1 );
		fprintf( stdout, "CRC : " );
	    	for( i=0; i<4; i++ )
		{
		    crcX = (crcX<<8) | buffer[index];
		    fprintf( stdout, "%02X ", buffer[index++] );
		}
		fprintf( stdout, "\n" );
		fprintf( stdout, "Calculated CRC = 0x%08X\n", crc );
		if( crc==crcX )
		{
		    fprintf( stdout, "CRC = OK\n" );
		} else {
		    fprintf( stdout, "CRC = NG\n" );
		}
		state = STATE_END;
	    	break;
	    case STATE_END :
	    	state = STATE_INIT;
		zCount=0;
		readed=0;
	    	break;
	    default :
		break;
	    }
	    zCount += readed;
	}
	fclose( bin_fp );
	return 0;
}

#ifndef MAIN
int bDebug=0;
int bDump=0;
int main( int argc, char *argv[] )
{
	return pat( argc, argv );
}
#endif

