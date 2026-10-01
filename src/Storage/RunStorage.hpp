#pragma once
#include "../Recording/RecordedRun.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace afc {

struct StoredRunInfo {
    std::filesystem::path path;
    std::string fileName;
    int levelID = 0;
    std::string levelName;
    int presses = 0;
    int durationTicks = 0;
};

// Plain JSON files (.json) in a directory chosen by the caller (the mod passes
// Mod::get()->getSaveDir()). No hard-coded paths. All operations report errors.
class RunStorage {
public:
    explicit RunStorage(std::filesystem::path dir) : m_dir(std::move(dir)) {}
    bool save(RecordedRun const& run, std::string& outName, std::string& err) const;
    bool load(std::string const& fileName, RecordedRun& out, std::string& err) const;
    bool remove(std::string const& fileName, std::string& err) const;
    std::vector<StoredRunInfo> list() const;   // corrupted files are skipped
private:
    std::filesystem::path m_dir;
    std::filesystem::path runsDir() const { return m_dir / "runs"; }
};

} // namespace afc
