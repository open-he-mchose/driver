// SPDX-License-Identifier: MIT
#include "mchose/script/lighting_script.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace mchose::script {

// Accessors for the Lua C callbacks.
struct ScriptAccess {
    static std::vector<Rgb>& colors(LightingScript& s) { return s.colors_; }
    static const std::function<void(const std::string&)>& print(const LightingScript& s) { return s.print_; }
};

struct LightingScript::State {
    lua_State* L = nullptr;
    LightingScript* owner = nullptr;
    std::size_t used = 0;
    std::size_t limit = 0;
    long count = 0;
    long instruction_limit = 0;
    int ctx_ref = LUA_NOREF;
};

namespace {

constexpr int kHookStep = 10000;

const char* kPrelude = R"lua(
function hsv(h, s, v)
  s = s or 1; v = v or 1
  h = (h % 360) / 60
  local i = math.floor(h)
  local f = h - i
  local p, q, t = v * (1 - s), v * (1 - s * f), v * (1 - s * (1 - f))
  local r, g, b
  if i == 0 then r, g, b = v, t, p
  elseif i == 1 then r, g, b = q, v, p
  elseif i == 2 then r, g, b = p, v, t
  elseif i == 3 then r, g, b = p, q, v
  elseif i == 4 then r, g, b = t, p, v
  else r, g, b = v, p, q end
  return r * 255, g * 255, b * 255
end
function mix(a, b, t) return a + (b - a) * t end
function clamp(x, lo, hi) if x < lo then return lo elseif x > hi then return hi end return x end
)lua";

LightingScript::State* state_of(lua_State* L)
{
    return static_cast<LightingScript::State*>(lua_touserdata(L, lua_upvalueindex(1)));
}

void* limited_alloc(void* ud, void* ptr, std::size_t osize, std::size_t nsize)
{
    auto* s = static_cast<LightingScript::State*>(ud);
    const std::size_t old = ptr ? osize : 0;
    if (nsize == 0) {
        std::free(ptr);
        s->used -= std::min(s->used, old);
        return nullptr;
    }
    if (s->limit && s->used - std::min(s->used, old) + nsize > s->limit)
        return nullptr;   // Lua raises a memory error
    void* p = std::realloc(ptr, nsize);
    if (p)
        s->used = s->used - std::min(s->used, old) + nsize;
    return p;
}

void count_hook(lua_State* L, lua_Debug*)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "mchose_state");
    auto* s = static_cast<LightingScript::State*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    s->count += kHookStep;
    if (s->instruction_limit && s->count > s->instruction_limit)
        luaL_error(L, "instruction limit exceeded (endless loop?)");
}

int traceback(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    luaL_traceback(L, L, msg ? msg : "error", 1);
    return 1;
}

std::uint8_t channel(lua_Number v)
{
    if (!(v > 0)) return 0;   // also NaN
    if (v >= 255) return 255;
    return static_cast<std::uint8_t>(std::lround(v));
}

int l_print(lua_State* L)
{
    auto* s = state_of(L);
    std::string line;
    const int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        std::size_t len = 0;
        const char* str = luaL_tolstring(L, i, &len);
        if (i > 1)
            line += '\t';
        line.append(str, len);
        lua_pop(L, 1);
    }
    if (s->owner && ScriptAccess::print(*s->owner))
        ScriptAccess::print(*s->owner)(line);
    return 0;
}

}  // namespace

namespace {

int l_set(lua_State* L)
{
    auto* s = state_of(L);
    auto& colors = ScriptAccess::colors(*s->owner);
    const lua_Integer i = luaL_checkinteger(L, 1);
    const Rgb c{channel(luaL_checknumber(L, 2)), channel(luaL_checknumber(L, 3)), channel(luaL_checknumber(L, 4))};
    if (i >= 0 && i < static_cast<lua_Integer>(colors.size()))
        colors[static_cast<std::size_t>(i)] = c;
    return 0;
}

int l_fill(lua_State* L)
{
    auto* s = state_of(L);
    const Rgb c{channel(luaL_checknumber(L, 1)), channel(luaL_checknumber(L, 2)), channel(luaL_checknumber(L, 3))};
    for (auto& x : ScriptAccess::colors(*s->owner))
        x = c;
    return 0;
}

int l_get(lua_State* L)
{
    auto* s = state_of(L);
    const auto& colors = ScriptAccess::colors(*s->owner);
    const lua_Integer i = luaL_checkinteger(L, 1);
    if (i < 0 || i >= static_cast<lua_Integer>(colors.size()))
        return luaL_error(L, "key index %d out of range", static_cast<int>(i));
    for (int k = 0; k < 3; ++k)
        lua_pushinteger(L, colors[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)]);
    return 3;
}

void push_index_list(lua_State* L, const std::vector<int>& v)
{
    lua_createtable(L, static_cast<int>(v.size()), 0);
    for (std::size_t i = 0; i < v.size(); ++i) {
        lua_pushinteger(L, v[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
}

}  // namespace

LightingScript::LightingScript(std::vector<KeyInfo> keys, int slots, float width, float height)
    : keys_(std::move(keys)), width_(width), height_(height), colors_(static_cast<std::size_t>(std::max(slots, 1)), Rgb{0, 0, 0})
{
}

LightingScript::~LightingScript()
{
    if (state_ && state_->L)
        lua_close(state_->L);
}

void LightingScript::reset_state()
{
    if (state_ && state_->L)
        lua_close(state_->L);
    state_ = std::make_unique<State>();
    state_->owner = this;
    state_->limit = memory_limit_;
    state_->instruction_limit = instruction_limit_;
    lua_State* L = lua_newstate(limited_alloc, state_.get());
    if (!L)
        throw ScriptError("cannot create the Lua state");
    state_->L = L;
    lua_pushlightuserdata(L, state_.get());
    lua_setfield(L, LUA_REGISTRYINDEX, "mchose_state");

    // Safe standard libraries only.
    static const luaL_Reg libs[] = {{LUA_GNAME, luaopen_base},       {LUA_COLIBNAME, luaopen_coroutine},
                                    {LUA_TABLIBNAME, luaopen_table}, {LUA_STRLIBNAME, luaopen_string},
                                    {LUA_MATHLIBNAME, luaopen_math}, {LUA_UTF8LIBNAME, luaopen_utf8}};
    for (const auto& lib : libs) {
        luaL_requiref(L, lib.name, lib.func, 1);
        lua_pop(L, 1);
    }
    for (const char* name : {"dofile", "loadfile", "load", "collectgarbage"}) {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }
    lua_pushlightuserdata(L, state_.get());
    lua_pushcclosure(L, l_print, 1);
    lua_setglobal(L, "print");
    lua_sethook(L, count_hook, LUA_MASKCOUNT, kHookStep);
}

void LightingScript::call(const char* function, bool optional)
{
    lua_State* L = state_->L;
    lua_pushcfunction(L, traceback);
    const int base = lua_gettop(L);
    lua_getglobal(L, function);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        if (optional)
            return;
        throw ScriptError(std::string("the script defines no function ") + function + "(ctx)");
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, state_->ctx_ref);
    state_->count = 0;
    if (lua_pcall(L, 1, 0, base) != LUA_OK) {
        std::string err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "error";
        lua_settop(L, base - 1);
        loaded_ = false;
        throw ScriptError(err);
    }
    lua_settop(L, base - 1);
}

namespace {
double snap(const Param& p, double v)
{
    const double lo = std::min(p.min, p.max), hi = std::max(p.min, p.max);
    v = std::clamp(v, lo, hi);
    if (p.step > 0)
        v = std::clamp(lo + std::round((v - lo) / p.step) * p.step, lo, hi);
    return v;
}
}  // namespace

void LightingScript::load(const std::string& source, const std::string& name)
{
    loaded_ = false;
    std::fill(colors_.begin(), colors_.end(), Rgb{0, 0, 0});
    params_.clear();
    reset_state();
    lua_State* L = state_->L;

    if (luaL_loadbufferx(L, kPrelude, std::strlen(kPrelude), "=prelude", "t") != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK)
        throw ScriptError(std::string("prelude: ") + lua_tostring(L, -1));

    // ctx table with the static parts.
    lua_newtable(L);
    lua_createtable(L, static_cast<int>(keys_.size()), 0);
    for (std::size_t i = 0; i < keys_.size(); ++i) {
        lua_createtable(L, 0, 5);
        lua_pushinteger(L, keys_[i].index);
        lua_setfield(L, -2, "index");
        lua_pushstring(L, keys_[i].name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushnumber(L, keys_[i].x);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, keys_[i].y);
        lua_setfield(L, -2, "y");
        lua_pushnumber(L, keys_[i].w);
        lua_setfield(L, -2, "w");
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_setfield(L, -2, "keys");
    lua_pushnumber(L, width_);
    lua_setfield(L, -2, "width");
    lua_pushnumber(L, height_);
    lua_setfield(L, -2, "height");
    lua_newtable(L);
    lua_setfield(L, -2, "param");
    static const luaL_Reg ctx_functions[] = {{"set", l_set}, {"fill", l_fill}, {"get", l_get}};
    for (const auto& f : ctx_functions) {
        lua_pushlightuserdata(L, state_.get());
        lua_pushcclosure(L, f.func, 1);
        lua_setfield(L, -2, f.name);
    }
    lua_pushnumber(L, 0);
    lua_setfield(L, -2, "t");
    lua_pushnumber(L, 0);
    lua_setfield(L, -2, "dt");
    for (const char* list : {"pressed", "released"}) {
        lua_newtable(L);
        lua_setfield(L, -2, list);
    }
    lua_newtable(L);
    lua_setfield(L, -2, "held");
    state_->ctx_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    // Main chunk (text only: no precompiled bytecode).
    lua_pushcfunction(L, traceback);
    const int base = lua_gettop(L);
    state_->count = 0;
    if (luaL_loadbufferx(L, source.data(), source.size(), ("=" + name).c_str(), "t") != LUA_OK ||
        lua_pcall(L, 0, 0, base) != LUA_OK) {
        std::string err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "error";
        lua_settop(L, 0);
        throw ScriptError(err);
    }
    lua_settop(L, 0);

    // Parameters declared by the script.
    lua_getglobal(L, "params");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            if (lua_type(L, -2) == LUA_TSTRING && lua_istable(L, -1)) {
                Param p;
                p.name = lua_tostring(L, -2);
                lua_getfield(L, -1, "min");
                p.min = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0;
                lua_getfield(L, -2, "max");
                p.max = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1;
                lua_getfield(L, -3, "default");
                p.def = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : p.min;
                lua_getfield(L, -4, "label");
                p.label = lua_isstring(L, -1) ? lua_tostring(L, -1) : p.name;
                lua_getfield(L, -5, "step");
                p.step = lua_isnumber(L, -1) ? std::max(0.0, static_cast<double>(lua_tonumber(L, -1))) : 0;
                lua_getfield(L, -6, "order");
                if (lua_isnumber(L, -1))
                    p.order = lua_tonumber(L, -1);
                lua_pop(L, 6);
                p.value = snap(p, p.def);
                params_.push_back(p);
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    std::sort(params_.begin(), params_.end(), [](const Param& a, const Param& b) { return a.order != b.order ? a.order < b.order : a.name < b.name; });
    lua_rawgeti(L, LUA_REGISTRYINDEX, state_->ctx_ref);
    lua_getfield(L, -1, "param");
    for (const auto& p : params_) {
        lua_pushnumber(L, p.value);
        lua_setfield(L, -2, p.name.c_str());
    }
    lua_settop(L, 0);

    loaded_ = true;
    call("init", true);
    lua_getglobal(L, "frame");
    const bool has_frame = lua_isfunction(L, -1);
    lua_pop(L, 1);
    if (!has_frame) {
        loaded_ = false;
        throw ScriptError("the script defines no function frame(ctx)");
    }
}

const std::vector<Rgb>& LightingScript::frame(const FrameInput& in)
{
    if (!loaded_)
        throw ScriptError("no script loaded");
    lua_State* L = state_->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, state_->ctx_ref);
    lua_pushnumber(L, in.t);
    lua_setfield(L, -2, "t");
    lua_pushnumber(L, in.dt);
    lua_setfield(L, -2, "dt");
    push_index_list(L, in.pressed);
    lua_setfield(L, -2, "pressed");
    push_index_list(L, in.released);
    lua_setfield(L, -2, "released");
    lua_createtable(L, 0, static_cast<int>(in.held.size()));
    for (int k : in.held) {
        lua_pushboolean(L, 1);
        lua_rawseti(L, -2, k);
    }
    lua_setfield(L, -2, "held");
    lua_pop(L, 1);
    call("frame", false);
    return colors_;
}

std::vector<Param> LightingScript::params() const { return params_; }

void LightingScript::set_param(const std::string& name, double value)
{
    for (auto& p : params_)
        if (p.name == name) {
            p.value = snap(p, value);
            if (state_ && state_->L) {
                lua_State* L = state_->L;
                lua_rawgeti(L, LUA_REGISTRYINDEX, state_->ctx_ref);
                lua_getfield(L, -1, "param");
                lua_pushnumber(L, p.value);
                lua_setfield(L, -2, p.name.c_str());
                lua_pop(L, 2);
            }
            return;
        }
}

}  // namespace mchose::script
