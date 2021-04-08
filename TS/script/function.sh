#
# function.sh
#	2011.10.27  by T.Oda
#
# ----------------------------------------
# CheckError [-n] command
# ----------------------------------------

if [ x${BASH} = x ]
then
echo "Exec with BASH"
exit 1
fi

TS=~/src/TS/ts
PES=~/src/TS/pes
CAL=~/src/TS/cal


function CheckError
{
LOG=0
if [ $1 = "-n" ]
then
shift
echo "$*"
$*
else
echo "$*"
$* > /tmp/result
fi
result=$?
if [ ${result} -ne 0 ]
then
echo "Error : exit"
exit 1
fi
#echo -n "Done : Enter "
#read n
echo "Done"
}

function GetPts
{
FILE=$1
LINE=$2
PTS_H=`tail -n -${LINE} ${FILE} | head -n 1 | awk '{print $4}'`
PTS_L=`tail -n -${LINE} ${FILE} | head -n 1 | awk '{print $5}' | cut -b-8`
PTS=${PTS_H}${PTS_L}
}

function GetLastPts 
{
GetPts $1 6
PTS6=${PTS}
GetPts $1 5
PTS5=${PTS}
GetPts $1 4
PTS4=${PTS}
GetPts $1 3
PTS3=${PTS}
GetPts $1 2
PTS2=${PTS}
GetPts $1 1
PTS1=${PTS}
PTSD=`${CAL} 0x${PTS6}`
PTSC=`${CAL} 0x${PTS5}`
if [ ${PTSC} -gt ${PTSD} ]
then
PTSD=${PTSC}
fi
PTSC=`${CAL} 0x${PTS4}`
if [ ${PTSC} -gt ${PTSD} ]
then
PTSD=${PTSC}
fi
PTSC=`${CAL} 0x${PTS3}`
if [ ${PTSC} -gt ${PTSD} ]
then
PTSD=${PTSC}
fi
PTSC=`${CAL} 0x${PTS2}`
if [ ${PTSC} -gt ${PTSD} ]
then
PTSD=${PTSC}
fi
PTSC=`${CAL} 0x${PTS1}`
if [ ${PTSC} -gt ${PTSD} ]
then
PTSD=${PTSC}
fi
echo "${PTSD}"
}

# ----------------------------------------
# Normalize src dst
# ----------------------------------------
function Normalize 
{
echo "Update CC"
CheckError ${TS} -r +Continuous $1 outC.ts

echo "Update PCR"
CheckError ${TS} -W -p0x101 outC.ts outP.ts -e$3
#${TS} -W -p0x101 outC.ts outP.ts -e$3

echo "Remux Video"
CheckError ${TS} -r outP.ts
#+R90000
CheckError ${PES} PES-0200 +R54000
#less /tmp/result
mv outP.ts- outP.ts

echo "Remux Audio"
CheckError ${TS} -r outP.ts
#+R27000
#+R45000
CheckError ${PES} PES-0201 +R27000
CheckError mv outP.ts- $2

#rm outC.ts
#rm outP.ts

}

# ----------------------------------------
# Connect file1 file2 fileO deltaPTS deltaPCR
# ----------------------------------------
function Connect 
{
NORMALIZE=1
if [ x$1 = x-n ]
then
shift
NORMALIZE=0
fi

FILE1=$1
FILE2=$2
FILEO=$3
DELTA=$4
DPCR=$5

if [ x${DPCR} = x ]
then
DPCR=-45000
fi
if [ x${DELTA} = x ]
then
echo "$0 file1 file2 outfile delta ($*)"
exit 1
fi

echo "-------------------------------------"
echo "Connect ${FILE1} and ${FILE2}, output ${FILEO}"
echo "-------------------------------------"

rm PES-*
rm PCR.txt

echo "Analyze 1st TS (${FILE1})"
echo "-------------------------------------"
CheckError ${TS} -r ${FILE1} 

echo "-------------------------------------"
PTS_H=`head -n 1 PES-0200.pts | awk '{print $4}'`
PTS_L=`head -n 1 PES-0200.pts | awk '{print $5}' | cut -b-8`

TOP_PTS1=${PTS_H}${PTS_L}

LAST_PTS1=`GetLastPts PES-0200.pts`

echo "Top PTS=${TOP_PTS1}"
echo "LastPTS=${LAST_PTS1}"

rm PES-*
rm PCR.txt
echo "Analyze 2nd TS (${FILE2})"
echo "-------------------------------------"
CheckError ${TS} -r ${FILE2}

echo "-------------------------------------"
PTS_H=`head -n 1 PES-0200.pts | awk '{print $4}'`
PTS_L=`head -n 1 PES-0200.pts | awk '{print $5}' | cut -b-8`

TOP_PTS2=${PTS_H}${PTS_L}

LAST_PTS2=`GetLastPts PES-0200.pts`

echo "Top PTS=${TOP_PTS2}"
echo "LastPTS=${LAST_PTS2}"

echo "-------------------------------------"
echo "Top PTS=${TOP_PTS1}"
echo "LastPTS=${LAST_PTS1}"
echo "Top PTS=${TOP_PTS2}"
echo "LastPTS=${LAST_PTS2}"

deltaPTS=0

#PTS1=`../../cal 0x${LAST_PTS1}`
PTS1=${LAST_PTS1}
PTS2=`${CAL} 0x${TOP_PTS2}`
PTS3=`${CAL} ${PTS1} - ${PTS2}`
deltaPTS=`${CAL} ${PTS3} + ${DELTA}`

END_PTS=`${CAL} ${LAST_PTS1} + ${LAST_PTS2}`

echo "PTS1=${PTS1}"
echo "PTS2=${PTS2}"
echo "PTS3=${PTS3}"
echo "deltaPTS=${deltaPTS}"

echo "END=${END_PTS}"

#echo -n "OK ? : Enter "
#read n

echo "-------------------------------------"
echo "Modify PTS"
rm PES-*
rm PCR.txt
CheckError cp ${FILE2} edit.ts

echo "Video(PES-0200)"
CheckError ${TS} -r edit.ts
CheckError ${PES} PES-0200 -AVC +D${deltaPTS} +P${deltaPTS}
mv edit.ts- editV.ts

echo "Audio(PES-0201)"
CheckError ${TS} -r editV.ts
CheckError ${PES} PES-0201 +D${deltaPTS} +P${deltaPTS}
mv editV.ts- editAV.ts

echo "-------------------------------------"
echo "Connect ${FILE1} ${FILE2}"
echo "cat ${FILE1} editAV.ts > out.ts"
cat ${FILE1} editAV.ts > out.ts

if [ ${NORMALIZE} -gt 0 ]
then
Normalize out.ts ${FILEO} ${END_PTS}
else
CheckError cp out.ts ${FILEO}
fi

#echo -n "OK ? : Enter "
#read n

rm edit*.ts
rm out.ts
}

