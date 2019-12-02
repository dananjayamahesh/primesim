# Installing Open MPI correctly

Most of the errors in the simulator can be caused by the MPI packages. You need libmpi installed correctly. Best way to resolve this is to reinstall the open MPI correctly set the libmpi.so path to env.sh.


Build Open MPI from Source
--------------------------
* Download the required version of openMPI [here](https://download.open-mpi.org/release/open-mpi/v3.0/openmpi-3.0.0.tar.gz)
* This [link](https://likymice.wordpress.com/2015/03/13/install-open-mpi-in-ubuntu-14-04-13-10/) might be helpful
* tar -xvf openmpi-3.0.0.tar.gz
* cd openmpi-3.0.0
* ./configure --prefix=/usr/local --enable-mpi-thread-multiple
	* --prefix is the path you want to install ompi.
* make all install
	* You may need sudo permission
* Setup a path to new opn mpi. You can add follwoing two paths to the env.sh as well.
	* export PATH="$PATH:/usr/local/bin"
	* export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/local/lib/"
* Setup the OPENMPI_LIB_PATH in the env.sh
 	* /usr/local/lib/libmpi.so

Build Open MPI using apt-get
-----------------------------
* sudo apt-get install openmpi-bin openmpi-common libopenmpi1.3 libopenmpi-dbg libopenmpi-dev


Locate the libmpi.so or similar file
------------------------------------
* please find the libmpi.so file 