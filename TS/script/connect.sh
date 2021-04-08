#
# connect $1 $2
#
PID=0200

#
rm PES-*
ts -r $1
pes2es PES-${PID}.bin file1.es
if [ $? -gt 0 ]
then
echo "Error : making file1.es"
exit
fi

#
rm PES-*
ts -r $2
pes2es PES-${PID}.bin file2.es
if [ $? -gt 0 ]
then
echo "Error : making file2.es"
exit
fi

cat file1.es file2.es > all.es

es2pes all.es all.pes

pes2tts all.pes -Oconnected.tts
if [ $? -eq 0 ]
then
echo "Connect success"
rm all.es
rm all.pes
rm file?.es
fi

