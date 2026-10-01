#include "RunStorage.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace afc {

static bool safeName(std::string const& n) {
    return !n.empty() && n.find('/') == std::string::npos && n.find('\\') == std::string::npos &&
           n.find("..") == std::string::npos;
}

bool RunStorage::save(RecordedRun const& run, std::string& outName, std::string& err) const {
    std::string verr;
    if (!run.validate(verr)) { err = "refusing to save invalid run: " + verr; return false; }
    std::error_code ec;
    fs::create_directories(runsDir(), ec);
    if (ec) { err = "cannot create folder: " + ec.message(); return false; }
    // Wall-clock is used ONLY to make file names unique, never as frame identity.
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string name = "run_" + std::to_string(run.levelID) + "_" + std::to_string(secs) + ".json";
    for (int i = 1; fs::exists(runsDir() / name, ec); ++i)
        name = "run_" + std::to_string(run.levelID) + "_" + std::to_string(secs) + "_" + std::to_string(i) + ".json";
    std::ofstream f(runsDir() / name, std::ios::binary | std::ios::trunc);
    if (!f) { err = "cannot open file for writing"; return false; }
    f << run.toJson().dump();
    f.flush();
    if (!f) { err = "write failed"; return false; }
    outName = name;
    return true;
}

bool RunStorage::load(std::string const& fileName, RecordedRun& out, std::string& err) const {
    if (!safeName(fileName)) { err = "invalid file name"; return false; }
    std::ifstream f(runsDir() / fileName, std::ios::binary);
    if (!f) { err = "cannot open file"; return false; }
    std::stringstream ss; ss << f.rdbuf();
    json::Value v;
    std::string perr;
    if (!json::parse(ss.str(), v, &perr)) { err = "corrupted file: " + perr; return false; }
    return RecordedRun::fromJson(v, out, err);
}

bool RunStorage::remove(std::string const& fileName, std::string& err) const {
    if (!safeName(fileName)) { err = "invalid file name"; return false; }
    std::error_code ec;
    fs::remove(runsDir() / fileName, ec);
    if (ec) { err = ec.message(); return false; }
    return true;
}

std::vector<StoredRunInfo> RunStorage::list() const {
    std::vector<StoredRunInfo> out;
    std::error_code ec;
    if (!fs::exists(runsDir(), ec)) return out;
    for (auto const& e : fs::directory_iterator(runsDir(), ec)) {
        if (!e.is_regular_file() || e.path().extension() != ".json") continue;
        RecordedRun r; std::string err;
        if (!load(e.path().filename().string(), r, err)) continue;
        StoredRunInfo i;
        i.path = e.path(); i.fileName = e.path().filename().string();
        i.levelID = r.levelID; i.levelName = r.levelName;
        i.presses = r.pressCount(); i.durationTicks = r.durationTicks;
        out.push_back(std::move(i));
    }
    std::sort(out.begin(), out.end(), [](auto const& a, auto const& b) { return a.fileName > b.fileName; });
    return out;
}

} // namespace afc
