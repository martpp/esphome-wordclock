#!/bin/sh
# Builds and runs the host tests. Uses g++ if available, otherwise a Podman/Docker container.
# Arguments are passed on: demo (terminal preview), export (JSON frames for tools/).
set -e
cd "$(dirname "$0")/.."
SRC=test/test_wordclock.cpp
FLAGS="-std=c++17 -Wall -Wextra -Werror -O1"
if command -v g++ >/dev/null 2>&1; then
  mkdir -p build && g++ $FLAGS -o build/test_wordclock $SRC && exec build/test_wordclock "$@"
fi
RUNTIME=$(command -v podman || command -v docker)
exec "$RUNTIME" run --rm -t -v "$PWD:/src:Z" -w /src docker.io/library/gcc:14 \
  sh -c "g++ $FLAGS -o /tmp/t $SRC && /tmp/t $*"
