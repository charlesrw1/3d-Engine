#ifndef THREADTOOLS_H
#define THREADTOOLS_H


typedef void (*ThreadFunc)(int);

void run_threads_on_function(ThreadFunc func, int start, int end);

#endif // !THREADTOOLS_H
