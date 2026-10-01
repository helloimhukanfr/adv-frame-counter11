#!/usr/bin/env bash
# Compiles and runs the Geode-free unit tests (core logic, JSON, run format, storage).
set -euo pipefail
cd "$(dirname "$0")/.."
g++ -std=c++20 -Wall -Wextra -Isrc tests/test_core.cpp src/Core/*.cpp src/Recording/*.cpp \
    src/Storage/*.cpp src/Methods/DirectTickMethod.cpp -o /tmp/afc_tests
/tmp/afc_tests
