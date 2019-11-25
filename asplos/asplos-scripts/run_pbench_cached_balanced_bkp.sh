echo $PRIME_PATH
#make -B
#balanced, addony, mremoved. THIS SHOULD BE BENCHTYPE.
#optype=balanced

prime_output=${PRIME_PATH}/asplos
output_directory=${prime_output}/asplos-output/cached

echo $output_directory

repeat=1 #for averages
num_threads=32 #number of threads
config=small # < 100000 workloads
op_t=balanced #optional
#Repeat for averages
for r in 1
do
	CONF_NAME=run-large-B-${r}
	CONF_PATH=${output_directory}/${CONF_NAME}
	DIMP_FILE=${output_directory}/stat.txt
	DIMP2_FILE=${output_directory}/stat2.txt
	DATA_FILE=${output_directory}/${CONF_NAME}/data_cpi.txt
	DATA2_FILE=${output_directory}/${CONF_NAME}/data_meta.txt
	
	optype=balanced
	
	if [ ! -d ${CONF_PATH} ] 
		rm -rf ${CONF_PATH}
	 	mkdir -p ${CONF_PATH}
	then
		echo ${CONF_PATH}
		echo ${PWD}
	    mkdir -p ${CONF_PATH}
	fi 
	
	TIME_FILE=${CONF_PATH}/time.out
	> ${TIME_FILE}
	now=$(date +"%T")
	echo "Start time : $now" >> ${TIME_FILE}
	
	#echo "" > ${DIMP_FILE}
	#> ${DIMP_FILE}
	> ${DATA_FILE}
	> ${DATA2_FILE}
	
	#Configurations
	num_threads=32
	alter="-A"
	mode="cached"
	max_operations=100000 #=nb_threads*per_thread_ops
	ops_pro=6000
	urate=100 #syncbench update rate
	lfd_size=64000
	rate2=64000
	initial_size=1000
	benchmark=linkedlist
	
	#Original rate = initial size =1000/
	for rate in 1000 
	do
	
	for threads in 32
	do
		#for operations in 1000 4000
	    #for threads in 1 8 16 32
	    for operations in 5000
		do
				
				#for bench in  linkedlist
				for bench in  hashmap bstree skiplist lfqueue2 lfqueue linkedlist
				#for bench in  hashmap bstree skiplist lfqueue2 linkedlist lfqueue
				#for bench in linkedlist hashmap bstree skiplist lfqueue lfqueue2
				#for bench in linkedlist hashmap bstree skiplist lfqueue lfqueue2 locklist
				#for bench in lfqueue
				#for bench in bstree skiplist-rotating-c
				do	
					echo "$bench,$threads,$rate,$rate2,$operations" >> ${DATA_FILE}
					echo "$bench,$threads,$rate,$rate2,$operations" >> ${DATA2_FILE}
					#urate=100
					urate=100
					if [ bench = lfqueue ]; then
						echo "update rate changed"
	              	  urate=50
	              	else
	              		urate=100
	            	fi

	            	optype=balanced
            		if [ "${bench}" == "linkedlist" ]; then
						echo "optype rate changed"
              		  	optype=regular
              		else
              			optype=balanced
            		fi

            		now=$(date +"%T")
					echo "${bench} ${optype} Start time : $now" >> ${TIME_FILE}	
	
					#for pmodel in 7
					for pmodel in 0 8 3 7 4 6
					#for pmodel in 0
					do
						echo "Executing $pmodel"
						#operations=${rate2}
						#echo "$bench,$pmodel," >> ${DIMP_FILE}
						#echo "$pmodel" >> ${DATA_FILE}
						#${HOME}/repos/primesim/xml/shared-llc/config_mbank_120cycles_${pmodel}.xml \					
						mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 \
						-np 1 ${PRIME_PATH}/bin/prime \
						${PRIME_PATH}/xml/banked-llc/config_mbank_120cycles_${pmodel}.xml \
						${CONF_PATH}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out : \
						-np 1 pin.sh -ifeellucky -t ${PRIME_PATH}/bin/prime.so \
						-c ${PRIME_PATH}/xml/banked-llc/config_mbank_120cycles_${pmodel}.xml \
						-o ${CONF_PATH}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out \
						-- ${PRIME_PATH}/pbench/syncbench/bin/${bench}_${optype} -t ${threads} -i ${rate} -r ${rate2} -o ${operations} -d 10 -x 6 -u ${urate}
						
						#cat ${DIMP_FILE} >> ${DATA_FILE3}
						DIMP_FILE=${CONF_PATH}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_stat
						DIMP2_FILE=${CONF_PATH}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_stat2
	
						cat ${DIMP_FILE} >> ${DATA_FILE}
						#cat ${DIMP2_FILE} >> ${DATA2_FILE}
						cat ${DIMP2_FILE} | head -n 1 >> ${DATA2_FILE}
						
						cat ${CONF_PATH}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_1 \
						${CONF_PATH}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_0 \
						> ${CONF_PATH}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out
											
						rm -f  ${CONF_PATH}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_0
						
						rm -f  ${CONF_PATH}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_1
						rm -f ${DIMP_FILE}
						rm -f ${DIMP2_FILE}
					done
				 
					now=$(date +"%T")
					echo "${bench} End time : $now" >> ${TIME_FILE}	

				done
			
		done
	done
	done
	
	now=$(date +"%T")
	echo "End time : $now" >> ${TIME_FILE}
	
	#python run_exec_time.py
	
	##Result
	#Running ###################################################################
	result_directory=${prime_output}/asplos-result/${mode}
	
	RESULT_CONF=${result_directory}/${CONF_NAME}
	EXEC_FILE=${RESULT_CONF}/exec_time.csv
	WBC_FILE=${RESULT_CONF}/wb_crit.csv
	
	if [ ! -d ${RESULT_CONF} ] 
		rm -rf ${RESULT_CONF}
	 	mkdir -p ${RESULT_CONF}
	then
		echo ${RESULT_CONF}
		echo ${PWD}
	    mkdir -p ${RESULT_CONF}
	fi 
	
	script_directory=${prime_output}/asplos-scripts
	
	python ${script_directory}/run_exec_time_norm.py ${DATA_FILE} ${EXEC_FILE}
	python ${script_directory}/run_wb_crit_norm.py ${DATA2_FILE} ${WBC_FILE}
	#############################################################################


done
#END OF SCRIPTS

echo 'LRP Simulation End'