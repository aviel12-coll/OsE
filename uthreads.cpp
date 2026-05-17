#include "uthreads.h"
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <iostream>
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <sys/time.h>
#ifdef __x86_64__
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7



void block_signals() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);
}

void unblock_signals() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}


enum ThreadState {
    RUNNING,
    READY,
    BLOCKED
};
int quantum_usecs;
class Thread {
public:
    int tid;
    thread_entry_point entry_point;
    char* stack;
    ThreadState state;
    int quantums;
    sigjmp_buf env;
    int quantums_until_wake_up;
    bool is_blocked_by_user;
    Thread(int tid, thread_entry_point entry_point) : tid(tid), entry_point(entry_point), state(READY), quantums(0), quantums_until_wake_up(0), is_blocked_by_user(false) {
        if ( tid == 0) {
            // main thread does not need a stack
            stack = nullptr;
        } else {
            stack = new char[STACK_SIZE];
        }
    }
    ~Thread() {
        delete[] stack;
    }
    int save_context() {
        std::cout << "save_context tid=" << tid  << std::endl;
        return sigsetjmp(env, 0);
    }
    int load_context() {
        siglongjmp(env, 1);
    }
};


// Forward declarations
int init_timer(int quantum_usecs);
int reset_timer();
void stub_func();

Thread* running_thread = nullptr;
Thread* pending_delete = nullptr;
std::deque<Thread*> ready_threads;
std::map<int, Thread*> all_threads;
std::map<int, Thread*> sleeping_threads; // tid -> quantums until wake up

std::set<int> free_tids;

int total_quantums = 0;

address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}
#else
typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}
#endif


  static void switch_to_next_thread(ThreadState target) {
    block_signals();
    if (pending_delete != nullptr) {
        delete pending_delete;
        pending_delete = nullptr;
    }
    int ret_val = sigsetjmp(running_thread->env, 0);
    
    if (ret_val != 0) {
        unblock_signals();
        return;
    }
    // If the current thread is still RUNNING, move it to the READY state and add it to the end of the READY queue
    running_thread->state = target;

    // If the current thread is still RUNNING, move it to the READY state and add it to the end of the READY queue
    if (target == READY) {
        ready_threads.push_back(running_thread);
    }
    
    if (ready_threads.empty()) {
        std::cerr << "thread library error: no ready threads available" << std::endl;
        unblock_signals();
        return;
    }
    
    running_thread = ready_threads.front();
    ready_threads.pop_front();

    running_thread->state = RUNNING;
    running_thread->quantums++;
    total_quantums++;


    reset_timer();

    siglongjmp(running_thread->env, 1);
}


// handle the timer signal by switching to the next thread

void timer_handler(int sig)
{

    // Decrease the quantums until wake up for all sleeping threads
    for (auto it = sleeping_threads.begin(); it != sleeping_threads.end();) {
        Thread* thread = it->second;
        thread->quantums_until_wake_up--;
        if (thread->quantums_until_wake_up <= 0) {

            if (thread->is_blocked_by_user == false) {
                thread->state = READY;
                ready_threads.push_back(thread);
            }
            it = sleeping_threads.erase(it);
        } else {
            ++it;
        }
    }
    switch_to_next_thread(READY);
    

    
}








/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to 
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs) {
    if (quantum_usecs <= 0) {
        std::cerr << "thread library error: quantum_usecs must be positive" << std::endl;
        return -1;
    }
    ::quantum_usecs = quantum_usecs;
    Thread* main_thread = new Thread(0, nullptr);
    main_thread->state = RUNNING;
    running_thread = main_thread;
    total_quantums = 1;
    all_threads[0] = main_thread;
    for (int tid = 0; tid < MAX_THREAD_NUM; ++tid) {
        free_tids.insert(tid);
    }
    free_tids.erase(0);
    
    if (init_timer(quantum_usecs) < 0) {
        std::cerr << "thread library error: timer initialization failed" << std::endl;
        return -1;
    }
    
    return 0;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/

// Wrapper function that calls entry_point and terminates the thread
void stub_func() {
    unblock_signals();
    running_thread->entry_point();
    uthread_terminate(uthread_get_tid());
}

static void init_thread_context(Thread* thread) {
    address_t sp = (address_t) thread->stack + STACK_SIZE - sizeof(address_t);
    // Point to stub_func instead of entry_point directly
    address_t pc = (address_t) stub_func;

    sigsetjmp(thread->env, 0);
    thread->env->__jmpbuf[JB_SP] = translate_address(sp);
    thread->env->__jmpbuf[JB_PC] = translate_address(pc);
    sigemptyset(&thread->env->__saved_mask);
}

int uthread_spawn(thread_entry_point entry_point) {
    block_signals() ;   
    if ( all_threads.size() >= MAX_THREAD_NUM) {
        std::cerr << "thread library error: maximum number of threads reached" << std::endl;
        unblock_signals() ;   
        return -1;
    }
    int tid = *free_tids.begin();
    free_tids.erase(free_tids.begin());
    Thread* new_thread = new Thread(tid, entry_point);
    init_thread_context(new_thread);
    ready_threads.push_back(new_thread);
    all_threads[tid] = new_thread;
    unblock_signals();
    return tid;
}


/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){
    block_signals();
    auto it = all_threads.find(tid);
    if (it == all_threads.end()) {
        unblock_signals();
        return -1;
    }

    if (tid == 0) {
        for (auto& pair : all_threads) {
            if (pair.second != running_thread) {
                delete pair.second;
            }
        }
        all_threads.clear();
        ready_threads.clear();
        free_tids.clear();
        exit(0);
    }


    Thread* thread_to_terminate = it->second;
    bool self_terminate = (thread_to_terminate == running_thread);
    if (!self_terminate && thread_to_terminate->state == READY) {
        auto ready_it = std::find(ready_threads.begin(), ready_threads.end(), thread_to_terminate);
        if (ready_it != ready_threads.end()) {
            ready_threads.erase(ready_it);
        }
    }

    free_tids.insert(tid);
    all_threads.erase(it);

    if (self_terminate) {
        if (ready_threads.empty()) {
            delete thread_to_terminate;
            exit(0);
        }
        Thread* next_thread = ready_threads.front();
        ready_threads.pop_front();

        reset_timer();

        next_thread->state = RUNNING;
        next_thread->quantums++;
        total_quantums++;
        running_thread = next_thread;
        pending_delete = thread_to_terminate;
        siglongjmp(next_thread->env, 1);
        unblock_signals();
        return 0;
    }

    delete thread_to_terminate;
    unblock_signals();
    return 0;
}



int reset_timer() {
    struct itimerval timer;
    timer.it_value.tv_sec = quantum_usecs / 1000000;
    timer.it_value.tv_usec = quantum_usecs % 1000000;
    timer.it_interval.tv_sec = quantum_usecs / 1000000;
    timer.it_interval.tv_usec = quantum_usecs % 1000000;

    if (setitimer(ITIMER_VIRTUAL, &timer, NULL) < 0) {
        std::cerr << "system error: setitimer failed" << std::endl;
        return -1;
    }
    return 0;
}



/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is *not* considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid) {
    if (tid == 0 ||all_threads.find(tid) == all_threads.end()) {
        std::cerr << "thread library error: cannot block main thread" << std::endl;
        return -1;
    }
    Thread *thread_to_block = all_threads[tid];
    if (thread_to_block->state == BLOCKED) {
        thread_to_block->is_blocked_by_user=true;
        return 0;
    }
    if (running_thread->tid == tid) {
        thread_to_block->state = BLOCKED;
        
        switch_to_next_thread(BLOCKED);
        
    } else if (thread_to_block->state == READY) {
        thread_to_block->state = BLOCKED;
        thread_to_block->is_blocked_by_user= true;
        ready_threads.erase(std::remove(ready_threads.begin(), ready_threads.end(), thread_to_block), ready_threads.end());
        
    }
    return 0;
}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * When a thread transition to the READY state it is placed at the end of the READY queue.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
   if (all_threads.find(tid) == all_threads.end()) {
        std::cerr << "thread library error: thread with ID " << tid << " does not exist" << std::endl;
        return -1;
    }
    Thread *thread_to_resume = all_threads[tid];
    if (thread_to_resume->state == BLOCKED) {
        
        thread_to_resume->is_blocked_by_user = false;

        if (sleeping_threads.find(tid) != sleeping_threads.end()) {
            return 0;
        }
        thread_to_resume->state = READY;
        ready_threads.push_back(thread_to_resume);
     
    }
    return 0;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up 
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * A call with num_quantums == 0 will immediately stop the thread and move it to the back of the execution queue.
 * 
 * It is considered an error if the main thread (tid == 0) calls this function with num_quantums != 0.
 *
 * @return On success, return 0. On failure, return -1.
*/
// static int save_current_thread_context() {
//     int ret = running_thread->save_context();
//     std::cerr << "save_context tid=" << running_thread->tid << " ret=" << ret << "\n";
//     return ret;
// }



int uthread_sleep(int num_quantums) {
    block_signals();
    // int ret_val = sigsetjmp(running_thread->env, 1);

    if (running_thread->tid == 0 && num_quantums != 0) {
        std::cerr << "thread library error: main thread cannot sleep for more than 0 quantums" << std::endl;
        unblock_signals();
        return -1;
    }
    if (num_quantums == 0) {
        switch_to_next_thread(READY);
        unblock_signals();
        return 0;
    }
    running_thread->quantums_until_wake_up = num_quantums;
    sleeping_threads[running_thread->tid] = running_thread;
    switch_to_next_thread(BLOCKED);

    unblock_signals();
    return 0;
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid() {
    return running_thread->tid;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums() {
    return total_quantums;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid) {
   if (all_threads.find(tid) == all_threads.end()) {
       std::cerr << "thread library error: thread with ID " << tid << " does not exist" << std::endl;
       return -1;
   }
   return all_threads[tid]->quantums;
}


int init_timer(int quantum_usecs)
{
    struct sigaction sa = {0};
    struct itimerval timer;

    // Register the timer_handler as the handler for SIGVTALRM
    sa.sa_handler = &timer_handler;
    
    /*
     * Initialize the signal mask. We block SIGVTALRM during the execution
     * of the handler to avoid nested signals (signal races).
     */
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGVTALRM); 
    
    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
    {
        return -1; // Return error if sigaction fails
    }

    /*
     * Configure the timer to expire after the given quantum.
     * We divide by 1,000,000 to get seconds and use modulo for microseconds.
     */
    
    // Initial expiration time
    timer.it_value.tv_sec = quantum_usecs / 1000000;
    timer.it_value.tv_usec = quantum_usecs % 1000000;

    // Recurring expiration time (for subsequent quantums)
    timer.it_interval.tv_sec = quantum_usecs / 1000000;
    timer.it_interval.tv_usec = quantum_usecs % 1000000;

    /*
     * Start a virtual timer (ITIMER_VIRTUAL). 
     * It counts down only when the process is executing in user mode.
     */
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
    {
        return -1; // Return error if setitimer fails
    }

    return 0;
}

