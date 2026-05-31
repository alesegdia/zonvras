#include "shader_watcher.h"

ShaderWatcher::ShaderWatcher(const std::string& path)
    : m_path(path)
{
    std::error_code ec;
    m_lastWrite = std::filesystem::last_write_time(m_path, ec);
}

bool ShaderWatcher::Changed() {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(m_path, ec);
    if (ec) return false;
    if (t != m_lastWrite) {
        m_lastWrite = t;
        return true;
    }
    return false;
}
