#!/usr/bin/env bash
set -e

# Run from repo root
doxygen docs/Doxyfile
echo ""
echo "✅ Doxygen docs generated:"
echo "   docs/html/index.html"
