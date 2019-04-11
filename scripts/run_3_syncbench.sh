DIMP_FILE=/home/s1797403/repos/primesim/output/syncbench-fixed/stat.txt
DATA_FILE=/home/s1797403/repos/primesim/output/syncbench-fixed/data.txt

#echo "" > ${DIMP_FILE}
> ${DIMP_FILE}
> ${DATA_FILE}

make -B
#SyncMap is 10000
for threads in 2 4 8 16 32 48
do
	for rate in 100 200 500 1000
	do
		#for rate2 in 200 400 600 800 1000 1200 1400 1600 1800 2000 10000
		for rate2 in 200 500 1000 2000 5000
		do
			
			if [ $rate2 -lt $rate ] ; then 
				continue 
			fi
			
			for bench in linkedlist hashmap bstree skiplist-rotating-c
			do	
				echo "$bench,$threads,$rate,$rate2" >> ${DATA_FILE}
				for pmodel in 0 3 4
				do
					echo "Executing $pmodel"
					#echo "$bench,$pmodel," >> ${DIMP_FILE}
					#echo "$pmodel" >> ${DATA_FILE}
					
					mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 \
					-np 1 /home/s1797403/repos/primesim/bin/prime \
					/home/s1797403/repos/primesim/xml/config_${pmodel}.xml \
					/home/s1797403/repos/primesim/output/syncbench-fixed/config.out : \
					-np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so \
					-c /home/s1797403/repos/primesim/xml/config_${pmodel}.xml \
					-o /home/s1797403/repos/primesim/output/syncbench-fixed/config.out \
					-- /home/s1797403/repos/primesim/pbench/syncbench/bin/${bench} -t ${threads} -i ${rate} -r ${rate2} -d 10 -x 6 -u 50
					
					cat ${DIMP_FILE} >> ${DATA_FILE}
					
					cat /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_1 \
					/home/s1797403/repos/primesim/output/syncbench-fixed/config.out_0 \
					> /home/s1797403/repos/primesim/output/syncbench-fixed/config.out
					
					cp  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out \
					 /home/s1797403/repos/primesim/output/syncbench-fixed/config_p.out
					
					rm -f  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_0
					
					rm -f  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_1
				done
			   
			done
		done
	done
done



