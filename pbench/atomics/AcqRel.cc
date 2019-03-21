#include <atomic>

std::atomic<int> A;
std::atomic<int> B;

void thread1() {
    // Lock A
    int expected = 0;
    while (!A.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
        expected = 0;
    }

    // Unlock A
    A.store(0, std::memory_order_release);

    // Lock B
    while (!B.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
        expected = 0;
    }

    // Unlock B
    B.store(0, std::memory_order_release);
}