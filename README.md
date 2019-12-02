
# Lazy Release Persistency (LRP).

LRP is integrated to the PriME simulator.  PriME is an execution-driven x86 simulator for manycore architectures. It is capable of simulating 1000+ cores with rich cache hierarchies, coherence protocols and On-chip Network. It is capable of running both multi-threaded and multi-programmed workloads. Moreover, it can distribute multi-programmed workloads into multiple host machines and communicate via OpenMPI.

Note: This repository contains only the up-to-date improved version of the LRP software, not any of the old versions.

Directory structure
-------------------

* bin: executable binary files 
* cmd: bash command files generated by run_prime
* dep: dependency files generated during compilation
* obj: object files generated during compilation
* output: default location for simulation report files
* src: source code files
* tools: python scripts to config and run PriME
* xml: default location for configuration xml files generated by config_prime

* pbench: benchmark based on the synchrobench lock-free data structures
* ppin: pin tests for lrp annotated with release semantics
* asplos: scripts and files for asplos artifacts.
	* /asplos
		* /asplos-scripts - scripts required to run some test programs
		* /asplos-result - final results of each program
		* /asplos-output - raw output of the primesim and lrp

Requirements for LRP
--------------------

* gcc v4.8 or higher is recommended 
* Intel Pin tested under v2.14 71313 (Download [here](https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads) )
* OpenMPI tested under v3.0.0 (Download [here](https://www.open-mpi.org/software/ompi/v3.0/)) (need to support MPI_THREAD_MULTIPLE which can be done by adding --enable-mpi-thread-multiple option during installation. If you already have OPEN MPI you can chek this by running "ompi_info | grep -i thread" command)
* libxml2 tested under v2.9.1
* Python 2.7 or above
* GNU bash, version 4.2.46(2)-release (x86_64-redhat-linux-gnu)

Our Setup
------------------
* Linux 3.10.0-1062.1.1.el7.x86_64
* AMD server with 64 CPUs (2 threads per core) with AMD Opteron(tm) Processor 6376 x86_64, 2.3 GHz.
* 16GiB in memory is enough

Download LRP and Build
----------------------
* git clone https://github.com/dananjayamahesh/primesim.git
* cd primesim
* git checkout dimp
* (make sure you are in the dimp branch which is LRP)

How to compile LRP (PRiME)
--------------------------
* Open and modify env.sh to set those environment variables to be the correct paths.(Important)
* source env.sh
* make -B

How to Compile Benchmarks (PBench)
----------------------------
* cd pbench/syncbench
* make -B
* cd ../..

Simple Test for MPI and Pin
---------------------------
* mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 ls
* Please also check pin. You can test both with the following test.


Test and Run the Framework
---------------------------
* This is just to make sure all the software/frameworks are working fine.
* Please make sure that you have write persmission for the prime directory
* chmod 777 run_all.sh
* ./run_all.sh test


Possible Issues/Errors and Fix(Please go to next section if no errors)
--------------------------------------------------------------
* libxml2 file path not found. Plase check if your libxml2 path in the env.sh is right ()
* libmpi.so path not found. Please check OPENMPI_LIB_PATH in env.sh and check if the path is in the right path.
* please check the MPI_THREAD_MULTIPLE flag. Most of the time it is not set by default. In case, you have to reinstall MPI with the flag set (--enable-mpi-thread-multiple).
* Run **"ompi_info | grep -i thread"**. It should say yes. (Thread support: posix (MPI_THREAD_MULTIPLE: yes, OPAL support: yes, OMPI progress: no, ORTE progress: yes, Event lib: yes))
* echo 0 > /proc/sys/kernel/yama/ptrace_scop . If you are asked to do this, please do it. 


Output Result Format
-------------------------
* LRP output two parameters: 
	* Execution Time normalized to the ideal (exec_time.csv)
	* Percentage of Critical Path Writeback  (wb_crit.csv)
* When you run a program, e.g. **test**, LRP generates follwing directories inside asplos-result/ and asplos-output path
	* asplos/
		* asplos-results/
			* **test**/
				* run-1/
				* run-2/
				* **exec_time.csv**
				* **wb_crit.csv**
		* asplos-output/
			* **test**/
				* run-1/
				* run-2/
* Two .csv files contain average values of two outputs: **normalized execution time** (exec_time.csv) to ideal and percentage of **critical path writebacks** (wb_crit.csv).
*  In case of running multiple times, you will see run-1, run-2 ... run-x.  And csv files inside directory give averages of all runs while each directory contains the result of each run.


Customizing Scripts to Run Different Configurations
---------------------------------------------------
* Finally to show the customizability and flexibility of using LRP, we show how to run a cutom programs.
* ./run_all.sh  [program_name]  [ benchmark]   [num_threads]  [repeat]
	* **program_name**: name you for the result directory. Please put something other than cached, uncached, multit, singlet, test. e.g. my_prog
	* **benchmark**: name of the benchmark. "all" runs all benchmaks: linkedlist, hashmap, bstree, skiplist, lfqueue. If you want to run a specific benchmark please specify it. E.g. hashmap
	* **num_threads**: Number of threads you want to run your benchmarks. E.g. 8
	* **repeat**: In case we want to run same benchmark for multiple times, we need to specify repeat_run. E.g. 2. Then the final output cintains the average values of all runs.
* *When increasing those values simulation time is also rapidly increasing.*

Example Run
---------------------
* ./run_all.sh my_prog all 8 2

* ths commad will run program called my_prog twice with all benchmarks on 8 threads and generate average output in the result directory asplos/asplos-results/my_prog. (You can find results of each run inside the folder asplos/asplos-results/my_prog/run-x)
* Please check run_all.sh file for more information. It is flexible to change other paramters as well.
* NOTE: if you run without any parameters it runs with defaults values. e.g program_name=example_prog

LRP Paper Sample Programs (Run only Once) 
------------------------------------------------
* ./run_all.sh cached (Cached mode with 32 threads. Estimated runtime : 8-15hrs in our setup)
* ./run_all.sh uncached (Uncached mode with 32 threads. Estimated runtime : 10-16hrs)
* ./run_all.sh multit (16 threads with cached mode. Estimated runtime : 8-12hrs )
* ./run_all.sh singlet (single thread with cached mode . Estimated runtime: within few hours)
* These programs corresponding to a up-to-date sample version of programs (that can run within hours) that we used to generate result of the paper. Also, these programs only run once. To take the averages you need to change the repeat variable. Please be aware that changing some parameters can result in completely different configuration. 

Output of LRP Programs
-----------------------

* OUTPUT: FOr every single run, as exaplined before, lrp creates a output and result folder inside asplos-output/ and asplos-result. Inside that folder you will find exec_time.csv and wb_crit.csv output files. E.g.
	* asplos/
		* asplos-result/
			* cached
				* run-1/
				* exec_time.csv
				* wb_crit.csv
* You can find the **expected results** inside **asplos/asplos-results-expected** folder for each run. E.g.
	* asplos/
		* asplos-result-expected/
			* cached/

NOTE: These program can run within much less time in faster processors.

Please contact [dananjayamahesh@gmail.com](dananjayamahesh@gmail.com) if you have any question or issue.