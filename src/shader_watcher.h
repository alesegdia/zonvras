#pragma once
#include <string>
#include <filesystem>

// Watches a single file and reports when it has been modified.
class ShaderWatcher {
public:
    explicit ShaderWatcher(const std::string& path);

    // Returns true once per modification (edge-triggered)
    bool Changed();

private:
    std::filesystem::path              m_path;
    std::filesystem::file_time_type    m_lastWrite;
};
