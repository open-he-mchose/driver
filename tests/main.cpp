// SPDX-License-Identifier: MIT
#include <exception>
#include <iostream>

#include "check.hpp"

int main()
{
    int run = 0;
    for (const auto& t : check::registry()) {
        const int before = check::failures();
        try {
            t.fn();
        } catch (const std::exception& e) {
            check::fail(t.name, 0, std::string("unexpected exception: ") + e.what());
        }
        ++run;
        std::cout << (check::failures() == before ? "ok   " : "FAIL ") << t.name << "\n";
    }
    std::cout << run << " tests, " << check::failures() << " failure(s)\n";
    return check::failures() ? 1 : 0;
}
