// SPDX-License-Identifier: MIT
// Simulated glw keyboard (Jet75-II layout) for unit tests. Implements the commands the core uses,
// validates checksums and can inject unsolicited reports before replies.
#pragma once

#include <deque>
#include <map>
#include <vector>

#include "mchose/glw/packet.hpp"
#include "mchose/transport.hpp"

class FakeKeyboard : public mchose::Transport {
public:
    std::map<std::uint8_t, std::vector<std::uint8_t>> mem;   // keyed by read command
    std::vector<mchose::Report> sent;                       // every request received
    std::deque<mchose::Report> inject;                      // delivered before the next reply
    int flash_writes = 0;
    int profile_switches = 0;
    int drop_requests = 0;                                  // swallow the next N requests

    FakeKeyboard()
    {
        using namespace mchose::glw;
        mem[cmd::GetBase] = std::vector<std::uint8_t>(56, 0xA2);
        auto& b = mem[cmd::GetBase];
        b[0] = 0; b[1] = 2; b[2] = 0; b[3] = 1; b[4] = 2; b[5] = 3; b[6] = 0xAA; b[7] = 0xBB;
        mem[cmd::GetFuncConfig] = std::vector<std::uint8_t>(256, 0);
        for (int p = 0; p < 4; ++p) {
            auto* f = &mem[cmd::GetFuncConfig][64 * p];
            f[0] = 0x50; f[1] = static_cast<std::uint8_t>(4 * p); f[2] = 0xAA; f[3] = 0xBB; f[4] = 0x02;
            f[8] = 6; f[9] = 80; f[10] = 4;
        }
        mem[cmd::GetKeymap] = std::vector<std::uint8_t>(8192, 0xF0);
        mem[cmd::GetFactoryKeymap] = std::vector<std::uint8_t>(2048, 0xF0);
        for (int k = 0; k < 80; ++k)
            for (int base : {0}) {
                auto& km = mem[cmd::GetKeymap];
                km[base + 3 * k] = 0x10; km[base + 3 * k + 1] = 0; km[base + 3 * k + 2] = static_cast<std::uint8_t>(4 + k % 40);
            }
        mem[cmd::GetColors] = std::vector<std::uint8_t>(2048, 0);
        mem[cmd::GetTrigger] = std::vector<std::uint8_t>(4096, 0);
        for (int i = 0; i < 4 * 128; ++i) {
            std::uint8_t e[8] = {0xA0, 0x01, 0x95, 0x00, 0x1D, 0x00, 0x1D, 0x00};
            std::copy(e, e + 8, mem[cmd::GetTrigger].begin() + 8 * i);
        }
        mem[cmd::GetDks] = std::vector<std::uint8_t>(3072, 0);
        mem[cmd::GetMt] = std::vector<std::uint8_t>(1024, 0);
        mem[cmd::GetTgl] = std::vector<std::uint8_t>(512, 0);
        mem[cmd::GetMacros] = std::vector<std::uint8_t>(8192, 0);
        mem[0xDD] = std::vector<std::uint8_t>(0x180, 0);
    }

    void write(const mchose::Report& r) override
    {
        using namespace mchose::glw;
        sent.push_back(r);
        if (drop_requests > 0) {
            --drop_requests;
            return;
        }
        mchose::Report rep{};
        rep[1] = r[1];
        rep[4] = r[4];
        rep[5] = r[5];
        rep[6] = r[6];
        const std::size_t size = r[4];
        const std::size_t off = r[5] | (r[6] << 8);
        if (r[0] != kRequestFlag)
            return;   // ignored by the firmware
        if (checksum(r) != r[3]) {
            rep[0] = kReplyError;
            const char* e = "cserr";
            std::copy(e, e + 5, rep.begin() + 8);
            queue_reply(rep);
            return;
        }
        rep[0] = kReplyOk;
        const std::uint8_t c = r[1];
        auto read_region = [&](std::uint8_t region) {
            auto& m = mem[region];
            for (std::size_t i = 0; i < size && off + i < m.size(); ++i)
                rep[8 + i] = m[off + i];
        };
        auto write_region = [&](std::uint8_t region) {
            auto& m = mem[region];
            for (std::size_t i = 0; i < size && off + i < m.size(); ++i)
                m[off + i] = r[8 + i];
        };
        switch (c) {
        case cmd::Info: {
            const char* build = "Aug 28 2025,10:45:06";
            rep[4] = 22;
            rep[8] = 0x16; rep[9] = 0x01;
            std::copy(build, build + 20, rep.begin() + 10);
            break;
        }
        case cmd::GetBase: case cmd::GetFuncConfig: case cmd::GetKeymap: case cmd::GetFactoryKeymap:
        case cmd::GetColors: case cmd::GetMacros: case cmd::GetTrigger: case cmd::GetDks: case cmd::GetMt:
        case cmd::GetTgl:
            read_region(c);
            break;
        case cmd::SetBase: {
            const int before = active();
            write_region(cmd::GetBase);
            ++flash_writes;
            if (active() != before) {
                ++profile_switches;
                mchose::Report a1{};
                a1[0] = 0xA1;
                a1[2] = static_cast<std::uint8_t>(active());
                queue_reply(rep);
                queue_reply(a1);
                return;
            }
            break;
        }
        case cmd::SetFuncConfig: write_region(cmd::GetFuncConfig); ++flash_writes; break;
        case cmd::SetKeymap: write_region(cmd::GetKeymap); ++flash_writes; break;
        case cmd::SetColors: write_region(cmd::GetColors); ++flash_writes; break;
        case cmd::SetMacros:
            if (off + size > 0x2000) { rep[0] = kReplyError; break; }
            write_region(cmd::GetMacros); ++flash_writes; break;
        case cmd::SetTrigger: write_region(cmd::GetTrigger); ++flash_writes; break;
        case cmd::SetDks: write_region(cmd::GetDks); ++flash_writes; break;
        case cmd::SetMt: write_region(cmd::GetMt); ++flash_writes; break;
        case cmd::SetTgl: write_region(cmd::GetTgl); ++flash_writes; break;
        case cmd::LiveColorsMain: write_region(0xDD); break;
        default: {
            rep[0] = kReplyError;
            const char* e = "unknw";
            std::copy(e, e + 5, rep.begin() + 8);
            break;
        }
        }
        queue_reply(rep);
    }

    std::optional<mchose::Report> read(std::chrono::milliseconds) override
    {
        if (out_.empty())
            return std::nullopt;
        auto r = out_.front();
        out_.pop_front();
        return r;
    }

    int active() const
    {
        const auto& b = mem.at(mchose::glw::cmd::GetBase);
        return b[0] < 4 ? b[2 + b[0]] : 0;
    }

private:
    void queue_reply(const mchose::Report& rep)
    {
        while (!inject.empty()) {
            out_.push_back(inject.front());
            inject.pop_front();
        }
        out_.push_back(rep);
    }
    std::deque<mchose::Report> out_;
};
