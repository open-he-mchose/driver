// SPDX-License-Identifier: MIT
#include "mchose/script/script_player.hpp"

#include <array>
#include <vector>

namespace mchose::script {

ScriptPlayer::ScriptPlayer(glw::Keyboard& kb) : kb_(kb)
{
    const Model& m = kb_.model();
    const auto factory = kb_.factory_keymap(0);
    usage_index_ = usage_index_map(factory, m.keys);
    std::vector<KeyInfo> keys;
    float width = 0, height = 0;
    if (!m.layout.empty()) {
        for (const auto& cap : m.layout) {
            keys.push_back({cap.index, glw::to_string(factory[static_cast<std::size_t>(cap.index)]), cap.x, cap.y, cap.w});
            width = std::max(width, cap.x + cap.w);
            height = std::max(height, cap.y + 1);
        }
    } else {   // unknown layout: a plain grid
        for (int i = 0; i < m.keys; ++i)
            keys.push_back({i, glw::to_string(factory[static_cast<std::size_t>(i)]), static_cast<float>(i % 16),
                            static_cast<float>(i / 16), 1});
        width = 16;
        height = static_cast<float>((m.keys + 15) / 16);
    }
    script_ = std::make_unique<LightingScript>(std::move(keys), m.key_slots, width, height);
}

void ScriptPlayer::load(const std::string& source, const std::string& name)
{
    script_->load(source, name);
    started_ = false;
}

void ScriptPlayer::step()
{
    const auto now = std::chrono::steady_clock::now();
    if (!started_) {
        start_ = last_ = now;
        started_ = true;
    }
    std::vector<std::uint8_t> pressed, released, held;
    input_.poll(pressed, released, held);
    FrameInput in;
    in.t = std::chrono::duration<double>(now - start_).count();
    in.dt = std::chrono::duration<double>(now - last_).count();
    last_ = now;
    auto map = [&](const std::vector<std::uint8_t>& from, std::vector<int>& to) {
        for (auto u : from)
            if (auto it = usage_index_.find(u); it != usage_index_.end())
                to.push_back(it->second);
    };
    map(pressed, in.pressed);
    map(released, in.released);
    map(held, in.held);
    const auto& colors = script_->frame(in);
    const int n = kb_.model().keys;
    kb_.live_colors(std::span<const std::array<std::uint8_t, 3>>(colors.data(), static_cast<std::size_t>(n)));
}

}  // namespace mchose::script
