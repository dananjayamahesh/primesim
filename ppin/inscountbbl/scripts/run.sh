#!/bin/bash  
program_name=inscountbbl

echo 'Starting Pin..'
echo $program_name
source ../../../env.sh
pin.sh -ifeellucky -t /home/mahesh/mahesh/edinburgh/repos/primesim/ppin/inscountbbl/bin/inscountbbl.so -o inscountbbl.out -- /bin/ls
