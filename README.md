# Virtual Memory (OS Exercise)

Our implementation of exercise 3 in the Operating Systems course — a
simulation of a virtual memory system with hierarchical page tables, page
faults and page eviction.

## What this is

We're simulating an MMU on top of a small, fixed physical memory. The
"hardware" (`PhysicalMemory.cpp`) just gives us a RAM array and a swap file
and lets us read/write words at a physical address, or evict/restore whole
pages. On top of that we implemented the actual virtual memory logic
(`VirtualMemory.cpp`): translating a virtual address into a physical one by
walking a tree of page tables, and dealing with what happens when a page
isn't in RAM yet.

Basically, every `VMread`/`VMwrite` call walks down `TABLES_DEPTH` levels of
tables (this number comes out of the address width constants, we didn't hard
code it). If a table doesn't exist yet or the page isn't loaded, that's a
page fault, and we need to find it a frame:

1. if there's a table sitting around with all zero entries, we reuse it,
2. otherwise if there's still an untouched frame, we take that,
3. otherwise RAM is full and we have to evict something.

For eviction we go with the algorithm from the exercise PDF (also in
`example/`): find the resident page whose page number has the largest
**cyclical distance** from the page we're trying to bring in, and kick that
one out (ties broken by smaller page number). We write it to the swap file
via `PMevict` and disconnect it from its parent table.

`VMgetMapping` is just a "peek" — it walks the tree like everything else but
never allocates/restores/evicts, it just tells you where (if anywhere) a page
currently lives.

## Files

```
src/
  MemoryConstants.h   address width constants + everything derived from them
                       (PAGE_SIZE, NUM_FRAMES, NUM_PAGES, TABLES_DEPTH...)
  PhysicalMemory.h/.cpp   the "hardware" layer we were given (RAM + swap file)
  VirtualMemory.h/.cpp    our code: table walking, page faults, eviction

tests/
  test0_sanity.cpp / .txt   the sanity test we got, with expected output

example/
  overrides.h              small address widths, used to actually trigger
                           eviction without needing huge tests
  Algorithm Example.pdf   the example from the course staff we followed for
                           the eviction algorithm
```

## Config

All the sizes (offset width, physical/virtual address width) live in
`MemoryConstants.h` and can be overridden with `-D` flags or by putting an
`overrides.h` on the include path — same trick as in `example/`. Everything
else (`PAGE_SIZE`, `NUM_FRAMES`, `NUM_PAGES`, `TABLES_DEPTH`) is computed from
those three, so we never had to touch it by hand while testing.

## Compiling / running

No makefile, just compile the sources together:

```bash
g++ -std=c++11 -Isrc src/VirtualMemory.cpp src/PhysicalMemory.cpp \
    tests/test0_sanity.cpp -o test0
./test0
```

And to run it with the tiny address space from `example/` (good for actually
forcing eviction to happen), add it to the include path so its
`overrides.h` gets picked up first:

```bash
g++ -std=c++11 -Iexample -Isrc src/VirtualMemory.cpp src/PhysicalMemory.cpp \
    tests/test0_sanity.cpp -o test0
./test0
```

Expected output for the sanity test is in `tests/test0_sanity.txt`.
