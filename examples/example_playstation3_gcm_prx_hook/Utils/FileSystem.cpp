#include "FileSystem.hpp"

sys_prx_id_t GetModuleHandle(const char* moduleName)
{
    return (moduleName) ? sys_prx_get_module_id_by_name(moduleName, 0, nullptr) : sys_prx_get_my_module_id();
}

sys_prx_module_info_t GetModuleInfo(sys_prx_id_t handle)
{
    sys_prx_module_info_t info{};
    static sys_prx_segment_info_t segments[10]{};
    static char filename[SYS_PRX_MODULE_FILENAME_SIZE]{};

    memset(segments, 0, sizeof(segments));
    memset(filename, 0, sizeof(filename));

    info.size = sizeof(info);
    info.segments = segments;
    info.segments_num = sizeof(segments) / sizeof(sys_prx_segment_info_t);
    info.filename = filename;
    info.filename_size = sizeof(filename);

    sys_prx_get_module_info(handle, 0, &info);
    return info;
}

std::string GetModuleFilePath(const char* moduleName)
{
    sys_prx_module_info_t info = GetModuleInfo(GetModuleHandle(moduleName));
    return std::string(info.filename);
}

std::string RemoveBaseNameFromPath(const std::string& filePath)
{
    size_t lastpath = filePath.find_last_of("/");
    if (lastpath == std::string::npos)
        return filePath;
    return filePath.substr(0, lastpath);
}

std::string GetFileName(const std::string& path)
{
    size_t lastslash = path.find_last_of("/");
    if (lastslash == std::string::npos)
        return path;
    return path.substr(lastslash + 1, path.length());
}

std::string GetCurrentDir()
{
    static std::string cachedModulePath;
    if (cachedModulePath.empty())
    {
        std::string path = RemoveBaseNameFromPath(GetModuleFilePath(nullptr));

        path += "/";  // include trailing slash

        cachedModulePath = path;
    }
    return cachedModulePath;
}