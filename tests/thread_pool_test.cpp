#include <tools/pools/thread_pool.hpp>
#include <atomic>
#include <barrier>
#include <cassert>
#include <chrono>
#include <iostream>

using namespace render;

int main() {
    thread_pool single(0);
    assert(single.size() == 1);
    assert(single.enqueue([] { return 42; }).get() == 42);
    thread_pool pool(4);
    assert(pool.enqueue([](std::unique_ptr<int> value) { return *value; }, std::make_unique<int>(7)).get() == 7);
    assert(pool.enqueue([value = std::make_unique<int>(9)] { return *value; }).get() == 9);
    int value = 0;
    pool.enqueue([](int& target) { target = 5; }, std::ref(value)).get();
    assert(value == 5);
    bool caught = false;
    try { pool.enqueue([] { throw std::runtime_error("test"); }).get(); }
    catch (const std::runtime_error&) { caught = true; }
    assert(caught);

    std::vector<int> hits(10003);
    parallelfor(pool, [&](size_t i) { ++hits[i]; }, hits.size(), 2048);
    for (int hit : hits) assert(hit == 1);
    parallelfor(pool, [&](size_t) { assert(false); }, 0);
    caught = false;
    try { parallelfor(pool, [](size_t) {}, 1, 0); }
    catch (const std::invalid_argument&) { caught = true; }
    assert(caught);
    const auto caller = std::this_thread::get_id();
    parallelfor(pool, [&](size_t) { assert(std::this_thread::get_id() == caller); }, 2048, 2048);

    // Occupy every worker, then nest parallelfor on the same pool.
    std::barrier gate(4);
    std::atomic<int> nested_hits{0};
    parallelfor(pool, [&](size_t) {
        gate.arrive_and_wait();
        parallelfor(pool, [&](size_t) { ++nested_hits; }, 100);
    }, 4);
    assert(nested_hits == 400);

    std::atomic<int> completed{0};
    caught = false;
    try {
        parallelfor(pool, [&](size_t i) {
            gate.arrive_and_wait();
            if (i == 0) throw std::runtime_error("parallel failure");
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ++completed;
        }, 4);
    } catch (const std::runtime_error&) { caught = true; }
    assert(caught && completed == 3); // no task survives the exceptional return
    assert(pool.enqueue([] { return true; }).get());

    std::atomic<int> drained{0};
    {
        thread_pool temporary(2);
        for (int i = 0; i < 100; ++i) temporary.enqueue([&] { ++drained; });
    }
    assert(drained == 100);
    std::cout << "thread_pool tests passed\n";
}
