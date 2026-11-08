#pragma once
#include <tools/structures/AABB.hpp>
#include <tools/pools/thread_pool.hpp>
#include <filesystem>
#include <array>

namespace render {
struct prim_instance {
    uint32_t id=0;
    AABB bounds;
    std::array<std::string,3> lod_guids;
};
class scence_prim {
private:
    struct state;
    std::shared_ptr<state> state_;
public:
    scence_prim();
    void upsert(prim_instance instance);
    bool erase(uint32_t id);
    std::vector<prim_instance> query(const AABB& bounds) const;
    std::vector<prim_instance> query_cell(const glm::vec3& position) const;
    std::future<void> upsert_async(prim_instance instance,thread_pool& pool=gThreadPool) const;
    std::future<bool> erase_async(uint32_t id,thread_pool& pool=gThreadPool) const;
    std::future<std::vector<prim_instance>> query_async(AABB bounds,thread_pool& pool=gThreadPool) const;
    void save(const std::filesystem::path& file) const;
    static scence_prim load(const std::filesystem::path& file);
    static std::future<scence_prim> load_async(std::filesystem::path file,thread_pool& pool=gThreadPool);
    std::future<void> save_async(std::filesystem::path file,thread_pool& pool=gThreadPool) const;
};
}
