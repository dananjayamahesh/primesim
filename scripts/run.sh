#!/bin/bash

#mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config.xml /home/s1797403/repos/primesim/output/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config.xml -o /home/s1797403/repos/primesim/output/config.out -- /home/s1797403/repos/primesim/pbench/atomics/atomics2 1000
mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config.xml /home/s1797403/repos/primesim/output/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config.xml -o /home/s1797403/repos/primesim/output/config.out -- /home/s1797403/repos/primesim/pbench/atomics/atomics2 1000

cat /home/s1797403/repos/primesim/output/config.out_1 /home/s1797403/repos/primesim/output/config.out_0 > /home/s1797403/repos/primesim/output/config.out

rm -f  /home/s1797403/repos/primesim/output/config.out_0
rm -f  /home/s1797403/repos/primesim/output/config.out_1
