#!/usr/bin/env bash
# Configure this repository to use the versioned .githooks/ directory.

set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

chmod +x .githooks/pre-commit
git config core.hooksPath .githooks

echo "Configured git core.hooksPath -> $(git config --get core.hooksPath)"
echo "Pre-commit hook is active. Test manually anytime with: .githooks/pre-commit --all"
