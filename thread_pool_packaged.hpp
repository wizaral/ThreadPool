#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace al {

class ThreadPoolPackaged final {
    using Task = std::function<void()>;

    std::vector<std::thread> m_threads;
    std::deque<Task> m_tasks;

    std::mutex m_tasks_mutex;
    std::condition_variable m_condition;
    std::atomic_bool m_terminate{false};

#if defined(THREAD_POOL_PACKAGED_WAIT)
    uint64_t m_working_threads = 0;
    std::condition_variable m_finished;
#endif

public:
    ThreadPoolPackaged() = default;

    explicit ThreadPoolPackaged(size_t amount) {
        start(amount);
    }

    ThreadPoolPackaged(const ThreadPoolPackaged &) = delete;
    ThreadPoolPackaged(ThreadPoolPackaged &&) = delete;
    ThreadPoolPackaged &operator=(const ThreadPoolPackaged &) = delete;
    ThreadPoolPackaged &operator=(ThreadPoolPackaged &&) = delete;

    ~ThreadPoolPackaged() {
        if (m_terminate == false) {
            stop();
        }
    }

    template <class Func, class... Args>
    auto add_task(Func &&task, Args &&...args) {
        auto wrapper = std::make_shared<std::packaged_task<decltype(task(args...))()>>(
            std::forward<Func>(task), std::forward<Args>(args)...);

        {
            std::unique_lock lock(m_tasks_mutex);
            m_tasks.emplace_back([wrapper] {
                (*wrapper)();
            });
        }

        m_condition.notify_one();
        return wrapper->get_future();
    }

    void start(size_t amount) {
        for (size_t i = 0; i < amount; ++i) {
            m_threads.emplace_back(&ThreadPoolPackaged::runner, this);
        }
    }

#if defined(THREAD_POOL_PACKAGED_WAIT)
    void wait() {
        std::unique_lock lock(m_tasks_mutex);

        m_finished.wait(lock, [this]() {
            return m_tasks.empty() && m_working_threads == 0;
        });
    }
#endif

    void stop() {
        if (m_terminate == false) {
            m_terminate = true;
            m_condition.notify_all();

            for (auto &thread : m_threads) {
                thread.join();
            }
            m_threads.clear();
        }
    }

    void terminate() {
        if (m_terminate == false) {
            {
                std::unique_lock lock(m_tasks_mutex);
                m_tasks.clear();
            }
            stop();
        }
    }

private:
    void runner() {
        for (Task task; true;) {
            std::unique_lock lock(m_tasks_mutex);

            m_condition.wait(lock, [this] {
                return !m_tasks.empty() || m_terminate;
            });

            if (m_terminate && m_tasks.empty()) {
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop_front();

#if defined(THREAD_POOL_PACKAGED_WAIT)
            ++m_working_threads;
            lock.unlock();
#endif

            task();

#if defined(THREAD_POOL_PACKAGED_WAIT)
            lock.lock();
            --m_working_threads;
            m_finished.notify_one();
#endif
        }
    }
};

} // namespace al
