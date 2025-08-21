## Variables ##
timestamp=$(date +%Y%m%d_%H%M%S)
values=(0 1 2 3 4 5 6_0 7_0 6_1 7_1 7_2 8_2 9_2 8_3 current)
#values=(0 current)
repeat=5

# Update variables
#subdtype=tris #update
subdtype=patches #update
resolution=960x540 #update

# Car scene
script=scripts/subd_car_tunnel
outimage=car_direct_cuda
outfile=time_eval_car_${timestamp}.txt
variant=car_back #update
sppx=4096 #update

# OptiX test scene
#script=scripts/subd_optix_tests
#outimage=test_direct_cuda
#outfile=time_eval_test_${timestamp}.txt
#variant=overview #update
#sppx=4096 #update

# Playground scene
#script=scripts/subd_playground
#outimage=playground_direct_cuda
#outfile=time_eval_playground_${timestamp}.txt
#variant=cartoon_dragon #update
#sppx=4096 #update


## Start script ##
:> $outfile
echo "Script: $script" >> $outfile
echo "Variant: $variant" >> $outfile
echo "SPPX: $sppx" >> $outfile
echo "Resolution: $resolution" >> $outfile
echo -e "Subd type: $subdtype\n" >> $outfile

# dry run one instance
echo "Dryrun:" >> $outfile
out=$(./rtgi_${values[0]} -s $script | grep Took)
echo $out >> $outfile
out=$(echo $out | grep -oE '[0-9]+ ms' | grep -oE '[0-9]+')
estimate="Will take around $((out*${#values[@]}*repeat/60000)) minutes"
echo $estimate >> $outfile
echo $estimate

#for j in $(seq 1 $repeat); do
for i in "${values[@]}"; do
	echo -e "\n$i:" >> $outfile
	sum=0
	for j in $(seq 1 $repeat); do
		out=$(./rtgi_${i} -s $script | grep Took)
		echo $out >> $outfile
		out=$(echo $out | grep -oE '[0-9]+ ms' | grep -oE '[0-9]+')
		sum=$((sum+out))
	done
	echo "Average: $((sum/repeat)) ms" >> $outfile
	mv out_subds/${outimage}.png out_subds/${outimage}_${i}.png
done
#done

echo -e "\nFinished evaluation" >> $outfile