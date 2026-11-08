#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace render {
std::filesystem::path path_from_utf8(std::string_view text);
std::string path_to_utf8(const std::filesystem::path& path);
}
