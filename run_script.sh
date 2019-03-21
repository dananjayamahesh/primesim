#!/bin/bash
cd /home/mahesh/mahesh/edinburgh/repos/parsec-3.0

source env.sh

mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/mahesh/mahesh/edinburgh/repos/primesim/bin/prime /home/mahesh/mahesh/edinburgh/repos/primesim/xml/config.xml /home/mahesh/mahesh/edinburgh/repos/primesim/output/config.out : -np 1 parsecmgmt -a run -p blackscholes -i simlarge -n 2 -c gcc-pthreads -s "pin.sh -ifeellucky -t /home/mahesh/mahesh/edinburgh/repos/primesim/bin/prime.so -c /home/mahesh/mahesh/edinburgh/repos/primesim/xml/config.xml -o /home/mahesh/mahesh/edinburgh/repos/primesim/output/config.out --"

cat /home/mahesh/mahesh/edinburgh/repos/primesim/output/config.out_1 /home/mahesh/mahesh/edinburgh/repos/primesim/output/config.out_0 > /home/mahesh/mahesh/edinburgh/repos/primesim/output/config.out

rm -f /home/mahesh/mahesh/edinburgh/repos/primesim/output/config.out_0
rm -f /home/mahesh/mahesh/edinburgh/repos/primesim/output/config.out_1
