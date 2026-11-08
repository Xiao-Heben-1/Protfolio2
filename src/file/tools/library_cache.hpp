#ifndef __RENDER_LIBRARY_CACHE_HPP
#define __RENDER_LIBRARY_CACHE_HPP
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cctype>

namespace render::library_cache{
    /// @brief 校验资源UID字符串是否合法。
    /// @param uid 资源UID；仅允许字母、数字、'-'和'_'，长度范围为1~128。
    inline void validate_uid(const std::string& uid){
        if(uid.empty() || uid.size() > 128) throw std::runtime_error("Invalid asset UID");
        for(unsigned char c : uid) if(!std::isalnum(c) && c != '-' && c != '_') throw std::runtime_error("Invalid asset UID");
    }

    /// @brief 以二进制方式读取整个文件。
    /// @param path 文件路径。
    /// @return 文件的完整字节数据。
    inline std::vector<uint8_t> read(const std::filesystem::path& path){
        std::ifstream input(path,std::ios::binary | std::ios::ate);
        if(!input) throw std::runtime_error("Cannot read asset: " + path.string());
        // 从文件末尾获取文件大小。
        auto size = input.tellg();
        if(size < 0) throw std::runtime_error("Cannot read asset size");
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        // 回到文件开头并一次性读取全部内容。
        input.seekg(0);
        if(!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()),bytes.size())) throw std::runtime_error("Incomplete asset read");

        return bytes;
    }

    /// @brief 计算字节数据的64位FNV-1a哈希。
    /// @param bytes 需要计算哈希的数据。
    /// @return 16位十六进制哈希字符串。
    inline std::string hash(const std::string& bytes){
        // FNV-1a offset basis。
        uint64_t value = 14695981039346656037ull;
        // FNV-1a：先异或当前字节，再乘FNV prime。
        for(unsigned char byte : bytes){
            value ^= byte;
            value *= 1099511628211ull;
        }
        // 将64位哈希格式化为固定16位十六进制字符串。
        std::ostringstream output;
        output << std::hex << std::setw(16) << std::setfill('0') << value;
        return output.str();
    }

    /// @brief 读取文件并计算其64位FNV-1a哈希。
    /// @param path 文件路径。
    /// @return 文件内容对应的16位十六进制哈希字符串。
    inline std::string hash_file(const std::filesystem::path& path){
        const auto bytes = read(path);
        return hash(std::string(bytes.begin(),bytes.end()));
    }

    /// @brief 判断指定路径是否位于给定根目录内部。
    /// @param path 待检查路径。
    /// @param root 根目录路径。
    /// @return 位于root内部返回true，否则返回false。
    inline bool inside(const std::filesystem::path& path,const std::filesystem::path& root){
        if(path.empty() || root.empty()) return false;
        const auto relative = std::filesystem::weakly_canonical(path).lexically_relative(std::filesystem::weakly_canonical(root));
        return !relative.empty() && !relative.is_absolute() && *relative.begin() != ".." && relative != ".";
    }

    /// @brief 删除Library目录内部的单个缓存文件。
    /// @param path 待删除的缓存文件路径。
    /// @param root 当前项目的Library根目录。
    inline void remove(const std::filesystem::path& path,const std::filesystem::path& root){
        if(path.empty()) return;

        // 只允许删除当前Library目录内部的文件。
        if(!inside(path,root)) throw std::runtime_error("Cache path is outside library: " + path.string());

        // 只删除普通文件，不递归删除目录。
        if(std::filesystem::is_regular_file(path)) std::filesystem::remove(path);
    }

    /// @brief 将临时缓存文件提交为正式缓存文件。
    /// @param temporary 临时文件路径。
    /// @param target 目标缓存文件路径。
    inline void commit(const std::filesystem::path& temporary,const std::filesystem::path& target){
        // 若目标已经存在则先替换旧缓存。
        if(std::filesystem::exists(target)) std::filesystem::remove(target);

        // 通过rename将完整临时文件提交为正式缓存。
        std::filesystem::rename(temporary,target);
    }

    /// @brief 为指定文件写入对应的.guid侧车文件。
    /// @param path 资源或缓存文件路径。
    /// @param uid 需要写入的资源UID。
    inline void write_uid(const std::filesystem::path& path,const std::string& uid){
        // 已存在相同UID时不重复写入。
        {
            std::ifstream input(path.string() + ".guid");
            std::string previous;
            if(input >> previous && previous == uid) return;
        }

        std::ofstream output(path.string() + ".guid");
        if(!(output << uid)) throw std::runtime_error("Cannot write cache UID");
    }
}
#endif
