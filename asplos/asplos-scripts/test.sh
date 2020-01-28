#!/bin/bash

echo $PRIME_PATH
export LRP="$PRIME_PATH This is nuts"
echo $LRP

#make -B
#balanced, addony, mremoved. THIS SHOULD BE BENCHTYPE.
#optype=balanced
bench=hashmap
                optype=balanced
                optional_alt=""
                if [ "${bench}" == "linkedlist" ]; then
            echo "optype rate changed"
                      optype=regular
                      optional_alt="-A"
                      #How about alternative                  
                  elif [ "${bench}" == "skiplist" ]; then
            echo "optype rate changed"
                      optype=regular
                      optional_alt=""
                      #How about alternative
                  else
                    optional_alt=""
                    optype=balanced
                fi

echo "$bench $optype $optional_alt"

prime_output=${PRIME_PATH}/asplos
output_directory=${prime_output}/asplos-output/cached

echo $output_directory

repeat=4 #for averages
num_threads=32 #number of threads --all
config=small # < 100000 workloads
op_t=balanced #optional
#Repeat for averages
for r in `seq 1 $repeat`
do
  CONF_NAME=run-B-${r}
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
  
  echo $CONF_NAME
  echo $CONF_PATH
  echo $DATA_FILE
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
  optional_alt="" #should enabled only for the linkedlist. balanced may take time.
  
  #Original rate = initial size =1000/
  for rate in 1000 
  do
  
  for threads in 32
  do
    #for operations in 1000 4000
      #for threads in 1 8 16 32
      for operations in 3000
    do
        
        #for bench in  linkedlist
        for bench in  hashmap linkedlist lfqueue
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
                      #How about alternative
                  else
                    optional_alt=""
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
            cmd="mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 \
            -np 1 ${PRIME_PATH}/bin/prime \
            ${PRIME_PATH}/xml/banked-llc/config_mbank_120cycles_${pmodel}.xml \
            ${CONF_PATH}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out : \
            -np 1 pin.sh -ifeellucky -t ${PRIME_PATH}/bin/prime.so \
            -c ${PRIME_PATH}/xml/banked-llc/config_mbank_120cycles_${pmodel}.xml \
            -o ${CONF_PATH}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out \
            -- ${PRIME_PATH}/pbench/syncbench/bin/${bench}_${optype} -t ${threads} -i ${rate} -r ${rate2} -o ${operations} -d 10 -x 6 -u ${urate} ${optional_alt}"
            
            #cat ${DIMP_FILE} >> ${DATA_FILE3}
            #echo $cmd

          done
         
          now=$(date +"%T")
          echo "${bench} End time : $now" >> ${TIME_FILE} 

        done
      
    done
  done
  done
  

done
#END OF SCRIPTS

echo 'LRP Simulation End'





N=3
arr=['seq 1 ${N}']
echo ${arr}

for i in `seq 1 $N`
do
  echo "$i"
done

repeat=16
rep=`seq 1 $repeat`
echo $rep
for r in `seq 1 $repeat`
do
  echo ${r}
done

bench=linkedlist
optype=balanced

	            	optype=balanced
            		if [ "${bench}" == "linkedlist" ]; then
						echo "optype rate changed"
              		  	optype=regular
              		else
              			optype=balanced
            		fi
	        #optype=balanced
         if [ "${bench}" == "linkedlist" ]; then
				echo "optype changed"
             	optype=regular
           else
           	optype=balanced
         fi
echo ${bench}
echo ${optype}

repeat=2
r_vec={1..${repeat}}
for r in ${r_vec[@]}
do
	echo "${r}"

done