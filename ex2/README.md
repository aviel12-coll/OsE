# ex2: Multi-threaded MapReduce Framework

A small MapReduce library. The client supplies `map` and `reduce` (by implementing `MapReduceClient`),
and the framework runs the job on N worker threads.

## Pipeline

```
input (K1,V1) ──► MAP ──► SORT ──► [barrier] ──► SHUFFLE ──► [barrier] ──► REDUCE ──► output (K3,V3)
                 all threads  per thread        thread 0               all threads
```

1. **Map:** threads take input pairs with a shared `std::atomic<int>` index (`fetch_add`), so each pair
   goes to exactly one thread without a lock. Each thread writes `(K2,V2)` to its own intermediate vector,
   so emitting needs no lock either.
2. **Sort:** each thread sorts its own vector by key.
3. **Shuffle:** after a `std::barrier`, thread 0 merges all the intermediate vectors and sorts them by key.
4. **Reduce:** after another barrier, threads take one whole key group at a time under a mutex,
   call `reduce` on it, and write `(K3,V3)` to the output.

## Progress tracking

`getState()` reports the current stage (UNDEFINED / MAP / SHUFFLE / REDUCE) and the percentage done.
The stage, the total and the processed count are packed into **one 64-bit atomic** (2 bits for the stage
and 31 bits for each counter). A reader always sees a consistent snapshot, without taking a lock.

## Build and run

```bash
cd ex2
make                                  # MapReduceJob.a + tests + SampleClient
./tests/test01_word_count             # compare with tests/test01_word_count.txt
./tests/test02_is_done_after_wait
./SampleClient
```

More tests are in `tests/` (an advanced state check, a complex client and a benchmark), and
`tests/updated_tests_ex2_OS/` has the course's updated tests. Build them the same way:

```bash
g++ -std=c++20 -pthread -I. tests/test05_benchmark.cpp MapReduceJob.a -o tests/test05
```

Requires C++20 (`std::barrier`).
