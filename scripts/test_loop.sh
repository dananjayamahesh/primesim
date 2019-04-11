
for threads in 2 4 8 16 31
do
	for rate in 100 200 300 400 500 600 700 800 900 1000
	do
		for rate2 in 200 400 600 800 1000 1200 1400 1600 1800 2000
		do
			
			if [ $rate2 -lt $rate ] ; then 
				continue 
			fi
			
			echo "$threads $rate $rate2"
		done
	done
done


