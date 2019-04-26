CONF_NAME=syncbench-init
CONF_PATH=${HOME}/repos/primesim/output/${CONF_NAME}/
DIMP_FILE=${HOME}/repos/primesim/output/stat.txt
DATA_FILE=${HOME}/repos/primesim/output/${CONF_NAME}/data_2.txt
DATA_FILE3=${HOME}/repos/primesim/output/${CONF_NAME}/data_3.txt

#echo "" > ${DIMP_FILE}
#> ${DIMP_FILE}
> ${DATA_FILE}

#make -B
#SyncMap is 10000

#for upd in 50 100
#do
operations=1000

for threads in 8 16 31
do
	#for rate in 500
	#do
		#for rate2 in 200 400 600 800 1000 1200 1400 1600 1800 2000 10000
		for rate2 in 1000 4000 8000
		do
			
			rate=${rate2}
			#operations=2000
			
			for bench in linkedlist hashmap bstree skiplist-rotating-c
			do	
				echo "$bench,$threads,$rate,$rate2,$operations" >> ${DATA_FILE}
				for pmodel in 0 3 4
				do
					echo "Executing $pmodel"
					#operations=${rate2}
					#echo "$bench,$pmodel," >> ${DIMP_FILE}
					#echo "$pmodel" >> ${DATA_FILE}
					
					mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 \
					-np 1 ${HOME}/repos/primesim/bin/prime \
					${HOME}/repos/primesim/xml/config_${pmodel}.xml \
					${HOME}/repos/primesim/output/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out : \
					-np 1 pin.sh -ifeellucky -t ${HOME}/repos/primesim/bin/prime.so \
					-c ${HOME}/repos/primesim/xml/config_${pmodel}.xml \
					-o ${HOME}/repos/primesim/output/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out \
					-- ${HOME}/repos/primesim/pbench/syncbench/bin/${bench} -t ${threads} -i ${rate} -r ${rate2} -o ${operations} -d 10 -x 6 -u 50
					
					#cat ${DIMP_FILE} >> ${DATA_FILE3}
					cat ${DIMP_FILE} >> ${DATA_FILE}
					
					cat ${HOME}/repos/primesim/output/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_1 \
					${HOME}/repos/primesim/output/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_0 \
					> ${HOME}/repos/primesim/output/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out
										
					rm -f  ${HOME}/repos/primesim/output/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_0
					
					rm -f  ${HOME}/repos/primesim/output/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_1
				done
			   
			done
		done
	#done
done
#done