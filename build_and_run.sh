#!/usr/bin/env bash
set -e

BUILD_DIR=build
CXX_STD=17

echo "==> Configuring project (C++${CXX_STD})"
cmake -S . -B ${BUILD_DIR} -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCXX_STD=${CXX_STD}

echo "==> Building project"
cmake --build ${BUILD_DIR}
echo "==> Done"

echo "==> Running CLI"
./${BUILD_DIR}/booking_cli

