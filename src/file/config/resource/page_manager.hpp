#pragma once
#include <tools/pools/thread_pool.hpp>
#include <filesystem>
#include <optional>
#include <span>

namespace render {
struct page_key {
    std::string guid;
    uint32_t page=0;
    bool operator==(const page_key&) const = default;
};
struct page_descriptor {
    page_key key;
    std::filesystem::path file;
    uint64_t offset=0;
    uint32_t bytes=0;
};
struct page_handle {
    uint32_t slot=UINT32_MAX;
    uint64_t generation=0;
    bool operator==(const page_handle&) const = default;
};
enum class page_state { loading,ready,failed };
struct page_view {
    page_handle handle;
    page_key key;
    page_state state=page_state::loading;
    std::shared_ptr<const std::byte[]> data;
    uint32_t bytes=0;
    uint64_t last_frame=0;
    std::string error;
};
struct page_budget {
    size_t page_bytes=64*1024;
    size_t cpu_bytes=256*1024*1024;
    size_t inflight_bytes=16*1024*1024;
};
struct page_address { page_key key; uint32_t offset=0; };
struct page_mapping {
    std::shared_ptr<const std::byte[]> owner;
    uint32_t offset=0,bytes=0;
    std::span<const std::byte> data() const;
};
// 独立CPU页面管理器，不持有Vulkan或渲染资源；实现和内部索引均在cpp。
class page_manager {
private:
    struct implementation;
    std::unique_ptr<implementation> impl;
public:
    explicit page_manager(page_budget budget={},thread_pool& workers=gThreadPool);
    ~page_manager();
    page_manager(const page_manager&)=delete;
    page_manager& operator=(const page_manager&)=delete;
    void register_page(page_descriptor page);
    std::optional<page_handle> request(const page_key& key,uint64_t frame=0);
    std::optional<page_view> query(page_handle handle) const;
    std::optional<page_mapping> resolve(const page_address& address,uint32_t bytes) const;
    void poll();
    bool evict(page_handle handle);
    void trim(uint64_t oldest_frame);
    size_t used_bytes() const;
};
}
