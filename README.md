# L1 cache measurement

## Result

Measured on Apple M3 Pro running macOS:

| Characteristic | Result |
| --- | ---: |
| L1 data cache capacity | 128 KiB |
| L1 line size | 64 bytes |
| Associativity | 8 ways |

## Build and run

Requires CMake 3.16 or later and a C++20 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/l1-cache
```

To write latency diagnostics for N = 6 through 12 to a file:

```sh
./build/l1-cache --debug 2> latencies.csv
```
