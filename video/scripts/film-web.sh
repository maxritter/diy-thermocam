#!/bin/sh
# The website copy of the mastered film (H.264 + AAC, fast start, about 30 MB) and a 1920x1080 poster.
#   sh scripts/film-web.sh   (after `bun run film:render`)
set -e
cd "$(dirname "$0")/.."
src=out/diy-thermocam-v4-film.mp4
web=out/web/diy-thermocam-v4.mp4
mkdir -p out/web
secs=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$src")
# Two-pass to a fixed size: 29 MB minus 128 kbit/s of audio
vk=$(python3 -c "print(int((29*8*1024*1024/$secs - 128000)/1000))")
ffmpeg -hide_banner -loglevel error -y -i "$src" -vf scale=out_range=tv:out_color_matrix=bt709 -c:v libx264 -preset veryslow -tune film -profile:v high -pix_fmt yuv420p -color_range tv -colorspace bt709 -color_primaries bt709 -color_trc bt709 \
  -b:v ${vk}k -pass 1 -passlogfile out/web/x264 -an -f mp4 /dev/null
ffmpeg -hide_banner -loglevel error -y -i "$src" -vf scale=out_range=tv:out_color_matrix=bt709 -c:v libx264 -preset veryslow -tune film -profile:v high -pix_fmt yuv420p -color_range tv -colorspace bt709 -color_primaries bt709 -color_trc bt709 \
  -b:v ${vk}k -maxrate $((vk * 3))k -bufsize $((vk * 4))k -pass 2 -passlogfile out/web/x264 \
  -c:a aac -b:a 128k -movflags +faststart "$web"
rm -f out/web/x264*
# Poster: the camera, title on, at the end of the opening pull-back
at=$(bun -e "import {scene} from './src/film/time'; const s = scene('open'); console.log((s.start + s.length - 0.6).toFixed(2))")
ffmpeg -hide_banner -loglevel error -y -ss "$at" -i "$src" -frames:v 1 -q:v 2 out/web/diy-thermocam-v4.jpg
cp out/diy-thermocam-v4-film.vtt out/web/diy-thermocam-v4.vtt
ls -la out/web
