#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/fp2-tests.XXXXXX")
trap 'rm -rf "$build_dir"' EXIT
for source in tests/*_test.cpp; do
  binary="$build_dir/$(basename "$source" .cpp)"
  "${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -pthread -I . "$source" -o "$binary"
  "$binary"
done
uv run --locked python -m unittest discover -s tests -p 'test_*.py'
bun test
