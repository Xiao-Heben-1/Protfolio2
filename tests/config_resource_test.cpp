#include <file/file_system.hpp>
#include <file/config/scene_config.hpp>
#include <file/config/object_config.hpp>
#include <file/tools/guid.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
using namespace render;
void test_config_resources(){
    const auto root=std::filesystem::absolute(std::filesystem::path("config_test_data")/generate_guid());
    std::filesystem::create_directories(root);
    scene_config scene;scene.name="component-config-scene";
    scene.skybox.ground_color={.1f,.2f,.4f};scene.skybox.exposure=.8f;scene.skybox.view_sample_count=24;
    scene.main_camera.position={3,4,5};scene.main_camera.direction={0,-1,0};
    scene.main_camera.fov=60;scene.main_camera.aspect=2;scene.main_camera.near_plane=.5f;scene.main_camera.far_plane=4000;
    scene.main_camera.navigation={8,.001f,.002f,2,3};scene.main_camera.dis={64,256,512,768};scene.main_camera.width=1920;scene.main_camera.height=960;
    scene.main_camera.g_buffer_format=VK_FORMAT_R32G32B32A32_SFLOAT;scene.main_camera.depth_format=VK_FORMAT_D32_SFLOAT;
    for(auto type:{light_type::main,light_type::point,light_type::directional}){
        light_config light;light.name="light";light.type=type;light.position={1,2,3};light.direction={1,0,0};light.color={.1f,.2f,.3f};
        light.emission=4;light.k1=.4f;light.k2=.8f;scene.lights.push_back(light);
    }
    object_config object;object.name="model";object.is_transparent=true;object.behavior=true;
    object.transform.position={-1,2,3};object.transform.rotation={.1f,.2f,.3f};object.transform.scale={2,3,4};
    object.mesh.emplace();object.mesh->source_guid=generate_guid();
    object.mesh->lod_guids={generate_guid(),generate_guid(),generate_guid()};object.mesh->bounds=AABB({-2,-3,-4},{5,6,7});
    object.material.emplace();object.material->guids={generate_guid(),generate_guid()};
    material_override_config material;material.slot=1;material.base_color=std::array<float,4>{.2f,.3f,.4f,.5f};
    material.emissive_color=std::array<float,4>{1,2,3,4};material.metallic=.2f;material.roughness=.6f;material.ao=.9f;
    material.texture_channels=std::array<uint32_t,4>{2,1,1,3};material.emissive_strength=2;material.alpha_mode=2;material.alpha_cutoff=.7f;material.shader_guid=generate_guid();
    for(const auto* slot:{"base_color","normal","metallic","roughness","ao","emissive"})material.textures[slot]=generate_guid();
    object.material->overrides.push_back(material);scene.objects.push_back(object);scene.objects.emplace_back();
    const auto verify_object=[&](const object_config& value){
        assert(value.guid==object.guid && value.name==object.name && value.is_transparent && value.behavior);
        assert(value.transform.position==object.transform.position && value.transform.rotation==object.transform.rotation && value.transform.scale==object.transform.scale);
        assert(value.mesh && value.mesh->source_guid==object.mesh->source_guid && value.mesh->lod_guids==object.mesh->lod_guids && value.mesh->bounds==object.mesh->bounds);
        assert(value.material && value.material->guids==object.material->guids && value.material->overrides.size()==1);
        const auto& m=value.material->overrides[0];
        assert(m.texture_channels==material.texture_channels);
        assert(m.slot==material.slot && m.base_color==material.base_color && m.emissive_color==material.emissive_color);
        assert(m.metallic==material.metallic && m.roughness==material.roughness && m.ao==material.ao && m.emissive_strength==material.emissive_strength);
        assert(m.alpha_mode==material.alpha_mode && m.alpha_cutoff==material.alpha_cutoff && m.shader_guid==material.shader_guid && m.textures==material.textures);
    };
    const auto verify_scene=[&](const scene_config& value){
        assert(value.guid==scene.guid && value.name==scene.name && value.objects.size()==2 && value.lights.size()==3);
        assert(value.skybox.ground_color==scene.skybox.ground_color && value.skybox.exposure==scene.skybox.exposure && value.skybox.view_sample_count==24);
        const auto& c=value.main_camera;const auto& expected=scene.main_camera;
        assert(c.position==expected.position && c.direction==expected.direction && c.fov==expected.fov && c.aspect==expected.aspect);
        assert(c.near_plane==expected.near_plane && c.far_plane==expected.far_plane && c.dis==expected.dis);
        assert(c.navigation==expected.navigation);
        assert(c.width==expected.width && c.height==expected.height && c.g_buffer_format==expected.g_buffer_format && c.depth_format==expected.depth_format);
        verify_object(value.objects[0]);assert(!value.objects[1].mesh && !value.objects[1].material);
        for(size_t i=0;i<3;++i){const auto& light=value.lights[i];const auto& expected_light=scene.lights[i];
            assert(light.guid==expected_light.guid && light.name==expected_light.name && light.type==expected_light.type);
            if(light.type==light_type::main){
                assert(light.direction==expected_light.direction && light.solar_irradiance==expected_light.solar_irradiance);
                assert(light.angular_radius==expected_light.angular_radius && light.disk_intensity==expected_light.disk_intensity);
            }else{
                assert(light.color==expected_light.color && light.emission==expected_light.emission);
                if(light.type==light_type::point)assert(light.position==expected_light.position && light.k1==expected_light.k1 && light.k2==expected_light.k2);
                else assert(light.direction==expected_light.direction);
            }
        }
    };
    {
        file_system fs(root);
        fs.get_scan().save_scene(scene.guid,"world.scene",scene);
        fs.get_scan().save_object(object.guid,"model.object",object);
        verify_scene(*fs.get_resources().get_scene_data().get_scene(scene.guid));
        verify_object(*fs.get_resources().get_object_data().get_object(object.guid));
        const auto& info=fs.get_files().at(object.guid);
        assert(info.type==file_type::object && info.source_guid==object.mesh->source_guid && info.lod_guids==object.mesh->lod_guids);
        assert(info.bounds==*object.mesh->bounds && info.transparent);
        fs.get_scan().save_scene(scene.guid);fs.get_scan().save_object(object.guid);
        verify_scene(scene_config::load(root/"world.scene"));verify_object(object_config::load(root/"model.object"));
        for(const auto& [path,guid]:{std::pair{"world.scene",scene.guid},std::pair{"model.object",object.guid}}){
            std::ifstream in(root/path);std::string first;std::getline(in,first);assert(first==guid);
        }
    }
    {
        const auto path=root/"old_sun.object";
        std::ofstream out(path);out<<generate_guid()<<"\n"<<R"({"lights":[{"type":"main","color":[1,0.5,0.25],"emission":4,"position":[7,8,9],"k1":0.7}]})";out.close();
        const auto sun=object_config::load(path).lights[0];
        assert((sun.solar_irradiance==std::array<float,3>{4,2,1}));
        file_system fs(root);auto object=object_config::load(path);fs.get_scan().save_object(object.guid,path,object);
        std::ifstream input(path);const std::string text((std::istreambuf_iterator<char>(input)),{});
        assert(text.find("solar_irradiance")!=std::string::npos && text.find("\"k1\"")==std::string::npos);
    }
    // 无sidecar时由首行恢复原GUID。
    std::filesystem::remove(root/"world.scene.guid");std::filesystem::remove(root/"model.object.guid");
    const auto legacy_guid=generate_guid();
    {std::ofstream out(root/"legacy.scene");out<<"{\"uid\":\""<<legacy_guid<<R"(","objects":[{"uid":"legacy-object","IsTransparent":true,"mesh_uid":"legacy-mesh","material_uids":["legacy-material"],"material_overrides":[{"slot":0,"shader_uid":"legacy-shader"}]}]})";}
    {
        file_system fs(root);verify_scene(*fs.get_resources().get_scene_data().get_scene(scene.guid));
        verify_object(*fs.get_resources().get_object_data().get_object(object.guid));
        const auto& legacy=*fs.get_resources().get_scene_data().get_scene(legacy_guid);
        assert((legacy.main_camera.dis==std::array<float,4>{128,512,512,1024}));
        const auto& item=legacy.objects[0];assert(item.guid=="legacy-object" && item.is_transparent);
        assert(item.mesh->source_guid=="legacy-mesh" && !item.mesh->bounds && item.mesh->lod_guids[0].empty());
        assert(item.material->guids[0]=="legacy-material" && item.material->overrides[0].shader_guid=="legacy-shader");
        assert(!item.material->overrides[0].alpha_mode && !item.material->overrides[0].base_color);
        fs.get_scan().save_scene(legacy_guid);
        assert(scene_config::load(root/"legacy.scene").objects[0].mesh->source_guid=="legacy-mesh");
        auto* cached=fs.get_resources().get_object_data().get_object(object.guid);
        cached->mesh.reset();cached->material.reset();cached->is_transparent=false;
        fs.get_scan().save_object(object.guid);
        assert(fs.get_files().at(object.guid).source_guid.empty() && !fs.get_files().at(object.guid).transparent);
        assert(!object_config::load(root/"model.object").mesh);
        fs.get_scan().delete_file(object.guid);
        assert(fs.get_resources().get_object_data().get_object(object.guid)==nullptr);
    }
    std::cout<<"Scene/object/component config roundtrip, GUID header and legacy migration tests passed\n";
}
