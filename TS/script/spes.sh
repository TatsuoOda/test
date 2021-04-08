#
for i in ../AVC-*.ts
do
echo $i
FILENAME=`echo $i | cut -d"/" -f2`
#FILE_HEAD=`echo ${FILENAME} | cut -d"." -f1`
#echo $i:${FILENAME}:${FILE_HEAD}

ts -r $i
pes2spes PES-0200.bin ${FILENAME}.spes
rm PES-*.*

done

