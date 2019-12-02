#!/bin/bash
#AUTHOR: Mahesh Dananjaya (University of Edinburgh)
#Run Benchmarks######################################################################################
#1 program-name or mode in case of paper
#2 number of threads
#3 operations per process
#4 repeat runs
#5 Benchmarks : all is default. [linkedlin hashmap bstree skiplist lfqueue]
#5 lfd size
#6 benchmark size: small or large
#7 benchmark configurations : regular, balanced, alternative, iterative, backed, addonly, memremoved 
#####################################################################################################

#pmode is taken as a program name in case of genral programs.
#Large programs require "alternative" and ops_pp>10000. It takes a lot of time

#CONF PARAMTERS
#####################################################################################
pmode=example_prog #cached #multi-thread
num_threads=8 #number of running threads
repeat=1
#If you run more than 100k. Need to change bench_type to alternative 
#bench_type=alternate
optype=balanced #1) regular or 2)alternate. 
#alternate bench give more accurate resulsts. But extremely slow.

#Benchmarks
benchmark=all #all or [linkedlist hashmap bstree skiplist lfqueue]
bench_size=small # large or xl

lfd_size=64000 # tested under 100k (>16k)
update_rate=100

max_operations=96000 #< only tested under 100k ops.
ops_pp=6000 #ops per process. = max_operations/num_threads. Tested under <10000 for artificts.
init_size=1000 #intial lfd size. <tested under 1000>
#######################################################################################################

if [ -z "$1" ]; then
	echo 'param1 is not defined'
else
	if [ "$1" == "-help" ]; then
		echo "Help"
	    exit 1
	fi	
fi	

#Program Name or Mode
if [ -z "$1" ]; then
    # Code for the case that the user didn't pass a third argument
    echo "pmode (default) 	   : [$pmode]"
else
	pmode=$1
	echo "mode                 : [$pmode]"
fi

#benchmarks
#Number of threads
if [ -z "$2" ]; then
    # Code for the case that the user didn't pass a third argument
    echo "Benchmark (default)    : [$benchmark]"
else
	benchmark=$2
	echo "Benchmark              : [$benchmark]"
fi

#Number of threads
if [ -z "$3" ]; then
    # Code for the case that the user didn't pass a third argument
    echo "threads (default)    : [$num_threads]"
else
	num_threads=$3
	echo "threads              : [$num_threads]"
fi

#Repeats
if [ -z "$4" ]; then
    # Code for the case that the user didn't pass a third argument
    echo "Repeats  (default)    : [$repeat]"
else
	repeat=$4
	echo "Repeats              : [$repeat]"
fi

#Operation Per process
if [ -z "$5" ]; then
    # Code for the case that the user didn't pass a third argument
    echo "operations Per Process  (default)    : [$ops_pp]"
else
	ops_pp=$5
	echo "operations Per Process              : [$ops_pp]"
fi

#Maximum Size of the Lock-free data structure
if [ -z "$6" ]; then
    # Code for the case that the user didn't pass a third argument
    echo "Max LFD Size  (default)    : [$lfd_size]"
else
	lfd_size=$6
	echo "Max LFD Size             : [$lfd_size]"
fi

#Type of benchmark configurations. Keep intact for now.
if [ -z "$7" ]; then
    # Code for the case that the user didn't pass a third argument
    echo "bench size (default) : [$bench_size]"
    echo "bench type (default) : [$optype]"
else
	bench_size=$7
	echo "bench size : [$bench_size]"
	echo "bench type : [$optype]"
	#change benchsize according to the 
fi

#if the num_threads==1 then it should run singlet.
#Check max operations and if its >96000 -A flag should be up for large workloads.

ASPLOS_PATH=${PRIME_PATH}/asplos
ASPLOS_SCRIPTS=${ASPLOS_PATH}/asplos-scripts

chmod 777 asplos/asplos-scripts/*


#simple version of (updated) benchmarks used to generate figures in the paper.
#Figure 4 and 5
if [ "${pmode}" == "simple-test" ]; then
	./asplos/asplos-scripts/run_simple_test.sh

elif [ "${pmode}" == "test" ]; then
	./asplos/asplos-scripts/run_test.sh

elif [ "${pmode}" == "cached" ]; then
	./asplos/asplos-scripts/run_pbench_cached_balanced.sh

#FIgure 6
elif [ "${pmode}" == "uncached" ]; then
	./asplos/asplos-scripts/run_pbench_uncached_balanced.sh #Thread number?

#Figure 7  
elif [ "${pmode}" == "multit" ]; then

	./asplos/asplos-scripts/run_pbench_cached_balanced_multit.sh #Thread number? #parameters

#Figure 7  # signle
elif [ "${pmode}" == "singlet" ]; then
	./asplos/asplos-scripts/run_pbench_cached_balanced_singlet.sh #Thread number? #Should be singlet

else #Generic name # Program name miust be given. It should be different to standard modes.
	echo "Generic Progrm is running $pmode"	
	./asplos/asplos-scripts/run_pbench.sh $pmode $benchmark $num_threads $repeat $ops_pp $lfd_size

fi


#parameters : threads, size, operations.

echo "Start Simulations"
echo "Running $file_path"
echo "Finished Running"