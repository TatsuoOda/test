#
for i in ../*.mp4
do
echo $i
#file=`echo $i | sed -e 's/^...//'`
#echo ${file}
filename=${i##*/}
#echo $filename
filename_no_ext=${filename%.*}
echo ${filename_no_ext}
outfile=${filename_no_ext}

mov "$i" +A
spes +e output.spes
pes out.264 -AVC +Z > "${outfile}.log"

mv output.spes "${outfile}.spes"
mv out.264 "${outfile}.264"
#exit

done

