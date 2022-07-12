#include "ThreadTools.h"

#include <vector>
#include <thread>
#include <mutex>

struct ThreadState
{
	std::mutex counter_mutex;
	std::vector<std::thread> running_threads;
	int current{};
	int end{};
};
static ThreadState state;

void run_job(ThreadFunc func)
{
	while (1)
	{
		int num;
		state.counter_mutex.lock();
		num = state.current++;
		state.counter_mutex.unlock();
		if (num >= state.end) {
			return;
		}
		func(num);
	}
}

void run_threads_on_function(ThreadFunc func, int start, int end) {
	unsigned int thread_count = std::thread::hardware_concurrency();
	state.running_threads.resize(thread_count);
	state.current = start;
	state.end = end;
	for (int i = 0; i < state.running_threads.size(); i++) {
		auto& t = state.running_threads[i];
		t = std::thread(&run_job, func);
	}

	for (int i = 0; i < state.running_threads.size(); i++) {
		state.running_threads[i].join();
	}

}