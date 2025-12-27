#!/usr/bin/env bash
set -euo pipefail

# Example helper: start a tiny web server in this directory.
# Use whatever you like (python, nginx, caddy, etc.)
# This is just a convenience script.

PORT="${PORT:-8070}"
echo "Serving $(pwd) on http://0.0.0.0:${PORT}/"
python3 -m http.server "${PORT}"
