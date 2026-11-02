# L1 cache measurement

## Result

### macOS

Measured on Apple M3 Pro running macOS:

| Characteristic | Result |
| --- | ---: |
| L1 data cache capacity | 128 KiB |
| L1 line size | 64 bytes |
| Associativity | 8 ways |

### Linux VPS

KVM, Intel Xeon E5-2640, 3 vCPUs, 1 GiB RAM.
Run with `taskset -c 0`.

| Characteristic | Original run | Two-threshold replay |
| --- | ---: | ---: |
| L1D capacity | 36 KiB | 32 KiB |
| L1 line size | 64 bytes | 64 bytes |
| Associativity | 9 ways | 8 ways |

The original detector missed the initial slowdown at 9 nodes because it
was below the 1.5 threshold. It detected the slowdown at 10 nodes and
reported 9 ways, giving 36 KiB instead of 32 KiB.

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
