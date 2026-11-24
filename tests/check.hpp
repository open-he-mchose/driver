// SPDX-License-Identifier: MIT
// Minimal test framework: TEST(name) { CHECK(...); CHECK_EQ(a, b); CHECK_THROWS(expr, Type); }
#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace check {

struct Test {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Test>& registry()
{
    static std::vector<Test> r;
    return r;
}

inline int& failures()
{
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

inline void fail(const char* file, int line, const std::string& msg)
{
    ++failures();
    std::cerr << file << ":" << line << ": FAILED: " << msg << "\n";
}

}  // namespace check

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2(a, b)
#define TEST(name)                                                                  \
    static void CHECK_CAT(test_fn_, name)();                                        \
    static check::Registrar CHECK_CAT(test_reg_, name)(#name, CHECK_CAT(test_fn_, name)); \
    static void CHECK_CAT(test_fn_, name)()

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond))                                                                \
            check::fail(__FILE__, __LINE__, #cond);                                 \
    } while (0)

#define CHECK_EQ(a, b)                                                              \
    do {                                                                            \
        auto&& check_a_ = (a);                                                      \
        auto&& check_b_ = (b);                                                      \
        if (!(check_a_ == check_b_)) {                                              \
            std::ostringstream check_os_;                                           \
            check_os_ << #a " == " #b " (" << check_a_ << " vs " << check_b_ << ")"; \
            check::fail(__FILE__, __LINE__, check_os_.str());                       \
        }                                                                           \
    } while (0)

#define CHECK_THROWS(expr, type)                                                    \
    do {                                                                            \
        bool check_thrown_ = false;                                                 \
        try {                                                                       \
            (void)(expr);                                                           \
        } catch (const type&) {                                                     \
            check_thrown_ = true;                                                   \
        } catch (...) {                                                             \
        }                                                                           \
        if (!check_thrown_)                                                         \
            check::fail(__FILE__, __LINE__, #expr " did not throw " #type);         \
    } while (0)
