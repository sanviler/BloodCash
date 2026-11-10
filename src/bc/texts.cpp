#include "bc/texts.h"
#include "amiga/machine.h"
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace bc {

namespace {
constexpr uint32_t kFirst = 0x7D9E, kEnd = 0x8193;     // the high score table starts at $8193

bool allowed(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '.' || c == ',' || c == '$' || c == '@';
}

std::string hex(uint32_t a) { char b[16]; std::snprintf(b, sizeof b, "%04X", unsigned(a)); return b; }

// "..." with \" and \\ ; returns false if malformed
bool readQuoted(const std::string& s, size_t& p, std::string& out) {
    while (p < s.size() && s[p] == ' ') ++p;
    if (p >= s.size() || s[p] != '"') return false;
    ++p;
    out.clear();
    while (p < s.size() && s[p] != '"') {
        if (s[p] == '\\' && p + 1 < s.size()) ++p;
        out += s[p++];
    }
    if (p >= s.size()) return false;
    ++p;
    return true;
}
} // namespace

// texts that the code writes into the title screen (F3 and F4 switches)
struct CodeText { std::vector<std::pair<uint32_t, uint32_t>> pieces; uint32_t mirror; };
const CodeText kCodeTexts[] = {
    {{{0xD1C, 4}, {0xD22, 4}}, 0x7E1C},                 // MUSIC ON
    {{{0xD68, 4}, {0xD6E, 4}}, 0x7E1C},                 // SOUND FX
    {{{0xDA4, 4}, {0xDAA, 4}, {0xDB1, 1}}, 0x7E32},     // PATIENT
    {{{0xDB8, 4}, {0xDBE, 4}, {0xDC5, 1}}, 0x7E32},     // IMPATIENT
};

std::vector<TextEntry> scanTexts(const uint8_t* chip) {
    std::vector<TextEntry> out;
    uint32_t p = kFirst;
    while (p + 2 < kEnd) {
        uint32_t t = p + 2;
        if (chip[p] == 0xFF) ++t;                              // screen start: attribute byte
        uint32_t e = t;
        while (e < kEnd && chip[e]) ++e;
        if (e >= kEnd) break;
        TextEntry te;
        te.addr = t;
        te.original.assign(reinterpret_cast<const char*>(chip + t), e - t);
        bool digits = !te.original.empty(), printable = !te.original.empty();
        for (char c : te.original) {
            if (!std::isdigit(static_cast<unsigned char>(c))) digits = false;
            if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x5A) printable = false;
        }
        te.numeric = digits;
        // the switches of the title screen: only the words before them are in the table entry
        for (const auto& ct : kCodeTexts)
            if (ct.mirror > t && ct.mirror < e) { te.original.resize(ct.mirror - t); te.rightAlign = true; }
        // "PLAYER 1 TO SELECT", "PLAYER 1 HAS A HIGH": the game writes the player number
        if (t == 0x7E80 || t == 0x807F) te.keep = {7};
        if (printable) out.push_back(te);
        p = e + 1;
    }
    for (const auto& ct : kCodeTexts) {
        TextEntry te;
        te.addr = ct.pieces[0].first;
        te.pieces = ct.pieces;
        te.mirror = ct.mirror;
        for (const auto& [a, n] : ct.pieces) te.original.append(reinterpret_cast<const char*>(chip + a), n);
        out.push_back(te);
    }
    return out;
}

std::vector<TextEntry> scanTexts(const amiga::Machine& m) {
    std::vector<uint8_t> mem(kEnd);
    for (uint32_t a = 0x400; a < kEnd; ++a) mem[a] = m.peek8(a);
    return scanTexts(mem.data());
}

bool exportTexts(const std::vector<TextEntry>& texts, const std::string& path, std::string& err) {
    std::ofstream o(path, std::ios::binary);
    if (!o) { err = "cannot write " + path; return false; }
    o << "# BloodCash texts (Blood Money, Psygnosis 1989). Private file: it contains the original texts.\n"
         "# address \"original\" = \"translation\". Change only the translation.\n"
         "# Rules: no longer than the original (shorter texts are centred); letters A-Z, digits, space and . , $ @\n"
         "# (no accents: the game font has none); \"$0000\" and \"000000\" stay in the same place, and so does the\n"
         "# player number in \"PLAYER 1 TO SELECT\" and \"PLAYER 1 HAS A HIGH\". Entries D1C, D68, DA4, DB8 are the\n"
         "# words of the F3 / F4 switches on the title screen, written by the code: same length or shorter.\n";
    for (const auto& t : texts) {
        if (t.numeric) continue;
        o << hex(t.addr) << " \"" << t.original << "\" = \"" << t.original << "\"\n";
    }
    return bool(o);
}

std::string checkTranslation(const TextEntry& e, const std::string& tr) {
    if (tr.size() > e.original.size())
        return "too long (" + std::to_string(tr.size()) + " characters, at most " + std::to_string(e.original.size()) + ")";
    for (char c : tr)
        if (!allowed(c)) return std::string("character not in the game font: '") + c + "'";
    for (int k : e.keep)
        if (size_t(k) >= tr.size() || tr[size_t(k)] != e.original[size_t(k)])
            return "character " + std::to_string(k + 1) + " is written by the game: keep '" + e.original.substr(size_t(k), 1) + "' there";
    for (const char* field : {"$0000", "000000"}) {
        const size_t at = e.original.find(field);
        if (at != std::string::npos && tr.find(field) == std::string::npos)
            return std::string("the field ") + field + " is missing";
        if (at != std::string::npos && tr.find(field) != at)
            return std::string("the field ") + field + " moved (it must stay at character " + std::to_string(at + 1) + ")";
    }
    return {};
}

TextReport applyTexts(amiga::Machine& m, const std::string& path) {
    TextReport r;
    std::ifstream in(path, std::ios::binary);
    if (!in) { r.warnings.push_back("cannot read " + path); return r; }
    const auto texts = scanTexts(m);
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        ++n;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const std::string where = "line " + std::to_string(n) + ": ";
        uint32_t addr = 0;
        size_t p = 0;
        try { addr = uint32_t(std::stoul(line, &p, 16)); } catch (...) { r.warnings.push_back(where + "no address"); continue; }
        std::string orig, tr;
        if (!readQuoted(line, p, orig)) { r.warnings.push_back(where + "original text not in quotes"); continue; }
        while (p < line.size() && line[p] == ' ') ++p;
        if (p >= line.size() || line[p] != '=') { r.warnings.push_back(where + "'=' missing"); continue; }
        ++p;
        if (!readQuoted(line, p, tr)) { r.warnings.push_back(where + "translation not in quotes"); continue; }
        const TextEntry* e = nullptr;
        for (const auto& t : texts) if (t.addr == addr) e = &t;
        if (!e || e->numeric) { r.warnings.push_back(where + "unknown address " + hex(addr)); continue; }
        if (e->original != orig) { r.warnings.push_back(where + "the original text does not match this version of the game"); continue; }
        if (tr == orig) continue;
        for (auto& c : tr) c = char(std::toupper(static_cast<unsigned char>(c)));
        if (const std::string why = checkTranslation(*e, tr); !why.empty()) { r.warnings.push_back(where + why); continue; }
        // shorter: centred with spaces, unless it holds a field that must keep its place
        std::string out = tr;
        const bool fixedPlace = !e->keep.empty() || !e->pieces.empty() ||
            e->original.find("$0000") != std::string::npos || e->original.find("000000") != std::string::npos;
        if (out.size() < e->original.size()) {
            const size_t pad = e->original.size() - out.size();
            out = e->rightAlign ? std::string(pad, ' ') + out
                : fixedPlace ? out + std::string(pad, ' ')
                : std::string(pad / 2, ' ') + out + std::string(pad - pad / 2, ' ');
        }
        if (e->pieces.empty()) {
            for (size_t i = 0; i < out.size(); ++i) m.poke8(addr + uint32_t(i), uint8_t(out[i]));
        } else {
            size_t k = 0;
            for (const auto& [a, len] : e->pieces)
                for (uint32_t i = 0; i < len; ++i) m.poke8(a + i, uint8_t(out[k++]));
            // the first switch value is also in the table at the start
            if (m.peek8(e->mirror) == uint8_t(e->original[0]) && m.peek8(e->mirror + 1) == uint8_t(e->original[1]))
                for (size_t i = 0; i < out.size(); ++i) m.poke8(e->mirror + uint32_t(i), uint8_t(out[i]));
        }
        ++r.changed;
    }
    return r;
}

} // namespace bc
