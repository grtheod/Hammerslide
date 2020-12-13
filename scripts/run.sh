#!/bin/bash

cd ..

echo "Building code..."

rm -r build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make

echo "Start running tests..."

./tests/hammerslide-test

echo "Start running benchmarks..."

./hammerslide-bench --duration 5000

echo "Done."
echo "Bye."

exit 0
