// SPDX-License-Identifier: MIT
// Minimal JSON value, writer and parser for the backup format (objects, arrays, strings, numbers, bools).
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace json {

struct Value;
using Object = std::map<std::string, Value>;
using Array = std::vector<Value>;

struct Value {
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> v = nullptr;

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : v(b) {}
    Value(int i) : v(static_cast<double>(i)) {}
    Value(double d) : v(d) {}
    Value(const char* s) : v(std::string(s)) {}
    Value(std::string s) : v(std::move(s)) {}
    Value(Array a) : v(std::move(a)) {}
    Value(Object o) : v(std::move(o)) {}

    const Object& obj() const { return get<Object>("object"); }
    const Array& arr() const { return get<Array>("array"); }
    const std::string& str() const { return get<std::string>("string"); }
    int num() const { return static_cast<int>(get<double>("number")); }
    const Value& at(const std::string& key) const
    {
        auto& o = obj();
        auto it = o.find(key);
        if (it == o.end())
            throw std::runtime_error("backup: missing field '" + key + "'");
        return it->second;
    }
    bool has(const std::string& key) const { return obj().count(key) != 0; }

private:
    template <class T>
    const T& get(const char* what) const
    {
        if (auto* p = std::get_if<T>(&v))
            return *p;
        throw std::runtime_error(std::string("backup: expected ") + what);
    }
};

std::string dump(const Value& v, int indent = 0);
Value parse(const std::string& text);

}  // namespace json
