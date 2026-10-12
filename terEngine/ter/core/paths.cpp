#include "ter/core/paths.h"

#include <mach-o/dyld.h>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace ter {

namespace {
std::string& assetRootOverride() {
    static std::string dir;
    return dir;
}
}  // namespace

std::string executableDir() {
    static const std::string dir = [] {
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::vector<char> buf(size + 1, 0);
        if (_NSGetExecutablePath(buf.data(), &size) != 0) return std::string(".");
        std::error_code ec;
        // resolve symlinks and ".." so the bundle check below sees the real layout
        fs::path p = fs::weakly_canonical(fs::path(buf.data()), ec);
        if (ec) p = fs::path(buf.data());
        return p.parent_path().string();
    }();
    return dir;
}

std::string resourceDir() {
    static const std::string dir = [] {
        fs::path exe = executableDir();
        // MyGame.app/Contents/MacOS -> MyGame.app/Contents/Resources
        if (exe.filename() == "MacOS" && exe.parent_path().filename() == "Contents") {
            fs::path res = exe.parent_path() / "Resources";
            std::error_code ec;
            if (fs::is_directory(res, ec)) return res.string();
        }
        return exe.string();
    }();
    return dir;
}

std::string assetPath(const std::string& relative) {
    const std::string& root = assetRootOverride();
    return (fs::path(root.empty() ? resourceDir() + "/assets" : root) / relative).string();
}

void setAssetRoot(const std::string& dir) { assetRootOverride() = dir; }

}  // namespace ter
