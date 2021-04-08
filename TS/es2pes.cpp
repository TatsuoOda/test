
/*
	es2pes
		2013.7.29  by T.Oda

		2013.7.29	Start coding
				AVC version
		2013.12.10	Support multi SEI in 1 AU : for allegro stream
		2013.12.11	use MAX_PPS (256)
				use frame_mbs_only_flag for calculate DTS
				use pic_struct for calculate DTS
		2013.12.12	Output_PES_Header : Fix constant field 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>	// memcpy

#include "tsLib.h"
#include "parseLib.h"
#include "lib.h"

#include "main.h"

static char NAL_typeStr[32][40] = {
"Unspecified",
"Coded slice of a non-IDR picture",
"Coded slice data partition A",
"Coded slice data partition B",
"Coded slice data partition C",
"Coded slice of an IDR picture",
"SEI",
"SPS",
"PPS",
"AUD",
"End of sequence",
"End of stream",
"Filler data",
"SPS extension",
"Prefix NAL unit",
"Subset SPS",
"Reserved",
"Reserved",
"Reserved",
"Coded slice of an auxiliary",
"Coded slice extension",
"Reserved",
"Reserved",
"Reserved",
"Unspecified",
"Unspecified",
"Unspecified",
"Unspecified",
"Unspecified",
"Unspecified",
"Unspecified",
"Unspecified",
};
static char SEIstr[47][40] = {
"buffering_period",
"pic_timing",
"pan_scan_rect",
"filler_payload",
"user_data_registered",
"user_data_unregistered",
"recovery_point",
"dec_ref_pic_marking_repetition",
"spare_pic",
"scene_info",
"sub_seq_info",
"sub_seq_layer_characteristics",
"sub_seq_characteristics",
"full_frame_freeze",
"full_frame_freeze_release",
"full_frame_snapshot",
"progressive_refinement_segment_start",
"progressive_refinement_segment_end",
"motion_constrained_slice_group_set",
"film_grain_characteristics",
"deblocking_filter_display_preference",
"stereo_video_info",
"post_filter_hint",
"tone_mapping_info",
"scalability_info",
"sub_pic_scalable_layer",
"non_required_layer_rep",
"priority_layer_info",
"layers_not_present",
"layer_depenency_change",
"scalable_nesting",
"base_layer_temporal_hrd",
"quality_layer_integrity_check",
"redundant_pic_property",
"tl0_dep_rep_index",
"tl_switching_point",
"parallel_decoding_info",
"mvc_scalable_nesting",
"view_scalability_info",
"multiview_scene_info",
"multiview_acquisition_info",
"non_required_view_component",
"view_dependency_change",
"operation_points_not_present",
"base_viw_temporal_hrd",
"frame_packing_arrangement",
"reserved_sei",
};

int Output_PES_Header( FILE *outfile, UINT DTS, UINT PTS )
{
int bPTS, bDTS;
int PTS32=0;
int DTS32=0;
unsigned char obuf[32];
int offset=0;
	if( PTS==0xFFFFFFFF )
	{
	    bPTS = 0;
	} else {
	    bPTS = 1;
	    if( DTS==PTS )
		bDTS = 0;
	}

	obuf[0] = 0x00;
	obuf[1] = 0x00;
	obuf[2] = 0x01;
	obuf[3] = 0xE0;
	obuf[4] = 0x00;
	obuf[5] = 0x00;
	obuf[6] = 0x80;
	obuf[7] = 0x00;
	if( bPTS )
	    obuf[7] |= 0x80;
	if( bDTS )
	    obuf[7] |= 0x40;
	if( bDTS && bPTS )
	    obuf[8] = 10;
	else if( bDTS || bPTS )
	    obuf[8] =  5;
	else
	    obuf[8] =  0;
	offset = 9;
	if( bPTS )
	{
	    obuf[offset+0] =  (PTS32<<3) | ((PTS>>30)<<1);
	    if( bDTS )
	    	obuf[offset+0] |= 0x31;
	    else
	    	obuf[offset+0] |= 0x21;
	    obuf[offset+1] = ((PTS>>22)<<0);
	    obuf[offset+2] = ((PTS>>15)<<1) | 0x01;
	    obuf[offset+3] = ((PTS>> 7)<<0);
	    obuf[offset+4] = ((PTS    )<<1) | 0x01;
	    offset += 5;
	}
	if( bDTS )
	{
	    obuf[offset+0] =  (DTS32<<3) | ((DTS>>30)<<1);
	    obuf[offset+0] |= 0x11;
	    obuf[offset+1] = ((DTS>>22)<<0);
	    obuf[offset+2] = ((DTS>>15)<<1) | 0x01;
	    obuf[offset+3] = ((DTS>> 7)<<0);
	    obuf[offset+4] = ((DTS    )<<1) | 0x01;
	    offset += 5;
	}
	int written = fwrite( obuf, 1, offset, outfile );	// Add 00
	if( written<offset )
	    fprintf( stdout, "Can't write %d/%d\n",
	    	written, offset );

	return 0;
}

// -------------------------------------------------------------

void es2pes_Usage( char *prg )
{
	fprintf( stdout, "%s filename\n", prg );
	exit( 1 );
}

int es2pes( int argc, char *argv[] )
{
int i;
int args=0;
char *filename=NULL;
unsigned char *buffer;
FILE *infile=NULL;
FILE *outfile=NULL;
int bDebug=0;
int bShow=1;
int bDebugSPS=0;
int bDebugSEI=0;
char pesFilename[1024];

	sprintf( pesFilename, "output.pes" );

	for( i=1; i<argc; i++ )
	{
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		default :
		    es2pes_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'D' :
		    bDebug = 1;
		    break;
		case 'S' :
		    bDebugSPS = 1;
		    bDebugSEI = 1;
		    break;
		default :
		    es2pes_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='=' )
	    {
	    	switch( argv[i][1] )
		{
		default :
		    es2pes_Usage( argv[0] );
		    break;
		}
	    } else {
	    	switch( args )
		{
		case 0 :
		    filename = argv[i];
		    break;
		case 1 :
		    sprintf( pesFilename, "%s", argv[i] );
		    break;
		default :
		    es2pes_Usage( argv[0] );
		    break;
		}
		args++;
	    }
	}

	if( filename==NULL )
	    es2pes_Usage( argv[0] );
#define BUFFER_SIZE	1024*1024*4
	buffer = (unsigned char *)malloc( BUFFER_SIZE );
	if( buffer==NULL )
	{
	    fprintf( stdout, "Can't alloc buffer\n" );
	    exit( 1 );
	}

	infile = fopen( filename, "rb" );
	if( infile==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}

	outfile = fopen( pesFilename, "wb" );
	if( outfile==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", pesFilename );
	    exit( 1 );
	}

	int state = 0;
	int pos = (-1);
	int nal_unit_type=(-1);
	int nZero=0;
	int payloadType = (-1);
	int payloadSize = (-1);
	int AVC_profile_idc = (-1);
	int AVC_constraint = (-1);
	int AVC_level_idc = (-1);
	unsigned char SpsData[256];
	int nSpsPos=0;
	unsigned char PpsData[256];
	int nPpsPos=0;
	unsigned char SeiData[256];
	int nSeiPos=0;
	int initial_cpb_removal_delay_length = (-1);
	int cpb_removal_delay_length = (-1);
	int dpb_output_delay_length = (-1);
#define MAX_SPS 32
#define MAX_PPS 256	// 2013.12.11
	int SPS_id=(-1);
	int PPS_id=(-1);
	int nal_hrd_parameters_present_flag[MAX_SPS];
	int vcl_hrd_parameters_present_flag[MAX_SPS];
	int CpbDpbDelaysPresentFlag[MAX_SPS];
	int frame_mbs_only_flag    [MAX_SPS];
	int time_offset_length     [MAX_SPS];
	int low_delay_hrd_flag     [MAX_SPS];
	int pic_struct_present_flag[MAX_SPS];

	int PPStoSPS[MAX_PPS];
	int cpb_cnt=(-1);
	int pic_struct=(-1);

	int cpb_removal_delay=(-1);
	int dpb_output_delay=(-1);

	int time_scale=(-1);
	int num_units_in_tick=(-1);

	UINT DTS=10, PTS=0;

#define STATE_NON_IDR           4
#define STATE_SEI1              5
#define STATE_SEI2              6
#define STATE_SEI3              7
#define STATE_SPS		8
#define STATE_PPS		9

	for( i=0; i<MAX_SPS; i++ )
	{
	    frame_mbs_only_flag[i]=(-1);
	    time_offset_length     [i]=(-1);
	    low_delay_hrd_flag     [i]=(-1);
	    pic_struct_present_flag[i]=(-1);
	}
	while( 1 )
	{
	    pos++;
	    if( pos>=BUFFER_SIZE )
	    {
	    	fprintf( stdout, "Too large size : pos=%d\n", pos );
		EXIT();
	    }
            int readed = gread( &buffer[pos], 1, 1, infile );
	    if( readed<1 )
	    {
	    	fprintf( stdout, "EOF@%llX\n", g_addr );
		break;
	    }
//	    fprintf( stdout, "readed(%04X:%02X)\n", pos, buffer[pos] );
#if 0
	    fprintf( stdout, "state=%d [%02X]\n", state, buffer[pos] );
#endif
	    switch( state )
	    {
	    case 0 :
	    	nZero = 0;
	    	if( buffer[pos]==0x00 )
		{
		    nZero++;
		    state++;
		}
		break;
	    case 1 :	// 00
	    	if( buffer[pos]==0x00 )
		{
		    nZero++;
		    state++;
		} else {
		    state=0;
		}
		break;
	    case 2 :	// 00 00
	    	if( buffer[pos]==0x01 )
		{
		    state++;
		} else if( buffer[pos]==0x00 )
		{
		    nZero++;
		} else {
		    state=0;
		}
		break;
	    case 3 :	// 00 00 01
	    	if( buffer[pos]>=0xE0 )
		{
		    fprintf( stdout, "PES_header?\n" );
		    EXIT( );
		}
		nal_unit_type = buffer[pos]&0x1F;
		switch( nal_unit_type )
		{
		case 1 :	// non IDR
//		    if( nSlice==0 )
		    {
			if( bDebug )
			    fprintf( stdout, "non IDR\n" );
//			InitBitBuffer( );
//			state = STATE_NON_IDR;
//			continue;
			state = 0;
		    }
		    break;
		case 5 :	// IDR
//		    if( nSlice==0 )
		    {
			if( bDebug )
			    fprintf( stdout, "%s\n",
				NAL_typeStr[nal_unit_type] );
#if 0
			fprintf( stdout, 
		"%8X : KEY(IDR) PTS=%08X (%2d:%02d:%02d.%03d)\n",
			last_header, PTS, h, m%60, s%60, ms%1000 );
			if( pKey )
			fprintf( pKey, "%8X : KEY(IDR) PTS=%08X\n",
				last_header, PTS );
#endif
		    }
//		    nSlice++;
		    break;
		case 6 :	// SEI
		    if( bDebug || bShow )
		    {
		    fprintf( stdout, "SEI : " );
		    state = STATE_SEI1;
		    InitBitBuffer( );
		    continue;
		    }
		    break;
		case 7 :	// SPS
		    if( bDebug || bShow )
		    {
			fprintf( stdout, "%s\n",
			    NAL_typeStr[nal_unit_type] );
		    state = STATE_SPS;
		    nZero = 0;
		    nSpsPos=0;
		    InitBitBuffer( );
		    continue;
		    }
		    break;
		case 8 :	// PPS
		    if( bDebug || bShow )
			fprintf( stdout, "%s\n",
			    NAL_typeStr[nal_unit_type] );
		    state = STATE_PPS;
		    nZero = 0;
		    nPpsPos = 0;
		    InitBitBuffer( );
		    continue;
		case 9 :	// AUD
		    if( bDebug || bShow )
			fprintf( stdout, "%s\n",
			    NAL_typeStr[nal_unit_type] );
//		    nSlice=0;
//		    OutputAU( buffer, outfile, nZero );
		    {
		    int AudSize = nZero+2;
		    fprintf( stdout, "pos=%d\n", pos );
		    if( cpb_removal_delay>=0 )
		    {
#if 0
		    fprintf( stdout, 
		    "dpb_output_delay=%d,num_units_in_tick=%d,time_scale=%d\n",
		    dpb_output_delay, num_units_in_tick, time_scale );
#endif
			PTS = DTS+90000
			    *dpb_output_delay*num_units_in_tick/time_scale;
		    	fprintf( stdout, "%8llX : Output_PES_Header(%d,%d)\n",
				g_addr, DTS, PTS );
			Output_PES_Header( outfile, DTS, PTS );
		    }
		    int delta=1;
		    if( frame_mbs_only_flag[PPStoSPS[PPS_id]]==1 )
		    {
		    	delta=2;
		    } else {
		    	switch( pic_struct )
			{
			case 0 :	// frame
			case 3 : 	// top/bottom
			case 4 : 	// bottom/top
			case 7 : 	// frame doubling
			    delta=2;
			    break;
			case 5 :	// top/bottom/top
			case 6 :	// bottom/top/bottom
			case 8 :	// frame tripling
			    delta=3;
			    break;
			}
		    }

		    DTS += 90000*delta*num_units_in_tick/time_scale;
		    if( cpb_removal_delay>=0 )
		    {
			PTS = DTS;
		    }
		    fwrite( buffer, 1, pos-AudSize+1, outfile );
		    cpb_removal_delay = (-1);
		    dpb_output_delay  = (-1);
		    memcpy( &buffer[0], &buffer[pos-AudSize+1], AudSize );
		    pos = AudSize-1;
		    }
		    break;
		case 10 :	// EOSeq
		    if( bDebug || bShow )
			fprintf( stdout, "%s\n",
			    NAL_typeStr[nal_unit_type] );
//		    nSlice=0;
		    break;
		case 12 :	// filler
		    if( bDebug || bShow )
			fprintf( stdout, "%s\n",
			    NAL_typeStr[nal_unit_type] );
		    break;
		default :
		    fprintf( stdout, 
			"nal_unit_type=%2d(0x%llX)\n", 
			nal_unit_type, g_addr-1 );
		    break;
		}
		state = 0;
		break;
	    case STATE_SEI1 :
	    	if( buffer[pos]==0x80 )
		    state = 0;
		else {
		    payloadType = buffer[pos];
		    payloadSize = 0;
//	    fprintf( stdout, "pos=0x%X, payloadType=0x%X\n", pos, payloadType );
		    if( bDebugSEI )
			fprintf( stdout, "%8llX\n", g_addr );
		    if( payloadType<47 )
		    {
			fprintf( stdout, "%02X : %s ", 
			    buffer[pos], SEIstr[payloadType] );
			if( payloadType!=0x01 )	// pic_timing
			    fprintf( stdout, "\n" );
		    }
		    state = STATE_SEI2;
		}
	    	break;
	    case STATE_SEI2 :
	    	payloadSize += buffer[pos];
		if( bDebugSEI )
		    fprintf( stdout, "SZ=%04X ", payloadSize );
		nSeiPos = 0;
		if( buffer[pos]!=0xFF )
		{
		    state = STATE_SEI3;
		    nZero=0;
		}
	    	break;
	    case STATE_SEI3 :
		if( bDebugSEI )
		    fprintf( stdout, "%02X ", buffer[pos] );
	    	if( payloadSize>0 )
		{
		    if( (nZero==2) && (buffer[pos]==0x03) )
		    {
		    	nZero=0;
		    } else {
		    	if( buffer[pos]==0x00 )
			    nZero++;
			else
			    nZero=0;
			if( nSeiPos<256 )
			    SeiData[nSeiPos++] = buffer[pos];
			payloadSize--;
		    }
		}
		if( payloadSize==0 )
		{
		    InitBitBuffer();
		    if( payloadType==0x01 )	// pic_timing
		    {
#if 1
		    	int nPos=0;
		    	if( CpbDpbDelaysPresentFlag[PPStoSPS[PPS_id]]==1 )
			{
#if 0
			fprintf( stdout, "SeiData : %02X %02X %02X\n",
				SeiData[0], SeiData[1], SeiData[2] );
#endif
			    cpb_removal_delay =
				GetBitFromBuffer( SeiData, &nPos, 
					cpb_removal_delay_length );
			    dpb_output_delay =
				GetBitFromBuffer( SeiData, &nPos, 
					dpb_output_delay_length );
			    fprintf( stdout, "(%2d,%2d)",
			    	cpb_removal_delay, dpb_output_delay );
			} else {
			    fprintf( stdout, "(No CpbDpbDelaysPresent)" );
			}
			if( pic_struct_present_flag[PPStoSPS[PPS_id]] )
			{
			    pic_struct = GetBitFromBuffer( SeiData, &nPos, 4 );
			    fprintf( stdout, "(%d)", pic_struct );
			}
			fprintf( stdout, "\n" );
#endif
		    } else if( payloadType==0x00 )	// bufferint period
		    {
		    	int nPos=0;
			int sps_id = GetGolombFromBuffer( SeiData, &nPos );
			fprintf( stdout, "sps_id=%d ", sps_id );

			if( nal_hrd_parameters_present_flag[sps_id] )
			{
			    int initial_cpb_removal_delay =
				GetBitFromBuffer( SeiData, &nPos, 
					initial_cpb_removal_delay_length );
			    int initial_cpb_removal_delay_offset =
				GetBitFromBuffer( SeiData, &nPos, 
					initial_cpb_removal_delay_length );
			    fprintf( stdout, "(%2d,%2d)\n",
			    	initial_cpb_removal_delay,
				initial_cpb_removal_delay_offset ); 
			}
			if( vcl_hrd_parameters_present_flag[sps_id] )
			{
			    int initial_cpb_removal_delay =
				GetBitFromBuffer( SeiData, &nPos, 
					initial_cpb_removal_delay_length );
			    int initial_cpb_removal_delay_offset =
				GetBitFromBuffer( SeiData, &nPos, 
					initial_cpb_removal_delay_length );
			    fprintf( stdout, "(%2d,%2d)\n",
			    	initial_cpb_removal_delay,
				initial_cpb_removal_delay_offset ); 
			}
//			fprintf( stdout, "pos=%d, nPos=%d\n", pos, nPos );
//			fprintf( stdout, "payloadSize=%d\n", payloadSize );
//			pos+=(nPos+1);
//			fprintf( stdout, "pos=%d(0x%X)\n", pos, pos );
//			pos--;
		    }
		    if( buffer[pos]==0x80 )
			state = 0;
		    else  {
		    	state = STATE_SEI1;
			fprintf( stdout, "Next %02X\n", buffer[pos] );
		    }
		}
	    	break;
	    case STATE_SPS :
		if( (nZero>=2) && ((buffer[pos]==0x00) || (buffer[pos]==0x01)) )
		{
		    state = 2;
		    int nPos=0;
		    int chroma_format_idc;
		    int matrix_present_flag;
		    int seq_scaling_list_present_flag;
		    int pic_order_cnt_type;
		    int num_ref_frames;
		    int max_num_ref_frames;
		    int pic_width_in_mbs_minus1;
		    int pic_height_in_map_units_minus1;
		    int direct_8x8;
		    int frame_cropping_flag;
		    int vui_parameters_present_flag;

		    AVC_profile_idc =
			GetBitFromBuffer( SpsData, &nPos, 8 );	// profile
		    AVC_constraint = 
			GetBitFromBuffer( SpsData, &nPos, 8 );	// constraint
		    AVC_level_idc = 
			GetBitFromBuffer( SpsData, &nPos, 8 );	// level
		    fprintf( stdout, 
			"profile=%3d, constraint=%3d, level=%2d, ",
			AVC_profile_idc, AVC_constraint, AVC_level_idc);
		    SPS_id = GetGolombFromBuffer( SpsData, &nPos );
//		    if( bDebugSPS )
			fprintf( stdout, "SPS id=%d\n", SPS_id );
		    switch( AVC_profile_idc )
		    {
		    case 100 :
		    case 110 :
		    case 122 :
		    case 244 :
		    case  44 :
		    case  83 :
		    case  86 :
		    case 118 :
		    case 128 :
			chroma_format_idc = 
				GetGolombFromBuffer( SpsData, &nPos );
			if( bDebugSPS )
			    fprintf( stdout, "chroma=%d\n",
				chroma_format_idc );
			if( chroma_format_idc==3 )
			    GetBitFromBuffer( SpsData, &nPos, 1 );
			GetGolombFromBuffer( SpsData, &nPos );
			GetGolombFromBuffer( SpsData, &nPos );
			GetBitFromBuffer( SpsData, &nPos, 1 );
			matrix_present_flag =
				GetBitFromBuffer( SpsData, &nPos, 1 );
			if( bDebugSPS )
			    fprintf( stdout, "matrix=%d\n",
				matrix_present_flag );
			if( matrix_present_flag )
			{
			    int ii, b;
			    int nn = chroma_format_idc!=3 ? 8 : 12;
			    for( ii=0; ii<nn; ii++ )
			    {
			    int bits = (i<6) ? 16 : 64;
			    GetBitFromBuffer( SpsData, &nPos, 1 );
			    seq_scaling_list_present_flag =
				GetBitFromBuffer( SpsData, &nPos, 1 );
				for( b=0; b<bits; b++ )
				{
				GetGolombFromBuffer( SpsData, &nPos );
				}
			    }
			}
			break;
		    }
		    GetGolombFromBuffer( SpsData, &nPos );
		    pic_order_cnt_type = 
			    GetGolombFromBuffer( SpsData, &nPos );
		    if( bDebugSPS )
			fprintf( stdout, "pic_order_cnt_type=%d\n",
			    pic_order_cnt_type );
		    if( pic_order_cnt_type==0 )
		    {
			GetGolombFromBuffer( SpsData, &nPos );
		    } else if( pic_order_cnt_type==1 )
		    {
			GetBitFromBuffer( SpsData, &nPos, 1 );
			GetGolombFromBuffer( SpsData, &nPos );
			GetGolombFromBuffer( SpsData, &nPos );
			num_ref_frames =
			    GetGolombFromBuffer( SpsData, &nPos );
//				if( bDebugSPS )
			    fprintf( stdout, "num_ref_frames=%d\n",
				num_ref_frames );
			int n;
			for( n=0; n<num_ref_frames; n++ )
			{
			    GetGolombFromBuffer( SpsData, &nPos );
			}
		    }
		    max_num_ref_frames = 
			    GetGolombFromBuffer( SpsData, &nPos );
		    if( bDebug )
			fprintf( stdout, "max_num_ref_frames=%d\n",
			    max_num_ref_frames );
		    GetBitFromBuffer( SpsData, &nPos, 1 );
		    pic_width_in_mbs_minus1 =
			GetGolombFromBuffer( SpsData, &nPos );
		    pic_height_in_map_units_minus1 =
			GetGolombFromBuffer( SpsData, &nPos );
		    if( bDebugSPS )
			fprintf( stdout, "pic_width_in_mbs_minus1=%d\n",
			pic_width_in_mbs_minus1 );
		    if( bDebugSPS )
			fprintf( stdout, 
			"pic_height_in_map_units_minus1=%d\n",
			pic_height_in_map_units_minus1 );
		    frame_mbs_only_flag[SPS_id] = 
			GetBitFromBuffer( SpsData, &nPos, 1 );

		    if( frame_mbs_only_flag[SPS_id]==0 )
		    {
			GetBitFromBuffer( SpsData, &nPos, 1 );
		    }
		    direct_8x8 = 
			GetBitFromBuffer( SpsData, &nPos, 1 );
		    if( bDebugSPS )
		    fprintf( stdout, "direct_8x8=%d\n",
			direct_8x8 );
		    frame_cropping_flag =
			GetBitFromBuffer( SpsData, &nPos, 1 );
		    if( bDebugSPS )
		    fprintf( stdout, "frame_cropping_flag=%d\n",
			frame_cropping_flag );
		    if( frame_cropping_flag )
		    {
			int crop_left =
			GetGolombFromBuffer( SpsData, &nPos );
			int crop_right =
			GetGolombFromBuffer( SpsData, &nPos );
			int crop_top =
			GetGolombFromBuffer( SpsData, &nPos );
			int crop_bottom =
			GetGolombFromBuffer( SpsData, &nPos );
			if( bDebugSPS )
			fprintf( stdout, "(%d,%d,%d,%d)\n",
				crop_left, crop_right,
				crop_top, crop_bottom );
		    }
		    nal_hrd_parameters_present_flag[SPS_id]=0;
		    CpbDpbDelaysPresentFlag[SPS_id]=0;
		    vui_parameters_present_flag =
			GetBitFromBuffer( SpsData, &nPos, 1 );
		    if( bDebugSPS )
		    fprintf( stdout, 
			    "vui_parameters_present_flag=%d\n",
			vui_parameters_present_flag );
		    if( vui_parameters_present_flag )
		    {
#define EXTENDED_SAR	255
			int aspect_ratio_info_present_flag;
			int aspect_ratio;
			int overscan_info_present_flag;
			int video_signal_type_present_flag;
			int colour_description_present_flag;
			int chroma_loc_info_present_flag;
			int timing_info_present_flag;
			int fixed_frame_rate_flag;
			aspect_ratio_info_present_flag = 
			    GetBitFromBuffer( SpsData, &nPos, 1 );
			if( aspect_ratio_info_present_flag )
			{
			    aspect_ratio =
				GetBitFromBuffer( SpsData, &nPos, 8 );
			    if( aspect_ratio==EXTENDED_SAR )
			    {
				GetBitFromBuffer( SpsData, &nPos, 16 );
				GetBitFromBuffer( SpsData, &nPos, 16 );
			    }
			    if( bDebugSPS )
			    fprintf( stdout, "aspect_ratio=%d\n",
				aspect_ratio );
			}
			overscan_info_present_flag = 
			    GetBitFromBuffer( SpsData, &nPos, 1 );
			if( overscan_info_present_flag )
			{
			    GetBitFromBuffer( SpsData, &nPos, 1 );
			}
			if( bDebugSPS )
			fprintf( stdout, 
			    "overscan_info_present_flag=%d\n",
			    overscan_info_present_flag );
			video_signal_type_present_flag = 
			    GetBitFromBuffer( SpsData, &nPos, 1 );
			if( bDebugSPS )
			fprintf( stdout, 
				"video_signal_type_present_flag=%d\n",
				video_signal_type_present_flag );
			if( video_signal_type_present_flag )
			{
			    int video_format =
				GetBitFromBuffer( SpsData, &nPos, 3 );
			    int video_full_range_flag =
				GetBitFromBuffer( SpsData, &nPos, 1 );
			    colour_description_present_flag =
				GetBitFromBuffer( SpsData, &nPos, 1 );
			    if( bDebugSPS )
			    fprintf( stdout, "video_format=%d\n",
				video_format );
			    if( bDebugSPS )
			    fprintf( stdout, 
				"video_full_range_flag=%d\n",
				video_full_range_flag );
			    if( bDebugSPS )
			    fprintf( stdout, 
				"colour_descripton_present_flag=%d\n",
				    colour_description_present_flag );
			    if( colour_description_present_flag )
			    {
				GetBitFromBuffer( SpsData, &nPos, 8 );
				GetBitFromBuffer( SpsData, &nPos, 8 );
				GetBitFromBuffer( SpsData, &nPos, 8 );
			    }
			}
			chroma_loc_info_present_flag =
			    GetBitFromBuffer( SpsData, &nPos, 1 );
			if( bDebugSPS )
			fprintf( stdout, 
				"chroma_loc_info_present_flag=%d\n",
				chroma_loc_info_present_flag );
			if( chroma_loc_info_present_flag )
			{
			    GetGolombFromBuffer( SpsData, &nPos );
			    GetGolombFromBuffer( SpsData, &nPos );
			}
			timing_info_present_flag =
			    GetBitFromBuffer( SpsData, &nPos, 1 );
			if( bDebugSPS )
			fprintf( stdout, 
				"timing_info_present_flag=%d\n",
				timing_info_present_flag );
			if( timing_info_present_flag )
			{
			    num_units_in_tick =
				GetBitFromBuffer( SpsData, &nPos, 32 );
			    time_scale =
				GetBitFromBuffer( SpsData, &nPos, 32 );
			    fixed_frame_rate_flag =
				GetBitFromBuffer( SpsData, &nPos, 1 );
			    if( bDebugSPS )
			    fprintf( stdout, "%3d/%3d\n",
				num_units_in_tick, time_scale );
			}
			// -------------------------------------------
			nal_hrd_parameters_present_flag[SPS_id] =
				GetBitFromBuffer( SpsData, &nPos, 1 );
			if( nal_hrd_parameters_present_flag[SPS_id] )
			{
			    int n;
			    cpb_cnt = GetGolombFromBuffer( SpsData, &nPos )+1;
			    GetBitFromBuffer( SpsData, &nPos, 4 );
			    GetBitFromBuffer( SpsData, &nPos, 4 );
			    for( n=0; n<cpb_cnt; n++ )
			    {
				GetGolombFromBuffer( SpsData, &nPos );
				GetGolombFromBuffer( SpsData, &nPos );
				GetBitFromBuffer( SpsData, &nPos, 1 );
			    }
			    initial_cpb_removal_delay_length =
				GetBitFromBuffer( SpsData, &nPos, 5 )+1;
			    cpb_removal_delay_length =
				GetBitFromBuffer( SpsData, &nPos, 5 )+1;
			    dpb_output_delay_length =
				GetBitFromBuffer( SpsData, &nPos, 5 )+1;
			    CpbDpbDelaysPresentFlag[SPS_id] = 1;
			    if( bDebug )
			    fprintf( stdout, 
				"initial_cpb_removal_delay_length=%d\n",
			    	initial_cpb_removal_delay_length );
			    if( bDebug )
			    fprintf( stdout, "cpb_removal_delay_length=%d\n",
			    	cpb_removal_delay_length );
			    if( bDebug )
			    fprintf( stdout, "dpb_output_delay_length=%d\n",
			    	dpb_output_delay_length );
			}
			// -------------------------------------------
			vcl_hrd_parameters_present_flag[SPS_id] =
				GetBitFromBuffer( SpsData, &nPos, 1 );
			if( vcl_hrd_parameters_present_flag[SPS_id] )
			{
			    int n;
			    cpb_cnt = GetGolombFromBuffer( SpsData, &nPos )+1;
			    GetBitFromBuffer( SpsData, &nPos, 4 );
			    GetBitFromBuffer( SpsData, &nPos, 4 );
			    for( n=0; n<cpb_cnt; n++ )
			    {
				GetGolombFromBuffer( SpsData, &nPos );
				GetGolombFromBuffer( SpsData, &nPos );
				GetBitFromBuffer( SpsData, &nPos, 1 );
			    }
			    initial_cpb_removal_delay_length =
				GetBitFromBuffer( SpsData, &nPos, 5 )+1;
			    cpb_removal_delay_length =
				GetBitFromBuffer( SpsData, &nPos, 5 )+1;
			    dpb_output_delay_length =
				GetBitFromBuffer( SpsData, &nPos, 5 )+1;
			    CpbDpbDelaysPresentFlag[SPS_id] = 1;
			    time_offset_length[SPS_id] =
				GetBitFromBuffer( SpsData, &nPos, 5 );
			    if( bDebug )
			    fprintf( stdout, 
			    	"initial_cpb_removal_delay_length=%d\n",
			    	initial_cpb_removal_delay_length );
			    if( bDebug )
			    fprintf( stdout, "cpb_removal_delay_length=%d\n",
			    	cpb_removal_delay_length );
			    if( bDebug )
			    fprintf( stdout, "dpb_output_delay_length=%d\n",
			    	dpb_output_delay_length );
			    if( bDebug )
			    fprintf( stdout, "time_offset_length(%d)=%d\n",
			    	SPS_id, time_offset_length[SPS_id] );
			}
			if( nal_hrd_parameters_present_flag[SPS_id]
			  | vcl_hrd_parameters_present_flag[SPS_id] )
			{
			    low_delay_hrd_flag[SPS_id] =
				GetBitFromBuffer( SpsData, &nPos, 1 );
			    if( bDebug )
			    fprintf( stdout, 
				"low_delay_hrd_flag(%d)=%d\n",
				SPS_id,
				low_delay_hrd_flag[SPS_id] );
			}
			pic_struct_present_flag[SPS_id] =
				GetBitFromBuffer( SpsData, &nPos, 1 );
			    fprintf( stdout, 
				"pic_struct_present_flag(%d)=%d\n",
				SPS_id,
				pic_struct_present_flag[SPS_id] );
		    }
		    if( time_scale<0 )
		    {
			fprintf( stdout, "%4dx%4d ?%c\n",
			    (pic_width_in_mbs_minus1+1)*16,
			    (pic_height_in_map_units_minus1+1)*16,
			    frame_mbs_only_flag[SPS_id] ? 'P' : 'I' );
		    } else {
			fprintf( stdout, "%4dx%4d %d/%d %c\n",
			    (pic_width_in_mbs_minus1+1)*16,
			    (pic_height_in_map_units_minus1+1)*16,
			    time_scale, num_units_in_tick*2,
			    frame_mbs_only_flag[SPS_id] ? 'P' : 'I' );
		    }
		} else {
		    if( nSpsPos<256 )
		    {
			SpsData[nSpsPos++] = buffer[pos];
		    }
		} 
		if( buffer[pos]==0x00 )
		    nZero++;
		else
		    nZero=0;
		break;
	    case STATE_PPS :
//		fprintf( stdout, "nZero=%d, [%02X]\n", nZero, buffer[pos] );
		if( (nZero>=2) && ((buffer[pos]==0x00) || (buffer[pos]==0x01)) )
		{
		    int nPos=0;
		    PPS_id = GetGolombFromBuffer( PpsData, &nPos );
		    PPStoSPS[PPS_id] = GetGolombFromBuffer( PpsData, &nPos );
		    fprintf( stdout, "PPStoSPS[%d]=%d\n",
		    	PPS_id, PPStoSPS[PPS_id] );
		    if( buffer[pos]==0x01 )
			state = 3;
		    else
			state = 2;
		} else {
		    if( nPpsPos<256 )
		    {
			PpsData[nPpsPos] = buffer[pos];
			nPpsPos++;
		    }
		}
		if( buffer[pos]==0x00 )
		    nZero++;
		else
		    nZero=0;
	    	break;
	    default :
		state = 0;
		break;
	    }
	}
	if( pos>0 )
	{
	    if( cpb_removal_delay>=0 )
	    {
			PTS = DTS+90000
			    *dpb_output_delay*num_units_in_tick/time_scale;
		    	fprintf( stdout, "%8llX : Output_PES_Header(%d,%d)\n",
				g_addr, DTS, PTS );
			Output_PES_Header( outfile, DTS, PTS );
	    }
	    fwrite( buffer, 1, pos, outfile );
	}

	fclose( infile );
	if( outfile )
	    fclose( outfile );
	return 0;
}

#ifndef MAIN
int bDebug=0;
int main( int argc, char *argv[] )
{
	return es2pes( argc, argv );
}
#endif

