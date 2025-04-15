
#pragma once
#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>


namespace Utils {
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = 1) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back(
                [this] {
                    while (true) {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(this->queueMutex);
                            this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

                            if (this->stop && this->tasks.empty())
                                return;

                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }

                        task();
                    }
                }
            );
        }
    }

    template<class F, class... Args>
	auto enqueue(F &&f, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>>
    {
	    using return_type = std::invoke_result_t<F, Args...>;

	    auto task =
		    std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

	    std::future<return_type> res = task->get_future();
	    {
		    std::unique_lock<std::mutex> lock(queueMutex);

		    if (stop) {
			    throw std::runtime_error("enqueue on stopped ThreadPool");
		    }

		    tasks.emplace([task]() { (*task)(); });
	    }

	    condition.notify_one();
	    return res;
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};
}
