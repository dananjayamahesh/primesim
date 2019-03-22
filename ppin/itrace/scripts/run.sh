#!/bin/bash  
program_name=itrace

echo 'Starting Pin..'
echo $program_name
source ../../../env.sh
pin.sh -ifeellucky -t /home/mahesh/mahesh/edinburgh/repos/primesim/ppin/itrace/bin/itrace.so -- /bin/ls
