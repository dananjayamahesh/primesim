#!/bin/bash  
program_name=inscount

echo 'Starting Pin..'
echo $program_name
source ../../../env.sh
pin.sh -ifeellucky -t /home/mahesh/mahesh/edinburgh/repos/primesim/ppin/inscount/bin/inscount.so -o inscount.log -- /bin/ls
