#pragma once
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace render {
// 有界、惰性分配的固定大小页池。lease 可跨线程/跨管理器生命周期持有。
// 只有最后一个 lease 释放时才归还槽位，淘汰不能覆盖仍在读的 CPU 数据。
class page_pool {
    struct state {
        std::mutex mutex;
        size_t page_bytes, capacity;
        std::vector<std::unique_ptr<std::byte[]>> storage;
        std::vector<size_t> free;
        state(size_t bytes,size_t count) : page_bytes(bytes),capacity(count) { free.reserve(count); }
    };
public:
    using lease = std::shared_ptr<std::byte[]>;
    page_pool(size_t page_bytes,size_t budget_bytes) {
        state_ = std::make_shared<state>(page_bytes,budget_bytes / page_bytes);
    }
    lease acquire() {
        auto s = state_;
        size_t slot;
        std::byte* ptr;
        {
            std::lock_guard lock(s->mutex);
            if(!s->free.empty()) { slot = s->free.back(); s->free.pop_back(); }
            else {
                if(s->storage.size() == s->capacity) return {};
                slot = s->storage.size(); s->storage.push_back(std::make_unique<std::byte[]>(s->page_bytes));
            }
            ptr = s->storage[slot].get();
        }
        return lease(ptr,[s,slot](std::byte*) noexcept { std::lock_guard lock(s->mutex); s->free.push_back(slot); });
    }
    size_t page_bytes() const noexcept { return state_->page_bytes; }
    size_t used_bytes() const {
        std::lock_guard lock(state_->mutex);
        return (state_->storage.size() - state_->free.size()) * state_->page_bytes;
    }
private:
    std::shared_ptr<state> state_;
};
} // namespace render
