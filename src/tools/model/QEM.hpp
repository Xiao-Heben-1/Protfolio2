#ifndef RENDER_TOOLS_MODEL_QEM_HPP
#define RENDER_TOOLS_MODEL_QEM_HPP

#include <core/component/mesh/mesh_data.hpp>
#include <tools/pools/thread_pool.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <queue>
#include <utility>

namespace render {

class QEM final{
    struct collapse_triangle{
        std::array<size_t,3> vertex_indices;
        size_t submesh_index;
        bool removed = false;
    };

    struct qem_data{
        std::vector<collapse_triangle> triangles;
        std::vector<glm::mat4> vertex_quadrics;
        std::vector<bool> locked_vertices;
    };

    struct collapse_candidate{
        size_t keep_vertex_index = 0;
        size_t remove_vertex_index = 0;
        glm::vec3 target_position{};
        double error = std::numeric_limits<double>::infinity();
    };

    static double quadric_error(const glm::dmat4& quadric,const glm::vec3& position){
        const glm::dvec4 homogeneous_position(position,1.0);
        return glm::dot(homogeneous_position,quadric * homogeneous_position);
    }

    // 可逆时求最优位置；奇异时从两端点和中点中选择误差最小的位置。
    static glm::vec3 edge_target(const glm::dmat4& combined_quadric,
                                 const glm::vec3& start_position,const glm::vec3& end_position){
        const glm::dmat3 position_matrix(combined_quadric);
        const glm::dvec3 plane_offset(combined_quadric[3]);
        if(std::abs(glm::determinant(position_matrix)) > 1e-12)
            return glm::vec3(-glm::inverse(position_matrix) * plane_offset);

        const glm::vec3 midpoint = (start_position + end_position) * 0.5f;
        const double start_error = quadric_error(combined_quadric,start_position);
        const double end_error = quadric_error(combined_quadric,end_position);
        const double midpoint_error = quadric_error(combined_quadric,midpoint);
        if(start_error <= end_error && start_error <= midpoint_error) return start_position;
        if(end_error <= midpoint_error) return end_position;
        return midpoint;
    }

    // 一次解析索引，生成折叠用三角形和顶点误差矩阵。
    static qem_data foreach_data(const mesh_data& source_mesh){
        qem_data prepared_data;
        prepared_data.triangles.reserve(source_mesh.indices.size() / 3);
        prepared_data.locked_vertices.assign(source_mesh.vertices.size(),false);
        const size_t unassigned_submesh = std::numeric_limits<size_t>::max();
        std::vector<size_t> vertex_submeshes(source_mesh.vertices.size(),unassigned_submesh);
        for(size_t submesh_index = 0;submesh_index < source_mesh.sub_meshes.size();++submesh_index){
            const auto& submesh = source_mesh.sub_meshes[submesh_index];
            const size_t index_end = size_t(submesh.front_idx) + submesh.index_count;
            for(size_t index_offset = submesh.front_idx;index_offset < index_end;index_offset += 3){
                collapse_triangle triangle{};
                triangle.submesh_index = submesh_index;
                for(size_t corner_index = 0;corner_index < 3;++corner_index){
                    const size_t vertex_index = static_cast<size_t>(
                        int64_t(source_mesh.indices[index_offset + corner_index]) + submesh.vertex_off);
                    triangle.vertex_indices[corner_index] = vertex_index;
                    if(vertex_submeshes[vertex_index] != unassigned_submesh &&
                       vertex_submeshes[vertex_index] != submesh_index)
                        prepared_data.locked_vertices[vertex_index] = true;
                    vertex_submeshes[vertex_index] = submesh_index;
                }
                prepared_data.triangles.push_back(triangle);
            }
        }

        std::vector<glm::mat4> triangle_quadrics(prepared_data.triangles.size());
        constexpr size_t triangles_per_task = 2048;
        parallelfor([&](size_t triangle_index){
            const auto& vertex_indices = prepared_data.triangles[triangle_index].vertex_indices;
            const glm::dvec3 first_position(source_mesh.vertices[vertex_indices[0]].position);
            const glm::dvec3 second_position(source_mesh.vertices[vertex_indices[1]].position);
            const glm::dvec3 third_position(source_mesh.vertices[vertex_indices[2]].position);
            const glm::dvec3 face_normal = glm::normalize(glm::cross(
                second_position - first_position,third_position - first_position));
            const glm::dvec4 face_plane(face_normal,-glm::dot(face_normal,first_position));
            triangle_quadrics[triangle_index] = glm::mat4(glm::outerProduct(face_plane,face_plane));
        },prepared_data.triangles.size(),triangles_per_task);

        // 各三角形独立并行计算，共享顶点的矩阵按顺序累加。
        prepared_data.vertex_quadrics.assign(source_mesh.vertices.size(),glm::mat4(0.0f));
        for(size_t triangle_index = 0;triangle_index < prepared_data.triangles.size();++triangle_index)
            for(size_t vertex_index:prepared_data.triangles[triangle_index].vertex_indices)
                prepared_data.vertex_quadrics[vertex_index] += triangle_quadrics[triangle_index];
        return prepared_data;
    }

    // 边两侧的对顶点应恰好等于端点的公共邻居；剩余面不能重复、退化或翻转。
    static bool can_collapse(const collapse_candidate& candidate,const std::vector<vertex>& working_vertices,
                             const std::vector<collapse_triangle>& triangles,
                             const std::vector<std::set<size_t>>& vertex_neighbors){
        std::set<size_t> opposite_vertices;
        std::set<std::array<size_t,3>> surviving_faces;
        for(const auto& triangle:triangles){
            if(triangle.removed) continue;
            const auto& vertex_indices = triangle.vertex_indices;
            const bool contains_kept_vertex = std::find(vertex_indices.begin(),vertex_indices.end(),candidate.keep_vertex_index) != vertex_indices.end();
            const bool contains_removed_vertex = std::find(vertex_indices.begin(),vertex_indices.end(),candidate.remove_vertex_index) != vertex_indices.end();
            if(!contains_kept_vertex && !contains_removed_vertex) continue;
            if(contains_kept_vertex && contains_removed_vertex){
                for(size_t vertex_index:vertex_indices)
                    if(vertex_index != candidate.keep_vertex_index && vertex_index != candidate.remove_vertex_index)
                        opposite_vertices.insert(vertex_index);
                continue;
            }

            auto collapsed_indices = vertex_indices;
            for(size_t& vertex_index:collapsed_indices)
                if(vertex_index == candidate.remove_vertex_index) vertex_index = candidate.keep_vertex_index;
            std::sort(collapsed_indices.begin(),collapsed_indices.end());
            // 例如四面体折叠后会留下两张重合面，需要单独排除。
            if(!surviving_faces.insert(collapsed_indices).second) return false;

            std::array<glm::dvec3,3> original_positions,collapsed_positions;
            for(size_t corner_index = 0;corner_index < 3;++corner_index){
                const size_t vertex_index = vertex_indices[corner_index];
                original_positions[corner_index] = glm::dvec3(working_vertices[vertex_index].position);
                collapsed_positions[corner_index] =
                    (vertex_index == candidate.keep_vertex_index || vertex_index == candidate.remove_vertex_index)
                    ? glm::dvec3(candidate.target_position) : original_positions[corner_index];
            }
            const glm::dvec3 original_normal = glm::cross(original_positions[1] - original_positions[0],original_positions[2] - original_positions[0]);
            const glm::dvec3 collapsed_normal = glm::cross(collapsed_positions[1] - collapsed_positions[0],collapsed_positions[2] - collapsed_positions[0]);
            const double original_area_squared = glm::dot(original_normal,original_normal);
            const double collapsed_area_squared = glm::dot(collapsed_normal,collapsed_normal);
            if(collapsed_area_squared <= original_area_squared * 1e-12 || glm::dot(original_normal,collapsed_normal) <= 1e-6 * std::sqrt(original_area_squared * collapsed_area_squared))
            return false;
        }
        std::set<size_t> common_neighbors;
        for(size_t vertex_index:vertex_neighbors[candidate.keep_vertex_index])
            if(vertex_neighbors[candidate.remove_vertex_index].count(vertex_index))
                common_neighbors.insert(vertex_index);
        return opposite_vertices.size() == 2 && common_neighbors == opposite_vertices;
    }

    // 将 remove_vertex_index 合并到 keep_vertex_index，返回删除的三角形数量。
    static size_t collapse_edge(const collapse_candidate& candidate,std::vector<vertex>& working_vertices,qem_data& prepared_data,const std::set<size_t>& affected_faces){
        auto& kept_vertex = working_vertices[candidate.keep_vertex_index];
        const auto& removed_vertex = working_vertices[candidate.remove_vertex_index];
        const glm::dvec3 edge_direction = glm::dvec3(removed_vertex.position) - glm::dvec3(kept_vertex.position);
        const glm::dvec3 target_offset = glm::dvec3(candidate.target_position) - glm::dvec3(kept_vertex.position);
        const float blend_weight = static_cast<float>(std::clamp(
            (glm::dot(edge_direction,edge_direction)>0 ? glm::dot(target_offset,edge_direction) / glm::dot(edge_direction,edge_direction) : 0.5),0.0,1.0));
        kept_vertex.position = candidate.target_position;
        kept_vertex.uv = glm::mix(kept_vertex.uv,removed_vertex.uv,blend_weight);
        const glm::dvec3 blended_normal = glm::mix(
            glm::dvec3(kept_vertex.normal),glm::dvec3(removed_vertex.normal),double(blend_weight));
        // 相反法线插值可能抵消，此时沿用保留顶点的法线。
        if(glm::dot(blended_normal,blended_normal) > 0.0)
            kept_vertex.normal = glm::vec3(glm::normalize(blended_normal));
        prepared_data.vertex_quadrics[candidate.keep_vertex_index] +=
            prepared_data.vertex_quadrics[candidate.remove_vertex_index];

        size_t removed_triangle_count = 0;
        for(auto face:affected_faces){
            auto& triangle=prepared_data.triangles[face];
            if(triangle.removed) continue;
            auto& vertex_indices = triangle.vertex_indices;
            for(size_t& vertex_index:vertex_indices)
                if(vertex_index == candidate.remove_vertex_index) vertex_index = candidate.keep_vertex_index;
            if(vertex_indices[0] == vertex_indices[1] || vertex_indices[1] == vertex_indices[2] ||
               vertex_indices[2] == vertex_indices[0]){
                triangle.removed = true;
                ++removed_triangle_count;
            }
        }
        return removed_triangle_count;
    }

public:
    // keep_ratio 为面数保留比例 [0,1]；输入由调用方保证有效。
    // 选择误差最小的合法内部边，固定边界和子网格交界；无合法边时提前停止。
    // 结果面数不低于目标，受几何和拓扑约束时可能高于目标。
    static mesh_data lod_generater(const mesh_data& source_mesh,float keep_ratio){
        if(keep_ratio == 1.0f) return source_mesh;
        auto prepared_data = foreach_data(source_mesh);
        auto working_vertices = source_mesh.vertices;
        size_t remaining_triangle_count = prepared_data.triangles.size();
        const size_t target_triangle_count = static_cast<size_t>(std::ceil(
            double(remaining_triangle_count) * keep_ratio));
        // 局部更新邻接；堆中旧候选通过端点修订号失效。
        using edge=std::pair<size_t,size_t>;
        std::map<edge,size_t> edge_faces;
        std::vector<std::set<size_t>> neighbors(working_vertices.size()),incident(working_vertices.size());
        std::vector<uint64_t> revisions(working_vertices.size());
        const auto add_face=[&](size_t index){
            const auto& t=prepared_data.triangles[index];
            for(size_t c=0;c<3;++c){const auto a=t.vertex_indices[c],b=t.vertex_indices[(c+1)%3];
                incident[a].insert(index);++edge_faces[std::minmax(a,b)];neighbors[a].insert(b);neighbors[b].insert(a);}
        };
        const auto remove_face=[&](size_t index){
            const auto& t=prepared_data.triangles[index];
            for(size_t c=0;c<3;++c){const auto a=t.vertex_indices[c],b=t.vertex_indices[(c+1)%3];
                incident[a].erase(index);auto found=edge_faces.find(std::minmax(a,b));
                if(--found->second==0){edge_faces.erase(found);neighbors[a].erase(b);neighbors[b].erase(a);}}
        };
        for(size_t i=0;i<prepared_data.triangles.size();++i)add_face(i);
        for(const auto& [e,count]:edge_faces)if(count!=2){prepared_data.locked_vertices[e.first]=true;prepared_data.locked_vertices[e.second]=true;}
        struct queued {collapse_candidate value;uint64_t first,second;};
        const auto greater=[](const queued& a,const queued& b){
            if(a.value.error!=b.value.error)return a.value.error>b.value.error;
            return std::tie(a.value.keep_vertex_index,a.value.remove_vertex_index)>std::tie(b.value.keep_vertex_index,b.value.remove_vertex_index);
        };
        std::priority_queue<queued,std::vector<queued>,decltype(greater)> queue(greater);
        const auto push_edge=[&](size_t a,size_t b){
            if(a>b)std::swap(a,b);
            if(prepared_data.locked_vertices[a] || prepared_data.locked_vertices[b])return;
            auto found=edge_faces.find({a,b});if(found==edge_faces.end() || found->second!=2)return;
            const auto q=glm::dmat4(prepared_data.vertex_quadrics[a])+glm::dmat4(prepared_data.vertex_quadrics[b]);
            const auto target=edge_target(q,working_vertices[a].position,working_vertices[b].position);
            const auto error=quadric_error(q,target);if(!std::isfinite(error))return;
            queue.push({{a,b,target,error},revisions[a],revisions[b]});
        };
        for(const auto& [e,count]:edge_faces)push_edge(e.first,e.second);
        while(remaining_triangle_count>=target_triangle_count+2 && !queue.empty()){
            const auto next=queue.top();queue.pop();const auto& candidate=next.value;
            const auto a=candidate.keep_vertex_index,b=candidate.remove_vertex_index;
            if(next.first!=revisions[a] || next.second!=revisions[b])continue;
            if(!neighbors[a].count(b))continue;
            std::set<size_t> faces=incident[a];faces.insert(incident[b].begin(),incident[b].end());
            std::vector<collapse_triangle> local;local.reserve(faces.size());for(auto f:faces)local.push_back(prepared_data.triangles[f]);
            if(!can_collapse(candidate,working_vertices,local,neighbors))continue;
            std::set<size_t> affected=neighbors[a];affected.insert(neighbors[b].begin(),neighbors[b].end());affected.insert(a);affected.insert(b);
            for(auto f:faces)remove_face(f);
            remaining_triangle_count-=collapse_edge(candidate,working_vertices,prepared_data,faces);
            for(auto f:faces)if(!prepared_data.triangles[f].removed)add_face(f);
            for(auto v:affected)++revisions[v];
            std::set<edge> changed;for(auto v:affected)for(auto n:neighbors[v])changed.insert(std::minmax(v,n));
            for(const auto& e:changed)push_edge(e.first,e.second);
        }

        mesh_data result;
        result.sub_meshes = source_mesh.sub_meshes;
        for(auto& submesh:result.sub_meshes){
            submesh.index_count = 0;
            submesh.vertex_off = 0; // 输出使用全局顶点索引。
        }
        const size_t unmapped_vertex = std::numeric_limits<size_t>::max();
        std::vector<size_t> vertex_remap(working_vertices.size(),unmapped_vertex);
        for(const auto& triangle:prepared_data.triangles){
            if(triangle.removed) continue;
            for(size_t vertex_index:triangle.vertex_indices){
                if(vertex_remap[vertex_index] == unmapped_vertex){
                    vertex_remap[vertex_index] = result.vertices.size();
                    result.vertices.push_back(working_vertices[vertex_index]);
                }
                result.indices.push_back(static_cast<uint32_t>(vertex_remap[vertex_index]));
            }
            result.sub_meshes[triangle.submesh_index].index_count += 3;
        }
        uint32_t index_offset = 0;
        for(auto& submesh:result.sub_meshes){
            submesh.front_idx = index_offset;
            index_offset += submesh.index_count;
        }
        return result;
    }
};

} // namespace render
#endif
