#
# Makefile
#

PROG = ts pes pesc spes spesMerge pesEdit \
	zero cal hd doremi \
	mjpeg avi mov jpeg wav \
	pes2es tts2ts ts2tts ts-prof yuv2avi ttsEdit \
	ts-index pes2spes es2pes pes-prof dss pes2tts jpeg2spes \
	yuv match crc pat

CFLAGS	= -O2 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CFLAGS	= -g -O0 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CPPFLAGS	= -O2 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

all: ${PROG}

clean:
	rm ${PROG}
	rm -f *.o

# ---------------------------------------------------
#  TS tool
# ---------------------------------------------------
ts : ts.cpp tsLib.cpp parseLib.cpp lib.cpp tsLib.h
	g++ ${CFLAGS} -o ts ts.cpp tsLib.cpp parseLib.cpp lib.cpp

ts-prof : ts.cpp tsLib.cpp parseLib.cpp lib.cpp tsLib.h
	g++ ${CFLAGS} -pg -o ts-prof ts.cpp tsLib.cpp parseLib.cpp lib.cpp

pes : pes.cpp tsLib.cpp parseLib.cpp pesParse.cpp lib.cpp pesAVC.cpp tsLib.h
	g++ ${CFLAGS} -o pes pes.cpp tsLib.cpp parseLib.cpp pesParse.cpp lib.cpp pesAVC.cpp

pes-prof : pes.cpp tsLib.cpp parseLib.cpp pesParse.cpp lib.cpp pesAVC.cpp tsLib.h
	g++ ${CFLAGS} -pg -o pes-prof pes.cpp tsLib.cpp parseLib.cpp pesParse.cpp lib.cpp pesAVC.cpp

dss : dss.cpp tsLib.cpp parseLib.cpp tsLib.h
	g++ ${CFLAGS} -o dss dss.cpp tsLib.cpp parseLib.cpp

pesc : pesc.cpp
	g++ ${CFLAGS} -o pesc pesc.cpp

spes : spes.cpp spesParse.cpp tsLib.cpp parseLib.cpp lib.cpp tsLib.h
	g++ ${CFLAGS} -o spes spes.cpp spesParse.cpp tsLib.cpp parseLib.cpp lib.cpp

ts-index : ts-index.cpp tsLib.cpp parseLib.cpp lib.cpp tsLib.h
	g++ ${CFLAGS} -o ts-index ts-index.cpp tsLib.cpp parseLib.cpp lib.cpp

match : match.cpp
	g++ ${CFLAGS} -o match match.cpp

pat : pat.cpp parseLib.cpp parseLib.h tsLib.cpp tsLib.h
	g++ ${CFLAGS} -o pat pat.cpp parseLib.cpp tsLib.cpp

# ---------------------------------------------------
# Edit tool
# ---------------------------------------------------
spesMerge : spesMerge.cpp 
	g++ ${CFLAGS} -o spesMerge spesMerge.cpp

pesEdit : pesEdit.cpp tsLib.cpp parseLib.cpp tsLib.h
	g++ ${CFLAGS} -o pesEdit pesEdit.cpp tsLib.cpp parseLib.cpp

# ---------------------------------------------------
# misc tool
# ---------------------------------------------------
zero : zero.cpp
	g++ ${CFLAGS} -o zero zero.cpp

hd : hd.cpp
	g++ ${CFLAGS} -o hd hd.cpp

cal : cal.cpp
	g++ ${CFLAGS} -o cal cal.cpp

doremi : doremi.cpp
	g++ ${CFLAGS} -o doremi doremi.cpp

# ---------------------------------------------------
# Container parser
# ---------------------------------------------------
mjpeg : mjpeg.cpp
	g++ ${CFLAGS} -o mjpeg mjpeg.cpp

avi : avi.cpp aviParse.cpp parseLib.cpp spesLib.cpp
	g++ ${CFLAGS} -o avi avi.cpp aviParse.cpp parseLib.cpp spesLib.cpp

mov : mov.cpp movParse.cpp parseLib.cpp spesLib.cpp
	g++ ${CFLAGS} -o mov mov.cpp movParse.cpp parseLib.cpp spesLib.cpp

jpeg : jpeg.cpp jpegParse.cpp parseLib.cpp
	g++ ${CFLAGS} -o jpeg jpeg.cpp jpegParse.cpp parseLib.cpp

wav : wav.cpp
	g++ ${CFLAGS} -lm -o wav wav.cpp

# ---------------------------------------------------
# Convert tool
# ---------------------------------------------------
pes2es : pes2es.cpp tsLib.cpp parseLib.cpp tsLib.h
	g++ ${CFLAGS} -o pes2es pes2es.cpp tsLib.cpp parseLib.cpp

es2pes : es2pes.cpp tsLib.cpp parseLib.cpp tsLib.h
	g++ ${CFLAGS} -o es2pes es2pes.cpp tsLib.cpp parseLib.cpp

pes2tts : pes2tts.cpp tsLib.cpp parseLib.cpp tsLib.h
	g++ ${CFLAGS} -o pes2tts pes2tts.cpp tsLib.cpp parseLib.cpp lib.cpp

pes2spes : pes2spes.cpp tsLib.cpp parseLib.cpp tsLib.h
	g++ ${CFLAGS} -o pes2spes pes2spes.cpp tsLib.cpp parseLib.cpp

jpeg2spes : jpeg2spes.cpp spesLib.cpp
	g++ ${CFLAGS} -o jpeg2spes jpeg2spes.cpp spesLib.cpp

tts2ts : tts2ts.cpp tsLib.cpp parseLib.cpp tsLib.h
	g++ ${CFLAGS} -o tts2ts tts2ts.cpp tsLib.cpp parseLib.cpp

ts2tts : ts2tts.cpp tsLib.cpp parseLib.cpp tsLib.h
	g++ ${CFLAGS} -o ts2tts ts2tts.cpp tsLib.cpp parseLib.cpp

yuv2avi : yuv2avi.cpp 
	g++ ${CFLAGS} -o yuv2avi yuv2avi.cpp 

yuv : yuv.cpp 
	g++ ${CFLAGS} -o yuv yuv.cpp 
	
# ---------------------------------------------------
# Test tool
# ---------------------------------------------------
crc : crc.cpp 
	g++ ${CFLAGS} -o crc crc.cpp 
# ---------------------------------------------------
ttsEdit : ttsEdit.cpp tsLib.cpp parseLib.cpp tsLib.h
	g++ ${CFLAGS} -o ttsEdit ttsEdit.cpp tsLib.cpp parseLib.cpp

