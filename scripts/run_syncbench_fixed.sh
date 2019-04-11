#!/bin/bash
make -B
mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config_3.xml /home/s1797403/repos/primesim/output/syncbench-fixed/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config_3.xml -o /home/s1797403/repos/primesim/output/syncbench-fixed/config.out -- /home/s1797403/repos/primesim/pbench/syncbench/lockfree-linkedlist/bin/linkedlist -t 16 -i 200 -r 100000 -d 10 -x 6 -u 50
cat /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_1 /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_0 > /home/s1797403/repos/primesim/output/syncbench-fixed/config.out
cp  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out  /home/s1797403/repos/primesim/output/syncbench-fixed/config_16_3.out
rm -f  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_0
rm -f  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_1


mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config_1.xml /home/s1797403/repos/primesim/output/syncbench-fixed/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config_1.xml -o /home/s1797403/repos/primesim/output/syncbench-fixed/config.out -- /home/s1797403/repos/primesim/pbench/syncbench/lockfree-linkedlist/bin/linkedlist -t 16 -i 200 -r 100000 -d 10 -x 6 -u 50
cat /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_1 /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_0 > /home/s1797403/repos/primesim/output/syncbench-fixed/config.out
cp  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out  /home/s1797403/repos/primesim/output/syncbench-fixed/config_16_1.out
rm -f  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_0
rm -f  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_1


mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config_0.xml /home/s1797403/repos/primesim/output/syncbench-fixed/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config_0.xml -o /home/s1797403/repos/primesim/output/syncbench-fixed/config.out -- /home/s1797403/repos/primesim/pbench/syncbench/lockfree-linkedlist/bin/linkedlist -t 16 -i 200 -r 100000 -d 10 -x 6 -u 50
cat /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_1 /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_0 > /home/s1797403/repos/primesim/output/syncbench-fixed/config.out
cp  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out  /home/s1797403/repos/primesim/output/syncbench-fixed/config_16_0.out
rm -f  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_0
rm -f  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_1

#mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config.xml /home/s1797403/repos/primesim/output/syncbench/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config.xml -o /home/s1797403/repos/primesim/output/syncbench/config.out -- /home/s1797403/repos/synchrobench/c-cpp/bin/lockfree-hashtable -t 16 > tmp_syncbench2.txt

#mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config.xml /home/s1797403/repos/primesim/output/syncbench/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config.xml -o /home/s1797403/repos/primesim/output/syncbench/config.out -- /home/s1797403/repos/synchrobench/c-cpp/bin/lockfree-linkedlist -t 4 -d 15000 -i 10000 > tmp_syncbench_16.txt
#mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config.xml /home/s1797403/repos/primesim/output/syncbench/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config.xml -o /home/s1797403/repos/primesim/output/syncbench/config.out -- /home/s1797403/repos/synchrobench/c-cpp/bin/lockfree-linkedlist -i 10000 -d 90000 -t 31 > tmp_syncbench2.txt


#After syncbench patch
# mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config.xml /home/s1797403/repos/primesim/output/syncbench/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config.xml -o /home/s1797403/repos/primesim/output/syncbench/config.out -- /home/s1797403/repos/primesim/pbench/syncbench/lockfree-linkedlist/bin/linkedlist -t 2 -i 5 -r 10 -d 1000 -x 6 -u 50

#mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config.xml /home/s1797403/repos/primesim/output/syncbench-fixed/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config.xml -o /home/s1797403/repos/primesim/output/syncbench-fixed/config.out -- /home/s1797403/repos/primesim/pbench/syncbench/lockfree-linkedlist/bin/linkedlist -t 16 -i 1000 -r 2000 -d 1000 -x 6 -u 50
#mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config.xml /home/s1797403/repos/primesim/output/syncbench-fixed/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config.xml -o /home/s1797403/repos/primesim/output/syncbench-fixed/config.out -- /home/s1797403/repos/primesim/pbench/syncbench/lockfree-linkedlist/bin/linkedlist -t 16 -i 200 -r 20000 -d 10 -x 6 -u 50

#mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config_3.xml /home/s1797403/repos/primesim/output/syncbench-fixed/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config_3.xml -o /home/s1797403/repos/primesim/output/syncbench-fixed/config.out -- /home/s1797403/repos/primesim/pbench/syncbench/lockfree-linkedlist/bin/linkedlist -t 16 -i 200 -r 1000 -d 10 -x 6 -u 50
