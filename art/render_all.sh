#!/bin/bash
# Renders everything the plugin needs from the faceplate, at preview quality, and packs it.
#   bash art/render_all.sh            (about half an hour on two cores)
set -euo pipefail
cd "$(dirname "$0")"
python3 make_textures.py
python3 render.py panel --state bare --preview
python3 render.py panel --state amber --preview
python3 render.py knob_sel
python3 render.py knob_small
python3 render.py parts
python3 pack_assets.py
python3 gen_layout_h.py
