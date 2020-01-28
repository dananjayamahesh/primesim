echo $PRIME_PATH
#make -B
#balanced, addony, mremoved. THIS SHOULD BE BENCHTYPE.
#optype=balanced

#mode=zfix-uncached-balanced-3000-32
mode=zfix-cached-regular-2000-32
num_threads=32
#mode=multi-t
#mode=uncached

prime_output=${PRIME_PATH}/asplos_final
output_directory=${prime_output}/asplos-output/${mode}

echo $output_directory

#b8m-run-A-1
#syncbench-mbank-120cycles
#now cached contains only fixed version of lrp
#CONF_NAME=run-single-A-1


CONF_NAME=run-1
CONF_PATH=${output_directory}/${CONF_NAME}
DIMP_FILE=${output_directory}/stat.txt
DIMP2_FILE=${output_directory}/stat2.txt
DATA_FILE=${output_directory}/${CONF_NAME}/data_cpi.txt
DATA2_FILE=${output_directory}/${CONF_NAME}/data_meta.txt

#Running 
result_directory=${prime_output}/asplos-result/${mode}

RESULT_CONF=${result_directory}/${CONF_NAME}
EXEC_FILE=${RESULT_CONF}/exec_time.csv
WBC_FILE=${RESULT_CONF}/wb_crit.csv
WBC_NORM_FILE=${RESULT_CONF}/wb_crit_norm.csv
WBC_TOTAL_FILE=${RESULT_CONF}/wb_total.csv

if [ ! -d ${RESULT_CONF} ] 
	rm -rf ${RESULT_CONF}
 	mkdir -p ${RESULT_CONF}
then
	echo ${RESULT_CONF}
	echo ${PWD}
    mkdir -p ${RESULT_CONF}
fi 

script_directory=${prime_output}/asplos-scripts

#change this with the single t.
python ${script_directory}/run_exec_time_norm.py ${DATA_FILE} ${EXEC_FILE}
#single t use normal.
#python ${script_directory}/run_wb_crit_norm.py ${DATA2_FILE} ${WBC_NORM_FILE}

python ${script_directory}/run_wb_crit.py ${DATA2_FILE} ${WBC_FILE}

python ${script_directory}/run_wb_total.py ${DATA2_FILE} ${WBC_TOTAL_FILE}

#After all run.
#Average-
echo 'taking averages'
python ${script_directory}/run_averages.py ${result_directory} run exec_time.csv 1 ${result_directory}
python ${script_directory}/run_averages.py ${result_directory} run wb_crit.csv 1 ${result_directory}
#python ${script_directory}/run_averages.py ${result_directory} run wb_crit_norm.csv 1 ${result_directory}

python ${script_directory}/run_averages.py ${result_directory} run wb_total.csv 1 ${result_directory}