#pragma once
// The texts of Blood Money (title, shop, game over, high scores): a table of records in the game code,
// [column][row] text 0, from $7D9E to $8193. A record starting with $FF opens a new screen and carries one more
// byte (attribute) before the text. The records follow each other, so a text keeps its length: a translation
// may be shorter (it is centred with spaces) but not longer. The game font has upper case letters, digits and
// a few signs only; "$0000" and "000000" are filled in by the game and must stay where they are.
//
// Text files (UTF-8, private: they contain the original texts, never in the repository):
//   7D9E "BLOOD MONEY" = "BLOOD MONEY"
// address, original, translation. Lines starting with '#' are comments.
#include <cstdint>
#include <string>
#include <vector>

namespace amiga { class Machine; }

namespace bc {

struct TextEntry {
    uint32_t addr = 0;          // first character (for texts in the code: the address of the first piece)
    std::string original;       // as in the game
    bool numeric = false;       // only digits (a score field): not a text
    bool rightAlign = false;    // shorter translations: spaces on the left (the text continues after it)
    std::vector<int> keep;      // positions the game writes into (a player number): unchanged
    // texts written by the code (move.l #'MUSI',(a0)+ ...): the pieces of the immediate data
    std::vector<std::pair<uint32_t, uint32_t>> pieces;
    uint32_t mirror = 0;        // where the same text is in the table at the start
};

// the texts as they are in memory once the game is loaded ($400)
std::vector<TextEntry> scanTexts(const amiga::Machine& m);
std::vector<TextEntry> scanTexts(const uint8_t* chip);
bool exportTexts(const std::vector<TextEntry>& texts, const std::string& path, std::string& err);

struct TextReport {
    int changed = 0;
    std::vector<std::string> warnings;
};
// reads a text file and writes the translations into memory; texts that break a rule are left as they are
// and reported
TextReport applyTexts(amiga::Machine& m, const std::string& path);
// the same checks on one text: empty string = valid
std::string checkTranslation(const TextEntry& e, const std::string& translation);

} // namespace bc
