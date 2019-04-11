mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 /home/s1797403/repos/primesim/bin/prime /home/s1797403/repos/primesim/xml/config_0.xml /home/s1797403/repos/primesim/output/syncbench-fixed/config.out : -np 1 pin.sh -ifeellucky -t /home/s1797403/repos/primesim/bin/prime.so -c /home/s1797403/repos/primesim/xml/config_0.xml -o /home/s1797403/repos/primesim/output/syncbench-fixed/config.out -- /home/s1797403/repos/primesim/pbench/syncbench/bin/linkedlist -t 31 -i 500 -r 1000 -d 10 -x 6 -u 50

cat /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_1 /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_0 > /home/s1797403/repos/primesim/output/syncbench-fixed/config.out

rm -f  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_0
rm -f  /home/s1797403/repos/primesim/output/syncbench-fixed/config.out_1