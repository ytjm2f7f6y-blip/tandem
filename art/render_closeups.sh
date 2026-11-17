#!/bin/bash
# Close-up crops of the look mockups, at 4 pixels per point:  bash art/render_closeups.sh
set -euo pipefail
cd "$(dirname "$0")"
export TD_LAYOUT=layout_final TD_BUILD=build_final
CROPS="knobs:690,138,262,262;control:1348,96,280,280;perf:24,330,212,212"
for t in ivory machined capped; do
  TD_THEME=$t python3 render.py panel --px 4 --samples 32 --out "close_$t" --closeups "$CROPS"
done
