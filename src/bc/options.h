#pragma once
// Bits of Machine::setCustom1 for BloodCash.
#include <cstdint>

namespace bc {

constexpr uint32_t kSkipIntro = 1u << 0;   // straight to the title screen
constexpr uint32_t kLives = 1u << 1;       // cheats (high scores are not saved when one is on)
constexpr uint32_t kEnergy = 1u << 2;
constexpr uint32_t kMoney = 1u << 3;
constexpr uint32_t kMaxMoney = 1u << 4;
constexpr uint32_t kCheatMask = kLives | kEnergy | kMoney | kMaxMoney;

bool introRunning();                       // the boot (intro) is running, the game has not started yet

} // namespace bc
