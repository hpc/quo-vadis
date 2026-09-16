#
# Copyright (c) 2020-2026 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

# User parameter to enable gcov code coverage instrumentation.
option(QV_COVERAGE "Enable gcov code coverage instrumentation" OFF)

message(CHECK_START "Determining code coverage status")

if(NOT QV_COVERAGE)
    message(CHECK_PASS "disabled")
    return()
endif()

# Coverage instrumentation is only supported with GCC or Clang.
if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
    message(
        FATAL_ERROR
        "QV_COVERAGE requires a GNU or Clang compiler "
        "(found ${CMAKE_CXX_COMPILER_ID})."
    )
endif()

# Force an unoptimized, debuggable build so that line-level coverage maps
# accurately back to the source.
message(STATUS "Adding code coverage compile/link flags...")
add_compile_options(-O0 -g --coverage)
add_link_options(--coverage)

# Locate gcovr to drive report generation. If it is not available, the build
# is still instrumented and the resulting .gcda/.gcno files can be processed
# manually with gcov.
find_program(GCOVR_EXECUTABLE gcovr)

if(GCOVR_EXECUTABLE)
    # Run the test suite to produce .gcda data, then generate the report.
    # ctest's exit status is intentionally ignored so that a coverage report
    # is still produced even when some tests fail (often exactly when the
    # report is most useful).
    add_custom_target(coverage
        COMMAND ${CMAKE_CTEST_COMMAND} || ${CMAKE_COMMAND} -E true
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory
                ${CMAKE_BINARY_DIR}/coverage
        COMMAND ${GCOVR_EXECUTABLE}
                --root ${CMAKE_SOURCE_DIR}
                --gcov-ignore-parse-errors negative_hits.warn
                --filter ${CMAKE_SOURCE_DIR}/src
                --filter ${CMAKE_SOURCE_DIR}/include
                --exclude ${CMAKE_SOURCE_DIR}/tests
                --print-summary
                --html-details ${CMAKE_BINARY_DIR}/coverage/index.html
                --xml ${CMAKE_BINARY_DIR}/coverage/coverage.xml
                ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests and generating coverage report"
        USES_TERMINAL
        VERBATIM
    )
else()
    message(
        WARNING
        "gcovr not found: build is instrumented, but the 'coverage' target "
        "is unavailable. Install gcovr (e.g. 'pip install gcovr') and "
        "re-run cmake, or process .gcda/.gcno files manually with gcov."
    )
endif()

message(CHECK_PASS "enabled")

# vim: ts=4 sts=4 sw=4 expandtab
