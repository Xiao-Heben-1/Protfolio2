#include <file/tools/file_path.hpp>
namespace render {
std::filesystem::path path_from_utf8(std::string_view text){
    return std::filesystem::path(std::u8string(text.begin(),text.end()));
}
std::string path_to_utf8(const std::filesystem::path& path){
    const auto text=path.generic_u8string();
    return {text.begin(),text.end()};
}
}
