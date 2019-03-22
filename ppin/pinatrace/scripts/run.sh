#!/bin/bash  
program_name=pinatrace

echo 'Starting Pin..'
echo $program_name
source ../../../env.sh
pin.sh -ifeellucky -t /home/mahesh/mahesh/edinburgh/repos/primesim/ppin/pinatrace/bin/pinatrace.so -- /bin/ls
