
/*
	pes2tts.cpp
		2013.7.31  by T.Oda

		2013.11.29 : rate may be small in some stream
			start rate modified
		2013.12. 5 : Video and Audio
		2013.12. 9 : continuity counter NG : use nPacket[]
		2013.12.10 : outM.mts : merge multi pes
		             +E : expand to longest pes
		2013.12.11 : +M : MaxDiff : rateOK threshold
			     =A : AudioCodec
				AudioCodec==0 	// MPEG1_AUDIO
				AudioCodec==1 	// AC3 : Dolby Digital
				AudioCodec==2	// MPEG2_AUDIO
				AudioCodec==3	// MPEG4-AAC(HEAAC)
				AudioCodec==4	// MPEG2-AAC
		2013.12.18 : Support file[0:9]
		2013.12.19 : use MakePat
			     use MakePmt
		2013.12.20 : Add Dolby & HEAAC ES_info
		2013.12.24 : HEAAC PMT change
		2013.12.27 : AAC2/AAC5/HEAAC2/HEAAC5
			     DOLBY1/DOLBY2/DOLBY5
			     DUAL1_AAC2/DUAL1_AAC5/DUAL1_HEAAC2/DUAL1_HEAAC5
			     DUAL2_AAC2/DUAL2_AAC5/DUAL2_HEAAC2/DUAL2_HEAAC5
				AudioCodec==0 	// MPEG1_AUDIO
				AudioCodec==1	// MPEG2_AUDIO
				AudioCodec==2 	// AC3 : Dolby Digital
				AudioCodec==3	// MPEG2-AAC
				AudioCodec==4	// MPEG4-AAC(HEAAC)
		2014.1.9  : NIT(PID=0x10), SDT(PID=0x11)
		2014.1.10 : use AudioConfig[0,3,...] as Pid
				Can set ProgramId, TransportStreamId
		2014.1.16 : change AAC descriptor 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>	// memset

#include "tsLib.h"
#include "parseLib.h"
#include "lib.h"

#include "main.h"

static int bDisplay=0;
static int AudioCodec=0;
static int ProgramId=1;
static int TransportStreamId=1;

#define TABLE_UNIT	8
#define MAX_FILE 256

#define VIDEO_AVC		0

#define AUDIO_MPEG1		11	// TMPGEnc
#define AUDIO_MPEG2		12	// TMPGEnc
#define AUDIO_DOLBY_TMPG	13	// TMPGEnc
#define AUDIO_AAC_TMPG		14	// TMPGEnc
#define AUDIO_HEAAC_TMPG	15	// TMPGEnc

#define AUDIO_DOLBY1		16	// Dolby stream
#define AUDIO_DOLBY2		17	// Dolby stream
#define AUDIO_DOLBY5		18	// Dolby stream

#define AUDIO_AAC2		20	// Dolby stream
#define AUDIO_AAC5		21	// Dolby stream
#define AUDIO_HEAAC2		22	// Dolby stream
#define AUDIO_HEAAC5		23	// Dolby stream

#define AUDIO_DUAL1_AAC2	24	// Dolby stream
#define AUDIO_DUAL1_AAC5	25	// Dolby stream
#define AUDIO_DUAL1_HEAAC2	26	// Dolby stream
#define AUDIO_DUAL1_HEAAC5	27	// Dolby stream

#define AUDIO_DUAL2_AAC2	28	// Dolby stream
#define AUDIO_DUAL2_AAC5	29	// Dolby stream
#define AUDIO_DUAL2_HEAAC2	30	// Dolby stream
#define AUDIO_DUAL2_HEAAC5	31	// Dolby stream

static int MaxDiff=45000;	// 0.5sec

// -------------------------
// PID=0x10
static int NIT_DataA[] = {
0x00,0x40,0xF1,0xC2,0xF4,0x26,0xC3,0x01,
0x01,0xF0,0x00,0xF1,0xB5,0x00,0xD9,0xF0,
0x01,0xF0,0x0D,0x44,0x0B,0x06,0x34,0x00,
0x00,0xFF,0xF2,0x05,0x00,0x69,0x00,0x0F,
0x00,0xDA,0xF0,0x01,0xF0,0x0D,0x44,0x0B,
0x06,0x42,0x00,0x00,0xFF,0xF2,0x05,0x00,
0x69,0x00,0x0F,0x04,0x45,0x00,0x01,0xF0,
0x0D,0x44,0x0B,0x06,0x50,0x00,0x00,0xFF,
0xF2,0x05,0x00,0x69,0x00,0x0F,0x00,0xDC,
0xF0,0x01,0xF0,0x0D,0x44,0x0B,0x06,0x58,
0x00,0x00,0xFF,0xF2,0x05,0x00,0x69,0x00,
0x0F,0x00,0xDD,0xF0,0x01,0xF0,0x0D,0x44,
0x0B,0x06,0x66,0x00,0x00,0xFF,0xF2,0x05,
0x00,0x69,0x00,0x0F,0x00,0xDE,0xF0,0x01,
0xF0,0x0D,0x44,0x0B,0x06,0x74,0x00,0x00,
0xFF,0xF2,0x05,0x00,0x69,0x00,0x0F,0x00,
0xDF,0xF0,0x01,0xF0,0x0D,0x44,0x0B,0x06,
0x82,0x00,0x00,0xFF,0xF2,0x03,0x00,0x69,
0x00,0x0F,0x00,0xE0,0xF0,0x01,0xF0,0x0D,
0x44,0x0B,0x06,0x90,0x00,0x00,0xFF,0xF2,
0x03,0x00,0x69,0x00,0x0F,0x00,0xE1,0xF0,
0x01,0xF0,0x0D,0x44,0x0B,0x06,0x98,0x00,
0x00,0xFF,0xF2,0x03,0x00,0x69,0x00,0x0F,
0x00,0xE2,0xF0,0x01,0xF0,0x0D,0x44,0x0B,
0x07,0x06,0x00,0x00,0xFF,0xF2,0x03,0x00,
0x69,0x00,0x0F,0x00,0xE3,0xF0,0x01,0xF0,
0x0D,0x44,0x0B,0x07,0x14,0x00,0x00,0xFF,
0xF2,0x03,0x00,0x69,0x00,0x0F,0x00,0xE4,
0xF0,0x01,0xF0,0x0D,0x44,0x0B,0x07,0x22,
0x00,0x00,0xFF,0xF2,0x03,0x00,0x69,0x00,
0x0F,0x00,0xE5,0xF0,0x01,0xF0,0x0D,0x44,
0x0B,0x07,0x30,0x00,0x00,0xFF,0xF2,0x03,
0x00,0x69,0x00,0x0F,0x00,0xE6,0xF0,0x01,
0xF0,0x0D,0x44,0x0B,0x07,0x38,0x00,0x00,
0xFF,0xF2,0x03,0x00,0x69,0x00,0x0F,0x00,
0xE7,0xF0,0x01,0xF0,0x0D,0x44,0x0B,0x07,
0x46,0x00,0x00,0xFF,0xF2,0x03,0x00,0x69,
0x00,0x0F,0x00,0xE8,0xF0,0x01,0xF0,0x0D,
0x44,0x0B,0x07,0x54,0x00,0x00,0xFF,0xF2,
0x03,0x00,0x69,0x00,0x0F,0x00,0xE9,0xF0,
0x01,0xF0,0x0D,0x44,0x0B,0x07,0x62,0x00,
0x00,0xFF,0xF2,0x03,0x00,0x69,0x00,0x0F,
0x00,0xEA,0xF0,0x01,0xF0,0x0D,0x44,0x0B,
0x07,0x70,0x00,0x00,0xFF,0xF2,0x03,0x00,
0x69,0x00,0x0F,0x00,0xEB,0xF0,0x01,0xF0,
0x0D,0x44,0x0B,0x07,0x78,0x00,0x00,0xFF,
0xF2,0x03,0x00,0x69,0x00,0x0F,0x00,0xEC,
0xF0,0x01,0xF0,0x0D,0x44,0x0B,0x07,0x86,
0x00,0x00,0xFF,0xF2,0x03,0x00,0x69,0x00,
0x0F,0x00,0xED,0xF0,0x01,0xF0,0x0D,0x44,
0x0B,0x07,0x94,0x00,0x00,0xFF,0xF2,0x03,
0x00,0x69,0x00,0x0F,0x00,0xEE,0xF0,0x01,
0xF0,0x0D,0x44,0x0B,0x08,0x02,0x00,0x00,
0xFF,0xF2,0x03,0x00,0x69,0x00,0x0F,0x00,
0xEF,0xF0,0x01,0xF0,0x0D,0x44,0x0B,0x08,
0x10,0x00,0x00,0xFF,0xF2,0x03,0x00,0x69,
0x00,0x0F,0xFC,0x97,0xB5,0xFF,0xFF,0xFF,
/*
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
*/
-1 };
static int NIT_DataB[] = {
0x00,0x40,0xF3,0xF4,0xF4,0x26,0xC3,0x00,
0x01,0xF0,0x39,0x40,0x0A,0x53,0x4F,0x4E,
0x59,0x20,0x4F,0x73,0x61,0x6B,0x69,0x4A, // [  9@ SONY OsakiJ]
0x07,0x00,0x01,0x00,0x85,0x00,0x00,0x0A,
0x4A,0x07,0x00,0x02,0x00,0x85,0x0F,0x80,
0xF0,0x4A,0x07,0x00,0x11,0x00,0x85,0x0F,
0x84,0xF2,0x4A,0x07,0x00,0x11,0x00,0x85,
0x0F,0x85,0xF3,0x4A,0x07,0x00,0x03,0x00,
0x85,0x00,0x64,0x90,0xF3,0xAE,0x27,0x12,
0xF0,0x01,0xF0,0x3C,0x44,0x0B,0x03,0x94,
0x00,0x00,0xFF,0xF2,0x05,0x00,0x69,0x00,
0x0F,0x41,0x2D,0xC3,0xBA,0x01,0xC6,0x0D,
0x01,0xC6,0x0E,0x01,0xC6,0x0F,0x01,0xC6,
0x10,0x01,0xC6,0x12,0x01,0xC6,0x13,0x01,
0xC6,0x14,0x01,0xC6,0x15,0x01,0xC6,0x16,
0x01,0xCF,0x79,0x01,0xCF,0x7A,0x01,0xCF,
0x7C,0x01,0xCF,0xD8,0x01,0xD0,0x99,0x01,
0x04,0x37,0x00,0x01,0xF0,0x0D,0x44,0x0B,
0x03,0x46,0x00,0x00,0xFF,0xF2,0x03,0x00,
0x69,0x00,0x0F,0x04,0x31,0x00,0x01,0xF0,
0x0D,0x44,0x0B,0x01,0x21,0x00,0x00,0xFF,
0xF2,0x03,0x00,0x69,0x00,0x0F,0x04,0x4D,
0x00,0x01,0xF0,0x0D,0x44,0x0B,0x01,0x13,
0x00,0x00,0xFF,0xF2,0x03,0x00,0x69,0x00,
0x0F,0x00,0x02,0x00,0x85,0xF0,0x21,0x44,
0x0B,0x03,0x70,0x00,0x00,0xFF,0xF2,0x03,
0x00,0x69,0x00,0x0F,0x41,0x12,0x00,0x0A,
0x01,0x00,0x0B,0x01,0x00,0x2B,0x01,0x00,
0x09,0x01,0x00,0x10,0x01,0x00,0x17,0x01,
0x00,0x03,0x00,0x85,0xF0,0x1B,0x44,0x0B,
0x03,0x62,0x00,0x00,0xFF,0xF2,0x03,0x00,
0x69,0x00,0x0F,0x41,0x0C,0x00,0x29,0x01,
0x00,0x14,0x01,0x02,0x04,0x01,0x00,0x11,
0x01,0x00,0x04,0x00,0x85,0xF0,0x63,0x44,
0x0B,0x03,0x78,0x00,0x00,0xFF,0xF2,0x03,
0x00,0x69,0x00,0x0F,0x41,0x54,0x00,0x1B,
0x01,0x00,0x12,0x01,0x02,0x01,0x01,0x02,
0x98,0x01,0x00,0x2A,0x01,0x00,0x24,0x01,
0x02,0x03,0x01,0x00,0x96,0x02,0x00,0x9B,
0x02,0x00,0xA3,0x02,0x00,0x98,0x02,0x00,
0x97,0x02,0x00,0xA2,0x02,0x00,0x9A,0x02,
0x00,0xA6,0x02,0x00,0xA7,0x02,0x00,0xA4,
0x02,0x00,0x9C,0x02,0x00,0x94,0x02,0x00,
0x92,0x02,0x00,0x93,0x02,0x00,0x95,0x02,
0x00,0x99,0x02,0x00,0x91,0x02,0x00,0x9D,
0x02,0x00,0x9F,0x02,0x00,0xA5,0x02,0x00,
0x9E,0x02,0x00,0x11,0x00,0x85,0xF0,0x27,
0x44,0x0B,0x03,0x54,0x00,0x00,0xFF,0xF2,
0x03,0x00,0x69,0x00,0x0F,0x41,0x18,0x00,
0x22,0x01,0x00,0x1C,0x01,0x00,0x13,0x01,
0x00,0x0E,0x01,0x00,0x0C,0x01,0x00,0x0D,
0x01,0x00,0x0F,0x01,0x00,0x15,0x01,0x00,
0x01,0x00,0x85,0xF0,0x27,0x44,0x0B,0x03,
0x86,0x00,0x00,0xFF,0xF2,0x03,0x00,0x69,
0x00,0x0F,0x41,0x18,0x00,0xA8,0x01,0x00,
0x18,0x01,0x02,0x06,0x01,0x00,0x16,0x01,
0x00,0x08,0x01,0x00,0x32,0x01,0x00,0x1D,
0x01,0x00,0x34,0x01,0x00,0x70,0xF0,0x01,
0xF0,0x3F,0x44,0x0B,0x04,0x02,0x00,0x00,
0xFF,0xF2,0x05,0x00,0x69,0x00,0x0F,0x41,
0x30,0xD0,0xFD,0x01,0xD0,0xFE,0x01,0xD1,
0x00,0x01,0xD1,0x01,0x01,0xD1,0x02,0x01,
0xD1,0x03,0x01,0xD1,0x06,0x01,0xD1,0x07,
0x01,0xCF,0x72,0x01,0xCF,0xDB,0x01,0xCF,
0xDD,0x01,0xCF,0xDF,0x01,0xCF,0xE0,0x01,
0xCF,0xE1,0x01,0xCF,0xE2,0x01,0xD1,0x08,
0x01,0x27,0x11,0xF0,0x01,0xF0,0x33,0x44,
0x0B,0x04,0x34,0x00,0x00,0xFF,0xF2,0x05,
0x00,0x69,0x00,0x0F,0x41,0x24,0xCF,0xD1,
0x01,0xCF,0xD2,0x01,0xCF,0xD3,0x01,0xCF,
0xD4,0x01,0xCF,0xD5,0x01,0xCF,0xD6,0x01,
0xCF,0xD9,0x01,0xCF,0xDA,0x01,0xD1,0x04,
0x01,0xD1,0x05,0x01,0xCF,0xDC,0x01,0xCF,
0xE3,0x01,0x27,0x13,0xF0,0x01,0xF0,0xB7,
0x44,0x0B,0x04,0x18,0x00,0x00,0xFF,0xF2,
0x05,0x00,0x69,0x00,0x0F,0x41,0xA8,0xC3,
0xB5,0x01,0xC3,0xB6,0x01,0xC3,0xB7,0x01,
0xC3,0xB9,0x01,0xC3,0xBC,0x01,0xC3,0xBD,
0x01,0xC3,0xBF,0x01,0xD0,0x35,0x01,0xD0,
0x36,0x01,0xC4,0x19,0x02,0xC4,0x1A,0x02,
0xC4,0x1B,0x02,0xC4,0x1C,0x02,0xC4,0x1D,
0x02,0xC4,0x1E,0x02,0xC4,0x1F,0x02,0xC4,
0x20,0x02,0xC4,0x21,0x02,0xC4,0x22,0x02,
0xC4,0x23,0x02,0xC4,0x24,0x02,0xC4,0x25,
0x02,0xC4,0x26,0x02,0xC4,0x27,0x02,0xC4,
0x29,0x02,0xC4,0x2B,0x02,0xC4,0x2C,0x02,
0xC4,0x2D,0x02,0xC4,0x2E,0x02,0xC4,0x2F,
0x02,0xC4,0x30,0x02,0xC4,0x31,0x02,0xC4,
0x32,0x02,0xC4,0x33,0x02,0xC4,0x34,0x02,
0xC4,0x35,0x02,0xC4,0x36,0x02,0xC4,0x37,
0x02,0xC4,0x38,0x02,0xC4,0x39,0x02,0xC4,
0x3A,0x02,0xC4,0x3B,0x02,0xC4,0x3C,0x02,
0xC4,0x3D,0x02,0xC4,0x3E,0x02,0xC4,0x3F,
0x02,0xC4,0x40,0x02,0xC4,0x41,0x02,0xC4,
0x42,0x02,0xC4,0x43,0x02,0xC4,0x44,0x02,
0xC4,0x45,0x02,0xC4,0x46,0x02,0xC4,0x47,
0x02,0xC4,0xE1,0x02,0xC4,0xE2,0x02,0x27,
0x14,0xF0,0x01,0xF0,0x39,0x44,0x0B,0x04,
0x10,0x00,0x00,0xFF,0xF2,0x05,0x00,0x69,
0x00,0x0F,0x41,0x2A,0xC4,0xE3,0x02,0xCF,
0x6D,0x01,0xCF,0x74,0x01,0xCF,0x75,0x01,
0xCF,0x78,0x01,0xD0,0x9C,0x01,0xD0,0x9E,
0x01,0xD0,0x9F,0x01,0xD0,0xA0,0x01,0xD0,
0xA1,0x01,0xD0,0xA2,0x01,0xD0,0xA3,0x01,
0xD0,0xA4,0x01,0xD0,0xFF,0x01,0x27,0x15,
0xF0,0x01,0xF0,0x36,0x44,0x0B,0x04,0x26,
0x00,0x00,0xFF,0xF2,0x05,0x00,0x69,0x00,
0x0F,0x41,0x27,0xC3,0xB8,0x01,0xC3,0xBB,
0x01,0xC4,0x7D,0x01,0xC4,0x7E,0x01,0xC4,
0x7F,0x01,0xC4,0x80,0x01,0xC4,0x81,0x01,
0xC4,0x82,0x01,0xC4,0x83,0x01,0xCF,0x71,
0x01,0xCF,0x7B,0x01,0xD0,0x9A,0x01,0xD0,
0x9D,0x01,0x00,0x21,0x00,0x85,0xF0,0x0D,
0x44,0x0B,0x04,0x66,0x00,0x00,0xFF,0xF2,
0x03,0x00,0x69,0x00,0x0F,0x27,0x17,0xF0,
0x01,0xF0,0x0D,0x44,0x0B,0x04,0x50,0x00,
0x00,0xFF,0xF2,0x05,0x00,0x69,0x00,0x0F,
0x04,0x41,0x00,0x01,0xF0,0x0D,0x44,0x0B,
0x04,0x58,0x00,0x00,0xFF,0xF2,0x03,0x00,
0x69,0x00,0x0F,0x2A,0xF9,0xF0,0x01,0xF0,
0x0D,0x44,0x0B,0x04,0x42,0x00,0x00,0xFF,
0xF2,0x05,0x00,0x69,0x00,0x0F,0x00,0x06,
0x00,0x85,0xF0,0x0D,0x44,0x0B,0x06,0x18,
0x00,0x00,0xFF,0xF2,0x05,0x00,0x69,0x00,
0x0F,0x04,0x5F,0x00,0x01,0xF0,0x0D,0x44,
0x0B,0x06,0x26,0x00,0x00,0xFF,0xF2,0x05,
0x00,0x69,0x00,0x0F,0xA1,0x82,0x39,0x11,
-1,
};
// PID=0x11
static int SDT_Data[] = {
0x00,0x42,0xF0,0x2C,0x04,0x5F,0xCB,0x00,
0x00,0x00,0x01,0xFF,0x27,0xD8,0xFC,0x80,//[ B , _      '   ]
0x1B,0x48,0x19,0x19,0x16,0x53,0x4F,0x4E,
0x59,0x20,0x4F,0x73,0x61,0x6B,0x69,0x20,//[ H   SONY Osaki ]
0x53,0x69,0x67,0x6E,0x61,0x6C,0x20,0x54,
0x65,0x61,0x6D,0x00,0x2E,0xCD,0x65,0x8D,//[Signal Team . e ]
-1,
};
// ---------------------------------------------------------------
void pes2tts_Usage( char *prg )
{
	fprintf( stdout, 
"%s filename [-Ooutname] [+Ooffset] [+Dmaxdiff] [-RrateKB]\n", 
	prg );
	fprintf( stdout, "\t[+Aconfig]\n" );
	fprintf( stdout, "\t\t+A%d : VIDEO_AVC\n", VIDEO_AVC );
	fprintf( stdout, "\t\t+A%d : AUDIO_MPEG1\n", AUDIO_MPEG1 );
	fprintf( stdout, "\t\t+A%d : AUDIO_MPEG2\n", AUDIO_MPEG2 );
	fprintf( stdout, "\t\t+A%d : DOLBY(TMPG)\n", AUDIO_DOLBY_TMPG );
	fprintf( stdout, "\t\t+A%d : AAC(TMPG)\n", AUDIO_AAC_TMPG );
	fprintf( stdout, "\t\t+A%d : HEAAC(TMPG)\n", AUDIO_HEAAC_TMPG );
	fprintf( stdout, "\t\t+A%d : DOLBY1\n", AUDIO_DOLBY1 );
	fprintf( stdout, "\t\t+A%d : DOLBY2\n", AUDIO_DOLBY2 );
	fprintf( stdout, "\t\t+A%d : DOLBY5\n", AUDIO_DOLBY5 );

	fprintf( stdout, "\t\t+A%d : AAC2\n", AUDIO_AAC2 );
	fprintf( stdout, "\t\t+A%d : AAC5\n", AUDIO_AAC5 );
	fprintf( stdout, "\t\t+A%d : HEAAC2\n", AUDIO_HEAAC2 );
	fprintf( stdout, "\t\t+A%d : HEAAC5\n", AUDIO_HEAAC5 );

	fprintf( stdout, "\t\t+A%d : DUAL1_AAC2\n", AUDIO_DUAL1_AAC2 );
	fprintf( stdout, "\t\t+A%d : DUAL1_AAC5\n", AUDIO_DUAL1_AAC5 );
	fprintf( stdout, "\t\t+A%d : DUAL1_HEAAC2\n", AUDIO_DUAL1_HEAAC2 );
	fprintf( stdout, "\t\t+A%d : DUAL1_HEAAC5\n", AUDIO_DUAL1_HEAAC5 );

	fprintf( stdout, "\t\t+A%d : DUAL2_AAC2\n", AUDIO_DUAL2_AAC2 );
	fprintf( stdout, "\t\t+A%d : DUAL2_AAC5\n", AUDIO_DUAL2_AAC5 );
	fprintf( stdout, "\t\t+A%d : DUAL2_HEAAC2\n", AUDIO_DUAL2_HEAAC2 );
	fprintf( stdout, "\t\t+A%d : DUAL2_HEAAC5\n", AUDIO_DUAL2_HEAAC5 );

	exit( 1 );
}


//int *Table[MAX_FILE];
// Table[0]=DTS
// Table[1]=PTS
// Table[2]=head_addr
// Table[3]=head_addr-l_head_addr
// Table[4]=DTS'-packet*188*90/rate
// Table[5]=DTS'
// Table[6]=space
void DumpTable( int *Table[], int nfile, int n, int diff )
{
	fprintf( stdout, 
	    "%5d : %8X %8X %8X %8X : %8X %8X :%3d (%7d)\n",
		n, 
		Table[nfile][n*TABLE_UNIT+0], 
		Table[nfile][n*TABLE_UNIT+1], 
		Table[nfile][n*TABLE_UNIT+2], 
		Table[nfile][n*TABLE_UNIT+3],
		Table[nfile][n*TABLE_UNIT+4], 
		Table[nfile][n*TABLE_UNIT+5],
		Table[nfile][n*TABLE_UNIT+6],
		diff
	    );
}

#if 1
int MakePat( unsigned char *bytes, int PID[], int nPid, 
	int transport_stream_id, int programId )
{
int n;
int index=0;
int PatPid=0;	// always 0
int section_length = 9+nPid*4;
	bytes[ 0] = 0x47;
	bytes[ 1] = ((PatPid>>8)&0x1F) | 0x40;
	bytes[ 2] =  (PatPid>>0)&0xFF;
	bytes[ 3] = 0x10;	// no adapt+payload

	bytes[ 0+4] = 0x00;	// pointer_field
	bytes[ 1+4] = 0x00;	// table_id=00 : PAT
	bytes[ 2+4] = 0xB0;
	bytes[ 3+4] = section_length;
	bytes[ 4+4] = (transport_stream_id>>8)&0xFF;
	bytes[ 5+4] = (transport_stream_id>>0)&0xFF;
	bytes[ 6+4] = 0xC1;	// version_number, current_netxt_indicator
	bytes[ 7+4] = 0x00;	// section_number
	bytes[ 8+4] = 0x00;	// last_section_number
	index = 9+4;
	for( n=0; n<nPid; n++ )
	{
	    bytes[index++] = (programId>>8)&0xFF;
	    bytes[index++] = (programId>>0)&0xFF;
	    bytes[index++] = ((PID[n]>>8)&0xFF) | 0xE0;
	    bytes[index++] = ((PID[n]>>0)&0xFF);
	    programId++;
	}
	unsigned int crc = crc32( &bytes[5], index-5 );
	bytes[index++] = (crc>>24)&0xFF;
	bytes[index++] = (crc>>16)&0xFF;
	bytes[index++] = (crc>> 8)&0xFF;
	bytes[index++] = (crc>> 0)&0xFF;

	return index;
}
#else
int MakePat( unsigned char *byteS, int PatPid )
{
	byteS[ 0] = 0x47;
	byteS[ 1] = ((PatPid>>8)&0x1F) | 0x40;
	byteS[ 2] =  (PatPid>>0)&0xFF;
	byteS[ 3] = 0x10;	// no adapt+payload

	byteS[ 4] = 0x00;
	byteS[ 5] = 0x00;
	byteS[ 6] = 0xB0;
	byteS[ 7] = 0x0D;
	byteS[ 8] = 0x00;
	byteS[ 9] = 0x00;
	byteS[10] = 0xC1;
	byteS[11] = 0x00;
	byteS[12] = 0x00;
	byteS[13] = 0x00;
	byteS[14] = 0x01;
	byteS[15] = 0xE1;
	byteS[16] = 0x00;
	byteS[17] = 0xB3;
	byteS[18] = 0x58;
	byteS[19] = 0x82;
	byteS[20] = 0xB7;
	return 0;
}
#endif
int MakePmt( unsigned char *bytes, 
	int program_number,
	int PgmPid,
	int PcrPid,
	int program_detail[], int nPid )
{
int n;
int index=0;
int section_length = 9+nPid*4;
int program_info_length=0;
int stream_type=(-1);
	bytes[ 0] = 0x47;
	bytes[ 1] = ((PgmPid>>8)&0x1F) | 0x40;
	bytes[ 2] =  (PgmPid>>0)&0xFF;
	bytes[ 3] = 0x10;	// no adapt+payload

	bytes[ 0+4] = 0x00;	// pointer_field
	bytes[ 1+4] = 0x02;	// table_id=02 : PMT
	bytes[ 2+4] = 0xB0;
	bytes[ 3+4] = section_length;
	bytes[ 4+4] = (program_number>>8)&0xFF;
	bytes[ 5+4] = (program_number>>0)&0xFF;
	bytes[ 6+4] = 0xC1;	// version_number, current_netxt_indicator
	bytes[ 7+4] = 0x00;	// section_number
	bytes[ 8+4] = 0x00;	// last_section_number
	bytes[ 9+4] = ((PcrPid>>8) & 0xFF) | 0xE0;
	bytes[10+4] = ((PcrPid>>0) & 0xFF);
	bytes[11+4] = ((program_info_length>>8) & 0xFF) | 0xF0;
	bytes[12+4] = ((program_info_length>>0) & 0xFF);
	index = 13+4;
static int AAC_Type[4] = {
0x51,	// AAC2
0x53,	// AAC5.1
0x58,	// HEAAC2
0x5A,	// HEAAC5.1
};
	for( n=0; n<nPid; n++ )
	{
	    int elementary_PID = program_detail[n*3+0];
	    int codec_type     = program_detail[n*3+1];
	    int type           = program_detail[n*3+2];
	    fprintf( stdout, "elementary_PID=0x%X\n", elementary_PID );
	    int ES_info_length = 0;
	    unsigned char ES_info[16];
/*
#define AUDIO_DOLBY		1
#define AUDIO_DOLBY_TMPG	2
#define AUDIO_HEAAC		3
#define AUDIO_HEAAC_DUAL	4
#define AUDIO_AAC		5
*/
	    switch( codec_type )
	    {
	    case VIDEO_AVC :
		stream_type = 27;
	    	break;
	    case AUDIO_MPEG1 :
		stream_type = 3;
	    	break;
	    case AUDIO_MPEG2 :
		stream_type = 4;
	    	break;
	    case AUDIO_DOLBY_TMPG :
		stream_type = 129;
		ES_info_length = 8;
		ES_info[0] = 0x81;
		ES_info[1] = 0x06;
		ES_info[2] = 0x08;
		ES_info[3] = 0x38;
		ES_info[4] = 0x0F;
		ES_info[5] = 0xFF;
		ES_info[6] = 0xEF;
		ES_info[7] = 0x01;
	    	break;
	    case AUDIO_AAC_TMPG :
	    	stream_type = 15;
		ES_info_length = 0;
	    	break;
	    case AUDIO_HEAAC_TMPG :
	    	stream_type = 17;
		ES_info_length = 0;
	    	break;
	    // ---------------------------
	    case AUDIO_DOLBY1 :
	    case AUDIO_DOLBY2 :
	    case AUDIO_DOLBY5 :
		stream_type = 6;
		ES_info_length = 12;
		ES_info[ 0] = 0x7A;	// enhanced_AC-3_descriptor
		ES_info[ 1] = 0x04;	// len
		ES_info[ 2] = (type==2) ? 0xD8 : 0xE0;
		if( type==2 )	// AD
		ES_info[ 3] = 0x90+(codec_type-AUDIO_DOLBY1)*2;
		else
		ES_info[ 3] = 0xC0+(codec_type-AUDIO_DOLBY1)*2;
		ES_info[ 4] = 0x10;
		ES_info[ 5] = 0x01;
		// ----------------------------------------------------
		ES_info[ 6] = 0x0A;	// ISO_639_language_descriptor
		ES_info[ 7] = 0x04;	// len
		ES_info[ 8] = 0x65;	// ISO_639_language_code
		ES_info[ 9] = 0x6E;
		ES_info[10] = 0x67;
		ES_info[11] = (type==2) ? 0x03 : 0x00;
	    	break;
	    case AUDIO_AAC2 :
	    case AUDIO_AAC5 :
	    case AUDIO_HEAAC2 :
	    case AUDIO_HEAAC5 :
	    	stream_type = 17;
		ES_info_length = 10;
		ES_info[ 0] = 0x7C;	// AAC descriptor_tag
		ES_info[ 1] = 0x02;
		ES_info[ 2] = AAC_Type[codec_type-AUDIO_AAC2];
		ES_info[ 3] = 0x7F;
		ES_info[ 4] = 0x0A;	// ISO_639_language_descriptor
		ES_info[ 5] = 0x04;	// len
		ES_info[ 6] = 0x65;	// ISO_639_language_code
		ES_info[ 7] = 0x6E;
		ES_info[ 8] = 0x67;
		ES_info[ 9] = 0x00;
		break;
	    case AUDIO_DUAL1_AAC2 :
	    case AUDIO_DUAL1_AAC5 :
	    case AUDIO_DUAL1_HEAAC2 :
	    case AUDIO_DUAL1_HEAAC5 :
	    	stream_type = 17;
		ES_info_length = 14;
		ES_info[ 0] = 0x0A;	// ISO_639_language_descriptor
		ES_info[ 1] = 0x04;	// len
		ES_info[ 2] = 0x65;	// ISO_639_language_code
		ES_info[ 3] = 0x6E;
		ES_info[ 4] = 0x67;
		ES_info[ 5] = (type==2) ? 0x03 : 0x00;	// 03:commentary
		// ----------------------------------------------------
		ES_info[ 6] = 0x7C;	// AAC desctiptor_tag
		ES_info[ 7] = 0x03;
		ES_info[ 8] = AAC_Type[codec_type-AUDIO_DUAL1_AAC2];// 0x58/5A
#if 0
		ES_info[ 9] = 0xFF;
		ES_info[10] = (type==2) ? 0x40 : 0x03;	// audio description 
#else
		ES_info[ 9] = 0x80;	// AAC_type_flag=1
		ES_info[10] = (type==2) ? 0x47 : 0x03;	// receiver mix
#endif
		// ----------------------------------------------------
		ES_info[11] = 0x52;	// stream_identifier_descriptor
		ES_info[12] = 0x01;	// len
		ES_info[13] = (type==2) ? 0x31 : 0x30;	// component_tag
		break;
	    // ----------------------------
	    case AUDIO_DUAL2_AAC2 :
	    case AUDIO_DUAL2_AAC5 :
	    case AUDIO_DUAL2_HEAAC2 :
	    case AUDIO_DUAL2_HEAAC5 :
	    	stream_type = 17;
		    ES_info[ 0] = 0x0A;	// ISO_639_language_descriptor
		    ES_info[ 1] = 0x04;
		    ES_info[ 2] = 0x65;	// ISO_639_language_code
		    ES_info[ 3] = 0x6E;
		    ES_info[ 4] = 0x67;
		    ES_info[ 5] = 0x00;
		// ----------------------------------------------------
		    ES_info[ 6] = 0x7C;	// AAC descriptor_tag
		    ES_info[ 7] = 0x02;
		    ES_info[ 8] = AAC_Type[codec_type-AUDIO_DUAL2_AAC2]; //0x58
		    ES_info[ 9] = 0x7F;	// AAC_type_flag=0
		// ----------------------------------------------------
		    ES_info[10] = 0x52;	// stream_identifier_descriptor
		    ES_info[11] = 0x01;	// len
		if( type==2 )
		{
		    ES_info_length = 20;
		    ES_info[ 5] = 0x03;

		    ES_info[12] = 0x41;	// component_tag
		// ----------------------------------------------------
		    ES_info[13] = 0x7F;	// extension descriptor
		    ES_info[14] = 0x05;	// len
		    ES_info[15] = 0x06;	// descriptor_tag_extension : supplement
		    ES_info[16] = 0x07;
		    ES_info[17] = 0x65;
		    ES_info[18] = 0x6E;
		    ES_info[19] = 0x67;
		} else {
		    ES_info_length = 13;
		    ES_info[ 5] = 0x00;

		    ES_info[12] = 0x40;	// component_tag
		}
		break;
	    }
	    fprintf( stdout, "stream_type   =%d\n", stream_type );
	    bytes[index++] = stream_type;
	    bytes[index++] = ((elementary_PID>>8)&0xFF) | 0xE0;
	    bytes[index++] = ((elementary_PID>>0)&0xFF);
	    bytes[index++] = (ES_info_length>>8) | 0xF0;
	    bytes[index++] = ES_info_length;
	    int e;
	    for( e=0; e<ES_info_length; e++ )
	    {
	    	bytes[index++] = ES_info[e];
	    }
	}
	bytes[ 3+4] = section_length = index-4;

	unsigned int crc = crc32( &bytes[5], index-5 );
	bytes[index++] = (crc>>24)&0xFF;
	bytes[index++] = (crc>>16)&0xFF;
	bytes[index++] = (crc>> 8)&0xFF;
	bytes[index++] = (crc>> 0)&0xFF;

	return index;
}


int Pes2Tts( FILE *infile, unsigned int off, 
	unsigned int sz, 
	unsigned int PCR, 
	int *nPacket,
	int AudioConfig[], int nConfig,
	FILE *outfile, int Pid, int rateKB, int bPCR, int bPAT )
{
int s;
int written;
unsigned char packet[192];
int PcrPid=0x101;
	fseek( infile, off, SEEK_SET );
unsigned int nPCR = PCR*300;	// 90kHz->27MHz
unsigned char *byteS = &packet[4];
#if 0
	fprintf( stdout, "-------------\n" );
	fprintf( stdout, "PCR=%X(%X)\n", PCR, nPCR );
#endif
	fprintf( stdout, "Pes2Tts(%04X:%8X:%d,%d)%8X\n", 
		Pid, PCR, bPCR, bPAT, nPCR );

	if( bPCR )
	{
	    // --------------------------------
	    // PCR packet
	    memset( packet, 0, 192 );
	    packet[0] = (nPCR>>24)&0xFF;
	    packet[1] = (nPCR>>16)&0xFF;
	    packet[2] = (nPCR>> 8)&0xFF;
	    packet[3] = (nPCR>> 0)&0xFF;

	    byteS[0] = 0x47;
	    byteS[1] = (PcrPid>>8)&0x1F;
	    byteS[2] = (PcrPid>>0)&0xFF;
	    byteS[3] = 0x20;	// adapt+no payload

	    byteS[4] = 0x07;	// adapt_len
	    byteS[5] = 0x10;	// bPCR=1
	    unsigned int nPCR_H = (PCR>>17);
	    unsigned int nPCR_L = (PCR<<15) & 0xFFFF8000;
	    byteS[ 6] = (nPCR_H>> 8)&0xFF;
	    byteS[ 7] = (nPCR_H>> 0)&0xFF;
	    byteS[ 8] = (nPCR_L>>24)&0xFF;
	    byteS[ 9] = (nPCR_L>>16)&0xFF;
	    byteS[10] = (nPCR_L>> 8)&0xFF;
	    byteS[11] = (nPCR_L>> 0)&0xFF;
	    written = fwrite( packet, 1, 192,  outfile );
	    nPCR = nPCR + 27000*188/rateKB;
	}

	if( bPAT )
	{
	    // --------------------------------
	    // PAT packet
	    memset( packet, 0xFF, 192 );
	    packet[0] = (nPCR>>24)&0xFF;
	    packet[1] = (nPCR>>16)&0xFF;
	    packet[2] = (nPCR>> 8)&0xFF;
	    packet[3] = (nPCR>> 0)&0xFF;
#define PROGRAMS1	1
static int ProgramPids1[PROGRAMS1] = { 0x100 };
#define PROGRAMS2	2
static int ProgramPids2[PROGRAMS2] = { 0x100, 0x110 };
	    if( AudioCodec!=5 )
		MakePat( byteS, ProgramPids1, PROGRAMS1, 
		    TransportStreamId, ProgramId );
	    else
		MakePat( byteS, ProgramPids2, PROGRAMS2,
		    TransportStreamId, ProgramId );
	    written = fwrite( packet, 1, 192,  outfile );
	    nPCR = nPCR + 27000*188/rateKB;

	    // --------------------------------
	    // PMT packet
	    memset( packet, 0xFF, 192 );
	    packet[0] = (nPCR>>24)&0xFF;
	    packet[1] = (nPCR>>16)&0xFF;
	    packet[2] = (nPCR>> 8)&0xFF;
	    packet[3] = (nPCR>> 0)&0xFF;
#if 1
#define N_PROGRAM0 2
static int Program0[N_PROGRAM0*3] = {
	0x200, VIDEO_AVC, 0,	// AVC
	0x201, AUDIO_MPEG1, 1,	// MPEG1_AUDIO
};
#define N_PROGRAM1 2
static int Program1[N_PROGRAM1*3] = {
	0x200, VIDEO_AVC, 0,	// AVC
	0x201, AUDIO_MPEG2, 1,	// MPEG2_AUDIO
};
#define N_PROGRAM2 2
static int Program2[N_PROGRAM2*3] = {
	0x200, VIDEO_AVC, 0,	// AVC
	0x201, AUDIO_DOLBY_TMPG, 1,	// Dolby
};
#define N_PROGRAM3 2
static int Program3[N_PROGRAM3*3] = {
	0x200, VIDEO_AVC, 0,	// AVC
	0x201, AUDIO_AAC_TMPG, 1,	// MPEG2-AAC
};
#define N_PROGRAM4 2
static int Program4[N_PROGRAM4*3] = {
	0x200, VIDEO_AVC, 0,	// AVC
	0x201, AUDIO_HEAAC_TMPG, 1,	// MPEG4-AAC:HEAAC
};
#define N_PROGRAM5 3
static int Program5[N_PROGRAM5*3] = {
	0x200, VIDEO_AVC, 0,	// AVC   : Video
	0x201, AUDIO_DOLBY5, 1,	// Dolby : Audio main
	0x204, AUDIO_DOLBY2, 2,	// Dolby : Audio AD
};
#define N_PROGRAM6 3
static int Program6[N_PROGRAM6*3] = {
	0x200, VIDEO_AVC, 0,	// AVC
	0x202, AUDIO_DUAL1_HEAAC5, 1,	// HEAAC
	0x207, AUDIO_DUAL1_HEAAC2, 2,	// HEAAC
};
	    if( nConfig>0 )
	    {
		    MakePmt( byteS, ProgramId, 
		    	0x100, 0x101, AudioConfig, nConfig/3 );
	    } else {
		switch( AudioCodec )
		{
		case 0 : // MPEG1_AUDIO
		    MakePmt( byteS, ProgramId,
		    	0x100, 0x101, Program0, N_PROGRAM0 );
		    break;
		case 1 : // MPEG2_AUDIO
		    MakePmt( byteS, ProgramId, 
		    	0x100, 0x101, Program1, N_PROGRAM1 );
		    break;
		case 2 :	// AC3 : Dolby Digital
		    MakePmt( byteS, ProgramId, 
		    	0x100, 0x101, Program2, N_PROGRAM2 );
		    break;
		case 3 : // MPEG2-AAC
		    MakePmt( byteS, ProgramId, 
		    	0x100, 0x101, Program3, N_PROGRAM3 );
		    break;
		case 4 : // MPEG4-AAC:HEAAC
		    MakePmt( byteS, ProgramId, 
		    	0x100, 0x101, Program4, N_PROGRAM4 );
		    break;
		case 5 : //	Multi pid
		    MakePmt( byteS, ProgramId, 
		    	0x100, 0x101, Program5, N_PROGRAM5 );
		    break;
		case 6 : //	Multi pid
		    MakePmt( byteS, ProgramId, 
		    	0x100, 0x101, Program6, N_PROGRAM6 );
		    break;
		case 7 : //	Multi pid
		    MakePmt( byteS, ProgramId, 
		    	0x100, 0x101, Program5, N_PROGRAM5 );
		    written = fwrite( packet, 1, 192,  outfile );
		    MakePmt( byteS, ProgramId+1, 
		    	0x110, 0x101, Program6, N_PROGRAM6 );
		    break;
		}
	    }
	    written = fwrite( packet, 1, 192,  outfile );
#else
	    int PmtPid=0x100;
	    byteS[ 0] = 0x47;
	    byteS[ 1] = ((PmtPid>>8)&0x1F) | 0x40;
	    byteS[ 2] =  (PmtPid>>0)&0xFF;
	    byteS[ 3] = 0x10;	// no adapt+payload
	    if( AudioCodec==1 )	// AC3 : Dolby Digital
	    {
		byteS[ 4] = 0x00;
		byteS[ 5] = 0x02;
		byteS[ 6] = 0xB0;
		byteS[ 7] = 0x1F;	//
		byteS[ 8] = 0x00;
		byteS[ 9] = 0x01;
		byteS[10] = 0xC1;
		byteS[11] = 0x00;
		byteS[12] = 0x00;
		byteS[13] = 0xE1;
		byteS[14] = 0x01;
		byteS[15] = 0xF0;
		byteS[16] = 0x00;
		byteS[17] = 0x1B;
		byteS[18] = 0xE2;
		byteS[19] = 0x00;
		byteS[20] = 0xF0;
		byteS[21] = 0x00;
		// Dolby : 81 + ES_info
		byteS[22] = 0x81;	//
		byteS[23] = 0xE2;
		byteS[24] = 0x01;
		byteS[25] = 0xF0;
		byteS[26] = 0x08;
		// Dolby ES_info
		byteS[27] = 0x81;
		byteS[28] = 0x06;
		byteS[29] = 0x08;
		byteS[30] = 0x38;
		byteS[31] = 0x0F;
		byteS[32] = 0xFF;
		byteS[33] = 0xEF;
		byteS[34] = 0x01;
		// CRC
		byteS[35] = 0x68;
		byteS[36] = 0x09;
		byteS[37] = 0x26;
		byteS[38] = 0xF6;
	    } else if( AudioCodec==4 )	// MPEG2-AAC
	    {
		byteS[ 4] = 0x00;
		byteS[ 5] = 0x02;
		byteS[ 6] = 0xB0;
		byteS[ 7] = 0x17;
		byteS[ 8] = 0x00;
		byteS[ 9] = 0x01;
		byteS[10] = 0xC1;
		byteS[11] = 0x00;
		byteS[12] = 0x00;
		byteS[13] = 0xE1;
		byteS[14] = 0x01;
		byteS[15] = 0xF0;
		byteS[16] = 0x00;
		byteS[17] = 0x1B;
		byteS[18] = 0xE2;
		byteS[19] = 0x00;
		byteS[20] = 0xF0;
		byteS[21] = 0x00;
		// MPEG2-AAC : 0F
		byteS[22] = 0x0F;
		byteS[23] = 0xE2;
		byteS[24] = 0x01;
		byteS[25] = 0xF0;
		byteS[26] = 0x00;
		// CRC
		byteS[27] = 0xB5;
		byteS[28] = 0x57;
		byteS[29] = 0xBE;
		byteS[30] = 0xED;
	    } else if( AudioCodec==3 )	// MPEG4-AAC:HEAAC
	    {
		byteS[ 4] = 0x00;
		byteS[ 5] = 0x02;
		byteS[ 6] = 0xB0;
		byteS[ 7] = 0x17;
		byteS[ 8] = 0x00;
		byteS[ 9] = 0x01;
		byteS[10] = 0xC1;
		byteS[11] = 0x00;
		byteS[12] = 0x00;
		byteS[13] = 0xE1;
		byteS[14] = 0x01;
		byteS[15] = 0xF0;
		byteS[16] = 0x00;
		byteS[17] = 0x1B;
		byteS[18] = 0xE2;
		byteS[19] = 0x00;
		byteS[20] = 0xF0;
		byteS[21] = 0x00;
		// MPEG4-AAC : 11
		byteS[22] = 0x11;
		byteS[23] = 0xE2;
		byteS[24] = 0x01;
		byteS[25] = 0xF0;
		byteS[26] = 0x00;
		// CRC
		byteS[27] = 0xC5;
		byteS[28] = 0x82;
		byteS[29] = 0xFB;
		byteS[30] = 0x7E;
	    } else if( AudioCodec==2 )	// MPEG2_AUDIO
	    {
		byteS[ 4] = 0x00;
		byteS[ 5] = 0x02;
		byteS[ 6] = 0xB0;
		byteS[ 7] = 0x17;
		byteS[ 8] = 0x00;
		byteS[ 9] = 0x01;
		byteS[10] = 0xC1;
		byteS[11] = 0x00;
		byteS[12] = 0x00;
		byteS[13] = 0xE1;
		byteS[14] = 0x01;
		byteS[15] = 0xF0;
		byteS[16] = 0x00;
		byteS[17] = 0x1B;
		byteS[18] = 0xE2;
		byteS[19] = 0x00;
		byteS[20] = 0xF0;
		byteS[21] = 0x00;
		// MPEG2_AUDIO : 04
		byteS[22] = 0x04;
		byteS[23] = 0xE2;
		byteS[24] = 0x01;
		byteS[25] = 0xF0;
		byteS[26] = 0x00;
		// CRC
		byteS[27] = 0x2F;
		byteS[28] = 0xA9;
		byteS[29] = 0x11;
		byteS[30] = 0x7C;
	    } else {	// MPEG1_AUDIO
		byteS[ 4] = 0x00;
		byteS[ 5] = 0x02;
		byteS[ 6] = 0xB0;
		byteS[ 7] = 0x17;
		byteS[ 8] = 0x00;
		byteS[ 9] = 0x01;
		byteS[10] = 0xC1;
		byteS[11] = 0x00;
		byteS[12] = 0x00;
		byteS[13] = 0xE1;
		byteS[14] = 0x01;
		byteS[15] = 0xF0;
		byteS[16] = 0x00;
		byteS[17] = 0x1B;
		byteS[18] = 0xE2;
		byteS[19] = 0x00;
		byteS[20] = 0xF0;
		byteS[21] = 0x00;
		// MPEG1_AUDIO : 03
		byteS[22] = 0x03;
		byteS[23] = 0xE2;
		byteS[24] = 0x01;
		byteS[25] = 0xF0;
		byteS[26] = 0x00;
		// CRC
		byteS[27] = 0xD4;
		byteS[28] = 0x4A;
		byteS[29] = 0x3A;
		byteS[30] = 0x68;
	    }
	    written = fwrite( packet, 1, 192,  outfile );
#endif
	    nPCR = nPCR + 27000*188/rateKB;
	}
#if 0
	if( bPAT )
	{
	    // --------------------------------
	    // SDT packet
	    int Pid = 0x11;
	    memset( packet, 0xFF, 192 );
	    packet[0] = (nPCR>>24)&0xFF;
	    packet[1] = (nPCR>>16)&0xFF;
	    packet[2] = (nPCR>> 8)&0xFF;
	    packet[3] = (nPCR>> 0)&0xFF;
	    byteS[0] = 0x47;
	    byteS[1] = (Pid>>8)&0x1F;
	    byteS[2] = (Pid>>0)&0xFF;
	    byteS[3] = 0x20;	// adapt+no payload
	    byteS[4] = 0x07;	// adapt_len
	    byteS[5] = 0x10;	// bPCR=1
	    unsigned int nPCR_H = (PCR>>17);
	    unsigned int nPCR_L = (PCR<<15) & 0xFFFF8000;
	    byteS[ 6] = (nPCR_H>> 8)&0xFF;
	    byteS[ 7] = (nPCR_H>> 0)&0xFF;
	    byteS[ 8] = (nPCR_L>>24)&0xFF;
	    byteS[ 9] = (nPCR_L>>16)&0xFF;
	    byteS[10] = (nPCR_L>> 8)&0xFF;
	    byteS[11] = (nPCR_L>> 0)&0xFF;
	    written = fwrite( packet, 1, 192,  outfile );
	    nPCR = nPCR + 27000*188/rateKB;
	}
#endif
	// --------------------------------
	for( s=0; s<sz; s+=184 )
	{
	    int readSize=sz-s;
	    if( readSize>184 )
	    	readSize=184;
	    memset( packet, 0, 192 );
	    packet[0] = (nPCR>>24)&0xFF;
	    packet[1] = (nPCR>>16)&0xFF;
	    packet[2] = (nPCR>> 8)&0xFF;
	    packet[3] = (nPCR>> 0)&0xFF;
/*
	    PID = ((bytes[1]<<8) | (bytes[2]<<0)) & 0x1FFF;
//	    int ts_error     = (bytes[1]>>7)&1;
	    int payloadStart = (bytes[1]>>6)&1;
//	    int ts_priority  = (bytes[1]>>5)&1;
	    int adapt = (bytes[3]>>4)&3;
	    int count = bytes[3]&15;
	    int adapt_len = bytes[4];
	    	switch( adapt )
		{
		case 0 :	// ?
		    fprintf( stdout, "adapt=%d\n", adapt );
		    break;
		case 1 :	// no adapt
		    bPayload[PID]=1;
		    bAdapt  =0;
		    break;
		case 2 :	// adapt
		    bPayload[PID]=0;
		    bAdapt  =1;
		    break;
		case 3 :	// adapt+payload
		    bPayload[PID]=1;
		    bAdapt  =1;
		    break;
		}
*/
	    int head=4;
	    byteS[0] = 0x47;
	    byteS[1] = (Pid>>8)&0x1F;
	    byteS[2] = (Pid>>0)&0xFF;
	    byteS[3] = 0x10 | ((*nPacket)&0xF);
	    if( readSize<=182 )
	    {
	    	head = 188-readSize;
		byteS[3] |= 0x20;	// Adaptation field
		byteS[4] =  head-5;	// Adaptation length
		byteS[5] =  0x00;	// no PCR
	    } else if( readSize<184 )
	    {
//	    	fprintf( stdout, "readSize=%d\n", readSize );
	    	head = 188-readSize;
		byteS[3] |= 0x20;	// Adaptation field
		byteS[4] =  head-5;	// Adaptation length
	    }
	    if( s==0 )
	    	byteS[1] |= 0x40;	// payloadStart
	    fread( &byteS[head], 1, readSize, infile );
#if 0
	    if( (s==0) || (readSize<184) )
	    {
		for( i=0; i<readSize; i++ )
		{
		    fprintf( stdout, "%02X ", packet[4+i] );
		    if( (i%16)==15 )
		    	fprintf( stdout, "\n" );
		}
		fprintf( stdout, "\n" );
	    }
#endif	    
	    written = fwrite( packet, 1, 192,  outfile );
	    if( written<188 )
	    {
	    	fprintf( stdout, "written=%d\n", written );
		exit( 1 );
	    }
	    (*nPacket)++;
	    nPCR = nPCR + 27000*188/rateKB;
	}
//	fprintf( stdout, "nPCR=%8X\n", nPCR );
//	fprintf( stdout, "Pes2Tts() done\n" );
	return nPCR/300;
}

int XXX2Tts( int Packet[], unsigned int PCR, 
	int *nPacket,
	FILE *outfile, int Pid, int rateKB )
{
int s;
int written;
unsigned char packet[192];
unsigned int nPCR = PCR*300;	// 90kHz->27MHz
unsigned char *byteS = &packet[4];
#if 0
	fprintf( stdout, "-------------\n" );
	fprintf( stdout, "PCR=%X(%X)\n", PCR, nPCR );
#endif
	fprintf( stdout, "Pes2Tts(%04X:%8X)%8X\n", Pid, PCR, nPCR );

	// --------------------------------
	for( s=0; Packet[s]>=0;  )
	{
//	    memset( packet, 0, 192 );
	    memset( packet, 0xFF, 192 );
	    packet[0] = (nPCR>>24)&0xFF;
	    packet[1] = (nPCR>>16)&0xFF;
	    packet[2] = (nPCR>> 8)&0xFF;
	    packet[3] = (nPCR>> 0)&0xFF;
	    byteS[0] = 0x47;
	    byteS[1] = (Pid>>8)&0x1F;
	    byteS[2] = (Pid>>0)&0xFF;
	    byteS[3] = 0x10 | ((*nPacket)&0xF);
	    if( s==0 )
	    	byteS[1] |= 0x40;	// payloadStart
	    int ss;
	    for( ss=0; ss<184; ss++ )
	    {
	    	if( Packet[s]>=0 )
		    byteS[4+ss] = Packet[s++];
	    }
#if 0
//	    if( (s==0) || (readSize<184) )
	    {
		for( i=0; i<184; i++ )
		{
		    fprintf( stdout, "%02X ", packet[4+i] );
		    if( (i%16)==15 )
		    	fprintf( stdout, "\n" );
		}
		fprintf( stdout, "\n" );
	    }
#endif	    
	    written = fwrite( packet, 1, 192,  outfile );
	    if( written<188 )
	    {
	    	fprintf( stdout, "written=%d\n", written );
		exit( 1 );
	    }
	    (*nPacket)++;
	    nPCR = nPCR + 27000*188/rateKB;
	}
	return nPCR/300;
}

int ParsePes( char *filename, int *Table, int n )
{
int readed;
unsigned char buffer[1024];
unsigned char header[1024];
int head_addr=0;
int l_head_addr=(-1);
int nZero=0;
int total=0;
int state=0;
int header_pos=0;
int nPes=0;
int i;
int pes_len=(-1);
int pes_pos=(-1);

	g_addr = 0;
	FILE *infile = fopen( filename, "rb" );
	if( infile==NULL )
	{
	    fprintf( stdout, "Can't open [%s]\n", filename );
	    exit( 1 );
	}
	while( 1 )
	{
	    readed = gread( buffer, 1, 1024, infile );
	    if( readed<1 )
	    {
//	    	fprintf( stdout, "EOF(0x%X)\n", total );
		break;
	    }
#if 0
	    if( bDisplay==0 )
	    {
	    	if( (g_addr-readed)%0x100000==0 )
		    fprintf( stdout, "%8llX\n", g_addr-readed );
	    }
#endif
	    total+=readed;
	    for( i=0; i<readed; i++ )
	    {
	    	switch( state )
		{
		case 0 :
		    nZero=0;
		    if( buffer[i]==0x00 )
		    {
		    	state++;
			nZero++;
		    } else {
		    }
		    break;
		case 1 :	// 00
		    if( buffer[i]==0x00 )
		    {	// 00 00
		    	state++;
			nZero++;
		    } else {	// 00 XX
			state=0;
		    }
		    break;
		case 2 :	// 00 00
		    if( buffer[i]==0x01 )
		    {	// 00 00 .. 01
		    	state++;
		    } else if( buffer[i]==0x00 )
		    {	// 00 00 00
			nZero++;
		    } else {	// 00 00 .. XX
			state=0;
		    }
		    break;
		case 3 :	// 00 00 01
#if 0
		    if( (buffer[i]==0xC0)
		     || (buffer[i]>=0xE0) )
#else
		    if( (buffer[i]>=0xC0)
		     || (buffer[i]==0xBD) )
#endif
		    {
			memset( header, 0, 1024 );
			head_addr = g_addr-readed+i-3;
			header[0] = 0;
			header[1] = 0;
			header[2] = 1;
			header[3] = buffer[i];
			header_pos=4;
//			fprintf( stdout, "00 00 01 %02X\n", buffer[i] );
		    	state++;
			pes_len = (-1);
			pes_pos = 0;
		    } else {
			state=0;
		    }
		    break;
		// -----------------------
		case 4 :	// 00 00 01 EX
		    pes_pos++;
		    if( header_pos<1024 )
		    {
			header[header_pos++] = buffer[i];
		    }
		    if( header_pos>=9 )
		    {
			int len = header[8];
			pes_len = (header[4]<<8)
			        | (header[5]<<0);
			if( header_pos>(len+8) )
			{
			    state = 5;
			    unsigned int PTSH, PTSL, DTSH, DTSL;
			    unsigned int PTS=0xFFFFFFFF;
			    unsigned int DTS=0xFFFFFFFF;
			    PTSL=0xFFFFFFFF;
			    {
			    	int offset;
				int pes_len = parsePES_header(
				    &header[4], 0,
				    &PTSH, &PTSL, &DTSH, &DTSL, &offset );
				if( PTSL!=0xFFFFFFFF )
				{
				    PTS=PTSL;
				    DTS=DTSL;
				    if( DTS==0xFFFFFFFF )
					DTS=PTS;
				    int dh, dm, ds, dms;
				    int ph, pm, ps, pms;
				    TcTime32( 0, DTS, &dh, &dm, &ds, &dms );
				    TcTime32( 0, PTS, &ph, &pm, &ps, &pms );
				    if( bDisplay )
				    fprintf( stdout, 
		"addr=%8X : DTS=%2d:%02d:%02d.%03d PTS=%2d:%02d:%02d.%03d",
		    head_addr, dh, dm, ds, dms, ph, pm, ps, pms );
				} else {
				    if( bDisplay )
				    fprintf( stdout, 
		"addr=%8X : DTS=--:--:--.--- PTS=--:--:--.---",
					head_addr );
				}
				if( bDisplay )
				{
				    fprintf( stdout, " : "  );
				    if( l_head_addr<0 )
				    {
					fprintf( stdout, "L=%6d,   -  :", 
					    pes_len );
				    } else {
					fprintf( stdout, "L=%6d,%6d:" ,
					    pes_len, head_addr-l_head_addr );
				    }
				    fprintf( stdout, "\n" );
				}
				Table[nPes*TABLE_UNIT+0] = DTS;
				Table[nPes*TABLE_UNIT+1] = PTS;
				Table[nPes*TABLE_UNIT+2] = head_addr;
				if( l_head_addr>=0 )
				    Table[(nPes-1)*TABLE_UNIT+3] 
				    	= head_addr-l_head_addr;
				Table[nPes*TABLE_UNIT+7] = n;
				nPes++;
			    }
			    l_head_addr = head_addr;
			// -------------------------------
			}
		    }
		    break;
		case 5 :
		    if( pes_len>pes_pos )
		    {
		    	pes_pos++;
		    } else {
		    	state = 0;
		    }
		    break;
		}
	    }
	}
	if( nPes>0 )
	    Table[(nPes-1)*TABLE_UNIT+3] = g_addr-l_head_addr;
	fprintf( stdout, "nPes=%d\n", nPes );

	fclose( infile );

	return nPes;
}

int CalcPCR( int Table[], int nPes, int rate )
{
int n;
int BAD=0;
	    for( n=nPes-1; n>=0; n-- )
	    {
	    	int DTS;
		unsigned int PTS, sz;
		int packet;
		DTS = Table[n*TABLE_UNIT+0];
		PTS = Table[n*TABLE_UNIT+1];
		sz  = Table[n*TABLE_UNIT+3];
		int packUnit=(188-4);
		packet = (sz+packUnit-1)/packUnit;
		int ms = packet*188/rate;
		int space = (ms+3)/4;	// for PCR and audio : every 4ms
		Table[n*TABLE_UNIT+6] = space;
		packet += space;
		int next=n+1;
#if 1
		if( next<(nPes-1) )
		{
		    if( DTS>Table[next*TABLE_UNIT+4] )
		    {
		    	DTS = Table[next*TABLE_UNIT+4]
			    - 1*188*90/rate;
//			fprintf( stdout, "Update DTS=%d\n", DTS );
#if 0
			if( DTS<0 )
			{
			    fprintf( stdout, "DTS[%d]=%d\n", n, DTS );
			    BAD++;
			}
#endif
		    }
		}
#endif
		Table[n*TABLE_UNIT+5] = DTS;
		Table[n*TABLE_UNIT+4] = DTS-packet*188*90/rate;
	    }
	    return BAD;
}

/*
Table[][0] DTS
Table[][1] PTS
Table[][2] pos
Table[][3] size
Table[][4] DTSnext-transferTime
Table[][5] DTSnext
*/
int AddM( int MTable[], int Msize, int Table[], int index, int f, int bExpand )
{
int i, m, n;
	for( n=0; n<Msize; n++ )
	{
	    if( MTable[TABLE_UNIT*n+4]==0 )
	    {
#ifdef DEBUG
		fprintf( stdout, "Set(%d:%d)->%d\n", f, index, n );
#endif
		break;
	    }
	    if( Table[TABLE_UNIT*index+4]<MTable[TABLE_UNIT*n+4] )
	    {
		for( m=Msize; m>n; m-- )
		{
		    for( i=0; i<8; i++ )
		    {
			MTable[TABLE_UNIT*m+i] = MTable[TABLE_UNIT*(m-1)+i];
		    }
		}
#ifdef DEBUG
		fprintf( stdout, "Ins(%d:%d)->%d\n", f, index, n );
#endif
		break;
	    }
	}
	if( n==Msize )
	{
	    if( bExpand )
	    {
#ifdef DEBUG
	    fprintf( stdout, "Set(%d:%d)->%d\n", f, index, n );
#endif
	    } else {
	    	return Msize;	// Not expand
	    }
	}
	for( i=0; i<7; i++ )
	{
	    MTable[TABLE_UNIT*n+i] = Table[TABLE_UNIT*index+i];
	}
	MTable[TABLE_UNIT*n+7] =  f;
	return Msize+1;
}

int MergePes( int *Table[], int nPes[], int nFiles, int rateKB,
	      int MTable[], int bExpand )
{
int Msize=0;
int f, n;
	fprintf( stdout, "MergePes() start : nFiles=%d, rateKB=%d\n", 
		nFiles, rateKB );
	for( f=0; f<nFiles; f++ )
	{
	    int bExp = (f==0) ? 1 : 0;
	    if( bExpand )
	    	bExp = 1;
	    for( n=0; n<nPes[f]; n++ )
	    {
	    	Msize = AddM( MTable, Msize, Table[f], n, f, bExp );
	    }
	}
	return Msize;
}

//int DumpTable( int *Table[], int nfile, int n, int diff )
int MakeTts( int *Table[], int nPes[], char *filename[MAX_FILE], int nFiles,
	char *outFilename, int offset, int rateKB, int bAddAudio )
{
FILE *infile[MAX_FILE];
FILE *outfile=NULL;
int n;

	    for( n=0; n<MAX_FILE; n++ )
	    {
	    	infile[n] = NULL;
	    }
	    for( n=0; n<nFiles; n++ )
	    {
		infile[n] = fopen( filename[n], "rb" );
		if( infile[n]==NULL )
		{
		    fprintf( stdout, "Can't open %d:%s\n", n, filename[n] );
		    exit( 1 );
		}
	    }
	    outfile = fopen( outFilename, "wb" );
	    if( outfile==NULL )
	    {
	    	fprintf( stdout, "Can't open %s\n", outFilename );
		exit( 1 );
	    }
	    int nPacket[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	    int bPCR=1;
	    int bPAT=1;
	    int nA=0;
	    int next=0;
	    for( n=0; n<nPes[0]; n++ )
	    {
	    	long long PCR;
		unsigned int DTS, PTS, off, sz;
		DTS = Table[0][n*TABLE_UNIT+0];
		PTS = Table[0][n*TABLE_UNIT+1];
		off = Table[0][n*TABLE_UNIT+2];
		sz  = Table[0][n*TABLE_UNIT+3];
		PCR = Table[0][n*TABLE_UNIT+4]-offset;
		long long nextPCR;
		if( bAddAudio )
		{
		    // Audio PES
		    if( nFiles>1 )
		    {
			while( nA<nPes[1] )
			{
			    int DTS_A = Table[1][nA*TABLE_UNIT+0];
			    int offA  = Table[1][nA*TABLE_UNIT+2];
			    int szA   = Table[1][nA*TABLE_UNIT+3];
			    int PCR_A = Table[1][nA*TABLE_UNIT+4]-offset;
			    if( PCR_A<PCR )
			    {
				nextPCR = PCR_A;
				if( PCR_A<next )
				{
				    nextPCR=next;
			fprintf( stdout, "PCR rewind(%8X->%8X)\n", next, PCR_A);
				}
				if( bDebug )
				fprintf( stdout, 
				    "%16llX : Audio%5d(%8X,%8X)\n", 
				    nextPCR, nA, PCR_A, DTS_A );
				next=Pes2Tts( infile[1], offA, szA, nextPCR,
					&nPacket[1], 
					NULL, 0, 
					outfile, 0x201, rateKB, 0, 0 );
				nA++;
			    } else {
				break;
			    }
			}
		    }
		}
		nextPCR = PCR;
		if( PCR<next )
		{
		    nextPCR = next;
		    fprintf( stdout, "PCR rewind(%8X->%8llX)\n", next, PCR );
		}
		if( bDebug )
		fprintf( stdout, 
		    "%16llX : Video%5d(%8llX,%8X)\n", 
		    	nextPCR, n, PCR, DTS );
		next=Pes2Tts( infile[0], off, sz, nextPCR, &nPacket[0], 
			NULL, 0, 
			outfile, 0x200, rateKB, bPCR, bPAT );
		bPAT=0;
	    }
	    for( n=0; n<nFiles; n++ )
	    {
		if( infile[n] )
		    fclose( infile[n] );
	    }
	    if( outfile )
		fclose( outfile );

	    return 0;
}

int MakeMergeTts( int Table[], int nPes, int Pid[],
	int AudioConfig[], int nConfig,
	char *filename[MAX_FILE], int nFiles,
	char *outFilename, int offset, int rateKB )
{
FILE *infile[MAX_FILE];
FILE *outfile=NULL;
int n;
int nPacket[MAX_FILE];
int nCount=0;

	if( nConfig>0 )
	{
	    for( n=0; n<nConfig; n+=3 )
	    {
	    	Pid[n/3] = AudioConfig[n+0];
	    }
	}

	for( n=0; n<MAX_FILE; n++ )
	{
	    infile[n] = NULL;
	    nPacket[n] = 0;
	}
	for( n=0; n<nFiles; n++ )
	{
	    infile[n] = fopen( filename[n], "rb" );
	    if( infile[n]==NULL )
	    {
		fprintf( stdout, "Can't open %d:%s\n", n, filename[n] );
		exit( 1 );
	    }
	}
	outfile = fopen( outFilename, "wb" );
	if( outfile==NULL )
	{
	    fprintf( stdout, "Can't open %s\n", outFilename );
	    exit( 1 );
	}
	int next=0;
	int L_PCR=(-1);
	int bPAT=1;
	long long L_PATPCR= Table[n*TABLE_UNIT+4]-offset;
	for( n=0; n<nPes; n++ )
	{
	    long long PCR;
	    unsigned int DTS, PTS, off, sz, fd;
	    DTS = Table[n*TABLE_UNIT+0];
	    PTS = Table[n*TABLE_UNIT+1];
	    off = Table[n*TABLE_UNIT+2];
	    sz  = Table[n*TABLE_UNIT+3];
	    PCR = Table[n*TABLE_UNIT+4]-offset;
	    fd  = Table[n*TABLE_UNIT+7];
	    long long nextPCR = PCR;
	    if( PCR<next )
	    {
		nextPCR = next;
		fprintf( stdout, "PCR rewind(%8X->%8llX)\n", next, PCR );
	    }
	    if( bDebug )
	    fprintf( stdout, 
		"%16llX : Video%5d(%8llX,%8X)\n", 
		    nextPCR, n, PCR, DTS );
	    int bPCR=0;
	    if( (L_PCR<0) || ((PCR-L_PCR)>8000) )	// 100ms<
//		if( fd==0 )
		bPCR = 1;
	    if( (nextPCR-L_PATPCR)>=90000 )
		bPAT = 1;
	    next=Pes2Tts( infile[fd], off, sz, nextPCR, &nPacket[fd], 
		    AudioConfig, nConfig,
		    outfile, Pid[fd], rateKB, bPCR, bPAT );
	    if( bPAT )
		L_PATPCR = nextPCR;
	    if( bPAT )
	    {
		switch( nCount % 3 )
		{
		case 0 :
		    next = XXX2Tts( NIT_DataA, next,
			    &nPacket[0x10], outfile, 0x10, rateKB );
		    break;
		case 1 :
		    next = XXX2Tts( NIT_DataB, next,
			    &nPacket[0x10], outfile, 0x10, rateKB );
		    break;
		case 2 :
		    next = XXX2Tts( SDT_Data, next,
			    &nPacket[0x11], outfile, 0x11, rateKB );
		    break;
		}
		nCount++;
	    }
	    bPAT = 0;
	    if( bPCR )
		L_PCR = PCR;
	    bPAT=0;
	}
	for( n=0; n<nFiles; n++ )
	{
	    if( infile[n] )
		fclose( infile[n] );
	}
	if( outfile )
	    fclose( outfile );

	return 0;
}



// ------------------------------------------------------

int pes2tts( int argc, char *argv[] )
{
int i, j;
int args=0;
int nFiles=0;
char *filename[MAX_FILE];
char outFilename[1024];

int *Table[MAX_FILE];
int rateKB=0;
int offset=0;
int bExpand=0;
int Pid[MAX_FILE];
int AudioConfig[16*3];
int nConfig=0;
int Error=0;
	for( i=0; i<16*3; i++ )
	{
	    AudioConfig[i] = (-1);
	}
	for( i=0; i<MAX_FILE; i++ )
	{
	    Pid[i] = 0x200+i;
	}
	sprintf( outFilename, "out.mts" );

	for( i=0; i<MAX_FILE; i++ )
	{
	    filename[i] = NULL;
	}
	for( i=1; i<argc; i++ )
	{
	    fprintf( stdout, "arg[%d]=%s\n", i, argv[i] );
	    if( argv[i][0]=='-' )
	    {
		switch( argv[i][1] )
		{
		case 'R' :
		    rateKB = atoi( &argv[i][2] );
		    break;
		case 'D' :
		    bDebug = 1;
		    break;
		case 'O' :
		    sprintf( outFilename, "%s", &argv[i][2] );
		    break;
		default :
		    pes2tts_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='+' )
	    {
		switch( argv[i][1] )
		{
		case 'E' :
		    bExpand=1;	// use all pes
		    break;
		case 'D' :
		    bDisplay=1;
		    break;
		case 'M' :
		    MaxDiff = atoi( &argv[i][2] );
		    if( MaxDiff<1 )
		    	MaxDiff= 45000;	// 0.5sec
		    break;
		case 'O' :
		    offset = atoi( &argv[i][2] );
		    break;
		case 'A' :
		    AudioConfig[nConfig++] = GetValue( &argv[i][2], &Error );
		    break;
		default :
		    pes2tts_Usage( argv[0] );
		    break;
		}
	    } else if( argv[i][0]=='=' )
	    {
		switch( argv[i][1] )
		{
		case 'A' :
		    AudioCodec = GetValue( &argv[i][2], &Error );
		    break;
		case 'P' :
		    ProgramId = GetValue( &argv[i][2], &Error );
		    break;
		case 'T' :
		    TransportStreamId = GetValue( &argv[i][2], &Error );
		    break;
		default :
		    pes2tts_Usage( argv[0] );
		    break;
		}
	    } else {
		switch( args )
		{
		case 0 :
		case 1 :
		case 2 :
		case 3 :
		case 4 :
		case 5 :
		case 6 :
		case 7 :
		case 8 :
		case 9 :
		    filename[nFiles++] = argv[i];
		    fprintf( stdout, "File%d=%s\n", args, filename[args] );
		    fflush( stdout );
		    break;
		default :
		    pes2tts_Usage( argv[0] );
		    break;
		}
		args++;
	    }
	}
	if( filename[0]==NULL )
	{
	    pes2tts_Usage( argv[0] );
	}
	// -------------------------------------------
	// initialize Table
	int n;
	for( n=0; n<nFiles; n++ )
	{
	    Table[n] = (int *)malloc( 4*TABLE_UNIT*1024*1024 );
	    if( Table[n]==NULL )
	    {
		fprintf( stdout, "Can't alloc Table[%d]\n", n );
		exit( 1 );
	    }
	    for( i=0; i<(1024*1024); i++ )
	    {
		for( j=0; j<TABLE_UNIT; j++ )
		{
		    Table[n][i*TABLE_UNIT+j] = 0;
		}
	    }
	}
	// -------------------------------------------
	// Parse PES 
	int nPes[MAX_FILE];
	for( n=0; n<nFiles; n++ )
	{
	    nPes[n] = ParsePes( filename[n], Table[n], n );
	    fprintf( stdout, "%s : %d pes\n", filename[n], nPes[n] );
	}

	// -------------------------------------------
	// Analyze bitrate
#define MAX_RT 601
	unsigned int minRT[MAX_RT];
	unsigned int maxRT[MAX_RT];
	unsigned int delta[MAX_RT];
	int sp=0;
	static int spn[] = { 15, 30, 59, 60, 120, 300, 600, -1 };
	for( n=0; n<MAX_RT; n++ )
	{
	    minRT[n] = 0x7FFFFFFF;
	    maxRT[n] = 0;
	    delta[n] = 0;
	}
	for( n=0; n<nPes[0]; n++ )
	{
	    int sz;
	    if( bDisplay )
	    fprintf( stdout, "%5d : %8X %8X %8X %8X\n",
		n, Table[0][n*TABLE_UNIT+0], Table[0][n*TABLE_UNIT+1], 
		Table[0][n*TABLE_UNIT+2], Table[0][n*TABLE_UNIT+3] );
	    for( sp=0; spn[sp]>0; sp++ )
	    {
#if 0
		fprintf( stdout, "sp=%2d : spn[sp]=%4d\n",
			sp, spn[sp] );
#endif
		if( (n+spn[sp])<nPes[0] )
		{
		    delta[spn[sp]] = Table[0][(n+spn[sp])*TABLE_UNIT+0]
				    -Table[0][(n        )*TABLE_UNIT+0];
		    sz = Table[0][(n+spn[sp])*TABLE_UNIT+2]
		       -Table[0][n*TABLE_UNIT+2];
		    if( sz<minRT[spn[sp]] )
			minRT[spn[sp]] = sz;
		    if( sz>maxRT[spn[sp]] )
			maxRT[spn[sp]] = sz;
		}
	    }
	}
	fprintf( stdout, "delta[spn[sp]] calculated\n" );
	fflush( stdout );
	for( sp=0; spn[sp]>0; sp++ )
	{
	    int rate=(-1);
	    long long maxDataSize = maxRT[spn[sp]]/1024;
	    maxDataSize *= 90000;

	    fprintf( stdout, "%3d : delta=%8d, min=%10d, max=%10d ",
	    	spn[sp],
	    	delta[spn[sp]],
	    	minRT[spn[sp]],
	    	maxRT[spn[sp]] );
	    fflush( stdout );
	    if( delta[spn[sp]]>0 )
	    {
	    	rate = maxDataSize/delta[spn[sp]];
		fprintf( stdout, ": %5d KB/s\n", rate );
	    } else {
	    	fprintf( stdout, "\n" );
	    }
	}
	// -------------------------------------------
	// Calcurate PCR
	// insert PCR packet every 100ms
	// audio may be 300Kbps
	// may have space every 4ms
	if( delta[spn[0]] < 1 )
	{
	    fprintf( stdout, "Can't calculate valid rate\n" );
	    exit( 1 );
	}
	int rate = maxRT[spn[0]]/1024*90000/delta[spn[0]];
	fprintf( stdout, "rate@spn[0]=%d\n", rate );
#if 0
	rate = (rate/100+1)*100;
#else
//	rate = (rate/100+3)*100;
	rate = (rate/100+10)*100;
#endif
	int rateOK=0;
	for( ; ; )
	{
	    fprintf( stdout, "rate=%4d\n", rate );
	    if( CalcPCR( Table[0], nPes[0], rate )!=0 )
	    	break;
// Table[0]=DTS
// Table[1]=PTS
// Table[2]=head_addr
// Table[3]=head_addr-l_head_addr
// Table[4]=DTS'-packet*188*90/rate
// Table[5]=DTS'
// Table[6]=space
	    int maxDiff=0;
	    int nfile=0;
	    for( n=0; n<nPes[nfile]; n++ )
	    {
		int diff = Table[nfile][n*TABLE_UNIT+0]
			  -Table[nfile][n*TABLE_UNIT+5] ;
#if 0
		DumpTable( Table, nfile, n, diff );
#endif
		if( diff>maxDiff )
		{
		    if( diff>180000 )
		    {
			DumpTable( Table, nfile, n+0, diff );
			DumpTable( Table, nfile, n+1, 0 );
		    }
		    fprintf( stdout, "n=%d : maxDiff(%d->%d)\n", 
			n, maxDiff, diff );
		    maxDiff = diff;
		}
	    }
	    fprintf( stdout, "maxDiff=%d\n", maxDiff );
	    if( maxDiff < MaxDiff )
		rateOK = rate;
	    else
	    	break;
	    if( maxDiff>180000 )	// 2sec
		break;
	    int rateD = (rate/100-1);
	    rate = rateD*100;
	}
	if( rateKB==0 )
	    rateKB = rateOK;
	fprintf( stdout, "rateKB=%d\n", rateKB );
	fflush( stdout );
// -----------------------------------------------------------------------
	if( rateKB>0 )
	{
	    // Calculate PCR with rateKB
	    int f;
	    for( f=0; f<nFiles; f++ )
	    {
		for( n=nPes[f]-1; n>=0; n-- )
		{
		    CalcPCR( Table[f], nPes[f], rateKB );
		}
	    }
	    int maxDiff=0;
	    for( f=0; f<nFiles; f++ )
	    {
	    	fprintf( stdout, "file=%s, nPES=%d\n", filename[f], nPes[f] );
		fprintf( stdout, 
		    " nPES :    DTS      PTS       addr      sz  :  PCR\n" );
		for( n=0; n<nPes[f]; n++ )
		{
		    int diff = Table[f][n*TABLE_UNIT+0]+offset
			      -Table[f][n*TABLE_UNIT+5] ;
		    DumpTable( Table, f, n, diff );
		    if( diff>maxDiff )
			maxDiff = diff;
		}
	    }
	    int bAddAudio=1;
	    if( nFiles<3 )
	    MakeTts( Table, nPes, filename, nFiles, 
	    	outFilename, offset, rateKB, bAddAudio );
	} else {
	    fprintf( stdout, "Invalid rateKB(%d)\n", rateKB );
	}
	fprintf( stdout, "%s generated\n", outFilename );

	int *MTable = (int *)malloc( 4*TABLE_UNIT*1024*1024 );
	if( MTable==NULL )
	{
	    fprintf( stdout, "Can't malloc\n" );
	    exit( 1 );
	}
	int Msize = MergePes( Table, nPes, nFiles, rateKB, MTable, bExpand );
	for( n=0; n<Msize; n++ )
	{
		fprintf( stdout, "%d: %8d,%8d,%8d\n", 
			MTable[n*TABLE_UNIT+7],	// f
			MTable[n*TABLE_UNIT+0],	// DTS
			MTable[n*TABLE_UNIT+1],	// PTS
			MTable[n*TABLE_UNIT+4] 	// DTSnex
			);
	}
	int c = CalcPCR( MTable, Msize, rateKB );
	fprintf( stdout, "CalcPCR()=%d\n", c );
	for( n=0; n<Msize; n++ )
	{
		fprintf( stdout, "%d: %8d,%8d,%8d\n", 
			MTable[n*TABLE_UNIT+7],	// f
			MTable[n*TABLE_UNIT+0],	// DTS
			MTable[n*TABLE_UNIT+1],	// PTS
			MTable[n*TABLE_UNIT+4] 	// DTSnex
			);
	}
	char ttsFilename[1024];
	sprintf( ttsFilename, "outM.mts" );
	MakeMergeTts( MTable, Msize, Pid,
		AudioConfig, nConfig,
		filename, nFiles, ttsFilename, offset, rateKB );
	free( MTable );


	return 0;
}

#ifndef MAIN
int bDebug=0;
int main( int argc, char *argv[] )
{
	return pes2tts( argc, argv );
}
#endif

