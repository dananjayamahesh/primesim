#!/bin/bash  
program_name=acqrel

echo 'Starting Pin..'
echo $program_name
source ../../../env.sh
pin.sh -ifeellucky -t /home/mahesh/mahesh/edinburgh/repos/primesim/ppin/acqrel/bin/acqrel.so -- /bin/ls
