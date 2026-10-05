#!/usr/bin/env bash
# Run idf.py, sourcing the ESP-IDF environment first if the caller has not
# already done so. Usage: scripts/idf.sh <idf.py args...>
#
# The environment is considered "already active" when idf.py is on PATH and
# IDF_PATH is set (what esp-idf/export.sh gives you). In that case it is used
# as is, whatever ESP-IDF version it points to. Otherwise the esp-idf
# submodule next to this repo is sourced, inside this process only, so the
# caller's shell is never modified.

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v idf.py >/dev/null 2>&1 || [ -z "${IDF_PATH-}" ]; then
    export_sh="$root/esp-idf/export.sh"
    if [ ! -f "$export_sh" ]; then
        echo "error: $export_sh not found (run: git submodule update --init --recursive)" >&2
        exit 1
    fi
    # export.sh is chatty; show its output only if it fails
    log="$(mktemp)"
    if ! . "$export_sh" >"$log" 2>&1; then
        cat "$log" >&2
        rm -f "$log"
        echo "error: failed to set up ESP-IDF (did you run esp-idf/install.sh?)" >&2
        exit 1
    fi
    rm -f "$log"
fi

exec idf.py "$@"
