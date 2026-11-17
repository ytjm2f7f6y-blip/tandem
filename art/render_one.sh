#!/bin/bash
# The one-panel Tandem: print masks, then the panel, close-ups, SHIFT pages, and the drawer pulled out.
#   bash art/render_one.sh
set -euo pipefail
cd "$(dirname "$0")"
export TD_LAYOUT=layout_one TD_THEME=one
TD_BUILD=build_one python3 textures_one.py
TD_BUILD=build_one python3 render_one.py --px 1.5 --samples 40 --out one.jpg \
  --crops "top:1150,8,460,82;filter:692,96,312,374;lfo:1016,96,194,374;perf:36,484,462,300;prog:744,484,530,300" \
  --crop-px 3.5 --pages
TD_DRAWER=1 TD_BUILD=build_one_open python3 textures_one.py
TD_DRAWER=1 TD_BUILD=build_one_open python3 render_one.py --px 1.5 --samples 40 --out one_open.jpg \
  --crops "drawer:40,770,1560,180" --crop-px 2.5
