#
# pes.sh 
#

if [ x$2 != x ]
then

FILE=`echo $2 | sed -e "s/^.\{3\}//"`
echo ${FILE}
FILE=`echo $FILE | sed -e "s/.\{3\}$//"`
echo ${FILE}
rm PES*.*
ts -w ../${FILE}.ts

PID=`du -s *.bin | grep -v PES-1FFF | sort -n -r | head -1 | sed 's/.*PES-\([0-Z]*\).*/\1/' `
echo "Video PID=${PID}"
echo "mv PES-${PID}.bin ${FILE}.pes"
mv PES-${PID}.bin ${FILE}.pes

else

for i in ../*.*ts
do
FILE=`echo $i | sed -e "s/^.\{3\}//"`
echo "$i -> ${FILE}"
FILE=`echo $FILE | sed -e "s/.\{3\}$//"`

basename=${i##*/}
filename=${basename%.*}
extension=${basename##*.}

echo "---------------------------------------"
#echo "FILE=${FILE}"
echo filename=${filename}
echo extension=${extension}
FILE=${filename}
EXT=${extension}
echo "---------------------------------------"
rm PES*.*
ts -w ../${FILE}.${EXT}
VIDEO0_BIN=`ls -l *.bin | grep -v PES-1FFF | sort -n -t 4 -r | head -2 | head -1 | awk '{print $9}'`
VIDEO1_BIN=`ls -l *.bin | grep -v PES-1FFF | sort -n -t 4 -r | head -2 | tail -1 | awk '{print $9}'`
echo "Video=${VIDEO0_BIN}"
if [ x$1 = xMVC ]
then
echo "mv ${VIDEO0_BIN} ${FILE}.bes"
echo "mv ${VIDEO1_BIN} ${FILE}.nes"
mv ${VIDEO0_BIN} ${FILE}.bes
mv ${VIDEO1_BIN} ${FILE}.nes
else
echo "mv ${VIDEO0_BIN} ${FILE}.pes"
mv ${VIDEO0_BIN} ${FILE}.pes
fi
rm PES*.bin
rm PES*.txt
done
fi
