#include "uthreads.h"
#include <iostream>
#include <cstdlib>

bool a_started = false;
bool a_completed = false;
int a_progress = 0;
int a_tid = -1;

void thread_a() {
    a_started = true;

    // First quantum of work.
    a_progress++;
    if (uthread_sleep(0) != 0) {
        std::cout << "Thread A failed to yield after first quantum" << std::endl;
        exit(1);
    }

    // Second quantum of work.
    a_progress++;
    if (uthread_sleep(0) != 0) {
        std::cout << "Thread A failed to yield after second quantum" << std::endl;
        exit(1);
    }

    // Third quantum of work.
    a_progress++;
    a_completed = true;

    // Keep the thread alive so the main thread can terminate it cleanly.
    while (true) {
        if (uthread_sleep(0) != 0) {
            std::cout << "Thread A failed to yield in idle loop" << std::endl;
            exit(1);
        }
    }
}

int main() {
    if (uthread_init(1000000) != 0) {
        std::cout << "uthread_init failed" << std::endl;
        return 1;
    }

    a_tid = uthread_spawn(thread_a);
    if (a_tid == -1) {
        std::cout << "Failed to spawn thread A" << std::endl;
        return 1;
    }

    // Let thread A start and run at least one quantum.
    while (!a_started || a_progress < 1) {
        if (uthread_sleep(0) != 0) {
            std::cout << "Main failed to yield while waiting for thread A" << std::endl;
            return 1;
        }
    }

    int before_block = a_progress;
    if (before_block != 1) {
        std::cout << "Thread A progressed too far before block: " << before_block << std::endl;
        return 1;
    }

    if (uthread_block(a_tid) != 0) {
        std::cout << "Failed to block thread A" << std::endl;
        return 1;
    }

    // Thread A should not make progress while blocked.
    for (int i = 0; i < 3; ++i) {
        if (uthread_sleep(0) != 0) {
            std::cout << "Main failed to yield while thread A is blocked" << std::endl;
            return 1;
        }
        if (a_progress != before_block) {
            std::cout << "Thread A advanced while blocked: " << a_progress << std::endl;
            return 1;
        }
    }

    if (uthread_resume(a_tid) != 0) {
        std::cout << "Failed to resume thread A" << std::endl;
        return 1;
    }

    // Allow thread A to complete its remaining work.
    while (!a_completed) {
        if (uthread_sleep(0) != 0) {
            std::cout << "Main failed to yield while waiting for thread A to complete" << std::endl;
            return 1;
        }
    }

    if (a_progress != 3) {
        std::cout << "Thread A did not complete expected work after resume: " << a_progress << std::endl;
        return 1;
    }

    if (uthread_terminate(a_tid) != 0) {
        std::cout << "Failed to terminate thread A" << std::endl;
        return 1;
    }

    std::cout << "Blocked/resume test passed" << std::endl;
    return 0;
}
