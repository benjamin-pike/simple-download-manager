#include "aux/ThreadPool.hpp"

ThreadPool::ThreadPool(size_t nThreads)
    : _stop(false)
{
    // Create and launch worker threads
    for (size_t i = 0; i < nThreads; ++i)
    {
        _workers.emplace_back(&ThreadPool::workerThread, this);
    }
}

// Signal all worker threads to stop and waits for them to finish
ThreadPool::~ThreadPool()
{
    {
        // Acquire lock to synchronise access to the task queue
        std::unique_lock<std::mutex> lock(_queueMutex);
        _stop = true;
    }

    // Notify all threads that they should wake up and check the stop condition
    _condition.notify_all();

    // Join each thread to ensure they have completed execution
    for (auto &worker : _workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

size_t ThreadPool::size() const
{
    return _workers.size();
}

// Add a new task to the thread pool's queue
// The task is a function with no parameters and no return value
void ThreadPool::enqueue(std::function<void()> f)
{
    {
        // Lock the task queue to ensure safe access
        std::unique_lock<std::mutex> lock(_queueMutex);
        _tasks.push(std::move(f));
    }

    // Notify a worker thread that a new task is available
    _condition.notify_one();
}

// Executed by each worker thread
// Continuously retrieves and executes tasks until the pool is signalled to stop
void ThreadPool::workerThread()
{
    while (true)
    {
        std::function<void()> task;
        {
            // Lock the task queue while waiting for a task
            std::unique_lock<std::mutex> lock(_queueMutex);
            // Wait until there is a task available or the thread pool is stopping
            _condition.wait(lock, [this]
                            { return _stop || !_tasks.empty(); });

            // If the pool is stopping and there are no remaining tasks, exit
            if (_stop && _tasks.empty())
                return;

            // Retrieve the next task from the queue
            task = std::move(_tasks.front());
            _tasks.pop();
        }

        // Execute the retrieved task outside of the lock scope
        task();
    }
}