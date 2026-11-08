#include <file/file_system.hpp>
#include <file/config/render_pass/pass_config.hpp>
#include <file/config/render_pass/shader_config.hpp>
#include <file/config/render_pass/sub_shader_config.hpp>
#include <file/tools/library_cache.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
using namespace render;
namespace {
std::span<const uint8_t> bytes(const std::string& text){return {reinterpret_cast<const uint8_t*>(text.data()),text.size()};}
}
void test_render_pass_configs(){
    const auto base=std::filesystem::absolute(std::filesystem::path("pass_config_test_data")/generate_guid());
    std::filesystem::create_directories(base/"workspace with spaces");
    workspace_config relative;
    relative.workspace_root="workspace with spaces";relative.library_root="../external library";
    relative.shader_compiler=TEST_GLSLC_EXECUTABLE;relative.shader_include_roots={"includes"};
    relative.save(base/"workspace.json");
    const auto config=workspace_config::load(base/"workspace.json");
    assert(config.workspace_root==base/"workspace with spaces" && config.library_root==base/"external library");
    assert(config.builtin_shader_root==base/"external library"/"builtin");
    assert(config.shader_include_roots[0]==base/"workspace with spaces"/"includes");
    pass_config pass;pass.name="culling";pass.stage=pass_stage::pre_processing;pass.kind=pass_kind::compute;
    pass.order=9;pass.depth_test=false;pass.depth_write=false;pass.topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    pass.cull_mode=VK_CULL_MODE_BACK_BIT;pass.polygon_mode=VK_POLYGON_MODE_LINE;pass.front_face=VK_FRONT_FACE_CLOCKWISE;
    pass.depth_compare=VK_COMPARE_OP_GREATER;pass.color_blends.emplace_back();
    auto& blend=pass.color_blends[0];blend.enabled=true;blend.src_color=VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dst_color=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;blend.color_op=VK_BLEND_OP_MAX;
    blend.src_alpha=VK_BLEND_FACTOR_DST_ALPHA;blend.dst_alpha=VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    blend.alpha_op=VK_BLEND_OP_MIN;blend.write_mask=VK_COLOR_COMPONENT_R_BIT;
    sub_shader_config sub;sub.name="compute variant";sub.stage=shader_stage::compute;sub.entry_point="entry_a";
    sub.defines={{"VALUE","7"}};sub.include_roots={"includes"};
    shader_config shader;shader.name="culling shader";shader.sub_shader_guids={sub.guid};pass.shader_guid=shader.guid;
    std::filesystem::path cached,texture_cache;std::string first_spirv,changed_spirv,texture_guid;
    {
        file_system fs(config);auto& scan=fs.get_scan();
        texture_guid=scan.create_file("textures/pixel.ppm",bytes(std::string("P6\n1 1\n255\n")+std::string(3,char(127))));
        texture_cache=scan.get_library_path(texture_guid);
        assert(texture_cache==config.library_root/"textures"/(texture_guid+".dds"));
        assert(std::filesystem::exists(texture_cache));
        scan.create_file("includes/settings.glsl",bytes("#define FACTOR 3\n"));
        sub.source_guid=scan.create_file("effects/source.hlsl",bytes(
            "#include \"settings.glsl\"\nRWStructuredBuffer<uint> outputData : register(u0);\n"
            "[numthreads(1,1,1)] void entry_a(uint3 id:SV_DispatchThreadID){outputData[id.x]=VALUE*FACTOR;}\n"
            "[numthreads(1,1,1)] void entry_b(uint3 id:SV_DispatchThreadID){outputData[id.x]=VALUE+FACTOR;}\n"));
        scan.save_sub_shader("passes/cull.sub_shader",sub);scan.save_shader("passes/cull.shader",shader);scan.save_pass("passes/cull.pass",pass);
        auto& data=fs.get_resources().get_render_pass_data();
        assert(data.get_shader(shader.guid)->sub_shader_guids==shader.sub_shader_guids);
        const auto* stage=data.get_sub_shader(sub.guid);
        assert(stage->stage==sub.stage && stage->entry_point==sub.entry_point && stage->defines==sub.defines && stage->include_roots==sub.include_roots);
        const auto* loaded=data.get_pass(pass.guid);
        assert(loaded->stage==pass.stage && loaded->kind==pass.kind && loaded->shader_guid==shader.guid && loaded->order==9);
        assert(loaded->topology==pass.topology && loaded->polygon_mode==pass.polygon_mode && loaded->cull_mode==pass.cull_mode && loaded->front_face==pass.front_face);
        assert(!loaded->depth_test && !loaded->depth_write && loaded->depth_compare==pass.depth_compare);
        const auto& b=loaded->color_blends[0];
        assert(b.enabled && b.src_color==blend.src_color && b.dst_color==blend.dst_color && b.color_op==blend.color_op);
        assert(b.src_alpha==blend.src_alpha && b.dst_alpha==blend.dst_alpha && b.alpha_op==blend.alpha_op && b.write_mask==blend.write_mask);
        assert(fs.get_files().at(pass.guid).type==file_type::render_pass && fs.get_files().at(shader.guid).type==file_type::shader_config && fs.get_files().at(sub.guid).type==file_type::sub_shader);
        cached=scan.get_library_path(sub.guid);
        assert(cached.lexically_relative(config.library_root).begin()->string()=="spirv");
        assert(std::filesystem::exists(cached) && !std::filesystem::exists(config.workspace_root/"library"));
        first_spirv=library_cache::hash_file(cached);
        const auto timestamp=std::filesystem::last_write_time(cached);scan.save_sub_shader("passes/cull.sub_shader",sub);
        assert(std::filesystem::last_write_time(cached)==timestamp);
        sub.defines["VALUE"]="11";scan.save_sub_shader("passes/cull.sub_shader",sub);
        assert(library_cache::hash_file(cached)!=first_spirv);first_spirv=library_cache::hash_file(cached);
        sub.entry_point="entry_b";scan.save_sub_shader("passes/cull.sub_shader",sub);
        assert(library_cache::hash_file(cached)!=first_spirv);first_spirv=library_cache::hash_file(cached);
        scan.create_file("includes/settings.glsl",bytes("#define FACTOR 6\n"));fs.update();
        changed_spirv=library_cache::hash_file(cached);assert(changed_spirv!=first_spirv);
        // 所有阶段均可保存，启用状态及同阶段顺序决定查询结果。
        for(auto stage:{pass_stage::opaque,pass_stage::transparent_capture,pass_stage::transparent_sort,pass_stage::transparent_resolve,pass_stage::post_processing}){
            pass_config item;item.stage=stage;scan.save_pass("passes/"+item.guid+".pass",item);
            assert(data.get_pass(item.guid)->stage==stage);
        }
        pass_config earlier;earlier.stage=pass.stage;earlier.order=1;scan.save_pass("passes/earlier.pass",earlier);
        pass_config disabled;disabled.stage=pass.stage;disabled.enabled=false;scan.save_pass("passes/disabled.pass",disabled);
        const auto ordered=data.get_passes(pass.stage);assert(ordered.size()==2 && ordered[0]->guid==earlier.guid && ordered[1]->guid==pass.guid);
        for(const auto& name:{"cull.pass","cull.shader","cull.sub_shader"}){
            std::ifstream in(config.workspace_root/"passes"/name);std::string line;std::getline(in,line);assert(line.size()==32);
        }
        for(auto stage:{shader_stage::vertex,shader_stage::fragment,shader_stage::geometry,shader_stage::tess_control,shader_stage::tess_evaluation}){
            sub_shader_config item;item.stage=stage;
            scan.save_sub_shader("passes/"+item.guid+".sub_shader",item);
            assert(data.get_sub_shader(item.guid)->stage==stage);
        }
    }
    std::filesystem::remove(config.workspace_root/"passes/cull.sub_shader.guid");
    {
        file_system fs(config);
        assert(fs.get_scan().get_library_path(texture_guid)==texture_cache);
        assert(std::filesystem::exists(texture_cache));
        fs.get_scan().delete_file(texture_guid);
        assert(!std::filesystem::exists(texture_cache) && !std::filesystem::exists(texture_cache.string()+".texture_config"));
        assert(fs.get_resources().get_render_pass_data().get_sub_shader(sub.guid)->entry_point=="entry_b");
        assert(fs.get_scan().get_library_path(sub.guid)==cached && library_cache::hash_file(cached)==changed_spirv);
        fs.get_scan().delete_file(sub.source_guid);fs.update();assert(!std::filesystem::exists(cached));
        fs.get_scan().delete_file(sub.guid);assert(!fs.get_resources().get_render_pass_data().get_sub_shader(sub.guid));
        fs.get_scan().delete_file(shader.guid);assert(!fs.get_resources().get_render_pass_data().get_shader(shader.guid));
        fs.get_scan().delete_file(pass.guid);assert(!fs.get_resources().get_render_pass_data().get_pass(pass.guid));
    }
    // 任意名称的内置缓存目录不进入源索引；旧的 library 名称可以正常作为源目录。
    workspace_config nested;nested.workspace_root=base/"nested";nested.library_root="generated/cache/";
    std::filesystem::create_directories(nested.workspace_root/"generated/cache");
    std::filesystem::create_directories(nested.workspace_root/"library");
    {std::ofstream out(nested.workspace_root/"generated/cache/ignore.obj");out<<"cache";}
    {std::ofstream out(nested.workspace_root/"library/keep.obj");out<<"source";}
    {
        file_system fs(nested);assert(fs.get_files().size()==1);
        assert(fs.get_files().begin()->second.path==std::filesystem::path("library/keep.obj"));
        fs.get_scan().create_file("generated/cache/new.obj",bytes("not a source"));fs.update();
        assert(fs.get_files().size()==1);
    }
    // 系统定义只从 engine 读取，生成配置与用户覆盖都不污染 src。
    workspace_config production;production.workspace_root=base/"production";production.source_root="src";
    production.engine_root=base/"engine";
    const auto definitions=production.engine_root/"config/system/render_pass";
    std::filesystem::create_directories(definitions);
    const auto system_guid=generate_guid();
    {std::ofstream out(definitions/"test.pass");out<<system_guid<<"\n{\"name\":\"system pass\",\"enabled\":true}\n";}
    const auto definition_hash=library_cache::hash_file(definitions/"test.pass");
    {
        file_system fs(production);const auto info=fs.get_files().at(system_guid);
        assert(info.generated && info.path==production.workspace_root/"library/generated/render_pass/test.pass");
        assert(std::filesystem::is_empty(production.workspace_root/"src"));
        auto edited=*fs.get_resources().get_render_pass_data().get_pass(system_guid);edited.enabled=false;
        fs.get_scan().save_pass(info.path,edited);fs.get_scan().save_project_config(system_guid);
        assert(std::filesystem::exists(production.workspace_root/"config/render_pass/test.pass"));
    }
    std::filesystem::remove_all(production.workspace_root/"library");
    {
        file_system fs(production);assert(!fs.get_resources().get_render_pass_data().get_pass(system_guid)->enabled);
        assert(std::filesystem::is_empty(production.workspace_root/"src"));
        assert(library_cache::hash_file(definitions/"test.pass")==definition_hash);
    }
    std::cout<<"Pass/shader/sub-shader configs, external/nested roots, shader variants and include invalidation passed\n";
}
