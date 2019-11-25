echo "Building Prime and LRP"
make -B
echo "Building Prime and LRP Finished: PBench"
echo "Building Benchmarks"
pbench_path=pbench/syncbench
cd pbench/syncbench
make -B
cd ../..
echo "Finished Building Benchmarks: PBench"
echo "Build-Complete"