#include <iostream>
#include <cstdio>
#include "../uthreads.h"

// משתנה גלובלי לעקוב אחרי הספירה של thread 1
int thread1_count = 0;

// פונקציה לת'רד השני - לולאה שעוצרת לפי מספר צעדים
void heavy_computation() {
    printf("Thread 1 (TID: %d) started\n", uthread_get_tid());
    
    for (int i = 0; i < 10000000; i++) {
        thread1_count++;
    }
    
    printf("Thread 1 (TID: %d) finished computation\n", uthread_get_tid());
}

int main() {
    printf("=== Basic Scheduler Test ===\n\n");
    
    // 1. אתחול הספרייה עם קוונטום של 100000 מיקרו-שניות
    printf("Main: Initializing thread library with 100ms quantum...\n");
    if (uthread_init(100000) < 0) {
        fprintf(stderr, "ERROR: Initialization failed\n");
        return 1;
    }
    printf("Main: Library initialized. Total quantums: %d\n", uthread_get_total_quantums());

    // 2. יצירת ת'רד חדש
    printf("\nMain: Spawning thread 1...\n");
    int tid = uthread_spawn(heavy_computation);
    if (tid < 0) {
        fprintf(stderr, "ERROR: Spawn failed\n");
        return 1;
    }
    printf("Main: Thread 1 spawned with TID: %d\n", tid);

    // 3. לולאה שבודקת את התקדמות הזמן
    printf("\nMain: Waiting for 5 quantums to pass...\n");
    printf("Main: This is the critical test - if context switching doesn't work,\n");
    printf("      the main thread will never regain control from thread 1.\n\n");
    
    int last_quantum = 1;
    while (uthread_get_total_quantums() < 5) {
        int current_quantum = uthread_get_total_quantums();
        if (current_quantum != last_quantum) {
            printf("Main: Quantum #%d passed. Thread 1 computation count: %d\n", 
                   current_quantum, thread1_count);
            last_quantum = current_quantum;
        }
    }

    // 4. הדפסת תוצאות
    printf("\n=== TEST SUCCESS! ===\n");
    printf("Total quantums: %d\n", uthread_get_total_quantums());
    printf("Main thread managed to regain control from thread 1.\n");
    printf("Thread 1 did %d iterations before being preempted.\n", thread1_count);
    printf("Main thread (TID: %d) quantums: %d\n", uthread_get_tid(), uthread_get_quantums(0));
    
    // Note: Thread 1 already terminated itself via stub_func, so we can't query its quantums
    printf("(Thread 1 has already terminated)\n");
    
    // 5. סיום התוכנית
    printf("\nMain: Terminating main thread...\n");
    uthread_terminate(0); 
    return 0;
}
