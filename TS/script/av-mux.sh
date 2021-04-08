#

rm Video.pes
rm Audio.pes

function MakeAllegroTS
{
#VIDEO_TS=Allegro_Inter_CABAC_10_L41_HD_10.2__2011-09-13.ts
#VIDEO_PID=0080
VIDEO_TS=$1
VIDEO_PID=$2

BASENAME=`basename ${VIDEO_TS} .ts`
echo ${BASENAME}

#AUDIO_TS=V+A__h264_smple_2k_1920x1080_profile_high_ac3_48000_5ch_dd_29fps_acmod_7.ts
#AUDIO_PID=0201
AUDIO_TS=$3
AUDIO_PID=$4
AUDIO=$5

case ${AUDIO} in
0 ) AUDIO_EXT=MPEG1-AUDIO;;
1 ) AUDIO_EXT=ac3_48000_5ch_dd;;
2 ) AUDIO_EXT=MPEG2-AUDIO;;
3 ) AUDIO_EXT=HEAAC;;
4 ) AUDIO_EXT=MPEG2-AAC;;
esac

rm PES-*

ts -r ${VIDEO_TS}
echo "pesEdit PES-${VIDEO_PID}.bin Video.pes +Allegro +O90000"
pesEdit PES-${VIDEO_PID}.bin Video.pes +Allegro +O90000
if [ $? -gt 0 ]
then
echo "Error"
exit
fi

if [ -f Audio.pes ]
then
echo "Audio.pes already exist"
else

ts -r ${AUDIO_TS}

echo "pesEdit PES-${AUDIO_PID}.bin Audio.pes +Allegro +O90000"
pesEdit PES-${AUDIO_PID}.bin Audio.pes +Allegro +O90000
#pesEdit PES-${AUDIO_PID}.bin Audio.pes +Allegro +O94165
#mv PES-${AUDIO_PID}.bin Audio.pes

if [ $? -gt 0 ]
then
echo "Error"
exit
fi
fi

#pes2tts Video.pes Audio.pes +M60000 =A${AUDIO}
pes2tts Video.pes Audio.pes +M45000 =A${AUDIO}
#pes2tts Video.pes Audio.pes +M30000 =A${AUDIO}
if [ $? -gt 0 ]
then
echo "Error"
exit
fi


OUTFILE_TTS=${BASENAME}-${AUDIO_EXT}.m2ts
OUTFILE_TS=${BASENAME}-${AUDIO_EXT}.ts

#rm Video.pes
#rm Audio.pes

TTS_OUT=out.mts
TTS_OUT=outM.mts

tts2ts ${TTS_OUT} ${OUTFILE_TS}
if [ $? -gt 0 ]
then
echo "Error"
exit
fi
mv ${TTS_OUT} ${OUTFILE_TTS}

echo "========================================================================"
echo "${OUTFILE_TS} is made"
echo "${OUTFILE_TTS} is made"
echo "========================================================================"
}

VIDEO_PID=0080
AUDIO_TS=V+A__h264_smple_2k_1920x1080_profile_high_ac3_48000_5ch_dd_29fps_acmod_7.ts
AUDIO_PID=0201
AUDIO=1

if [ x$2 != x ]
then
    VIDEO_PID=$2
fi
if [ x$3 != x ]
then
    AUDIO_TS=$3
fi
if [ x$4 != x ]
then
    AUDIO_PID=$4
fi
if [ x$5 != x ]
then
    AUDIO=$5
fi
if [ x$1 != x ]
then
VIDEO_TS=$1
echo ${VIDEO_TS}
MakeAllegroTS ${VIDEO_TS} ${VIDEO_PID} ${AUDIO_TS} ${AUDIO_PID} ${AUDIO}

else

for VIDEO_TS in /newhdd_1/streams/from_testdisc/allegro/TS/L4.1/*/*/*.ts
do
echo ${VIDEO_TS}
MakeAllegroTS ${VIDEO_TS} ${VIDEO_PID} ${AUDIO_TS} ${AUDIO_PID} ${AUDIO}
done

for VIDEO_TS in /newhdd_1/streams/from_testdisc/allegro/TS/50p_60p/*/*/*.ts
do
echo ${VIDEO_TS}
MakeAllegroTS ${VIDEO_TS} ${VIDEO_PID} ${AUDIO_TS} ${AUDIO_PID} ${AUDIO}
done
fi

