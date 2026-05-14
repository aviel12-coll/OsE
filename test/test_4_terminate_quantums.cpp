#include "uthreads.h"
#include <iostream>
#include <unistd.h> // for usleep

bool a_started = false;
bool b_started = false;
int a_tid = -1;
int b_tid = -1;

void thread_a() {
    a_started = true;
    // Consume some quantums
    for (volatile int i = 0; i < 10000000; i++) {
        // busy wait to consume time
    }
    // This should not reach here if terminated externally
    std::cout << "Thread A finished unexpectedly" << std::endl;
}

void thread_b() {
    b_started = true;
    // Wait for A to start
    while (!a_started) {
        usleep(10000); // 10ms
    }
    // Sleep for 2 quantums
    uthread_sleep(2);
    // Terminate A
    int res = uthread_terminate(a_tid);
    if (res != 0) {
        std::cout << "Failed to terminate thread A" << std::endl;
        exit(1);
    }
    // Check quantums for A
    int a_quantums = uthread_get_quantums(a_tid);
    if (a_quantums <= 0) {
        std::cout << "Thread A has invalid quantums: " << a_quantums << std::endl;
        exit(1);
    }
    std::cout << "Thread A ran for " << a_quantums << " quantums" << std::endl;
    // Check total quantums
    int total = uthread_get_total_quantums();
    if (total <= 0) {
        std::cout << "Invalid total quantums: " << total << std::endl;
        exit(1);
    }
    std::cout << "Total quantums: " << total << std::endl;
}

void self_terminate_thread() {
    // Consume some quantums
    for (volatile int i = 0; i < 5000000; i++) {
        // busy wait
    }
    int my_tid = uthread_get_tid();
    int my_quantums = uthread_get_quantums(my_tid);
    std::cout << "Self-terminating thread ran for " << my_quantums << " quantums before terminating" << std::endl;
    uthread_terminate(my_tid);
}

int main() {
    int result = uthread_init(1000000); // 1 second quantum
    if (result != 0) {
        std::cout << "uthread_init failed" << std::endl;
        return 1;
    }

    // Test external termination
    a_tid = uthread_spawn(thread_a);
    if (a_tid == -1) {
        std::cout << "Failed to spawn thread A" << std::endl;
        return 1;
    }
    b_tid = uthread_spawn(thread_b);
    if (b_tid == -1) {
        std::cout << "Failed to spawn thread B" << std::endl;
        return 1;
    }

    // Wait for B to finish
    while (b_started) {
        usleep(100000); // 100ms
    }
    // Wait a bit more to ensure termination
    usleep(2000000); // 2 seconds

    // Now test self-termination
    int c_tid = uthread_spawn(self_terminate_thread);
    if (c_tid == -1) {
        std::cout << "Failed to spawn self-terminating thread" << std::endl;
        return 1;
    }

    // Wait for it to terminate
    usleep(3000000); // 3 seconds

    // Check if it terminated
    int c_quantums = uthread_get_quantums(c_tid);
    if (c_quantums != -1) {
        std::cout << "Self-terminating thread still exists with quantums: " << c_quantums << std::endl;
        return 1;
    }

    std::cout << "Test passed" << std::endl;
    return 0;
}