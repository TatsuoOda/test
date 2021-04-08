#
# edit.sh
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

for i in \
 AVC-1904x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1888x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1872x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1856x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1840x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1904x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1888x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1872x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1856x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1840x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1904x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1888x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1872x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1856x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1840x1080-30I-B16pR4-H41-30Mbps-90sec.pes \
 AVC-1920x1080-30I-B16pR4-H41-30Mbps-90sec.pes \

do
NEXT=`expr ${FROM} + ${DURATION}`
TO=`expr ${NEXT} - 1`
echo "$i  : ${FROM}-${TO}"

FROM_STR=`printf "%08d" ${FROM}`
TO_STR=`printf "%08d" ${TO}`
#echo "pesEdit $i -F${FROM} -T${TO} out-${FROM_STR}.pes"
EXEC "pesEdit $i -F${FROM} -T${TO} -d6000 out-${FROM_STR}.pes"

FROM=${NEXT}

done

