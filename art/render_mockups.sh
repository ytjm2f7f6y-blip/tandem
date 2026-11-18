#!/bin/bash
# The look mockups of the final instrument, one render per finish:  bash art/render_mockups.sh
set -euo pipefail
cd "$(dirname "$0")"
export TD_LAYOUT=layout_final TD_BUILD=build_final
python3 make_textures.py
for t in ivory machined capped; do
  TD_THEME=$t python3 render.py panel --px 1.5 --samples 40 --out "mockup_$t.jpg"
done
