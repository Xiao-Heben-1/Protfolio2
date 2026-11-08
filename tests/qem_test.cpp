#include <tools/model/QEM.hpp>
#include <cassert>
#include <iostream>

using namespace render;

static mesh_data make_grid(uint32_t side_length = 7){
    mesh_data source_mesh;
    for(uint32_t row = 0;row < side_length;++row)
        for(uint32_t column = 0;column < side_length;++column)
            source_mesh.vertices.emplace_back(glm::vec3(column,row,2),
                glm::vec2(column,row) / float(side_length - 1),glm::vec3(0,0,1));
    for(uint32_t row = 0;row + 1 < side_length;++row){
        for(uint32_t column = 0;column + 1 < side_length;++column){
            const uint32_t first_vertex_index = row * side_length + column;
            source_mesh.indices.insert(source_mesh.indices.end(),{
                first_vertex_index,first_vertex_index + 1,first_vertex_index + side_length,
                first_vertex_index + 1,first_vertex_index + side_length + 1,first_vertex_index + side_length});
        }
    }
    source_mesh.sub_meshes = {{0,static_cast<uint32_t>(source_mesh.indices.size()),0,17}};
    return source_mesh;
}

static void check_result(const mesh_data& result){
    size_t covered_index_count = 0;
    for(const auto& submesh:result.sub_meshes){
        assert(submesh.front_idx == covered_index_count && submesh.vertex_off == 0);
        assert(submesh.index_count % 3 == 0);
        covered_index_count += submesh.index_count;
    }
    assert(covered_index_count == result.indices.size());
    std::set<std::array<uint32_t,3>> unique_faces;
    std::vector<bool> used_vertices(result.vertices.size(),false);
    for(size_t index_offset = 0;index_offset < result.indices.size();index_offset += 3){
        const std::array<uint32_t,3> vertex_indices{
            result.indices[index_offset],result.indices[index_offset + 1],result.indices[index_offset + 2]};
        assert(vertex_indices[0] != vertex_indices[1] && vertex_indices[1] != vertex_indices[2] &&
               vertex_indices[2] != vertex_indices[0]);
        for(uint32_t vertex_index:vertex_indices){
            assert(vertex_index < result.vertices.size());
            used_vertices[vertex_index] = true;
        }
        auto sorted_indices = vertex_indices;
        std::sort(sorted_indices.begin(),sorted_indices.end());
        assert(unique_faces.insert(sorted_indices).second);
        const glm::vec3 face_normal = glm::cross(
            result.vertices[vertex_indices[1]].position - result.vertices[vertex_indices[0]].position,
            result.vertices[vertex_indices[2]].position - result.vertices[vertex_indices[0]].position);
        assert(glm::dot(face_normal,face_normal) > 0.f);
    }
    for(bool used:used_vertices) assert(used);
    for(const auto& mesh_vertex:result.vertices){
        assert(std::isfinite(mesh_vertex.position.x) && std::isfinite(mesh_vertex.position.y) &&
               std::isfinite(mesh_vertex.position.z));
        assert(std::abs(glm::length(mesh_vertex.normal) - 1.f) < 1e-5f);
    }
}

static void check_closed_topology(const mesh_data& result,int expected_euler_characteristic){
    std::map<std::pair<uint32_t,uint32_t>,int> edge_face_counts;
    for(size_t index_offset = 0;index_offset < result.indices.size();index_offset += 3)
        for(size_t corner_index = 0;corner_index < 3;++corner_index)
            ++edge_face_counts[std::minmax(result.indices[index_offset + corner_index],
                result.indices[index_offset + (corner_index + 1) % 3])];
    for(const auto& [edge_indices,face_count]:edge_face_counts){
        (void)edge_indices;
        assert(face_count == 2);
    }
    assert(int(result.vertices.size()) - int(edge_face_counts.size()) + int(result.indices.size() / 3) ==
        expected_euler_characteristic);
}

int main(){
    const auto source_mesh = make_grid();
    const auto result = QEM::lod_generater(source_mesh,0.5f);
    check_result(result);
    assert(result.indices.size() == source_mesh.indices.size() / 2);
    assert(result.vertices.size() < source_mesh.vertices.size());
    assert(result.sub_meshes[0].mat_idx == 17);
    for(const auto& mesh_vertex:result.vertices){
        assert(mesh_vertex.position.z == 2.f);
        assert(glm::length(mesh_vertex.uv - glm::vec2(mesh_vertex.position) / 6.f) < 1e-5f);
    }
    for(size_t index_offset = 0;index_offset < result.indices.size();index_offset += 3){
        const glm::vec3 first_position = result.vertices[result.indices[index_offset]].position;
        const glm::vec3 second_position = result.vertices[result.indices[index_offset + 1]].position;
        const glm::vec3 third_position = result.vertices[result.indices[index_offset + 2]].position;
        assert(glm::cross(second_position - first_position,third_position - first_position).z > 0.f);
    }
    for(const auto& mesh_vertex:source_mesh.vertices){
        if(mesh_vertex.position.x != 0 && mesh_vertex.position.x != 6 &&
           mesh_vertex.position.y != 0 && mesh_vertex.position.y != 6) continue;
        assert(std::any_of(result.vertices.begin(),result.vertices.end(),[&](const vertex& result_vertex){
            return result_vertex.position == mesh_vertex.position;
        }));
    }
    assert(QEM::lod_generater(source_mesh,0.5f).indices == result.indices);
    assert(QEM::lod_generater(source_mesh,1.f).indices == source_mesh.indices);
    const auto minimum_grid = QEM::lod_generater(source_mesh,0.f);
    check_result(minimum_grid);
    assert(!minimum_grid.indices.empty());
    assert(QEM::lod_generater({},0.f).vertices.empty());
    assert(source_mesh.indices == make_grid().indices);

    auto split_mesh = source_mesh;
    split_mesh.vertices.insert(split_mesh.vertices.end(),source_mesh.vertices.begin(),source_mesh.vertices.end());
    split_mesh.indices.clear();
    for(uint32_t vertex_index:source_mesh.indices) split_mesh.indices.push_back(vertex_index + 1);
    split_mesh.indices.insert(split_mesh.indices.end(),source_mesh.indices.begin(),source_mesh.indices.end());
    const uint32_t index_count = static_cast<uint32_t>(source_mesh.indices.size());
    split_mesh.sub_meshes = {{0,index_count,-1,17},{index_count,index_count,49,29},{2 * index_count,0,0,31}};
    const auto split_result = QEM::lod_generater(split_mesh,0.75f);
    check_result(split_result);
    assert(split_result.indices.size() == size_t(index_count * 1.5f));
    assert(split_result.sub_meshes[0].mat_idx == 17 && split_result.sub_meshes[1].mat_idx == 29);
    assert(split_result.sub_meshes[2].mat_idx == 31 && split_result.sub_meshes[2].index_count == 0);

    auto shared_mesh = source_mesh;
    shared_mesh.sub_meshes = {{0,index_count / 2,0,1},{index_count / 2,index_count / 2,0,2}};
    const auto shared_result = QEM::lod_generater(shared_mesh,0.f);
    check_result(shared_result);
    for(const auto& mesh_vertex:source_mesh.vertices){
        if(mesh_vertex.position.y != 3.f) continue;
        assert(std::any_of(shared_result.vertices.begin(),shared_result.vertices.end(),[&](const vertex& result_vertex){
            return result_vertex.position == mesh_vertex.position;
        }));
    }

    mesh_data octahedron;
    for(auto position:{glm::vec3(1,0,0),glm::vec3(-1,0,0),glm::vec3(0,1,0),
                       glm::vec3(0,-1,0),glm::vec3(0,0,1),glm::vec3(0,0,-1)})
        octahedron.vertices.emplace_back(position,glm::vec2(0),position);
    octahedron.indices = {0,2,4,2,1,4,1,3,4,3,0,4,2,0,5,1,2,5,3,1,5,0,3,5};
    octahedron.sub_meshes = {{0,24,0,0}};
    const auto closed_result = QEM::lod_generater(octahedron,0.5f);
    check_result(closed_result);
    assert(closed_result.indices.size() == 12);
    check_closed_topology(closed_result,2);
    const auto minimum_closed = QEM::lod_generater(octahedron,0.f);
    check_result(minimum_closed);
    assert(minimum_closed.indices.size() == 12); // tetrahedron must not collapse to duplicate faces
    check_closed_topology(minimum_closed,2);

    mesh_data non_manifold_mesh;
    for(auto position:{glm::vec3(0,0,0),glm::vec3(1,0,0),glm::vec3(0,1,0),
                       glm::vec3(0,-1,0),glm::vec3(0,0,1)})
        non_manifold_mesh.vertices.emplace_back(position,glm::vec2(0),glm::vec3(0,0,1));
    non_manifold_mesh.indices = {0,1,2,1,0,3,0,1,4};
    non_manifold_mesh.sub_meshes = {{0,9,0,0}};
    const auto non_manifold_result = QEM::lod_generater(non_manifold_mesh,0.f);
    check_result(non_manifold_result);
    assert(non_manifold_result.indices.size() == 9);

    mesh_data torus;
    constexpr uint32_t ring_segments = 8;
    for(uint32_t ring_index = 0;ring_index < ring_segments;++ring_index){
        const float ring_angle = float(ring_index) * 6.28318530718f / ring_segments;
        for(uint32_t tube_index = 0;tube_index < ring_segments;++tube_index){
            const float tube_angle = float(tube_index) * 6.28318530718f / ring_segments;
            const glm::vec3 surface_normal(std::cos(ring_angle) * std::cos(tube_angle),
                std::sin(ring_angle) * std::cos(tube_angle),std::sin(tube_angle));
            const glm::vec3 ring_center(3.f * std::cos(ring_angle),3.f * std::sin(ring_angle),0);
            torus.vertices.emplace_back(ring_center + surface_normal,glm::vec2(0),surface_normal);
            const uint32_t first_vertex = ring_index * ring_segments + tube_index;
            const uint32_t next_ring_vertex = ((ring_index + 1) % ring_segments) * ring_segments + tube_index;
            const uint32_t next_tube_vertex = ring_index * ring_segments + (tube_index + 1) % ring_segments;
            const uint32_t diagonal_vertex = ((ring_index + 1) % ring_segments) * ring_segments +
                (tube_index + 1) % ring_segments;
            torus.indices.insert(torus.indices.end(),{first_vertex,next_ring_vertex,next_tube_vertex,
                next_ring_vertex,diagonal_vertex,next_tube_vertex});
        }
    }
    torus.sub_meshes = {{0,static_cast<uint32_t>(torus.indices.size()),0,0}};
    const auto reduced_torus = QEM::lod_generater(torus,0.5f);
    check_result(reduced_torus);
    assert(reduced_torus.indices.size() < torus.indices.size());
    check_closed_topology(reduced_torus,0); // preserve the torus hole

    const auto large_mesh = make_grid(34); // exercises parallel quadric calculation
    const auto large_result = QEM::lod_generater(large_mesh,0.995f);
    check_result(large_result);
    assert(large_result.indices.size() < large_mesh.indices.size());
    std::cout << "QEM tests passed\n";
}
