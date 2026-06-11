#!/usr/bin/env bash
# setup-pipeline.sh
# Run this once after cloning to install local pre-commit hooks.
set -e

echo "→ Installing pre-commit..."
pip install pre-commit commitizen detect-secrets

echo "→ Installing hooks..."
pre-commit install                        # runs on git commit
pre-commit install --hook-type commit-msg # runs commitizen on commit-msg

echo "→ Generating secrets baseline (first time only)..."
if [ ! -f .secrets.baseline ]; then
  detect-secrets scan > .secrets.baseline
  echo "   .secrets.baseline created — commit this file."
else
  echo "   .secrets.baseline already exists, skipping."
fi

echo ""
echo "✓ Done. Pre-commit hooks are active."
echo ""
echo "Optional: run all hooks against existing files now:"
echo "  pre-commit run --all-files"
