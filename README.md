# Virtual Memory Simulator

A C++ simulation of a hierarchical (multi-level) page table memory management
unit (MMU), including page fault handling and page eviction.

The simulator exposes a small **virtual memory (VM)** API — `VMread` /
`VMwrite` — backed by a fixed-size **physical memory (PM)** consisting of RAM
frames and a swap file. Address translation walks a tree of page tables of
configurable depth, allocating frames on demand and evicting pages when RAM
is full.

## How it works

- Virtual addresses are split into a chain of table indices plus a page
  offset. The number of levels (`TABLES_DEPTH`) is derived automatically from
  `VIRTUAL_ADDRESS_WIDTH`, `PHYSICAL_ADDRESS_WIDTH`, and `OFFSET_WIDTH`.
- `VMread` / `VMwrite` walk the table tree one level at a time via
  `PMread`/`PMwrite`, creating any missing intermediate tables along the way.
- When a page fault occurs, a free frame is located by, in order:
  1. reusing an already-allocated table frame that is entirely empty,
  2. taking the next never-used frame, or
  3. evicting a resident data page.
- Eviction picks the page whose virtual page number has the **greatest
  cyclical distance** from the page currently being faulted in (with the
  lower page index as a tie-breaker), writes it out via `PMevict`, and frees
  its frame. Restoring a previously evicted page happens via `PMrestore`.
- `VMgetMapping` performs a read-only walk of the table tree to report the
  physical frame a virtual page currently maps to, without allocating or
  restoring anything.

See [`example/Algorithm Example.pdf`](example/Algorithm%20Example.pdf) for a
worked walkthrough of the algorithm.

## Project layout

```
src/
  MemoryConstants.h   Address-width constants and derived sizes (PAGE_SIZE,
                       NUM_FRAMES, NUM_PAGES, TABLES_DEPTH, ...)
  PhysicalMemory.h     PM interface: PMread, PMwrite, PMevict, PMrestore
  PhysicalMemory.cpp   PM implementation (RAM array + in-memory swap file)
  VirtualMemory.h      VM interface: VMinitialize, VMread, VMwrite,
                       VMgetMapping
  VirtualMemory.cpp    VM implementation: table walking, page-fault
                       handling, and eviction

tests/
  test0_sanity.cpp     Minimal end-to-end sanity check
  test0_sanity.txt     Expected output for the sanity check

example/
  overrides.h          Sample address-width overrides used by the example
  Algorithm Example.pdf  Worked example of the eviction algorithm
```

## Configuration

Address widths are defined in [`src/MemoryConstants.h`](src/MemoryConstants.h)
and can be overridden by defining `OFFSET_WIDTH`, `PHYSICAL_ADDRESS_WIDTH`,
and `VIRTUAL_ADDRESS_WIDTH` before including it — either via compiler flags
(`-D...`) or by providing an `overrides.h` header on the include path (see
[`example/overrides.h`](example/overrides.h)).

## Building and running

The project has no build system; compile the sources directly with any
C++11-or-later compiler. To run the sanity test with the default constants:

```bash
g++ -std=c++11 -Isrc src/VirtualMemory.cpp src/PhysicalMemory.cpp \
    tests/test0_sanity.cpp -o test0
./test0
```

To run it against the smaller address widths used in the example (also
useful for exercising eviction with a tiny address space), add the
`example/` directory to the include path so `overrides.h` is picked up:

```bash
g++ -std=c++11 -Iexample -Isrc src/VirtualMemory.cpp src/PhysicalMemory.cpp \
    tests/test0_sanity.cpp -o test0
./test0
```

Expected output is in [`tests/test0_sanity.txt`](tests/test0_sanity.txt).
