#!/bin/sh
# Render every 3D shot of the launch film, one after another (resumable: finished frames are skipped).
#   sh scripts/render-shots.sh [shot ...]
#   sh scripts/render-shots.sh --points   # part positions for the labels that follow the parts
set -e
cd "$(dirname "$0")/../.."
if [ "$1" = "--points" ]; then
  blender -b --factory-startup -P video/blender/film_shots.py -- --shot assemble \
    --project lepton_module,teensy,lipo,display,shell,front_shell,screws_front,pcba 2>&1 | grep -E "^\[film\]|Error|Traceback"
  blender -b --factory-startup -P video/blender/film_shots.py -- --shot explode \
    --project pcba,shell,front_shell,teensy,display,lipo,lepton_module 2>&1 | grep -E "^\[film\]|Error|Traceback"
  cp video/work/blender/assemble/points.json video/src/film/data/assemble-points.json
  cp video/work/blender/explode/points.json video/src/film/data/explode-points.json
  exit 0
fi
shots=${*:-"open boot assemble sensor case-shell case-laser explode"}
for s in $shots; do
  echo "== $s $(date +%T)"
  blender -b --factory-startup -P video/blender/film_shots.py -- --shot "$s" --samples 48 2>&1 | grep --line-buffered -E "^\[film\]|Error|Traceback"
done
echo "== done $(date +%T)"
