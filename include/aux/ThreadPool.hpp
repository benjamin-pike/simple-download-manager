#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool
{
public:
    explicit ThreadPool(size_t nThreads);
    ~ThreadPool();

    size_t size() const;
    void enqueue(std::function<void()> func);

private:
    void workerThread();

    std::vector<std::thread> _workers;
    std::queue<std::function<void()>> _tasks;
    std::mutex _queueMutex;
    std::condition_variable _condition;
    bool _stop;
};

#endif