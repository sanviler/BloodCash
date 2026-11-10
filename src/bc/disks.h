#pragma once
// The user's Blood Money disks: the two images of the WHDLoad install ("Disk.1", "Disk.2", 979,600 bytes,
// made by RawDIC from the original Psygnosis disks). Each image holds 158 logical tracks of 6,200 bytes:
// 79 tracks of the lower head (side 0) followed by 79 tracks of the upper head (side 1).
// No game data is distributed with BloodCash: the files are recognised by their SHA-256 digests.
#include <cstdint>
#include <string>
#include <vector>

namespace bc {

constexpr uint32_t kTrackSize = 0x1838;      // 6,200 bytes
constexpr int kTracksPerSide = 79;
constexpr uint32_t kImageSize = kTrackSize * kTracksPerSide * 2;

class Disks {
public:
    static Disks& instance();
    // looks for Disk.1 and Disk.2 in each folder (and in a "BloodMoney" or "data" subfolder)
    bool locate(const std::vector<std::string>& folders, std::string& message);
    bool ready() const { return !image_[0].empty() && !image_[1].empty(); }
    const std::string& folder() const { return folder_; }
    // copies "count" tracks starting at "track" (1..79) of side 0/1 of disk 1/2; false if out of range
    bool read(int disk, int side, int track, int count, uint8_t* dst, uint32_t dstSize) const;
    bool verified() const { return verified_; }      // both digests match the known WHDLoad images

private:
    std::vector<uint8_t> image_[2];
    std::string folder_;
    bool verified_ = false;
};

} // namespace bc
