#!/bin/sh
# mkvideo.sh <dir>: the test clips for tests/mf (needs ffmpeg with libx264). clip.mp4 is any short 512x288 H.264 clip;
# layout.mp4 is 854x480 (not a multiple of 4 wide, as Photoshop's What's New videos), white top quarter, then red left
# and blue right.
d=${1:-.}
ffmpeg -v error -y -f lavfi -i testsrc2=s=512x288:d=4:r=30 -c:v libx264 -pix_fmt yuv420p "$d/clip.mp4" &&
ffmpeg -v error -y -f lavfi -i "color=c=red:s=854x480:d=4:r=30,drawbox=x=427:y=0:w=427:h=480:c=blue:t=fill,drawbox=x=0:y=0:w=854:h=120:c=white:t=fill" \
       -c:v libx264 -pix_fmt yuv420p "$d/layout.mp4"
