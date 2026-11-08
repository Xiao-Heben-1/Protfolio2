#include <core/object/object_loader.hpp>
#include <core/object/object.hpp>
#include <core/component/component_enter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <memory>
#include <tools/model/QEM.hpp>
#include <file/config/resource/mesh_lod_resource.hpp>
#include <engine.hpp>
#include <file/tools/file_path.hpp>
#include <file/tools/library_cache.hpp>
#include <fstream>
#include <set>
#include <sstream>
#include <glm/gtc/constants.hpp>
namespace render{
object* object_loader::load_model(const std::filesystem::path& path,object* target){
    const auto guid=import_source(path);
    return load(path_to_utf8(egn.get_file_system().get_scan().get_source_path(guid)),target);
}
object* object_loader::create_geometry(geometry_type type){
    constexpr const char* names[]={"Cube","Sphere","Capsule","Cylinder","Plane","Quad"};
    const auto source=geometry_source(type);auto& fs=egn.get_file_system();
    const auto guid=fs.get_scan().create_file(fs.get_config().library_root/"generated/builtin_geometry"/(std::array<const char*,6>{"cube","sphere","capsule","cylinder","plane","quad"}[size_t(type)]+std::string(".obj")),
        {reinterpret_cast<const uint8_t*>(source.data()),source.size()});
    object_config config;config.name=names[size_t(type)];config.mesh.emplace();config.mesh->source_guid=guid;
    config.material.emplace();config.material->overrides.emplace_back();
    config.material->overrides.front().base_color=std::array<float,4>{.72f,.72f,.72f,1};
    config.material->overrides.front().roughness=.65f;
    auto* result=load(config);
    result->get_mesh()->builtin_geometry=std::array<const char*,6>{"cube","sphere","capsule","cylinder","plane","quad"}[size_t(type)];
    return result;
}
std::string object_loader::import_source(const std::filesystem::path& input){
    auto& scan=egn.get_file_system().get_scan();const auto& root_path=scan.root_path;
    const auto source=std::filesystem::weakly_canonical(input);
    if(scan.in_library(source)){
        scan.register_generated(source);return scan.path_mp.at(source.generic_string());
    }
    const auto relative=source.lexically_relative(root_path);
    if(!relative.empty() && *relative.begin()!=L".."){
        scan.add_file(relative);return scan.path_mp.at(relative.generic_string());
    }
    Assimp::Importer importer;
    const auto* model=importer.ReadFile(path_to_utf8(source),0);
    if(!model)throw std::runtime_error(importer.GetErrorString());
    std::set<std::filesystem::path> dependencies{source};
    for(unsigned m=0;m<model->mNumMaterials;++m)for(int t=1;t<=int(AI_TEXTURE_TYPE_MAX);++t){
        const auto type=aiTextureType(t);
        for(unsigned i=0;i<model->mMaterials[m]->GetTextureCount(type);++i){
            aiString name;model->mMaterials[m]->GetTexture(type,i,&name);
            if(name.length==0 || name.C_Str()[0]=='*')continue;
            auto texture=source.parent_path()/path_from_utf8(name.C_Str());
            if(!std::filesystem::exists(texture)){
                for(const auto& entry:std::filesystem::recursive_directory_iterator(source.parent_path()))
                    if(entry.is_regular_file() && entry.path().filename()==texture.filename()){texture=entry.path();break;}
            }
            if(std::filesystem::exists(texture))dependencies.insert(std::filesystem::weakly_canonical(texture));
        }
    }
    if(source.extension()==".obj" || source.extension()==".OBJ"){
        std::ifstream stream(source);std::string line;
        while(std::getline(stream,line))if(line.starts_with("mtllib ")){
            auto name=line.substr(7);if(!name.empty() && name.back()=='\r')name.pop_back();
            const auto material=source.parent_path()/path_from_utf8(name);
            if(std::filesystem::exists(material))dependencies.insert(std::filesystem::weakly_canonical(material));
        }
    }
    if(source.extension()==".gltf"){
        const auto bytes=library_cache::read(source);rapidjson::Document json;
        json.Parse(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        if(json.HasMember("buffers"))for(const auto& buffer:json["buffers"].GetArray())if(buffer.HasMember("uri")){
            const std::string uri=buffer["uri"].GetString();if(uri.starts_with("data:"))continue;
            std::string decoded;
            for(size_t i=0;i<uri.size();++i){if(uri[i]=='%' && i+2<uri.size()){decoded+=char(std::stoul(uri.substr(i+1,2),nullptr,16));i+=2;}else decoded+=uri[i];}
            dependencies.insert(std::filesystem::weakly_canonical(source.parent_path()/path_from_utf8(decoded)));
        }
    }
    auto base=source.parent_path();
    for(const auto& path:dependencies)if(path.root_name()==base.root_name())
        while(*path.lexically_relative(base).begin()==L"..")base=base.parent_path();
    const auto target=root_path/"imported_models"/(path_to_utf8(source.stem())+"_"+library_cache::hash(path_to_utf8(source)).substr(0,8));
    for(const auto& path:dependencies){
        const auto destination=target/(path.root_name()==base.root_name() ? path.lexically_relative(base) : std::filesystem::path("Textures")/path.filename());
        std::filesystem::create_directories(destination.parent_path());
        std::filesystem::copy_file(path,destination,std::filesystem::copy_options::overwrite_existing);
        // 旧测试空间已经有 GUID 时沿用引用，保留旧 .scene/.object 的关联。
        if(std::filesystem::exists(path.wstring()+L".guid") && !std::filesystem::exists(destination.wstring()+L".guid"))
            std::filesystem::copy_file(path.wstring()+L".guid",destination.wstring()+L".guid");
    }
    scan.scan(target.lexically_relative(root_path));
    return scan.path_mp.at((target/source.lexically_relative(base)).lexically_relative(root_path).generic_string());
}
std::string object_loader::geometry_source(geometry_type kind){
    struct vertex {glm::vec3 p,n;glm::vec2 uv;};
    std::vector<vertex> vertices;std::vector<std::array<int,3>> faces;
    const auto triangle=[&](int a,int b,int c,glm::vec3 normal){
        const auto cross=glm::cross(vertices[b].p-vertices[a].p,vertices[c].p-vertices[a].p);
        if(glm::dot(cross,cross)<1e-12f)return;
        if(glm::dot(cross,normal)<0)std::swap(b,c);
        faces.push_back({a,b,c});
    };
    const auto quad=[&](glm::vec3 center,glm::vec3 u,glm::vec3 v){
        const auto n=glm::normalize(glm::cross(u,v));const int first=int(vertices.size());
        vertices.push_back({center-u-v,n,{0,0}});vertices.push_back({center+u-v,n,{1,0}});
        vertices.push_back({center+u+v,n,{1,1}});vertices.push_back({center-u+v,n,{0,1}});
        triangle(first,first+1,first+2,n);triangle(first,first+2,first+3,n);
    };
    if(kind==geometry_type::cube){
        quad({.5f,0,0},{0,0,-.5f},{0,.5f,0});quad({-.5f,0,0},{0,0,.5f},{0,.5f,0});
        quad({0,.5f,0},{.5f,0,0},{0,0,-.5f});quad({0,-.5f,0},{.5f,0,0},{0,0,.5f});
        quad({0,0,.5f},{.5f,0,0},{0,.5f,0});quad({0,0,-.5f},{-.5f,0,0},{0,.5f,0});
    }else if(kind==geometry_type::plane)quad({0,0,0},{5,0,0},{0,0,-5});
    else if(kind==geometry_type::quad)quad({0,0,0},{.5f,0,0},{0,.5f,0});
    else if(kind==geometry_type::cylinder){
        constexpr int segments=32;
        for(int i=0;i<=segments;++i){
            const float a=glm::two_pi<float>()*i/segments;const glm::vec3 n{std::cos(a),0,std::sin(a)};
            vertices.push_back({n*.5f+glm::vec3(0,-1,0),n,{float(i)/segments,0}});
            vertices.push_back({n*.5f+glm::vec3(0,1,0),n,{float(i)/segments,1}});
            if(i){const int b=i*2;triangle(b-2,b,b+1,n);triangle(b-2,b+1,b-1,n);}
        }
        for(float y:{-1.f,1.f}){
            const int center=int(vertices.size());const glm::vec3 n{0,y,0};vertices.push_back({{0,y,0},n,{.5f,.5f}});
            for(int i=0;i<=segments;++i){const float a=glm::two_pi<float>()*i/segments;
                const float x=.5f*std::cos(a),z=.5f*std::sin(a);vertices.push_back({{x,y,z},n,{x+.5f,z+.5f}});
                if(i)triangle(center,center+i,center+i+1,n);
            }
        }
    }else{
        constexpr int rows=24,cols=32;
        const bool capsule=kind==geometry_type::capsule;
        // 胶囊额外的一圈把上下半球的赤道连接成圆柱。
        const int rings=rows+(capsule ? 1 : 0);
        for(int row=0;row<=rings;++row){
            const int latitude=capsule && row>rows/2 ? row-1 : row;
            const float theta=glm::pi<float>()*latitude/rows;
            const float offset=capsule ? (row<=rows/2 ? .5f : -.5f) : 0.f;
            for(int col=0;col<=cols;++col){
                const float phi=glm::two_pi<float>()*col/cols;
                const glm::vec3 n{std::sin(theta)*std::cos(phi),std::cos(theta),std::sin(theta)*std::sin(phi)};
                vertices.push_back({n*.5f+glm::vec3(0,offset,0),n,{float(col)/cols,1.f-float(row)/rings}});
                if(row && col){const int a=row*(cols+1)+col,b=a-(cols+1);triangle(a-1,b-1,b,n);triangle(a-1,b,a,n);}
            }
        }
    }
    std::ostringstream out;out<<"# Render primitive\n";
    for(const auto& v:vertices)out<<"v "<<v.p.x<<' '<<v.p.y<<' '<<v.p.z<<'\n';
    for(const auto& v:vertices)out<<"vt "<<v.uv.x<<' '<<v.uv.y<<'\n';
    for(const auto& v:vertices)out<<"vn "<<v.n.x<<' '<<v.n.y<<' '<<v.n.z<<'\n';
    for(const auto& f:faces){out<<'f';for(int i:f)out<<' '<<i+1<<'/'<<i+1<<'/'<<i+1;out<<'\n';}
    return out.str();
}


object* object_loader::load(const std::string& input,object* target){
    const auto guid=import_source(path_from_utf8(input));
    const auto path=path_to_utf8(egn.get_file_system().get_scan().get_source_path(guid));
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,aiProcess_Triangulate|aiProcess_GenSmoothNormals|aiProcess_JoinIdenticalVertices|aiProcess_PreTransformVertices|aiProcess_FlipUVs|aiProcess_SortByPType);
    if(scene == nullptr){
        throw std::runtime_error(importer.GetErrorString());
    }
    auto allocated=target ? std::unique_ptr<object>() : std::make_unique<object>();
    auto* obj=target ? target : allocated.get();
    obj->remove_component(obj->get_mesh());obj->remove_component(obj->get_material());
    //创建 mesh component
    mesh* mesh_comp = static_cast<mesh*>(obj->add_component(static_cast<int>(component_type::mesh_type)));
    //创建 material component
    material* material_comp = static_cast<material*>(obj->add_component(static_cast<int>(component_type::material_type)));
    
    //读取全部 material
    bool transparent = false;
    for(uint32_t i = 0;i < scene->mNumMaterials;i++){
        load_material(scene->mMaterials[i],material_comp);
        load_textures(scene,scene->mMaterials[i],material_comp,path_from_utf8(path));
        aiString alpha_mode;
        if(scene->mMaterials[i]->Get(AI_MATKEY_GLTF_ALPHAMODE,alpha_mode) == AI_SUCCESS){
            transparent |= std::string(alpha_mode.C_Str()) == "BLEND";
        }else{
            transparent |= material_comp->sub_materials.back()->uniform_data.base_color.a < 1.0f;
            transparent |= scene->mMaterials[i]->GetTextureCount(aiTextureType_OPACITY) > 0;
        }
    }

    // 材质槽分别进入不透明/透明阶段；Object 标志仅用于显式强制透明。
    auto resources=egn.get_file_system().get_scan().load_mesh_resources(path_from_utf8(path),false,[&]{
        mesh_data data;
        for(uint32_t i=0;i<scene->mNumMeshes;++i)load_mesh(scene->mMeshes[i],data);
        return data;
    },egn.get_render_system().get_page_manager());
    mesh_comp->create_streamed(std::move(resources));
    if(allocated)allocated.release();return obj;
}


void object_loader::load_mesh(const aiMesh* source_mesh,mesh_data& data){
    if(!(source_mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE))return;
    sub_mesh sub;

    sub.front_idx = static_cast<uint32_t>(data.indices.size());
    sub.vertex_off = static_cast<int32_t>(data.vertices.size());
    sub.mat_idx = source_mesh->mMaterialIndex;

    //读取顶点
    for(uint32_t i = 0;i < source_mesh->mNumVertices;i++){
        glm::vec3 position{source_mesh->mVertices[i].x,source_mesh->mVertices[i].y,source_mesh->mVertices[i].z};

        glm::vec2 uv{0.0f,0.0f};

        if(source_mesh->mTextureCoords[0] != nullptr){
            uv.x = source_mesh->mTextureCoords[0][i].x;
            uv.y = source_mesh->mTextureCoords[0][i].y;
        }

        glm::vec3 normal{source_mesh->mNormals[i].x,source_mesh->mNormals[i].y,source_mesh->mNormals[i].z};
        data.vertices.emplace_back(position,uv,normal);
    }

    //读取索引
    for(uint32_t i = 0;i < source_mesh->mNumFaces;i++){
        const aiFace& face = source_mesh->mFaces[i];
        for(uint32_t j = 0;j < face.mNumIndices;j++){
            data.indices.push_back(face.mIndices[j]);
        }
    }
    sub.index_count = static_cast<uint32_t>(data.indices.size()) - sub.front_idx;
    data.sub_meshes.push_back(sub);
}


void object_loader::load_material(const aiMaterial* source_material,material* mat){
    sub_material* sub_mat = mat->add_sub_material();
    aiString name;source_material->Get(AI_MATKEY_NAME,name);sub_mat->name=name.C_Str();

    aiColor4D base_color{1.0f,1.0f,1.0f,1.0f};
    aiColor3D emissive_color{0.0f,0.0f,0.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;

    //基础颜色
    if(source_material->Get(AI_MATKEY_BASE_COLOR,base_color) != AI_SUCCESS){
        aiColor3D diffuse_color{1.0f,1.0f,1.0f};
        if(source_material->Get(AI_MATKEY_COLOR_DIFFUSE,diffuse_color) == AI_SUCCESS){
            base_color = aiColor4D(diffuse_color.r,diffuse_color.g,diffuse_color.b,1.0f);
        }
    }

    //自发光颜色
    source_material->Get(AI_MATKEY_COLOR_EMISSIVE,emissive_color);
    //PBR metallic / roughness
    source_material->Get(AI_MATKEY_METALLIC_FACTOR,metallic);
    source_material->Get(AI_MATKEY_ROUGHNESS_FACTOR,roughness);

    float opacity = 1.0f;
    source_material->Get(AI_MATKEY_OPACITY,opacity);
    // 有些导入器在 base_color.a 和 opacity 中重复保存同一个值，不能再相乘。
    base_color.a = std::clamp(std::min(base_color.a,opacity),0.0f,1.0f);
    sub_mat->uniform_data.base_color = glm::vec4(base_color.r,base_color.g,base_color.b,base_color.a);
    aiString alpha_mode;
    if(source_material->Get(AI_MATKEY_GLTF_ALPHAMODE,alpha_mode) == AI_SUCCESS){
        const std::string mode = alpha_mode.C_Str();
        sub_mat->uniform_data.texture_flags_2.z = mode == "MASK" ? 2u : mode == "BLEND" ? 3u : 1u;
        float cutoff = 0.5f;
        source_material->Get(AI_MATKEY_GLTF_ALPHACUTOFF,cutoff);
        sub_mat->uniform_data.texture_flags_2.w = std::bit_cast<uint32_t>(cutoff);
    }
    sub_mat->uniform_data.emissive_color = glm::vec4(emissive_color.r,emissive_color.g,emissive_color.b,1.0f);

    sub_mat->uniform_data.parameter.x = metallic;
    sub_mat->uniform_data.parameter.y = roughness;
    sub_mat->uniform_data.parameter.z = 1.0f;//AO
    sub_mat->uniform_data.parameter.w = 1.0f;//emissive strength
}

}

