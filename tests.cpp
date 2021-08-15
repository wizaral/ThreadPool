#include <chrono>
#include <iostream>
#include <random>

#define THREAD_POOL_PACKAGED_WAIT
#include "thread_pool_packaged.hpp"

template <class T>
struct Any {
    T t{0};

    operator const T() const {
        return t;
    }

    explicit Any(const T &t)
    : t(t) {}
};

int main() {
    std::ios::sync_with_stdio(false);

    std::mt19937 mt(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<> uid{0, 50};

    al::ThreadPoolPackaged tp{std::thread::hardware_concurrency() - 1};
    std::vector<std::future<Any<int>>> v;

    for (auto i = 0, end = uid(mt); i < end; ++i) {
        v.emplace_back(tp.add_task([i, &uid, &mt] {
            std::this_thread::sleep_for(std::chrono::milliseconds(uid(mt) * uid(mt)));
            return Any{i};
        }));
    }

#if defined(THREAD_POOL_PACKAGED_WAIT)
    tp.wait();
#endif

    for (auto &i : v) {
        std::cout << "VALUE: " << i.get() << std::endl;
    }
}
