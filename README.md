# TachyonX: Ultra-Low Latency Order Matching Engine

> A deterministic, sub-microsecond Limit Order Book (LOB) matching engine built in C++17. Engineered for zero-allocation steady-state execution, strict cache-locality, and complete operating system kernel bypass.

[![Language](https://img.shields.io/badge/Language-C++17-blue.svg)](#) 
[![Architecture](https://img.shields.io/badge/Architecture-x86__64-lightgrey.svg)](#)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](#)

---

## ⚙ Core Architecture & Design Philosophy

The system is designed from the ground up to minimize execution latency by avoiding system calls, dynamic memory allocation, and CPU cache misses during critical path execution.

► **Kernel Bypass & Custom Memory Management**
Standard OS heap allocators (`malloc`/`new`) introduce unacceptable latency spikes. TachyonX bypasses the OS via a custom `O(1)` contiguous free-list stack (`MemoryPool.hpp`) initialized at startup. 

► **Hardware Cache Optimization (L1/L2)**
Data structures are aggressively padded and cache-line aligned to prevent False Sharing across CPU cores. Structs utilize relative pointer compression to maximize L1/L2 hardware cache utility.

► **Lock-Free Concurrency**
Order ingress is handled via a Single-Producer Single-Consumer (SPSC) ring buffer (`SPSCLockFreeRingBuffer.hpp`), entirely eliminating the need for slow POSIX mutexes or locking mechanisms.

► **OS & Thread Determinism**
Strict execution determinism is enforced via thread affinity pinning, isolating the matching thread to a dedicated CPU core to prevent OS context switching.

---

## ■ Algorithmic Data Structures

TachyonX implements Price-Time Priority matching with absolute `O(1)` time complexity for all critical path operations (Add, Cancel, Execute).

* **Order Dictionary:** A pre-allocated hash map for `O(1)` order lookups.
* **Price Levels (`PriceLevel.hpp`):** Maintained via an intrusive doubly-linked list.
* **Order Queues:** Each price level maintains its own intrusive queue of orders, ensuring that appending or canceling an order requires zero allocation and constant time.

---

## ◇ Project Structure

```text
├── benchmark/
│   ├── HdrBenchmark.cpp           # High Dynamic Range latency profiling
│   └── WinHdrBenchmark.cpp        # Windows-specific latency benchmarking
├── build/                         # Compiled object files and binaries
│   ├── engine                     # Main executable
│   ├── Main.o
│   └── OrderBook.o
├── data/
│   └── timing.png                 # Latency distribution visualization
├── include/
│   ├── MarketDataParser.hpp       # Binary/FIX market data parser
│   ├── MemoryPool.hpp             # Custom O(1) contiguous allocator
│   ├── OrderBook.hpp              # Core LOB matching logic
│   ├── PriceLevel.hpp             # Intrusive price level tracking
│   ├── SPSCLockFreeRingBuffer.hpp # Lock-free concurrency queue
│   ├── Telemetry.hpp              # Performance metrics collection
│   └── Types.hpp                  # Core data structures and aliases
├── src/
│   ├── Main.cpp                   # Application entry point & thread pinning
│   └── OrderBook.cpp              # Order book implementation
├── Makefile                       # Build configuration
├── market_traffic.dat             # Sample market data ingress payload
└── trades.csv                     # Output execution log

```

## ⚡ Build Instructions

A modern C++ compiler (GCC 9+ or Clang 10+) supporting C++17 is required.

```bash
git clone https://github.com/Krishna-1107/TachyonX.git
cd TachyonX
```

**2. Build via Make**
*Note: The Makefile defaults to aggressive compiler optimizations `-O3` required for accurate latency profiling.*
```bash
make clean
make
```

**3. Run the Engine**
```bash
make run
```

---

## ◆ Developed by

**Krishna Parashar** Indian Institute of Technology (IIT) Guwahati, Computer Science and Engineering
* **GitHub:** [Krishna-1107](https://github.com/Krishna-1107)
* **Codeforces:** [Krishna_1107](https://codeforces.com/profile/Krishna_1107)
* **Email:** krishna1107p@gmail.com | k.parashar@iitg.ac.in


