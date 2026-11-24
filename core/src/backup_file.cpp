// SPDX-License-Identifier: MIT
#include "mchose/glw/backup_file.hpp"

#include <stdexcept>
#include <string>

#include "json.hpp"

namespace mchose::glw {

namespace {

json::Value image_to_json(const KeyboardImage& img, const std::string& created)
{
    json::Array profiles;
    for (const auto& p : img.profiles) {
        json::Array keymaps;
        for (const auto& km : p.keymaps)
            keymaps.push_back(to_hex(km));
        profiles.push_back(json::Object{
            {"funcconfig", to_hex(p.funcconfig)}, {"triggers", to_hex(p.triggers)}, {"keymaps", keymaps},
            {"colors", to_hex(p.colors)}, {"dks", to_hex(p.dks)}, {"mt", to_hex(p.mt)}, {"tgl", to_hex(p.tgl)}});
    }
    return json::Object{{"format", "mchose-backup"}, {"version", 1}, {"model", img.model},
                        {"firmware", img.firmware}, {"created", created},
                        {"base", to_hex(img.base)}, {"macros", to_hex(img.macros)}, {"profiles", profiles}};
}

KeyboardImage image_from_json(const json::Value& v)
{
    if (v.at("format").str() != "mchose-backup" || v.at("version").num() != 1)
        throw std::runtime_error("not an mchose backup (version 1)");
    KeyboardImage img;
    img.model = v.at("model").str();
    img.firmware = v.at("firmware").str();
    auto base = from_hex(v.at("base").str());
    if (base.size() != img.base.size())
        throw std::runtime_error("backup: base info has the wrong size");
    std::copy(base.begin(), base.end(), img.base.begin());
    img.macros = from_hex(v.at("macros").str());
    for (const auto& pv : v.at("profiles").arr()) {
        ProfileImage p;
        auto fc = from_hex(pv.at("funcconfig").str());
        if (fc.size() != 64)
            throw std::runtime_error("backup: FuncConfig has the wrong size");
        std::copy(fc.begin(), fc.end(), p.funcconfig.begin());
        p.triggers = from_hex(pv.at("triggers").str());
        const auto& kms = pv.at("keymaps").arr();
        for (std::size_t l = 0; l < kms.size() && l < 4; ++l)
            p.keymaps[l] = from_hex(kms[l].str());
        p.colors = from_hex(pv.at("colors").str());
        p.dks = from_hex(pv.at("dks").str());
        p.mt = from_hex(pv.at("mt").str());
        p.tgl = from_hex(pv.at("tgl").str());
        img.profiles.push_back(std::move(p));
    }
    return img;
}

}  // namespace

std::string backup_to_json(const KeyboardImage& image, const std::string& created)
{
    return json::dump(image_to_json(image, created), 1) + "\n";
}

KeyboardImage backup_from_json(const std::string& text)
{
    return image_from_json(json::parse(text));
}

}  // namespace mchose::glw
