cmake -S . -B build
cmake --build build


./build/sched 1 < tests/t1.iloc
./build/sched 1 < tests/t2.iloc
./build/sched 1 < tests/t3.iloc


./build/sched 2 < tests/t1.iloc
./build/sched 4 < tests/t1.iloc