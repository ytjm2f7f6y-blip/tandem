#!/bin/bash
# The plugin's art for the one-panel Tandem: the bare panel and its lamps lit, the drawer, the
# knob strips and the fader cap. Then pack_one.py puts them in plugin/assets.
#   bash art/render_plugin_art.sh
set -euo pipefail
cd "$(dirname "$0")"
export TD_LAYOUT=layout_one TD_THEME=one
TD_BUILD=build_plugin python3 textures_one.py
TD_BUILD=build_plugin python3 render_one.py --state bare --px 2 --samples 32 --out panel_bare.jpg
TD_BUILD=build_plugin python3 render_one.py --state lit --px 2 --samples 16 --out panel_lit.jpg
TD_DRAWER=1 TD_BUILD=build_plugin_open python3 textures_one.py
TD_DRAWER=1 TD_BUILD=build_plugin_open python3 render_one.py --state bare --no-main --crops "drawer:0,800,1460,150" --crop-px 2 --out bare.jpg --samples 32
TD_DRAWER=1 TD_BUILD=build_plugin_open python3 render_one.py --state lit --no-main --crops "drawer:0,800,1460,150" --crop-px 2 --out lit.jpg --samples 16
TD_DRAWER=1 TD_BUILD=build_plugin python3 render_one.py --strips all --samples 32
TD_BUILD=build_plugin python3 render_one.py --strips fader_cap --samples 48
