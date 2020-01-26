
# Lazy Release Persistency (LRP).

LRP is integrated to the PriME simulator.  PriME is an execution-driven x86 simulator for manycore architectures. It is capable of simulating 1000+ cores with rich cache hierarchies, coherence protocols and On-chip Network. It is capable of running both multi-threaded and multi-programmed workloads. Moreover, it can distribute multi-programmed workloads into multiple host machines and communicate via OpenMPI.

Note that this is not cycle accurate and a preliminary prototype of the idea.

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
* We recommend you to install these software manually. But you can also use **build_all.sh**
* gcc v4.8 (tested) or higher is recommended 
* Intel Pin tested under v2.14 71313 (Download [here](https://software.intel.com/sites/landingpage/pintool/downloads/pin-2.14-71313-gcc.4.4.7-linux.tar.gz) )
* OpenMPI tested under v3.0.0 (Download [here](https://download.open-mpi.org/release/open-mpi/v3.0/openmpi-3.0.0.tar.gz)) 
	* Need to enable MPI_THREAD_MULTIPLE which can be done by adding --enable-mpi-thread-multiple option during installation. If you already have OPEN MPI you can chek this by running "ompi_info | grep -i thread" command. If not you have to install openmpi. Please follow the [MPI_README.md](https://github.com/dananjayamahesh/primesim/blob/dimp/MPI_README.md) for more information. 
* libxml2 tested under v2.9.1
* Python 2.7 or above
* GNU bash, version 4.2.46(2)-release (x86_64-redhat-linux-gnu)

Our Setup
------------------
* Linux 3.10.0-1062.1.1.el7.x86_64
* AMD server with 64 CPUs (2 threads per core) with AMD Opteron(tm) Processor 6376 x86_64, 2.3 GHz.
* 16GiB in memory is enough


### Installation ![alt text](https://github.com/adam-p/markdown-here/raw/master/src/common/images/icon48.png "Logo Title Text 1")

Option 1: Build from DOI or other sources
-----------------------------------------
* Access doi [here]( https://doi.org/10.5281/zenodo.3562964)
* Run the script provided. 
* **sh build_lrp_all.sh**
	* This will create lrp+primesim and all the required software.I.e. 
		* primsim
		* software
	* Setup the environment on primesim/env.sh
	* compile lrp and pbench and generate binaries.
	* You can continue with the common-steps and experimental-workflow.


Option 2: Download LRP Source and Build (Preferred)
-----------------------------------------
* git clone https://github.com/dananjayamahesh/primesim.git
* cd primesim
* git checkout dimp
* (make sure you are in the dimp branch which is LRP)


Build all and compile automatically
-----------------------------------
* please check build_all.sh and run,
* **sh build_all.sh**
	* This creates software directory outside your primesim path with openmpi and pin.
	* Compile lrp and pbench and generate binaries.

Option 3: Build Manually 
------------------------
* This might be helpful if you already have some software like open MPI. 
* Install all software dependencies manually.
	* Check **build_all.sh** as well env.sh.


### Common Steps (From primesim path) ![alt text](https://github.com/adam-p/markdown-here/raw/master/src/common/images/icon48.png "Logo Title Text 1")

Compile LRP (PRiME)
--------------------------
* check and modify **env.sh** to set those environment variables to be the correct paths.(Important)
* source env.sh
* make -B

Compile Benchmarks (PBench)
----------------------------
* cd pbench/syncbench
* make -B
* cd ../..


### Experimental Workflow ![alt text](https://github.com/adam-p/markdown-here/raw/master/src/common/images/icon48.png "Logo Title Text 1")

Simple Test for MPI,Pin and LRP
-------------------------------
* mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 -np 1 ls
* Run this script to check if all components are working.

* **sh run_all.sh simple-test**

Test and Run the Framework
---------------------------
* This is just to make sure all the software/frameworks are working fine.
* **sh run_all.sh test** 


Possible Issues/Errors and Fix(Please go to next section if no errors)
--------------------------------------------------------------
* libxml2 file path not found. Plase check if your libxml2 path in the env.sh is right.
	* sudo apt-get install libxml2-dev (please check [here](https://askubuntu.com/questions/733169/how-to-install-libxml2-in-ubuntu-15-10))
* MPI Errors: libmpi.so path not found or similar MPI ERRORs. Please check OPENMPI_LIB_PATH in env.sh and check if the path is in the right path. [MPI_README.md](https://github.com/dananjayamahesh/primesim/blob/dimp/MPI_README.md)
	* Please check the env.sh and make sure libmpi.so file is in the correct path.
	* Also make sure your PATH and LD_LIBRARY_PATH have been set to correct locations.
	* Please check build_all.sh and env.sh file.
	* Reinstall Opem MPI, edit env.sh and source env.sh.
* please check the MPI_THREAD_MULTIPLE flag. Most of the time it is not set by default. In case, you have to reinstall MPI with the flag set (--enable-mpi-thread-multiple).
* Run **"ompi_info | grep -i thread"**. It should say yes. (Thread support: posix (MPI_THREAD_MULTIPLE: yes, OPAL support: yes, OMPI progress: no, ORTE progress: yes, Event lib: yes))
* echo 0 > /proc/sys/kernel/yama/ptrace_scop . If you are asked to do this, please do it.  You may need to do this as root. (su && echo 0 > /proc/sys/kernel/yama/ptrace_scop)


Please contact [dananjayamahesh@gmail.com](dananjayamahesh@gmail.com) if you have any question or issue.