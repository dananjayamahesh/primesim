#!/bin/bash  
program_name=proccount
#procedure counts
echo 'Starting Pin..'
echo $program_name
source ../../../env.sh
pin.sh -ifeellucky -t /home/mahesh/mahesh/edinburgh/repos/primesim/ppin/proccount/bin/proccount.so -- /bin/ls
