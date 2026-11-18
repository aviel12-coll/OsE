# uthreads: A User-Level Threads Library in C++

A static library that runs many threads inside a single Linux process, with no kernel threads involved.
The library itself does the scheduling, context switching and thread bookkeeping, using only signals,
timers and `sigsetjmp`/`siglongjmp`.

The code is in [`ex1/`](ex1/). It was built as part of the Operating Systems course at the Hebrew University of Jerusalem.

## API ([`uthreads.h`](ex1/uthreads.h))

| Function | What it does |
|----------|--------------|
| `uthread_init(quantum_usecs)` | Initializes the library and starts the virtual timer. The main thread is tid 0 |
| `uthread_spawn(entry_point)` | Creates a thread with its own 100 KB stack and puts it at the end of the READY queue |
| `uthread_terminate(tid)` | Terminates a thread and frees its resources (terminating tid 0 ends the process) |
| `uthread_block(tid)` / `uthread_resume(tid)` | Moves a thread to BLOCKED and back to READY |
| `uthread_sleep(num_quantums)` | Puts the running thread to sleep for N quantums |
| `uthread_get_tid()`, `uthread_get_total_quantums()`, `uthread_get_quantums(tid)` | Introspection |

Up to `MAX_THREAD_NUM` (100) threads. A new thread always gets the smallest free tid.

## How it works

- **Preemption:** `setitimer(ITIMER_VIRTUAL)` fires `SIGVTALRM` every quantum. The signal handler moves the
  running thread to the back of the READY queue and switches to the next thread (round-robin).
- **Context switch:** each thread keeps a `sigjmp_buf`. The scheduler saves the current thread with `sigsetjmp`
  and jumps to the next one with `siglongjmp`.
- **Creating a thread:** the library allocates a stack, then writes the stack pointer and the entry point into
  the thread's `sigjmp_buf` directly (translated with the glibc pointer mangling, `translate_address`).
  The first jump into that buffer starts the thread on its own stack.
- **Critical sections:** library calls block `SIGVTALRM` with `sigprocmask`, so a timer tick can't interrupt
  an update to the scheduler's data structures.
- **Data structures:** a `std::deque` READY queue, a `std::map` from tid to thread, a map of sleeping threads
  with the quantums left until they wake up, and a `std::set` of free tids.

## Build and run

```bash
cd ex1
make                 # builds build/test_basic_scheduler
make run-basic

# The course tests: each test_*.cpp has an expected-output test_*.txt
python3 test/run_tests.py
```

## Demos

[`ex1/games/`](ex1/games/) has two terminal games, and all of their "concurrency" runs on this library:
- `ants.cpp`: ants that move across a board, each one a uthread. Ants are frozen and released with `uthread_block`/`uthread_resume`.
- `thread_tron.cpp`: Tron light-cycles. The player and the enemy bikes are separate uthreads.

[`ex1/demos/`](ex1/demos/) has small standalone programs for the building blocks: `setitimer`,
`sigsetjmp`/`siglongjmp` and a `SIGINT` handler.

---

## Other exercises in this repository

| Folder | Topic |
|--------|-------|
| [`ex2/`](ex2/) | Multi-threaded MapReduce framework: map → sort → shuffle → reduce with `std::thread`, atomics and barriers |
| [`ex3/`](ex3/) | Virtual memory with hierarchical page tables (in progress) |
