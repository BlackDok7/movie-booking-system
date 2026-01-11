#!/usr/bin/env bash
set -e

# 1) Build + run tests with coverage flags
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCXX_STD=17 -DENABLE_COVERAGE=ON
cmake --build build
ctest --test-dir build --output-on-failure

# Run the CLI app and tests commands for line coverage
CLI="./build/booking_cli"
if [ -x "$CLI" ]; then
  "$CLI" <<'EOF'
help
movies
theaters 1
theaters 33
seats 1 1
seats 22 1
book 1 1 a1
book 22 1 a1
seats 1 1
seaats
EOF
else
  echo "CLI not found at $CLI (skipping CLI coverage run)"
fi

# 2) Ensure lcov/genhtml exist (Debian/Ubuntu)
if ! command -v lcov >/dev/null 2>&1; then
  apt-get update
  apt-get install -y lcov
fi

# 3) Capture coverage
rm -f coverage.info coverage_filtered.info
rm -rf coverage

lcov --capture --directory build --output-file coverage.info

# 4) Filter out system + googletest + build artifacts (keep your sources)
lcov --remove coverage.info \
  '/usr/*' \
  '*/_deps/*' \
  '*/build/*' \
  --output-file coverage_filtered.info

# 5) Generate HTML report
genhtml coverage_filtered.info --output-directory coverage

echo
echo "Coverage report generated in: ./coverage/index.html"
