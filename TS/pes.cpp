
/*
	pes.cpp
		2011.9.5 PTS/DTS output

		2011.9.23 PTS/DTS update : fromPts,toPts, fromDts,toDts
		2011.9.29 pic_timing : debugged
		2011.9.29 00 00 03 count : debugged
		2011.10.13 Audio PTS (00 00 01 C0)
		2011.10.29 MODE_REMUX (+Remux)
		2011.10.31 Change PES-XXXX.txt format
			TS-addr(top), TS-addr(PES), PES-addr, PES-size, PCR
		2011.11.17 use_data support by Akagawa
		2011.11.24 Remux : append rest of data at end of file
		2011.11.30 SeekHeader() :  bug fixed
		2011.12.13 Show SPS information
		2012. 4.27  MPEG : Video E0-EF
		2012. 5. 2 =p : remove pic_struct
		           =t : remove timing_info
		2012. 5. 8 =f : edit FrameMbsOnlyFlag
		2012. 5.13 -H -dXX +dYY : cut pes
		2012. 6.14 XXX.pts address fix
		2012. 6.26 bSkipError -> skip Invalid Prefix
 */

int bDebug=0;

int pes( int argc, char *argv[] );

int main( int argc, char *argv[] )
{
	return pes( argc, argv );
}

