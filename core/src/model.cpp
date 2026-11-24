// SPDX-License-Identifier: MIT
#include "mchose/model.hpp"

#include <vector>

namespace mchose {

namespace {

std::vector<Model> make_models()
{
    std::vector<Model> m;
    Model jet75;
    jet75.name = "Jet75-II";
    jet75.vid = kMchoseVid;
    jet75.pid = 0x211A;
    jet75.keys = 80;
    jet75.verified = true;
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
