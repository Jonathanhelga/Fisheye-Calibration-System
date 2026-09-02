#include "Measure3dTypes.h"

#include <filesystem>

namespace Measure3d {

std::string getOutputDir(const std::string &prefix) {
    namespace fs = std::filesystem;
    fs::path base = fs::path("image_cali") / "output_3D";
    fs::path dir = prefix.empty() ? base : base / prefix;
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string();
}

}  // namespace Measure3d
