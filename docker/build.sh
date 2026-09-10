#!/bin/sh
# Build the three dev images. Tags match config/project-types.json.
set -e
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

docker build -t ai-dev-base:0.1 "$here/base"
docker build -t ai-dev-cpp:0.1  "$here/cpp"
docker build -t ai-dev-next:0.1 "$here/nextjs"

echo
echo "built: ai-dev-base:0.1  ai-dev-cpp:0.1  ai-dev-next:0.1"
