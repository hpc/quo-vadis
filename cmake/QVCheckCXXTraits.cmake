#
# Copyright (c)      2026 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

# Verify that the selected C++ compiler actually provides the C++20 standard
# library features we rely on. Setting CMAKE_CXX_STANDARD only makes CMake pass
# the -std=c++20 flag if the compiler accepts it; it does not guarantee that the
# accompanying standard library is complete. Older toolchains accept -std=c++20
# but ship an incomplete libstdc++ that lacks <ranges>, which causes confusing
# failures partway through the build instead of at configure time. Probe for
# real support here so we fail fast with an actionable message.

include(CheckCXXSourceCompiles)

# Compile the probe as C++20 regardless of any inherited flags.
set(QVI_SAVED_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "-std=c++20")

check_cxx_source_compiles(
"
#include <version>
#include <ranges>
#include <concepts>
static_assert(__cpp_lib_ranges >= 201911L, \"std::ranges not supported\");
int main() {
    auto v = std::ranges::views::iota(0, 3);
    return std::ranges::distance(v) == 3 ? 0 : 1;
}
"
    QV_HAVE_CXX20
)

set(CMAKE_REQUIRED_FLAGS "${QVI_SAVED_CMAKE_REQUIRED_FLAGS}")
unset(QVI_SAVED_CMAKE_REQUIRED_FLAGS)

if(NOT QV_HAVE_CXX20)
    message(
        FATAL_ERROR
        "The selected C++ compiler "
        "(${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}) does not "
        "provide adequate C++20 standard library support. quo-vadis requires "
        "C++20 library features such as <ranges>. Please use a newer toolchain."
    )
endif()

# vim: ts=4 sts=4 sw=4 expandtab
