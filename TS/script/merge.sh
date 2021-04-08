#
# merge.sh
#
# 3sec * 30 = 90sec
#  1sec =   90000
#  3sec =  270000
#  5sec =  450000
# 90sec = 8100000

function EXEC {
echo "$1"
eval $1 > /tmp/log.txt
}

declare -i DURATION=270000
#declare -i DURATION=450000
declare -i FROM=0
declare -i NEXT

FILES=

rm all.pes
touch all.pes

# Edit after for i in \
for i in \
AVC-1280x1080-30IT-B3R3-23M.ts \
AVC-1280x720-30IT-B3R3-15M.ts \
AVC-1440x1080-30IT-B3R3-25M.ts \
AVC-1840x1080-30I-B16pR4-H41-30Mbps-90sec.ts \
AVC-1856x1080-30I-B16pR4-H41-30Mbps-90sec.ts
do
NEXT=`expr ${FROM} + ${DURATION}`
TO=`expr ${NEXT} - 1`
echo "$i  : ${FROM}-${TO}"

ts -r $i
mv PES-0200.bin $i.pes

FROM_STR=`printf "%08d" ${FROM}`
TO_STR=`printf "%08d" ${TO}`
OUTPUT=out-${FROM_STR}.pes
EXEC "pesEdit $i.pes -F${FROM} -T${TO} -d6000 ${OUTPUT}"

FILES="${FILES} ${OUTPUT}"
mv all.pes all0.pes
cat all0.pes ${OUTPUT} > all.pes

FROM=${NEXT}
done

pes2tts all.pes
