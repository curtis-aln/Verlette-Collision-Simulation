#pragma once
#include <functional>
#include <vector>
#include <thread>
#include <atomic>
#include <semaphore>

class BarrierThreadPool
{
    struct Worker
    {
        std::counting_semaphore<1> start_sem{ 0 };  // main signals this to kick off
        std::function<void()>      job;
        std::thread                thread;
        std::atomic<bool>          done{ true };
    };

    std::vector<std::unique_ptr<Worker>> workers_;
    std::atomic<int>                     pending_{ 0 };
    std::binary_semaphore                all_done_sem_{ 0 };
    bool                                 stop_ = false;

    int job_count_ = 0;

public:
    explicit BarrierThreadPool(int n)
    {
        workers_.reserve(n);
        for (int i = 0; i < n; ++i)
        {
            auto& w = workers_.emplace_back(std::make_unique<Worker>());
            w->thread = std::thread([this, &worker = *w]
                {
                    while (true)
                    {
                        worker.start_sem.acquire();      // sleep until signalled
                        if (stop_) return;

                        worker.job();                    // run assigned job

                        // last thread to finish wakes the main thread
                        if (pending_.fetch_sub(1, std::memory_order_acq_rel) == 1)
                            all_done_sem_.release();
                    }
                });
        }
    }

    ~BarrierThreadPool()
    {
        stop_ = true;
        for (auto& w : workers_)
            w->start_sem.release();
        for (auto& w : workers_)
            w->thread.join();
    }

    // Call this once after construction, not run_and_wait every frame
    void assign_jobs(const std::vector<std::function<void()>>& jobs)
    {
        for (int i = 0; i < (int)jobs.size(); ++i)
            workers_[i]->job = jobs[i];
        job_count_ = (int)jobs.size(); // store this
    }

    // Then run_and_wait becomes just the signal/wait, no copies
    void run_and_wait()
    {
        pending_.store(job_count_, std::memory_order_release);
        for (int i = 0; i < job_count_; ++i)
            workers_[i]->start_sem.release();
        all_done_sem_.acquire();
    }

    int worker_count() const { return static_cast<int>(workers_.size()); }
};