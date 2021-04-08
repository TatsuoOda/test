/*
	spesLib.h
			2012.10.12  by T.Oda

			2012.10.12 separate from movParse.cpp
 */

extern int bExifReplace;

int SPS( FILE *out, unsigned char *buf, int size, int frame_rate );
int AVC( FILE *out, FILE *fp, int size, int DTS, int PTS );
int MPEG4H( FILE *out, unsigned char *buf, int size );
int MPEG4( FILE *out, FILE *fp, int size, int DTS, int PTS );
int MJPEGH( FILE *out );
int MJPEG( FILE *out, FILE *fp, int size, int DTS, int PTS );

