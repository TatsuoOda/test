
/*
	ts-index.cpp
		2013.4.22 by T.Oda
		2013.4.23 +E : bAddEOS
		2013.4.24 Correct EndPts check and break position
		2013.4.24 print "." while GetIndex
		2013.5. 2 output min_PTS/max_PTS to index file
		2013.6. 7 show slice_type in character
		2013.6.14 +A : nAddFrame
		2013.6.19 +A -> Compare DTS 
		2013.6.19 +P -> bPrev (2GOP)
		2013.6.21 +M : bMake=1
		2013.6.21 +s : bFromStart
		2013.6.21 +e : bToEnd
		2013.7. 1 =AVC  : AVC 
		          =MPEG : MPEG
			  show PTS as hour:min:sec.ms
		2013.7.24 Support adaptation field
			  Handle picture in separate packet(AVC)
		2013.7.25 use MPEG_Data1/2
			  Handle picture in separate packet(MPEG)
			  Support TTS : GetIndex() done
		2013.7.26
			  Support TTS : GetPes() done
			  Parse profile/level/width/height/fps/ip
		2013.7.30 use Packets[PID]
*/

#include <stdio.h>
#include <stdlib.h>	// exit
#include <string.h>	// strncmp
#include <sys/time.h>	// gettimeofday

#include "tsLib.h"
#include "parseLib.h"
#include "lib.h"

#include "main.h"

// -----------------------------------------
#ifndef MAIN
int bDebug=0;
#endif
static int bShowNonKey=0;
static int bAddEOS=0;
static int nAddFrame=0;
static int bShow=0;
static int bDebugSPS=0;

//
// -----------------------------------------

static int Packets[0x2000];
static int bAVC=0;

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
int GetIndex( char *filename, char *indexFilename, int PID )
{
unsigned char buffer[1024];
struct timeval timeval0, timeval1, timevalc;
int packetSize=188;
int poffset=0;
int payloadSize=0;
int payloadType=0;
int nZero=0;

	fprintf( stdout, "GetIndex(%s:PID=%d)\n", filename, PID );
	gettimeofday( &timeval0, NULL );
	timeval1 = timeval0;
	FILE *fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open [%s]\n", filename );
	    exit( 1 );
	}
	FILE *pKey = fopen( indexFilename, "wb" );
	if( pKey==NULL )
	{
	    fprintf( stdout, "Can't write %s\n", indexFilename );
	    exit( 1 );
	}

	int nPacket=0;
	int Pid=(-1);
	int state=0;
	int SpsPos=0;
#define MAX_SPS_SIZE	1024
	unsigned char SpsData[MAX_SPS_SIZE];
	int nal_unit_type=(-1);
	int nSlice=0;
	int AVC_profile_idc = (-1);
	int AVC_constraint = (-1);
	int AVC_level_idc = (-1);
	int last_header=0;
	unsigned int PTSH=0xFFFFFFFF, PTSL=0xFFFFFFFF;
	unsigned int DTSH=0xFFFFFFFF, DTSL=0xFFFFFFFF;
	unsigned int PTS=0xFFFFFFFF;
	unsigned int DTS=0xFFFFFFFF;
	unsigned int min_PTS=0xFFFFFFFF;
	unsigned int max_PTS=0;
	int MPEG_Data1 = 0;
	int MPEG_Data2 = 0;
	int MPEG_Data3 = 0;
	int MPEG_Data4 = 0;
	int MPEG_CG = (-1);	// Closed GOP flag
	int MPEG_BL = (-1);	// Bloken Link flag
	int i=0;
	int notMatch=0;
	for( i=0; ; i++ )
	{
	    gettimeofday( &timevalc, NULL );
	    int tt;
	    tt = (timevalc.tv_sec -timeval1.tv_sec )*1000;
	    tt +=(timevalc.tv_usec-timeval1.tv_usec)/1000;
	    if( tt>1000 )
	    {
#if 0
	    	fprintf( stderr, "." );
		fflush( stderr );
#endif
	    	timeval1=timevalc;
	    }
	    int readed = gread( buffer, 1, packetSize, fp );
	    if( readed<packetSize )
	    {
//	    	fprintf( stdout, "EOF\n" );
		break;
	    }
	    if( buffer[poffset]!=0x47 )
	    {
	    	if( (i==0) && (buffer[4]==0x47) )
		{
		    fprintf( stdout, 
		    "%6d : Not TS :May be TTS : [%02X %02X %02X %02X %02X]\n",
			i, 
			buffer[0], buffer[1], buffer[2], buffer[3], buffer[4] );
		    packetSize = 192;
		    poffset = 4;
		    readed = gread( &buffer[188], 1, 4, fp );
		} else {
		fprintf( stdout, "%6d : Not TS : [%02X %02X %02X %02X %02X]\n",
			i, 
			buffer[0], buffer[1], buffer[2], buffer[3], buffer[4] );
		    exit( 1 );
		}
	    }
	    unsigned char *pbuf = (unsigned char *)&buffer[poffset];
	    Pid=((pbuf[1]<<8) | pbuf[2]) & 0x1FFF;
	    Packets[Pid]++;
//#define SHOW_ALL_PID
#ifdef SHOW_ALL_PID
	    if( bDebug )
	    if( Pid!=0x1FFF )
	    fprintf( stdout, "Pid=%4d(%llX)\n", Pid, g_addr-packetSize );
#endif
	    if( Pid==PID )
	    {
	    	notMatch=0;
#if 1
		if( bDebug )
		if( Pid!=0x1FFF )
		fprintf( stdout, "Pid=%4d(%llX)\n", Pid, g_addr-packetSize );
#endif
		int adapt = (pbuf[3]>>4)& 3;
//		int count = (pbuf[3]>>0)&15;
		int adapt_len = pbuf[4];
		int bAdapt=0;
		int bPayload=0;
		switch( adapt )
		{
		case 1 :
		    bAdapt = 0;
		    bPayload=1;
		    break;
		case 3 :
		    bAdapt = 1;
		    bPayload=1;
		    break;
		}
		int offset1=0;
		if( bAdapt )
		    offset1 = adapt_len+1;

		int offset2=0;
	    	if( (pbuf[4+offset1]==0x00)
	    	 && (pbuf[5+offset1]==0x00)
	    	 && (pbuf[6+offset1]==0x01)
	    	 && (pbuf[7+offset1]>=0xE0) && (pbuf[7+offset1]<=0xEF) )
		{	// PES header
		    if( bDebug )
		    fprintf( stdout, "PES_header(%llX)\n", g_addr );
		    int len = parsePES_header( &pbuf[8+offset1], 0,
		    	&PTSH, &PTSL, &DTSH, &DTSL, &offset2 );
//		    fprintf( stdout, "offset2=%d\n", offset2 );
		    if( bDebug )
			fprintf( stdout, "len=%d\n", len );
		    if( PTSH!=0xFFFFFFFF )
		    {
		    	if( DTSH==0xFFFFFFFF )
			    DTS=DTSL;
			else
			    DTS=PTSL;
			PTS=PTSL;
			if( PTS<min_PTS )
			    min_PTS = PTS;
			if( PTS>max_PTS )
			    max_PTS = PTS;
		    	if( bDebug | bShow )
		    	fprintf( stdout, "%8llX : %08X:%08X,%08X:%08X\n",
				g_addr-packetSize, PTSH, PTSL, DTSH, DTSL );
		    }
		    offset2+=8;
		    last_header = g_addr-packetSize;
		} else {
		    offset2=4;
		}
		int ii;
		int ms= PTS/90;
		int s = ms/1000;
		int m = s/60;
		int h = m/60;
		for( ii=offset1+offset2; ii<188; ii++ )
		{
#if 0
		    if( bDebug )
		    fprintf( stdout, "ii=%d(%d,%d,%d) state=%2d[%02X]\n",
			ii, offset1, offset2, packetSize, state, pbuf[ii] );
#endif
#define STATE_ZEROx1		1
#define STATE_ZEROx2		2
#define STATE_NON_IDR		4
#define STATE_SEI1		5
#define STATE_SEI2		6
#define STATE_SEI3		7
#define STATE_SPS		8

#define STATE_MPEG_PICTURE1	20
#define STATE_MPEG_PICTURE2	21
#define STATE_MPEG_GOP1		22
#define STATE_MPEG_GOP2		23
#define STATE_MPEG_GOP3		24
#define STATE_MPEG_GOP4		25
		    switch( state )
		    {
		    case 0 :
		    	if( pbuf[ii]==0x00 )
			    state=STATE_ZEROx1;
			break;
		    case STATE_ZEROx1 :	// 00
		    	if( pbuf[ii]==0x00 )
			    state=STATE_ZEROx2;
			else
			    state=0;
			break;
		    case STATE_ZEROx2 :	// 00 00
		    	if( pbuf[ii]==0x00 )
			{
			} else if( pbuf[ii]==0x01 )
			{
			    state=3;
			} else {
			    state=0;
			}
			break;
		    case 3 :	// 00 00 01
		    	if( bAVC )
			{
			    nal_unit_type = pbuf[ii]&0x1F;
			    switch( nal_unit_type )
			    {
			    case 1 :
				if( nSlice==0 )
				{
				    if( bDebug )
					fprintf( stdout, "non IDR\n" );
				    InitBitBuffer( );
				    state = STATE_NON_IDR;
				    continue;
				}
				break;
			    case 5 :
				if( nSlice==0 )
				{
				    if( bDebug )
//					fprintf( stdout, "IDR\n" );
					fprintf( stdout, "%s\n",
					    NAL_typeStr[nal_unit_type] );
				    fprintf( stdout, 
			    "%8X : KEY(IDR) PTS=%08X (%2d:%02d:%02d.%03d)\n",
				    last_header, PTS, h, m%60, s%60, ms%1000 );
				    if( pKey )
				    fprintf( pKey, "%8X : KEY(IDR) PTS=%08X\n",
					    last_header, PTS );
				}
				nSlice++;
				break;
			    case 6 :
				if( bDebug || bShow )
				{
				fprintf( stdout, "SEI : " );
				state = STATE_SEI1;
				continue;
				}
				break;
			    case 7 :
				if( bDebug || bShow )
				{
//				fprintf( stdout, "SPS\n" );
				    fprintf( stdout, "%s\n",
					NAL_typeStr[nal_unit_type] );
				state = STATE_SPS;
				memset( SpsData, 0, MAX_SPS_SIZE );
				nZero = 0;
				SpsPos=0;
				continue;
				}
				break;
			    case 8 :
				if( bDebug || bShow )
//				fprintf( stdout, "PPS\n" );
				    fprintf( stdout, "%s\n",
					NAL_typeStr[nal_unit_type] );
				break;
			    case 9 :
				if( bDebug || bShow )
//				fprintf( stdout, "AUD\n" );
				    fprintf( stdout, "%s\n",
					NAL_typeStr[nal_unit_type] );
				nSlice=0;
				break;
			    case 10 :
				if( bDebug || bShow )
//				fprintf( stdout, "EOSeq\n" );
				    fprintf( stdout, "%s\n",
					NAL_typeStr[nal_unit_type] );
				nSlice=0;
				break;
			    case 12 :
				if( bDebug || bShow )
//				fprintf( stdout, "filler\n" );
				    fprintf( stdout, "%s\n",
					NAL_typeStr[nal_unit_type] );
				break;
			    default :
				fprintf( stdout, 
				    "nal_unit_type=%2d(0x%llX,0x%llX:%d)\n", 
				    nal_unit_type, 
				    g_addr-packetSize,
				    g_addr-packetSize+poffset+ii, ii );
				break;
			    }
			} else {
			    switch( pbuf[ii] )
			    {
			    case 0x00 :	// MPEG Picture
			    	state = STATE_MPEG_PICTURE1;
				continue;
			    case 0xB8 :	// MPEG GOP
			    	state = STATE_MPEG_GOP1;
				continue;
			    default :
				break;
			    }
			}
			state = 0;
			break;
		    case STATE_NON_IDR :
		    	{
			    char SliceType[10][3] = {
				"P", "B", "I", 
				"SP", "SI", 
				"P", "B", "I", 
				"SP", "SI" };
			    int bufOffset=0;
			    int first_mb_in_slice    = 
				GetGolombFromBuffer( &pbuf[ii], &bufOffset );
			    int slice_type           = 
				GetGolombFromBuffer( &pbuf[ii], &bufOffset );
			    int pic_parameter_set_id = 
				GetGolombFromBuffer( &pbuf[ii], &bufOffset );
			    if( bDebug )
			    {
			    fprintf( stdout, "first_mb_in_slice=%d\n", 
				first_mb_in_slice );
			    fprintf( stdout, "slice_type=%d\n", 
				slice_type );
			    fprintf( stdout, "pic_parameter_set_id=%d\n", 
				pic_parameter_set_id );
			    }
			    if( (slice_type==2) || (slice_type==7) )
			    {
				fprintf( stdout, 
			"%8X : KEY( I ) PTS=%08X (%2d:%02d:%02d.%03d)\n",
				last_header, PTS, h, m%60, s%60, ms%1000 );
				if( pKey )
				fprintf( pKey, "%8X : KEY( I ) PTS=%08X\n",
					last_header, PTS );
			    } else {
				if( bShowNonKey )
				{
				    fprintf( stdout, 
					"%8X :  - (%2s ) PTS=%08X\n",
					last_header, SliceType[slice_type], 
					PTS );
				}
			    }
			    nSlice++;
			    state = 0;
			}
			break;
		    case STATE_SPS :
		    	if( pbuf[ii]==0x00 )
			    nZero++;
			else
			    nZero=0;
			if( nZero<3 )
			{
			    if( SpsPos<MAX_SPS_SIZE )
			    {
				SpsData[SpsPos++] = pbuf[ii];
			    }
			} else {
			    state = STATE_ZEROx2;
			    int nPos=0;
			    int chroma_format_idc;
			    int matrix_present_flag;
			    int seq_scaling_list_present_flag;
			    int pic_order_cnt_type;
			    int num_ref_frames;
			    int max_num_ref_frames;
			    int pic_width_in_mbs_minus1;
			    int pic_height_in_map_units_minus1;
			    int seq_parameter_set_id;
			    int frame_mbs_only_flag;
			    int direct_8x8;
			    int frame_cropping_flag;
			    int vui_parameters_present_flag;

			    if( bDebugSPS )
			    {
			    	fprintf( stdout, "SpsPos=%d\n", SpsPos );
			    }
			    InitBitBuffer( );
			    AVC_profile_idc = 
			    	GetBitFromBuffer( SpsData, &nPos, 8 );
			    AVC_constraint =
				GetBitFromBuffer( SpsData, &nPos, 8 );
			    AVC_level_idc = 
				GetBitFromBuffer( SpsData, &nPos, 8 );
			    seq_parameter_set_id = 
				    GetGolombFromBuffer( SpsData, &nPos );
			    fprintf( stdout, 
				"profile=%3d, constraint=%3d, level=%2d, ",
				AVC_profile_idc, AVC_constraint, AVC_level_idc);
			    if( bDebugSPS )
				fprintf( stdout, "SPS id=%d\n", 
				    seq_parameter_set_id );
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
			    frame_mbs_only_flag = 
				GetBitFromBuffer( SpsData, &nPos, 1 );

			    if( frame_mbs_only_flag==0 )
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
			    int time_scale=(-1);
			    int num_units_in_tick=(-1);
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
			    }
			    if( time_scale<0 )
			    {
				fprintf( stdout, "%4dx%4d ?%c\n",
				    (pic_width_in_mbs_minus1+1)*16,
				    (pic_height_in_map_units_minus1+1)*16,
				    frame_mbs_only_flag ? 'P' : 'I' );
			    } else {
				fprintf( stdout, "%4dx%4d %d/%d %c\n",
				    (pic_width_in_mbs_minus1+1)*16,
				    (pic_height_in_map_units_minus1+1)*16,
				    time_scale, num_units_in_tick*2,
				    frame_mbs_only_flag ? 'P' : 'I' );
			    }
			}
		    	break;
		    case STATE_SEI1 :
		    	{
			payloadType = pbuf[ii];
			payloadSize = 0;
			if( bDebug || bShow )
			{
			    if( payloadType<47 )
			    fprintf( stdout, 
				"%02X : %s\n", pbuf[ii], SEIstr[payloadType] );
			    else
			    fprintf( stdout, "%02X : ---\n", pbuf[ii] );
			}
			}
			state = STATE_SEI2;
			break;
		    case STATE_SEI2 :
		    	{
//			fprintf( stdout, "(%02X)\n", pbuf[ii] );
			payloadSize += pbuf[ii];
			nZero = 0;
			if( pbuf[ii]!=0xFF )
			    state = STATE_SEI3;
			}
			break;
		    case STATE_SEI3 :
		    	{
//			fprintf( stdout, "(%d:%02X)\n", payloadSize, pbuf[ii] );
			if( payloadSize>0 )
			{
			    if( (nZero==2) && (pbuf[ii]==0x03) )
			    {
			    	nZero=0;
			    } else {
				if( pbuf[ii]==0x00 )
				    nZero++;
				else
				    nZero=0;
				payloadSize--;
			    }
			} else {
			    if( pbuf[ii]==0x80 )
				state = 0;
			    else {
			    	fprintf( stdout, "Next(%02X)", pbuf[ii] );
				state = STATE_SEI1;
			    }
			}
			}
			break;
		    case STATE_MPEG_PICTURE1 :
//				TR   = GetBitStream( fp,10 );
//				PCT  = GetBitStream( fp, 3 );
			MPEG_Data1 = pbuf[ii];
			state = STATE_MPEG_PICTURE2;
			break;
			break;
		    case STATE_MPEG_PICTURE2 :
		    	{
			    MPEG_Data2 = pbuf[ii];
    #if 0
			    int TR = (pbuf[ii+1]<<2)
				   | (pbuf[ii+2]>>6);
			    int PCT= (pbuf[ii+2]>>3) & 7;
    #else
			    int TR = (MPEG_Data1<<2)
				   | (MPEG_Data2>>6);
			    int PCT= (MPEG_Data2>>3) & 7;
    #endif
			    if( bDebug )
				fprintf( stdout, "%02X %02X TR=%2d PCT=%d\n", 
					MPEG_Data1,
					MPEG_Data2,
					TR, PCT );
			    if( PCT==1 )
			    {
				fprintf( stdout, 
	    "%8X : KEY( I ) PTS=%08X (%2d:%02d:%02d.%03d) CG=%d,BL=%d\n",
				last_header, PTS, h, m%60, s%60, ms%1000,
				MPEG_CG, MPEG_BL );
				if( pKey )
				fprintf( pKey, "%8X : KEY( I ) PTS=%08X\n",
					    last_header, PTS );
			    } else {
			static char PctStr[4][2] = { "-", "I", "P", "B" };
				if( bShowNonKey )
				    fprintf( stdout, 
					"%8X :  - ( %s ) PTS=%08X\n",
					last_header, PctStr[PCT], 
					PTS );
			    }
			    state = 0;
			}
			break;
/*
	TC   = GetBitStream( fp,25 );
	CG   = GetBitStream( fp, 1 );	// Closed GOP flag
	BL   = GetBitStream( fp, 1 );
*/
		    case STATE_MPEG_GOP1 :
		    	MPEG_Data1 = pbuf[ii];
			state = STATE_MPEG_GOP2;
			break;
		    case STATE_MPEG_GOP2 :
		    	MPEG_Data2 = pbuf[ii];
			state = STATE_MPEG_GOP3;
			break;
		    case STATE_MPEG_GOP3 :
		    	MPEG_Data3 = pbuf[ii];
			state = STATE_MPEG_GOP4;
			break;
		    case STATE_MPEG_GOP4 :
		    	MPEG_Data4 = pbuf[ii];
			{
			int TC =  (MPEG_Data1<<17)
			       |  (MPEG_Data2<< 9)
			       |  (MPEG_Data3<< 1)
			       | ((MPEG_Data4>> 7)&1);
		        MPEG_CG =  (MPEG_Data4>> 6)&1;
		        MPEG_BL =  (MPEG_Data4>> 5)&1;
			if( bDebug )
			fprintf( stdout, "TC=%d, CG=%d, BL=%d\n",
				TC, MPEG_CG, MPEG_BL );
			}
			state = 0;
			break;
		    default :
			state = 0;
			break;
		    }
		}
	    } else {
	    	notMatch++;
		if( notMatch>10000 )
		{
		    fprintf( stdout, "notMatch(%d)\n", notMatch );
		    break;
		}
	    }
	    nPacket++;
	}
	fprintf( stdout, "\n" );
	fprintf( stdout, "%d packets\n", nPacket );
	fprintf( stdout, "min_PTS=%08X\n", min_PTS );
	fprintf( stdout, "max_PTS=%08X\n", max_PTS );
	if( pKey )
	{
	fprintf( pKey, "   -     : Last     PTS=%08X\n", PTS );
	fprintf( pKey, "min_PTS=%08X\n", min_PTS );
	fprintf( pKey, "max_PTS=%08X\n", max_PTS );
	}
	for( i=0; i<0x2000; i++ )
	{
	    if( Packets[i]>1024 )
	    {
	    	fprintf( stdout, "Packets[%4d]=%d\n", i, Packets[i] );
	    }
	}
	fclose( fp );
	if( pKey )
	    fclose( pKey );
	return 0;
}

int GetPes( char *filename, char *outfilename,
	unsigned int seekOffset, int PID, int EndPts )
{
unsigned char buffer[1024];

	int packetSize = 188;
	int bEnd = 0;
	fprintf( stdout, "GetPes(Offset=0x%X,PID=0x%X,End=0x%X)\n",
		seekOffset, PID, EndPts );
	FILE *fp = fopen( filename, "rb" );
	if( fp==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", filename );
	    exit( 1 );
	}
	FILE *pOut = fopen( outfilename, "wb" );
	if( pOut==NULL )
	    fprintf( stdout, "Can't write %s\n", outfilename );
	fseek( fp, seekOffset, SEEK_SET );
	fprintf( stdout, "EndPts=0x%X, seek@0x%X\n", EndPts, seekOffset );

	int nPacket=0;
	int Pid=(-1);
	int last_header=0;
	unsigned int PTSH=0xFFFFFFFF, PTSL=0xFFFFFFFF;
	unsigned int DTSH=0xFFFFFFFF, DTSL=0xFFFFFFFF;
	unsigned int PTS=0xFFFFFFFF;
	unsigned int DTS=0xFFFFFFFF;
	int i;
	int poffset=0;
	for( i=0; ; i++ )
	{
	    int readed = gread( buffer, 1, packetSize, fp );
	    if( readed<packetSize )
	    {
//	    	fprintf( stdout, "EOF\n" );
		break;
	    }
	    if( buffer[poffset]!=0x47 )
	    {
	    	if( (i==0) && (buffer[4]==0x47) )
		{
		    fprintf( stdout, 
		    "%6d : Not TS :May be TTS : [%02X %02X %02X %02X %02X]\n",
			i, 
			buffer[0], buffer[1], buffer[2], buffer[3], buffer[4] );
		    packetSize = 192;
		    poffset = 4;
		    readed = gread( &buffer[188], 1, 4, fp );
		} else {
		fprintf( stdout, "%6d : Not TS : [%02X %02X %02X %02X %02X]\n",
			i, 
			buffer[0], buffer[1], buffer[2], buffer[3], buffer[4] );
		    exit( 1 );
		}
	    }
	    unsigned char *pbuf = (unsigned char *)&buffer[poffset];
	    int adapt     = (pbuf[3]>>4)&3;
	    int adapt_len = pbuf[4];
	    int bPayload=0;
	    int ts_offset=0;
	    switch( adapt )
	    {
	    case 0 :
		break;
	    case 1 :	// no adapt
	    	bPayload=1;
		ts_offset=4;
		break;
	    case 2 :	// adapt
		break;
	    case 3 :	// adapt+payload
	    	bPayload=1;
		ts_offset=5+adapt_len;
		break;
	    }
	    Pid=((pbuf[1]<<8) | pbuf[2]) & 0x1FFF;
	    if( (Pid==PID) && bPayload )
	    {
	    	if( (pbuf[ts_offset+0]==0x00)
	    	 && (pbuf[ts_offset+1]==0x00)
	    	 && (pbuf[ts_offset+2]==0x01)
	    	 && (pbuf[ts_offset+3]>=0xE0) 
		 && (pbuf[ts_offset+3]<=0xEF) )
		{	// PES header
		    int offset;
		    int len = parsePES_header( &pbuf[ts_offset+4], 0,
		    	&PTSH, &PTSL, &DTSH, &DTSL, &offset );
		    if( bDebug )
			fprintf( stdout, "len=%d\n", len );
		    if( PTSH!=0xFFFFFFFF )
		    {
#if 0
		    	if( DTSH==0xFFFFFFFF )
			    DTS=DTSL;
			else
			    DTS=PTSL;
#else
		    	if( DTSH!=0xFFFFFFFF )
			    DTS=DTSL;
			else
			    DTS=PTSL;
#endif
			PTS=PTSL;
		    	if( bDebug )
		    	fprintf( stdout, "%8llX : %08X:%08X,%08X:%08X\n",
				g_addr-packetSize, PTSH, PTSL, DTSH, DTSL );
		    }
		    last_header = g_addr-packetSize;
		}
		if( PTS==EndPts )
		    bEnd = 1;
		int bDone=0;
		if( nAddFrame==0 )
		{
		    if( ((PTS>EndPts) && bEnd) || (PTS>(EndPts+3000*6)) )
		    	bDone=1;
		} else {
//		    if( (PTS>(EndPts+3000*nAddFrame)) )
		    if( (DTS>(EndPts+3000*nAddFrame)) )
		    {
		    	fprintf( stdout, "Found DTS=%8X > %8X\n",
				DTS, (EndPts+3000*nAddFrame) );
		    	bDone=1;
		    }
		}
		if( bDone )
		{
		    fprintf( stdout, "EndPts=%8X, PTS=%08X\n", EndPts, PTS );
		    fprintf( stdout, "offset=%8X, last=%8llX\n", 
		    	seekOffset, g_addr+seekOffset );
		    break;
		}
#if 1
		if( pOut )
		{
		fwrite( &pbuf[ts_offset], 1, 188-ts_offset, pOut );
//		fprintf( stdout, "output(%d)\n", nPacket );
		}
#else
		int ii;
		for( ii=ts_offset; ii<packetSize; ii++ )
		{
		    fprintf( stdout, "%02X ", buffer[ii] );
		}
#endif
	    }
	    nPacket++;
	}
	fclose( fp );
	if( pOut )
	{
	    if( bAddEOS )
	    {
	    	if( bAVC )
		{
		    buffer[0] = 0x00;
		    buffer[1] = 0x00;
		    buffer[2] = 0x01;
		    buffer[3] = 0x0A;
		    fwrite( buffer, 1, 4, pOut );
		} else {	// MPEG
		    buffer[0] = 0x00;
		    buffer[1] = 0x00;
		    buffer[2] = 0x01;
		    buffer[3] = 0xB7;
		    fwrite( buffer, 1, 4, pOut );
		}
	    }
	    fclose( pOut );
	}
	return 0;
}

// -----------------------------------------
void ts_index_Usage( char *prg )
{
	fprintf( stdout, 
		"%s [=AVC or =MPEG] filename -Ppid -Ofilename\n", prg );
	fprintf( stdout, 
		"\t+Nonkey +Gop +Prev +Make +start +end +An +Eos +Debug\n" );
	fprintf( stdout, "\t+N : Show No key frame\n" );
	fprintf( stdout, "\t+M : Force Make index.txt\n" );
	fprintf( stdout, "\t+G : output to End of GOP\n" );
	fprintf( stdout, "\t+P : output to Previous GOP\n" );
	fprintf( stdout, "\t+E : output with EOS\n" );
	exit( 1 );
}

int ts_index( int argc, char *argv[] )
{
int i;
int args=0;
char *filename=NULL;
int PID=(-1);
FILE *pKey=NULL;
int FirstPts=(-1);
int Prev_Pts=(-1);
int Error=0;
char outfilename[1024];
int bEndGop=0;
int bPrev=0;
int bMake=0;
int bFromStart=0;
int bToEnd=0;

	sprintf( outfilename, "out.pes" );
	for( i=0; i<0x2000; i++ )
	{
	    Packets[i] = 0;
	}
	for( i=1; i<argc; i++ )
	{
//	    fprintf( stdout, "argv[%d][0] = %c\n", i, argv[i][0] );
	    if( argv[i][0]=='-' )
	    {
	    	switch( argv[i][1] )
		{
		case 'P' :
		    PID = GetValue( &argv[i][2], &Error );
		    fprintf( stdout, "PID=%d\n", PID );
		    break;
		default :
		fprintf( stdout, "Parse param -\n" );
		    ts_index_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
	    	switch( argv[i][1] )
		{
		case 'D' :
		    bDebug=1;
		    break;
		case 'S' :
		    bShow=1;
		    break;
		case 'p' :
		    bDebugSPS=1;
		    break;
		case 'E' :
		    bAddEOS=1;
		    break;
		case 'N' :
		    bShowNonKey=1;
		    break;
		case 'A' :
		    nAddFrame = atoi( &argv[i][2] );
		    if( nAddFrame<1 )
			nAddFrame=1;
		    break;
		case 'O' :
		    sprintf( outfilename, "%s", &argv[i][2] );
		    break;
		case 'G' :
		    bEndGop=1;
		    break;
		case 'P' :
		    bPrev=1;
		    break;
		case 'M' :
		    bMake=1;
		    break;
		case 's' :
		    bFromStart=1;
		    break;
		case 'e' :
		    bToEnd=1;
		    break;
		default :
		fprintf( stdout, "Parse param +\n" );
		    ts_index_Usage( argv[0] );
		    break;
		}
#if 0
	    } else if( argv[i][0]=='/' )
	    {
	    	switch( argv[i][1] )
		{
		default :
		fprintf( stdout, "Parse param /\n" );
		    ts_index_Usage( argv[0] );
		    break;
		}
#endif
	    } else if( argv[i][0]=='=' )
	    {
	    	switch( argv[i][1] )
		{
		case 'A' :
		    bAVC=1;
		    break;
		case 'M' :
		    bAVC=0;
		    break;
		default :
		fprintf( stdout, "Parse param =\n" );
		    ts_index_Usage( argv[0] );
		    break;
		}
	    } else {
		switch( args )
		{
		case 0 :
		    filename=argv[i];
		    break;
		case 1 :
		    FirstPts = GetValue( argv[i], &Error );
		    break;
		case 2 :
		    Prev_Pts = GetValue( argv[i], &Error );
		    break;
		default :
		fprintf( stdout, "Parse args=%d\n", args );
		    ts_index_Usage( argv[0] );
		    break;
		}
		args++;
	    }
	}

	if( filename==NULL )
	{
	    fprintf( stdout, "No filename\n" );
	    ts_index_Usage( argv[0] );
	}
	if( PID<0 )
	{
	    fprintf( stdout, "No PID\n" );
	    ts_index_Usage( argv[0] );
	}

	unsigned int index[1024*64][2];
	int nIndex=0;
	unsigned int LastPts=0;

	static char indexFilename[] = "index.txt";

	if( bMake )
	    GetIndex( filename, indexFilename, PID );

	pKey = fopen( indexFilename, "rb" );
	char buffer[1024];
	if( pKey )
	{
	    fprintf( stdout, "index (%s) exist\n", indexFilename );
	    unsigned int PTS=0xFFFFFFFF;
	    unsigned int min_PTS=0xFFFFFFFF;
	    unsigned int max_PTS=0;
	    while( 1 )
	    {
	    	PTS=0xFFFFFFFF;
	    	if( fgets( buffer, 1024, pKey )==NULL )
		    break;
		if( strncmp( &buffer[8], " : KEY(IDR) ", 12 )==0)
		{
		    sscanf( buffer, "%X : KEY(IDR) PTS= %X",
			&index[nIndex][0], &index[nIndex][1] );
		    PTS = index[nIndex][1];
		} else if(strncmp( &buffer[8], " : KEY( I ) ", 12 )==0)
		{
		    sscanf( buffer, "%X : KEY( I ) PTS= %X",
			&index[nIndex][0], &index[nIndex][1] );
		    PTS = index[nIndex][1];
		} else if( strncmp( &buffer[8], " : Last     ", 12 )==0 )
		{
		    sscanf( buffer, " - : Last     PTS= %X", &LastPts );
		    fprintf( stdout, "LastPts=%8X\n", LastPts );
		    break;
		} else {
			fprintf( stdout, "Can't parse %s", buffer );
		}
		if( PTS!=0xFFFFFFFF )
		{
		    if( PTS<min_PTS )
			min_PTS = PTS;
		    if( PTS>max_PTS )
			max_PTS = PTS;
		}
		nIndex++;
	    }
	    fclose( pKey );
	    fprintf( stdout, "min_PTS=%08X\n", min_PTS );
	    fprintf( stdout, "max_PTS=%08X\n", max_PTS );
	    unsigned int offset = 0;
	    unsigned int prev_offset = (-1);
	    unsigned int keyPts = 0;
	    if( Prev_Pts>0 )
	    {
	    	for( i=1; i<nIndex; i++ )
		{
		    prev_offset = index[i-1][0];
		    if( index[i][1]>Prev_Pts )
		    {
		    	fprintf( stdout, "Prev_Pts=%8X\n", Prev_Pts );
		    	fprintf( stdout, "%8X : KEY(IDR) PTS=%08X\n",
				prev_offset, index[i-1][1] );
		    	break;
		    }
		}
	    }
	    if( FirstPts>=0 )
	    {
	    	int bNextKey=0;
		int nextOffset = 0;
	    	int nextKeyPts = 0;
		int prevOffset = (-1);
		int prevKeyPts = 0;
		fprintf( stdout, "FirstPts=%8X\n", FirstPts );
	    	for( i=1; i<=nIndex; i++ )
		{
		    offset     = index[i-1][0];
		    keyPts     = index[i-1][1];
		    if( i==nIndex )
		    {
			nextOffset = -1;
			nextKeyPts = -1;
		    } else {
			nextOffset = index[i][0];
			nextKeyPts = index[i][1];
		    }
		    fprintf( stdout, 
			"offset=%8X, keyPts=%8X, nextKeyPts=%8X\n",
			offset, keyPts, nextKeyPts );
		    if( nextKeyPts>FirstPts )
		    {
		    	bNextKey=1;
		    	break;
		    }
		    if( nextKeyPts<0 )
		    	break;
		    prevOffset = offset;
		    prevKeyPts = keyPts;
		}
		if( bPrev>0 )
		{
		    if( prevOffset>=0 )
		    {
		    offset = prevOffset;
		    keyPts = prevKeyPts;
		    }
		}
		if( bNextKey==0 )
		{
		    nextKeyPts = LastPts;
		}
		if( bFromStart )
		    offset = 0;
		int s = keyPts/90000;
		int m = s/60;
		int h = m/60;
		fprintf( stdout, "%8X : KEY(IDR) PTS=%08X (%2d:%02d:%02d)\n", 
			offset, keyPts, h, m%60, s%60 );
		if( offset == prev_offset )
		{
		    fprintf( stdout, "Same offset\n" );
		} else {
		    if( bToEnd )
		    {
			GetPes( filename, outfilename,
			    offset, PID, LastPts );
		    } else if( bEndGop )
		    {
			GetPes( filename, outfilename,
			    offset, PID, nextKeyPts );
		    } else {
			GetPes( filename, outfilename,
    #if 1
			    offset, PID, FirstPts );
    #else
			    offset, PID, FirstPts+3000*6 );
    #endif
		    }
		}
#if 0
	    } else {
		    GetPes( filename, outfilename, 0, PID, LastPts );
#endif
	    }
	} else {
	    GetIndex( filename, indexFilename, PID );
	}

	return 0;
}

#ifndef MAIN
int main( int argc, char *argv[] )
{
	return ts_index( argc, argv );
}
#endif

