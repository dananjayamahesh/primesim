echo $PRIME_PATH
echo "Cached Singlet Mode"
#make -B
#balanced, addony, mremoved. THIS SHOULD BE BENCHTYPE.
#optype=balanced

mode="singlet"

prime_output=${PRIME_PATH}/asplos
output_directory=${prime_output}/asplos-output/${mode}

echo $output_directory

repeat=1 #for averages
num_threads=1 #number of threads --all
config=small # < 100000 workloads
op_t=balanced #optional
optype=balanced

#Repeat for averages
#Configurations
#num_threads=32
alter="-A"
max_operations=100000 #=nb_threads*per_thread_ops
ops_pp=96000 #if ops >10000 please set to 10000
urate=100 #syncbench update rate
lfd_size=64000
rate2=64000
initial_size=1000
benchmark=linkedlist
optional_alt=""

for r in `seq 1 $repeat`
do
	CONF_NAME=run-${r}
	CONF_PATH=${output_directory}/${CONF_NAME}
	DIMP_FILE=${output_directory}/stat.txt
	DIMP2_FILE=${output_directory}/stat2.txt
	DATA_FILE=${output_directory}/${CONF_NAME}/data_cpi.txt
	DATA2_FILE=${output_directory}/${CONF_NAME}/data_meta.txt
	
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
	
	#Original rate = initial size =1000/
	for rate in 1000 
	do
	
	for threads in ${num_threads}
	do
		#for operations in 1000 4000
	    #for threads in 1 8 16 32
	    for operations in ${ops_pp} 
		do
				
				#for bench in  linkedlist
				for bench in  hashmap bstree skiplist lfqueue2 linkedlist
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
	            	optional_alt=""
            		if [ "${bench}" == "linkedlist" ]; then
						echo "optype rate changed"
              		  	optype=regular
              		  	optional_alt="-A"
              		  	#Should enable the -A as well.
              		else
              			optype=balanced
              			optional_alt=""
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
						-- ${PRIME_PATH}/pbench/syncbench/bin/${bench}_${optype} -t ${threads} -i ${rate} -r ${rate2} -o ${operations} -d 10 -x 6 -u ${urate} ${optional_alt}
						
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
	python ${script_directory}/run_wb_crit.py ${DATA2_FILE} ${WBC_FILE}
	#############################################################################


done
#END OF SCRIPTS

echo 'LRP Simulation End'
echo 'Taking Averages of $repeat Runs'
python ${script_directory}/run_averages.py ${result_directory} run exec_time.csv $repeat ${result_directory}
python ${script_directory}/run_averages.py ${result_directory} run wb_crit.csv $repeat ${result_directory}

