#cd ..

export LRP=$PWD
echo $LRP
#source "export LRP="$PWD""

if [ ! -d "software" ]; then
	echo "create software folder"
	mkdir software
fi

cd software


#Open MPI
#curl "https://download.open-mpi.org/release/open-mpi/v3.0/openmpi-3.0.0.tar.gz"  --output openmpi-3.0.0.tar.gz

if [ ! -f "openmpi-3.0.0.tar.gz" ]; then
	echo "Downloading open mpi"
	rm -rf openmpi-3.0.0
   	wget "https://download.open-mpi.org/release/open-mpi/v3.0/openmpi-3.0.0.tar.gz"
	tar -xvf openmpi-3.0.0.tar.gz
fi


#Install OpenMPI in the path software/ompi
cd openmpi-3.0.0
./configure --prefix=${LRP}/software/ompi --enable-mpi-thread-multiple
make && make all install
#Make sure that libmpi.so exists
cd ..

#Pin
if [ ! -f "pin-2.14-71313-gcc.4.4.7-linux.tar.gz" ]; then
	echo "Downloading pin"
	rm -rf pin-2.14-71313-gcc.4.4.7-linux
   	wget "https://software.intel.com/sites/landingpage/pintool/downloads/pin-2.14-71313-gcc.4.4.7-linux.tar.gz"
	tar -xvf pin-2.14-71313-gcc.4.4.7-linux.tar.gz
fi
#curl "https://software.intel.com/sites/landingpage/pintool/downloads/pin-2.14-71313-gcc.4.4.7-linux.tar.gz" --output pin-2.14-71313-gcc.4.4.7-linux
cd ..


#Libxml2
$FILE="/usr/include/libxml2"
if [ -d "$FILE" ]; then
    echo "$FILE Path (libxml2) exist"

else
	echo -n "Do you want to install Libxml2 uisng apt-get(y/n)? "
	read answer
	answer="Y"
		if [ "$answer" != "${answer#[Yy]}" ] ;then
	    		echo Yes
	    		sudo apt-get install libxml2-dev
		else
	   		echo No		 
		fi
fi

#rm -rf primesim
#sudo apt-get install libxml2-dev
if [ ! -d "primesim" ]; then
	echo "Download primesim"
	git clone https://github.com/dananjayamahesh/primesim.git
	
fi

cd primesim
git checkout dimp

#Make sure all the paths are correct
#########################################################
source env.sh
#########################################################
source env.sh

if [ $? -eq 0 ]; then
    echo "Environment is setup"
else
    echo "Please check the primesim/env.sh"
	exit 1
fi

#Please check your libxml2
echo "Building Prime and LRP"
make -B
echo "Building Prime and LRP Finished"
echo "Building Benchmarks"

export PBENCH_PATH="$LRP/pbench/syncbench"
pbench_path=pbench/syncbench
cd pbench/syncbench
make -B
cd ../..
echo "Finished Building Benchmarks: PBench"
echo "Build-Complete"
echo "If no errors You are ready to run the experiments. See README.md inside primesim"

echo "You are in the primesim path. run [sh run_all.sh simple-test]"
#RUN a simple test
#sh run_all.sh simple-test