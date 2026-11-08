#ifndef __RENDER_OBJECT_LOADER_HPP
#define __RENDER_OBJECT_LOADER_HPP

#include <string>
#include <assimp/scene.h>
#include <file/config/object_config.hpp>

namespace render{

class object;
class material;
struct mesh_data;

/// @brief 模型文件加载器
enum class geometry_type {cube,sphere,capsule,cylinder,plane,quad};
class object_loader final{
public:
    static object* load_model(const std::filesystem::path& path,object* target=nullptr);
    static object* create_geometry(geometry_type type);
    static object* load(const std::string& path,object* target=nullptr);//UTF-8路径；可替换已有对象的mesh/material。
    static object* load(const object_config& config);
    static object_config describe(const object& value,object_config config);

private:
    static std::string import_source(const std::filesystem::path& path);
    static std::string geometry_source(geometry_type type);
    static void load_mesh(const aiMesh* source_mesh,mesh_data& data);//读取 mesh 数据
    static void load_material(const aiMaterial* source_material,material* mat);//读取材质数据
    static void load_textures(const aiScene* scene,const aiMaterial* source,material* mat,const std::filesystem::path& path);
};

}

#endif // __RENDER_OBJECT_LOADER_HPP
