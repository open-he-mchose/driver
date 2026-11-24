// SPDX-License-Identifier: MIT
#include "mchose/model.hpp"

#include <vector>

namespace mchose {

namespace {

// Jet75-II: key set and matrix indices verified; widths and the gaps between key groups inferred.
constexpr KeyCap kJet75Layout[] = {
    {50, "Esc", 0.00f, 0.00f, 1.00f},
    {65, "F1", 1.25f, 0.00f, 1.00f},
    {75, "F2", 2.25f, 0.00f, 1.00f},
    {55, "F3", 3.25f, 0.00f, 1.00f},
    {25, "F4", 4.25f, 0.00f, 1.00f},
    {15, "F5", 5.50f, 0.00f, 1.00f},
    {5, "F6", 6.50f, 0.00f, 1.00f},
    {35, "F7", 7.50f, 0.00f, 1.00f},
    {45, "F8", 8.50f, 0.00f, 1.00f},
    {69, "F9", 9.75f, 0.00f, 1.00f},
    {79, "F10", 10.75f, 0.00f, 1.00f},
    {59, "F11", 11.75f, 0.00f, 1.00f},
    {49, "F12", 12.75f, 0.00f, 1.00f},
    {9, "Del", 15.00f, 0.00f, 1.00f},
    {40, "`", 0.00f, 1.25f, 1.00f},
    {30, "1", 1.00f, 1.25f, 1.00f},
    {51, "2", 2.00f, 1.25f, 1.00f},
    {41, "3", 3.00f, 1.25f, 1.00f},
    {31, "4", 4.00f, 1.25f, 1.00f},
    {52, "5", 5.00f, 1.25f, 1.00f},
    {72, "6", 6.00f, 1.25f, 1.00f},
    {32, "7", 7.00f, 1.25f, 1.00f},
    {53, "8", 8.00f, 1.25f, 1.00f},
    {73, "9", 9.00f, 1.25f, 1.00f},
    {33, "0", 10.00f, 1.25f, 1.00f},
    {3, "-", 11.00f, 1.25f, 1.00f},
    {54, "=", 12.00f, 1.25f, 1.00f},
    {44, "Bksp", 13.00f, 1.25f, 2.00f},
    {19, "Home", 15.00f, 1.25f, 1.00f},
    {70, "Tab", 0.00f, 2.25f, 1.50f},
    {0, "Q", 1.50f, 2.25f, 1.00f},
    {71, "W", 2.50f, 2.25f, 1.00f},
    {1, "E", 3.50f, 2.25f, 1.00f},
    {21, "R", 4.50f, 2.25f, 1.00f},
    {42, "T", 5.50f, 2.25f, 1.00f},
    {2, "Y", 6.50f, 2.25f, 1.00f},
    {63, "U", 7.50f, 2.25f, 1.00f},
    {43, "I", 8.50f, 2.25f, 1.00f},
    {13, "O", 9.50f, 2.25f, 1.00f},
    {23, "P", 10.50f, 2.25f, 1.00f},
    {74, "[", 11.50f, 2.25f, 1.00f},
    {64, "]", 12.50f, 2.25f, 1.00f},
    {34, "\\", 13.50f, 2.25f, 1.50f},
    {14, "PgUp", 15.00f, 2.25f, 1.00f},
    {60, "Caps", 0.00f, 3.25f, 1.75f},
    {10, "A", 1.75f, 3.25f, 1.00f},
    {61, "S", 2.75f, 3.25f, 1.00f},
    {11, "D", 3.75f, 3.25f, 1.00f},
    {62, "F", 4.75f, 3.25f, 1.00f},
    {12, "G", 5.75f, 3.25f, 1.00f},
    {22, "H", 6.75f, 3.25f, 1.00f},
    {47, "J", 7.75f, 3.25f, 1.00f},
    {27, "K", 8.75f, 3.25f, 1.00f},
    {48, "L", 9.75f, 3.25f, 1.00f},
    {68, ";", 10.75f, 3.25f, 1.00f},
    {78, "'", 11.75f, 3.25f, 1.00f},
    {4, "Enter", 12.75f, 3.25f, 2.25f},
    {24, "PgDn", 15.00f, 3.25f, 1.00f},
    {66, "Shift", 0.00f, 4.25f, 2.25f},
    {20, "Z", 2.25f, 4.25f, 1.00f},
    {16, "X", 3.25f, 4.25f, 1.00f},
    {6, "C", 4.25f, 4.25f, 1.00f},
    {36, "V", 5.25f, 4.25f, 1.00f},
    {26, "B", 6.25f, 4.25f, 1.00f},
    {67, "N", 7.25f, 4.25f, 1.00f},
    {57, "M", 8.25f, 4.25f, 1.00f},
    {17, ",", 9.25f, 4.25f, 1.00f},
    {7, ".", 10.25f, 4.25f, 1.00f},
    {58, "/", 11.25f, 4.25f, 1.00f},
    {28, "Shift", 12.25f, 4.25f, 1.75f},
    {18, "↑", 14.00f, 4.25f, 1.00f},
    {46, "Ctrl", 0.00f, 5.25f, 1.25f},
    {76, "Win", 1.25f, 5.25f, 1.25f},
    {56, "Alt", 2.50f, 5.25f, 1.25f},
    {77, "Space", 3.75f, 5.25f, 6.25f},
    {37, "Fn", 10.00f, 5.25f, 1.00f},
    {38, "Ctrl", 11.00f, 5.25f, 1.00f},
    {8, "←", 13.00f, 5.25f, 1.00f},
    {29, "↓", 14.00f, 5.25f, 1.00f},
    {39, "→", 15.00f, 5.25f, 1.00f},
};

std::vector<Model> make_models()
{
    std::vector<Model> m;
    Model jet75;
    jet75.name = "Jet75-II";
    jet75.vid = kMchoseVid;
    jet75.pid = 0x211A;
    jet75.keys = 80;
    jet75.verified = true;
    jet75.layout = kJet75Layout;
    m.push_back(jet75);
    // More glw models will be generated from docs/data/devices.json once their layouts are documented.
    return m;
}

const std::vector<Model>& models()
{
    static const std::vector<Model> m = make_models();
    return m;
}

}  // namespace

std::span<const Model> known_models() { return models(); }

std::optional<Model> find_model(std::uint16_t vid, std::uint16_t pid)
{
    for (const auto& m : models())
        if (m.vid == vid && m.pid == pid)
            return m;
    return std::nullopt;
}

}  // namespace mchose
