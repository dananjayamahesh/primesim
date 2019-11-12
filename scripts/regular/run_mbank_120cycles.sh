#I am fixing few bugs. 
# 1. NOP does not need any write backs.
#2 rp replacement
#3 core id delay
#4 remove release persistency  replacement writeback.
#remove delay from natural write back. it may have effected core 1

#balanced, addony, mremoved
optype=regular
#optype=balanced
#RUn History
#syncbench-mbank-120cycles-nopfixed-4 ->initial run after fixing rp writeback delays
#syncbench-mbank-120cycles-nopfixed-4-1 ->same
#syncbench-mbank-120cycles-nopfixed-4-2 ->add more counters for write backs and epoch sizes, add thread 1,
#CONF_NAME=syncbench-mbank-120cycles-nopfixed-6 original with u 100
CONF_NAME=syncbench-mbank-120cycles-nopfixed-35
CONF_PATH=${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/
DIMP_FILE=${HOME}/repos/primesim/output/${optype}/stat.txt
DIMP2_FILE=${HOME}/repos/primesim/output/${optype}/stat2.txt
DATA_FILE=${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/data.txt
DATA2_FILE=${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/data2.txt

#8 - parity load 4 bytes. NO pa
#9 increasing parity load for 8bytes. 2 long variables. 
#10 reduce it to 32 bit or no 
#10 I identified my shared llc is buggy
#benchmark - 32B 32byte load

#11 is the base model of our results
#11 4B, shared-llc for mbank , 64K
#12 4B, non shared-llc as 8, but with 4B,64K
#13 again 2nd run of 11. 11 seems good 4B, shared-llc mbank, 64K
#14 again 3nd run of 11. 11 seems good 4B, shared-llc mbank, 64K

#15 again 2nd run of 11. 11 seems good 4B, shared-llc mbank, With 1 Million nodes
#16 again 2nd run of 11. 11 seems good 4B, shared-llc mbank, With 1 Million nodes, 15 again

#17 again 2nd run of 11. 11 seems good 4B, shared-llc mbank, With 8k nodes, 15 again
#18 again 2nd run of 11. 11 seems good 4B, shared-llc mbank, With 8k nodes, 15 again

#19 again 2nd run of 11. 11 seems good 4B, shared-llc mbank, With 4M nodes, 15 again

#Parity: load
#20 11 with 64K and 32B parity load, 32B, shared-llc mbank, With 64k nodes.

#21 11 with 64K and 32B parity load, 64B, shared-llc mbank, With 64k nodes.- Stop in the moddle. Regula is finished


#22 11 with 64K and 4B parity load, 4B, shared-llc mbank, With 32k nodes. only in Regular
#23 11 with 64K and 4B parity load, 4B, shared-llc mbank, With 1024 nodes. Only in regular

#11-wbfixed as same as 11 running only on balanced mode. After fixing WB count.

#24 chaning L1 cache size to 64k , 65536 from  32768
#Originally L1 num of ways are 4-way
#25- chaning L1 cache size to 32K again and change to 4-ways 

#New runs after the asplos paper
#26- chaning L1 cache size to 32K again and change to 8-ways 

#26 failed unexpectedly: only 120, 300 regular and balanced
#27  26- changing L1 cache size to 32K again and change to 8-ways 

#asplos after
#28 balanced and regular with M and E states counters. 
#Add writeback to the visibility conflicts. CLWB

#starting to change the cache hierarchy
#29 96B, 64K elements, 32K caches eveythig as old. but, No persistency: WB no dram access and Invalidations. only M->S
#30 - 29 same. with more counters to lowest epoch sizes. path for proactive flushing.
#30 has a bug. 31 again

#31 had a segmentation faults in skiplist and queue (balanced) in BEP. queue (regular).
#issue: epoch counter goes beyond 10k

#Seems like we have a problem with the epoch ID. Epoch COunter > 10000. is operations == 10000.
#L1 cache sizes

#asplos after: with visibility write back and nop fixed.

#32 fixed and run. runnign ok. Without PF. RP, SB and BB numbbers are fine.

#33. BEP+Poactive flushing (with lowest epoch id). BB+PF is working/

#Check.... There is a difference between BEP+PF and Epoch Persistency in 33.

#34: Run all benchmarks with all persistency models including BEP+PF=7. 0 3 7 4 6. 
#- Original BEP results were changed from 32. 32 without PF.
#34: SB went viral. lower values.


#35: Changes: Add lazy_wb=false to RP, FB, BEP properly. I think the problem of SB (FB) is that lazy_wb is randomly set. method-local.

if [ ! -d ${CONF_PATH} ] 
then
    mkdir -p ${CONF_PATH}
fi 

#echo "" > ${DIMP_FILE}
#> ${DIMP_FILE}
> ${DATA_FILE}
> ${DATA2_FILE}

#make -B
#SyncMap is 10000

#for upd in 50 100
#do
#make -B
urate=100


#rate=1000
#rate2=64000
#rate2=1000000
#rate2=8000
#rate2=4000000
#rate2=32000
rate2=64000

#for threads in 8 16 32
for rate in 1000 
do
for operations in 1000
do
	#for operations in 1000 4000
    #for threads in 1 8 16 32
    for threads in 32
	do
			
			for bench in  lfqueue hashmap bstree skiplist lfqueue2 linkedlist
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

				for pmodel in 6 0 3 7 4
							 
				do
					echo "Executing $pmodel"
					#operations=${rate2}
					#echo "$bench,$pmodel," >> ${DIMP_FILE}
					#echo "$pmodel" >> ${DATA_FILE}
					#${HOME}/repos/primesim/xml/shared-llc/config_mbank_120cycles_${pmodel}.xml \
					
					mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 \
					-np 1 ${HOME}/repos/primesim/bin/prime \
					${HOME}/repos/primesim/xml/shared-llc/config_mbank_120cycles_${pmodel}.xml \
					${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out : \
					-np 1 pin.sh -ifeellucky -t ${HOME}/repos/primesim/bin/prime.so \
					-c ${HOME}/repos/primesim/xml/shared-llc/config_mbank_120cycles_${pmodel}.xml \
					-o ${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out \
					-- ${HOME}/repos/primesim/pbench/syncbench/bin/${bench}_${optype} -t ${threads} -i ${rate} -r ${rate2} -o ${operations} -d 10 -x 6 -u ${urate}
					
					#cat ${DIMP_FILE} >> ${DATA_FILE3}
					DIMP_FILE=${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_stat
					DIMP2_FILE=${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_stat2

					cat ${DIMP_FILE} >> ${DATA_FILE}
					#cat ${DIMP2_FILE} >> ${DATA2_FILE}
					cat ${DIMP2_FILE} | head -n 1 >> ${DATA2_FILE}
					
					cat ${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_1 \
					${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_0 \
					> ${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out
										
					rm -f  ${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_0
					
					rm -f  ${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_1
					rm -f ${DIMP_FILE}
					rm -f ${DIMP2_FILE}
				done
			   
			done
		
	done
done
done