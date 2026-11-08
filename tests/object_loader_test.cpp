#include <engine.hpp>
#include <core/object/object_loader.hpp>
#include <file/tools/file_path.hpp>
#include <file/tools/bc_texture.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
using namespace render;
int main(){
    const auto root=std::filesystem::absolute(std::filesystem::path("object_loader_test_data")/generate_guid());
    const auto models=root/path_from_utf8("model/中文模型");std::filesystem::create_directories(models);
    {std::ofstream file(models/"pixel.ppm",std::ios::binary);file<<"P6\n1 1\n255\n";const char pixel[]={char(240),char(60),char(30)};file.write(pixel,3);}
    // 与游戏模型相同：RGB 有颜色，但 Alpha 是全零的独立遮罩。
    {std::ofstream file(models/"packed.tga",std::ios::binary);const unsigned char pixels[]={0,0,2,0,0,0,0,0,0,0,0,0,1,0,1,0,32,0x28,30,60,240,0};file.write(reinterpret_cast<const char*>(pixels),sizeof(pixels));}
    {std::ofstream file(models/"quad.mtl");file<<"newmtl Opaque\nKd 1 1 1\nmap_Kd packed.tga\nnewmtl Glass\nKd 0.2 0.5 0.8\nd 0.4\nmap_Kd packed.tga\n";}
    {std::ofstream file(models/"quad.obj");file<<"mtllib quad.mtl\nv 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nusemtl Opaque\nf 1/1 2/2 3/3\nusemtl Glass\nf 1/1 3/3 4/4\n";}
    workspace_config workspace;workspace.workspace_root=root;workspace.source_root="src";egn.init(workspace);
    {
        assert(egn.get_file_system().get_files().empty()); // model 位于源目录外，启动时不得索引。
        std::unique_ptr<object> loaded(object_loader::load_model(models/"quad.obj"));
        assert(!std::filesystem::exists(models/"quad.obj.guid") && !std::filesystem::exists(models/"pixel.ppm.guid"));
        for(const auto& [guid,file]:egn.get_file_system().get_files()){
            assert(*file.path.begin()!=L"..");assert(egn.get_file_system().get_scan().get_source_path(guid)==root/"src"/file.path);
        }
        const auto source_guid=loaded->get_mesh()->get_lods()[0].source_guid;
        const auto imported=egn.get_file_system().get_scan().get_source_path(source_guid);
        assert(std::filesystem::exists(imported.parent_path()/"quad.mtl"));
        assert(std::filesystem::exists(imported.parent_path()/"packed.tga"));
        assert(loaded->get_mesh() && loaded->get_material());assert(!loaded->get_transparent());
        bool texture=false,glass=false;
        for(auto* sub:loaded->get_material()->get_sub_materials()){
            if(sub->name=="Opaque"){
                assert(!sub->base_color_map.image_uid.empty() && sub->uniform_data.texture_flags.x);
                assert(sub->uniform_data.texture_flags_2.z==0 && sub->uniform_data.alpha_options.x==0);
                assert(egn.get_file_system().get_resources().get_image_data().get_entries().at(sub->base_color_map.image_uid).config.has_transparency);
                sub->visible=false;texture=true;
            }
            if(sub->name=="Glass"){
                assert(std::abs(sub->uniform_data.base_color.a-.4f)<.001f);
                assert(sub->uniform_data.texture_flags_2.z==0 && sub->uniform_data.alpha_options.x==0);glass=true;
            }
        }
        assert(texture && glass);
        object_config config;config.name="Imported Object";config.mesh.emplace();
        for(const auto& [guid,file]:egn.get_file_system().get_files())if(file.type==file_type::model)config.mesh->source_guid=guid;
        loaded->get_transform()->position={3,4,5};loaded->add_component<behavior>();
        const auto saved=object_loader::describe(*loaded,config);const auto file=root/"src/Objects/test.object";
        egn.get_file_system().get_scan().save_object(saved.guid,file,saved);
        auto restored_config=object_config::load(file);std::unique_ptr<object> restored(object_loader::load(restored_config));
        assert(restored->get_transform()->position==glm::vec3(3,4,5));
        assert(restored->get_components().size()==4);
        const auto roundtrip=object_loader::describe(*restored,restored_config);
        assert(roundtrip.mesh->lod_guids==saved.mesh->lod_guids);
        for(size_t i=0;i<saved.material->overrides.size();++i){
            const auto& a=saved.material->overrides[i];const auto& b=roundtrip.material->overrides[i];
            assert(a.base_color==b.base_color && a.textures==b.textures && a.alpha_mode==b.alpha_mode && a.texture_channels==b.texture_channels);
            assert(a.base_color_alpha==b.base_color_alpha && a.visible==b.visible);
        }
        object replacement;object_loader::load_model(models/"quad.obj",&replacement);
        assert(replacement.get_mesh()->get_lods()[0].source_guid==source_guid);
        assert(replacement.get_mesh() && replacement.get_material());
        // 声明过的 glTF 不透明 / 裁剪 / 混合模式必须保留，不能被 FBX 修复覆盖。
        {std::ofstream bin(models/"triangle.bin",std::ios::binary);const float positions[]={0,0,0,1,0,0,0,1,0};bin.write(reinterpret_cast<const char*>(positions),sizeof(positions));}
        {std::ofstream gltf(models/"alpha.gltf");gltf<<R"({"asset":{"version":"2.0"},"buffers":[{"uri":"triangle.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],"materials":[{"name":"Solid","alphaMode":"OPAQUE"},{"name":"Cutout","alphaMode":"MASK","alphaCutoff":0.3},{"name":"Blend","alphaMode":"BLEND"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"material":0},{"attributes":{"POSITION":0},"material":1},{"attributes":{"POSITION":0},"material":2}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";}
        {std::unique_ptr<object> gltf(object_loader::load_model(models/"alpha.gltf"));unsigned checked=0;
            for(auto* sub:gltf->get_material()->get_sub_materials()){
                const unsigned expected=sub->name=="Solid" ? 1u : sub->name=="Cutout" ? 2u : sub->name=="Blend" ? 3u : 0u;
                if(expected){assert(sub->uniform_data.texture_flags_2.z==expected && sub->uniform_data.alpha_options.x==1);++checked;}
            }assert(checked==3);
        }
        // 压缩后的小 mip 仍应保留白色 RGB，即使原图 Alpha 全为零。
        const std::vector<uint8_t> packed={255,255,255,0,255,255,255,0,255,255,255,0,255,255,255,0};
        const auto first_bc3_rgb=[](const bc_mip& mip){
            const auto& b=mip.blocks;
            const auto rgb=[](uint16_t c){return glm::vec3((c>>11)&31,(c>>5)&63,c&31)/glm::vec3(31,63,31);};
            const auto c0=rgb(uint16_t(b[8])|(uint16_t(b[9])<<8)),c1=rgb(uint16_t(b[10])|(uint16_t(b[11])<<8));
            const glm::vec3 palette[]={c0,c1,(2.f*c0+c1)/3.f,(c0+2.f*c1)/3.f};
            return palette[b[12]&3];
        };
        const auto mips=compress_bc(packed,2,2,true,true,true);
        assert(mips.size()==2 && glm::length(first_bc3_rgb(mips.back())-glm::vec3(1))<.01f);
        const auto opacity_mips=compress_bc(packed,2,2,true,true,true,true);
        assert(glm::length(first_bc3_rgb(opacity_mips.back()))<.01f);
        object empty;empty.add_component(int(component_type::mesh_type));empty.add_component(int(component_type::material_type));
        empty.update();empty.late_update();assert(egn.get_render_system().get_prim_table().query(AABB({-10,-10,-10},{10,10,10})).empty());
        for(const auto type:{geometry_type::cube,geometry_type::sphere,geometry_type::capsule,geometry_type::cylinder,geometry_type::plane,geometry_type::quad}){
            std::unique_ptr<object> geometry(object_loader::create_geometry(type));
            const auto& lod=geometry->get_mesh()->get_lods()[0];assert(lod.vertex_bytes>0 && lod.index_bytes>=6*4);
            const auto extent=lod.bounds.get_max()-lod.bounds.get_min();
            if(type==geometry_type::cube || type==geometry_type::sphere)assert(glm::length(extent-glm::vec3(1))<.001f);
            if(type==geometry_type::capsule || type==geometry_type::cylinder)assert(glm::length(extent-glm::vec3(1,2,1))<.001f);
            auto saved=object_loader::describe(*geometry,{});assert(!saved.mesh->source_guid.empty());
            std::unique_ptr<object> restored(object_loader::load(saved));assert(restored->get_mesh()->get_lods()[0].guid==lod.guid);
        }
        object_config camera_object;camera_object.name="Test Camera";camera_object.camera.emplace();camera_object.camera->is_main=true;
        camera_object.transform.position={7,8,9};camera_object.transform.rotation={.2f,.4f,0};
        camera_object.lights.emplace_back();camera_object.lights.back().type=light_type::point;camera_object.lights.back().emission=2.5f;
        {
            std::unique_ptr<object> runtime(object_loader::load(camera_object));runtime->update();
            auto saved=object_loader::describe(*runtime,camera_object);
            assert(saved.camera->is_main && saved.camera->position==camera_object.transform.position);
            egn.get_file_system().get_scan().save_object(saved.guid,root/"src/Objects/camera.object",saved);
            const auto read=object_config::load(root/"src/Objects/camera.object");
            assert(read.camera->is_main && read.lights.size()==1 && read.lights[0].emission==2.5f);
            std::unique_ptr<object> restored(object_loader::load(read));restored->update();
            assert(!runtime->get_camera()->get_is_main() && restored->get_camera()->get_is_main());
            assert(glm::length(restored->get_camera()->get_position()-glm::vec3(7,8,9))<.001f);
            assert(egn.get_render_system().get_light_data().light_datas.size()==2);
        }
        assert(egn.get_render_system().get_light_data().light_datas.empty());
        std::cout<<"OBJ/MTL texture and alpha import, Unicode path, Object configuration and component restoration passed\n";
    }
    egn.shutdown();std::filesystem::remove_all(root);
}
