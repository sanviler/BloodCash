#include "bc/disks.h"
#include "platform/sha256.h"
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace bc {

namespace {
// WHDLoad install of Blood Money by Wepl and StingRay (RawDIC images, identical in v1.4 and v1.5a)
constexpr const char* kSha[2] = {
    "391aca778b46d031f6a68ba8eecdf5b22eedd7da2fb99948431522d9c17ca3cd",
    "7486697df2e67795313d16a57947987bcc6aa75c64c0a23a0a52a9a4a580c18f",
};

bool readAll(const fs::path& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), {});
    return true;
}
} // namespace

Disks& Disks::instance() {
    static Disks d;
    return d;
}

bool Disks::locate(const std::vector<std::string>& folders, std::string& message) {
    for (const auto& f : folders) {
        for (const char* sub : {"", "BloodMoney", "data"}) {
            const fs::path dir = fs::path(f) / sub;
            std::error_code ec;
            if (!fs::exists(dir / "Disk.1", ec) || !fs::exists(dir / "Disk.2", ec)) continue;
            std::vector<uint8_t> a, b;
            if (!readAll(dir / "Disk.1", a) || !readAll(dir / "Disk.2", b)) continue;
            if (a.size() != kImageSize || b.size() != kImageSize) {
                message = "Disk.1 / Disk.2 in " + dir.string() + " are not Blood Money WHDLoad images (wrong size)";
                continue;
            }
            verified_ = dm::sha256Hex(a.data(), a.size()) == kSha[0] && dm::sha256Hex(b.data(), b.size()) == kSha[1];
            image_[0] = std::move(a);
            image_[1] = std::move(b);
            folder_ = dir.string();
            message = verified_ ? "Blood Money disks found" : "Blood Money disks found (unknown version, trying anyway)";
            return true;
        }
    }
    if (message.empty())
        message = "Blood Money files not found: copy Disk.1 and Disk.2 of the WHDLoad install next to the program";
    return false;
}

bool Disks::read(int disk, int side, int track, int count, uint8_t* dst, uint32_t dstSize) const {
    if (disk < 1 || disk > 2 || track < 1 || count < 0 || track - 1 + count > kTracksPerSide) return false;
    const auto& img = image_[disk - 1];
    const size_t off = size_t(track - 1 + (side ? kTracksPerSide : 0)) * kTrackSize;
    const size_t len = size_t(count) * kTrackSize;
    if (img.size() < off + len || len > dstSize) return false;
    std::memcpy(dst, img.data() + off, len);
    return true;
}

} // namespace bc
