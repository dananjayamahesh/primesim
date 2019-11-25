#Run Benchmarks
#!/bin/bash
pmode=multit #cached #multi-thread
num_threads=32 #number of running threads

#If you run more than 100k. Need to change bench_type to alternative 
#bench_type=alternate
bench_type=balanced #1) regular or 2)alternate. 
#alternate bench give more accurate resulsts. But extremely slow.

#Benchmarks
benchmark=all #all or 
bench_size=small # large or xl

lfd_size=64000 # tested under 100k (>16k)
update_rate=100

max_operations=100000 #< only tested under 100k ops.
ops=3000 #ops per process.
init_size=1000


if [ -z "$1" ]; then
	echo 'param1 is not defined'
else
	if [ "$1" == "-help" ]; then
		echo "Help"
	    exit 1
	fi	
fi	

#parameters
if [ -z "$1" ]; then
    # Code for the case that the user didn't pass a third argument
    echo "pmode (default) 	   : [$pmode]"
else
	pmode=$1
	echo "mode                 : [$pmode]"
fi


if [ -z "$2" ]; then
    # Code for the case that the user didn't pass a third argument
    echo "threads (default)    : [$num_threads]"
else
	num_threads=$2
	echo "threads              : [$num_threads]"
fi

if [ -z "$3" ]; then
    # Code for the case that the user didn't pass a third argument
    echo "bench size (default) : [$bench_size]"
    echo "bench type (default) : [$bench_type]"
else
	bench_size=$3
	echo "bench size : [$bench_size]"
	echo "bench type : [$bench_type]"
	#change benchsize according to the 
fi


ASPLOS_PATH=${PRIME_PATH}/asplos
ASPLOS_SCRIPTS=${ASPLOS_PATH}/asplos-scripts

chmod 777 asplos/asplos-scripts/*

if [ "${pmode}" == "cached" ]; then
	file_path=${ASPLOS_SCRIPTS}/run_pbench_${pmode}_${bench_type}.sh

elif [ "${pmode}" == "uncached" ]; then
	file_path=${ASPLOS_SCRIPTS}/run_pbench_${pmode}_${bench_type}.sh #Thread number?

elif [ "${pmode}" == "multit" ]; then
	file_path=${ASPLOS_SCRIPTS}/run_pbench_cached_${bench_type}_${pmode}.sh #Thread number?

else
	file_path=${ASPLOS_SCRIPTS}/run_pbench_${pmode}_${bench_type}.sh
fi

#parameters : threads, size, operations.

echo "Start Simulations"
echo "Running $file_path"
echo "Finished Running"


